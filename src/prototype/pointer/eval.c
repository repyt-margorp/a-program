#include "eval_internal.h"

#include <string.h>
#include <stdlib.h>

static const struct pg_term *readback(struct pg_closure closure,
	const struct pg_argument *arguments, struct pg_graph *graph);

const struct pg_eval_continuation *pg_eval_continuation_find(const char *name,
	size_t count, const struct pg_eval_continuation *const *entries)
{
	if (!name) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!strcmp(name, entries[i]->name)) return entries[i];
	return NULL;
}

const struct pg_eval_work_operation *pg_eval_work_find(const char *name,
	size_t count, const struct pg_eval_work_operation *const *entries)
{
	if (!name) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (entries[i]->name && !strcmp(name, entries[i]->name)) return entries[i];
	return NULL;
}

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
	const struct pg_eval_continuation *continuation, const void *state)
{
	if (!machine->output || !continuation || !continuation->resume || !value.term) return -1;
	struct pg_eval_frame *frame = pg_alloc(&machine->temporary, sizeof(*frame));
	if (!frame) return -1;
	*frame = (struct pg_eval_frame){.caller = machine->current, .arguments = machine->arguments,
		.target = target, .continuation = continuation, .state = state, .parent = machine->frames, .cursor = machine->arguments};
	machine->frames = frame;
	machine->current = value;
	machine->arguments = NULL;
	return 0;
}

int pg_eval_demand(struct pg_eval *machine, size_t index,
	const struct pg_eval_continuation *continuation, const void *state)
{
	const struct pg_argument *argument = argument_at(machine, index);
	return argument ? demand(machine, argument->value, argument, continuation, state) : -1;
}

int pg_eval_demand_closure(struct pg_eval *machine, struct pg_closure value,
	const struct pg_eval_continuation *continuation, const void *state)
{
	return demand(machine, value, NULL, continuation, state);
}

struct pg_argument *pg_eval_frame_copy_argument(struct pg_eval_frame *frame, struct pg_graph *arena)
{
	if (!frame->cursor) return NULL;
	struct pg_argument *copy = pg_alloc(arena, sizeof(*copy));
	if (!copy) return NULL;
	*copy = *frame->cursor;
	if (frame->last) frame->last->next = copy;
	else frame->first = copy;
	frame->last = copy;
	frame->cursor = frame->cursor->next;
	return copy;
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
		struct pg_argument *copy = pg_eval_frame_copy_argument(frame, &machine->temporary);
		if (!copy) return -1;
		if (source != frame->target) return 0;
		copy->value = (struct pg_closure){answer, NULL};
		arguments = frame->first;
	}
	machine->current = frame->caller;
	machine->arguments = arguments;
	machine->frames = frame->parent;
	machine->head_ready = 0;
	pg_materialize_destroy(&frame->answer);
	return frame->continuation->resume(machine, answer, frame->state);
}

void pg_eval_init(struct pg_eval *machine, const struct pg_term *term)
{
	memset(machine, 0, sizeof(*machine));
	machine->current.term = term;
	machine->status = term ? PG_EVAL_PENDING : PG_EVAL_ERROR;
}

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

static struct readback_entry *reify_request(struct readback_context *context, struct pg_closure closure,
	struct pg_graph *storage)
{
	if (!closure.term) return NULL;
	/* Environments contain binder substitutions, never semantic references. */
	if (closure.term->kind == PG_REFERENCE && closure.term->as.reference->kind == PG_SEMANTIC_OBJECT)
		closure.environment = NULL;
	struct readback_entry *existing = reify_find(context, closure);
	if (existing) return existing;
	struct readback_entry *entry = pg_alloc(storage ? storage : &context->temporary, sizeof(*entry));
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
				entry->left = reify_request(context, child, NULL);
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
						(struct pg_closure){term->as.application.argument, environment}, NULL);
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
		work->entry = reify_request(&work->readback, closure, NULL);
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
		work->entry = reify_request(&work->readback, work->remaining->value, NULL);
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

