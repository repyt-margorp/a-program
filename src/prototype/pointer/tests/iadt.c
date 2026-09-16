#include "iadt.h"
#include "computation.h"
#include "conversion.h"
#include "identity.h"
#include "action.h"
#include "derivation.h"
#include "synthesis.h"

#include <assert.h>
#include <stdio.h>

static void common_rule(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *proof)
{
	struct pg_derivation_input input;
	assert(!pg_derivation_input_header(proof, &input));
	const struct pg_evidence **premises = pg_alloc(typing->graph, input.count * sizeof(*premises));
	assert(premises && input.count);
	for (size_t i = 0; i < input.count; ++i) premises[i] = pg_evidence_premise(proof, i);
	size_t proofs = typing->proofs.count, terms = typing->graph->terms.count;
	assert(pg_prove_derivation(typing, classifiers, input.rule, &input.parameters, input.count, premises) == proof);
	assert(typing->proofs.count == proofs && typing->graph->terms.count == terms);
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, typing->graph));
		assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job **jobs = pg_alloc(typing->graph, input.count * sizeof(*jobs));
		assert(jobs);
		for (size_t i = 0; i < input.count; ++i) jobs[i] = pg_synthesis_evidence(&synthesis, premises[i]);
		struct pg_synthesis_job *job = pg_synthesis_rule(&synthesis, &input, jobs, NULL, NULL);
		assert(job && job == pg_synthesis_rule(&synthesis, &input, jobs, NULL, NULL));
		while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
			assert(synthesis.steps < 10000);
			pg_synthesis_advance(&synthesis, chunk);
		}
		assert(pg_synthesis_result(job) == proof && typing->proofs.count == proofs);
		if (input.parameters.induction) {
			struct pg_induction_allocation conflict = *input.parameters.induction;
			conflict.argument = conflict.recursion;
			struct pg_derivation_input invalid = input;
			invalid.parameters.induction = &conflict;
			struct pg_synthesis_job *rejected = pg_synthesis_rule(&synthesis, &invalid, jobs, NULL, NULL);
			assert(rejected && rejected != job);
			while (pg_synthesis_status(rejected) == PG_SYNTHESIS_PENDING) {
				assert(synthesis.steps < 10000);
				pg_synthesis_advance(&synthesis, chunk);
			}
			assert(!pg_synthesis_result(rejected));
			assert(pg_synthesis_result(job) == proof);
		}
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	assert(!pg_prove_derivation(typing, classifiers, input.rule, &input.parameters, input.count - 1, premises));
	if (input.count > 1) assert(premises[input.count - 1] != premises[0]);
	premises[input.count - 1] = input.count > 1 ? premises[0] : NULL;
	assert(!pg_prove_derivation(typing, classifiers, input.rule, &input.parameters, input.count, premises));
}

static void positive_fields(void)
{
	struct pg_graph graph;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_classifiers_init(&classifiers, &graph));
	const struct pg_object *self = pg_binder(&graph), *x = pg_binder(&graph);
	const struct pg_term *recursive = pg_reference(&graph, self);
	const struct pg_term *index = pg_reference(&graph, x);
	const struct pg_term *a = pg_universe(&classifiers, 0);
	const struct pg_term *fiber = pg_application(&graph, recursive, index);
	assert(pg_data_field_positive(recursive, self, 0) == 1);
	assert(pg_data_field_positive(fiber, self, 1) == 1);
	assert(pg_data_field_positive(fiber, self, 0) == 0);
	assert(pg_data_field_positive(recursive, self, 1) == 0);
	assert(pg_data_field_positive(pg_application(&graph, recursive, recursive), self, 1) == 0);
	/* Acc-shaped recursion: independent quantified inputs, recursive output. */
	const struct pg_term *positive = pg_thunk_type(&classifiers,
		pg_pi(&graph, a, x, pg_return_type(&classifiers, fiber)));
	assert(pg_data_field_positive(positive, self, 1) == 1);
	assert(pg_data_recursive_field(recursive, self) == 1);
	assert(pg_data_recursive_field(positive, self) == 1);
	assert(pg_data_recursive_field(pg_thunk_type(&classifiers, pg_return_type(&classifiers, fiber)), self) == 1);
	assert(pg_data_recursive_field(a, self) == 0);
	static const struct pg_object_class label_class = {"test-effect"};
	static const struct pg_object label = {PG_SEMANTIC_OBJECT, &label_class};
	const struct pg_object *op = &label;
	const struct pg_effect_row *effects = pg_effect_row(&graph, 1, &op);
	assert(effects);
	assert(pg_data_recursive_field(pg_thunk_type(&classifiers,
		pg_pi(&graph, a, x, pg_effect_type(&classifiers, effects, fiber))), self) == -1);
	const struct pg_term *negative = pg_thunk_type(&classifiers,
		pg_pi(&graph, recursive, x, pg_return_type(&classifiers, a)));
	assert(pg_data_field_positive(negative, self, 0) == 0);
	assert(pg_data_recursive_field(negative, self) == -1);
	assert(pg_data_recursive_field(pg_application(&graph, recursive, recursive), self) == -1);
	/* Double negation is not strict positivity. */
	assert(pg_data_field_positive(pg_thunk_type(&classifiers,
		pg_pi(&graph, negative, x, pg_return_type(&classifiers, a))), self, 0) == 0);
	/* Unknown type constructors do not acquire an assumed variance. */
	assert(pg_data_field_positive(pg_application(&graph, index, recursive), self, 0) == 0);
	assert(pg_data_field_positive(pg_application(&graph, index, a), self, 0) == 1);
	assert(pg_data_field_positive(pg_lambda(&graph, self, recursive), self, 0) == 1);
	assert(pg_data_field_positive(pg_pi(&graph, a, self, recursive), self, 0) == 1);
	const struct pg_term *redex = pg_application(&graph, pg_lambda(&graph, x, a), recursive);
	assert(pg_data_field_positive(redex, self, 0) == 0);
	assert(pg_data_field_positive(a, self, 0) == 1);
	for (size_t i = 0; i < 10000; ++i)
		positive = pg_thunk_type(&classifiers, pg_return_type(&classifiers, positive));
	assert(pg_data_field_positive(positive, self, 1) == 1);
	assert(pg_data_field_positive(NULL, self, 0) == -1);
	assert(pg_data_field_positive(a, a->as.reference, 0) == -1);
	pg_classifiers_destroy(&classifiers);
	pg_graph_destroy(&graph);
}

static void solved_family_lift(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_object *binder, const struct pg_evidence *expected)
{
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, typing->graph));
		assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
		size_t proofs = typing->proofs.count;
		struct pg_synthesis_job *job = pg_synthesis_substitution_lift(&synthesis, substitution, extension, binder);
		assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING && typing->proofs.count == proofs);
		while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) pg_synthesis_advance(&synthesis, chunk);
		const struct pg_evidence *result = pg_synthesis_result(job);
		assert(result && job == pg_synthesis_substitution_lift(&synthesis, substitution, extension, binder));
		const struct pg_object *source = pg_evidence_context(extension)->binder;
		const struct pg_evidence *image = pg_substitution_image(typing, result, source);
		const struct pg_evidence *reference = pg_substitution_image(typing, expected, source);
		assert(image && reference && pg_evidence_judgement(image) == PG_JUDGEMENT_TYPE_FAMILY);
		assert(pg_evidence_subject(image)->core == pg_evidence_subject(reference)->core);
		assert(pg_alpha_equal(pg_evidence_classifier(image), pg_evidence_classifier(reference)) == 1);
		common_rule(typing, classifiers, result);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
}

static void scoped_type_families(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *t = pg_binder(&graph), *a = pg_binder(&graph), *x = pg_binder(&graph);
	const struct pg_object *f = pg_binder(&graph), *v = pg_binder(&graph);
	const struct pg_evidence *base = pg_prove_context_extension(&typing, empty, t, u);
	const struct pg_evidence *ac = pg_prove_context_extension(&typing, base, a, pg_prove_projection(&typing, base, u));
	const struct pg_evidence *xc = pg_prove_context_extension(&typing, ac, x, pg_prove_variable(&typing, ac, a));
	const struct pg_evidence *universe = pg_prove_projection(&typing, xc, u);
	const struct pg_evidence *fc = pg_prove_family_context_extension(&typing, base, f, xc, universe);
	assert(fc && pg_evidence_rule(fc) == PG_CONTEXT_FAMILY_EXTEND);
	common_rule(&typing, &classifiers, fc);
	assert(!pg_prove_family_context_extension(&typing, base, f, base, pg_prove_projection(&typing, base, u)));
	assert(!pg_prove_family_context_extension(&typing, base, f, xc, u));
	const struct pg_evidence *vc = pg_prove_context_extension(&typing, fc, v, pg_prove_variable(&typing, fc, t));
	const struct pg_evidence *family = pg_prove_variable(&typing, vc, f);
	const struct pg_evidence *type = pg_prove_variable(&typing, vc, t);
	const struct pg_evidence *value = pg_prove_variable(&typing, vc, v);
	assert(family && pg_evidence_judgement(family) == PG_JUDGEMENT_TYPE_FAMILY);
	assert(!pg_prove_value_type(&typing, family) && !pg_prove_type_value(&typing, family));
	assert(!pg_prove_return(&typing, &classifiers, family) && !pg_prove_force(&typing, family));
	assert(!pg_prove_application(&typing, family, type));
	assert(!pg_prove_family_application(&typing, family, value));
	const struct pg_evidence *partial = pg_prove_family_application(&typing, family, type);
	assert(partial && pg_evidence_judgement(partial) == PG_JUDGEMENT_TYPE_FAMILY);
	const struct pg_evidence *fiber = pg_prove_family_application(&typing, partial, value);
	assert(fiber && pg_evidence_judgement(fiber) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(fiber) == pg_evidence_subject(u)->core);
	assert(!pg_prove_family_application(&typing, fiber, value));
	assert(pg_prove_type_value(&typing, fiber));
	common_rule(&typing, &classifiers, partial);
	common_rule(&typing, &classifiers, fiber);
	const struct pg_evidence *abstracted = pg_prove_family_abstraction(&typing, vc, fiber);
	assert(abstracted && pg_evidence_subject(abstracted)->core->kind == PG_LAMBDA);
	common_rule(&typing, &classifiers, abstracted);
	const struct pg_evidence *environment = NULL;
	assert(!pg_prove_construction_origin(&typing, &classifiers, empty, &environment));
	assert(!pg_prove_construction_origin(&typing, &classifiers, vc, &environment));
	assert(pg_prove_construction_origin(&typing, &classifiers, abstracted, &environment) == abstracted && !environment);
	assert(pg_prove_construction_origin(&typing, &classifiers, partial, &environment) == partial && !environment);
	assert(pg_prove_construction_origin(&typing, &classifiers, fiber, &environment) == fiber && !environment);
	const struct pg_evidence *projected_family = pg_prove_projection(&typing, vc, abstracted);
	assert(pg_prove_construction_origin(&typing, &classifiers, projected_family, &environment) == abstracted);
	assert(environment && pg_evidence_context_map(environment)->source == pg_evidence_context(fc));
	assert(pg_evidence_context_map(environment)->destination == pg_evidence_context(vc));
	size_t occurrence_count = typing.occurrences.count, proof_count = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_prove_construction_origin(&typing, &classifiers, projected_family, &environment) == abstracted);
	assert(typing.occurrences.count == occurrence_count && typing.proofs.count == proof_count);
	/* Substitution preserves the binding sort, even though both images are
	 * ordinary Core terms. Neither a type value nor a raw thunk is a family. */
	const struct pg_evidence *prefix = pg_prove_substitution_projection(&typing, base, vc);
	const struct pg_evidence *map = pg_prove_substitution_pair(&typing, prefix, fc, family);
	assert(map && pg_prove_substitution_extend(&typing, prefix, fc, 1, &family) == map);
	assert(!pg_prove_substitution_pair(&typing, prefix, fc, type));
	assert(!pg_prove_substitution_extend(&typing, prefix, fc, 1, &type));
	const struct pg_evidence *zero = pg_prove_substitution_projection(&typing, empty, vc);
	assert(!pg_prove_substitution_pair(&typing, zero, base, family));
	const struct pg_evidence *projection = pg_prove_substitution_projection(&typing, vc, vc);
	assert(projection && pg_prove_reindex(&typing, projection, family));
	assert(!pg_prove_construction_origin(&typing, &classifiers, projection, &environment));
	/* Admit an empty indexed family with the dependent signature (A, x:A).
	 * Recovery must retain both typed images, in order, across wrappers. */
	const struct pg_evidence *ia = pg_prove_context_extension(&typing, fc, a, pg_prove_projection(&typing, fc, u));
	const struct pg_evidence *ix = pg_prove_context_extension(&typing, ia, x, pg_prove_variable(&typing, ia, a));
	const struct pg_data_schema *schema = pg_data_schema(&typing, pg_data_signature(&typing, fc, ix), 0, NULL);
	const struct pg_evidence *nominal = pg_prove_inductive_type(&typing, &classifiers, schema);
	assert(nominal);
	struct pg_inductive_instance recovered;
	assert(!pg_inductive_instance(&typing, fiber, &recovered));
	const struct pg_evidence *nf = pg_prove_projection(&typing, vc, nominal);
	const struct pg_evidence *np = pg_prove_family_application(&typing, nf, type);
	const struct pg_evidence *nt = pg_prove_family_application(&typing, np, value);
	assert(nt && !pg_inductive_instance(&typing, np, &recovered));
	assert(pg_inductive_instance(&typing, nf, &recovered) && !recovered.indices);
	const struct pg_evidence *outer = pg_prove_context_extension(&typing, vc, pg_binder(&graph), nt);
	const struct pg_evidence *suspended = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_return_type(&typing, &classifiers, nt));
	assert(suspended && !pg_inductive_instance(&typing, suspended, &recovered));
	const struct pg_evidence *resumed = pg_prove_return_content(&typing,
		pg_prove_thunk_content(&typing, pg_prove_reindex(&typing,
			pg_prove_substitution_projection(&typing, vc, outer), suspended)));
	const struct pg_evidence *wrapped[] = {nt, pg_prove_projection(&typing, outer, nt),
		pg_prove_reindex(&typing, pg_prove_substitution_projection(&typing, vc, outer), nt), resumed};
	for (size_t i = 0; i < sizeof(wrapped) / sizeof(*wrapped); ++i) {
		assert(wrapped[i] && pg_inductive_instance(&typing, wrapped[i], &recovered));
		assert(recovered.schema == schema && recovered.formation == nominal && recovered.indices);
		assert(pg_evidence_context(recovered.indices) == pg_evidence_context(wrapped[i]));
		assert(pg_evidence_context(pg_evidence_premise(recovered.indices, 0)) == pg_evidence_context(ix));
		assert(pg_evidence_subject(pg_substitution_image(&typing, recovered.indices, a))->core == pg_evidence_subject(type)->core);
		assert(pg_evidence_subject(pg_substitution_image(&typing, recovered.indices, x))->core == pg_evidence_subject(value)->core);
		for (size_t chunk = 1; chunk <= 64; chunk *= 64) {
			struct pg_inductive_recovery work;
			assert(!pg_inductive_recovery_init(&work, &typing, wrapped[i]));
			while (!pg_inductive_recovery_advance(&work, chunk)) {}
			assert(work.status == 1 && work.result.indices == recovered.indices);
			pg_inductive_recovery_destroy(&work);
		}
	}
	const struct pg_object *w = pg_binder(&graph);
	const struct pg_evidence *wc = pg_prove_context_extension(&typing, vc, w, type);
	const struct pg_evidence *wv = pg_prove_variable(&typing, wc, w);
	const struct pg_evidence *images[] = {pg_prove_projection(&typing, wc, type),
		pg_prove_projection(&typing, wc, family), wv};
	const struct pg_evidence *changed = pg_prove_substitution(&typing, vc, wc, 3, images);
	assert(changed);
	const struct pg_evidence *changed_fiber = pg_prove_reindex(&typing, changed, nt);
	assert(changed_fiber && pg_inductive_instance(&typing, changed_fiber, &recovered));
	assert(pg_evidence_subject(pg_substitution_image(&typing, recovered.indices, x))->core == pg_evidence_subject(wv)->core);
	/* A wrapper between the first and second application must transport the
	 * first argument without applying that substitution to the second. */
	const struct pg_evidence *mixed = pg_prove_family_application(&typing,
		pg_prove_reindex(&typing, changed, np), wv);
	assert(mixed && pg_inductive_instance(&typing, mixed, &recovered));
	assert(pg_evidence_subject(pg_substitution_image(&typing, recovered.indices, x))->core == pg_evidence_subject(wv)->core);
	assert(pg_evidence_subject(pg_substitution_image(&typing, recovered.indices, a))->core == pg_evidence_subject(type)->core);
	/* Moving a family into a context containing its original index binders
	 * must not capture those binders. */
	const struct pg_object *g = pg_binder(&graph);
	const struct pg_evidence *into_indices = pg_prove_substitution_projection(&typing, base, xc);
	const struct pg_evidence *lifted = pg_prove_substitution_lift(&typing, into_indices, fc, g);
	assert(lifted);
	const struct pg_evidence *destination = pg_evidence_premise(lifted, 1);
	const struct pg_evidence *lifted_family = pg_substitution_image(&typing, lifted, f);
	assert(pg_evidence_judgement(lifted_family) == PG_JUDGEMENT_TYPE_FAMILY);
	assert(pg_evidence_subject(lifted_family)->core == pg_reference(&graph, g));
	assert(pg_prove_family_application(&typing, pg_prove_family_application(&typing,
		lifted_family, pg_prove_variable(&typing, destination, a)), pg_prove_variable(&typing, destination, x)));
	assert(!pg_prove_substitution_lift(&typing, into_indices, fc, a));
	assert(!pg_prove_substitution_lift(&typing, zero, fc, g));
	common_rule(&typing, &classifiers, lifted);
	common_rule(&typing, &classifiers, destination);
	solved_family_lift(&typing, &classifiers, into_indices, fc, g, lifted);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

static void higher_family_lift(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *f = pg_binder(&graph), *h = pg_binder(&graph);
	const struct pg_evidence *ac = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *fc = pg_prove_family_context_extension(&typing, empty, f, ac,
		pg_prove_projection(&typing, ac, u));
	const struct pg_evidence *fa = pg_prove_context_extension(&typing, fc, a, pg_prove_projection(&typing, fc, u));
	const struct pg_evidence *fiber = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, fa, f), pg_prove_variable(&typing, fa, a));
	const struct pg_object *x = pg_binder(&graph);
	const struct pg_evidence *fx = pg_prove_context_extension(&typing, fa, x, fiber);
	const struct pg_evidence *hc = pg_prove_family_context_extension(&typing, empty, h, fx,
		pg_prove_projection(&typing, fx, u));
	assert(hc);
	/* The source substitution has no images, but its destination already
	 * contains the index binder A. Higher signatures need fresh scopes too. */
	const struct pg_evidence *prefix = pg_prove_substitution_projection(&typing, empty, ac);
	const struct pg_evidence *lifted = pg_prove_substitution_lift(&typing, prefix, hc, h);
	assert(lifted);
	const struct pg_evidence *destination = pg_evidence_premise(lifted, 1);
	assert(pg_evidence_rule(destination) == PG_CONTEXT_FAMILY_EXTEND);
	assert(pg_alpha_equal(pg_evidence_classifier(pg_prove_variable(&typing, hc, h)),
		pg_evidence_classifier(pg_prove_variable(&typing, destination, h))) == 1);
	common_rule(&typing, &classifiers, lifted);
	common_rule(&typing, &classifiers, destination);
	solved_family_lift(&typing, &classifiers, prefix, hc, h, lifted);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

/* Acc under genuine logical-family assumptions. This does not assert that
 * an arbitrary empty-effect CBPV function is a terminating type family. */
