#include "synthesis_handler.h"
#include "synthesis_source.h"
#include "synthesis_effect.h"
#include "effect_inference.h"
#include "derivation.h"

#include <string.h>

struct source_handler_clause {
	const struct pg_synthesis_job *operation;
	struct pg_synthesis_job *body;
};
struct handler_state {
	struct pg_synthesis_job *source;
	size_t scanned, count;
	struct pg_effect_inference effects;
	struct pg_effect_equation *equation;
	struct pg_synthesis_job *carrier, *collection;
	const struct pg_effect_row *handled;
	const struct pg_object **labels;
	size_t collected;
	struct handler_state *effect_owner;
	const struct pg_source_scope *scope;
	/* Only the owner counts handlers whose equation edges are incomplete. */
	size_t registering;
	int registered;
	enum pg_synthesis_status failure;
	struct source_handler_clause clauses[];
};

struct return_work {
	const struct pg_source_scope *inner;
	const struct pg_object *binder;
	struct pg_synthesis_job *left, *value_job;
};
struct clause_work {
	const struct pg_source_scope *inner;
	struct pg_synthesis_job *left, *value_job, *handler_owner;
	struct pg_handler_clause_input *handler_clause;
};
struct handler_work {
	struct pg_synthesis_job *left, *right, *value_job;
	struct handler_state *handler;
};

static void handler_return_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void handler_clause_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void handler_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void handler_destroy(struct pg_synthesis_job *);
static void handler_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static struct pg_synthesis_projection handler_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class HANDLER_RETURN_JOB[1] = {{
	.size = sizeof(struct return_work), .advance = handler_return_step, .project = handler_project}};
static const struct pg_synthesis_work_class HANDLER_CLAUSE_JOB[1] = {{
	.size = sizeof(struct clause_work), .advance = handler_clause_step, .project = handler_project}};
static const struct pg_synthesis_work_class HANDLER_JOB[1] = {{
	.size = sizeof(struct handler_work), .advance = handler_step, .project = handler_project,
	.destroy = handler_destroy, .completed = handler_completed}};

static const struct pg_source_scope *input_scope(const struct pg_synthesis_job *job)
{
	return job->inputs[0];
}

static const struct pg_syntax *input_syntax(const struct pg_synthesis_job *job)
{
	return job->inputs[2];
}

static struct return_work *return_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, HANDLER_RETURN_JOB);
}

static struct clause_work *clause_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, HANDLER_CLAUSE_JOB);
}

static struct handler_work *handler_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, HANDLER_JOB);
}

static struct pg_synthesis_projection handler_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = job->role == HANDLER_RETURN_JOB ? return_work(job)->value_job
		: job->role == HANDLER_CLAUSE_JOB ? clause_work(job)->value_job : handler_work(job)->value_job;
	return (struct pg_synthesis_projection){rule, !rule, job->role == HANDLER_JOB ? 0 : -1};
}

static void handler_destroy(struct pg_synthesis_job *job)
{
	struct handler_state *state = handler_work(job)->handler;
	if (state && state->effect_owner == state) pg_effect_inference_destroy(&state->effects);
}

static void handler_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int first)
{
	(void)first;
	struct handler_state *state = handler_work(job)->handler;
	if (job->status <= PG_SYNTHESIS_DONE || !state || !state->effect_owner) return;
	struct handler_state *owner = state->effect_owner;
	if (owner->effects.sealed || owner->failure) return;
	owner->failure = job->status;
	owner->effects.failed = 1;
	pg_synthesis_effect_inference(synthesis, &owner->effects);
}

const struct pg_source_scope *pg_synthesis_handler_environment(const struct pg_synthesis_job *job)
{
	const struct handler_work *local = handler_work(job);
	return local && local->handler ? local->handler->scope : NULL;
}

int pg_synthesis_handler_environment_input(const struct pg_source_scope *scope, struct pg_source_environment *input)
{
	if (!scope->effect_owner || scope->effect_owner->scope != scope) return 0;
	*input = (struct pg_source_environment){.parent = scope->parent,
		.handler = scope->effect_owner->source->inputs[2]};
	return 1;
}

struct pg_synthesis_job *pg_synthesis_return_handler(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *input = pg_synthesis_request(synthesis, scope, syntax->left);
	struct pg_synthesis_job *returned = pg_synthesis_handler_return(synthesis, scope, input, syntax->items[0].expression);
	return returned ? pg_synthesis_sequence(synthesis, scope->context_job, return_work(returned)->left, returned) : NULL;
}

