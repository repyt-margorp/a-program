#include "synthesis.h"

#include <string.h>

struct pg_source_scope {
	const struct pg_synthesis *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder;
	const struct pg_evidence *context;
	struct definition_state *definitions;
};
struct waiter {
	struct pg_synthesis_job *parent;
	struct pg_synthesis_job *child;
	struct waiter *next;
};
struct continuation_frame {
	const struct pg_evidence *input;
	const struct pg_evidence *value;
	const struct pg_evidence *domain;
	const struct pg_evidence *context;
	const struct continuation_frame *parent;
};
struct block_name {
	struct pg_index_entry index;
	struct pg_token name;
	struct pg_synthesis_job *producer;
};
struct definition_state {
	struct pg_index names;
	const struct pg_syntax *syntax;
	struct pg_source_scope *scope;
	struct pg_synthesis_job **entries;
	size_t count, indexed, activated, next;
	struct pg_synthesis_job *selected;
};
struct block_state {
	const struct pg_syntax *syntax;
	size_t next, end;
	const struct pg_source_scope *scope;
	const struct continuation_frame *frames;
	const struct pg_evidence *tail;
	struct pg_index names;
};
struct pg_synthesis_job {
	struct pg_index_entry index;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	enum pg_synthesis_status status;
	unsigned stage;
	struct pg_synthesis_job *next;
	struct waiter *waiters;
	struct waiter *dependency;
	struct pg_synthesis_job *left;
	struct pg_synthesis_job *right;
	const struct pg_source_scope *inner;
	const struct pg_evidence *domain;
	const struct pg_evidence *result;
	const struct pg_evidence *checking_term;
	const struct pg_evidence *checking_type;
	const struct pg_evidence *type_input;
	const struct pg_evidence *function;
	struct pg_conversion comparison;
	int comparing;
	struct block_state *block;
	const struct continuation_frame *application_frame;
	int definition;
	struct definition_state *definitions;
};

int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_beta_work *beta,
	enum pg_definition_policy definition_policy)
{
	memset(synthesis, 0, sizeof(*synthesis));
	if (classifiers->graph != typing->graph || beta->graph != typing->graph) return -1;
	if ((unsigned)definition_policy > PG_DEFINITION_EXPLICIT_THUNK) return -1;
	synthesis->typing = typing;
	synthesis->classifiers = classifiers;
	synthesis->beta = beta;
	synthesis->definition_policy = definition_policy;
	return pg_index_init(&synthesis->jobs);
}

void pg_synthesis_destroy(struct pg_synthesis *synthesis)
{
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
			pg_conversion_destroy(&job->comparison);
			if (job->block) pg_index_destroy(&job->block->names);
			if (job->definitions) pg_index_destroy(&job->definitions->names);
		}
	pg_index_destroy(&synthesis->jobs);
	memset(synthesis, 0, sizeof(*synthesis));
}

const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis)
{
	struct pg_source_scope *scope = pg_alloc(synthesis->typing->graph, sizeof(*scope));
	if (!scope) return NULL;
	scope->owner = synthesis;
	scope->context = pg_prove_empty_context(synthesis->typing);
	return scope->context ? scope : NULL;
}

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!parent || parent->owner != synthesis || !extended_context) return NULL;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return NULL;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!context || context->parent != pg_evidence_context(parent->context)) return NULL;
	if (context->binder != binder) return NULL;
	/* Check ownership through a primitive judgement, not just a context pointer. */
	if (!pg_prove_variable(synthesis->typing, extended_context, binder)) return NULL;
	struct pg_source_scope *scope = pg_alloc(synthesis->typing->graph, sizeof(*scope));
	if (!scope) return NULL;
	*scope = (struct pg_source_scope){.owner = synthesis, .parent = parent, .name = name,
		.binder = binder, .context = extended_context};
	return scope;
}

