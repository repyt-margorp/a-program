#include "synthesis.h"
#include "computation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_beta_work beta;
	struct pg_synthesis synthesis;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	assert(pg_beta_work_init(&beta, &graph) == 0);
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
	const struct pg_evidence *typed_reduct = pg_reduce_beta(&typing, a_context, typed_application);
	assert(typed_reduct && pg_evidence_subject(typed_reduct)->core->kind == PG_LAMBDA);
	assert(pg_evidence_subject(typed_application)->core->kind == PG_APPLICATION);
	assert(pg_alpha_equal(pg_evidence_classifier(typed_application), pg_evidence_classifier(typed_reduct)) == 1);
	size_t reduction_terms = graph.terms.count, reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) assert(pg_reduce_beta(&typing, a_context, typed_application) == typed_reduct);
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	const struct pg_evidence *x_value = pg_prove_variable(&typing, x_context, x);
	const struct pg_evidence *second_application = pg_prove_application(&typing,
		pg_prove_projection(&typing, x_context, typed_reduct), x_value);
	const struct pg_evidence *second_reduct = pg_reduce_beta(&typing, x_context, second_application);
	const struct pg_evidence *return_x = pg_prove_return(&typing, &classifiers, x_value);
	assert(second_reduct && pg_evidence_subject(second_reduct)->core == pg_evidence_subject(return_x)->core);
	assert(pg_evidence_classifier(second_reduct) == pg_evidence_classifier(return_x));
	const struct pg_evidence *images[] = {pg_prove_variable(&typing, x_context, a), x_value};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, x_context, x_context, 2, images);
	const struct pg_evidence *twice_reindexed = pg_prove_reindex(&typing, sigma,
		pg_prove_projection(&typing, x_context, typed_reduct));
	const struct pg_evidence *third_application = pg_prove_application(&typing, twice_reindexed, x_value);
	const struct pg_evidence *third_reduct = pg_reduce_beta(&typing, x_context, third_application);
	assert(third_reduct && pg_evidence_subject(third_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *reindexed_application = pg_prove_reindex(&typing, sigma, second_application);
	const struct pg_evidence *reindexed_reduct = pg_reduce_computation(&typing, x_context, reindexed_application);
	assert(reindexed_reduct && pg_evidence_subject(reindexed_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *projected_application = pg_prove_projection(&typing, x_context, typed_application);
	const struct pg_evidence *projected_reduct = pg_reduce_computation(&typing, x_context, projected_application);
	assert(projected_reduct && pg_alpha_equal(pg_evidence_subject(projected_reduct)->core,
		pg_evidence_subject(typed_reduct)->core) == 1);
	reduction_terms = graph.terms.count;
	reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_reduce_beta(&typing, x_context, second_application) == second_reduct);
		assert(pg_reduce_beta(&typing, x_context, third_application) == third_reduct);
		assert(pg_reduce_computation(&typing, x_context, reindexed_application) == reindexed_reduct);
		assert(pg_reduce_computation(&typing, x_context, projected_application) == projected_reduct);
	}
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, a_scope, x_name, x, x_context);
	struct pg_synthesis_job *callee_step = pg_synthesis_reduce(&synthesis, x_context, projected_application);
	const struct pg_evidence *outer_application = pg_prove_application(&typing, projected_application, x_value);
	struct pg_synthesis_job *outer_step = pg_synthesis_reduce(&synthesis, x_context, outer_application);
	assert(callee_step && outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_synthesis_dependency(outer_step) == callee_step);
	assert(pg_synthesis_status(callee_step) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *other_application = pg_prove_application(&typing, projected_application,
		pg_prove_reindex(&typing, sigma, x_value));
	struct pg_synthesis_job *other_step = pg_synthesis_reduce(&synthesis, x_context, other_application);
	assert(other_step && other_step != outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_synthesis_dependency(other_step) == callee_step);
	assert(pg_synthesis_dependency(outer_step) == callee_step);
	const struct pg_evidence *outer_reduct = complete(&synthesis, outer_step, PG_SYNTHESIS_DONE);
	complete(&synthesis, other_step, PG_SYNTHESIS_DONE);
	assert(outer_reduct == pg_reduce_computation(&typing, x_context, outer_application));
	assert(pg_synthesis_status(callee_step) == PG_SYNTHESIS_DONE);
	uint64_t shared_steps = synthesis.steps;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_reduce(&synthesis, x_context, projected_application) == callee_step);
		assert(pg_synthesis_reduce(&synthesis, x_context, outer_application) == outer_step);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(synthesis.steps == shared_steps);
	assert(!pg_synthesis_reduce(&synthesis, a_context, outer_application));
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
	const struct pg_evidence *typed_steps[] = {second_application, third_application, reindexed_application, outer_application};
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
	pg_beta_work_destroy(&beta);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("source synthesis: pending jobs, raw nested lambdas, application, quotation and post-synthesis expectation passed");
}
