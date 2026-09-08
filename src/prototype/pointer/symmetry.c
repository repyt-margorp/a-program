#include "symmetry.h"

#include <stdlib.h>

static const struct pg_object_class symmetry_class = {"dimension-permutation"};
struct symmetry_entry {
	struct pg_object_entry base;
	size_t dimension;
	const size_t *axes;
	int identity;
};

static const struct symmetry_entry *owner(const struct pg_term *term)
{
	if (term->kind != PG_REFERENCE || term->as.reference->owner != &symmetry_class) return NULL;
	const char *object = (const char *)term->as.reference;
	return (const struct symmetry_entry *)(object - offsetof(struct pg_object_entry, object));
}

static const struct pg_term *operator(struct pg_graph *graph, size_t dimension, const size_t *axes)
{
	if (!graph->objects.capacity && pg_index_init(&graph->objects) != 0) return NULL;
	uint64_t hash = UINT64_C(1469598103934665603) ^ dimension;
	for (size_t i = 0; i < dimension; ++i) hash = (hash ^ axes[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		const struct pg_object_entry *base = (const struct pg_object_entry *)p;
		if (base->object.owner != &symmetry_class) continue;
		const struct symmetry_entry *entry = (const struct symmetry_entry *)base;
		if (entry->dimension != dimension) continue;
		size_t i = 0;
		while (i < dimension && entry->axes[i] == axes[i]) ++i;
		if (i == dimension) return pg_reference(graph, &base->object);
	}
	if (dimension > SIZE_MAX / sizeof(size_t)) return NULL;
	struct symmetry_entry *entry = pg_alloc(graph, sizeof(*entry));
	size_t *copy = pg_alloc(graph, dimension * sizeof(*copy));
	if (!entry || !copy) return NULL;
	entry->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &symmetry_class};
	entry->dimension = dimension;
	entry->axes = copy;
	entry->identity = 1;
	for (size_t i = 0; i < dimension; ++i) {
		copy[i] = axes[i];
		if (axes[i] != i) entry->identity = 0;
	}
	if (pg_index_insert(&graph->objects, &entry->base.index, hash) != 0) return NULL;
	return pg_reference(graph, &entry->base.object);
}

const struct pg_term *pg_symmetry(struct pg_graph *graph,
	const struct pg_dimension_map *permutation, const struct pg_term *term)
{
	if (!term || !permutation || permutation->source != permutation->target) return NULL;
	size_t n = permutation->source;
	if (n > SIZE_MAX / sizeof(size_t) || (n && !permutation->coordinates)) return NULL;
	size_t *axes = malloc((n ? n : 1) * sizeof(*axes));
	unsigned char *seen = calloc(n ? n : 1, 1);
	if (!axes || !seen) { free(axes); free(seen); return NULL; }
	const struct pg_term *result = NULL;
	for (size_t i = 0; i < n; ++i) {
		struct pg_coordinate c = permutation->coordinates[i];
		if (c.kind != PG_AXIS || c.axis >= n || seen[c.axis]) goto done;
		seen[c.axis] = 1;
		axes[i] = c.axis;
	}
	result = pg_application(graph, operator(graph, n, axes), term);
done:
	free(axes);
	free(seen);
	return result;
}

struct composition_work {
	const struct symmetry_entry *outer, *inner;
	const struct pg_term *argument;
	size_t *axes;
	size_t position;
};

static int composition_poll(void *state)
{
	struct composition_work *work = state;
	if (work->position == work->outer->dimension) return 1;
	size_t i = work->position++;
	work->axes[i] = work->inner->axes[work->outer->axes[i]];
	return 0;
}

static int composition_resume(struct pg_eval *machine, void *state)
{
	struct composition_work *work = state;
	const struct pg_term *composed = operator(machine->output, work->outer->dimension, work->axes);
	const struct pg_term *result = pg_application(machine->output, composed, work->argument);
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1);
}

static void composition_destroy(void *state)
{
	(void)state; /* Work is owned by the evaluator's temporary arena. */
}

static int symmetry_answer(struct pg_eval *machine, const struct pg_term *term, const void *state)
{
	const struct symmetry_entry *outer = state;
	if (term->kind != PG_APPLICATION) return 1;
	const struct symmetry_entry *inner = owner(term->as.application.function);
	if (!inner || inner->dimension != outer->dimension) return 1;
	size_t n = outer->dimension;
	struct composition_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	work->outer = outer;
	work->inner = inner;
	work->argument = term->as.application.argument;
	work->position = 0;
	work->axes = pg_alloc(&machine->temporary, n * sizeof(*work->axes));
	if (!work->axes) return -1;
	return pg_eval_defer(machine, work, composition_poll, composition_resume, composition_destroy);
}

int pg_symmetry_dispatch(struct pg_eval *machine)
{
	const struct symmetry_entry *outer = owner(machine->current.term);
	if (!outer) return 1;
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	if (!argument) return 1;
	if (outer->identity) return pg_eval_enter(machine, *argument, 1);
	return pg_eval_demand(machine, 0, symmetry_answer, outer);
}
