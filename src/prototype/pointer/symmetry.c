#include "symmetry_internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

static const struct pg_object_class symmetry_class = {"dimension-permutation"};
const struct symmetry_entry *pg_symmetry_owner(const struct pg_object *reference)
{
	if (!reference || reference->owner != &symmetry_class) return NULL;
	const char *object = (const char *)reference;
	return (const struct symmetry_entry *)(object - offsetof(struct pg_object_entry, object));
}

static const struct symmetry_entry *owner(const struct pg_term *term)
{
	return term->kind == PG_REFERENCE ? pg_symmetry_owner(term->as.reference) : NULL;
}

int pg_symmetry_object_view(const struct pg_object *object, size_t *dimension, const size_t **axes)
{
	const struct symmetry_entry *entry = pg_symmetry_owner(object);
	if (!entry) return 0;
	*dimension = entry->dimension;
	*axes = entry->axes;
	return 1;
}

static const char name_prefix[] = "kernel/symmetry/v1/";

const char *pg_symmetry_name(const struct pg_object *object, char *buffer, size_t capacity)
{
	const struct symmetry_entry *entry = pg_symmetry_owner(object);
	if (!entry || !buffer || capacity < sizeof(name_prefix)) return NULL;
	memcpy(buffer, name_prefix, sizeof(name_prefix));
	size_t used = sizeof(name_prefix) - 1;
	for (size_t i = 0; i < entry->dimension; ++i) {
		int length = snprintf(buffer + used, capacity - used, "%s%zu", i ? "," : "", entry->axes[i]);
		if (length < 0 || (size_t)length >= capacity - used) return NULL;
		used += (size_t)length;
	}
	return buffer;
}

int pg_symmetry_view(const struct pg_term *term, size_t *dimension,
	const size_t **axes, const struct pg_term **argument)
{
	if (!term || !dimension || !axes || !argument) return 0;
	if (term->kind != PG_APPLICATION) return 0;
	const struct symmetry_entry *entry = owner(term->as.application.function);
	if (!entry) return 0;
	*dimension = entry->dimension;
	*axes = entry->axes;
	*argument = term->as.application.argument;
	return 1;
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
	entry->fixed_prefix = 0;
	for (size_t i = 0; i < dimension; ++i) {
		copy[i] = axes[i];
		if (entry->fixed_prefix == i && axes[i] == i) ++entry->fixed_prefix;
	}
	if (pg_index_insert(&graph->objects, &entry->base.index, hash) != 0) return NULL;
	return pg_reference(graph, &entry->base.object);
}

static const struct pg_term *validated_operator(struct pg_graph *graph, size_t dimension, const size_t *axes)
{
	if (!graph || dimension > SIZE_MAX / sizeof(size_t) || (dimension && !axes)) return NULL;
	unsigned char *seen = calloc(dimension ? dimension : 1, 1);
	if (!seen) return NULL;
	const struct pg_term *result = NULL;
	for (size_t i = 0; i < dimension; ++i) {
		if (axes[i] >= dimension || seen[axes[i]]) goto done;
		seen[axes[i]] = 1;
	}
	result = operator(graph, dimension, axes);
done:
	free(seen);
	return result;
}

const struct pg_object *pg_symmetry_restore(struct pg_graph *graph, size_t dimension, const size_t *axes)
{
	const struct pg_term *term = validated_operator(graph, dimension, axes);
	return term ? term->as.reference : NULL;
}

const struct pg_object *pg_symmetry_resolve(struct pg_graph *graph, const char *name)
{
	if (!graph || !name || strncmp(name, name_prefix, sizeof(name_prefix) - 1)) return NULL;
	const char *digits = name + sizeof(name_prefix) - 1;
	size_t dimension = *digits ? 1 : 0;
	for (const char *p = digits; *p; ++p) if (*p == ',') ++dimension;
	if (dimension > SIZE_MAX / sizeof(size_t)) return NULL;
	size_t *axes = malloc((dimension ? dimension : 1) * sizeof(*axes));
	const struct pg_object *result = NULL;
	if (!axes) goto done;
	for (size_t i = 0; i < dimension; ++i) {
		if (*digits < '0' || *digits > '9') goto done;
		char *end;
		errno = 0;
		uintmax_t axis = strtoumax(digits, &end, 10);
		if (errno == ERANGE || axis > SIZE_MAX) goto done;
		if (*digits == '0' && end != digits + 1) goto done;
		if (*end != (i + 1 < dimension ? ',' : '\0')) goto done;
		axes[i] = (size_t)axis;
		digits = i + 1 < dimension ? end + 1 : end;
	}
	result = pg_symmetry_restore(graph, dimension, axes);
done:
	free(axes);
	return result;
}

