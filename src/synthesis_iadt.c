#include "synthesis_source.h"
#include "iadt.h"
#include "action.h"

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

struct index_progress {
	const struct pg_context *field;
	struct pg_comparison comparison;
	size_t counts[2];
	/* 0 starts a candidate; 1/2/3 inspect target/old/new support. */
	unsigned next;
};
struct transport_scope {
	const struct pg_evidence *map, *left, *right, *extended;
	struct pg_typed_query *query;
	struct pg_context_lift *lift;
	struct pg_occurrence_action *action;
	struct pg_synthesis_job **branches;
	size_t count, next, scoped;
	const struct pg_evidence *extensions[];
};
struct index_scope {
	const struct pg_evidence *prefix, *map, *domain;
	const struct pg_evidence *source, *left, *right;
	const struct pg_evidence **values;
	struct pg_typed_query *query;
	struct transport_scope *boundary;
	size_t count, next, value_count, image_next;
	const struct pg_evidence *fields[];
};
struct index_transport_state {
	const struct pg_evidence *cursor, *path, *endpoints[2];
	struct pg_synthesis_job *normal[2], *candidates[2], *checks[2];
	struct index_scope *scopes[2];
	size_t candidate_next, building;
	int direct_checked, normalized_checked, constructor_checked;
	struct index_progress progress;
};
struct constructor_transport_work {
	struct pg_synthesis_job *left, *right, *value_job;
	struct transport_scope *transport_scope;
};
struct index_transport_work {
	struct pg_synthesis_job *left;
	struct index_transport_state *index_transport;
};

static void constructor_transport_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void index_transport_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void index_transport_destroy(struct pg_synthesis_job *job)
{
	struct index_transport_work *local = pg_synthesis_work_state(job, job->role);
	if (local->index_transport) pg_comparison_destroy(&local->index_transport->progress.comparison);
}
static const struct pg_synthesis_work_class CONSTRUCTOR_TRANSPORT_JOB[1] = {{
	.size = sizeof(struct constructor_transport_work), .advance = constructor_transport_step}};
static const struct pg_synthesis_work_class INDEX_TRANSPORT_JOB[1] = {{
	.size = sizeof(struct index_transport_work), .advance = index_transport_step, .destroy = index_transport_destroy}};
static const struct pg_synthesis_work_class INDEX_RESULT_JOB[1] = {{
	.size = sizeof(struct index_transport_work), .advance = index_transport_step, .destroy = index_transport_destroy}};

static struct pg_synthesis_job *index_transport_request(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, struct pg_synthesis_job *context,
	struct pg_synthesis_job *argument, struct pg_synthesis_job *target)
{
	if (!synthesis) return NULL;
	const void *inputs[] = {context, argument, target};
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_synthesis_job *input = inputs[i];
		if (!input || input->owner != synthesis->owner_key) return NULL;
	}
	return pg_synthesis_work_request(synthesis, role, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_index_transport(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *argument, struct pg_synthesis_job *target)
{
	return index_transport_request(synthesis, INDEX_TRANSPORT_JOB, context, argument, target);
}

struct pg_synthesis_job *pg_synthesis_index_result(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *argument, struct pg_synthesis_job *destination)
{
	return index_transport_request(synthesis, INDEX_RESULT_JOB, context, argument, destination);
}

struct pg_synthesis_job *pg_synthesis_index_transport_target(const struct pg_synthesis_job *job)
{
	return job && job->role == INDEX_TRANSPORT_JOB ? (void *)job->inputs[2] : NULL;
}

static struct pg_synthesis_job *constructor_transport_request(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	struct pg_synthesis_job *value, struct pg_synthesis_job *target_type,
	const struct pg_object *field)
{
	if (!synthesis) return NULL;
	const void *inputs[] = {context, left, right, path, value, target_type, field};
	for (size_t i = 0; i < 6; ++i) {
		const struct pg_synthesis_job *input = inputs[i];
		if (!input || input->owner != synthesis->owner_key) return NULL;
	}
	return pg_synthesis_work_request(synthesis, CONSTRUCTOR_TRANSPORT_JOB, 7, inputs);
}

struct pg_synthesis_job *pg_synthesis_disjoint_transport(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	struct pg_synthesis_job *value, struct pg_synthesis_job *target_type)
{
	return constructor_transport_request(synthesis, context, left, right, path, value, target_type, NULL);
}

struct pg_synthesis_job *pg_synthesis_constructor_field_identity(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	const struct pg_object *field, struct pg_synthesis_job *left_field,
	struct pg_synthesis_job *right_field)
{
	if (!field || field->kind != PG_BINDER) return NULL;
	return constructor_transport_request(synthesis, context, left, right, path, left_field, right_field, field);
}

static struct pg_synthesis_job *identity_classifier(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *term)
{
	return pg_synthesis_identity_formation(synthesis, pg_synthesis_classifier_formation(synthesis,
		pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, term)));
}

/* Vary the complete Identity boundary, not only a value in its left fiber.
 * The ordinary lifted telescope retains the outer context used by the result
 * family. Every endpoint image is checked by substitution pairing. */
