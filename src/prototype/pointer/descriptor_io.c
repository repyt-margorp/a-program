#include "descriptor_io.h"
#include "evidence.h"
#include "computation.h"
#include "identity.h"
#include <string.h>

static const struct pg_effect_row *object_row(const struct pg_object *object)
{
	const struct pg_term reference = {.kind = PG_REFERENCE, .as.reference = object};
	return pg_effect_row_view(&reference);
}

static const char *descriptor_name(void *context, const struct pg_object *object)
{
	(void)context;
	const struct pg_term *payload, *response;
	if (pg_operation_label_types(object, &payload, &response)) return "operation-label/v1";
	if (object_row(object)) return "effect-row/v1";
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
	const struct pg_term *payload, *response;
	if (pg_operation_label_types(object, &payload, &response)) {
		if (index >= 2) return 0;
		*child = index ? response : payload;
		return 1;
	}
	const struct pg_effect_row *row = object_row(object);
	if (!row) return -2;
	if (index >= pg_effect_count(row)) return 0;
	*child = pg_reference(scratch, pg_effect_label(row, index));
	return *child ? 1 : -1;
}

static const struct pg_object *descriptor_restore(void *context, struct pg_graph *graph,
	const char *name, size_t count, const struct pg_term *const *terms)
{
	(void)context;
	if (!strcmp(name, "operation-label/v1"))
		return count == 2 ? pg_operation_label_create(graph, terms[0], terms[1]) : NULL;
	if (strcmp(name, "effect-row/v1") || count > SIZE_MAX / sizeof(struct pg_object *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	const struct pg_object *result = NULL;
	if (!labels) goto done;
	for (size_t i = 0; i < count; ++i) {
		if (terms[i]->kind != PG_REFERENCE) goto done;
		labels[i] = terms[i]->as.reference;
	}
	const struct pg_effect_row *row = pg_effect_row(graph, count, labels);
	if (pg_effect_count(row) != count) goto done;
	const struct pg_term *reference = pg_effect_reference(graph, row);
	result = reference ? reference->as.reference : NULL;
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_graph_codec pg_builtin_graph_codec = {
	.name = descriptor_name, .resolve = descriptor_resolve,
	.child = descriptor_child, .restore = descriptor_restore
};
