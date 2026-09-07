#include "graph.h"
#include "dimension.h"
#include "eval.h"
#include "typing.h"
#include "conversion.h"
#include "classifier.h"
#include "evidence.h"
#include "computation.h"
#include "action.h"

#include <assert.h>
#include <stdio.h>

static void graph_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *vz = pg_reference(graph, z);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	assert(identity == pg_lambda(graph, x, vx));
	assert(identity != pg_lambda(graph, y, vy));
	assert(pg_alpha_equal(identity, pg_lambda(graph, y, vy)) == 1);
	assert(vx != vy);
	assert(pg_lambda(graph, x, vy) != identity);
	const struct pg_term *first = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *second = pg_lambda(graph, x, pg_lambda(graph, y, vy));
	assert(first != second);
	assert(!pg_alpha_equal(first, second));
	assert(pg_alpha_equal(first, pg_lambda(graph, y, pg_lambda(graph, z, vy))) == 1);
	assert(pg_lambda(graph, x, vz) != pg_lambda(graph, y, vz));
	assert(pg_alpha_equal(pg_lambda(graph, x, vz), pg_lambda(graph, y, vz)) == 1);
	assert(pg_lambda(graph, x, vz) != pg_lambda(graph, x, vy));
	const struct pg_term *app = pg_application(graph, identity, vx);
	assert(app == pg_application(graph, identity, vx));
	assert(app != pg_application(graph, vx, identity));
	struct pg_object *opaque = pg_alloc(graph, sizeof(*opaque));
	assert(opaque);
	opaque->kind = PG_SEMANTIC_OBJECT;
	assert(!pg_lambda(graph, opaque, vx));
	assert(pg_reference(graph, opaque) != vx);
	for (size_t i = 0; i < 4096; ++i) {
		const struct pg_object *fresh = pg_binder(graph);
		assert(fresh);
		assert(pg_reference(graph, fresh));
	}
	assert(graph->terms.capacity > 1024);
	assert(vx->as.reference == x);
	assert(identity == pg_lambda(graph, x, vx));
	assert(app == pg_application(graph, identity, vx));
	/* These DAGs contain 40 application nodes each, not 2^40 independent
	 * subterms. Hashing and alpha comparison must preserve that sharing. */
	const struct pg_term *left_dag = vx;
	const struct pg_term *right_dag = vy;
	for (size_t i = 0; i < 40; ++i) {
		left_dag = pg_application(graph, left_dag, left_dag);
		right_dag = pg_application(graph, right_dag, right_dag);
	}
	const struct pg_term *left_lambda = pg_lambda(graph, x, left_dag);
	const struct pg_term *right_lambda = pg_lambda(graph, y, right_dag);
	assert(left_lambda != right_lambda);
	assert(pg_alpha_equal(left_lambda, right_lambda) == 1);
	assert(pg_alpha_equal(left_dag, right_dag) == 0);
}

static const struct pg_dimension_map *maps[128];
static size_t map_count;

static void context_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	assert(pg_typing_init(&typing, graph) == 0);
	struct pg_object *type_a = pg_alloc(graph, sizeof(*type_a));
	struct pg_object *type_b = pg_alloc(graph, sizeof(*type_b));
	assert(type_a && type_b);
	type_a->kind = PG_SEMANTIC_OBJECT;
	type_b->kind = PG_SEMANTIC_OBJECT;
	const struct pg_term *a = pg_reference(graph, type_a);
	const struct pg_term *b = pg_reference(graph, type_b);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_context *in_a = pg_context_bind(&typing, NULL, x, a);
	const struct pg_context *in_b = pg_context_bind(&typing, NULL, x, b);
	assert(in_a && in_b);
	assert(in_a != in_b);
	assert(in_a == pg_context_bind(&typing, NULL, x, a));
	const struct pg_term *identity = pg_lambda(graph, x, pg_reference(graph, x));
	assert(identity == pg_lambda(graph, in_a->binder, pg_reference(graph, in_a->binder)));
	assert(identity == pg_lambda(graph, in_b->binder, pg_reference(graph, in_b->binder)));
	assert(pg_context_lookup(in_a, x)->declared_type == a);
	assert(pg_context_lookup(in_b, x)->declared_type == b);
	assert(!pg_context_lookup(in_a, y));
	const struct pg_context *extended = pg_context_bind(&typing, in_a, y, b);
	assert(extended->parent == in_a);
	assert(pg_context_lookup(extended, x) == in_a);
	assert(pg_context_lookup(extended, y) == extended);
	for (size_t i = 0; i < 1000; ++i) {
		assert(pg_context_bind(&typing, in_a, pg_binder(graph), a));
	}
	assert(in_a == pg_context_bind(&typing, NULL, x, a));
	assert(in_a->declared_type == a);
	assert(in_b->declared_type == b);
	assert(!pg_context_bind(&typing, NULL, type_a, a));
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_occurrence *body_a = pg_occurrence(&typing, in_a, vx, NULL, 0, NULL);
	const struct pg_occurrence *body_b = pg_occurrence(&typing, in_b, vx, NULL, 0, NULL);
	assert(body_a && body_b && body_a != body_b);
	assert(body_a->core == body_b->core);
	const struct pg_occurrence *lambda_a = pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a);
	const struct pg_occurrence *lambda_b = pg_occurrence(&typing, NULL, identity, NULL, 1, &body_b);
	assert(lambda_a && lambda_b && lambda_a != lambda_b);
	assert(lambda_a->core == lambda_b->core);
	assert(lambda_a == pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a));
	assert(lambda_a->operands[0]->context == in_a);
	assert(lambda_b->operands[0]->context == in_b);
	const struct pg_occurrence *annotated = pg_occurrence(&typing, in_a, vx, a, 0, NULL);
	assert(annotated && annotated != body_a);
	assert(annotated->annotation == a);
	assert(!pg_occurrence(&typing, NULL, NULL, NULL, 0, NULL));
	assert(!pg_occurrence(&typing, NULL, identity, NULL, 1, NULL));
	assert(!pg_occurrence(&typing, NULL, identity, NULL, SIZE_MAX, &body_a));
	for (size_t i = 0; i < 1000; ++i) {
		const struct pg_term *variable = pg_reference(graph, pg_binder(graph));
		assert(pg_occurrence(&typing, NULL, variable, NULL, 0, NULL));
	}
	assert(lambda_a == pg_occurrence(&typing, NULL, identity, NULL, 1, &body_a));
	pg_typing_destroy(&typing);
	puts("typing inputs: persistent contexts and distinct occurrences over shared Core passed");
}

