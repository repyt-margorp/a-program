#include "synthesis_source.h"
#include "derivation.h"

struct binding_work {
	const struct pg_object *binder;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job *domain;
};
struct domain_work { struct pg_synthesis_job *returned, *normalized; };
struct telescope_work {
	const struct pg_source_scope *scope;
	const struct pg_syntax *tail;
	struct pg_source_context_allocation *allocation;
};
struct declaration_work {
	struct pg_synthesis_job *input;
	const struct pg_term *result;
};

static void binding_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void domain_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void telescope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void telescope_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void declared_type_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void domain_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static const struct pg_term *declaration_result(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class BINDING_JOB[1] = {{
	.size = sizeof(struct binding_work), .advance = binding_step}};
static const struct pg_synthesis_work_class DOMAIN_JOB[1] = {{
	.size = sizeof(struct domain_work), .advance = domain_step}};
static const struct pg_synthesis_work_class TELESCOPE_JOB[1] = {{.advance = telescope_step}};
static const struct pg_synthesis_work_class TELESCOPE_STRUCTURE_JOB[1] = {{
	.size = sizeof(struct telescope_work), .advance = telescope_structure_step}};
static const struct pg_synthesis_work_class DECLARED_TYPE_JOB[1] = {{
	.size = sizeof(struct declaration_work), .advance = declared_type_step, .structure = declaration_result}};
static const struct pg_synthesis_work_class DOMAIN_STRUCTURE_JOB[1] = {{
	.size = sizeof(struct declaration_work), .advance = domain_structure_step, .structure = declaration_result}};

static int scope_ready(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_source_scope *scope)
{
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return 0;
	if (pg_synthesis_await(synthesis, job, scope->context_job)) return 0;
	if (pg_synthesis_scope_context(scope)) return 1;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 0;
}

struct pg_synthesis_job *pg_synthesis_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return pg_synthesis_binding_at(synthesis, scope, syntax, NULL);
}

struct pg_synthesis_job *pg_synthesis_binding_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_object *binder)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!syntax || (syntax->kind != PG_SYNTAX_LAMBDA && syntax->kind != PG_SYNTAX_PI)) return NULL;
	if (binder && binder->kind != PG_BINDER) return NULL;
	const void *inputs[] = {scope, syntax};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, BINDING_JOB, 2, inputs);
	if (!job) return NULL;
	struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	if (local->binder && binder && local->binder != binder) return NULL;
	if (!local->binder) local->binder = pg_synthesis_source_binder(synthesis, scope, syntax, 0, binder);
	if (!local->binder) return NULL;
	if (!local->domain) {
		const struct pg_syntax *domain = syntax->left;
		if (syntax->kind == PG_SYNTAX_PI && domain->kind == PG_SYNTAX_BINDER) domain = domain->left;
		struct pg_synthesis_job *annotation = pg_synthesis_request(synthesis, scope, domain);
		if (!annotation) return NULL;
		local->domain = pg_synthesis_work_request(synthesis, DOMAIN_JOB, 2,
			(const void *[]){scope, annotation});
		if (!local->domain) return NULL;
	}
	if (!local->scope) {
		struct pg_token name = syntax->token;
		if (syntax->kind == PG_SYNTAX_PI)
			name = syntax->left->kind == PG_SYNTAX_BINDER ? syntax->left->token : (struct pg_token){0};
		local->scope = pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
			.name = name, .binder = local->binder, .context_job = job});
	}
	return local->scope ? job : NULL;
}

int pg_synthesis_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax, const struct pg_object **binder)
{
	const struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	if (!synthesis || !local || !scope || !syntax || !binder) return -1;
	if (job->owner != synthesis->owner_key || !local->binder) return -1;
	*scope = job->inputs[0]; *syntax = job->inputs[1]; *binder = local->binder;
	return 0;
}

const struct pg_object *pg_synthesis_binding_binder(const struct pg_synthesis_job *job)
{
	const struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	return local ? local->binder : NULL;
}