static struct pg_synthesis_job *request_role(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax, int definition)
{
	if (!scope || scope->owner != synthesis || !syntax) return NULL;
	uint64_t hash = ((uintptr_t)scope ^ (uintptr_t)syntax ^ (unsigned)definition) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (job->scope == scope && job->syntax == syntax && job->definition == definition) return job;
	}
	struct pg_synthesis_job *job = pg_alloc(synthesis->typing->graph, sizeof(*job));
	if (!job) return NULL;
	job->scope = scope;
	job->syntax = syntax;
	job->definition = definition;
	if (pg_index_insert(&synthesis->jobs, &job->index, hash) != 0) return NULL;
	if (!definition) {
		job->next = synthesis->ready;
		synthesis->ready = job;
	}
	return job;
}

struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return request_role(synthesis, scope, syntax, 0);
}

static void finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	job->status = status;
	for (struct waiter *waiter = job->waiters; waiter; waiter = waiter->next) {
		waiter->parent->dependency = NULL;
		waiter->parent->next = synthesis->ready;
		synthesis->ready = waiter->parent;
	}
	job->waiters = NULL;
}

/* Subscribe exactly once at a stage transition. Completed dependencies need
 * no subscription; pending dependencies wake their consumers on completion. */
static void depend(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child)
{
	if (!child) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	if (child->status != PG_SYNTHESIS_PENDING) {
		parent->next = synthesis->ready;
		synthesis->ready = parent;
		return;
	}
	struct waiter *waiter = pg_alloc(synthesis->typing->graph, sizeof(*waiter));
	if (!waiter) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	*waiter = (struct waiter){parent, child, child->waiters};
	child->waiters = waiter;
	parent->dependency = waiter;
}

static const struct pg_evidence *value_type(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION_TYPE)
		return pg_prove_thunk_type(synthesis->typing, synthesis->classifiers, proof);
	return pg_prove_value_type(synthesis->typing, proof);
}

/* Type positions may advance pure computations, but only checked RETURN
 * evidence exposes a value. Expected types never supply missing synthesis. */
static const struct pg_evidence *type_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *context,
	const struct pg_evidence *proof)
{
	if (!job->type_input) job->type_input = proof;
	proof = job->type_input;
	if (pg_evidence_judgement(proof) != PG_JUDGEMENT_COMPUTATION) {
		job->type_input = NULL;
		return proof;
	}
	const struct pg_evidence *returned = pg_prove_return_value(synthesis->typing, proof);
	if (returned) {
		job->type_input = NULL;
		return returned;
	}
	job->type_input = pg_reduce_computation(synthesis->typing, context, proof);
	if (!job->type_input) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return NULL; }
	job->next = synthesis->ready;
	synthesis->ready = job;
	return NULL;
}

static const struct pg_evidence *value(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE_TYPE)
		return pg_prove_type_value(synthesis->typing, proof);
	return pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE ? proof : NULL;
}

static const struct pg_evidence *computation(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION) return proof;
	proof = value(synthesis, proof);
	return proof ? pg_prove_return(synthesis->typing, synthesis->classifiers, proof) : NULL;
}

static uint64_t name_hash(struct pg_token name)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	for (size_t i = 0; i < name.length; ++i) hash = (hash ^ (unsigned char)name.text[i]) * UINT64_C(1099511628211);
	return hash;
}