const struct pg_term *pg_symmetry(struct pg_graph *graph,
	const struct pg_dimension_map *permutation, const struct pg_term *term)
{
	if (!term || !permutation || permutation->source != permutation->target) return NULL;
	size_t n = permutation->source;
	if (n > SIZE_MAX / sizeof(size_t) || (n && !permutation->coordinates)) return NULL;
	size_t *axes = malloc((n ? n : 1) * sizeof(*axes));
	if (!axes) return NULL;
	const struct pg_term *result = NULL;
	for (size_t i = 0; i < n; ++i) {
		struct pg_coordinate c = permutation->coordinates[i];
		if (c.kind != PG_AXIS) goto done;
		axes[i] = c.axis;
	}
	const struct pg_term *head = validated_operator(graph, n, axes);
	if (head) result = pg_application(graph, head, term);
done:
	free(axes);
	return result;
}

static size_t extended_axis(const struct symmetry_entry *entry, size_t dimension, size_t axis)
{
	size_t prefix = dimension - entry->dimension;
	return axis < prefix ? axis : prefix + entry->axes[axis - prefix];
}

static int composition_poll(void *state)
{
	struct composition_work *work = state;
	if (work->position == work->dimension) return 1;
	size_t i = work->position++;
	work->axes[i] = extended_axis(work->inner, work->dimension,
		extended_axis(work->outer, work->dimension, i));
	return 0;
}

static int composition_resume(struct pg_eval *machine, void *state)
{
	struct composition_work *work = state;
	const struct pg_term *composed = operator(machine->output, work->dimension, work->axes);
	const struct pg_term *result = pg_application(machine->output, composed, work->argument);
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1);
}

static void composition_destroy(void *state)
{
	(void)state; /* Work is owned by the evaluator's temporary arena. */
}

const struct pg_eval_work_operation pg_symmetry_composition_operation = {
	composition_poll, composition_resume, composition_destroy
};

static int symmetry_answer(struct pg_eval *machine, const struct pg_term *term, const void *state);
static const struct pg_eval_continuation symmetry_answer_continuation = {
	"symmetry/symmetry_answer/v1", symmetry_answer
};

static int symmetry_answer(struct pg_eval *machine, const struct pg_term *term, const void *state)
{
	(void)state;
	const struct symmetry_entry *outer = owner(machine->current.term);
	if (term->kind != PG_APPLICATION) return 1;
	const struct symmetry_entry *inner = owner(term->as.application.function);
	if (!inner) return 1;
	size_t n = outer->dimension > inner->dimension ? outer->dimension : inner->dimension;
	struct composition_work *work = pg_alloc(&machine->temporary, sizeof(*work));
	if (!work) return -1;
	work->outer = outer;
	work->inner = inner;
	work->argument = term->as.application.argument;
	work->position = 0;
	work->dimension = n;
	work->axes = pg_alloc(&machine->temporary, n * sizeof(*work->axes));
	if (!work->axes) return -1;
	return pg_eval_defer(machine, &pg_symmetry_composition_operation, work);
}

struct prefix_work {
	const struct symmetry_entry *outer;
	struct pg_closure argument;
	size_t *axes;
	size_t position;
};

static int prefix_poll(void *state)
{
	struct prefix_work *work = state;
	size_t prefix = work->outer->fixed_prefix;
	if (work->position == work->outer->dimension - prefix) return 1;
	size_t i = work->position++;
	work->axes[i] = work->outer->axes[prefix + i] - prefix;
	return 0;
}

static int prefix_resume(struct pg_eval *machine, void *state)
{
	struct prefix_work *work = state;
	const struct pg_term *reduced = operator(machine->output,
		work->outer->dimension - work->outer->fixed_prefix, work->axes);
	if (!reduced) return -1;
	return pg_eval_apply(machine, (struct pg_closure){reduced, NULL}, work->argument, 1);
}

static const struct pg_eval_work_operation prefix_operation = {
	prefix_poll, prefix_resume, composition_destroy
};

const struct pg_eval_continuation *pg_symmetry_continuation_resolve(const char *name)
{
	static const struct pg_eval_continuation *const entries[] = {
		&symmetry_answer_continuation
	};
	return pg_eval_continuation_find(name, sizeof(entries) / sizeof(*entries), entries);
}

int pg_symmetry_dispatch(struct pg_eval *machine)
{
	const struct symmetry_entry *outer = owner(machine->current.term);
	if (!outer) return 1;
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	if (!argument) return 1;
	if (outer->fixed_prefix == outer->dimension) return pg_eval_enter(machine, *argument, 1);
	if (outer->fixed_prefix) {
		struct prefix_work *work = pg_alloc(&machine->temporary, sizeof(*work));
		if (!work) return -1;
		work->outer = outer;
		work->argument = *argument;
		work->position = 0;
		work->axes = pg_alloc(&machine->temporary,
			(outer->dimension - outer->fixed_prefix) * sizeof(*work->axes));
		if (!work->axes) return -1;
		return pg_eval_defer(machine, &prefix_operation, work);
	}
	return pg_eval_demand(machine, 0, &symmetry_answer_continuation, NULL);
}
