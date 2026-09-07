#include "synthesis.h"

#include <string.h>

struct pg_source_scope {
	const struct pg_synthesis *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder;
	const struct pg_evidence *context;
};
struct waiter {
	struct pg_synthesis_job *parent;
	struct waiter *next;
};
struct block_frame {
	const struct pg_evidence *input;
	const struct pg_evidence *value;
	const struct pg_evidence *domain;
	const struct pg_evidence *context;
	const struct block_frame *parent;
};
struct block_name {
	struct pg_index_entry index;
	struct pg_token name;
};
struct block_state {
	const struct pg_syntax *syntax;
	size_t next, end;
	const struct pg_source_scope *scope;
	const struct block_frame *frames;
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
	struct pg_synthesis_job *left;
	struct pg_synthesis_job *right;
	const struct pg_source_scope *inner;
	const struct pg_evidence *domain;
	const struct pg_evidence *result;
	const struct pg_evidence *checking_term;
	const struct pg_evidence *checking_type;
	const struct pg_evidence *function;
	struct pg_conversion comparison;
	int comparing;
	struct block_state *block;
};

int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_beta_work *beta)
{
	memset(synthesis, 0, sizeof(*synthesis));
	if (classifiers->graph != typing->graph || beta->graph != typing->graph) return -1;
	synthesis->typing = typing;
	synthesis->classifiers = classifiers;
	synthesis->beta = beta;
	return pg_index_init(&synthesis->jobs);
}

void pg_synthesis_destroy(struct pg_synthesis *synthesis)
{
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
			pg_conversion_destroy(&job->comparison);
			if (job->block) pg_index_destroy(&job->block->names);
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
	*scope = (struct pg_source_scope){synthesis, parent, name, binder, extended_context};
	return scope;
}

struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis || !syntax) return NULL;
	uint64_t hash = ((uintptr_t)scope ^ (uintptr_t)syntax) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (job->scope == scope && job->syntax == syntax) return job;
	}
	struct pg_synthesis_job *job = pg_alloc(synthesis->typing->graph, sizeof(*job));
	if (!job) return NULL;
	job->scope = scope;
	job->syntax = syntax;
	if (pg_index_insert(&synthesis->jobs, &job->index, hash) != 0) return NULL;
	job->next = synthesis->ready;
	synthesis->ready = job;
	return job;
}

static void finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	job->status = status;
	for (struct waiter *waiter = job->waiters; waiter; waiter = waiter->next) {
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
	*waiter = (struct waiter){parent, child->waiters};
	child->waiters = waiter;
}

static const struct pg_evidence *value_type(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION_TYPE)
		return pg_prove_thunk_type(synthesis->typing, synthesis->classifiers, proof);
	return pg_prove_value_type(synthesis->typing, proof);
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

