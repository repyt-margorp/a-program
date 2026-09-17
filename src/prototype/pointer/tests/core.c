#include "graph.h"
#include "dag.h"
#include "dimension.h"
#include "eval.h"
#include "eval_internal.h"
#include "typing.h"
#include "conversion.h"
#include "classifier.h"
#include "evidence.h"
#include "function_graph.h"
#include "derivation.h"
#include "computation.h"
#include "action.h"
#include "symmetry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct dag_fixture {
	size_t count;
	const struct dag_fixture *children[2];
};

static int dag_child(void *context, const void *key, size_t index, const void **child)
{
	++*(size_t *)context;
	const struct dag_fixture *node = key;
	if (index == node->count) return 0;
	*child = node->children[index];
	return 1;
}

static void dag_test(void)
{
	struct dag_fixture leaf = {0}, middle = {2, {&leaf, &leaf}}, root = {2, {&middle, &middle}};
	struct pg_dag dag;
	size_t visits = 0;
	assert(pg_dag_init(&dag, dag_child, &visits) == 0);
	assert(pg_dag_add(&dag, &root) == 0 && dag.count == 3 && visits == 7);
	assert(pg_dag_find(&dag, &leaf)->id == 1 && pg_dag_find(&dag, &root)->id == 3);
	assert(pg_dag_add(&dag, &root) == 0 && visits == 7);
	assert(pg_dag_add(&dag, &leaf) == 0 && visits == 7);
	struct dag_fixture cycle = {1, {NULL}};
	cycle.children[0] = &cycle;
	assert(pg_dag_add(&dag, &cycle) == -1 && dag.failed);
	assert(pg_dag_add(&dag, &root) == -1);
	pg_dag_destroy(&dag);
	assert(pg_dag_init(&dag, dag_child, &visits) == 0);
	struct dag_fixture missing = {1, {NULL}};
	assert(pg_dag_add(&dag, &missing) == -1);
	pg_dag_destroy(&dag);
}

static void reconstruct_derivation(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *source)
{
	assert(source);
	struct pg_derivation_parameters parameters;
	assert(pg_derivation_parameters(source, &parameters) == 0);
	size_t count = pg_evidence_premise_count(source);
	const struct pg_evidence **premises = pg_alloc(typing->graph, (count + 1) * sizeof(*premises));
	assert(premises);
	for (size_t i = 0; i < count; ++i) premises[i] = pg_evidence_premise(source, i);
	assert(pg_prove_derivation(typing, classifiers, pg_evidence_rule(source), &parameters, count, premises) == source);
	premises[count] = source;
	assert(!pg_prove_derivation(typing, classifiers, pg_evidence_rule(source), &parameters, count + 1, premises));
	if (count) {
		premises[0] = NULL;
		assert(!pg_prove_derivation(typing, classifiers, pg_evidence_rule(source), &parameters, count, premises));
	}
}

static void index_distribution_test(void)
{
	struct pg_index index;
	struct pg_index_entry entries[1024];
	assert(pg_index_init(&index) == 0);
	for (size_t i = 0; i < 1024; ++i)
		assert(pg_index_insert(&index, &entries[i], ((uint64_t)i << 32) | 32) == 0);
	size_t occupied = 0, longest = 0;
	for (size_t i = 0; i < index.capacity; ++i) {
		size_t length = 0;
		for (struct pg_index_entry *p = index.buckets[i]; p; p = p->next) ++length;
		if (length) ++occupied;
		if (length > longest) longest = length;
	}
	printf("aligned index: %zu occupied buckets, longest chain %zu\n", occupied, longest);
	assert(occupied > 512 && longest < 16);
	struct pg_index_entry collisions[2];
	for (size_t i = 0; i < 2; ++i)
		assert(pg_index_insert(&index, &collisions[i], 32) == 0);
	size_t same_hash = 0;
	for (struct pg_index_entry *p = pg_index_candidates(&index, 32); p; p = p->next)
		if (p->hash == 32) ++same_hash;
	assert(same_hash == 3);
	for (size_t i = 0; i < 1024; ++i) {
		assert(entries[i].hash == (((uint64_t)i << 32) | 32));
		struct pg_index_entry *p = pg_index_candidates(&index, entries[i].hash);
		while (p && p != &entries[i]) p = p->next;
		assert(p == &entries[i]);
	}
	pg_index_destroy(&index);
}

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
	/* Identical scopes do not require traversing a shared body again. */
	const struct pg_term *shared_left = pg_lambda(graph, x,
		pg_application(graph, left_dag, pg_lambda(graph, y, vy)));
	const struct pg_term *shared_right = pg_lambda(graph, x,
		pg_application(graph, left_dag, pg_lambda(graph, z, vz)));
	struct pg_comparison shared;
	assert(pg_comparison_init(&shared, shared_left, shared_right, NULL, NULL) == 0);
	assert(pg_comparison_advance(&shared, 100) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_task_count(&shared) == 4);
	pg_comparison_destroy(&shared);
	/* An identical inner binder masks a nonidentity outer correspondence. */
	assert(pg_alpha_equal(pg_lambda(graph, x, pg_lambda(graph, x, vx)),
		pg_lambda(graph, y, pg_lambda(graph, x, vx))) == 1);
	assert(pg_alpha_equal(pg_lambda(graph, x, pg_lambda(graph, z, vx)),
		pg_lambda(graph, y, pg_lambda(graph, z, vx))) == 0);
	assert(pg_term_independent(vx, x) == 0);
	assert(pg_term_independent(vy, x) == 1);
	assert(pg_term_independent(identity, x) == 1);
	assert(pg_term_independent(pg_lambda(graph, y, vx), x) == 0);
	assert(pg_term_independent(pg_lambda(graph, x, identity), x) == 1);
	assert(pg_term_independent(pg_application(graph, identity, vx), x) == 0);
	assert(pg_term_independent(NULL, x) == -1);
	assert(pg_term_independent(vx, NULL) == -1);
	assert(pg_term_independent(vx, opaque) == -1);
	struct pg_comparison split, whole;
	size_t terms = graph->terms.count;
	assert(pg_independence_init(&split, left_lambda, x) == 0);
	assert(pg_independence_init(&whole, left_lambda, x) == 0);
	assert(pg_comparison_advance(&split, 0) == PG_COMPARISON_PENDING);
	while (pg_comparison_advance(&split, 1) == PG_COMPARISON_PENDING)
		assert(pg_comparison_steps(&split) < 1000);
	assert(pg_comparison_status(&split) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_advance(&whole, UINT64_MAX) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_steps(&split) == pg_comparison_steps(&whole));
	assert(pg_comparison_task_count(&split) == 42);
	assert(graph->terms.count == terms);
	pg_comparison_destroy(&split);
	pg_comparison_destroy(&whole);
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
	const struct pg_context *in_a = pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_VALUE);
	const struct pg_context *in_b = pg_context_bind(&typing, NULL, x, b, PG_JUDGEMENT_VALUE);
	assert(in_a && in_b);
	assert(in_a != in_b);
	const struct pg_context *family_binding = pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_TYPE_FAMILY);
	assert(family_binding && family_binding != in_a);
	assert(family_binding == pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_TYPE_FAMILY));
	assert(family_binding->binder == in_a->binder && family_binding->declared_type == in_a->declared_type);
	assert(!pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_COMPUTATION));
	assert(in_a == pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_VALUE));
	const struct pg_term *identity = pg_lambda(graph, x, pg_reference(graph, x));
	assert(identity == pg_lambda(graph, in_a->binder, pg_reference(graph, in_a->binder)));
	assert(identity == pg_lambda(graph, in_b->binder, pg_reference(graph, in_b->binder)));
	assert(pg_context_lookup(in_a, x)->declared_type == a);
	assert(pg_context_lookup(in_b, x)->declared_type == b);
	assert(!pg_context_lookup(in_a, y));
	const struct pg_context *extended = pg_context_bind(&typing, in_a, y, b, PG_JUDGEMENT_VALUE);
	assert(extended->parent == in_a);
	assert(pg_context_lookup(extended, x) == in_a);
	assert(pg_context_lookup(extended, y) == extended);
	for (size_t i = 0; i < 1000; ++i) {
		assert(pg_context_bind(&typing, in_a, pg_binder(graph), a, PG_JUDGEMENT_VALUE));
	}
	assert(in_a == pg_context_bind(&typing, NULL, x, a, PG_JUDGEMENT_VALUE));
	assert(in_a->declared_type == a);
	assert(in_b->declared_type == b);
	assert(!pg_context_bind(&typing, NULL, type_a, a, PG_JUDGEMENT_VALUE));
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_occurrence *body_a = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, in_a, vx, NULL, NULL, 0, NULL);
	const struct pg_occurrence *body_b = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, in_b, vx, NULL, NULL, 0, NULL);
	assert(body_a && body_b && body_a != body_b);
	assert(body_a->core == body_b->core);
	const struct pg_occurrence *lambda_a = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, 1, &body_a);
	const struct pg_occurrence *lambda_b = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, 1, &body_b);
	assert(lambda_a && lambda_b && lambda_a != lambda_b);
	assert(lambda_a->core == lambda_b->core);
	assert(lambda_a == pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, 1, &body_a));
	assert(lambda_a->operands[0]->context == in_a);
	assert(lambda_b->operands[0]->context == in_b);
	assert(pg_occurrence_scoped_input(lambda_a, 0) == body_a);
	assert(pg_occurrence_scoped_input(lambda_b, 0) == body_b);
	assert(!pg_occurrence_scoped_input(NULL, 0));
	assert(!pg_occurrence_scoped_input(lambda_a, 1));
	const struct pg_occurrence *wrong_binder = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL,
		pg_lambda(graph, y, vx), NULL, NULL, 1, &body_a);
	const struct pg_occurrence *wrong_body = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL,
		pg_lambda(graph, x, a), NULL, NULL, 1, &body_a);
	const struct pg_occurrence *wrong_scope = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, in_a,
		identity, NULL, NULL, 1, &body_a);
	assert(!pg_occurrence_scoped_input(wrong_binder, 0));
	assert(!pg_occurrence_scoped_input(wrong_body, 0));
	assert(!pg_occurrence_scoped_input(wrong_scope, 0));
	const struct pg_occurrence *annotated = pg_occurrence(&typing, PG_JUDGEMENT_INPUT, in_a, vx, NULL, a, 0, NULL);
	assert(annotated && annotated != body_a);
	assert(annotated->annotation == a);
	const struct pg_occurrence *typed_a = pg_occurrence(&typing, PG_JUDGEMENT_VALUE, in_a, vx, a, NULL, 0, NULL);
	const struct pg_occurrence *typed_b = pg_occurrence(&typing, PG_JUDGEMENT_VALUE, in_a, vx, b, NULL, 0, NULL);
	assert(typed_a && typed_b && typed_a != typed_b && typed_a != body_a);
	assert(typed_a->core == typed_b->core && typed_a->context == typed_b->context);
	assert(typed_a->classifier == a && typed_b->classifier == b);
	assert(typed_a == pg_occurrence(&typing, PG_JUDGEMENT_VALUE, in_a, vx, a, NULL, 0, NULL));
	const struct pg_occurrence *as_type = pg_occurrence_boundary(&typing, typed_a, PG_JUDGEMENT_VALUE_TYPE, a);
	assert(as_type && as_type != typed_a && as_type->core == typed_a->core);
	assert(as_type->classifier == typed_a->classifier && as_type->judgement == PG_JUDGEMENT_VALUE_TYPE);
	assert(as_type->origin == typed_a && !as_type->operand_count);
	assert(pg_occurrence_boundary(&typing, as_type, PG_JUDGEMENT_VALUE, a) == typed_a);
	assert(!pg_occurrence_boundary(&typing, typed_a, PG_JUDGEMENT_CONTEXT, a));
	assert(!pg_occurrence_boundary(&typing, typed_a, PG_JUDGEMENT_VALUE, NULL));
	assert(!pg_occurrence_boundary(&typing, typed_a, PG_JUDGEMENT_INPUT, a));
	assert(typing.proofs.count == 0);
	assert(!pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, NULL, NULL, NULL, 0, NULL));
	assert(!pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, 1, NULL));
	assert(!pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, SIZE_MAX, &body_a));
	for (size_t i = 0; i < 1000; ++i) {
		const struct pg_term *variable = pg_reference(graph, pg_binder(graph));
		assert(pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, variable, NULL, NULL, 0, NULL));
	}
	assert(lambda_a == pg_occurrence(&typing, PG_JUDGEMENT_INPUT, NULL, identity, NULL, NULL, 1, &body_a));
	const struct pg_occurrence *parent = pg_occurrence(&typing, PG_JUDGEMENT_VALUE,
		in_a, pg_application(graph, identity, vx), a, NULL, 1, &typed_a);
	const struct pg_context_map *general = pg_context_map(&typing, in_a, in_a, 1, &typed_a);
	const struct pg_context_map *same = pg_context_map_projection(&typing, in_a, in_a);
	assert(same == general);
	const struct pg_context_map *projection = pg_context_map_projection(&typing, in_a, extended);
	assert(projection && projection->count == 1);
	assert(pg_context_map(&typing, in_a, extended, 1, projection->images) == projection);
	const struct pg_context_map *family_projection = pg_context_map_projection(&typing, family_binding, family_binding);
	assert(family_projection && family_projection->images[0]->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	size_t projections = typing.context_projections.count, maps = typing.context_maps.count;
	size_t occurrences = typing.occurrences.count, terms = graph->terms.count;
	for (size_t i = 0; i < 10000; ++i) {
		assert(pg_context_map_projection(&typing, in_a, in_a) == general);
		assert(pg_context_map_projection(&typing, in_a, extended) == projection);
	}
	assert(!pg_context_map_projection(&typing, in_a, in_b));
	assert(!pg_context_map_projection(&typing, extended, in_a));
	assert(!pg_context_map_projection(&typing, family_binding, in_a));
	assert(typing.context_projections.count == projections && typing.context_maps.count == maps);
	assert(typing.occurrences.count == occurrences && graph->terms.count == terms);
	const struct pg_context_map *empty_map = pg_context_map(&typing, NULL, extended, 0, NULL);
	assert(empty_map && pg_context_map_projection(&typing, NULL, extended) == empty_map);
	const struct pg_occurrence *deep = parent;
	for (size_t i = 0; i < 10000; ++i)
		deep = pg_occurrence_mapped(&typing, PG_JUDGEMENT_VALUE, parent->core, a, NULL, deep, same);
	struct pg_occurrence_input *input = pg_occurrence_input_request(&typing, deep, 0);
	assert(input && !pg_occurrence_input_result(input));
	assert(pg_occurrence_input_advance(input, 0) == PG_INPUT_PENDING);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(input) == typed_a);
	assert(pg_occurrence_input_steps(input) == 20002);
	assert(pg_occurrence_input_request(&typing, deep, 0) == input);
	assert(pg_occurrence_input_advance(input, 1000) == PG_INPUT_READY);
	assert(pg_occurrence_input_steps(input) == 20002);
	input = pg_occurrence_input_request(&typing, parent, 1);
	assert(pg_occurrence_input_advance(input, 10) == PG_INPUT_UNAVAILABLE);
	assert(!pg_occurrence_input_result(input));
	const struct pg_occurrence *scoped = pg_occurrence(&typing, PG_JUDGEMENT_COMPUTATION,
		NULL, identity, a, NULL, 1, &typed_a);
	input = pg_occurrence_input_request(&typing, scoped, 0);
	assert(pg_occurrence_input_advance(input, 10) == PG_INPUT_READY);
	assert(pg_occurrence_input_result(input) == typed_a);
	const struct pg_occurrence *derived = pg_occurrence_derived(&typing, parent, PG_JUDGEMENT_VALUE, vx, a);
	assert(derived && !derived->operand_count && derived->origin == parent && !derived->map);
	assert(!pg_occurrence_scoped_input(deep, 0));
	assert(!pg_occurrence_scoped_input(derived, 0));
	input = pg_occurrence_input_request(&typing, derived, 0);
	assert(pg_occurrence_input_advance(input, 10) == PG_INPUT_UNAVAILABLE);
	assert(pg_occurrence_input_advance(NULL, 10) == PG_INPUT_ERROR);
	/* Repeated component selection has depth independent of the C stack.
	 * One outer transition must not advance the entire dependency chain. */
	for (size_t budget = 1; budget <= 64; budget *= 64) {
		const struct pg_occurrence *nested[10001];
		const struct pg_term *head = pg_reference(graph, pg_binder(graph));
		nested[0] = parent;
		for (size_t i = 1; i <= 10000; ++i)
			nested[i] = pg_occurrence(&typing, PG_JUDGEMENT_VALUE, in_a,
				pg_application(graph, head, nested[i - 1]->core), a, NULL, 1, &nested[i - 1]);
		deep = nested[10000];
		for (size_t i = 10000; i; --i)
			deep = pg_occurrence_selected(&typing, deep, 0, NULL,
				PG_JUDGEMENT_VALUE, nested[i - 1]->core, a);
		input = pg_occurrence_input_request(&typing, deep, 0);
		size_t requests = typing.occurrence_inputs.count;
		assert(pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING);
		assert(pg_occurrence_input_steps(input) == 1);
		assert(typing.occurrence_inputs.count == requests + 1);
		/* Finish a shared dependency separately while the outer query waits. */
		struct pg_occurrence_input *shared = pg_occurrence_input_request(&typing, deep->origin, 0);
		while (pg_occurrence_input_advance(shared, budget) == PG_INPUT_PENDING) {}
		while (pg_occurrence_input_advance(input, budget) == PG_INPUT_PENDING) {}
		assert(pg_occurrence_input_result(input) == typed_a);
		assert(pg_occurrence_input_steps(shared) <= 40002);
		assert(pg_occurrence_input_steps(input) <= 5);
		uint64_t steps = pg_occurrence_input_steps(input);
		requests = typing.occurrence_inputs.count;
		assert(pg_occurrence_input_request(&typing, deep, 0) == input);
		assert(pg_occurrence_input_advance(input, 100000) == PG_INPUT_READY);
		assert(pg_occurrence_input_steps(input) == steps && typing.occurrence_inputs.count == requests);
	}
	assert(!typing.proofs.count && !typing.occurrence_actions.count);
	pg_typing_destroy(&typing);
	puts("typing inputs: persistent contexts and distinct occurrences over shared Core passed");
}