static void evidence_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	assert(empty && pg_evidence_rule(empty) == PG_CONTEXT_EMPTY);
	assert(empty == pg_prove_empty_context(&typing));
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *u1 = pg_prove_universe(&typing, &classifiers, empty, 1);
	assert(u0 && u1);
	assert(pg_evidence_classifier(u0) == pg_universe(&classifiers, 1));
	assert(pg_evidence_premise(u0, 0) == empty);
	assert(pg_evidence_premise_count(u0) == 1);
	assert(!pg_evidence_premise(u0, 1));
	assert(!pg_prove_universe(&typing, &classifiers, empty, UINT64_MAX));
	const struct pg_object *a = pg_binder(graph);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, a, u0);
	assert(a_context && pg_evidence_premise(a_context, 1) == u0);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_context, a);
	assert(a_type && pg_evidence_classifier(a_type) == pg_universe(&classifiers, 0));
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	assert(x_context);
	const struct pg_evidence *fa = pg_prove_return_type(&typing, &classifiers, a_type);
	assert(fa && pg_evidence_judgement(fa) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(!pg_prove_value_type(&typing, fa));
	assert(!pg_prove_context_extension(&typing, a_context, x, fa));
	assert(!pg_prove_return_type(&typing, &classifiers, fa));
	assert(!pg_prove_thunk_type(&typing, &classifiers, a_type));
	const struct pg_evidence *ufa = pg_prove_thunk_type(&typing, &classifiers, fa);
	assert(ufa && pg_evidence_judgement(ufa) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_prove_context_extension(&typing, a_context, pg_binder(graph), ufa));
	const struct pg_evidence *a_in_x = pg_prove_variable(&typing, x_context, a);
	const struct pg_evidence *fa_in_x = pg_prove_return_type(&typing, &classifiers, a_in_x);
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, a_type, x_context, fa_in_x);
	assert(pi && pg_evidence_judgement(pi) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(pi) == pg_universe(&classifiers, 0));
	assert(pg_prove_pi(&typing, &classifiers, a_type, x_context, fa_in_x) == pi);
	assert(pg_evidence_premise(pi, 2) == fa_in_x);
	assert(!pg_prove_pi(&typing, &classifiers, a_type, x_context, a_in_x));
	assert(!pg_prove_pi(&typing, &classifiers, a_type, x_context, fa));
	assert(!pg_prove_pi(&typing, &classifiers, u0, x_context, fa_in_x));
	assert(!pg_prove_context_extension(&typing, a_context, pg_binder(graph), pi));
	const struct pg_evidence *high = pg_prove_universe(&typing, &classifiers, x_context, 2);
	const struct pg_evidence *fhigh = pg_prove_return_type(&typing, &classifiers, high);
	const struct pg_evidence *high_pi = pg_prove_pi(&typing, &classifiers, a_type, x_context, fhigh);
	assert(high_pi && pg_evidence_classifier(high_pi) == pg_universe(&classifiers, 3));
	const struct pg_term *inner;
	assert(pg_thunk_type_view(pg_evidence_subject(ufa)->core, &inner));
	assert(inner == pg_evidence_subject(fa)->core);
	assert(pg_return_type_view(inner, &inner) && inner == pg_reference(graph, a));
	const struct pg_evidence *x_term = pg_prove_variable(&typing, x_context, x);
	assert(x_term && pg_evidence_classifier(x_term) == pg_reference(graph, a));
	const struct pg_evidence *returned = pg_prove_return(&typing, &classifiers, x_term);
	assert(returned && pg_evidence_classifier(returned) == pg_evidence_subject(fa_in_x)->core);
	assert(pg_prove_return_value(&typing, returned) == x_term);
	assert(!pg_prove_return_value(&typing, x_term));
	assert(!pg_prove_return_value(&typing, NULL));
	const struct pg_evidence *delayed = pg_prove_thunk(&typing, &classifiers, returned);
	const struct pg_evidence *forced = pg_prove_force(&typing, delayed);
	assert(forced && pg_evidence_classifier(forced) == pg_evidence_classifier(returned));
	assert(pg_evidence_subject(forced)->core != pg_evidence_subject(returned)->core);
	assert(pg_reduce_computation(&typing, x_context, forced) == returned);
	assert(!pg_reduce_computation(&typing, x_context, returned));
	assert(!pg_reduce_computation(&typing, x_context, x_term));
	assert(!pg_reduce_computation(&typing, x_context, NULL));
	assert(!pg_prove_force(&typing, x_term));
	assert(!pg_prove_return(&typing, &classifiers, returned));
	assert(!pg_prove_thunk(&typing, &classifiers, x_term));
	const struct pg_evidence *identity = pg_prove_lambda(&typing, pi, returned);
	assert(identity && pg_evidence_classifier(identity) == pg_evidence_subject(pi)->core);
	assert(!pg_prove_lambda(&typing, pi, x_term));
	assert(!pg_prove_lambda(&typing, high_pi, returned));
	assert(!pg_prove_application(&typing, identity, x_term)); /* Different scopes. */
	const struct pg_evidence *y_context = pg_prove_context_extension(&typing, x_context, y, a_in_x);
	const struct pg_evidence *y_term = pg_prove_variable(&typing, y_context, y);
	const struct pg_evidence *a_in_y = pg_prove_variable(&typing, y_context, a);
	const struct pg_evidence *fa_in_y = pg_prove_return_type(&typing, &classifiers, a_in_y);
	const struct pg_evidence *pi_y = pg_prove_pi(&typing, &classifiers, a_in_x, y_context, fa_in_y);
	const struct pg_evidence *return_y = pg_prove_return(&typing, &classifiers, y_term);
	const struct pg_evidence *identity_y = pg_prove_lambda(&typing, pi_y, return_y);
	const struct pg_evidence *app = pg_prove_application(&typing, identity_y, x_term);
	assert(app && pg_evidence_classifier(app) == pg_evidence_classifier(returned));
	assert(pg_prove_application(&typing, identity_y, x_term) == app);
	assert(pg_evidence_premise(app, 0) == identity_y);
	const struct pg_evidence *reduct = pg_reduce_beta(&typing, x_context, app);
	assert(reduct && pg_evidence_rule(reduct) == PG_REINDEX);
	assert(pg_evidence_subject(reduct)->core == pg_evidence_subject(returned)->core);
	assert(pg_evidence_classifier(reduct) == pg_evidence_classifier(app));
	assert(pg_reduce_beta(&typing, x_context, app) == reduct);
	assert(pg_reduce_computation(&typing, x_context, app) == reduct);
	assert(!pg_reduce_beta(&typing, y_context, app));
	assert(!pg_reduce_beta(&typing, x_context, returned));
	const struct pg_evidence *weakened_function = pg_prove_projection(&typing, y_context, identity_y);
	const struct pg_evidence *weakened_app = pg_prove_application(&typing, weakened_function, y_term);
	const struct pg_evidence *weakened_reduct = pg_reduce_beta(&typing, y_context, weakened_app);
	assert(weakened_reduct && pg_evidence_subject(weakened_reduct)->core == pg_evidence_subject(return_y)->core);
	assert(!pg_prove_application(&typing, identity_y, returned));
	assert(!pg_prove_application(&typing, identity_y, a_in_x));
	const struct pg_evidence *quoted_function = pg_prove_thunk(&typing, &classifiers, identity_y);
	assert(!pg_prove_application(&typing, quoted_function, x_term));
	assert(pg_prove_application(&typing, pg_prove_force(&typing, quoted_function), x_term));
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *z_context = pg_prove_context_extension(&typing, x_context, z, a_in_x);
	const struct pg_evidence *a_in_z = pg_prove_variable(&typing, z_context, a);
	const struct pg_evidence *fa_in_z = pg_prove_return_type(&typing, &classifiers, a_in_z);
	const struct pg_evidence *pi_z = pg_prove_pi(&typing, &classifiers, a_in_x, z_context, fa_in_z);
	const struct pg_evidence *upi_z = pg_prove_thunk_type(&typing, &classifiers, pi_z);
	const struct pg_term *old_classifier = pg_evidence_classifier(quoted_function);
	const struct pg_term *new_classifier = pg_evidence_subject(upi_z)->core;
	assert(old_classifier != new_classifier);
	const struct pg_object *f = pg_binder(graph);
	const struct pg_evidence *f_context = pg_prove_context_extension(&typing, x_context, f, upi_z);
	const struct pg_evidence *f_body = pg_prove_projection(&typing, f_context, returned);
	const struct pg_evidence *f_pi = pg_prove_pi(&typing, &classifiers, upi_z, f_context,
		pg_prove_projection(&typing, f_context, fa_in_x));
	const struct pg_evidence *ignore_function = pg_prove_lambda(&typing, f_pi, f_body);
	assert(pg_prove_application(&typing, ignore_function, quoted_function));
	assert(!pg_prove_application(&typing, ignore_function, delayed));
	assert(pg_prove_fold(&typing, pg_prove_return(&typing, &classifiers, quoted_function), ignore_function));
	assert(!pg_prove_fold(&typing, returned, ignore_function));
	assert(pg_evidence_classifier(quoted_function) == old_classifier);
	struct pg_beta_work work;
	struct pg_conversion comparison;
	assert(pg_beta_work_init(&work, graph) == 0);
	assert(pg_conversion_init(&comparison, &work, old_classifier, new_classifier) == 0);
	assert(!pg_conversion_certificate(&comparison));
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(!pg_conversion_certificate(&comparison));
	const struct pg_conversion_certificate *certificate = pg_conversion_certificate(&comparison);
	assert(certificate);
	pg_conversion_destroy(&comparison);
	pg_beta_work_destroy(&work);
	const struct pg_evidence *converted = pg_prove_conversion(&typing, quoted_function, upi_z, certificate);
	assert(converted && pg_evidence_classifier(converted) == new_classifier);
	assert(pg_evidence_classifier(quoted_function) == old_classifier);
	assert(pg_evidence_subject(converted) == pg_evidence_subject(quoted_function));
	assert(pg_evidence_conversion(converted) == certificate);
	assert(pg_evidence_premise(converted, 0) == quoted_function);
	assert(pg_evidence_premise(converted, 1) == upi_z);
	assert(pg_prove_conversion(&typing, quoted_function, upi_z, certificate) == converted);
	assert(!pg_prove_conversion(&typing, quoted_function, upi_z, NULL));
	assert(!pg_prove_conversion(&typing, x_term, upi_z, certificate));
	assert(!pg_prove_conversion(&typing, quoted_function, ufa, certificate));
	assert(!pg_prove_conversion(&typing, quoted_function, pi_z, certificate));
	assert(pg_prove_application(&typing, pg_prove_force(&typing, converted), x_term));
	const struct pg_evidence *folded = pg_prove_fold(&typing, returned, identity_y);
	assert(folded && pg_evidence_classifier(folded) == pg_evidence_classifier(returned));
	assert(!pg_prove_fold(&typing, x_term, identity_y));
	assert(!pg_prove_fold(&typing, returned, quoted_function));
	assert(pg_reduce_computation(&typing, x_context, folded) == app);
	assert(!pg_reduce_computation(&typing, y_context, folded));
	const struct pg_evidence *reindexed_fold = pg_prove_fold(&typing, reduct, identity_y);
	const struct pg_evidence *reindexed_app = pg_reduce_computation(&typing, x_context, reindexed_fold);
	assert(reindexed_app && pg_evidence_rule(reindexed_app) == PG_APP_ELIM);
	assert(pg_evidence_subject(reindexed_app)->core == pg_evidence_subject(app)->core);
	const struct pg_evidence *reindexed_result = pg_reduce_computation(&typing, x_context, reindexed_app);
	assert(reindexed_result && pg_evidence_subject(reindexed_result)->core == pg_evidence_subject(returned)->core);
	const struct pg_evidence *weakened_delayed = pg_prove_projection(&typing, y_context, delayed);
	const struct pg_evidence *weakened_forced = pg_prove_force(&typing, weakened_delayed);
	const struct pg_evidence *weakened_return = pg_prove_projection(&typing, y_context, returned);
	assert(pg_reduce_computation(&typing, y_context, weakened_forced) == weakened_return);
	const struct pg_evidence *weakened_fold = pg_prove_fold(&typing, weakened_return, weakened_function);
	const struct pg_evidence *weakened_fold_result = pg_reduce_computation(&typing, y_context, weakened_fold);
	assert(weakened_fold_result && pg_evidence_context(weakened_fold_result) == pg_evidence_context(y_context));
	assert(pg_evidence_subject(pg_evidence_premise(weakened_fold_result, 1))->core == pg_reference(graph, x));
	const struct pg_evidence *substitution = pg_evidence_premise(reduct, 0);
	const struct pg_evidence *reindexed_thunk = pg_prove_reindex(&typing, substitution,
		pg_prove_thunk(&typing, &classifiers, return_y));
	const struct pg_evidence *reindexed_force = pg_prove_force(&typing, reindexed_thunk);
	assert(pg_reduce_computation(&typing, x_context, reindexed_force) == reduct);
	const struct pg_evidence *exposed = pg_prove_return_value(&typing, reduct);
	assert(exposed && pg_evidence_subject(exposed)->core == pg_evidence_subject(x_term)->core);
	assert(pg_evidence_classifier(exposed) == pg_evidence_classifier(x_term));
	assert(!pg_reduce_computation(&typing, x_context, pg_prove_force(&typing, converted)));
	size_t reduction_terms = graph->terms.count, reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_reduce_computation(&typing, x_context, reindexed_force) == reduct);
		assert(pg_reduce_computation(&typing, x_context, reindexed_fold) == reindexed_app);
		assert(pg_reduce_computation(&typing, y_context, weakened_fold) == weakened_fold_result);
	}
	assert(graph->terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_evidence *fold_formation = pg_prove_classifier(&typing, &classifiers, x_context, folded);
	assert(fold_formation && pg_evidence_subject(fold_formation)->core == pg_evidence_classifier(folded));
	size_t fold_terms = graph->terms.count, fold_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_fold(&typing, returned, identity_y) == folded);
		assert(pg_prove_classifier(&typing, &classifiers, x_context, folded) == fold_formation);
	}
	assert(graph->terms.count == fold_terms && typing.proofs.count == fold_proofs);
	const struct pg_evidence *x_formation = pg_prove_classifier(&typing, &classifiers, x_context, x_term);
	assert(x_formation && pg_evidence_subject(x_formation)->core == pg_evidence_classifier(x_term));
	const struct pg_evidence *body_formation = pg_prove_classifier(&typing, &classifiers, x_context, returned);
	assert(body_formation && pg_evidence_subject(body_formation)->core == pg_evidence_classifier(returned));
	const struct pg_evidence *synth_pi = pg_prove_pi(&typing, &classifiers, a_type, x_context, body_formation);
	assert(synth_pi && pg_prove_lambda(&typing, synth_pi, returned));
	const struct pg_evidence *delayed_formation = pg_prove_classifier(&typing, &classifiers, x_context, delayed);
	assert(delayed_formation && pg_evidence_subject(delayed_formation)->core == pg_evidence_classifier(delayed));
	const struct pg_evidence *forced_formation = pg_prove_classifier(&typing, &classifiers, x_context, forced);
	assert(forced_formation && pg_evidence_subject(forced_formation)->core == pg_evidence_classifier(forced));
	assert(pg_prove_classifier(&typing, &classifiers, a_context, identity) == pi);
	assert(pg_prove_classifier(&typing, &classifiers, x_context, converted) == upi_z);
	const struct pg_evidence *projected_x = pg_prove_projection(&typing, y_context, x_term);
	assert(projected_x && pg_evidence_subject(projected_x)->core == pg_evidence_subject(x_term)->core);
	assert(pg_evidence_context(projected_x) == pg_evidence_context(y_context));
	assert(pg_evidence_premise(projected_x, 1) == x_term);
	assert(pg_prove_projection(&typing, y_context, x_term) == projected_x);
	assert(pg_prove_projection(&typing, x_context, x_term) == x_term);
	assert(!pg_prove_projection(&typing, a_context, x_term));
	assert(!pg_prove_projection(&typing, z_context, y_term));
	assert(!pg_prove_projection(&typing, x_context, a_context));
	const struct pg_evidence *projected_formation = pg_prove_classifier(&typing, &classifiers, y_context, projected_x);
	assert(projected_formation && pg_evidence_subject(projected_formation)->core == pg_evidence_classifier(projected_x));
	assert(!pg_prove_classifier(&typing, &classifiers, y_context, x_term));
	struct pg_eval application_machine;
	pg_eval_init(&application_machine, pg_evidence_subject(app)->core);
	assert(pg_eval_advance(&application_machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&application_machine, graph) == pg_evidence_subject(returned)->core);
	pg_eval_destroy(&application_machine);
	assert(!pg_prove_context_extension(&typing, x_context, y, x_term));
	assert(!pg_prove_context_extension(&typing, a_context, a, a_type));
	assert(!pg_prove_context_extension(&typing, a_context, x, u0));
	assert(!pg_prove_variable(&typing, empty, x));
	assert(!pg_prove_variable(&typing, x_term, x));
	assert(x_term == pg_prove_variable(&typing, x_context, x));
	const struct pg_evidence *a_context_high = pg_prove_context_extension(&typing, empty, a, u1);
	const struct pg_evidence *a_type_high = pg_prove_variable(&typing, a_context_high, a);
	assert(a_type_high && a_type_high != a_type);
	assert(pg_evidence_subject(a_type)->core == pg_evidence_subject(a_type_high)->core);
	assert(pg_evidence_subject(a_type) != pg_evidence_subject(a_type_high));
	assert(pg_evidence_classifier(a_type) != pg_evidence_classifier(a_type_high));
	for (uint64_t i = 2; i < 300; ++i) assert(pg_prove_universe(&typing, &classifiers, empty, i));
	assert(u0 == pg_prove_universe(&typing, &classifiers, empty, 0));
	assert(pg_evidence_premise(a_context, 1) == u0);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("evidence: checked contexts, stratified universes and occurrence-based variable derivations passed");
}

