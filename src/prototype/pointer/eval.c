#include "eval.h"

#include <string.h>

struct pg_environment {
	const struct pg_object *binder;
	struct pg_closure value;
	const struct pg_environment *parent;
};
struct pg_argument {
	struct pg_closure value;
	const struct pg_argument *next;
};

static const struct pg_closure *lookup(const struct pg_environment *environment,
	const struct pg_object *binder)
{
	for (; environment; environment = environment->parent) {
		if (environment->binder == binder) return &environment->value;
	}
	return NULL;
}

void pg_eval_init(struct pg_eval *machine, const struct pg_term *term)
{
	memset(machine, 0, sizeof(*machine));
	machine->current.term = term;
	machine->status = term ? PG_EVAL_PENDING : PG_EVAL_ERROR;
}

static int step(struct pg_eval *machine)
{
	const struct pg_term *term = machine->current.term;
	switch (term->kind) {
	case PG_APPLICATION: {
		struct pg_argument *argument = pg_alloc(&machine->temporary, sizeof(*argument));
		if (!argument) return -1;
		argument->value = (struct pg_closure){term->as.application.argument, machine->current.environment};
		argument->next = machine->arguments;
		machine->arguments = argument;
		machine->current.term = term->as.application.function;
		return 0;
	}
	case PG_LAMBDA: {
		if (!machine->arguments) return 1;
		struct pg_environment *environment = pg_alloc(&machine->temporary, sizeof(*environment));
		if (!environment) return -1;
		environment->binder = term->as.lambda.binder;
		environment->value = machine->arguments->value;
		environment->parent = machine->current.environment;
		machine->arguments = machine->arguments->next;
		machine->current = (struct pg_closure){term->as.lambda.body, environment};
		return 0;
	}
	case PG_REFERENCE: {
		const struct pg_closure *value = lookup(machine->current.environment, term->as.reference);
		if (!value) return 1;
		machine->current = *value;
		return 0;
	}
	}
	return -1;
}

enum pg_eval_status pg_eval_advance(struct pg_eval *machine, uint64_t budget)
{
	while (machine->status == PG_EVAL_PENDING && budget) {
		budget--;
		machine->steps++;
		int result = step(machine);
		if (result > 0) machine->status = PG_EVAL_WHNF;
		if (result < 0) machine->status = PG_EVAL_ERROR;
	}
	return machine->status;
}

/* Readback is substitution, not evaluation. Fresh binder references prevent
 * capture when closures from different lexical environments are combined. */
static const struct pg_term *reify(struct pg_graph *graph, struct pg_closure closure)
{
	const struct pg_term *term = closure.term;
	switch (term->kind) {
	case PG_REFERENCE: {
		const struct pg_closure *value = lookup(closure.environment, term->as.reference);
		return value ? reify(graph, *value) : term;
	}
	case PG_APPLICATION: {
		const struct pg_term *function = reify(graph,
			(struct pg_closure){term->as.application.function, closure.environment});
		if (!function) return NULL;
		const struct pg_term *argument = reify(graph,
			(struct pg_closure){term->as.application.argument, closure.environment});
		return pg_application(graph, function, argument);
	}
	case PG_LAMBDA: {
		const struct pg_object *binder = pg_binder(graph);
		const struct pg_term *variable = pg_reference(graph, binder);
		if (!variable) return NULL;
		struct pg_environment environment = {term->as.lambda.binder, {variable, NULL}, closure.environment};
		const struct pg_term *body = reify(graph, (struct pg_closure){term->as.lambda.body, &environment});
		return pg_lambda(graph, binder, body);
	}
	}
	return NULL;
}

const struct pg_term *pg_eval_readback(struct pg_eval *machine, struct pg_graph *graph)
{
	if (machine->status == PG_EVAL_ERROR) return NULL;
	const struct pg_term *result = reify(graph, machine->current);
	for (const struct pg_argument *argument = machine->arguments; argument; argument = argument->next) {
		if (!result) return NULL;
		result = pg_application(graph, result, reify(graph, argument->value));
	}
	return result;
}

void pg_eval_destroy(struct pg_eval *machine)
{
	pg_graph_destroy(&machine->temporary);
	memset(machine, 0, sizeof(*machine));
}