struct pg_synthesis_job *pg_synthesis_handler_return(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *input,
	const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, input, clause};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, HANDLER_RETURN_JOB, 3, inputs);
	if (job && !return_work(job)->left) {
		struct pg_synthesis_job *body = pg_synthesis_body(synthesis, input, NULL);
		if (!body) return NULL;
		struct pg_synthesis_job *premises[] = {scope->context_job, body};
		return_work(job)->left = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		if (!return_work(job)->left) return NULL;
	}
	return job;
}

struct pg_synthesis_job *pg_synthesis_handler_clause(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!carrier || carrier->owner != synthesis->owner_key) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, carrier, clause};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, HANDLER_CLAUSE_JOB, 3, inputs);
	return job;
}

struct pg_synthesis_job *pg_synthesis_handler_carrier(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *returned,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation)
{
	if (!equation) return NULL;
	struct pg_synthesis_job *result = pg_synthesis_constant_result(synthesis, context, returned, 1);
	result = pg_synthesis_plain_rule(synthesis, PG_RETURN_CONTENT, NULL, 1, &result);
	struct pg_derivation_input carrier = {.rule = PG_RETURN_TYPE_FORM, .count = 1};
	return pg_synthesis_rule(synthesis, &carrier, &result, work, equation);
}

struct pg_synthesis_job *pg_synthesis_handler(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (carrier && carrier->owner != synthesis->owner_key) return NULL;
	if (!syntax || syntax->kind != PG_SYNTAX_ELIMINATION) return NULL;
	const void *inputs[] = {scope, carrier, syntax};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, HANDLER_JOB, 3, inputs);
	return job;
}

int pg_synthesis_return_clause(const struct pg_syntax *clause)
{
	if (!clause || !clause->left) return 0;
	const struct pg_syntax *head = clause->left;
	if (head->kind != PG_SYNTAX_QUALIFIED) return 0;
	if (!head->left || !head->right) return 0;
	if (head->left->kind != PG_SYNTAX_ATOM) return 0;
	struct pg_token root = head->left->token, name = head->right->token;
	return root.kind == '#' &&
		name.length == 6 && !memcmp(name.text, "return", 6);
}

int pg_synthesis_handler_syntax(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_ELIMINATION || !syntax->item_count) return 0;
	for (size_t i = 0; i < syntax->item_count; ++i)
		if (pg_synthesis_return_clause(syntax->items[i].expression)) return 1;
	return 0;
}

