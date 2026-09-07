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

static const struct pg_evidence *parameter_result(struct pg_typing *typing,
	const struct pg_evidence *parameters, const struct pg_evidence *fields)
{
	const struct pg_evidence *a = pg_prove_variable(typing, fields, pg_evidence_context(parameters)->binder);
	return pg_prove_substitution(typing, parameters, fields, 1, &a);
}

static const struct pg_term *boundary_apply(struct pg_graph *graph, const struct pg_term *function,
	const struct pg_term *left, const struct pg_term *right, const struct pg_term *path)
{
	return pg_application(graph, pg_identity_instance(graph, function, left, right), path);
}

static const struct pg_term *action_match(struct pg_graph *graph, const struct pg_term *matcher,
	const struct pg_term *left, const struct pg_term *right, const struct pg_term *path,
	size_t count, const struct pg_term *const *branches)
{
	const struct pg_term *result = boundary_apply(graph, matcher, left, right, path);
	for (size_t i = 0; i < count; ++i)
		result = boundary_apply(graph, result, branches[i], branches[i], pg_identity_action(graph, branches[i]));
	return result;
}

static const struct pg_evidence *checked_case(struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_whnf_work *work,
	const struct pg_data_schema *schema, const struct pg_object *constructor,
	const struct pg_evidence *motive, const struct pg_evidence *body)
{
	const struct pg_evidence *target = pg_data_branch_motive(typing, schema, constructor, motive);
	assert(target && pg_evidence_rule(target) == PG_REINDEX);
	assert(pg_evidence_premise(target, 0) == pg_data_schema_result(schema, constructor));
	assert(pg_evidence_premise(target, 1) == motive);
	assert(pg_data_branch_motive(typing, schema, constructor, motive) == target);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, work, pg_evidence_classifier(body), pg_evidence_subject(target)->core) == 0);
	assert(!pg_data_case(typing, classifiers, schema, constructor, motive, body, NULL));
	if (pg_conversion_status(&comparison) == PG_CONVERSION_PENDING)
		assert(!pg_conversion_certificate(&comparison));
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&comparison) < 100000);
	assert(pg_conversion_status(&comparison) == PG_CONVERSION_EQUAL);
	const struct pg_conversion_certificate *certificate = pg_conversion_certificate(&comparison);
	const struct pg_evidence *result = pg_data_case(typing, classifiers, schema, constructor, motive, body, certificate);
	assert(result && pg_data_case(typing, classifiers, schema, constructor, motive, body, certificate) == result);
	const struct pg_evidence *leaf = result;
	while (pg_evidence_rule(leaf) == PG_LAMBDA_INTRO) leaf = pg_evidence_premise(leaf, 1);
	assert(pg_evidence_rule(leaf) == PG_TYPE_CONVERSION);
	assert(pg_evidence_premise(leaf, 0) == body && pg_evidence_premise(leaf, 1) == target);
	pg_conversion_destroy(&comparison);
	return result;
}

