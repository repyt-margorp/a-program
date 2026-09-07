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
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next)
			pg_conversion_destroy(&((struct pg_synthesis_job *)entry)->comparison);
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

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
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
				finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
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
