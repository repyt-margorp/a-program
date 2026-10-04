#include "synthesis_source.h"
#include "derivation.h"

struct binding_work {
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
	struct pg_synthesis_structure input;
	const struct pg_term *result;
};

static void binding_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void scope_context_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void domain_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void telescope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void declared_type_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void domain_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static const struct pg_term *declaration_result(const struct pg_synthesis_job *);
static struct pg_synthesis_input domain_output(const struct pg_synthesis_job *);
static struct pg_synthesis_projection telescope_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_input telescope_output(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class BINDING_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct binding_work), .advance = binding_step}};
static const struct pg_synthesis_work_class SCOPE_CONTEXT_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .advance = scope_context_step,
	.output = pg_synthesis_scope_context_input}};
static const struct pg_synthesis_work_class DOMAIN_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct domain_work), .advance = domain_step, .output = domain_output}};
static const struct pg_synthesis_work_class TELESCOPE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct telescope_work), .advance = telescope_step,
	.project = telescope_projection, .output = telescope_output}};
static const struct pg_synthesis_work_class DECLARED_TYPE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct declaration_work), .advance = declared_type_step, .structure = declaration_result}};
static const struct pg_synthesis_work_class DOMAIN_STRUCTURE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct declaration_work), .advance = domain_structure_step, .structure = declaration_result}};

static struct pg_synthesis_input domain_output(const struct pg_synthesis_job *job)
{
	const struct domain_work *local = pg_synthesis_work_state(job, DOMAIN_JOB);
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(local->normalized)};
}

static int scope_ready(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_source_scope *scope)
{
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return 0;
	if (pg_synthesis_await_input(synthesis, job, scope->context)) return 0;
	if (pg_synthesis_scope_context(scope)) return 1;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 0;
}

struct pg_synthesis_input pg_synthesis_scope_context_input(const struct pg_synthesis_job *job)
{
	return job && pg_synthesis_work_role(job) == SCOPE_CONTEXT_JOB
		? pg_synthesis_work_dependency(job, 2) : (struct pg_synthesis_input){0};
}

static int binding_context(struct pg_synthesis *synthesis, const struct pg_source_scope *parent,
	const struct pg_object *binder, struct pg_synthesis_input input, const struct pg_object *field)
{
	if (!parent || parent->owner != synthesis->owner_key) return 0;
	const struct pg_evidence *extended_context = pg_synthesis_input_result(input);
	if (!extended_context) {
		if (field) return 0;
		struct pg_synthesis_job *producer = pg_pending_job(input.pending);
		const struct pg_derivation_input *rule = pg_synthesis_plain_derivation(producer);
		if (!rule) return 0;
		switch (rule->rule) {
		case PG_CONTEXT_EXTEND: if (rule->count != 2) return 0; break;
		case PG_CONTEXT_FAMILY_EXTEND: if (rule->count != 3) return 0; break;
		default: return 0;
		}
		if (rule->parameters.binder != binder) return 0;
		/* This is the actual extension contract, not accepted Context evidence. */
		struct pg_synthesis_input base = pg_synthesis_rule_input(synthesis, producer, 0);
		if (base.checked != parent->context.checked) return 0;
		return base.pending == parent->context.pending;
	}
	if (!pg_evidence_owned_by(extended_context, synthesis->typing)) return 0;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return 0;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!pg_synthesis_scope_context(parent)) return 0;
	if (!context || context->parent != pg_evidence_context(pg_synthesis_scope_context(parent))) return 0;
	if (context->binder != binder) return 0;
	return !field || pg_context_lookup(context->parent, field);
}

static void scope_context_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *parent = job->inputs[0];
	if (!scope_ready(synthesis, job, parent)) return;
	struct pg_synthesis_input context = pg_synthesis_scope_context_input(job);
	if (pg_synthesis_await_input(synthesis, job, context)) return;
	pg_synthesis_finish(synthesis, job, binding_context(synthesis, parent, job->inputs[1],
		context, job->inputs[4]) ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}