static void handler_return_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct return_work *local = return_work(job);
	const struct pg_source_scope *source_scope = input_scope(job);
	if (pg_synthesis_scope_wait(synthesis, job, source_scope)) return;
	const struct pg_syntax *clause = input_syntax(job);
	if (!pg_synthesis_return_clause(clause)) goto rejected;
	if (clause->item_count != 1) goto rejected;
	if (clause->items[0].operation) goto rejected;
	if (!local->inner) {
		if (!local->binder) local->binder = pg_synthesis_source_binder(synthesis, source_scope, clause, 0, NULL);
		if (!local->binder) goto error;
		struct pg_synthesis_job *context = pg_synthesis_result_context(synthesis,
			source_scope->context_job, local->left, local->binder);
		if (!context) goto error;
		local->inner = pg_synthesis_bind_context(synthesis, source_scope, clause->items[0].name,
			local->binder, context);
		if (!local->inner) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_synthesis_job *body = pg_synthesis_request(synthesis, local->inner, clause->right);
		local->value_job = pg_synthesis_lambda_body(synthesis,
			context, body);
	}
	if (!local->value_job) goto error;
	if (pg_synthesis_await(synthesis, job, local->value_job)) return;
	job->result = local->value_job->result;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static struct pg_synthesis_job *handler_rule(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *body, struct pg_synthesis_job *returned,
	struct pg_synthesis_job *carrier, size_t count, const struct source_handler_clause *clauses)
{
	body = pg_synthesis_body(synthesis, body, NULL);
	if (!count) {
		struct pg_synthesis_job *fold = pg_synthesis_plain_rule(synthesis, PG_FOLD_ELIM, NULL, 2,
			(struct pg_synthesis_job *[]){body, returned});
		return pg_synthesis_plain_rule(synthesis, PG_EFFECT_SUBSUMPTION, NULL, 2,
			(struct pg_synthesis_job *[]){fold, carrier});
	}
	if (count > (SIZE_MAX / sizeof(struct pg_synthesis_job *) - 3) / 3) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **premises = pg_alloc(&temporary, (3 + 3 * count) * sizeof(*premises));
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	struct pg_synthesis_job *result = NULL;
	if (!premises || !labels) goto done;
	premises[0] = body; premises[1] = returned; premises[2] = carrier;
	for (size_t i = 0; i < count; ++i) {
		struct pg_operation_input signature;
		if (pg_synthesis_operation_input(synthesis, clauses[i].operation, &signature)) goto done;
		labels[i] = signature.label;
		premises[3 + 3 * i] = signature.payload;
		premises[4 + 3 * i] = signature.response;
		premises[5 + 3 * i] = clauses[i].body;
	}
	struct pg_derivation_input input = {.rule = PG_HANDLER_ELIM, .count = 3 + 3 * count,
		.parameters.handler = pg_handler_signature(synthesis->typing->graph, count, labels)};
	if (input.parameters.handler) result = pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_synthesis_job *pg_synthesis_handler_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *carrier,
	struct pg_synthesis_job *payload_type, struct pg_synthesis_job *response_type, const struct pg_object *payload,
	const struct pg_object *resume, const struct pg_object *response)
{
	if (!synthesis) return NULL;
	struct pg_synthesis_job *inputs[] = {context, carrier, payload_type, response_type};
	for (size_t i = 0; i < 4; ++i)
		if (!inputs[i] || inputs[i]->owner != synthesis->owner_key) return NULL;
	const struct pg_object *binders[] = {payload, resume, response};
	for (size_t i = 0; i < 3; ++i)
		if (!binders[i] || binders[i]->kind != PG_BINDER) return NULL;
	struct pg_synthesis_job *a = payload_type, *b = response_type;
	b = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, b});
	struct pg_synthesis_job *response_context = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND,
		response, 2, (struct pg_synthesis_job *[]){context, b});
	struct pg_synthesis_job *codomain = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
		(struct pg_synthesis_job *[]){response_context, carrier});
	struct pg_synthesis_job *pi = pg_synthesis_plain_rule(synthesis, PG_PI_FORM, NULL, 2,
		(struct pg_synthesis_job *[]){response_context, codomain});
	struct pg_synthesis_job *delayed = pg_synthesis_plain_rule(synthesis, PG_THUNK_TYPE_FORM, NULL, 1, &pi);
	a = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, a});
	struct pg_synthesis_job *payload_context = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, payload, 2,
		(struct pg_synthesis_job *[]){context, a});
	delayed = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
		(struct pg_synthesis_job *[]){payload_context, delayed});
	return pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, resume, 2,
		(struct pg_synthesis_job *[]){payload_context, delayed});
}

