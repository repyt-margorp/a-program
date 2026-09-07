#include "synthesis.h"
#include "computation.h"
#include "identity.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Different proof paths may establish the same judgement without being interned
 * as one derivation. Check its semantic fields independently of that choice. */
static void same_judgement(const struct pg_evidence *left, const struct pg_evidence *right)
{
	assert(left && right);
	assert(pg_evidence_context(left) == pg_evidence_context(right));
	assert(pg_evidence_judgement(left) == pg_evidence_judgement(right));
	assert(pg_alpha_equal(pg_evidence_subject(left)->core, pg_evidence_subject(right)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(left), pg_evidence_classifier(right)) == 1);
}

static struct pg_synthesis_job *request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const char *source)
{
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, synthesis->typing->graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, definition.expression);
	assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_request(synthesis, scope, definition.expression) == job);
	assert(!pg_synthesis_result(job));
	assert(pg_parser_next(&parser, &definition) == 0);
	return job;
}

static const struct pg_evidence *complete(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, enum pg_synthesis_status expected)
{
	unsigned steps = 0;
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(!pg_synthesis_result(job));
		assert(++steps < 1000);
		pg_synthesis_advance(synthesis, 1);
	}
	assert(pg_synthesis_status(job) == expected);
	assert(!pg_synthesis_dependency(job));
	assert(!pg_synthesis_cycle(job));
	return pg_synthesis_result(job);
}

static const struct pg_evidence *normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *input)
{
	return complete(synthesis, pg_synthesis_normalize(synthesis, context, input), PG_SYNTHESIS_DONE);
}

static struct pg_synthesis_job *program(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const char *source)
{
	struct pg_parser parser;
	pg_parser_init(&parser, synthesis->typing->graph, source, strlen(source));
	const struct pg_syntax *syntax = pg_parser_program(&parser);
	assert(syntax);
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_request(synthesis, scope, syntax) == job);
	return job;
}

static int arbitrary_policy(struct pg_eval *machine)
{
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	return argument ? pg_eval_enter(machine, *argument, 1) : 1;
}

static void normalization_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *source, const struct pg_evidence *value)
{
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	struct pg_synthesis_job *job = pg_synthesis_normalize(&split, context, source);
	assert(job && pg_synthesis_normalize(&split, context, source) == job);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(job));
	struct pg_whnf_job *computation = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(source)->core);
	assert(!pg_whnf_certificate(computation));
	assert(!pg_prove_normalization(typing, source, pg_whnf_certificate(computation)));
	const struct pg_evidence *answer = complete(&split, job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(answer) == PG_PURE_NORMALIZATION);
	assert(pg_evidence_premise(answer, 0) == source);
	assert(pg_evidence_classifier(answer) == pg_evidence_classifier(source));
	assert(pg_evidence_judgement(answer) == pg_evidence_judgement(source));
	assert(pg_evidence_subject(answer)->core == pg_evidence_subject(pg_prove_return(typing, classifiers, value))->core);
	const struct pg_whnf_certificate *certificate = pg_whnf_certificate(computation);
	assert(certificate && certificate == pg_evidence_normalization(answer));
	assert(pg_whnf_source(certificate) == pg_evidence_subject(source)->core);
	assert(pg_whnf_target(certificate) == pg_evidence_subject(answer)->core);
	assert(!pg_evidence_conversion(answer));
	assert(pg_prove_normalization(typing, source, certificate) == answer);
	assert(pg_prove_classifier(typing, classifiers, context, answer));
	assert(!pg_prove_return_value(typing, source));
	const struct pg_evidence *content_value = complete(&split,
		pg_synthesis_return(&split, context, answer), PG_SYNTHESIS_DONE);
	same_judgement(content_value, value);
	assert(pg_evidence_rule(content_value) == PG_RETURN_VALUE);
	assert(pg_evidence_premise(content_value, 0) == answer);
	assert(!pg_prove_thunk_computation(typing, answer));
	uint64_t steps = pg_whnf_steps(computation);
	struct pg_synthesis_job *second = pg_synthesis_normalize(&whole, context, source);
	pg_synthesis_advance(&whole, 1000);
	assert(pg_synthesis_result(second) == answer && pg_whnf_steps(computation) == steps);
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, value);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, context, pg_binder(typing->graph), type);
	const struct pg_evidence *projected = pg_prove_projection(typing, extended, source);
	const struct pg_evidence *other = complete(&split,
		pg_synthesis_normalize(&split, extended, projected), PG_SYNTHESIS_DONE);
	assert(other != answer && pg_evidence_context(other) == pg_evidence_context(extended));
	assert(pg_evidence_normalization(other) == certificate && pg_whnf_steps(computation) == steps);
	const struct pg_evidence *projected_answer = pg_prove_projection(typing, extended, answer);
	const struct pg_evidence *projected_type = pg_prove_projection(typing, extended,
		pg_prove_classifier(typing, classifiers, context, answer));
	const struct pg_evidence *recoveries[] = {
		pg_prove_classifier(typing, classifiers, extended, projected_answer),
		pg_prove_classifier(typing, classifiers, extended, other)
	};
	for (size_t i = 0; i < 2; ++i) {
		assert(recoveries[i] && pg_evidence_context(recoveries[i]) == pg_evidence_context(extended));
		assert(pg_evidence_judgement(recoveries[i]) == pg_evidence_judgement(projected_type));
		assert(pg_alpha_equal(pg_evidence_subject(recoveries[i])->core,
			pg_evidence_subject(projected_type)->core) == 1);
	}
	assert(!pg_synthesis_normalize(&split, context, projected));
	assert(!pg_synthesis_normalize(&split, context, context));
	assert(complete(&split, pg_synthesis_normalize(&split, context, value), PG_SYNTHESIS_DONE) == value);
	const struct pg_evidence *acted = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, context, source), source);
	const struct pg_evidence *action_answer = complete(&split,
		pg_synthesis_normalize(&split, context, acted), PG_SYNTHESIS_DONE);
	assert(action_answer && pg_evidence_normalization(action_answer));
	assert(pg_evidence_classifier(action_answer) == pg_evidence_classifier(acted));
	const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, context, acted);
	const struct pg_evidence *normalized_type = complete(&split,
		pg_synthesis_normalize(&split, context, formation), PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(normalized_type) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(normalized_type) == pg_evidence_classifier(formation));
	const struct pg_term *content;
	assert(pg_return_type_view(pg_evidence_subject(normalized_type)->core, &content));
	static const struct pg_eval_policy foreign_policy = {arbitrary_policy};
	struct pg_whnf_job *foreign = pg_whnf_request(&work, &foreign_policy, pg_evidence_subject(source)->core);
	assert(pg_whnf_advance(foreign, 10000) == PG_EVAL_WHNF);
	assert(pg_whnf_certificate(foreign));
	assert(!pg_prove_normalization(typing, source, pg_whnf_certificate(foreign)));
	/* Equal by beta expansion is not directed reduction from a typed value. */
	const struct pg_term *bad = pg_application(typing->graph,
		pg_lambda(typing->graph, pg_binder(typing->graph), pg_evidence_subject(value)->core),
		pg_reference(typing->graph, pg_binder(typing->graph)));
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(value)->core, bad) == 0);
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	struct pg_whnf_job *expansion = pg_whnf_request(&work, &pg_pure_policy, bad);
	assert(pg_whnf_advance(expansion, 10000) == PG_EVAL_WHNF);
	assert(!pg_prove_normalization(typing, value, pg_whnf_certificate(expansion)));
	struct pg_typing foreign_typing;
	assert(pg_typing_init(&foreign_typing, typing->graph) == 0);
	assert(!pg_prove_normalization(&foreign_typing, source, certificate));
	pg_typing_destroy(&foreign_typing);
	/* Same erased FORCE/THUNK identity, different annotated function domains. */
	const struct pg_evidence *domains[] = {type, pg_prove_universe(typing, classifiers, context, 0)};
	const struct pg_evidence *typed_sources[2], *typed_answers[2];
	const struct pg_object *z = pg_binder(typing->graph);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *local = pg_prove_context_extension(typing, context, z, domains[i]);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, local, z));
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domains[i], local,
			pg_prove_classifier(typing, classifiers, local, body));
		typed_sources[i] = pg_prove_force(typing,
			pg_prove_thunk(typing, classifiers, pg_prove_lambda(typing, pi, body)));
		typed_answers[i] = complete(&split, pg_synthesis_normalize(&split, context, typed_sources[i]), PG_SYNTHESIS_DONE);
		assert(pg_evidence_classifier(typed_answers[i]) == pg_evidence_classifier(typed_sources[i]));
		assert(pg_evidence_premise(typed_answers[i], 0) == typed_sources[i]);
	}
	assert(pg_evidence_subject(typed_sources[0])->core == pg_evidence_subject(typed_sources[1])->core);
	assert(pg_evidence_subject(typed_answers[0])->core == pg_evidence_subject(typed_answers[1])->core);
	assert(pg_evidence_normalization(typed_answers[0]) == pg_evidence_normalization(typed_answers[1]));
	assert(pg_evidence_classifier(typed_answers[0]) != pg_evidence_classifier(typed_answers[1]));
	pg_synthesis_destroy(&whole);
	pg_synthesis_destroy(&split);
	pg_whnf_work_destroy(&work);
	assert(pg_evidence_normalization(answer) == certificate);
	assert(pg_whnf_target(certificate) == pg_evidence_subject(answer)->core);
	assert(pg_prove_normalization(typing, source, certificate) == answer);
}

