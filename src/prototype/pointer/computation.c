#include "computation.h"

static const struct pg_object_class return_class = {"return"};
static const struct pg_object_class thunk_class = {"thunk"};
static const struct pg_object_class force_class = {"force"};
const struct pg_object pg_return_operation = {PG_SEMANTIC_OBJECT, &return_class};
const struct pg_object pg_thunk_operation = {PG_SEMANTIC_OBJECT, &thunk_class};
const struct pg_object pg_force_operation = {PG_SEMANTIC_OBJECT, &force_class};

static int force_answer(struct pg_eval *machine, const struct pg_term *answer)
{
	if (answer->kind != PG_APPLICATION) return 1;
	const struct pg_term *head = answer->as.application.function;
	if (head->kind != PG_REFERENCE) return 1;
	if (head->as.reference != &pg_thunk_operation) return 1;
	return pg_eval_enter(machine, (struct pg_closure){answer->as.application.argument, NULL}, 1);
}

static int dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &pg_force_operation) return 1;
	if (!pg_eval_argument(machine, 0)) return 1;
	return pg_eval_demand(machine, 0, force_answer);
}

void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term)
{
	pg_eval_init(machine, term);
	machine->output = output;
	machine->dispatch = dispatch;
}