static struct transport_scope *transport_scope_start(struct pg_typing *typing,
	const struct pg_evidence *base, const struct pg_identity_boundary *boundary)
{
	if (!base) return NULL;
	size_t count = boundary->left_substitution ? boundary->path_count : 0;
	if (count > (SIZE_MAX - sizeof(struct transport_scope)) / sizeof(const struct pg_evidence *)) return NULL;
	struct transport_scope *work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->extensions));
	if (!work) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(base, 0);
	work->left = work->right = base;
	work->count = count;
	if (boundary->left_substitution) {
		const struct pg_evidence *ls = pg_prove_context_map(typing, boundary->left_substitution);
		if (!ls) return NULL;
		const struct pg_evidence *source = pg_evidence_premise(ls, 0);
		size_t arity = pg_evidence_context_map(ls)->count;
		if (count > arity) return NULL;
		for (size_t i = count; i; --i, source = pg_evidence_premise(source, 0))
			work->extensions[i - 1] = source;
		const struct pg_evidence *map = pg_prove_substitution_compose(typing,
			pg_prove_substitution_projection(typing, source, pg_evidence_premise(ls, 0)), ls);
		work->query = pg_substitution_rebase_request(typing, context, map);
	} else {
		work->map = pg_prove_substitution_projection(typing, context, context);
		if (!work->map) return NULL;
		work->query = pg_rebase_request(typing, context, pg_prove_structural_subject(typing, boundary->family));
	}
	return work->query ? work : NULL;
}

static int transport_scope_advance(struct pg_typing *typing, struct transport_scope *work,
	const struct pg_identity_boundary *boundary,
	const struct pg_evidence *left_value, const struct pg_evidence *right_value)
{
	if (work->extended) return 1;
	const struct pg_evidence *type;
	if (work->query) {
		int status = pg_typed_query_advance(work->query, 1);
		if (status <= 0) return status;
		const struct pg_evidence *result = pg_typed_query_result(work->query);
		work->query = NULL;
		if (!result) return -1;
		if (!boundary->left_substitution) { type = result; goto extend; }
		work->map = result;
	}
	if (work->next < work->count) {
		const struct pg_evidence *field = work->extensions[work->next];
		if (!work->lift) work->lift = pg_context_lift_request(typing,
			pg_evidence_context_map(work->map), pg_evidence_context(field), pg_binder(typing->graph));
		enum pg_substitution_status status = pg_context_lift_advance(work->lift, 1);
		if (status == PG_SUBSTITUTION_PENDING) return 0;
		if (status != PG_SUBSTITUTION_DONE) return -1;
		const struct pg_context_map *lifted = pg_context_lift_result(work->lift);
		work->map = pg_prove_substitution_lift(typing, work->map, field, lifted->destination->binder);
		if (!work->map) return -1;
		const struct pg_evidence *extension = pg_evidence_premise(work->map, 1);
		size_t image = boundary->left_substitution->count - work->count + work->next;
		work->left = pg_prove_substitution_pair(typing, work->left, extension,
			pg_prove_structural_subject(typing, boundary->left_substitution->images[image]));
		work->right = pg_prove_substitution_pair(typing, work->right, extension,
			pg_prove_structural_subject(typing, boundary->right_substitution->images[image]));
		if (!work->left || !work->right) return -1;
		++work->next;
		work->lift = NULL;
		return 0;
	}
	if (!work->action) work->action = pg_occurrence_action_request(typing,
		pg_evidence_context_map(work->map), boundary->family);
	enum pg_substitution_status status = pg_occurrence_action_advance(work->action, 1);
	if (status == PG_SUBSTITUTION_PENDING) return 0;
	if (status != PG_SUBSTITUTION_DONE) return -1;
	type = pg_prove_reindex(typing, work->map, pg_prove_structural_subject(typing, boundary->family));
extend:;
	const struct pg_evidence *extended = pg_prove_context_extension(typing,
		pg_evidence_premise(work->map, 1), pg_binder(typing->graph), type);
	if (!extended) return -1;
	work->left = pg_prove_substitution_pair(typing, work->left, extended, left_value);
	work->right = pg_prove_substitution_pair(typing, work->right, extended, right_value);
	if (!work->left || !work->right) return -1;
	work->extended = extended;
	return 1;
}

/* Prepare each telescope through the same producer used by source constructors.
 * Branch bodies are built only after all scopes are ready. */
static int transport_constructor_scopes(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *parent, struct transport_scope *work, const struct pg_inductive_instance *instance,
	const struct pg_evidence *parameters)
{
	size_t count = pg_data_constructor_count(instance->schema);
	if (!work->branches) {
		if (count > SIZE_MAX / sizeof(*work->branches)) return -1;
		work->branches = pg_alloc(synthesis->typing->graph, count * sizeof(*work->branches));
		if (count && !work->branches) return -1;
		for (size_t i = 0; i < count; ++i) {
			work->branches[i] = pg_synthesis_constructor_scope(synthesis,
				pg_synthesis_evidence(synthesis, instance->formation),
				pg_data_constructor(pg_data_schema_layout(instance->schema), i),
				pg_synthesis_evidence(synthesis, parameters));
			if (!work->branches[i]) return -1;
		}
	}
	for (; work->scoped < count; ++work->scoped) {
		struct pg_synthesis_job *branch = work->branches[work->scoped];
		if (branch->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, parent, branch, 0); return 0; }
		if (branch->status != PG_SYNTHESIS_DONE) return -1;
	}
	return 1;
}

