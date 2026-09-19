#include "graph_internal.h"

#include <stdlib.h>
#include <string.h>

struct pg_block {
	struct pg_block *next;
	size_t used;
	size_t capacity;
	max_align_t data[];
};

struct pg_entry {
	struct pg_index_entry index;
	struct pg_term term;
};

void *pg_alloc(struct pg_graph *graph, size_t bytes)
{
	size_t alignment = sizeof(max_align_t);
	if (bytes > SIZE_MAX - alignment) return NULL;
	size_t units = (bytes + alignment - 1) / alignment;
	if (units == 0) units = 1;
	struct pg_block *block = graph->blocks;
	if (!block || units > block->capacity - block->used) {
		size_t capacity = units > 512 ? units : 512;
		if (capacity > (SIZE_MAX - sizeof(*block)) / alignment) return NULL;
		block = calloc(1, sizeof(*block) + capacity * alignment);
		if (!block) return NULL;
		block->capacity = capacity;
		block->next = graph->blocks;
		graph->blocks = block;
	}
	void *result = block->data + block->used;
	block->used += units;
	return result;
}

int pg_graph_init(struct pg_graph *graph)
{
	memset(graph, 0, sizeof(*graph));
	return pg_index_init(&graph->terms);
}

void pg_graph_destroy(struct pg_graph *graph)
{
	while (graph->blocks) {
		struct pg_block *next = graph->blocks->next;
		free(graph->blocks);
		graph->blocks = next;
	}
	pg_index_destroy(&graph->terms);
	pg_index_destroy(&graph->objects);
	memset(graph, 0, sizeof(*graph));
}

static uint64_t mix(uint64_t left, uint64_t right)
{
	return (left ^ (right + UINT64_C(0x9e3779b97f4a7c15))) * UINT64_C(1099511628211);
}

static uint64_t comparison_hash(const struct pg_term *left, const struct pg_term *right, const struct binder_pair *scope)
{
	return mix(mix((uintptr_t)left, (uintptr_t)right), (uintptr_t)scope);
}