static int prepare_handler_clause(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_handler_clause_input *input)
{
	struct clause_work *local = clause_work(job);
	const struct pg_source_scope *source_scope = input_scope(job);
	const struct pg_syntax *clause = input_syntax(job);
	if (clause->item_count != 2 || pg_synthesis_return_clause(clause)) return PG_SYNTHESIS_REJECTED;
	if (clause->items[0].operation || clause->items[1].operation) return PG_SYNTHESIS_REJECTED;
	struct pg_operation_input signature;
	if (!input || pg_synthesis_operation_input(synthesis, input->operation, &signature)) return PG_SYNTHESIS_REJECTED;
	if (!input->payload || input->payload->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (!input->resume || input->resume->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (!input->response || input->response->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (local->handler_clause) {
		const struct pg_handler_clause_input *previous = local->handler_clause;
		if (previous->operation != input->operation || previous->payload != input->payload ||
			previous->resume != input->resume || previous->response != input->response) return PG_SYNTHESIS_REJECTED;
		if (local->inner) return 0;
	} else {
		if (job->status != PG_SYNTHESIS_PENDING) return PG_SYNTHESIS_REJECTED;
		local->handler_clause = pg_alloc(synthesis->typing->graph, sizeof(*input));
		if (!local->handler_clause) return PG_SYNTHESIS_ERROR;
		*local->handler_clause = *input;
	}
	struct pg_synthesis_job *context = pg_synthesis_handler_context(synthesis, source_scope->context_job,
		(void *)job->inputs[1], signature.payload, signature.response, input->payload, input->resume, input->response);
	if (!context) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis_job *payload_context = pg_synthesis_rule_premise(synthesis, context, 0);
	const struct pg_source_scope *scope = pg_synthesis_scope_bind(synthesis, source_scope,
		clause->items[0].name, input->payload, payload_context, NULL, job, PG_SOURCE_UNASSOCIATED);
	local->inner = pg_synthesis_scope_bind(synthesis, scope, clause->items[1].name, input->resume, context, NULL, job, PG_SOURCE_UNASSOCIATED);
	if (!local->inner) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis_job *body = pg_synthesis_request(synthesis, local->inner, clause->right);
	struct pg_synthesis_job *inner_lambda = pg_synthesis_lambda_body(synthesis, context, body);
	local->value_job = pg_synthesis_lambda_body(synthesis, payload_context, inner_lambda);
	return local->value_job ? 0 : PG_SYNTHESIS_ERROR;
}

struct pg_synthesis_job *pg_synthesis_handler_clause_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *clause, const struct pg_handler_clause_input *input)
{
	struct pg_synthesis_job *job = pg_synthesis_handler_clause(synthesis, scope, carrier, clause);
	return job && !prepare_handler_clause(synthesis, job, input) ? job : NULL;
}

const struct pg_source_scope *pg_synthesis_handler_clause_scope(const struct pg_synthesis_job *job)
{
	const struct clause_work *local = clause_work(job);
	return job && job->role == HANDLER_CLAUSE_JOB ? local->inner : NULL;
}

int pg_synthesis_handler_clause_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_handler_clause_input *input)
{
	const struct clause_work *local = clause_work(job);
	if (!synthesis || !job || !input || job->owner != synthesis->owner_key) return -1;
	if (job->role != HANDLER_CLAUSE_JOB || !local->handler_clause) return -1;
	*input = *local->handler_clause;
	return 0;
}

static void handler_clause_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct clause_work *local = clause_work(job);
	const struct pg_source_scope *source_scope = input_scope(job);
	if (pg_synthesis_scope_wait(synthesis, job, source_scope)) return;
	const struct pg_syntax *clause = input_syntax(job);
	if (clause->item_count != 2 || pg_synthesis_return_clause(clause)) goto rejected;
	if (clause->items[0].operation || clause->items[1].operation) goto rejected;
	if (!local->left) {
		local->left = pg_synthesis_operation_reference(synthesis,
			pg_synthesis_request(synthesis, source_scope, clause->left));
		if (!local->left) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	{
		struct pg_synthesis_job *operation = (void *)pg_synthesis_operation_origin(local->left);
		if (!operation) {
			if (local->left->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, local->left, 0); return; }
			pg_synthesis_finish(synthesis, job, local->left->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : local->left->status); return;
		}
		struct pg_handler_clause_input input = {.operation = operation};
		if (local->handler_clause) {
			input = *local->handler_clause;
			input.operation = operation;
		} else {
			input.payload = pg_synthesis_source_binder(synthesis, source_scope, clause, 0, NULL);
			input.resume = pg_synthesis_source_binder(synthesis, source_scope, clause, 1, NULL);
			input.response = pg_synthesis_source_binder(synthesis, source_scope, clause, 2, NULL);
		}
		if (!input.payload || !input.resume || !input.response) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		int status = prepare_handler_clause(synthesis, job, &input);
		if (status) { pg_synthesis_finish(synthesis, job, status); return; }
	}
	if (pg_synthesis_await(synthesis, job, local->value_job)) return;
	if (pg_synthesis_await(synthesis, job, local->left)) return;
	const struct pg_effect_row *effects;
	const struct pg_term *value;
	if (!carrier->result || pg_evidence_judgement(carrier->result) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
	if (pg_evidence_context(carrier->result) != pg_evidence_context(pg_synthesis_scope_context(source_scope))) goto rejected;
	if (!pg_effect_type_view(pg_evidence_subject(carrier->result)->core, &effects, &value)) goto rejected;
	job->result = local->value_job->result;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static int prepare_handler(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int independent)
{
	struct handler_work *local = handler_work(job);
	const struct pg_source_scope *source_scope = input_scope(job);
	const struct pg_syntax *source_syntax = input_syntax(job);
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	size_t count = source_syntax->item_count;
	if (local->handler) return !independent || local->handler->effect_owner == local->handler ? 0 : -1;
	if (count > (SIZE_MAX - sizeof(*local->handler)) / sizeof(struct source_handler_clause)) return -1;
	struct handler_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state) + count * sizeof(struct source_handler_clause));
	if (!state) return -1;
	state->source = job;
	state->scope = source_scope;
	if (!carrier) {
		state->labels = pg_alloc(synthesis->typing->graph, count * sizeof(*state->labels));
		if (!state->labels) return -1;
		struct handler_state *owner = source_scope->effect_owner;
		if (independent || !owner || owner->effects.sealed) {
			owner = state;
			if (pg_effect_inference_init(&owner->effects, synthesis->typing->graph)) return -1;
		}
		state->effect_owner = owner;
		if (source_scope->effect_owner != owner)
			state->scope = pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){
				.parent = source_scope, .context_job = source_scope->context_job, .effect_owner = owner});
		if (!state->scope) {
			if (owner == state) pg_effect_inference_destroy(&state->effects);
			return -1;
		}
		++owner->registering;
	}
	local->handler = state;
	return 0;
}

