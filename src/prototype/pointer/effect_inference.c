#include "effect_inference.h"

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
	const struct pg_term *term;
	struct pg_effect_equation *equation;
};

static struct pg_effect_equation *row_source(const struct pg_effect_inference *work,
	const struct pg_term *term)
{
	uint64_t hash = ((uintptr_t)term >> 3) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->row_sources, hash); entry; entry = entry->next) {
		struct row_source *source = (void *)entry;
		if (entry->hash == hash && source->term == term) return source->equation;
	}
	return NULL;
}

static int register_source(struct pg_effect_inference *work, const struct pg_term *term,
	struct pg_effect_equation *equation)
{
	struct row_source *source = pg_alloc(&work->arena, sizeof(*source));
	if (!source) { work->failed = 1; return -1; }
	*source = (struct row_source){.term = term, .equation = equation};
	uint64_t hash = ((uintptr_t)term >> 3) * UINT64_C(1099511628211);
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
	struct pg_effect_equation *equation = pg_alloc(&work->arena, sizeof(*equation));
	if (!equation) { work->failed = 1; return NULL; }
	const struct pg_object *parameter = pg_binder(work->rows);
	if (!parameter) { work->failed = 1; return NULL; }
	*equation = (struct pg_effect_equation){.owner = work, .parameter = parameter, .seed = seed, .value = seed};
	const struct pg_term *term = pg_reference(work->rows, parameter);
	if (!term) { work->failed = 1; return NULL; }
	if (register_source(work, term, equation)) return NULL;
	enqueue(work, equation);
	return equation;
}

const struct pg_object *pg_effect_equation_parameter(const struct pg_effect_inference *work,
	const struct pg_effect_equation *equation)
{
	return equation && equation->owner == work ? equation->parameter : NULL;
}

int pg_effect_contribution(struct pg_effect_inference *work,
	const struct pg_term *row_term, const struct pg_effect_row *mask,
	struct pg_effect_equation *target)
{
	if (!work->rows || work->sealed || work->failed || !row_term || !mask || !target) return -1;
	if (target->owner != work) return -1;
	struct pg_effect_equation *source = row_source(work, row_term);
	if (!source) {
		const struct pg_effect_row *row = pg_effect_row_view(row_term);
		if (!row) return -1;
		source = pg_effect_equation(work, row);
		if (!source || register_source(work, row_term, source)) return -1;
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
