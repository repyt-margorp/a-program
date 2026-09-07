#include "action.h"

const struct pg_evidence *pg_identity_context_extend(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *family, const struct pg_object *left,
	const struct pg_object *right, const struct pg_object *center)
{
	const struct pg_evidence *left_type = pg_prove_identity_endpoint_type(typing,
		classifiers, family, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *right_type = pg_prove_identity_endpoint_type(typing,
		classifiers, family, PG_IDENTITY_RIGHT_TYPE);
	if (!left_type || !right_type) return NULL;
	context = pg_prove_context_extension(typing, context, left, left_type);
	if (!context) return NULL;
	right_type = pg_prove_projection(typing, context, right_type);
	context = pg_prove_context_extension(typing, context, right, right_type);
	if (!context) return NULL;
	family = pg_prove_projection(typing, context, family);
	const struct pg_evidence *x0 = pg_prove_variable(typing, context, left);
	const struct pg_evidence *x1 = pg_prove_variable(typing, context, right);
	const struct pg_evidence *center_type = pg_prove_identity_instance(typing,
		classifiers, family, x0, x1);
	return pg_prove_context_extension(typing, context, center, center_type);
}

const struct pg_evidence *pg_context_restrict(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_dimension_map *face, size_t count,
	const struct pg_binding_face *const *bindings)
{
	if (dimensions->graph != typing->graph || !source || !face) return NULL;
	face = pg_dimension_face(dimensions, face);
	if (!face) return NULL;
	if (pg_evidence_judgement(source) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (count && !bindings) return NULL;
	size_t arity = 0;
	for (const struct pg_context *context = pg_evidence_context(source); context; context = context->parent) ++arity;
	if (arity != count || count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_evidence *result = NULL;
	if (count && !extensions) goto done;
	const struct pg_evidence *prefix = source;
	for (size_t i = count; i; --i) {
		extensions[i - 1] = prefix;
		prefix = pg_evidence_premise(prefix, 0);
	}
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	result = pg_prove_substitution(typing, prefix, empty, 0, NULL);
	for (size_t i = 0; result && i < count; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		if (bindings[i]) {
			if (&bindings[i]->variable != binder) { result = NULL; break; }
			const struct pg_binding_face *restricted = pg_binding_restrict(dimensions, bindings[i], face);
			if (!restricted) { result = NULL; break; }
			binder = &restricted->variable;
		}
		result = pg_prove_substitution_lift(typing, result, extensions[i], binder);
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}