const struct pg_source_scope *pg_synthesis_scope_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, struct pg_synthesis_input context, const struct pg_object *field,
	struct pg_synthesis_job *clause, enum pg_source_association association)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (!binder || binder->kind != PG_BINDER) return NULL;
	if (!pg_synthesis_input_owned(synthesis, context)) return NULL;
	if (field && field->kind != PG_BINDER) return NULL;
	const void *inputs[] = {parent, binder, context.checked, context.pending, field};
	/* Preserve existing validation keys; exact extension contracts already own
	 * parent/Binder checking. Neither path changes pending lexical identity. */
	struct pg_synthesis_job *checked = pg_synthesis_work_find(synthesis, SCOPE_CONTEXT_JOB, 5, inputs);
	if (checked || !binding_context(synthesis, parent, binder, context, field)) {
		if (!checked && context.checked && pg_synthesis_scope_context(parent)) return NULL;
		if (!checked) checked = pg_synthesis_work_request(synthesis, SCOPE_CONTEXT_JOB, 5, inputs);
		if (!checked) return NULL;
		context = (struct pg_synthesis_input){.pending = pg_synthesis_pending(checked)};
	}
	return pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context = context, .associated_binder = field,
		.association = association, .clause = clause});
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
	if (local->scope) return !binder || local->scope->binder == binder ? job : NULL;
	binder = pg_synthesis_source_binder(synthesis, scope, syntax, 0, binder);
	if (!binder) return NULL;
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
			.name = name, .binder = binder, .context = {.pending = pg_synthesis_pending(job)}});
	}
	return local->scope ? job : NULL;
}

int pg_synthesis_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax, const struct pg_object **binder)
{
	const struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	if (!synthesis || !local || !scope || !syntax || !binder) return -1;
	if (job->pending.owner != synthesis->owner_key || !local->scope) return -1;
	*scope = job->inputs[0]; *syntax = job->inputs[1]; *binder = local->scope->binder;
	return 0;
}

const struct pg_object *pg_synthesis_binding_binder(const struct pg_synthesis_job *job)
{
	const struct binding_work *local = pg_synthesis_work_state(job, BINDING_JOB);
	return local && local->scope ? local->scope->binder : NULL;
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
		const struct pg_evidence *input = pg_synthesis_result(annotation);
		const struct pg_evidence *context = pg_synthesis_scope_context(scope);
		if (pg_evidence_judgement(input) == PG_JUDGEMENT_COMPUTATION) {
			if (!local->returned) local->returned = pg_synthesis_return(synthesis, context, input);
			if (pg_synthesis_await(synthesis, job, local->returned)) return;
			input = pg_synthesis_result(local->returned);
		}
		input = pg_evidence_judgement(input) == PG_JUDGEMENT_COMPUTATION_TYPE
			? pg_prove_thunk_type(synthesis->typing, input) : pg_prove_value_type(synthesis->typing, input);
		if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->normalized = pg_synthesis_normalize(synthesis, context, input);
	}
	pg_synthesis_finish(synthesis, job, local->normalized ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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
	if (!local->scope) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (pg_synthesis_await_pending(synthesis, job, pg_synthesis_pending(local->domain))) return;
	const struct pg_evidence *domain = pg_synthesis_result(local->domain);
	job->result = family_parameter_annotation(pg_evidence_subject(domain)->core)
		? family_parameter_context(synthesis, pg_synthesis_scope_context(scope), local->scope->binder, domain)
		: pg_prove_context_extension(synthesis->typing, pg_synthesis_scope_context(scope), local->scope->binder, domain);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

struct pg_synthesis_job *pg_synthesis_telescope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis->owner_key || !syntax) return NULL;
	return pg_synthesis_work_request(synthesis, TELESCOPE_JOB, 2, (const void *[]){scope, syntax});
}