static void constructor_transport_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct constructor_transport_work *local = pg_synthesis_work_state(job, CONSTRUCTOR_TRANSPORT_JOB);
	if (local->value_job) { pg_synthesis_forward(synthesis, job, local->value_job); return; }
	const struct pg_evidence *inputs[6];
	for (size_t i = 0; i < 6; ++i) {
		struct pg_synthesis_job *input = (void *)job->inputs[i];
		if (pg_synthesis_await(synthesis, job, input)) return;
		inputs[i] = input->result;
		if (!inputs[i]) goto rejected;
	}
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = inputs[0], *value = inputs[4], *target = inputs[5];
	const struct pg_object *field = job->inputs[6];
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) goto rejected;
	for (size_t i = 1; i < 6; ++i)
		if (pg_evidence_context(inputs[i]) != pg_evidence_context(context)) goto rejected;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) goto rejected;
	if (pg_evidence_judgement(target) != (field ? PG_JUDGEMENT_VALUE : PG_JUDGEMENT_VALUE_TYPE)) goto rejected;
	if (!local->left) local->left = pg_synthesis_normalize_jobs(synthesis, (void *)job->inputs[0], (void *)job->inputs[1], PG_REDUCTION_WHNF);
	if (!local->right) local->right = pg_synthesis_normalize_jobs(synthesis, (void *)job->inputs[0], (void *)job->inputs[2], PG_REDUCTION_WHNF);
	struct pg_synthesis_job *endpoints[] = {local->left, local->right};
	const struct pg_object *constructors[2];
	for (size_t i = 0; i < 2; ++i) {
		struct pg_synthesis_job *endpoint = endpoints[i];
		if (!endpoint) goto error;
		if (pg_synthesis_await(synthesis, job, endpoint)) return;
		if (pg_evidence_judgement(endpoint->result) != PG_JUDGEMENT_VALUE) goto rejected;
		const struct pg_term *head = pg_evidence_subject(endpoint->result)->core;
		while (head->kind == PG_APPLICATION) head = head->as.application.function;
		if (head->kind != PG_REFERENCE) goto unsupported;
		constructors[i] = head->as.reference;
	}
	struct pg_identity_boundary boundary;
	struct pg_synthesis_job *identity = identity_classifier(synthesis, context, inputs[3]);
	if (pg_synthesis_await(synthesis, job, identity)) return;
	if (!identity->result || !pg_identity_boundary_view(pg_evidence_subject(identity->result), &boundary)) goto unsupported;
	if (!local->transport_scope) local->transport_scope = transport_scope_start(typing,
		pg_prove_substitution_projection(typing, context, context), &boundary);
	if (!local->transport_scope) goto unsupported;
	int status = transport_scope_advance(typing, local->transport_scope, &boundary, inputs[1], inputs[2]);
	if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
	if (status < 0) goto unsupported;
	const struct pg_evidence *left = local->transport_scope->left, *right = local->transport_scope->right;
	const struct pg_evidence *extended = local->transport_scope->extended;
	const struct pg_evidence *type = pg_evidence_premise(extended, 1);
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, type, &instance)) goto unsupported;
	const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
	size_t positions[2];
	for (size_t i = 0; i < 2; ++i)
		if (!pg_data_constructor_position(layout, constructors[i], &positions[i])) goto unsupported;
	if (field ? positions[0] != positions[1] : positions[0] == positions[1]) goto unsupported;
	if (field) {
		const struct pg_context *c = pg_evidence_context(pg_data_schema_fields(instance.schema, constructors[0]));
		const struct pg_context *prefix = pg_evidence_context(pg_evidence_premise(instance.formation, 0));
		while (c != prefix && c->binder != field) c = c->parent;
		if (c == prefix) goto rejected;
	}
	const struct pg_evidence *parameters = pg_prove_substitution_compose(typing, instance.parameters,
		pg_prove_substitution_projection(typing, pg_evidence_premise(extended, 0), extended));
	status = transport_constructor_scopes(synthesis, job, local->transport_scope, &instance, parameters);
	if (!status) return;
	if (status < 0) goto unsupported;
	const struct pg_evidence *field_type = NULL;
	if (field) {
		field_type = pg_prove_classifier(typing, context, value);
		target = pg_prove_identity_type(typing, field_type, value, target);
		value = pg_prove_reflexivity(typing, field_type, value);
		if (!target || !value) goto unsupported;
	}
	const struct pg_evidence *source = pg_prove_classifier(typing, context, value);
	if (!source) goto unsupported;
	size_t count = pg_data_constructor_count(instance.schema);
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
	const struct pg_evidence **branches = malloc(count * sizeof(*branches));
	if (!branches) goto error;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *map = local->transport_scope->branches[i]->result;
		const struct pg_evidence *fields = map ? pg_evidence_premise(map, 1) : NULL;
		const struct pg_evidence *branch;
		if (field && i == positions[0]) {
			/* The caller chooses which endpoint occurrence varies. This explicit
			 * injectivity theorem is not automatic result-type inference. */
			const struct pg_evidence *image = pg_substitution_image(typing, map, field);
			branch = image ? pg_prove_identity_type(typing, pg_prove_projection(typing, fields, field_type),
				pg_prove_projection(typing, fields, inputs[4]), image) : NULL;
		} else branch = pg_prove_projection(typing, fields, i == positions[0] ? source : target);
		for (; branch && pg_evidence_context(fields) != pg_evidence_context(extended); fields = pg_evidence_premise(fields, 0))
			branch = pg_prove_family_abstraction(typing, fields, branch);
		branches[i] = branch;
	}
	const struct pg_evidence *family = pg_prove_type_case(typing, instance.formation,
		parameters, pg_prove_variable(typing, extended, pg_evidence_context(extended)->binder), count, branches);
	free(branches);
	if (!family) goto unsupported;
	if (boundary.path_count >= SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
	size_t path_count = boundary.path_count + 1;
	struct pg_synthesis_job **paths = malloc(path_count * sizeof(*paths));
	if (!paths) goto error;
	for (size_t i = 0; i < boundary.path_count; ++i)
		paths[i] = pg_synthesis_evidence(synthesis, pg_prove_projection(typing, context, pg_prove_structural_subject(typing, boundary.paths[i])));
	paths[boundary.path_count] = (void *)job->inputs[3];
	struct pg_synthesis_job *transport = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), left, right, path_count, paths, pg_synthesis_evidence(synthesis, value), PG_IDENTITY_RIGHT);
	free(paths);
	local->value_job = pg_synthesis_expect(synthesis, transport, pg_synthesis_evidence(synthesis, target));
	if (!local->value_job) goto rejected;
	pg_synthesis_forward(synthesis, job, local->value_job);
	return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* Result synthesis asks whether the transported classifier can leave a branch
 * scope, not whether it matches a surface expectation. Both goals use the same
 * scoped path search and ordinary transport/checking rules. */
