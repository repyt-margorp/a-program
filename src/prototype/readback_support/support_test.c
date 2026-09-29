#include "eval.h"
#include "eval_internal.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef PG_SUPPORT_CANDIDATE
#include "support.h"

static const struct pg_term *subset(struct pg_graph *graph, const struct pg_term *empty,
	const struct pg_object *const *binders, uint32_t mask)
{
	const struct pg_term *term = empty;
	for (unsigned i = 0; i < 32; ++i)
		if (mask & (UINT32_C(1) << i)) term = pg_application(graph, term, pg_reference(graph, binders[i]));
	return term;
}

static void support_sets(struct pg_graph *graph, const struct pg_term *empty)
{
	const struct pg_object *binders[32];
	for (unsigned i = 0; i < 32; ++i) binders[i] = pg_binder(graph);
	uint32_t state = 7;
	for (unsigned trial = 0; trial < 128; ++trial) {
		state = state * UINT32_C(1664525) + UINT32_C(1013904223);
		uint32_t left = state;
		state = state * UINT32_C(1664525) + UINT32_C(1013904223);
		uint32_t right = state, remaining = left | right;
		const struct pg_term *a = subset(graph, empty, binders, left);
		const struct pg_term *b = subset(graph, empty, binders, right);
		const struct pg_term *term = pg_application(graph, a, b);
		assert(term->support == pg_application(graph, b, a)->support);
		assert(term->support == subset(graph, empty, binders, remaining)->support);
		for (unsigned i = 0; i < 32; ++i) {
			unsigned index = (i * 13) % 32;
			term = pg_lambda(graph, binders[index], term);
			remaining &= ~(UINT32_C(1) << index);
			assert(term && pg_term_closed(term) == !remaining);
			assert(term->support == subset(graph, empty, binders, remaining)->support);
			for (unsigned k = 0; k < 32; ++k)
				assert(pg_support_contains(term, binders[k]) == !!(remaining & (UINT32_C(1) << k)));
		}
	}
}
#endif

int main(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph), *z = pg_binder(&graph);
	const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y), *vz = pg_reference(&graph, z);
	const struct pg_term *identity = pg_lambda(&graph, x, vx);
	const struct pg_term *other_identity = pg_lambda(&graph, y, vy);
	assert(identity != other_identity && pg_alpha_equal(identity, other_identity) == 1);
	const struct pg_term *dag = identity;
	for (size_t i = 0; i < 12; ++i)
		dag = pg_lambda(&graph, pg_binder(&graph), pg_application(&graph, dag, dag));
	assert(dag);
	struct pg_binding_value binding = {z, vx};
	struct pg_substitution_work work;
	assert(!pg_substitution_work_init(&work, &graph));
	size_t before = graph.terms.count;
	struct pg_substitution *request = pg_substitution_request(&work, dag, 1, &binding);
	assert(request && pg_substitution_input(request)->environment);
	assert(pg_substitution_request(&work, dag, 1, &binding) == request);
	while (pg_substitution_status(request) == PG_SUBSTITUTION_PENDING && pg_substitution_steps(request) < 1000000)
		pg_substitution_advance(request, 64);
	const struct pg_term *result = pg_substitution_result(request);
	assert(result && pg_alpha_equal(dag, result) == 1);
	printf("substitution: input=26 new-terms=%zu steps=%" PRIu64 " reused=%d\n",
		graph.terms.count - before, pg_substitution_steps(request), result == dag);
	assert(result != dag && result->as.lambda.binder != dag->as.lambda.binder);
	struct pg_environment environment = {z, {vx, NULL}, NULL};
	struct materialization materialization = {0};
	before = graph.terms.count;
	int status;
	do {
		status = pg_materialize_step(&materialization, &graph,
			(struct pg_closure){dag, &environment}, NULL);
	} while (!status && materialization.readback.steps < 1000000);
	assert(status == 1 && pg_alpha_equal(dag, materialization.partial) == 1);
	printf("materialization: input=26 new-terms=%zu steps=%" PRIu64 " reused=%d\n",
		graph.terms.count - before, materialization.readback.steps, materialization.partial == dag);
#ifdef PG_SUPPORT_CANDIDATE
	assert(materialization.partial == dag && graph.terms.count == before && materialization.readback.steps == 0);
	assert(pg_term_closed(dag) && !pg_term_closed(vx));
	assert(pg_term_closed(pg_lambda(&graph, x, pg_lambda(&graph, x, vx))));
	assert(!pg_term_closed(pg_lambda(&graph, x, pg_application(&graph, vx, vy))));
	struct pg_term unknown = {.kind = PG_REFERENCE, .as.reference = z};
	assert(!pg_term_closed(&unknown));
	const struct pg_term *unknown_lambda = pg_lambda(&graph, z, &unknown);
	assert(unknown_lambda && !pg_term_closed(unknown_lambda));
	assert(pg_support_contains(&unknown, z) == 1 && pg_support_contains(&unknown, x) == 0);
	assert(pg_support_contains(unknown_lambda, z) == -1);
	const struct pg_object *binders[65];
	const struct pg_term *many = identity;
	for (size_t i = 0; i < 65; ++i) {
		binders[i] = pg_binder(&graph);
		many = pg_application(&graph, many, pg_reference(&graph, binders[i]));
		assert(many && !pg_term_closed(many));
	}
	for (size_t i = 0; i < 65; ++i) many = pg_lambda(&graph, binders[i], many);
	assert(many && pg_term_closed(many));
	support_sets(&graph, identity);
#endif
	pg_materialize_destroy(&materialization);
	const struct pg_term *open = pg_lambda(&graph, y, vx);
	struct pg_binding_value capture = {x, vy};
	const struct pg_term *renamed = pg_term_substitute(&graph, open, 1, &capture);
	assert(renamed && pg_alpha_equal(renamed, pg_lambda(&graph, z, vy)) == 1);
	assert(pg_alpha_equal(renamed, other_identity) == 0);
	struct pg_binding_value shadow[] = {{x, vy}, {x, vz}};
	assert(pg_term_substitute(&graph, vx, 2, shadow) == vz);
	assert(pg_term_substitute(&graph, identity, 2, shadow) == identity ||
		pg_alpha_equal(pg_term_substitute(&graph, identity, 2, shadow), identity) == 1);
	pg_substitution_work_destroy(&work);
	pg_graph_destroy(&graph);
	puts("support/capture checks passed");
	return 0;
}
