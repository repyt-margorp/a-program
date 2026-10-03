#include "computation.h"
#include "iadt.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static void check_splits(struct pg_graph *graph, const struct pg_term *input,
	const struct pg_term *expected)
{
	struct pg_eval whole;
	pg_computation_eval_init(&whole, graph, input);
	assert(pg_eval_advance(&whole, 100000) == PG_EVAL_WHNF);
	uint64_t steps = whole.steps;
	assert(pg_alpha_equal(pg_eval_readback(&whole, graph), expected) == 1);
	pg_eval_destroy(&whole);
	for (uint64_t cut = 0; cut <= steps; ++cut) {
		struct pg_eval split;
		pg_computation_eval_init(&split, graph, input);
		pg_eval_advance(&split, cut);
		assert(pg_eval_readback(&split, graph));
		while (pg_eval_advance(&split, 1) == PG_EVAL_PENDING)
			assert(split.steps < steps);
		assert(split.status == PG_EVAL_WHNF && split.steps == steps);
		assert(pg_alpha_equal(pg_eval_readback(&split, graph), expected) == 1);
		pg_eval_destroy(&split);
	}
	printf("direct case: %" PRIu64 " raw transitions, all cuts agree\n", steps);
}

int main(void)
{
	struct pg_graph graph = {0};
	assert(!pg_graph_init(&graph));
	struct pg_object_class pair_class = {"test-pair"};
	struct pg_object pair = {PG_SEMANTIC_OBJECT, &pair_class};
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_object *z = pg_binder(&graph), *a = pg_binder(&graph);
	const struct pg_object *b = pg_binder(&graph), *unused = pg_binder(&graph);
	const struct pg_term *vx = pg_reference(&graph, x), *vy = pg_reference(&graph, y);
	const struct pg_term *va = pg_reference(&graph, a), *vb = pg_reference(&graph, b);
	const struct pg_term *self = pg_lambda(&graph, z,
		pg_application(&graph, pg_reference(&graph, z), pg_reference(&graph, z)));
	const struct pg_term *omega = pg_application(&graph, self, self);
	size_t arities[] = {0, 2};
	const struct pg_data_layout *layout = pg_data_layout(&graph, 2, arities);
	const struct pg_object *leaf = pg_data_constructor(layout, 0);
	const struct pg_object *node = pg_data_constructor(layout, 1);
	const struct pg_term *scrutinee = pg_application(&graph,
		pg_application(&graph, pg_reference(&graph, node), vx), omega);
	scrutinee = pg_application(&graph, pg_lambda(&graph, x, scrutinee), vb);
	const struct pg_term *body = pg_application(&graph,
		pg_application(&graph, pg_reference(&graph, &pair), vx), vy);
	const struct pg_term *branch = pg_lambda(&graph, y, pg_lambda(&graph, unused, body));
	struct pg_match_clause clauses[] = {{leaf, omega}, {node, branch}};
	const struct pg_term *input = pg_data_match(&graph, layout, scrutinee, 2, clauses);
	input = pg_application(&graph, pg_lambda(&graph, x, input), va);
	const struct pg_term *expected = pg_application(&graph,
		pg_application(&graph, pg_reference(&graph, &pair), va), vb);
	check_splits(&graph, input, expected);
	/* A selected branch can return a function; trailing caller arguments survive. */
	clauses[1].branch = pg_lambda(&graph, y, pg_lambda(&graph, unused,
		pg_lambda(&graph, z, pg_reference(&graph, z))));
	input = pg_data_match(&graph, layout, scrutinee, 2, clauses);
	check_splits(&graph, pg_application(&graph, input, va), va);
	/* Zero-field selection must also preserve its captured branch environment. */
	clauses[0].branch = vx;
	input = pg_data_match(&graph, layout, pg_reference(&graph, leaf), 2, clauses);
	check_splits(&graph, pg_application(&graph, pg_lambda(&graph, x, input), vb), vb);
	/* A neutral scrutinee remains neutral; no branch or field is forced. */
	input = pg_data_match(&graph, layout, va, 2, clauses);
	check_splits(&graph, input, input);
	pg_graph_destroy(&graph);
}