static struct pg_synthesis_job *index_transport_check(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *argument)
{
	struct pg_synthesis_job *target = (void *)job->inputs[2];
	if (job->role == INDEX_RESULT_JOB) {
		struct pg_synthesis_job *context = (void *)job->inputs[0];
		struct pg_synthesis_job *type = pg_synthesis_constant_motive(synthesis, target->result, context->result, argument);
		type = pg_synthesis_plain_rule(synthesis, PG_RETURN_CONTENT, NULL, 1, &type);
		target = pg_synthesis_plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, type});
	}
	return pg_synthesis_expect(synthesis, argument, target);
}

/* Keep later independent declarations when varying an earlier index. The
 * resulting map is checked, not a partial assignment to discarded variables. */
static struct index_scope *index_transport_scope(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *prefix,
	const struct pg_index *omitted)
{
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(prefix), &count)) return NULL;
	if (count > (SIZE_MAX - sizeof(struct index_scope)) / sizeof(const struct pg_evidence *)) return NULL;
	struct index_scope *work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->fields));
	if (!work) return NULL;
	work->prefix = prefix;
	work->count = count;
	work->map = pg_prove_substitution_projection(typing, prefix, context);
	const struct pg_evidence *extension = context;
	for (size_t i = count; i; --i, extension = pg_evidence_premise(extension, 0)) {
		const struct pg_object *binder = pg_evidence_context(extension)->binder;
		struct pg_index_entry *entry = pg_index_candidates(omitted, (uintptr_t)binder);
		while (entry && entry->hash != (uintptr_t)binder) entry = entry->next;
		if (!entry && pg_evidence_rule(extension) == PG_CONTEXT_EXTEND) work->fields[i - 1] = extension;
	}
	return work;
}

/* One existing rebase-query transition per turn. Failed domains are omitted
 * exactly as in the synchronous telescope check; later fields still see the
 * accepted prefix built so far. No candidate restarts its field scan. */
static int index_scope_advance(struct pg_typing *typing, struct index_scope *work)
{
	if (!work->map) return -1;
	if (work->next == work->count) return 1;
	const struct pg_evidence *field = work->fields[work->next];
	if (field) {
		const struct pg_evidence *scope = pg_evidence_premise(work->map, 0);
		if (!work->query) work->query = pg_rebase_request(typing, scope, pg_evidence_premise(field, 1));
		if (!pg_typed_query_advance(work->query, 1)) return 0;
		const struct pg_evidence *domain = pg_typed_query_result(work->query);
		work->query = NULL;
		if (domain) {
			const struct pg_object *binder = pg_evidence_context(field)->binder;
			const struct pg_evidence *extension = pg_prove_context_extension(typing, scope, binder, domain);
			work->map = pg_prove_substitution_pair(typing, work->map, extension,
				pg_prove_variable(typing, pg_evidence_premise(work->map, 1), binder));
		}
	}
	++work->next;
	return work->map ? work->next == work->count : -1;
}

/* Factor the classifier through a typed index endpoint, then transport along
 * its identity. Pattern inversion constructs
 * checked substitutions; it never rewrites a classifier's raw Core in place. */