static void higher_matches(struct pg_graph *graph, struct pg_whnf_work *work)
{
	size_t arities[] = {0, 1};
	const struct pg_data_layout *layout = pg_data_layout(graph, 2, arities);
	const struct pg_term *zero = pg_reference(graph, pg_data_constructor(layout, 0));
	const struct pg_term *succ = pg_reference(graph, pg_data_constructor(layout, 1));
	const struct pg_term *one = pg_application(graph, succ, zero), *two = pg_application(graph, succ, one);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *id = pg_lambda(graph, x, vx);
	const struct pg_term *p = pg_reference(graph, pg_binder(graph)), *q = pg_reference(graph, pg_binder(graph));
	const struct pg_term *branches[] = {zero, id};
	const struct pg_term *operation = pg_identity_action(graph, pg_reference(graph, pg_data_matcher(layout)));
	const struct pg_term *center = pg_identity_apply(graph, succ, zero, one, p);
	const struct pg_term *term = action_match(graph, operation, one, two, center, 2, branches);
	check(work, term, p);
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *branch_path = pg_lambda(graph, x, pg_lambda(graph, y, pg_lambda(graph, z, q)));
	const struct pg_term *selected = boundary_apply(graph, operation, one, two, center);
	selected = boundary_apply(graph, selected, zero, zero, pg_identity_action(graph, zero));
	check(work, boundary_apply(graph, selected, id, id, branch_path), q);
	const struct pg_term *different = pg_identity_apply(graph, succ, zero, one, q);
	check(work, action_match(graph, operation, one, two, different, 2, branches), q);
	check(work, action_match(graph, operation, one, one, pg_identity_action(graph, one), 2, branches),
		pg_identity_action(graph, zero));
	check(work, action_match(graph, operation, zero, zero, pg_identity_action(graph, zero), 2, branches),
		pg_identity_action(graph, zero));
	struct pg_match_clause clauses[] = {{pg_data_constructor(layout, 0), zero}, {pg_data_constructor(layout, 1), id}};
	const struct pg_term *pred = pg_lambda(graph, y, pg_data_match(graph, layout, vy, 2, clauses));
	check(work, pg_identity_apply(graph, pred, one, two, center), p);
	const struct pg_term *delta = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, delta, delta);
	branches[0] = omega;
	check(work, action_match(graph, operation, omega, omega, center, 2, branches), p);
	branches[0] = zero;
	branches[1] = pg_lambda(graph, x, vy);
	const struct pg_term *captured = pg_lambda(graph, y,
		action_match(graph, operation, one, two, center, 2, branches));
	check(work, pg_application(graph, captured, zero), pg_identity_action(graph, zero));
	branches[1] = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *function_path = action_match(graph, operation, one, two, center, 2, branches);
	check(work, boundary_apply(graph, function_path, zero, one, q), p);
	branches[1] = id;
	/* A supplied matcher prefix and beta-reduced matcher source agree. */
	const struct pg_term *partial = pg_application(graph, pg_reference(graph, pg_data_matcher(layout)), one);
	const struct pg_term *prefixed = pg_identity_action(graph, partial);
	for (size_t i = 0; i < 2; ++i)
		prefixed = boundary_apply(graph, prefixed, branches[i], branches[i], pg_identity_action(graph, branches[i]));
	check(work, prefixed, pg_identity_action(graph, zero));
	const struct pg_term *beta = pg_identity_action(graph,
		pg_application(graph, id, pg_reference(graph, pg_data_matcher(layout))));
	check(work, action_match(graph, beta, one, two, center, 2, branches), p);
	struct pg_whnf_job *head = pg_whnf_request(work, &pg_pure_policy, operation);
	assert(pg_whnf_advance(head, 100000) == PG_EVAL_WHNF);
	const struct pg_term *lowered = pg_whnf_result(head);
	const struct pg_data_layout *foreign = pg_data_layout(graph, 2, arities);
	const struct pg_term *invalid[] = {p, pg_identity_action(graph, succ),
		pg_application(graph, center, zero),
		pg_identity_action(graph, pg_reference(graph, pg_data_constructor(foreign, 0)))};
	for (size_t i = 0; i < 4; ++i)
		check(work, action_match(graph, operation, one, two, invalid[i], 2, branches),
			action_match(graph, lowered, one, two, invalid[i], 2, branches));
	check(work, boundary_apply(graph, operation, one, two, omega), boundary_apply(graph, lowered, one, two, omega));
	/* A constructor path may mix compressed diagonal fields and selected paths. */
	size_t pair_arity = 2;
	const struct pg_data_layout *pair = pg_data_layout(graph, 1, &pair_arity);
	const struct pg_term *mk = pg_reference(graph, pg_data_constructor(pair, 0));
	const struct pg_term *pair_left = pg_application(graph, pg_application(graph, mk, zero), one);
	const struct pg_term *pair_right = pg_application(graph, pg_application(graph, mk, zero), two);
	const struct pg_term *pair_path = pg_identity_apply(graph, mk, zero, zero, pg_identity_action(graph, zero));
	pair_path = boundary_apply(graph, pair_path, one, two, p);
	const struct pg_term *second = pg_lambda(graph, x, pg_lambda(graph, y, vy));
	check(work, action_match(graph, pg_identity_action(graph, pg_reference(graph, pg_data_matcher(pair))),
		pair_left, pair_right, pair_path, 1, &second), p);
	struct pg_whnf_work bulk;
	assert(pg_whnf_work_init(&bulk, graph) == 0);
	struct pg_whnf_job *whole = pg_whnf_request(&bulk, &pg_pure_policy, term);
	assert(pg_whnf_advance(whole, 100000) == PG_EVAL_WHNF && pg_whnf_result(whole) == p);
	assert(pg_whnf_steps(whole) == pg_whnf_steps(pg_whnf_request(work, &pg_pure_policy, term)));
	struct pg_whnf_job *opaque = pg_whnf_request(&bulk, &pg_beta_policy, term);
	assert(pg_whnf_advance(opaque, 100000) == PG_EVAL_WHNF && pg_whnf_result(opaque) == term);
	pg_whnf_work_destroy(&bulk);
	puts("Match action: selected constructor paths, diagonal prefixes, neutral heads and split budgets passed");
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
	const struct pg_evidence *results[] = {parameter_result(&typing, parameters, parameters),
		parameter_result(&typing, parameters, first), parameter_result(&typing, parameters, fields)};
	const struct pg_data_schema *schema = pg_data_schema(&typing, parameters, parameters, 3, results);
	assert(schema);
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	const struct pg_object *ctor = pg_data_constructor(layout, 2);
	assert(pg_data_schema_fields(schema, ctor) == fields);
	const struct pg_data_schema *another = pg_data_schema(&typing, parameters, parameters, 3, results);
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
	/* The result indices themselves form a dependent telescope. */
	const struct pg_object *i = pg_binder(graph), *q = pg_binder(graph);
	const struct pg_evidence *index_first = pg_prove_context_extension(&typing, parameters, i,
		pg_prove_variable(&typing, parameters, a));
	const struct pg_evidence *iv = pg_prove_variable(&typing, index_first, i);
	const struct pg_evidence *index_type = pg_prove_value_type(&typing, pg_prove_variable(&typing, index_first, a));
	const struct pg_evidence *indices = pg_prove_context_extension(&typing, index_first, q,
		pg_prove_identity_type(&typing, index_type, iv, iv));
	const struct pg_evidence *index_images[] = {pg_prove_variable(&typing, fields, a),
		pg_prove_variable(&typing, fields, x), pv};
	const struct pg_evidence *index_map = pg_prove_substitution(&typing, indices, fields, 3, index_images);
	const struct pg_data_schema *indexed = pg_data_schema(&typing, parameters, indices, 1, &index_map);
	assert(indexed);
	assert(pg_data_schema_indices(indexed) == indices);
	assert(pg_data_schema_indices(pg_data_schema(&typing, parameters, indices, 0, NULL)) == indices);
	const struct pg_object *indexed_ctor = pg_data_constructor(pg_data_schema_layout(indexed), 0);
	assert(pg_data_schema_result(indexed, indexed_ctor) == index_map);
	assert(pg_data_schema_fields(indexed, indexed_ctor) == fields);
	assert(pg_data_instance(&typing, indexed, indexed_ctor, params, 2, values) == instance);
	const struct pg_evidence *index_result = pg_data_result(&typing, indexed, indexed_ctor, instance);
	assert(index_result && pg_evidence_premise(index_result, 0) == indices);
	assert(pg_data_result(&typing, indexed, indexed_ctor, instance) == index_result);
	for (size_t n = 0; n < 2; ++n)
		assert(pg_evidence_subject(pg_evidence_premise(index_result, n + 3))->core == pg_evidence_subject(values[n])->core);
	assert(!pg_data_schema(&typing, parameters, indices, 1, results));
	assert(!pg_data_schema(&typing, parameters, empty, 0, NULL));
	assert(!pg_data_result(&typing, indexed, indexed_ctor, params));
	assert(!pg_data_result(&typing, indexed, ctor, instance));
	assert(!pg_data_result(&foreign, indexed, indexed_ctor, instance));
	/* A well-typed substitution that changes a fixed parameter is not a
	 * constructor result for that declaration, even without any indices. */
	const struct pg_object *b = pg_binder(graph);
	const struct pg_evidence *with_b = pg_prove_context_extension(&typing, parameters, b,
		pg_prove_projection(&typing, parameters, u));
	const struct pg_evidence *bv = pg_prove_variable(&typing, with_b, b);
	const struct pg_evidence *changed_parameter = pg_prove_substitution(&typing, parameters, with_b, 1, &bv);
	assert(changed_parameter && !pg_data_schema(&typing, parameters, parameters, 1, &changed_parameter));
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
	/* Branch abstraction synthesizes raw Pi/Lambda from the field telescope.
	 * Its application agrees with direct typed substitution of the body. */
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers, pv);
	const struct pg_evidence *branch = pg_data_branch(&typing, &classifiers, indexed, indexed_ctor, body);
	assert(branch && pg_evidence_context(branch) == pg_evidence_context(parameters));
	assert(pg_data_branch(&typing, &classifiers, indexed, indexed_ctor, body) == branch);
	const struct pg_evidence *applied = pg_prove_reindex(&typing, params, branch);
	for (size_t n = 0; n < 2; ++n) applied = pg_prove_application(&typing, applied, values[n]);
	assert(applied);
	const struct pg_evidence *body_instance = pg_prove_reindex(&typing, instance, body);
	const struct pg_term *answer = pg_evidence_subject(body_instance)->core;
	check(&work, pg_evidence_subject(applied)->core, answer);
	const struct pg_whnf_certificate *receipt = pg_whnf_certificate(pg_whnf_request(&work, &pg_pure_policy,
		pg_evidence_subject(applied)->core));
	const struct pg_evidence *reduced = pg_prove_normalization(&typing, applied, receipt);
	assert(reduced && pg_alpha_equal(pg_evidence_classifier(reduced), pg_evidence_classifier(body_instance)) == 1);
	const struct pg_evidence *qv = pg_prove_variable(&typing, indices, q);
	const struct pg_evidence *motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_classifier(&typing, &classifiers, indices, qv));
	const struct pg_evidence *result_type = pg_prove_reindex(&typing, index_result, motive);
	assert(result_type && pg_alpha_equal(pg_evidence_classifier(applied), pg_evidence_subject(result_type)->core) == 1);
	const struct pg_evidence *case_proof = checked_case(&typing, &classifiers, &work, indexed, indexed_ctor, motive, body);
	assert(pg_evidence_subject(case_proof)->core == pg_evidence_subject(branch)->core);
	const struct pg_evidence *case_application = pg_prove_reindex(&typing, params, case_proof);
	for (size_t n = 0; n < 2; ++n) case_application = pg_prove_application(&typing, case_application, values[n]);
	assert(case_application && pg_alpha_equal(pg_evidence_classifier(case_application), pg_evidence_subject(result_type)->core) == 1);
	check(&work, pg_evidence_subject(case_application)->core, answer);
	assert(!pg_data_branch_motive(&foreign, indexed, indexed_ctor, motive));
	assert(!pg_data_branch_motive(&typing, indexed, ctor, motive));
	assert(!pg_data_branch_motive(&typing, indexed, indexed_ctor, qv));
	assert(!pg_data_branch_motive(&typing, indexed, indexed_ctor, result_type));
	assert(!pg_data_branch_motive(&typing, NULL, indexed_ctor, motive));
	const struct pg_evidence *wrong_motive = pg_prove_return_type(&typing, &classifiers, index_type);
	wrong_motive = pg_prove_projection(&typing, indices, wrong_motive);
	const struct pg_evidence *wrong_target = pg_data_branch_motive(&typing, indexed, indexed_ctor, wrong_motive);
	assert(wrong_target);
	const struct pg_evidence *checked_body = case_proof;
	while (pg_evidence_rule(checked_body) == PG_LAMBDA_INTRO) checked_body = pg_evidence_premise(checked_body, 1);
	const struct pg_conversion_certificate *valid = pg_evidence_conversion(checked_body);
	assert(valid && !pg_data_case(&typing, &classifiers, indexed, indexed_ctor, wrong_motive, body, valid));
	assert(!pg_data_case(&foreign, &classifiers, indexed, indexed_ctor, motive, body, valid));
	assert(!pg_data_case(&typing, &classifiers, indexed, indexed_ctor, motive, pv, valid));
	struct pg_conversion mismatch;
	assert(pg_conversion_init(&mismatch, &work, pg_evidence_classifier(body), pg_evidence_subject(wrong_target)->core) == 0);
	assert(pg_conversion_advance(&mismatch, 100000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_data_case(&typing, &classifiers, indexed, indexed_ctor, wrong_motive, body, pg_conversion_certificate(&mismatch)));
	pg_conversion_destroy(&mismatch);
	struct pg_match_clause typed_clause = {indexed_ctor, pg_evidence_subject(branch)->core};
	const struct pg_term *indexed_data = pg_application(graph, pg_application(graph,
		pg_reference(graph, indexed_ctor), vx), pg_evidence_subject(values[1])->core);
	check(&work, pg_data_match(graph, pg_data_schema_layout(indexed), indexed_data, 1, &typed_clause), answer);
	assert(!pg_data_branch(&typing, &classifiers, indexed, indexed_ctor, pv));
	assert(!pg_data_branch(&typing, &classifiers, indexed, indexed_ctor,
		pg_prove_return(&typing, &classifiers, xv)));
	assert(!pg_data_branch(&typing, &classifiers, indexed, ctor, body));
	assert(!pg_data_branch(&foreign, &classifiers, indexed, indexed_ctor, body));
	const struct pg_evidence *constant = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, parameters, a));
	assert(pg_data_branch(&typing, &classifiers, schema, pg_data_constructor(layout, 0), constant) == constant);
	const struct pg_evidence *constant_case = checked_case(&typing, &classifiers, &work, schema,
		pg_data_constructor(layout, 0), pg_prove_classifier(&typing, &classifiers, parameters, constant), constant);
	assert(pg_evidence_subject(constant_case) == pg_evidence_subject(constant));
	/* A branch returning a raw function stays a computation Pi, not F(U Pi). */
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *field_a = pg_prove_value_type(&typing, index_images[0]);
	const struct pg_evidence *under_z = pg_prove_context_extension(&typing, fields, z, field_a);
	const struct pg_evidence *return_p = pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, under_z, p));
	const struct pg_evidence *function_type = pg_prove_pi(&typing, &classifiers, field_a, under_z,
		pg_prove_classifier(&typing, &classifiers, under_z, return_p));
	const struct pg_evidence *function_body = pg_prove_lambda(&typing, function_type, return_p);
	const struct pg_evidence *function_branch = pg_data_branch(&typing, &classifiers, indexed, indexed_ctor, function_body);
	const struct pg_object *index_z = pg_binder(graph);
	const struct pg_evidence *index_a = pg_prove_value_type(&typing, pg_prove_variable(&typing, indices, a));
	const struct pg_evidence *index_under_z = pg_prove_context_extension(&typing, indices, index_z, index_a);
	const struct pg_evidence *function_motive = pg_prove_pi(&typing, &classifiers, index_a, index_under_z,
		pg_prove_projection(&typing, index_under_z, motive));
	assert(function_motive);
	const struct pg_evidence *function_case = checked_case(&typing, &classifiers, &work, indexed, indexed_ctor,
		function_motive, function_body);
	assert(pg_evidence_subject(function_case)->core == pg_evidence_subject(function_branch)->core);
	applied = pg_prove_reindex(&typing, params, function_branch);
	for (size_t n = 0; n < 2; ++n) applied = pg_prove_application(&typing, applied, values[n]);
	assert(applied);
	const struct pg_term *domain, *codomain;
	const struct pg_object *bound;
	assert(pg_pi_view(pg_evidence_classifier(applied), &domain, &bound, &codomain));
	applied = pg_prove_application(&typing, applied, xv);
	assert(applied);
	check(&work, pg_evidence_subject(applied)->core, answer);
	pg_whnf_work_destroy(&work);
	assert(!pg_data_instance(&typing, schema, ctor, params, 1, values));
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, NULL));
	const struct pg_evidence *bad[] = {values[1], xv};
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, bad));
	bad[0] = pg_prove_return(&typing, &classifiers, xv);
	bad[1] = values[1];
	assert(!pg_data_instance(&typing, schema, ctor, params, 2, bad));
	assert(!pg_data_instance(&foreign, schema, ctor, params, 2, values));
	assert(!pg_data_schema(&foreign, parameters, parameters, 3, results));
	assert(!pg_data_schema(&typing, u, parameters, 0, NULL));
	assert(!pg_data_schema(&typing, parameters, parameters, 1, &empty));
	assert(!pg_data_schema(&typing, parameters, parameters, 1, NULL));
	const struct pg_evidence *foreign_empty = pg_prove_empty_context(&foreign);
	assert(!pg_data_schema(&typing, empty, empty, 1, &foreign_empty));
	const struct pg_evidence *empty_sub = pg_prove_substitution(&typing, empty, empty, 0, NULL);
	const struct pg_data_schema *unit_schema = pg_data_schema(&typing, empty, empty, 1, &empty_sub);
	assert(unit_schema && pg_data_instance(&typing, unit_schema,
		pg_data_constructor(pg_data_schema_layout(unit_schema), 0), empty_sub, 0, NULL) == empty_sub);
	assert(pg_data_schema(&typing, empty, empty, 0, NULL));
	assert(!pg_data_schema_layout(NULL) && !pg_data_schema_fields(NULL, ctor));
	/* Context identity, not a chosen derivation of that context, selects the
	 * parameter prefix. Both derivations remain available as immutable evidence. */
	const struct pg_evidence *alternate = pg_prove_context_extension(&typing, empty, a,
		pg_prove_value_type(&typing, pg_prove_type_value(&typing, u)));
	assert(alternate != parameters && pg_evidence_context(alternate) == pg_evidence_context(parameters));
	assert(pg_data_schema(&typing, alternate, parameters, 3, results));
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
	assert(boundary);
	const struct pg_evidence *boundary_result = parameter_result(&typing, parameters, boundary);
	assert(pg_data_schema(&typing, parameters, parameters, 1, &boundary_result));
	const struct pg_evidence *sides[] = {left, right};
	for (size_t side = 0; side < 2; ++side) {
		av = pg_evidence_premise(sides[side], 2);
		params = pg_prove_substitution(&typing, parameters, boundary, 1, &av);
		values[0] = pg_evidence_premise(sides[side], 3);
		values[1] = pg_evidence_premise(sides[side], 4);
		assert(pg_data_instance(&typing, schema, ctor, params, 2, values) == sides[side]);
		index_result = pg_data_result(&typing, indexed, indexed_ctor, sides[side]);
		assert(index_result);
		for (size_t n = 0; n < 2; ++n)
			assert(pg_evidence_subject(pg_evidence_premise(index_result, n + 3))->core == pg_evidence_subject(values[n])->core);
	}
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_evidence *body_type = pg_prove_classifier(&typing, &classifiers, fields, body);
	const struct pg_evidence *acted_body = pg_prove_family_action(&typing, body_type, body, left, right, 2, paths);
	const struct pg_term *acted_answer = pg_application(graph, pg_reference(graph, &pg_return_operation),
		pg_evidence_subject(paths[1])->core);
	assert(acted_body);
	const struct pg_evidence *branch_type = pg_prove_classifier(&typing, &classifiers, parameters, branch);
	const struct pg_evidence *branch_action = pg_prove_reflexivity(&typing, branch_type, branch);
	assert(branch_action);
	const struct pg_term *acted_branch = pg_evidence_subject(branch_action)->core;
	const struct pg_term *endpoints[] = {pg_reference(graph, indexed_ctor), pg_reference(graph, indexed_ctor)};
	const struct pg_term *constructor_path = pg_identity_action(graph, endpoints[0]);
	for (size_t n = 0; n < 2; ++n) {
		const struct pg_term *l = pg_evidence_subject(pg_evidence_premise(left, n + 3))->core;
		const struct pg_term *r = pg_evidence_subject(pg_evidence_premise(right, n + 3))->core;
		const struct pg_term *path = pg_evidence_subject(paths[n])->core;
		acted_branch = boundary_apply(graph, acted_branch, l, r, path);
		constructor_path = boundary_apply(graph, constructor_path, l, r, path);
		endpoints[0] = pg_application(graph, endpoints[0], l);
		endpoints[1] = pg_application(graph, endpoints[1], r);
	}
	const struct pg_term *branch_core = pg_evidence_subject(branch)->core;
	const struct pg_term *acted_match = action_match(graph,
		pg_identity_action(graph, pg_reference(graph, pg_data_matcher(pg_data_schema_layout(indexed)))),
		endpoints[0], endpoints[1], constructor_path, 1, &branch_core);
	/* RETURN is WHNF before its payload reduces; compare below that head
	 * explicitly instead of demanding stronger evaluation from WHNF.
	 * This checks erased Match coherence with the typed body action, not
	 * datatype membership or a formation proof for the whole Match. */
	const struct pg_term *actions[] = {pg_evidence_subject(acted_body)->core, acted_branch, acted_match};
	for (size_t n = 0; n < sizeof(actions) / sizeof(*actions); ++n) {
		struct pg_conversion comparison;
		assert(pg_conversion_init(&comparison, &work, actions[n], acted_answer) == 0);
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
		pg_conversion_destroy(&comparison);
	}
	for (size_t n = 0; n < 2; ++n) {
		const struct pg_evidence *type = n ? pg_prove_projection(&typing, fields, id)
			: pg_prove_value_type(&typing, index_images[0]);
		const struct pg_evidence *acted = pg_prove_family_action(&typing, type, index_images[n + 1], left, right, 2, paths);
		assert(acted);
		check(&work, pg_evidence_subject(acted)->core, pg_evidence_subject(paths[n])->core);
	}
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&foreign);
	pg_typing_destroy(&typing);
	puts("data schemas: dependent fields/indices, fixed parameters, composition and selected boundaries passed");
}

int main(void)
{
	struct pg_graph graph;
	struct pg_whnf_work work;
	assert(pg_graph_init(&graph) == 0);
	schemas(&graph);
	assert(pg_whnf_work_init(&work, &graph) == 0);
	higher_matches(&graph, &work);
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