static const struct pg_evidence *checked_normalize(struct pg_typing *typing,
	struct pg_whnf_work *work, const struct pg_evidence *input)
{
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, pg_evidence_subject(input)->core);
	assert(job && pg_whnf_advance(job, 100000) == PG_EVAL_WHNF);
	const struct pg_evidence *result = pg_prove_normalization(typing, input, pg_whnf_certificate(job));
	assert(result && pg_evidence_context(result) == pg_evidence_context(input));
	assert(pg_evidence_classifier(result) == pg_evidence_classifier(input));
	return result;
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
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, x_context, fa_in_x);
	assert(pi && pg_evidence_judgement(pi) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(pi) == pg_universe(&classifiers, 0));
	assert(pg_prove_pi(&typing, &classifiers, x_context, fa_in_x) == pi);
	assert(pg_evidence_premise(pi, 0) == x_context);
	assert(pg_evidence_premise(pi, 1) == fa_in_x);
	assert(!pg_prove_pi(&typing, &classifiers, x_context, a_in_x));
	assert(!pg_prove_pi(&typing, &classifiers, x_context, fa));
	assert(!pg_prove_pi(&typing, &classifiers, empty, fa_in_x));
	assert(!pg_prove_context_extension(&typing, a_context, pg_binder(graph), pi));
	const struct pg_evidence *high = pg_prove_universe(&typing, &classifiers, x_context, 2);
	const struct pg_evidence *fhigh = pg_prove_return_type(&typing, &classifiers, high);
	const struct pg_evidence *high_pi = pg_prove_pi(&typing, &classifiers, x_context, fhigh);
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
	struct pg_whnf_work evaluation;
	assert(pg_whnf_work_init(&evaluation, graph) == 0);
	assert(pg_evidence_subject(checked_normalize(&typing, &evaluation, forced))->core
		== pg_evidence_subject(returned)->core);
	assert(checked_normalize(&typing, &evaluation, returned) == returned);
	assert(checked_normalize(&typing, &evaluation, x_term) == x_term);
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
	const struct pg_evidence *pi_y = pg_prove_pi(&typing, &classifiers, y_context, fa_in_y);
	const struct pg_evidence *return_y = pg_prove_return(&typing, &classifiers, y_term);
	const struct pg_evidence *identity_y = pg_prove_lambda(&typing, pi_y, return_y);
	const struct pg_evidence *app = pg_prove_application(&typing, identity_y, x_term);
	assert(app && pg_evidence_classifier(app) == pg_evidence_classifier(returned));
	assert(pg_prove_application(&typing, identity_y, x_term) == app);
	assert(pg_evidence_premise(app, 0) == identity_y);
	/* Check beta's substitution independently of the evaluator. */
	const struct pg_evidence *beta_images[] = {a_in_x, x_term, x_term};
	const struct pg_evidence *substitution = pg_prove_substitution(&typing, y_context, x_context, 3, beta_images);
	const struct pg_evidence *reduct = pg_prove_reindex(&typing, substitution, return_y);
	assert(reduct && pg_evidence_rule(reduct) == PG_REINDEX);
	assert(pg_evidence_subject(reduct)->core == pg_evidence_subject(returned)->core);
	assert(pg_evidence_classifier(reduct) == pg_evidence_classifier(app));
	assert(pg_prove_reindex(&typing, substitution, return_y) == reduct);
	assert(pg_evidence_subject(checked_normalize(&typing, &evaluation, app))->core
		== pg_evidence_subject(reduct)->core);
	const struct pg_evidence *weakened_function = pg_prove_projection(&typing, y_context, identity_y);
	const struct pg_evidence *weakened_app = pg_prove_application(&typing, weakened_function, y_term);
	const struct pg_evidence *weakened_reduct = checked_normalize(&typing, &evaluation, weakened_app);
	assert(weakened_reduct && pg_evidence_subject(weakened_reduct)->core == pg_evidence_subject(return_y)->core);
	/* Alpha alignment may rename bound pointers, never free arguments. */
	assert(!pg_prove_normalization(&typing, app, pg_evidence_normalization(weakened_reduct)));
	/* Congruent NF exposes checked result inputs, not the source's redexes.
	 * The Lambda body retains its extended context; RETURN/THUNK do not. */
	const struct pg_evidence *suspended_app = pg_prove_thunk(&typing, &classifiers, app);
	const struct pg_evidence *redex_lambda = pg_prove_lambda(&typing, pi_y, weakened_app);
	const struct pg_evidence *nf_scope = pg_prove_context_extension(&typing, x_context, pg_binder(graph), a_in_x);
	const struct pg_evidence *nf_images[] = {a_in_y, y_term};
	const struct pg_evidence *nf_map = pg_prove_substitution(&typing, x_context, y_context, 2, nf_images);
	assert(nf_scope && nf_map);
	const struct pg_evidence *normal_inputs[] = {suspended_app,
		pg_prove_return(&typing, &classifiers, suspended_app),
		redex_lambda, pg_prove_projection(&typing, nf_scope, suspended_app),
		pg_prove_reindex(&typing, nf_map, suspended_app),
		pg_prove_projection(&typing, nf_scope, redex_lambda),
		pg_prove_reindex(&typing, nf_map, redex_lambda)};
	const struct pg_term *input_results[] = {pg_evidence_subject(returned)->core,
		pg_evidence_subject(delayed)->core, NULL, pg_evidence_subject(returned)->core,
		pg_evidence_subject(return_y)->core, NULL, NULL};
	for (size_t i = 0; i < sizeof(normal_inputs) / sizeof(*normal_inputs); ++i) {
		assert(normal_inputs[i]);
		struct pg_nf_job *nf = pg_nf_request(&evaluation, &pg_pure_policy,
			pg_evidence_subject(normal_inputs[i])->core);
		while (pg_nf_advance(nf, i % 2 ? 64 : 1) == PG_NF_PENDING)
			assert(pg_nf_steps(nf) < 100000);
		const struct pg_reduction_certificate *receipt = pg_nf_certificate(nf);
		assert(pg_reduction_congruence(receipt));
		const struct pg_evidence *input = pg_prove_normalization_input(&typing, normal_inputs[i], receipt, 0);
		assert(input);
		const struct pg_context *parent_scope = pg_evidence_context(normal_inputs[i]);
		if (input_results[i]) {
			assert(pg_evidence_subject(input)->core == input_results[i]);
			assert(pg_evidence_context(input) == parent_scope);
		} else {
			const struct pg_term *lambda = pg_nf_result(nf);
			assert(lambda->kind == PG_LAMBDA);
			assert(pg_evidence_subject(input)->core == lambda->as.lambda.body);
			assert(pg_evidence_context(input)->parent == parent_scope);
			assert(pg_evidence_context(input)->binder == lambda->as.lambda.binder);
		}
		struct pg_occurrence_input *source_input = pg_occurrence_input_request(&typing, pg_evidence_subject(normal_inputs[i]), 0);
		assert(pg_occurrence_input_advance(source_input, 0) == PG_INPUT_READY);
		assert(pg_evidence_classifier(input) == pg_occurrence_input_result(source_input)->classifier);
		reconstruct_derivation(&typing, &classifiers, input);
		const struct pg_evidence *normal = pg_prove_normalization(&typing, normal_inputs[i], receipt);
		assert(normal && pg_evidence_subject(normal)->origin == pg_evidence_subject(normal_inputs[i]));
		assert(!pg_evidence_subject(normal)->operand_count);
		if (input_results[i]) {
			const struct pg_evidence *extraction = i == 1 ? pg_prove_return_value(&typing, normal)
				: pg_prove_thunk_computation(&typing, normal);
			assert(extraction && pg_evidence_subject(extraction) == pg_evidence_subject(input));
			reconstruct_derivation(&typing, &classifiers, extraction);
		}
		size_t proofs = typing.proofs.count, occurrences = typing.occurrences.count;
		assert(pg_prove_normalization_input(&typing, normal_inputs[i], receipt, 0) == input);
		assert(typing.proofs.count == proofs && typing.occurrences.count == occurrences);
		assert(!pg_prove_normalization_input(&typing, normal_inputs[i], receipt, 1));
		assert(!pg_prove_normalization_input(&typing, returned, receipt, 0));
		if (i == 0 || i == 2) {
			const struct pg_evidence *wrapped[] = {pg_prove_projection(&typing, nf_scope, normal),
				pg_prove_reindex(&typing, nf_map, normal)};
			for (size_t j = 0; j < 2; ++j) {
				assert(wrapped[j]);
				const struct pg_occurrence *parent = pg_evidence_subject(wrapped[j]);
				struct pg_occurrence_input *blocked = pg_occurrence_input_request(&typing, parent, 0);
				proofs = typing.proofs.count;
				while (pg_occurrence_input_advance(blocked, 1) == PG_INPUT_PENDING) {}
				assert(pg_occurrence_input_blocked_source(blocked) == pg_evidence_subject(normal));
				assert(!pg_occurrence_input_resume_request(&typing, blocked, NULL));
				struct pg_occurrence_input *moved_input = pg_occurrence_input_resume_request(&typing,
					blocked, pg_evidence_subject(input));
				assert(pg_occurrence_input_advance(moved_input, 0) == PG_INPUT_PENDING);
				while (pg_occurrence_input_advance(moved_input, j ? 64 : 1) == PG_INPUT_PENDING)
					assert(pg_occurrence_input_steps(moved_input) < 100000);
				const struct pg_occurrence *moved = pg_occurrence_input_result(moved_input);
				assert(moved && typing.proofs.count == proofs);
				struct pg_typed_query *checked_input = pg_typed_input_request(&typing, wrapped[j], 0);
				assert(checked_input && !pg_typed_query_advance(checked_input, 0));
				uint64_t steps = 0;
				while (!pg_typed_query_advance(checked_input, j ? 64 : 1)) {
					assert(pg_typed_query_steps(checked_input) <= steps + (j ? 64 : 1));
					steps = pg_typed_query_steps(checked_input);
					assert(steps < 10000);
				}
				assert(pg_typed_query_result(checked_input));
				struct pg_nf_job *again = pg_nf_request(&evaluation, &pg_pure_policy, pg_evidence_subject(wrapped[j])->core);
				while (pg_nf_advance(again, j ? 64 : 1) == PG_NF_PENDING) assert(pg_nf_steps(again) < 100000);
				const struct pg_reduction_certificate *again_receipt = pg_nf_certificate(again);
				const struct pg_evidence *child = pg_prove_normalization_input(&typing, wrapped[j], again_receipt, 0);
				assert(child);
				assert(pg_typed_query_result(checked_input) == child);
				assert(moved->core == pg_evidence_subject(child)->core);
				assert(moved->context == pg_evidence_context(child));
				assert(moved->classifier == pg_evidence_classifier(child));
				if (i == 2) {
					const struct pg_term *lambda = pg_nf_result(again);
					assert(lambda->kind == PG_LAMBDA && pg_evidence_subject(child)->core == lambda->as.lambda.body);
					assert(pg_evidence_context(child)->parent == pg_evidence_context(wrapped[j]));
					assert(pg_evidence_context(child)->binder == lambda->as.lambda.binder);
				} else {
					assert(pg_evidence_subject(child)->core == pg_evidence_subject(j ? return_y : returned)->core);
					assert(pg_evidence_context(child) == pg_evidence_context(wrapped[j]));
				}
				reconstruct_derivation(&typing, &classifiers, child);
				proofs = typing.proofs.count; occurrences = typing.occurrences.count;
				uint64_t moved_steps = pg_occurrence_input_steps(moved_input);
				assert(pg_occurrence_input_resume_request(&typing, blocked, pg_evidence_subject(input)) == moved_input);
				assert(pg_occurrence_input_advance(moved_input, 64) == PG_INPUT_READY);
				assert(pg_occurrence_input_steps(moved_input) == moved_steps);
				steps = pg_typed_query_steps(checked_input);
				assert(pg_typed_input_request(&typing, wrapped[j], 0) == checked_input);
				assert(pg_typed_query_advance(checked_input, 64) == 1 && pg_typed_query_steps(checked_input) == steps);
				assert(pg_prove_normalization_input(&typing, wrapped[j], again_receipt, 0) == child);
				assert(typing.proofs.count == proofs && typing.occurrences.count == occurrences);
			}
		}
	}
	assert(!pg_occurrence_input_resume_request(&typing, NULL, pg_evidence_subject(app)));
	assert(!pg_occurrence_input_resume_request(&typing,
		pg_occurrence_input_request(&typing, pg_evidence_subject(suspended_app), 0), pg_evidence_subject(app)));
	assert(!pg_typed_input_request(&typing, empty, 0));
	assert(!pg_typed_input_request(&typing, app, SIZE_MAX));
	struct pg_nf_job *beta_nf = pg_nf_request(&evaluation, &pg_pure_policy, pg_evidence_subject(app)->core);
	assert(pg_nf_advance(beta_nf, 100000) == PG_NF_DONE);
	assert(!pg_reduction_congruence(pg_nf_certificate(beta_nf)));
	assert(pg_reduction_head_congruence(pg_nf_certificate(beta_nf)));
	const struct pg_evidence *beta_result = pg_prove_normalization(&typing, app, pg_nf_certificate(beta_nf));
	struct pg_typed_query *changed_head = pg_typed_input_request(&typing, beta_result, 0);
	while (!pg_typed_query_advance(changed_head, 1)) assert(pg_typed_query_steps(changed_head) < 10000);
	/* The source is APP but its result is RETURN. Input zero must be the
	 * returned value, never the original callee. */
	assert(pg_typed_query_advance(changed_head, 0) == 1 && pg_typed_query_result(changed_head));
	const struct pg_evidence *beta_input = pg_typed_query_result(changed_head);
	assert(pg_evidence_subject(beta_input)->core == pg_evidence_subject(x_term)->core);
	assert(pg_evidence_classifier(beta_input) == pg_evidence_classifier(x_term));
	assert(pg_evidence_context(beta_input) == pg_evidence_context(x_term));
	assert(pg_prove_normalization_input(&typing, app, pg_nf_certificate(beta_nf), 0) == beta_input);
	assert(!pg_prove_normalization_input(&typing, app, pg_nf_certificate(beta_nf), 1));
	reconstruct_derivation(&typing, &classifiers, beta_input);
	const struct pg_evidence *mapped_beta[] = {pg_prove_projection(&typing, nf_scope, beta_result),
		pg_prove_reindex(&typing, nf_map, beta_result)};
	for (size_t i = 0; i < 2; ++i) {
		struct pg_typed_query *query = pg_typed_input_request(&typing, mapped_beta[i], 0);
		assert(query && !pg_typed_query_advance(query, 0));
		while (!pg_typed_query_advance(query, i ? 64 : 1)) assert(pg_typed_query_steps(query) < 10000);
		const struct pg_evidence *child = pg_typed_query_result(query);
		assert(child && pg_evidence_context(child) == pg_evidence_context(mapped_beta[i]));
		assert(pg_evidence_subject(child)->core == pg_evidence_subject(i ? y_term : x_term)->core);
		assert(pg_evidence_classifier(child) == pg_evidence_classifier(x_term));
		reconstruct_derivation(&typing, &classifiers, child);
		uint64_t steps = pg_typed_query_steps(query);
		assert(pg_typed_input_request(&typing, mapped_beta[i], 0) == query);
		assert(pg_typed_query_advance(query, 64) == 1 && pg_typed_query_steps(query) == steps);
	}
	assert(!pg_prove_normalization_input(&typing, NULL, pg_nf_certificate(beta_nf), 0));
	assert(!pg_prove_normalization_input(&typing, suspended_app, NULL, 0));
	assert(!pg_reduction_congruence(NULL));
	assert(!pg_reduction_head_congruence(NULL));
	assert(!pg_prove_application(&typing, identity_y, returned));
	assert(!pg_prove_application(&typing, identity_y, a_in_x));
	const struct pg_evidence *quoted_function = pg_prove_thunk(&typing, &classifiers, identity_y);
	assert(!pg_prove_application(&typing, quoted_function, x_term));
	assert(pg_prove_application(&typing, pg_prove_force(&typing, quoted_function), x_term));
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *z_context = pg_prove_context_extension(&typing, x_context, z, a_in_x);
	const struct pg_evidence *a_in_z = pg_prove_variable(&typing, z_context, a);
	const struct pg_evidence *fa_in_z = pg_prove_return_type(&typing, &classifiers, a_in_z);
	const struct pg_evidence *pi_z = pg_prove_pi(&typing, &classifiers, z_context, fa_in_z);
	const struct pg_evidence *upi_z = pg_prove_thunk_type(&typing, &classifiers, pi_z);
	const struct pg_term *old_classifier = pg_evidence_classifier(quoted_function);
	const struct pg_term *new_classifier = pg_evidence_subject(upi_z)->core;
	assert(old_classifier != new_classifier);
	const struct pg_object *f = pg_binder(graph);
	const struct pg_evidence *f_context = pg_prove_context_extension(&typing, x_context, f, upi_z);
	const struct pg_evidence *f_body = pg_prove_projection(&typing, f_context, returned);
	const struct pg_evidence *f_pi = pg_prove_pi(&typing, &classifiers, f_context,
		pg_prove_projection(&typing, f_context, fa_in_x));
	const struct pg_evidence *ignore_function = pg_prove_lambda(&typing, f_pi, f_body);
	assert(pg_prove_application(&typing, ignore_function, quoted_function));
	assert(!pg_prove_application(&typing, ignore_function, delayed));
	assert(pg_prove_fold(&typing, &classifiers, pg_prove_return(&typing, &classifiers, quoted_function), ignore_function));
	assert(!pg_prove_fold(&typing, &classifiers, returned, ignore_function));
	assert(pg_evidence_classifier(quoted_function) == old_classifier);
	struct pg_whnf_work work;
	struct pg_conversion comparison;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_conversion_init(&comparison, &work, old_classifier, new_classifier) == 0);
	assert(!pg_conversion_certificate(&comparison));
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(!pg_conversion_certificate(&comparison));
	const struct pg_conversion_certificate *certificate = pg_conversion_certificate(&comparison);
	assert(certificate);
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&work);
	const struct pg_evidence *converted = pg_prove_conversion(&typing, quoted_function, upi_z, certificate);
	assert(converted && pg_evidence_classifier(converted) == new_classifier);
	assert(pg_evidence_classifier(quoted_function) == old_classifier);
	assert(pg_evidence_subject(converted) != pg_evidence_subject(quoted_function));
	assert(pg_evidence_subject(converted)->core == pg_evidence_subject(quoted_function)->core);
	assert(pg_evidence_subject(converted)->classifier == new_classifier);
	assert(pg_evidence_subject(quoted_function)->classifier == old_classifier);
	assert(pg_evidence_subject(converted)->origin == pg_evidence_subject(quoted_function));
	assert(!pg_evidence_subject(converted)->operand_count);
	struct pg_occurrence_input *converted_input = pg_occurrence_input_request(&typing, pg_evidence_subject(converted), 0);
	while (pg_occurrence_input_advance(converted_input, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(converted_input) == pg_evidence_subject(quoted_function)->operands[0]);
	assert(pg_evidence_conversion(converted) == certificate);
	assert(pg_evidence_premise(converted, 0) == quoted_function);
	assert(pg_evidence_premise(converted, 1) == upi_z);
	assert(pg_prove_conversion(&typing, quoted_function, upi_z, certificate) == converted);
	assert(!pg_prove_conversion(&typing, quoted_function, upi_z, NULL));
	assert(!pg_prove_conversion(&typing, x_term, upi_z, certificate));
	assert(!pg_prove_conversion(&typing, quoted_function, ufa, certificate));
	assert(!pg_prove_conversion(&typing, quoted_function, pi_z, certificate));
	assert(pg_prove_application(&typing, pg_prove_force(&typing, converted), x_term));
	/* Cancelling weakening must preserve a variable's converted classifier,
	 * including its exact typed origin; no extra identity map is needed. */
	const struct pg_object *converted_binder = pg_binder(graph);
	const struct pg_evidence *converted_scope = pg_prove_context_extension(&typing, x_context,
		converted_binder, pg_prove_classifier(&typing, &classifiers, x_context, quoted_function));
	const struct pg_evidence *converted_variable = pg_prove_conversion(&typing,
		pg_prove_variable(&typing, converted_scope, converted_binder),
		pg_prove_projection(&typing, converted_scope, upi_z), certificate);
	assert(converted_variable);
	const struct pg_evidence *outer_scope = pg_prove_context_extension(&typing, converted_scope,
		pg_binder(graph), pg_prove_projection(&typing, converted_scope, a_in_x));
	const struct pg_evidence *weakened_variable = pg_prove_projection(&typing, outer_scope, converted_variable);
	assert(weakened_variable);
	size_t unproject_nodes = typing.occurrences.count;
	assert(pg_occurrence_unproject(&typing, pg_evidence_subject(weakened_variable),
		pg_evidence_context(converted_scope)) == pg_evidence_subject(converted_variable));
	assert(typing.occurrences.count == unproject_nodes);
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(identity_y), pg_evidence_subject(pi_z)->core) == 0);
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING) {}
	const struct pg_evidence *converted_lambda = pg_prove_conversion(&typing, identity_y, pi_z,
		pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&work);
	assert(converted_lambda && pg_evidence_subject(converted_lambda)->origin == pg_evidence_subject(identity_y));
	assert(pg_occurrence_scoped_input(pg_evidence_subject(converted_lambda), 0) == pg_evidence_subject(return_y));
	const struct pg_evidence *construction_map = NULL;
	size_t construction_proofs = typing.proofs.count, construction_subjects = typing.occurrences.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_prove_construction_origin(&typing, &classifiers, converted_lambda, &construction_map) == identity_y);
	assert(!construction_map && typing.proofs.count == construction_proofs && typing.occurrences.count == construction_subjects);
	{
		/* Swap then duplicate is not duplicate then swap. Accumulating an
		 * outer projection must preserve that order and the dependent type A. */
		const struct pg_evidence *vx = pg_prove_variable(&typing, y_context, x);
		const struct pg_evidence *swap_images[] = {a_in_y, y_term, vx};
		const struct pg_evidence *duplicate_images[] = {a_in_y, vx, vx};
		const struct pg_evidence *swap = pg_prove_substitution(&typing, y_context, y_context, 3, swap_images);
		const struct pg_evidence *duplicate = pg_prove_substitution(&typing, y_context, y_context, 3, duplicate_images);
		const struct pg_evidence *scope = pg_prove_context_extension(&typing, y_context, pg_binder(graph), a_in_y);
		const struct pg_evidence *moved = pg_prove_projection(&typing, scope,
			pg_prove_reindex(&typing, duplicate, pg_prove_reindex(&typing, swap, return_y)));
		const struct pg_evidence *environment = NULL;
		assert(moved && pg_prove_construction_origin(&typing, &classifiers, moved, &environment) == return_y);
		assert(pg_evidence_context_map(environment)->source == pg_evidence_context(y_context));
		assert(pg_evidence_context_map(environment)->destination == pg_evidence_context(scope));
		const struct pg_evidence *image = pg_substitution_image(&typing, environment, y);
		assert(image && pg_evidence_subject(image)->core == pg_evidence_subject(vx)->core);
		assert(pg_evidence_classifier(image) == pg_evidence_subject(a_in_y)->core);
		const struct pg_evidence *rebuilt = pg_prove_reindex(&typing, environment, return_y);
		assert(rebuilt && pg_evidence_subject(rebuilt)->core == pg_evidence_subject(moved)->core);
		size_t proofs = typing.proofs.count, subjects = typing.occurrences.count;
		const struct pg_evidence *again = NULL;
		assert(pg_prove_construction_origin(&typing, &classifiers, moved, &again) == return_y && again == environment);
		assert(typing.proofs.count == proofs && typing.occurrences.count == subjects);
		const struct pg_evidence *parameter = pg_prove_context_extension(&typing, y_context, pg_binder(graph), a_in_y);
		const struct pg_evidence *constant_body = pg_prove_projection(&typing, parameter, return_y);
		const struct pg_evidence *constant = pg_prove_lambda(&typing,
			pg_prove_pi(&typing, &classifiers, parameter,
				pg_prove_classifier(&typing, &classifiers, parameter, constant_body)), constant_body);
		constant = pg_prove_projection(&typing, scope,
			pg_prove_reindex(&typing, duplicate, pg_prove_reindex(&typing, swap, constant)));
		struct pg_typed_query *queries[] = {
			pg_return_body_request(&typing, moved),
			pg_application_body_request(&typing, constant, pg_prove_projection(&typing, scope, y_term))
		};
		for (size_t i = 0; i < 2; ++i) {
			struct pg_typed_query *query = queries[i];
			while (!pg_typed_query_advance(query, i ? 64 : 1)) assert(pg_typed_query_steps(query) < 10000);
			const struct pg_evidence *result = pg_typed_query_result(query);
			const struct pg_evidence *expected = i ? pg_prove_return(&typing, &classifiers, image) : image;
			assert(result && pg_evidence_context(result) == pg_evidence_context(scope));
			assert(pg_evidence_subject(result)->core == pg_evidence_subject(expected)->core);
			assert(pg_evidence_classifier(result) == pg_evidence_classifier(expected));
			proofs = typing.proofs.count; subjects = typing.occurrences.count;
			uint64_t steps = pg_typed_query_steps(query);
			assert(pg_typed_query_advance(query, 64) == 1 && pg_typed_query_steps(query) == steps);
			assert(typing.proofs.count == proofs && typing.occurrences.count == subjects);
		}
	}
	reconstruct_derivation(&typing, &classifiers, converted_lambda);
	const struct pg_evidence *folded = pg_prove_fold(&typing, &classifiers, returned, identity_y);
	assert(folded && pg_evidence_classifier(folded) == pg_evidence_classifier(returned));
	assert(!pg_prove_fold(&typing, &classifiers, x_term, identity_y));
	assert(!pg_prove_fold(&typing, &classifiers, returned, quoted_function));
	assert(pg_evidence_subject(checked_normalize(&typing, &evaluation, folded))->core
		== pg_evidence_subject(returned)->core);
	const struct pg_evidence *reindexed_fold = pg_prove_fold(&typing, &classifiers, reduct, identity_y);
	const struct pg_evidence *reindexed_result = checked_normalize(&typing, &evaluation, reindexed_fold);
	assert(pg_evidence_premise(reindexed_result, 0) == reindexed_fold);
	assert(reindexed_result && pg_evidence_subject(reindexed_result)->core == pg_evidence_subject(returned)->core);
	const struct pg_evidence *weakened_delayed = pg_prove_projection(&typing, y_context, delayed);
	const struct pg_evidence *weakened_forced = pg_prove_force(&typing, weakened_delayed);
	const struct pg_evidence *weakened_return = pg_prove_projection(&typing, y_context, returned);
	const struct pg_evidence *weak_result = checked_normalize(&typing, &evaluation, weakened_forced);
	assert(pg_evidence_premise(weak_result, 0) == weakened_forced);
	assert(pg_evidence_subject(weak_result)->core == pg_evidence_subject(weakened_return)->core);
	assert(pg_evidence_classifier(weak_result) == pg_evidence_classifier(weakened_return));
	const struct pg_evidence *weakened_fold = pg_prove_fold(&typing, &classifiers, weakened_return, weakened_function);
	const struct pg_evidence *weakened_fold_result = checked_normalize(&typing, &evaluation, weakened_fold);
	assert(weakened_fold_result && pg_evidence_context(weakened_fold_result) == pg_evidence_context(y_context));
	assert(pg_evidence_subject(weakened_fold_result)->core == pg_evidence_subject(weakened_return)->core);
	const struct pg_evidence *reindexed_thunk = pg_prove_reindex(&typing, substitution,
		pg_prove_thunk(&typing, &classifiers, return_y));
	const struct pg_evidence *reindexed_force = pg_prove_force(&typing, reindexed_thunk);
	const struct pg_evidence *force_result = checked_normalize(&typing, &evaluation, reindexed_force);
	assert(pg_evidence_premise(force_result, 0) == reindexed_force);
	assert(pg_evidence_subject(force_result)->core == pg_evidence_subject(reduct)->core);
	assert(pg_evidence_classifier(force_result) == pg_evidence_classifier(reduct));
	const struct pg_evidence *exposed = pg_prove_return_value(&typing, reduct);
	assert(exposed && pg_evidence_subject(exposed)->core == pg_evidence_subject(x_term)->core);
	assert(pg_evidence_classifier(exposed) == pg_evidence_classifier(x_term));
	const struct pg_evidence *converted_force = pg_prove_force(&typing, converted);
	const struct pg_evidence *converted_code = checked_normalize(&typing, &evaluation, converted_force);
	assert(pg_evidence_premise(converted_code, 0) == converted_force);
	assert(pg_evidence_classifier(converted_code) == pg_evidence_subject(pi_z)->core);
	/* Beta reads typed construction, independently of the chosen receipt and
	 * of conversion, normalization or suspended-function wrappers. */
	const struct pg_evidence *function_variable = pg_prove_variable(&typing, f_context, f);
	const struct pg_evidence *apply_pi = pg_prove_pi(&typing, &classifiers, f_context,
		pg_prove_projection(&typing, f_context, pi_z));
	const struct pg_evidence *apply_function = pg_prove_lambda(&typing, apply_pi,
		pg_prove_force(&typing, function_variable));
	const struct pg_evidence *nested_application = pg_prove_application(&typing, apply_function, converted);
	const struct pg_evidence *identity_map = pg_prove_substitution_projection(&typing, x_context, x_context);
	const struct pg_evidence *fold_function = pg_prove_fold(&typing, &classifiers,
		pg_prove_return(&typing, &classifiers, converted), apply_function);
	assert(fold_function);
	/* Different callers share the callee's beta work and the Fold prefix,
	 * instead of evaluating them on private continuation stacks. */
	struct pg_typed_query *shared_beta = pg_application_body_request(&typing, apply_function, converted);
	struct pg_typed_query *shared_prefix = pg_return_body_request(&typing,
		pg_prove_return(&typing, &classifiers, converted));
	assert(shared_beta && shared_prefix);
	assert(!pg_typed_query_advance(shared_beta, 0) && !pg_typed_query_advance(shared_prefix, 0));
	const struct pg_evidence *beta_functions[] = {identity_y,
		pg_prove_reindex(&typing, identity_map, identity_y), converted_force,
		converted_code, pg_prove_thunk_computation(&typing, converted), nested_application,
		fold_function, checked_normalize(&typing, &evaluation, fold_function),
		pg_prove_force(&typing, pg_prove_thunk(&typing, &classifiers, fold_function))};
	const struct pg_evidence *beta_results[sizeof(beta_functions) / sizeof(*beta_functions)];
	for (size_t i = 0; i < sizeof(beta_functions) / sizeof(*beta_functions); ++i) {
		struct pg_typed_query *work = pg_application_body_request(&typing, beta_functions[i], x_term);
		assert(work && pg_application_body_request(&typing, beta_functions[i], x_term) == work);
		uint64_t steps = pg_typed_query_steps(work);
		int status = pg_typed_query_advance(work, 0);
		assert(pg_typed_query_steps(work) == steps);
		while (!status) {
			status = pg_typed_query_advance(work, i % 2 ? 64 : 1);
			assert(pg_typed_query_steps(work) <= steps + (i % 2 ? 64 : 1));
			steps = pg_typed_query_steps(work);
			assert(steps < 10000);
		}
		assert(status == 1);
		beta_results[i] = pg_typed_query_result(work);
		assert(pg_prove_application_body(&typing, beta_functions[i], x_term) == beta_results[i]);
		assert(pg_typed_query_steps(work) == steps);
		assert(beta_results[i]);
		assert(pg_evidence_context(beta_results[i]) == pg_evidence_context(x_term));
		assert(pg_alpha_equal(pg_evidence_subject(beta_results[i])->core, pg_evidence_subject(returned)->core) == 1);
		assert(pg_alpha_equal(pg_evidence_classifier(beta_results[i]), pg_evidence_classifier(returned)) == 1);
		reconstruct_derivation(&typing, &classifiers, beta_results[i]);
	}
	assert(pg_application_body_request(&typing, beta_functions[0], x_term) ==
		pg_application_body_request(&typing, beta_functions[1], x_term));
	assert(pg_typed_query_advance(shared_beta, 0) == 1 && pg_typed_query_result(shared_beta));
	assert(pg_typed_query_advance(shared_prefix, 0) == 1);
	assert(pg_typed_query_result(shared_prefix) == converted);
	uint64_t shared_beta_steps = pg_typed_query_steps(shared_beta);
	uint64_t shared_prefix_steps = pg_typed_query_steps(shared_prefix);
	assert(!pg_application_body_request(&typing, NULL, x_term));
	assert(!pg_application_body_request(&typing, identity_y, returned));
	assert(pg_typed_query_advance(NULL, 1) == -1);
	assert(!pg_typed_query_result(NULL) && !pg_typed_query_steps(NULL));
	assert(!pg_return_body_request(&typing, x_term));
	const struct pg_object *suspension = pg_binder(graph);
	const struct pg_evidence *suspension_scope = pg_prove_context_extension(&typing, x_context, suspension,
		pg_prove_classifier(&typing, &classifiers, x_context, delayed));
	const struct pg_evidence *suspension_map = pg_prove_substitution_pair(&typing, identity_map, suspension_scope, delayed);
	const struct pg_evidence *mapped_force = pg_prove_reindex(&typing, suspension_map,
		pg_prove_force(&typing, pg_prove_variable(&typing, suspension_scope, suspension)));
	assert(mapped_force);
	const struct pg_evidence *suspension_identity = pg_prove_abstract(&typing, &classifiers, x_context, suspension_scope,
		pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, suspension_scope, suspension)));
	const struct pg_evidence *returned_suspension = pg_prove_return_value(&typing, checked_normalize(&typing, &evaluation,
		pg_prove_application(&typing, suspension_identity, delayed)));
	assert(returned_suspension);
	assert(pg_evidence_subject(returned_suspension)->core == pg_evidence_subject(delayed)->core);
	assert(pg_evidence_classifier(returned_suspension) == pg_evidence_classifier(delayed));
	const struct pg_evidence *computed_force = pg_prove_force(&typing, returned_suspension);
	const struct pg_evidence *return_sources[] = {returned, app, folded, reindexed_fold, weakened_fold,
		forced, mapped_force, computed_force};
	for (size_t i = 0; i < sizeof(return_sources) / sizeof(*return_sources); ++i) {
		struct pg_typed_query *work = pg_return_body_request(&typing, return_sources[i]);
		assert(work && pg_return_body_request(&typing, return_sources[i]) == work);
		uint64_t steps = pg_typed_query_steps(work);
		pg_typed_query_advance(work, 0);
		assert(pg_typed_query_steps(work) == steps);
		while (!pg_typed_query_advance(work, i % 2 ? 64 : 1)) {
			assert(pg_typed_query_steps(work) <= steps + (i % 2 ? 64 : 1));
			steps = pg_typed_query_steps(work);
			assert(steps < 10000);
		}
		const struct pg_evidence *value = pg_typed_query_result(work);
		assert(value && pg_evidence_context(value) == pg_evidence_context(return_sources[i]));
		assert(pg_alpha_equal(pg_evidence_subject(value)->core, pg_evidence_subject(x_term)->core) == 1);
		steps = pg_typed_query_steps(work);
		assert(pg_typed_query_advance(work, 64) == 1 && pg_typed_query_steps(work) == steps);
	}
	const struct pg_evidence *projected_beta = pg_prove_application_body(&typing, weakened_function, y_term);
	assert(projected_beta && pg_evidence_subject(projected_beta)->core == pg_evidence_subject(return_y)->core);
	const struct pg_evidence *projected_fold = pg_prove_application_body(&typing,
		pg_prove_projection(&typing, y_context, fold_function), y_term);
	assert(projected_fold && pg_evidence_context(projected_fold) == pg_evidence_context(y_term));
	assert(pg_alpha_equal(pg_evidence_subject(projected_fold)->core, pg_evidence_subject(return_y)->core) == 1);
	assert(pg_typed_query_steps(shared_beta) == shared_beta_steps);
	assert(pg_typed_query_steps(shared_prefix) == shared_prefix_steps);
	for (size_t i = 0; i < sizeof(beta_functions) / sizeof(*beta_functions); ++i) {
		struct pg_nf_job *nf = pg_nf_request(&evaluation, &pg_pure_policy, pg_evidence_subject(beta_functions[i])->core);
		while (pg_nf_advance(nf, 64) == PG_NF_PENDING) assert(pg_nf_steps(nf) < 10000);
		const struct pg_term *lambda = pg_nf_result(nf);
		assert(lambda && lambda->kind == PG_LAMBDA);
		const struct pg_evidence *body = pg_prove_normalization_input(&typing, beta_functions[i], pg_nf_certificate(nf), 0);
		assert(body && pg_evidence_subject(body)->core == lambda->as.lambda.body);
		assert(pg_evidence_context(body)->parent == pg_evidence_context(beta_functions[i]));
		assert(pg_evidence_context(body)->binder == lambda->as.lambda.binder);
		reconstruct_derivation(&typing, &classifiers, body);
	}
	assert(!pg_prove_application_body(&typing, identity_y, a_in_x));
	assert(!pg_prove_application_body(&typing, identity_y, returned));
	size_t beta_terms = graph->terms.count, beta_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i)
		for (size_t j = 0; j < sizeof(beta_functions) / sizeof(*beta_functions); ++j)
			assert(pg_prove_application_body(&typing, beta_functions[j], x_term) == beta_results[j]);
	assert(graph->terms.count == beta_terms && typing.proofs.count == beta_proofs);
	const struct pg_evidence *deep_function = identity_y;
	for (size_t i = 0; i < 10000; ++i) {
		deep_function = pg_prove_force(&typing, pg_prove_thunk(&typing, &classifiers, deep_function));
		assert(deep_function);
	}
	struct pg_typed_query *deep_body = pg_application_body_request(&typing, deep_function, x_term);
	assert(deep_body && !pg_typed_query_advance(deep_body, 0));
	assert(!pg_typed_query_steps(deep_body) && !pg_typed_query_result(deep_body));
	assert(!pg_typed_query_advance(deep_body, 1) && pg_typed_query_steps(deep_body) == 1);
	assert(pg_application_body_request(&typing, deep_function, x_term) == deep_body);
	while (!pg_typed_query_advance(deep_body, 64)) assert(pg_typed_query_steps(deep_body) < 100000);
	assert(pg_typed_query_result(deep_body) == beta_results[0]);
	uint64_t deep_steps = pg_typed_query_steps(deep_body);
	assert(deep_steps >= 20000);
	assert(pg_prove_application_body(&typing, deep_function, x_term) == beta_results[0]);
	assert(pg_typed_query_steps(deep_body) == deep_steps);
	size_t reduction_terms = graph->terms.count, reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(checked_normalize(&typing, &evaluation, reindexed_force) == force_result);
		assert(checked_normalize(&typing, &evaluation, reindexed_fold) == reindexed_result);
		assert(checked_normalize(&typing, &evaluation, weakened_fold) == weakened_fold_result);
	}
	assert(graph->terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_evidence *fold_formation = pg_prove_classifier(&typing, &classifiers, x_context, folded);
	assert(fold_formation && pg_evidence_subject(fold_formation)->core == pg_evidence_classifier(folded));
	size_t fold_terms = graph->terms.count, fold_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_fold(&typing, &classifiers, returned, identity_y) == folded);
		assert(pg_prove_classifier(&typing, &classifiers, x_context, folded) == fold_formation);
	}
	assert(graph->terms.count == fold_terms && typing.proofs.count == fold_proofs);
	/* U eta holds for both returned computations and raw Pi computations. */
	const struct pg_evidence *computations[] = {returned, identity_y};
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *quote = pg_prove_thunk(&typing, &classifiers, computations[i]);
		const struct pg_evidence *type = pg_prove_classifier(&typing, &classifiers, x_context, quote);
		const struct pg_object *u = pg_binder(graph);
		const struct pg_evidence *context = pg_prove_context_extension(&typing, x_context, u, type);
		const struct pg_evidence *value = pg_prove_variable(&typing, context, u);
		const struct pg_evidence *code = pg_prove_force(&typing, value);
		const struct pg_evidence *eta = pg_prove_thunk(&typing, &classifiers, code);
		const struct pg_evidence *normal = checked_normalize(&typing, &evaluation, eta);
		assert(normal && pg_evidence_subject(normal)->core == pg_evidence_subject(value)->core);
		assert(pg_evidence_classifier(normal) == pg_evidence_classifier(value));
		assert(pg_prove_classifier(&typing, &classifiers, context, normal));
		if (i) continue;
		const struct pg_evidence *unit = pg_prove_projection(&typing, context, identity_y);
		const struct pg_evidence *sequence = pg_prove_fold(&typing, &classifiers, code, unit);
		normal = checked_normalize(&typing, &evaluation, sequence);
		assert(pg_evidence_subject(normal)->core == pg_evidence_subject(code)->core);
		assert(pg_evidence_classifier(normal) == pg_evidence_classifier(code));
		eta = pg_prove_thunk(&typing, &classifiers, sequence);
		normal = checked_normalize(&typing, &evaluation, eta);
		assert(pg_evidence_subject(normal)->core == pg_evidence_subject(value)->core);
		assert(pg_evidence_classifier(normal) == pg_evidence_classifier(value));
	}
	pg_whnf_work_destroy(&evaluation);
	const struct pg_evidence *x_formation = pg_prove_classifier(&typing, &classifiers, x_context, x_term);
	assert(x_formation && pg_evidence_subject(x_formation)->core == pg_evidence_classifier(x_term));
	const struct pg_evidence *body_formation = pg_prove_classifier(&typing, &classifiers, x_context, returned);
	assert(body_formation && pg_evidence_subject(body_formation)->core == pg_evidence_classifier(returned));
	const struct pg_evidence *synth_pi = pg_prove_pi(&typing, &classifiers, x_context, body_formation);
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
	const struct pg_evidence *u0_value = pg_prove_type_value(&typing, u0);
	assert(u0_value && pg_evidence_subject(u0_value) != pg_evidence_subject(u0));
	assert(pg_evidence_subject(u0_value)->core == pg_evidence_subject(u0)->core);
	assert(pg_evidence_subject(u0_value)->judgement == PG_JUDGEMENT_VALUE);
	const struct pg_evidence *u0_again = pg_prove_value_type(&typing, u0_value);
	assert(u0_again != u0 && pg_evidence_subject(u0_again) == pg_evidence_subject(u0));
	assert(pg_evidence_premise(a_context, 1) == u0);
	const struct pg_evidence *records[] = {empty, u0, a_context, a_type, x_context,
		fa, ufa, pi, high_pi, x_term, returned, delayed, forced, identity,
		identity_y, app, converted, projected_x, projected_formation};
	for (size_t i = 0; i < sizeof(records) / sizeof(*records); ++i) {
		const struct pg_occurrence *subject = pg_evidence_subject(records[i]);
		if (subject) {
			assert(subject->judgement == pg_evidence_judgement(records[i]));
			assert(subject->classifier == pg_evidence_classifier(records[i]));
			assert(subject->context == pg_evidence_context(records[i]));
		}
		reconstruct_derivation(&typing, &classifiers, records[i]);
	}
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
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, a_context, fa);
	assert(pi);
	/* Independent readback can rename a binder in both its body and the
	 * body's dependent classifier. Pi and Lambda use one scope action. */
	const struct pg_object *inner_x = pg_binder(graph);
	const struct pg_evidence *xc = pg_prove_context_extension(&typing, a_context, inner_x, a_type);
	const struct pg_evidence *inner = pg_prove_abstract(&typing, &classifiers, a_context, xc,
		pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, xc, inner_x)));
	const struct pg_evidence *outer = pg_prove_abstract(&typing, &classifiers, empty, a_context, inner);
	const struct pg_evidence *sources[] = {pi, outer,
		pg_prove_force(&typing, pg_prove_thunk(&typing, &classifiers, outer))};
	struct pg_whnf_work normalization;
	assert(!pg_whnf_work_init(&normalization, graph));
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_object *renamed = pg_binder(graph);
		const struct pg_binding_value binding = {a, pg_reference(graph, renamed)};
		const struct pg_evidence *original_child = i ? inner : fa;
		const struct pg_term *body = pg_substitution_compute(&typing.substitutions,
			pg_evidence_subject(original_child)->core, 1, &binding);
		const struct pg_term *alpha = i ? pg_lambda(graph, renamed, body)
			: pg_pi(graph, pg_evidence_subject(u1)->core, renamed, body);
		if (i == 2) alpha = pg_application(graph, pg_reference(graph, &pg_force_operation),
			pg_application(graph, pg_reference(graph, &pg_thunk_operation), alpha));
		struct pg_nf_job *nf = pg_nf_request(&normalization, &pg_pure_policy, alpha);
		while (pg_nf_advance(nf, 1) == PG_NF_PENDING) assert(pg_nf_steps(nf) < 10000);
		const struct pg_evidence *normal = pg_prove_normalization(&typing, sources[i], pg_nf_certificate(nf));
		assert(normal);
		struct pg_typed_query *query = pg_typed_input_request(&typing, normal, i ? 0 : 1);
		while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
		const struct pg_evidence *child = pg_typed_query_result(query);
		assert(child && pg_evidence_context(child)->parent == pg_evidence_context(empty));
		assert(pg_evidence_context(child)->binder == renamed && pg_evidence_subject(child)->core == body);
		const struct pg_term *classifier = pg_substitution_compute(&typing.substitutions,
			pg_evidence_classifier(original_child), 1, &binding);
		assert(pg_evidence_classifier(child) == classifier);
		reconstruct_derivation(&typing, &classifiers, child);
	}
	pg_whnf_work_destroy(&normalization);
	assert(!pg_prove_type_value(&typing, pi));
	assert(!pg_prove_type_value(&typing, fa));
	const struct pg_evidence *upi = pg_prove_thunk_type(&typing, &classifiers, pi);
	assert(pg_prove_type_value(&typing, upi));
	/* Pi's Universe bound can exceed that of its constant body. F/U
	 * inversion preserves the body's construction across that boundary. */
	const struct pg_evidence *closed = pg_prove_return_type(&typing, &classifiers, upi);
	const struct pg_evidence *u3 = pg_prove_universe(&typing, &classifiers, empty, 3);
	const struct pg_evidence *unused = pg_prove_context_extension(&typing, empty, pg_binder(graph), u3);
	const struct pg_evidence *wide = pg_prove_pi(&typing, &classifiers, unused,
		pg_prove_projection(&typing, unused, closed));
	const struct pg_evidence *constant_body = pg_prove_pi_constant_codomain(&typing, wide);
	const struct pg_evidence *exposed_upi = pg_prove_return_content(&typing, constant_body);
	const struct pg_evidence *exposed_pi = pg_prove_thunk_content(&typing, exposed_upi);
	assert(exposed_pi && pg_evidence_classifier(exposed_pi) == pg_evidence_classifier(wide));
	assert(pg_evidence_subject(exposed_upi)->origin == pg_evidence_subject(upi));
	assert(!pg_evidence_subject(exposed_upi)->operand_count);
	assert(pg_evidence_subject(exposed_pi) == pg_occurrence_boundary(&typing, pg_evidence_subject(pi),
		PG_JUDGEMENT_COMPUTATION_TYPE, pg_evidence_classifier(wide)));
	const struct pg_evidence *exposed_domain = pg_prove_pi_domain(&typing, exposed_pi);
	assert(exposed_domain && pg_evidence_subject(exposed_domain) == pg_evidence_subject(u1));
	assert(pg_evidence_rule(exposed_domain) == PG_PI_DOMAIN);
	reconstruct_derivation(&typing, &classifiers, exposed_pi);
	reconstruct_derivation(&typing, &classifiers, exposed_domain);
	size_t input_count = typing.occurrences.count, proof_count = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_return_content(&typing, constant_body) == exposed_upi);
		assert(pg_prove_thunk_content(&typing, exposed_upi) == exposed_pi);
		assert(pg_prove_pi_domain(&typing, exposed_pi) == exposed_domain);
	}
	assert(typing.occurrences.count == input_count && typing.proofs.count == proof_count);
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
	assert(!pg_pi_constant_codomain(pg_evidence_subject(pi)->core));
	assert(!pg_pi_constant_codomain(NULL));
	assert(!pg_pi_constant_codomain(pg_universe(&classifiers, 0)));
	const struct pg_term *constant = pg_return_type(&classifiers, pg_universe(&classifiers, 0));
	assert(pg_pi_constant_codomain(pg_pi(graph, pg_universe(&classifiers, 1), a, constant)) == constant);
	/* A beta-redex that discards its argument is still structurally dependent. */
	const struct pg_term *redex = pg_application(graph, pg_lambda(graph, pg_binder(graph), constant), pg_reference(graph, a));
	assert(!pg_pi_constant_codomain(pg_pi(graph, pg_universe(&classifiers, 1), a, redex)));
	assert(!pg_prove_fold(&typing, &classifiers, pg_prove_return(&typing, &classifiers, argument), function));
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
	const struct pg_evidence *inner_pi = pg_prove_pi(&typing, &classifiers, x_context, inner_fa);
	const struct pg_evidence *outer_pi = pg_prove_pi(&typing, &classifiers, a_context, inner_pi);
	const struct pg_evidence *g_context = pg_prove_context_extension(&typing, empty, g,
		pg_prove_thunk_type(&typing, &classifiers, outer_pi));
	const struct pg_evidence *g_term = pg_prove_force(&typing, pg_prove_variable(&typing, g_context, g));
	const struct pg_evidence *g_argument = pg_prove_type_value(&typing, pg_prove_universe(&typing, &classifiers, g_context, 0));
	struct pg_binding_value image = {a, pg_evidence_subject(g_argument)->core};
	struct pg_substitution *prepared = pg_substitution_request(&typing.substitutions,
		pg_evidence_subject(inner_pi)->core, 1, &image);
	assert(prepared && pg_substitution_advance(prepared, 1) == PG_SUBSTITUTION_PENDING);
	const struct pg_evidence *g_app = pg_prove_application(&typing, g_term, g_argument);
	assert(pg_substitution_status(prepared) == PG_SUBSTITUTION_DONE);
	assert(pg_evidence_classifier(g_app) == pg_substitution_result(prepared));
	uint64_t substituted_steps = pg_substitution_steps(prepared);
	assert(pg_substitution_request(&typing.substitutions, pg_evidence_subject(inner_pi)->core, 1, &image) == prepared);
	assert(pg_substitution_advance(prepared, 100) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_steps(prepared) == substituted_steps);
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
	/* A structural map may first be certified with a projected variable as
	 * its image. Nominal lookup must not chase that receipt back into itself. */
	const struct pg_evidence *projected_a = pg_prove_projection(&typing, source, a_type);
	const struct pg_evidence *projected_image_map = pg_prove_substitution(&typing, a_scope, source, 1, &projected_a);
	assert(projected_image_map && pg_prove_context_map(&typing, pg_evidence_context_map(projected_image_map)) == projected_image_map);
	const struct pg_evidence *unknown_type = pg_prove_value_type(&typing, projected_a);
	for (size_t budget = 1; budget <= 64; budget *= 64) {
		struct pg_inductive_recovery recovery;
		assert(!pg_inductive_recovery_init(&recovery, &typing, unknown_type));
		for (size_t i = 0; i < 16 && !recovery.status; ++i)
			pg_inductive_recovery_advance(&recovery, budget);
		assert(recovery.status < 0);
		pg_inductive_recovery_destroy(&recovery);
	}
	const struct pg_evidence *images[] = {destination_b, destination_y};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, source, destination, 2, images);
	assert(sigma && pg_evidence_judgement(sigma) == PG_JUDGEMENT_SUBSTITUTION);
	assert(pg_prove_substitution(&typing, source, destination, 2, images) == sigma);
	const struct pg_context_map *map = pg_evidence_context_map(sigma);
	assert(map && map->source == pg_evidence_context(source));
	assert(map->destination == pg_evidence_context(destination) && map->count == 2);
	assert(map->images[0] == pg_evidence_subject(destination_b));
	assert(map->images[1] == pg_evidence_subject(destination_y));
	assert(pg_context_map_image(map, a) == map->images[0]);
	assert(pg_context_map_image(map, x) == map->images[1]);
	assert(!pg_context_map_image(map, b));
	assert(!pg_context_map_image(map, NULL));
	assert(!pg_context_map_image(NULL, a));
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	assert(bindings[0].binder == a && bindings[0].value == pg_evidence_subject(destination_b)->core);
	assert(bindings[1].binder == x && bindings[1].value == pg_evidence_subject(destination_y)->core);
	const struct pg_evidence *alternate_b = pg_prove_type_value(&typing,
		pg_prove_value_type(&typing, destination_b));
	assert(alternate_b != destination_b && pg_evidence_subject(alternate_b) == pg_evidence_subject(destination_b));
	const struct pg_evidence *alternate_images[] = {alternate_b, destination_y};
	size_t map_count = typing.context_maps.count;
	const struct pg_evidence *alternate = pg_prove_substitution(&typing, source, destination, 2, alternate_images);
	assert(alternate && alternate != sigma && pg_evidence_context_map(alternate) == map);
	assert(typing.context_maps.count == map_count);
	assert(pg_substitution_image(&typing, sigma, a) == destination_b);
	assert(pg_substitution_image(&typing, alternate, a) == alternate_b);
	assert(!pg_evidence_context_map(destination_b) && !pg_evidence_context_map(NULL));
	assert(!pg_context_map(&typing, map->source, map->destination, 1, map->images));
	struct pg_reindex split, whole;
	assert(pg_reindex_init(&split, &typing, sigma, source_x) == 0);
	assert(pg_reindex_init(&whole, &typing, sigma, source_x) == 0);
	size_t pending_proofs = typing.proofs.count;
	assert(pg_reindex_advance(&split, 0) == PG_REINDEX_PENDING);
	assert(pg_reindex_steps(&split) == 0);
	while (pg_reindex_status(&split) == PG_REINDEX_PENDING) {
		assert(!pg_reindex_result(&split));
		assert(typing.proofs.count == pending_proofs);
		uint64_t steps = pg_reindex_steps(&split);
		pg_reindex_advance(&split, 1);
		assert(pg_reindex_steps(&split) == steps + 1);
	}
	size_t substitutions = typing.substitutions.jobs.count, substituted_terms = graph->terms.count;
	assert(pg_reindex_advance(&whole, UINT64_MAX) == PG_REINDEX_DONE);
	/* These consumers share a work store, unlike independent split-fuel runs. */
	assert(pg_reindex_steps(&whole) <= pg_reindex_steps(&split));
	assert(typing.substitutions.jobs.count == substitutions && graph->terms.count == substituted_terms);
	assert(pg_reindex_result(&whole) == pg_reindex_result(&split));
	assert(typing.proofs.count == pending_proofs + 1);
	const struct pg_evidence *split_result = pg_reindex_result(&split);
	pg_reindex_destroy(&split);
	pg_reindex_destroy(&whole);
	const struct pg_evidence *reindexed = pg_prove_reindex(&typing, sigma, source_x);
	assert(reindexed == split_result);
	struct pg_occurrence_action *action = pg_occurrence_action_request(&typing, map, pg_evidence_subject(source_x));
	assert(action && pg_occurrence_action_result(action) == pg_evidence_subject(destination_y));
	uint64_t action_steps = pg_occurrence_action_steps(action);
	const struct pg_evidence *alternative_result = pg_prove_reindex(&typing, alternate, source_x);
	assert(alternative_result != reindexed && pg_evidence_subject(alternative_result) == pg_evidence_subject(reindexed));
	const struct pg_occurrence *same_subject = pg_evidence_subject(reindexed);
	int saw_original = 0, saw_alternative = 0;
	size_t receipt_count = 0;
	for (const struct pg_evidence *receipt = pg_evidence_for_subject(&typing, same_subject, NULL);
		receipt; receipt = pg_evidence_for_subject(&typing, same_subject, receipt)) {
		assert(pg_evidence_subject(receipt) == same_subject);
		saw_original |= receipt == reindexed;
		saw_alternative |= receipt == alternative_result;
		assert(++receipt_count <= typing.proofs.count);
	}
	assert(saw_original && saw_alternative);
	assert(!pg_evidence_for_subject(&typing, pg_evidence_subject(source_x), reindexed));
	assert(!pg_evidence_for_subject(&typing, NULL, NULL));
	assert(pg_occurrence_action_request(&typing, pg_evidence_context_map(alternate), pg_evidence_subject(source_x)) == action);
	assert(pg_occurrence_action_steps(action) == action_steps);
	assert(pg_reindex_init(&split, &typing, sigma, source_x) == 0);
	assert(pg_reindex_advance(&split, 0) == PG_REINDEX_DONE);
	assert(pg_reindex_steps(&split) == 0);
	pg_reindex_destroy(&split);
	assert(pg_reindex_init(&split, &typing, sigma, destination_y) != 0);
	assert(pg_reindex_status(&split) == PG_REINDEX_ERROR);
	pg_reindex_destroy(&split);
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
	const struct pg_evidence *alternate_return = pg_prove_reindex(&typing, alternate, returned);
	assert(alternate_return != reindexed_return && pg_evidence_subject(alternate_return) == pg_evidence_subject(reindexed_return));
	assert(pg_typed_input_request(&typing, alternate_return, 0) == pg_typed_input_request(&typing, reindexed_return, 0));
	assert(pg_evidence_subject(reindexed_return)->origin == pg_evidence_subject(returned));
	assert(pg_evidence_subject(reindexed_return)->map == map);
	assert(pg_evidence_subject(reindexed_return)->operand_count == 0);
	size_t input_proofs = typing.proofs.count;
	struct pg_occurrence_input *input = pg_occurrence_input_request(&typing, pg_evidence_subject(reindexed_return), 0);
	assert(input && !pg_occurrence_input_result(input));
	assert(pg_occurrence_input_advance(input, 0) == PG_INPUT_PENDING);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING)
		assert(!pg_occurrence_input_result(input));
	assert(pg_occurrence_input_result(input) == pg_evidence_subject(destination_y));
	assert(typing.proofs.count == input_proofs);
	uint64_t input_steps = pg_occurrence_input_steps(input);
	assert(pg_occurrence_input_request(&typing, pg_evidence_subject(reindexed_return), 0) == input);
	assert(pg_occurrence_input_advance(input, 0) == PG_INPUT_READY);
	assert(pg_occurrence_input_steps(input) == input_steps);
	const struct pg_evidence *extracted = pg_prove_return_value(&typing, reindexed_return);
	assert(extracted && pg_evidence_subject(extracted) == pg_evidence_subject(destination_y));
	assert(pg_evidence_premise(extracted, 0) == reindexed_return);
	/* A lazily mapped input has no receipt until ordinary rules check its map.
	 * Lookup itself is read-only; invalid boundaries never gain acceptance. */
	const struct pg_evidence *suspended_return = pg_prove_thunk(&typing, &classifiers, returned);
	struct pg_occurrence_action *suspended_action = pg_occurrence_action_request(&typing,
		map, pg_evidence_subject(suspended_return));
	while (pg_occurrence_action_advance(suspended_action, 1) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_occurrence *mapped_suspended = pg_occurrence_action_result(suspended_action);
	assert(mapped_suspended && !pg_evidence_for_subject(&typing, mapped_suspended, NULL));
	size_t before_receipt = typing.proofs.count;
	const struct pg_evidence *suspended_receipt = pg_prove_structural_subject(&typing, mapped_suspended);
	assert(suspended_receipt && pg_evidence_subject(suspended_receipt) == mapped_suspended);
	assert(typing.proofs.count == before_receipt + 1);
	assert(pg_evidence_rule(suspended_receipt) == PG_REINDEX);
	for (size_t i = 0; i < 100; ++i)
		assert(pg_prove_structural_subject(&typing, mapped_suspended) == suspended_receipt);
	assert(typing.proofs.count == before_receipt + 1);
	const struct pg_occurrence *invalid_suspended = pg_occurrence_boundary(&typing,
		mapped_suspended, PG_JUDGEMENT_VALUE, pg_universe(&classifiers, 3));
	assert(!pg_prove_structural_subject(&typing, invalid_suspended));
	assert(!pg_evidence_for_subject(&typing, invalid_suspended, NULL));
	assert(typing.proofs.count == before_receipt + 1);
	struct pg_typing foreign;
	assert(!pg_typing_init(&foreign, graph));
	assert(!pg_evidence_for_subject(&foreign, mapped_suspended, NULL));
	assert(!pg_evidence_for_subject(&foreign, mapped_suspended, suspended_receipt));
	assert(!pg_prove_structural_subject(&foreign, mapped_suspended));
	pg_typing_destroy(&foreign);
	assert(!pg_prove_reindex(&typing, sigma, destination_y));
	assert(!pg_prove_reindex(&typing, sigma, source));
	assert(!pg_prove_projection(&typing, destination, sigma));
	assert(!pg_prove_substitution(&typing, source, destination, 1, images));
	const struct pg_evidence *bad[] = {destination_y, destination_b};
	const struct pg_occurrence *bad_subjects[] = {pg_evidence_subject(bad[0]), pg_evidence_subject(bad[1])};
	size_t accepted = typing.proofs.count;
	const struct pg_context_map *bad_map = pg_context_map(&typing, map->source, map->destination, 2, bad_subjects);
	assert(bad_map);
	assert(typing.proofs.count == accepted);
	const struct pg_occurrence *invalid_map_result = pg_occurrence_mapped(&typing,
		PG_JUDGEMENT_COMPUTATION, pg_evidence_subject(reindexed_return)->core,
		pg_evidence_classifier(reindexed_return), NULL, pg_evidence_subject(returned), bad_map);
	assert(invalid_map_result && !pg_prove_structural_subject(&typing, invalid_map_result));
	assert(!pg_evidence_for_subject(&typing, invalid_map_result, NULL));
	assert(typing.proofs.count == accepted);
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	assert(typing.proofs.count == accepted);
	bad[0] = destination_b;
	bad[1] = pg_prove_return(&typing, &classifiers, destination_y);
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	bad[1] = source_x;
	assert(!pg_prove_substitution(&typing, source, destination, 2, bad));
	const struct pg_evidence *closed = pg_prove_substitution(&typing, empty, destination, 0, NULL);
	assert(closed && pg_prove_reindex(&typing, closed, universe));
	assert(pg_prove_substitution_rebase(&typing, destination, sigma) == sigma);
	assert(pg_prove_substitution_rebase(&typing, empty, closed));
	assert(!pg_prove_substitution_rebase(&typing, empty, sigma));
	assert(!pg_prove_substitution_rebase(&typing, source, sigma));
	assert(!pg_prove_substitution_rebase(&typing, NULL, sigma));
	assert(!pg_prove_substitution_rebase(&typing, empty, NULL));
	assert(!pg_prove_substitution_rebase(&typing, empty, source));
	const struct pg_evidence *extended_destination = pg_prove_context_extension(&typing,
		destination, pg_binder(graph), pg_prove_classifier(&typing, &classifiers, destination, destination_y));
	assert(extended_destination);
	size_t before_projection = typing.proofs.count, actions = typing.occurrence_actions.count;
	const struct pg_evidence *projected = pg_prove_projection(&typing, extended_destination, reindexed_return);
	assert(projected && typing.proofs.count == before_projection + 1);
	assert(typing.occurrence_actions.count == actions);
	assert(pg_evidence_subject(projected)->origin == pg_evidence_subject(reindexed_return));
	assert(pg_evidence_subject(projected)->operand_count == 0);
	assert(pg_prove_projection(&typing, extended_destination, reindexed_return) == projected);
	assert(typing.proofs.count == before_projection + 1 && typing.occurrence_actions.count == actions);
	const struct pg_evidence *projection_map = pg_prove_substitution_projection(&typing, destination, extended_destination);
	assert(pg_evidence_subject(projected)->map == pg_evidence_context_map(projection_map));
	assert(pg_evidence_subject(pg_prove_reindex(&typing, projection_map, reindexed_return)) == pg_evidence_subject(projected));
	assert(pg_occurrence_unproject(&typing, pg_evidence_subject(projected), pg_evidence_context(destination)) ==
		pg_evidence_subject(reindexed_return));
	assert(!pg_occurrence_unproject(&typing, pg_evidence_subject(projected), NULL));
	assert(!pg_occurrence_unproject(&typing, pg_evidence_subject(projected), pg_evidence_context(source)));
	const struct pg_occurrence *changed_boundary = pg_occurrence_boundary(&typing,
		pg_evidence_subject(projected), PG_JUDGEMENT_COMPUTATION, pg_evidence_classifier(universe));
	assert(changed_boundary && !pg_occurrence_unproject(&typing, changed_boundary, pg_evidence_context(destination)));
	size_t unprojected_subjects = typing.occurrences.count, unprojected_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_occurrence_unproject(&typing, pg_evidence_subject(projected), pg_evidence_context(destination)) ==
			pg_evidence_subject(reindexed_return));
	assert(typing.occurrences.count == unprojected_subjects && typing.proofs.count == unprojected_proofs);
	const struct pg_evidence *new_scope = pg_prove_context_extension(&typing, extended_destination,
		pg_binder(graph), pg_prove_projection(&typing, extended_destination,
			pg_prove_classifier(&typing, &classifiers, destination, destination_y)));
	assert(new_scope);
	const struct pg_context_map *new_projection = pg_context_map_projection(&typing,
		pg_evidence_context(destination), pg_evidence_context(new_scope));
	const struct pg_occurrence *new_projected = pg_occurrence_projection(&typing,
		new_projection, mapped_suspended);
	assert(new_projected && !pg_evidence_for_subject(&typing, new_projected, NULL));
	const struct pg_evidence *new_projected_proof = pg_prove_structural_subject(&typing, new_projected);
	assert(new_projected_proof && pg_evidence_subject(new_projected_proof) == new_projected);
	assert(pg_evidence_context_map(pg_evidence_premise(new_projected_proof, 0)) == new_projection);
	extracted = pg_prove_return_value(&typing, projected);
	assert(extracted && pg_evidence_context(extracted) == pg_evidence_context(extended_destination));
	assert(pg_evidence_subject(extracted)->core == pg_reference(graph, y));
	assert(!pg_evidence_subject(extracted)->operand_count);
	assert(!pg_occurrence_projection(&typing, map, pg_evidence_subject(returned)));
	const struct pg_evidence *extended_map = pg_prove_substitution_compose(&typing, sigma,
		pg_prove_substitution_projection(&typing, destination, extended_destination));
	assert(extended_map);
	assert(pg_prove_substitution_rebase(&typing, destination, extended_map) == sigma);
	size_t image_proofs = typing.proofs.count, image_terms = graph->terms.count;
	assert(pg_substitution_image(&typing, sigma, a) == destination_b);
	assert(pg_substitution_image(&typing, sigma, x) == destination_y);
	assert(!pg_substitution_image(&typing, closed, a));
	assert(!pg_substitution_image(&typing, sigma, NULL));
	assert(!pg_substitution_image(&typing, source, a));
	assert(!pg_substitution_image(&typing, NULL, a));
	assert(typing.proofs.count == image_proofs && graph->terms.count == image_terms);
	/* Telescope instantiation and the flat input API have one authority. */
	const struct pg_evidence *type_pair = pg_prove_substitution_pair(&typing, closed, a_scope, destination_b);
	assert(type_pair);
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *shifted_type = pg_prove_reindex(&typing, closed, u0);
	const struct pg_evidence *type_value = pg_prove_type_value(&typing, shifted_type);
	const struct pg_evidence *type_values = pg_prove_substitution_pair(&typing, closed, a_scope, type_value);
	const struct pg_evidence *rebased_values = pg_prove_substitution_rebase(&typing, empty, type_values);
	assert(rebased_values);
	const struct pg_evidence *rebased_type = pg_substitution_image(&typing, rebased_values, a);
	assert(pg_evidence_judgement(rebased_type) == PG_JUDGEMENT_VALUE);
	assert(pg_evidence_subject(rebased_type)->core == pg_evidence_subject(u0)->core);
	const struct pg_evidence *roundtrip = pg_prove_type_value(&typing, pg_prove_value_type(&typing, type_value));
	const struct pg_evidence *roundtrip_values = pg_prove_substitution_pair(&typing, closed, a_scope, roundtrip);
	assert(pg_prove_substitution_rebase(&typing, empty, roundtrip_values));
	reconstruct_derivation(&typing, &classifiers, rebased_values);
	/* A captured Lambda keeps its substituted type argument; a removed free
	 * argument cannot be justified merely by the Lambda's erased shape. */
	const struct pg_evidence *function = pg_prove_abstract(&typing, &classifiers, a_scope, source, returned);
	const struct pg_evidence *suspended = pg_prove_thunk(&typing, &classifiers, function);
	const struct pg_object *function_binder = pg_binder(graph);
	const struct pg_evidence *function_scope = pg_prove_context_extension(&typing, a_scope, function_binder,
		pg_prove_classifier(&typing, &classifiers, a_scope, suspended));
	const struct pg_evidence *function_values = pg_prove_substitution_pair(&typing, type_pair,
		function_scope, pg_prove_reindex(&typing, type_pair, suspended));
	const struct pg_evidence *function_image = pg_substitution_image(&typing, function_values, function_binder);
	const struct pg_evidence *function_variable = pg_prove_variable(&typing, function_scope, function_binder);
	action = pg_occurrence_action_request(&typing, pg_evidence_context_map(function_values),
		pg_evidence_subject(function_variable));
	while (pg_occurrence_action_advance(action, 1) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_occurrence *function_subject = pg_occurrence_action_result(action);
	assert(function_subject && !pg_evidence_for_subject(&typing, function_subject, NULL));
	const struct pg_evidence *mapped_function = pg_prove_structural_subject(&typing, function_subject);
	assert(mapped_function == pg_prove_reindex(&typing, function_values, function_variable));
	assert(mapped_function && pg_evidence_subject(mapped_function)->core == pg_evidence_subject(function_image)->core);
	assert(pg_evidence_classifier(mapped_function) != pg_evidence_classifier(function_image));
	assert(pg_alpha_equal(pg_evidence_classifier(mapped_function), pg_evidence_classifier(function_image)) == 1);
	assert(function_subject->map == pg_evidence_context_map(function_values));
	assert(function_subject->origin == pg_evidence_subject(function_variable));
	/* The typed action itself creates no evidence, and completed work is reused. */
	const struct pg_context_map *function_map = pg_evidence_context_map(type_pair);
	size_t before_action = typing.proofs.count;
	action = pg_occurrence_action_request(&typing, function_map, pg_evidence_subject(function));
	assert(action && pg_occurrence_action_advance(action, 0) == PG_SUBSTITUTION_PENDING);
	while (pg_occurrence_action_advance(action, 1) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_occurrence *mapped_lambda = pg_occurrence_action_result(action);
	assert(mapped_lambda && mapped_lambda->origin == pg_evidence_subject(function));
	assert(mapped_lambda->map == function_map && mapped_lambda->operand_count == 0);
	assert(mapped_lambda->context == function_map->destination && typing.proofs.count == before_action);
	assert(mapped_lambda->core->kind == PG_LAMBDA);
	assert(mapped_lambda->core->as.lambda.binder != x);
	struct pg_occurrence_input *under_map = pg_occurrence_input_mapped_request(&typing,
		pg_evidence_subject(function), 0, function_map);
	while (pg_occurrence_input_advance(under_map, 1) == PG_INPUT_PENDING) {}
	const struct pg_occurrence *under_body = pg_occurrence_input_result(under_map);
	assert(under_body && under_body->context->parent == function_map->destination);
	assert(!pg_context_lookup(function_map->destination, under_body->context->binder));
	assert(pg_alpha_equal(pg_lambda(graph, under_body->context->binder, under_body->core), mapped_lambda->core) == 1);
	assert(typing.proofs.count == before_action);
	assert(pg_occurrence_input_mapped_request(&typing, pg_evidence_subject(function), 0, function_map) == under_map);
	const struct pg_context_map *body_map = pg_context_map_lift(&typing, function_map,
		pg_evidence_context(source), mapped_lambda->core->as.lambda.binder);
	assert(body_map && body_map->source == pg_evidence_context(source));
	assert(body_map->destination->parent == mapped_lambda->context);
	assert(body_map->destination->judgement == PG_JUDGEMENT_VALUE);
	assert(body_map->destination->declared_type == pg_reference(graph, b));
	struct pg_occurrence_action *body_action = pg_occurrence_action_request(&typing, body_map, pg_evidence_subject(returned));
	while (pg_occurrence_action_advance(body_action, 1) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_occurrence *mapped_body = pg_occurrence_action_result(body_action);
	assert(mapped_body && mapped_body->core == mapped_lambda->core->as.lambda.body);
	assert(mapped_body->context == body_map->destination && typing.proofs.count == before_action);
	input = pg_occurrence_input_request(&typing, mapped_lambda, 0);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(input) == mapped_body);
	assert(typing.proofs.count == before_action);
	assert(body_map == pg_context_map_lift(&typing, function_map, pg_evidence_context(source), mapped_lambda->core->as.lambda.binder));
	assert(!pg_context_map_lift(&typing, function_map, pg_evidence_context(destination), y));
	assert(!pg_context_map_lift(&typing, function_map, pg_evidence_context(source), b));
	action_steps = pg_occurrence_action_steps(action);
	assert(pg_occurrence_action_advance(action, 64) == PG_SUBSTITUTION_DONE);
	assert(pg_occurrence_action_steps(action) == action_steps);
	assert(pg_evidence_subject(pg_prove_reindex(&typing, type_pair, function)) == mapped_lambda);
	input = pg_occurrence_input_request(&typing, function_subject, 0);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	struct pg_occurrence_input *image_input = pg_occurrence_input_request(&typing, pg_evidence_subject(function_image), 0);
	while (pg_occurrence_input_advance(image_input, 1) == PG_INPUT_PENDING) {}
	assert(pg_occurrence_input_result(input) && pg_occurrence_input_result(input) == pg_occurrence_input_result(image_input));
	/* Open projected scopes without reusing a binder already in the context. */
	const struct pg_evidence *shadowed = pg_prove_projection(&typing, source, function);
	input = pg_occurrence_input_request(&typing, pg_evidence_subject(shadowed), 0);
	input_proofs = typing.proofs.count;
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	const struct pg_occurrence *opened_body = pg_occurrence_input_result(input);
	assert(opened_body && opened_body->context->parent == pg_evidence_context(source));
	assert(opened_body->context->binder != x);
	assert(!pg_context_lookup(pg_evidence_context(source), opened_body->context->binder));
	assert(pg_alpha_equal(pg_lambda(graph, opened_body->context->binder, opened_body->core),
		pg_evidence_subject(shadowed)->core) == 1);
	assert(typing.proofs.count == input_proofs);
	const struct pg_evidence *pi = pg_prove_classifier(&typing, &classifiers, a_scope, function);
	const struct pg_evidence *mapped_pi = pg_prove_reindex(&typing, type_pair, pi);
	/* Classifier recovery resumes the same typed edge query used by views,
	 * including its substitution work, rather than copying a map stack. */
	struct pg_occurrence_input *type_input = pg_occurrence_type_request(&typing, mapped_lambda);
	uint64_t type_steps = pg_occurrence_input_steps(type_input);
	struct pg_classifier_recovery classifier_work;
	const struct pg_evidence *classifier_function = pg_prove_reindex(&typing, type_pair, function);
	assert(!pg_classifier_recovery_init(&classifier_work, &typing, &classifiers, destination, classifier_function));
	assert(!pg_classifier_recovery_advance(&classifier_work, 0));
	assert(pg_occurrence_input_steps(type_input) == type_steps);
	size_t classifier_calls = 0;
	while (!pg_classifier_recovery_advance(&classifier_work, 1)) {
		assert(pg_occurrence_input_steps(type_input) <= type_steps + 1);
		type_steps = pg_occurrence_input_steps(type_input);
		assert(++classifier_calls < 10000);
	}
	assert(classifier_work.status == 1 && classifier_work.result == mapped_pi);
	assert(pg_occurrence_input_result(type_input) == pg_evidence_subject(mapped_pi));
	pg_classifier_recovery_destroy(&classifier_work);
	type_steps = pg_occurrence_input_steps(type_input);
	input_proofs = typing.proofs.count;
	assert(pg_prove_classifier(&typing, &classifiers, destination, classifier_function) == mapped_pi);
	assert(pg_occurrence_input_steps(type_input) == type_steps && typing.proofs.count == input_proofs);
	const struct pg_term *pi_domain, *pi_codomain;
	const struct pg_object *pi_binder;
	assert(mapped_pi && pg_pi_view(pg_evidence_subject(mapped_pi)->core, &pi_domain, &pi_binder, &pi_codomain));
	input = pg_occurrence_input_request(&typing, pg_evidence_subject(mapped_pi), 1);
	input_proofs = typing.proofs.count;
	assert(pg_occurrence_input_advance(input, 0) == PG_INPUT_PENDING);
	while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
	const struct pg_occurrence *typed_codomain = pg_occurrence_input_result(input);
	assert(typed_codomain && typed_codomain->context->parent == pg_evidence_context(destination));
	assert(typed_codomain->context->binder == pi_binder);
	assert(pg_alpha_equal(typed_codomain->core, pi_codomain) == 1);
	assert(typing.proofs.count == input_proofs);
	struct pg_occurrence_action *instantiation = pg_occurrence_instantiate_request(&typing,
		typed_codomain, pg_evidence_subject(destination_y));
	assert(instantiation && pg_occurrence_action_advance(instantiation, 0) == PG_SUBSTITUTION_PENDING);
	while (pg_occurrence_action_advance(instantiation, 1) == PG_SUBSTITUTION_PENDING) {}
	assert(pg_occurrence_action_result(instantiation));
	assert(typing.proofs.count == input_proofs);
	assert(!pg_occurrence_instantiate_request(&typing, typed_codomain, pg_evidence_subject(source_x)));
	const struct pg_evidence *applied_codomain = pg_prove_pi_codomain(&typing, mapped_pi, destination_y);
	const struct pg_occurrence *applied_type = pg_evidence_subject(applied_codomain);
	assert(applied_type && applied_type->origin == pg_evidence_subject(mapped_pi) && applied_type->selection == 2);
	assert(applied_type->operand_count == 1 && applied_type->operands[0] == pg_evidence_subject(destination_y));
	assert(pg_alpha_equal(applied_type->core, pg_occurrence_action_result(instantiation)->core) == 1);
	assert(applied_type->context == pg_occurrence_action_result(instantiation)->context);
	const struct pg_evidence *returned_type = pg_prove_return_content(&typing, applied_codomain);
	assert(returned_type && pg_evidence_subject(returned_type)->core == pg_reference(graph, b));
	assert(pg_evidence_context(returned_type) == pg_evidence_context(destination));
	reconstruct_derivation(&typing, &classifiers, applied_codomain);
	assert(pg_occurrence_instantiate_request(&typing, typed_codomain,
		pg_evidence_subject(destination_y)) == instantiation);
	const struct pg_evidence *rebased_function = pg_prove_substitution_rebase(&typing, b_scope, function_values);
	assert(rebased_function);
	assert(pg_evidence_judgement(pg_substitution_image(&typing, rebased_function, function_binder)) == PG_JUDGEMENT_VALUE);
	assert(!pg_prove_substitution_rebase(&typing, empty, function_values));
	reconstruct_derivation(&typing, &classifiers, rebased_function);
	assert(pg_prove_substitution_pair(&typing, type_pair, source, destination_y) == sigma);
	assert(!pg_prove_substitution_pair(&typing, closed, source, destination_y));
	assert(!pg_prove_substitution_pair(&typing, type_pair, source, destination_b));
	assert(!pg_prove_substitution_pair(&typing, type_pair, source, source_x));
	assert(!pg_prove_substitution_pair(&typing, type_pair, source,
		pg_prove_return(&typing, &classifiers, destination_y)));
	assert(!pg_prove_substitution_pair(&typing, type_pair, source, NULL));
	assert(!pg_prove_substitution_pair(&typing, source, source, destination_y));
	const struct pg_evidence *fb = pg_prove_return_type(&typing, &classifiers, destination_b);
	const struct pg_evidence *pattern_type = pg_prove_pattern_type(&typing, &classifiers, empty, sigma, fb);
	assert(pattern_type && pg_evidence_context(pattern_type) == pg_evidence_context(source));
	assert(pg_evidence_subject(pattern_type)->core == pg_return_type(&classifiers, pg_reference(graph, a)));
	reconstruct_derivation(&typing, &classifiers, pattern_type);
	assert(!pg_prove_pattern_type(&typing, &classifiers, empty, sigma, destination_y));
	assert(!pg_prove_pattern_type(&typing, &classifiers, source, sigma, fb));
	assert(!pg_prove_pattern_type(&typing, &classifiers, empty, NULL, fb));
	assert(!pg_prove_pattern_type(&typing, NULL, empty, sigma, fb));
	assert(!pg_prove_pattern_type(NULL, &classifiers, empty, sigma, fb));
	const struct pg_evidence *one_variable_pattern = pg_prove_substitution_pair(&typing, closed, a_scope, destination_b);
	pattern_type = pg_prove_pattern_type(&typing, &classifiers, empty, one_variable_pattern, fb);
	assert(pattern_type && pg_evidence_context(pattern_type) == pg_evidence_context(a_scope));
	assert(pg_evidence_subject(pattern_type)->core == pg_return_type(&classifiers, pg_reference(graph, a)));
	/* Omitting the type variable cannot justify a result that still uses it. */
	assert(!pg_prove_pattern_type(&typing, &classifiers, empty, closed, fb));
	const struct pg_evidence *duplicate_scope = pg_prove_context_extension(&typing, a_scope,
		pg_binder(graph), pg_prove_projection(&typing, a_scope, universe));
	const struct pg_evidence *duplicate = pg_prove_substitution_pair(&typing,
		one_variable_pattern, duplicate_scope, destination_b);
	assert(duplicate && !pg_prove_pattern_type(&typing, &classifiers, empty, duplicate, fb));
	const struct pg_evidence *universe_value = pg_prove_type_value(&typing,
		pg_prove_universe(&typing, &classifiers, empty, 0));
	const struct pg_evidence *nonvariable = pg_prove_substitution_pair(&typing,
		pg_prove_substitution_projection(&typing, empty, empty), a_scope, universe_value);
	/* A closed result can ignore a nonvariable image; it is not inverted. */
	pattern_type = pg_prove_pattern_type(&typing, &classifiers, empty, nonvariable,
		pg_prove_return_type(&typing, &classifiers, universe_value));
	assert(nonvariable && pattern_type);
	assert(pg_evidence_subject(pattern_type)->core == pg_return_type(&classifiers,
		pg_evidence_subject(universe_value)->core));
	reconstruct_derivation(&typing, &classifiers, pattern_type);
	const struct pg_object *extra = pg_binder(graph), *index = pg_binder(graph);
	const struct pg_evidence *prefix_universe = pg_prove_projection(&typing, a_scope, universe);
	const struct pg_evidence *extra_scope = pg_prove_context_extension(&typing, a_scope, extra, prefix_universe);
	const struct pg_evidence *index_scope = pg_prove_context_extension(&typing, a_scope, index, prefix_universe);
	const struct pg_evidence *extra_value = pg_prove_variable(&typing, extra_scope, extra);
	const struct pg_evidence *fixed_prefix = pg_prove_substitution_pair(&typing,
		pg_prove_substitution_projection(&typing, a_scope, extra_scope), index_scope, extra_value);
	const struct pg_evidence *fextra = pg_prove_return_type(&typing, &classifiers, extra_value);
	pattern_type = pg_prove_pattern_type(&typing, &classifiers, a_scope, fixed_prefix, fextra);
	assert(pattern_type && pg_evidence_subject(pattern_type)->core == pg_return_type(&classifiers, pg_reference(graph, index)));
	const struct pg_evidence *swapped_images[] = {extra_value, pg_prove_variable(&typing, extra_scope, a)};
	const struct pg_evidence *swapped = pg_prove_substitution(&typing, index_scope, extra_scope, 2, swapped_images);
	assert(swapped);
	assert(!pg_prove_pattern_type(&typing, &classifiers, a_scope, swapped, fextra));
	pattern_type = pg_prove_pattern_type(&typing, &classifiers, empty, swapped, fextra);
	assert(pattern_type && pg_evidence_subject(pattern_type)->core == pg_return_type(&classifiers, pg_reference(graph, a)));
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
	assert(pg_prove_substitution_compose(&typing,
		pg_prove_substitution_projection(&typing, source, source), sigma) == sigma);
	assert(pg_prove_substitution_compose(&typing, sigma,
		pg_prove_substitution_projection(&typing, destination, destination)) == sigma);
	for (size_t i = 0; i < 2; ++i)
		assert(pg_evidence_premise(composite, i + 2) == second_images[i]);
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
	/* A descriptive lift is checked by the same rule as an explicit lift. */
	const struct pg_object *fresh = pg_binder(graph);
	const struct pg_context_map *raw_lift = pg_context_map_lift(&typing,
		pg_evidence_context_map(sigma), pg_evidence_context(source_extension), fresh);
	assert(raw_lift);
	const struct pg_evidence *checked = pg_prove_context_map(&typing, raw_lift);
	assert(checked && pg_evidence_context_map(checked) == raw_lift);
	assert(pg_prove_substitution_lift(&typing, sigma, source_extension, fresh) == checked);
	const struct pg_occurrence *free_image = raw_lift->images[1];
	assert(pg_occurrence_unproject(&typing, free_image, raw_lift->destination) == free_image);
	assert(pg_occurrence_unproject(&typing, free_image, pg_evidence_context(destination)) ==
		pg_evidence_subject(destination_y));
	assert(!pg_occurrence_unproject(&typing, raw_lift->images[2], pg_evidence_context(destination)));
	const struct pg_occurrence *wrong_variable = pg_occurrence_boundary(&typing,
		free_image, PG_JUDGEMENT_VALUE, pg_evidence_subject(universe)->core);
	assert(wrong_variable && !pg_occurrence_unproject(&typing, wrong_variable, pg_evidence_context(destination)));
	const struct pg_context *wrong_scope = pg_context_bind(&typing, pg_evidence_context(destination),
		pg_binder(graph), pg_evidence_subject(universe)->core, PG_JUDGEMENT_VALUE);
	const struct pg_context_map *weakening = pg_context_map_projection(&typing, pg_evidence_context(destination), wrong_scope);
	const struct pg_occurrence *wrong_images[] = {
		pg_occurrence_projection(&typing, weakening, pg_evidence_subject(destination_b)),
		pg_occurrence_projection(&typing, weakening, pg_evidence_subject(destination_y)),
		pg_occurrence(&typing, PG_JUDGEMENT_VALUE, wrong_scope,
			pg_reference(graph, wrong_scope->binder), wrong_scope->declared_type, NULL, 0, NULL)
	};
	const struct pg_context_map *wrong_lift = pg_context_map(&typing,
		pg_evidence_context(source_extension), wrong_scope, 3, wrong_images);
	assert(wrong_lift && !pg_prove_context_map(&typing, wrong_lift));
	size_t lifted_proofs = typing.proofs.count, lifted_subjects = typing.occurrences.count;
	assert(pg_prove_context_map(&typing, raw_lift) == checked);
	assert(typing.proofs.count == lifted_proofs && typing.occurrences.count == lifted_subjects);
	const struct pg_evidence *a_extended = pg_prove_variable(&typing, source_extension, a);
	const struct pg_evidence *fa_extended = pg_prove_return_type(&typing, &classifiers, a_extended);
	const struct pg_evidence *paired = pg_prove_substitution_pair(&typing, sigma, source_extension, destination_y);
	assert(paired);
	const struct pg_evidence *fa_instance = pg_prove_reindex(&typing, paired, fa_extended);
	assert(fa_instance && pg_evidence_judgement(fa_instance) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_subject(fa_instance)->core == pg_return_type(&classifiers, pg_reference(graph, b)));
	const struct pg_evidence *ufa_extended = pg_prove_thunk_type(&typing, &classifiers, fa_extended);
	const struct pg_evidence *ufa_instance = pg_prove_reindex(&typing, paired, ufa_extended);
	assert(ufa_instance && pg_evidence_judgement(ufa_instance) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(ufa_instance)->core == pg_thunk_type(&classifiers, pg_evidence_subject(fa_instance)->core));
	const struct pg_evidence *source_pi = pg_prove_pi(&typing, &classifiers, source_extension, fa_extended);
	assert(pg_occurrence_scoped_input(pg_evidence_subject(source_pi), 1) == pg_evidence_subject(fa_extended));
	assert(pg_evidence_subject(source_pi)->operands[0] == pg_evidence_subject(pg_prove_value_type(&typing, a_in_source)));
	const struct pg_evidence *source_lambda = pg_prove_lambda(&typing, source_pi,
		pg_prove_return(&typing, &classifiers, source_p));
	assert(source_lambda);
	const struct pg_evidence *same_images[] = {a_in_source, source_x};
	const struct pg_evidence *same_scope = pg_prove_substitution(&typing, source, source, 2, same_images);
	assert(same_scope);
	size_t unchanged_terms = graph->terms.count;
	const struct pg_evidence *unchanged_lambda = pg_prove_reindex(&typing, same_scope, source_lambda);
	assert(unchanged_lambda && pg_evidence_rule(unchanged_lambda) == PG_REINDEX);
	assert(pg_evidence_subject(unchanged_lambda) == pg_evidence_subject(source_lambda));
	assert(pg_evidence_classifier(unchanged_lambda) == pg_evidence_classifier(source_lambda));
	assert(pg_evidence_premise(unchanged_lambda, 0) == same_scope);
	assert(pg_evidence_premise(unchanged_lambda, 1) == source_lambda);
	assert(graph->terms.count == unchanged_terms);
	pending_proofs = typing.proofs.count;
	assert(pg_reindex_init(&split, &typing, sigma, source_lambda) == 0);
	assert(pg_reindex_advance(&split, 1) == PG_REINDEX_PENDING);
	assert(!pg_reindex_result(&split));
	pg_reindex_destroy(&split);
	assert(typing.proofs.count == pending_proofs);
	assert(pg_reindex_init(&split, &typing, sigma, source_lambda) == 0);
	const struct pg_evidence *moved_lambda = pg_prove_reindex(&typing, sigma, source_lambda);
	while (pg_reindex_advance(&split, 1) == PG_REINDEX_PENDING) assert(!pg_reindex_result(&split));
	assert(pg_reindex_result(&split) == moved_lambda);
	assert(pg_evidence_subject(moved_lambda)->annotation);
	assert(pg_evidence_subject(moved_lambda)->annotation == pg_reference(graph, b));
	pg_reindex_destroy(&split);
	const struct pg_evidence *moved_pi = pg_prove_reindex(&typing, sigma, source_pi);
	assert(!pg_occurrence_scoped_input(pg_evidence_subject(moved_pi), 1));
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
	const struct pg_evidence *alternate_lift = pg_prove_substitution_lift(&typing, alternate, function_context, g);
	assert(alternate_lift && alternate_lift != function_lift);
	assert(pg_evidence_context_map(alternate_lift) == pg_evidence_context_map(function_lift));
	assert(pg_evidence_premise(alternate_lift, 2) == pg_prove_projection(&typing,
		pg_evidence_premise(alternate_lift, 1), alternate_b));
	assert(pg_evidence_premise(alternate_lift, 2) != pg_evidence_premise(function_lift, 2));
	reconstruct_derivation(&typing, &classifiers, alternate_lift);
	const struct pg_evidence *fresh_alternate_lift = pg_prove_substitution_lift(&typing,
		alternate, function_context, pg_binder(graph));
	assert(fresh_alternate_lift && pg_prove_context_map(&typing,
		pg_evidence_context_map(fresh_alternate_lift)) == fresh_alternate_lift);
	/* A fresh destination must not first publish an unrequested alternative
	 * assembled from a different prefix proof. */
	assert(pg_evidence_premise(fresh_alternate_lift, 2) == pg_prove_projection(&typing,
		pg_evidence_premise(fresh_alternate_lift, 1), alternate_b));
	/* Extending an accepted map must not freshen Pi binders in its prefix. */
	const struct pg_evidence *function_destination = pg_evidence_premise(function_lift, 1);
	const struct pg_evidence *extended_universe = pg_prove_projection(&typing, function_context, universe);
	const struct pg_evidence *last_context = pg_prove_context_extension(&typing,
		function_context, pg_binder(graph), extended_universe);
	const struct pg_evidence *last_image = pg_prove_variable(&typing, function_destination, b);
	const struct pg_evidence *expected_last = pg_prove_reindex(&typing, function_lift, extended_universe);
	assert(last_context && last_image && expected_last);
	size_t prefix_terms = graph->terms.count, prefix_proofs = typing.proofs.count;
	const struct pg_evidence *last_pair = pg_prove_substitution_pair(&typing, function_lift, last_context, last_image);
	assert(last_pair);
	assert(graph->terms.count == prefix_terms);
	assert(typing.proofs.count == prefix_proofs + 1);
	const struct pg_evidence *last_images[] = {
		pg_evidence_premise(function_lift, 2), pg_evidence_premise(function_lift, 3),
		pg_evidence_premise(function_lift, 4), last_image
	};
	assert(pg_prove_substitution(&typing, last_context, function_destination, 4, last_images) == last_pair);
	assert(pg_evidence_premise(last_pair, 4) == pg_evidence_premise(function_lift, 4));
	size_t term_count = graph->terms.count;
	size_t proof_count = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_prove_substitution_pair(&typing, type_pair, source, destination_y) == sigma);
		assert(pg_prove_substitution_pair(&typing, sigma, source_extension, destination_y) == paired);
		assert(pg_prove_reindex(&typing, sigma, source_pi) == moved_pi);
		assert(pg_prove_reindex(&typing, sigma, source_upi) == moved_upi);
		assert(pg_prove_substitution_lift(&typing, sigma, function_context, g) == function_lift);
	}
	assert(graph->terms.count == term_count);
	assert(typing.proofs.count == proof_count);
	const struct pg_evidence *records[] = {sigma, paired, moved_pi, moved_upi, content, codomain, function_lift, last_pair};
	for (size_t i = 0; i < sizeof(records) / sizeof(*records); ++i)
		reconstruct_derivation(&typing, &classifiers, records[i]);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("typed substitution: dependent declarations, simultaneous images and shared premise DAG passed");
}

