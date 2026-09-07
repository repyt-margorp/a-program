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
		assert(++steps < 1000);
		pg_synthesis_advance(synthesis, 1);
	}
	assert(pg_synthesis_status(job) == expected);
	return pg_synthesis_result(job);
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
	assert(pg_synthesis_init(&synthesis, &typing, &classifiers, &beta) == 0);
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
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, a_scope, x_name, x, x_context);
	const struct pg_evidence *application = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) x;"), PG_SYNTHESIS_DONE);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(application)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	const struct pg_term *expected = pg_application(&graph, pg_reference(&graph, &pg_return_operation), pg_reference(&graph, x));
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	const struct pg_evidence *checked = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) :: A -> A;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(checked) == PG_TYPE_CONVERSION);
	complete(&synthesis, request(&synthesis, scope, "main := (\\y : A => missing) :: A -> A;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := x :: @;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := &(\\y : A => y);"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, scope, "main := (&(\\y : A => y)) x;"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, scope, "main := (\\y : A => y) ((\\y : A => y) x);"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, request(&synthesis, root, "Nat := @{zero:*; succ:*->*;};"), PG_SYNTHESIS_UNSUPPORTED);
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