static struct alpha_entry *comparison_find(struct pg_comparison_state *context, const struct pg_term *left, const struct pg_term *right,
	const struct binder_pair *scope)
{
	uint64_t hash = comparison_hash(left, right, scope);
	for (struct pg_index_entry *candidate = pg_index_candidates(&context->seen, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		struct alpha_entry *entry = (struct alpha_entry *)candidate;
		if (entry->left != left) continue;
		if (entry->right != right) continue;
		if (entry->scope == scope) return entry;
	}
	return NULL;
}

int pg_comparison_index(struct pg_comparison_state *context, struct alpha_entry *entry)
{
	if (comparison_find(context, entry->left, entry->right, entry->scope)) return -1;
	uint64_t hash = comparison_hash(entry->left, entry->right, entry->scope);
	return pg_index_insert(&context->seen, &entry->index, hash);
}

static int comparison_push(struct pg_comparison_state *context, const struct pg_term *left, const struct pg_term *right,
	const struct binder_pair *scope)
{
	if (!left || !right) return -1;
	if (left == right && !scope) return 0;
	if (comparison_find(context, left, right, scope)) return 0;
	struct alpha_entry *entry = pg_alloc(&context->arena, sizeof(*entry));
	if (!entry) return -1;
	entry->left = left;
	entry->right = right;
	entry->scope = scope;
	entry->cursor = scope;
	entry->next = context->pending;
	uint64_t hash = comparison_hash(left, right, scope);
	if (pg_index_insert(&context->seen, &entry->index, hash) != 0) return -1;
	context->pending = entry;
	return 0;
}

static int comparison_init(struct pg_comparison *work, const struct pg_term *left,
	const struct pg_term *right, void *policy,
	int (*normalize)(void *, const struct pg_term *, const struct pg_term **),
	const struct pg_object *absent, const struct binder_pair *seed);

static enum pg_comparison_status comparison_step(struct pg_comparison_state *context)
{
	struct alpha_entry *entry = context->pending;
	/* Compare finite binding structure before unfolding each subproblem.
	 * The probe borrows the same scope, so bound/free references stay distinct. */
	if (context->normalize && !entry->structural_checked) {
		if (!context->structural.state && comparison_init(&context->structural,
			entry->left, entry->right, NULL, NULL, NULL, entry->scope)) return PG_COMPARISON_ERROR;
		enum pg_comparison_status status = pg_comparison_advance(&context->structural, 1);
		if (status == PG_COMPARISON_PENDING || status == PG_COMPARISON_ERROR) return status;
		context->structural_tasks += pg_comparison_task_count(&context->structural);
		pg_comparison_destroy(&context->structural);
		entry->structural_checked = 1;
		if (status == PG_COMPARISON_EQUAL) {
			context->pending = entry->next;
			return context->pending ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
		}
		return PG_COMPARISON_PENDING;
	}
	if (entry->stage < 2) {
		unsigned side = entry->stage;
		const struct pg_term *input = side ? entry->right : entry->left;
		int result = 1;
		if (context->normalize) result = context->normalize(context->policy, input, &entry->normalized[side]);
		else entry->normalized[side] = input;
		if (result < 0) return PG_COMPARISON_ERROR;
		if (!result) return PG_COMPARISON_PENDING;
		if (!entry->normalized[side]) return PG_COMPARISON_ERROR;
		++entry->stage;
		return PG_COMPARISON_PENDING;
	}
	const struct pg_term *left = entry->normalized[0], *right = entry->normalized[1];
	if (left == right && !entry->scope) {
		context->pending = entry->next;
		return context->pending ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
	}
	if (left->kind != right->kind) return PG_COMPARISON_DIFFERENT;
	if (left->kind == PG_REFERENCE) {
		const struct binder_pair *scope = entry->cursor;
		if (scope) {
			if (left->as.reference == scope->left) {
				if (right->as.reference != scope->right) return PG_COMPARISON_DIFFERENT;
			} else {
				if (right->as.reference == scope->right) return PG_COMPARISON_DIFFERENT;
				entry->cursor = scope->parent;
				return PG_COMPARISON_PENDING;
			}
		} else if (left->as.reference != right->as.reference) return PG_COMPARISON_DIFFERENT;
	}
	context->pending = entry->next;
	switch (left->kind) {
	case PG_APPLICATION:
		if (comparison_push(context, left->as.application.argument, right->as.application.argument, entry->scope) != 0) return PG_COMPARISON_ERROR;
		if (comparison_push(context, left->as.application.function, right->as.application.function, entry->scope) != 0) return PG_COMPARISON_ERROR;
		break;
	case PG_LAMBDA: {
		const struct binder_pair *scope = entry->scope;
		/* Independence compares one term with itself under x -> absent.
		 * Other binders leave that question unchanged; binding x discharges it.
		 * Keep the same scope key so shared bodies remain shared work. */
		if (left == right && scope && !scope->parent && !scope->right) {
			if (left->as.lambda.binder == scope->left) scope = NULL;
		} else
		/* An identical binder needs no map in an already identical scope.
		 * Under a nonidentity map it must still shadow older pairs. */
		if (scope || left->as.lambda.binder != right->as.lambda.binder) {
			struct binder_pair *binder = pg_alloc(&context->arena, sizeof(*binder));
			if (!binder) return PG_COMPARISON_ERROR;
			*binder = (struct binder_pair){left->as.lambda.binder, right->as.lambda.binder, scope};
			scope = binder;
		}
		if (comparison_push(context, left->as.lambda.body, right->as.lambda.body, scope) != 0) return PG_COMPARISON_ERROR;
		break;
	}
	case PG_REFERENCE: break;
	default: return PG_COMPARISON_ERROR;
	}
	return context->pending ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
}

static int comparison_init(struct pg_comparison *work, const struct pg_term *left,
	const struct pg_term *right, void *policy,
	int (*normalize)(void *, const struct pg_term *, const struct pg_term **),
	const struct pg_object *absent, const struct binder_pair *seed)
{
	work->state = calloc(1, sizeof(*work->state));
	if (!work->state) return -1;
	work->state->policy = policy;
	work->state->normalize = normalize;
	if (pg_index_init(&work->state->seen) != 0) goto fail;
	const struct binder_pair *scope = seed;
	if (absent) {
		struct binder_pair *binding = pg_alloc(&work->state->arena, sizeof(*binding));
		if (!binding) goto fail;
		/* No reference can match NULL. Inner lambdas shadow this seed. */
		*binding = (struct binder_pair){absent, NULL, NULL};
		scope = binding;
	}
	if (comparison_push(work->state, left, right, scope) != 0) goto fail;
	work->state->status = work->state->pending ? PG_COMPARISON_PENDING : PG_COMPARISON_EQUAL;
	return 0;
fail:
	pg_comparison_destroy(work);
	return -1;
}

int pg_comparison_init(struct pg_comparison *work, const struct pg_term *left,
	const struct pg_term *right, void *policy,
	int (*normalize)(void *, const struct pg_term *, const struct pg_term **))
{
	return comparison_init(work, left, right, policy, normalize, NULL, NULL);
}

int pg_independence_init(struct pg_comparison *work, const struct pg_term *term,
	const struct pg_object *binder)
{
	work->state = NULL;
	if (!binder || binder->kind != PG_BINDER) return -1;
	return comparison_init(work, term, term, NULL, NULL, binder, NULL);
}

void pg_comparison_destroy(struct pg_comparison *work)
{
	if (!work->state) return;
	pg_comparison_destroy(&work->state->structural);
	pg_index_destroy(&work->state->seen);
	pg_graph_destroy(&work->state->arena);
	free(work->state);
	work->state = NULL;
}

enum pg_comparison_status pg_comparison_status(const struct pg_comparison *work)
{
	return work->state ? work->state->status : PG_COMPARISON_ERROR;
}

enum pg_comparison_status pg_comparison_advance(struct pg_comparison *work, uint64_t budget)
{
	while (pg_comparison_status(work) == PG_COMPARISON_PENDING && budget) {
		--budget;
		++work->state->steps;
		work->state->status = comparison_step(work->state);
	}
	return pg_comparison_status(work);
}

uint64_t pg_comparison_steps(const struct pg_comparison *work)
{
	return work->state ? work->state->steps : 0;
}

size_t pg_comparison_task_count(const struct pg_comparison *work)
{
	return work->state ? work->state->seen.count + work->state->structural_tasks
		+ pg_comparison_task_count(&work->state->structural) : 0;
}

static int comparison_finish(struct pg_comparison *work)
{
	while (pg_comparison_advance(work, UINT64_MAX) == PG_COMPARISON_PENDING) {}
	enum pg_comparison_status result = pg_comparison_status(work);
	pg_comparison_destroy(work);
	return result == PG_COMPARISON_EQUAL ? 1 : result == PG_COMPARISON_DIFFERENT ? 0 : -1;
}

int pg_term_independent(const struct pg_term *term, const struct pg_object *binder)
{
	struct pg_comparison work;
	if (pg_independence_init(&work, term, binder) != 0) return -1;
	return comparison_finish(&work);
}

int pg_alpha_equal(const struct pg_term *left, const struct pg_term *right)
{
	if (!left || !right) return 0;
	if (left == right) return 1;
	struct pg_comparison work;
	if (pg_comparison_init(&work, left, right, NULL, NULL) != 0) return -1;
	return comparison_finish(&work);
}

int pg_index_init(struct pg_index *index)
{
	memset(index, 0, sizeof(*index));
	index->capacity = 64;
	index->buckets = calloc(index->capacity, sizeof(*index->buckets));
	return index->buckets ? 0 : -1;
}

void pg_index_destroy(struct pg_index *index)
{
	free(index->buckets);
	memset(index, 0, sizeof(*index));
}

static size_t index_bucket(uint64_t hash, size_t capacity)
{
	/* Avalanche high pointer bits before reducing to a power-of-two table.
	 * The stored hash and exact-key comparison remain unchanged. */
	hash ^= hash >> 33;
	hash *= UINT64_C(0xff51afd7ed558ccd);
	hash ^= hash >> 33;
	hash *= UINT64_C(0xc4ceb9fe1a85ec53);
	hash ^= hash >> 33;
	return hash & (capacity - 1);
}

struct pg_index_entry *pg_index_candidates(const struct pg_index *index, uint64_t hash)
{
	return index->buckets[index_bucket(hash, index->capacity)];
}

static int grow_index(struct pg_index *index)
{
	if (index->count < index->capacity) return 0;
	if (index->capacity > SIZE_MAX / 2 / sizeof(*index->buckets)) return -1;
	size_t capacity = index->capacity * 2;
	struct pg_index_entry **buckets = calloc(capacity, sizeof(*buckets));
	if (!buckets) return -1;
	for (size_t i = 0; i < index->capacity; ++i) {
		struct pg_index_entry *entry = index->buckets[i];
		while (entry) {
			struct pg_index_entry *next = entry->next;
			size_t bucket = index_bucket(entry->hash, capacity);
			entry->next = buckets[bucket];
			buckets[bucket] = entry;
			entry = next;
		}
	}
	free(index->buckets);
	index->buckets = buckets;
	index->capacity = capacity;
	return 0;
}

int pg_index_insert(struct pg_index *index, struct pg_index_entry *entry, uint64_t hash)
{
	if (grow_index(index) != 0) return -1;
	size_t bucket = index_bucket(hash, index->capacity);
	entry->hash = hash;
	entry->next = index->buckets[bucket];
	index->buckets[bucket] = entry;
	index->count++;
	return 0;
}

static const struct pg_term *intern(struct pg_graph *graph, struct pg_term *term)
{
	uint64_t hash;
	if (term->kind == PG_APPLICATION) {
		hash = mix(mix(PG_APPLICATION + 1, (uintptr_t)term->as.application.function),
			(uintptr_t)term->as.application.argument);
	} else if (term->kind == PG_REFERENCE) {
		hash = mix(PG_REFERENCE + 1, (uintptr_t)term->as.reference);
	} else {
		hash = mix(mix(PG_LAMBDA + 1, (uintptr_t)term->as.lambda.binder),
			(uintptr_t)term->as.lambda.body);
	}
	for (struct pg_index_entry *candidate = pg_index_candidates(&graph->terms, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_entry *entry = (const struct pg_entry *)candidate;
		if (entry->term.kind != term->kind) continue;
		if (term->kind == PG_APPLICATION) {
			if (entry->term.as.application.function != term->as.application.function) continue;
			if (entry->term.as.application.argument != term->as.application.argument) continue;
			return &entry->term;
		}
		if (term->kind == PG_REFERENCE) {
			if (entry->term.as.reference == term->as.reference) return &entry->term;
			continue;
		}
		if (entry->term.as.lambda.binder != term->as.lambda.binder) continue;
		if (entry->term.as.lambda.body == term->as.lambda.body) return &entry->term;
	}
	struct pg_entry *entry = pg_alloc(graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->term = *term;
	if (pg_index_insert(&graph->terms, &entry->index, hash) != 0) return NULL;
	return &entry->term;
}

const struct pg_object *pg_binder(struct pg_graph *graph)
{
	struct pg_object *binder = pg_alloc(graph, sizeof(*binder));
	if (binder) binder->kind = PG_BINDER;
	return binder;
}

const struct pg_term *pg_reference(struct pg_graph *graph, const struct pg_object *object)
{
	if (!object) return NULL;
	struct pg_term term = {.kind = PG_REFERENCE, .as.reference = object};
	return intern(graph, &term);
}

const struct pg_term *pg_application(struct pg_graph *graph,
	const struct pg_term *function, const struct pg_term *argument)
{
	if (!function || !argument) return NULL;
	struct pg_term term = {.kind = PG_APPLICATION, .as.application = {function, argument}};
	return intern(graph, &term);
}

const struct pg_term *pg_lambda(struct pg_graph *graph,
	const struct pg_object *binder, const struct pg_term *body)
{
	if (!binder || !body) return NULL;
	if (binder->kind != PG_BINDER) return NULL;
	struct pg_term term = {.kind = PG_LAMBDA, .as.lambda = {binder, body}};
	return intern(graph, &term);
}
