#include "synthesis_source.h"
#include "iadt.h"

#include <stdlib.h>

struct data_result_work { struct pg_synthesis_job *substitution; };

struct constructor_scope_work {
	struct pg_source_context_allocation *allocation;
	const struct pg_evidence **fields;
	size_t count, next;
	struct pg_synthesis_job *pending;
};
struct induction_scope_work {
	struct pg_source_context_allocation *allocation;
	struct pg_synthesis_job *fields, *pending;
	const struct pg_evidence *context;
	size_t next;
};

static void constructor_scope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void induction_scope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void data_result_step(struct pg_synthesis *, struct pg_synthesis_job *);
static const struct pg_synthesis_work_class DATA_RESULT_JOB[1] = {{
	.size = sizeof(struct data_result_work), .advance = data_result_step}};
static const struct pg_synthesis_work_class CONSTRUCTOR_SCOPE_JOB[1] = {{
	.size = sizeof(struct constructor_scope_work), .advance = constructor_scope_step}};
static const struct pg_synthesis_work_class INDUCTION_SCOPE_JOB[1] = {{
	.size = sizeof(struct induction_scope_work), .advance = induction_scope_step}};

struct pg_synthesis_job *pg_synthesis_constructor_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters)
{
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	if (!parameters || parameters->owner != synthesis->owner_key || !constructor) return NULL;
	const void *inputs[] = {formation, parameters, constructor};
	return pg_synthesis_work_request(synthesis, CONSTRUCTOR_SCOPE_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_constructor_scope_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *job = pg_synthesis_constructor_scope(synthesis, formation, constructor, parameters);
	struct constructor_scope_work *local = pg_synthesis_work_state(job, CONSTRUCTOR_SCOPE_JOB);
	if (!job || pg_synthesis_context_allocation_at(synthesis, &local->allocation, prefix, end, local->pending != NULL)) return NULL;
	return job;
}

struct pg_synthesis_job *pg_synthesis_induction_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, struct pg_synthesis_job *motive_context,
	struct pg_synthesis_job *motive)
{
	struct pg_synthesis_job *producers[] = {formation, parameters, motive_context, motive};
	for (size_t i = 0; i < 4; ++i)
		if (!producers[i] || producers[i]->owner != synthesis->owner_key) return NULL;
	if (!constructor) return NULL;
	const void *inputs[] = {formation, parameters, motive_context, motive, constructor};
	return pg_synthesis_work_request(synthesis, INDUCTION_SCOPE_JOB, 5, inputs);
}