static void accessibility_elimination(enum pg_totality field_totality)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *r = pg_binder(&graph), *p = pg_binder(&graph);
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph), *e = pg_binder(&graph);
	const struct pg_evidence *ac = pg_prove_context_extension(&typing, empty, a, universe);
	const struct pg_evidence *rx = pg_prove_context_extension(&typing, ac, x, pg_prove_variable(&typing, ac, a));
	const struct pg_evidence *ry = pg_prove_context_extension(&typing, rx, y, pg_prove_variable(&typing, rx, a));
	const struct pg_evidence *rc = pg_prove_family_context_extension(&typing, ac, r, ry,
		pg_prove_projection(&typing, ry, universe));
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, &graph));
		assert(!pg_synthesis_init(&synthesis, &typing, &classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
		const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, pg_synthesis_root(&synthesis),
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "A", .length = 1}, a, ac);
		scope = pg_synthesis_bind(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "R", .length = 1}, r, rc);
		assert(scope);
		const char source[] = "Acc := @\\subject : A => { acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x; };";
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &graph, source, sizeof(source) - 1);
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *job = pg_synthesis_request(&synthesis, scope, definition.expression);
		assert(job);
		while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING && synthesis.steps < 10000)
			pg_synthesis_advance(&synthesis, chunk);
		assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
		assert(pg_evidence_judgement(pg_synthesis_result(job)) == PG_JUDGEMENT_TYPE_FAMILY);
		struct pg_inductive_instance instance;
		assert(pg_inductive_instance(&typing, pg_synthesis_result(job), &instance));
		assert(pg_data_constructor_count(instance.schema) == 1);
		const struct pg_evidence *parameters = pg_data_schema_parameters(instance.schema);
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(instance.schema), 0);
		const struct pg_evidence *source_fields = pg_data_schema_fields(instance.schema, constructor);
		size_t field_count;
		assert(!pg_context_extension_size(pg_evidence_context(source_fields), pg_evidence_context(parameters), &field_count));
		assert(field_count == 2 && pg_data_recursive_field(pg_evidence_context(source_fields)->declared_type,
			pg_evidence_context(parameters)->binder) == 1);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	const struct pg_evidence *index = pg_prove_context_extension(&typing, rc, pg_binder(&graph),
		pg_prove_variable(&typing, rc, a));
	const struct pg_object *self = pg_binder(&graph), *down = pg_binder(&graph);
	const struct pg_evidence *sc = pg_prove_family_context_extension(&typing, rc, self, index,
		pg_prove_projection(&typing, index, universe));
	const struct pg_evidence *indices = pg_prove_context_extension(&typing, sc, pg_binder(&graph),
		pg_prove_variable(&typing, sc, a));
	const struct pg_evidence *xc = pg_prove_context_extension(&typing, sc, x, pg_prove_variable(&typing, sc, a));
	const struct pg_evidence *yc = pg_prove_context_extension(&typing, xc, y, pg_prove_variable(&typing, xc, a));
	const struct pg_evidence *edge = pg_prove_family_application(&typing,
		pg_prove_family_application(&typing, pg_prove_variable(&typing, yc, r), pg_prove_variable(&typing, yc, y)),
		pg_prove_variable(&typing, yc, x));
	const struct pg_evidence *ec = pg_prove_context_extension(&typing, yc, e, edge);
	const struct pg_evidence *recursive = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, ec, self), pg_prove_variable(&typing, ec, y));
	const struct pg_evidence *down_type = pg_prove_pi(&typing, &classifiers, ec,
		pg_prove_computation_type(&typing, &classifiers, field_totality,
			pg_effect_row(&graph, 0, NULL), recursive));
	down_type = pg_prove_pi(&typing, &classifiers, yc, down_type);
	const struct pg_evidence *fields = pg_prove_context_extension(&typing, xc, down,
		pg_prove_thunk_type(&typing, &classifiers, down_type));
	const struct pg_evidence *result = pg_prove_substitution_pair(&typing,
		pg_prove_substitution_projection(&typing, sc, fields), indices, pg_prove_variable(&typing, fields, x));
	const struct pg_data_schema *schema = pg_data_schema(&typing,
		pg_data_signature(&typing, sc, indices), 1, &result);
	const struct pg_evidence *acc = pg_prove_inductive_type(&typing, &classifiers, schema);
	assert(acc);
	common_rule(&typing, &classifiers, acc);
	const struct pg_evidence *acc_r = pg_prove_family_abstraction(&typing, rc, acc);
	const struct pg_evidence *acc_a = pg_prove_family_abstraction(&typing, ac, acc_r);
	assert(acc_r && acc_a);
	common_rule(&typing, &classifiers, acc_r);
	common_rule(&typing, &classifiers, acc_a);
	const struct pg_evidence *applied_a = pg_prove_family_application(&typing,
		pg_prove_projection(&typing, rc, acc_a), pg_prove_variable(&typing, rc, a));
	const struct pg_evidence *applied_r = pg_prove_family_application(&typing, applied_a, pg_prove_variable(&typing, rc, r));
	assert(applied_r);
	common_rule(&typing, &classifiers, applied_r);
	const struct pg_evidence *applied_body = pg_prove_application_body(&typing, applied_a, pg_prove_variable(&typing, rc, r));
	assert(applied_body);
	struct pg_inductive_instance instance;
	assert(pg_inductive_instance(&typing, applied_body, &instance));
	assert(instance.formation == acc && instance.schema == schema);
	assert(!pg_prove_family_application(&typing, applied_a, pg_prove_variable(&typing, rc, a)));
	const struct pg_evidence *unary_context = pg_prove_context_extension(&typing, rc, pg_binder(&graph),
		pg_prove_variable(&typing, rc, a));
	const struct pg_evidence *unary = pg_prove_family_abstraction(&typing, unary_context,
		pg_prove_value_type(&typing, pg_prove_variable(&typing, unary_context, a)));
	assert(unary && !pg_prove_family_application(&typing, applied_a, unary));
	assert(!pg_prove_application_body(&typing, applied_a, unary));
	const struct pg_evidence *delayed = pg_prove_thunk(&typing, &classifiers,
		pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, rc, a)));
	assert(delayed && !pg_prove_family_application(&typing, applied_a, delayed));
	/* A higher signature can quantify over R itself; it still admits only a
	 * logical family of the specified arity, never an arbitrary computation. */
	const struct pg_object *operator = pg_binder(&graph);
	const struct pg_evidence *oc = pg_prove_family_context_extension(&typing, ac, operator, index,
		pg_prove_projection(&typing, index, universe));
	assert(oc);
	common_rule(&typing, &classifiers, oc);
	const struct pg_evidence *map = pg_prove_substitution_projection(&typing, ac, oc);
	map = pg_prove_substitution_lift(&typing, map, rx, x);
	map = pg_prove_substitution_lift(&typing, map, ry, y);
	const struct pg_evidence *arguments = pg_evidence_premise(map, 1);
	const struct pg_evidence *both = pg_prove_family_context_extension(&typing, oc, r, arguments,
		pg_prove_projection(&typing, arguments, universe));
	assert(both);
	const struct pg_evidence *higher = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, both, operator), pg_prove_variable(&typing, both, r));
	assert(higher && pg_evidence_judgement(higher) == PG_JUDGEMENT_TYPE_FAMILY);
	common_rule(&typing, &classifiers, higher);
	const struct pg_evidence *higher_value_context = pg_prove_context_extension(&typing, both, pg_binder(&graph),
		pg_prove_variable(&typing, both, a));
	const struct pg_evidence *higher_fiber = pg_prove_family_application(&typing,
		pg_prove_projection(&typing, higher_value_context, higher),
		pg_prove_variable(&typing, higher_value_context, pg_evidence_context(higher_value_context)->binder));
	assert(higher_fiber && pg_evidence_judgement(higher_fiber) == PG_JUDGEMENT_VALUE_TYPE);
	common_rule(&typing, &classifiers, higher_fiber);
	const struct pg_evidence *pc = pg_prove_family_context_extension(&typing, rc, p, index,
		pg_prove_projection(&typing, index, universe));
	/* step : (x:A) -> U((y:A) -> R y x -> F(P y)) -> F(P x). */
	xc = pg_prove_context_extension(&typing, pc, x, pg_prove_variable(&typing, pc, a));
	yc = pg_prove_context_extension(&typing, xc, y, pg_prove_variable(&typing, xc, a));
	edge = pg_prove_family_application(&typing,
		pg_prove_family_application(&typing, pg_prove_variable(&typing, yc, r), pg_prove_variable(&typing, yc, y)),
		pg_prove_variable(&typing, yc, x));
	ec = pg_prove_context_extension(&typing, yc, e, edge);
	const struct pg_evidence *py = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, ec, p), pg_prove_variable(&typing, ec, y));
	const struct pg_evidence *ih_type = pg_prove_pi(&typing, &classifiers, ec,
		pg_prove_return_type(&typing, &classifiers, py));
	ih_type = pg_prove_pi(&typing, &classifiers, yc, ih_type);
	const struct pg_evidence *hc = pg_prove_context_extension(&typing, xc, pg_binder(&graph),
		pg_prove_thunk_type(&typing, &classifiers, ih_type));
	const struct pg_evidence *px = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, hc, p), pg_prove_variable(&typing, hc, x));
	const struct pg_evidence *step_type = pg_prove_pi(&typing, &classifiers, hc, pg_prove_return_type(&typing, &classifiers, px));
	step_type = pg_prove_pi(&typing, &classifiers, xc, step_type);
	const struct pg_object *step = pg_binder(&graph), *subject = pg_binder(&graph), *proof = pg_binder(&graph);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, pc, step,
		pg_prove_thunk_type(&typing, &classifiers, step_type));
	context = pg_prove_context_extension(&typing, context, subject, pg_prove_variable(&typing, context, a));
	const struct pg_evidence *fiber = pg_prove_family_application(&typing,
		pg_prove_projection(&typing, context, acc), pg_prove_variable(&typing, context, subject));
	context = pg_prove_context_extension(&typing, context, proof, fiber);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(&typing, rc, context);
	const struct pg_evidence *mc = pg_prove_inductive_motive_context(&typing, acc, parameters, pg_binder(&graph));
	const struct pg_evidence *motive_index = pg_prove_variable(&typing, mc, pg_evidence_context(mc)->parent->binder);
	const struct pg_evidence *motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_family_application(&typing, pg_prove_variable(&typing, mc, p), motive_index));
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(schema), 0);
	/* Infer the candidate from step x's result before an IH exists. The
	 * constructor maps the generic scrutinee to acc x down, not a variable. */
	const struct pg_evidence *field_map = pg_prove_constructor_scope(&typing, acc, constructor, parameters);
	const struct pg_evidence *field_context = pg_evidence_premise(field_map, 1);
	const struct pg_evidence *field_values[] = {
		pg_substitution_image(&typing, field_map, x), pg_substitution_image(&typing, field_map, down)};
	const struct pg_evidence *total_motive = pg_prove_computation_type(&typing, &classifiers,
		PG_TOTALITY_TOTAL, pg_effect_row(&graph, 0, NULL), pg_prove_return_content(&typing, motive));
	const struct pg_evidence *bounded_ih = pg_prove_inductive_hypothesis_type(&typing, &classifiers,
		acc, parameters, mc, total_motive, field_context, field_values[1]);
	assert(bounded_ih);
	const struct pg_term *tail, *domain, *codomain, *content;
	const struct pg_object *parameter;
	assert(pg_thunk_type_view(pg_evidence_subject(bounded_ih)->core, &tail));
	while (pg_pi_view(tail, &domain, &parameter, &codomain)) tail = codomain;
	const struct pg_effect_row *row;
	enum pg_totality ih_totality;
	assert(pg_computation_type_view(tail, &ih_totality, &row, &content));
	assert(ih_totality == field_totality && !pg_effect_count(row));
	/* Depend on the returned child itself, not only on its index. This is
	 * admissible only when the recursive field carries a total pure contract. */
	const struct pg_evidence *z = pg_prove_variable(&typing, mc, pg_evidence_context(mc)->binder);
	const struct pg_evidence *child_identity = pg_prove_identity_type(&typing,
		pg_prove_classifier(&typing, &classifiers, mc, z), z, z);
	const struct pg_evidence *child_motive = pg_prove_return_type(&typing, &classifiers, child_identity);
	assert(child_motive);
	const struct pg_evidence *dependent_ih = pg_prove_inductive_hypothesis_type(&typing, &classifiers,
		acc, parameters, mc, child_motive, field_context, field_values[1]);
	assert(!!dependent_ih == (field_totality == PG_TOTALITY_TOTAL));
	if (dependent_ih) {
		common_rule(&typing, &classifiers, dependent_ih);
		assert(pg_prove_induction_scope(&typing, &classifiers, acc, constructor,
			parameters, mc, child_motive));
	}
	const struct pg_evidence *constructor_value = pg_prove_constructor(&typing, acc, constructor,
		pg_prove_substitution_projection(&typing, rc, field_context), 2, field_values);
	const struct pg_evidence *constructor_pattern = pg_prove_inductive_motive_substitution(&typing,
		&classifiers, acc, parameters, mc, field_context, constructor_value);
	const struct pg_evidence *step_at_field = pg_prove_application(&typing,
		pg_prove_force(&typing, pg_prove_variable(&typing, field_context, step)), field_values[0]);
	const struct pg_evidence *branch_result = pg_prove_pi_constant_codomain(&typing,
		pg_prove_classifier(&typing, &classifiers, field_context, step_at_field));
	const struct pg_evidence *inferred = pg_prove_pattern_type(&typing, &classifiers,
		context, constructor_pattern, branch_result);
	assert(inferred && pg_alpha_equal(pg_evidence_subject(inferred)->core, pg_evidence_subject(motive)->core) == 1);
	common_rule(&typing, &classifiers, inferred);
	const struct pg_evidence *total_branch_type = pg_prove_computation_type(&typing, &classifiers,
		PG_TOTALITY_TOTAL, pg_effect_row(&graph, 0, NULL), pg_prove_return_content(&typing, branch_result));
	const struct pg_evidence *total_inferred = pg_prove_pattern_type(&typing, &classifiers,
		context, constructor_pattern, total_branch_type);
	assert(total_inferred && pg_alpha_equal(pg_evidence_subject(total_inferred)->core,
		pg_evidence_subject(total_motive)->core) == 1);
	const struct pg_evidence *down_identity = pg_prove_identity_type(&typing,
		pg_prove_classifier(&typing, &classifiers, field_context, field_values[1]), field_values[1], field_values[1]);
	assert(down_identity && !pg_prove_pattern_type(&typing, &classifiers, context, constructor_pattern,
		pg_prove_return_type(&typing, &classifiers, down_identity)));
	motive = inferred;
	const struct pg_evidence *scope = pg_prove_induction_scope(&typing, &classifiers, acc, constructor, parameters, mc, motive);
	assert(scope);
	const struct pg_evidence *branch_context = pg_evidence_premise(scope, 1);
	const struct pg_evidence *ih = pg_prove_variable(&typing, branch_context, pg_evidence_context(branch_context)->binder);
	const struct pg_evidence *at_x = pg_prove_application(&typing,
		pg_prove_force(&typing, pg_prove_variable(&typing, branch_context, step)), pg_substitution_image(&typing, scope, x));
	const struct pg_evidence *body = pg_prove_application(&typing, at_x, ih);
	assert(body && !pg_prove_application(&typing, at_x, pg_substitution_image(&typing, scope, down)));
	const struct pg_evidence *branch = pg_prove_abstract(&typing, &classifiers, context, branch_context, body);
	const struct pg_evidence *elimination = pg_prove_induction(&typing, &classifiers, acc, parameters,
		pg_prove_variable(&typing, context, proof), mc, motive, 1, &branch);
	assert(elimination);
	assert(!pg_prove_elimination_body(&typing, &classifiers, elimination));
	common_rule(&typing, &classifiers, elimination);
	const struct pg_evidence *constructor_parameters = pg_prove_substitution_projection(&typing, rc, field_context);
	const struct pg_evidence *projected = pg_prove_elimination_reindex(&typing, &classifiers,
		pg_prove_substitution_projection(&typing, context, field_context), elimination);
	const struct pg_evidence *projected_branch = pg_evidence_premise(projected, 5);
	const struct pg_evidence *at_constructor = pg_prove_induction(&typing, &classifiers, acc,
		constructor_parameters, constructor_value, pg_evidence_premise(projected, 4),
		pg_evidence_premise(projected, 0), 1, &projected_branch);
	assert(at_constructor);
	const struct pg_evidence *unfolded = pg_prove_elimination_body(&typing, &classifiers, at_constructor);
	assert(unfolded);
	assert(pg_alpha_equal(pg_evidence_classifier(at_constructor), pg_evidence_classifier(unfolded)) == 1);
	const struct pg_evidence *unfolded_again = pg_prove_elimination_body(&typing, &classifiers, at_constructor);
	assert(unfolded_again && pg_alpha_equal(pg_evidence_subject(unfolded)->core,
		pg_evidence_subject(unfolded_again)->core) == 1);
	common_rule(&typing, &classifiers, unfolded);
	struct pg_whnf_work unfolding_work;
	assert(!pg_whnf_work_init(&unfolding_work, &graph));
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_conversion unfolding;
		assert(!pg_conversion_init(&unfolding, &unfolding_work,
			pg_evidence_subject(at_constructor)->core, pg_evidence_subject(unfolded)->core));
		while (pg_conversion_advance(&unfolding, chunk) == PG_CONVERSION_PENDING)
			assert(pg_conversion_steps(&unfolding) < 100000);
		assert(pg_conversion_status(&unfolding) == PG_CONVERSION_EQUAL);
		pg_conversion_destroy(&unfolding);
	}
	pg_whnf_work_destroy(&unfolding_work);
	const struct pg_evidence *expected = pg_prove_return_type(&typing, &classifiers,
		pg_prove_family_application(&typing, pg_prove_variable(&typing, context, p), pg_prove_variable(&typing, context, subject)));
	assert(pg_alpha_equal(pg_evidence_classifier(elimination), pg_evidence_subject(expected)->core) == 1);
	/* Close the same eliminator over A, R and P, then instantiate it using
	 * ordinary APP and typed substitution. Logical families are not values. */
	const struct pg_evidence *closed = pg_prove_abstract(&typing, &classifiers, empty, context, elimination);
	assert(closed && !pg_evidence_context(closed));
	common_rule(&typing, &classifiers, closed);
	const struct pg_evidence *opened = pg_prove_projection(&typing, context, closed);
	const struct pg_object *parameter_binders[] = {a, r, p, step, subject, proof};
	for (size_t i = 0; i < sizeof(parameter_binders) / sizeof(parameter_binders[0]); ++i) {
		const struct pg_evidence *argument = pg_prove_variable(&typing, context, parameter_binders[i]);
		const struct pg_evidence *pi = pg_prove_classifier(&typing, &classifiers, context, opened);
		assert(pi);
		if (i == 1 || i == 2) {
			assert(!pg_prove_pi_domain(&typing, pi));
			assert(!pg_prove_application(&typing, opened, pg_prove_projection(&typing, context, delayed)));
		}
		if (i == 1) assert(!pg_prove_application(&typing, opened, pg_prove_projection(&typing, context, unary)));
		const struct pg_evidence *codomain = pg_prove_pi_codomain(&typing, pi, argument);
		opened = pg_prove_application(&typing, opened, argument);
		assert(opened && codomain);
		assert(pg_alpha_equal(pg_evidence_classifier(opened), pg_evidence_subject(codomain)->core) == 1);
		common_rule(&typing, &classifiers, opened);
	}
	assert(pg_alpha_equal(pg_evidence_classifier(opened), pg_evidence_subject(expected)->core) == 1);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("Acc: open logical relation, indexed function-field IH and dependent elimination passed");
}

static void retained_substitution_prefix(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *contexts[2], *function_types[2], *images[2];
	const struct pg_object *function_binders[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_object *a = pg_binder(&graph), *x = pg_binder(&graph);
		function_binders[i] = pg_binder(&graph);
		const struct pg_evidence *base = pg_prove_context_extension(&typing, empty, a, u);
		const struct pg_evidence *domain = pg_prove_variable(&typing, base, a);
		const struct pg_evidence *body = pg_prove_context_extension(&typing, base, x, domain);
		function_types[i] = pg_prove_thunk_type(&typing, &classifiers,
			pg_prove_pi(&typing, &classifiers, body,
				pg_prove_return_type(&typing, &classifiers, pg_prove_variable(&typing, body, a))));
		contexts[i] = pg_prove_context_extension(&typing, base, function_binders[i], function_types[i]);
		assert(contexts[i]);
		if (i) {
			images[0] = pg_prove_variable(&typing, contexts[i], a);
			images[1] = pg_prove_variable(&typing, contexts[i], function_binders[i]);
		}
	}
	const struct pg_evidence *prefix = pg_prove_substitution(&typing, contexts[0], contexts[1], 2, images);
	assert(prefix);
	const struct pg_evidence *alternate = pg_prove_context_extension(&typing,
		pg_evidence_premise(contexts[0], 0), function_binders[0],
		pg_prove_value_type(&typing, pg_prove_type_value(&typing, function_types[0])));
	assert(alternate != contexts[0] && pg_evidence_context(alternate) == pg_evidence_context(contexts[0]));
	size_t terms = graph.terms.count;
	const struct pg_evidence *result = pg_prove_substitution_extend(&typing, prefix, alternate, 0, NULL);
	assert(result && result != prefix && graph.terms.count == terms);
	assert(pg_evidence_premise(result, 0) == alternate);
	assert(pg_prove_substitution(&typing, alternate, contexts[1], 2, images) == result);
	assert(pg_evidence_premise(result, 2) == images[0] && pg_evidence_premise(result, 3) == images[1]);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

static void check(struct pg_whnf_work *work, const struct pg_term *term, const struct pg_term *expected)
{
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, term);
	assert(job && pg_whnf_request(work, &pg_pure_policy, term) == job);
	while (pg_whnf_advance(job, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(job) < 100000);
	assert(pg_whnf_status(job) == PG_EVAL_WHNF && pg_whnf_result(job) == expected);
	uint64_t steps = pg_whnf_steps(job);
	assert(pg_whnf_advance(job, 100) == PG_EVAL_WHNF && pg_whnf_steps(job) == steps);
}

static const struct pg_evidence *solve_index_proof(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, uint64_t chunk, enum pg_synthesis_status expected)
{
	assert(job);
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(synthesis->steps < 100000);
		pg_synthesis_advance(synthesis, chunk);
	}
	assert(pg_synthesis_status(job) == expected);
	return pg_synthesis_result(job);
}

