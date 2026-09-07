#include "iadt.h"
#include "computation.h"
#include "conversion.h"
#include "identity.h"
#include "action.h"

#include <assert.h>
#include <stdio.h>

static void check(struct pg_whnf_work *work, const struct pg_term *term, const struct pg_term *expected)
{
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, term);
	assert(job && pg_whnf_request(work, &pg_pure_policy, term) == job);
	while (pg_whnf_advance(job, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(job) < 100000);
	assert(pg_whnf_status(job) == PG_EVAL_WHNF && pg_whnf_result(job) == expected);
	uint64_t steps = pg_whnf_steps(job);
	assert(pg_whnf_advance(job, 100) == PG_EVAL_WHNF && pg_whnf_steps(job) == steps);
}

static void schemas(struct pg_graph *graph)
{
	struct pg_typing typing, foreign;
	struct pg_classifiers classifiers;
	struct pg_dimensions dimensions;
	assert(pg_typing_init(&typing, graph) == 0 && pg_typing_init(&foreign, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph), *p = pg_binder(graph);
	const struct pg_evidence *parameters = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *av = pg_prove_variable(&typing, parameters, a);
	const struct pg_evidence *first = pg_prove_context_extension(&typing, parameters, x, av);
	const struct pg_evidence *xv = pg_prove_variable(&typing, first, x);
	const struct pg_evidence *at = pg_prove_value_type(&typing, pg_prove_variable(&typing, first, a));
	const struct pg_evidence *id = pg_prove_identity_type(&typing, at, xv, xv);
	const struct pg_evidence *fields = pg_prove_context_extension(&typing, first, p, id);
	const struct pg_evidence *contexts[] = {parameters, first, fields};
	const struct pg_data_schema *schema = pg_data_schema(&typing, parameters, 3, contexts);
	assert(schema);
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	const struct pg_object *ctor = pg_data_constructor(layout, 2);
	assert(pg_data_schema_fields(schema, ctor) == fields);
	const struct pg_data_schema *another = pg_data_schema(&typing, parameters, 3, contexts);
	assert(another && pg_data_schema_layout(another) != layout);
	assert(!pg_data_schema_fields(schema, pg_data_constructor(pg_data_schema_layout(another), 2)));
	const struct pg_evidence *params = pg_prove_substitution(&typing, parameters, parameters, 1, &av);
	assert(pg_data_instance(&typing, schema, pg_data_constructor(layout, 0), params, 0, NULL) == params);
	const struct pg_evidence *dest = first;
	av = pg_prove_variable(&typing, dest, a);
	params = pg_prove_substitution(&typing, parameters, dest, 1, &av);
	const struct pg_evidence *values[] = {xv, pg_prove_reflexivity(&typing, at, xv)};
	const struct pg_evidence *instance = pg_data_instance(&typing, schema, ctor, params, 2, values);
	assert(instance && pg_evidence_rule(instance) == PG_CONTEXT_SUBSTITUTION);
	assert(pg_evidence_premise(instance, 0) == fields);
	assert(pg_data_instance(&typing, schema, ctor, params, 2, values) == instance);
	const struct pg_evidence *pv = pg_prove_variable(&typing, fields, p);
	const struct pg_evidence *instantiated = pg_prove_reindex(&typing, instance, pv);
	assert(instantiated && pg_evidence_subject(instantiated)->core == pg_evidence_subject(values[1])->core);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_term *vx = pg_evidence_subject(xv)->core;
	const struct pg_term *data = pg_application(graph, pg_application(graph, pg_reference(graph, ctor), vx),
		pg_evidence_subject(values[1])->core);
	struct pg_match_clause clauses[] = {
		{pg_data_constructor(layout, 0), vx},
		{pg_data_constructor(layout, 1), pg_lambda(graph, x, vx)},
		{ctor, pg_lambda(graph, x, pg_lambda(graph, p, vx))}
	};
	check(&work, pg_data_match(graph, layout, data, 3, clauses), vx);
	pg_whnf_work_destroy(&work);
	assert(!pg_data_instance(&typing, schema, ctor, params, 1, values));
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, NULL));
	const struct pg_evidence *bad[] = {values[1], xv};
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, bad));
	bad[0] = pg_prove_return(&typing, &classifiers, xv);
	bad[1] = values[1];
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, bad));
	assert(!pg_data_instance(&foreign, schema, ctor, params, 2, values));
	assert(!pg_data_schema(&foreign, parameters, 3, contexts));
	assert(!pg_data_schema(&typing, u, 0, NULL));
	assert(!pg_data_schema(&typing, parameters, 1, &empty));
	assert(!pg_data_schema(&typing, parameters, 1, NULL));
	const struct pg_evidence *foreign_empty = pg_prove_empty_context(&foreign);
	assert(!pg_data_schema(&typing, empty, 1, &foreign_empty));
	const struct pg_data_schema *unit_schema = pg_data_schema(&typing, empty, 1, &empty);
	const struct pg_evidence *empty_sub = pg_prove_substitution(&typing, empty, empty, 0, NULL);
	assert(unit_schema && pg_data_instance(&typing, unit_schema,
		pg_data_constructor(pg_data_schema_layout(unit_schema), 0), empty_sub, 0, NULL) == empty_sub);
	assert(pg_data_schema(&typing, empty, 0, NULL));
	assert(!pg_data_schema_layout(NULL) && !pg_data_schema_fields(NULL, ctor));
	/* Context identity, not a chosen derivation of that context, selects the
	 * parameter prefix. Both derivations remain available as immutable evidence. */
	const struct pg_evidence *alternate = pg_prove_context_extension(&typing, empty, a,
		pg_prove_value_type(&typing, pg_prove_type_value(&typing, u)));
	assert(alternate != parameters && pg_evidence_context(alternate) == pg_evidence_context(parameters));
	assert(pg_data_schema(&typing, alternate, 3, contexts));
	const struct pg_evidence *alternate_params = pg_prove_substitution(&typing, alternate, dest, 1, &av);
	assert(pg_data_instance(&typing, schema, ctor, alternate_params, 2, values) == instance);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_evidence *extra = pg_prove_context_extension(&typing, dest, y, at);
	const struct pg_evidence *ap = pg_prove_variable(&typing, extra, a);
	const struct pg_evidence *extra_params = pg_prove_substitution(&typing, parameters, extra, 1, &ap);
	bad[0] = pg_prove_variable(&typing, extra, y);
	bad[1] = pg_prove_projection(&typing, extra, values[1]);
	assert(!pg_data_instance(&typing, schema, ctor, extra_params, 2, bad));
	bad[1] = pg_prove_reflexivity(&typing, pg_prove_value_type(&typing, ap), bad[0]);
	assert(pg_data_instance(&typing, schema, ctor, extra_params, 2, bad));
	/* Act on the dependent field telescope using the existing checked action.
	 * Instantiating either endpoint reuses its ordinary substitution evidence. */
	const struct pg_binding_face *centers[2];
	for (size_t i = 0; i < 2; ++i)
		centers[i] = pg_binding_face(&dimensions, pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *left, *right, *paths[2];
	const struct pg_evidence *boundary = pg_identity_context(&typing, &dimensions, fields, 2, centers, &left, &right, paths);
	assert(boundary && pg_data_schema(&typing, parameters, 1, &boundary));
	const struct pg_evidence *sides[] = {left, right};
	for (size_t i = 0; i < 2; ++i) {
		av = pg_evidence_premise(sides[i], 2);
		params = pg_prove_substitution(&typing, parameters, boundary, 1, &av);
		values[0] = pg_evidence_premise(sides[i], 3);
		values[1] = pg_evidence_premise(sides[i], 4);
		assert(pg_data_instance(&typing, schema, ctor, params, 2, values) == sides[i]);
	}
	pg_dimensions_destroy(&dimensions);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&foreign);
	pg_typing_destroy(&typing);
	puts("field schemas: checked contexts, dependent instances, evidence reuse and selected boundaries passed");
}