static struct block_name *lookup_name(struct pg_index *names, struct pg_token name)
{
	uint64_t hash = name_hash(name);
	for (struct pg_index_entry *entry = pg_index_candidates(names, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct block_name *found = (struct block_name *)entry;
		if (found->name.length != name.length) continue;
		if (memcmp(found->name.text, name.text, name.length) == 0) return found;
	}
	return NULL;
}

static void atom(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		job->result = pg_prove_projection(synthesis->typing, job->scope->context, job->left->result);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	struct pg_token token = job->syntax->token;
	if (token.kind == '@') {
		job->result = pg_prove_universe(synthesis->typing, synthesis->classifiers, job->scope->context, 0);
	} else if (token.kind == PG_TOKEN_IDENT) {
		for (const struct pg_source_scope *scope = job->scope; scope; scope = scope->parent) {
			if (scope->definitions) {
				struct block_name *name = lookup_name(&scope->definitions->names, token);
				if (name) {
					job->left = name->producer;
					depend(synthesis, job, job->left);
					return;
				}
			}
			if (!scope->binder || scope->name.length != token.length) continue;
			if (memcmp(scope->name.text, token.text, token.length) != 0) continue;
			job->result = pg_prove_variable(synthesis->typing, job->scope->context, scope->binder);
			break;
		}
		if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	} else { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static const struct pg_evidence *compare(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->comparing) {
		if (pg_conversion_init(&job->comparison, synthesis->beta,
			pg_evidence_classifier(job->checking_term), pg_evidence_subject(job->checking_type)->core) != 0) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL;
		}
		job->comparing = 1;
	}
	enum pg_conversion_status status = pg_conversion_advance(&job->comparison, 1);
	if (status == PG_CONVERSION_PENDING) {
		job->next = synthesis->ready;
		synthesis->ready = job;
		return NULL;
	}
	if (status == PG_CONVERSION_DIFFERENT) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL; }
	if (status == PG_CONVERSION_ERROR) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL; }
	const struct pg_evidence *result = pg_prove_conversion(synthesis->typing,
		job->checking_term, job->checking_type, pg_conversion_certificate(&job->comparison));
	if (!result) finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return result;
}

static struct continuation_frame *open_continuation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *input)
{
	const struct pg_evidence *return_type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, input);
	const struct pg_evidence *domain = pg_prove_return_content(synthesis->typing, return_type);
	if (!domain) return NULL;
	const struct pg_object *binder = pg_binder(synthesis->typing->graph);
	context = pg_prove_context_extension(synthesis->typing, context, binder, domain);
	if (!context) return NULL;
	struct continuation_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
	if (frame) *frame = (struct continuation_frame){.input = input, .domain = domain, .context = context};
	return frame;
}

static const struct pg_evidence *close_continuation(struct pg_synthesis *synthesis,
	const struct continuation_frame *frame, const struct pg_evidence *body)
{
	const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, frame->context, body);
	const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, frame->domain, frame->context, codomain);
	const struct pg_evidence *continuation = pg_prove_lambda(synthesis->typing, pi, body);
	/* Known values discharge FOLD(RETURN v,K) by APP(K,v). Unknown
	 * returning computations retain their sequencing operation. */
	return frame->value ? pg_prove_application(synthesis->typing, continuation, frame->value)
		: pg_prove_fold(synthesis->typing, frame->input, continuation);
}

static int sequence_operand(struct pg_synthesis *synthesis,
	const struct continuation_frame **frames, const struct pg_evidence **context,
	const struct pg_evidence **operand, const struct pg_evidence **other)
{
	struct continuation_frame *frame = open_continuation(synthesis, *context, *operand);
	if (!frame) return -1;
	frame->parent = *frames;
	*frames = frame;
	*context = frame->context;
	*other = pg_prove_projection(synthesis->typing, *context, *other);
	*operand = pg_prove_variable(synthesis->typing, *context, pg_evidence_context(*context)->binder);
	return *other && *operand ? 0 : -1;
}

static int same_name(struct pg_token left, struct pg_token right)
{
	return left.length == right.length && memcmp(left.text, right.text, left.length) == 0;
}

static int register_name(struct pg_synthesis *synthesis, struct pg_index *names, struct pg_token name)
{
	if (!name.length) return 0;
	if (lookup_name(names, name)) return 1;
	struct block_name *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return -1;
	entry->name = name;
	return pg_index_insert(names, &entry->index, name_hash(name));
}

