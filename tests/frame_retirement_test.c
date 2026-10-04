#include "eval_internal.h"

#include <assert.h>
#include <stdio.h>

struct callback_state {
	const struct pg_term *caller, *values[3];
	const struct pg_eval_continuation *continuation;
	struct pg_eval_frame *frame;
	unsigned calls;
	int stateless;
};

static struct callback_state *active_state;

static int continue_callback(struct pg_eval *machine, const struct pg_term *answer,
	struct callback_state *state)
{
	assert(state->calls < 3 && answer == state->values[state->calls]);
	assert(machine->current.term == state->caller && !machine->frames);
	struct pg_eval_frame *borrowed = state->frame;
	const void *callback_state = state->stateless ? NULL : state;
	assert(borrowed->caller.term == state->caller && borrowed->state == callback_state);
	if (++state->calls < 3) {
		assert(!pg_eval_demand_closure(machine,
			(struct pg_closure){state->values[state->calls], NULL},
			state->continuation, callback_state));
		/* A reentrant callback still borrows its old frame until return. */
		assert(machine->frames != borrowed);
		assert(borrowed->caller.term == state->caller && borrowed->state == callback_state);
		state->frame = machine->frames;
		return 0;
	}
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 0);
}

static int callback_answer(struct pg_eval *machine, const struct pg_term *answer,
	const void *state)
{
	return continue_callback(machine, answer, state ? (void *)state : active_state);
}

static int callback_head(struct pg_eval *machine, struct pg_closure head,
	const struct pg_argument *arguments, const void *state)
{
	assert(!head.environment && !arguments);
	return continue_callback(machine, head.term, state ? (void *)state : active_state);
}

static const struct pg_eval_continuation answer_callback = {
	"performance/test_reentrant_answer/v1", callback_answer
};
static const struct pg_eval_head_continuation head_callback = {
	{"performance/test_reentrant_head/v1", pg_eval_head_marker},
	callback_head, &answer_callback
};

static void reentrant_case(int captured, int stateless)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	struct callback_state state = {
		.caller = pg_reference(&graph, pg_binder(&graph)),
		.continuation = captured ? &head_callback.continuation : &answer_callback,
		.stateless = stateless
	};
	active_state = &state;
	for (size_t i = 0; i < 3; ++i)
		state.values[i] = pg_reference(&graph, pg_binder(&graph));
	struct pg_eval machine;
	pg_eval_init(&machine, state.caller);
	machine.output = &graph;
	assert(!pg_eval_demand_closure(&machine,
		(struct pg_closure){state.values[0], NULL}, state.continuation,
		stateless ? NULL : &state));
	state.frame = machine.frames;
	while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING)
		assert(machine.steps < 100);
	assert(machine.status == PG_EVAL_WHNF && state.calls == 3);
	assert(pg_eval_readback(&machine, &graph) == state.values[2]);
	printf("reentrant callback captured=%d stateless=%d: calls=%u steps=%llu\n",
		captured, stateless, state.calls, (unsigned long long)machine.steps);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
	active_state = NULL;
}

static struct {
	const struct pg_term *caller, *value;
	unsigned calls;
} escaped;
static const struct pg_eval_continuation escaped_callback;
static const struct pg_eval_head_continuation escaped_head;
static int escaped_captured;

static int escaped_answer(struct pg_eval *machine, const struct pg_term *answer,
	const void *state)
{
	const struct pg_closure *borrowed = state;
	assert(borrowed->term == escaped.caller && !borrowed->environment);
	assert(answer == escaped.value && machine->current.term == escaped.caller);
	if (++escaped.calls == 1) {
		/* The next demand borrows inline state from the old frame. */
		assert(!pg_eval_demand_closure(machine,
			(struct pg_closure){escaped.value, NULL},
			escaped_captured ? &escaped_head.continuation : &escaped_callback, borrowed));
		assert(borrowed->term == escaped.caller);
		return 0;
	}
	return pg_eval_enter(machine, (struct pg_closure){answer, NULL}, 0);
}

static const struct pg_eval_continuation escaped_callback = {
	"performance/test_escaped_frame_state/v1", escaped_answer
};

static int escaped_head_answer(struct pg_eval *machine, struct pg_closure head,
	const struct pg_argument *arguments, const void *state)
{
	assert(!head.environment && !arguments);
	return escaped_answer(machine, head.term, state);
}

static const struct pg_eval_head_continuation escaped_head = {
	{"performance/test_escaped_frame_head/v1", pg_eval_head_marker},
	escaped_head_answer, &escaped_callback
};

static void state_from_frame(int captured)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	escaped.caller = pg_reference(&graph, pg_binder(&graph));
	escaped.value = pg_reference(&graph, pg_binder(&graph));
	escaped.calls = 0;
	escaped_captured = captured;
	struct pg_eval machine;
	pg_eval_init(&machine, escaped.caller);
	machine.output = &graph;
	assert(!pg_eval_demand_closure(&machine,
		(struct pg_closure){escaped.value, NULL},
		captured ? &escaped_head.continuation : &escaped_callback, NULL));
	machine.frames->state = &machine.frames->caller;
	while (pg_eval_advance(&machine, 1) == PG_EVAL_PENDING)
		assert(machine.steps < 100);
	assert(machine.status == PG_EVAL_WHNF && escaped.calls == 2);
	assert(pg_eval_readback(&machine, &graph) == escaped.value);
	printf("inline frame-state captured=%d: calls=%u steps=%llu\n",
		captured, escaped.calls, (unsigned long long)machine.steps);
	pg_eval_destroy(&machine);
	pg_graph_destroy(&graph);
}

int main(void)
{
	reentrant_case(0, 0);
	reentrant_case(1, 0);
	reentrant_case(0, 1);
	reentrant_case(1, 1);
	state_from_frame(0);
	state_from_frame(1);
	return 0;
}
