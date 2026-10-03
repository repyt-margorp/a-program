#include "eval_internal.h"

#include <assert.h>
#include <stdio.h>

static struct {
	int behavior;
	unsigned head_calls, fallback_calls;
	const struct pg_term *caller, *value;
} work;

static int answer_head(struct pg_eval *machine, struct pg_closure value,
	const struct pg_argument *arguments, const void *state)
{
	assert(!state && machine->current.term == work.caller && !machine->frames);
	assert(value.term == work.value && !value.environment && !arguments);
	++work.head_calls;
	return work.behavior ? work.behavior : pg_eval_enter(machine, value, 0);
}

static int answer_fallback(struct pg_eval *machine, const struct pg_term *answer,
	const void *state)
{
	assert(!state && machine->current.term == work.caller && answer == work.value);
	++work.fallback_calls;
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 0);
}

/* Local controls do not register a continuation name with a portable codec. */
static const struct pg_eval_continuation fallback = {
	"performance/test_stateless_fallback/v1", answer_fallback
};
static const struct pg_eval_head_continuation head = {
	{"performance/test_stateless_cleanup/v1", pg_eval_head_marker},
	answer_head, &fallback
};

static void cleanup_case(int behavior)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	work.behavior = behavior;
	work.head_calls = work.fallback_calls = 0;
	work.caller = pg_reference(&graph, pg_binder(&graph));
	work.value = pg_reference(&graph, pg_binder(&graph));
	struct pg_eval machine;
	pg_eval_init(&machine, work.caller);
	machine.output = &graph;
	assert(!pg_eval_demand_closure(&machine, (struct pg_closure){work.value, NULL},
		&head.continuation, NULL));
	struct pg_eval_frame *frame = machine.frames;
	/* The inert decoder can give an otherwise empty frame private scratch. */
	assert(pg_alloc(&frame->answer.readback.temporary, 0));
	while (!work.head_calls) {
		pg_eval_advance(&machine, 1);
		assert(machine.steps < 100);
	}
	assert(work.head_calls == 1 && !work.fallback_calls);
	if (behavior == 2) {
		assert(machine.status == PG_EVAL_PENDING && machine.frames == frame);
		assert(frame->answer.readback.temporary.blocks);
		while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING)
			assert(machine.steps < 100);
		assert(work.fallback_calls == 1);
	} else {
		assert(!machine.frames && !frame->answer.readback.temporary.blocks);
		if (behavior) assert(machine.status == PG_EVAL_ERROR);
		else while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING)
			assert(machine.steps < 100);
	}
	assert(!frame->answer.readback.temporary.blocks);
	assert(!frame->answer.readback.results.buckets);
	if (!behavior || behavior == 2) {
		assert(machine.status == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == work.value);
	}
	printf("stateless head behavior=%d: fallback=%u steps=%llu\n",
		behavior, work.fallback_calls, (unsigned long long)machine.steps);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

int main(void)
{
	cleanup_case(0);
	cleanup_case(-1);
	cleanup_case(2);
	cleanup_case(3);
	return 0;
}
