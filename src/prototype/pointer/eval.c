#include "eval_internal.h"

#include <string.h>
#include <stdlib.h>

static const struct pg_term *readback(struct pg_closure closure,
	const struct pg_argument *arguments, struct pg_graph *graph);

const struct pg_closure *pg_eval_next_argument(const struct pg_argument **cursor)
{
	const struct pg_argument *argument = *cursor;
	if (!argument) return NULL;
	*cursor = argument->next;
	return &argument->value;
}

static const struct pg_argument *argument_at(const struct pg_eval *machine, size_t index)
{
	const struct pg_argument *cursor = machine->arguments;
	while (index--) if (!pg_eval_next_argument(&cursor)) return NULL;
	return cursor;
}

const struct pg_closure *pg_eval_argument(const struct pg_eval *machine, size_t index)
{
	const struct pg_argument *argument = argument_at(machine, index);
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

static int demand(struct pg_eval *machine, struct pg_closure value, const struct pg_argument *target,
	int (*resume)(struct pg_eval *, const struct pg_term *, const void *), const void *state)
{
	if (!machine->output || !resume || !value.term) return -1;
	struct pg_eval_frame *frame = pg_alloc(&machine->temporary, sizeof(*frame));
	if (!frame) return -1;
	*frame = (struct pg_eval_frame){.caller = machine->current, .arguments = machine->arguments,
		.target = target, .resume = resume, .state = state, .parent = machine->frames, .cursor = machine->arguments};
	machine->frames = frame;
	machine->current = value;
	machine->arguments = NULL;
	return 0;
}

int pg_eval_demand(struct pg_eval *machine, size_t index,
	int (*resume)(struct pg_eval *, const struct pg_term *, const void *), const void *state)
{
	const struct pg_argument *argument = argument_at(machine, index);
	return argument ? demand(machine, argument->value, argument, resume, state) : -1;
}

int pg_eval_demand_closure(struct pg_eval *machine, struct pg_closure value,
	int (*resume)(struct pg_eval *, const struct pg_term *, const void *), const void *state)
{
	return demand(machine, value, NULL, resume, state);
}

static int resume_frame(struct pg_eval *machine)
{
	struct pg_eval_frame *frame = machine->frames;
	if (!frame->answer.done) {
		int status = pg_materialize_step(&frame->answer, machine->output, machine->current, machine->arguments);
		return status < 0 ? -1 : 0;
	}
	const struct pg_term *answer = frame->answer.partial;
	const struct pg_argument *arguments = frame->arguments;
	if (frame->target) {
		/* A suspended prefix is an unchanged copy from arguments to cursor.
		 * Only the final, frame-removing step replaces the demanded argument. */
		const struct pg_argument *source = frame->cursor;
		struct pg_argument *copy = pg_alloc(&machine->temporary, sizeof(*copy));
		if (!copy || !source) return -1;
		*copy = *source;
		if (frame->last) frame->last->next = copy;
		else frame->first = copy;
		frame->last = copy;
		frame->cursor = source->next;
		if (source != frame->target) return 0;
		copy->value = (struct pg_closure){answer, NULL};
		arguments = frame->first;
	}
	machine->current = frame->caller;
	machine->arguments = arguments;
	machine->frames = frame->parent;
	machine->head_ready = 0;
	pg_materialize_destroy(&frame->answer);
	return frame->resume(machine, answer, frame->state);
}

void pg_eval_init(struct pg_eval *machine, const struct pg_term *term)
{
	memset(machine, 0, sizeof(*machine));
	machine->current.term = term;
	machine->status = term ? PG_EVAL_PENDING : PG_EVAL_ERROR;
}

struct pg_eval_task {
	const struct pg_eval_work_operation *operation;
	void *state;
};

int pg_eval_defer(struct pg_eval *machine,
	const struct pg_eval_work_operation *operation, void *state)
{
	if (machine->status != PG_EVAL_PENDING) return -1;
	if (machine->task || !state || !operation) return -1;
	if (!operation->poll || !operation->resume || !operation->destroy) return -1;
	struct pg_eval_task *task = pg_alloc(&machine->temporary, sizeof(*task));
	if (!task) return -1;
	*task = (struct pg_eval_task){operation, state};
	machine->task = task;
	return 0;
}

static int task_step(struct pg_eval *machine)
{
	struct pg_eval_task *task = machine->task;
	int status = task->operation->poll(task->state);
	if (!status) return 0;
	machine->task = NULL;
	int result = status < 0 ? -1 : task->operation->resume(machine, task->state);
	task->operation->destroy(task->state);
	return result;
}

static int step(struct pg_eval *machine)
{
	if (machine->task) return task_step(machine);
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
		if (term->as.reference->kind == PG_SEMANTIC_OBJECT) machine->current.environment = NULL;
		const struct pg_environment *environment = machine->current.environment;
		if (!environment) return machine->dispatch ? machine->dispatch(machine) : 1;
		if (environment->binder == term->as.reference) machine->current = environment->value;
		else machine->current.environment = environment->parent;
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
static uint64_t reify_hash(struct pg_closure closure)
{
	return ((uintptr_t)closure.term * UINT64_C(1099511628211)) ^ (uintptr_t)closure.environment;
}

static struct readback_entry *reify_find(struct readback_context *context, struct pg_closure closure)
{
	uint64_t hash = reify_hash(closure);
	for (struct pg_index_entry *candidate = pg_index_candidates(&context->results, hash); candidate; candidate = candidate->next) {
		struct readback_entry *other = (struct readback_entry *)candidate;
		if (candidate->hash == hash && other->input.term == closure.term
			&& other->input.environment == closure.environment) return other;
	}
	return NULL;
}

int pg_readback_index(struct readback_context *context, struct readback_entry *entry)
{
	if (reify_find(context, entry->input)) return -1;
	return pg_index_insert(&context->results, &entry->index, reify_hash(entry->input));
}

static struct readback_entry *reify_request(struct readback_context *context, struct pg_closure closure)
{
	if (!closure.term) return NULL;
	/* Environments contain binder substitutions, never semantic references. */
	if (closure.term->kind == PG_REFERENCE && closure.term->as.reference->kind == PG_SEMANTIC_OBJECT)
		closure.environment = NULL;
	struct readback_entry *existing = reify_find(context, closure);
	if (existing) return existing;
	struct readback_entry *entry = pg_alloc(&context->temporary, sizeof(*entry));
	if (!entry) return NULL;
	memset(entry, 0, sizeof(*entry));
	entry->input = closure;
	entry->cursor = closure.environment;
	if (pg_index_insert(&context->results, &entry->index, reify_hash(closure)) != 0) return NULL;
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

int pg_materialize_step(struct materialization *work, struct pg_graph *graph,
	struct pg_closure closure, const struct pg_argument *arguments)
{
	if (work->done) return 1;
	if (!work->readback.output) {
		work->readback.output = graph;
		if (pg_index_init(&work->readback.results) != 0) return -1;
		work->entry = reify_request(&work->readback, closure);
		if (!work->entry) return -1;
		work->remaining = arguments;
		return 0;
	}
	if (work->readback.pending)
		return reify_advance(&work->readback, 1) < 0 ? -1 : 0;
	const struct pg_term *answer = work->entry->result;
	if (!answer) return -1;
	work->partial = work->partial ? pg_application(graph, work->partial, answer) : answer;
	if (!work->partial) return -1;
	if (work->remaining) {
		work->entry = reify_request(&work->readback, work->remaining->value);
		if (!work->entry) return -1;
		work->remaining = work->remaining->next;
		return 0;
	}
	work->done = 1;
	return 1;
}

void pg_readback_destroy(struct readback_context *context)
{
	pg_index_destroy(&context->results);
	pg_graph_destroy(&context->temporary);
	memset(context, 0, sizeof(*context));
}

void pg_materialize_destroy(struct materialization *work)
{
	pg_readback_destroy(&work->readback);
	memset(work, 0, sizeof(*work));
}

static const struct pg_term *readback(struct pg_closure closure,
	const struct pg_argument *arguments, struct pg_graph *graph)
{
	struct materialization work = {0};
	int status;
	do { status = pg_materialize_step(&work, graph, closure, arguments); } while (!status);
	const struct pg_term *result = status > 0 ? work.partial : NULL;
	pg_materialize_destroy(&work);
	return result;
}

const struct pg_term *pg_eval_readback(struct pg_eval *machine, struct pg_graph *graph)
{
	if (machine->status == PG_EVAL_ERROR) return NULL;
	const struct pg_term *result = readback(machine->current, machine->arguments, graph);
	for (const struct pg_eval_frame *frame = machine->frames; frame; frame = frame->parent) {
		if (!result) return NULL;
		const struct pg_term *caller = readback(frame->caller, NULL, graph);
		for (const struct pg_argument *argument = frame->arguments; argument; argument = argument->next) {
			const struct pg_term *value = argument == frame->target ? result : readback(argument->value, NULL, graph);
			caller = pg_application(graph, caller, value);
			if (!caller) return NULL;
		}
		result = caller;
	}
	return result;
}

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
	pg_readback_destroy(&work->state->context);
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

const struct pg_closure *pg_substitution_input(const struct pg_substitution *work)
{
	return work && work->state && work->state->root ? &work->state->root->input : NULL;
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
	if (machine->task) machine->task->operation->destroy(machine->task->state);
	for (struct pg_eval_frame *frame = machine->frames; frame; frame = frame->parent)
		pg_materialize_destroy(&frame->answer);
	pg_graph_destroy(&machine->temporary);
	memset(machine, 0, sizeof(*machine));
}

const struct pg_eval_policy pg_beta_policy = {NULL};

struct pg_reduction_certificate {
	const struct pg_term *source;
	const struct pg_term *target;
	const struct pg_eval_policy *policy;
	enum pg_reduction_kind kind;
	const struct pg_reduction_phase *phases;
	const struct pg_reduction_certificate *normality;
};

struct pg_whnf_job {
	struct pg_index_entry index;
	struct pg_whnf_work *work;
	const struct pg_term *input;
	const struct pg_eval_policy *policy;
	struct pg_eval machine;
	const struct pg_reduction_certificate *certificate;
	struct materialization output;
	enum pg_eval_status status;
	uint64_t steps;
};

struct pg_nf_job {
	struct pg_index_entry index;
	struct pg_whnf_work *work;
	const struct pg_eval_policy *policy;
	const struct pg_term *input, *body;
	const struct pg_reduction_certificate *certificate;
	const struct pg_reduction_phase *phases;
	struct pg_whnf_job *head;
	struct pg_nf_job *children[2];
	struct pg_nf_job **stack;
	size_t depth, capacity;
	uint64_t steps;
	enum pg_nf_status status;
	enum { NF_HEAD, NF_CHILDREN, NF_RECHECK } stage;
};

int pg_whnf_work_init(struct pg_whnf_work *work, struct pg_graph *graph)
{
	memset(work, 0, sizeof(*work));
	work->graph = graph;
	if (pg_index_init(&work->jobs) != 0) return -1;
	if (pg_index_init(&work->normal_forms) == 0) return 0;
	pg_index_destroy(&work->jobs);
	return -1;
}

void pg_whnf_work_destroy(struct pg_whnf_work *work)
{
	for (size_t i = 0; i < work->jobs.capacity; ++i) {
		for (struct pg_index_entry *entry = work->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_whnf_job *job = (struct pg_whnf_job *)entry;
			pg_materialize_destroy(&job->output);
			pg_eval_destroy(&job->machine);
		}
	}
	pg_index_destroy(&work->jobs);
	for (size_t i = 0; i < work->normal_forms.capacity; ++i)
		for (struct pg_index_entry *entry = work->normal_forms.buckets[i]; entry; entry = entry->next)
			free(((struct pg_nf_job *)entry)->stack);
	pg_index_destroy(&work->normal_forms);
	pg_graph_destroy(&work->storage);
	memset(work, 0, sizeof(*work));
}

struct pg_whnf_job *pg_whnf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input)
{
	if (!input || !policy) return NULL;
	uint64_t hash = ((uintptr_t)input ^ (uintptr_t)policy) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_whnf_job *job = (struct pg_whnf_job *)entry;
		if (job->input == input && job->policy == policy) return job;
	}
	struct pg_whnf_job *job = pg_alloc(&work->storage, sizeof(*job));
	if (!job) return NULL;
	memset(job, 0, sizeof(*job));
	job->work = work;
	job->input = input;
	job->policy = policy;
	pg_eval_init(&job->machine, input);
	job->machine.output = work->graph;
	job->machine.dispatch = policy->dispatch;
	if (pg_index_insert(&work->jobs, &job->index, hash) != 0) return NULL;
	return job;
}

static struct pg_reduction_certificate *reduction_certificate(struct pg_graph *graph,
	const struct pg_term *source, const struct pg_term *target, const struct pg_eval_policy *policy,
	enum pg_reduction_kind kind)
{
	struct pg_reduction_certificate *certificate = pg_alloc(graph, sizeof(*certificate));
	if (certificate) *certificate = (struct pg_reduction_certificate){.source = source,
		.target = target, .policy = policy, .kind = kind};
	return certificate;
}

static enum pg_eval_status whnf_step(struct pg_whnf_job *job)
{
	if (job->machine.status == PG_EVAL_PENDING) {
		if (pg_eval_advance(&job->machine, 1) == PG_EVAL_ERROR) return PG_EVAL_ERROR;
		return PG_EVAL_PENDING;
	}
	int status = pg_materialize_step(&job->output, job->work->graph, job->machine.current, job->machine.arguments);
	if (status < 0) return PG_EVAL_ERROR;
	if (!status) return PG_EVAL_PENDING;
	const struct pg_reduction_certificate *certificate = reduction_certificate(job->work->graph,
		job->input, job->output.partial, job->policy, PG_REDUCTION_WHNF);
	if (!certificate) return PG_EVAL_ERROR;
	/* Materialized WHNF is its own answer under this same immutable policy.
	 * Retain a separate reflexive receipt, never merge source and target terms. */
	struct pg_whnf_job *canonical = pg_whnf_request(job->work, job->policy, certificate->target);
	if (!canonical || canonical->status == PG_EVAL_ERROR) return PG_EVAL_ERROR;
	if (canonical != job && canonical->status == PG_EVAL_PENDING) {
		struct pg_reduction_certificate *reflexive = reduction_certificate(job->work->graph,
			certificate->target, certificate->target, job->policy, PG_REDUCTION_WHNF);
		if (!reflexive) return PG_EVAL_ERROR;
		reflexive->normality = certificate;
		pg_materialize_destroy(&canonical->output);
		pg_eval_destroy(&canonical->machine);
		canonical->certificate = reflexive;
		canonical->status = PG_EVAL_WHNF;
	}
	job->certificate = certificate;
	pg_materialize_destroy(&job->output);
	pg_graph_destroy(&job->machine.temporary);
	job->machine.current = (struct pg_closure){certificate->target, NULL};
	job->machine.arguments = NULL;
	return PG_EVAL_WHNF;
}

enum pg_eval_status pg_whnf_advance(struct pg_whnf_job *job, uint64_t budget)
{
	while (job->status == PG_EVAL_PENDING && budget) {
		--budget;
		++job->steps;
		job->status = whnf_step(job);
	}
	return job->status;
}

enum pg_eval_status pg_whnf_status(const struct pg_whnf_job *job)
{
	return job->status;
}

uint64_t pg_whnf_steps(const struct pg_whnf_job *job)
{
	return job->steps;
}

const struct pg_term *pg_whnf_result(const struct pg_whnf_job *job)
{
	return job->certificate ? job->certificate->target : NULL;
}

const struct pg_reduction_certificate *pg_whnf_certificate(const struct pg_whnf_job *job) { return job->certificate; }
const struct pg_term *pg_reduction_source(const struct pg_reduction_certificate *certificate) { return certificate->source; }
const struct pg_term *pg_reduction_target(const struct pg_reduction_certificate *certificate) { return certificate->target; }
const struct pg_eval_policy *pg_reduction_policy(const struct pg_reduction_certificate *certificate) { return certificate->policy; }
enum pg_reduction_kind pg_reduction_kind(const struct pg_reduction_certificate *certificate) { return certificate->kind; }
const struct pg_reduction_phase *pg_reduction_phases(const struct pg_reduction_certificate *certificate) { return certificate->phases; }
const struct pg_reduction_certificate *pg_reduction_normality(const struct pg_reduction_certificate *certificate) { return certificate->normality; }

struct pg_nf_job *pg_nf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input)
{
	if (!input || !policy) return NULL;
	uint64_t hash = ((uintptr_t)input ^ (uintptr_t)policy) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&work->normal_forms, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_nf_job *job = (struct pg_nf_job *)entry;
		if (job->input == input && job->policy == policy) return job;
	}
	struct pg_nf_job *job = pg_alloc(&work->storage, sizeof(*job));
	if (!job) return NULL;
	job->work = work;
	job->policy = policy;
	job->input = input;
	if (pg_index_insert(&work->normal_forms, &job->index, hash) != 0) return NULL;
	return job;
}