static struct pg_synthesis_job *index_transport_candidate(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job,
	const struct pg_evidence *lv, const struct pg_evidence *rv, enum pg_identity_direction direction,
	const struct pg_evidence *normalized_from, int *pending)
{
	struct index_transport_work *local = pg_synthesis_work_state(job, job->role);
	if (!lv || !rv) return NULL;
	struct pg_typing *typing = synthesis->typing;
	struct index_transport_state *state = local->index_transport;
	const struct pg_evidence *context = ((struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_term *from = pg_evidence_subject(direction == PG_IDENTITY_LEFT ? rv : lv)->core;
	struct index_scope **slot = &state->scopes[direction == PG_IDENTITY_RIGHT];
	if (!*slot && from->kind == PG_REFERENCE && from->as.reference->kind == PG_BINDER) {
		const struct pg_evidence *extension = context;
		while (pg_evidence_context(extension) && pg_evidence_context(extension)->binder != from->as.reference)
			extension = pg_evidence_premise(extension, 0);
		if (!pg_evidence_context(extension) || pg_evidence_rule(extension) != PG_CONTEXT_EXTEND) return NULL;
		const struct pg_evidence *prefix = pg_evidence_premise(extension, 0);
		struct pg_index omitted = {0};
		struct pg_index_entry entry;
		if (pg_index_init(&omitted)) return NULL;
		if (pg_index_insert(&omitted, &entry, (uintptr_t)from->as.reference)) {
			pg_index_destroy(&omitted); return NULL;
		}
		*slot = index_transport_scope(typing, context, prefix, &omitted);
		pg_index_destroy(&omitted);
		if (*slot) (*slot)->domain = pg_evidence_premise(extension, 1);
	} else if (!*slot) {
		*slot = index_transport_scope(typing, context, context, NULL);
		if (*slot) (*slot)->domain = pg_prove_classifier(typing, context, direction == PG_IDENTITY_LEFT ? rv : lv);
	}
	if (!*slot) return NULL;
	int status = index_scope_advance(typing, *slot);
	if (!status) { pg_synthesis_enqueue(synthesis, job); *pending = 1; return NULL; }
	if (status < 0) return NULL;
	struct index_scope *work = *slot;
	const struct pg_evidence *map = work->map, *prefix = work->prefix;
	if (!work->source) {
		/* Named transport along arbitrary Universe paths stays explicit. */
		struct pg_inductive_instance instance;
		const struct pg_evidence *index_type = pg_prove_classifier(typing, context, lv);
		if (!pg_inductive_instance(typing, index_type, &instance)) return NULL;
		const struct pg_evidence *scope = pg_evidence_premise(map, 0);
		const struct pg_evidence *domain = pg_prove_projection(typing, scope, work->domain);
		work->source = pg_prove_context_extension(typing, scope, pg_binder(typing->graph), domain);
		work->left = pg_prove_substitution_pair(typing, map, work->source, lv);
		work->right = pg_prove_substitution_pair(typing, map, work->source, rv);
	}
	const struct pg_evidence *source = work->source, *ls = work->left, *rs = work->right;
	if (!ls || !rs) return NULL;
	struct pg_synthesis_job *argument = (void *)job->inputs[1];
	const struct pg_evidence *type = pg_prove_classifier(typing, context, argument->result);
	/* Factor the typed index occurrence, not a raw normal form of its type.
	 * The latter retains the original operands and cannot be inverted as if
	 * they had reduced. Conversion checks the occurrence against the endpoint;
	 * transport below still checks the original path and its source type. */
	if (normalized_from) {
		struct pg_inductive_instance target;
		if (pg_inductive_instance(typing, type, &target) && target.indices) {
			if (!work->image_next) work->image_next = pg_evidence_context_map(target.parameters)->count + 1;
			size_t count = pg_evidence_context_map(target.indices)->count;
			for (; work->image_next < count; ++work->image_next) {
				const struct pg_evidence *image = pg_substitution_image_at(typing, target.indices, work->image_next);
				if (!pg_prove_substitution_pair(typing, map, source, image)) continue;
				struct pg_synthesis_job *comparison = pg_synthesis_compare_terms(synthesis,
					pg_evidence_subject(normalized_from)->core, pg_evidence_subject(image)->core);
				if (!comparison) return NULL;
				if (comparison->status == PG_SYNTHESIS_PENDING) {
					pg_synthesis_subscribe(synthesis, job, comparison, 0);
					*pending = 1; return NULL;
				}
				if (comparison->status != PG_SYNTHESIS_DONE) continue;
				normalized_from = image;
				break;
			}
		}
	}
	const struct pg_evidence *pattern = normalized_from
		? pg_prove_substitution_pair(typing, map, source, normalized_from)
		: direction == PG_IDENTITY_LEFT ? rs : ls;
	const struct pg_evidence *family = pg_prove_pattern_type(typing, prefix, pattern,
		pg_prove_return_type(typing, type));
	if (!family) return NULL;
	family = pg_prove_return_content(typing, family);
	struct pg_synthesis_job *path = pg_synthesis_evidence(synthesis, state->path);
	struct pg_synthesis_job *transported = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), ls, rs, 1, &path, argument, direction);
	return transported;
}

/* Abstract the result over all fields together. A dependent tail therefore
 * travels with its size, without manufacturing separate homogeneous paths.
 * Only the selected constructor is used at either endpoint; other clauses
 * merely make the type family total and carry no inhabitance assertion. */