static int substitution_inputs(const struct pg_term *term, size_t *length,
	const struct pg_binding_value **images)
{
	size_t count = *length;
	const struct pg_binding_value *bindings = *images;
	if (!term) return -1;
	if (count && !bindings) return -1;
	if (count > SIZE_MAX / sizeof(struct pg_environment)) return -1;
	for (size_t i = 0; i < count; ++i) {
		if (!bindings[i].binder || !bindings[i].value) return -1;
		if (bindings[i].binder->kind != PG_BINDER) return -1;
	}
	if (term->kind == PG_REFERENCE && term->as.reference->kind == PG_SEMANTIC_OBJECT) count = 0;
	/* An identity prefix is the empty substitution. Keep later identities:
	 * they can shadow a preceding nonidentity image of the same binder. */
	while (count && bindings->value->kind == PG_REFERENCE &&
		bindings->value->as.reference == bindings->binder) {
		++bindings;
		--count;
	}
	*length = count;
	*images = bindings;
	return 0;
}

static int substitution_init(struct pg_substitution *work, struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings,
	struct pg_graph *input_storage)
{
	struct pg_substitution_state *state = calloc(1, sizeof(*state));
	if (!state) return -1;
	work->state = state;
	state->context.output = graph;
	state->input_storage = input_storage;
	if (pg_index_init(&state->context.results) != 0) goto failure;
	struct pg_environment *environment = count ? pg_alloc(input_storage ? input_storage : &state->context.temporary,
		count * sizeof(*environment)) : NULL;
	if (count && !environment) goto failure;
	for (size_t i = 0; i < count; ++i) {
		environment[i] = (struct pg_environment){bindings[i].binder,
			{bindings[i].value, NULL}, i ? &environment[i - 1] : NULL};
	}
	state->root = reify_request(&state->context,
		(struct pg_closure){term, count ? &environment[count - 1] : NULL}, input_storage);
	if (!state->root) goto failure;
	state->status = state->root->result ? PG_SUBSTITUTION_DONE : PG_SUBSTITUTION_PENDING;
	return 0;
failure:
	pg_substitution_destroy(work);
	return -1;
}