static void nf_complete(struct pg_nf_job *job, const struct pg_term *result)
{
	struct pg_reduction_certificate *certificate = reduction_certificate(job->work->graph,
		job->input, result, job->policy, PG_REDUCTION_NF);
	if (!certificate) { job->status = PG_NF_ERROR; return; }
	certificate->phases = job->phases;
	/* A computed normal form is also its own answer, without another walk. */
	struct pg_nf_job *canonical = pg_nf_request(job->work, job->policy, result);
	if (!canonical || canonical->status == PG_NF_ERROR) {
		job->status = PG_NF_ERROR;
		return;
	}
	if (canonical != job && canonical->status == PG_NF_PENDING) {
		struct pg_reduction_certificate *reflexive = reduction_certificate(job->work->graph,
			result, result, job->policy, PG_REDUCTION_NF);
		if (!reflexive) { job->status = PG_NF_ERROR; return; }
		reflexive->normality = certificate;
		canonical->certificate = reflexive;
		canonical->status = PG_NF_DONE;
	}
	job->certificate = certificate;
	job->status = PG_NF_DONE;
}

static int nf_phase(struct pg_nf_job *job, const struct pg_term *rebuilt, int children)
{
	struct pg_reduction_phase *phase = pg_alloc(job->work->graph, sizeof(*phase));
	if (!phase) return -1;
	*phase = (struct pg_reduction_phase){.previous = job->phases,
		.head = pg_whnf_certificate(job->head), .rebuilt = rebuilt};
	if (children)
		for (size_t i = 0; i < 2; ++i)
			if (job->children[i]) phase->children[i] = pg_nf_certificate(job->children[i]);
	job->phases = phase;
	return 0;
}