const struct pg_source_scope *pg_synthesis_binding_scope(const struct pg_synthesis_job *job)
{
	const struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	return local ? local->scope : NULL;
}

static void domain_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = job->inputs[0];
	if (!scope_ready(synthesis, job, scope)) return;
	struct domain_work *local = pg_synthesis_work_state(job, DOMAIN_JOB);
	if (!local->normalized) {
		struct pg_synthesis_job *annotation = (void *)job->inputs[1];
		if (pg_synthesis_await(synthesis, job, annotation)) return;
		const struct pg_evidence *input = annotation->result;
		const struct pg_evidence *context = pg_synthesis_scope_context(scope);
		if (pg_evidence_judgement(input) == PG_JUDGEMENT_COMPUTATION) {
			if (!local->returned) local->returned = pg_synthesis_return(synthesis, context, input);
			if (pg_synthesis_await(synthesis, job, local->returned)) return;
			input = local->returned->result;
		}
		input = pg_evidence_judgement(input) == PG_JUDGEMENT_COMPUTATION_TYPE
			? pg_prove_thunk_type(synthesis->typing, input) : pg_prove_value_type(synthesis->typing, input);
		if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->normalized = pg_synthesis_normalize(synthesis, context, input);
	}
	pg_synthesis_forward(synthesis, job, local->normalized);
}

/* In binder annotations, an arrow telescope ending in @ declares a logical
 * family, not a promise to extract a type from an arbitrary computation.
 * Inspect the synthesized annotation, so an alias has the same contract. */
static int family_parameter_annotation(const struct pg_term *type)
{
	if (!pg_thunk_type_view(type, &type)) return 0;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	size_t count = 0;
	while (pg_pi_view(type, &domain, &binder, &body)) {
		++count;
		type = body;
	}
	uint64_t level;
	enum pg_totality totality;
	return count && pg_pure_computation_type_view(type, &totality, &body) && pg_universe_level(body, &level);
}

static const struct pg_term *family_parameter_structure(struct pg_graph *graph, const struct pg_term *type)
{
	struct parameter { const struct pg_term *domain; const struct pg_object *binder; struct parameter *next; };
	struct pg_graph temporary = {0};
	struct parameter *parameters = NULL;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	const struct pg_term *result = NULL;
	if (!pg_thunk_type_view(type, &type)) goto done;
	while (pg_pi_view(type, &domain, &binder, &body)) {
		struct parameter *next = pg_alloc(&temporary, sizeof(*next));
		if (!next) goto done;
		*next = (struct parameter){domain, binder, parameters};
		parameters = next;
		type = body;
	}
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(type, &totality, &result)) goto done;
	while (parameters && result) {
		result = pg_pi(graph, parameters->domain, parameters->binder, result);
		parameters = parameters->next;
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *family_parameter_context(struct pg_synthesis *synthesis,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *annotation)
{
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *type = pg_prove_thunk_content(typing, annotation);
	const struct pg_evidence *indices = parent;
	const struct pg_term *domain, *body;
	const struct pg_object *argument;
	while (type && pg_pi_view(pg_evidence_subject(type)->core, &domain, &argument, &body)) {
		indices = pg_prove_context_extension(typing, indices, argument, pg_prove_pi_domain(typing, type));
		if (!indices) return NULL;
		type = pg_prove_pi_codomain(typing, pg_prove_projection(typing, indices, type),
			pg_prove_variable(typing, indices, argument));
	}
	return pg_prove_family_context_extension(typing, parent, binder, indices,
		pg_prove_return_content(typing, type));
}

static void binding_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = job->inputs[0];
	if (!scope_ready(synthesis, job, scope)) return;
	const struct pg_syntax *syntax = job->inputs[1];
	if (syntax->binder_marker) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	if (pg_synthesis_await(synthesis, job, local->domain)) return;
	const struct pg_evidence *domain = local->domain->result;
	job->result = family_parameter_annotation(pg_evidence_subject(domain)->core)
		? family_parameter_context(synthesis, pg_synthesis_scope_context(scope), local->binder, domain)
		: pg_prove_context_extension(synthesis->typing, pg_synthesis_scope_context(scope), local->binder, domain);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

struct pg_synthesis_job *pg_synthesis_telescope_structure(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis->owner_key || !syntax) return NULL;
	return pg_synthesis_work_request(synthesis, TELESCOPE_STRUCTURE_JOB, 2, (const void *[]){scope, syntax});
}