int main(void)
{
	struct pg_graph graph;
	struct pg_whnf_work work;
	assert(pg_graph_init(&graph) == 0);
	schemas(&graph);
	assert(pg_whnf_work_init(&work, &graph) == 0);
	size_t arities[] = {0, 1};
	const struct pg_data_layout *nat = pg_data_layout(&graph, 2, arities);
	const struct pg_data_layout *other = pg_data_layout(&graph, 2, arities);
	assert(nat && other && nat != other);
	const struct pg_object *z = pg_data_constructor(nat, 0), *s = pg_data_constructor(nat, 1);
	const struct pg_term *zero = pg_reference(&graph, z), *succ = pg_reference(&graph, s);
	const struct pg_term *foreign = pg_reference(&graph, pg_data_constructor(other, 0));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y);
	const struct pg_term *id = pg_lambda(&graph, x, vx);
	const struct pg_term *delta = pg_lambda(&graph, y, pg_application(&graph, vy, vy));
	const struct pg_term *omega = pg_application(&graph, delta, delta);
	struct pg_match_clause clauses[] = {{z, zero}, {s, id}};
	const struct pg_term *one = pg_application(&graph, succ, zero);
	const struct pg_term *pred = pg_data_match(&graph, nat, one, 2, clauses);
	assert(pred);
	check(&work, pred, zero);
	check(&work, pg_identity_action(&graph, pred), pg_identity_action(&graph, zero));
	struct pg_match_clause reordered[] = {clauses[1], clauses[0]};
	assert(pg_data_match(&graph, nat, one, 2, reordered) == pred);
	check(&work, pg_data_match(&graph, nat, pg_application(&graph, succ, one), 2, clauses), one);
	check(&work, pg_data_match(&graph, nat, pg_application(&graph, id, one), 2, clauses), zero);
	const struct pg_term *neutral[] = {vx, foreign, succ, pg_application(&graph, one, zero)};
	for (size_t i = 0; i < sizeof(neutral) / sizeof(*neutral); ++i) {
		const struct pg_term *term = pg_data_match(&graph, nat, neutral[i], 2, clauses);
		check(&work, term, term);
	}
	/* Incomplete elimination spines neither demand a scrutinee nor select a case. */
	const struct pg_term *partial = pg_application(&graph, pg_reference(&graph, pg_data_matcher(nat)), omega);
	check(&work, partial, partial);
	clauses[0].branch = omega;
	check(&work, pg_data_match(&graph, nat, one, 2, clauses), zero);
	struct pg_whnf_job *diverges = pg_whnf_request(&work, &pg_pure_policy,
		pg_data_match(&graph, nat, zero, 2, clauses));
	assert(pg_whnf_advance(diverges, 100) == PG_EVAL_PENDING && !pg_whnf_certificate(diverges));
	clauses[0].branch = vx;
	clauses[1].branch = pg_lambda(&graph, y, vx);
	const struct pg_term *captured = pg_lambda(&graph, x, pg_data_match(&graph, nat, one, 2, clauses));
	check(&work, pg_application(&graph, captured, foreign), foreign);
	clauses[0].branch = zero;
	clauses[1].branch = id;
	captured = pg_lambda(&graph, y, pg_data_match(&graph, nat, pg_application(&graph, succ, vy), 2, clauses));
	check(&work, pg_application(&graph, captured, foreign), foreign);
	clauses[0].branch = id;
	clauses[1].branch = pg_lambda(&graph, x, pg_lambda(&graph, y, vx));
	check(&work, pg_application(&graph, pg_data_match(&graph, nat, one, 2, clauses), foreign), zero);
	/* Iota must not capture a field's free binder in the selected branch. */
	const struct pg_term *open = pg_application(&graph, succ, vy);
	const struct pg_term *open_match = pg_data_match(&graph, nat, open, 2, clauses);
	check(&work, pg_application(&graph, open_match, foreign), vy);
	clauses[1].branch = id;
	const struct pg_term *two = pg_application(&graph, succ, one);
	const struct pg_term *inner = pg_data_match(&graph, nat, two, 2, clauses);
	check(&work, pg_data_match(&graph, nat, inner, 2, clauses), zero);
	/* Field order and unused divergent fields are preserved without evaluation. */
	size_t pair_arity = 2;
	const struct pg_data_layout *pair = pg_data_layout(&graph, 1, &pair_arity);
	const struct pg_object *mk = pg_data_constructor(pair, 0);
	const struct pg_term *first = pg_lambda(&graph, x, pg_lambda(&graph, y, vx));
	const struct pg_term *second = pg_lambda(&graph, x, pg_lambda(&graph, y, vy));
	const struct pg_term *fields = pg_application(&graph, pg_application(&graph, pg_reference(&graph, mk), foreign), zero);
	struct pg_match_clause clause = {mk, first};
	check(&work, pg_data_match(&graph, pair, fields, 1, &clause), foreign);
	clause.branch = second;
	fields = pg_application(&graph, pg_application(&graph, pg_reference(&graph, mk), omega), zero);
	check(&work, pg_data_match(&graph, pair, fields, 1, &clause), zero);
	struct pg_match_clause duplicate[] = {{z, zero}, {z, zero}};
	assert(!pg_data_match(&graph, nat, zero, 2, duplicate));
	assert(!pg_data_match(&graph, nat, zero, 1, clauses));
	assert(!pg_data_match(&graph, nat, zero, 2, NULL));
	clauses[0].constructor = pg_data_constructor(other, 0);
	assert(!pg_data_match(&graph, nat, zero, 2, clauses));
	assert(!pg_data_constructor(nat, 2) && !pg_data_constructor(NULL, 0));
	assert(!pg_data_layout(&graph, 1, NULL));
	const struct pg_data_layout *empty = pg_data_layout(&graph, 0, NULL);
	const struct pg_term *empty_match = pg_data_match(&graph, empty, zero, 0, NULL);
	check(&work, empty_match, empty_match);
	struct pg_whnf_work bulk;
	assert(pg_whnf_work_init(&bulk, &graph) == 0);
	struct pg_whnf_job *whole = pg_whnf_request(&bulk, &pg_pure_policy, pred);
	assert(pg_whnf_advance(whole, 100000) == PG_EVAL_WHNF && pg_whnf_result(whole) == zero);
	assert(pg_whnf_steps(whole) == pg_whnf_steps(pg_whnf_request(&work, &pg_pure_policy, pred)));
	struct pg_whnf_job *beta = pg_whnf_request(&bulk, &pg_beta_policy, pred);
	assert(pg_whnf_advance(beta, 100000) == PG_EVAL_WHNF && pg_whnf_result(beta) == pred);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pred, zero) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	assert(pg_conversion_init(&comparison, &work, zero, foreign) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&bulk);
	pg_whnf_work_destroy(&work);
	pg_graph_destroy(&graph);
	puts("erased data: pointer-labelled Match, saturation, capture, lazy fields and split budgets passed");
	return 0;
}