int pg_substitution_init(struct pg_substitution *work, struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	work->state = NULL;
	if (!graph || substitution_inputs(term, &count, &bindings)) return -1;
	return substitution_init(work, graph, term, count, bindings, NULL);
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

static void substitution_finish(struct pg_substitution_state *state)
{
	if (!state->input_storage) return;
	/* The root stays in its input owner; only traversal edges become obsolete. */
	*state->root = (struct readback_entry){.input = state->root->input, .result = state->root->result};
	struct pg_graph *graph = state->context.output;
	uint64_t steps = state->context.steps;
	pg_readback_destroy(&state->context);
	state->context.output = graph;
	state->context.steps = steps;
}

enum pg_substitution_status pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	if (pg_substitution_status(work) != PG_SUBSTITUTION_PENDING) return pg_substitution_status(work);
	int status = reify_advance(&work->state->context, budget);
	if (status < 0) work->state->status = PG_SUBSTITUTION_ERROR;
	if (status > 0) {
		work->state->status = PG_SUBSTITUTION_DONE;
		substitution_finish(work->state);
	}
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

struct substitution_request {
	struct pg_index_entry index;
	struct pg_substitution work;
	size_t count;
};

int pg_substitution_work_init(struct pg_substitution_work *work, struct pg_graph *graph)
{
	memset(work, 0, sizeof(*work));
	work->graph = graph;
	if (!graph) return -1;
	if (!pg_index_init(&work->jobs)) return 0;
	pg_index_destroy(&work->jobs);
	work->graph = NULL;
	return -1;
}

void pg_substitution_work_destroy(struct pg_substitution_work *work)
{
	for (size_t i = 0; i < work->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = work->jobs.buckets[i]; entry; entry = entry->next)
			pg_substitution_destroy(&((struct substitution_request *)entry)->work);
	pg_index_destroy(&work->jobs);
	pg_graph_destroy(&work->storage);
	memset(work, 0, sizeof(*work));
}

struct pg_substitution *pg_substitution_request(struct pg_substitution_work *work,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	if (!work || !work->graph || substitution_inputs(term, &count, &bindings)) return NULL;
	uint64_t hash = ((uintptr_t)term ^ count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) {
		hash = (hash ^ (uintptr_t)bindings[i].binder) * UINT64_C(1099511628211);
		hash = (hash ^ (uintptr_t)bindings[i].value) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *entry = pg_index_candidates(&work->jobs, hash); entry; entry = entry->next) {
		struct substitution_request *request = (struct substitution_request *)entry;
		const struct pg_closure *input = pg_substitution_input(&request->work);
		if (entry->hash != hash || input->term != term || request->count != count) continue;
		const struct pg_environment *image = input->environment;
		size_t i = count;
		while (i && image->binder == bindings[i - 1].binder && image->value.term == bindings[i - 1].value) {
			--i;
			image = image->parent;
		}
		if (!i) return &request->work;
	}
	struct substitution_request *request = pg_alloc(&work->storage, sizeof(*request));
	if (!request) return NULL;
	request->work.state = NULL;
	request->count = count;
	if (substitution_init(&request->work, work->graph, term, count, bindings, &work->storage)) return NULL;
	if (pg_substitution_status(&request->work) == PG_SUBSTITUTION_DONE) substitution_finish(request->work.state);
	if (pg_index_insert(&work->jobs, &request->index, hash)) {
		pg_substitution_destroy(&request->work);
		return NULL;
	}
	return &request->work;
}

const struct pg_term *pg_substitution_compute(struct pg_substitution_work *work,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings)
{
	struct pg_substitution *request = pg_substitution_request(work, term, count, bindings);
	if (!request) return NULL;
	while (pg_substitution_advance(request, UINT64_MAX) == PG_SUBSTITUTION_PENDING) {}
	return pg_substitution_result(request);
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

static struct pg_reduction_request *reduction_find(const struct pg_index *index,
	const struct pg_eval_policy *policy, const struct pg_term *input, uint64_t *hash)
{
	*hash = ((uintptr_t)input ^ (uintptr_t)policy) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(index, *hash); entry; entry = entry->next) {
		if (entry->hash != *hash) continue;
		struct pg_reduction_request *request = (struct pg_reduction_request *)entry;
		if (request->input == input && request->policy == policy) return request;
	}
	return NULL;
}

const struct pg_reduction_certificate *pg_reduction_find(const struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input, enum pg_reduction_kind kind)
{
	if (!work || !policy || !input) return NULL;
	uint64_t hash;
	switch (kind) {
	case PG_REDUCTION_WHNF: {
		const struct pg_whnf_job *job = (const void *)reduction_find(&work->jobs, policy, input, &hash);
		return job ? pg_whnf_certificate(job) : NULL;
	}
	case PG_REDUCTION_NF:
	case PG_REDUCTION_PREFIX: {
		const struct pg_nf_job *job = (const void *)reduction_find(&work->normal_forms, policy, input, &hash);
		return job ? (kind == PG_REDUCTION_PREFIX ? job->prefix : pg_nf_certificate(job)) : NULL;
	}
	}
	return NULL;
}

static void *reduction_request(struct pg_whnf_work *work, struct pg_index *index,
	const struct pg_eval_policy *policy, const struct pg_term *input, size_t size, int *created)
{
	*created = 0;
	if (!input || !policy) return NULL;
	uint64_t hash;
	struct pg_reduction_request *found = reduction_find(index, policy, input, &hash);
	if (found) return found;
	struct pg_reduction_request *request = pg_alloc(&work->storage, size);
	if (!request) return NULL;
	request->work = work;
	request->input = input;
	request->policy = policy;
	if (pg_index_insert(index, &request->index, hash)) return NULL;
	*created = 1;
	return request;
}

int pg_whnf_job_attach(struct pg_whnf_work *work, struct pg_whnf_job *job)
{
	if (!work || !job || job->request.work || !job->request.input || !job->request.policy) return -1;
	if (job->status != PG_EVAL_PENDING || job->certificate || job->machine.output != work->graph) return -1;
	uint64_t hash;
	if (reduction_find(&work->jobs, job->request.policy, job->request.input, &hash)) return -1;
	if (pg_index_insert(&work->jobs, &job->request.index, hash)) return -1;
	job->request.work = work;
	return 0;
}

struct pg_whnf_job *pg_whnf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input)
{
	int created;
	struct pg_whnf_job *job = reduction_request(work, &work->jobs, policy, input, sizeof(*job), &created);
	if (created) {
		pg_eval_init(&job->machine, input);
		job->machine.output = work->graph;
		job->machine.dispatch = policy->dispatch;
	}
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

const struct pg_reduction_certificate *pg_reduction_identity(struct pg_graph *graph,
	const struct pg_eval_policy *policy, const struct pg_term *term)
{
	return graph && policy && term
		? reduction_certificate(graph, term, term, policy, PG_REDUCTION_PREFIX) : NULL;
}

static enum pg_eval_status whnf_step(struct pg_whnf_job *job)
{
	if (job->machine.status == PG_EVAL_PENDING) {
		if (pg_eval_advance(&job->machine, 1) == PG_EVAL_ERROR) return PG_EVAL_ERROR;
		return PG_EVAL_PENDING;
	}
	int status = pg_materialize_step(&job->output, job->request.work->graph, job->machine.current, job->machine.arguments);
	if (status < 0) return PG_EVAL_ERROR;
	if (!status) return PG_EVAL_PENDING;
	const struct pg_reduction_certificate *certificate = reduction_certificate(job->request.work->graph,
		job->request.input, job->output.partial, job->request.policy, PG_REDUCTION_WHNF);
	if (!certificate) return PG_EVAL_ERROR;
	/* Materialized WHNF is its own answer under this same immutable policy.
	 * Retain a separate reflexive receipt, never merge source and target terms. */
	struct pg_whnf_job *canonical = pg_whnf_request(job->request.work, job->request.policy, certificate->target);
	if (!canonical || canonical->status == PG_EVAL_ERROR) return PG_EVAL_ERROR;
	if (canonical != job && canonical->status == PG_EVAL_PENDING) {
		struct pg_reduction_certificate *reflexive = reduction_certificate(job->request.work->graph,
			certificate->target, certificate->target, job->request.policy, PG_REDUCTION_WHNF);
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
	if (!job->request.work) return PG_EVAL_ERROR;
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
	int created;
	return reduction_request(work, &work->normal_forms, policy, input, sizeof(struct pg_nf_job), &created);
}

static void nf_finish(struct pg_nf_job *job, const struct pg_reduction_certificate *certificate)
{
	free(job->stack);
	job->stack = NULL;
	job->depth = job->capacity = 0;
	job->certificate = certificate;
	job->status = PG_NF_DONE;
}

static int nf_publish(struct pg_nf_job *job, const struct pg_reduction_certificate *certificate)
{
	if (job->status == PG_NF_ERROR) return -1;
	if (job->status == PG_NF_DONE) return 0;
	const struct pg_term *result = certificate->target;
	/* A computed normal form is also its own answer, without another walk. */
	struct pg_nf_job *canonical = pg_nf_request(job->request.work, job->request.policy, result);
	if (!canonical || canonical->status == PG_NF_ERROR) return -1;
	if (canonical != job && canonical->status == PG_NF_PENDING) {
		struct pg_reduction_certificate *reflexive = reduction_certificate(job->request.work->graph,
			result, result, job->request.policy, PG_REDUCTION_NF);
		if (!reflexive) return -1;
		reflexive->normality = certificate;
		nf_finish(canonical, reflexive);
	}
	nf_finish(job, certificate);
	return 0;
}

int pg_nf_remember(struct pg_whnf_work *work, const struct pg_reduction_certificate *certificate)
{
	if (!work || !certificate || certificate->kind != PG_REDUCTION_NF || !certificate->target) return -1;
	struct pg_nf_job *job = pg_nf_request(work, certificate->policy, certificate->source);
	return job ? nf_publish(job, certificate) : -1;
}

static void nf_complete(struct pg_nf_job *job, const struct pg_term *result)
{
	struct pg_reduction_certificate *certificate = reduction_certificate(job->request.work->graph,
		job->request.input, result, job->request.policy, PG_REDUCTION_NF);
	if (certificate) certificate->phases = job->phases;
	if (!certificate || nf_publish(job, certificate)) job->status = PG_NF_ERROR;
}

static int nf_premise(const struct pg_reduction_certificate *child, const struct pg_eval_policy *policy)
{
	return child && child->kind == PG_REDUCTION_NF && child->policy == policy && child->source && child->target;
}

const struct pg_term *pg_reduction_phase_rebuild(struct pg_graph *graph,
	const struct pg_reduction_phase *previous,
	const struct pg_reduction_certificate *head,
	const struct pg_reduction_certificate *left, const struct pg_reduction_certificate *right)
{
	if (!head || head->kind != PG_REDUCTION_WHNF || !head->policy || !head->source || !head->target) return NULL;
	if (previous) {
		if (!previous->head || previous->rebuilt != head->source) return NULL;
		if (previous->head->policy != head->policy) return NULL;
	}
	const struct pg_term *body = head->target;
	if (!left) return right ? NULL : body;
	if (!nf_premise(left, head->policy)) return NULL;
	switch (body->kind) {
	case PG_LAMBDA:
		if (right || left->source != body->as.lambda.body) return NULL;
		return pg_lambda(graph, body->as.lambda.binder, left->target);
	case PG_APPLICATION:
		if (!nf_premise(right, head->policy)) return NULL;
		if (left->source != body->as.application.function || right->source != body->as.application.argument) return NULL;
		return pg_application(graph, left->target, right->target);
	default:
		return NULL;
	}
}

const struct pg_reduction_certificate *pg_reduction_prefix(struct pg_graph *graph,
	const struct pg_reduction_certificate *head,
	const struct pg_reduction_certificate *left, const struct pg_reduction_certificate *right)
{
	if (!graph) return NULL;
	const struct pg_term *rebuilt = pg_reduction_phase_rebuild(graph, NULL, head, left, right);
	if (!rebuilt) return NULL;
	if (!left && rebuilt->kind != PG_REFERENCE) return NULL;
	struct pg_reduction_phase *phase = pg_alloc(graph, sizeof(*phase));
	struct pg_reduction_certificate *certificate = reduction_certificate(graph,
		head->source, rebuilt, head->policy, PG_REDUCTION_PREFIX);
	if (!phase || !certificate) return NULL;
	*phase = (struct pg_reduction_phase){.head = head, .children = {left, right}, .rebuilt = rebuilt};
	certificate->phases = phase;
	return certificate;
}

int pg_reduction_nf_terminal(const struct pg_reduction_phase *previous,
	const struct pg_reduction_certificate *head)
{
	if (!head || head->kind != PG_REDUCTION_WHNF || !head->target) return 0;
	if (head->target->kind == PG_REFERENCE) return 1;
	if (!previous || !previous->children[0]) return 0;
	return previous->rebuilt == head->source && head->source == head->target;
}

const struct pg_reduction_phase *pg_reduction_head_congruence(const struct pg_reduction_certificate *certificate)
{
	if (!certificate) return NULL;
	const struct pg_reduction_phase *last = certificate->phases;
	if (certificate->kind == PG_REDUCTION_PREFIX) {
		if (!last || last->previous || !last->children[0]) return NULL;
		if (last->head->source != certificate->source || last->rebuilt != certificate->target) return NULL;
		return last;
	}
	if (certificate->kind != PG_REDUCTION_NF) return NULL;
	if (!last || last->children[0] || last->children[1]) return NULL;
	const struct pg_reduction_phase *phase = last->previous;
	if (!phase || phase->previous || !phase->children[0]) return NULL;
	if (phase->head->source != certificate->source) return NULL;
	if (last->head->source != phase->rebuilt || last->head->target != phase->rebuilt) return NULL;
	return certificate->target == phase->rebuilt ? phase : NULL;
}

const struct pg_reduction_phase *pg_reduction_congruence(const struct pg_reduction_certificate *certificate)
{
	const struct pg_reduction_phase *phase = pg_reduction_head_congruence(certificate);
	return phase && phase->head->target == certificate->source ? phase : NULL;
}

static int nf_phase(struct pg_nf_job *job, int children)
{
	struct pg_reduction_phase result = {.previous = job->phases, .head = pg_whnf_certificate(job->head)};
	if (children)
		for (size_t i = 0; i < 2; ++i)
			if (job->children[i]) result.children[i] = pg_nf_certificate(job->children[i]);
	result.rebuilt = pg_reduction_phase_rebuild(job->request.work->graph, result.previous,
		result.head, result.children[0], result.children[1]);
	if (!result.rebuilt) return -1;
	struct pg_reduction_phase *phase = pg_alloc(job->request.work->graph, sizeof(*phase));
	if (!phase) return -1;
	*phase = result;
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
		if (nf_phase(job, 1)) goto failure;
		job->body = job->phases->rebuilt;
		job->head = pg_whnf_request(job->request.work, job->request.policy, job->body);
		if (!job->head) goto failure;
		job->stage = NF_RECHECK;
	} else {
		if (!job->head) job->head = pg_whnf_request(job->request.work, job->request.policy, job->request.input);
		if (!job->head) goto failure;
		if (pg_whnf_status(job->head) == PG_EVAL_PENDING) {
			pg_whnf_advance(job->head, 1);
			return NULL;
		}
		const struct pg_term *body = pg_whnf_result(job->head);
		if (!body) goto failure;
		if (pg_reduction_nf_terminal(job->phases, pg_whnf_certificate(job->head))) {
			if (nf_phase(job, 0)) goto failure;
			nf_complete(job, body);
			return NULL;
		}
		job->body = body;
		job->children[0] = pg_nf_request(job->request.work, job->request.policy,
			body->kind == PG_LAMBDA ? body->as.lambda.body : body->as.application.function);
		job->children[1] = body->kind == PG_APPLICATION
			? pg_nf_request(job->request.work, job->request.policy, body->as.application.argument) : NULL;
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
int pg_nf_prefix_certificate(struct pg_nf_job *job,
	const struct pg_reduction_certificate **certificate)
{
	if (!job || !certificate || job->status == PG_NF_ERROR) return -1;
	if (!job->prefix) {
		const struct pg_reduction_phase *phase = job->phases;
		if (!phase && job->certificate) phase = job->certificate->phases;
		while (phase && phase->previous) phase = phase->previous;
		if (phase) {
			job->prefix = pg_reduction_prefix(job->request.work->graph,
				phase->head, phase->children[0], phase->children[1]);
		} else if (job->certificate) {
			if (job->certificate->source != job->certificate->target) return -1;
			struct pg_reduction_certificate *prefix = reduction_certificate(job->request.work->graph,
				job->request.input, job->request.input, job->request.policy, PG_REDUCTION_PREFIX);
			if (prefix) prefix->normality = job->certificate;
			job->prefix = prefix;
		} else return 0;
		if (!job->prefix) return -1;
	}
	*certificate = job->prefix;
	return 1;
}
uint64_t pg_nf_steps(const struct pg_nf_job *job) { return job->steps; }