struct pg_synthesis_job *pg_synthesis_telescope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *structure = pg_synthesis_telescope_structure(synthesis, scope, syntax);
	return structure ? pg_synthesis_work_request(synthesis, TELESCOPE_JOB, 1, (const void *[]){structure}) : NULL;
}

struct pg_synthesis_job *pg_synthesis_telescope_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *structure = pg_synthesis_telescope_structure(synthesis, scope, syntax);
	if (!structure) return NULL;
	struct telescope_work *local = pg_synthesis_work_state(structure, TELESCOPE_STRUCTURE_JOB);
	if (pg_synthesis_context_allocation_at(synthesis, &local->allocation, prefix, end, local->scope != NULL)) return NULL;
	return pg_synthesis_telescope(synthesis, scope, syntax);
}

/* The checked telescope borrows the structural request's scope and tail.
 * It has no second cursor or copied result; its result certifies the Context. */
static const struct telescope_work *telescope_result(const struct pg_synthesis_job *job)
{
	if (!job || job->status != PG_SYNTHESIS_DONE) return NULL;
	if (job->role == TELESCOPE_JOB) job = job->inputs[0];
	return pg_synthesis_work_state(job, TELESCOPE_STRUCTURE_JOB);
}

const struct pg_source_scope *pg_synthesis_telescope_scope(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = telescope_result(job);
	return local ? local->scope : NULL;
}

const struct pg_syntax *pg_synthesis_telescope_body(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = telescope_result(job);
	return local ? local->tail : NULL;
}

static void telescope_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = job->inputs[0];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	const struct pg_syntax *syntax = job->inputs[1];
	struct telescope_work *local = pg_synthesis_work_state(job, TELESCOPE_STRUCTURE_JOB);
	if (!local->scope) { local->scope = scope; local->tail = syntax; }
	if (local->tail->kind != syntax->kind ||
		(local->tail->kind != PG_SYNTAX_LAMBDA && local->tail->kind != PG_SYNTAX_PI)) {
		pg_synthesis_finish(synthesis, job, local->allocation && local->allocation->next != local->allocation->count
			? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE);
		return;
	}
	const struct pg_object *binder = NULL;
	if (local->allocation) {
		struct pg_source_context_allocation *allocation = local->allocation;
		if (allocation->next == allocation->count) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		binder = allocation->contexts[allocation->next++]->binder;
	}
	struct pg_synthesis_job *binding = pg_synthesis_binding_at(synthesis, local->scope, local->tail, binder);
	if (!binding) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	local->scope = pg_synthesis_binding_scope(binding);
	local->tail = local->tail->right;
	pg_synthesis_enqueue(synthesis, job);
}

static void telescope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *structure = (void *)job->inputs[0];
	const struct pg_source_scope *outer = structure->inputs[0];
	if (pg_synthesis_scope_wait(synthesis, job, outer)) return;
	if (pg_synthesis_await(synthesis, job, structure)) return;
	const struct pg_source_scope *scope = pg_synthesis_telescope_scope(structure);
	if (pg_synthesis_await(synthesis, job, scope->context_job)) return;
	job->result = pg_synthesis_scope_context(scope);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

struct pg_synthesis_job *pg_synthesis_declared_type(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, const struct pg_object *binder)
{
	if (!context || context->owner != synthesis->owner_key || !binder) return NULL;
	return pg_synthesis_work_request(synthesis, DECLARED_TYPE_JOB, 2, (const void *[]){context, binder});
}