static void indexed_path_motive(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *scrutinee, const struct pg_evidence *mc,
	const struct pg_evidence *fields, const struct pg_evidence *original_type,
	const struct pg_evidence *original_value)
{
	const struct pg_evidence *context = pg_evidence_premise(parameters, 1);
	struct pg_inductive_instance actual, generic;
	assert(pg_inductive_instance(typing, pg_prove_classifier(typing, classifiers, context, scrutinee), &actual));
	assert(pg_inductive_instance(typing, pg_evidence_premise(mc, 1), &generic));
	const struct pg_evidence *projection = pg_prove_substitution_projection(typing, context, mc);
	const struct pg_evidence *left = pg_prove_substitution_compose(typing, actual.indices, projection);
	const struct pg_evidence *right = pg_prove_substitution_compose(typing, generic.indices,
		pg_prove_substitution_projection(typing, pg_evidence_premise(generic.indices, 1), mc));
	const struct pg_object *binders[] = {pg_binder(typing->graph), pg_binder(typing->graph)};
	const struct pg_evidence *paths[2];
	const struct pg_evidence *pc = pg_identity_substitution_context(typing, left, right, 2, binders, paths);
	assert(pc);
	/* Motive: for each generic (A',x') and d:D A' x', consume paths
	 * (A,x)=(A',x') and return an A. A is the fixed ambient type, so a
	 * branch field of type A' requires transport, not just substitution. */
	const struct pg_evidence *motive = pg_prove_return_type(typing, classifiers,
		pg_prove_projection(typing, pc, original_type));
	const struct pg_evidence *extensions[] = {pg_evidence_premise(pc, 0), pc};
	for (const struct pg_evidence *c = pc; pg_evidence_context(c) != pg_evidence_context(mc); c = pg_evidence_premise(c, 0))
		motive = pg_prove_pi(typing, classifiers, c, motive);
	assert(motive);
	const struct pg_evidence *fc = pg_evidence_premise(fields, 1);
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(actual.schema), 0);
	const struct pg_evidence *values[] = {
		pg_evidence_premise(fields, pg_evidence_premise_count(fields) - 2),
		pg_evidence_premise(fields, pg_evidence_premise_count(fields) - 1)
	};
	const struct pg_evidence *field_parameters = pg_prove_substitution_compose(typing, parameters,
		pg_prove_substitution_projection(typing, context, fc));
	const struct pg_evidence *data = pg_prove_constructor(typing, formation, constructor, field_parameters, 2, values);
	const struct pg_evidence *pattern = pg_prove_inductive_motive_substitution(typing, classifiers,
		formation, parameters, mc, fc, data);
	for (size_t i = 0; i < 2; ++i)
		pattern = pg_prove_substitution_lift(typing, pattern, extensions[i], pg_binder(typing->graph));
	assert(pattern);
	const struct pg_evidence *bc = pg_evidence_premise(pattern, 1);
	const struct pg_evidence *type_path = pg_substitution_image(typing, pattern, binders[0]);
	const struct pg_evidence *field = pg_prove_projection(typing, bc, values[1]);
	const struct pg_evidence *transported = pg_prove_identity_transport(typing, classifiers,
		type_path, field, PG_IDENTITY_LEFT);
	assert(transported && pg_evidence_classifier(transported) == pg_evidence_subject(original_type)->core);
	const struct pg_evidence *branch = pg_prove_abstract(typing, classifiers, context, bc,
		pg_prove_return(typing, classifiers, transported));
	const struct pg_evidence *match = pg_prove_match(typing, classifiers, formation, parameters,
		scrutinee, mc, motive, 1, &branch);
	assert(match);
	const struct pg_evidence *untransported = pg_prove_abstract(typing, classifiers, context, bc,
		pg_prove_return(typing, classifiers, field));
	assert(untransported && !pg_prove_match(typing, classifiers, formation, parameters,
		scrutinee, mc, motive, 1, &untransported));
	common_rule(typing, classifiers, match);
	const struct pg_evidence *type_value = pg_prove_type_value(typing, original_type);
	const struct pg_evidence *arguments[] = {
		pg_prove_reflexivity(typing, pg_prove_classifier(typing, classifiers, context, type_value), type_value),
		pg_prove_reflexivity(typing, original_type, original_value)
	};
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, typing->graph));
		assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job *applied = pg_synthesis_evidence(&synthesis, match);
		for (size_t i = 0; i < 2; ++i)
			applied = pg_synthesis_application(&synthesis, context, applied, pg_synthesis_evidence(&synthesis, arguments[i]));
		const struct pg_evidence *result = solve_index_proof(&synthesis, applied, chunk, PG_SYNTHESIS_DONE);
		assert(pg_evidence_classifier(result) == pg_return_type(classifiers, pg_evidence_subject(original_type)->core));
		const struct pg_evidence *normal = solve_index_proof(&synthesis,
			pg_synthesis_normalize_jobs(&synthesis, pg_synthesis_evidence(&synthesis, context), applied, PG_REDUCTION_NF),
			chunk, PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(normal)->core == pg_evidence_subject(pg_prove_return(typing, classifiers, original_value))->core);
		solve_index_proof(&synthesis, pg_synthesis_application(&synthesis, context,
			pg_synthesis_evidence(&synthesis, match), pg_synthesis_evidence(&synthesis, arguments[1])),
			chunk, PG_SYNTHESIS_REJECTED);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	puts("indexed Match: dependent path motive, branch transport and reflexive application compute through Solve");
}

static void indexed_match(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *u1 = pg_prove_universe(&typing, &classifiers, empty, 1);
	const struct pg_object *a = pg_binder(&graph), *x = pg_binder(&graph), *f = pg_binder(&graph);
	const struct pg_evidence *ac = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *xc = pg_prove_context_extension(&typing, ac, x, pg_prove_variable(&typing, ac, a));
	const struct pg_evidence *fc = pg_prove_family_context_extension(&typing, empty, f, xc,
		pg_prove_projection(&typing, xc, u1));
	const struct pg_evidence *ia = pg_prove_context_extension(&typing, fc, a, pg_prove_projection(&typing, fc, u));
	const struct pg_evidence *ix = pg_prove_context_extension(&typing, ia, x, pg_prove_variable(&typing, ia, a));
	const struct pg_evidence *result_map = pg_prove_substitution_projection(&typing, ix, ix);
	const struct pg_data_schema *schema = pg_data_schema(&typing, pg_data_signature(&typing, fc, ix), 1, &result_map);
	const struct pg_evidence *formation = pg_prove_inductive_type(&typing, &classifiers, schema);
	assert(formation);
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(schema), 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(&typing, empty, xc);
	const struct pg_evidence *av = pg_prove_variable(&typing, xc, a), *xv = pg_prove_variable(&typing, xc, x);
	const struct pg_evidence *values[] = {av, xv};
	const struct pg_evidence *value = pg_prove_constructor(&typing, formation, constructor, parameters, 2, values);
	assert(value);
	const struct pg_evidence *mc = pg_prove_inductive_motive_context(&typing, formation, parameters, pg_binder(&graph));
	assert(mc);
	const struct pg_context *mi = pg_evidence_context(mc)->parent;
	const struct pg_evidence *motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_variable(&typing, mc, mi->parent->binder));
	/* The result type is the first index A, not a constant carrier. */
	assert(motive);
	const struct pg_evidence *fields = pg_prove_constructor_scope(&typing, formation, constructor, parameters);
	const struct pg_evidence *field_context = pg_evidence_premise(fields, 1);
	indexed_path_motive(&typing, &classifiers, formation, parameters, value, mc, fields,
		pg_prove_value_type(&typing, av), xv);
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers,
		pg_substitution_image(&typing, fields, x));
	const struct pg_evidence *branch = pg_prove_abstract(&typing, &classifiers, xc, field_context, body);
	const struct pg_evidence *match = pg_prove_match(&typing, &classifiers, formation, parameters,
		value, mc, motive, 1, &branch);
	assert(match && pg_evidence_classifier(match) == pg_return_type(&classifiers, pg_evidence_subject(av)->core));
	const struct pg_occurrence *structure = pg_evidence_subject(match);
	assert(structure->operand_count == 4 && structure->map_count == 1);
	assert(structure->operands[0] == pg_evidence_subject(value));
	assert(structure->operands[1] == pg_evidence_subject(branch));
	assert(structure->operands[2] == pg_evidence_subject(motive));
	assert(structure->operands[2]->context == pg_evidence_context(mc));
	assert(structure->operands[3] == pg_evidence_subject(formation));
	assert(pg_occurrence_maps(structure)[0] == pg_evidence_context_map(parameters));
	common_rule(&typing, &classifiers, match);
	/* Reconstruct the elimination after substituting both dependent indices.
	 * Its generic motive remains generic, rather than fixing the source fiber. */
	const struct pg_object *renamed_a = pg_binder(&graph), *renamed_x = pg_binder(&graph);
	const struct pg_evidence *renamed_ac = pg_prove_context_extension(&typing, empty, renamed_a, u);
	const struct pg_evidence *renamed_xc = pg_prove_context_extension(&typing, renamed_ac, renamed_x,
		pg_prove_variable(&typing, renamed_ac, renamed_a));
	const struct pg_evidence *renamed_values[] = {
		pg_prove_variable(&typing, renamed_xc, renamed_a), pg_prove_variable(&typing, renamed_xc, renamed_x)};
	const struct pg_evidence *renaming = pg_prove_substitution(&typing, xc, renamed_xc, 2, renamed_values);
	const struct pg_evidence *renamed_match = pg_prove_elimination_reindex(&typing, &classifiers, renaming, match);
	const struct pg_evidence *mapped_match = pg_prove_reindex(&typing, renaming, match);
	assert(renamed_match && mapped_match && pg_evidence_rule(renamed_match) == PG_MATCH_ELIM);
	assert(pg_evidence_subject(mapped_match)->origin == structure);
	const struct pg_occurrence *renamed_structure = pg_evidence_subject(renamed_match);
	assert(renamed_structure->operand_count == 4 && renamed_structure->map_count == 1);
	assert(pg_occurrence_maps(renamed_structure)[0]->destination == pg_evidence_context(renamed_xc));
	assert(renamed_structure->operands[3] == structure->operands[3]);
	assert(renamed_structure->operands[2]->context != structure->operands[2]->context);
	assert(pg_alpha_equal(pg_evidence_subject(renamed_match)->core, pg_evidence_subject(mapped_match)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(renamed_match), pg_evidence_classifier(mapped_match)) == 1);
	common_rule(&typing, &classifiers, renamed_match);
	const struct pg_evidence *inverse = pg_prove_substitution(&typing, renamed_xc, xc, 2, values);
	const struct pg_evidence *twice = pg_prove_elimination_reindex(&typing, &classifiers, inverse, renamed_match);
	const struct pg_evidence *once = pg_prove_elimination_reindex(&typing, &classifiers,
		pg_prove_substitution_compose(&typing, renaming, inverse), match);
	assert(twice && once);
	assert(pg_alpha_equal(pg_evidence_subject(twice)->core, pg_evidence_subject(once)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(twice), pg_evidence_classifier(once)) == 1);
	common_rule(&typing, &classifiers, twice);
	assert(!pg_prove_elimination_reindex(&typing, &classifiers, parameters, match));
	assert(!pg_prove_elimination_reindex(&typing, &classifiers, renaming, value));
	struct pg_whnf_work work;
	assert(!pg_whnf_work_init(&work, &graph));
	check(&work, pg_evidence_subject(match)->core, pg_evidence_subject(pg_prove_return(&typing, &classifiers, xv))->core);
	const struct pg_evidence *selected_body = pg_prove_elimination_body(&typing, &classifiers, match);
	const struct pg_evidence *renamed_body = pg_prove_elimination_body(&typing, &classifiers, renamed_match);
	assert(selected_body && renamed_body);
	assert(pg_alpha_equal(pg_evidence_classifier(selected_body), pg_evidence_classifier(match)) == 1);
	check(&work, pg_evidence_subject(match)->core, pg_evidence_subject(selected_body)->core);
	check(&work, pg_evidence_subject(renamed_match)->core, pg_evidence_subject(renamed_body)->core);
	common_rule(&typing, &classifiers, selected_body);
	common_rule(&typing, &classifiers, renamed_body);
	size_t structures = typing.occurrences.count, proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_prove_elimination_body(&typing, &classifiers, match) == selected_body);
	assert(typing.occurrences.count == structures && typing.proofs.count == proofs);
	assert(!pg_prove_elimination_body(&typing, &classifiers, value));
	assert(!pg_prove_elimination_body(&typing, &classifiers, NULL));
	const struct pg_evidence *branch_type = pg_prove_value_type(&typing,
		pg_substitution_image(&typing, fields, a));
	const struct pg_evidence *scope = field_context;
	while (pg_evidence_context(scope) != pg_evidence_context(xc)) {
		branch_type = pg_prove_family_abstraction(&typing, scope, branch_type);
		scope = pg_evidence_premise(scope, 0);
	}
	const struct pg_evidence *selected_type = pg_prove_type_case(&typing, &classifiers,
		formation, parameters, value, 1, &branch_type);
	assert(selected_type && pg_evidence_judgement(selected_type) == PG_JUDGEMENT_VALUE_TYPE);
	check(&work, pg_evidence_subject(selected_type)->core, pg_evidence_subject(av)->core);
	size_t proof_count = typing.proofs.count, term_count = graph.terms.count;
	assert(pg_prove_type_case(&typing, &classifiers, formation, parameters, value, 1, &branch_type) == selected_type);
	assert(typing.proofs.count == proof_count && graph.terms.count == term_count);
	struct pg_derivation_parameters no_parameters = {0};
	const struct pg_evidence *type_premises[] = {formation, parameters, value, branch_type};
	assert(pg_prove_derivation(&typing, &classifiers, PG_TYPE_CASE, &no_parameters, 4, type_premises) == selected_type);
	assert(!pg_prove_type_case(&typing, &classifiers, formation, parameters, value, 0, NULL));
	assert(!pg_prove_type_case(&typing, &classifiers, formation, parameters, value, 1, &branch));
	assert(!pg_prove_type_case(&typing, &classifiers, formation, parameters, xv, 1, &branch_type));
	assert(!pg_prove_type_case(&typing, &classifiers, formation, parameters, value, 1, &u));
	const struct pg_object *packet = pg_binder(&graph);
	const struct pg_evidence *packet_context = pg_prove_context_extension(&typing, xc,
		packet, pg_evidence_premise(value, 0));
	/* Refining the packet replaces A and x too, and retypes a dependent
	 * function bound after it. Keeping that function at the old A is invalid. */
	const struct pg_evidence *function_domain = pg_prove_context_extension(&typing, packet_context,
		pg_binder(&graph), pg_prove_projection(&typing, packet_context, pg_evidence_premise(value, 0)));
	const struct pg_evidence *function_type = pg_prove_pi(&typing, &classifiers, function_domain,
		pg_prove_return_type(&typing, &classifiers, pg_prove_variable(&typing, function_domain, a)));
	const struct pg_object *consumer = pg_binder(&graph);
	const struct pg_evidence *consumer_context = pg_prove_context_extension(&typing, packet_context, consumer,
		pg_prove_thunk_type(&typing, &classifiers, function_type));
	const struct pg_evidence *consumer_packet = pg_prove_variable(&typing, consumer_context, packet);
	const struct pg_evidence *refinement = pg_prove_constructor_refinement(&typing, &classifiers,
		consumer_context, consumer_packet, constructor);
	assert(refinement);
	const struct pg_evidence *other_refinement = pg_prove_constructor_refinement(&typing, &classifiers,
		consumer_context, consumer_packet, constructor);
	const struct pg_evidence *factor = pg_prove_refinement_factor(&typing, refinement, other_refinement, packet);
	assert(factor);
	common_rule(&typing, &classifiers, factor);
	const struct pg_evidence *composite = pg_prove_substitution_compose(&typing, refinement, factor);
	assert(composite);
	for (size_t i = 2; i < pg_evidence_premise_count(composite); ++i)
		assert(pg_alpha_equal(pg_evidence_subject(pg_evidence_premise(composite, i))->core,
			pg_evidence_subject(pg_evidence_premise(other_refinement, i))->core) == 1);
	assert(!pg_prove_refinement_factor(&typing, refinement, parameters, packet));
	assert(!pg_prove_refinement_factor(&typing, refinement, other_refinement, consumer));
	const struct pg_evidence *refined_a = pg_substitution_image(&typing, refinement, a);
	const struct pg_evidence *refined_packet = pg_substitution_image(&typing, refinement, packet);
	const struct pg_evidence *refined_consumer = pg_substitution_image(&typing, refinement, consumer);
	assert(refined_a && refined_packet && refined_consumer);
	assert(pg_evidence_subject(refined_a)->core != pg_evidence_subject(av)->core);
	assert(!pg_prove_projection(&typing, pg_evidence_premise(refinement, 1),
		pg_prove_variable(&typing, consumer_context, consumer)));
	const struct pg_evidence *applied = pg_prove_application(&typing,
		pg_prove_force(&typing, refined_consumer), refined_packet);
	assert(applied && pg_alpha_equal(pg_evidence_classifier(applied),
		pg_return_type(&classifiers, pg_evidence_subject(refined_a)->core)) == 1);
	assert(!pg_prove_constructor_refinement(&typing, &classifiers, xc, value, constructor));
	assert(!pg_prove_constructor_refinement(&typing, &classifiers, consumer_context, consumer_packet, pg_binder(&graph)));
	const struct pg_evidence *consumer_parameters = pg_prove_substitution_projection(&typing, empty, consumer_context);
	const struct pg_evidence *consumer_mc = pg_prove_inductive_motive_context(&typing, formation,
		consumer_parameters, pg_binder(&graph));
	const struct pg_context *consumer_indices = pg_evidence_context(consumer_mc)->parent;
	const struct pg_evidence *consumer_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_variable(&typing, consumer_mc, consumer_indices->parent->binder));
	const struct pg_evidence *consumer_branch = pg_prove_projection(&typing, consumer_context, branch);
	const struct pg_evidence *consumer_match = pg_prove_match(&typing, &classifiers, formation,
		consumer_parameters, consumer_packet, consumer_mc, consumer_motive, 1, &consumer_branch);
	const struct pg_evidence *refined_match = pg_prove_elimination_reindex(&typing, &classifiers, refinement, consumer_match);
	assert(refined_match);
	assert(!pg_prove_elimination_body(&typing, &classifiers, consumer_match));
	const struct pg_evidence *refined_body = pg_prove_elimination_body(&typing, &classifiers, refined_match);
	assert(refined_body);
	check(&work, pg_evidence_subject(refined_match)->core, pg_evidence_subject(refined_body)->core);
	common_rule(&typing, &classifiers, refined_body);
	check(&work, pg_evidence_subject(refined_match)->core, pg_evidence_subject(pg_prove_return(&typing,
		&classifiers, pg_substitution_image(&typing, refinement, x)))->core);
	common_rule(&typing, &classifiers, refinement);
	common_rule(&typing, &classifiers, refined_match);
	/* Match may return a function whose argument depends on the generic
	 * indices. Specialize the pending argument along with the scrutinee. */
	const struct pg_evidence *generic_packet = pg_evidence_premise(consumer_mc, 1);
	const struct pg_evidence *generic_argument = pg_prove_context_extension(&typing, consumer_mc,
		pg_binder(&graph), pg_prove_projection(&typing, consumer_mc, generic_packet));
	const struct pg_evidence *generic_function = pg_prove_pi(&typing, &classifiers, generic_argument,
		pg_prove_return_type(&typing, &classifiers,
			pg_prove_variable(&typing, generic_argument, consumer_indices->parent->binder)));
	const struct pg_evidence *generic_consumer = pg_prove_context_extension(&typing, consumer_mc,
		pg_binder(&graph), pg_prove_thunk_type(&typing, &classifiers, generic_function));
	const struct pg_evidence *function_motive = pg_prove_pi(&typing, &classifiers, generic_consumer,
		pg_prove_projection(&typing, generic_consumer, consumer_motive));
	const struct pg_evidence *consumer_fields = pg_prove_constructor_scope(&typing, formation, constructor, consumer_parameters);
	const struct pg_evidence *consumer_field_context = pg_evidence_premise(consumer_fields, 1);
	const struct pg_evidence *field_values[] = {pg_substitution_image(&typing, consumer_fields, a),
		pg_substitution_image(&typing, consumer_fields, x)};
	const struct pg_evidence *field_packet = pg_prove_constructor(&typing, formation, constructor,
		pg_prove_substitution_projection(&typing, empty, consumer_field_context), 2, field_values);
	const struct pg_evidence *field_function_type = pg_prove_inductive_motive_at(&typing, &classifiers, formation,
		consumer_parameters, consumer_mc, function_motive, consumer_field_context, field_packet);
	const struct pg_object *field_consumer = pg_binder(&graph);
	const struct pg_evidence *field_consumer_context = pg_prove_context_extension(&typing, consumer_field_context,
		field_consumer, pg_prove_pi_domain(&typing, field_function_type));
	const struct pg_evidence *function_branch = pg_prove_application(&typing,
		pg_prove_force(&typing, pg_prove_variable(&typing, field_consumer_context, field_consumer)),
		pg_prove_projection(&typing, field_consumer_context, field_packet));
	function_branch = pg_prove_abstract(&typing, &classifiers, consumer_context, field_consumer_context, function_branch);
	const struct pg_evidence *function_match = pg_prove_match(&typing, &classifiers, formation, consumer_parameters,
		consumer_packet, consumer_mc, function_motive, 1, &function_branch);
	assert(function_match);
	const struct pg_evidence *specialized_function = pg_prove_elimination_reindex(&typing, &classifiers, refinement, function_match);
	const struct pg_evidence *specialized_application = pg_prove_application(&typing, specialized_function, refined_consumer);
	assert(specialized_application);
	check(&work, pg_evidence_subject(specialized_application)->core, pg_evidence_subject(applied)->core);
	const struct pg_evidence *function_body = pg_prove_elimination_body(&typing, &classifiers, specialized_function);
	assert(function_body);
	const struct pg_evidence *body_application = pg_prove_application_body(&typing, function_body, refined_consumer);
	assert(body_application);
	check(&work, pg_evidence_subject(body_application)->core, pg_evidence_subject(applied)->core);
	common_rule(&typing, &classifiers, body_application);
	const struct pg_evidence *result_type = pg_prove_classifier(&typing, &classifiers, consumer_context, consumer_match);
	const struct pg_evidence *assembled = pg_prove_refined_match(&typing, &classifiers, consumer_context,
		consumer_packet, result_type, 1, &refinement, &body_application);
	assert(assembled && pg_alpha_equal(pg_evidence_classifier(assembled), pg_evidence_subject(result_type)->core) == 1);
	common_rule(&typing, &classifiers, assembled);
	const struct pg_evidence *assembled_branch = pg_prove_reindex(&typing, refinement, assembled);
	assert(assembled_branch);
	check(&work, pg_evidence_subject(assembled_branch)->core, pg_evidence_subject(applied)->core);
	assert(!pg_prove_refined_match(&typing, &classifiers, consumer_context,
		consumer_packet, result_type, 0, NULL, NULL));
	assert(!pg_prove_refined_match(&typing, &classifiers, consumer_context,
		consumer_packet, result_type, 1, &refinement, &consumer_match));
	const struct pg_evidence *wrong_refinement = pg_prove_substitution_projection(&typing, consumer_context, consumer_context);
	assert(!pg_prove_refined_match(&typing, &classifiers, consumer_context,
		consumer_packet, result_type, 1, &wrong_refinement, &consumer_match));
	const struct pg_object *predicate = pg_binder(&graph);
	const struct pg_evidence *predicate_domain = pg_prove_context_extension(&typing, consumer_context,
		pg_binder(&graph), pg_prove_variable(&typing, consumer_context, a));
	const struct pg_evidence *predicate_context = pg_prove_family_context_extension(&typing,
		consumer_context, predicate, predicate_domain, pg_prove_projection(&typing, predicate_domain, u));
	const struct pg_evidence *family_refinement = pg_prove_constructor_refinement(&typing, &classifiers,
		predicate_context, pg_prove_variable(&typing, predicate_context, packet), constructor);
	assert(family_refinement);
	const struct pg_evidence *family_image = pg_substitution_image(&typing, family_refinement, predicate);
	assert(family_image && pg_evidence_judgement(family_image) == PG_JUDGEMENT_TYPE_FAMILY);
	assert(pg_prove_family_application(&typing, family_image, pg_substitution_image(&typing, family_refinement, x)));
	const struct pg_evidence *property = pg_prove_family_application(&typing,
		pg_prove_variable(&typing, predicate_context, predicate), pg_prove_variable(&typing, predicate_context, x));
	const struct pg_object *property_binder = pg_binder(&graph);
	const struct pg_evidence *property_context = pg_prove_context_extension(&typing, predicate_context, property_binder, property);
	const struct pg_evidence *property_packet = pg_prove_variable(&typing, property_context, packet);
	const struct pg_evidence *property_refinement = pg_prove_constructor_refinement(&typing, &classifiers,
		property_context, property_packet, constructor);
	const struct pg_evidence *property_body = pg_prove_return(&typing, &classifiers,
		pg_substitution_image(&typing, property_refinement, property_binder));
	const struct pg_evidence *property_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, property_context, property));
	const struct pg_evidence *property_match = pg_prove_refined_match(&typing, &classifiers, property_context,
		property_packet, property_motive, 1, &property_refinement, &property_body);
	assert(property_match);
	common_rule(&typing, &classifiers, property_match);
	check(&work, pg_evidence_subject(pg_prove_reindex(&typing, property_refinement, property_match))->core,
		pg_evidence_subject(property_body)->core);
	const struct pg_evidence *open_parameters = pg_prove_substitution_projection(&typing, empty, packet_context);
	const struct pg_evidence *open_branch = pg_prove_projection(&typing, packet_context, branch_type);
	const struct pg_evidence *neutral = pg_prove_type_case(&typing, &classifiers,
		formation, open_parameters, pg_prove_variable(&typing, packet_context, packet), 1, &open_branch);
	assert(neutral);
	const struct pg_evidence *replace = pg_prove_substitution_pair(&typing,
		pg_prove_substitution_projection(&typing, xc, xc), packet_context, value);
	const struct pg_evidence *instantiated = pg_prove_reindex(&typing, replace, neutral);
	assert(instantiated && pg_evidence_judgement(instantiated) == PG_JUDGEMENT_VALUE_TYPE);
	check(&work, pg_evidence_subject(instantiated)->core, pg_evidence_subject(av)->core);
	/* A fixed-fiber motive cannot replace the generic index telescope. */
	const struct pg_evidence *fixed = pg_prove_context_extension(&typing, xc, pg_binder(&graph),
		pg_evidence_premise(value, 0));
	const struct pg_evidence *fixed_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, fixed, av));
	assert(fixed && fixed_motive);
	assert(!pg_prove_match(&typing, &classifiers, formation, parameters, value, fixed, fixed_motive, 1, &branch));
	const struct pg_evidence *wrong = pg_prove_abstract(&typing, &classifiers, xc, field_context,
		pg_prove_return(&typing, &classifiers, pg_prove_projection(&typing, field_context, xv)));
	assert(wrong && !pg_prove_match(&typing, &classifiers, formation, parameters, value, mc, motive, 1, &wrong));
	pg_whnf_work_destroy(&work);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
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