static void dependent_application_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u1 = pg_prove_universe(&typing, &classifiers, empty, 1);
	const struct pg_object *a = pg_binder(graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, a, u1);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_context, a);
	const struct pg_evidence *fa = pg_prove_return_type(&typing, &classifiers, a_type);
	/* A : U1 |- F A computation type. No runtime result is guessed. */
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, u1, a_context, fa);
	assert(pi);
	assert(!pg_prove_type_value(&typing, pi));
	assert(!pg_prove_type_value(&typing, fa));
	const struct pg_evidence *upi = pg_prove_thunk_type(&typing, &classifiers, pi);
	assert(pg_prove_type_value(&typing, upi));
	const struct pg_object *f = pg_binder(graph);
	const struct pg_evidence *f_context = pg_prove_context_extension(&typing, empty, f, upi);
	const struct pg_evidence *function = pg_prove_force(&typing, pg_prove_variable(&typing, f_context, f));
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, f_context, 0);
	const struct pg_evidence *argument = pg_prove_type_value(&typing, u0);
	assert(argument && pg_evidence_judgement(argument) == PG_JUDGEMENT_VALUE);
	assert(pg_prove_type_value(&typing, u0) == argument);
	const struct pg_evidence *app = pg_prove_application(&typing, function, argument);
	assert(app);
	assert(pg_evidence_classifier(app) == pg_return_type(&classifiers, pg_universe(&classifiers, 0)));
	assert(pg_evidence_premise(app, 1) == argument);
	assert(!pg_prove_pi_constant_codomain(&typing, pi));
	assert(!pg_prove_fold(&typing, pg_prove_return(&typing, &classifiers, argument), function));
	const struct pg_evidence *app_formation = pg_prove_classifier(&typing, &classifiers, f_context, app);
	assert(app_formation && pg_evidence_subject(app_formation)->core == pg_evidence_classifier(app));
	assert(!pg_prove_application(&typing, function, u0));
	/* Open value arguments stay symbolic: the output is F B, not F U0. */
	const struct pg_evidence *u1_in_f = pg_prove_universe(&typing, &classifiers, f_context, 1);
	const struct pg_object *b = pg_binder(graph);
	const struct pg_evidence *b_context = pg_prove_context_extension(&typing, f_context, b, u1_in_f);
	const struct pg_evidence *open_function = pg_prove_force(&typing, pg_prove_variable(&typing, b_context, f));
	const struct pg_evidence *b_value = pg_prove_variable(&typing, b_context, b);
	const struct pg_evidence *open_app = pg_prove_application(&typing, open_function, b_value);
	assert(open_app);
	assert(pg_evidence_classifier(open_app) == pg_return_type(&classifiers, pg_reference(graph, b)));
	const struct pg_evidence *open_formation = pg_prove_classifier(&typing, &classifiers, b_context, open_app);
	assert(open_formation && pg_evidence_subject(open_formation)->core == pg_evidence_classifier(open_app));
	/* A returning function leaves binders in the substituted classifier. */
	const struct pg_object *x = pg_binder(graph), *g = pg_binder(graph);
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	const struct pg_evidence *inner_fa = pg_prove_return_type(&typing, &classifiers, pg_prove_variable(&typing, x_context, a));
	const struct pg_evidence *inner_pi = pg_prove_pi(&typing, &classifiers, a_type, x_context, inner_fa);
	const struct pg_evidence *outer_pi = pg_prove_pi(&typing, &classifiers, u1, a_context, inner_pi);
	const struct pg_evidence *g_context = pg_prove_context_extension(&typing, empty, g,
		pg_prove_thunk_type(&typing, &classifiers, outer_pi));
	const struct pg_evidence *g_term = pg_prove_force(&typing, pg_prove_variable(&typing, g_context, g));
	const struct pg_evidence *g_argument = pg_prove_type_value(&typing, pg_prove_universe(&typing, &classifiers, g_context, 0));
	const struct pg_evidence *g_app = pg_prove_application(&typing, g_term, g_argument);
	const struct pg_evidence *g_formation = pg_prove_classifier(&typing, &classifiers, g_context, g_app);
	assert(g_app && g_formation);
	size_t terms = graph->terms.count, proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_application(&typing, g_term, g_argument) == g_app);
		assert(pg_prove_classifier(&typing, &classifiers, g_context, g_app) == g_formation);
	}
	assert(graph->terms.count == terms && typing.proofs.count == proofs);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("dependent application: concrete and open type arguments substitute without executing computations");
}