static void identity_contents(struct pg_typing *typing, struct pg_classifiers *classifiers,
	struct pg_whnf_work *normalization, const struct pg_evidence *context,
	const struct pg_evidence *source, const struct pg_evidence *value,
	const struct pg_evidence *substitution)
{
	struct pg_synthesis split, whole;
	assert(pg_synthesis_init(&split, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *source_type = pg_prove_classifier(typing, classifiers, context, source);
	const struct pg_evidence *acted = pg_prove_reflexivity(typing, source_type, source);
	assert(acted);
	struct pg_synthesis_job *source_job = pg_synthesis_return(&split, context, source);
	struct pg_synthesis_job *acted_job = pg_synthesis_return(&split, context, acted);
	assert(acted_job && !pg_synthesis_result(acted_job));
	pg_synthesis_advance(&split, 1);
	assert(pg_synthesis_dependency(acted_job) == pg_synthesis_normalize(&split, context, acted));
	const struct pg_evidence *answer = complete(&split, acted_job, PG_SYNTHESIS_DONE);
	const struct pg_term *expected = pg_identity_action(typing->graph, pg_evidence_subject(value)->core);
	assert(pg_evidence_subject(answer)->core == expected);
	assert(pg_evidence_rule(answer) == PG_RETURN_VALUE);
	assert(pg_evidence_rule(pg_evidence_premise(answer, 0)) == PG_TYPE_CONVERSION);
	complete(&split, source_job, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_return(&split, context, acted) == acted_job);
	struct pg_synthesis_job *whole_source = pg_synthesis_return(&whole, context, source);
	struct pg_synthesis_job *whole_action = pg_synthesis_return(&whole, context, acted);
	pg_synthesis_advance(&whole, 1000);
	assert(pg_synthesis_status(whole_source) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whole_action) == PG_SYNTHESIS_DONE);
	same_judgement(pg_synthesis_result(whole_action), answer);
	/* The second scheduler shares already accepted reindex/conversion work;
	 * equal answers, not equal cold-start costs, are the invariant here. */
	assert(whole.steps <= split.steps);
	pg_synthesis_destroy(&whole);
	for (size_t dimension = 1; dimension < 4; ++dimension) {
		acted = pg_prove_reflexivity(typing,
			pg_prove_classifier(typing, classifiers, context, acted), acted);
		expected = pg_identity_action(typing->graph, expected);
		answer = complete(&split, pg_synthesis_return(&split, context, acted), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(answer)->core == expected);
		const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, answer);
		const struct pg_evidence *return_type = pg_prove_return_type(typing, classifiers, type);
		struct pg_conversion conversion;
		assert(pg_conversion_init(&conversion, normalization, pg_evidence_classifier(acted),
			pg_evidence_subject(return_type)->core) == 0);
		while (pg_conversion_advance(&conversion, 1) == PG_CONVERSION_PENDING)
			assert(pg_conversion_steps(&conversion) < 10000);
		assert(pg_conversion_status(&conversion) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *converted = pg_prove_conversion(typing, acted, return_type,
			pg_conversion_certificate(&conversion));
		pg_conversion_destroy(&conversion);
		assert(converted);
		same_judgement(complete(&split, pg_synthesis_return(&split, context, converted), PG_SYNTHESIS_DONE), answer);
	}
	const struct pg_evidence *moved = pg_prove_reindex(typing, substitution, acted);
	assert(moved);
	const struct pg_evidence *moved_answer = complete(&split,
		pg_synthesis_return(&split, context, moved), PG_SYNTHESIS_DONE);
	same_judgement(moved_answer, pg_prove_reindex(typing, substitution, answer));
	const struct pg_evidence *extension = pg_prove_context_extension(typing, context,
		pg_binder(typing->graph), pg_prove_classifier(typing, classifiers, context, value));
	const struct pg_evidence *projected = pg_prove_projection(typing, extension, acted);
	same_judgement(complete(&split, pg_synthesis_return(&split, extension, projected), PG_SYNTHESIS_DONE),
		pg_prove_projection(typing, extension, answer));
	const struct pg_evidence *new_value = pg_prove_variable(typing, extension,
		pg_evidence_context(extension)->binder);
	const struct pg_evidence *changed_images[] = {
		pg_prove_variable(typing, extension, pg_evidence_context(context)->parent->binder), new_value
	};
	const struct pg_evidence *changed = pg_prove_substitution(typing, context, extension, 2, changed_images);
	assert(changed);
	const struct pg_evidence *changed_action = pg_prove_reindex(typing, changed, acted);
	const struct pg_evidence *changed_answer = complete(&split,
		pg_synthesis_return(&split, extension, changed_action), PG_SYNTHESIS_DONE);
	same_judgement(changed_answer, pg_prove_reindex(typing, changed, answer));
	expected = pg_evidence_subject(new_value)->core;
	for (size_t dimension = 0; dimension < 4; ++dimension)
		expected = pg_identity_action(typing->graph, expected);
	assert(pg_evidence_subject(changed_answer)->core == expected);
	pg_synthesis_destroy(&split);

	/* Extracting acted THUNK code must not execute its beta-redex source. */
	assert(pg_synthesis_init(&split, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *quoted = pg_prove_thunk(typing, classifiers, source);
	const struct pg_evidence *quoted_type = pg_prove_classifier(typing, classifiers, context, quoted);
	const struct pg_evidence *quoted_action = pg_prove_reflexivity(typing, quoted_type, quoted);
	struct pg_synthesis_job *unthunk = pg_synthesis_unthunk(&split, context, quoted_action);
	pg_synthesis_advance(&split, 0);
	assert(!pg_synthesis_result(unthunk));
	const struct pg_evidence *code = complete(&split, unthunk, PG_SYNTHESIS_DONE);
	const struct pg_evidence *source_action = pg_prove_reflexivity(typing, source_type, source);
	assert(pg_evidence_subject(code)->core == pg_evidence_subject(source_action)->core);
	/* U observation retains FORCE(THUNK source) in the classifier. It is
	 * convertible to source, but is not a structurally identical endpoint. */
	struct pg_conversion code_type;
	assert(pg_conversion_init(&code_type, normalization, pg_evidence_classifier(code),
		pg_evidence_classifier(source_action)) == 0);
	assert(pg_conversion_advance(&code_type, 10000) == PG_CONVERSION_EQUAL);
	same_judgement(pg_prove_conversion(typing, code,
		pg_prove_classifier(typing, classifiers, context, source_action),
		pg_conversion_certificate(&code_type)), source_action);
	pg_conversion_destroy(&code_type);
	assert(pg_evidence_rule(code) == PG_THUNK_COMPUTATION);
	assert(!pg_synthesis_return(&split, context, quoted_action));
	assert(!pg_synthesis_unthunk(&split, context, code));
	pg_synthesis_destroy(&split);
}

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_whnf_work beta;
	struct pg_synthesis synthesis;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	assert(pg_whnf_work_init(&beta, &graph) == 0);
	assert(pg_synthesis_init(&synthesis, &typing, &classifiers, &beta, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	struct pg_synthesis_job *polymorphic = request(&synthesis, root, "id := \\A : @ => \\x : A => x;");
	pg_synthesis_advance(&synthesis, 0);
	assert(pg_synthesis_status(polymorphic) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *identity = complete(&synthesis, polymorphic, PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(identity) == PG_JUDGEMENT_COMPUTATION);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	assert(pg_pi_view(pg_evidence_classifier(identity), &domain, &binder, &codomain));
	assert(domain == pg_universe(&classifiers, 0));
	assert(pg_pi_view(codomain, &domain, &binder, &codomain));
	assert(pg_return_type_view(codomain, &codomain) && codomain == domain);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *x = pg_binder(&graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, a, universe);
	struct pg_token a_name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "A"};
	struct pg_token x_name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "x"};
	const struct pg_source_scope *a_scope = pg_synthesis_bind(&synthesis, root, a_name, a, a_context);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_context, a);
	const struct pg_evidence *typed_application = pg_prove_application(&typing,
		pg_prove_projection(&typing, a_context, identity), a_type);
	const struct pg_evidence *typed_reduct = normalize(&synthesis, a_context, typed_application);
	assert(typed_reduct && pg_evidence_subject(typed_reduct)->core->kind == PG_LAMBDA);
	assert(pg_evidence_subject(typed_application)->core->kind == PG_APPLICATION);
	assert(pg_alpha_equal(pg_evidence_classifier(typed_application), pg_evidence_classifier(typed_reduct)) == 1);
	size_t reduction_terms = graph.terms.count, reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) assert(normalize(&synthesis, a_context, typed_application) == typed_reduct);
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	const struct pg_evidence *x_value = pg_prove_variable(&typing, x_context, x);
	const struct pg_evidence *second_application = pg_prove_application(&typing,
		pg_prove_projection(&typing, x_context, typed_reduct), x_value);
	const struct pg_evidence *second_reduct = normalize(&synthesis, x_context, second_application);
	const struct pg_evidence *return_x = pg_prove_return(&typing, &classifiers, x_value);
	assert(second_reduct && pg_evidence_subject(second_reduct)->core == pg_evidence_subject(return_x)->core);
	assert(pg_evidence_classifier(second_reduct) == pg_evidence_classifier(return_x));
	const struct pg_evidence *images[] = {pg_prove_variable(&typing, x_context, a), x_value};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, x_context, x_context, 2, images);
	const struct pg_evidence *twice_reindexed = pg_prove_reindex(&typing, sigma,
		pg_prove_projection(&typing, x_context, typed_reduct));
	const struct pg_evidence *third_application = pg_prove_application(&typing, twice_reindexed, x_value);
	const struct pg_evidence *third_reduct = normalize(&synthesis, x_context, third_application);
	assert(third_reduct && pg_evidence_subject(third_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *reindexed_application = pg_prove_reindex(&typing, sigma, second_application);
	const struct pg_evidence *reindexed_reduct = normalize(&synthesis, x_context, reindexed_application);
	assert(reindexed_reduct && pg_evidence_subject(reindexed_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *projected_application = pg_prove_projection(&typing, x_context, typed_application);
	const struct pg_evidence *projected_reduct = normalize(&synthesis, x_context, projected_application);
	assert(projected_reduct && pg_alpha_equal(pg_evidence_subject(projected_reduct)->core,
		pg_evidence_subject(typed_reduct)->core) == 1);
	reduction_terms = graph.terms.count;
	reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(normalize(&synthesis, x_context, second_application) == second_reduct);
		assert(normalize(&synthesis, x_context, third_application) == third_reduct);
		assert(normalize(&synthesis, x_context, reindexed_application) == reindexed_reduct);
		assert(normalize(&synthesis, x_context, projected_application) == projected_reduct);
	}
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, a_scope, x_name, x, x_context);
	identity_contents(&typing, &classifiers, &beta, x_context, second_application, x_value, sigma);
	normalization_jobs(&typing, &classifiers, x_context, second_application, x_value);
	const struct pg_evidence *deep_body = return_x;
	for (size_t i = 0; i < 120; ++i)
		deep_body = pg_prove_return(&typing, &classifiers, pg_prove_thunk(&typing, &classifiers, deep_body));
	const struct pg_evidence *deep_pi = pg_prove_pi(&typing, &classifiers, a_type, x_context,
		pg_prove_classifier(&typing, &classifiers, x_context, deep_body));
	const struct pg_evidence *deep_function = pg_prove_projection(&typing, x_context,
		pg_prove_lambda(&typing, deep_pi, deep_body));
	const struct pg_evidence *deep_application = pg_prove_application(&typing, deep_function, x_value);
	struct pg_synthesis_job *deep_step = pg_synthesis_normalize(&synthesis, x_context, deep_application);
	assert(deep_step && !pg_synthesis_result(deep_step));
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_whnf_status(pg_whnf_request(&beta, &pg_pure_policy,
		pg_evidence_subject(deep_application)->core)) == PG_EVAL_PENDING);
	pg_synthesis_advance(&synthesis, 32);
	assert(pg_synthesis_status(deep_step) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(deep_step));
	for (size_t i = 0; pg_synthesis_status(deep_step) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 1000);
		pg_synthesis_advance(&synthesis, 11);
	}
	const struct pg_evidence *deep_result = pg_synthesis_result(deep_step);
	assert(deep_result && pg_evidence_subject(deep_result)->core == pg_evidence_subject(deep_body)->core);
	assert(deep_result == normalize(&synthesis, x_context, deep_application));
	assert(pg_synthesis_normalize(&synthesis, x_context, deep_application) == deep_step);
	const struct pg_evidence *delayed_type = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_return_type(&typing, &classifiers, pg_prove_variable(&typing, x_context, a)));
	const struct pg_object *m = pg_binder(&graph);
	const struct pg_evidence *m_context = pg_prove_context_extension(&typing, x_context, m, delayed_type);
	const struct pg_evidence *m_value = pg_prove_variable(&typing, m_context, m);
	assert(!pg_prove_thunk_computation(&typing, m_value));
	complete(&synthesis, pg_synthesis_unthunk(&synthesis, m_context, m_value), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *neutral_force = pg_prove_force(&typing, m_value);
	assert(!pg_prove_return_value(&typing, neutral_force));
	complete(&synthesis, pg_synthesis_return(&synthesis, m_context, neutral_force), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *delayed_x = pg_prove_thunk(&typing, &classifiers, return_x);
	const struct pg_evidence *m_images[] = {pg_prove_variable(&typing, x_context, a), x_value, delayed_x};
	const struct pg_evidence *m_substitution = pg_prove_substitution(&typing, m_context, x_context, 3, m_images);
	const struct pg_evidence *substituted_m = pg_prove_reindex(&typing, m_substitution, m_value);
	same_judgement(substituted_m, delayed_x);
	assert(pg_evidence_rule(substituted_m) == PG_REINDEX);
	assert(pg_evidence_premise(substituted_m, 1) == m_value);
	const struct pg_evidence *substituted_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, x_context, substituted_m), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(substituted_code)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *substituted_force = pg_prove_reindex(&typing, m_substitution, pg_prove_force(&typing, m_value));
	const struct pg_evidence *distributed_force = pg_prove_force(&typing, substituted_m);
	same_judgement(substituted_force, distributed_force);
	const struct pg_evidence *distributed_application = pg_prove_application(&typing,
		pg_prove_reindex(&typing, sigma, pg_evidence_premise(second_application, 0)),
		pg_prove_reindex(&typing, sigma, x_value));
	same_judgement(reindexed_application, distributed_application);
	const struct pg_evidence *m_fold = pg_prove_fold(&typing, pg_prove_force(&typing, m_value),
		pg_prove_projection(&typing, m_context, typed_reduct));
	const struct pg_evidence *substituted_fold = pg_prove_reindex(&typing, m_substitution, m_fold);
	const struct pg_evidence *distributed_fold = pg_prove_fold(&typing, substituted_force,
		pg_prove_reindex(&typing, m_substitution, pg_evidence_premise(m_fold, 1)));
	same_judgement(substituted_fold, distributed_fold);
	struct pg_synthesis_job *substituted_return = pg_synthesis_return(&synthesis, x_context, substituted_force);
	const struct pg_evidence *substituted_result = complete(&synthesis, substituted_return, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(substituted_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(substituted_result) == pg_reference(&graph, a));
	assert(pg_evidence_rule(substituted_force) == PG_REINDEX);
	const struct pg_evidence *extended_m = pg_prove_context_extension(&typing, m_context,
		pg_binder(&graph), pg_prove_variable(&typing, m_context, a));
	const struct pg_evidence *extended_images[] = {m_images[0], m_images[1], m_images[2], x_value};
	const struct pg_evidence *extended_substitution = pg_prove_substitution(&typing,
		extended_m, x_context, 4, extended_images);
	const struct pg_evidence *weakened_force = pg_prove_projection(&typing, extended_m,
		pg_prove_force(&typing, m_value));
	const struct pg_evidence *substituted_weakening = pg_prove_reindex(&typing, extended_substitution, weakened_force);
	same_judgement(substituted_weakening, substituted_force);
	assert(pg_evidence_premise(substituted_weakening, 1) == weakened_force);
	const struct pg_evidence *weakening_result = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, substituted_weakening), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(weakening_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(weakening_result) == pg_reference(&graph, a));
	size_t weakening_proofs = typing.proofs.count, weakening_terms = graph.terms.count;
	for (size_t i = 0; i < 20; ++i)
		assert(pg_prove_reindex(&typing, extended_substitution, weakened_force) == substituted_weakening);
	assert(typing.proofs.count == weakening_proofs && graph.terms.count == weakening_terms);
	const struct pg_evidence *nested_result = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, substituted_fold), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(nested_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(nested_result) == pg_reference(&graph, a));
	struct pg_synthesis_job *substituted_force_step = pg_synthesis_normalize(&synthesis, x_context, substituted_force);
	const struct pg_evidence *force_step_result = complete(&synthesis, substituted_force_step, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(force_step_result)->core == pg_evidence_subject(return_x)->core);
	assert(pg_evidence_rule(substituted_force) == PG_REINDEX);
	uint64_t image_steps = synthesis.steps;
	assert(pg_synthesis_return(&synthesis, x_context, substituted_force) == substituted_return);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == image_steps);
	const struct pg_evidence *a_images[] = {a_type};
	const struct pg_evidence *a_substitution = pg_prove_substitution(&typing, a_context, a_context, 1, a_images);
	const struct pg_evidence *reindexed_polymorphic = pg_prove_reindex(&typing, a_substitution, typed_application);
	struct pg_synthesis_job *polymorphic_step = pg_synthesis_normalize(&synthesis, a_context, reindexed_polymorphic);
	const struct pg_evidence *polymorphic_result = complete(&synthesis, polymorphic_step, PG_SYNTHESIS_DONE);
	assert(pg_alpha_equal(pg_evidence_classifier(polymorphic_result), pg_evidence_classifier(reindexed_polymorphic)) == 1);
	assert(pg_alpha_equal(pg_evidence_subject(polymorphic_result)->core, pg_evidence_subject(typed_reduct)->core) == 1);
	image_steps = synthesis.steps;
	assert(pg_synthesis_normalize(&synthesis, a_context, reindexed_polymorphic) == polymorphic_step);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == image_steps);
	struct pg_synthesis_job *callee_step = pg_synthesis_normalize(&synthesis, x_context, projected_application);
	const struct pg_evidence *outer_application = pg_prove_application(&typing, projected_application, x_value);
	struct pg_synthesis_job *outer_step = pg_synthesis_normalize(&synthesis, x_context, outer_application);
	assert(callee_step && outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(!pg_synthesis_result(outer_step));
	assert(pg_synthesis_status(callee_step) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *other_application = pg_prove_application(&typing, projected_application,
		pg_prove_reindex(&typing, sigma, x_value));
	struct pg_synthesis_job *other_step = pg_synthesis_normalize(&synthesis, x_context, other_application);
	assert(other_step && other_step != outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(!pg_synthesis_result(other_step));
	const struct pg_evidence *outer_reduct = complete(&synthesis, outer_step, PG_SYNTHESIS_DONE);
	const struct pg_evidence *other_reduct = complete(&synthesis, other_step, PG_SYNTHESIS_DONE);
	assert(other_reduct != outer_reduct);
	assert(pg_evidence_normalization(other_reduct) == pg_evidence_normalization(outer_reduct));
	same_judgement(other_reduct, outer_reduct);
	assert(outer_reduct == normalize(&synthesis, x_context, outer_application));
	complete(&synthesis, callee_step, PG_SYNTHESIS_DONE);
	uint64_t shared_steps = synthesis.steps;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_normalize(&synthesis, x_context, projected_application) == callee_step);
		assert(pg_synthesis_normalize(&synthesis, x_context, outer_application) == outer_step);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(synthesis.steps == shared_steps);
	assert(!pg_synthesis_normalize(&synthesis, a_context, outer_application));
	size_t evaluation_jobs = synthesis.jobs.count;
	struct pg_synthesis_job *shared_return = pg_synthesis_return(&synthesis, x_context, second_application);
	assert(shared_return && pg_synthesis_status(shared_return) == PG_SYNTHESIS_PENDING);
	assert(synthesis.jobs.count == evaluation_jobs + 1);
	assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
	uint64_t evaluation_steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 0);
	assert(synthesis.steps == evaluation_steps && !pg_synthesis_result(shared_return));
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_synthesis_status(shared_return) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
	const struct pg_evidence *shared_value = complete(&synthesis, shared_return, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(shared_value)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(shared_value) == pg_reference(&graph, a));
	evaluation_steps = synthesis.steps;
	reduction_terms = graph.terms.count;
	reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(synthesis.steps == evaluation_steps);
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	assert(!pg_synthesis_return(&synthesis, a_context, second_application));
	assert(!pg_synthesis_return(&synthesis, x_context, x_value));
	assert(!pg_synthesis_return(&synthesis, NULL, second_application));
	struct pg_synthesis_job *other_return = pg_synthesis_return(&synthesis, x_context, reindexed_application);
	assert(other_return && other_return != shared_return);
	assert(pg_evidence_subject(complete(&synthesis, other_return, PG_SYNTHESIS_DONE))->core == pg_reference(&graph, x));
	const struct pg_evidence *extra_context = pg_prove_context_extension(&typing, x_context,
		pg_binder(&graph), pg_prove_variable(&typing, x_context, a));
	const struct pg_evidence *projected_input = pg_prove_projection(&typing, extra_context, second_application);
	assert(pg_evidence_subject(projected_input)->core == pg_evidence_subject(second_application)->core);
	struct pg_synthesis_job *projected_return = pg_synthesis_return(&synthesis, extra_context, projected_input);
	assert(projected_return && projected_return != shared_return);
	assert(pg_evidence_context(complete(&synthesis, projected_return, PG_SYNTHESIS_DONE)) == pg_evidence_context(extra_context));
	struct pg_typing foreign_typing;
	assert(pg_typing_init(&foreign_typing, &graph) == 0);
	const struct pg_evidence *foreign_context = pg_prove_empty_context(&foreign_typing);
	const struct pg_evidence *foreign_value = pg_prove_type_value(&foreign_typing,
		pg_prove_universe(&foreign_typing, &classifiers, foreign_context, 0));
	assert(!pg_synthesis_return(&synthesis, foreign_context,
		pg_prove_return(&foreign_typing, &classifiers, foreign_value)));
	pg_typing_destroy(&foreign_typing);
	struct pg_parser shared_parser;
	struct pg_definition shared_definition;
	const char *shared_source = "main := \\y : ((\\T : @ => T) A) => y;";
	pg_parser_init(&shared_parser, &graph, shared_source, strlen(shared_source));
	assert(pg_parser_next(&shared_parser, &shared_definition) == 1);
	struct pg_syntax *shared_copy = pg_alloc(&graph, sizeof(*shared_copy));
	assert(shared_copy);
	*shared_copy = *shared_definition.expression;
	struct pg_synthesis_job *first_consumer = pg_synthesis_request(&synthesis, scope, shared_definition.expression);
	struct pg_synthesis_job *second_consumer = pg_synthesis_request(&synthesis, scope, shared_copy);
	assert(first_consumer != second_consumer);
	complete(&synthesis, first_consumer, PG_SYNTHESIS_DONE);
	complete(&synthesis, second_consumer, PG_SYNTHESIS_DONE);
	const struct pg_evidence *shared_input = pg_synthesis_result(pg_synthesis_request(&synthesis, scope,
		shared_definition.expression->left));
	evaluation_jobs = synthesis.jobs.count;
	struct pg_synthesis_job *type_producer = pg_synthesis_return(&synthesis, x_context, shared_input);
	assert(type_producer && pg_synthesis_status(type_producer) == PG_SYNTHESIS_DONE);
	assert(synthesis.jobs.count == evaluation_jobs);
	const struct pg_evidence *computed_domain = complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\T : @ => T) A) => y;"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(computed_domain), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	const struct pg_evidence *computed_codomain = complete(&synthesis, request(&synthesis, scope,
		"main := A -> ((\\T : @ => T) A);"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_subject(computed_codomain)->core, &domain, &binder, &codomain));
	assert(pg_return_type_view(codomain, &codomain) && codomain == pg_reference(&graph, a));
	const char *checked_types[] = {
		"main := \\y : (((\\T : @ => T) A) :: @) => y;",
		"main := \\y : ((\\T : @ => (T :: @)) A) => y;",
		"main := \\y : ((\\T : @ => ((\\S : @ => S) T) :: @) A) => y;"
	};
	for (size_t i = 0; i < sizeof(checked_types) / sizeof(*checked_types); ++i) {
		const struct pg_evidence *checked_type = complete(&synthesis,
			request(&synthesis, scope, checked_types[i]), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_classifier(checked_type), pg_evidence_classifier(computed_domain)) == 1);
	}
	const struct pg_evidence *computed_expect = complete(&synthesis, request(&synthesis, scope,
		"main := x :: ((\\T : @ => T) ((\\S : @ => S) A));"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(computed_expect)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(computed_expect) == pg_reference(&graph, a));
	const struct pg_evidence *computed_function_domain = complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\T : @ => \\S : @ => S) A A) => y;"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(computed_function_domain), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\z : A => z) x) => y;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *original_quote = pg_prove_thunk(&typing, &classifiers, computed_domain);
	const struct pg_evidence *original_return = pg_prove_return(&typing, &classifiers, original_quote);
	const struct pg_evidence *target_quote_type = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_classifier(&typing, &classifiers, x_context, computed_function_domain));
	const struct pg_evidence *target_return_type = pg_prove_return_type(&typing, &classifiers, target_quote_type);
	assert(pg_evidence_classifier(original_quote) != pg_evidence_subject(target_quote_type)->core);
	struct pg_conversion return_conversion;
	assert(pg_conversion_init(&return_conversion, &beta, pg_evidence_classifier(original_return),
		pg_evidence_subject(target_return_type)->core) == 0);
	while (pg_conversion_advance(&return_conversion, 1) == PG_CONVERSION_PENDING) {}
	assert(pg_conversion_status(&return_conversion) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted_return = pg_prove_conversion(&typing, original_return,
		target_return_type, pg_conversion_certificate(&return_conversion));
	pg_conversion_destroy(&return_conversion);
	assert(converted_return);
	struct pg_synthesis_job *converted_value_job = pg_synthesis_return(&synthesis, x_context, converted_return);
	const struct pg_evidence *converted_value = complete(&synthesis, converted_value_job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(converted_value) == PG_RETURN_VALUE);
	assert(pg_evidence_classifier(converted_value) == pg_evidence_subject(target_quote_type)->core);
	assert(pg_alpha_equal(pg_evidence_subject(converted_value)->core, pg_evidence_subject(original_quote)->core) == 1);
	assert(pg_evidence_classifier(original_quote) != pg_evidence_classifier(converted_value));
	assert(pg_evidence_premise(converted_value, 0) == converted_return);
	assert(pg_evidence_conversion(converted_return));
	struct pg_synthesis_job *unthunk_job = pg_synthesis_unthunk(&synthesis, x_context, converted_value);
	assert(unthunk_job && pg_synthesis_status(unthunk_job) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *unthunked = complete(&synthesis, unthunk_job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(unthunked) == PG_THUNK_COMPUTATION);
	assert(pg_alpha_equal(pg_evidence_subject(unthunked)->core, pg_evidence_subject(computed_domain)->core) == 1);
	assert(pg_thunk_type_view(pg_evidence_classifier(converted_value), &codomain));
	assert(pg_evidence_classifier(unthunked) == codomain);
	assert(pg_evidence_premise(unthunked, 0) == converted_value);
	const struct pg_evidence *force_converted = pg_prove_force(&typing, converted_value);
	const struct pg_evidence *force_result = complete(&synthesis,
		pg_synthesis_normalize(&synthesis, x_context, force_converted), PG_SYNTHESIS_DONE);
	same_judgement(force_result, unthunked);
	assert(pg_evidence_premise(force_result, 0) == force_converted);
	const struct pg_evidence *converted_application = pg_prove_application(&typing, unthunked, x_value);
	const struct pg_evidence *converted_application_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, converted_application), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(converted_application_value)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(converted_application_value) == pg_reference(&graph, a));
	assert(pg_evidence_classifier(force_result) == pg_evidence_classifier(force_converted));
	assert(!pg_synthesis_unthunk(&synthesis, x_context, force_converted));
	assert(!pg_synthesis_unthunk(&synthesis, a_context, converted_value));
	const struct pg_evidence *projected_conversion = pg_prove_projection(&typing, extra_context, converted_return);
	const struct pg_evidence *projected_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, extra_context, projected_conversion), PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(projected_value) == pg_evidence_context(extra_context));
	assert(pg_evidence_classifier(projected_value) == pg_evidence_classifier(converted_value));
	const struct pg_evidence *projected_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, extra_context, projected_value), PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(projected_code) == pg_evidence_context(extra_context));
	assert(pg_evidence_classifier(projected_code) == pg_evidence_classifier(unthunked));
	const struct pg_evidence *reindexed_conversion = pg_prove_reindex(&typing, sigma, converted_return);
	const struct pg_evidence *reindexed_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, reindexed_conversion), PG_SYNTHESIS_DONE);
	assert(pg_return_type_view(pg_evidence_classifier(reindexed_conversion), &codomain));
	assert(pg_alpha_equal(pg_evidence_classifier(reindexed_value), codomain) == 1);
	const struct pg_evidence *reindexed_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, x_context, reindexed_value), PG_SYNTHESIS_DONE);
	assert(pg_thunk_type_view(pg_evidence_classifier(reindexed_value), &codomain));
	assert(pg_alpha_equal(pg_evidence_classifier(reindexed_code), codomain) == 1);
	uint64_t conversion_steps = synthesis.steps;
	assert(pg_synthesis_return(&synthesis, x_context, converted_return) == converted_value_job);
	assert(pg_synthesis_unthunk(&synthesis, x_context, converted_value) == unthunk_job);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == conversion_steps);
	struct pg_synthesis_job *batched_type = request(&synthesis, scope,
		"main := \\y : ((\\T : @ => \\S : @ => S) A A) => y;");
	pg_synthesis_advance(&synthesis, 0);
	assert(pg_synthesis_status(batched_type) == PG_SYNTHESIS_PENDING);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(batched_type) == PG_SYNTHESIS_DONE);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(batched_type))->core,
		pg_evidence_subject(computed_function_domain)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(pg_synthesis_result(batched_type)),
		pg_evidence_classifier(computed_function_domain)) == 1);
	complete(&synthesis, request(&synthesis, scope,
		"main := missing :: ((\\T : @ => T) A);"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\f : @ -> @ => \\y : f A => y;"), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *application = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) x;"), PG_SYNTHESIS_DONE);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(application)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	const struct pg_term *expected = pg_application(&graph, pg_reference(&graph, &pg_return_operation), pg_reference(&graph, x));
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	const struct pg_evidence *typed_steps[] = {second_application, third_application, reindexed_application,
		outer_application, distributed_application, distributed_force, distributed_fold};
	for (size_t i = 0; i < sizeof(typed_steps) / sizeof(*typed_steps); ++i) {
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(typed_steps[i])->core);
		assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == pg_evidence_subject(return_x)->core);
		pg_eval_destroy(&machine);
	}
	const struct pg_evidence *sequenced_higher = complete(&synthesis, request(&synthesis, scope,
		"main := (\\f : A -> A => f x) { &(\\y : A => y); };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(sequenced_higher) == PG_FOLD_ELIM);
	const struct pg_evidence *sequence_body = pg_evidence_premise(pg_evidence_premise(sequenced_higher, 1), 1);
	assert(pg_evidence_rule(sequence_body) == PG_APP_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(sequence_body, 1)) == PG_TYPE_CONVERSION);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(sequenced_higher)->core);
	assert(pg_eval_advance(&machine, 300) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis, request(&synthesis, scope,
		"main := (\\f : A -> A => f x) { &(\\y : A => A); };"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *checked = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) :: A -> A;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(checked) == PG_TYPE_CONVERSION);
	complete(&synthesis, request(&synthesis, scope, "main := (\\y : A => missing) :: A -> A;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := x :: @;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := &(\\y : A => y);"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, scope, "main := (&(\\y : A => y)) x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *higher = complete(&synthesis,
		request(&synthesis, scope, "main := (\\f : A -> A => f x) &(\\y : A => y);"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(higher) == PG_APP_ELIM);
	const struct pg_evidence *converted_argument = pg_evidence_premise(higher, 1);
	assert(pg_evidence_rule(converted_argument) == PG_TYPE_CONVERSION);
	const struct pg_conversion_certificate *conversion = pg_evidence_conversion(converted_argument);
	assert(conversion && pg_conversion_left(conversion) != pg_conversion_right(conversion));
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(higher)->core);
	assert(pg_eval_advance(&machine, 200) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis,
		request(&synthesis, scope, "main := (\\f : A -> A => f x) &(\\y : A => A);"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *sequenced = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) ((\\y : A => y) x);"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(sequenced) == PG_FOLD_ELIM);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(sequenced)->core);
	assert(pg_eval_advance(&machine, 200) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\z : A => (\\y : A => y) ((\\y : A => y) z);"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *curried = complete(&synthesis, request(&synthesis, scope,
		"main := ((\\u : A => \\v : A => v) ((\\y : A => y) x)) x;"), PG_SYNTHESIS_DONE);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(curried)->core);
	assert(pg_eval_advance(&machine, 300) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	const struct pg_evidence *typed_programs[] = {application, higher, sequenced_higher, sequenced, curried};
	for (size_t i = 0; i < sizeof(typed_programs) / sizeof(*typed_programs); ++i) {
		const struct pg_evidence *value = complete(&synthesis,
			pg_synthesis_return(&synthesis, x_context, typed_programs[i]), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(value)->core == pg_reference(&graph, x));
		assert(pg_evidence_classifier(value) == pg_reference(&graph, a));
	}
	complete(&synthesis, request(&synthesis, root, "Nat := @{zero:*; succ:*->*;};"), PG_SYNTHESIS_UNSUPPORTED);
	const char *blocks[] = {
		"main := { alias := x; alias; };",
		"main := { first := (\\y : A => y) x; second := (\\y : A => y) first; second; };",
		"main := { selected := x; dead := missing; }.selected;",
		"main := { x := x; x; };",
		"main := { x; (\\y : A => y) x; };",
		"main := { result : A := (\\y : A => y) x; result; };",
		"main := { outer := { inner := x; inner; }; outer; };",
		"main := { f := &(\\y : A => y); f x; };",
		"main := { &(\\y : A => y); } x;",
		"main := (&{ &(\\y : A => y); }) x;",
		"main := { &(\\y : A => y); } { x; };",
		"main := { &(\\f : A -> A => f x); } { &(\\y : A => y); };"
	};
	for (size_t i = 0; i < sizeof(blocks) / sizeof(*blocks); ++i) {
		const struct pg_evidence *block = complete(&synthesis, request(&synthesis, scope, blocks[i]), PG_SYNTHESIS_DONE);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(block)->core);
		assert(pg_eval_advance(&machine, 500) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
		const struct pg_evidence *value = complete(&synthesis,
			pg_synthesis_return(&synthesis, x_context, block), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(value)->core == pg_reference(&graph, x));
		assert(pg_evidence_classifier(value) == pg_reference(&graph, a));
	}
	complete(&synthesis, request(&synthesis, scope, "main := { temp := x; }.missing;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *ordered = complete(&synthesis, request(&synthesis, scope,
		"main := { &(\\y : A => y); } { x; };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(ordered) == PG_FOLD_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(ordered, 0)) == PG_RETURN_INTRO);
	const struct pg_evidence *argument_fold = pg_evidence_premise(pg_evidence_premise(ordered, 1), 1);
	assert(pg_evidence_rule(argument_fold) == PG_FOLD_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(argument_fold, 0)) == PG_CONTEXT_PROJECTION);
	const struct pg_evidence *ordered_app = pg_evidence_premise(pg_evidence_premise(argument_fold, 1), 1);
	assert(pg_evidence_rule(ordered_app) == PG_APP_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(ordered_app, 0)) == PG_FORCE_ELIM);
	complete(&synthesis, request(&synthesis, scope, "main := { x; } x;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := { &(\\y : A => y); } { A; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { temp := x; temp := x; temp; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { temp : @ := x; temp; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { !x; };"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, request(&synthesis, scope, "main := &{ x; };"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *dependent_block = complete(&synthesis, request(&synthesis, scope,
		"main := { B := A; \\y : B => y; };"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(dependent_block), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	assert(pg_return_type_view(codomain, &codomain) && codomain == domain);
	const char *dependent_computations[] = {
		"main := { B := (\\T : @ => T) A; \\y : B => y; };",
		"main := { B := (\\T : @ => T) A; C := (\\T : @ => T) B; \\y : C => y; };",
		"main := (\\B : @ => \\y : B => y) ((\\T : @ => T) A);"
	};
	for (size_t i = 0; i < sizeof(dependent_computations) / sizeof(*dependent_computations); ++i) {
		const struct pg_evidence *result = complete(&synthesis,
			request(&synthesis, scope, dependent_computations[i]), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_classifier(result), pg_evidence_classifier(dependent_block)) == 1);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(result)->core);
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		const struct pg_term *function_result = pg_eval_readback(&machine, &graph);
		pg_eval_destroy(&machine);
		assert(function_result && function_result->kind == PG_LAMBDA);
		pg_computation_eval_init(&machine, &graph, pg_application(&graph, function_result, pg_reference(&graph, x)));
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
	}
	complete(&synthesis, request(&synthesis, scope,
		"main := \\f : A -> @ => { B := f x; \\y : B => y; };"), PG_SYNTHESIS_UNSUPPORTED);
	const char *modules[] = {
		"{{ main := id x; id := \\y:A=>y; }}.main",
		"{{ main :: A -> A; main := id; id := \\y:A=>y; }}.main",
		"{{ id := \\y:Alias=>y; Alias:=A; main:=id x; }}.main",
		"{{ main:=alias x; alias:=id; id:=\\y:A=>y; }}.main"
	};
	for (size_t i = 0; i < sizeof(modules) / sizeof(*modules); ++i) {
		const struct pg_evidence *result = complete(&synthesis, program(&synthesis, scope, modules[i]), PG_SYNTHESIS_DONE);
		assert(result && pg_evidence_judgement(result) == PG_JUDGEMENT_VALUE);
		const struct pg_evidence *run = pg_prove_force(&typing, result);
		if (i == 1) run = pg_prove_application(&typing, run, pg_prove_variable(&typing, x_context, x));
		assert(run);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(run)->core);
		assert(pg_eval_advance(&machine, 500) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
	}
	complete(&synthesis, program(&synthesis, scope, "id:=\\y:A=>y; id::A->A;"), PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *library = program(&synthesis, scope, "left:=id; right:=id; id:=\\y:A=>y;");
	struct pg_token left_name = {.text="left", .length=4}, right_name = {.text="right", .length=5};
	size_t before_indexing = synthesis.jobs.count, before_terms = graph.terms.count;
	pg_synthesis_advance(&synthesis, 1);
	assert(synthesis.jobs.count == before_indexing + 1 && graph.terms.count == before_terms);
	assert(pg_synthesis_definition(library, left_name));
	assert(!pg_synthesis_definition(library, right_name));
	assert(pg_synthesis_status(pg_synthesis_definition(library, left_name)) == PG_SYNTHESIS_PENDING);
	assert(!complete(&synthesis, library, PG_SYNTHESIS_DONE));
	struct pg_synthesis_job *left_alias = pg_synthesis_definition(library, left_name);
	struct pg_synthesis_job *right_alias = pg_synthesis_definition(library, right_name);
	assert(left_alias && right_alias && pg_synthesis_status(left_alias) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(left_alias) == pg_synthesis_result(right_alias));
	assert(!pg_synthesis_definition(library, x_name));
	const struct pg_syntax shared_body = {.kind=PG_SYNTAX_ATOM, .token=x_name};
	const struct pg_syntax_item shared_entries[] = {
		{.name=left_name, .expression=&shared_body, .operation=PG_TOKEN_ASSIGN},
		{.name=right_name, .expression=&shared_body, .operation=PG_TOKEN_ASSIGN}
	};
	const struct pg_syntax shared_root = {.kind=PG_SYNTAX_DEFINITIONS, .items=shared_entries, .item_count=2};
	struct pg_synthesis_job *shared = pg_synthesis_request(&synthesis, scope, &shared_root);
	assert(!complete(&synthesis, shared, PG_SYNTHESIS_DONE));
	assert(pg_synthesis_definition(shared, left_name) == pg_synthesis_definition(shared, right_name));
	complete(&synthesis, program(&synthesis, scope, ""), PG_SYNTHESIS_DONE);
	const char *invalid_modules[] = {
		"{{main:=x; main:=A;}}.main", "{{main:=x;}}.missing",
		"{{main:=x; other:=missing;}}.main", "{{main:=x; main::@;}}.main",
		"missing::A;", "{{main::A; main:=missing;}}.main"
	};
	for (size_t i = 0; i < sizeof(invalid_modules) / sizeof(*invalid_modules); ++i)
		complete(&synthesis, program(&synthesis, scope, invalid_modules[i]), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope, "import Library;"), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *cycle = program(&synthesis, scope, "{{main:=other; other:=main;}}.main");
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(cycle) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(cycle));
	assert(!synthesis.ready);
	const struct pg_synthesis_job *member = pg_synthesis_cycle(cycle);
	assert(member && pg_synthesis_dependency(member));
	const struct pg_synthesis_job *cursor = member;
	unsigned cycle_length = 0;
	do {
		assert(pg_synthesis_status(cursor) == PG_SYNTHESIS_PENDING);
		assert(!pg_synthesis_result(cursor));
		cursor = pg_synthesis_dependency(cursor);
		assert(cursor && ++cycle_length < 20);
	} while (cursor != member);
	uint64_t stopped_steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == stopped_steps && pg_synthesis_cycle(cycle) == member);
	struct pg_synthesis strict;
	assert(pg_synthesis_init(&strict, &typing, &classifiers, &beta, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *strict_root = pg_synthesis_root(&strict);
	const struct pg_source_scope *strict_a = pg_synthesis_bind(&strict, strict_root, a_name, a, a_context);
	const struct pg_source_scope *strict_scope = pg_synthesis_bind(&strict, strict_a, x_name, x, x_context);
	complete(&strict, program(&strict, strict_scope, "{{main:={x;};}}.main"), PG_SYNTHESIS_REJECTED);
	complete(&strict, program(&strict, strict_scope, "{{main:=&{x;};}}.main"), PG_SYNTHESIS_DONE);
	complete(&strict, program(&strict, strict_scope, "{{main:=x;}}.main"), PG_SYNTHESIS_DONE);
	pg_synthesis_destroy(&strict);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 100);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&beta);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("source synthesis: pending jobs, raw nested lambdas, application, quotation and post-synthesis expectation passed");
}
