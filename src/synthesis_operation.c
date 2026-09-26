#include "synthesis_source.h"
#include "derivation.h"

struct operation_work {
	struct pg_synthesis_job *rule;
	const struct pg_operation_declaration *declaration;
	const struct pg_context *allocation;
};
struct reference_work {
	struct pg_synthesis_job *dependency;
	const struct pg_synthesis_job *origin;
};

static void operation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void reference_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection operation_project(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class OPERATION_JOB[1] = {{
	.size = sizeof(struct operation_work), .advance = operation_step, .project = operation_project}};
static const struct pg_synthesis_work_class OPERATION_REFERENCE_JOB[1] = {{
	.size = sizeof(struct reference_work), .advance = reference_step}};

static struct operation_work *operation_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, OPERATION_JOB);
}

static struct reference_work *reference_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, OPERATION_REFERENCE_JOB);
}

static struct pg_synthesis_projection operation_project(const struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *rule = operation_work(job)->rule;
	return (struct pg_synthesis_projection){.rule = rule, .preparing = !rule, .value_kind = -1};
}

struct pg_synthesis_job *pg_synthesis_operation_jobs(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_job *payload, struct pg_synthesis_job *response)
{
	const struct pg_term *a, *b;
	if (!payload || !response || payload->owner != synthesis->owner_key || response->owner != synthesis->owner_key) return NULL;
	if (!pg_operation_label_types(label, &a, &b)) return NULL;
	const void *inputs[] = {label, payload, response};
	return pg_synthesis_work_request(synthesis, OPERATION_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_operation(struct pg_synthesis *synthesis,
	const struct pg_operation_declaration *declaration)
{
	if (!declaration) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_operation_jobs(synthesis, pg_operation_label(declaration),
		pg_synthesis_evidence(synthesis, pg_operation_payload_type(declaration)),
		pg_synthesis_evidence(synthesis, pg_operation_response_type(declaration)));
	if (job && pg_evidence_owned_by(pg_operation_payload_type(declaration), synthesis->typing)
		&& pg_evidence_owned_by(pg_operation_response_type(declaration), synthesis->typing)) operation_work(job)->declaration = declaration;
	return job;
}

int pg_synthesis_operation_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_operation_input *input)
{
	const struct operation_work *local = operation_work(job);
	if (!synthesis || !local || !input || job->owner != synthesis->owner_key) return -1;
	*input = (struct pg_operation_input){job->inputs[0], (void *)job->inputs[1], (void *)job->inputs[2], local->allocation};
	return 0;
}

struct pg_synthesis_job *pg_synthesis_operation_at(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_job *payload, struct pg_synthesis_job *response,
	const struct pg_context *allocation)
{
	struct pg_synthesis_job *job = pg_synthesis_operation_jobs(synthesis, label, payload, response);
	size_t count;
	if (!job || pg_context_extension_size(allocation, NULL, &count) || count != 2) return NULL;
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
		for (size_t i = 1; i < 3; ++i)
			if (pg_synthesis_await(synthesis, job, (void *)job->inputs[i])) return;
		const struct pg_operation_declaration *operation = pg_operation_declaration_at(synthesis->typing,
			job->inputs[0], pg_synthesis_result(job->inputs[1]), pg_synthesis_result(job->inputs[2]));
		if (!operation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->declaration = operation;
		struct pg_synthesis_job *payload = pg_synthesis_evidence(synthesis, pg_operation_payload_type(operation));
		struct pg_synthesis_job *response = pg_synthesis_evidence(synthesis, pg_operation_response_type(operation));
		struct pg_synthesis_job *empty = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EMPTY, NULL, 0, NULL);
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
		struct pg_synthesis_job *scope = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, a, 2,
			(struct pg_synthesis_job *[]){empty, payload});
		struct pg_synthesis_job *domain = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){scope, response});
		struct pg_synthesis_job *response_scope = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND, b, 2,
			(struct pg_synthesis_job *[]){scope, domain});
		struct pg_synthesis_job *value = pg_synthesis_plain_rule(synthesis, PG_VARIABLE, b, 1, &response_scope);
		struct pg_derivation_input return_rule = {.rule = PG_RETURN_INTRO, .count = 1, .parameters.totality = PG_TOTALITY_TOTAL};
		struct pg_synthesis_job *returned = pg_synthesis_rule(synthesis, &return_rule, &value, NULL, NULL);
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, response_scope, returned);
		struct pg_synthesis_job *argument = pg_synthesis_plain_rule(synthesis, PG_VARIABLE, a, 1, &scope);
		struct pg_derivation_input input = {.rule = PG_REQUEST_INTRO, .count = 4, .parameters.operation_label = pg_operation_label(operation)};
		struct pg_synthesis_job *request = pg_synthesis_rule(synthesis, &input,
			(struct pg_synthesis_job *[]){payload, response, argument, continuation}, NULL, NULL);
		local->rule = pg_synthesis_lambda_body(synthesis, scope, request);
	}
	pg_synthesis_forward(synthesis, job, local->rule);
}

struct pg_synthesis_job *pg_synthesis_operation_reference(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer)
{
	if (!producer || producer->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {producer};
	return pg_synthesis_work_request(synthesis, OPERATION_REFERENCE_JOB, 1, inputs);
}

static const struct pg_synthesis_job *operation_origin(const struct pg_synthesis_job *producer)
{
	const struct reference_work *local = reference_work(producer);
	if (local) return local->origin ? local->origin : producer->inputs[0];
	return pg_synthesis_source_origin(producer);
}

const struct pg_synthesis_job *pg_synthesis_operation_origin(const struct pg_synthesis_job *job)
{
	if (!reference_work(job)) return NULL;
	if (job->status != PG_SYNTHESIS_PENDING && job->status != PG_SYNTHESIS_DONE) return NULL;
	const struct pg_synthesis_job *slow = job, *fast = job;
	while (slow) {
		if (slow->status != PG_SYNTHESIS_PENDING && slow->status != PG_SYNTHESIS_DONE) return NULL;
		if (operation_work(slow)) return slow;
		slow = operation_origin(slow);
		fast = operation_origin(operation_origin(fast));
		if (slow && slow == fast && !operation_work(slow)) return NULL;
	}
	return NULL;
}

const struct pg_operation_declaration *pg_synthesis_operation_declaration(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *origin = pg_synthesis_operation_origin(job);
	return origin ? operation_work(origin)->declaration : NULL;
}

int pg_synthesis_operation_reference_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *reference, struct pg_operation_input *input)
{
	if (!synthesis || !reference || reference->owner != synthesis->owner_key) return -1;
	return pg_synthesis_operation_input(synthesis, pg_synthesis_operation_origin(reference), input);
}

static void reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct reference_work *local = reference_work(job);
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (pg_synthesis_await(synthesis, job, producer)) return;
	if (operation_work(producer)) {
		local->origin = producer;
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	if (!local->dependency) {
		const struct pg_synthesis_job *source = operation_origin(producer);
		if (!source) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		local->dependency = pg_synthesis_operation_reference(synthesis, (void *)source);
		pg_synthesis_subscribe(synthesis, job, local->dependency, 0);
		return;
	}
	local->origin = reference_work(local->dependency)->origin;
	pg_synthesis_finish(synthesis, job, local->dependency->status);
}