static struct pg_nf_job *nf_step(struct pg_nf_job *job)
{
	if (job->stage == NF_CHILDREN) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_nf_job *child = job->children[i];
			if (!child) continue;
			if (child->status == PG_NF_ERROR) goto failure;
			if (child->status == PG_NF_PENDING) return child;
		}
		struct pg_graph *graph = job->work->graph;
		job->body = job->body->kind == PG_LAMBDA
			? pg_lambda(graph, job->body->as.lambda.binder, pg_nf_result(job->children[0]))
			: pg_application(graph, pg_nf_result(job->children[0]), pg_nf_result(job->children[1]));
		if (!job->body || nf_phase(job, job->body, 1)) goto failure;
		job->head = pg_whnf_request(job->work, job->policy, job->body);
		if (!job->head) goto failure;
		job->stage = NF_RECHECK;
	} else {
		if (!job->head) job->head = pg_whnf_request(job->work, job->policy, job->input);
		if (!job->head) goto failure;
		if (pg_whnf_status(job->head) == PG_EVAL_PENDING) {
			pg_whnf_advance(job->head, 1);
			return NULL;
		}
		const struct pg_term *body = pg_whnf_result(job->head);
		if (!body) goto failure;
		if (job->stage == NF_RECHECK && body == job->body) {
			if (nf_phase(job, body, 0)) goto failure;
			nf_complete(job, body);
			return NULL;
		}
		job->body = body;
		if (body->kind == PG_REFERENCE) {
			if (nf_phase(job, body, 0)) goto failure;
			nf_complete(job, body);
			return NULL;
		}
		job->children[0] = pg_nf_request(job->work, job->policy,
			body->kind == PG_LAMBDA ? body->as.lambda.body : body->as.application.function);
		job->children[1] = body->kind == PG_APPLICATION
			? pg_nf_request(job->work, job->policy, body->as.application.argument) : NULL;
		if (!job->children[0]) goto failure;
		if (body->kind == PG_APPLICATION && !job->children[1]) goto failure;
		job->stage = NF_CHILDREN;
	}
	return NULL;