static void typed_substitution_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 1);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_object *b = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_evidence *a_scope = pg_prove_context_extension(&typing, empty, a, universe);
	const struct pg_evidence *b_scope = pg_prove_context_extension(&typing, empty, b, universe);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_scope, a);
	const struct pg_evidence *b_type = pg_prove_variable(&typing, b_scope, b);
	const struct pg_evidence *source = pg_prove_context_extension(&typing, a_scope, x, a_type);
	const struct pg_evidence *destination = pg_prove_context_extension(&typing, b_scope, y, b_type);
	const struct pg_evidence *source_x = pg_prove_variable(&typing, source, x);
	const struct pg_evidence *destination_b = pg_prove_variable(&typing, destination, b);
	const struct pg_evidence *destination_y = pg_prove_variable(&typing, destination, y);
	const struct pg_evidence *images[] = {destination_b, destination_y};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, source, destination, 2, images);
	assert(sigma && pg_evidence_judgement(sigma) == PG_JUDGEMENT_SUBSTITUTION);
	assert(pg_prove_substitution(&typing, source, destination, 2, images) == sigma);
	const struct pg_evidence *reindexed = pg_prove_reindex(&typing, sigma, source_x);
	assert(reindexed && pg_evidence_subject(reindexed)->core == pg_reference(graph, y));
	assert(pg_evidence_classifier(reindexed) == pg_reference(graph, b));
	assert(pg_evidence_context(reindexed) == pg_evidence_context(destination));
	assert(pg_evidence_premise(reindexed, 0) == sigma);
	assert(pg_evidence_premise(reindexed, 1) == source_x);
	assert(pg_evidence_subject(source_x)->core == pg_reference(graph, x));
	const struct pg_evidence *formation = pg_prove_classifier(&typing, &classifiers, destination, reindexed);
	assert(formation && pg_evidence_subject(formation)->core == pg_reference(graph, b));
	const struct pg_evidence *returned = pg_prove_return(&typing, &classifiers, source_x);
	const struct pg_evidence *reindexed_return = pg_prove_reindex(&typing, sigma, returned);
	assert(reindexed_return && pg_evidence_classifier(reindexed_return) == pg_return_type(&classifiers, pg_reference(graph, b)));
	assert(!pg_prove_reindex(&typing, sigma, destination_y));
	assert(!pg_prove_reindex(&typing, sigma, source));
	assert(!pg_prove_projection(&typing, destination, sigma));
	assert(!pg_prove_substitution(&typing, source, destination, 1, images));
	const struct pg_evidence *bad[] = {destination_y, destination_b};
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	bad[0] = destination_b;
	bad[1] = pg_prove_return(&typing, &classifiers, destination_y);
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	bad[1] = source_x;
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	const struct pg_evidence *closed = pg_prove_substitution(&typing, empty, destination, 0, NULL);
	assert(closed && pg_prove_reindex(&typing, closed, universe));
	const struct pg_object *c = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_evidence *c_scope = pg_prove_context_extension(&typing, empty, c, universe);
	const struct pg_evidence *c_type = pg_prove_variable(&typing, c_scope, c);
	const struct pg_evidence *third = pg_prove_context_extension(&typing, c_scope, z, c_type);
	const struct pg_evidence *second_images[] = {
		pg_prove_variable(&typing, third, c), pg_prove_variable(&typing, third, z)
	};
	const struct pg_evidence *tau = pg_prove_substitution(&typing, destination, third, 2, second_images);
	const struct pg_evidence *composite = pg_prove_substitution_compose(&typing, sigma, tau);
	assert(composite);
	assert(!pg_prove_substitution_compose(&typing, tau, sigma));
	const struct pg_evidence *once = pg_prove_reindex(&typing, composite, returned);
	const struct pg_evidence *twice = pg_prove_reindex(&typing, tau, reindexed_return);
	assert(once && twice);
	assert(pg_evidence_subject(once)->core == pg_evidence_subject(twice)->core);
	assert(pg_evidence_classifier(once) == pg_evidence_classifier(twice));
	const struct pg_object *p = pg_binder(graph), *q = pg_binder(graph);
	const struct pg_evidence *a_in_source = pg_prove_variable(&typing, source, a);
	const struct pg_evidence *source_extension = pg_prove_context_extension(&typing, source, p, a_in_source);
	const struct pg_evidence *lifted = pg_prove_substitution_lift(&typing, sigma, source_extension, q);
	assert(lifted && pg_evidence_context(lifted)->declared_type == pg_reference(graph, b));
	const struct pg_evidence *source_p = pg_prove_variable(&typing, source_extension, p);
	const struct pg_evidence *lifted_p = pg_prove_reindex(&typing, lifted, source_p);
	assert(lifted_p && pg_evidence_subject(lifted_p)->core == pg_reference(graph, q));
	assert(pg_evidence_classifier(lifted_p) == pg_reference(graph, b));
	const struct pg_evidence *source_x_extended = pg_prove_variable(&typing, source_extension, x);
	const struct pg_evidence *lifted_x = pg_prove_reindex(&typing, lifted, source_x_extended);
	assert(lifted_x && pg_evidence_subject(lifted_x)->core == pg_reference(graph, y));
	assert(!pg_prove_substitution_lift(&typing, sigma, source_extension, y));
	assert(!pg_prove_substitution_lift(&typing, sigma, destination, q));
	const struct pg_evidence *a_extended = pg_prove_variable(&typing, source_extension, a);
	const struct pg_evidence *fa_extended = pg_prove_return_type(&typing, &classifiers, a_extended);
	const struct pg_evidence *source_pi = pg_prove_pi(&typing, &classifiers, a_in_source, source_extension, fa_extended);
	const struct pg_evidence *moved_pi = pg_prove_reindex(&typing, sigma, source_pi);
	const struct pg_evidence *codomain = pg_prove_pi_codomain(&typing, moved_pi, destination_y);
	assert(codomain && pg_evidence_subject(codomain)->core == pg_return_type(&classifiers, pg_reference(graph, b)));
	assert(pg_evidence_classifier(codomain) == pg_evidence_classifier(moved_pi));
	assert(!pg_prove_pi_codomain(&typing, moved_pi, destination_b));
	assert(!pg_prove_pi_codomain(&typing, moved_pi, source_x));
	assert(!pg_prove_pi_codomain(&typing, source_x, destination_y));
	const struct pg_evidence *source_upi = pg_prove_thunk_type(&typing, &classifiers, source_pi);
	const struct pg_evidence *moved_upi = pg_prove_reindex(&typing, sigma, source_upi);
	const struct pg_evidence *content = pg_prove_thunk_content(&typing, moved_upi);
	assert(content && pg_alpha_equal(pg_evidence_subject(content)->core, pg_evidence_subject(moved_pi)->core) == 1);
	assert(!pg_prove_thunk_content(&typing, moved_pi));
	assert(!pg_prove_thunk_content(&typing, universe));
	const struct pg_object *f = pg_binder(graph), *g = pg_binder(graph);
	const struct pg_evidence *function_context = pg_prove_context_extension(&typing, source, f, source_upi);
	const struct pg_evidence *function_lift = pg_prove_substitution_lift(&typing, sigma, function_context, g);
	assert(function_lift);
	size_t term_count = graph->terms.count;
	size_t proof_count = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_reindex(&typing, sigma, source_pi) == moved_pi);
		assert(pg_prove_reindex(&typing, sigma, source_upi) == moved_upi);
		assert(pg_prove_substitution_lift(&typing, sigma, function_context, g) == function_lift);
	}
	assert(graph->terms.count == term_count);
	assert(typing.proofs.count == proof_count);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("typed substitution: dependent declarations, simultaneous images and shared premise DAG passed");
}

