#include "synthesis_source.h"
#include "derivation.h"

struct operation_work {
	struct pg_synthesis_job *rule;
	const struct pg_operation_declaration *declaration;
	const struct pg_context *allocation;
};
static void operation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection operation_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class OPERATION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct operation_work), .advance = operation_step, .project = operation_project, .output = pg_synthesis_projected_output}};
static struct operation_work *operation_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, OPERATION_JOB);
}

static struct pg_synthesis_projection operation_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = operation_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = rule, .preparing = !rule, .value_kind = -1};
}

struct pg_synthesis_job *pg_synthesis_operation_request(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_input payload, struct pg_synthesis_input response)
{
	const struct pg_term *a, *b;
	if (!pg_synthesis_input_owned(synthesis, payload)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, response)) return NULL;
	if (!pg_operation_label_types(label, &a, &b)) return NULL;
	const void *inputs[] = {label, payload.checked, payload.pending, response.checked, response.pending};
	return pg_synthesis_work_request(synthesis, OPERATION_JOB, 5, inputs);
}

struct pg_synthesis_job *pg_synthesis_operation(struct pg_synthesis *synthesis,
	const struct pg_operation_declaration *declaration)
{
	if (!declaration) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_operation_request(synthesis, pg_operation_label(declaration),
		(struct pg_synthesis_input){.checked = pg_operation_payload_type(declaration)},
		(struct pg_synthesis_input){.checked = pg_operation_response_type(declaration)});
	if (job) operation_work(job)->declaration = declaration;
	return job;
}

int pg_synthesis_operation_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_operation_input *input)
{
	const struct operation_work *local = operation_work(job);
	if (!synthesis || !local || !input || job->pending.owner != synthesis->owner_key) return -1;
	*input = (struct pg_operation_input){job->inputs[0], pg_synthesis_work_dependency(job, 1),
		pg_synthesis_work_dependency(job, 3), local->allocation};
	return 0;
}

struct pg_synthesis_job *pg_synthesis_operation_at(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_input payload, struct pg_synthesis_input response,
	const struct pg_context *allocation)
{
	size_t count;
	if (pg_context_extension_size(allocation, NULL, &count) || count != 2) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_operation_request(synthesis, label, payload, response);
	if (!job) return NULL;
	struct operation_work *local = operation_work(job);
	if (local->allocation) return local->allocation == allocation ? job : NULL;
	if (local->rule) return NULL;
	local->allocation = allocation;
	return job;
}

static void operation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct operation_work *local = operation_work(job);
	if (!local->rule) {
		const struct pg_evidence *signature[2];
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_input input = pg_synthesis_work_dependency(job, 1 + 2 * i);
			if (pg_synthesis_await_input(synthesis, job, input)) return;
			signature[i] = pg_synthesis_input_result(input);
		}
		const struct pg_operation_declaration *operation = pg_operation_declaration_at(synthesis->typing,
			job->inputs[0], signature[0], signature[1]);
		if (!operation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->declaration = operation;
		struct pg_synthesis_input payload = {.checked = pg_operation_payload_type(operation)};
		struct pg_synthesis_input response = {.checked = pg_operation_response_type(operation)};
		struct pg_synthesis_job *empty = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_EMPTY, NULL, 0, NULL);
		if (!local->allocation) {
			const struct pg_context *allocation = pg_context_bind(synthesis->typing, NULL,
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_payload_type(operation))->core, PG_JUDGEMENT_VALUE);
			if (allocation) allocation = pg_context_bind(synthesis->typing, allocation,
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_response_type(operation))->core, PG_JUDGEMENT_VALUE);
			if (!allocation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			local->allocation = allocation;
		}
		/* Restore binding identities only; ordinary rules reconstruct the
		 * checked contexts from the signature, not the saved field types. */
		const struct pg_object *a = local->allocation->parent->binder;
		const struct pg_object *b = local->allocation->binder;
		struct pg_synthesis_job *scope = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_EXTEND, a, 2,
			(struct pg_synthesis_input[]){{.pending = pg_synthesis_pending(empty)}, payload});
		struct pg_synthesis_job *domain = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_input[]){{.pending = pg_synthesis_pending(scope)}, response});
		struct pg_synthesis_job *response_scope = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_EXTEND, b, 2, (struct pg_synthesis_input[]){
			{.pending = pg_synthesis_pending(scope)},
			{.pending = pg_synthesis_pending(domain)}
		});
		struct pg_synthesis_job *value = pg_synthesis_plain_rule_inputs(synthesis, PG_VARIABLE, b, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(response_scope)});
		struct pg_derivation_input return_rule = {.rule = PG_RETURN_INTRO, .count = 1, .parameters.totality = PG_TOTALITY_TOTAL};
		struct pg_synthesis_job *returned = pg_synthesis_rule_inputs(synthesis, &return_rule, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(value)}, NULL, NULL);
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis,
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(response_scope)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(returned)});
		struct pg_synthesis_job *argument = pg_synthesis_plain_rule_inputs(synthesis, PG_VARIABLE, a, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(scope)});
		struct pg_derivation_input input = {.rule = PG_REQUEST_INTRO, .count = 4, .parameters.operation_label = pg_operation_label(operation)};
		struct pg_synthesis_job *request = pg_synthesis_rule_inputs(synthesis, &input,
			(struct pg_synthesis_input[]){payload, response, {.pending = pg_synthesis_pending(argument)}, {.pending = pg_synthesis_pending(continuation)}}, NULL, NULL);
		local->rule = pg_synthesis_lambda_body(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(scope)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(request)});
	}
	pg_synthesis_forward(synthesis, job, local->rule);
}

const struct pg_synthesis_job *pg_synthesis_operation_origin(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *slow = job, *fast = job;
	while (slow) {
		if (slow->status != PG_SYNTHESIS_PENDING && slow->status != PG_SYNTHESIS_DONE) return NULL;
		if (operation_work(slow)) return slow;
		slow = pg_synthesis_source_origin(slow);
		fast = pg_synthesis_source_origin(pg_synthesis_source_origin(fast));
		if (slow && slow == fast && !operation_work(slow)) return NULL;
	}
	return NULL;
}

const struct pg_operation_declaration *pg_synthesis_operation_declaration(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *origin = pg_synthesis_operation_origin(job);
	return origin ? operation_work(origin)->declaration : NULL;
}