const struct pg_synthesis_work_class *pg_synthesis_binding_type_class(const struct pg_synthesis_job *input)
{
	return input && input->role == DOMAIN_JOB ? DOMAIN_STRUCTURE_JOB : NULL;
}

static const struct pg_term *declaration_result(const struct pg_synthesis_job *job)
{
	const struct declaration_work *local = pg_synthesis_work_state(job, job->role);
	return local->result;
}

static void declaration_forward(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, job->role);
	if (pg_synthesis_await(synthesis, job, local->input)) return;
	local->result = pg_synthesis_type_structure_result(local->input);
	pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void domain_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, DOMAIN_STRUCTURE_JOB);
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	const struct pg_evidence *proof = pg_synthesis_result(producer);
	if (proof) {
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind == PG_JUDGEMENT_VALUE_TYPE || kind == PG_JUDGEMENT_COMPUTATION_TYPE)
			local->result = pg_evidence_subject(proof)->core;
		pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (!local->input) {
		struct pg_synthesis_job *annotation = (void *)producer->inputs[1], *rule;
		if (pg_synthesis_await_preparation(synthesis, job, annotation, &rule)) return;
		if (pg_synthesis_source_value_kind(annotation) == 2)
			local->input = pg_synthesis_type_structure(synthesis, annotation);
		else {
			const struct pg_derivation_input *input = pg_synthesis_plain_derivation(rule);
			if (input && input->rule == PG_VARIABLE) local->input = pg_synthesis_term_structure(synthesis, rule);
		}
	}
	if (local->input) { declaration_forward(synthesis, job); return; }
	if (pg_synthesis_await(synthesis, job, producer)) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static void declared_type_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, DECLARED_TYPE_JOB);
	struct pg_synthesis_job *context = (void *)job->inputs[0];
	const struct pg_object *binder = job->inputs[1];
	const struct pg_evidence *accepted = pg_synthesis_result(context);
	if (accepted) {
		const struct pg_context *declaration = pg_evidence_judgement(accepted) == PG_JUDGEMENT_CONTEXT
			? pg_context_lookup(pg_evidence_context(accepted), binder) : NULL;
		local->result = declaration ? declaration->declared_type : NULL;
		pg_synthesis_finish(synthesis, job, declaration ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	const struct binding_work *binding = pg_synthesis_work_state(context, BINDING_JOB);
	if (!local->input) {
		struct pg_synthesis_job *prepared;
		if (pg_synthesis_await_preparation(synthesis, job, context, &prepared)) return;
		if (!prepared) prepared = pg_synthesis_scope_context_input(context);
		if (prepared) local->input = pg_synthesis_declared_type(synthesis, prepared, binder);
		else if (binding) {
			const struct pg_source_scope *scope = context->inputs[0];
			local->input = binding->binder == binder ? pg_synthesis_type_structure(synthesis, binding->domain)
				: pg_synthesis_declared_type(synthesis, scope->context_job, binder);
		} else {
			const struct pg_derivation_input *input = pg_synthesis_plain_derivation(context);
			if (input && input->rule == PG_CONTEXT_EXTEND) {
				struct pg_synthesis_job *premise = pg_synthesis_rule_premise(synthesis, context,
					input->parameters.binder == binder ? 1 : 0);
				if (!premise) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
				local->input = input->parameters.binder == binder
					? pg_synthesis_type_structure(synthesis, premise) : pg_synthesis_declared_type(synthesis, premise, binder);
			}
		}
	}
	if (local->input) {
		if (pg_synthesis_await(synthesis, job, local->input)) return;
		local->result = pg_synthesis_type_structure_result(local->input);
		if (binding && binding->binder == binder && family_parameter_annotation(local->result))
			local->result = family_parameter_structure(synthesis->typing->graph, local->result);
		pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (pg_synthesis_await(synthesis, job, context)) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}