static void typed_restriction_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_dimensions dimensions;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	const struct pg_dimension_map *identity = pg_dimension_identity(&dimensions, 2);
	const struct pg_binding_cube *a_cube = pg_binding_cube(&dimensions, 2);
	const struct pg_binding_cube *x_cube = pg_binding_cube(&dimensions, 2);
	const struct pg_binding_face *a = pg_binding_face(&dimensions, a_cube, identity);
	const struct pg_binding_face *x = pg_binding_face(&dimensions, x_cube, identity);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, &a->variable, universe);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_context, &a->variable);
	const struct pg_evidence *source = pg_prove_context_extension(&typing, a_context, &x->variable, a_type);
	const struct pg_evidence *source_x = pg_prove_variable(&typing, source, &x->variable);
	const struct pg_binding_face *bindings[] = {a, x};
	struct pg_coordinate edge_coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
	struct pg_coordinate vertex_coordinates[] = {{PG_ENDPOINT_ONE, 0}};
	const struct pg_dimension_map *edge = pg_dimension_map(&dimensions, 1, 2, edge_coordinates);
	const struct pg_dimension_map *vertex = pg_dimension_map(&dimensions, 0, 1, vertex_coordinates);
	const struct pg_evidence *edge_substitution = pg_context_restrict(&typing, &dimensions, source, edge, 2, bindings);
	assert(edge_substitution);
	const struct pg_binding_face *edge_a = pg_binding_restrict(&dimensions, a, edge);
	const struct pg_binding_face *edge_x = pg_binding_restrict(&dimensions, x, edge);
	const struct pg_evidence *edge_context = pg_evidence_premise(edge_substitution, 1);
	assert(pg_evidence_context(edge_context)->binder == &edge_x->variable);
	assert(pg_evidence_context(edge_context)->declared_type == pg_reference(graph, &edge_a->variable));
	const struct pg_evidence *edge_term = pg_prove_reindex(&typing, edge_substitution, source_x);
	assert(edge_term && pg_evidence_subject(edge_term)->core == pg_reference(graph, &edge_x->variable));
	assert(pg_evidence_classifier(edge_term) == pg_reference(graph, &edge_a->variable));
	const struct pg_binding_face *edge_bindings[] = {edge_a, edge_x};
	const struct pg_evidence *vertex_substitution = pg_context_restrict(&typing, &dimensions, edge_context, vertex, 2, edge_bindings);
	const struct pg_dimension_map *composite = pg_dimension_compose(&dimensions, edge, vertex);
	const struct pg_evidence *direct = pg_context_restrict(&typing, &dimensions, source, composite, 2, bindings);
	assert(vertex_substitution && direct);
	assert(pg_evidence_context(vertex_substitution) == pg_evidence_context(direct));
	const struct pg_evidence *twice = pg_prove_reindex(&typing, vertex_substitution, edge_term);
	const struct pg_evidence *once = pg_prove_reindex(&typing, direct, source_x);
	assert(once && twice && pg_evidence_subject(once)->core == pg_evidence_subject(twice)->core);
	assert(pg_evidence_classifier(once) == pg_evidence_classifier(twice));
	const struct pg_evidence *identity_substitution = pg_context_restrict(&typing, &dimensions, source, identity, 2, bindings);
	const struct pg_evidence *domain = pg_prove_variable(&typing, source, &a->variable);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_evidence *body_context = pg_prove_context_extension(&typing, source, y, domain);
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, body_context, y));
	const struct pg_evidence *codomain = pg_prove_classifier(&typing, &classifiers, body_context, body);
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, domain, body_context, codomain);
	const struct pg_evidence *function = pg_prove_lambda(&typing, pi, body);
	const struct pg_evidence *application = pg_prove_application(&typing, function, source_x);
	assert(function && application);
	const struct pg_evidence *restricted_function = pg_prove_reindex(&typing, direct, function);
	const struct pg_evidence *restricted_application = pg_prove_reindex(&typing, direct, application);
	const struct pg_evidence *rebuilt_application = pg_prove_application(&typing, restricted_function, once);
	assert(restricted_application && rebuilt_application);
	assert(pg_alpha_equal(pg_evidence_subject(restricted_application)->core,
		pg_evidence_subject(rebuilt_application)->core) == 1);
	assert(pg_evidence_classifier(restricted_application) == pg_evidence_classifier(rebuilt_application));
	const struct pg_evidence *vertex_context = pg_evidence_premise(direct, 1);
	const struct pg_evidence *restricted_formation = pg_prove_classifier(&typing, &classifiers,
		vertex_context, restricted_application);
	assert(restricted_formation && pg_evidence_subject(restricted_formation)->core == pg_evidence_classifier(restricted_application));
	const struct pg_evidence *twice_function = pg_prove_reindex(&typing, vertex_substitution,
		pg_prove_reindex(&typing, edge_substitution, function));
	assert(twice_function);
	assert(pg_alpha_equal(pg_evidence_subject(twice_function)->core,
		pg_evidence_subject(restricted_function)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(twice_function), pg_evidence_classifier(restricted_function)) == 1);
	/* Restrict the independently derived beta result, then compare execution.
	 * No Identity witness is inferred from this metatheoretic fixture. */
	const struct pg_evidence *beta_result = pg_prove_return(&typing, &classifiers, source_x);
	const struct pg_evidence *restricted_result = pg_prove_reindex(&typing, direct, beta_result);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, pg_evidence_subject(restricted_application)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == pg_evidence_subject(restricted_result)->core);
	assert(pg_evidence_classifier(restricted_result) == pg_evidence_classifier(restricted_application));
	pg_eval_destroy(&machine);
	assert(identity_substitution && pg_evidence_context(identity_substitution) == pg_evidence_context(source));
	assert(!pg_context_restrict(&typing, &dimensions, source, edge, 1, bindings));
	assert(!pg_context_restrict(&typing, &dimensions, source, edge, 2, edge_bindings));
	assert(!pg_context_restrict(&typing, &dimensions, source, vertex, 2, bindings));
	const struct pg_dimension_map *degeneracy = pg_dimension_map(&dimensions, 1, 0, NULL);
	assert(degeneracy && !pg_context_restrict(&typing, &dimensions, empty, degeneracy, 0, NULL));
	assert(pg_context_restrict(&typing, &dimensions, empty, vertex, 0, NULL));
	const struct pg_coordinate duplicated_axes[] = {{PG_AXIS, 0}, {PG_AXIS, 0}};
	const struct pg_dimension_map invalid_face = {2, 2, duplicated_axes};
	assert(!pg_context_restrict(&typing, &dimensions, source, &invalid_face, 2, bindings));
	pg_dimensions_destroy(&dimensions);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("typed restriction: dependent context faces preserve classifiers and compose without new Identity axioms");
}

static void computation_execution_test(struct pg_graph *graph)
{
	const struct pg_term *force = pg_reference(graph, &pg_force_operation);
	const struct pg_term *thunk = pg_reference(graph, &pg_thunk_operation);
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *returned = pg_application(graph, ret, vx);
	const struct pg_term *quoted = pg_application(graph, thunk, returned);
	const struct pg_term *delayed_argument = pg_application(graph, identity, quoted);
	const struct pg_term *input = pg_application(graph, force, delayed_argument);
	struct pg_eval beta, semantic;
	pg_eval_init(&beta, input);
	assert(pg_eval_advance(&beta, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&beta, graph) == input);
	pg_eval_destroy(&beta);
	pg_computation_eval_init(&semantic, graph, input);
	while (pg_eval_advance(&semantic, 1) == PG_EVAL_PENDING) {
		const struct pg_term *pending = pg_eval_readback(&semantic, graph);
		assert(pending);
		struct pg_eval resumed;
		pg_computation_eval_init(&resumed, graph, pending);
		assert(pg_eval_advance(&resumed, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&resumed, graph) == returned);
		pg_eval_destroy(&resumed);
	}
	assert(semantic.status == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == returned);
	uint64_t split_steps = semantic.steps;
	pg_eval_destroy(&semantic);
	pg_computation_eval_init(&semantic, graph, input);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(semantic.steps == split_steps);
	pg_eval_destroy(&semantic);
	const struct pg_term *neutral = pg_application(graph, force, pg_application(graph, identity, vx));
	pg_computation_eval_init(&semantic, graph, neutral);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == pg_application(graph, force, vx));
	pg_eval_destroy(&semantic);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	const struct pg_term *quoted_omega = pg_application(graph, thunk, omega);
	pg_computation_eval_init(&semantic, graph, quoted_omega);
	assert(pg_eval_advance(&semantic, 10) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == quoted_omega);
	pg_eval_destroy(&semantic);
	pg_computation_eval_init(&semantic, graph, pg_application(graph, force, quoted_omega));
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_PENDING);
	pg_eval_destroy(&semantic);
	/* Releasing a function preserves the arguments waiting outside FORCE. */
	const struct pg_term *call = pg_application(graph,
		pg_application(graph, force, pg_application(graph, thunk, identity)), returned);
	pg_computation_eval_init(&semantic, graph, call);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == returned);
	pg_eval_destroy(&semantic);
	/* Nested demands and a captured thunk body share the lexical evaluator. */
	const struct pg_term *inner = pg_application(graph, force, pg_application(graph, thunk, quoted));
	const struct pg_term *nested = pg_application(graph, force, inner);
	pg_computation_eval_init(&semantic, graph, nested);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == returned);
	pg_eval_destroy(&semantic);
	const struct pg_term *capture = pg_application(graph, pg_lambda(graph, x, pg_application(graph, force, quoted)), identity);
	pg_computation_eval_init(&semantic, graph, capture);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(pg_alpha_equal(pg_eval_readback(&semantic, graph), pg_application(graph, ret, identity)) == 1);
	pg_eval_destroy(&semantic);
	const struct pg_term *fold = pg_reference(graph, &pg_fold_operation);
	const struct pg_term *continuation = pg_lambda(graph, x, pg_application(graph, ret, vx));
	const struct pg_term *sequence = pg_application(graph, pg_application(graph, fold, returned), continuation);
	pg_computation_eval_init(&semantic, graph, sequence);
	unsigned ticks = 0;
	while (pg_eval_advance(&semantic, 1) == PG_EVAL_PENDING) {
		assert(++ticks < 100);
		struct pg_eval restart;
		pg_computation_eval_init(&restart, graph, pg_eval_readback(&semantic, graph));
		assert(pg_eval_advance(&restart, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&restart, graph) == returned);
		pg_eval_destroy(&restart);
	}
	assert(pg_eval_readback(&semantic, graph) == returned);
	pg_eval_destroy(&semantic);
	const struct pg_term *blocked = pg_application(graph, pg_application(graph, fold, vx), continuation);
	pg_computation_eval_init(&semantic, graph, blocked);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&semantic, graph) == blocked);
	pg_eval_destroy(&semantic);
	const struct pg_term *diverging = pg_application(graph, pg_application(graph, fold, omega), continuation);
	pg_computation_eval_init(&semantic, graph, diverging);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_PENDING);
	pg_eval_destroy(&semantic);
	puts("computation execution: force/thunk, fold, captured environments, neutral demands and split budgets passed");
}

