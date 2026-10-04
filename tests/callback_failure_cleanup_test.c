#include "eval.h"

#include <assert.h>
#include <stdio.h>

static const struct pg_term *expected;

static int fail_after_cleanup(struct pg_eval *machine,
	const struct pg_term *answer, const void *state)
{
	assert(!state && answer == expected);
	/* A materialized failure has no further use for the machine arena. */
	pg_eval_destroy(machine);
	return -1;
}

static const struct pg_eval_continuation failure = {
	"performance/test_failure_cleanup/v1", fail_after_cleanup
};

int main(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_term *caller = pg_reference(&graph, pg_binder(&graph));
	expected = pg_reference(&graph, pg_binder(&graph));
	struct pg_eval machine;
	pg_eval_init(&machine, caller);
	machine.output = &graph;
	assert(!pg_eval_demand_closure(&machine, (struct pg_closure){expected, NULL},
		&failure, NULL));
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_ERROR);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
	puts("materialized failure cleanup: error returned safely");
	return 0;
}
