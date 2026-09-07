#include "graph.h"

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

struct binder_pair {
	const struct pg_object *left;
	const struct pg_object *right;
	const struct binder_pair *parent;
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
	memset(graph, 0, sizeof(*graph));
}

static uint64_t mix(uint64_t left, uint64_t right)
{
	return (left ^ (right + UINT64_C(0x9e3779b97f4a7c15))) * UINT64_C(1099511628211);
}

struct alpha_entry {
	struct pg_index_entry index;
	const struct pg_term *left;
	const struct pg_term *right;
	const struct binder_pair *scope;
};

struct alpha_context {
	struct pg_graph arena;
	struct pg_index seen;
};

static int alpha_equal(struct alpha_context *context, const struct pg_term *left, const struct pg_term *right,
	const struct binder_pair *scope)
{
	if (left->kind != right->kind) return 0;
	if (left->kind == PG_REFERENCE) {
		for (const struct binder_pair *p = scope; p; p = p->parent) {
			if (left->as.reference == p->left) return right->as.reference == p->right;
			if (right->as.reference == p->right) return 0;
		}
		return left->as.reference == right->as.reference;
	}
	uint64_t hash = mix(mix((uintptr_t)left, (uintptr_t)right), (uintptr_t)scope);
	for (struct pg_index_entry *candidate = pg_index_candidates(&context->seen, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct alpha_entry *entry = (const struct alpha_entry *)candidate;
		if (entry->left != left) continue;
		if (entry->right != right) continue;
		if (entry->scope == scope) return 1;
	}
	int result;
	switch (left->kind) {
	case PG_APPLICATION:
		result = alpha_equal(context, left->as.application.function, right->as.application.function, scope);
		if (result != 1) return result;
		result = alpha_equal(context, left->as.application.argument, right->as.application.argument, scope);
		break;
	case PG_LAMBDA: {
		struct binder_pair *binder = pg_alloc(&context->arena, sizeof(*binder));
		if (!binder) return -1;
		*binder = (struct binder_pair){left->as.lambda.binder, right->as.lambda.binder, scope};
		result = alpha_equal(context, left->as.lambda.body, right->as.lambda.body, binder);
		break;
	}
	default:
		return 0;
	}
	if (result != 1) return result;
	struct alpha_entry *entry = pg_alloc(&context->arena, sizeof(*entry));
	if (!entry) return -1;
	entry->left = left;
	entry->right = right;
	entry->scope = scope;
	if (pg_index_insert(&context->seen, &entry->index, hash) != 0) return -1;
	return 1;
}

int pg_alpha_equal(const struct pg_term *left, const struct pg_term *right)
{
	if (!left || !right) return 0;
	if (left == right) return 1;
	struct alpha_context context = {0};
	if (pg_index_init(&context.seen) != 0) return -1;
	int result = alpha_equal(&context, left, right, NULL);
	pg_index_destroy(&context.seen);
	pg_graph_destroy(&context.arena);
	return result;
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

struct pg_index_entry *pg_index_candidates(const struct pg_index *index, uint64_t hash)
{
	return index->buckets[hash % index->capacity];
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
			size_t bucket = entry->hash % capacity;
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
	size_t bucket = hash % index->capacity;
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
