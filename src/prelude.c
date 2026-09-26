#include "prelude.h"
#include "action.h"

/* Fixed library conversions share the caller's normalizer and retain its
 * checked certificate; they do not evaluate arbitrary source programs. */
static const struct pg_evidence *convert(struct pg_typing *typing, struct pg_whnf_work *work,
	const struct pg_evidence *term, const struct pg_evidence *type)
{
	if (!term || !type) return NULL;
	struct pg_conversion conversion;
	if (pg_conversion_init(&conversion, work, pg_evidence_classifier(term), pg_evidence_subject(type)->core) != 0)
		return NULL;
	const struct pg_evidence *result = NULL;
	if (pg_conversion_advance(&conversion, UINT64_MAX) == PG_CONVERSION_EQUAL)
		result = pg_prove_conversion(typing, term, type, pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	return result;
}

static const struct pg_evidence *congruence_function(struct pg_typing *typing,
	struct pg_whnf_work *work,
	const struct pg_evidence *empty, const struct pg_evidence *context,
	const struct pg_object *a, const struct pg_object *b)
{
	const struct pg_object *x = pg_binder(typing->graph), *f = pg_binder(typing->graph);
	const struct pg_evidence *domain = pg_prove_value_type(typing, pg_prove_variable(typing, context, a));
	const struct pg_evidence *inner = pg_prove_context_extension(typing, context, x, domain);
	const struct pg_evidence *codomain = pg_prove_computation_type(typing, PG_TOTALITY_TOTAL,
		pg_effect_row(typing->graph, 0, NULL),
		pg_prove_value_type(typing, pg_prove_variable(typing, inner, b)));
	const struct pg_evidence *pi = pg_prove_pi(typing, inner, codomain);
	context = pg_prove_context_extension(typing, context, f, pg_prove_thunk_type(typing, pi));
	const struct pg_evidence *function = pg_prove_force(typing, pg_prove_variable(typing, context, f));
	pi = pg_prove_projection(typing, context, pi);
	const struct pg_evidence *action = pg_prove_reflexivity(typing, pi, function);
	const struct pg_evidence *expanded = pg_identity_pi_type(typing, context, pi, function, function,
		pg_binder(typing->graph), pg_binder(typing->graph), pg_binder(typing->graph));
	return pg_prove_abstract(typing, empty, context, convert(typing, work, action, expanded));
}

static const struct pg_evidence *path_function(struct pg_typing *typing,
	struct pg_whnf_work *work,
	const struct pg_evidence *empty, const struct pg_evidence *a_context,
	const struct pg_object *a, uint64_t level, int compose)
{
	const struct pg_evidence *context = a_context;
	const struct pg_object *bindings[5];
	const struct pg_evidence *images[6];
	/* Images are A,x,y,p for symmetry, or A,x,y,z,p,q for composition. */
	size_t points = compose ? 3 : 2, count = compose ? 5 : 3;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *type = pg_prove_value_type(typing, pg_prove_variable(typing, context, a));
		if (i >= points) type = pg_prove_identity_type(typing, type,
			pg_prove_variable(typing, context, bindings[i - points]),
			pg_prove_variable(typing, context, bindings[i - points + 1]));
		bindings[i] = pg_binder(typing->graph);
		context = pg_prove_context_extension(typing, context, bindings[i], type);
		if (!context) return NULL;
	}
	images[0] = pg_prove_variable(typing, context, a);
	for (size_t i = 0; i < count; ++i) images[i + 1] = pg_prove_variable(typing, context, bindings[i]);
	const struct pg_evidence *prefix = pg_prove_substitution(typing, context, context, count + 1, images);
	const struct pg_evidence *base = pg_prove_value_type(typing, images[0]);
	const struct pg_object *t = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, context, t, base);
	const struct pg_evidence *v = pg_prove_variable(typing, source, t);
	const struct pg_evidence *x = pg_prove_projection(typing, source, images[1]);
	const struct pg_evidence *family = pg_prove_type_value(typing, pg_prove_identity_type(typing,
		pg_prove_projection(typing, source, base), compose ? x : v, compose ? v : x));
	const struct pg_evidence *kind = pg_prove_classifier(typing, source, family);
	const struct pg_evidence *left = pg_prove_substitution_pair(typing, prefix, source, images[compose ? 2 : 1]);
	const struct pg_evidence *right = pg_prove_substitution_pair(typing, prefix, source, images[compose ? 3 : 2]);
	const struct pg_evidence *path = images[compose ? 5 : 3];
	const struct pg_evidence *action = pg_prove_family_action(typing, kind, family, left, right, 1, &path);
	const struct pg_evidence *target = pg_prove_identity_type(typing,
		pg_prove_universe(typing, context, level),
		pg_prove_reindex(typing, left, family), pg_prove_reindex(typing, right, family));
	const struct pg_evidence *checked = convert(typing, work, action, target);
	const struct pg_evidence *input = compose ? images[4] : pg_prove_reflexivity(typing, base, images[1]);
	const struct pg_evidence *result = pg_prove_identity_transport(typing, checked, input, PG_IDENTITY_RIGHT);
	return pg_prove_abstract(typing, empty, context, pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, result));
}