static void index_paths(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *nat, const struct pg_evidence *zero,
	const struct pg_evidence *empty_type, const struct pg_object *succ)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *n = pg_binder(typing->graph), *m = pg_binder(typing->graph);
	const struct pg_evidence *nc = pg_prove_context_extension(typing, empty, n, nat);
	const struct pg_evidence *mc = pg_prove_context_extension(typing, nc, m, pg_prove_projection(typing, nc, nat));
	/* F (Family (succ k)) generalizes the whole pattern, without inventing
	 * an inverse k for an arbitrary n. Repeated pattern images are ambiguous. */
	const struct pg_object *self = pg_binder(typing->graph), *index = pg_binder(typing->graph);
	const struct pg_evidence *sc = pg_prove_family_context_extension(typing, empty, self, nc,
		pg_prove_universe(typing, classifiers, nc, 0));
	const struct pg_evidence *indices = pg_prove_context_extension(typing, sc, index,
		pg_prove_projection(typing, sc, nat));
	const struct pg_data_schema *schema = pg_data_schema(typing,
		pg_data_signature(typing, sc, indices), 0, NULL);
	const struct pg_evidence *family = pg_prove_inductive_type(typing, classifiers, schema);
	assert(family);
	const struct pg_evidence *fields = pg_prove_context_extension(typing, empty, m, nat);
	const struct pg_evidence *field = pg_prove_variable(typing, fields, m);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(typing, empty, fields);
	const struct pg_evidence *successor = pg_prove_constructor(typing, nat, succ, parameters, 1, &field);
	const struct pg_evidence *pattern = pg_prove_substitution(typing, nc, fields, 1, &successor);
	const struct pg_evidence *body = pg_prove_return_type(typing, classifiers,
		pg_prove_family_application(typing, pg_prove_projection(typing, fields, family), successor));
	const struct pg_evidence *generalized = pg_prove_pattern_type(typing, classifiers, empty, pattern, body);
	const struct pg_evidence *expected = pg_prove_return_type(typing, classifiers,
		pg_prove_family_application(typing, pg_prove_projection(typing, nc, family), pg_prove_variable(typing, nc, n)));
	assert(generalized && expected);
	assert(pg_alpha_equal(pg_evidence_subject(generalized)->core, pg_evidence_subject(expected)->core) == 1);
	common_rule(typing, classifiers, generalized);
	const struct pg_evidence *back = pg_prove_reindex(typing, pattern, generalized);
	assert(back && pg_alpha_equal(pg_evidence_subject(back)->core, pg_evidence_subject(body)->core) == 1);
	/* A field at Family (succ m) cannot be replaced by one at Family n.
	 * It may still be discarded when the proposed result is independent. */
	const struct pg_object *p = pg_binder(typing->graph), *q = pg_binder(typing->graph);
	const struct pg_evidence *source_field = pg_prove_context_extension(typing, nc, p,
		pg_prove_return_content(typing, expected));
	const struct pg_evidence *target_field = pg_prove_context_extension(typing, fields, q,
		pg_prove_return_content(typing, body));
	const struct pg_evidence *target_value = pg_prove_variable(typing, target_field, q);
	const struct pg_evidence *extended_images[] = {
		pg_prove_projection(typing, target_field, successor), target_value};
	const struct pg_evidence *extended_pattern = pg_prove_substitution(typing,
		source_field, target_field, 2, extended_images);
	const struct pg_evidence *extended_body = pg_prove_projection(typing, target_field, body);
	const struct pg_evidence *extended_result = pg_prove_pattern_type(typing, classifiers,
		empty, extended_pattern, extended_body);
	assert(extended_result && pg_alpha_equal(pg_evidence_subject(extended_result)->core,
		pg_evidence_subject(expected)->core) == 1);
	assert(pg_evidence_context(extended_result) == pg_evidence_context(source_field));
	common_rule(typing, classifiers, extended_result);
	const struct pg_evidence *dependent_body = pg_prove_return_type(typing, classifiers,
		pg_prove_identity_type(typing, pg_prove_return_content(typing, extended_body), target_value, target_value));
	assert(dependent_body && !pg_prove_pattern_type(typing, classifiers, empty, extended_pattern, dependent_body));
	const struct pg_evidence *duplicate[] = {successor, successor};
	assert(!pg_prove_pattern_type(typing, classifiers, empty,
		pg_prove_substitution(typing, mc, fields, 2, duplicate), body));
	const struct pg_evidence *wrapped = body, *wrapped_expected = expected;
	for (size_t i = 0; i < 32; ++i) {
		wrapped = pg_prove_return_type(typing, classifiers, pg_prove_thunk_type(typing, classifiers, wrapped));
		wrapped_expected = pg_prove_return_type(typing, classifiers,
			pg_prove_thunk_type(typing, classifiers, wrapped_expected));
	}
	const struct pg_evidence *under_wrappers = pg_prove_pattern_type(typing, classifiers, empty, pattern, wrapped);
	assert(under_wrappers && pg_alpha_equal(pg_evidence_subject(under_wrappers)->core,
		pg_evidence_subject(wrapped_expected)->core) == 1);
	common_rule(typing, classifiers, under_wrappers);
	const struct pg_object *argument = pg_binder(typing->graph);
	const struct pg_evidence *argument_scope = pg_prove_context_extension(typing, fields, argument,
		pg_prove_projection(typing, fields, nat));
	const struct pg_evidence *pi_body = pg_prove_pi(typing, classifiers, argument_scope,
		pg_prove_projection(typing, argument_scope, wrapped));
	const struct pg_evidence *pi_expected_scope = pg_prove_context_extension(typing, nc, argument,
		pg_prove_projection(typing, nc, nat));
	const struct pg_evidence *pi_expected = pg_prove_pi(typing, classifiers, pi_expected_scope,
		pg_prove_projection(typing, pi_expected_scope, wrapped_expected));
	const struct pg_evidence *under_pi = pg_prove_pattern_type(typing, classifiers, empty, pattern, pi_body);
	assert(under_pi && pg_alpha_equal(pg_evidence_subject(under_pi)->core,
		pg_evidence_subject(pi_expected)->core) == 1);
	common_rule(typing, classifiers, under_pi);
	/* Removing an unused domain must retain constructed index images, even
	 * when their value proofs were built inside that larger context. */
	const struct pg_evidence *unused = pg_prove_context_extension(typing, fields,
		pg_binder(typing->graph), pg_prove_projection(typing, fields, empty_type));
	const struct pg_evidence *large_parameters = pg_prove_substitution_projection(typing, empty, unused);
	const struct pg_evidence *large_value = pg_prove_projection(typing, unused, field), *small_value = field;
	for (size_t i = 0; i < 64; ++i) {
		large_value = pg_prove_constructor(typing, nat, succ, large_parameters, 1, &large_value);
		small_value = pg_prove_constructor(typing, nat, succ, parameters, 1, &small_value);
		assert(large_value && small_value);
	}
	const struct pg_evidence *large_type = pg_prove_return_type(typing, classifiers,
		pg_prove_family_application(typing, pg_prove_projection(typing, unused, family), large_value));
	const struct pg_evidence *smaller = pg_prove_return_content(typing,
		pg_prove_pi_constant_codomain(typing, pg_prove_pi(typing, classifiers, unused, large_type)));
	struct pg_inductive_instance recovered, again;
	assert(smaller && pg_inductive_instance(typing, smaller, &recovered));
	assert(recovered.schema == schema);
	const struct pg_evidence *last = pg_evidence_premise(recovered.indices,
		pg_evidence_premise_count(recovered.indices) - 1);
	assert(pg_evidence_context(last) == pg_evidence_context(fields));
	assert(pg_evidence_subject(last)->core == pg_evidence_subject(small_value)->core);
	assert(pg_inductive_instance(typing, smaller, &again) && recovered.indices == again.indices);
	common_rule(typing, classifiers, last);
	const struct pg_evidence *dependent = pg_prove_context_extension(typing, fields, pg_binder(typing->graph),
		pg_prove_projection(typing, fields, nat));
	const struct pg_evidence *local = pg_prove_variable(typing, dependent, pg_evidence_context(dependent)->binder);
	const struct pg_evidence *local_type = pg_prove_return_type(typing, classifiers,
		pg_prove_family_application(typing, pg_prove_projection(typing, dependent, family), local));
	assert(!pg_prove_pi_constant_codomain(typing, pg_prove_pi(typing, classifiers, dependent, local_type)));
	/* A dependent Pi may survive removal of an unrelated ambient field.
	 * Lift the context action under its binders before applying arguments. */
	for (size_t arity = 1; arity <= 2; ++arity) {
		const struct pg_object *x = pg_binder(typing->graph);
		const struct pg_evidence *xc = pg_prove_context_extension(typing, unused, x,
			pg_prove_projection(typing, unused, nat));
		const struct pg_evidence *result_type = pg_prove_return_type(typing, classifiers,
			pg_prove_family_application(typing, pg_prove_projection(typing, xc, family),
				pg_prove_variable(typing, xc, x)));
		if (arity == 2) {
			const struct pg_evidence *yc = pg_prove_context_extension(typing, xc, pg_binder(typing->graph),
				pg_prove_projection(typing, xc, nat));
			result_type = pg_prove_pi(typing, classifiers, yc, pg_prove_projection(typing, yc, result_type));
		}
		const struct pg_evidence *pi = pg_prove_pi_constant_codomain(typing,
			pg_prove_pi(typing, classifiers, unused, pg_prove_pi(typing, classifiers, xc, result_type)));
		assert(pi && !pg_prove_pi_codomain(typing, pi,
			pg_prove_type_value(typing, pg_prove_projection(typing, fields, nat))));
		for (size_t mode = 0; mode < 3; ++mode) {
			const struct pg_evidence *context = mode == 1 ? unused : mode == 2 ? empty : fields;
			const struct pg_evidence *actual = mode == 2 ? zero : pg_prove_projection(typing, context, field);
			const struct pg_evidence *applied = mode == 2
				? pg_prove_reindex(typing, pg_prove_substitution(typing, fields, empty, 1, &zero), pi)
				: pg_prove_projection(typing, context, pi);
			applied = pg_prove_pi_codomain(typing, applied, actual);
			if (arity == 2) applied = pg_prove_pi_codomain(typing, applied, pg_prove_projection(typing, context, zero));
			applied = pg_prove_return_content(typing, applied);
			assert(applied && pg_inductive_instance(typing, applied, &recovered));
			assert(recovered.schema == schema && pg_evidence_context(recovered.parameters) == pg_evidence_context(context));
			last = pg_evidence_premise(recovered.indices, pg_evidence_premise_count(recovered.indices) - 1);
			assert(pg_alpha_equal(pg_evidence_subject(last)->core, pg_evidence_subject(actual)->core) == 1);
			common_rule(typing, classifiers, applied);
			common_rule(typing, classifiers, last);
		}
	}
	for (size_t injection = 0; injection < 2; ++injection) {
		const struct pg_evidence *nv = pg_prove_variable(typing, mc, n), *mv = pg_prove_variable(typing, mc, m);
		const struct pg_evidence *parameters = pg_prove_substitution_projection(typing, empty, mc);
		const struct pg_evidence *left = injection ? pg_prove_constructor(typing, nat, succ, parameters, 1, &nv)
			: pg_prove_projection(typing, mc, zero);
		const struct pg_evidence *right = pg_prove_constructor(typing, nat, succ, parameters, 1, &mv);
		const struct pg_evidence *identity = pg_prove_identity_type(typing,
			pg_prove_projection(typing, mc, nat), left, right);
		const struct pg_object *p = pg_binder(typing->graph), *q = pg_binder(typing->graph);
		const struct pg_evidence *context = pg_prove_context_extension(typing, mc, p, identity);
		context = pg_prove_context_extension(typing, context, q, pg_prove_projection(typing, context, identity));
		assert(context);
		left = pg_prove_projection(typing, context, left);
		right = pg_prove_projection(typing, context, right);
		nv = pg_prove_variable(typing, context, n); mv = pg_prove_variable(typing, context, m);
		const struct pg_evidence *input = injection
			? pg_prove_reflexivity(typing, pg_prove_projection(typing, context, nat), nv)
			: pg_prove_projection(typing, context, zero);
		const struct pg_evidence *expected = injection
			? pg_prove_identity_type(typing, pg_prove_projection(typing, context, nat), nv, mv)
			: pg_prove_projection(typing, context, empty_type);
		/* D zero = Nat, D (succ k) = Empty gives disjointness.
		 * D zero = Empty, D (succ k) = Id Nat n k gives injectivity.
		 * Both are ordinary type cases, not special Nat or no-confusion rules. */
		const struct pg_object *z = pg_binder(typing->graph);
		const struct pg_evidence *zc = pg_prove_context_extension(typing, context, z,
			pg_prove_projection(typing, context, nat));
		parameters = pg_prove_substitution_projection(typing, empty, zc);
		const struct pg_evidence *fields = pg_prove_constructor_scope(typing, nat, succ, parameters);
		const struct pg_evidence *fc = pg_evidence_premise(fields, 1);
		const struct pg_evidence *k = pg_evidence_premise(fields, pg_evidence_premise_count(fields) - 1);
		const struct pg_evidence *branch = injection
			? pg_prove_identity_type(typing, pg_prove_projection(typing, fc, nat),
				pg_prove_projection(typing, fc, nv), k)
			: pg_prove_projection(typing, fc, empty_type);
		const struct pg_evidence *branches[] = {
			pg_prove_projection(typing, zc, injection ? empty_type : nat),
			pg_prove_family_abstraction(typing, fc, branch)
		};
		const struct pg_evidence *family = pg_prove_type_case(typing, classifiers, nat,
			parameters, pg_prove_variable(typing, zc, z), 2, branches);
		assert(family);
		const struct pg_evidence *prefix = pg_prove_substitution_projection(typing, context, context);
		const struct pg_evidence *ls = pg_prove_substitution_pair(typing, prefix, zc, left);
		const struct pg_evidence *rs = pg_prove_substitution_pair(typing, prefix, zc, right);
		const struct pg_evidence *paths[] = {pg_prove_variable(typing, context, p), pg_prove_variable(typing, context, q)};
		const struct pg_evidence *results[2] = {0};
		for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
			struct pg_whnf_work work;
			struct pg_synthesis synthesis;
			assert(!pg_whnf_work_init(&work, typing->graph));
			assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
			struct pg_synthesis_job *fj = pg_synthesis_evidence(&synthesis, family);
			struct pg_synthesis_job *vj = pg_synthesis_evidence(&synthesis, input);
			for (size_t i = 0; i < 2; ++i) {
				struct pg_synthesis_job *pj = pg_synthesis_evidence(&synthesis, paths[i]);
				struct pg_synthesis_job *job = pg_synthesis_family_transport_jobs(&synthesis,
					fj, ls, rs, 1, &pj, vj, PG_IDENTITY_RIGHT);
				assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
				assert(!pg_synthesis_result(job));
				const struct pg_evidence *result = solve_index_proof(&synthesis, job, chunk, PG_SYNTHESIS_DONE);
				assert(pg_evidence_rule(result) == PG_IDENTITY_TRANSPORT);
				const struct pg_evidence *checked = solve_index_proof(&synthesis,
					pg_synthesis_expect(&synthesis, job, pg_synthesis_evidence(&synthesis, expected)),
					chunk, PG_SYNTHESIS_DONE);
				assert(pg_evidence_subject(checked)->core == pg_evidence_subject(result)->core);
				assert(pg_evidence_classifier(checked) == pg_evidence_subject(expected)->core);
				/* Discharge the assumed paths into a closed function, rather than
				 * claiming an inhabitant of Empty or an unconditional n=m. */
				const struct pg_evidence *theorem = pg_prove_abstract(typing, classifiers, empty, context,
					pg_prove_return(typing, classifiers, checked));
				assert(theorem && !pg_evidence_context(theorem));
				if (!injection) {
					struct pg_synthesis_job *cj = pg_synthesis_evidence(&synthesis, context);
					struct pg_synthesis_job *lj = pg_synthesis_evidence(&synthesis, left);
					struct pg_synthesis_job *rj = pg_synthesis_evidence(&synthesis, right);
					struct pg_synthesis_job *tj = pg_synthesis_evidence(&synthesis, expected);
					struct pg_synthesis_job *derived = pg_synthesis_disjoint_transport(&synthesis, cj, lj, rj, pj, vj, tj);
					assert(derived && !pg_synthesis_result(derived));
					const struct pg_evidence *eliminated = solve_index_proof(&synthesis, derived, chunk, PG_SYNTHESIS_DONE);
					assert(pg_evidence_classifier(eliminated) == pg_evidence_subject(expected)->core);
					assert(pg_term_independent(pg_evidence_subject(eliminated)->core, i ? q : p) == 0);
					assert(pg_term_independent(pg_evidence_subject(eliminated)->core, i ? p : q) == 1);
					assert(pg_evidence_rule(eliminated) == PG_TYPE_CONVERSION);
					const struct pg_evidence *transported = pg_evidence_premise(eliminated, 0);
					assert(pg_evidence_rule(transported) == PG_IDENTITY_TRANSPORT);
					common_rule(typing, classifiers, transported);
					const struct pg_evidence *closed = pg_prove_abstract(typing, classifiers, empty, context,
						pg_prove_return(typing, classifiers, eliminated));
					assert(closed && !pg_evidence_context(closed));
					size_t before = typing->proofs.count, terms = typing->graph->terms.count;
					assert(derived == pg_synthesis_disjoint_transport(&synthesis, cj, lj, rj, pj, vj, tj));
					assert(before == typing->proofs.count && terms == typing->graph->terms.count);
					solve_index_proof(&synthesis, pg_synthesis_disjoint_transport(&synthesis, cj, rj, lj, pj, vj, tj),
						chunk, PG_SYNTHESIS_REJECTED);
					solve_index_proof(&synthesis, pg_synthesis_disjoint_transport(&synthesis, cj, lj, lj, pj, vj, tj),
						chunk, PG_SYNTHESIS_UNSUPPORTED);
					const struct pg_evidence *diagonal = pg_prove_reflexivity(typing,
						pg_prove_projection(typing, context, nat), left);
					solve_index_proof(&synthesis, pg_synthesis_disjoint_transport(&synthesis, cj, lj, rj,
						pg_synthesis_evidence(&synthesis, diagonal), vj, tj), chunk, PG_SYNTHESIS_REJECTED);
					assert(!pg_synthesis_disjoint_transport(&synthesis, cj, lj, rj, NULL, vj, tj));
					struct pg_synthesis_job *computed = pg_synthesis_evidence(&synthesis,
						pg_prove_return(typing, classifiers, input));
					solve_index_proof(&synthesis, pg_synthesis_disjoint_transport(&synthesis, cj, lj, rj, pj, computed, tj),
						chunk, PG_SYNTHESIS_REJECTED);
					const struct pg_object *rp = pg_binder(typing->graph);
					const struct pg_evidence *reverse_context = pg_prove_context_extension(typing, context, rp,
						pg_prove_identity_type(typing, pg_prove_projection(typing, context, nat), right, left));
					struct pg_synthesis_job *reverse = pg_synthesis_disjoint_transport(&synthesis,
						pg_synthesis_evidence(&synthesis, reverse_context),
						pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, reverse_context, right)),
						pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, reverse_context, left)),
						pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, reverse_context, rp)),
						pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, reverse_context, input)),
						pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, reverse_context, expected)));
					solve_index_proof(&synthesis, reverse, chunk, PG_SYNTHESIS_DONE);
				}
				if (injection) {
					struct pg_inductive_instance instance;
					assert(pg_inductive_instance(typing, nat, &instance));
					const struct pg_object *field = pg_evidence_context(pg_data_schema_fields(instance.schema, succ))->binder;
					struct pg_synthesis_job *cj = pg_synthesis_evidence(&synthesis, context);
					struct pg_synthesis_job *lj = pg_synthesis_evidence(&synthesis, left);
					struct pg_synthesis_job *rj = pg_synthesis_evidence(&synthesis, right);
					struct pg_synthesis_job *nj = pg_synthesis_evidence(&synthesis, nv);
					struct pg_synthesis_job *mj = pg_synthesis_evidence(&synthesis, mv);
					struct pg_synthesis_job *derived = pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, pj, field, nj, mj);
					const struct pg_evidence *proof = solve_index_proof(&synthesis, derived, chunk, PG_SYNTHESIS_DONE);
					assert(pg_evidence_classifier(proof) == pg_evidence_subject(expected)->core);
					assert(!pg_term_independent(pg_evidence_subject(proof)->core, i ? q : p));
					assert(pg_term_independent(pg_evidence_subject(proof)->core, i ? p : q) == 1);
					const struct pg_evidence *transported = pg_evidence_premise(proof, 0);
					assert(pg_evidence_rule(transported) == PG_IDENTITY_TRANSPORT);
					common_rule(typing, classifiers, transported);
					size_t before = typing->proofs.count, terms = typing->graph->terms.count;
					assert(derived == pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, pj, field, nj, mj));
					assert(before == typing->proofs.count && terms == typing->graph->terms.count);
					solve_index_proof(&synthesis, pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, pj,
						field, mj, nj), chunk, PG_SYNTHESIS_REJECTED);
					solve_index_proof(&synthesis, pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, pj,
						pg_binder(typing->graph), nj, mj), chunk, PG_SYNTHESIS_REJECTED);
					assert(!pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, pj, NULL, nj, mj));
					assert(!pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, NULL, field, nj, mj));
					const struct pg_evidence *refl = pg_prove_reflexivity(typing,
						pg_prove_projection(typing, context, nat), left);
					struct pg_synthesis_job *rfl = pg_synthesis_evidence(&synthesis, refl);
					solve_index_proof(&synthesis, pg_synthesis_constructor_field_identity(&synthesis, cj, lj, rj, rfl,
						field, nj, mj), chunk, PG_SYNTHESIS_REJECTED);
					struct pg_synthesis_job *diagonal = pg_synthesis_constructor_field_identity(&synthesis,
						cj, lj, lj, rfl, field, nj, nj);
					const struct pg_evidence *normal = solve_index_proof(&synthesis,
						pg_synthesis_normalize_jobs(&synthesis, cj, diagonal, PG_REDUCTION_NF), chunk, PG_SYNTHESIS_DONE);
					assert(pg_alpha_equal(pg_evidence_subject(normal)->core, pg_evidence_subject(input)->core) == 1);
				}
				if (chunk == 1) results[i] = checked;
				else assert(pg_alpha_equal(pg_evidence_subject(checked)->core, pg_evidence_subject(results[i])->core) == 1);
				size_t proofs = typing->proofs.count, terms = typing->graph->terms.count;
				assert(pg_synthesis_family_transport_jobs(&synthesis, fj, ls, rs, 1, &pj, vj, PG_IDENTITY_RIGHT) == job);
				assert(typing->proofs.count == proofs && typing->graph->terms.count == terms);
				common_rule(typing, classifiers, result);
			}
			assert(pg_evidence_subject(results[0])->core != pg_evidence_subject(results[1])->core);
			const struct pg_evidence *refl = pg_prove_reflexivity(typing, pg_prove_projection(typing, context, nat), left);
			struct pg_synthesis_job *refl_job = pg_synthesis_evidence(&synthesis, refl);
			for (unsigned direction = PG_IDENTITY_RIGHT; direction <= PG_IDENTITY_LEFT; ++direction) {
				struct pg_synthesis_job *diagonal = pg_synthesis_family_transport_jobs(&synthesis,
					fj, ls, ls, 1, &refl_job, vj, direction);
				solve_index_proof(&synthesis, diagonal, chunk, PG_SYNTHESIS_DONE);
				const struct pg_evidence *reduced = solve_index_proof(&synthesis,
					pg_synthesis_normalize_jobs(&synthesis, pg_synthesis_evidence(&synthesis, context),
						diagonal, PG_REDUCTION_NF), chunk, PG_SYNTHESIS_DONE);
				assert(pg_alpha_equal(pg_evidence_subject(reduced)->core, pg_evidence_subject(input)->core) == 1);
			}
			/* Distinct constructors do not themselves supply a path. Neither a
			 * reversed boundary nor refl of one endpoint proves the required Eq. */
			struct pg_synthesis_job *pj = pg_synthesis_evidence(&synthesis, paths[0]);
			solve_index_proof(&synthesis, pg_synthesis_family_transport_jobs(&synthesis,
				fj, rs, ls, 1, &pj, vj, PG_IDENTITY_RIGHT), chunk, PG_SYNTHESIS_REJECTED);
			solve_index_proof(&synthesis, pg_synthesis_family_transport_jobs(&synthesis,
				fj, ls, rs, 1, &refl_job, vj, PG_IDENTITY_RIGHT), chunk, PG_SYNTHESIS_REJECTED);
			struct pg_synthesis_job *wrong = pg_synthesis_evidence(&synthesis,
				pg_prove_return(typing, classifiers, input));
			solve_index_proof(&synthesis, pg_synthesis_family_transport_jobs(&synthesis,
				fj, ls, rs, 1, &pj, wrong, PG_IDENTITY_RIGHT), chunk, PG_SYNTHESIS_REJECTED);
			struct pg_synthesis_job *computation_type = pg_synthesis_evidence(&synthesis,
				pg_prove_return_type(typing, classifiers, family));
			solve_index_proof(&synthesis, pg_synthesis_family_transport_jobs(&synthesis,
				computation_type, ls, rs, 1, &pj, vj, PG_IDENTITY_RIGHT), chunk, PG_SYNTHESIS_REJECTED);
			assert(!pg_synthesis_family_transport_jobs(&synthesis, fj, ls, rs, 1, NULL, vj, PG_IDENTITY_RIGHT));
			assert(!pg_synthesis_family_transport_jobs(&synthesis, fj, ls, rs, 1, &pj, vj, 2));
			pg_synthesis_destroy(&synthesis);
			pg_whnf_work_destroy(&work);
		}
	}
	puts("index paths: constructor disjointness/injectivity via type-case action and checked transport passed");
}