struct pg_synthesis_job *pg_synthesis_telescope_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *job = pg_synthesis_telescope(synthesis, scope, syntax);
	if (!job) return NULL;
	struct telescope_work *local = pg_synthesis_work_state(job, TELESCOPE_JOB);
	if (pg_synthesis_context_allocation_at(synthesis, &local->allocation, prefix, end, local->scope != NULL)) return NULL;
	return job;
}

/* Lexical discovery is available before Context admission. Both readiness
 * notification and readers use this cursor, not another structural owner. */
static const struct telescope_work *prepared_telescope(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = pg_synthesis_work_state(job, TELESCOPE_JOB);
	if (!local || !local->scope || !local->tail) return NULL;
	const struct pg_syntax *syntax = job->inputs[1];
	if (local->tail->kind == syntax->kind &&
		(local->tail->kind == PG_SYNTAX_LAMBDA || local->tail->kind == PG_SYNTAX_PI)) return NULL;
	if (local->allocation && local->allocation->next != local->allocation->count) return NULL;
	return local;
}

static struct pg_synthesis_projection telescope_projection(const struct pg_synthesis_job *job)
{
	return (struct pg_synthesis_projection){.value_kind = -1,
		.preparing = job->status == PG_SYNTHESIS_PENDING && !prepared_telescope(job)};
}

static struct pg_synthesis_input telescope_output(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = prepared_telescope(job);
	return local ? local->scope->context : (struct pg_synthesis_input){0};
}

const struct pg_source_scope *pg_synthesis_telescope_scope(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = prepared_telescope(job);
	return local ? local->scope : NULL;
}

const struct pg_syntax *pg_synthesis_telescope_body(const struct pg_synthesis_job *job)
{
	const struct telescope_work *local = prepared_telescope(job);
	return local ? local->tail : NULL;
}