static void family_instance_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u1 = pg_prove_universe(&typing, &classifiers, empty, 1);
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *indices = pg_prove_context_extension(&typing, empty, pg_binder(graph), u0);
	const struct pg_object *f = pg_binder(graph), *g = pg_binder(graph);
	const struct pg_evidence *family_scope = pg_prove_family_context_extension(&typing, empty, f,
		indices, pg_prove_universe(&typing, &classifiers, indices, 0));
	assert(family_scope && pg_evidence_context(family_scope)->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	const struct pg_evidence *family_variable = pg_prove_variable(&typing, family_scope, f);
	assert(pg_evidence_judgement(family_variable) == PG_JUDGEMENT_TYPE_FAMILY);
	const struct pg_evidence *family_pi = pg_prove_pi(&typing, &classifiers, family_scope,
		pg_prove_return_type(&typing, &classifiers, pg_prove_universe(&typing, &classifiers, family_scope, 0)));
	assert(family_pi && pg_evidence_subject(family_pi)->operands[0] == pg_evidence_subject(family_variable));
	const struct pg_context_map *empty_map = pg_context_map_projection(&typing, NULL, NULL);
	size_t evidence_before_lift = typing.proofs.count;
	const struct pg_context_map *lift = pg_context_map_lift(&typing, empty_map, pg_evidence_context(family_scope), g);
	assert(lift && lift->destination->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	assert(lift->images[0]->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	assert(lift->images[0]->core == pg_reference(graph, g));
	assert(pg_alpha_equal(lift->destination->declared_type, pg_evidence_context(family_scope)->declared_type) == 1);
	assert(typing.proofs.count == evidence_before_lift);
	const struct pg_evidence *checked_lift = pg_prove_substitution_lift(&typing,
		pg_prove_substitution_projection(&typing, empty, empty), family_scope, g);
	/* Both layers use one signature telescope, including its binder pointers. */
	assert(checked_lift && pg_evidence_context(checked_lift)->binder == g);
	assert(pg_evidence_context(checked_lift)->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	assert(pg_alpha_equal(pg_evidence_context(checked_lift)->declared_type, lift->destination->declared_type) == 1);
	assert(pg_evidence_context_map(checked_lift) == lift);
	assert(pg_evidence_context(family_scope)->indices == pg_evidence_context(indices));
	const struct pg_evidence *outer = pg_prove_family_context_extension(&typing, empty, pg_binder(graph),
		family_scope, pg_prove_universe(&typing, &classifiers, family_scope, 0));
	const struct pg_context_map *prefix_map = pg_context_map_projection(&typing, NULL, pg_evidence_context(indices));
	const struct pg_object *outer_binder = pg_binder(graph);
	evidence_before_lift = typing.proofs.count;
	struct pg_context_lift *nested_work = pg_context_lift_request(&typing, prefix_map, pg_evidence_context(outer), outer_binder);
	size_t before_steps = graph->terms.count;
	assert(nested_work && pg_context_lift_advance(nested_work, 0) == PG_SUBSTITUTION_PENDING);
	assert(graph->terms.count == before_steps && !pg_context_lift_steps(nested_work));
	while (pg_context_lift_advance(nested_work, 1) == PG_SUBSTITUTION_PENDING)
		assert(pg_context_lift_steps(nested_work) < 1000);
	const struct pg_context_map *nested_lift = pg_context_lift_result(nested_work);
	assert(nested_lift && typing.proofs.count == evidence_before_lift);
	const struct pg_context *nested = nested_lift->destination->indices;
	assert(nested && nested->indices && nested->indices->parent == pg_evidence_context(indices));
	assert(nested->indices->binder != pg_evidence_context(indices)->binder);
	const struct pg_evidence *nested_checked = pg_prove_context_map(&typing, nested_lift);
	assert(nested_checked && pg_evidence_context_map(nested_checked) == nested_lift);
	size_t terms_after_lift = graph->terms.count, proofs_after_lift = typing.proofs.count;
	uint64_t completed_steps = pg_context_lift_steps(nested_work);
	for (size_t i = 0; i < 10; ++i) {
		assert(pg_context_map_lift(&typing, prefix_map, pg_evidence_context(outer), outer_binder) == nested_lift);
		assert(pg_prove_context_map(&typing, nested_lift) == nested_checked);
		assert(pg_context_lift_request(&typing, prefix_map, pg_evidence_context(outer), outer_binder) == nested_work);
		assert(pg_context_lift_steps(nested_work) == completed_steps);
	}
	assert(graph->terms.count == terms_after_lift && typing.proofs.count == proofs_after_lift);
	reconstruct_derivation(&typing, &classifiers, nested_checked);
	struct pg_occurrence_input *family_body = pg_occurrence_input_mapped_request(&typing,
		pg_evidence_subject(family_pi), 1, prefix_map);
	evidence_before_lift = typing.proofs.count;
	while (pg_occurrence_input_advance(family_body, 1) == PG_INPUT_PENDING) {}
	const struct pg_occurrence *body_input = pg_occurrence_input_result(family_body);
	assert(body_input && body_input->context->parent == pg_evidence_context(indices));
	assert(typing.proofs.count == evidence_before_lift);
	assert(pg_prove_structural_subject(&typing, body_input));
	struct pg_context invalid = *pg_evidence_context(family_scope);
	invalid.declared_type = pg_pi(graph, pg_evidence_subject(u0)->core, pg_binder(graph), pg_evidence_subject(u0)->core);
	const struct pg_context *wrong_signature = pg_context_intern(&typing, &invalid);
	assert(wrong_signature && !pg_context_lift_request(&typing, empty_map, wrong_signature, pg_binder(graph)));
	invalid.judgement = PG_JUDGEMENT_VALUE;
	assert(!pg_context_intern(&typing, &invalid));
	/* Every signature-local binder collides with the destination. The map
	 * checker must follow nested signature actions without another lift walk. */
	const struct pg_evidence *deep = family_scope, *ambient = empty;
	for (size_t i = 0; i < 64; ++i) {
		ambient = pg_prove_context_extension(&typing, ambient, pg_evidence_context(deep)->binder,
			pg_prove_universe(&typing, &classifiers, ambient, 0));
		deep = pg_prove_family_context_extension(&typing, empty, pg_binder(graph), deep,
			pg_prove_universe(&typing, &classifiers, deep, 0));
		assert(deep && ambient);
	}
	const struct pg_context_map *deep_base = pg_context_map_projection(&typing, NULL, pg_evidence_context(ambient));
	const struct pg_context_map *deep_map = pg_context_map_lift(&typing, deep_base, pg_evidence_context(deep), pg_binder(graph));
	const struct pg_evidence *deep_checked = pg_prove_context_map(&typing, deep_map);
	assert(deep_checked && pg_evidence_context_map(deep_checked) == deep_map);
	size_t checked_count = typing.proofs.count;
	assert(pg_prove_context_map(&typing, deep_map) == deep_checked && typing.proofs.count == checked_count);
	const struct pg_evidence *ambient_identity = pg_prove_substitution_projection(&typing, ambient, ambient);
	const struct pg_object *extra_binder = pg_binder(graph);
	const struct pg_evidence *extra_scope = pg_prove_context_extension(&typing, ambient, extra_binder,
		pg_prove_universe(&typing, &classifiers, ambient, 0));
	const struct pg_context_map *extra_map = pg_context_map_lift(&typing,
		pg_evidence_context_map(ambient_identity), pg_evidence_context(extra_scope), extra_binder);
	checked_count = typing.proofs.count;
	const struct pg_evidence *extra_checked = pg_prove_context_map(&typing, extra_map);
	assert(extra_checked && extra_checked == pg_prove_substitution_lift(&typing,
		ambient_identity, extra_scope, extra_binder));
	/* Structural lookup and explicit lifting use the same checked prefix,
	 * not competing variable/projection proofs for every prefix image. */
	assert(typing.proofs.count - checked_count <= 64 + 4);
	/* An already checked destination needs only checked images, not a proof
	 * of their strengthened prefix or a speculative signature lifting. */
	const struct pg_evidence *plain_source = pg_prove_context_extension(&typing, indices, pg_binder(graph),
		pg_prove_universe(&typing, &classifiers, indices, 0));
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph);
	const struct pg_evidence *plain_prefix = pg_prove_context_extension(&typing, empty, a, u0);
	const struct pg_evidence *plain_destination = pg_prove_context_extension(&typing, plain_prefix, b,
		pg_prove_universe(&typing, &classifiers, plain_prefix, 0));
	const struct pg_occurrence *plain_images[] = {
		pg_evidence_subject(pg_prove_variable(&typing, plain_destination, a)),
		pg_evidence_subject(pg_prove_variable(&typing, plain_destination, b))
	};
	const struct pg_context_map *plain_map = pg_context_map(&typing,
		pg_evidence_context(plain_source), pg_evidence_context(plain_destination), 2, plain_images);
	size_t lift_count = typing.context_lifts.count;
	assert(pg_prove_context_map(&typing, plain_map));
	assert(typing.context_lifts.count == lift_count);
	const struct pg_evidence *value = pg_prove_type_value(&typing, u0);
	const struct pg_object *left = pg_binder(graph), *right = pg_binder(graph);
	const struct pg_evidence *left_scope = pg_prove_context_extension(&typing, empty, left, u1);
	const struct pg_evidence *boundary = pg_prove_context_extension(&typing, left_scope, right,
		pg_prove_projection(&typing, left_scope, u1));
	const struct pg_evidence *families[] = {
		pg_prove_value_type(&typing, pg_prove_variable(&typing, boundary, left)),
		pg_prove_value_type(&typing, pg_prove_variable(&typing, boundary, right))
	};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, empty, empty, 0, NULL);
	sigma = pg_prove_substitution_pair(&typing, sigma, left_scope, value);
	sigma = pg_prove_substitution_pair(&typing, sigma, boundary, value);
	assert(sigma);
	const struct pg_evidence *instances[2];
	for (size_t i = 0; i < 2; ++i) {
		instances[i] = pg_prove_reindex(&typing, sigma, families[i]);
		assert(instances[i]);
		assert(pg_evidence_judgement(instances[i]) == PG_JUDGEMENT_VALUE_TYPE);
		assert(pg_evidence_subject(instances[i])->core == pg_evidence_subject(u0)->core);
		assert(pg_evidence_premise(instances[i], 0) == sigma);
		assert(pg_evidence_premise(instances[i], 1) == families[i]);
	}
	/* Equal instances do not erase which family was selected. These are ordinary
	 * formations, not a certificate that either family is an Identity action. */
	assert(instances[0] != instances[1]);
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *target = pg_prove_context_extension(&typing, empty, z, u0);
	const struct pg_evidence *tau = pg_prove_substitution(&typing, empty, target, 0, NULL);
	const struct pg_evidence *composite = pg_prove_substitution_compose(&typing, sigma, tau);
	const struct pg_evidence *family = pg_prove_projection(&typing, boundary, u1);
	const struct pg_evidence *term = pg_prove_variable(&typing, boundary, left);
	const struct pg_evidence *action = pg_prove_family_action(&typing, family, term, sigma, sigma, 0, NULL);
	reconstruct_derivation(&typing, &classifiers, action);
	reconstruct_derivation(&typing, &classifiers, pg_evidence_premise(action, 0));
	const struct pg_evidence *u2 = pg_prove_universe(&typing, &classifiers, empty, 2);
	const struct pg_evidence *reflexivity = pg_prove_reflexivity(&typing, u2, pg_prove_type_value(&typing, u1));
	reconstruct_derivation(&typing, &classifiers, reflexivity);
	reconstruct_derivation(&typing, &classifiers, pg_evidence_premise(reflexivity, 0));
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *transport = pg_prove_identity_transport(&typing, &classifiers, reflexivity, value, side);
		const struct pg_evidence *lift = pg_prove_identity_lift(&typing, &classifiers, reflexivity, value, side);
		reconstruct_derivation(&typing, &classifiers, transport);
		reconstruct_derivation(&typing, &classifiers, pg_evidence_premise(transport, 0));
		reconstruct_derivation(&typing, &classifiers, lift);
		reconstruct_derivation(&typing, &classifiers, pg_evidence_premise(lift, 0));
		struct pg_derivation_parameters parameters;
		assert(pg_derivation_parameters(transport, &parameters) == 0);
		const struct pg_evidence *wrong_target = pg_prove_identity_endpoint_type(&typing, &classifiers,
			reflexivity, side == PG_IDENTITY_RIGHT ? PG_IDENTITY_LEFT_TYPE : PG_IDENTITY_RIGHT_TYPE);
		const struct pg_evidence *wrong[] = {wrong_target, reflexivity, value};
		/* The endpoint types coincide here, but the directional premise does not. */
		assert(!pg_prove_derivation(&typing, &classifiers, PG_IDENTITY_TRANSPORT, &parameters, 3, wrong));
	}
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *direct = pg_prove_reindex(&typing, composite, families[i]);
		const struct pg_evidence *iterated = pg_prove_reindex(&typing, tau, instances[i]);
		assert(direct && iterated);
		assert(pg_evidence_subject(direct)->core == pg_evidence_subject(iterated)->core);
		assert(pg_evidence_classifier(direct) == pg_evidence_classifier(iterated));
		assert(pg_evidence_premise(iterated, 1) == instances[i]);
	}
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("family instances: chosen formation survives equal endpoints and iterated context action");
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
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, body_context, codomain);
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
	const struct pg_evidence *total = pg_prove_return_contract(&typing, &classifiers, PG_TOTALITY_TOTAL, source_x);
	const struct pg_evidence *pure_value = pg_prove_total_pure_value(&typing, total);
	const struct pg_evidence *direct_value = pg_prove_reindex(&typing, direct, pure_value);
	const struct pg_evidence *twice_value = pg_prove_reindex(&typing, vertex_substitution,
		pg_prove_reindex(&typing, edge_substitution, pure_value));
	const struct pg_evidence *rebuilt_value = pg_prove_total_pure_value(&typing,
		pg_prove_reindex(&typing, direct, total));
	assert(direct_value && twice_value && rebuilt_value);
	assert(pg_alpha_equal(pg_evidence_subject(direct_value)->core, pg_evidence_subject(twice_value)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_subject(direct_value)->core, pg_evidence_subject(rebuilt_value)->core) == 1);
	assert(pg_evidence_classifier(direct_value) == pg_evidence_classifier(rebuilt_value));
	pg_computation_eval_init(&machine, graph, pg_evidence_subject(direct_value)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == pg_evidence_subject(once)->core);
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

