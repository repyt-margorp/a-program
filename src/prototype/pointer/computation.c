#include "computation.h"
#include "identity.h"

static const struct pg_object_class return_class = {"return"};
static const struct pg_object_class thunk_class = {"thunk"};
static const struct pg_object_class force_class = {"force"};
static const struct pg_object_class fold_class = {"computation-fold"};
const struct pg_object pg_return_operation = {PG_SEMANTIC_OBJECT, &return_class};
const struct pg_object pg_thunk_operation = {PG_SEMANTIC_OBJECT, &thunk_class};
const struct pg_object pg_force_operation = {PG_SEMANTIC_OBJECT, &force_class};
const struct pg_object pg_fold_operation = {PG_SEMANTIC_OBJECT, &fold_class};

static int force_answer(struct pg_eval *machine, const struct pg_term *answer)
{
	const struct pg_term *source;
	if (pg_identity_action_view(answer, &source)) {
		/* Keep reflexivity outside a neutral observation, so pre-normalizing a
		 * callee cannot hide the diagonal application rule behind FORCE. */
		const struct pg_term *observed = pg_application(machine->output,
			pg_reference(machine->output, &pg_force_operation), source);
		return pg_eval_enter(machine, (struct pg_closure){pg_identity_action(machine->output, observed), NULL}, 1);
	}
	if (answer->kind != PG_APPLICATION) return 1;
	const struct pg_term *head = answer->as.application.function;
	if (head->kind != PG_REFERENCE) return 1;
	if (head->as.reference != &pg_thunk_operation) return 1;
	return pg_eval_enter(machine, (struct pg_closure){answer->as.application.argument, NULL}, 1);
}

static int fold_answer(struct pg_eval *machine, const struct pg_term *answer)
{
	if (answer->kind != PG_APPLICATION) return 1;
	const struct pg_term *head = answer->as.application.function;
	if (head->kind != PG_REFERENCE) return 1;
	if (head->as.reference != &pg_return_operation) return 1;
	struct pg_closure continuation = *pg_eval_argument(machine, 1);
	return pg_eval_apply(machine, continuation, (struct pg_closure){answer->as.application.argument, NULL}, 2);
}

static int dispatch(struct pg_eval *machine)
{
	const struct pg_object *operation = machine->current.term->as.reference;
	if (operation == &pg_force_operation) {
		if (!pg_eval_argument(machine, 0)) return 1;
		return pg_eval_demand(machine, 0, force_answer);
	}
	if (operation == &pg_fold_operation) {
		if (!pg_eval_argument(machine, 1)) return 1;
		return pg_eval_demand(machine, 0, fold_answer);
	}
	return pg_identity_dispatch(machine);
}

const struct pg_eval_policy pg_pure_policy = {dispatch};

void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term)
{
	pg_eval_init(machine, term);
	machine->output = output;
	machine->dispatch = pg_pure_policy.dispatch;
}
