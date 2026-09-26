#include "effect_inference.h"
#include "dag.h"

#include <string.h>

struct pg_effect_equation {
	const struct pg_effect_inference *owner;
	const struct pg_object *parameter;
	const struct pg_effect_row *seed;
	const struct pg_effect_row *value;
	struct pg_effect_dependency *outgoing;
	struct pg_effect_equation *next;
	int queued;
};
struct pg_effect_dependency {
	struct pg_index_entry index;
	struct pg_effect_equation *source, *target;
	const struct pg_effect_row *mask;
	struct pg_effect_dependency *next;
};
struct row_source {
	struct pg_index_entry index;
	const struct pg_object *object;
	struct pg_effect_equation *equation;
};

static struct pg_effect_equation *row_source(const struct pg_effect_inference *work,
	const struct pg_object *object)
{
	uint64_t hash = ((uintptr_t)object >> 3) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->row_sources, hash); entry; entry = entry->next) {
		struct row_source *source = (void *)entry;
		if (entry->hash == hash && source->object == object) return source->equation;
	}
	return NULL;
}

static int register_source(struct pg_effect_inference *work, const struct pg_object *object,
	struct pg_effect_equation *equation)
{
	struct row_source *source = pg_alloc(&work->arena, sizeof(*source));
	if (!source) { work->failed = 1; return -1; }
	*source = (struct row_source){.object = object, .equation = equation};
	uint64_t hash = ((uintptr_t)object >> 3) * UINT64_C(1099511628211);
	if (pg_index_insert(&work->row_sources, &source->index, hash)) {
		work->failed = 1; return -1;
	}
	return 0;
}

static void enqueue(struct pg_effect_inference *work, struct pg_effect_equation *equation)
{
	if (equation->queued) return;
	equation->queued = 1;
	equation->next = NULL;
	if (work->tail) work->tail->next = equation;
	else work->head = equation;
	work->tail = equation;
}

int pg_effect_inference_init(struct pg_effect_inference *work, struct pg_graph *rows)
{
	memset(work, 0, sizeof(*work));
	work->rows = rows;
	if (!rows) return -1;
	if (pg_index_init(&work->dependencies)) return -1;
	if (!pg_index_init(&work->row_sources)) return 0;
	pg_effect_inference_destroy(work);
	return -1;
}

void pg_effect_inference_destroy(struct pg_effect_inference *work)
{
	pg_index_destroy(&work->dependencies);
	pg_index_destroy(&work->row_sources);
	pg_graph_destroy(&work->arena);
	memset(work, 0, sizeof(*work));
}

struct pg_effect_equation *pg_effect_equation(struct pg_effect_inference *work,
	const struct pg_effect_row *seed)
{
	if (!work->rows || work->sealed || work->failed || !seed) return NULL;
	const struct pg_object *parameter = pg_binder(work->rows);
	if (!parameter) { work->failed = 1; return NULL; }
	return pg_effect_equation_at(work, parameter, seed);
}

struct pg_effect_equation *pg_effect_equation_at(struct pg_effect_inference *work,
	const struct pg_object *parameter, const struct pg_effect_row *seed)
{
	if (!work->rows || work->failed || !seed || !parameter) return NULL;
	if (parameter->kind != PG_BINDER || parameter->owner) return NULL;
	struct pg_effect_equation *existing = row_source(work, parameter);
	if (existing) return existing->seed == seed ? existing : NULL;
	if (work->sealed) return NULL;
	struct pg_effect_equation *equation = pg_alloc(&work->arena, sizeof(*equation));
	if (!equation) { work->failed = 1; return NULL; }
	*equation = (struct pg_effect_equation){.owner = work, .parameter = parameter, .seed = seed, .value = seed};
	if (register_source(work, parameter, equation)) return NULL;
	enqueue(work, equation);
	return equation;
}

const struct pg_effect_row *pg_effect_equation_seed(const struct pg_effect_inference *work,
	const struct pg_effect_equation *equation)
{
	return equation && equation->owner == work ? equation->seed : NULL;
}

struct pg_effect_equation *pg_effect_equation_find(const struct pg_effect_inference *work,
	const struct pg_object *parameter)
{
	if (!work || !work->rows || work->failed || !parameter) return NULL;
	if (parameter->kind != PG_BINDER || parameter->owner) return NULL;
	return row_source(work, parameter);
}

