#include "identity.h"
#include "evidence.h"
#include "computation.h"
#include "action.h"

#include <assert.h>
#include <stdio.h>

static int first_argument(struct pg_eval *machine)
{
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	return argument ? pg_eval_enter(machine, *argument, 1) : 1;
}

static void normalizes(struct pg_whnf_work *work, const struct pg_term *input, const struct pg_term *expected)
{
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, input);
	assert(job && job == pg_whnf_request(work, &pg_pure_policy, input));
	while (pg_whnf_status(job) == PG_EVAL_PENDING) {
		assert(!pg_whnf_result(job));
		pg_whnf_advance(job, 1);
		assert(pg_whnf_steps(job) < 10000);
	}
	assert(pg_whnf_status(job) == PG_EVAL_WHNF);
	assert(pg_whnf_result(job) == expected);
}

static void boundary_context(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *p, const struct pg_evidence *q,
	const struct pg_evidence *substitution)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 1);
	const struct pg_coordinate zero = {PG_ENDPOINT_ZERO, 0}, one = {PG_ENDPOINT_ONE, 0};
	const struct pg_binding_face *left = pg_binding_face(&dimensions, cube,
		pg_dimension_map(&dimensions, 0, 1, &zero));
	const struct pg_binding_face *right = pg_binding_face(&dimensions, cube,
		pg_dimension_map(&dimensions, 0, 1, &one));
	const struct pg_binding_face *center = pg_binding_face(&dimensions, cube,
		pg_dimension_identity(&dimensions, 1));
	assert(left && right && center);
	const struct pg_evidence *lp = pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *rp = pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_RIGHT_TYPE);
	const struct pg_evidence *lq = pg_prove_identity_endpoint_type(typing, classifiers, q, PG_IDENTITY_LEFT_TYPE);
	assert(lp && rp && lq && lp != lq);
	assert(pg_evidence_premise(lp, 0) == p && pg_evidence_premise(lq, 0) == q);
	assert(pg_evidence_judgement(lp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(lp)->core == pg_evidence_subject(lq)->core);
	assert(pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_LEFT_TYPE) == lp);
	assert(!pg_prove_identity_endpoint_type(typing, classifiers, p, PG_PI_DOMAIN));
	assert(!pg_prove_identity_endpoint_type(typing, classifiers, NULL, PG_IDENTITY_LEFT_TYPE));
	const struct pg_evidence *cp = pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable);
	const struct pg_evidence *cq = pg_identity_context_extend(typing, classifiers, scope, q,
		&left->variable, &right->variable, &center->variable);
	assert(cp && cq && cp != cq);
	assert(pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable) == cp);
	const struct pg_context *context = pg_evidence_context(cp);
	assert(context->parent->parent->parent == pg_evidence_context(scope));
	assert(context->parent->parent->declared_type == pg_evidence_subject(lp)->core);
	assert(context->parent->declared_type == pg_evidence_subject(rp)->core);
	assert(context->declared_type == pg_identity_instance(typing->graph, pg_evidence_subject(p)->core,
		pg_reference(typing->graph, &left->variable), pg_reference(typing->graph, &right->variable)));
	assert(context->declared_type != pg_evidence_context(cq)->declared_type);
	const struct pg_evidence *variable = pg_prove_variable(typing, cp, &center->variable);
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, cp, variable);
	assert(type && pg_evidence_classifier(type) == pg_evidence_classifier(lp));
	assert(pg_evidence_subject(type)->core == context->declared_type);
	assert(pg_evidence_classifier(variable) == context->declared_type);
	const struct pg_evidence *extensions[] = {
		pg_evidence_premise(pg_evidence_premise(cp, 0), 0), pg_evidence_premise(cp, 0), cp
	};
	const struct pg_object *binders[] = {&left->variable, &right->variable, &center->variable};
	const struct pg_evidence *lifted = substitution;
	for (size_t i = 0; i < 3; ++i) {
		lifted = pg_prove_substitution_lift(typing, lifted, extensions[i], binders[i]);
		assert(lifted);
	}
	assert(pg_evidence_context(pg_evidence_premise(lifted, 1)) == pg_evidence_context(cq));
	const struct pg_evidence *moved = pg_prove_reindex(typing, lifted, variable);
	assert(moved && pg_evidence_classifier(moved) == pg_evidence_context(cq)->declared_type);
	assert(pg_evidence_subject(moved)->core == pg_evidence_subject(variable)->core);
	/* The dependent boundary uses the existing raw computation Pi/Lambda rules. */
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, variable);
	for (size_t i = 3; i; --i) {
		const struct pg_evidence *codomain = pg_prove_classifier(typing, classifiers, extensions[i - 1], body);
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers,
			pg_evidence_premise(extensions[i - 1], 1), extensions[i - 1], codomain);
		body = pg_prove_lambda(typing, pi, body);
		assert(body && pg_evidence_judgement(body) == PG_JUDGEMENT_COMPUTATION);
	}
	assert(pg_evidence_context(body) == pg_evidence_context(scope));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &left->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &right->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		pg_evidence_context(scope)->binder, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		NULL, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, pg_prove_empty_context(typing), p,
		&left->variable, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope,
		pg_prove_return(typing, classifiers, p), &left->variable, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, variable,
		&left->variable, &right->variable, &center->variable));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_identity_context_extend(&foreign, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable));
	pg_typing_destroy(&foreign);
	pg_dimensions_destroy(&dimensions);
}

