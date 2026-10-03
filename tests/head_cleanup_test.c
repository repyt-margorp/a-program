#include "eval_internal.h"

#include <assert.h>
#include <stdio.h>

struct cleanup_state {
	int behavior;
	unsigned head_calls, fallback_calls;
	const struct pg_term *caller, *value;
};

static int cleanup_head(struct pg_eval *machine, struct pg_closure head,
	const struct pg_argument *arguments, const void *state)
{
	struct cleanup_state *work = (void *)state;
	assert(machine->current.term == work->caller && !machine->frames);
	assert(head.term == work->value && !head.environment && !arguments);
	++work->head_calls;
	if (work->behavior) return work->behavior;
	return pg_eval_enter(machine, head, 0);
}

static int cleanup_fallback(struct pg_eval *machine, const struct pg_term *answer,
	const void *state)
{
	struct cleanup_state *work = (void *)state;
	assert(machine->current.term == work->caller && answer == work->value);
	++work->fallback_calls;
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 0);
}

/* This test-only descriptor never enters a codec or registers schema authority. */
static const struct pg_eval_continuation fallback = {"performance/test_fallback/v1", cleanup_fallback};
static const struct pg_eval_head_continuation head = {
	{"performance/test_head_cleanup/v1", pg_eval_head_marker}, cleanup_head, &fallback
};

static void cleanup_case(int behavior)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct cleanup_state work = {.behavior = behavior};
	work.caller = pg_reference(&graph, pg_binder(&graph));
	work.value = pg_reference(&graph, pg_binder(&graph));
	struct pg_eval machine;
	pg_eval_init(&machine, work.caller);
	machine.output = &graph;
	assert(!pg_eval_demand_closure(&machine, (struct pg_closure){work.value, NULL},
		&head.continuation, &work));
	struct pg_eval_frame *frame = machine.frames;
	assert(frame);
	/* Empty materialization decode still allocates its private zero-entry array. */
	assert(pg_alloc(&frame->answer.readback.temporary, 0));
	assert(!frame->answer.readback.output && frame->answer.readback.temporary.blocks);
	while (!work.head_calls) {
		pg_eval_advance(&machine, 1);
		assert(machine.steps < 100);
	}
	assert(work.head_calls == 1 && !work.fallback_calls);
	if (behavior == 2) {
		assert(machine.status == PG_EVAL_PENDING && machine.frames == frame);
		assert(frame->answer.readback.temporary.blocks);
		while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < 100);
		assert(work.fallback_calls == 1);
	} else {
		assert(!machine.frames && !frame->answer.readback.temporary.blocks);
		assert(!frame->answer.readback.results.buckets);
		if (behavior) assert(machine.status == PG_EVAL_ERROR);
		else while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING) assert(machine.steps < 100);
	}
	assert(!frame->answer.readback.temporary.blocks);
	if (behavior == 0 || behavior == 2) {
		assert(machine.status == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == work.value);
	}
	printf("head cleanup behavior=%d: scratch released; fallback=%u; steps=%llu\n",
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