const struct pg_source_scope *pg_synthesis_handler_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *job = pg_synthesis_handler(synthesis, parent, NULL, syntax);
	if (!job || prepare_handler(synthesis, job, 1)) return NULL;
	return handler_work(job)->handler->scope;
}

/* Reserve dependencies, not answers. The return body and effect equation are
 * the same producers subsequently advanced by the ordinary handler worker. */
static int prepare_handler_carrier(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct handler_work *local = handler_work(job);
	const struct pg_syntax *source_syntax = input_syntax(job);
	struct handler_state *state = local->handler;
	if (!local->right) {
		size_t selected = SIZE_MAX;
		for (size_t i = 0; i < source_syntax->item_count; ++i) {
			if (!pg_synthesis_return_clause(source_syntax->items[i].expression)) continue;
			if (selected != SIZE_MAX) return PG_SYNTHESIS_REJECTED;
			selected = i;
		}
		if (selected == SIZE_MAX) return PG_SYNTHESIS_REJECTED;
		local->left = pg_synthesis_request(synthesis, state->scope, source_syntax->left);
		if (!local->left) return PG_SYNTHESIS_ERROR;
		local->right = pg_synthesis_handler_return(synthesis, state->scope, local->left,
			source_syntax->items[selected].expression);
		if (!local->right) return PG_SYNTHESIS_ERROR;
	}
	if (!job->inputs[1] && !state->carrier) {
		struct handler_state *owner = state->effect_owner;
		const struct pg_effect_row *empty = pg_effect_row(synthesis->typing->graph, 0, NULL);
		if (!state->equation) state->equation = pg_effect_equation(&owner->effects, empty);
		if (!state->equation) return PG_SYNTHESIS_ERROR;
		state->carrier = pg_synthesis_handler_carrier(synthesis, state->scope->context_job, local->right,
			&owner->effects, state->equation);
		if (!state->carrier) return PG_SYNTHESIS_ERROR;
	}
	return 0;
}

struct pg_synthesis_job *pg_synthesis_source_handler_carrier(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *job = pg_synthesis_handler(synthesis, parent, NULL, syntax);
	if (!job || prepare_handler(synthesis, job, 0) || prepare_handler_carrier(synthesis, job)) return NULL;
	return handler_work(job)->handler->carrier;
}

static struct pg_synthesis_job *source_handler_clause_job(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *handler, struct pg_synthesis_job *carrier, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *clause = pg_synthesis_handler_clause(synthesis, handler_work(handler)->handler->scope, carrier, syntax);
	if (!clause || (clause_work(clause)->handler_owner && clause_work(clause)->handler_owner != handler)) return NULL;
	clause_work(clause)->handler_owner = handler;
	return clause;
}

int pg_synthesis_handler_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_handler_binding_input *input)
{
	if (!synthesis || !scope || !input || scope->owner != synthesis->owner_key) return -1;
	const struct pg_synthesis_job *clause = scope->clause;
	if (!clause || !clause_work(clause)->handler_owner || clause_work(clause)->handler_owner->inputs[1]) return 0;
	const struct pg_synthesis_job *handler = clause_work(clause)->handler_owner;
	if (!clause_work(clause)->inner || !clause_work(clause)->handler_clause) return -1;
	unsigned slot;
	if (scope == clause_work(clause)->inner) slot = 1;
	else if (scope == clause_work(clause)->inner->parent) slot = 0;
	else return -1;
	*input = (struct pg_handler_binding_input){.parent = input_scope(handler), .handler = input_syntax(handler),
		.clause = input_syntax(clause), .allocation = *clause_work(clause)->handler_clause, .slot = slot};
	return 1;
}