static void constructor_field_paths(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *nat)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *self = pg_prove_context_extension(typing, empty, pg_binder(typing->graph),
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_object *fields[] = {pg_binder(typing->graph), pg_binder(typing->graph)};
	const struct pg_evidence *fc = self;
	for (size_t i = 0; i < 2; ++i)
		fc = pg_prove_context_extension(typing, fc, fields[i], pg_prove_projection(typing, fc, nat));
	const struct pg_evidence *cases[] = {parameter_result(typing, self, self), parameter_result(typing, self, fc)};
	const struct pg_data_schema *schema = pg_data_schema(typing, pg_data_signature(typing, self, self), 2, cases);
	const struct pg_evidence *pair = pg_prove_inductive_type(typing, classifiers, schema);
	assert(pair);
	const struct pg_object *ctor = pg_data_constructor(pg_data_schema_layout(schema), 1);
	const struct pg_object *n = pg_binder(typing->graph), *m = pg_binder(typing->graph), *p = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, n, nat);
	context = pg_prove_context_extension(typing, context, m, pg_prove_projection(typing, context, nat));
	const struct pg_evidence *values[] = {pg_prove_variable(typing, context, n), pg_prove_variable(typing, context, m)};
	const struct pg_evidence *reverse[] = {values[1], values[0]};
	const struct pg_evidence *params = pg_prove_substitution_projection(typing, empty, context);
	const struct pg_evidence *left = pg_prove_constructor(typing, pair, ctor, params, 2, values);
	const struct pg_evidence *right = pg_prove_constructor(typing, pair, ctor, params, 2, reverse);
	context = pg_prove_context_extension(typing, context, p,
		pg_prove_identity_type(typing, pg_prove_projection(typing, context, pair), left, right));
	assert(context);
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, typing->graph));
		assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job *jobs[2];
		for (size_t i = 0; i < 2; ++i) {
			const struct pg_evidence *lv = pg_prove_projection(typing, context, values[i]);
			const struct pg_evidence *rv = pg_prove_projection(typing, context, reverse[i]);
			jobs[i] = pg_synthesis_constructor_field_identity(&synthesis, pg_synthesis_evidence(&synthesis, context),
				pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, context, left)),
				pg_synthesis_evidence(&synthesis, pg_prove_projection(typing, context, right)),
				pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, p)), fields[i],
				pg_synthesis_evidence(&synthesis, lv), pg_synthesis_evidence(&synthesis, rv));
			const struct pg_evidence *proof = solve_index_proof(&synthesis, jobs[i], chunk, PG_SYNTHESIS_DONE);
			const struct pg_evidence *expected = pg_prove_identity_type(typing, pg_prove_projection(typing, context, nat), lv, rv);
			assert(pg_evidence_classifier(proof) == pg_evidence_subject(expected)->core);
			assert(pg_prove_abstract(typing, classifiers, empty, context, pg_prove_return(typing, classifiers, proof)));
		}
		assert(jobs[0] != jobs[1]);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	puts("constructor fields: exact telescope binders select distinct injectivity proofs through Solve");
}