static void dependent_families(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *a, const struct pg_evidence *b,
	const struct pg_evidence *x, const struct pg_evidence *y,
	const struct pg_evidence *p, const struct pg_evidence *q, const struct pg_evidence *refl_a)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *z = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, z, universe);
	const struct pg_evidence *family = pg_prove_value_type(typing, pg_prove_variable(typing, source, z));
	const struct pg_evidence *ls = pg_prove_substitution(typing, source, scope, 1, &a);
	const struct pg_evidence *rs = pg_prove_substitution(typing, source, scope, 1, &b);
	assert(ls && rs && family);
	const struct pg_evidence *vp = pg_prove_family_identity_type(typing, family, ls, rs, p, x, y);
	const struct pg_evidence *vq = pg_prove_family_identity_type(typing, family, ls, rs, q, x, y);
	assert(vp && vq && vp != vq);
	assert(pg_evidence_classifier(vp) == pg_universe(classifiers, 0));
	assert(pg_evidence_judgement(vp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(vp)->core != pg_evidence_subject(vq)->core);
	assert(pg_evidence_premise(vp, 0) == family && pg_evidence_premise(vp, 3) == p);
	assert(pg_prove_family_identity_type(typing, family, ls, rs, p, x, y) == vp);
	const struct pg_term *abstraction = pg_lambda(typing->graph, z, pg_evidence_subject(family)->core);
	const struct pg_term *expected = pg_identity_instance(typing->graph,
		pg_identity_apply(typing->graph, abstraction, pg_evidence_subject(a)->core,
			pg_evidence_subject(b)->core, pg_evidence_subject(p)->core),
		pg_evidence_subject(x)->core, pg_evidence_subject(y)->core);
	assert(pg_evidence_subject(vp)->core == expected);
	const struct pg_evidence *cf = pg_prove_return_type(typing, classifiers, family);
	const struct pg_evidence *rx = pg_prove_return(typing, classifiers, x);
	const struct pg_evidence *ry = pg_prove_return(typing, classifiers, y);
	const struct pg_evidence *cp = pg_prove_family_identity_type(typing, cf, ls, rs, p, rx, ry);
	assert(cp && pg_evidence_judgement(cp) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(cp) == pg_evidence_classifier(vp));
	assert(!pg_prove_type_value(typing, cp));
	assert(!pg_prove_context_extension(typing, scope, pg_binder(typing->graph), cp));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, p, y, x));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, x, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, refl_a, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, p, rx, ry));
	assert(!pg_prove_family_identity_type(typing, cf, ls, rs, p, x, y));
	assert(!pg_prove_family_identity_type(typing, family, rs, ls, p, y, x));
	assert(!pg_prove_family_identity_type(typing, universe, ls, rs, p, a, b));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, NULL, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, p, NULL, y));

	/* A dependent family of identifications, with a common ambient type t:
	 * C(z) = Id Universe t z. Vary z along p, retaining t := A on both faces. */
	const struct pg_object *t = pg_binder(typing->graph);
	const struct pg_evidence *prefix = pg_prove_context_extension(typing, empty, t, universe);
	source = pg_prove_context_extension(typing, prefix, z, pg_prove_projection(typing, prefix, universe));
	const struct pg_evidence *higher = pg_prove_identity_type(typing,
		pg_prove_projection(typing, source, universe),
		pg_prove_variable(typing, source, t), pg_prove_variable(typing, source, z));
	const struct pg_evidence *li[] = {a, a}, *ri[] = {a, b}, *wrong[] = {b, b};
	ls = pg_prove_substitution(typing, source, scope, 2, li);
	rs = pg_prove_substitution(typing, source, scope, 2, ri);
	const struct pg_evidence *hs = pg_prove_family_identity_type(typing, higher, ls, rs, p, refl_a, p);
	assert(hs && pg_evidence_classifier(hs) == pg_universe(classifiers, 1));
	assert(pg_prove_family_identity_type(typing, higher, ls, rs, p, refl_a, p) == hs);
	const struct pg_evidence *bad = pg_prove_substitution(typing, source, scope, 2, wrong);
	assert(bad && !pg_prove_family_identity_type(typing, higher, ls, bad, p, refl_a, p));
	const struct pg_term *ambient_type = pg_identity_instance(typing->graph,
		pg_identity_action(typing->graph, pg_universe(classifiers, 0)),
		pg_evidence_subject(a)->core, pg_reference(typing->graph, z));
	expected = pg_identity_instance(typing->graph,
		pg_identity_apply(typing->graph, pg_lambda(typing->graph, z, ambient_type),
			pg_evidence_subject(a)->core, pg_evidence_subject(b)->core, pg_evidence_subject(p)->core),
		pg_evidence_subject(refl_a)->core, pg_evidence_subject(p)->core);
	assert(pg_alpha_equal(pg_evidence_subject(hs)->core, expected) == 1);
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_prove_family_identity_type(&foreign, higher, ls, rs, p, refl_a, p));
	pg_typing_destroy(&foreign);
}