static const struct pg_term *request_whnf(struct pg_graph *graph,
	const struct pg_term *term, uint64_t chunk)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, term);
	while (pg_eval_advance(&machine, chunk) == PG_EVAL_PENDING) assert(machine.steps < 10000);
	assert(machine.status == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	assert(result);
	pg_eval_destroy(&machine);
	return result;
}

static void fold_association_test(struct pg_graph *graph)
{
	static const struct pg_object_class operation_class = {"association-operation"};
	static const struct pg_object first = {PG_SEMANTIC_OBJECT, &operation_class};
	static const struct pg_object second = {PG_SEMANTIC_OBJECT, &operation_class};
	const struct pg_object *m = pg_binder(graph), *x = pg_binder(graph), *h = pg_binder(graph);
	const struct pg_term *vm = pg_reference(graph, m), *vx = pg_reference(graph, x);
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_term *answer = pg_reference(graph, pg_binder(graph));
	const struct pg_term *unit = pg_lambda(graph, x, pg_application(graph, ret, vx));
	const struct pg_term *k = pg_lambda(graph, x, pg_computation_request(graph, &second, vx, unit));
	const struct pg_term *last = pg_lambda(graph, x, pg_application(graph, ret, answer));
	const struct pg_term *left = pg_computation_fold(graph, pg_computation_fold(graph, vm, k, 0, NULL),
		pg_reference(graph, h), 0, NULL);
	const struct pg_term *composed = pg_lambda(graph, x,
		pg_computation_fold(graph, pg_application(graph, k, vx), last, 0, NULL));
	const struct pg_term *right = pg_computation_fold(graph, vm, composed, 0, NULL);
	left = pg_application(graph, pg_lambda(graph, h, left), last);
	struct pg_whnf_work work;
	struct pg_conversion comparison;
	assert(!pg_whnf_work_init(&work, graph));
	assert(!pg_conversion_init(&comparison, &work, left, right));
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 8) {
		const struct pg_term *normal = request_whnf(graph, left, chunk);
		assert(pg_alpha_equal(normal, request_whnf(graph, right, chunk)) == 1);
		const struct pg_term *request = pg_computation_request(graph, &first, answer, unit);
		const struct pg_term *applied = pg_application(graph, pg_lambda(graph, m, left), request);
		const struct pg_term *payload, *resume;
		const struct pg_object *label;
		const struct pg_term *result = request_whnf(graph, applied, chunk);
		assert(pg_computation_request_view(result, &label, &payload, &resume));
		assert(label == &first && payload == answer);
		result = request_whnf(graph, pg_application(graph, resume, answer), chunk);
		assert(pg_computation_request_view(result, &label, &payload, &resume));
		assert(label == &second && payload == answer);
		result = request_whnf(graph, pg_application(graph, resume, answer), chunk);
		assert(result == pg_application(graph, ret, answer));
	}
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *quoted_x = pg_application(graph, pg_reference(graph, &pg_thunk_operation), vx);
	const struct pg_term *pure_k = pg_lambda(graph, x, pg_application(graph, ret, quoted_x));
	const struct pg_term *function_last = pg_lambda(graph, x,
		pg_lambda(graph, z, pg_application(graph, ret, vx)));
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	const struct pg_term *applied_left = pg_application(graph, pg_computation_fold(graph,
		pg_computation_fold(graph, vm, pure_k, 0, NULL), function_last, 0, NULL), omega);
	const struct pg_term *applied_right = pg_application(graph, pg_computation_fold(graph, vm,
		pg_lambda(graph, x, pg_computation_fold(graph,
			pg_application(graph, pure_k, vx), function_last, 0, NULL)), 0, NULL), omega);
	assert(!pg_conversion_init(&comparison, &work, applied_left, applied_right));
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	const struct pg_term *closed = pg_application(graph, pg_lambda(graph, m, applied_left),
		pg_application(graph, ret, answer));
	assert(request_whnf(graph, closed, 1) == pg_application(graph, ret,
		pg_application(graph, pg_reference(graph, &pg_thunk_operation), answer)));
	/* Result extraction is not a license to discard an effectful input:
	 * q(bind(request, const(return a))) differs from q(const(return a)(q request)). */
	const struct pg_term *request = pg_computation_request(graph, &first, answer, unit);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *observed = pg_computation_fold(graph,
		pg_computation_fold(graph, request, last, 0, NULL), identity, 0, NULL);
	const struct pg_term *discarded = pg_computation_fold(graph,
		pg_application(graph, last, pg_computation_fold(graph, request, identity, 0, NULL)), identity, 0, NULL);
	assert(!pg_conversion_init(&comparison, &work, observed, discarded));
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&work);
	puts("fold association: neutral inputs, captured continuations, operation order and extraction boundary passed");
}