static void classifiers_test(struct pg_graph *graph)
{
	struct pg_classifiers classifiers;
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_term *u0 = pg_universe(&classifiers, 0);
	const struct pg_term *u1 = pg_universe(&classifiers, 1);
	assert(u0 && u1 && u0 != u1);
	assert(u0 == pg_universe(&classifiers, 0));
	uint64_t level;
	assert(pg_universe_level(u0, &level) && level == 0);
	assert(pg_universe_level(u1, &level) && level == 1);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	assert(!pg_universe_level(vx, &level));
	const struct pg_term *pi_x = pg_pi(&classifiers, u0, x, vx);
	const struct pg_term *pi_y = pg_pi(&classifiers, u0, y, vy);
	assert(pi_x && pi_y && pi_x != pi_y);
	assert(pi_x == pg_pi(&classifiers, u0, x, vx));
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	assert(pg_pi_view(pi_x, &domain, &binder, &codomain));
	assert(domain == u0 && binder == x && codomain == vx);
	struct pg_binding_value argument = {binder, u1};
	assert(pg_term_substitute(graph, codomain, 1, &argument) == u1);
	assert(!pg_pi_view(u0, &domain, &binder, &codomain));
	assert(!pg_pi_view(pg_application(graph, pi_x, u0), &domain, &binder, &codomain));
	struct pg_beta_work work;
	struct pg_conversion conversion;
	assert(pg_beta_work_init(&work, graph) == 0);
	assert(pg_conversion_init(&conversion, &work, pi_x, pi_y) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	assert(pg_conversion_init(&conversion, &work, u0, u1) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&conversion);
	pg_beta_work_destroy(&work);
	for (uint64_t i = 2; i < 1000; ++i) assert(pg_universe(&classifiers, i));
	assert(pg_universe_level(pg_universe(&classifiers, UINT64_MAX), &level));
	assert(level == UINT64_MAX);
	assert(u0 == pg_universe(&classifiers, 0));
	pg_classifiers_destroy(&classifiers);
	assert(pg_universe_level(u0, &level) && level == 0);
	puts("classifiers: distinct universe levels and Pi spines reuse Core without typed Lambda tags");
}

static void restriction_test(struct pg_graph *graph)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 2);
	const struct pg_dimension_map *identity = pg_dimension_identity(&dimensions, 2);
	const struct pg_binding_face *center = pg_binding_face(&dimensions, cube, identity);
	const struct pg_coordinate edge_coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
	const struct pg_dimension_map *edge_map = pg_dimension_map(&dimensions, 1, 2, edge_coordinates);
	const struct pg_coordinate zero_coordinate = {PG_ENDPOINT_ZERO, 0};
	const struct pg_dimension_map *zero = pg_dimension_map(&dimensions, 0, 1, &zero_coordinate);
	const struct pg_binding_face *edge = pg_binding_restrict(&dimensions, center, edge_map);
	const struct pg_binding_face *corner = pg_binding_restrict(&dimensions, edge, zero);
	assert(center && edge && corner);
	const struct pg_term *variable = pg_reference(graph, &center->variable);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *lambda = pg_lambda(graph, x, pg_application(graph, vx, variable));
	assert(pg_term_restrict_bindings(&dimensions, lambda, identity, 1, &center) == lambda);
	const struct pg_term *edge_term = pg_term_restrict_bindings(&dimensions, lambda, edge_map, 1, &center);
	const struct pg_term *iterated = pg_term_restrict_bindings(&dimensions, edge_term, zero, 1, &edge);
	const struct pg_dimension_map *composite = pg_dimension_compose(&dimensions, edge_map, zero);
	const struct pg_term *direct = pg_term_restrict_bindings(&dimensions, lambda, composite, 1, &center);
	assert(iterated && direct && pg_alpha_equal(iterated, direct) == 1);
	assert(direct->as.lambda.body->as.application.argument == pg_reference(graph, &corner->variable));
	/* Restricting before or after beta yields the same free boundary cell. */
	const struct pg_term *redex = pg_application(graph, pg_lambda(graph, x, vx), variable);
	const struct pg_term *restricted = pg_term_restrict_bindings(&dimensions, redex, composite, 1, &center);
	assert(restricted && restricted->kind == PG_APPLICATION);
	struct pg_eval machine;
	pg_eval_init(&machine, restricted);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == pg_term_restrict_bindings(&dimensions, variable, composite, 1, &center));
	pg_eval_destroy(&machine);
	/* A lambda-bound occurrence is not a free cube boundary occurrence. */
	const struct pg_term *bound = pg_lambda(graph, &center->variable, variable);
	assert(pg_alpha_equal(bound, pg_term_restrict_bindings(&dimensions, bound, composite, 1, &center)) == 1);
	assert(!pg_term_restrict_bindings(&dimensions, variable, zero, 1, &center));
	const struct pg_coordinate projection_coordinates[] = {{PG_AXIS, 0}};
	const struct pg_dimension_map *projection = pg_dimension_map(&dimensions, 2, 1, projection_coordinates);
	assert(!pg_term_restrict_bindings(&dimensions, variable, projection, 0, NULL));
	struct pg_dimension_map copied_identity = *identity;
	assert(pg_dimension_face(&dimensions, &copied_identity) == identity);
	assert(pg_binding_face(&dimensions, center->cube, &copied_identity) == center);
	assert(pg_term_restrict_bindings(&dimensions, variable, &copied_identity, 1, &center) == variable);
	const struct pg_coordinate repeated_axes[] = {{PG_AXIS, 0}, {PG_AXIS, 0}};
	const struct pg_dimension_map not_a_face = {2, 2, repeated_axes};
	assert(!pg_dimension_face(&dimensions, &not_a_face));
	assert(!pg_binding_face(&dimensions, center->cube, &not_a_face));
	assert(!pg_term_restrict_bindings(&dimensions, variable, &not_a_face, 0, NULL));
	assert(!pg_dimension_compose(&dimensions, &not_a_face, identity));
	pg_dimensions_destroy(&dimensions);
	puts("restriction: term faces compose and commute with beta without capturing bound variables");
}

