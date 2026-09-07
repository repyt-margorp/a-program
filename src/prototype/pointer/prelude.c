#include "prelude.h"

const struct pg_identity_library *pg_identity_library(struct pg_typing *typing,
	struct pg_classifiers *classifiers, uint64_t level)
{
	if (!typing || !classifiers || typing->graph != classifiers->graph) return NULL;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, level);
	if (!universe) return NULL;
	const struct pg_object *a = pg_binder(typing->graph), *b = pg_binder(typing->graph);
	const struct pg_object *x = pg_binder(typing->graph), *y = pg_binder(typing->graph);
	const struct pg_object *r = pg_binder(typing->graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(typing, empty, a, universe);
	const struct pg_evidence *x_context = pg_prove_context_extension(typing, a_context, x,
		pg_prove_variable(typing, a_context, a));
	const struct pg_evidence *y_context = pg_prove_context_extension(typing, x_context, y,
		pg_prove_variable(typing, x_context, a));
	if (!y_context) return NULL;
	const struct pg_evidence *type = pg_prove_value_type(typing, pg_prove_variable(typing, y_context, a));
	const struct pg_evidence *identity = pg_prove_identity_type(typing, type,
		pg_prove_variable(typing, y_context, x), pg_prove_variable(typing, y_context, y));
	const struct pg_evidence *eq_body = pg_prove_return(typing, classifiers, pg_prove_type_value(typing, identity));
	type = pg_prove_value_type(typing, pg_prove_variable(typing, x_context, a));
	const struct pg_evidence *refl_body = pg_prove_return(typing, classifiers,
		pg_prove_reflexivity(typing, type, pg_prove_variable(typing, x_context, x)));
	struct pg_identity_library *library = pg_alloc(typing->graph, sizeof(*library));
	if (!library) return NULL;
	library->equality = pg_prove_abstract(typing, classifiers, empty, y_context, eq_body);
	library->reflexivity = pg_prove_abstract(typing, classifiers, empty, x_context, refl_body);
	if (!library->equality || !library->reflexivity) return NULL;
	const struct pg_evidence *b_context = pg_prove_context_extension(typing, a_context, b,
		pg_prove_projection(typing, a_context, universe));
	const struct pg_evidence *relation = pg_prove_identity_type(typing,
		pg_prove_projection(typing, b_context, universe),
		pg_prove_variable(typing, b_context, a), pg_prove_variable(typing, b_context, b));
	const struct pg_evidence *r_context = pg_prove_context_extension(typing, b_context, r, relation);
	if (!r_context) return NULL;
	const struct pg_evidence *left_context = pg_prove_context_extension(typing, r_context, x,
		pg_prove_variable(typing, r_context, a));
	const struct pg_evidence *endpoints = pg_prove_context_extension(typing, left_context, y,
		pg_prove_variable(typing, left_context, b));
	const struct pg_evidence *instance = pg_prove_identity_instance(typing, classifiers,
		pg_prove_variable(typing, endpoints, r), pg_prove_variable(typing, endpoints, x),
		pg_prove_variable(typing, endpoints, y));
	library->instance = pg_prove_abstract(typing, classifiers, empty, endpoints,
		pg_prove_return(typing, classifiers, pg_prove_type_value(typing, instance)));
	if (!library->instance) return NULL;
	for (enum pg_identity_direction direction = PG_IDENTITY_RIGHT; direction <= PG_IDENTITY_LEFT; ++direction) {
		const struct pg_object *endpoint = direction == PG_IDENTITY_RIGHT ? a : b;
		const struct pg_evidence *context = pg_prove_context_extension(typing, r_context, x,
			pg_prove_variable(typing, r_context, endpoint));
		const struct pg_evidence *family = pg_prove_variable(typing, context, r);
		const struct pg_evidence *value = pg_prove_variable(typing, context, x);
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers, family, value, direction);
		const struct pg_evidence *lift = pg_prove_identity_lift(typing, classifiers, family, value, direction);
		library->transport[direction] = pg_prove_abstract(typing, classifiers, empty, context,
			pg_prove_return(typing, classifiers, transport));
		library->lifting[direction] = pg_prove_abstract(typing, classifiers, empty, context,
			pg_prove_return(typing, classifiers, lift));
		if (!library->transport[direction] || !library->lifting[direction]) return NULL;
	}
	return library;
}