static void request_forwarding_test(struct pg_graph *graph)
{
	static const struct pg_object_class operation_class = {"test-operation"};
	static const struct pg_object first = {PG_SEMANTIC_OBJECT, &operation_class};
	static const struct pg_object second = {PG_SEMANTIC_OBJECT, &operation_class};
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *r = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_term *fold = pg_reference(graph, &pg_fold_operation);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	const struct pg_term *inner = pg_computation_request(graph, &second, vx,
		pg_lambda(graph, y, pg_application(graph, ret, vx)));
	const struct pg_term *resume = pg_lambda(graph, x, inner);
	const struct pg_term *request = pg_computation_request(graph, &first, omega, resume);
	assert(request && request == pg_computation_request(graph, &first, omega, resume));
	assert(!strcmp(pg_computation_name(&pg_request_operation), "kernel/request/v1"));
	assert(pg_computation_resolve("kernel/request/v1") == &pg_request_operation);
	const struct pg_object *label = NULL;
	const struct pg_term *payload = NULL, *continuation = NULL;
	assert(pg_computation_request_view(request, &label, &payload, &continuation));
	assert(label == &first && payload == omega && continuation == resume);
	assert(!pg_computation_request(graph, x, vx, ret));
	assert(!pg_computation_request_view(pg_application(graph, request, vx), &label, &payload, &continuation));
	assert(label == &first && payload == omega && continuation == resume);
	assert(request_whnf(graph, request, 1) == request);
	/* Captured R must survive forwarding; neither the payload nor R is run
	 * while the request is exposed. The two labels share a class, not identity. */
	const struct pg_term *return_r = pg_lambda(graph, y, pg_application(graph, ret, pg_reference(graph, r)));
	const struct pg_term *body = pg_application(graph, pg_application(graph, fold, request), return_r);
	const struct pg_term *source = pg_application(graph, pg_lambda(graph, r, body), vy);
	const struct pg_term *expected = request_whnf(graph, source, 10000);
	assert(pg_alpha_equal(expected, request_whnf(graph, source, 1)) == 1);
	assert(pg_computation_request_view(expected, &label, &payload, &continuation));
	assert(label == &first && pg_alpha_equal(payload, omega) == 1);
	const struct pg_term *next = request_whnf(graph, pg_application(graph, continuation, vx), 1);
	assert(pg_computation_request_view(next, &label, &payload, &continuation));
	assert(label == &second && payload == vx);
	const struct pg_term *result = request_whnf(graph, pg_application(graph, continuation, vy), 1);
	assert(result == pg_application(graph, ret, vy));
	const struct pg_term *diverging_return = pg_application(graph, pg_application(graph, fold, request), omega);
	assert(pg_computation_request_view(request_whnf(graph, diverging_return, 1), &label, &payload, &continuation));
	assert(label == &first && pg_alpha_equal(payload, omega) == 1);
	struct pg_eval machine;
	pg_eval_init(&machine, body);
	assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == body);
	pg_eval_destroy(&machine);
	pg_computation_eval_init(&machine, graph, source);
	while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) {
		assert(machine.steps < 10000);
		assert(pg_alpha_equal(expected, request_whnf(graph, pg_eval_readback(&machine, graph), 10000)) == 1);
	}
	assert(machine.status == PG_EVAL_WHNF);
	pg_eval_destroy(&machine);
	puts("requests: inert pointer labels, zero-clause forwarding, captured continuations and split resume passed");
	/* Clauses receive a suspended deep continuation; label order is arbitrary. */
	const struct pg_term *force = pg_reference(graph, &pg_force_operation);
	const struct pg_term *invoke = pg_application(graph, force, pg_reference(graph, r));
	const struct pg_term *first_clause = pg_lambda(graph, x, pg_lambda(graph, r,
		pg_application(graph, invoke, vy)));
	const struct pg_term *second_clause = pg_lambda(graph, x, pg_lambda(graph, r,
		pg_application(graph, invoke, vx)));
	struct pg_operation_clause clauses[] = {{&second, second_clause}, {&first, first_clause}};
	const struct pg_term *handled = pg_computation_fold(graph, request, ret, 2, clauses);
	assert(handled && handled == pg_computation_fold(graph, request, ret, 2, clauses));
	assert(pg_computation_fold(graph, request, return_r, 0, NULL) == body);
	assert(request_whnf(graph, handled, 1) == pg_application(graph, ret, vy));
	assert(request_whnf(graph, handled, 10000) == pg_application(graph, ret, vy));
	size_t fold_demands = 0;
	const struct pg_eval_continuation *fold_continuation = pg_computation_continuation_resolve("computation/fold_answer/v2");
	pg_computation_eval_init(&machine, graph, handled);
	while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) {
		for (const struct pg_eval_frame *frame = machine.frames; frame; frame = frame->parent) {
			if (frame->continuation != fold_continuation) continue;
			assert(!frame->state && frame->caller.term->kind == PG_REFERENCE);
			++fold_demands;
		}
		assert(machine.steps < 10000);
		assert(request_whnf(graph, pg_eval_readback(&machine, graph), 10000) == pg_application(graph, ret, vy));
	}
	assert(machine.status == PG_EVAL_WHNF);
	assert(fold_demands);
	pg_eval_destroy(&machine);
	size_t owners = graph->objects.count;
	struct pg_operation_clause duplicate[] = {{&first, first_clause}, {&first, second_clause}};
	assert(!pg_computation_fold(graph, request, ret, 2, duplicate));
	assert(graph->objects.count == owners);
	/* A simultaneous swap must not recapture a request emitted by its clause. */
	clauses[0].body = pg_lambda(graph, x, pg_lambda(graph, r,
		pg_computation_request(graph, &first, vx, ret)));
	clauses[1].body = pg_lambda(graph, x, pg_lambda(graph, r,
		pg_computation_request(graph, &second, vx, ret)));
	const struct pg_object *inputs[] = {&first, &second};
	const struct pg_object *outputs[] = {&second, &first};
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *input = pg_computation_request(graph, inputs[i], vy, ret);
		const struct pg_term *swapped = request_whnf(graph, pg_computation_fold(graph, input, ret, 2, clauses), 1);
		assert(pg_computation_request_view(swapped, &label, &payload, &continuation));
		assert(label == outputs[i] && payload == vy);
	}
	assert(graph->objects.count == owners);
	static const struct pg_object third = {PG_SEMANTIC_OBJECT, &operation_class};
	const struct pg_term *unhandled = pg_computation_request(graph, &third, vx,
		pg_lambda(graph, r, pg_computation_request(graph, &first, vy, ret)));
	const struct pg_term *forwarded = request_whnf(graph, pg_computation_fold(graph, unhandled, ret, 2, clauses), 1);
	assert(pg_computation_request_view(forwarded, &label, &payload, &continuation));
	assert(label == &third && payload == vx);
	forwarded = request_whnf(graph, pg_application(graph, continuation, vy), 1);
	assert(pg_computation_request_view(forwarded, &label, &payload, &continuation));
	assert(label == &second && payload == vy);
	/* Resume the same k twice. Expose both observable requests, not a cached
	 * receipt of an earlier invocation. An unused divergent clause stays inert. */
	const struct pg_term *reply1 = pg_reference(graph, pg_binder(graph));
	const struct pg_term *reply2 = pg_reference(graph, pg_binder(graph));
	const struct pg_term *twice = pg_computation_fold(graph,
		pg_application(graph, invoke, reply1),
		pg_lambda(graph, pg_binder(graph), pg_application(graph, invoke, reply2)), 0, NULL);
	clauses[0].body = omega;
	clauses[1].body = pg_lambda(graph, x, pg_lambda(graph, r, twice));
	const struct pg_term *trace_return = pg_lambda(graph, x, pg_computation_request(graph, &third, vx, ret));
	const struct pg_term *multi = pg_computation_fold(graph,
		pg_computation_request(graph, &first, vx, ret), trace_return, 2, clauses);
	multi = request_whnf(graph, multi, 1);
	assert(pg_computation_request_view(multi, &label, &payload, &continuation));
	assert(label == &third && payload == reply1);
	multi = request_whnf(graph, pg_application(graph, continuation, vx), 1);
	assert(pg_computation_request_view(multi, &label, &payload, &continuation));
	assert(label == &third && payload == reply2);
	assert(request_whnf(graph, pg_application(graph, continuation, vy), 1) == pg_application(graph, ret, vy));
	struct pg_operation_clause many[256];
	for (size_t i = 0; i < 256; ++i) {
		struct pg_object *object = pg_alloc(graph, sizeof(*object));
		assert(object);
		*object = (struct pg_object){PG_SEMANTIC_OBJECT, &operation_class};
		many[i] = (struct pg_operation_clause){object, omega};
	}
	many[255].body = pg_lambda(graph, x, pg_lambda(graph, r, pg_application(graph, ret, vx)));
	const struct pg_term *large = pg_computation_fold(graph,
		pg_computation_request(graph, many[255].label, vy, ret), ret, 256, many);
	pg_computation_eval_init(&machine, graph, large);
	for (;;) {
		size_t before = graph->terms.count;
		enum pg_eval_status status = pg_eval_advance(&machine, 1);
		assert(graph->terms.count - before <= 16);
		assert(machine.steps < 100000);
		if (status != PG_EVAL_PENDING) break;
	}
	assert(machine.status == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == pg_application(graph, ret, vy));
	uint64_t large_steps = machine.steps;
	pg_eval_destroy(&machine);
	pg_computation_eval_init(&machine, graph, large);
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	assert(machine.steps == large_steps);
	pg_eval_destroy(&machine);
	pg_computation_eval_init(&machine, graph, large);
	while (!machine.task) {
		assert(pg_eval_advance(&machine, 1) == PG_EVAL_PENDING);
		assert(machine.steps < large_steps);
	}
	assert(pg_eval_advance(&machine, 16) == PG_EVAL_PENDING);
	assert(machine.task && pg_alpha_equal(pg_eval_readback(&machine, graph), large) == 1);
	pg_eval_destroy(&machine);
	puts("fold clauses: simultaneous swaps, deep resumption, unhandled forwarding and exact layout reuse passed");
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
	assert(pg_eval_readback(&semantic, graph) == vx);
	pg_eval_destroy(&semantic);
	const struct pg_term *diverging = pg_application(graph, pg_application(graph, fold, omega), continuation);
	pg_computation_eval_init(&semantic, graph, diverging);
	assert(pg_eval_advance(&semantic, 100) == PG_EVAL_PENDING);
	pg_eval_destroy(&semantic);
	/* Right-unit removal under a thunk must not run its suspended source. */
	const struct pg_term *eta = pg_application(graph, thunk, pg_application(graph, force, vx));
	const struct pg_term *quoted_fold = pg_application(graph, thunk, diverging);
	const struct pg_term *composite = pg_application(graph, thunk,
		pg_application(graph, pg_application(graph, fold, pg_application(graph, force, vx)), continuation));
	const struct pg_term *inputs[] = {eta, quoted_fold, composite,
		pg_application(graph, pg_lambda(graph, x, composite), quoted_omega)};
	const struct pg_term *outputs[] = {vx, quoted_omega, vx, quoted_omega};
	for (size_t i = 0; i < 4; ++i) {
		pg_computation_eval_init(&semantic, graph, inputs[i]);
		while (pg_eval_advance(&semantic, 1) == PG_EVAL_PENDING) assert(semantic.steps < 100);
		assert(semantic.status == PG_EVAL_WHNF);
		assert(pg_alpha_equal(pg_eval_readback(&semantic, graph), outputs[i]) == 1);
		uint64_t steps = semantic.steps;
		pg_eval_destroy(&semantic);
		pg_computation_eval_init(&semantic, graph, inputs[i]);
		assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF && semantic.steps == steps);
		pg_eval_destroy(&semantic);
	}
	const struct pg_term *eta_bodies[] = {eta, blocked, composite};
	const struct pg_term *left = pg_reference(graph, pg_binder(graph));
	const struct pg_term *right = pg_reference(graph, pg_binder(graph));
	const struct pg_term *path = pg_reference(graph, pg_binder(graph));
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_term *action = pg_identity_apply(graph, pg_lambda(graph, x, eta_bodies[i]), left, right, path);
		pg_computation_eval_init(&semantic, graph, action);
		assert(pg_eval_advance(&semantic, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&semantic, graph) == path);
		pg_eval_destroy(&semantic);
	}
	pg_eval_init(&beta, eta);
	assert(pg_eval_advance(&beta, 100) == PG_EVAL_WHNF && pg_eval_readback(&beta, graph) == eta);
	pg_eval_destroy(&beta);
	/* Neither a constant-return clause nor a divergent callee is the unit. */
	const struct pg_term *other = pg_reference(graph, pg_binder(graph));
	const struct pg_term *callees[] = {pg_lambda(graph, x, pg_application(graph, ret, other)), omega};
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *term = pg_application(graph, pg_application(graph, fold, vx), callees[i]);
		pg_computation_eval_init(&semantic, graph, term);
		assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&semantic, graph) == term);
		pg_eval_destroy(&semantic);
		term = pg_application(graph, thunk, term);
		pg_computation_eval_init(&semantic, graph, term);
		assert(pg_eval_advance(&semantic, 100) == PG_EVAL_WHNF && pg_eval_readback(&semantic, graph) == term);
		pg_eval_destroy(&semantic);
	}
	puts("computation execution: force/thunk, fold, captured environments, neutral demands and split budgets passed");
}

static size_t demand_resumes;
static const struct pg_object_class demand_class = {"test-demand"};
static const struct pg_object demand_operation = {PG_SEMANTIC_OBJECT, &demand_class};

static int demand_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state);
static const struct pg_eval_continuation demand_answer_continuation = {
	"tests/core/demand_answer/v1", demand_answer
};

static int demand_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	assert(state == &demand_operation);
	const struct pg_closure *argument = pg_eval_argument(machine, 63);
	assert(argument && !argument->environment && argument->term == answer);
	++demand_resumes;
	return 1;
}

static int demand_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &demand_operation) return 1;
	return pg_eval_demand(machine, 63, &demand_answer_continuation, &demand_operation);
}

static const struct pg_argument *auxiliary_arguments;
static const struct pg_term *auxiliary_source;

static int auxiliary_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state);
static const struct pg_eval_continuation auxiliary_answer_continuation = {
	"tests/core/auxiliary_answer/v1", auxiliary_answer
};

static int auxiliary_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	assert(state == auxiliary_source);
	assert(machine->arguments == auxiliary_arguments);
	assert(pg_eval_argument(machine, 0)->term == auxiliary_source);
	++demand_resumes;
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 1);
}

static int auxiliary_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &demand_operation) return 1;
	auxiliary_arguments = machine->arguments;
	auxiliary_source = pg_eval_argument(machine, 0)->term;
	return pg_eval_demand_closure(machine, *pg_eval_argument(machine, 0), &auxiliary_answer_continuation, auxiliary_source);
}

static size_t check_demand_prefix(const struct pg_eval_frame *frame)
{
	const struct pg_argument *original = frame->arguments, *copy = frame->first, *last = NULL;
	size_t copied = 0;
	while (copy && copy != frame->cursor) {
		assert(++copied < 64);
		assert(original && copy && original != copy);
		assert(original != frame->target);
		assert(original->value.term == copy->value.term);
		assert(original->value.environment == copy->value.environment);
		last = copy;
		original = original->next;
		copy = copy->next;
	}
	assert(original == frame->cursor && last == frame->last);
	if (copied) {
		assert(frame->answer.done);
		assert(copy == frame->cursor);
	} else assert(!frame->first && !frame->last);
	if (!frame->target) assert(!copied);
	return copied;
}

static void auxiliary_demand_test(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *source = pg_application(graph, pg_lambda(graph, x, vx), vy);
	const struct pg_term *input = pg_application(graph,
		pg_application(graph, pg_reference(graph, &demand_operation), source), vx);
	const struct pg_term *expected = pg_application(graph, vy, vx);
	struct pg_eval full;
	pg_eval_init(&full, input);
	full.output = graph; full.dispatch = auxiliary_dispatch;
	demand_resumes = 0;
	assert(pg_eval_advance(&full, 1000) == PG_EVAL_WHNF && demand_resumes == 1);
	assert(pg_eval_readback(&full, graph) == expected);
	uint64_t steps = full.steps;
	pg_eval_destroy(&full);
	for (uint64_t cut = 0; cut < steps; ++cut) {
		struct pg_eval split;
		pg_eval_init(&split, input);
		split.output = graph; split.dispatch = auxiliary_dispatch;
		demand_resumes = 0;
		assert(pg_eval_advance(&split, cut) == PG_EVAL_PENDING);
		if (split.frames) {
			assert(pg_eval_readback(&split, graph) == input);
			check_demand_prefix(split.frames);
		}
		assert(pg_eval_advance(&split, 1000) == PG_EVAL_WHNF && demand_resumes == 1);
		assert(split.steps == steps && pg_eval_readback(&split, graph) == expected);
		pg_eval_destroy(&split);
	}
	puts("auxiliary demand: unchanged caller arguments, pending readback and every split budget passed");
}

struct deferred_test_work {
	size_t remaining;
	int fail;
	const struct pg_term *answer;
};
static size_t deferred_destroyed, deferred_resumed;

static int deferred_poll(void *opaque)
{
	struct deferred_test_work *work = opaque;
	if (--work->remaining) return 0;
	return work->fail ? -1 : 1;
}

static int deferred_resume(struct pg_eval *machine, void *opaque)
{
	struct deferred_test_work *work = opaque;
	assert(!machine->task);
	++deferred_resumed;
	return pg_eval_enter(machine, (struct pg_closure){work->answer, NULL}, 0);
}

static void deferred_destroy(void *opaque)
{
	assert(opaque);
	++deferred_destroyed;
}

static const struct pg_eval_work_operation deferred_operation = {
	deferred_poll, deferred_resume, deferred_destroy, NULL
};

static void deferred_work_test(struct pg_graph *graph)
{
	const struct pg_term *input = pg_reference(graph, pg_binder(graph));
	const struct pg_term *answer = pg_reference(graph, pg_binder(graph));
	for (size_t cut = 0; cut < 5; ++cut) {
		for (int cancel = 0; cancel < 2; ++cancel) {
			struct pg_eval machine;
			pg_eval_init(&machine, input);
			struct deferred_test_work work = {5, 0, answer};
			deferred_destroyed = deferred_resumed = 0;
			assert(pg_eval_defer(&machine, &deferred_operation, &work) == 0);
			assert(pg_eval_defer(&machine, &deferred_operation, &work) == -1);
			assert(pg_eval_advance(&machine, cut) == PG_EVAL_PENDING);
			assert(work.remaining == 5 - cut && machine.steps == cut);
			assert(pg_eval_readback(&machine, graph) == input);
			assert(!deferred_destroyed && !deferred_resumed);
			if (!cancel) {
				assert(pg_eval_advance(&machine, 10) == PG_EVAL_WHNF);
				assert(machine.steps == 6 && deferred_resumed == 1);
				assert(deferred_destroyed == 1 && pg_eval_readback(&machine, graph) == answer);
			}
			pg_eval_destroy(&machine);
			assert(deferred_destroyed == 1);
		}
	}
	struct pg_eval machine;
	pg_eval_init(&machine, input);
	struct deferred_test_work failure = {1, 1, answer};
	deferred_destroyed = deferred_resumed = 0;
	assert(pg_eval_defer(&machine, &deferred_operation, &failure) == 0);
	assert(pg_eval_advance(&machine, 1) == PG_EVAL_ERROR);
	assert(deferred_destroyed == 1 && !deferred_resumed);
	pg_eval_destroy(&machine);
	assert(deferred_destroyed == 1);
	const struct pg_eval_work_operation incomplete[] = {
		{NULL, deferred_resume, deferred_destroy, NULL},
		{deferred_poll, NULL, deferred_destroy, NULL},
		{deferred_poll, deferred_resume, NULL, NULL}
	};
	pg_eval_init(&machine, input);
	deferred_destroyed = deferred_resumed = 0;
	for (size_t i = 0; i < sizeof(incomplete) / sizeof(*incomplete); ++i)
		assert(pg_eval_defer(&machine, &incomplete[i], &failure) == -1);
	assert(pg_eval_defer(&machine, NULL, &failure) == -1);
	assert(pg_eval_defer(&machine, &deferred_operation, NULL) == -1);
	assert(!machine.task && !machine.steps && failure.remaining == 0);
	pg_eval_destroy(&machine);
	assert(!deferred_destroyed && !deferred_resumed);
	/* Sharing the algorithm must not share invocation progress or answers. */
	struct pg_eval other;
	pg_eval_init(&machine, input);
	pg_eval_init(&other, input);
	struct deferred_test_work first = {1, 0, answer}, second = {3, 0, input};
	assert(pg_eval_defer(&machine, &deferred_operation, &first) == 0);
	assert(pg_eval_defer(&other, &deferred_operation, &second) == 0);
	assert(pg_eval_advance(&other, 1) == PG_EVAL_PENDING);
	assert(second.remaining == 2 && first.remaining == 1);
	assert(pg_eval_advance(&machine, 2) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, graph) == answer && second.remaining == 2);
	pg_eval_destroy(&machine);
	assert(deferred_destroyed == 1 && deferred_resumed == 1);
	assert(pg_eval_advance(&other, 3) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&other, graph) == input && other.steps == 4);
	pg_eval_destroy(&other);
	assert(deferred_destroyed == 2 && deferred_resumed == 2);
	puts("deferred pure work: exact fuel, split resume, pending readback, cancellation and failure cleanup passed");
}

static void demand_budget_test(struct pg_graph *graph)
{
	const size_t depth = 5000;
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *deep = vx, *expected = vy;
	for (size_t i = 0; i < depth; ++i) {
		deep = pg_application(graph, deep, deep);
		expected = pg_application(graph, expected, expected);
	}
	const struct pg_term *quoted = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
		pg_lambda(graph, y, deep));
	const struct pg_term *input = pg_application(graph, pg_lambda(graph, x,
		pg_application(graph, pg_reference(graph, &pg_force_operation), quoted)), vy);
	struct pg_eval split, whole;
	pg_computation_eval_init(&split, graph, input);
	assert(pg_eval_advance(&split, 100) == PG_EVAL_PENDING);
	assert(split.frames && split.head_ready);
	/* Explicit diagnostic readback may discard work, but must preserve meaning. */
	const struct pg_term *residual = pg_eval_readback(&split, graph);
	pg_computation_eval_init(&whole, graph, residual);
	assert(pg_eval_advance(&whole, UINT64_MAX) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&whole, graph);
	assert(result->kind == PG_LAMBDA && result->as.lambda.binder != y);
	assert(result->as.lambda.body == expected);
	pg_eval_destroy(&whole);
	while (split.status == PG_EVAL_PENDING) {
		uint64_t steps = split.steps;
		pg_eval_advance(&split, 7);
		assert(split.steps - steps <= 7 && split.steps < 20 * depth);
	}
	assert(split.status == PG_EVAL_WHNF && split.steps > depth);
	result = pg_eval_readback(&split, graph);
	assert(result->kind == PG_LAMBDA && result->as.lambda.binder != y);
	assert(result->as.lambda.body == expected);
	pg_computation_eval_init(&whole, graph, input);
	assert(pg_eval_advance(&whole, UINT64_MAX) == PG_EVAL_WHNF);
	assert(whole.steps == split.steps);
	assert(pg_alpha_equal(pg_eval_readback(&whole, graph), result) == 1);
	pg_eval_destroy(&whole);
	pg_eval_destroy(&split);
	pg_computation_eval_init(&split, graph, input);
	assert(pg_eval_advance(&split, 100) == PG_EVAL_PENDING);
	pg_eval_destroy(&split); /* Release suspended demand traversal, not just frames. */
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input);
	assert(pg_whnf_advance(job, 100) == PG_EVAL_PENDING);
	assert(!pg_whnf_result(job) && !pg_whnf_certificate(job));
	assert(pg_whnf_advance(job, UINT64_MAX) == PG_EVAL_WHNF);
	assert(pg_alpha_equal(pg_whnf_result(job), result) == 1);
	pg_whnf_work_destroy(&work);
	/* Copying the argument prefix must also suspend without calling resume. */
	input = expected = pg_reference(graph, &demand_operation);
	const struct pg_term *redex = pg_application(graph, pg_lambda(graph, x, vx), vy);
	for (size_t i = 0; i < 65; ++i) {
		input = pg_application(graph, input, i == 63 ? redex : vx);
		expected = pg_application(graph, expected, i == 63 ? vy : vx);
	}
	demand_resumes = 0;
	pg_eval_init(&split, input);
	split.output = graph; split.dispatch = demand_dispatch;
	while (!split.head_ready) {
		assert(pg_eval_advance(&split, 1) == PG_EVAL_PENDING);
		assert(split.steps < 100);
	}
	assert(pg_eval_advance(&split, 32) == PG_EVAL_PENDING && demand_resumes == 0);
	assert(pg_eval_readback(&split, graph) == expected);
	assert(pg_eval_advance(&split, UINT64_MAX) == PG_EVAL_WHNF && demand_resumes == 1);
	assert(pg_eval_readback(&split, graph) == expected);
	uint64_t steps = split.steps;
	assert(pg_eval_advance(&split, 100) == PG_EVAL_WHNF && split.steps == steps && demand_resumes == 1);
	pg_eval_destroy(&split);
	pg_eval_init(&split, input);
	split.output = graph; split.dispatch = demand_dispatch;
	while (!split.head_ready) assert(pg_eval_advance(&split, 1) == PG_EVAL_PENDING);
	assert(pg_eval_advance(&split, 32) == PG_EVAL_PENDING && demand_resumes == 1);
	pg_eval_destroy(&split);
	/* All prefix lengths are derived storage, not another source of values. */
	unsigned char seen_prefix[64] = {0};
	pg_eval_init(&split, input);
	split.output = graph; split.dispatch = demand_dispatch;
	while (split.status == PG_EVAL_PENDING) {
		assert(split.steps <= steps);
		if (split.frames) {
			seen_prefix[check_demand_prefix(split.frames)] = 1;
		}
		pg_eval_advance(&split, 1);
	}
	assert(split.status == PG_EVAL_WHNF && split.steps == steps);
	for (size_t i = 0; i < 64; ++i) assert(seen_prefix[i]);
	assert(pg_eval_readback(&split, graph) == expected);
	pg_eval_destroy(&split);
	puts("demand budget: shared materialization, capture, split fuel, prefix suspension and callback delivery passed");
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
	const struct pg_term *pi_x = pg_pi(graph, u0, x, vx);
	const struct pg_term *pi_y = pg_pi(graph, u0, y, vy);
	assert(pi_x && pi_y && pi_x != pi_y);
	assert(pi_x == pg_pi(graph, u0, x, vx));
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	assert(pg_pi_view(pi_x, &domain, &binder, &codomain));
	assert(domain == u0 && binder == x && codomain == vx);
	struct pg_binding_value argument = {binder, u1};
	assert(pg_term_substitute(graph, codomain, 1, &argument) == u1);
	assert(!pg_pi_view(u0, &domain, &binder, &codomain));
	assert(!pg_pi_view(pg_application(graph, pi_x, u0), &domain, &binder, &codomain));
	struct pg_whnf_work work;
	struct pg_conversion conversion;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_conversion_init(&conversion, &work, pi_x, pi_y) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	assert(pg_conversion_init(&conversion, &work, u0, u1) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
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
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *other_identity = pg_lambda(graph, y, vy);
	const struct pg_term *left = pg_lambda(graph, x, pg_application(graph, other_identity, vx));
	struct pg_comparison structural;
	assert(pg_comparison_init(&structural, left, identity, NULL, NULL) == 0);
	assert(pg_comparison_advance(&structural, 100) == PG_COMPARISON_DIFFERENT);
	pg_comparison_destroy(&structural);
	assert(pg_comparison_init(&structural, identity, pg_lambda(graph, y, vx), NULL, NULL) == 0);
	assert(pg_comparison_advance(&structural, 100) == PG_COMPARISON_DIFFERENT);
	pg_comparison_destroy(&structural);
	const struct pg_term *deep_left = vx, *deep_right = vy;
	for (size_t i = 0; i < 20000; ++i) {
		deep_left = pg_application(graph, deep_left, deep_left);
		deep_right = pg_application(graph, deep_right, deep_right);
	}
	deep_left = pg_lambda(graph, x, deep_left);
	deep_right = pg_lambda(graph, y, deep_right);
	struct pg_comparison whole;
	assert(pg_comparison_init(&structural, deep_left, deep_right, NULL, NULL) == 0);
	assert(pg_comparison_init(&whole, deep_left, deep_right, NULL, NULL) == 0);
	assert(pg_comparison_advance(&structural, 0) == PG_COMPARISON_PENDING);
	assert(pg_comparison_steps(&structural) == 0);
	while (pg_comparison_status(&structural) == PG_COMPARISON_PENDING) {
		uint64_t steps = pg_comparison_steps(&structural);
		pg_comparison_advance(&structural, 7);
		assert(pg_comparison_steps(&structural) - steps <= 7);
	}
	assert(pg_comparison_status(&structural) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_advance(&whole, UINT64_MAX) == PG_COMPARISON_EQUAL);
	assert(pg_comparison_steps(&whole) == pg_comparison_steps(&structural));
	assert(pg_comparison_task_count(&structural) == 20002);
	pg_comparison_destroy(&structural);
	pg_comparison_destroy(&whole);
	assert(pg_alpha_equal(deep_left, deep_right) == 1);
	assert(pg_term_independent(deep_left, x) == 1);
	assert(pg_term_independent(deep_left->as.lambda.body, x) == 0);
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
	/* Alpha equality must not demand normalization of divergent subterms. */
	const struct pg_term *self_x = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *self_y = pg_lambda(graph, y, pg_application(graph, vy, vy));
	const struct pg_term *omega_x = pg_application(graph, self_x, self_x);
	const struct pg_term *omega_y = pg_application(graph, self_y, self_y);
	assert(omega_x != omega_y);
	assert(pg_conversion_init(&conversion, &work, omega_x, omega_y) == 0);
	assert(pg_conversion_advance(&conversion, 100) == PG_CONVERSION_EQUAL);
	assert(pg_whnf_steps(pg_whnf_request(&work, &pg_pure_policy, omega_x)) == 0);
	pg_conversion_destroy(&conversion);
	/* Reduction elsewhere must still compare recursive subterms structurally,
	 * under the enclosing alpha map rather than an empty binding scope. */
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *pair = pg_reference(graph, pg_binder(graph));
	const struct pg_term *wrapped_y = pg_application(graph,
		pg_lambda(graph, z, pg_reference(graph, z)), vy);
	const struct pg_term *mixed_left = pg_lambda(graph, x,
		pg_application(graph, pg_application(graph, pair, vx), omega_x));
	const struct pg_term *mixed_right = pg_lambda(graph, y,
		pg_application(graph, pg_application(graph, pair, wrapped_y), omega_y));
	assert(!pg_alpha_equal(mixed_left, mixed_right));
	assert(pg_conversion_init(&conversion, &work, mixed_left, mixed_right) == 0);
	assert(pg_conversion_advance(&conversion, 1000) == PG_CONVERSION_EQUAL);
	assert(pg_whnf_steps(pg_whnf_request(&work, &pg_pure_policy, omega_x)) == 0);
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
	/* Distinct inputs that expose the exact same DAG need a bounded structural
	 * probe and one reduction task, not an expansion of the shared DAG. */
	const struct pg_term *beta_dag = pg_application(graph, identity, dag_x);
	assert(beta_dag != dag_x);
	assert(pg_conversion_init(&conversion, &work, beta_dag, dag_x) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	assert(pg_conversion_task_count(&conversion) == 4);
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
	pg_whnf_work_destroy(&work);
	puts("conversion: explicit beta comparison, binder scope, shared DAG and pending divergence passed");
}

static int reduction_child(void *owner, const void *key, size_t index, const void **child)
{
	(void)owner;
	const struct pg_reduction_certificate *certificate = key;
	if (!index) {
		*child = pg_reduction_normality(certificate);
		return *child ? 1 : 2;
	}
	--index;
	const struct pg_reduction_phase *phase = pg_reduction_phases(certificate);
	for (size_t i = 0; phase && i < index / 3; ++i) phase = phase->previous;
	if (!phase) return 0;
	*child = index % 3 ? phase->children[index % 3 - 1] : phase->head;
	return *child ? 1 : 2;
}

static void nf_dependencies(struct pg_graph *graph, const struct pg_reduction_certificate *certificate)
{
	assert(pg_reduction_kind(certificate) == PG_REDUCTION_NF);
	const struct pg_reduction_certificate *normality = pg_reduction_normality(certificate);
	if (normality) {
		assert(!pg_reduction_phases(certificate));
		assert(pg_reduction_source(certificate) == pg_reduction_target(certificate));
		assert(pg_reduction_target(normality) == pg_reduction_source(certificate));
		assert(pg_reduction_kind(normality) == PG_REDUCTION_NF);
		assert(pg_reduction_policy(normality) == pg_reduction_policy(certificate));
		return;
	}
	const struct pg_term *expected = pg_reduction_target(certificate);
	size_t count = 0;
	for (const struct pg_reduction_phase *phase = pg_reduction_phases(certificate); phase; phase = phase->previous) {
		assert(++count < 1000 && phase->rebuilt == expected);
		assert(pg_reduction_kind(phase->head) == PG_REDUCTION_WHNF);
		assert(pg_reduction_policy(phase->head) == pg_reduction_policy(certificate));
		const struct pg_term *body = pg_reduction_target(phase->head);
		const struct pg_term *rebuilt = body;
		if (phase->children[0]) {
			const struct pg_reduction_certificate *left = phase->children[0];
			assert(pg_reduction_kind(left) == PG_REDUCTION_NF);
			assert(pg_reduction_policy(left) == pg_reduction_policy(certificate));
			if (body->kind == PG_LAMBDA) {
				assert(!phase->children[1] && pg_reduction_source(left) == body->as.lambda.body);
				rebuilt = pg_lambda(graph, body->as.lambda.binder, pg_reduction_target(left));
			} else {
				const struct pg_reduction_certificate *right = phase->children[1];
				assert(body->kind == PG_APPLICATION && right);
				assert(pg_reduction_source(left) == body->as.application.function);
				assert(pg_reduction_source(right) == body->as.application.argument);
				assert(pg_reduction_kind(right) == PG_REDUCTION_NF);
				assert(pg_reduction_policy(right) == pg_reduction_policy(certificate));
				rebuilt = pg_application(graph, pg_reduction_target(left), pg_reduction_target(right));
			}
		} else assert(!phase->children[1]);
		assert(rebuilt == phase->rebuilt);
		expected = pg_reduction_source(phase->head);
	}
	assert(count && expected == pg_reduction_source(certificate));
}

static void reduction_congruence_test(struct pg_graph *graph)
{
	const struct pg_object *binder = pg_binder(graph);
	const struct pg_term *x = pg_reference(graph, binder);
	const struct pg_term *y = pg_reference(graph, pg_binder(graph));
	const struct pg_term *lambda = pg_lambda(graph, binder, x);
	/* Synthetic premises test only the congruence rule, not leaf acceptance. */
	struct pg_reduction_certificate head = {.kind = PG_REDUCTION_WHNF,
		.policy = &pg_beta_policy, .source = lambda, .target = lambda};
	struct pg_reduction_certificate child = {.kind = PG_REDUCTION_NF,
		.policy = &pg_beta_policy, .source = x, .target = y};
	assert(pg_reduction_phase_rebuild(graph, NULL, &head, &child, NULL) == pg_lambda(graph, binder, y));
	assert(pg_reduction_phase_rebuild(graph, NULL, &head, NULL, NULL) == lambda);
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &child, &child));
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, NULL, &child));
	assert(!pg_reduction_phase_rebuild(graph, NULL, NULL, &child, NULL));
	struct pg_reduction_certificate bad = child;
	bad.kind = PG_REDUCTION_WHNF;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &bad, NULL));
	bad = child;
	bad.policy = &pg_pure_policy;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &bad, NULL));
	bad = child;
	bad.source = y;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &bad, NULL));
	bad = child;
	bad.target = NULL;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &bad, NULL));
	bad = head;
	bad.kind = PG_REDUCTION_NF;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &bad, NULL, NULL));
	bad = head;
	bad.policy = NULL;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &bad, NULL, NULL));
	head.source = head.target = pg_application(graph, x, x);
	assert(pg_reduction_phase_rebuild(graph, NULL, &head, &child, &child) == pg_application(graph, y, y));
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &child, NULL));
	bad = child;
	bad.source = y;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &child, &bad));
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &bad, &child));
	bad = child;
	bad.policy = &pg_pure_policy;
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &child, &bad));
	head.source = head.target = x;
	assert(pg_reduction_phase_rebuild(graph, NULL, &head, NULL, NULL) == x);
	assert(!pg_reduction_phase_rebuild(graph, NULL, &head, &child, NULL));
	struct pg_reduction_phase previous = {.head = &head, .rebuilt = x};
	assert(pg_reduction_phase_rebuild(graph, &previous, &head, NULL, NULL) == x);
	previous.rebuilt = y;
	assert(!pg_reduction_phase_rebuild(graph, &previous, &head, NULL, NULL));
	previous.rebuilt = x;
	bad = head;
	bad.policy = &pg_pure_policy;
	previous.head = &bad;
	assert(!pg_reduction_phase_rebuild(graph, &previous, &head, NULL, NULL));
	previous.head = NULL;
	assert(!pg_reduction_phase_rebuild(graph, &previous, &head, NULL, NULL));
	assert(pg_reduction_nf_terminal(NULL, &head));
	head.source = head.target = lambda;
	assert(!pg_reduction_nf_terminal(NULL, &head));
	previous = (struct pg_reduction_phase){.head = &head, .rebuilt = lambda, .children = {&child, NULL}};
	assert(pg_reduction_nf_terminal(&previous, &head));
	head.target = pg_lambda(graph, binder, y);
	assert(!pg_reduction_nf_terminal(&previous, &head));
	head.target = lambda;
	previous.children[0] = NULL;
	assert(!pg_reduction_nf_terminal(&previous, &head));
	puts("NF congruence: shared reconstruction rejects mismatched premise endpoints, kinds and policies");
}

