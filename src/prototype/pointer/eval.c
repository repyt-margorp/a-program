#include "eval.h"

#include <string.h>
#include <stdlib.h>

struct pg_environment {
	const struct pg_object *binder;
	struct pg_closure value;
	const struct pg_environment *parent;
};
struct pg_argument {
	struct pg_closure value;
	const struct pg_argument *next;
};
struct pg_eval_frame {
	struct pg_closure caller;
	const struct pg_argument *arguments;
	size_t index;
	int (*resume)(struct pg_eval *machine, const struct pg_term *answer);
	const struct pg_eval_frame *parent;
};

static const struct pg_term *readback(struct pg_closure closure,
	const struct pg_argument *arguments, struct pg_graph *graph);

const struct pg_closure *pg_eval_argument(const struct pg_eval *machine, size_t index)
{
	const struct pg_argument *argument = machine->arguments;
	while (argument && index) { argument = argument->next; --index; }
	return argument ? &argument->value : NULL;
}

int pg_eval_enter(struct pg_eval *machine, struct pg_closure value, size_t consume)
{
	if (!value.term) return -1;
	const struct pg_argument *rest = machine->arguments;
	while (consume) {
		if (!rest) return -1;
		rest = rest->next;
		--consume;
	}
	machine->current = value;
	machine->arguments = rest;
	return 0;
}

int pg_eval_apply(struct pg_eval *machine, struct pg_closure function,
	struct pg_closure argument, size_t consume)
{
	if (!argument.term) return -1;
	struct pg_argument *pending = pg_alloc(&machine->temporary, sizeof(*pending));
	if (!pending) return -1;
	if (pg_eval_enter(machine, function, consume) != 0) return -1;
	*pending = (struct pg_argument){argument, machine->arguments};
	machine->arguments = pending;
	return 0;
}

int pg_eval_demand(struct pg_eval *machine, size_t index,
	int (*resume)(struct pg_eval *machine, const struct pg_term *answer))
{
	if (!machine->output || !resume) return -1;
	const struct pg_closure *argument = pg_eval_argument(machine, index);
	if (!argument) return -1;
	struct pg_eval_frame *frame = pg_alloc(&machine->temporary, sizeof(*frame));
	if (!frame) return -1;
	*frame = (struct pg_eval_frame){machine->current, machine->arguments, index, resume, machine->frames};
	machine->frames = frame;
	machine->current = *argument;
	machine->arguments = NULL;
	return 0;
}

static int resume_frame(struct pg_eval *machine)
{
	const struct pg_eval_frame *frame = machine->frames;
	const struct pg_term *answer = readback(machine->current, machine->arguments, machine->output);
	if (!answer) return -1;
	/* Rebuild only the argument-list prefix; the tail and all terms stay shared. */
	struct pg_argument *first = NULL, *last = NULL;
	const struct pg_argument *argument = frame->arguments;
	for (size_t i = 0; i <= frame->index; ++i) {
		struct pg_argument *copy = pg_alloc(&machine->temporary, sizeof(*copy));
		if (!copy) return -1;
		*copy = *argument;
		if (last) last->next = copy;
		else first = copy;
		last = copy;
		argument = argument->next;
	}
	last->value = (struct pg_closure){answer, NULL};
	machine->current = frame->caller;
	machine->arguments = first;
	machine->frames = frame->parent;
	machine->head_ready = 0;
	return frame->resume(machine, answer);
}

struct readback_entry {
	struct pg_index_entry index;
	struct pg_closure input;
	const struct pg_term *result;
	struct readback_entry *next, *left, *right;
	const struct pg_object *binder;
	unsigned stage;
	const struct pg_environment *cursor;
};

struct readback_context {
	struct pg_graph *output;
	struct pg_graph temporary;
	struct pg_index results;
	struct readback_entry *pending;
	uint64_t steps;
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
	if (machine->head_ready) return resume_frame(machine);
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
		if (!value) return machine->dispatch ? machine->dispatch(machine) : 1;
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
		if (result > 0) {
			if (machine->frames) machine->head_ready = 1;
			else machine->status = PG_EVAL_WHNF;
		}
		if (result < 0) machine->status = PG_EVAL_ERROR;
	}
	return machine->status;
}

/* Readback is substitution, not evaluation. Fresh binder references prevent
 * capture when closures from different lexical environments are combined. */