static void conversion_test(struct pg_graph *graph)
{
	struct pg_beta_work work;
	assert(pg_beta_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *other_identity = pg_lambda(graph, y, vy);
	const struct pg_term *left = pg_lambda(graph, x, pg_application(graph, other_identity, vx));
	struct pg_conversion conversion;
	assert(pg_conversion_init(&conversion, &work, left, identity) == 0);
	assert(pg_conversion_advance(&conversion, 0) == PG_CONVERSION_PENDING);
	while (pg_conversion_advance(&conversion, 1) == PG_CONVERSION_PENDING) assert(pg_conversion_steps(&conversion) < 100);
	assert(pg_conversion_status(&conversion) == PG_CONVERSION_EQUAL);
	assert(left != identity);
	pg_conversion_destroy(&conversion);
	/* An identical body pointer does not identify a bound variable with a
	 * free variable on the opposite side. */
	assert(pg_conversion_init(&conversion, &work, identity, pg_lambda(graph, y, vx)) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&conversion);
	assert(pg_conversion_init(&conversion, &work, identity, other_identity) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	const struct pg_term *dag_x = vx;
	const struct pg_term *dag_y = vy;
	for (size_t i = 0; i < 40; ++i) {
		dag_x = pg_application(graph, dag_x, dag_x);
		dag_y = pg_application(graph, dag_y, dag_y);
	}
	assert(pg_conversion_init(&conversion, &work, pg_lambda(graph, x, dag_x), pg_lambda(graph, y, dag_y)) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	assert(pg_conversion_task_count(&conversion) < 100);
	pg_conversion_destroy(&conversion);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	assert(pg_conversion_init(&conversion, &work, omega, vy) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_PENDING);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_PENDING);
	pg_conversion_destroy(&conversion);
	assert(pg_conversion_init(&conversion, &work, omega, omega) == 0);
	assert(pg_conversion_advance(&conversion, 0) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	pg_beta_work_destroy(&work);
	puts("conversion: explicit beta comparison, binder scope, shared DAG and pending divergence passed");
}

static void beta_work_test(struct pg_graph *graph)
{
	struct pg_beta_work work;
	assert(pg_beta_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *input = pg_application(graph, identity, vy);
	struct pg_beta_job *job = pg_beta_request(&work, input);
	assert(job && job == pg_beta_request(&work, input));
	assert(pg_beta_status(job) == PG_EVAL_PENDING);
	assert(pg_beta_steps(job) == 0 && !pg_beta_result(job));
	assert(pg_beta_advance(job, 0) == PG_EVAL_PENDING);
	assert(pg_beta_advance(job, 1) == PG_EVAL_PENDING);
	assert(pg_beta_steps(job) == 1);
	assert(job == pg_beta_request(&work, input));
	assert(pg_beta_advance(job, 100) == PG_EVAL_WHNF);
	assert(pg_beta_result(job) == vy);
	uint64_t steps = pg_beta_steps(job);
	assert(pg_beta_advance(job, 100) == PG_EVAL_WHNF);
	assert(pg_beta_steps(job) == steps);
	assert(input != vy && pg_application(graph, identity, vy) == input);
	assert(pg_beta_request(&work, vy) != job);
	/* Both requests enter the same lambda body but capture different values. */
	const struct pg_term *constant = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	struct pg_beta_job *left = pg_beta_request(&work, pg_application(graph, constant, vx));
	struct pg_beta_job *right = pg_beta_request(&work, pg_application(graph, constant, vy));
	assert(left != right);
	assert(pg_beta_advance(left, 100) == PG_EVAL_WHNF);
	assert(pg_beta_advance(right, 100) == PG_EVAL_WHNF);
	assert(pg_beta_result(left)->as.lambda.body == vx);
	assert(pg_beta_result(right)->as.lambda.body == vy);
	const struct pg_term *stable = pg_beta_result(left);
	assert(pg_beta_advance(left, 100) == PG_EVAL_WHNF);
	assert(pg_beta_result(left) == stable);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	struct pg_beta_job *loop = pg_beta_request(&work, pg_application(graph, self, self));
	assert(pg_beta_advance(loop, 30) == PG_EVAL_PENDING);
	assert(pg_beta_advance(loop, 30) == PG_EVAL_PENDING);
	assert(pg_beta_steps(loop) == 60 && !pg_beta_result(loop));
	for (size_t i = 0; i < 1000; ++i) {
		assert(pg_beta_request(&work, pg_reference(graph, pg_binder(graph))));
	}
	assert(job == pg_beta_request(&work, input));
	assert(pg_beta_result(job) == vy);
	assert(!pg_beta_request(&work, NULL));
	pg_beta_work_destroy(&work);
	assert(stable->as.lambda.body == vx);
	puts("beta work: shared pending jobs, stable answers, split fuel and environment isolation passed");
}

static void substitution_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	struct pg_binding_value bindings[] = {{x, vy}, {y, vx}};
	const struct pg_term *app = pg_application(graph, vx, vy);
	assert(pg_term_substitute(graph, app, 0, NULL) == app);
	assert(pg_term_substitute(graph, app, 2, bindings) == pg_application(graph, vy, vx));
	/* Substituted free y must not be captured by the lambda's binder. */
	const struct pg_term *lambda = pg_lambda(graph, y, vx);
	const struct pg_term *result = pg_term_substitute(graph, lambda, 1, bindings);
	assert(result && result->kind == PG_LAMBDA);
	assert(result->as.lambda.binder != y);
	assert(result->as.lambda.body == vy);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	result = pg_term_substitute(graph, identity, 2, bindings);
	assert(result && pg_alpha_equal(result, identity) == 1);
	const struct pg_term *redex = pg_application(graph, identity, vx);
	result = pg_term_substitute(graph, redex, 1, bindings);
	assert(result && result->kind == PG_APPLICATION);
	assert(result != vy);
	assert(result->as.application.argument == vy);
	struct pg_eval machine;
	pg_eval_init(&machine, result);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == vy);
	pg_eval_destroy(&machine);
	const struct pg_term *dag = vx;
	const struct pg_term *expected = vy;
	for (size_t i = 0; i < 40; ++i) {
		dag = pg_application(graph, dag, dag);
		expected = pg_application(graph, expected, expected);
	}
	assert(pg_term_substitute(graph, dag, 1, bindings) == expected);
	/* Deep shared structure must use heap work frames, not C recursion. */
	for (size_t i = 0; i < 50000; ++i) {
		dag = pg_application(graph, dag, dag);
		expected = pg_application(graph, expected, expected);
	}
	assert(pg_term_substitute(graph, dag, 1, bindings) == expected);
	struct pg_binding_value shadow[] = {{x, vy}, {x, vx}};
	struct pg_substitution split, whole;
	assert(pg_substitution_init(&split, graph, dag, 1, bindings) == 0);
	assert(pg_substitution_init(&whole, graph, dag, 1, bindings) == 0);
	assert(!pg_substitution_result(&split));
	assert(pg_substitution_advance(&split, 0) == PG_SUBSTITUTION_PENDING);
	assert(pg_substitution_steps(&split) == 0);
	while (pg_substitution_status(&split) == PG_SUBSTITUTION_PENDING) {
		assert(!pg_substitution_result(&split));
		uint64_t steps = pg_substitution_steps(&split);
		pg_substitution_advance(&split, 7);
		assert(pg_substitution_steps(&split) - steps <= 7);
	}
	assert(pg_substitution_advance(&whole, UINT64_MAX) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_result(&split) == expected);
	assert(pg_substitution_result(&whole) == expected);
	assert(pg_substitution_steps(&split) == pg_substitution_steps(&whole));
	uint64_t complete_steps = pg_substitution_steps(&split);
	assert(pg_substitution_advance(&split, 100) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_steps(&split) == complete_steps);
	pg_substitution_destroy(&split);
	pg_substitution_destroy(&whole);
	assert(pg_substitution_status(&split) == PG_SUBSTITUTION_ERROR);
	assert(!pg_substitution_result(&split));
	assert(pg_substitution_init(&split, graph, lambda, 1, bindings) == 0);
	assert(pg_substitution_advance(&split, 1) == PG_SUBSTITUTION_PENDING);
	pg_substitution_destroy(&split);
	assert(pg_substitution_init(&split, graph, lambda, 1, bindings) == 0);
	while (pg_substitution_advance(&split, 1) == PG_SUBSTITUTION_PENDING)
		assert(!pg_substitution_result(&split));
	result = pg_substitution_result(&split);
	assert(result && result->kind == PG_LAMBDA);
	assert(result->as.lambda.binder != y && result->as.lambda.body == vy);
	pg_substitution_destroy(&split);
	struct pg_binding_value lookup_bindings[32];
	lookup_bindings[0] = (struct pg_binding_value){x, vy};
	for (size_t i = 1; i < 32; ++i) lookup_bindings[i] = (struct pg_binding_value){y, vx};
	assert(pg_substitution_init(&split, graph, vx, 32, lookup_bindings) == 0);
	lookup_bindings[0].value = vx; /* Initialization owns its binding snapshot. */
	for (size_t i = 0; i < 31; ++i) {
		assert(pg_substitution_advance(&split, 1) == PG_SUBSTITUTION_PENDING);
		assert(pg_substitution_steps(&split) == i + 1);
	}
	assert(pg_substitution_advance(&split, 2) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_result(&split) == vy);
	pg_substitution_destroy(&split);
	assert(pg_substitution_init(&split, graph, vx, 1, NULL) != 0);
	pg_substitution_destroy(&split);
	assert(pg_term_substitute(graph, vx, 2, shadow) == vx);
	assert(!pg_term_substitute(graph, NULL, 0, NULL));
	assert(!pg_term_substitute(graph, vx, 1, NULL));
	assert(!pg_term_substitute(graph, vx, SIZE_MAX, bindings));
	puts("substitution: simultaneous images, capture avoidance, sharing and no reduction passed");
}

static void evaluation_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_object *a = pg_binder(graph);
	const struct pg_object *b = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *va = pg_reference(graph, a);
	const struct pg_term *vb = pg_reference(graph, b);
	const struct pg_term *constant = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *input = pg_application(graph, pg_application(graph, constant, va), vb);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *redex = pg_application(graph, identity, va);
	assert(redex != va);
	struct pg_eval whole, split;
	pg_eval_init(&whole, input);
	pg_eval_init(&split, input);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_advance(&split, 0) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&split, 2) == PG_EVAL_PENDING);
	const struct pg_term *residual = pg_eval_readback(&split, graph);
	assert(residual);
	while (pg_eval_advance(&split, 1) == PG_EVAL_PENDING) assert(split.steps < 100);
	assert(split.steps == whole.steps);
	assert(pg_eval_readback(&whole, graph) == va);
	assert(pg_eval_readback(&split, graph) == va);
	pg_eval_destroy(&whole);
	pg_eval_destroy(&split);
	pg_eval_init(&whole, redex);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&whole, graph) == va);
	assert(pg_application(graph, identity, va) == redex);
	assert(redex != va);
	pg_eval_destroy(&whole);
	pg_eval_init(&whole, residual);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&whole, graph) == va);
	pg_eval_destroy(&whole);
	/* The free y in the argument must not become bound during readback. */
	pg_eval_init(&whole, pg_application(graph, constant, vy));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	const struct pg_term *captured = pg_eval_readback(&whole, graph);
	assert(pg_alpha_equal(captured, pg_lambda(graph, a, vy)) == 1);
	assert(pg_alpha_equal(captured, pg_lambda(graph, y, vy)) == 0);
	pg_eval_destroy(&whole);
	/* One shared lambda is used under two distinct environments. */
	const struct pg_term *inner = pg_lambda(graph, y, vx);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *argument = i ? vb : va;
		pg_eval_init(&whole, pg_application(graph, pg_lambda(graph, x, inner), argument));
		assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
		assert(pg_alpha_equal(pg_eval_readback(&whole, graph), pg_lambda(graph, y, argument)) == 1);
		pg_eval_destroy(&whole);
	}
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	pg_eval_init(&whole, pg_application(graph, self, self));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_PENDING);
	assert(whole.steps == 200);
	pg_eval_destroy(&whole);
	const struct pg_term *shared = vx;
	const struct pg_term *expected = va;
	for (size_t i = 0; i < 40; ++i) {
		shared = pg_application(graph, shared, shared);
		expected = pg_application(graph, expected, expected);
	}
	const struct pg_term *suspended = pg_lambda(graph, x, pg_lambda(graph, y, shared));
	pg_eval_init(&whole, pg_application(graph, suspended, va));
	assert(pg_eval_advance(&whole, 100) == PG_EVAL_WHNF);
	assert(pg_alpha_equal(pg_eval_readback(&whole, graph), pg_lambda(graph, y, expected)) == 1);
	pg_eval_destroy(&whole);
	puts("evaluation: lexical capture, shared closures, split budgets and divergence passed");
}