static void telescope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = job->inputs[0];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	const struct pg_syntax *syntax = job->inputs[1];
	struct telescope_work *local = pg_synthesis_work_state(job, TELESCOPE_JOB);
	if (!local->scope) { local->scope = scope; local->tail = syntax; }
	if (local->tail->kind != syntax->kind ||
		(local->tail->kind != PG_SYNTAX_LAMBDA && local->tail->kind != PG_SYNTAX_PI)) {
		if (local->allocation && local->allocation->next != local->allocation->count) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_synthesis_await_input(synthesis, job, local->scope->context)) return;
		pg_synthesis_finish(synthesis, job, pg_synthesis_scope_context(local->scope) ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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

struct pg_synthesis_structure pg_synthesis_declared_type(struct pg_synthesis *synthesis,
	struct pg_synthesis_input context, const struct pg_object *binder)
{
	if (!binder) return (struct pg_synthesis_structure){0};
	for (;;) {
		if (!pg_synthesis_input_owned(synthesis, context)) return (struct pg_synthesis_structure){0};
		const struct pg_evidence *proof = pg_synthesis_input_result(context);
		if (proof) {
			const struct pg_context *declaration = pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT
				? pg_context_lookup(pg_evidence_context(proof), binder) : NULL;
			return (struct pg_synthesis_structure){.term = declaration ? declaration->declared_type : NULL};
		}
		struct pg_synthesis_job *producer = pg_pending_job(context.pending);
		const struct pg_derivation_input *input = pg_synthesis_plain_derivation(producer);
		if (!input || input->rule != PG_CONTEXT_EXTEND || input->count != 2) break;
		if (input->parameters.binder == binder)
			return pg_synthesis_type_structure_input(synthesis, pg_synthesis_rule_input(synthesis, producer, 1));
		context = pg_synthesis_rule_input(synthesis, producer, 0);
	}
	return (struct pg_synthesis_structure){.pending = pg_synthesis_work_request(synthesis, DECLARED_TYPE_JOB, 3,
		(const void *[]){context.checked, context.pending, binder})};
}

const struct pg_synthesis_work_class *pg_synthesis_binding_type_class(const struct pg_synthesis_job *input)
{
	return input && pg_synthesis_work_role(input) == DOMAIN_JOB ? DOMAIN_STRUCTURE_JOB : NULL;
}

static const struct pg_term *declaration_result(const struct pg_synthesis_job *job)
{
	const struct declaration_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	return local->result;
}

static void declaration_forward(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (pg_synthesis_await_structure(synthesis, job, local->input)) return;
	local->result = pg_synthesis_type_structure_result(local->input);
	pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void domain_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, DOMAIN_STRUCTURE_JOB);
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	const struct pg_evidence *proof = pg_synthesis_input_result(dependency);
	if (!proof) {
		if (!pg_synthesis_structure_valid(local->input)) {
			struct pg_synthesis_job *annotation = (void *)producer->inputs[1], *rule;
			if (pg_synthesis_await_preparation(synthesis, job, annotation, &rule)) return;
			if (pg_synthesis_input_value_kind((struct pg_synthesis_input){.pending = pg_synthesis_pending(annotation)}) == 2)
				local->input = pg_synthesis_type_structure(synthesis, annotation);
			else {
				const struct pg_derivation_input *input = pg_synthesis_plain_derivation(rule);
				if (input && input->rule == PG_VARIABLE) local->input = pg_synthesis_term_structure(synthesis, rule);
			}
		}
		if (pg_synthesis_structure_valid(local->input)) { declaration_forward(synthesis, job); return; }
		if (pg_synthesis_await_input(synthesis, job, dependency)) return;
		proof = pg_synthesis_input_result(dependency);
	}
	enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
	if (kind == PG_JUDGEMENT_VALUE_TYPE || kind == PG_JUDGEMENT_COMPUTATION_TYPE)
		local->result = pg_evidence_subject(proof)->core;
	pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void declared_type_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct declaration_work *local = pg_synthesis_work_state(job, DECLARED_TYPE_JOB);
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *context = pg_pending_job(dependency.pending);
	const struct pg_object *binder = job->inputs[2];
	const struct pg_evidence *accepted = pg_synthesis_input_result(dependency);
	if (accepted) {
		const struct pg_context *declaration = pg_evidence_judgement(accepted) == PG_JUDGEMENT_CONTEXT
			? pg_context_lookup(pg_evidence_context(accepted), binder) : NULL;
		local->result = declaration ? declaration->declared_type : NULL;
		pg_synthesis_finish(synthesis, job, declaration ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	const struct binding_work *binding = pg_synthesis_work_state(context, BINDING_JOB);
	if (!pg_synthesis_structure_valid(local->input)) {
		struct pg_synthesis_job *prepared;
		if (pg_synthesis_await_preparation(synthesis, job, context, &prepared)) return;
		struct pg_synthesis_input input = prepared ? (struct pg_synthesis_input){.pending = pg_synthesis_pending(prepared)}
			: pg_synthesis_scope_context_input(context);
		if (input.checked || input.pending) local->input = pg_synthesis_declared_type(synthesis, input, binder);
		else if (binding) {
			const struct pg_source_scope *scope = context->inputs[0];
			local->input = binding->scope && binding->scope->binder == binder ? pg_synthesis_type_structure(synthesis, binding->domain)
				: pg_synthesis_declared_type(synthesis, scope->context, binder);
		}
	}
	if (pg_synthesis_structure_valid(local->input)) {
		if (pg_synthesis_await_structure(synthesis, job, local->input)) return;
		local->result = pg_synthesis_type_structure_result(local->input);
		if (binding && binding->scope && binding->scope->binder == binder && family_parameter_annotation(local->result))
			local->result = family_parameter_structure(synthesis->typing->graph, local->result);
		pg_synthesis_finish(synthesis, job, local->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (pg_synthesis_await(synthesis, job, context)) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}