static void normal_form_test(struct pg_graph *graph)
{
	struct pg_whnf_work split_work, whole_work;
	assert(pg_whnf_work_init(&split_work, graph) == 0);
	assert(pg_whnf_work_init(&whole_work, graph) == 0);
	const struct pg_object *x = pg_binder(graph), *u = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vu = pg_reference(graph, u);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *delayed_unit = pg_lambda(graph, x, pg_application(graph,
		pg_reference(graph, &pg_return_operation), pg_application(graph, identity, vx)));
	const struct pg_term *term = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
		pg_application(graph, pg_application(graph, pg_reference(graph, &pg_fold_operation),
			pg_application(graph, pg_reference(graph, &pg_force_operation), vu)), delayed_unit));
	struct pg_nf_job *split = pg_nf_request(&split_work, &pg_pure_policy, term);
	assert(split && pg_nf_request(&split_work, &pg_pure_policy, term) == split);
	assert(split_work.jobs.count == 0 && !pg_nf_result(split));
	assert(pg_nf_advance(split, 0) == PG_NF_PENDING && pg_nf_steps(split) == 0);
	assert(!pg_nf_certificate(split));
	while (pg_nf_status(split) == PG_NF_PENDING) {
		uint64_t steps = pg_nf_steps(split);
		pg_nf_advance(split, 1);
		assert(pg_nf_steps(split) == steps + 1 && steps < 10000);
	}
	assert(pg_nf_status(split) == PG_NF_DONE && pg_nf_result(split) == vu);
	assert(pg_reduction_source(pg_nf_certificate(split)) == term);
	assert(pg_reduction_target(pg_nf_certificate(split)) == vu);
	assert(pg_reduction_policy(pg_nf_certificate(split)) == &pg_pure_policy);
	nf_dependencies(graph, pg_nf_certificate(split));
	struct pg_nf_job *whole = pg_nf_request(&whole_work, &pg_pure_policy, term);
	assert(pg_nf_advance(whole, 10000) == PG_NF_DONE);
	assert(pg_nf_result(whole) == vu && pg_nf_steps(whole) == pg_nf_steps(split));
	size_t count = split_work.normal_forms.count, terms = graph->terms.count;
	uint64_t steps = pg_nf_steps(split);
	assert(pg_nf_advance(pg_nf_request(&split_work, &pg_pure_policy, term), 10000) == PG_NF_DONE);
	assert(pg_nf_steps(split) == steps && split_work.normal_forms.count == count && graph->terms.count == terms);
	struct pg_nf_job *beta = pg_nf_request(&split_work, &pg_beta_policy, term);
	assert(beta != split && pg_nf_advance(beta, 10000) == PG_NF_DONE);
	assert(pg_nf_result(beta) != vu);
	nf_dependencies(graph, pg_nf_certificate(beta));
	/* NF strengthens comparison, not the runtime WHNF strategy. */
	struct pg_whnf_job *weak = pg_whnf_request(&split_work, &pg_pure_policy, term);
	assert(pg_whnf_advance(weak, 10000) == PG_EVAL_WHNF && pg_whnf_result(weak) == term);
	struct pg_conversion conversion;
	assert(pg_conversion_init(&conversion, &whole_work, vu, term) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	assert(pg_conversion_certificate(&conversion));
	uint64_t comparison_steps = pg_conversion_steps(&conversion);
	pg_conversion_destroy(&conversion);
	assert(pg_conversion_init(&conversion, &split_work, vu, term) == 0);
	while (pg_conversion_advance(&conversion, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&conversion) <= comparison_steps);
	assert(pg_conversion_status(&conversion) == PG_CONVERSION_EQUAL);
	assert(pg_conversion_steps(&conversion) == comparison_steps);
	pg_conversion_destroy(&conversion);
	/* Interleaved requests reuse child progress, with no per-parent copy. */
	const struct pg_term *body = pg_application(graph, identity, vu);
	struct pg_nf_job *child = pg_nf_request(&split_work, &pg_beta_policy, body);
	assert(pg_nf_advance(child, 1) == PG_NF_PENDING);
	struct pg_nf_job *parent = pg_nf_request(&split_work, &pg_beta_policy, pg_lambda(graph, x, body));
	assert(pg_nf_advance(parent, 10000) == PG_NF_DONE);
	assert(pg_nf_status(child) == PG_NF_DONE && pg_nf_result(child) == vu);
	assert(pg_nf_result(parent) == pg_lambda(graph, x, vu));
	struct pg_nf_job *answer = pg_nf_request(&split_work, &pg_beta_policy, pg_nf_result(parent));
	assert(pg_nf_status(answer) == PG_NF_DONE && pg_nf_steps(answer) == 0);
	assert(pg_reduction_source(pg_nf_certificate(answer)) == pg_nf_result(parent));
	assert(pg_reduction_target(pg_nf_certificate(answer)) == pg_nf_result(parent));
	nf_dependencies(graph, pg_nf_certificate(parent));
	nf_dependencies(graph, pg_nf_certificate(answer));
	assert(pg_reduction_normality(pg_nf_certificate(answer)) == pg_nf_certificate(parent));
	const struct pg_reduction_phase *parent_phase = pg_reduction_phases(pg_nf_certificate(parent));
	assert(parent_phase->previous->children[0] == pg_nf_certificate(child));
	/* Demand the head before descending: a discarded divergent argument does
	 * not prevent normalization. Under a retained thunk it does remain pending. */
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	struct pg_nf_job *discarded = pg_nf_request(&split_work, &pg_pure_policy,
		pg_application(graph, pg_lambda(graph, x, vu), omega));
	assert(pg_nf_advance(discarded, 10000) == PG_NF_DONE && pg_nf_result(discarded) == vu);
	struct pg_nf_job *divergent = pg_nf_request(&split_work, &pg_pure_policy,
		pg_application(graph, pg_reference(graph, &pg_thunk_operation), omega));
	assert(pg_nf_advance(divergent, 100) == PG_NF_PENDING && !pg_nf_result(divergent));
	assert(pg_nf_advance(divergent, 100) == PG_NF_PENDING);
	assert(!pg_nf_certificate(divergent));
	assert(pg_conversion_init(&conversion, &split_work, vu,
		pg_application(graph, pg_reference(graph, &pg_thunk_operation), omega)) == 0);
	assert(pg_conversion_advance(&conversion, 1000) == PG_CONVERSION_PENDING);
	assert(!pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	const struct pg_term *deep = vu;
	for (size_t i = 0; i < 10000; ++i) deep = pg_lambda(graph, pg_binder(graph), deep);
	struct pg_nf_job *nested = pg_nf_request(&split_work, &pg_pure_policy, deep);
	assert(pg_nf_advance(nested, 1000000) == PG_NF_DONE && pg_nf_result(nested) == deep);
	const struct pg_term *dag = vu;
	for (size_t i = 0; i < 20; ++i) dag = pg_application(graph, dag, dag);
	count = split_work.normal_forms.count;
	struct pg_nf_job *shared = pg_nf_request(&split_work, &pg_beta_policy, dag);
	assert(pg_nf_advance(shared, 100000) == PG_NF_DONE && pg_nf_result(shared) == dag);
	assert(split_work.normal_forms.count - count <= 21);
	struct pg_dag dependencies;
	assert(!pg_dag_init(&dependencies, reduction_child, NULL));
	assert(!pg_dag_add(&dependencies, pg_nf_certificate(shared)));
	/* A duplicated child edge must not expand a binary tree of receipts. */
	assert(dependencies.count <= 100);
	assert(!pg_dag_add(&dependencies, pg_nf_certificate(split)));
	assert(!pg_dag_add(&dependencies, pg_nf_certificate(beta)));
	assert(!pg_dag_add(&dependencies, pg_nf_certificate(answer)));
	assert(!pg_dag_add(&dependencies, pg_nf_certificate(nested)));
	assert(dependencies.count < 40000);
	for (const struct pg_dag_node *node = dependencies.first; node; node = node->next) {
		const struct pg_reduction_certificate *receipt = node->key;
		for (size_t i = 0; ; ++i) {
			const void *child = NULL;
			int edge = reduction_child(NULL, receipt, i, &child);
			if (!edge) break;
			if (edge == 2) continue;
			const struct pg_dag_node *premise = pg_dag_find(&dependencies, child);
			assert(premise && premise->id < node->id);
		}
		const struct pg_reduction_certificate *normality = pg_reduction_normality(receipt);
		if (normality) {
			assert(pg_dag_find(&dependencies, normality)->id < node->id);
			assert(pg_reduction_target(normality) == pg_reduction_source(receipt));
			assert(pg_reduction_kind(normality) == pg_reduction_kind(receipt));
			assert(pg_reduction_policy(normality) == pg_reduction_policy(receipt));
		}
		if (pg_reduction_kind(receipt) == PG_REDUCTION_NF) nf_dependencies(graph, receipt);
	}
	const struct pg_reduction_certificate *retained = pg_nf_certificate(parent);
	const struct pg_reduction_certificate *retained_normality = pg_nf_certificate(answer);
	pg_whnf_work_destroy(&whole_work);
	pg_whnf_work_destroy(&split_work);
	nf_dependencies(graph, retained);
	nf_dependencies(graph, retained_normality);
	/* Certificates and every reachable dependency outlive mutable job stores. */
	for (const struct pg_dag_node *node = dependencies.first; node; node = node->next)
		assert(pg_reduction_source(node->key) && pg_reduction_target(node->key));
	pg_dag_destroy(&dependencies);
	puts("normal forms: shared pure work, parent contraction, policies, split fuel and suspended divergence passed");
}

static void beta_work_test(struct pg_graph *graph)
{
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_object *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x);
	const struct pg_term *vy = pg_reference(graph, y);
	const struct pg_term *identity = pg_lambda(graph, x, vx);
	const struct pg_term *input = pg_application(graph, identity, vy);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_beta_policy, input);
	assert(job && job == pg_whnf_request(&work, &pg_beta_policy, input));
	assert(pg_whnf_status(job) == PG_EVAL_PENDING);
	assert(pg_whnf_steps(job) == 0 && !pg_whnf_result(job));
	assert(pg_whnf_advance(job, 0) == PG_EVAL_PENDING);
	assert(pg_whnf_advance(job, 1) == PG_EVAL_PENDING);
	assert(pg_whnf_steps(job) == 1);
	assert(job == pg_whnf_request(&work, &pg_beta_policy, input));
	assert(pg_whnf_advance(job, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_result(job) == vy);
	uint64_t steps = pg_whnf_steps(job);
	assert(pg_whnf_advance(job, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(job) == steps);
	assert(input != vy && pg_application(graph, identity, vy) == input);
	struct pg_whnf_job *normal = pg_whnf_request(&work, &pg_beta_policy, vy);
	assert(normal != job && pg_whnf_status(normal) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(normal) == 0 && pg_whnf_advance(normal, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(normal) == 0);
	assert(pg_reduction_source(pg_whnf_certificate(job)) == input);
	assert(pg_reduction_source(pg_whnf_certificate(normal)) == vy);
	assert(pg_reduction_target(pg_whnf_certificate(normal)) == vy);
	assert(pg_reduction_normality(pg_whnf_certificate(normal)) == pg_whnf_certificate(job));
	const struct pg_eval_policy separate_policy = {NULL};
	struct pg_whnf_job *separate = pg_whnf_request(&work, &separate_policy, vy);
	assert(separate != normal && pg_whnf_status(separate) == PG_EVAL_PENDING);
	const struct pg_term *vz = pg_reference(graph, pg_binder(graph));
	struct pg_whnf_job *waiting = pg_whnf_request(&work, &pg_beta_policy, vz);
	assert(pg_whnf_advance(waiting, 1) == PG_EVAL_PENDING);
	struct pg_whnf_job *reaches = pg_whnf_request(&work, &pg_beta_policy,
		pg_application(graph, identity, vz));
	assert(pg_whnf_advance(reaches, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_status(waiting) == PG_EVAL_WHNF && pg_whnf_result(waiting) == vz);
	assert(pg_whnf_steps(waiting) == 1 && pg_whnf_advance(waiting, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(waiting) == 1);
	/* Both requests enter the same lambda body but capture different values. */
	const struct pg_term *constant = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	struct pg_whnf_job *left = pg_whnf_request(&work, &pg_beta_policy, pg_application(graph, constant, vx));
	struct pg_whnf_job *right = pg_whnf_request(&work, &pg_beta_policy, pg_application(graph, constant, vy));
	assert(left != right);
	assert(pg_whnf_advance(left, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_advance(right, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_result(left)->as.lambda.body == vx);
	assert(pg_whnf_result(right)->as.lambda.body == vy);
	const struct pg_term *stable = pg_whnf_result(left);
	assert(pg_whnf_advance(left, 100) == PG_EVAL_WHNF);
	assert(pg_whnf_result(left) == stable);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	struct pg_whnf_job *loop = pg_whnf_request(&work, &pg_beta_policy, pg_application(graph, self, self));
	assert(pg_whnf_advance(loop, 30) == PG_EVAL_PENDING);
	assert(pg_whnf_advance(loop, 30) == PG_EVAL_PENDING);
	assert(pg_whnf_steps(loop) == 60 && !pg_whnf_result(loop));
	const struct pg_term *deep = vx, *deep_expected = vy;
	for (size_t i = 0; i < 5000; ++i) {
		deep = pg_application(graph, deep, deep);
		deep_expected = pg_application(graph, deep_expected, deep_expected);
	}
	const struct pg_term *deep_input = pg_application(graph,
		pg_lambda(graph, x, pg_lambda(graph, y, deep)), vy);
	struct pg_whnf_job *deep_job = pg_whnf_request(&work, &pg_beta_policy, deep_input);
	assert(pg_whnf_advance(deep_job, 100) == PG_EVAL_PENDING);
	assert(!pg_whnf_result(deep_job)); /* WHNF execution is short; readback is not. */
	while (pg_whnf_status(deep_job) == PG_EVAL_PENDING) {
		steps = pg_whnf_steps(deep_job);
		assert(!pg_whnf_result(deep_job));
		pg_whnf_advance(deep_job, 11);
		assert(pg_whnf_steps(deep_job) - steps <= 11);
		assert(pg_whnf_request(&work, &pg_beta_policy, deep_input) == deep_job);
	}
	const struct pg_term *deep_result = pg_whnf_result(deep_job);
	assert(deep_result && deep_result->kind == PG_LAMBDA);
	assert(deep_result->as.lambda.binder != y);
	assert(deep_result->as.lambda.body == deep_expected);
	struct pg_whnf_work whole_work;
	assert(pg_whnf_work_init(&whole_work, graph) == 0);
	struct pg_whnf_job *whole_job = pg_whnf_request(&whole_work, &pg_beta_policy, deep_input);
	assert(pg_whnf_advance(whole_job, UINT64_MAX) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(whole_job) == pg_whnf_steps(deep_job));
	assert(pg_whnf_result(whole_job)->as.lambda.body == deep_expected);
	assert(pg_whnf_result(whole_job) != deep_result); /* No alpha interning. */
	pg_whnf_work_destroy(&whole_work);
	assert(pg_whnf_work_init(&whole_work, graph) == 0);
	whole_job = pg_whnf_request(&whole_work, &pg_beta_policy, deep_input);
	assert(pg_whnf_advance(whole_job, 100) == PG_EVAL_PENDING);
	pg_whnf_work_destroy(&whole_work); /* Release suspended readback and closures. */
	const struct pg_term *neutral_body = pg_application(graph,
		pg_application(graph, vy, vx), identity);
	struct pg_whnf_job *neutral_job = pg_whnf_request(&work, &pg_beta_policy,
		pg_application(graph, pg_lambda(graph, x, neutral_body), vy));
	while (pg_whnf_advance(neutral_job, 1) == PG_EVAL_PENDING)
		assert(!pg_whnf_result(neutral_job));
	const struct pg_term *neutral_expected = pg_application(graph,
		pg_application(graph, vy, vy), identity);
	assert(pg_alpha_equal(pg_whnf_result(neutral_job), neutral_expected) == 1);
	/* Both indexes share registration, not normalization modes or progress. */
	struct pg_nf_job *nf_loop = pg_nf_request(&work, &pg_beta_policy, pg_application(graph, self, self));
	assert(nf_loop && pg_nf_advance(nf_loop, 5) == PG_NF_PENDING);
	uint64_t weak_steps = pg_whnf_steps(loop), strong_steps = pg_nf_steps(nf_loop);
	for (size_t i = 0; i < 1000; ++i) {
		const struct pg_term *input = pg_reference(graph, pg_binder(graph));
		assert(pg_whnf_request(&work, &pg_beta_policy, input));
		assert(pg_nf_request(&work, &pg_beta_policy, input));
	}
	assert(pg_nf_request(&work, &pg_beta_policy, pg_application(graph, self, self)) == nf_loop);
	assert(pg_nf_steps(nf_loop) == strong_steps && pg_whnf_steps(loop) == weak_steps);
	assert(job == pg_whnf_request(&work, &pg_beta_policy, input));
	assert(pg_whnf_result(job) == vy);
	assert(!pg_whnf_request(&work, &pg_beta_policy, NULL));
	assert(!pg_nf_request(&work, NULL, input) && !pg_whnf_request(&work, NULL, input));
	pg_whnf_work_destroy(&work);
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
	const struct pg_term *semantic = pg_reference(graph, &pg_return_operation);
	struct pg_binding_value irrelevant[64];
	const struct pg_term *closed_function = semantic;
	for (size_t i = 0; i < 64; ++i) {
		irrelevant[i] = (struct pg_binding_value){pg_binder(graph), vx};
		closed_function = pg_lambda(graph, irrelevant[i].binder, closed_function);
	}
	struct pg_substitution closed;
	assert(!pg_substitution_init(&closed, graph, semantic, 64, irrelevant));
	assert(pg_substitution_result(&closed) == semantic && pg_substitution_steps(&closed) == 0);
	pg_substitution_destroy(&closed);
	struct pg_binding_value invalid_binding = {&pg_return_operation, vx};
	assert(pg_substitution_init(&closed, graph, semantic, 1, &invalid_binding) == -1);
	for (size_t i = 0; i < 64; ++i) closed_function = pg_application(graph, closed_function, vx);
	struct pg_eval constant;
	pg_eval_init(&constant, closed_function);
	/* 64 APP steps, 64 beta steps, then the semantic head without 64 lookups. */
	assert(pg_eval_advance(&constant, 129) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&constant, graph) == semantic);
	pg_eval_destroy(&constant);
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
	struct pg_binding_value unchanged[] = {{x, vx}, {y, vy}};
	assert(!pg_substitution_init(&closed, graph, identity, 2, unchanged));
	assert(pg_substitution_status(&closed) == PG_SUBSTITUTION_DONE);
	assert(pg_substitution_result(&closed) == identity);
	assert(pg_substitution_steps(&closed) == 0);
	pg_substitution_destroy(&closed);
	/* Identity entries after a nonidentity image still have to shadow it. */
	struct pg_binding_value masked[] = {{x, vy}, {x, vx}};
	assert(pg_term_substitute(graph, vx, 2, masked) == vx);
	struct pg_binding_value identity_prefix[] = {{y, vy}, {x, vy}};
	result = pg_term_substitute(graph, lambda, 2, identity_prefix);
	assert(result && result->kind == PG_LAMBDA);
	assert(result->as.lambda.binder != y && result->as.lambda.body == vy);
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

static void shared_substitution_test(struct pg_graph *graph)
{
	struct pg_substitution_work work, independent;
	assert(!pg_substitution_work_init(&work, graph));
	assert(!pg_substitution_work_init(&independent, graph));
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y), *vz = pg_reference(graph, z);
	const struct pg_term *body = pg_application(graph, vx, vx);
	const struct pg_term *term = pg_lambda(graph, y, body);
	struct pg_binding_value image = {x, vy}, copy = image;
	struct pg_substitution *first = pg_substitution_request(&work, term, 1, &image);
	assert(first && pg_substitution_steps(first) == 0 && !pg_substitution_result(first));
	assert(pg_substitution_advance(first, 1) == PG_SUBSTITUTION_PENDING);
	size_t terms = graph->terms.count;
	struct pg_substitution *second = pg_substitution_request(&work, term, 1, &copy);
	assert(second == first && pg_substitution_steps(second) == 1 && graph->terms.count == terms);
	copy.value = vz;
	struct pg_substitution *different = pg_substitution_request(&work, term, 1, &copy);
	struct pg_substitution *other_store = pg_substitution_request(&independent, term, 1, &image);
	assert(different && different != first && !pg_substitution_steps(different));
	assert(other_store && other_store != first && !pg_substitution_steps(other_store));
	const struct pg_term *answer = pg_substitution_compute(&work, term, 1, &image);
	assert(answer && answer == pg_substitution_result(first));
	assert(answer->as.lambda.binder != y);
	assert(answer->as.lambda.body == pg_application(graph, vy, vy));
	assert(!first->state->context.temporary.blocks && !first->state->context.results.capacity);
	uint64_t steps = pg_substitution_steps(first);
	assert(pg_substitution_compute(&work, term, 1, &image) == answer);
	assert(pg_substitution_steps(first) == steps);
	assert(pg_substitution_status(different) == PG_SUBSTITUTION_PENDING);
	assert(pg_substitution_status(other_store) == PG_SUBSTITUTION_PENDING);
	const struct pg_term *alpha = pg_lambda(graph, z, body);
	assert(pg_alpha_equal(term, alpha) == 1);
	assert(pg_substitution_request(&work, alpha, 1, &image) != first);
	struct pg_binding_value ordered[] = {{x, vy}, {y, vx}}, reversed[] = {{y, vx}, {x, vy}};
	assert(pg_substitution_request(&work, body, 2, ordered) != pg_substitution_request(&work, body, 2, reversed));
	struct pg_binding_value identity[] = {{x, vx}, {y, vy}};
	assert(pg_substitution_request(&work, term, 2, identity) == pg_substitution_request(&work, term, 0, NULL));
	const struct pg_term *semantic = pg_reference(graph, &pg_return_operation);
	assert(pg_substitution_request(&work, semantic, 1, &image) == pg_substitution_request(&work, semantic, 0, NULL));
	size_t requests = work.jobs.count;
	struct pg_binding_value invalid = {&pg_return_operation, vx};
	assert(!pg_substitution_request(&work, semantic, 1, &invalid));
	assert(!pg_substitution_request(&work, term, 1, NULL));
	assert(!pg_substitution_request(&work, term, SIZE_MAX, &image));
	assert(!pg_substitution_request(&work, NULL, 0, NULL));
	assert(work.jobs.count == requests);
	/* Cancellation releases pending traversals without invalidating outputs. */
	pg_substitution_work_destroy(&independent);
	pg_substitution_work_destroy(&work);
	assert(!pg_substitution_request(&work, term, 1, &image));
	assert(answer->as.lambda.body == pg_application(graph, vy, vy));
	puts("shared substitution: exact inputs, capture, pending reuse, compact results and independent owners passed");
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
	const struct pg_term *lookup_body = vx;
	for (size_t i = 0; i < 64; ++i) lookup_body = pg_lambda(graph, pg_binder(graph), lookup_body);
	const struct pg_term *lookup_input = pg_application(graph, pg_lambda(graph, x, lookup_body), va);
	for (size_t i = 0; i < 64; ++i) lookup_input = pg_application(graph, lookup_input, vb);
	pg_eval_init(&whole, lookup_input);
	pg_eval_init(&split, lookup_input);
	assert(pg_eval_advance(&split, 130) == PG_EVAL_PENDING);
	assert(split.current.term == vx);
	for (size_t i = 0; i < 64; ++i) {
		assert(pg_eval_advance(&split, 1) == PG_EVAL_PENDING);
		assert(split.current.term == vx);
		assert(pg_eval_readback(&split, graph) == va);
	}
	assert(pg_eval_advance(&split, 2) == PG_EVAL_WHNF);
	assert(pg_eval_advance(&whole, 1000) == PG_EVAL_WHNF);
	assert(split.steps == whole.steps);
	assert(pg_eval_readback(&whole, graph) == va);
	pg_eval_destroy(&whole);
	pg_eval_destroy(&split);
	puts("evaluation: lexical capture, shared closures, split budgets and divergence passed");
}

static void enumerate(struct pg_dimensions *dimensions, size_t source,
	size_t target, struct pg_coordinate *coordinates, size_t position)
{
	if (position == target) {
		const struct pg_dimension_map *map = pg_dimension_map(dimensions, source, target, coordinates);
		if (map) {
			const struct pg_dimension_map *ordered = map, *intrinsic = map;
			int factored = pg_dimension_face_factor(dimensions, map, &ordered, &intrinsic);
			if (pg_dimension_face(dimensions, map)) {
				assert(factored == 0);
				assert(pg_dimension_compose(dimensions, ordered, intrinsic) == map);
				assert(intrinsic->source == source && intrinsic->target == source);
				for (size_t i = 0, axis = 0; i < target; ++i) {
					if (ordered->coordinates[i].kind == PG_AXIS) assert(ordered->coordinates[i].axis == axis++);
				}
				const struct pg_dimension_map *again, *identity;
				assert(pg_dimension_face_factor(dimensions, ordered, &again, &identity) == 0);
				assert(again == ordered && identity == pg_dimension_identity(dimensions, source));
			} else {
				assert(factored == -1 && ordered == map && intrinsic == map);
			}
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

static const struct pg_term *symmetry_normalize(struct pg_graph *graph, const struct pg_term *term)
{
	struct pg_eval machine;
	pg_eval_init(&machine, term);
	machine.output = graph;
	machine.dispatch = pg_pure_policy.dispatch;
	assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	pg_eval_destroy(&machine);
	return result;
}

static void induced_face_permutations(struct pg_dimensions *dimensions)
{
	for (size_t dimension = 0, slots = 1; dimension <= 5; ++dimension, slots *= 3) {
		struct pg_coordinate coordinates[5];
		for (size_t slot = 0; slot < slots; ++slot) {
			size_t source = SIZE_MAX;
			assert(pg_dimension_cube_coordinates(dimension, slot, coordinates, &source) == 0);
			size_t encoded = 0, axes = 0;
			for (size_t i = 0; i < dimension; ++i) {
				size_t digit = coordinates[i].kind;
				if (coordinates[i].kind == PG_AXIS) assert(coordinates[i].axis == axes++);
				encoded = 3 * encoded + digit;
			}
			assert(encoded == slot && source == axes);
			const struct pg_dimension_map *face = pg_dimension_map(dimensions, source, dimension, coordinates);
			assert(face && pg_dimension_face(dimensions, face) == face);
			if (slot + 1 == slots) assert(face == pg_dimension_identity(dimensions, dimension));
		}
		coordinates[0] = (struct pg_coordinate){PG_AXIS, 42};
		size_t source = 42;
		assert(pg_dimension_cube_coordinates(dimension, slots, coordinates, &source) == -1);
		assert(source == 42 && coordinates[0].axis == 42);
		assert(pg_dimension_cube_coordinates(SIZE_MAX, 0, coordinates, &source) == -1);
		assert(source == 42 && coordinates[0].axis == 42);
	}
	size_t empty_source = SIZE_MAX;
	assert(pg_dimension_cube_coordinates(0, 0, NULL, &empty_source) == 0 && !empty_source);
	assert(pg_dimension_cube_coordinates(1, 0, NULL, &empty_source) == -1);
	const size_t orders[6][3] = {{0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}};
	const struct pg_dimension_map *permutations[6];
	struct pg_coordinate coordinates[3];
	for (size_t p = 0; p < 6; ++p) {
		for (size_t i = 0; i < 3; ++i) coordinates[i] = (struct pg_coordinate){PG_AXIS, orders[p][i]};
		permutations[p] = pg_dimension_map(dimensions, 3, 3, coordinates);
		assert(permutations[p]);
		const struct pg_dimension_map *inverse = pg_dimension_inverse(dimensions, permutations[p]);
		assert(inverse && pg_dimension_inverse(dimensions, inverse) == permutations[p]);
		assert(pg_dimension_compose(dimensions, inverse, permutations[p]) == pg_dimension_identity(dimensions, 3));
		assert(pg_dimension_compose(dimensions, permutations[p], inverse) == pg_dimension_identity(dimensions, 3));
		assert(pg_dimension_prefix(dimensions, 0, permutations[p]) == permutations[p]);
		const struct pg_dimension_map *extended = pg_dimension_prefix(dimensions, 1, permutations[p]);
		assert(extended && extended->coordinates[0].kind == PG_AXIS && extended->coordinates[0].axis == 0);
		for (size_t i = 0; i < 3; ++i) assert(extended->coordinates[i + 1].axis == orders[p][i] + 1);
		assert(pg_dimension_inverse(dimensions, extended) == pg_dimension_prefix(dimensions, 1, inverse));
		unsigned char visited[27] = {0};
		for (size_t slot = 0; slot < 27; ++slot) {
			size_t moved = SIZE_MAX, source;
			const struct pg_dimension_map *intrinsic = NULL;
			size_t maps_before = dimensions->maps.count;
			assert(pg_dimension_cube_permute_slot(dimensions, permutations[p], slot, &moved, &intrinsic) == 0);
			/* Only the resulting intrinsic map may be new, not temporary faces. */
			assert(dimensions->maps.count <= maps_before + 1);
			assert(moved < 27 && !visited[moved]++);
			size_t digits[] = {slot / 9, (slot / 3) % 3, slot % 3};
			assert(moved == 9 * digits[orders[p][0]] + 3 * digits[orders[p][1]] + digits[orders[p][2]]);
			assert(pg_dimension_cube_coordinates(3, slot, coordinates, &source) == 0);
			const struct pg_dimension_map *face = pg_dimension_map(dimensions, source, 3, coordinates);
			assert(pg_dimension_cube_coordinates(3, moved, coordinates, &source) == 0);
			const struct pg_dimension_map *ordered = pg_dimension_map(dimensions, source, 3, coordinates);
			assert(pg_dimension_compose(dimensions, ordered, intrinsic) ==
				pg_dimension_compose(dimensions, permutations[p], face));
			if (slot == 26) assert(intrinsic == permutations[p]);
			size_t restored;
			const struct pg_dimension_map *back;
			assert(pg_dimension_cube_permute_slot(dimensions, inverse, moved, &restored, &back) == 0);
			assert(restored == slot);
			assert(pg_dimension_compose(dimensions, back, intrinsic) == pg_dimension_identity(dimensions, source));
		}
		size_t unchanged = 42;
		const struct pg_dimension_map *orientation = inverse;
		assert(pg_dimension_cube_permute_slot(dimensions, permutations[p], 27, &unchanged, &orientation) == -1);
		assert(unchanged == 42 && orientation == inverse);
	}
	assert(pg_dimension_prefix(dimensions, 1, permutations[0]) == pg_dimension_identity(dimensions, 4));
	assert(!pg_dimension_prefix(dimensions, SIZE_MAX, permutations[0]));
	assert(!pg_dimension_prefix(dimensions, 1, NULL));
	for (size_t code = 0; code < 27; ++code) {
		size_t rest = code, axes = 0;
		for (size_t i = 0; i < 3; ++i, rest /= 3) {
			size_t digit = rest % 3;
			coordinates[i] = (struct pg_coordinate){(enum pg_coordinate_kind)digit, digit == 2 ? axes++ : 0};
		}
		const struct pg_dimension_map *face = pg_dimension_map(dimensions, axes, 3, coordinates);
		assert(face);
		for (size_t p = 0; p < 6; ++p) {
			const struct pg_dimension_map *moved, *local;
			assert(pg_dimension_prefix(dimensions, 1, pg_dimension_compose(dimensions, permutations[p], face)) ==
				pg_dimension_compose(dimensions, pg_dimension_prefix(dimensions, 1, permutations[p]),
					pg_dimension_prefix(dimensions, 1, face)));
			assert(pg_dimension_face_factor(dimensions, pg_dimension_compose(dimensions, permutations[p], face), &moved, &local) == 0);
			for (size_t q = 0; q < 6; ++q) {
				const struct pg_dimension_map *twice, *second, *direct, *combined;
				assert(pg_dimension_face_factor(dimensions, pg_dimension_compose(dimensions, permutations[q], moved), &twice, &second) == 0);
				const struct pg_dimension_map *composition = pg_dimension_compose(dimensions, permutations[q], permutations[p]);
				assert(pg_dimension_face_factor(dimensions, pg_dimension_compose(dimensions, composition, face), &direct, &combined) == 0);
				assert(twice == direct);
				assert(pg_dimension_compose(dimensions, second, local) == combined);
			}
		}
	}
	struct pg_graph *graph = dimensions->graph;
	const struct pg_object *x = pg_binder(graph);
	const struct pg_term *value = pg_reference(graph, x);
	for (size_t p = 0; p < 6; ++p) {
		const struct pg_term *once = pg_symmetry(graph, permutations[p], value);
		assert(once && once != value);
		assert(pg_symmetry(graph, permutations[p], value) == once);
		size_t dimension = 0;
		const size_t *axes = NULL;
		const struct pg_term *argument = NULL;
		size_t terms = graph->terms.count, objects = graph->objects.count;
		assert(pg_symmetry_view(once, &dimension, &axes, &argument));
		assert(dimension == 3 && argument == value);
		for (size_t i = 0; i < dimension; ++i) assert(axes[i] == permutations[p]->coordinates[i].axis);
		assert(graph->terms.count == terms && graph->objects.count == objects);
		const size_t *saved_axes = axes;
		assert(!pg_symmetry_view(NULL, &dimension, &axes, &argument));
		assert(!pg_symmetry_view(value, &dimension, &axes, &argument));
		assert(!pg_symmetry_view(once->as.application.function, &dimension, &axes, &argument));
		assert(!pg_symmetry_view(pg_application(graph, once, value), &dimension, &axes, &argument));
		assert(dimension == 3 && axes == saved_axes && argument == value);
		for (size_t q = 0; q < 6; ++q) {
			const struct pg_term *input = pg_symmetry(graph, permutations[q], once);
			const struct pg_dimension_map *map = pg_dimension_compose(dimensions, permutations[q], permutations[p]);
			const struct pg_term *expected = pg_symmetry(graph, map, value);
			assert(symmetry_normalize(graph, input) == symmetry_normalize(graph, expected));
		}
	}
	/* Prefix extension must respect composition, including noncommuting cycles. */
	for (size_t p = 0; p < 6; ++p) for (size_t q = 0; q < 6; ++q) {
		const struct pg_dimension_map *combined = pg_dimension_compose(dimensions,
			permutations[q], permutations[p]);
		const struct pg_term *expected = symmetry_normalize(graph, pg_symmetry(graph, combined, value));
		for (size_t left = 0; left < 3; ++left) for (size_t right = 0; right < 3; ++right) {
			const struct pg_dimension_map *inner = pg_dimension_prefix(dimensions, left, permutations[p]);
			const struct pg_dimension_map *outer = pg_dimension_prefix(dimensions, right, permutations[q]);
			const struct pg_term *input = pg_symmetry(graph, outer, pg_symmetry(graph, inner, value));
			assert(symmetry_normalize(graph, input) == expected);
			/* Normalization never changes the exact interner's answer. */
			assert(input == pg_symmetry(graph, outer, pg_symmetry(graph, inner, value)));
		}
	}
}

static void dimension_test(struct pg_graph *graph)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	induced_face_permutations(&dimensions);
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
	const struct pg_dimension_map *ordered, *intrinsic;
	assert(pg_dimension_face_factor(&dimensions, cycle, &ordered, &intrinsic) == 0);
	assert(ordered == pg_dimension_identity(&dimensions, 3) && intrinsic == cycle);
	assert(pg_dimension_face_factor(&dimensions, NULL, &ordered, &intrinsic) == -1);
	assert(pg_dimension_face_factor(&dimensions, cycle, NULL, &intrinsic) == -1);
	const struct pg_dimension_map *twice = pg_dimension_compose(&dimensions, cycle, cycle);
	assert(pg_dimension_compose(&dimensions, cycle, twice) == pg_dimension_identity(&dimensions, 3));
	struct pg_coordinate face_coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ONE, 0}, {PG_AXIS, 1}};
	struct pg_coordinate projection_coordinates[] = {{PG_AXIS, 0}, {PG_AXIS, 2}};
	const struct pg_dimension_map *face = pg_dimension_map(&dimensions, 2, 3, face_coordinates);
	const struct pg_dimension_map *moved = pg_dimension_compose(&dimensions, cycle, face);
	assert(pg_dimension_face_factor(&dimensions, moved, &ordered, &intrinsic) == 0);
	assert(pg_dimension_compose(&dimensions, ordered, intrinsic) == moved);
	assert(intrinsic->coordinates[0].axis == 1 && intrinsic->coordinates[1].axis == 0);
	assert(ordered->coordinates[2].kind == PG_ENDPOINT_ONE);
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
	/* Cube permutations postcompose every face; restrictions precompose it. */
	struct pg_coordinate swap_axes[] = {{PG_AXIS, 1}, {PG_AXIS, 0}};
	const struct pg_dimension_map *swap = pg_dimension_map(&dimensions, 2, 2, swap_axes);
	assert(pg_binding_permute(&dimensions, bottom, swap) == side);
	assert(pg_binding_permute(&dimensions, side, swap) == bottom);
	assert(pg_binding_permute(&dimensions, pg_binding_restrict(&dimensions, bottom, zero), swap)
		== pg_binding_restrict(&dimensions, pg_binding_permute(&dimensions, bottom, swap), zero));
	const struct pg_binding_face *rotated = pg_binding_permute(&dimensions, line, cycle);
	assert(rotated && rotated != line && rotated->cube == cube);
	assert(pg_binding_permute(&dimensions, rotated, twice) == line);
	assert(pg_binding_permute(&dimensions, rotated, cycle) == pg_binding_permute(&dimensions, line, twice));
	assert(pg_binding_permute(&dimensions, surface, pg_dimension_identity(&dimensions, 3)) == surface);
	assert(pg_binding_restrict(&dimensions, pg_binding_permute(&dimensions, surface, cycle), edge) == rotated);
	assert(!pg_binding_permute(&dimensions, NULL, cycle));
	assert(!pg_binding_permute(&dimensions, surface, NULL));
	assert(!pg_binding_permute(&dimensions, bottom, cycle));
	assert(!pg_binding_permute(&dimensions, surface, projection));
	struct pg_coordinate constant_axes[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	assert(!pg_binding_permute(&dimensions, bottom, pg_dimension_map(&dimensions, 2, 2, constant_axes)));
	/* Dropping a source dimension cannot introduce an independent binder. */
	assert(!pg_binding_face(&dimensions, square, projection));
	assert(!pg_binding_restrict(&dimensions, top, edge));
	assert(pg_binding_restrict(&dimensions, top, pg_dimension_identity(&dimensions, 3)) == top);
	for (size_t n = 4; n < 150; ++n) assert(pg_dimension_identity(&dimensions, n));
	assert(dimensions.maps.capacity > 64);
	assert(cycle == pg_dimension_map(&dimensions, 3, 3, permutation));
	const struct pg_object *captured = pg_binder(graph);
	const struct pg_term *captured_value = pg_reference(graph, captured);
	const struct pg_term *twice_swapped = pg_symmetry(graph, swap, pg_symmetry(graph, swap, captured_value));
	const struct pg_term *closed_symmetry = pg_application(graph, pg_lambda(graph, captured, twice_swapped), line_term);
	const struct pg_term *hidden_symmetry = pg_symmetry(graph, swap,
		pg_application(graph, pg_lambda(graph, captured, pg_symmetry(graph, swap, captured_value)), line_term));
	struct pg_coordinate reverse_axes[128];
	for (size_t i = 0; i < 128; ++i) reverse_axes[i] = (struct pg_coordinate){PG_AXIS, 127 - i};
	const struct pg_dimension_map *reverse = pg_dimension_map(&dimensions, 128, 128, reverse_axes);
	const struct pg_term *wide_symmetry = pg_symmetry(graph, reverse, pg_symmetry(graph, reverse, line_term));
	const struct pg_dimension_map *prefixed_swap = pg_dimension_prefix(&dimensions, 126, swap);
	const struct pg_term *prefixed = pg_symmetry(graph, prefixed_swap, captured_value);
	assert(prefixed && prefixed != pg_symmetry(graph, swap, captured_value));
	assert(prefixed == pg_symmetry(graph, prefixed_swap, captured_value));
	const struct pg_term *prefix_capture = pg_application(graph,
		pg_lambda(graph, captured, pg_symmetry(graph, swap, prefixed)), line_term);
	const struct pg_term *prefix_outer = pg_application(graph,
		pg_lambda(graph, captured, pg_symmetry(graph, prefixed_swap,
			pg_symmetry(graph, swap, captured_value))), line_term);
	const struct pg_term *prefix_applied = pg_application(graph,
		pg_symmetry(graph, prefixed_swap, pg_symmetry(graph, swap,
			pg_lambda(graph, captured, captured_value))), line_term);
	assert(!pg_symmetry(graph, projection, line_term));
	assert(!pg_dimension_inverse(&dimensions, projection));
	assert(!pg_dimension_inverse(&dimensions, NULL));
	assert(pg_dimension_inverse(&dimensions, pg_dimension_identity(&dimensions, 0)) == pg_dimension_identity(&dimensions, 0));
	struct pg_coordinate repeated_axes[] = {{PG_AXIS, 0}, {PG_AXIS, 0}};
	struct pg_dimension_map invalid_permutation = {2, 2, repeated_axes};
	assert(!pg_symmetry(graph, &invalid_permutation, line_term));
	assert(!pg_dimension_inverse(&dimensions, &invalid_permutation));
	printf("dimension: %zu maps, %zu composable triples; 3D faces/permutations passed\n", map_count, triples);
	pg_dimensions_destroy(&dimensions);
	/* Operator lifetime follows the graph, and composition retains capture. */
	assert(pg_binding_face_view(&top->variable) == top);
	assert(pg_binding_face_view(&bottom->variable) == bottom);
	assert(!pg_binding_face_view(captured));
	assert(!pg_binding_face_view(NULL));
	struct pg_object semantic = {PG_SEMANTIC_OBJECT, top->variable.owner};
	assert(!pg_binding_face_view(&semantic));
	const struct pg_term *face_identity = pg_lambda(graph, &top->variable, pg_reference(graph, &top->variable));
	const struct pg_term *ordinary_identity = pg_lambda(graph, captured, captured_value);
	assert(face_identity != ordinary_identity);
	assert(pg_alpha_equal(face_identity, ordinary_identity) == 1);
	const struct pg_term *symmetry_cases[] = {closed_symmetry, hidden_symmetry, wide_symmetry,
		prefix_capture, prefix_outer, prefix_applied};
	size_t case_count = sizeof(symmetry_cases) / sizeof(*symmetry_cases);
	size_t symmetry_demands = 0;
	const struct pg_eval_continuation *symmetry_continuation = pg_symmetry_continuation_resolve("symmetry/symmetry_answer/v1");
	for (size_t test = 0; test < case_count; ++test) {
		struct pg_eval whole;
		pg_eval_init(&whole, symmetry_cases[test]);
		whole.output = graph;
		whole.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&whole, 1000) == PG_EVAL_WHNF);
		uint64_t steps = whole.steps;
		if (test == 2) assert(steps >= 128);
		assert(pg_eval_readback(&whole, graph) == line_term);
		pg_eval_destroy(&whole);
		for (uint64_t cut = 0; cut <= steps; ++cut) {
			struct pg_eval split;
			pg_eval_init(&split, symmetry_cases[test]);
			split.output = graph;
			split.dispatch = pg_pure_policy.dispatch;
			pg_eval_advance(&split, cut);
			for (const struct pg_eval_frame *frame = split.frames; frame; frame = frame->parent) {
				if (frame->continuation != symmetry_continuation) continue;
				assert(!frame->state && frame->caller.term->kind == PG_REFERENCE);
				++symmetry_demands;
			}
			assert(pg_eval_advance(&split, steps - cut) == PG_EVAL_WHNF);
			assert(split.steps == steps);
			assert(pg_eval_readback(&split, graph) == line_term);
			pg_eval_destroy(&split);
		}
	}
	assert(symmetry_demands);
	for (size_t test = 0; test < case_count; ++test) for (uint64_t cut = 0; cut < 160; ++cut) {
		struct pg_eval machine;
		pg_eval_init(&machine, symmetry_cases[test]);
		machine.output = graph;
		machine.dispatch = pg_pure_policy.dispatch;
		pg_eval_advance(&machine, cut);
		const struct pg_term *snapshot = pg_eval_readback(&machine, graph);
		assert(snapshot);
		pg_eval_destroy(&machine);
		pg_eval_init(&machine, snapshot);
		machine.output = graph;
		machine.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, graph) == line_term);
		pg_eval_destroy(&machine);
	}
	const struct pg_term *self = pg_lambda(graph, captured, pg_application(graph, captured_value, captured_value));
	const struct pg_term *divergent = pg_symmetry(graph, swap, pg_application(graph, self, self));
	struct pg_eval pending;
	pg_eval_init(&pending, divergent);
	pending.output = graph;
	pending.dispatch = pg_pure_policy.dispatch;
	assert(pg_eval_advance(&pending, 100) == PG_EVAL_PENDING);
	assert(pg_eval_readback(&pending, graph));
	pg_eval_destroy(&pending);
}

static void evidence_owner_test(struct pg_graph *graph)
{
	struct pg_typing first, second;
	struct pg_classifiers classifiers;
	assert(!pg_typing_init(&first, graph) && !pg_typing_init(&second, graph));
	assert(!pg_classifiers_init(&classifiers, graph));
	const struct pg_evidence *contexts[] = {
		pg_prove_empty_context(&first), pg_prove_empty_context(&second)};
	const struct pg_evidence *types[] = {
		pg_prove_universe(&first, &classifiers, contexts[0], 0),
		pg_prove_universe(&second, &classifiers, contexts[1], 0)};
	assert(types[0] && types[1] && types[0] != types[1]);
	assert(pg_evidence_context(types[0]) == pg_evidence_context(types[1]));
	assert(pg_evidence_subject(types[0])->core == pg_evidence_subject(types[1])->core);
	struct pg_typing *stores[] = {&first, &second};
	for (size_t i = 0; i < 2; ++i) {
		struct pg_typing *store = stores[i];
		const struct pg_evidence *foreign = types[1 - i];
		size_t count = store->proofs.count;
		assert(pg_evidence_owned_by(types[i], store));
		assert(!pg_evidence_owned_by(foreign, store));
		assert(!pg_evidence_owned_by(NULL, store));
		assert(!pg_evidence_owned_by(types[i], NULL));
		assert(!pg_prove_value_type(store, foreign));
		assert(!pg_prove_type_value(store, foreign));
		assert(!pg_prove_return_type(store, &classifiers, foreign));
		assert(!pg_prove_universe(store, &classifiers, contexts[1 - i], 0));
		assert(!pg_prove_projection(store, contexts[i], foreign));
		assert(!pg_prove_substitution(store, contexts[i], contexts[1 - i], 0, NULL));
		assert(store->proofs.count == count);
		assert(pg_prove_universe(store, &classifiers, contexts[i], 0) == types[i]);
	}
	/* Arena-owned evidence survives index disposal, but not as accepted input
	 * to another initialization of the same C storage address. */
	pg_typing_destroy(&second);
	assert(!pg_evidence_owned_by(types[1], &second));
	assert(!pg_typing_init(&second, graph));
	assert(!pg_evidence_owned_by(types[1], &second));
	assert(!pg_prove_value_type(&second, types[1]));
	assert(!pg_prove_universe(&second, &classifiers, contexts[1], 0));
	assert(second.proofs.count == 0);
	const struct pg_evidence *fresh = pg_prove_empty_context(&second);
	assert(fresh && fresh != contexts[1] && pg_evidence_owned_by(fresh, &second));
	assert(pg_prove_universe(&second, &classifiers, fresh, 0));
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&second);
	pg_typing_destroy(&first);
	puts("evidence ownership: shared Core and reused storage do not transfer acceptance between stores");
}

static void effect_classifier_test(struct pg_graph *graph)
{
	static const struct pg_object_class label_class = {"test-effect-label"};
	static const struct pg_object a = {PG_SEMANTIC_OBJECT, &label_class};
	static const struct pg_object b = {PG_SEMANTIC_OBJECT, &label_class};
	static const struct pg_object c = {PG_SEMANTIC_OBJECT, &label_class};
	const struct pg_object *labels[] = {&b, &a, &b};
	const struct pg_effect_row *empty = pg_effect_row(graph, 0, NULL);
	const struct pg_effect_row *ab = pg_effect_row(graph, 3, labels);
	const struct pg_effect_row *only_b = pg_effect_row(graph, 1, labels);
	const struct pg_effect_row *only_a = pg_effect_row(graph, 1, labels + 1);
	const struct pg_object *third[] = {&c};
	const struct pg_effect_row *only_c = pg_effect_row(graph, 1, third);
	assert(empty && ab && only_a && only_b && only_c);
	assert(pg_effect_count(empty) == 0 && pg_effect_count(ab) == 2);
	assert(pg_effect_count(NULL) == SIZE_MAX && pg_effect_contains(NULL, &a) == -1);
	assert(pg_effect_contains(ab, &a) == 1 && pg_effect_contains(ab, &c) == 0);
	assert(pg_effect_union(graph, only_a, only_b) == ab);
	assert(pg_effect_union(graph, only_b, only_a) == ab);
	assert(pg_effect_union(graph, empty, ab) == ab && pg_effect_union(graph, ab, empty) == ab);
	assert(pg_effect_union(graph, ab, ab) == ab);
	assert(pg_effect_union(graph, ab, only_c) == pg_effect_union(graph, only_a, pg_effect_union(graph, only_b, only_c)));
	assert(!pg_effect_union(graph, NULL, empty));
	assert(pg_effect_subset(NULL, empty) == -1 && pg_effect_subset(empty, NULL) == -1);
	const struct pg_effect_row *sets[8];
	const struct pg_object *alphabet[] = {&a, &b, &c};
	for (unsigned mask = 0; mask < 8; ++mask) {
		const struct pg_object *selected[3];
		size_t count = 0;
		for (unsigned bit = 0; bit < 3; ++bit)
			if (mask & (1u << bit)) selected[count++] = alphabet[bit];
		sets[mask] = pg_effect_row(graph, count, selected);
		assert(sets[mask]);
	}
	for (unsigned i = 0; i < 8; ++i)
		for (unsigned j = 0; j < 8; ++j) {
			assert(pg_effect_subset(sets[i], sets[j]) == ((i & j) == i));
			assert(pg_effect_difference(graph, sets[i], sets[j]) == sets[i & ~j]);
		}
	assert(!pg_effect_difference(graph, empty, NULL));
	assert(!pg_effect_row(graph, 1, NULL));
	const struct pg_object *invalid[] = {pg_binder(graph)};
	assert(!pg_effect_row(graph, 1, invalid));
	struct pg_classifiers classifiers;
	assert(!pg_classifiers_init(&classifiers, graph));
	const struct pg_term *u = pg_universe(&classifiers, 0);
	const struct pg_term *pure = pg_return_type(&classifiers, u);
	assert(pure == pg_effect_type(&classifiers, empty, u));
	const struct pg_term *effectful = pg_effect_type(&classifiers, ab, u);
	assert(effectful && pg_alpha_equal(pure, effectful) == 0);
	const struct pg_effect_row *row = NULL;
	const struct pg_term *value = NULL;
	assert(pg_effect_type_view(pure, &row, &value) && row == empty && value == u);
	assert(pg_effect_type_view(effectful, &row, &value) && row == ab && value == u);
	assert(!pg_return_type_view(effectful, &value) && value == u);
	assert(pg_return_type_view(pure, &value) && value == u);
	assert(!pg_effect_type(&classifiers, NULL, u));
	/* A structural row parameter is not a nominal operation or a closed row.
	 * Its binder lives in the graph, not in a temporary effect worker. */
	const struct pg_object *rho = pg_binder(graph);
	const struct pg_term *row_parameter = pg_reference(graph, rho);
	const struct pg_term *pending = pg_effect_type_spine(&classifiers, row_parameter, u);
	assert(pending == pg_effect_type_spine(&classifiers, row_parameter, u));
	const struct pg_term *row_term;
	assert(pg_effect_type_spine_view(pending, &row_term, &value));
	assert(row_term == row_parameter && value == u);
	row = ab;
	assert(!pg_effect_type_view(pending, &row, &value) && row == ab);
	assert(!pg_return_type_view(pending, &value));
	assert(!pg_effect_row_view(row_parameter));
	const struct pg_object *parameter_label[] = {rho};
	assert(!pg_effect_row(graph, 1, parameter_label));
	assert(!pg_effect_type_spine(&classifiers, NULL, u));
	assert(!pg_effect_type_spine_view(u, &row_term, &value));
	struct pg_binding_value row_binding = {rho, pg_effect_reference(graph, ab)};
	assert(pg_term_substitute(graph, pending, 1, &row_binding) == effectful);
	const struct pg_object *argument = pg_binder(graph);
	const struct pg_term *latent = pg_thunk_type(&classifiers, pg_pi(graph, u, argument, pending));
	const struct pg_term *resolved = pg_term_substitute(graph, latent, 1, &row_binding);
	assert(pg_alpha_equal(resolved, pg_thunk_type(&classifiers,
		pg_pi(graph, u, argument, effectful))) == 1);
	assert(!pg_effect_type_view(pending, &row, &value));
	assert(!pg_classifier_resolve(&classifiers, "kernel/return-type/v1"));
	assert(!pg_classifier_resolve(&classifiers, "kernel/return-type/v2"));
	assert(pg_classifier_resolve(&classifiers, "kernel/return-type/v3"));
	assert(pg_classifier_resolve(&classifiers, "kernel/effect-row/empty/v1"));
	struct pg_typing typing;
	assert(!pg_typing_init(&typing, graph));
	const struct pg_evidence *context = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, context, 0);
	size_t accepted_before = typing.proofs.count;
	const struct pg_context *pending_scope = pg_context_bind(&typing, NULL, argument, latent, PG_JUDGEMENT_VALUE);
	assert(pending_scope && pending_scope->declared_type == latent);
	assert(pg_occurrence(&typing, PG_JUDGEMENT_INPUT, pending_scope, pg_reference(graph, argument), NULL, latent, 0, NULL));
	assert(typing.proofs.count == accepted_before);
	const struct pg_evidence *formation = pg_prove_effect_type(&typing, &classifiers, ab, universe);
	assert(formation && pg_evidence_rule(formation) == PG_RETURN_TYPE_FORM);
	assert(pg_evidence_subject(formation)->core == effectful);
	assert(formation == pg_prove_effect_type(&typing, &classifiers, ab, universe));
	assert(!pg_prove_effect_type(&typing, &classifiers, NULL, universe));
	assert(pg_evidence_subject(pg_prove_return_content(&typing, formation))->core == u);
	struct pg_derivation_parameters parameters;
	assert(!pg_derivation_parameters(formation, &parameters) && parameters.effects == ab);
	const struct pg_evidence *premises[] = {pg_evidence_premise(formation, 0)};
	assert(pg_prove_derivation(&typing, &classifiers, PG_RETURN_TYPE_FORM, &parameters, 1, premises) == formation);
	parameters.effects = NULL;
	assert(!pg_prove_derivation(&typing, &classifiers, PG_RETURN_TYPE_FORM, &parameters, 1, premises));
	const struct pg_object *m = pg_binder(graph);
	const struct pg_evidence *scope = pg_prove_context_extension(&typing, context, m,
		pg_prove_thunk_type(&typing, &classifiers, formation));
	const struct pg_evidence *force = pg_prove_force(&typing, pg_prove_variable(&typing, scope, m));
	assert(force && pg_evidence_classifier(force) == effectful);
	assert(!pg_prove_return_value(&typing, force));
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, scope, force))->core == effectful);
	const struct pg_object *x = pg_binder(graph), *k = pg_binder(graph);
	const struct pg_evidence *domain = pg_prove_projection(&typing, scope, universe);
	const struct pg_evidence *body_scope = pg_prove_context_extension(&typing, scope, x, domain);
	const struct pg_evidence *following_type = pg_prove_effect_type(&typing, &classifiers, only_c,
		pg_prove_projection(&typing, body_scope, universe));
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, body_scope, following_type);
	const struct pg_evidence *both = pg_prove_context_extension(&typing, scope, k,
		pg_prove_thunk_type(&typing, &classifiers, pi));
	const struct pg_evidence *source = pg_prove_projection(&typing, both, force);
	const struct pg_evidence *continuation = pg_prove_force(&typing, pg_prove_variable(&typing, both, k));
	const struct pg_evidence *fold = pg_prove_fold(&typing, &classifiers, source, continuation);
	const struct pg_effect_row *abc = pg_effect_union(graph, ab, only_c);
	assert(fold && pg_effect_type_view(pg_evidence_classifier(fold), &row, &value));
	assert(row == abc && value == u && pg_effect_count(row) == 3);
	assert(pg_prove_fold(&typing, &classifiers, source, continuation) == fold);
	const struct pg_evidence *recovered = pg_prove_classifier(&typing, &classifiers, both, fold);
	assert(recovered && pg_evidence_subject(recovered)->core == pg_evidence_classifier(fold));
	assert(!pg_prove_return_value(&typing, fold));
	assert(!pg_derivation_parameters(fold, &parameters));
	const struct pg_evidence *fold_premises[] = {source, continuation};
	assert(pg_prove_derivation(&typing, &classifiers, PG_FOLD_ELIM, &parameters, 2, fold_premises) == fold);
	const struct pg_object *ignored = pg_binder(graph);
	const struct pg_evidence *raw_scope = pg_prove_context_extension(&typing, both, ignored,
		pg_prove_projection(&typing, both, universe));
	const struct pg_evidence *raw_pi = pg_prove_pi(&typing, &classifiers, raw_scope, pg_prove_projection(&typing, raw_scope, pi));
	const struct pg_evidence *raw_continuation = pg_prove_lambda(&typing, raw_pi,
		pg_prove_projection(&typing, raw_scope, continuation));
	assert(raw_continuation && !pg_prove_fold(&typing, &classifiers, source, raw_continuation));
	const struct pg_evidence *target = pg_prove_effect_type(&typing, &classifiers, abc,
		pg_prove_projection(&typing, scope, universe));
	const struct pg_evidence *widened = pg_prove_effect_subsumption(&typing, force, target);
	assert(widened && pg_evidence_rule(widened) == PG_EFFECT_SUBSUMPTION);
	assert(pg_evidence_subject(widened) != pg_evidence_subject(force));
	assert(pg_evidence_subject(widened)->core == pg_evidence_subject(force)->core);
	assert(pg_evidence_classifier(force) == effectful);
	assert(pg_evidence_classifier(widened) == pg_evidence_subject(target)->core);
	assert(pg_prove_classifier(&typing, &classifiers, scope, widened) == target);
	assert(pg_prove_effect_subsumption(&typing, force, target) == widened);
	assert(!pg_prove_effect_subsumption(&typing, widened, pg_prove_projection(&typing, scope, formation)));
	assert(!pg_prove_effect_subsumption(&typing, force, formation));
	const struct pg_evidence *wrong_type = pg_prove_effect_type(&typing, &classifiers, abc,
		pg_prove_universe(&typing, &classifiers, scope, 1));
	assert(!pg_prove_effect_subsumption(&typing, force, wrong_type));
	assert(!pg_prove_return_value(&typing, widened));
	assert(!pg_derivation_parameters(widened, &parameters));
	const struct pg_evidence *widening_premises[] = {force, target};
	assert(pg_prove_derivation(&typing, &classifiers, PG_EFFECT_SUBSUMPTION, &parameters, 2, widening_premises) == widened);
	pg_typing_destroy(&typing);
	pg_classifiers_destroy(&classifiers);
	puts("effects: explicit closed sets, union laws, unknown is not empty, and pure-only views passed");
}