failure:
	job->status = PG_NF_ERROR;
	return NULL;
}

enum pg_nf_status pg_nf_advance(struct pg_nf_job *job, uint64_t budget)
{
	while (job->status == PG_NF_PENDING && budget) {
		--budget;
		++job->steps;
		struct pg_nf_job *current = job->depth ? job->stack[job->depth - 1] : job;
		if (current->status != PG_NF_PENDING) {
			--job->depth;
			continue;
		}
		struct pg_nf_job *dependency = nf_step(current);
		if (!dependency) continue;
		if (job->depth == job->capacity) {
			size_t capacity = job->capacity ? 2 * job->capacity : 16;
			if (capacity < job->capacity || capacity > SIZE_MAX / sizeof(*job->stack)) {
				job->status = PG_NF_ERROR;
				break;
			}
			void *stack = realloc(job->stack, capacity * sizeof(*job->stack));
			if (!stack) { job->status = PG_NF_ERROR; break; }
			job->stack = stack;
			job->capacity = capacity;
		}
		job->stack[job->depth++] = dependency;
	}
	return job->status;
}

enum pg_nf_status pg_nf_status(const struct pg_nf_job *job) { return job->status; }
const struct pg_term *pg_nf_result(const struct pg_nf_job *job) { return job->certificate ? job->certificate->target : NULL; }
const struct pg_reduction_certificate *pg_nf_certificate(const struct pg_nf_job *job) { return job->certificate; }
uint64_t pg_nf_steps(const struct pg_nf_job *job) { return job->steps; }
