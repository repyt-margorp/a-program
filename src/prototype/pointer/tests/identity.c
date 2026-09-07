#include "identity.h"
#include "evidence.h"
#include "computation.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *b = pg_binder(&graph);
	const struct pg_object *p = pg_binder(&graph), *q = pg_binder(&graph);
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_evidence *scope = pg_prove_context_extension(&typing, empty, a, u0);
	scope = pg_prove_context_extension(&typing, scope, b, pg_prove_projection(&typing, scope, u0));
	const struct pg_evidence *left_type = pg_prove_variable(&typing, scope, a);
	const struct pg_evidence *right_type = pg_prove_variable(&typing, scope, b);
	const struct pg_evidence *universe = pg_prove_projection(&typing, scope, u0);
	const struct pg_evidence *family_type = pg_prove_identity_type(&typing, universe, left_type, right_type);
	assert(family_type && pg_evidence_judgement(family_type) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(family_type) == pg_universe(&classifiers, 1));
	scope = pg_prove_context_extension(&typing, scope, p, family_type);
	scope = pg_prove_context_extension(&typing, scope, q, pg_prove_projection(&typing, scope, family_type));
	scope = pg_prove_context_extension(&typing, scope, x, pg_prove_projection(&typing, scope, left_type));
	scope = pg_prove_context_extension(&typing, scope, y, pg_prove_projection(&typing, scope, right_type));
	assert(scope);
	const struct pg_evidence *pp = pg_prove_variable(&typing, scope, p);
	const struct pg_evidence *qq = pg_prove_variable(&typing, scope, q);
	const struct pg_evidence *xx = pg_prove_variable(&typing, scope, x);
	const struct pg_evidence *yy = pg_prove_variable(&typing, scope, y);
	const struct pg_evidence *rp = pg_prove_identity_instance(&typing, &classifiers, pp, xx, yy);
	const struct pg_evidence *rq = pg_prove_identity_instance(&typing, &classifiers, qq, xx, yy);
	assert(rp && rq && rp != rq);
	assert(pg_evidence_judgement(rp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(rp) == pg_universe(&classifiers, 0));
	assert(pg_evidence_subject(rp)->core != pg_evidence_subject(rq)->core);
	assert(pg_alpha_equal(pg_evidence_subject(rp)->core, pg_evidence_subject(rq)->core) == 0);
	assert(pg_evidence_premise(rp, 0) == pp && pg_evidence_premise(rq, 0) == qq);
	assert(pg_prove_identity_instance(&typing, &classifiers, pp, xx, yy) == rp);
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp, yy, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, xx, xx, yy));
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp, left_type, yy));
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp,
		pg_prove_return(&typing, &classifiers, xx), yy));
	assert(!pg_prove_application(&typing, pp, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, NULL, xx, yy));
	assert(pg_prove_context_extension(&typing, scope, pg_binder(&graph), rp));

	/* Id A and refl A are the same graph action, not independent former tags. */
	const struct pg_evidence *a_value = pg_prove_projection(&typing, scope, left_type);
	const struct pg_evidence *a_type = pg_prove_value_type(&typing, a_value);
	universe = pg_prove_projection(&typing, scope, u0);
	const struct pg_evidence *id_a = pg_prove_reflexivity(&typing, universe, a_value);
	const struct pg_evidence *diagonal = pg_prove_identity_type(&typing, a_type, xx, xx);
	const struct pg_evidence *via_family = pg_prove_identity_instance(&typing, &classifiers, id_a, xx, xx);
	assert(diagonal && via_family);
	assert(pg_evidence_subject(diagonal)->core == pg_evidence_subject(via_family)->core);
	assert(pg_evidence_classifier(diagonal) == pg_evidence_classifier(via_family));
	const struct pg_evidence *refl_x = pg_prove_reflexivity(&typing, a_type, xx);
	assert(refl_x && pg_evidence_judgement(refl_x) == PG_JUDGEMENT_VALUE);
	assert(pg_evidence_classifier(refl_x) == pg_evidence_subject(diagonal)->core);
	assert(pg_prove_classifier(&typing, &classifiers, scope, refl_x) == diagonal);
	assert(!pg_prove_identity_type(&typing, a_type, xx, yy));
	assert(!pg_prove_identity_type(&typing, a_value, xx, xx));
	assert(!pg_prove_reflexivity(&typing, universe, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, refl_x, xx, xx));
	const struct pg_term *base, *left, *right;
	assert(pg_identity_action_view(pg_evidence_subject(refl_x)->core, &base));
	assert(base == pg_evidence_subject(xx)->core);
	assert(!pg_identity_action_view(pg_evidence_subject(diagonal)->core, &base));
	assert(!pg_identity_view(pg_evidence_subject(refl_x)->core, &base, &left, &right));
	assert(!pg_identity_view(NULL, &base, &left, &right));
	assert(pg_identity_view(pg_evidence_classifier(refl_x), &base, &left, &right));
	assert(base == pg_evidence_subject(a_type)->core);
	assert(left == pg_evidence_subject(xx)->core && right == left);
	assert(!pg_identity_view(pg_evidence_subject(rp)->core, &base, &left, &right));
	const struct pg_evidence *p_type = pg_prove_classifier(&typing, &classifiers, scope, pp);
	const struct pg_evidence *refl_p = pg_prove_reflexivity(&typing, p_type, pp);
	const struct pg_evidence *p_to_q = pg_prove_identity_type(&typing, p_type, pp, qq);
	struct pg_beta_work beta;
	struct pg_conversion comparison;
	assert(pg_beta_work_init(&beta, &graph) == 0);
	assert(pg_conversion_init(&comparison, &beta, pg_evidence_classifier(refl_p),
		pg_evidence_subject(p_to_q)->core) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_prove_conversion(&typing, refl_p, p_to_q, pg_conversion_certificate(&comparison)));
	pg_conversion_destroy(&comparison);
	pg_beta_work_destroy(&beta);
	const struct pg_object *argument = pg_binder(&graph);
	const struct pg_evidence *body_scope = pg_prove_context_extension(&typing, scope, argument, a_type);
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, body_scope, argument));
	const struct pg_evidence *body_type = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, body_scope, a_type));
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, a_type, body_scope, body_type);
	const struct pg_evidence *function = pg_prove_lambda(&typing, pi, body);
	assert(function);
	assert(!pg_prove_identity_instance(&typing, &classifiers, function, xx, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers,
		pg_prove_thunk(&typing, &classifiers, function), xx, xx));
	const struct pg_evidence *b_type = pg_prove_value_type(&typing, pg_prove_projection(&typing, scope, right_type));
	const struct pg_evidence *other_scope = pg_prove_context_extension(&typing, scope, argument, b_type);
	const struct pg_evidence *other_body = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, other_scope, argument));
	const struct pg_evidence *other_pi = pg_prove_pi(&typing, &classifiers, b_type, other_scope,
		pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, other_scope, b_type)));
	const struct pg_evidence *other_function = pg_prove_lambda(&typing, other_pi, other_body);
	assert(other_function && pg_evidence_subject(function)->core == pg_evidence_subject(other_function)->core);
	const struct pg_evidence *function_action = pg_prove_reflexivity(&typing, pi, function);
	const struct pg_evidence *other_action = pg_prove_reflexivity(&typing, other_pi, other_function);
	assert(function_action && other_action);
	assert(pg_evidence_subject(function_action)->core == pg_evidence_subject(other_action)->core);
	assert(pg_evidence_classifier(function_action) != pg_evidence_classifier(other_action));
	assert(!pg_prove_identity_type(&typing, pi, function, other_function));

	/* Higher symbolic diagonal witnesses retain their preceding classifier and
	 * term. This is not yet the full higher boundary-instantiation algorithm. */
	const struct pg_evidence *witness = pp;
	for (size_t dimension = 0; dimension < 3; ++dimension) {
		const struct pg_evidence *type = pg_prove_classifier(&typing, &classifiers, scope, witness);
		const struct pg_evidence *next = pg_prove_reflexivity(&typing, type, witness);
		assert(next && pg_evidence_judgement(next) == PG_JUDGEMENT_VALUE);
		assert(pg_identity_view(pg_evidence_classifier(next), &base, &left, &right));
		assert(base == pg_evidence_classifier(witness));
		assert(left == pg_evidence_subject(witness)->core && right == left);
		assert(pg_evidence_premise(next, 1) == witness);
		witness = next;
	}

	/* Computation families remain computation types and never become context
	 * values just because they have a universe bound. No endpoint is executed. */
	const struct pg_evidence *fa = pg_prove_return_type(&typing, &classifiers, a_type);
	const struct pg_evidence *returned = pg_prove_return(&typing, &classifiers, xx);
	const struct pg_evidence *cid = pg_prove_identity_type(&typing, fa, returned, returned);
	const struct pg_evidence *crefl = pg_prove_reflexivity(&typing, fa, returned);
	assert(cid && pg_evidence_judgement(cid) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(crefl && pg_evidence_judgement(crefl) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_prove_classifier(&typing, &classifiers, scope, crefl) == cid);
	assert(!pg_prove_type_value(&typing, cid));
	assert(!pg_prove_context_extension(&typing, scope, pg_binder(&graph), cid));
	assert(!pg_prove_identity_type(&typing, fa, xx, xx));
	assert(!pg_prove_identity_type(&typing, a_type, returned, returned));
	assert(!pg_prove_identity_instance(&typing, &classifiers, crefl, xx, xx));
	const struct pg_evidence *quoted = pg_prove_thunk(&typing, &classifiers, crefl);
	assert(quoted && pg_evidence_judgement(quoted) == PG_JUDGEMENT_VALUE);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(crefl)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == pg_evidence_subject(crefl)->core);
	pg_eval_destroy(&machine);

	/* No new authority for reindexing Identity: the family parameter stays a
	 * visible Core operand and is substituted by the existing traversal. */
	size_t count = 6;
	const struct pg_evidence *images[] = {
		a_value, pg_prove_projection(&typing, scope, right_type), qq, qq, xx, yy
	};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, scope, scope, count, images);
	assert(sigma);
	const struct pg_evidence *moved = pg_prove_reindex(&typing, sigma, rp);
	assert(moved && pg_evidence_subject(moved)->core == pg_evidence_subject(rq)->core);
	const struct pg_evidence *moved_witness = pg_prove_reindex(&typing, sigma, refl_x);
	assert(moved_witness);
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, scope, moved_witness))->core
		== pg_evidence_classifier(moved_witness));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, &graph) == 0);
	assert(!pg_prove_identity_type(&foreign, a_type, xx, xx));
	assert(!pg_prove_reflexivity(&foreign, a_type, xx));
	assert(!pg_prove_identity_instance(&foreign, &classifiers, pp, xx, yy));
	pg_typing_destroy(&foreign);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("identity formation: chosen families, symbolic reflexivity, polarity, universes and reindex passed");
	return 0;
}