static void enumerate(struct pg_dimensions *dimensions, size_t source,
	size_t target, struct pg_coordinate *coordinates, size_t position)
{
	if (position == target) {
		const struct pg_dimension_map *map = pg_dimension_map(dimensions, source, target, coordinates);
		if (map) {
			assert(map_count < sizeof(maps) / sizeof(*maps));
			maps[map_count++] = map;
		}
		return;
	}
	coordinates[position] = (struct pg_coordinate){PG_ENDPOINT_ZERO, 0};
	enumerate(dimensions, source, target, coordinates, position + 1);
	coordinates[position] = (struct pg_coordinate){PG_ENDPOINT_ONE, 0};
	enumerate(dimensions, source, target, coordinates, position + 1);
	for (size_t i = 0; i < source; ++i) {
		coordinates[position] = (struct pg_coordinate){PG_AXIS, i};
		enumerate(dimensions, source, target, coordinates, position + 1);
	}
}

static void dimension_test(struct pg_graph *graph)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	struct pg_coordinate coordinates[3];
	for (size_t source = 0; source <= 2; ++source) {
		for (size_t target = 0; target <= 2; ++target) {
			enumerate(&dimensions, source, target, coordinates, 0);
		}
	}
	size_t triples = 0;
	for (size_t i = 0; i < map_count; ++i) {
		const struct pg_dimension_map *a = maps[i];
		assert(pg_dimension_compose(&dimensions, pg_dimension_identity(&dimensions, a->target), a) == a);
		assert(pg_dimension_compose(&dimensions, a, pg_dimension_identity(&dimensions, a->source)) == a);
		for (size_t j = 0; j < map_count; ++j) {
			const struct pg_dimension_map *b = maps[j];
			if (a->source != b->target) continue;
			const struct pg_dimension_map *ab = pg_dimension_compose(&dimensions, a, b);
			assert(ab);
			for (size_t k = 0; k < map_count; ++k) {
				const struct pg_dimension_map *c = maps[k];
				if (b->source != c->target) continue;
				const struct pg_dimension_map *bc = pg_dimension_compose(&dimensions, b, c);
				assert(bc);
				assert(pg_dimension_compose(&dimensions, ab, c) == pg_dimension_compose(&dimensions, a, bc));
				triples++;
			}
		}
	}
	struct pg_coordinate duplicate[] = {{PG_AXIS, 0}, {PG_AXIS, 0}};
	assert(!pg_dimension_map(&dimensions, 1, 2, duplicate));
	struct pg_coordinate invalid = {PG_AXIS, 1};
	assert(!pg_dimension_map(&dimensions, 1, 1, &invalid));
	invalid = (struct pg_coordinate){PG_ENDPOINT_ZERO, 1};
	assert(!pg_dimension_map(&dimensions, 0, 1, &invalid));
	struct pg_coordinate permutation[] = {{PG_AXIS, 2}, {PG_AXIS, 0}, {PG_AXIS, 1}};
	const struct pg_dimension_map *cycle = pg_dimension_map(&dimensions, 3, 3, permutation);
	const struct pg_dimension_map *twice = pg_dimension_compose(&dimensions, cycle, cycle);
	assert(pg_dimension_compose(&dimensions, cycle, twice) == pg_dimension_identity(&dimensions, 3));
	struct pg_coordinate face_coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ONE, 0}, {PG_AXIS, 1}};
	struct pg_coordinate projection_coordinates[] = {{PG_AXIS, 0}, {PG_AXIS, 2}};
	const struct pg_dimension_map *face = pg_dimension_map(&dimensions, 2, 3, face_coordinates);
	const struct pg_dimension_map *projection = pg_dimension_map(&dimensions, 3, 2, projection_coordinates);
	assert(pg_dimension_compose(&dimensions, projection, face) == pg_dimension_identity(&dimensions, 2));
	assert(!pg_dimension_compose(&dimensions, face, face));
	/* Restricting a cube along equal composites names the same variable. */
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 3);
	const struct pg_binding_face *top = pg_binding_face(&dimensions, cube,
		pg_dimension_identity(&dimensions, 3));
	assert(top);
	const struct pg_binding_face *surface = pg_binding_restrict(&dimensions, top, face);
	assert(surface);
	struct pg_coordinate edge_coordinates[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	const struct pg_dimension_map *edge = pg_dimension_map(&dimensions, 1, 2, edge_coordinates);
	const struct pg_binding_face *line = pg_binding_restrict(&dimensions, surface, edge);
	assert(line == pg_binding_restrict(&dimensions, top, pg_dimension_compose(&dimensions, face, edge)));
	const struct pg_term *line_term = pg_reference(graph, &line->variable);
	assert(line_term == pg_reference(graph, &pg_binding_face(&dimensions, cube, line->face)->variable));
	const struct pg_binding_cube *other_cube = pg_binding_cube(&dimensions, 3);
	assert(pg_binding_face(&dimensions, other_cube, line->face) != line);
	const struct pg_binding_cube *square = pg_binding_cube(&dimensions, 2);
	struct pg_coordinate down[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	struct pg_coordinate left[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
	struct pg_coordinate endpoint = {PG_ENDPOINT_ZERO, 0};
	const struct pg_dimension_map *zero = pg_dimension_map(&dimensions, 0, 1, &endpoint);
	const struct pg_binding_face *bottom = pg_binding_face(&dimensions, square,
		pg_dimension_map(&dimensions, 1, 2, down));
	const struct pg_binding_face *side = pg_binding_face(&dimensions, square,
		pg_dimension_map(&dimensions, 1, 2, left));
	assert(pg_binding_restrict(&dimensions, bottom, zero) == pg_binding_restrict(&dimensions, side, zero));
	/* Dropping a source dimension cannot introduce an independent binder. */
	assert(!pg_binding_face(&dimensions, square, projection));
	assert(!pg_binding_restrict(&dimensions, top, edge));
	assert(pg_binding_restrict(&dimensions, top, pg_dimension_identity(&dimensions, 3)) == top);
	for (size_t n = 4; n < 150; ++n) assert(pg_dimension_identity(&dimensions, n));
	assert(dimensions.maps.capacity > 64);
	assert(cycle == pg_dimension_map(&dimensions, 3, 3, permutation));
	printf("dimension: %zu maps, %zu composable triples; 3D faces/permutations passed\n", map_count, triples);
	pg_dimensions_destroy(&dimensions);
}

int main(void)
{
	struct pg_graph graph;
	assert(pg_graph_init(&graph) == 0);
	graph_test(&graph);
	context_test(&graph);
	evidence_test(&graph);
	dependent_application_test(&graph);
	typed_substitution_test(&graph);
	typed_restriction_test(&graph);
	computation_execution_test(&graph);
	classifiers_test(&graph);
	restriction_test(&graph);
	conversion_test(&graph);
	beta_work_test(&graph);
	substitution_test(&graph);
	evaluation_test(&graph);
	dimension_test(&graph);
	printf("graph: %zu terms; pointer-key interning and separate alpha comparison passed\n", graph.terms.count);
	pg_graph_destroy(&graph);
	return 0;
}