struct pg_synthesis_job *pg_synthesis_induction_scope_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, struct pg_synthesis_job *motive_context,
	struct pg_synthesis_job *motive, const struct pg_context *fields, const struct pg_context *end)
{
	struct pg_synthesis_job *job = pg_synthesis_induction_scope(synthesis, formation, constructor, parameters, motive_context, motive);
	struct induction_scope_work *local = pg_synthesis_work_state(job, INDUCTION_SCOPE_JOB);
	if (!job || pg_synthesis_context_allocation_at(synthesis, &local->allocation, fields, end, local->context != NULL)) return NULL;
	return job;
}
struct pg_synthesis_job *pg_synthesis_data_result(struct pg_synthesis *synthesis,
	const struct pg_source_scope *fields, const struct pg_evidence *parameters,
	const struct pg_evidence *indices, const struct pg_syntax *result)
{
	if (!fields || fields->owner != synthesis->owner_key || !result) return NULL;
	if (!pg_evidence_owned_by(parameters, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(indices, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(parameters) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (pg_evidence_judgement(indices) != PG_JUDGEMENT_CONTEXT) return NULL;
	const void *inputs[] = {fields, parameters, indices, result};
	return pg_synthesis_work_request(synthesis, DATA_RESULT_JOB, 4, inputs);
}

static void data_result_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct data_result_work *local = pg_synthesis_work_state(job, DATA_RESULT_JOB);
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = job->inputs[3];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (local->substitution) { pg_synthesis_forward(synthesis, job, local->substitution); return; }
	if (pg_synthesis_await(synthesis, job, scope->context_job)) return;
	const struct pg_evidence *parameters = job->inputs[1], *indices = job->inputs[2];
	const struct pg_evidence *fields = pg_synthesis_scope_context(scope);
	if (!fields) goto error;
	const struct pg_context *prefix = pg_evidence_context(parameters);
	size_t index_count, parameter_count, field_count;
	if (pg_context_extension_size(pg_evidence_context(indices), prefix, &index_count)) goto rejected;
	if (pg_context_extension_size(pg_evidence_context(fields), prefix, &field_count)) goto rejected;
	if (pg_context_extension_size(prefix, NULL, &parameter_count)) goto error;
	const struct pg_syntax *head = syntax;
	size_t arguments = 0;
	while (head->kind == PG_SYNTAX_APPLICATION) { ++arguments; head = head->left; }
	if (head->kind != PG_SYNTAX_ATOM || head->token.kind != '*' || arguments != index_count) goto rejected;
	if (index_count > SIZE_MAX - parameter_count) goto error;
	size_t count = parameter_count + index_count;
	if (count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
	struct pg_synthesis_job **images = malloc(count * sizeof(*images));
	if (count && !images) goto error;
	const struct pg_evidence *extension = indices;
	head = syntax;
	for (size_t i = count; i; --i, extension = pg_evidence_premise(extension, 0)) {
		if (i > parameter_count) {
			images[i - 1] = pg_synthesis_request(synthesis, scope, head->right);
			head = head->left;
		} else {
			const struct pg_evidence *image = pg_prove_variable(synthesis->typing,
				fields, pg_evidence_context(extension)->binder);
			images[i - 1] = pg_synthesis_evidence(synthesis, image);
		}
	}
	local->substitution = pg_synthesis_substitution(synthesis, indices, fields, count, images);
	free(images);
	if (!local->substitution) goto error;
	pg_synthesis_forward(synthesis, job, local->substitution);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

int pg_synthesis_constructor_scope_allocation(const struct pg_synthesis_job *job,
	const struct pg_context **prefix, const struct pg_context **end)
{
	const struct constructor_scope_work *local = pg_synthesis_work_state(job, CONSTRUCTOR_SCOPE_JOB);
	if (!local || !local->allocation) return 0;
	*prefix = local->allocation->prefix;
	*end = local->allocation->end;
	return 1;
}

static void constructor_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct constructor_scope_work *local = pg_synthesis_work_state(job, CONSTRUCTOR_SCOPE_JOB);
	if (!local->pending) {
		const struct pg_evidence *proofs[2];
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *producer = (void *)job->inputs[i];
			if (pg_synthesis_await(synthesis, job, producer)) return;
			proofs[i] = producer->result;
		}
		const struct pg_evidence *formation = proofs[0], *parameters = proofs[1];
		if (!formation || !parameters) goto rejected;
		if (pg_evidence_rule(formation) != PG_INDUCTIVE_FORM || pg_evidence_rule(parameters) != PG_CONTEXT_SUBSTITUTION) goto rejected;
		if (pg_evidence_context_map(parameters)->source != pg_evidence_context(formation)) goto rejected;
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis, (void *)job->inputs[0]);
		if (!instance_job) goto error;
		if (pg_synthesis_await(synthesis, job, instance_job)) return;
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) goto error;
		const struct pg_evidence *fields = pg_data_schema_fields(instance.schema, job->inputs[2]);
		const struct pg_evidence *self = pg_evidence_premise(formation, 0);
		size_t count;
		if (!fields || pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(self), &count)) goto rejected;
		if (local->allocation) {
			const struct pg_source_context_allocation *allocation = local->allocation;
			if (allocation->count != count || !pg_context_same_allocation_shape(allocation->prefix,
				pg_evidence_context_map(parameters)->destination)) goto rejected;
		}
		if (count > SIZE_MAX / sizeof(*local->fields)) goto error;
		local->fields = pg_alloc(synthesis->typing->graph, count * sizeof(*local->fields));
		if (count && !local->fields) goto error;
		local->count = count;
		for (size_t i = count; i; --i, fields = pg_evidence_premise(fields, 0)) local->fields[i - 1] = fields;
		struct pg_synthesis_job *family = pg_synthesis_reindex_jobs(synthesis, (void *)job->inputs[1], (void *)job->inputs[0]);
		struct pg_synthesis_job *value = pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY
			? family : pg_synthesis_plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &family);
		struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(synthesis, self),
			pg_synthesis_evidence(synthesis, pg_evidence_premise(parameters, 1)), (void *)job->inputs[1], value};
		local->pending = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_SUBSTITUTION, NULL, 4, premises);
		if (!local->pending) goto error;
	}
	if (pg_synthesis_await(synthesis, job, local->pending)) return;
	const struct pg_evidence *map = local->pending->result;
	if (local->next == local->count) {
		job->result = map;
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	const struct pg_object *binder;
	if (local->allocation) binder = local->allocation->contexts[local->next]->binder;
	else {
		const struct pg_evidence *parameters = pg_synthesis_result(job->inputs[1]);
		binder = pg_synthesis_constructor_binder(synthesis,
			pg_evidence_context_map(parameters)->destination, job->inputs[2], local->next);
		if (!binder) goto rejected;
	}
	local->pending = pg_synthesis_substitution_lift(synthesis, map, local->fields[local->next++], binder);
	if (!local->pending) goto error;
	pg_synthesis_subscribe(synthesis, job, local->pending, 0);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void induction_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct induction_scope_work *local = pg_synthesis_work_state(job, INDUCTION_SCOPE_JOB);
	const struct pg_evidence *proofs[4];
	for (size_t i = 0; i < 4; ++i) {
		struct pg_synthesis_job *producer = (void *)job->inputs[i];
		if (pg_synthesis_await(synthesis, job, producer)) return;
		proofs[i] = producer->result;
		if (!proofs[i]) goto rejected;
	}
	const struct pg_evidence *formation = proofs[0], *parameters = proofs[1], *motive_context = proofs[2], *motive = proofs[3];
	if (!local->fields) {
		if (pg_evidence_rule(motive_context) != PG_CONTEXT_EXTEND) goto rejected;
		if (pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
		if (pg_evidence_context(motive) != pg_evidence_context(motive_context)) goto rejected;
		if (!pg_inductive_motive_context_valid(synthesis->typing, formation, parameters, motive_context)) goto rejected;
		local->fields = pg_synthesis_constructor_scope(synthesis, (void *)job->inputs[0], job->inputs[4], (void *)job->inputs[1]);
		if (!local->fields) goto error;
	}
	if (pg_synthesis_await(synthesis, job, local->fields)) return;
	const struct pg_evidence *map = local->fields->result;
	if (!local->context) {
		if (local->allocation && !pg_context_same_allocation_shape(local->allocation->prefix,
			pg_evidence_context_map(map)->destination)) goto rejected;
		local->context = pg_evidence_premise(map, 1);
	}
	if (local->pending) {
		if (pg_synthesis_await(synthesis, job, local->pending)) return;
		local->context = local->pending->result;
		local->pending = NULL;
	}
	const struct pg_evidence *context = local->context;
	const struct constructor_scope_work *fields = pg_synthesis_work_state(local->fields, CONSTRUCTOR_SCOPE_JOB);
	const struct pg_object *self = pg_evidence_context(pg_evidence_premise(formation, 0))->binder;
	while (local->next < fields->count) {
		size_t i = local->next++;
		int recursive = pg_data_recursive_field(pg_evidence_context(fields->fields[i])->declared_type, self);
		if (!recursive) continue;
		if (recursive < 0) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *field = pg_prove_projection(synthesis->typing, context,
			pg_substitution_image_at(synthesis->typing, map, pg_evidence_context_map(parameters)->count + 1 + i));
		const struct pg_context *retained = NULL;
		if (local->allocation) {
			struct pg_source_context_allocation *allocation = local->allocation;
			if (allocation->next == allocation->count) goto rejected;
			retained = allocation->contexts[allocation->next++];
		}
		const struct pg_evidence *ih = pg_prove_inductive_hypothesis_type(synthesis->typing,
			formation, parameters, motive_context, motive, context, field, retained ? retained->declared_type : NULL);
		if (!ih) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *at_field = pg_synthesis_evidence(synthesis, ih);
		struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(synthesis, context), at_field};
		local->pending = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_EXTEND,
			retained ? retained->binder : pg_binder(synthesis->typing->graph), 2, premises);
		if (!local->pending) goto error;
		pg_synthesis_subscribe(synthesis, job, local->pending, 0);
		return;
	}
	if (local->allocation && local->allocation->next != local->allocation->count) goto rejected;
	job->result = pg_prove_substitution_extension(synthesis->typing,
		pg_evidence_premise(map, 0), context, map, 0, NULL);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}