static struct index_scope *index_constructor_scope(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *value, const struct pg_object *constructor)
{
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_prove_classifier(typing, context, value), &instance)) return NULL;
	const struct pg_evidence *schema_fields = pg_data_schema_fields(instance.schema, constructor);
	const struct pg_evidence *schema_prefix = pg_evidence_premise(instance.formation, 0);
	size_t count;
	if (!schema_fields || pg_context_extension_size(pg_evidence_context(schema_fields),
		pg_evidence_context(schema_prefix), &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *) || count > SIZE_MAX / sizeof(struct pg_index_entry)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index omitted = {0};
	struct index_scope *result = NULL;
	const struct pg_evidence **values = pg_alloc(typing->graph, count * sizeof(*values));
	struct pg_index_entry *entries = pg_alloc(&temporary, count * sizeof(*entries));
	if (count && (!values || !entries)) goto done;
	if (pg_index_init(&omitted)) goto done;
	for (size_t i = count; i; --i, schema_fields = pg_evidence_premise(schema_fields, 0)) {
		values[i - 1] = pg_prove_constructor_field(typing, value, pg_evidence_context(schema_fields)->binder);
		if (!values[i - 1]) goto done;
		const struct pg_term *core = pg_evidence_subject(values[i - 1])->core;
		if (core->kind == PG_REFERENCE && core->as.reference->kind == PG_BINDER &&
			pg_index_insert(&omitted, &entries[i - 1], (uintptr_t)core->as.reference)) goto done;
	}
	const struct pg_evidence *prefix = context;
	for (const struct pg_evidence *scope = context; pg_evidence_context(scope); scope = pg_evidence_premise(scope, 0)) {
		uintptr_t binder = (uintptr_t)pg_evidence_context(scope)->binder;
		struct pg_index_entry *entry = pg_index_candidates(&omitted, binder);
		while (entry && entry->hash != binder) entry = entry->next;
		if (entry) prefix = pg_evidence_premise(scope, 0);
	}
	result = index_transport_scope(typing, context, prefix, &omitted);
	if (result) { result->values = values; result->value_count = count; }
done:
	pg_index_destroy(&omitted);
	pg_graph_destroy(&temporary);
	return result;
}

static struct pg_synthesis_job *index_constructor_candidate(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, enum pg_identity_direction direction, int *pending)
{
	struct index_transport_work *local = pg_synthesis_work_state(job, job->role);
	struct pg_typing *typing = synthesis->typing;
	struct index_transport_state *state = local->index_transport;
	const struct pg_evidence *context = ((struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_evidence *value = state->normal[direction == PG_IDENTITY_LEFT]->result;
	const struct pg_term *head = pg_evidence_subject(value)->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	if (head->kind != PG_REFERENCE) return NULL;
	const struct pg_object *constructor = head->as.reference;
	struct index_scope **slot = &state->scopes[direction == PG_IDENTITY_RIGHT];
	if (!*slot) *slot = index_constructor_scope(typing, context, value, constructor);
	if (!*slot) return NULL;
	int status = index_scope_advance(typing, *slot);
	if (!status) { pg_synthesis_enqueue(synthesis, job); *pending = 1; return NULL; }
	if (status < 0) return NULL;
	const struct pg_evidence *base = (*slot)->map, *prefix = (*slot)->prefix;
	const struct pg_evidence *const *values = (*slot)->values;
	size_t count = (*slot)->value_count;
	struct pg_identity_boundary boundary;
	struct pg_synthesis_job *identity = identity_classifier(synthesis, context, state->path);
	if (!identity) return NULL;
	if (identity->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, identity, 0); *pending = 1; return NULL; }
	if (!identity->result || !pg_identity_boundary_view(pg_evidence_subject(identity->result), &boundary)) return NULL;
	if (!(*slot)->boundary) (*slot)->boundary = transport_scope_start(typing, base, &boundary);
	if (!(*slot)->boundary) return NULL;
	status = transport_scope_advance(typing, (*slot)->boundary, &boundary, state->endpoints[0], state->endpoints[1]);
	if (!status) { pg_synthesis_enqueue(synthesis, job); *pending = 1; return NULL; }
	if (status < 0) return NULL;
	const struct pg_evidence *extended = (*slot)->boundary->extended;
	const struct pg_evidence *left = (*slot)->boundary->left, *right = (*slot)->boundary->right;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_evidence_premise(extended, 1), &instance)) return NULL;
	const struct pg_evidence *base_context = pg_evidence_premise(base, 0);
	struct pg_typed_query *parameters = pg_substitution_rebase_request(typing, base_context, instance.parameters);
	status = pg_typed_query_advance(parameters, 1);
	if (!status) { pg_synthesis_enqueue(synthesis, job); *pending = 1; return NULL; }
	const struct pg_evidence *parameter_map = pg_typed_query_result(parameters);
	if (!parameter_map) return NULL;
	status = transport_constructor_scopes(synthesis, job, (*slot)->boundary, &instance, parameter_map);
	if (!status) { *pending = 1; return NULL; }
	if (status < 0) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job *result = NULL;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	if (count && !extensions) goto done;
	const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
	size_t branch_count = pg_data_constructor_count(instance.schema);
	if (branch_count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	const struct pg_evidence **branches = pg_alloc(&temporary, branch_count * sizeof(*branches));
	if (!branches) goto done;
	struct pg_synthesis_job *argument = (void *)job->inputs[1];
	const struct pg_evidence *type = pg_prove_classifier(typing, context, argument->result);
	for (size_t i = 0; i < branch_count; ++i) {
		const struct pg_object *label = pg_data_constructor(layout, i);
		const struct pg_evidence *map = (*slot)->boundary->branches[i]->result;
		const struct pg_evidence *fields = pg_evidence_premise(map, 1), *branch;
		if (label == constructor) {
			const struct pg_evidence *scope = fields;
			for (size_t j = count; j; --j, scope = pg_evidence_premise(scope, 0)) extensions[j - 1] = scope;
			if (pg_evidence_context(scope) != pg_evidence_context(base_context)) goto done;
			const struct pg_evidence *pattern = base;
			for (size_t j = 0; pattern && j < count; ++j)
				pattern = pg_prove_substitution_pair(typing, pattern, extensions[j], values[j]);
			branch = pg_prove_pattern_type(typing, prefix, pattern, pg_prove_return_type(typing, type));
			branch = pg_prove_return_content(typing, branch);
		} else branch = pg_prove_universe(typing, fields, 0);
		for (; branch && pg_evidence_context(fields) != pg_evidence_context(base_context); fields = pg_evidence_premise(fields, 0))
			branch = pg_prove_family_abstraction(typing, fields, branch);
		branches[i] = pg_prove_projection(typing, extended, branch);
		if (!branches[i]) goto done;
	}
	parameter_map = pg_prove_substitution_compose(typing, parameter_map,
		pg_prove_substitution_projection(typing, base_context, extended));
	const struct pg_evidence *family = pg_prove_type_case(typing, instance.formation, parameter_map,
		pg_prove_variable(typing, extended, pg_evidence_context(extended)->binder), branch_count, branches);
	if (!family || boundary.path_count >= SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto done;
	size_t path_count = boundary.path_count + 1;
	struct pg_synthesis_job **paths = pg_alloc(&temporary, path_count * sizeof(*paths));
	if (!paths) goto done;
	for (size_t i = 0; i < boundary.path_count; ++i)
		paths[i] = pg_synthesis_evidence(synthesis, pg_prove_projection(typing, context, pg_prove_structural_subject(typing, boundary.paths[i])));
	paths[boundary.path_count] = pg_synthesis_evidence(synthesis, state->path);
	struct pg_synthesis_job *transported = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), left, right, path_count, paths, argument, direction);
	const struct pg_evidence *target = pg_prove_reindex(typing, direction == PG_IDENTITY_LEFT ? left : right, family);
	result = pg_synthesis_expect(synthesis, transported, pg_synthesis_normalize(synthesis, context, target));
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* Continue through several checked paths only when the classifier loses a
 * dependency that the destination cannot mention. This finite measure prevents
 * left/right transport cycles; failure still explores the other candidates. */
