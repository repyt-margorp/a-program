#include "descriptor_io.h"
#include "evidence.h"
#include "computation.h"
#include "identity.h"
#include "iadt.h"
#include "symmetry.h"
#include <string.h>

static const struct pg_effect_row *object_row(const struct pg_object *object)
{
	const struct pg_term reference = {.kind = PG_REFERENCE, .as.reference = object};
	return pg_effect_row_view(&reference);
}

static const struct pg_handler_signature *object_handler(const struct pg_object *object)
{
	const struct pg_term reference = {.kind = PG_REFERENCE, .as.reference = object};
	return pg_handler_signature_view(&reference);
}

static const char *descriptor_name(void *context, const struct pg_object *object)
{
	(void)context;
	const struct pg_term *payload, *response;
	const struct pg_data_layout *layout;
	size_t position, arity;
	const size_t *axes;
	if (pg_symmetry_object_view(object, &arity, &axes)) return "symmetry/v1";
	const struct pg_clause_position *clauses;
	if (pg_computation_handler_view(object, &arity, &clauses)) return "computation-handler/v1";
	if (pg_data_layout_view(object)) return "data-layout/v1";
	if (pg_data_constructor_view(object, &layout, &position, &arity)) return "data-constructor/v1";
	if (pg_operation_label_types(object, &payload, &response)) return "operation-label/v1";
	if (object_row(object)) return "effect-row/v1";
	if (object_handler(object)) return "handler-signature/v1";
	static _Thread_local char buffer[64];
	const char *name = pg_classifier_name(object, buffer, sizeof(buffer));
	if (name) return name;
	name = pg_identity_name(object);
	return name ? name : pg_computation_name(object);
}

static const struct pg_object *descriptor_resolve(void *context, const char *name)
{
	const struct pg_object *object = pg_classifier_resolve(context, name);
	if (object) return object;
	object = pg_identity_resolve(name);
	return object ? object : pg_computation_resolve(name);
}

static int descriptor_child(void *context, struct pg_graph *scratch,
	const struct pg_object *object, size_t index, const struct pg_term **child)
{
	(void)context;
	size_t clause_count;
	const size_t *axes;
	if (pg_symmetry_object_view(object, &clause_count, &axes)) return 0;
	const struct pg_clause_position *clauses;
	if (pg_computation_handler_view(object, &clause_count, &clauses)) {
		if (index >= clause_count) return 0;
		*child = pg_reference(scratch, clauses[index].label);
		return *child ? 1 : -1;
	}
	if (pg_data_layout_view(object)) return 0;
	const struct pg_data_layout *layout;
	size_t position, arity;
	if (pg_data_constructor_view(object, &layout, &position, &arity)) {
		if (index) return 0;
		*child = pg_reference(scratch, pg_data_matcher(layout));
		return *child ? 1 : -1;
	}
	const struct pg_term *payload, *response;
	if (pg_operation_label_types(object, &payload, &response)) {
		if (index >= 2) return 0;
		*child = index ? response : payload;
		return 1;
	}
	const struct pg_effect_row *row = object_row(object);
	const struct pg_handler_signature *handler = object_handler(object);
	if (!row && !handler) return -2;
	if (index >= (row ? pg_effect_count(row) : pg_handler_signature_count(handler))) return 0;
	*child = pg_reference(scratch, row ? pg_effect_label(row, index) : pg_handler_signature_label(handler, index));
	return *child ? 1 : -1;
}

static int descriptor_scalar(void *context, const struct pg_object *object, size_t index, uint64_t *value)
{
	(void)context;
	size_t clause_count;
	const size_t *axes;
	if (pg_symmetry_object_view(object, &clause_count, &axes)) {
		if (index >= clause_count) return 0;
		*value = axes[index];
		return 1;
	}
	const struct pg_clause_position *clauses;
	if (pg_computation_handler_view(object, &clause_count, &clauses)) {
		if (index >= clause_count) return 0;
		*value = clauses[index].position;
		return 1;
	}
	const struct pg_data_layout *layout = pg_data_layout_view(object);
	size_t position, arity;
	if (layout) {
		if (index >= pg_data_layout_count(layout)) return 0;
		if (!pg_data_constructor_view(pg_data_constructor(layout, index), &layout, &position, &arity)) return -1;
		*value = arity;
		return 1;
	}
	if (!pg_data_constructor_view(object, &layout, &position, &arity) || index) return 0;
	*value = position;
	return 1;
}