const struct pg_source_scope *pg_synthesis_restore_handler_binding(struct pg_synthesis *synthesis,
	const struct pg_handler_binding_input *input)
{
	if (!input || input->slot > 1 || !input->handler || !input->clause) return NULL;
	if (input->handler->kind != PG_SYNTAX_ELIMINATION) return NULL;
	size_t index;
	for (index = 0; index < input->handler->item_count; ++index)
		if (input->handler->items[index].expression == input->clause) break;
	if (index == input->handler->item_count || pg_synthesis_return_clause(input->clause)) return NULL;
	struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(synthesis, input->parent, input->handler);
	if (!carrier) return NULL;
	struct pg_synthesis_job *handler = pg_synthesis_handler(synthesis, input->parent, NULL, input->handler);
	struct pg_synthesis_job *clause = source_handler_clause_job(synthesis, handler, carrier, input->clause);
	if (!clause) return NULL;
	if (prepare_handler_clause(synthesis, clause, &input->allocation)) return NULL;
	return input->slot ? clause_work(clause)->inner : clause_work(clause)->inner->parent;
}

static void handler_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct handler_work *local = handler_work(job);
	const struct pg_source_scope *source_scope = input_scope(job);
	const struct pg_syntax *source_syntax = input_syntax(job);
	if (pg_synthesis_scope_wait(synthesis, job, source_scope)) return;
	if (source_syntax->right) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	size_t count = source_syntax->item_count;
	if (prepare_handler(synthesis, job, 0)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	int preparation = prepare_handler_carrier(synthesis, job);
	if (preparation) { pg_synthesis_finish(synthesis, job, preparation); return; }
	struct handler_state *state = local->handler;
	const struct pg_source_scope *scope = state->scope;
	struct handler_state *owner = state->effect_owner;
	if (owner && owner->failure) { pg_synthesis_finish(synthesis, job, owner->failure); return; }
	if (state->scanned < count) {
		const struct pg_syntax *clause = source_syntax->items[state->scanned].expression;
		if (!pg_synthesis_return_clause(clause)) {
			struct pg_synthesis_job *operation = pg_synthesis_operation_reference(synthesis,
				pg_synthesis_request(synthesis, scope, clause->left));
			if (!operation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_operation_input signature;
			const struct pg_synthesis_job *origin = pg_synthesis_operation_origin(operation);
			if (pg_synthesis_operation_input(synthesis, origin, &signature)) {
				if (operation->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, operation, 0); return; }
				pg_synthesis_finish(synthesis, job, operation->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : operation->status);
				return;
			}
			struct pg_synthesis_job *body = source_handler_clause_job(synthesis, job,
				carrier ? carrier : state->carrier, clause);
			if (!body) { pg_synthesis_finish(synthesis, job, carrier ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_REJECTED); return; }
			if (!carrier) state->labels[state->count] = signature.label;
			state->clauses[state->count++] = (struct source_handler_clause){origin, body};
		}
		++state->scanned;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (!local->right) goto rejected;
	if (!carrier) {
		const struct pg_effect_row *empty = pg_effect_row(synthesis->typing->graph, 0, NULL);
		if (!state->handled) {
			state->handled = pg_effect_row(synthesis->typing->graph, state->count, state->labels);
			if (!state->handled) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (pg_effect_count(state->handled) != state->count) goto rejected;
		}
		carrier = state->carrier;
		if (state->collected < state->count + 2) {
			if (!state->collection) {
				struct pg_synthesis_job *body;
				size_t parameters;
				if (!state->collected) {
					body = pg_synthesis_body(synthesis, local->left, NULL);
					parameters = 0;
				} else if (state->collected == 1) { body = local->right; parameters = 1; }
				else {
					body = state->clauses[state->collected - 2].body;
					parameters = 2;
				}
				struct pg_synthesis_job *formation = pg_synthesis_constant_result(synthesis, scope->context_job, body, parameters);
				state->collection = pg_synthesis_effect_contribution(synthesis, &owner->effects, state->equation,
					state->collected ? empty : state->handled, formation);
			}
			if (pg_synthesis_await(synthesis, job, state->collection)) return;
			state->collection = NULL;
			++state->collected;
			pg_synthesis_enqueue(synthesis, job);
			return;
		}
		if (!state->registered) {
			state->registered = 1;
			if (!--owner->registering) {
				pg_effect_inference_seal(&owner->effects);
				if (!pg_synthesis_effect_inference(synthesis, &owner->effects)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			}
		}
	}
	if (!local->value_job) local->value_job = handler_rule(synthesis, local->left, local->right,
		carrier, state->count, state->clauses);
	pg_synthesis_forward(synthesis, job, local->value_job);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}