static void schema_positivity(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	assert(!pg_classifiers_init(&classifiers, &graph));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *self = pg_binder(&graph), *x = pg_binder(&graph);
	/* An ordinary assumed type supplies test fields, not an admitted Self. */
	const struct pg_evidence *parameters = pg_prove_context_extension(&typing, empty, self, u);
	const struct pg_evidence *type = pg_prove_value_type(&typing, pg_prove_variable(&typing, parameters, self));
	const struct pg_evidence *fields = pg_prove_context_extension(&typing, parameters, x, type);
	const struct pg_evidence *negative = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_pi(&typing, &classifiers, fields,
			pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, fields, u))));
	const struct pg_evidence *bad_fields = pg_prove_context_extension(&typing, parameters, pg_binder(&graph), negative);
	const struct pg_data_signature *signature = pg_data_signature(&typing, parameters, parameters);
	const struct pg_evidence *recursive_result = parameter_result(&typing, parameters, fields);
	const struct pg_evidence *results[] = {parameter_result(&typing, parameters, parameters),
		recursive_result, recursive_result, parameter_result(&typing, parameters, bad_fields)};
	const struct pg_data_schema *good = pg_data_schema(&typing, signature, 3, results);
	const struct pg_data_schema *bad = pg_data_schema(&typing, signature, 4, results);
	const struct pg_data_schema *none = pg_data_schema(&typing, signature, 0, NULL);
	assert(good && bad && none);
	size_t proofs = typing.proofs.count, terms = graph.terms.count;
	assert(pg_data_schema_positive(good, self) == 1);
	assert(pg_data_schema_positive(bad, self) == 0);
	assert(pg_data_schema_positive(none, self) == 1);
	assert(pg_data_schema_positive(NULL, self) == -1);
	assert(pg_data_schema_positive(good, NULL) == -1);
	uint64_t level = UINT64_MAX;
	assert(!pg_data_schema_field_level(good, &level) && level == 0);
	assert(!pg_data_schema_field_level(bad, &level) && level == 1);
	assert(!pg_data_schema_field_level(none, &level) && level == 0);
	level = 42;
	assert(pg_data_schema_field_level(NULL, &level) == -1 && level == 42);
	assert(pg_data_schema_field_level(good, NULL) == -1);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	const struct pg_data_schema *nat_schema = pg_data_schema(&typing, signature, 2, results);
	const struct pg_evidence *nat = pg_prove_inductive_type(&typing, &classifiers, nat_schema);
	assert(nat && pg_evidence_rule(nat) == PG_INDUCTIVE_FORM);
	constructor_field_paths(&typing, &classifiers, nat);
	struct pg_inductive_instance recovered;
	assert(pg_inductive_instance(&typing, nat, &recovered));
	assert(recovered.schema == nat_schema && recovered.formation == nat);
	assert(pg_evidence_premise_count(recovered.parameters) == 2);
	{
		const struct pg_evidence *context = empty, *projected = nat;
		for (size_t i = 0; i < 32; ++i) {
			context = pg_prove_context_extension(&typing, context, pg_binder(&graph), projected);
			projected = pg_prove_projection(&typing, context, projected);
			assert(projected);
		}
		struct pg_inductive_recovery work;
		assert(!pg_inductive_recovery_init(&work, &typing, projected));
		assert(!pg_inductive_recovery_advance(&work, 0));
		assert(!pg_inductive_recovery_advance(&work, 32));
		assert(work.formation == nat && !work.map && !work.result.formation);
		assert(!pg_inductive_recovery_advance(&work, 33));
		assert(work.map && !work.frames && !work.result.formation);
		assert(pg_inductive_recovery_advance(&work, 1) == 1);
		struct pg_inductive_instance expected = work.result;
		assert(expected.formation == nat && expected.schema == nat_schema);
		assert(pg_evidence_context(expected.parameters) == pg_evidence_context(context));
		pg_inductive_recovery_destroy(&work);
		size_t saved_proofs = typing.proofs.count, saved_terms = graph.terms.count;
		const size_t chunks[] = {1, 7, 64};
		for (size_t i = 0; i < sizeof(chunks) / sizeof(*chunks); ++i) {
			assert(!pg_inductive_recovery_init(&work, &typing, projected));
			size_t calls = 0;
			while (!pg_inductive_recovery_advance(&work, chunks[i])) assert(++calls < 100);
			assert(work.status == 1 && work.result.parameters == expected.parameters);
			pg_inductive_recovery_destroy(&work);
		}
		struct pg_inductive_instance sync;
		assert(pg_inductive_instance(&typing, projected, &sync));
		assert(sync.parameters == expected.parameters);
		assert(typing.proofs.count == saved_proofs && graph.terms.count == saved_terms);
		assert(!pg_inductive_recovery_init(&work, &typing, projected));
		assert(!pg_inductive_recovery_advance(&work, 37));
		pg_inductive_recovery_destroy(&work);
		assert(pg_inductive_recovery_init(&work, &typing, NULL) == -1);
		assert(pg_inductive_recovery_advance(&work, 1) == -1);
		pg_inductive_recovery_destroy(&work);
	}
	assert(!pg_evidence_context(nat));
	assert(pg_evidence_premise(nat, 0) == parameters);
	assert(pg_evidence_premise_count(nat) == 3);
	assert(pg_evidence_classifier(nat) == pg_universe(&classifiers, 0));
	assert(pg_term_independent(pg_evidence_subject(nat)->core, self) == 1);
	assert(!pg_prove_inductive_type(&typing, &classifiers, bad));
	assert(!pg_prove_inductive_type(&typing, &classifiers, NULL));
	const struct pg_evidence *identity = pg_prove_substitution(&typing, empty, empty, 0, NULL);
	const struct pg_data_layout *nat_layout = pg_data_schema_layout(nat_schema);
	size_t position = 99;
	assert(pg_data_constructor_position(nat_layout, pg_data_constructor(nat_layout, 1), &position) && position == 1);
	assert(!pg_data_constructor_position(nat_layout, self, &position) && position == 1);
	assert(!pg_data_constructor_position(NULL, pg_data_constructor(nat_layout, 0), &position));
	const struct pg_evidence *zero = pg_prove_constructor(&typing, nat,
		pg_data_constructor(nat_layout, 0), identity, 0, NULL);
	const struct pg_evidence *succ = pg_prove_constructor(&typing, nat,
		pg_data_constructor(nat_layout, 1), identity, 1, &zero);
	assert(zero && succ && pg_evidence_rule(succ) == PG_CONSTRUCTOR_INTRO);
	index_paths(&typing, &classifiers, nat, zero,
		pg_prove_inductive_type(&typing, &classifiers, none), pg_data_constructor(nat_layout, 1));
	assert(pg_evidence_classifier(succ) == pg_evidence_subject(nat)->core);
	assert(pg_evidence_subject(succ)->core == pg_application(&graph,
		pg_reference(&graph, pg_data_constructor(nat_layout, 1)), pg_evidence_subject(zero)->core));
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, empty, succ)) ==
		pg_evidence_subject(pg_evidence_premise(succ, 0)));
	proofs = typing.proofs.count; terms = graph.terms.count;
	assert(pg_prove_inductive_type(&typing, &classifiers, nat_schema) == nat);
	assert(pg_prove_constructor(&typing, nat, pg_data_constructor(nat_layout, 1), identity, 1, &zero) == succ);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	const struct pg_data_schema *other_schema = pg_data_schema(&typing, signature, 2, results);
	const struct pg_evidence *other = pg_prove_inductive_type(&typing, &classifiers, other_schema);
	assert(other && pg_evidence_subject(other)->core != pg_evidence_subject(nat)->core);
	assert(!pg_prove_constructor(&typing, other, pg_data_constructor(nat_layout, 0), identity, 0, NULL));
	assert(!pg_prove_constructor(&typing, other,
		pg_data_constructor(pg_data_schema_layout(other_schema), 1), identity, 1, &zero));
	assert(!pg_prove_constructor(&typing, nat, pg_data_constructor(nat_layout, 1), identity, 0, NULL));
	struct pg_derivation_parameters wire_parameters;
	assert(!pg_derivation_parameters(nat, &wire_parameters));
	assert(wire_parameters.declaration == pg_data_schema_declaration(nat_schema));
	const struct pg_evidence *formation_premises[] = {pg_evidence_premise(nat, 0),
		pg_evidence_premise(nat, 1), pg_evidence_premise(nat, 2)};
	proofs = typing.proofs.count;
	assert(pg_prove_derivation(&typing, &classifiers, PG_INDUCTIVE_FORM, &wire_parameters, 3, formation_premises) == nat);
	assert(typing.proofs.count == proofs);
	assert(!pg_derivation_parameters(succ, &wire_parameters));
	assert(wire_parameters.constructor == pg_data_constructor(nat_layout, 1));
	common_rule(&typing, &classifiers, zero);
	common_rule(&typing, &classifiers, succ);
	const struct pg_evidence *successor_function = pg_prove_constructor_function(&typing,
		&classifiers, nat, pg_data_constructor(nat_layout, 1), identity);
	assert(successor_function && pg_evidence_rule(successor_function) == PG_LAMBDA_INTRO);
	const struct pg_evidence *successor_application = pg_prove_application(&typing, successor_function, zero);
	assert(successor_application);
	const struct pg_evidence *successor_body = pg_prove_application_body(&typing, successor_function, zero);
	assert(successor_body && pg_evidence_rule(successor_body) == PG_REINDEX);
	assert(pg_evidence_classifier(successor_body) == pg_evidence_classifier(successor_application));
	proofs = typing.proofs.count; terms = graph.terms.count;
	assert(pg_prove_application_body(&typing, successor_function, zero) == successor_body);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	assert(!pg_prove_application_body(&typing, zero, zero));
	const struct pg_evidence *other_zero = pg_prove_constructor(&typing, other,
		pg_data_constructor(pg_data_schema_layout(other_schema), 0), identity, 0, NULL);
	assert(other_zero);
	assert(!pg_prove_application_body(&typing, successor_function, other_zero));
	struct pg_whnf_work constructor_work;
	assert(!pg_whnf_work_init(&constructor_work, &graph));
	const struct pg_term *returned_successor = pg_application(&graph,
		pg_reference(&graph, &pg_return_operation), pg_evidence_subject(succ)->core);
	check(&constructor_work, pg_evidence_subject(successor_application)->core, returned_successor);
	check(&constructor_work, pg_evidence_subject(successor_body)->core, returned_successor);
	const struct pg_evidence *zero_function = pg_prove_constructor_function(&typing,
		&classifiers, nat, pg_data_constructor(nat_layout, 0), identity);
	assert(zero_function && pg_evidence_rule(zero_function) == PG_RETURN_INTRO);
	assert(pg_prove_return_value(&typing, zero_function) == zero);
	assert(pg_evidence_classifier(zero_function) == pg_computation_type(&classifiers,
		PG_TOTALITY_TOTAL, pg_effect_row(&graph, 0, NULL), pg_evidence_subject(nat)->core));
	/* The following hand-built motives deliberately use the weaker contract. */
	zero_function = pg_prove_effect_subsumption(&typing, zero_function,
		pg_prove_return_type(&typing, &classifiers, nat));
	assert(zero_function);
	/* Case elimination checks synthesized branches, not just the chosen branch. */
	const struct pg_object *z = pg_binder(&graph), *n = pg_binder(&graph);
	const struct pg_evidence *z_context = pg_prove_context_extension(&typing, empty, z, nat);
	const struct pg_evidence *n_context = pg_prove_context_extension(&typing, empty, n, nat);
	const struct pg_evidence *n_value = pg_prove_variable(&typing, n_context, n);
	const struct pg_evidence *n_type = pg_prove_classifier(&typing, &classifiers, n_context, n_value);
	assert(pg_inductive_instance(&typing, n_type, &recovered));
	assert(recovered.formation == nat && pg_evidence_context(recovered.parameters) == pg_evidence_context(n_context));
	struct pg_inductive_instance unchanged = recovered;
	assert(!pg_inductive_instance(&typing, u, &recovered));
	assert(recovered.formation == unchanged.formation && recovered.parameters == unchanged.parameters);
	assert(!pg_inductive_instance(&typing, zero, &recovered));
	assert(!pg_prove_substitution_projection(&typing, n_context, empty));
	{
		/* A high codomain must not inflate the retained Nat domain. */
		const struct pg_evidence *large_result = pg_prove_return_type(&typing, &classifiers,
			pg_prove_universe(&typing, &classifiers, z_context, 2));
		const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, z_context, large_result);
		assert(pg_evidence_classifier(pi) == pg_universe(&classifiers, 3));
		const struct pg_evidence *map = pg_prove_substitution_projection(&typing, empty, n_context);
		const struct pg_evidence *derived[] = {pi,
			pg_prove_projection(&typing, n_context, pi),
			pg_prove_reindex(&typing, map, pi),
			pg_prove_thunk_content(&typing, pg_prove_thunk_type(&typing, &classifiers, pi))};
		for (size_t i = 0; i < sizeof(derived) / sizeof(*derived); ++i) {
			const struct pg_evidence *domain = pg_prove_pi_domain(&typing, derived[i]);
			assert(domain && pg_evidence_classifier(domain) == pg_universe(&classifiers, 0));
			assert(pg_evidence_context(domain) == pg_evidence_context(derived[i]));
			assert(pg_evidence_subject(domain)->core == pg_evidence_subject(nat)->core);
			common_rule(&typing, &classifiers, domain);
			size_t before = typing.proofs.count;
			assert(pg_prove_pi_domain(&typing, derived[i]) == domain);
			assert(typing.proofs.count == before);
		}
		/* Narrow domain formation survives removing an unused outer binder. */
		const struct pg_evidence *nested = pg_prove_pi(&typing, &classifiers, z_context,
			pg_prove_projection(&typing, z_context, pi));
		const struct pg_evidence *applied = pg_prove_pi_codomain(&typing, nested, zero);
		assert(applied);
		struct pg_occurrence_input *input = pg_occurrence_input_request(&typing, pg_evidence_subject(applied), 0);
		while (pg_occurrence_input_advance(input, 1) == PG_INPUT_PENDING) {}
		const struct pg_occurrence *retained_domain = pg_occurrence_input_result(input);
		assert(retained_domain && retained_domain->classifier == pg_universe(&classifiers, 0));
		assert(retained_domain->core == pg_evidence_subject(nat)->core && !retained_domain->context);
		common_rule(&typing, &classifiers, applied);
		const struct pg_evidence *inner = pg_prove_pi_constant_codomain(&typing, nested);
		assert(inner && pg_evidence_subject(inner) == pg_evidence_subject(pi));
		size_t before_domain = typing.proofs.count;
		const struct pg_evidence *inner_domain = pg_prove_pi_domain(&typing, inner);
		assert(inner_domain && pg_evidence_classifier(inner_domain) == pg_universe(&classifiers, 0));
		assert(pg_evidence_rule(inner_domain) == PG_PI_DOMAIN);
		assert(pg_evidence_premise(inner_domain, 0) == inner);
		assert(typing.proofs.count == before_domain + 1);
		common_rule(&typing, &classifiers, inner_domain);
		/* A directly formed inner Pi is not merely a retained projection. */
		const struct pg_evidence *direct_scope = pg_prove_context_extension(&typing, z_context,
			pg_binder(&graph), pg_prove_projection(&typing, z_context, nat));
		const struct pg_evidence *direct = pg_prove_pi(&typing, &classifiers, direct_scope,
			pg_prove_return_type(&typing, &classifiers, pg_prove_universe(&typing, &classifiers, direct_scope, 2)));
		inner = pg_prove_pi_constant_codomain(&typing, pg_prove_pi(&typing, &classifiers, z_context, direct));
		inner_domain = pg_prove_pi_domain(&typing, inner);
		assert(inner_domain && pg_evidence_classifier(inner_domain) == pg_universe(&classifiers, 0));
		common_rule(&typing, &classifiers, inner_domain);
		/* Recover an actual large domain without lowering its universe. */
		const struct pg_evidence *large = pg_prove_universe(&typing, &classifiers, empty, 2);
		const struct pg_evidence *scope = pg_prove_context_extension(&typing, empty, pg_binder(&graph), large);
		pi = pg_prove_pi(&typing, &classifiers, scope,
			pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, scope, nat)));
		const struct pg_evidence *domain = pg_prove_pi_domain(&typing, pi);
		assert(domain && pg_evidence_subject(domain) == pg_evidence_subject(large));
		assert(pg_evidence_classifier(domain) == pg_universe(&classifiers, 3));
		common_rule(&typing, &classifiers, domain);
	}
	{
		const struct pg_evidence *result_type = pg_prove_return_type(&typing, &classifiers,
			pg_prove_projection(&typing, z_context, nat));
		const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, z_context, result_type);
		const struct pg_evidence *map = pg_prove_substitution_projection(&typing, empty, n_context);
		const struct pg_evidence *wrapped = pg_prove_thunk_type(&typing, &classifiers, pi);
		assert(wrapped && !pg_inductive_instance(&typing, wrapped, &recovered));
		const struct pg_evidence *curried = pg_prove_pi(&typing, &classifiers, z_context,
			pg_prove_projection(&typing, z_context, pi));
		const struct pg_evidence *inner_context = pg_prove_context_extension(&typing, z_context,
			pg_binder(&graph), pg_prove_projection(&typing, z_context, nat));
		const struct pg_evidence *scoped_curried = pg_prove_pi(&typing, &classifiers, z_context,
			pg_prove_pi(&typing, &classifiers, inner_context,
				pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, inner_context, nat))));
		const struct pg_evidence *derived[] = {
			pg_prove_projection(&typing, n_context, pi),
			pg_prove_reindex(&typing, map, pi),
			pg_prove_projection(&typing, n_context, pg_prove_pi_codomain(&typing, curried, zero)),
			pg_prove_projection(&typing, n_context, pg_prove_pi_constant_codomain(&typing, curried)),
			pg_prove_pi_constant_codomain(&typing, pg_prove_reindex(&typing, map, curried)),
			pg_prove_pi_constant_codomain(&typing, pg_prove_projection(&typing, n_context, curried)),
			pg_prove_projection(&typing, n_context, pg_prove_pi_constant_codomain(&typing, scoped_curried)),
			pg_prove_thunk_content(&typing, pg_prove_projection(&typing, n_context, wrapped)),
			pg_prove_thunk_content(&typing, pg_prove_reindex(&typing, map, wrapped))
		};
		for (size_t i = 0; i < sizeof(derived) / sizeof(*derived); ++i) {
			const struct pg_evidence *domain = pg_prove_pi_domain(&typing, derived[i]);
			struct pg_inductive_instance domain_instance;
			assert(domain && pg_inductive_instance(&typing, domain, &domain_instance));
			assert(domain_instance.formation == nat && domain_instance.schema == nat_schema);
			assert(pg_evidence_context(domain_instance.parameters) == pg_evidence_context(n_context));
			const struct pg_evidence *content = pg_prove_return_content(&typing,
				pg_prove_pi_constant_codomain(&typing, derived[i]));
			assert(content);
			struct pg_inductive_instance instance;
			assert(pg_inductive_instance(&typing, content, &instance));
			assert(instance.formation == nat && instance.schema == nat_schema);
			assert(pg_evidence_context(instance.parameters) == pg_evidence_context(n_context));
			content = pg_prove_return_content(&typing, pg_prove_pi_codomain(&typing, derived[i], n_value));
			assert(content && pg_inductive_instance(&typing, content, &instance));
			assert(instance.formation == nat && instance.schema == nat_schema);
			assert(pg_evidence_context(instance.parameters) == pg_evidence_context(n_context));
		}
		const struct pg_evidence *returned = pg_prove_return_type(&typing, &classifiers, nat);
		wrapped = pg_prove_thunk_type(&typing, &classifiers, returned);
		assert(wrapped && !pg_inductive_instance(&typing, wrapped, &recovered));
		const struct pg_evidence *content = pg_prove_return_content(&typing,
			pg_prove_thunk_content(&typing, pg_prove_reindex(&typing, map, wrapped)));
		assert(content && pg_inductive_instance(&typing, content, &recovered));
		assert(recovered.formation == nat && recovered.schema == nat_schema);
		assert(pg_evidence_context(recovered.parameters) == pg_evidence_context(n_context));
		const struct pg_evidence *z_value = pg_prove_variable(&typing, z_context, z);
		const struct pg_evidence *dependent = pg_prove_pi(&typing, &classifiers, z_context,
			pg_prove_return_type(&typing, &classifiers, pg_prove_identity_type(&typing,
				pg_prove_projection(&typing, z_context, nat), z_value, z_value)));
		assert(dependent && !pg_prove_pi_constant_codomain(&typing, dependent));
		assert(!pg_prove_pi_constant_codomain(&typing, pg_prove_reindex(&typing, map, dependent)));
	}
	const struct pg_evidence *pred_branch = pg_prove_abstract(&typing, &classifiers,
		empty, n_context, pg_prove_return(&typing, &classifiers, n_value));
	const struct pg_evidence *nat_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, z_context, nat));
	const struct pg_evidence *induction_scope = pg_prove_induction_scope(&typing,
		&classifiers, nat, pg_data_constructor(nat_layout, 1), identity, z_context, nat_motive);
	assert(induction_scope);
	const struct pg_evidence *ih_context = pg_evidence_premise(induction_scope, 1);
	const struct pg_evidence *ih = pg_prove_variable(&typing, ih_context, pg_evidence_context(ih_context)->binder);
	const struct pg_evidence *ih_call = pg_prove_force(&typing, ih);
	assert(ih_call && pg_evidence_classifier(ih_call) == pg_return_type(&classifiers, pg_evidence_subject(nat)->core));
	size_t induction_bindings;
	assert(!pg_context_extension_size(pg_evidence_context(ih_context), NULL, &induction_bindings));
	assert(induction_bindings == 2);
	const struct pg_context *ih_allocation = pg_evidence_context(ih_context);
	assert(pg_prove_induction_scope_at(&typing, &classifiers, nat,
		pg_data_constructor(nat_layout, 1), identity, z_context, nat_motive, ih_allocation) == induction_scope);
	const struct pg_context *wrong_ih = pg_context_bind(&typing, ih_allocation->parent,
		ih_allocation->binder, pg_universe(&classifiers, 0), PG_JUDGEMENT_VALUE);
	assert(wrong_ih != ih_allocation);
	assert(pg_prove_induction_scope_at(&typing, &classifiers, nat,
		pg_data_constructor(nat_layout, 1), identity, z_context, nat_motive, wrong_ih) == induction_scope);
	assert(!pg_prove_induction_scope_at(&typing, &classifiers, nat,
		pg_data_constructor(nat_layout, 1), identity, z_context, nat_motive, ih_allocation->parent));
	assert(!pg_prove_projection(&typing, empty, ih_call));
	const struct pg_evidence *base_scope = pg_prove_induction_scope(&typing,
		&classifiers, nat, pg_data_constructor(nat_layout, 0), identity, z_context, nat_motive);
	assert(base_scope && !pg_evidence_context(base_scope));
	assert(pg_prove_induction_scope_at(&typing, &classifiers, nat,
		pg_data_constructor(nat_layout, 0), identity, z_context, nat_motive, NULL) == base_scope);
	{
		const struct pg_evidence *two_fields = pg_prove_context_extension(&typing, fields,
			pg_binder(&graph), pg_prove_projection(&typing, fields, type));
		const struct pg_evidence *tree_results[] = {results[0], parameter_result(&typing, parameters, two_fields)};
		const struct pg_data_schema *tree_schema = pg_data_schema(&typing, signature, 2, tree_results);
		const struct pg_evidence *tree = pg_prove_inductive_type(&typing, &classifiers, tree_schema);
		const struct pg_object *node = pg_data_constructor(pg_data_schema_layout(tree_schema), 1);
		const struct pg_evidence *tree_context = pg_prove_context_extension(&typing, empty, pg_binder(&graph), tree);
		const struct pg_evidence *tree_motive = pg_prove_return_type(&typing, &classifiers,
			pg_prove_projection(&typing, tree_context, nat));
		const struct pg_evidence *tree_scope = pg_prove_induction_scope(&typing, &classifiers,
			tree, node, identity, tree_context, tree_motive);
		assert(tree_scope);
		const struct pg_context *allocated = pg_evidence_context(pg_evidence_premise(tree_scope, 1));
		size_t bindings;
		assert(!pg_context_extension_size(allocated, NULL, &bindings) && bindings == 4);
		assert(pg_prove_induction_scope_at(&typing, &classifiers, tree, node, identity,
			tree_context, tree_motive, allocated) == tree_scope);
		assert(!pg_prove_induction_scope_at(&typing, &classifiers, tree, node, identity,
			tree_context, tree_motive, allocated->parent));
		const struct pg_evidence *leaf = pg_prove_constructor(&typing, tree,
			pg_data_constructor(pg_data_schema_layout(tree_schema), 0), identity, 0, NULL);
		const struct pg_evidence *leaf_fields[] = {leaf, leaf};
		const struct pg_evidence *left = pg_prove_constructor(&typing, tree, node, identity, 2, leaf_fields);
		const struct pg_evidence *root_fields[] = {left, leaf};
		const struct pg_evidence *root = pg_prove_constructor(&typing, tree, node, identity, 2, root_fields);
		const struct pg_evidence *scope = pg_evidence_premise(tree_scope, 1);
		const struct pg_evidence *first_ih = pg_prove_variable(&typing, scope, allocated->parent->binder);
		const struct pg_evidence *successor = pg_prove_constructor_function(&typing, &classifiers,
			nat, pg_data_constructor(nat_layout, 1), identity);
		const struct pg_evidence *body = pg_prove_fold(&typing, &classifiers,
			pg_prove_force(&typing, first_ih), pg_prove_projection(&typing, scope, successor));
		const struct pg_evidence *step = pg_prove_abstract(&typing, &classifiers, empty, scope, body);
		const struct pg_evidence *branches[] = {zero_function, step};
		const struct pg_evidence *elimination = pg_prove_induction(&typing, &classifiers,
			tree, identity, root, tree_context, tree_motive, 2, branches);
		const struct pg_evidence *unfolded = pg_prove_elimination_body(&typing, &classifiers, elimination);
		const struct pg_evidence *two = pg_prove_constructor(&typing, nat,
			pg_data_constructor(nat_layout, 1), identity, 1, &succ);
		assert(elimination && unfolded && two);
		const struct pg_term *expected = pg_evidence_subject(pg_prove_return(&typing, &classifiers, two))->core;
		check(&constructor_work, pg_evidence_subject(elimination)->core, expected);
		check(&constructor_work, pg_evidence_subject(unfolded)->core, expected);
		common_rule(&typing, &classifiers, unfolded);
	}
	assert(!pg_prove_induction_scope(&typing, &classifiers, other,
		pg_data_constructor(pg_data_schema_layout(other_schema), 1), identity, z_context, nat_motive));
	const struct pg_evidence *recursive_branch = pg_prove_abstract(&typing,
		&classifiers, empty, ih_context, ih_call);
	const struct pg_evidence *recursive_branches[] = {zero_function, recursive_branch};
	const struct pg_evidence *twice = pg_prove_constructor(&typing, nat,
		pg_data_constructor(nat_layout, 1), identity, 1, &succ);
	const struct pg_evidence *countdown = pg_prove_induction(&typing, &classifiers,
		nat, identity, twice, z_context, nat_motive, 2, recursive_branches);
	assert(countdown && pg_evidence_rule(countdown) == PG_INDUCTION_ELIM);
	const struct pg_evidence *countdown_body = pg_prove_elimination_body(&typing, &classifiers, countdown);
	assert(countdown_body && pg_evidence_context(countdown_body) == pg_evidence_context(countdown));
	assert(pg_alpha_equal(pg_evidence_classifier(countdown_body), pg_evidence_classifier(countdown)) == 1);
	check(&constructor_work, pg_evidence_subject(countdown_body)->core, pg_evidence_subject(zero_function)->core);
	common_rule(&typing, &classifiers, countdown_body);
	assert(pg_prove_elimination_body(&typing, &classifiers, countdown) == countdown_body);
	{
		const struct pg_evidence *map = pg_prove_substitution_projection(&typing, empty, n_context);
		const struct pg_evidence *scope = pg_prove_inductive_motive_context(&typing, nat, map, pg_binder(&graph));
		const struct pg_evidence *motive = pg_prove_return_type(&typing, &classifiers,
			pg_prove_projection(&typing, scope, nat));
		const struct pg_evidence *branches[] = {pg_prove_projection(&typing, n_context, zero_function),
			pg_prove_projection(&typing, n_context, recursive_branch)};
		const struct pg_evidence *input = pg_prove_constructor(&typing, nat,
			pg_data_constructor(nat_layout, 1), map, 1, &n_value);
		const struct pg_evidence *outer = pg_prove_induction(&typing, &classifiers,
			nat, map, input, scope, motive, 2, branches);
		const struct pg_evidence *inner = pg_prove_induction(&typing, &classifiers,
			nat, map, n_value, scope, motive, 2, branches);
		assert(outer && inner && !pg_prove_elimination_body(&typing, &classifiers, inner));
		const struct pg_evidence *body = pg_prove_elimination_body(&typing, &classifiers, outer);
		const struct pg_evidence *expected_body = pg_prove_force(&typing,
			pg_prove_thunk(&typing, &classifiers, inner));
		assert(body && expected_body);
		assert(pg_alpha_equal(pg_evidence_subject(body)->core, pg_evidence_subject(expected_body)->core) == 1);
		assert(pg_alpha_equal(pg_evidence_classifier(body), pg_evidence_classifier(outer)) == 1);
		struct pg_conversion comparison;
		assert(!pg_conversion_init(&comparison, &constructor_work,
			pg_evidence_subject(outer)->core, pg_evidence_subject(body)->core));
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
		pg_conversion_destroy(&comparison);
		common_rule(&typing, &classifiers, body);
	}
	assert(!pg_prove_elimination_body(&typing, NULL, countdown));
	assert(!pg_prove_elimination_body(NULL, &classifiers, countdown));
	{
		struct pg_typing foreign;
		assert(!pg_typing_init(&foreign, &graph));
		assert(!pg_prove_elimination_body(&foreign, &classifiers, countdown));
		pg_typing_destroy(&foreign);
	}
	const struct pg_induction_allocation *countdown_allocation = pg_evidence_induction_allocation(countdown);
	assert(countdown_allocation && countdown_allocation->count == 2);
	assert(countdown_allocation == pg_evidence_subject(countdown)->induction);
	assert(pg_occurrence_with_induction(&typing, pg_evidence_subject(countdown), countdown_allocation) == pg_evidence_subject(countdown));
	{
		const struct pg_evidence *environment = NULL;
		assert(pg_prove_construction_origin(&typing, &classifiers, countdown, &environment) == countdown && !environment);
		const struct pg_evidence *projected = pg_prove_projection(&typing, z_context, countdown);
		assert(pg_prove_construction_origin(&typing, &classifiers, projected, &environment) == countdown);
		assert(environment && pg_evidence_context_map(environment)->source == pg_evidence_context(countdown));
		assert(pg_evidence_context_map(environment)->destination == pg_evidence_context(z_context));
		size_t before = typing.proofs.count, subjects = typing.occurrences.count;
		for (size_t i = 0; i < 100; ++i)
			assert(pg_prove_construction_origin(&typing, &classifiers, projected, &environment) == countdown);
		assert(typing.proofs.count == before && typing.occurrences.count == subjects);
		assert(!pg_prove_construction_origin(NULL, &classifiers, countdown, &environment));
		assert(!pg_prove_construction_origin(&typing, &classifiers, countdown, NULL));
		/* This boundary has no introduction receipt until structural recovery.
		 * Its typed Return child is unchanged by totality subsumption. */
		const struct pg_evidence *scope = pg_prove_context_extension(&typing, empty, pg_binder(&graph), nat);
		const struct pg_evidence *input = pg_prove_variable(&typing, scope, pg_evidence_context(scope)->binder);
		const struct pg_evidence *total = pg_prove_return_contract(&typing, &classifiers, PG_TOTALITY_TOTAL, input);
		const struct pg_evidence *target = pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, scope, nat));
		const struct pg_evidence *boundary = pg_prove_effect_subsumption(&typing, total, target);
		assert(boundary && pg_evidence_rule(boundary) == PG_EFFECT_SUBSUMPTION);
		assert(pg_evidence_for_subject(&typing, pg_evidence_subject(boundary), NULL) == boundary);
		const struct pg_evidence *recovered = pg_prove_construction_origin(&typing, &classifiers, boundary, &environment);
		assert(recovered && pg_evidence_rule(recovered) == PG_RETURN_INTRO && !environment);
		assert(pg_evidence_subject(recovered)->core == pg_evidence_subject(boundary)->core);
		assert(pg_evidence_for_subject(&typing, pg_evidence_subject(boundary), NULL) == boundary);
	}
	assert(!pg_evidence_induction_allocation(zero));
	assert(pg_prove_induction_at(&typing, &classifiers, nat, identity, twice, z_context,
		nat_motive, 2, recursive_branches, countdown_allocation) == countdown);
	const struct pg_evidence *retained_base = pg_prove_induction_at(&typing, &classifiers,
		nat, identity, zero, z_context, nat_motive, 2, recursive_branches, countdown_allocation);
	assert(retained_base);
	assert(pg_prove_elimination_body(&typing, &classifiers, retained_base) == zero_function);
	const struct pg_term *countdown_core = pg_evidence_subject(countdown)->core;
	assert(countdown_core->kind == PG_APPLICATION);
	assert(pg_evidence_subject(retained_base)->core == pg_application(&graph,
		countdown_core->as.application.function, pg_evidence_subject(zero)->core));
	struct pg_induction_allocation invalid_allocation = *countdown_allocation;
	invalid_allocation.argument = invalid_allocation.recursion;
	assert(!pg_prove_induction_at(&typing, &classifiers, nat, identity, twice, z_context,
		nat_motive, 2, recursive_branches, &invalid_allocation));
	invalid_allocation = *countdown_allocation;
	invalid_allocation.recursion = pg_binder(&graph);
	assert(!pg_prove_induction_at(&typing, &classifiers, nat, identity, twice, z_context,
		nat_motive, 2, recursive_branches, &invalid_allocation));
	const struct pg_context *short_clauses[] = {countdown_allocation->clauses[0], countdown_allocation->clauses[1]->parent};
	invalid_allocation = *countdown_allocation;
	invalid_allocation.clauses = short_clauses;
	assert(!pg_prove_induction_at(&typing, &classifiers, nat, identity, succ, z_context,
		nat_motive, 2, recursive_branches, &invalid_allocation));
	invalid_allocation = *countdown_allocation;
	invalid_allocation.recursion = countdown_allocation->clauses[1]->parent->binder;
	assert(!pg_prove_induction_at(&typing, &classifiers, nat, identity, succ, z_context,
		nat_motive, 2, recursive_branches, &invalid_allocation));
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, empty, countdown))->core
		== pg_return_type(&classifiers, pg_evidence_subject(nat)->core));
	check(&constructor_work, pg_evidence_subject(countdown)->core, pg_evidence_subject(zero_function)->core);
	proofs = typing.proofs.count; terms = graph.terms.count;
	assert(pg_prove_induction(&typing, &classifiers, nat, identity, twice,
		z_context, nat_motive, 2, recursive_branches) == countdown);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	assert(!pg_derivation_parameters(countdown, &wire_parameters));
	assert(wire_parameters.induction == countdown_allocation);
	struct pg_derivation_input retained_induction;
	assert(!pg_derivation_input_header(countdown, &retained_induction));
	assert(retained_induction.parameters.induction == countdown_allocation);
	common_rule(&typing, &classifiers, countdown);
	assert(!pg_prove_match(&typing, &classifiers, nat, identity, twice,
		z_context, nat_motive, 2, recursive_branches));
	const struct pg_evidence *pred_branches[] = {zero_function, pred_branch};
	assert(!pg_prove_induction(&typing, &classifiers, nat, identity, twice,
		z_context, nat_motive, 2, pred_branches));
	const struct pg_evidence *pred = pg_prove_match(&typing, &classifiers, nat,
		identity, succ, z_context, nat_motive, 2, pred_branches);
	assert(pred && pg_evidence_rule(pred) == PG_MATCH_ELIM);
	check(&constructor_work, pg_evidence_subject(pred)->core, pg_evidence_subject(zero_function)->core);
	const struct pg_reduction_certificate *pred_receipt = pg_whnf_certificate(
		pg_whnf_request(&constructor_work, &pg_pure_policy, pg_evidence_subject(pred)->core));
	const struct pg_evidence *pred_reduced = pg_prove_normalization(&typing, pred, pred_receipt);
	assert(pred_reduced && pg_prove_return_value(&typing, pred_reduced));
	assert(pg_evidence_classifier(pred_reduced) == pg_evidence_classifier(pred));
	/* Recover constructor evidence across sequencing, not from erased Core. */
	const struct pg_evidence *sequenced = zero_function;
	for (size_t i = 0; i < 2; ++i) {
		sequenced = pg_prove_fold(&typing, &classifiers, sequenced, successor_function);
		assert(sequenced);
		check(&constructor_work, pg_evidence_subject(sequenced)->core,
			pg_evidence_subject(pg_prove_return(&typing, &classifiers, i ? twice : succ))->core);
		const struct pg_reduction_certificate *receipt = pg_whnf_certificate(
			pg_whnf_request(&constructor_work, &pg_pure_policy, pg_evidence_subject(sequenced)->core));
		const struct pg_evidence *returned = pg_prove_return_value(&typing,
			pg_prove_normalization(&typing, sequenced, receipt));
		const struct pg_evidence *field = pg_prove_constructor_field(&typing, returned, x);
		assert(field && pg_evidence_context(field) == NULL);
		assert(pg_evidence_subject(field)->core == pg_evidence_subject(i ? succ : zero)->core);
		assert(pg_evidence_classifier(field) == pg_evidence_subject(nat)->core);
		assert(!pg_prove_constructor_field(&typing, returned, self));
		size_t field_proofs = typing.proofs.count, field_terms = graph.terms.count;
		for (size_t j = 0; j < 100; ++j)
			assert(pg_prove_constructor_field(&typing, returned, x) == field);
		assert(typing.proofs.count == field_proofs && graph.terms.count == field_terms);
		const struct pg_evidence *selected = pg_prove_match(&typing, &classifiers,
			nat, identity, returned, z_context, nat_motive, 2, pred_branches);
		assert(returned && selected);
		const struct pg_evidence *body = pg_prove_elimination_body(&typing, &classifiers, selected);
		assert(body);
		check(&constructor_work, pg_evidence_subject(selected)->core, pg_evidence_subject(body)->core);
		common_rule(&typing, &classifiers, body);
		const struct pg_evidence *map = pg_prove_substitution_projection(&typing, empty, n_context);
		const struct pg_evidence *mapped = pg_prove_elimination_reindex(&typing, &classifiers, map, selected);
		const struct pg_evidence *mapped_body = pg_prove_elimination_body(&typing, &classifiers, mapped);
		assert(mapped_body);
		check(&constructor_work, pg_evidence_subject(mapped)->core, pg_evidence_subject(mapped_body)->core);
		common_rule(&typing, &classifiers, mapped_body);
		/* A map inside a pending Fold must resume in the outer context. */
		const struct pg_evidence *scoped = pg_prove_fold(&typing, &classifiers,
			pg_prove_projection(&typing, n_context, sequenced),
			pg_prove_projection(&typing, n_context, successor_function));
		assert(scoped);
		struct pg_whnf_job *state = pg_whnf_request(&constructor_work, &pg_pure_policy,
			pg_evidence_subject(scoped)->core);
		assert(state);
		while (pg_whnf_advance(state, 64) == PG_EVAL_PENDING) assert(pg_whnf_steps(state) < 100000);
		const struct pg_evidence *scoped_value = pg_prove_return_value(&typing,
			pg_prove_normalization(&typing, scoped, pg_whnf_certificate(state)));
		const struct pg_evidence *scoped_field = pg_prove_constructor_field(&typing, scoped_value, x);
		assert(scoped_field && pg_evidence_context(scoped_field) == pg_evidence_context(n_context));
		assert(pg_evidence_subject(scoped_field)->core == pg_evidence_subject(i ? twice : succ)->core);
		assert(pg_evidence_classifier(scoped_field) == pg_evidence_subject(nat)->core);
		const struct pg_evidence *scoped_branches[] = {
			pg_evidence_premise(mapped, 5), pg_evidence_premise(mapped, 6)};
		const struct pg_evidence *scoped_match = pg_prove_match(&typing, &classifiers, nat, map,
			scoped_value, pg_evidence_premise(mapped, 4), pg_evidence_premise(mapped, 0), 2, scoped_branches);
		const struct pg_evidence *scoped_body = pg_prove_elimination_body(&typing, &classifiers, scoped_match);
		assert(scoped_body);
		check(&constructor_work, pg_evidence_subject(scoped_body)->core,
			pg_evidence_subject(pg_prove_return(&typing, &classifiers,
				pg_prove_projection(&typing, n_context, i ? twice : succ)))->core);
		common_rule(&typing, &classifiers, scoped_body);
	}
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, empty, pred))->core ==
		pg_return_type(&classifiers, pg_evidence_subject(nat)->core));
	proofs = typing.proofs.count; terms = graph.terms.count;
	assert(pg_prove_match(&typing, &classifiers, nat, identity, succ,
		z_context, nat_motive, 2, pred_branches) == pred);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	assert(!pg_derivation_parameters(pred, &wire_parameters));
	common_rule(&typing, &classifiers, pred);
	assert(!pg_prove_match(&typing, &classifiers, nat, identity, succ, z_context, nat_motive, 1, pred_branches));
	const struct pg_evidence *wrong_branches[] = {zero_function, zero_function};
	assert(!pg_prove_match(&typing, &classifiers, nat, identity, zero, z_context, nat_motive, 2, wrong_branches));
	assert(!pg_prove_match(&typing, &classifiers, other, identity, succ, z_context, nat_motive, 2, pred_branches));
	assert(!pg_prove_match(&typing, &classifiers, nat, identity, succ, z_context, nat, 2, pred_branches));
	/* A genuinely dependent motive: z |-> F (Identity Nat z z). */
	const struct pg_evidence *z_value = pg_prove_variable(&typing, z_context, z);
	const struct pg_evidence *path_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_identity_type(&typing, pg_prove_projection(&typing, z_context, nat), z_value, z_value));
	const struct pg_evidence *dependent_scope = pg_prove_induction_scope(&typing,
		&classifiers, nat, pg_data_constructor(nat_layout, 1), identity, z_context, path_motive);
	assert(dependent_scope);
	const struct pg_evidence *dependent_context = pg_evidence_premise(dependent_scope, 1);
	const struct pg_evidence *recursive_field = pg_evidence_premise(dependent_scope, 3);
	const struct pg_evidence *field_identity = pg_prove_identity_type(&typing,
		pg_prove_projection(&typing, dependent_context, nat), recursive_field, recursive_field);
	const struct pg_evidence *dependent_ih = pg_prove_force(&typing,
		pg_prove_variable(&typing, dependent_context, pg_evidence_context(dependent_context)->binder));
	assert(field_identity && dependent_ih);
	assert(pg_evidence_classifier(dependent_ih) == pg_return_type(&classifiers, pg_evidence_subject(field_identity)->core));
	const struct pg_evidence *n_parameters = pg_prove_substitution(&typing, empty, n_context, 0, NULL);
	const struct pg_evidence *open_z_context = pg_prove_context_extension(&typing, n_context,
		pg_binder(&graph), pg_prove_projection(&typing, n_context, nat));
	const struct pg_evidence *open_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, open_z_context, nat));
	const struct pg_evidence *open_branches[] = {pg_prove_projection(&typing, n_context, zero_function),
		pg_prove_projection(&typing, n_context, pred_branch)};
	const struct pg_evidence *neutral_match = pg_prove_match(&typing, &classifiers, nat,
		n_parameters, n_value, open_z_context, open_motive, 2, open_branches);
	assert(neutral_match && pg_evidence_context(neutral_match) == pg_evidence_context(n_context));
	check(&constructor_work, pg_evidence_subject(neutral_match)->core, pg_evidence_subject(neutral_match)->core);
	const struct pg_evidence *refinements[2], *refined_bodies[2];
	for (size_t i = 0; i < 2; ++i) {
		refinements[i] = pg_prove_constructor_refinement(&typing, &classifiers, n_context, n_value,
			pg_data_constructor(nat_layout, i));
		assert(refinements[i]);
		refined_bodies[i] = pg_prove_elimination_body(&typing, &classifiers,
			pg_prove_elimination_reindex(&typing, &classifiers, refinements[i], neutral_match));
		assert(refined_bodies[i]);
	}
	const struct pg_evidence *refined_motive = pg_prove_classifier(&typing, &classifiers, n_context, neutral_match);
	const struct pg_evidence *refined_pred = pg_prove_refined_match(&typing, &classifiers, n_context,
		n_value, refined_motive, 2, refinements, refined_bodies);
	assert(refined_pred);
	common_rule(&typing, &classifiers, refined_pred);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *selected = pg_prove_reindex(&typing, refinements[i], refined_pred);
		assert(selected);
		check(&constructor_work, pg_evidence_subject(selected)->core, pg_evidence_subject(refined_bodies[i])->core);
	}
	/* Same typed destination and constant carrier, but the wrong source
	 * constructor: checking only the branch result type would miss this. */
	const struct pg_evidence *wrong_refined_context = pg_evidence_premise(refinements[1], 1);
	const struct pg_evidence *wrong_refinement = pg_prove_substitution_pair(&typing,
		pg_prove_substitution_projection(&typing, empty, wrong_refined_context), n_context,
		pg_prove_projection(&typing, wrong_refined_context, zero));
	const struct pg_evidence *wrong_refinements[] = {refinements[0], wrong_refinement};
	assert(wrong_refinement && !pg_prove_refined_match(&typing, &classifiers, n_context,
		n_value, refined_motive, 2, wrong_refinements, refined_bodies));
	const struct pg_evidence *succ_n = pg_prove_constructor(&typing, nat,
		pg_data_constructor(nat_layout, 1), n_parameters, 1, &n_value);
	const struct pg_evidence *path_branches[] = {
		pg_prove_return(&typing, &classifiers, pg_prove_reflexivity(&typing, nat, zero)),
		pg_prove_abstract(&typing, &classifiers, empty, n_context,
			pg_prove_return(&typing, &classifiers, pg_prove_reflexivity(&typing,
				pg_prove_projection(&typing, n_context, nat), succ_n)))};
	const struct pg_evidence *path_match = pg_prove_match(&typing, &classifiers, nat,
		identity, succ, z_context, path_motive, 2, path_branches);
	assert(path_match);
	const struct pg_evidence *succ_path = pg_prove_identity_type(&typing, nat, succ, succ);
	assert(pg_evidence_classifier(path_match) == pg_return_type(&classifiers, pg_evidence_subject(succ_path)->core));
	/* Match may return a raw function computation, not F(U(Pi ...)). */
	const struct pg_evidence *function_type = pg_prove_classifier(&typing, &classifiers, empty, successor_function);
	const struct pg_evidence *function_motive = pg_prove_projection(&typing, z_context, function_type);
	const struct pg_evidence *function_branches[] = {successor_function,
		pg_prove_abstract(&typing, &classifiers, empty, n_context,
			pg_prove_projection(&typing, n_context, successor_function))};
	const struct pg_evidence *function_match = pg_prove_match(&typing, &classifiers, nat,
		identity, succ, z_context, function_motive, 2, function_branches);
	assert(function_match);
	const struct pg_evidence *match_app = pg_prove_application(&typing, function_match, zero);
	assert(match_app);
	check(&constructor_work, pg_evidence_subject(match_app)->core, returned_successor);
	pg_whnf_work_destroy(&constructor_work);
	/* Positivity does not establish the universe bound. */
	const struct pg_evidence *stored_universe = pg_prove_context_extension(&typing, parameters,
		pg_binder(&graph), pg_prove_projection(&typing, parameters, u));
	const struct pg_evidence *stored_result = parameter_result(&typing, parameters, stored_universe);
	const struct pg_data_schema *too_large = pg_data_schema(&typing, signature, 1, &stored_result);
	assert(too_large && pg_data_schema_positive(too_large, self) == 1);
	assert(!pg_prove_inductive_type(&typing, &classifiers, too_large));
	const struct pg_evidence *wide_parameters = pg_prove_context_extension(&typing, empty,
		pg_binder(&graph), pg_prove_universe(&typing, &classifiers, empty, 1));
	const struct pg_evidence *wide_fields = pg_prove_context_extension(&typing, wide_parameters,
		pg_binder(&graph), pg_prove_projection(&typing, wide_parameters, u));
	const struct pg_evidence *wide_result = parameter_result(&typing, wide_parameters, wide_fields);
	const struct pg_data_schema *wide_schema = pg_data_schema(&typing,
		pg_data_signature(&typing, wide_parameters, wide_parameters), 1, &wide_result);
	const struct pg_evidence *wide = pg_prove_inductive_type(&typing, &classifiers, wide_schema);
	assert(wide && pg_evidence_classifier(wide) == pg_universe(&classifiers, 1));
	/* Dependent fields become ordinary nested Pi/Lambda, not tuple metadata. */
	const struct pg_object *packed_type = pg_binder(&graph);
	const struct pg_evidence *packed_prefix = pg_prove_context_extension(&typing, wide_parameters,
		packed_type, pg_prove_projection(&typing, wide_parameters, u));
	const struct pg_evidence *packed_fields = pg_prove_context_extension(&typing, packed_prefix,
		pg_binder(&graph), pg_prove_variable(&typing, packed_prefix, packed_type));
	const struct pg_evidence *packed_result = parameter_result(&typing, wide_parameters, packed_fields);
	const struct pg_data_schema *packed_schema = pg_data_schema(&typing,
		pg_data_signature(&typing, wide_parameters, wide_parameters), 1, &packed_result);
	const struct pg_evidence *packed = pg_prove_inductive_type(&typing, &classifiers, packed_schema);
	const struct pg_object *packed_constructor = pg_data_constructor(pg_data_schema_layout(packed_schema), 0);
	const struct pg_evidence *packed_function = pg_prove_constructor_function(&typing,
		&classifiers, packed, packed_constructor, identity);
	assert(packed_function && pg_evidence_rule(packed_function) == PG_LAMBDA_INTRO);
	const struct pg_evidence *packed_map = pg_prove_constructor_scope(&typing, packed, packed_constructor, identity);
	assert(packed_map);
	const struct pg_context *packed_allocation = pg_evidence_context(pg_evidence_premise(packed_map, 1));
	assert(pg_prove_constructor_scope_at(&typing, packed, packed_constructor, identity, packed_allocation) == packed_map);
	const struct pg_context *wrong_packed = pg_context_bind(&typing, packed_allocation->parent,
		packed_allocation->binder, pg_universe(&classifiers, 0), PG_JUDGEMENT_VALUE);
	assert(wrong_packed && wrong_packed != packed_allocation);
	assert(pg_prove_constructor_scope_at(&typing, packed, packed_constructor, identity, wrong_packed) == packed_map);
	assert(!pg_prove_constructor_scope_at(&typing, packed, packed_constructor, identity, packed_allocation->parent));
	const struct pg_evidence *packed_first = pg_prove_application(&typing, packed_function,
		pg_prove_type_value(&typing, nat));
	const struct pg_evidence *packed_second = pg_prove_application(&typing, packed_first, zero);
	assert(packed_second && pg_evidence_classifier(packed_second) ==
		pg_computation_type(&classifiers, PG_TOTALITY_TOTAL, pg_effect_row(&graph, 0, NULL), pg_evidence_subject(packed)->core));
	assert(!pg_prove_constructor_function(&typing, &classifiers, packed,
		pg_data_constructor(nat_layout, 0), identity));
	/* Parameters survive discharge and are actual operands of the family. */
	const struct pg_object *a = pg_binder(&graph), *box_self = pg_binder(&graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *box_parameters = pg_prove_context_extension(&typing, a_context, box_self,
		pg_prove_universe(&typing, &classifiers, a_context, 0));
	const struct pg_evidence *box_fields = pg_prove_context_extension(&typing, box_parameters,
		pg_binder(&graph), pg_prove_variable(&typing, box_parameters, a));
	const struct pg_evidence *box_images[] = {
		pg_prove_variable(&typing, box_fields, a), pg_prove_variable(&typing, box_fields, box_self)};
	const struct pg_evidence *box_result = pg_prove_substitution(&typing, box_parameters, box_fields, 2, box_images);
	const struct pg_data_schema *box_schema = pg_data_schema(&typing,
		pg_data_signature(&typing, box_parameters, box_parameters), 1, &box_result);
	const struct pg_evidence *box = pg_prove_inductive_type(&typing, &classifiers, box_schema);
	assert(box && pg_evidence_context(box) == pg_evidence_context(a_context));
	assert(pg_evidence_subject(box)->core == pg_application(&graph,
		pg_reference(&graph, pg_data_family_object(box_schema)), pg_reference(&graph, a)));
	const struct pg_evidence *nat_value = pg_prove_type_value(&typing, nat);
	const struct pg_evidence *box_arguments = pg_prove_substitution(&typing, a_context, empty, 1, &nat_value);
	const struct pg_object *box_constructor = pg_data_constructor(pg_data_schema_layout(box_schema), 0);
	const struct pg_evidence *boxed = pg_prove_constructor(&typing, box, box_constructor, box_arguments, 1, &zero);
	assert(boxed && !pg_evidence_context(boxed));
	assert(pg_evidence_classifier(boxed) == pg_application(&graph,
		pg_reference(&graph, pg_data_family_object(box_schema)), pg_evidence_subject(nat)->core));
	const struct pg_evidence *boxed_type = pg_prove_classifier(&typing, &classifiers, empty, boxed);
	assert(pg_inductive_instance(&typing, boxed_type, &recovered));
	assert(recovered.formation == box && recovered.schema == box_schema);
	assert(pg_evidence_subject(pg_evidence_premise(recovered.parameters, 2))->core == pg_evidence_subject(nat)->core);
	const struct pg_evidence *projected_box = pg_prove_projection(&typing, n_context, boxed_type);
	const struct pg_evidence *substituted_box = pg_prove_reindex(&typing,
		pg_prove_substitution(&typing, n_context, empty, 1, &zero), projected_box);
	const struct pg_evidence *coerced_box = pg_prove_value_type(&typing, pg_prove_type_value(&typing, substituted_box));
	assert(pg_inductive_instance(&typing, coerced_box, &recovered));
	assert(recovered.formation == box && !pg_evidence_context(recovered.parameters));
	assert(pg_evidence_subject(pg_evidence_premise(recovered.parameters, 2))->core == pg_evidence_subject(nat)->core);
	const struct pg_evidence *reconstructed_box = pg_prove_reindex(&typing, recovered.parameters, recovered.formation);
	assert(pg_evidence_subject(reconstructed_box)->core == pg_evidence_subject(boxed_type)->core);
	proofs = typing.proofs.count; terms = graph.terms.count;
	const struct pg_evidence *recovered_map = recovered.parameters;
	assert(pg_inductive_instance(&typing, coerced_box, &recovered) && recovered.parameters == recovered_map);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	/* A constant codomain can retain a renamed parameter through REINDEX;
	 * recover the current binder, not the binder in the original proof. */
	{
		const struct pg_object *renamed = pg_binder(&graph);
		const struct pg_evidence *renamed_context = pg_prove_context_extension(&typing, empty, renamed, u);
		const struct pg_evidence *argument_context = pg_prove_context_extension(&typing, renamed_context,
			pg_binder(&graph), pg_prove_projection(&typing, renamed_context, nat));
		const struct pg_evidence *image = pg_prove_variable(&typing, argument_context, renamed);
		const struct pg_evidence *map = pg_prove_substitution(&typing, a_context, argument_context, 1, &image);
		const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, argument_context,
			pg_prove_return_type(&typing, &classifiers, pg_prove_reindex(&typing, map, box)));
		const struct pg_evidence *content = pg_prove_return_content(&typing,
			pg_prove_pi_constant_codomain(&typing, pi));
		assert(content && pg_inductive_instance(&typing, content, &recovered));
		assert(recovered.formation == box && recovered.schema == box_schema);
		assert(pg_evidence_context(recovered.parameters) == pg_evidence_context(renamed_context));
		assert(pg_evidence_subject(pg_evidence_premise(recovered.parameters, 2))->core == pg_reference(&graph, renamed));
		assert(!pg_prove_variable(&typing, renamed_context, a));
	}
	const struct pg_evidence *box_motive_context = pg_prove_context_extension(&typing, empty, pg_binder(&graph), boxed_type);
	const struct pg_evidence *box_motive = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, box_motive_context, nat));
	const struct pg_evidence *unboxed = pg_prove_match(&typing, &classifiers, box,
		box_arguments, boxed, box_motive_context, box_motive, 1, &pred_branch);
	assert(unboxed);
	assert(!pg_whnf_work_init(&constructor_work, &graph));
	check(&constructor_work, pg_evidence_subject(unboxed)->core, pg_evidence_subject(zero_function)->core);
	pg_whnf_work_destroy(&constructor_work);
	const struct pg_evidence *other_value = pg_prove_type_value(&typing, other);
	const struct pg_evidence *wrong_arguments = pg_prove_substitution(&typing, a_context, empty, 1, &other_value);
	assert(!pg_prove_constructor(&typing, box, box_constructor, wrong_arguments, 1, &zero));
	/* A large index universe does not become a constructor field bound. */
	const struct pg_evidence *large = pg_prove_universe(&typing, &classifiers, parameters, 4);
	const struct pg_evidence *indices = pg_prove_context_extension(&typing, parameters, pg_binder(&graph), large);
	const struct pg_evidence *images[] = {pg_prove_variable(&typing, parameters, self),
		pg_prove_type_value(&typing, pg_prove_universe(&typing, &classifiers, parameters, 3))};
	const struct pg_evidence *indexed_result = pg_prove_substitution(&typing, indices, parameters, 2, images);
	const struct pg_data_schema *indexed = pg_data_schema(&typing,
		pg_data_signature(&typing, parameters, indices), 1, &indexed_result);
	assert(indexed && !pg_data_schema_field_level(indexed, &level) && level == 0);
	assert(!pg_prove_inductive_type(&typing, &classifiers, indexed));
	/* A retained signature does not belong to a reinitialized typing store,
	 * even when no constructors would otherwise force a premise check. */
	pg_typing_destroy(&typing);
	assert(!pg_typing_init(&typing, &graph));
	assert(!pg_data_schema(&typing, signature, 0, NULL));
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
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
	size_t proof_count = typing.proofs.count;
	const struct pg_data_signature *signature = pg_data_signature(&typing, parameters, parameters);
	assert(signature && typing.proofs.count == proof_count);
	const struct pg_data_schema *schema = pg_data_schema(&typing, signature, 3, results);
	assert(schema);
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	const struct pg_object *ctor = pg_data_constructor(layout, 2);
	assert(pg_data_schema_fields(schema, ctor) == fields);
	const struct pg_data_schema *another = pg_data_schema(&typing, signature, 3, results);
	assert(another && pg_data_schema_layout(another) != layout);
	assert(typing.proofs.count == proof_count);
	const struct pg_term *images[] = {pg_reference(graph, a)};
	struct pg_data_constructor_input raw[] = {
		{pg_evidence_context(parameters), images},
		{pg_evidence_context(first), images},
		{pg_evidence_context(fields), images}
	};
	const struct pg_data_declaration *raw_declaration = pg_data_declaration(graph,
		pg_evidence_context(parameters), pg_evidence_context(parameters), 3, raw);
	assert(raw_declaration && typing.proofs.count == proof_count);
	const struct pg_data_schema *checked = pg_data_schema_check(&typing, raw_declaration, signature, 3, results);
	assert(checked && pg_data_schema_declaration(checked) == raw_declaration);
	assert(pg_data_family_object(checked) == pg_data_declaration_family(raw_declaration));
	assert(pg_data_family_object(checked) != pg_data_family_object(schema));
	const struct pg_data_schema *checked_again = pg_data_schema_check(&typing, raw_declaration, signature, 3, results);
	assert(checked_again && pg_data_family_object(checked_again) == pg_data_family_object(checked));
	assert(pg_data_schema_layout(checked_again) == pg_data_schema_layout(checked));
	/* Caller arrays are copied; another same-arity map cannot redefine a family. */
	images[0] = pg_reference(graph, x);
	assert(pg_data_schema_check(&typing, raw_declaration, signature, 3, results));
	const struct pg_data_declaration *wrong = pg_data_declaration(graph,
		pg_evidence_context(parameters), pg_evidence_context(parameters), 3, raw);
	assert(wrong && !pg_data_schema_check(&typing, wrong, signature, 3, results));
	assert(!pg_data_schema_check(&typing, raw_declaration, signature, 2, results));
	assert(!pg_data_schema_check(&foreign, raw_declaration, signature, 3, results));
	assert(!pg_data_schema_check(&typing, NULL, signature, 3, results));
	assert(typing.proofs.count == proof_count && !foreign.proofs.count);
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
	const struct pg_data_signature *indexed_signature = pg_data_signature(&typing, parameters, indices);
	const struct pg_evidence *index_instance = pg_data_signature_instance(&typing, indexed_signature, params, 2, values);
	assert(index_instance && pg_evidence_premise(index_instance, 0) == indices);
	assert(pg_data_signature_instance(&typing, indexed_signature, params, 2, values) == index_instance);
	assert(!pg_data_signature_instance(&typing, indexed_signature, params, 1, values));
	assert(!pg_data_signature_instance(&typing, indexed_signature, params, 2, NULL));
	assert(!pg_data_signature_instance(&foreign, indexed_signature, params, 2, values));
	assert(!pg_data_signature_instance(&typing, indexed_signature, empty, 2, values));
	assert(!pg_data_signature_instance(&typing, NULL, params, 0, NULL));
	const struct pg_evidence *wrong_indices[] = {values[1], values[0]};
	assert(!pg_data_signature_instance(&typing, indexed_signature, params, 2, wrong_indices));
	assert(pg_data_signature_instance(&typing, signature, params, 0, NULL) == params);
	assert(!pg_prove_substitution_extend(&typing, params, empty, 0, NULL));
	const struct pg_evidence *index_images[] = {pg_prove_variable(&typing, fields, a),
		pg_prove_variable(&typing, fields, x), pv};
	const struct pg_evidence *index_map = pg_prove_substitution(&typing, indices, fields, 3, index_images);
	const struct pg_data_schema *indexed = pg_data_schema(&typing, indexed_signature, 1, &index_map);
	assert(indexed);
	assert(pg_data_schema_indices(indexed) == indices);
	assert(pg_data_schema_indices(pg_data_schema(&typing, pg_data_signature(&typing, parameters, indices), 0, NULL)) == indices);
	const struct pg_object *indexed_ctor = pg_data_constructor(pg_data_schema_layout(indexed), 0);
	assert(pg_data_schema_result(indexed, indexed_ctor) == index_map);
	assert(pg_data_schema_fields(indexed, indexed_ctor) == fields);
	assert(pg_data_instance(&typing, indexed, indexed_ctor, params, 2, values) == instance);
	const struct pg_evidence *index_result = pg_data_result(&typing, indexed, indexed_ctor, instance);
	assert(index_result && pg_evidence_premise(index_result, 0) == indices);
	assert(pg_evidence_context(index_result) == pg_evidence_context(index_instance));
	for (size_t n = 0; n < 3; ++n)
		assert(pg_evidence_subject(pg_evidence_premise(index_result, n + 2))->core
			== pg_evidence_subject(pg_evidence_premise(index_instance, n + 2))->core);
	assert(pg_data_result(&typing, indexed, indexed_ctor, instance) == index_result);
	for (size_t n = 0; n < 2; ++n)
		assert(pg_evidence_subject(pg_evidence_premise(index_result, n + 3))->core == pg_evidence_subject(values[n])->core);
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, parameters, indices), 1, results));
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, parameters, empty), 0, NULL));
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
	assert(changed_parameter && !pg_data_schema(&typing, pg_data_signature(&typing, parameters, parameters), 1, &changed_parameter));
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
	const struct pg_reduction_certificate *receipt = pg_whnf_certificate(pg_whnf_request(&work, &pg_pure_policy,
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
	const struct pg_evidence *function_type = pg_prove_pi(&typing, &classifiers, under_z,
		pg_prove_classifier(&typing, &classifiers, under_z, return_p));
	const struct pg_evidence *function_body = pg_prove_lambda(&typing, function_type, return_p);
	const struct pg_evidence *function_branch = pg_data_branch(&typing, &classifiers, indexed, indexed_ctor, function_body);
	const struct pg_object *index_z = pg_binder(graph);
	const struct pg_evidence *index_a = pg_prove_value_type(&typing, pg_prove_variable(&typing, indices, a));
	const struct pg_evidence *index_under_z = pg_prove_context_extension(&typing, indices, index_z, index_a);
	const struct pg_evidence *function_motive = pg_prove_pi(&typing, &classifiers, index_under_z,
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
	assert(!pg_data_schema(&foreign, signature, 3, results));
	assert(!pg_data_schema(&foreign, pg_data_signature(&foreign, parameters, parameters), 3, results));
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, u, parameters), 0, NULL));
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, parameters, parameters), 1, &empty));
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, parameters, parameters), 1, NULL));
	const struct pg_evidence *foreign_empty = pg_prove_empty_context(&foreign);
	assert(!pg_data_schema(&typing, pg_data_signature(&typing, empty, empty), 1, &foreign_empty));
	const struct pg_evidence *empty_sub = pg_prove_substitution(&typing, empty, empty, 0, NULL);
	const struct pg_data_schema *unit_schema = pg_data_schema(&typing, pg_data_signature(&typing, empty, empty), 1, &empty_sub);
	assert(unit_schema && pg_data_instance(&typing, unit_schema,
		pg_data_constructor(pg_data_schema_layout(unit_schema), 0), empty_sub, 0, NULL) == empty_sub);
	assert(pg_data_schema(&typing, pg_data_signature(&typing, empty, empty), 0, NULL));
	assert(!pg_data_schema_layout(NULL) && !pg_data_schema_fields(NULL, ctor));
	/* Context identity, not a chosen derivation of that context, selects the
	 * parameter prefix. Both derivations remain available as immutable evidence. */
	const struct pg_evidence *alternate = pg_prove_context_extension(&typing, empty, a,
		pg_prove_value_type(&typing, pg_prove_type_value(&typing, u)));
	assert(alternate != parameters && pg_evidence_context(alternate) == pg_evidence_context(parameters));
	assert(pg_data_schema(&typing, pg_data_signature(&typing, alternate, parameters), 3, results));
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
	assert(pg_data_schema(&typing, pg_data_signature(&typing, parameters, parameters), 1, &boundary_result));
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
	const struct pg_evidence *acted_indices[2], *again[2];
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, paths, 2, acted_indices) == 0);
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, paths, 2, again) == 0);
	for (size_t n = 0; n < 2; ++n) {
		assert(acted_indices[n] == again[n]);
		check(&work, pg_evidence_subject(acted_indices[n])->core, pg_evidence_subject(paths[n])->core);
	}
	const struct pg_binding_face *index_centers[2];
	for (size_t n = 0; n < 2; ++n)
		index_centers[n] = pg_binding_face(&dimensions, pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *index_left, *index_right, *index_paths[2];
	const struct pg_evidence *index_boundary = pg_identity_context(&typing, &dimensions, indices, 2, index_centers,
		&index_left, &index_right, index_paths);
	assert(index_boundary);
	const struct pg_evidence *map_endpoints[] = {pg_data_result(&typing, indexed, indexed_ctor, left),
		pg_data_result(&typing, indexed, indexed_ctor, right)};
	const struct pg_evidence *image_values[7] = {pg_evidence_premise(map_endpoints[0], 2)};
	for (size_t n = 0; n < 2; ++n) {
		image_values[1 + 3 * n] = pg_evidence_premise(map_endpoints[0], n + 3);
		image_values[2 + 3 * n] = pg_evidence_premise(map_endpoints[1], n + 3);
		image_values[3 + 3 * n] = acted_indices[n];
	}
	/* Acting on an image's classifier and substituting into the acted
	 * classifier agree by computation, not necessarily structural alpha. */
	const struct pg_evidence *declarations[7], *declaration = index_boundary;
	for (size_t n = 7; n; --n) {
		declarations[n - 1] = declaration;
		declaration = pg_evidence_premise(declaration, 0);
	}
	const struct pg_evidence *acted_map = pg_prove_substitution(&typing, declaration, boundary, 0, NULL);
	for (size_t n = 0; n < 7; ++n) {
		const struct pg_evidence *expected = pg_prove_reindex(&typing, acted_map, pg_evidence_premise(declarations[n], 1));
		assert(expected);
		struct pg_conversion comparison;
		assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(image_values[n]), pg_evidence_subject(expected)->core) == 0);
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *converted = pg_prove_conversion(&typing, image_values[n], expected, pg_conversion_certificate(&comparison));
		acted_map = pg_prove_substitution_pair(&typing, acted_map, declarations[n], converted);
		assert(acted_map);
		pg_conversion_destroy(&comparison);
	}
	const struct pg_evidence *index_sides[] = {index_left, index_right};
	for (size_t side = 0; side < 2; ++side) {
		const struct pg_evidence *composite = pg_prove_substitution_compose(&typing, index_sides[side], acted_map);
		assert(composite);
		for (size_t n = 0; n < 3; ++n)
			assert(pg_alpha_equal(pg_evidence_subject(pg_evidence_premise(composite, n + 2))->core,
				pg_evidence_subject(pg_evidence_premise(map_endpoints[side], n + 2))->core) == 1);
	}
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, paths, 0, NULL) == 0);
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, left, 0, NULL, 2, again) == 0);
	for (size_t n = 0; n < 2; ++n)
		check(&work, pg_evidence_subject(again[n])->core,
			pg_identity_action(graph, pg_evidence_subject(pg_evidence_premise(map_endpoints[0], n + 3))->core));
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, paths, 2, again) == 0);
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, paths, 4, again) == -1);
	assert(pg_identity_substitution_images(&foreign, &classifiers, index_map, left, right, 2, paths, 2, again) == -1);
	const struct pg_evidence *invalid_paths[] = {paths[0], paths[0]};
	assert(pg_identity_substitution_images(&typing, &classifiers, index_map, left, right, 2, invalid_paths, 2, again) == -1);
	assert(again[0] == acted_indices[0] && again[1] == acted_indices[1]);
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&foreign);
	pg_typing_destroy(&typing);
	puts("data schemas: dependent fields/indices, fixed parameters, composition and selected boundaries passed");
}