static void totality_classifier_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_typing_init(&typing, graph) && !pg_classifiers_init(&classifiers, graph));
	const struct pg_evidence *context = pg_prove_empty_context(&typing);
	const struct pg_evidence *a = pg_prove_universe(&typing, &classifiers, context, 1);
	const struct pg_evidence *v = pg_prove_type_value(&typing,
		pg_prove_universe(&typing, &classifiers, context, 0));
	const struct pg_effect_row *empty = pg_effect_row(graph, 0, NULL), *row;
	const struct pg_evidence *types[2], *returned[2], *functions[2];
	const struct pg_term *value;
	enum pg_totality grade;
	const struct pg_object *x = pg_binder(graph);
	const struct pg_evidence *scope = pg_prove_context_extension(&typing, context, x, a);
	for (unsigned i = 0; i < 2; ++i) {
		types[i] = pg_prove_computation_type(&typing, &classifiers, i, empty, a);
		returned[i] = pg_prove_return_contract(&typing, &classifiers, i, v);
		assert(types[i] && returned[i]);
		assert(pg_evidence_classifier(returned[i]) == pg_evidence_subject(types[i])->core);
		assert(pg_prove_return_contract(&typing, &classifiers, i, v) == returned[i]);
		assert(pg_prove_return_value(&typing, returned[i]) == v);
		assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, context, returned[i]))->core == pg_evidence_subject(types[i])->core);
		const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, scope,
			pg_prove_projection(&typing, scope, types[i]));
		functions[i] = pg_prove_lambda(&typing, pi, pg_prove_return_contract(&typing, &classifiers, i,
			pg_prove_variable(&typing, scope, x)));
		assert(functions[i]);
		const struct pg_evidence *applied = pg_prove_application(&typing, functions[i], v);
		assert(applied && pg_evidence_classifier(applied) == pg_evidence_subject(types[i])->core);
		const struct pg_evidence *force = pg_prove_force(&typing, pg_prove_thunk(&typing, &classifiers, applied));
		assert(force && pg_evidence_classifier(force) == pg_evidence_classifier(applied));
		struct pg_derivation_parameters parameters;
		assert(!pg_derivation_parameters(returned[i], &parameters) && parameters.totality == i);
		assert(pg_prove_derivation(&typing, &classifiers, PG_RETURN_INTRO, &parameters, 1, &v) == returned[i]);
		parameters.totality = 2;
		assert(!pg_prove_derivation(&typing, &classifiers, PG_RETURN_INTRO, &parameters, 1, &v));
	}
	assert(pg_evidence_subject(returned[0]) != pg_evidence_subject(returned[1]) && returned[0] != returned[1]);
	assert(pg_evidence_subject(returned[0])->core == pg_evidence_subject(returned[1])->core);
	assert(pg_alpha_equal(pg_evidence_classifier(returned[0]), pg_evidence_classifier(returned[1])) == 0);
	assert(!pg_effect_type_view(pg_evidence_subject(types[1])->core, &row, &value));
	assert(!pg_return_type_view(pg_evidence_subject(types[1])->core, &value));
	assert(!pg_computation_type(&classifiers, 2, empty, pg_evidence_subject(a)->core));
	assert(!pg_computation_type(&classifiers, (enum pg_totality)-1, empty, pg_evidence_subject(a)->core));
	assert(!pg_prove_effect_subsumption(&typing, returned[0], types[1]));
	const struct pg_evidence *weakened = pg_prove_effect_subsumption(&typing, returned[1], types[0]);
	/* Widening retains the total construction, rather than replacing its inputs
	 * with those of a separately constructed partial Return. */
	assert(weakened && pg_evidence_subject(weakened)->origin == pg_evidence_subject(returned[1]));
	assert(!pg_evidence_subject(weakened)->operand_count);
	assert(pg_evidence_classifier(weakened) == pg_evidence_classifier(returned[0]));
	assert(weakened != returned[0] && pg_evidence_premise(weakened, 0) == returned[1]);
	const struct pg_evidence *environment = NULL;
	size_t proof_count = typing.proofs.count, subject_count = typing.occurrences.count;
	assert(pg_prove_construction_origin(&typing, &classifiers, weakened, &environment) == returned[1]);
	assert(!environment && typing.proofs.count == proof_count && typing.occurrences.count == subject_count);
	reconstruct_derivation(&typing, &classifiers, weakened);
	const struct pg_evidence *pure_value = pg_prove_total_pure_value(&typing, returned[1]);
	assert(pure_value && pg_evidence_judgement(pure_value) == PG_JUDGEMENT_VALUE);
	assert(pg_prove_total_pure_value(&typing, returned[1]) == pure_value);
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, context, pure_value))->core == pg_evidence_subject(a)->core);
	assert(request_whnf(graph, pg_evidence_subject(pure_value)->core, 1) == pg_evidence_subject(v)->core);
	assert(!pg_prove_total_pure_value(&typing, weakened));
	assert(!pg_prove_total_pure_value(&typing, functions[1]));
	assert(!pg_prove_total_pure_value(&typing, v));
	assert(!pg_prove_total_pure_value(&typing, NULL));
	const struct pg_effect_row *nonempty = pg_effect_row(graph, 1, (const struct pg_object *[]){&pg_return_operation});
	const struct pg_evidence *effectful = pg_prove_effect_subsumption(&typing, returned[1],
		pg_prove_computation_type(&typing, &classifiers, PG_TOTALITY_TOTAL, nonempty, a));
	assert(effectful && !pg_prove_total_pure_value(&typing, effectful));
	reconstruct_derivation(&typing, &classifiers, pure_value);
	/* Formation is not a termination proof for an arbitrary suspended input.
	 * FOLD preserves exactly the contracts assumed in its typed premises. */
	for (unsigned i = 0; i < 2; ++i) {
		const struct pg_object *m = pg_binder(graph);
		const struct pg_evidence *outer = pg_prove_context_extension(&typing, context, m,
			pg_prove_thunk_type(&typing, &classifiers, types[i]));
		const struct pg_evidence *input = pg_prove_force(&typing, pg_prove_variable(&typing, outer, m));
		assert(input && !pg_prove_return_value(&typing, input));
		const struct pg_evidence *symbolic = pg_prove_total_pure_value(&typing, input);
		assert(!!symbolic == i);
		if (symbolic) {
			assert(pg_evidence_classifier(symbolic) == pg_evidence_subject(a)->core);
			const struct pg_evidence *substitution = pg_prove_substitution_pair(&typing,
				pg_prove_substitution_projection(&typing, context, context), outer,
				pg_prove_thunk(&typing, &classifiers, returned[i]));
			const struct pg_evidence *specialized = pg_prove_reindex(&typing, substitution, symbolic);
			const struct pg_evidence *mapped = pg_prove_total_pure_value(&typing,
				pg_prove_reindex(&typing, substitution, input));
			assert(specialized && mapped);
			assert(pg_alpha_equal(pg_evidence_subject(specialized)->core, pg_evidence_subject(mapped)->core) == 1);
			assert(request_whnf(graph, pg_evidence_subject(specialized)->core, 1) == pg_evidence_subject(v)->core);
			reconstruct_derivation(&typing, &classifiers, symbolic);
		}
		const struct pg_evidence *suspended = pg_prove_variable(&typing, outer, m);
		const struct pg_evidence *suspended_type = pg_prove_classifier(&typing, &classifiers, outer, suspended);
		const struct pg_evidence *termination = pg_prove_termination_type(&typing, &classifiers, suspended_type, suspended);
		assert(termination && pg_termination_type_view(pg_evidence_subject(termination)->core, &value));
		assert(value == pg_evidence_subject(suspended)->core);
		const struct pg_evidence *witness = pg_prove_termination(&typing, &classifiers, termination, suspended);
		assert(!!witness == i);
		struct pg_derivation_parameters parameters = {0};
		assert(pg_prove_derivation(&typing, &classifiers, PG_TERMINATION_INTRO, &parameters, 2,
			(const struct pg_evidence *[]){termination, suspended}) == witness);
		if (witness) {
			assert(pg_evidence_classifier(witness) == pg_evidence_subject(termination)->core);
			assert(pg_prove_classifier(&typing, &classifiers, outer, witness) == termination);
			struct pg_derivation_parameters parameters;
			assert(!pg_derivation_parameters(witness, &parameters));
			assert(pg_prove_derivation(&typing, &classifiers, PG_TERMINATION_INTRO, &parameters, 2,
				(const struct pg_evidence *[]){termination, suspended}) == witness);
		}
		assert(!pg_prove_termination_type(&typing, &classifiers, a, v));
		assert(!pg_prove_termination_type(&typing, &classifiers, suspended_type, v));
		assert(!pg_prove_termination(&typing, &classifiers, termination, v));
		const struct pg_evidence *other = pg_prove_thunk(&typing, &classifiers,
			pg_prove_projection(&typing, outer, returned[1]));
		assert(!pg_prove_termination(&typing, &classifiers, termination, other));
		const struct pg_evidence *raw = pg_prove_thunk(&typing, &classifiers,
			pg_prove_projection(&typing, outer, functions[1]));
		const struct pg_evidence *raw_type = pg_prove_classifier(&typing, &classifiers, outer, raw);
		const struct pg_evidence *raw_termination = pg_prove_termination_type(&typing, &classifiers, raw_type, raw);
		assert(raw_termination && !pg_prove_termination(&typing, &classifiers, raw_termination, raw));
		const struct pg_evidence *returned_thunk = pg_prove_return_contract(&typing, &classifiers, PG_TOTALITY_TOTAL, suspended);
		assert(returned_thunk);
		/* Termination of RETURN(thunk M) does not establish termination of M. */
		const struct pg_evidence *outer_thunk = pg_prove_thunk(&typing, &classifiers, returned_thunk);
		const struct pg_evidence *outer_type = pg_prove_classifier(&typing, &classifiers, outer, outer_thunk);
		assert(pg_prove_termination(&typing, &classifiers,
			pg_prove_termination_type(&typing, &classifiers, outer_type, outer_thunk), outer_thunk));
		assert(pg_evidence_classifier(pg_prove_force(&typing, pg_prove_return_value(&typing, returned_thunk))) == pg_evidence_classifier(input));
		for (unsigned j = 0; j < 2; ++j) {
			const struct pg_evidence *continuation = pg_prove_projection(&typing, outer, functions[j]);
			const struct pg_evidence *fold = pg_prove_fold(&typing, &classifiers, input, continuation);
			assert(fold && pg_computation_type_view(pg_evidence_classifier(fold), &grade, &row, &value));
			assert(grade == (i && j) && row == empty && value == pg_evidence_subject(a)->core);
			assert(pg_prove_fold(&typing, &classifiers, input, continuation) == fold);
			/* The result projection has its own typed domain. An ignored
			 * neutral prefix still blocks ordinary Fold, even when TOTAL. */
			const struct pg_evidence *constant_body = pg_prove_return_contract(&typing, &classifiers, j,
				pg_prove_projection(&typing, scope, v));
			const struct pg_evidence *constant = pg_prove_lambda(&typing,
				pg_evidence_premise(functions[j], 0), constant_body);
			constant = pg_prove_projection(&typing, outer, constant);
			const struct pg_evidence *constant_fold = pg_prove_fold(&typing, &classifiers, input, constant);
			struct pg_typed_query *prefix = pg_return_body_request(&typing, constant_fold);
			while (!pg_typed_query_advance(prefix, 1)) {}
			assert(!pg_typed_query_result(prefix));
			const struct pg_evidence *result = pg_prove_total_pure_value(&typing, constant_fold);
			assert(constant_fold && !!result == (i && j));
			assert(request_whnf(graph, pg_evidence_subject(constant_fold)->core, 1)
				!= pg_evidence_subject(returned[j])->core);
			if (result) {
				assert(request_whnf(graph, pg_evidence_subject(result)->core, 1) == pg_evidence_subject(v)->core);
				reconstruct_derivation(&typing, &classifiers, result);
				const struct pg_evidence *applied = pg_prove_application(&typing, constant, symbolic);
				assert(applied);
				const struct pg_evidence *right = pg_prove_total_pure_value(&typing, applied);
				assert(right && request_whnf(graph, pg_evidence_subject(right)->core, 64) == pg_evidence_subject(v)->core);
			}
			assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, outer, fold))->core == pg_evidence_classifier(fold));
			const struct pg_object *ignored = pg_binder(graph);
			const struct pg_evidence *inner = pg_prove_context_extension(&typing, outer, ignored,
				pg_prove_projection(&typing, outer, a));
			const struct pg_evidence *raw_body = pg_prove_projection(&typing, inner, functions[j]);
			const struct pg_evidence *raw_type = pg_prove_pi(&typing, &classifiers, inner,
				pg_prove_classifier(&typing, &classifiers, inner, raw_body));
			const struct pg_evidence *raw = pg_prove_lambda(&typing, raw_type, raw_body);
			assert(raw);
			const struct pg_evidence *raw_fold = pg_prove_fold(&typing, &classifiers, input, raw);
			assert(!!raw_fold == (i || !j));
			if (raw_fold) assert(!pg_prove_application_body(&typing, raw_fold,
				pg_prove_projection(&typing, outer, v)));
			/* A literal RETURN is finite even when its declared guarantee was
			 * weakened. This uses typed inversion, not WHNF or an empty row. */
			assert(pg_prove_fold(&typing, &classifiers,
				pg_prove_projection(&typing, outer, weakened), raw));
		}
	}
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	puts("totality: distinct contracts, RETURN, directed weakening, APP/FORCE and Fold bounds passed");
}