static void block_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->block) {
		struct block_state *block = pg_alloc(synthesis->typing->graph, sizeof(*block));
		if (!block) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->block = block;
		block->syntax = job->syntax;
		block->scope = job->scope;
		if (pg_index_init(&block->names) != 0) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->syntax->kind == PG_SYNTAX_QUALIFIED) {
			block->syntax = job->syntax->left;
			for (size_t i = 0; i < block->syntax->item_count; ++i) {
				struct pg_token name = block->syntax->items[i].name;
				if (name.length && same_name(name, job->syntax->right->token)) { block->end = i + 1; break; }
			}
			if (!block->end) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		} else block->end = block->syntax->item_count;
	}
	struct block_state *block = job->block;
	if (block->tail) {
		if (!block->frames) {
			job->result = block->tail;
			finish(synthesis, job, PG_SYNTHESIS_DONE);
			return;
		}
		const struct continuation_frame *frame = block->frames;
		block->tail = close_continuation(synthesis, frame, block->tail);
		if (!block->tail) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		block->frames = frame->parent;
		job->next = synthesis->ready;
		synthesis->ready = job;
		return;
	}
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *proof = job->left->result;
		const struct pg_evidence *input = computation(synthesis, proof);
		if (!input) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (block->next == block->end) {
			block->tail = input;
		} else {
			struct continuation_frame *frame = open_continuation(synthesis, block->scope->context, input);
			if (!frame) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			frame->value = value(synthesis, proof);
			frame->parent = block->frames;
			block->frames = frame;
			block->scope = pg_synthesis_bind(synthesis, block->scope, block->syntax->items[block->next - 1].name,
				pg_evidence_context(frame->context)->binder, frame->context);
			if (!block->scope) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		job->left = NULL;
		job->next = synthesis->ready;
		synthesis->ready = job;
		return;
	}
	const struct pg_syntax_item *item = &block->syntax->items[block->next++];
	int name_status = register_name(synthesis, &block->names, item->name);
	if (name_status) { finish(synthesis, job, name_status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
	const struct pg_syntax *expression = item->expression;
	if (item->annotation) {
		struct pg_syntax *check = pg_alloc(synthesis->typing->graph, sizeof(*check));
		if (!check) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		*check = (struct pg_syntax){.kind = PG_SYNTAX_EXPECT, .token = item->name, .left = expression, .right = item->annotation};
		expression = check;
	}
	job->left = pg_synthesis_request(synthesis, block->scope, expression);
	depend(synthesis, job, job->left);
}

static void definition_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->left = pg_synthesis_request(synthesis, job->scope, job->syntax);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *result = job->left->result;
	if (pg_evidence_judgement(result) == PG_JUDGEMENT_COMPUTATION) {
		if (synthesis->definition_policy == PG_DEFINITION_EXPLICIT_THUNK) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		result = pg_prove_thunk(synthesis->typing, synthesis->classifiers, result);
	}
	job->result = result;
	finish(synthesis, job, result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void definitions_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->definitions) {
		const struct pg_syntax *syntax = job->syntax;
		if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
		struct definition_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
		if (!state) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->definitions = state;
		if (pg_index_init(&state->names) != 0) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		state->scope = pg_alloc(synthesis->typing->graph, sizeof(*state->scope));
		if (syntax->item_count > SIZE_MAX / sizeof(*state->entries)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		state->entries = pg_alloc(synthesis->typing->graph, syntax->item_count * sizeof(*state->entries));
		if (!state->scope || !state->entries) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		*state->scope = (struct pg_source_scope){.owner = synthesis, .parent = job->scope,
			.context = job->scope->context, .definitions = state};
		state->count = syntax->item_count;
		state->syntax = syntax;
	}
	struct definition_state *state = job->definitions;
	/* Register one producer per transition. Dormant producers cannot observe
	 * a partially constructed name index. */
	if (state->indexed < state->count) {
		size_t i = state->indexed++;
		const struct pg_syntax_item *item = &state->syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) {
			if (item->operation != PG_TOKEN_ASSIGN) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			int status = register_name(synthesis, &state->names, item->name);
			if (status) { finish(synthesis, job, status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
			struct block_name *name = lookup_name(&state->names, item->name);
			if (!name) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			name->producer = request_role(synthesis, state->scope, item->expression, 1);
			state->entries[i] = name->producer;
			if (!name->producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		job->next = synthesis->ready;
		synthesis->ready = job;
		return;
	}
	if (state->activated < state->count) {
		size_t i = state->activated++;
		if (state->entries[i]) {
			struct pg_synthesis_job *producer = state->entries[i];
			if (!producer->stage) {
				producer->stage = 1;
				producer->next = synthesis->ready;
				synthesis->ready = producer;
			}
		} else {
			const struct pg_syntax_item *item = &state->syntax->items[i];
			if (!lookup_name(&state->names, item->name)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			struct pg_syntax *name = pg_alloc(synthesis->typing->graph, sizeof(*name));
			struct pg_syntax *check = pg_alloc(synthesis->typing->graph, sizeof(*check));
			if (!name || !check) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			*name = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = item->name};
			*check = (struct pg_syntax){.kind = PG_SYNTAX_EXPECT, .left = name, .right = item->expression};
			state->entries[i] = pg_synthesis_request(synthesis, state->scope, check);
			if (!state->entries[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		job->next = synthesis->ready;
		synthesis->ready = job;
		return;
	}
	if (!job->stage) {
		if (job->syntax->kind == PG_SYNTAX_QUALIFIED) {
			struct block_name *name = lookup_name(&state->names, job->syntax->right->token);
			if (!name) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			state->selected = name->producer;
		}
		job->stage = 1;
	}
	if (job->left && job->left->status != PG_SYNTHESIS_DONE) {
		finish(synthesis, job, job->left->status); return;
	}
	if (state->next < state->count) {
		job->left = state->entries[state->next++];
		depend(synthesis, job, job->left);
		return;
	}
	job->result = state->selected ? state->selected->result : NULL;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
	if (job->definition) { definition_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_BLOCK) { block_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_BLOCK) { block_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_ATOM) { atom(synthesis, job); return; }
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA:
		if (syntax->binder_marker) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		break;
	case PG_SYNTAX_PI: case PG_SYNTAX_APPLICATION: case PG_SYNTAX_QUOTE: case PG_SYNTAX_EXPECT:
		break;
	default:
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (!job->stage) {
		const struct pg_syntax *left = syntax->left;
		if (syntax->kind == PG_SYNTAX_PI && left->kind == PG_SYNTAX_BINDER) left = left->left;
		job->left = pg_synthesis_request(synthesis, job->scope, left);
		job->stage = 1;
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (syntax->kind == PG_SYNTAX_QUOTE) {
		job->result = pg_prove_thunk(synthesis->typing, synthesis->classifiers, job->left->result);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
		return;
	}
	if (job->stage == 1) {
		const struct pg_source_scope *scope = job->scope;
		if (syntax->kind == PG_SYNTAX_LAMBDA || syntax->kind == PG_SYNTAX_PI) {
			const struct pg_evidence *input = type_input(synthesis, job, scope->context, job->left->result);
			if (!input) return;
			job->domain = value_type(synthesis, input);
			if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			struct pg_token name = syntax->token;
			if (syntax->kind == PG_SYNTAX_PI) {
				name = syntax->left->token;
				if (syntax->left->kind != PG_SYNTAX_BINDER) name.length = 0;
			}
			const struct pg_object *binder = pg_binder(synthesis->typing->graph);
			const struct pg_evidence *context = pg_prove_context_extension(synthesis->typing, scope->context, binder, job->domain);
			job->inner = pg_synthesis_bind(synthesis, scope, name, binder, context);
			if (!job->inner) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			scope = job->inner;
		}
		job->right = pg_synthesis_request(synthesis, scope, syntax->right);
		job->stage = 2;
		depend(synthesis, job, job->right);
		return;
	}
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	const struct pg_evidence *left = job->left->result, *right = job->right->result;
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA: {
		const struct pg_evidence *body = computation(synthesis, right);
		if (!body) break;
		const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, job->inner->context, body);
		if (!codomain) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, job->inner->context, codomain);
		job->result = pg_prove_lambda(synthesis->typing, pi, body);
		break;
	}
	case PG_SYNTAX_PI: {
		const struct pg_evidence *codomain = type_input(synthesis, job, job->inner->context, right);
		if (!codomain) return;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, job->inner->context, codomain);
		break;
	}
	case PG_SYNTAX_APPLICATION: {
		if (!job->checking_term) {
			const struct pg_evidence *context = job->scope->context;
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) left = pg_prove_force(synthesis->typing, left);
			if (!left) break;
			const struct pg_term *returned;
			if (pg_return_type_view(pg_evidence_classifier(left), &returned)) {
				if (sequence_operand(synthesis, &job->application_frame, &context, &left, &right) != 0) {
					finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
				}
			}
			if (pg_evidence_judgement(right) == PG_JUDGEMENT_COMPUTATION) {
				if (sequence_operand(synthesis, &job->application_frame, &context, &right, &left) != 0) {
					finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
				}
			}
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) left = pg_prove_force(synthesis->typing, left);
			right = value(synthesis, right);
			if (!left || !right) break;
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			if (!pg_pi_view(pg_evidence_classifier(left), &domain, &binder, &codomain)) break;
			const struct pg_evidence *pi = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, left);
			job->checking_type = pg_prove_pi_domain(synthesis->typing, pi);
			if (!job->checking_type) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			job->checking_term = right;
			job->function = left;
		}
		right = job->checking_term;
		if (pg_evidence_classifier(right) != pg_evidence_subject(job->checking_type)->core)
			right = compare(synthesis, job);
		if (!right) return;
		job->result = pg_prove_application(synthesis->typing, job->function, right);
		for (const struct continuation_frame *frame = job->application_frame; job->result && frame; frame = frame->parent) {
			job->result = close_continuation(synthesis, frame, job->result);
			if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		}
		break;
	}
	case PG_SYNTAX_EXPECT: {
		if (!job->checking_term) {
			right = type_input(synthesis, job, job->scope->context, right);
			if (!right) return;
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE_TYPE) left = value(synthesis, left);
			if (!left) break;
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) right = value_type(synthesis, right);
			else if (pg_evidence_judgement(right) != PG_JUDGEMENT_COMPUTATION_TYPE)
				right = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, right));
			if (!right) break;
			job->checking_term = left;
			job->checking_type = right;
		}
		job->result = compare(synthesis, job);
		if (!job->result) return;
		break;
	}
	default: break;
	}
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}

void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget)
{
	while (synthesis->ready && budget) {
		struct pg_synthesis_job *job = synthesis->ready;
		synthesis->ready = job->next;
		job->next = NULL;
		--budget;
		++synthesis->steps;
		step(synthesis, job);
	}
}
enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job) { return job->status; }
const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job) { return job->result; }
const struct pg_synthesis_job *pg_synthesis_dependency(const struct pg_synthesis_job *job)
{
	return job && job->dependency ? job->dependency->child : NULL;
}

const struct pg_synthesis_job *pg_synthesis_cycle(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *slow = job, *fast = job;
	do {
		slow = pg_synthesis_dependency(slow);
		fast = pg_synthesis_dependency(pg_synthesis_dependency(fast));
		if (!slow || !fast) return NULL;
	} while (slow != fast);
	return slow;
}

struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name)
{
	if (!root || !root->definitions || !name.length || !name.text) return NULL;
	struct block_name *entry = lookup_name(&root->definitions->names, name);
	return entry ? entry->producer : NULL;
}