int main(void)
{
	positive_fields();
	scoped_type_families();
	higher_family_lift();
	accessibility_elimination(PG_TOTALITY_UNSPECIFIED);
	accessibility_elimination(PG_TOTALITY_TOTAL);
	indexed_match();
	schema_positivity();
	retained_substitution_prefix();
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
	const struct pg_object *recursion = pg_binder(&graph);
	const struct pg_object *argument = pg_binder(&graph), *self_call = pg_binder(&graph);
	struct pg_match_clause recursive[] = {
		{z, zero},
		{s, pg_lambda(&graph, x, pg_application(&graph, pg_reference(&graph, recursion), vx))}
	};
	const struct pg_term *deep = zero;
	for (size_t i = 0; i < 64; ++i) deep = pg_application(&graph, succ, deep);
	const struct pg_term *countdown = pg_data_recursive_match(&graph, nat, recursion, argument, self_call, deep, 2, recursive);
	assert(countdown);
	size_t retained_terms = graph.terms.count;
	assert(pg_data_recursive_match(&graph, nat, recursion, argument, self_call, deep, 2, recursive) == countdown);
	assert(graph.terms.count == retained_terms);
	check(&work, countdown, zero);
	/* Recursive branches keep their lexical environment; the fixed point does
	 * not serialize a captured term inside a runtime descriptor. */
	recursive[0].branch = vy;
	const struct pg_term *captured_recursion = pg_lambda(&graph, y,
		pg_data_recursive_match(&graph, nat, recursion, argument, self_call, deep, 2, recursive));
	check(&work, pg_application(&graph, captured_recursion, foreign), foreign);
	recursive[1].branch = pg_lambda(&graph, x, omega);
	check(&work, pg_application(&graph, pg_lambda(&graph, y,
		pg_data_recursive_match(&graph, nat, recursion, argument, self_call, zero, 2, recursive)), foreign), foreign);
	assert(!pg_data_recursive_match(&graph, nat, z, argument, self_call, zero, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, self_call, zero, 1, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, self_call, zero, 2, NULL));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, self_call, NULL, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, recursion, self_call, zero, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, argument, zero, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, recursion, zero, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, NULL, self_call, zero, 2, recursive));
	assert(!pg_data_recursive_match(&graph, nat, recursion, argument, z, zero, 2, recursive));
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