int pg_effect_inference_visit(const struct pg_effect_inference *work, void *context,
	int (*equation)(void *, const struct pg_effect_equation *, const struct pg_effect_row *),
	int (*dependency)(void *, const struct pg_effect_equation *, const struct pg_effect_row *, const struct pg_effect_equation *))
{
	if (!work->rows || work->failed) return -1;
	if (equation) for (size_t i = 0; i < work->row_sources.capacity; ++i)
		for (const struct pg_index_entry *entry = work->row_sources.buckets[i]; entry; entry = entry->next) {
			const struct row_source *source = (const void *)entry;
			if (source->object != source->equation->parameter) continue;
			if (equation(context, source->equation, source->equation->seed)) return -1;
		}
	if (dependency) for (size_t i = 0; i < work->dependencies.capacity; ++i)
		for (const struct pg_index_entry *entry = work->dependencies.buckets[i]; entry; entry = entry->next) {
			const struct pg_effect_dependency *edge = (const void *)entry;
			if (dependency(context, edge->source, edge->mask, edge->target)) return -1;
		}
	return 0;
}

const struct pg_object *pg_effect_equation_parameter(const struct pg_effect_inference *work,
	const struct pg_effect_equation *equation)
{
	return equation && equation->owner == work ? equation->parameter : NULL;
}

static int collect_equation(void *context, const struct pg_effect_equation *equation,
	const struct pg_effect_row *seed)
{
	struct pg_dag *terms = context;
	if (pg_dag_add(terms, pg_reference(&terms->storage, equation->parameter))) return -1;
	return pg_dag_add(terms, pg_effect_reference(&terms->storage, seed));
}

static int collect_dependency(void *context, const struct pg_effect_equation *source,
	const struct pg_effect_row *mask, const struct pg_effect_equation *target)
{
	(void)source;
	(void)target;
	struct pg_dag *terms = context;
	/* Both endpoints were collected with their definitions, before edges. */
	return pg_dag_add(terms, pg_effect_reference(&terms->storage, mask));
}

int pg_effect_inference_collect(const struct pg_effect_inference *work, struct pg_dag *terms)
{
	if (!work || !terms || !terms->index.capacity || !terms->storage.terms.capacity || terms->failed) return -1;
	return pg_effect_inference_visit(work, terms, collect_equation, collect_dependency);
}

struct effect_pack {
	struct pg_graph *storage;
	const struct pg_term **roots;
	size_t equations, count;
};

static int pack_equation(void *context, const struct pg_effect_equation *equation,
	const struct pg_effect_row *seed)
{
	struct effect_pack *pack = context;
	const struct pg_term *parameter = pg_reference(pack->storage, equation->parameter);
	const struct pg_term *row = pg_effect_reference(pack->storage, seed);
	if (!parameter || !row) return -1;
	pack->roots[pack->count++] = parameter;
	pack->roots[pack->count++] = row;
	++pack->equations;
	return 0;
}

static int pack_dependency(void *context, const struct pg_effect_equation *source,
	const struct pg_effect_row *mask, const struct pg_effect_equation *target)
{
	struct effect_pack *pack = context;
	const struct pg_term *a = pg_reference(pack->storage, source->parameter);
	const struct pg_term *b = pg_reference(pack->storage, target->parameter);
	const struct pg_term *row = pg_effect_reference(pack->storage, mask);
	if (!a || !b || !row) return -1;
	pack->roots[pack->count++] = a;
	pack->roots[pack->count++] = row;
	pack->roots[pack->count++] = b;
	return 0;
}

int pg_effect_inference_pack(const struct pg_effect_inference *work, struct pg_graph *storage,
	size_t *equations, size_t *count, const struct pg_term *const **roots)
{
	if (!work->rows || work->failed || !storage || !equations || !count || !roots) return -1;
	/* row_sources includes disposable constant aliases; this is an upper bound. */
	size_t limit = SIZE_MAX / sizeof(struct pg_term *);
	if (work->row_sources.count > limit / 2) return -1;
	size_t capacity = 2 * work->row_sources.count;
	if (work->dependencies.count > (limit - capacity) / 3) return -1;
	capacity += 3 * work->dependencies.count;
	struct effect_pack pack = {.storage = storage};
	pack.roots = pg_alloc(storage, capacity * sizeof(*pack.roots));
	if (!pack.roots || pg_effect_inference_visit(work, &pack, pack_equation, pack_dependency)) return -1;
	*equations = pack.equations;
	*count = pack.count;
	*roots = pack.roots;
	return 0;
}

int pg_effect_inference_unpack(struct pg_effect_inference *work,
	size_t equations, size_t count, const struct pg_term *const *roots)
{
	if (!work->rows || work->failed || work->sealed || work->row_sources.count) goto fail;
	if (equations > count / 2 || (count - 2 * equations) % 3) goto fail;
	if (count && !roots) goto fail;
	for (size_t i = 0; i < count; ++i)
		if (!roots[i] || roots[i]->kind != PG_REFERENCE) goto fail;
	for (size_t i = 0; i < 2 * equations; i += 2) {
		const struct pg_object *parameter = roots[i]->as.reference;
		if (row_source(work, parameter)) goto fail;
		if (!pg_effect_equation_at(work, parameter, pg_effect_row_view(roots[i + 1]))) goto fail;
	}
	for (size_t i = 2 * equations; i < count; i += 3) {
		struct pg_effect_equation *source = row_source(work, roots[i]->as.reference);
		struct pg_effect_equation *target = row_source(work, roots[i + 2]->as.reference);
		if (pg_effect_dependency(work, source, pg_effect_row_view(roots[i + 1]), target)) goto fail;
	}
	return 0;
fail:
	work->failed = 1;
	return -1;
}