static void dependent_pi_action(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *z = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, z, universe);
	const struct pg_evidence *value = pg_prove_variable(typing, source, z);
	const struct pg_evidence *refl = pg_prove_reflexivity(typing,
		pg_prove_projection(typing, source, universe), value);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, refl);
	const struct pg_evidence *codomain = pg_prove_classifier(typing, classifiers, source, body);
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, universe, source, codomain);
	const struct pg_evidence *function = pg_prove_lambda(typing, pi, body);
	assert(function);
	const struct pg_evidence *witness = pg_prove_reflexivity(typing, pi, function);
	const struct pg_object *x0 = pg_binder(typing->graph), *x1 = pg_binder(typing->graph), *p = pg_binder(typing->graph);
	const struct pg_evidence *expanded = pg_identity_pi_type(typing, classifiers, empty, pi,
		function, function, x0, x1, p);
	assert(expanded && pg_evidence_judgement(expanded) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_identity_pi_type(typing, classifiers, empty, pi, function, function, x0, x1, p) == expanded);
	assert(pg_evidence_classifier(expanded) == pg_evidence_classifier(pi));
	const struct pg_evidence *inner = expanded;
	for (size_t i = 0; i < 3; ++i) {
		assert(pg_evidence_rule(inner) == PG_PI_FORM);
		inner = pg_evidence_premise(inner, 2);
	}
	assert(pg_evidence_rule(inner) == PG_FAMILY_IDENTITY_FORM);
	assert(pg_evidence_subject(pg_evidence_premise(inner, 3))->core == pg_reference(typing->graph, p));
	struct pg_whnf_work work;
	struct pg_conversion comparison;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(witness),
		pg_evidence_subject(expanded)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing, witness, expanded,
		pg_conversion_certificate(&comparison));
	assert(converted);
	pg_conversion_destroy(&comparison);
	const struct pg_object *a = pg_binder(typing->graph);
	const struct pg_evidence *arguments = pg_prove_context_extension(typing, empty, a, universe);
	const struct pg_evidence *av = pg_prove_variable(typing, arguments, a);
	const struct pg_evidence *ap = pg_prove_reflexivity(typing,
		pg_prove_projection(typing, arguments, universe), av);
	const struct pg_evidence *application = pg_prove_projection(typing, arguments, converted);
	application = pg_prove_application(typing, application, av);
	application = pg_prove_application(typing, application, av);
	assert(application && !pg_prove_application(typing, application, av));
	application = pg_prove_application(typing, application, ap);
	assert(application && pg_evidence_judgement(application) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_prove_classifier(typing, classifiers, arguments, application));
	const struct pg_term *input = pg_evidence_classifier(witness);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input);
	while (pg_whnf_status(job) == PG_EVAL_PENDING) pg_whnf_advance(job, 1);
	assert(pg_whnf_status(job) == PG_EVAL_WHNF);
	const struct pg_term *domain, *tail;
	const struct pg_object *binder;
	assert(pg_pi_view(pg_whnf_result(job), &domain, &binder, &tail));
	assert(domain == pg_evidence_subject(universe)->core);
	struct pg_whnf_job *beta = pg_whnf_request(&work, &pg_beta_policy, input);
	assert(pg_whnf_advance(beta, 1000) == PG_EVAL_WHNF && pg_whnf_result(beta) == input);
	/* The type rule retains endpoint computations, even nonterminating ones. */
	const struct pg_term *v = pg_reference(typing->graph, z);
	const struct pg_term *self = pg_lambda(typing->graph, z, pg_application(typing->graph, v, v));
	const struct pg_term *omega = pg_application(typing->graph, self, self);
	input = pg_identity_instance(typing->graph, pg_identity_action(typing->graph,
		pg_evidence_subject(pi)->core), omega, omega);
	job = pg_whnf_request(&work, &pg_pure_policy, input);
	assert(pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	assert(pg_pi_view(pg_whnf_result(job), &domain, &binder, &tail));
	assert(!pg_identity_pi_type(typing, classifiers, empty, pi, function, function, x0, x0, p));
	assert(!pg_identity_pi_type(typing, classifiers, empty, pi, function, value, x0, x1, p));
	assert(!pg_identity_pi_type(typing, classifiers, empty, universe, function, function, x0, x1, p));
	pg_whnf_work_destroy(&work);
}

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	dependent_pi_action(&typing, &classifiers);
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
	struct pg_whnf_work beta;
	struct pg_conversion comparison;
	assert(pg_whnf_work_init(&beta, &graph) == 0);
	assert(pg_conversion_init(&comparison, &beta, pg_evidence_classifier(refl_p),
		pg_evidence_subject(p_to_q)->core) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_prove_conversion(&typing, refl_p, p_to_q, pg_conversion_certificate(&comparison)));
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&beta);
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
	const struct pg_evidence *return_refl = pg_prove_return(&typing, &classifiers, refl_x);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(crefl)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == pg_evidence_subject(return_refl)->core);
	pg_eval_destroy(&machine);
	struct pg_whnf_work normalization;
	assert(pg_whnf_work_init(&normalization, &graph) == 0);
	const struct pg_term *input = pg_evidence_subject(crefl)->core;
	struct pg_whnf_job *beta_job = pg_whnf_request(&normalization, &pg_beta_policy, input);
	assert(pg_whnf_advance(beta_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(beta_job) == input);
	normalizes(&normalization, input, pg_evidence_subject(return_refl)->core);
	assert(beta_job != pg_whnf_request(&normalization, &pg_pure_policy, input));
	assert(pg_whnf_result(beta_job) == input);
	assert(!pg_whnf_request(&normalization, NULL, input));
	const struct pg_evidence *return_identity = pg_prove_return_type(&typing, &classifiers, diagonal);
	normalizes(&normalization, pg_evidence_subject(cid)->core, pg_evidence_subject(return_identity)->core);
	assert(pg_conversion_init(&comparison, &normalization, pg_evidence_classifier(crefl),
		pg_evidence_subject(return_identity)->core) == 0);
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&comparison) < 1000);
	assert(pg_conversion_status(&comparison) == PG_CONVERSION_EQUAL);
	assert(pg_prove_conversion(&typing, crefl, return_identity, pg_conversion_certificate(&comparison)));
	pg_conversion_destroy(&comparison);
	const struct pg_evidence *ufa = pg_prove_thunk_type(&typing, &classifiers, fa);
	const struct pg_evidence *delayed = pg_prove_thunk(&typing, &classifiers, returned);
	const struct pg_evidence *urefl = pg_prove_reflexivity(&typing, ufa, delayed);
	normalizes(&normalization, pg_evidence_subject(urefl)->core, pg_evidence_subject(quoted)->core);
	const struct pg_evidence *uid = pg_prove_identity_type(&typing, ufa, delayed, delayed);
	const struct pg_evidence *ucid = pg_prove_thunk_type(&typing, &classifiers, cid);
	normalizes(&normalization, pg_evidence_subject(uid)->core, pg_evidence_subject(ucid)->core);
	assert(pg_conversion_init(&comparison, &normalization, pg_evidence_classifier(urefl),
		pg_evidence_classifier(quoted)) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	/* WHNF respects suspension. Normalizing a family never needs an unhandled
	 * operation's answer or the contents of a THUNK to choose this equation. */
	const struct pg_object *v = pg_binder(&graph);
	const struct pg_term *vv = pg_reference(&graph, v);
	/* Another evaluator policy cannot authorize the checker's conversion. */
	static const struct pg_eval_policy other_policy = {first_argument};
	const struct pg_term *opaque_app = pg_application(&graph, vv, pg_evidence_subject(xx)->core);
	struct pg_whnf_job *other_job = pg_whnf_request(&normalization, &other_policy, opaque_app);
	assert(pg_whnf_advance(other_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(other_job) == pg_evidence_subject(xx)->core);
	assert(pg_conversion_init(&comparison, &normalization, opaque_app, pg_evidence_subject(xx)->core) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	const struct pg_term *self = pg_lambda(&graph, v, pg_application(&graph, vv, vv));
	const struct pg_term *omega = pg_application(&graph, self, self);
	const struct pg_term *thunk = pg_reference(&graph, &pg_thunk_operation);
	const struct pg_term *suspended = pg_application(&graph, thunk, omega);
	normalizes(&normalization, pg_identity_action(&graph, suspended),
		pg_application(&graph, thunk, pg_identity_action(&graph, omega)));
	const struct pg_term *ufamily = pg_identity_action(&graph, pg_evidence_subject(ufa)->core);
	normalizes(&normalization, pg_identity_instance(&graph, ufamily, suspended, suspended),
		pg_thunk_type(&classifiers, pg_identity_instance(&graph,
			pg_identity_action(&graph, pg_evidence_subject(fa)->core), omega, omega)));
	const struct pg_term *ffamily = pg_identity_action(&graph, pg_evidence_subject(fa)->core);
	const struct pg_term *neutral = pg_identity_instance(&graph, ffamily, vv, omega);
	normalizes(&normalization, neutral, neutral);
	normalizes(&normalization, ffamily, ffamily);
	normalizes(&normalization, pg_evidence_subject(rp)->core, pg_evidence_subject(rp)->core);
	/* Beta exposure and lexical capture use the same evaluator demand frames. */
	const struct pg_term *ret = pg_reference(&graph, &pg_return_operation);
	const struct pg_term *captured = pg_lambda(&graph, v,
		pg_identity_instance(&graph, ffamily, pg_application(&graph, ret, vv), pg_evidence_subject(returned)->core));
	normalizes(&normalization, pg_application(&graph, captured, pg_evidence_subject(xx)->core),
		pg_evidence_subject(return_identity)->core);
	struct pg_whnf_work unsplit;
	assert(pg_whnf_work_init(&unsplit, &graph) == 0);
	struct pg_whnf_job *whole_job = pg_whnf_request(&unsplit, &pg_pure_policy, input);
	assert(pg_whnf_advance(whole_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(whole_job) == pg_whnf_result(pg_whnf_request(&normalization, &pg_pure_policy, input)));
	assert(pg_whnf_steps(whole_job) == pg_whnf_steps(pg_whnf_request(&normalization, &pg_pure_policy, input)));
	pg_whnf_work_destroy(&unsplit);
	pg_whnf_work_destroy(&normalization);

	/* No new authority for reindexing Identity: the family parameter stays a
	 * visible Core operand and is substituted by the existing traversal. */
	size_t count = 6;
	const struct pg_evidence *images[] = {
		a_value, pg_prove_projection(&typing, scope, right_type), qq, qq, xx, yy
	};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, scope, scope, count, images);
	assert(sigma);
	boundary_context(&typing, &classifiers, scope, pp, qq, sigma);
	dependent_families(&typing, &classifiers, scope, a_value,
		pg_prove_projection(&typing, scope, right_type), xx, yy, pp, qq, id_a);
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
	puts("identity: chosen boundary contexts, F/U action, fixed pure conversion, policy isolation and reindex passed");
	return 0;
}