static int index_transport_progress(struct pg_synthesis_job *job,
	const struct pg_evidence *transported, int *progress)
{
	struct index_transport_work *local = pg_synthesis_work_state(job, job->role);
	const struct pg_synthesis_job *context = job->inputs[0], *argument = job->inputs[1], *target = job->inputs[2];
	*progress = 0;
	if (!target->result) return 1;
	const struct pg_term *before = pg_evidence_classifier(argument->result);
	const struct pg_term *after = pg_evidence_classifier(transported);
	if (!before || !after) return 1;
	struct index_progress *work = &local->index_transport->progress;
	if (!work->next) {
		work->field = pg_evidence_context(context->result);
		work->counts[0] = work->counts[1] = 0;
		work->next = 1;
	}
	if (work->field) {
		if (work->next == 1 && job->role == INDEX_RESULT_JOB) {
			work->next = pg_context_lookup(pg_evidence_context(target->result), work->field->binder) ? 4 : 2;
		} else {
			const struct pg_term *term = work->next == 1
				? pg_evidence_subject(target->result)->core : work->next == 2 ? before : after;
			if (!work->comparison.state && pg_independence_init(&work->comparison,
				term, work->field->binder)) return -1;
			enum pg_comparison_status status = pg_comparison_advance(&work->comparison, 1);
			if (status == PG_COMPARISON_PENDING) return 0;
			pg_comparison_destroy(&work->comparison);
			if (status == PG_COMPARISON_ERROR) return -1;
			if (work->next == 1) work->next = status == PG_COMPARISON_EQUAL ? 2 : 4;
			else {
				work->counts[work->next - 2] += status == PG_COMPARISON_DIFFERENT;
				++work->next;
			}
		}
		if (work->next == 4) {
			work->field = work->field->parent;
			work->next = 1;
		}
		if (work->field) return 0;
	}
	*progress = work->counts[1] < work->counts[0];
	work->next = 0;
	return 1;
}