int pg_effect_contribution(struct pg_effect_inference *work,
	const struct pg_term *row_term, const struct pg_effect_row *mask,
	struct pg_effect_equation *target)
{
	if (!work->rows || work->sealed || work->failed || !row_term || !mask || !target) return -1;
	if (target->owner != work) return -1;
	if (row_term->kind != PG_REFERENCE) return -1;
	struct pg_effect_equation *source = row_source(work, row_term->as.reference);
	if (!source) {
		const struct pg_effect_row *row = pg_effect_row_view(row_term);
		if (!row) return -1;
		source = pg_effect_equation(work, row);
		if (!source || register_source(work, row_term->as.reference, source)) return -1;
	}
	return pg_effect_dependency(work, source, mask, target);
}

int pg_effect_dependency(struct pg_effect_inference *work,
	struct pg_effect_equation *source, const struct pg_effect_row *mask,
	struct pg_effect_equation *target)
{
	if (work->sealed || work->failed || !mask || !source || !target) return -1;
	if (source->owner != work || target->owner != work) return -1;
	uint64_t hash = ((uintptr_t)source ^ ((uintptr_t)target >> 3)) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)mask) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->dependencies, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_effect_dependency *edge = (void *)entry;
		if (edge->source == source && edge->target == target && edge->mask == mask) return 0;
	}
	struct pg_effect_dependency *edge = pg_alloc(&work->arena, sizeof(*edge));
	if (!edge) { work->failed = 1; return -1; }
	*edge = (struct pg_effect_dependency){.source = source, .target = target,
		.mask = mask, .next = source->outgoing};
	if (pg_index_insert(&work->dependencies, &edge->index, hash)) { work->failed = 1; return -1; }
	source->outgoing = edge;
	return 0;
}

int pg_effect_handler_dependencies(struct pg_effect_inference *work,
	struct pg_effect_equation *target, struct pg_effect_equation *input,
	const struct pg_effect_row *handled, struct pg_effect_equation *returned,
	size_t count, struct pg_effect_equation *const *clauses)
{
	if (!target || !input || !returned || !handled) return -1;
	if (target->owner != work || input->owner != work || returned->owner != work) return -1;
	if (count && !clauses) return -1;
	for (size_t i = 0; i < count; ++i)
		if (!clauses[i] || clauses[i]->owner != work) return -1;
	const struct pg_effect_row *empty = pg_effect_row(work->rows, 0, NULL);
	if (pg_effect_dependency(work, input, handled, target)) return -1;
	if (pg_effect_dependency(work, returned, empty, target)) return -1;
	for (size_t i = 0; i < count; ++i)
		if (pg_effect_dependency(work, clauses[i], empty, target)) return -1;
	return 0;
}

void pg_effect_inference_seal(struct pg_effect_inference *work)
{
	work->sealed = 1;
}

int pg_effect_inference_advance(struct pg_effect_inference *work, uint64_t budget)
{
	if (!work->rows || work->failed) return -1;
	if (!work->sealed) return 0;
	while (budget && (work->head || work->current)) {
		--budget;
		if (!work->current) {
			work->current = work->head;
			work->head = work->head->next;
			if (!work->head) work->tail = NULL;
			work->current->queued = 0;
			work->cursor = work->current->outgoing;
		}
		struct pg_effect_dependency *edge = work->cursor;
		if (edge) {
			const struct pg_effect_row *contribution = pg_effect_difference(work->rows, work->current->value, edge->mask);
			if (!contribution) { work->failed = 1; return -1; }
			if (pg_effect_subset(contribution, edge->target->value) != 1) {
				const struct pg_effect_row *value = pg_effect_union(work->rows, edge->target->value, contribution);
				if (!value) { work->failed = 1; return -1; }
				edge->target->value = value;
				enqueue(work, edge->target);
			}
			work->cursor = edge->next;
		}
		if (!work->cursor) work->current = NULL;
	}
	return !work->head && !work->current;
}

const struct pg_effect_row *pg_effect_inference_result(const struct pg_effect_inference *work,
	const struct pg_effect_equation *equation)
{
	if (!work->sealed || work->failed || work->head || work->current) return NULL;
	return equation && equation->owner == work ? equation->value : NULL;
}