const struct pg_identity_library *pg_identity_library(struct pg_typing *typing,
	struct pg_whnf_work *normalization, uint64_t level)
{
	if (!typing) return NULL;
	if (!normalization || normalization->graph != typing->graph) return NULL;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, empty, level);
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
	const struct pg_evidence *eq_body = pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, pg_prove_type_value(typing, identity));
	type = pg_prove_value_type(typing, pg_prove_variable(typing, x_context, a));
	const struct pg_evidence *refl_body = pg_prove_return_contract(typing, PG_TOTALITY_TOTAL,
		pg_prove_reflexivity(typing, type, pg_prove_variable(typing, x_context, x)));
	struct pg_identity_library *library = pg_alloc(typing->graph, sizeof(*library));
	if (!library) return NULL;
	library->equality = pg_prove_abstract(typing, empty, y_context, eq_body);
	library->reflexivity = pg_prove_abstract(typing, empty, x_context, refl_body);
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
	const struct pg_evidence *instance = pg_prove_identity_instance(typing,
		pg_prove_variable(typing, endpoints, r), pg_prove_variable(typing, endpoints, x),
		pg_prove_variable(typing, endpoints, y));
	library->instance = pg_prove_abstract(typing, empty, endpoints,
		pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, pg_prove_type_value(typing, instance)));
	if (!library->instance) return NULL;
	for (enum pg_identity_direction direction = PG_IDENTITY_RIGHT; direction <= PG_IDENTITY_LEFT; ++direction) {
		const struct pg_object *endpoint = direction == PG_IDENTITY_RIGHT ? a : b;
		const struct pg_evidence *context = pg_prove_context_extension(typing, r_context, x,
			pg_prove_variable(typing, r_context, endpoint));
		const struct pg_evidence *family = pg_prove_variable(typing, context, r);
		const struct pg_evidence *value = pg_prove_variable(typing, context, x);
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, family, value, direction);
		const struct pg_evidence *lift = pg_prove_identity_lift(typing, family, value, direction);
		library->transport[direction] = pg_prove_abstract(typing, empty, context,
			pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, transport));
		library->lifting[direction] = pg_prove_abstract(typing, empty, context,
			pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, lift));
		if (!library->transport[direction] || !library->lifting[direction]) return NULL;
	}
	library->symmetry = path_function(typing, normalization, empty, a_context, a, level, 0);
	library->composition = path_function(typing, normalization, empty, a_context, a, level, 1);
	library->congruence = congruence_function(typing, normalization, empty, b_context, a, b);
	return library->symmetry && library->composition && library->congruence ? library : NULL;
}