static void deep_classifier_test(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph));
	assert(!pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *value = pg_prove_type_value(&typing, universe);
	for (size_t i = 0; i < 20000; ++i) {
		value = pg_prove_thunk(&typing, &classifiers,
			pg_prove_return(&typing, &classifiers, value));
		assert(value);
	}
	const struct pg_evidence *formation = pg_prove_classifier(&typing, &classifiers, empty, value);
	assert(formation && pg_evidence_judgement(formation) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(formation)->core == pg_evidence_classifier(value));
	size_t proofs = typing.proofs.count, terms = graph.terms.count;
	assert(pg_prove_classifier(&typing, &classifiers, empty, value) == formation);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	assert(!pg_prove_classifier(&typing, NULL, empty, value));
	for (size_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_classifier_recovery work;
		assert(!pg_classifier_recovery_init(&work, &typing, &classifiers, empty, value));
		assert(!pg_classifier_recovery_advance(&work, 0));
		assert(!work.result);
		size_t calls = 0;
		while (!pg_classifier_recovery_advance(&work, chunk)) assert(++calls < 100000);
		assert(work.status == 1 && work.result == formation);
		assert(typing.proofs.count == proofs && graph.terms.count == terms);
		pg_classifier_recovery_destroy(&work);
	}
	struct pg_classifier_recovery cancelled;
	assert(!pg_classifier_recovery_init(&cancelled, &typing, &classifiers, empty, value));
	assert(!pg_classifier_recovery_advance(&cancelled, 0));
	pg_classifier_recovery_destroy(&cancelled);
	assert(pg_classifier_recovery_init(&cancelled, &typing, NULL, empty, value) == -1);
	assert(pg_classifier_recovery_advance(&cancelled, 10) == -1);
	pg_classifier_recovery_destroy(&cancelled);
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, empty, binder, universe);
	for (size_t i = 0; i < 64; ++i)
		context = pg_prove_context_extension(&typing, context, pg_binder(&graph), pg_prove_projection(&typing, context, universe));
	const struct pg_evidence *variable = pg_prove_variable(&typing, context, binder);
	assert(!pg_classifier_recovery_init(&cancelled, &typing, &classifiers, context, variable));
	assert(!pg_classifier_recovery_advance(&cancelled, 0));
	assert(!cancelled.result);
	assert(pg_classifier_recovery_advance(&cancelled, 1) == 1);
	assert(pg_evidence_subject(cancelled.result)->core == pg_evidence_subject(universe)->core);
	assert(pg_evidence_context(cancelled.result) == pg_evidence_context(context));
	assert(pg_evidence_judgement(cancelled.result) == PG_JUDGEMENT_VALUE_TYPE);
	pg_classifier_recovery_destroy(&cancelled);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("classifier: 40000 nested constructors, direct formation reuse, no proof-history traversal");
}

static const struct pg_evidence *resume_clause(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_operation_declaration *operation, const struct pg_evidence *carrier, const struct pg_operation_declaration *emit)
{
	const struct pg_object *a = pg_binder(typing->graph), *k = pg_binder(typing->graph);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *extended = pg_prove_handler_context(typing, classifiers, operation, empty, carrier, a, k);
	assert(extended);
	const struct pg_evidence *resume = pg_prove_force(typing, pg_prove_variable(typing, extended, k));
	const struct pg_evidence *argument = pg_prove_variable(typing, extended, a);
	const struct pg_evidence *body = emit ? pg_prove_request(typing, classifiers, emit, argument, resume)
		: pg_prove_application(typing, resume, argument);
	return pg_prove_abstract(typing, classifiers, empty, extended, body);
}

static void request_typing_test(struct pg_graph *graph)
{
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_typing_init(&typing, graph) && !pg_classifiers_init(&classifiers, graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *u1 = pg_prove_universe(&typing, &classifiers, empty, 1);
	const struct pg_operation_declaration *op = pg_operation_declaration(&typing, u1, u1);
	const struct pg_operation_declaration *other = pg_operation_declaration(&typing, u1, u1);
	assert(op && other && pg_operation_label(op) != pg_operation_label(other));
	size_t proofs_before = typing.proofs.count;
	const struct pg_term *u1_core = pg_evidence_subject(u1)->core, *raw_payload, *raw_response;
	const struct pg_object *raw_label = pg_operation_label_create(graph, u1_core, u1_core);
	assert(raw_label && raw_label != pg_operation_label(op) && typing.proofs.count == proofs_before);
	assert(pg_operation_label_types(raw_label, &raw_payload, &raw_response));
	assert(raw_payload == u1_core && raw_response == u1_core);
	assert(!pg_operation_label_types(pg_binder(graph), &raw_payload, &raw_response));
	const struct pg_operation_declaration *checked = pg_operation_declaration_at(&typing, raw_label, u1, u1);
	assert(checked && pg_operation_label(checked) == raw_label);
	assert(checked == pg_operation_declaration_at(&typing, raw_label, u1, u1));
	assert(op == pg_operation_declaration_at(&typing, pg_operation_label(op), u1, u1));
	assert(!pg_operation_declaration_at(&typing, raw_label, u0, u1));
	assert(!pg_operation_declaration_at(&typing, raw_label, u1, u0));
	struct pg_typing local;
	assert(!pg_typing_init(&local, graph));
	assert(!pg_operation_declaration_at(&local, raw_label, u1, u1));
	const struct pg_evidence *local_u1 = pg_prove_universe(&local, &classifiers, pg_prove_empty_context(&local), 1);
	const struct pg_operation_declaration *local_declaration = pg_operation_declaration_at(&local, raw_label, local_u1, local_u1);
	assert(local_declaration && local_declaration != checked && pg_operation_label(local_declaration) == raw_label);
	assert(!pg_operation_declaration_at(&typing, raw_label, local_u1, local_u1));
	pg_typing_destroy(&local);
	const struct pg_object *x = pg_binder(graph);
	const struct pg_evidence *scope = pg_prove_context_extension(&typing, empty, x, u1);
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, scope, x));
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, scope,
		pg_prove_classifier(&typing, &classifiers, scope, body));
	const struct pg_evidence *k = pg_prove_lambda(&typing, pi, body);
	const struct pg_evidence *payload = pg_prove_type_value(&typing, u0);
	for (enum pg_totality grade = PG_TOTALITY_UNSPECIFIED; grade <= PG_TOTALITY_TOTAL; ++grade) {
		const struct pg_evidence *ret = pg_prove_return_contract(&typing, &classifiers, grade, pg_prove_variable(&typing, scope, x));
		const struct pg_evidence *cont = pg_prove_lambda(&typing,
			pg_prove_pi(&typing, &classifiers, scope, pg_prove_classifier(&typing, &classifiers, scope, ret)), ret);
		const struct pg_evidence *req = pg_prove_request(&typing, &classifiers, op, payload, cont);
		enum pg_totality actual;
		const struct pg_effect_row *effects;
		const struct pg_term *value;
		assert(req && pg_computation_type_view(pg_evidence_classifier(req), &actual, &effects, &value));
		assert(actual == grade && pg_effect_count(effects) == 1);
		assert(pg_effect_contains(effects, pg_operation_label(op)) == 1);
		const struct pg_evidence *strong_return = pg_prove_return_contract(&typing, &classifiers,
			PG_TOTALITY_TOTAL, pg_prove_variable(&typing, scope, x));
		const struct pg_evidence *strong_cont = pg_prove_lambda(&typing,
			pg_prove_pi(&typing, &classifiers, scope,
				pg_prove_classifier(&typing, &classifiers, scope, strong_return)), strong_return);
		for (enum pg_totality target = PG_TOTALITY_UNSPECIFIED; target <= PG_TOTALITY_TOTAL; ++target) {
			const struct pg_evidence *carrier = pg_prove_computation_type(&typing, &classifiers, target,
				pg_effect_row(graph, 0, NULL), u1);
			struct pg_handler_clause clause = {op, resume_clause(&typing, &classifiers, op, carrier, NULL)};
			/* A stronger return clause does not establish termination of the input. */
			assert((pg_prove_handler(&typing, &classifiers, req, strong_cont, carrier, 1, &clause) != NULL)
				== (grade >= target));
			const struct pg_evidence *handled = pg_prove_handler(&typing, &classifiers, req, cont, carrier, 1, &clause);
			assert((handled != NULL) == (grade >= target));
			if (!handled) continue;
			assert(pg_prove_classifier(&typing, &classifiers, empty, handled) == carrier);
			struct pg_derivation_parameters parameters;
			assert(!pg_derivation_parameters(handled, &parameters));
			const struct pg_evidence *premises[6];
			for (size_t i = 0; i < 6; ++i) premises[i] = pg_evidence_premise(handled, i);
			assert(pg_prove_derivation(&typing, &classifiers, PG_HANDLER_ELIM, &parameters, 6, premises) == handled);
		}
	}
	const struct pg_evidence *request = pg_prove_request(&typing, &classifiers, op, payload, k);
	assert(request && pg_evidence_rule(request) == PG_REQUEST_INTRO);
	assert(pg_prove_request(&typing, &classifiers, op, payload, k) == request);
	const struct pg_object *label;
	const struct pg_term *a, *continuation;
	assert(pg_computation_request_view(pg_evidence_subject(request)->core, &label, &a, &continuation));
	assert(label == pg_operation_label(op) && a == pg_evidence_subject(payload)->core);
	assert(continuation == pg_evidence_subject(k)->core);
	const struct pg_effect_row *row;
	const struct pg_term *result_type;
	assert(pg_effect_type_view(pg_evidence_classifier(request), &row, &result_type));
	assert(pg_effect_count(row) == 1 && pg_effect_contains(row, label) == 1);
	assert(result_type == pg_evidence_subject(u1)->core);
	const struct pg_evidence *formation = pg_prove_classifier(&typing, &classifiers, empty, request);
	assert(formation && pg_evidence_subject(formation)->core == pg_evidence_classifier(request));
	assert(!pg_prove_return_value(&typing, request));
	struct pg_derivation_parameters parameters;
	assert(!pg_derivation_parameters(request, &parameters));
	assert(parameters.operation_label == pg_operation_label(op) && pg_evidence_request_declaration(request) == op);
	assert(!pg_evidence_request_declaration(payload));
	const struct pg_evidence *request_premises[] = {
		pg_operation_payload_type(op), pg_operation_response_type(op), payload, k
	};
	assert(pg_prove_derivation(&typing, &classifiers, PG_REQUEST_INTRO, &parameters, 4, request_premises) == request);
	assert(!pg_prove_derivation(&typing, &classifiers, PG_REQUEST_INTRO, &parameters, 3, request_premises));
	request_premises[0] = u0;
	assert(!pg_prove_derivation(&typing, &classifiers, PG_REQUEST_INTRO, &parameters, 4, request_premises));
	request_premises[0] = pg_operation_payload_type(op);
	parameters.operation_label = pg_operation_label(other);
	const struct pg_evidence *other_request = pg_prove_derivation(&typing, &classifiers, PG_REQUEST_INTRO, &parameters, 4, request_premises);
	assert(other_request && other_request != request && pg_evidence_request_declaration(other_request) == other);
	assert(!pg_prove_request(&typing, &classifiers, op, pg_prove_type_value(&typing, u1), k));
	const struct pg_operation_declaration *wrong_response = pg_operation_declaration(&typing, u1, u0);
	assert(wrong_response && !pg_prove_request(&typing, &classifiers, wrong_response, payload, k));
	assert(!pg_operation_declaration(&typing, pg_prove_projection(&typing, scope, u1), u1));
	assert(!pg_prove_request(&typing, &classifiers, op, payload, pg_prove_thunk(&typing, &classifiers, k)));
	const struct pg_object *other_label = pg_operation_label(other);
	const struct pg_evidence *effect_type = pg_prove_effect_type(&typing, &classifiers,
		pg_effect_row(graph, 1, &other_label), pg_prove_projection(&typing, scope, u1));
	const struct pg_evidence *effect_pi = pg_prove_pi(&typing, &classifiers, scope, effect_type);
	const struct pg_object *f = pg_binder(graph);
	const struct pg_evidence *function_scope = pg_prove_context_extension(&typing, empty, f,
		pg_prove_thunk_type(&typing, &classifiers, effect_pi));
	const struct pg_evidence *effect_k = pg_prove_force(&typing, pg_prove_variable(&typing, function_scope, f));
	const struct pg_evidence *chained = pg_prove_request(&typing, &classifiers, op,
		pg_prove_projection(&typing, function_scope, payload), effect_k);
	assert(chained && pg_effect_type_view(pg_evidence_classifier(chained), &row, &result_type));
	assert(pg_effect_count(row) == 2 && pg_effect_contains(row, label) == 1 && pg_effect_contains(row, other_label) == 1);
	formation = pg_prove_classifier(&typing, &classifiers, function_scope, chained);
	assert(formation && pg_evidence_subject(formation)->core == pg_evidence_classifier(chained));
	struct pg_whnf_work work;
	assert(!pg_whnf_work_init(&work, graph));
	const struct pg_evidence *normal = checked_normalize(&typing, &work, request);
	assert(normal && pg_computation_request_view(pg_evidence_subject(normal)->core, &label, &a, &continuation));
	assert(label == pg_operation_label(op));
	const struct pg_evidence *function = pg_prove_operation_function(&typing, &classifiers, op);
	assert(function && pg_evidence_rule(function) == PG_LAMBDA_INTRO);
	const struct pg_evidence *application = pg_prove_application(&typing, function, payload);
	enum pg_totality function_totality;
	assert(application && pg_computation_type_view(pg_evidence_classifier(application), &function_totality, &row, &result_type));
	assert(function_totality == PG_TOTALITY_TOTAL);
	assert(pg_effect_count(row) == 1 && pg_effect_contains(row, pg_operation_label(op)) == 1);
	normal = checked_normalize(&typing, &work, application);
	assert(normal && pg_computation_request_view(pg_evidence_subject(normal)->core, &label, &a, &continuation));
	assert(label == pg_operation_label(op) && a == pg_evidence_subject(payload)->core);
	const struct pg_evidence *carrier = pg_prove_return_type(&typing, &classifiers, u1);
	const struct pg_object *payload_binder = pg_binder(graph), *resume_binder = pg_binder(graph);
	assert(!pg_prove_handler_context(&typing, &classifiers, op, empty, u1, payload_binder, resume_binder));
	assert(!pg_prove_handler_context(&typing, &classifiers, op, scope, carrier, payload_binder, resume_binder));
	assert(!pg_prove_handler_context(&typing, &classifiers, op, empty, carrier, payload_binder, payload_binder));
	const struct pg_evidence *open_carrier = pg_prove_projection(&typing, function_scope, carrier);
	const struct pg_evidence *open_clause = pg_prove_handler_context(&typing, &classifiers, op,
		function_scope, open_carrier, payload_binder, resume_binder);
	assert(open_clause && pg_evidence_context(open_clause)->parent->parent == pg_evidence_context(function_scope));
	const struct pg_evidence *clause = resume_clause(&typing, &classifiers, op, carrier, NULL);
	assert(clause);
	const struct pg_evidence *second_request = pg_prove_request(&typing, &classifiers, other,
		pg_prove_variable(&typing, scope, x), pg_prove_projection(&typing, scope, k));
	const struct pg_evidence *second_continuation = pg_prove_lambda(&typing,
		pg_prove_pi(&typing, &classifiers, scope, pg_prove_classifier(&typing, &classifiers, scope, second_request)), second_request);
	const struct pg_evidence *two = pg_prove_request(&typing, &classifiers, op, payload, second_continuation);
	struct pg_handler_clause clauses[] = {{op, clause}, {other, clause}};
	const struct pg_evidence *handled = pg_prove_handler(&typing, &classifiers, two, k, carrier, 2, clauses);
	assert(handled && pg_evidence_rule(handled) == PG_HANDLER_ELIM);
	assert(pg_evidence_classifier(handled) == pg_evidence_subject(carrier)->core);
	assert(pg_prove_classifier(&typing, &classifiers, empty, handled) == carrier);
	assert(pg_prove_handler(&typing, &classifiers, two, k, carrier, 2, clauses) == handled);
	const struct pg_handler_signature *signature = pg_evidence_handler_signature(handled);
	assert(signature && pg_handler_signature_count(signature) == 2);
	assert(pg_handler_signature_label(signature, 0) == pg_operation_label(op));
	assert(pg_handler_signature_label(signature, 1) == pg_operation_label(other));
	assert(!pg_handler_signature_label(signature, 2));
	assert(signature == pg_handler_signature(graph, 2,
		(const struct pg_object *[]){pg_operation_label(op), pg_operation_label(other)}));
	struct pg_derivation_parameters handler_parameters;
	assert(!pg_derivation_parameters(handled, &handler_parameters) && handler_parameters.handler == signature);
	const struct pg_evidence *handler_premises[9];
	for (size_t i = 0; i < 9; ++i) handler_premises[i] = pg_evidence_premise(handled, i);
	assert(pg_prove_derivation(&typing, &classifiers, PG_HANDLER_ELIM, &handler_parameters, 9, handler_premises) == handled);
	assert(!pg_prove_derivation(&typing, &classifiers, PG_HANDLER_ELIM, &handler_parameters, 8, handler_premises));
	handler_premises[3] = u0;
	assert(!pg_prove_derivation(&typing, &classifiers, PG_HANDLER_ELIM, &handler_parameters, 9, handler_premises));
	handler_premises[3] = pg_evidence_premise(handled, 3);
	normal = checked_normalize(&typing, &work, handled);
	const struct pg_evidence *answer = pg_prove_return_value(&typing, normal);
	assert(answer && pg_evidence_subject(answer)->core == pg_evidence_subject(payload)->core);
	{
		/* A receipt can expose RETURN while the checked input query does not
		 * yet support the source handler. Graph generation must not dereference
		 * a missing child; checked RETURN inversion can still form its leaf. */
		struct pg_typed_query *input = pg_typed_input_request(&typing, normal, 0);
		while (!pg_typed_query_advance(input, 1)) assert(pg_typed_query_steps(input) < 10000);
		assert(!pg_typed_query_result(input));
		const struct pg_evidence *body = pg_prove_projection(&typing, scope, normal);
		const struct pg_evidence *function = pg_prove_lambda(&typing,
			pg_prove_pi(&typing, &classifiers, scope,
				pg_prove_classifier(&typing, &classifiers, scope, body)), body);
		struct pg_function_graph_work generated;
		assert(function && !pg_function_graph_init(&generated, &typing, &classifiers, &work, function));
		enum pg_function_graph_status status;
		size_t steps = 0;
		do {
			status = pg_function_graph_advance(&generated, 1);
			assert(++steps < 10000);
		} while (status == PG_FUNCTION_GRAPH_PENDING);
		assert(status == PG_FUNCTION_GRAPH_DONE && pg_function_graph_formation(&generated));
		do {
			status = pg_function_graph_witness_advance(&generated, 1);
			assert(++steps < 10000);
		} while (status == PG_FUNCTION_GRAPH_PENDING);
		assert(status == PG_FUNCTION_GRAPH_DONE);
		const struct pg_evidence *call = pg_prove_application(&typing, pg_function_graph_witness(&generated), payload);
		const struct pg_evidence *packet = pg_prove_return_value(&typing, checked_normalize(&typing, &work, call));
		assert(packet);
		const struct pg_term *pair = pg_evidence_subject(packet)->core;
		assert(pair->kind == PG_APPLICATION && pair->as.application.function->kind == PG_APPLICATION);
		assert(pair->as.application.function->as.application.argument == pg_evidence_subject(payload)->core);
		pg_function_graph_destroy(&generated);
	}
	assert(!pg_prove_handler(&typing, &classifiers, two, k, carrier, 1, clauses));
	clauses[1] = clauses[0];
	assert(!pg_prove_handler(&typing, &classifiers, two, k, carrier, 2, clauses));
	clauses[0].operation = other; clauses[1].operation = op;
	const struct pg_evidence *reordered = pg_prove_handler(&typing, &classifiers, two, k, carrier, 2, clauses);
	assert(reordered);
	assert(pg_evidence_handler_signature(reordered) != signature);
	handler_parameters.handler = pg_evidence_handler_signature(reordered);
	assert(pg_prove_derivation(&typing, &classifiers, PG_HANDLER_ELIM, &handler_parameters, 9, handler_premises) == reordered);
	normal = checked_normalize(&typing, &work, reordered);
	assert(pg_evidence_subject(pg_prove_return_value(&typing, normal))->core == pg_evidence_subject(payload)->core);
	const struct pg_evidence *forward_carrier = pg_prove_effect_type(&typing, &classifiers,
		pg_effect_row(graph, 1, &other_label), u1);
	struct pg_handler_clause forward = {op, resume_clause(&typing, &classifiers, op, forward_carrier, NULL)};
	const struct pg_evidence *forwarded = pg_prove_handler(&typing, &classifiers, two, k, forward_carrier, 1, &forward);
	assert(forwarded);
	normal = checked_normalize(&typing, &work, forwarded);
	assert(pg_computation_request_view(pg_evidence_subject(normal)->core, &label, &a, &continuation));
	assert(label == other_label);
	forward.body = clause;
	assert(!pg_prove_handler(&typing, &classifiers, two, k, forward_carrier, 1, &forward));
	assert(!pg_prove_handler(&typing, &classifiers, two, second_continuation, carrier, 2, clauses));
	struct pg_handler_clause extra = {op, resume_clause(&typing, &classifiers, op, carrier, other)};
	assert(extra.body && !pg_prove_handler(&typing, &classifiers, request, k, carrier, 1, &extra));
	const struct pg_evidence *pure_input = pg_prove_return(&typing, &classifiers, payload);
	assert(pg_prove_handler(&typing, &classifiers, pure_input, k, carrier, 0, NULL));
	const struct pg_object *both_labels[] = {pg_operation_label(op), other_label};
	const struct pg_evidence *swap_carrier = pg_prove_effect_type(&typing, &classifiers,
		pg_effect_row(graph, 2, both_labels), u1);
	struct pg_handler_clause swaps[] = {
		{op, resume_clause(&typing, &classifiers, op, swap_carrier, other)},
		{other, resume_clause(&typing, &classifiers, other, swap_carrier, op)}
	};
	const struct pg_evidence *swapped = pg_prove_handler(&typing, &classifiers, two, k, swap_carrier, 2, swaps);
	assert(swapped);
	normal = checked_normalize(&typing, &work, swapped);
	assert(pg_computation_request_view(pg_evidence_subject(normal)->core, &label, &a, &continuation));
	assert(label == other_label);
	pg_whnf_work_destroy(&work);
	struct pg_typing separate;
	assert(!pg_typing_init(&separate, graph));
	assert(!pg_prove_request(&separate, &classifiers, op, payload, k));
	pg_typing_destroy(&separate);
	pg_typing_destroy(&typing);
	pg_classifiers_destroy(&classifiers);
	puts("typed effects: signatures, multi-clause handlers, deep resumption, forwarding and effect bounds passed");
}

static void continuation_names(void)
{
	const char *names[] = {
		"computation/force_answer/v1", "computation/fold_answer/v2",
		"iadt/match_answer/v1", "iadt/action_answer/v1",
		"symmetry/symmetry_answer/v1", "identity/right_endpoint/v1",
		"identity/left_endpoint/v1", "identity/action_body/v1",
		"identity/action_source/v1", "identity/thunk_return_field/v1",
		"identity/field_answer/v1"
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
		const struct pg_eval_continuation *entry = pg_computation_continuation_resolve(names[i]);
		assert(entry && entry->resume && !strcmp(entry->name, names[i]));
		assert(pg_computation_continuation_resolve(entry->name) == entry);
		for (size_t j = 0; j < i; ++j)
			assert(pg_computation_continuation_resolve(names[j]) != entry);
	}
	assert(!pg_computation_continuation_resolve(NULL));
	assert(!pg_computation_continuation_resolve("identity/action_body/v2"));
	assert(!pg_computation_continuation_resolve("tests/core/demand_answer/v1"));
	assert(!pg_computation_continuation_resolve("computation/fold_answer/v1"));
	assert(!pg_identity_continuation_resolve("computation/fold_answer/v2"));
	puts("continuation owners: versioned identities resolve only existing algorithms");
}

int main(void)
{
	continuation_names();
	deep_classifier_test();
	dag_test();
	index_distribution_test();
	struct pg_graph graph;
	assert(pg_graph_init(&graph) == 0);
	graph_test(&graph);
	context_test(&graph);
	evidence_test(&graph);
	evidence_owner_test(&graph);
	dependent_application_test(&graph);
	typed_substitution_test(&graph);
	family_instance_test(&graph);
	typed_restriction_test(&graph);
	computation_execution_test(&graph);
	request_forwarding_test(&graph);
	fold_association_test(&graph);
	demand_budget_test(&graph);
	auxiliary_demand_test(&graph);
	deferred_work_test(&graph);
	classifiers_test(&graph);
	effect_classifier_test(&graph);
	totality_classifier_test(&graph);
	request_typing_test(&graph);
	restriction_test(&graph);
	conversion_test(&graph);
	reduction_congruence_test(&graph);
	normal_form_test(&graph);
	beta_work_test(&graph);
	substitution_test(&graph);
	shared_substitution_test(&graph);
	evaluation_test(&graph);
	dimension_test(&graph);
	printf("graph: %zu terms; pointer-key interning and separate alpha comparison passed\n", graph.terms.count);
	pg_graph_destroy(&graph);
	return 0;
}