static void index_transport_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct index_transport_work *local = pg_synthesis_work_state(job, job->role);
	if (!local->left) local->left = index_transport_check(synthesis, job, (void *)job->inputs[1]);
	if (!local->left) goto error;
	if (local->left->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, local->left, 0); return; }
	if (local->left->status != PG_SYNTHESIS_REJECTED &&
		!(job->role == INDEX_RESULT_JOB && local->left->status == PG_SYNTHESIS_UNSUPPORTED)) {
		pg_synthesis_forward(synthesis, job, local->left); return;
	}
	struct pg_synthesis_job *cj = (void *)job->inputs[0], *argument = (void *)job->inputs[1];
	if (pg_synthesis_await(synthesis, job, cj)) return;
	if (argument->status != PG_SYNTHESIS_DONE || pg_evidence_judgement(argument->result) != PG_JUDGEMENT_VALUE) goto rejected;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = cj->result;
	if (!local->index_transport) {
		local->index_transport = pg_alloc(typing->graph, sizeof(*local->index_transport));
		if (!local->index_transport) goto error;
		local->index_transport->cursor = context;
		local->index_transport->candidate_next = 2;
	}
	struct index_transport_state *state = local->index_transport;
	if (state->candidate_next < 2) {
		size_t index = state->candidate_next;
		struct pg_synthesis_job *candidate = state->candidates[index];
		if (candidate) {
			if (candidate->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, candidate, 0); return; }
			if (candidate->status == PG_SYNTHESIS_ERROR) goto error;
			if (candidate->status == PG_SYNTHESIS_DONE) {
				if (!state->checks[index]) {
					int progress;
					int status = index_transport_progress(job, candidate->result, &progress);
					if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
					if (status < 0) goto error;
					const void *inputs[] = {job->inputs[0], candidate, job->inputs[2]};
					state->checks[index] = progress ? pg_synthesis_work_request(synthesis, job->role, 3, inputs)
						: index_transport_check(synthesis, job, candidate);
				}
				struct pg_synthesis_job *check = state->checks[index];
				if (!check) goto error;
				if (check->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, check, 0); return; }
				if (check->status == PG_SYNTHESIS_DONE) { pg_synthesis_forward(synthesis, job, check); return; }
				if (check->status == PG_SYNTHESIS_ERROR) goto error;
			}
		}
		state->checks[index] = NULL;
		++state->candidate_next;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (state->constructor_checked) goto next_context;
	if (!pg_evidence_context(state->cursor)) goto rejected;
	if (!state->path) {
		if (pg_evidence_rule(state->cursor) != PG_CONTEXT_EXTEND) goto next_context;
		struct pg_synthesis_job *formation = pg_synthesis_identity_formation(synthesis,
			pg_synthesis_evidence(synthesis, pg_evidence_premise(state->cursor, 1)));
		if (!formation) goto error;
		if (formation->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, formation, 0); return; }
		struct pg_identity_boundary boundary;
		if (!formation->result || !pg_identity_boundary_view(pg_evidence_subject(formation->result), &boundary)) goto next_context;
		state->endpoints[0] = pg_prove_projection(typing, context, pg_prove_structural_subject(typing, boundary.left));
		state->endpoints[1] = pg_prove_projection(typing, context, pg_prove_structural_subject(typing, boundary.right));
		state->path = pg_prove_variable(typing, context, pg_evidence_context(state->cursor)->binder);
		if (!state->endpoints[0] || !state->endpoints[1] || !state->path) goto next_context;
	}
	if (!state->direct_checked) {
		const struct pg_evidence *left = state->endpoints[0];
		const struct pg_evidence *right = state->endpoints[1];
		for (; state->building < 2; ++state->building) {
			size_t i = state->building;
			int pending = 0;
			state->candidates[i] = index_transport_candidate(synthesis, job, left, right,
				i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, NULL, &pending);
			if (pending) return;
		}
		state->building = 0;
		state->candidate_next = 0; state->direct_checked = 1;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	for (size_t i = 0; i < 2; ++i) {
		if (!state->normal[i]) state->normal[i] = pg_synthesis_normalize_jobs(synthesis, cj,
			pg_synthesis_evidence(synthesis, state->endpoints[i]), PG_REDUCTION_WHNF);
		if (!state->normal[i]) goto error;
		if (state->normal[i]->status == PG_SYNTHESIS_PENDING) { pg_synthesis_subscribe(synthesis, job, state->normal[i], 0); return; }
		if (state->normal[i]->status == PG_SYNTHESIS_ERROR) goto error;
		if (state->normal[i]->status != PG_SYNTHESIS_DONE) goto next_context;
	}
	if (!state->normalized_checked) {
		for (; state->building < 2; ++state->building) {
			size_t i = state->building;
			const struct pg_evidence *image = state->normal[1 - i]->result;
			state->candidates[i] = NULL;
			if (pg_alpha_equal(pg_evidence_subject(image)->core,
				pg_evidence_subject(state->endpoints[1 - i])->core) == 1) continue;
			int pending = 0;
			state->candidates[i] = index_transport_candidate(synthesis, job,
				state->endpoints[0], state->endpoints[1], i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, image, &pending);
			if (pending) return;
		}
		state->building = 0;
		state->scopes[0] = state->scopes[1] = NULL;
		state->candidate_next = 0; state->normalized_checked = 1;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	const struct pg_object *heads[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *head = pg_evidence_subject(state->normal[i]->result)->core;
		while (head->kind == PG_APPLICATION) head = head->as.application.function;
		if (head->kind != PG_REFERENCE) goto next_context;
		heads[i] = head->as.reference;
	}
	if (heads[0] != heads[1]) goto next_context;
	struct pg_inductive_instance instance;
	const struct pg_evidence *type = pg_prove_classifier(typing, context, state->normal[0]->result);
	if (!pg_inductive_instance(typing, type, &instance)) goto next_context;
	size_t ordinal;
	if (!pg_data_constructor_position(pg_data_schema_layout(instance.schema), heads[0], &ordinal)) goto next_context;
	for (; state->building < 2; ++state->building) {
		size_t i = state->building;
		int pending = 0;
		state->candidates[i] = index_constructor_candidate(synthesis, job,
			i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, &pending);
		if (pending) return;
	}
	state->building = 0;
	state->candidate_next = 0; state->constructor_checked = 1;
	pg_synthesis_enqueue(synthesis, job); return;
next_context:
	state->cursor = pg_evidence_premise(state->cursor, 0);
	state->path = NULL;
	state->direct_checked = state->normalized_checked = state->constructor_checked = 0;
	state->normal[0] = state->normal[1] = NULL;
	state->scopes[0] = state->scopes[1] = NULL;
	pg_synthesis_enqueue(synthesis, job); return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}