static struct readback_entry *reify_request(struct readback_context *context, struct pg_closure closure)
{
	if (!closure.term) return NULL;
	uint64_t hash = ((uintptr_t)closure.term * UINT64_C(1099511628211)) ^ (uintptr_t)closure.environment;
	for (struct pg_index_entry *candidate = pg_index_candidates(&context->results, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		struct readback_entry *entry = (struct readback_entry *)candidate;
		if (entry->input.term != closure.term) continue;
		if (entry->input.environment == closure.environment) return entry;
	}
	struct readback_entry *entry = pg_alloc(&context->temporary, sizeof(*entry));
	if (!entry) return NULL;
	memset(entry, 0, sizeof(*entry));
	entry->input = closure;
	entry->cursor = closure.environment;
	if (pg_index_insert(&context->results, &entry->index, hash) != 0) return NULL;
	if (!closure.environment) entry->result = closure.term;
	else {
		entry->next = context->pending;
		context->pending = entry;
	}
	return entry;
}

static int reify_advance(struct readback_context *context, uint64_t budget)
{
	while (context->pending && budget) {
		--budget;
		++context->steps;
		struct readback_entry *entry = context->pending;
		const struct pg_term *term = entry->input.term;
		const struct pg_environment *environment = entry->input.environment;
		if (!entry->stage) {
			struct pg_closure child;
			switch (term->kind) {
			case PG_REFERENCE: {
				if (!entry->cursor) { entry->result = term; break; }
				if (entry->cursor->binder != term->as.reference) {
					entry->cursor = entry->cursor->parent;
					continue;
				}
				child = entry->cursor->value;
				break;
			}
			case PG_APPLICATION:
				child = (struct pg_closure){term->as.application.function, environment};
				break;
			case PG_LAMBDA: {
				entry->binder = pg_binder(context->output);
				const struct pg_term *variable = pg_reference(context->output, entry->binder);
				struct pg_environment *extended = pg_alloc(&context->temporary, sizeof(*extended));
				if (!variable || !extended) return -1;
				*extended = (struct pg_environment){term->as.lambda.binder, {variable, NULL}, environment};
				child = (struct pg_closure){term->as.lambda.body, extended};
				break;
			}
			default: return -1;
			}
			entry->stage = 1;
			if (!entry->result) {
				entry->left = reify_request(context, child);
				if (!entry->left) return -1;
				continue;
			}
		}
		if (!entry->result) {
			if (!entry->left->result) return -1;
			switch (term->kind) {
			case PG_REFERENCE: entry->result = entry->left->result; break;
			case PG_LAMBDA:
				entry->result = pg_lambda(context->output, entry->binder, entry->left->result);
				break;
			case PG_APPLICATION:
				if (entry->stage == 1) {
					entry->stage = 2;
					entry->right = reify_request(context,
						(struct pg_closure){term->as.application.argument, environment});
					if (!entry->right) return -1;
					continue;
				}
				entry->result = pg_application(context->output, entry->left->result, entry->right->result);
				break;
			}
		}
		if (!entry->result) return -1;
		context->pending = entry->next;
	}
	return context->pending ? 0 : 1;
}

static const struct pg_term *reify(struct readback_context *context, struct pg_closure closure)
{
	struct readback_entry *root = reify_request(context, closure);
	if (!root) return NULL;
	int status;
	do { status = reify_advance(context, UINT64_MAX); } while (!status);
	if (status < 0) return NULL;
	return root->result;
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
	const struct pg_term *result = readback(machine->current, machine->arguments, graph);
	for (const struct pg_eval_frame *frame = machine->frames; frame; frame = frame->parent) {
		if (!result) return NULL;
		const struct pg_term *caller = readback(frame->caller, NULL, graph);
		size_t index = 0;
		for (const struct pg_argument *argument = frame->arguments; argument; argument = argument->next, ++index) {
			const struct pg_term *value = index == frame->index ? result : readback(argument->value, NULL, graph);
			caller = pg_application(graph, caller, value);
			if (!caller) return NULL;
		}
		result = caller;
	}
	return result;
}

struct pg_substitution_state {
	struct readback_context context;
	struct readback_entry *root;
	enum pg_substitution_status status;
};

int pg_substitution_init(struct pg_substitution *work, struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	work->state = NULL;
	if (!graph || !term) return -1;
	if (count && !bindings) return -1;
	if (count > SIZE_MAX / sizeof(struct pg_environment)) return -1;
	for (size_t i = 0; i < count; ++i) {
		if (!bindings[i].binder || !bindings[i].value) return -1;
		if (bindings[i].binder->kind != PG_BINDER) return -1;
	}
	struct pg_substitution_state *state = calloc(1, sizeof(*state));
	if (!state) return -1;
	work->state = state;
	state->context.output = graph;
	if (pg_index_init(&state->context.results) != 0) goto failure;
	struct pg_environment *environment = pg_alloc(&state->context.temporary, count * sizeof(*environment));
	if (count && !environment) goto failure;
	for (size_t i = 0; i < count; ++i) {
		environment[i] = (struct pg_environment){bindings[i].binder,
			{bindings[i].value, NULL}, i ? &environment[i - 1] : NULL};
	}
	state->root = reify_request(&state->context, (struct pg_closure){term, count ? &environment[count - 1] : NULL});
	if (!state->root) goto failure;
	state->status = state->root->result ? PG_SUBSTITUTION_DONE : PG_SUBSTITUTION_PENDING;
	return 0;
failure:
	pg_substitution_destroy(work);
	return -1;
}

void pg_substitution_destroy(struct pg_substitution *work)
{
	if (!work->state) return;
	pg_index_destroy(&work->state->context.results);
	pg_graph_destroy(&work->state->context.temporary);
	free(work->state);
	work->state = NULL;
}

enum pg_substitution_status pg_substitution_status(const struct pg_substitution *work)
{
	return work->state ? work->state->status : PG_SUBSTITUTION_ERROR;
}

enum pg_substitution_status pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	if (pg_substitution_status(work) != PG_SUBSTITUTION_PENDING) return pg_substitution_status(work);
	int status = reify_advance(&work->state->context, budget);
	if (status < 0) work->state->status = PG_SUBSTITUTION_ERROR;
	if (status > 0) work->state->status = PG_SUBSTITUTION_DONE;
	return work->state->status;
}

uint64_t pg_substitution_steps(const struct pg_substitution *work)
{
	return work->state ? work->state->context.steps : 0;
}

const struct pg_term *pg_substitution_result(const struct pg_substitution *work)
{
	return pg_substitution_status(work) == PG_SUBSTITUTION_DONE ? work->state->root->result : NULL;
}

const struct pg_term *pg_term_substitute(struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	struct pg_substitution work;
	if (pg_substitution_init(&work, graph, term, count, bindings) != 0) return NULL;
	while (pg_substitution_advance(&work, UINT64_MAX) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_term *result = pg_substitution_result(&work);
	pg_substitution_destroy(&work);
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
