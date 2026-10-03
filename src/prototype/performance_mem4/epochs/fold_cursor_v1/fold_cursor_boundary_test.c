#include "computation.h"

#include <assert.h>
#include <stdio.h>

static void check_error(struct pg_graph *graph, const struct pg_object *operation,
	int incomplete_handler)
{
	const struct pg_term *caller = pg_reference(graph, operation);
	const struct pg_term *value = pg_reference(graph, pg_binder(graph));
	struct pg_argument returned = {{value, NULL}, NULL};
	struct pg_argument continuation = {{incomplete_handler ? value : NULL, NULL}, NULL};
	struct pg_argument source = {{value, NULL}, &continuation};
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, caller);
	machine.arguments = &source;
	const struct pg_eval_continuation *descriptor =
		pg_computation_continuation_resolve("computation/fold_head/v1");
	assert(descriptor && descriptor->resume == pg_eval_head_marker);
	const struct pg_eval_head_continuation *head =
		(const struct pg_eval_head_continuation *)descriptor;
	assert(head->resume(&machine, (struct pg_closure){
		pg_reference(graph, &pg_return_operation), NULL}, &returned, NULL) == -1);
	assert(machine.current.term == caller && machine.arguments == &source);
	assert(machine.steps == 0 && !machine.frames && !machine.task);
	pg_eval_destroy(&machine);
}

int main(void)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	check_error(&graph, &pg_fold_operation, 0);
	const struct pg_term *value = pg_reference(&graph, pg_binder(&graph));
	struct pg_operation_clause clause = {&pg_request_operation, value};
	const struct pg_term *folded = pg_computation_fold(&graph, value, value, 1, &clause);
	assert(folded);
	const struct pg_term *head = folded;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	assert(head->kind == PG_REFERENCE);
	check_error(&graph, head->as.reference, 1);
	pg_graph_destroy(&graph);
	puts("Fold null-continuation and incomplete-handler error boundaries pass");
	return 0;
}
