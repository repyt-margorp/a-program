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

struct readback_entry {
	struct pg_index_entry index;
	struct pg_closure input;
	const struct pg_term *result;
};

struct readback_context {
	struct pg_graph *output;
	struct pg_graph temporary;
	struct pg_index results;
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
static const struct pg_term *reify(struct readback_context *context, struct pg_closure closure);

static const struct pg_term *reify_node(struct readback_context *context, struct pg_closure closure)
{
	struct pg_graph *graph = context->output;
	const struct pg_term *term = closure.term;
	switch (term->kind) {
	case PG_REFERENCE: {
		const struct pg_closure *value = lookup(closure.environment, term->as.reference);
		return value ? reify(context, *value) : term;
	}
	case PG_APPLICATION: {
		const struct pg_term *function = reify(context,
			(struct pg_closure){term->as.application.function, closure.environment});
		if (!function) return NULL;
		const struct pg_term *argument = reify(context,
			(struct pg_closure){term->as.application.argument, closure.environment});
		return pg_application(graph, function, argument);
	}
	case PG_LAMBDA: {
		const struct pg_object *binder = pg_binder(graph);
		const struct pg_term *variable = pg_reference(graph, binder);
		if (!variable) return NULL;
		struct pg_environment *environment = pg_alloc(&context->temporary, sizeof(*environment));
		if (!environment) return NULL;
		*environment = (struct pg_environment){term->as.lambda.binder, {variable, NULL}, closure.environment};
		const struct pg_term *body = reify(context, (struct pg_closure){term->as.lambda.body, environment});
		return pg_lambda(graph, binder, body);
	}
	}
	return NULL;
}

static const struct pg_term *reify(struct readback_context *context, struct pg_closure closure)
{
	if (!closure.environment) return closure.term;
	uint64_t hash = ((uintptr_t)closure.term * UINT64_C(1099511628211)) ^ (uintptr_t)closure.environment;
	for (struct pg_index_entry *candidate = pg_index_candidates(&context->results, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct readback_entry *entry = (const struct readback_entry *)candidate;
		if (entry->input.term != closure.term) continue;
		if (entry->input.environment == closure.environment) return entry->result;
	}
	const struct pg_term *result = reify_node(context, closure);
	if (!result) return NULL;
	struct readback_entry *entry = pg_alloc(&context->temporary, sizeof(*entry));
	if (!entry) return NULL;
	entry->input = closure;
	entry->result = result;
	if (pg_index_insert(&context->results, &entry->index, hash) != 0) return NULL;
	return result;
}

static const struct pg_term *readback(struct pg_closure closure,
	const struct pg_argument *arguments, struct pg_graph *graph)
{
	struct readback_context context = {.output = graph};
	if (pg_index_init(&context.results) != 0) return NULL;
	const struct pg_term *result = reify(&context, closure);
	for (const struct pg_argument *argument = arguments; argument; argument = argument->next) {
		if (!result) break;
		result = pg_application(graph, result, reify(&context, argument->value));
	}
	pg_index_destroy(&context.results);
	pg_graph_destroy(&context.temporary);
	return result;
}

const struct pg_term *pg_eval_readback(struct pg_eval *machine, struct pg_graph *graph)
{
	if (machine->status == PG_EVAL_ERROR) return NULL;
	return readback(machine->current, machine->arguments, graph);
}

const struct pg_term *pg_term_substitute(struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	if (!term) return NULL;
	if (!count) return term;
	if (!bindings) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_environment)) return NULL;
	for (size_t i = 0; i < count; ++i) {
		if (!bindings[i].binder || !bindings[i].value) return NULL;
		if (bindings[i].binder->kind != PG_BINDER) return NULL;
	}
	struct pg_graph temporary = {0};
	struct pg_environment *environment = pg_alloc(&temporary, count * sizeof(*environment));
	if (!environment) return NULL;
	for (size_t i = 0; i < count; ++i) {
		environment[i] = (struct pg_environment){bindings[i].binder,
			{bindings[i].value, NULL}, i ? &environment[i - 1] : NULL};
	}
	const struct pg_term *result = readback((struct pg_closure){term, &environment[count - 1]}, NULL, graph);
	pg_graph_destroy(&temporary);
	return result;
}

void pg_eval_destroy(struct pg_eval *machine)
{
	pg_graph_destroy(&machine->temporary);
	memset(machine, 0, sizeof(*machine));
}

struct pg_beta_job {
	struct pg_index_entry index;
	struct pg_graph *graph;
	const struct pg_term *input;
	struct pg_eval machine;
	const struct pg_term *result;
};

int pg_beta_work_init(struct pg_beta_work *work, struct pg_graph *graph)
{
	memset(work, 0, sizeof(*work));
	work->graph = graph;
	return pg_index_init(&work->jobs);
}

void pg_beta_work_destroy(struct pg_beta_work *work)
{
	for (size_t i = 0; i < work->jobs.capacity; ++i) {
		for (struct pg_index_entry *entry = work->jobs.buckets[i]; entry; entry = entry->next) {
			pg_eval_destroy(&((struct pg_beta_job *)entry)->machine);
		}
	}
	pg_index_destroy(&work->jobs);
	pg_graph_destroy(&work->storage);
	memset(work, 0, sizeof(*work));
}

struct pg_beta_job *pg_beta_request(struct pg_beta_work *work, const struct pg_term *input)
{
	if (!input) return NULL;
	uint64_t hash = (uintptr_t)input * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_beta_job *job = (struct pg_beta_job *)entry;
		if (job->input == input) return job;
	}
	struct pg_beta_job *job = pg_alloc(&work->storage, sizeof(*job));
	if (!job) return NULL;
	job->graph = work->graph;
	job->input = input;
	job->result = NULL;
	pg_eval_init(&job->machine, input);
	if (pg_index_insert(&work->jobs, &job->index, hash) != 0) return NULL;
	return job;
}

enum pg_eval_status pg_beta_advance(struct pg_beta_job *job, uint64_t budget)
{
	if (job->machine.status != PG_EVAL_PENDING) return job->machine.status;
	if (pg_eval_advance(&job->machine, budget) != PG_EVAL_WHNF) return job->machine.status;
	job->result = pg_eval_readback(&job->machine, job->graph);
	if (!job->result) {
		job->machine.status = PG_EVAL_ERROR;
		return PG_EVAL_ERROR;
	}
	/* No closure is needed after the answer is materialized. */
	pg_graph_destroy(&job->machine.temporary);
	job->machine.current = (struct pg_closure){job->result, NULL};
	job->machine.arguments = NULL;
	return PG_EVAL_WHNF;
}

enum pg_eval_status pg_beta_status(const struct pg_beta_job *job)
{
	return job->machine.status;
}

uint64_t pg_beta_steps(const struct pg_beta_job *job)
{
	return job->machine.steps;
}

const struct pg_term *pg_beta_result(const struct pg_beta_job *job)
{
	return job->result;
}
