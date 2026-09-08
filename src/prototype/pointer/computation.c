#include "computation.h"
#include "identity.h"
#include "iadt.h"
#include "symmetry.h"

static const struct pg_object_class return_class = {"return"};
static const struct pg_object_class thunk_class = {"thunk"};
static const struct pg_object_class force_class = {"force"};
static const struct pg_object_class fold_class = {"computation-fold"};
const struct pg_object pg_return_operation = {PG_SEMANTIC_OBJECT, &return_class};
const struct pg_object pg_thunk_operation = {PG_SEMANTIC_OBJECT, &thunk_class};
const struct pg_object pg_force_operation = {PG_SEMANTIC_OBJECT, &force_class};
const struct pg_object pg_fold_operation = {PG_SEMANTIC_OBJECT, &fold_class};

static const struct pg_term *unary_argument(const struct pg_term *term, const struct pg_object *operation)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = term->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != operation) return NULL;
	return term->as.application.argument;
}

static int return_continuation(const struct pg_term *term)
{
	if (term->kind != PG_LAMBDA) return 0;
	const struct pg_term *body = unary_argument(term->as.lambda.body, &pg_return_operation);
	return body && body->kind == PG_REFERENCE && body->as.reference == term->as.lambda.binder;
}

static const struct pg_term *fold_unit_source(const struct pg_term *term)
{
	if (term->kind != PG_APPLICATION) return NULL;
	const struct pg_term *body = unary_argument(term->as.application.function, &pg_fold_operation);
	return body && return_continuation(term->as.application.argument) ? body : NULL;
}

const struct pg_term *pg_computation_eta(struct pg_graph *graph, const struct pg_term *term)
{
	if (!term) return NULL;
	const struct pg_term *body = unary_argument(term, &pg_thunk_operation);
	if (!body) return fold_unit_source(term);
	const struct pg_term *value = unary_argument(body, &pg_force_operation);
	if (value) return value;
	const struct pg_term *source = fold_unit_source(body);
	return source ? pg_application(graph, term->as.application.function, source) : NULL;
}

static int force_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_term *source;
	if (pg_identity_action_view(answer, &source)) {
		/* Keep reflexivity outside a neutral observation, so pre-normalizing a
		 * callee cannot hide the diagonal application rule behind FORCE. */
		const struct pg_term *observed = pg_application(machine->output,
			pg_reference(machine->output, &pg_force_operation), source);
		return pg_eval_enter(machine, (struct pg_closure){pg_identity_action(machine->output, observed), NULL}, 1);
	}
	const struct pg_term *body = unary_argument(answer, &pg_thunk_operation);
	return body ? pg_eval_enter(machine, (struct pg_closure){body, NULL}, 1) : pg_identity_force(machine, answer);
}

static int fold_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_term *value = unary_argument(answer, &pg_return_operation);
	if (!value) return 1;
	struct pg_closure continuation = *pg_eval_argument(machine, 1);
	return pg_eval_apply(machine, continuation, (struct pg_closure){value, NULL}, 2);
}

static int dispatch(struct pg_eval *machine)
{
	const struct pg_object *operation = machine->current.term->as.reference;
	if (operation == &pg_thunk_operation) {
		const struct pg_closure *body = pg_eval_argument(machine, 0);
		if (!body) return 1;
		const struct pg_term *value = unary_argument(body->term, &pg_force_operation);
		if (value) return pg_eval_enter(machine, (struct pg_closure){value, body->environment}, 1);
		/* Strip only an administrative right unit under suspension, not an
		 * arbitrary computation, so THUNK(FOLD(FORCE(v), return)) contracts too. */
		value = pg_computation_eta(machine->output, body->term);
		if (!value) return 1;
		return pg_eval_apply(machine, machine->current, (struct pg_closure){value, body->environment}, 1);
	}
	if (operation == &pg_force_operation) {
		if (!pg_eval_argument(machine, 0)) return 1;
		return pg_eval_demand(machine, 0, force_answer, NULL);
	}
	if (operation == &pg_fold_operation) {
		const struct pg_closure *continuation = pg_eval_argument(machine, 1);
		if (!continuation) return 1;
		/* Recognize the right unit without evaluating a continuation that M
		 * might never invoke. The returned reference must be this lambda's binder. */
		if (return_continuation(continuation->term)) return pg_eval_enter(machine, *pg_eval_argument(machine, 0), 2);
		return pg_eval_demand(machine, 0, fold_answer, NULL);
	}
	int data = pg_data_dispatch(machine);
	if (data != 1) return data;
	int identity = pg_identity_dispatch(machine);
	return identity == 1 ? pg_symmetry_dispatch(machine) : identity;
}

const struct pg_eval_policy pg_pure_policy = {dispatch};

void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term)
{
	pg_eval_init(machine, term);
	machine->output = output;
	machine->dispatch = pg_pure_policy.dispatch;
}