static void atom(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_token token = job->syntax->token;
	if (token.kind == '@') {
		job->result = pg_prove_universe(synthesis->typing, synthesis->classifiers, job->scope->context, 0);
	} else if (token.kind == PG_TOKEN_IDENT) {
		for (const struct pg_source_scope *scope = job->scope; scope; scope = scope->parent) {
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

static const struct pg_evidence *sequence_application(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_evidence *function,
	const struct pg_evidence *argument)
{
	if (pg_evidence_judgement(function) == PG_JUDGEMENT_VALUE)
		function = pg_prove_force(synthesis->typing, function);
	if (!function) return NULL;
	const struct pg_evidence *return_type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, scope->context, argument);
	const struct pg_evidence *domain = pg_prove_return_content(synthesis->typing, return_type);
	if (!domain) return NULL;
	const struct pg_object *binder = pg_binder(synthesis->typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(synthesis->typing, scope->context, binder, domain);
	if (!context) return NULL;
	function = pg_prove_projection(synthesis->typing, context, function);
	const struct pg_evidence *variable = pg_prove_variable(synthesis->typing, context, binder);
	const struct pg_evidence *body = pg_prove_application(synthesis->typing, function, variable);
	const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, body);
	const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, domain, context, codomain);
	const struct pg_evidence *continuation = pg_prove_lambda(synthesis->typing, pi, body);
	return pg_prove_fold(synthesis->typing, argument, continuation);
}

static int same_name(struct pg_token left, struct pg_token right)
{
	return left.length == right.length && memcmp(left.text, right.text, left.length) == 0;
}

static int block_name(struct pg_synthesis *synthesis, struct block_state *block, struct pg_token name)
{
	if (!name.length) return 0;
	uint64_t hash = UINT64_C(14695981039346656037);
	for (size_t i = 0; i < name.length; ++i) hash = (hash ^ (unsigned char)name.text[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&block->names, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		if (same_name(((struct block_name *)entry)->name, name)) return 1;
	}
	struct block_name *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return -1;
	entry->name = name;
	return pg_index_insert(&block->names, &entry->index, hash);
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
		const struct block_frame *frame = block->frames;
		const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, frame->context, block->tail);
		const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, frame->domain, frame->context, codomain);
		const struct pg_evidence *continuation = pg_prove_lambda(synthesis->typing, pi, block->tail);
		/* A syntactic value has already been obtained. This is the checked
		 * FOLD(RETURN v,K) equation, not execution of an unknown computation. */
		block->tail = frame->value ? pg_prove_application(synthesis->typing, continuation, frame->value)
			: pg_prove_fold(synthesis->typing, frame->input, continuation);
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
			const struct pg_evidence *formation = pg_prove_classifier(synthesis->typing, synthesis->classifiers, block->scope->context, input);
			const struct pg_evidence *domain = pg_prove_return_content(synthesis->typing, formation);
			if (!domain) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			const struct pg_object *binder = pg_binder(synthesis->typing->graph);
			const struct pg_evidence *context = pg_prove_context_extension(synthesis->typing, block->scope->context, binder, domain);
			struct block_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
			if (!frame || !context) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			*frame = (struct block_frame){input, value(synthesis, proof), domain, context, block->frames};
			block->frames = frame;
			block->scope = pg_synthesis_bind(synthesis, block->scope, block->syntax->items[block->next - 1].name, binder, context);
			if (!block->scope) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		job->left = NULL;
		job->next = synthesis->ready;
		synthesis->ready = job;
		return;
	}
	const struct pg_syntax_item *item = &block->syntax->items[block->next++];
	int name_status = block_name(synthesis, block, item->name);
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

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
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
			job->domain = value_type(synthesis, job->left->result);
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
		const struct pg_evidence *codomain = right;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, job->inner->context, codomain);
		break;
	}
	case PG_SYNTAX_APPLICATION: {
		if (!job->checking_term) {
			if (pg_evidence_judgement(right) == PG_JUDGEMENT_COMPUTATION) {
				job->result = sequence_application(synthesis, job->scope, left, right);
				finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED); return;
			}
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) left = pg_prove_force(synthesis->typing, left);
			right = value(synthesis, right);
			if (!left || !right) break;
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			if (!pg_pi_view(pg_evidence_classifier(left), &domain, &binder, &codomain)) break;
			if (domain == pg_evidence_classifier(right)) {
				job->result = pg_prove_application(synthesis->typing, left, right);
				break;
			}
			const struct pg_evidence *pi = pg_prove_classifier(synthesis->typing, synthesis->classifiers, job->scope->context, left);
			job->checking_type = pg_prove_pi_domain(synthesis->typing, pi);
			if (!job->checking_type) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			job->checking_term = right;
			job->function = left;
		}
		right = compare(synthesis, job);
		if (!right) return;
		job->result = pg_prove_application(synthesis->typing, job->function, right);
		break;
	}
	case PG_SYNTAX_EXPECT: {
		if (!job->checking_term) {
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