static const struct pg_object *descriptor_restore(void *context, struct pg_graph *graph,
	const char *name, size_t count, const struct pg_term *const *terms,
	size_t scalar_count, const uint64_t *scalars)
{
	(void)context;
	if (!strcmp(name, "symmetry/v1")) {
		if (count || scalar_count > SIZE_MAX / sizeof(size_t)) return NULL;
		struct pg_graph temporary = {0};
		size_t *axes = pg_alloc(&temporary, scalar_count * sizeof(*axes));
		const struct pg_object *result = NULL;
		if (!axes) goto symmetry_done;
		for (size_t i = 0; i < scalar_count; ++i) {
			if ((uint64_t)(size_t)scalars[i] != scalars[i]) goto symmetry_done;
			axes[i] = (size_t)scalars[i];
		}
		result = pg_symmetry_restore(graph, scalar_count, axes);
	symmetry_done:
		pg_graph_destroy(&temporary);
		return result;
	}
	if (!strcmp(name, "computation-handler/v1")) {
		if (!count || count != scalar_count || count > SIZE_MAX / sizeof(struct pg_clause_position)) return NULL;
		struct pg_graph temporary = {0};
		struct pg_clause_position *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
		const struct pg_object *result = NULL;
		if (!clauses) goto clauses_done;
		for (size_t i = 0; i < count; ++i) {
			if (terms[i]->kind != PG_REFERENCE || scalars[i] >= count) goto clauses_done;
			clauses[i] = (struct pg_clause_position){terms[i]->as.reference, (size_t)scalars[i]};
		}
		result = pg_computation_handler_restore(graph, count, clauses);
	clauses_done:
		pg_graph_destroy(&temporary);
		return result;
	}
	if (!strcmp(name, "data-layout/v1")) {
		if (count || scalar_count > SIZE_MAX / sizeof(size_t)) return NULL;
		struct pg_graph temporary = {0};
		size_t *arities = pg_alloc(&temporary, scalar_count * sizeof(*arities));
		if (!arities) return NULL;
		const struct pg_data_layout *layout = NULL;
		for (size_t i = 0; i < scalar_count; ++i) {
			arities[i] = (size_t)scalars[i];
			if ((uint64_t)arities[i] != scalars[i]) goto layout_done;
		}
		layout = pg_data_layout(graph, scalar_count, arities);
layout_done:
		pg_graph_destroy(&temporary);
		return pg_data_matcher(layout);
	}
	if (!strcmp(name, "data-constructor/v1")) {
		if (count != 1 || scalar_count != 1 || terms[0]->kind != PG_REFERENCE) return NULL;
		if ((uint64_t)(size_t)scalars[0] != scalars[0]) return NULL;
		return pg_data_constructor(pg_data_layout_view(terms[0]->as.reference), (size_t)scalars[0]);
	}
	if (scalar_count) return NULL;
	if (!strcmp(name, "operation-label/v1"))
		return count == 2 ? pg_operation_label_create(graph, terms[0], terms[1]) : NULL;
	int handler = !strcmp(name, "handler-signature/v1");
	if ((!handler && strcmp(name, "effect-row/v1")) || count > SIZE_MAX / sizeof(struct pg_object *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	const struct pg_object *result = NULL;
	if (!labels) goto done;
	for (size_t i = 0; i < count; ++i) {
		if (terms[i]->kind != PG_REFERENCE) goto done;
		labels[i] = terms[i]->as.reference;
	}
	const struct pg_term *reference;
	if (handler) reference = pg_handler_signature_reference(graph, pg_handler_signature(graph, count, labels));
	else {
		const struct pg_effect_row *row = pg_effect_row(graph, count, labels);
		if (pg_effect_count(row) != count) goto done;
		reference = pg_effect_reference(graph, row);
	}
	result = reference ? reference->as.reference : NULL;
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_graph_codec pg_builtin_graph_codec = {
	.name = descriptor_name, .resolve = descriptor_resolve,
	.child = descriptor_child, .scalar = descriptor_scalar, .restore = descriptor_restore
};
