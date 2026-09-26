#include "synthesis_source.h"
#include "action.h"
#include "derivation.h"

struct formation_work { struct pg_identity_formation_work *formation; };
struct face_work {
	struct pg_synthesis_job *formation;
	struct pg_identity_face_work *face;
};
struct reflexivity_work { struct pg_synthesis_job *classifier; };
struct instance_work {
	struct pg_synthesis_job *family, *endpoints[2];
	size_t next;
};
struct family_work {
	struct pg_synthesis_job *classifier, *checked;
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations, **paths;
	size_t count, common, next;
};

static void formation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void face_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void reflexivity_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void family_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void instance_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void formation_destroy(struct pg_synthesis_job *);
static void face_destroy(struct pg_synthesis_job *);

static const struct pg_synthesis_work_class FORMATION_JOB[1] = {{
	.size = sizeof(struct formation_work), .advance = formation_step, .destroy = formation_destroy}};
static const struct pg_synthesis_work_class FACE_JOB[1] = {{
	.size = sizeof(struct face_work), .advance = face_step, .destroy = face_destroy}};
static const struct pg_synthesis_work_class REFLEXIVITY_JOB[1] = {{
	.size = sizeof(struct reflexivity_work), .advance = reflexivity_step}};
static const struct pg_synthesis_work_class FAMILY_ACTION_JOB[1] = {{
	.size = sizeof(struct family_work), .advance = family_step}};
static const struct pg_synthesis_work_class INSTANCE_JOB[1] = {{
	.size = sizeof(struct instance_work), .advance = instance_step}};

struct pg_synthesis_job *pg_synthesis_identity_instance(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *family,
	struct pg_synthesis_job *left, struct pg_synthesis_job *right)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!family || family->owner != synthesis->owner_key) return NULL;
	if (!left || left->owner != synthesis->owner_key) return NULL;
	if (!right || right->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, family, left, right};
	return pg_synthesis_work_request(synthesis, INSTANCE_JOB, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_reflexivity(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *input)
{
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!context || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	const void *inputs[] = {context, input};
	return pg_synthesis_work_request(synthesis, REFLEXIVITY_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_family_action_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	struct pg_synthesis_job *const *paths)
{
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(right_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(left_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (pg_evidence_judgement(right_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (count && !paths) return NULL;
	if (count > SIZE_MAX / sizeof(const void *) - 3) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!paths[i] || paths[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {left_substitution, right_substitution, input};
	return pg_synthesis_work_request_inputs(synthesis, FAMILY_ACTION_JOB, 3, inputs, count, paths);
}

struct pg_synthesis_job *pg_synthesis_family_action(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths)
{
	if (count && !paths) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_evidence_owned_by(paths[i], synthesis->typing)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **jobs = pg_alloc(&temporary, count * sizeof(*jobs));
	if (count && !jobs) { pg_graph_destroy(&temporary); return NULL; }
	for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_evidence(synthesis, paths[i]);
	struct pg_synthesis_job *result = pg_synthesis_family_action_jobs(synthesis,
		input, left_substitution, right_substitution, count, jobs);
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_synthesis_job *pg_synthesis_family_transport_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *family, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	struct pg_synthesis_job *const *paths, struct pg_synthesis_job *value,
	enum pg_identity_direction direction)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	if (!family || family->owner != synthesis->owner_key) return NULL;
	if (!value || value->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_rule(left_substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	struct pg_synthesis_job *type_value = pg_synthesis_plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &family);
	struct pg_synthesis_job *action = pg_synthesis_family_action_jobs(synthesis,
		type_value, left_substitution, right_substitution, count, paths);
	if (!action) return NULL;
	struct pg_synthesis_job *context = pg_synthesis_evidence(synthesis,
		pg_evidence_premise(left_substitution, 1));
	action = pg_synthesis_normalize_classifier_jobs(synthesis, context, action);
	if (!action) return NULL;
	struct pg_synthesis_job *left = pg_synthesis_plain_rule(synthesis, PG_IDENTITY_LEFT_TYPE, NULL, 1, &action);
	struct pg_synthesis_job *right = pg_synthesis_plain_rule(synthesis, PG_IDENTITY_RIGHT_TYPE, NULL, 1, &action);
	struct pg_synthesis_job *checked = pg_synthesis_expect(synthesis, value,
		direction == PG_IDENTITY_RIGHT ? left : right);
	struct pg_derivation_input transport = {.rule = PG_IDENTITY_TRANSPORT,
		.count = 3, .parameters.direction = direction};
	struct pg_synthesis_job *premises[] = {
		direction == PG_IDENTITY_RIGHT ? right : left, action, checked
	};
	return pg_synthesis_rule(synthesis, &transport, premises, NULL, NULL);
}

static int endpoint_selector(const struct pg_dimension_map *face)
{
	if (!face || face->target <= face->source || face->target - face->source != 1) return 0;
	if (!face->coordinates) return 0;
	if (face->coordinates[0].kind == PG_AXIS) return 0;
	size_t axes = 0, fixed = 0;
	for (size_t i = 0; i < face->target; ++i) {
		switch (face->coordinates[i].kind) {
		case PG_AXIS:
			if (face->coordinates[i].axis != axes++) return 0;
			break;
		case PG_ENDPOINT_ZERO: case PG_ENDPOINT_ONE:
			if (fixed++) return 0;
			break;
		default: return 0;
		}
	}
	return fixed == 1 && axes == face->source;
}

struct pg_synthesis_job *pg_synthesis_identity_endpoint(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!endpoint_selector(face)) return NULL;
	return pg_synthesis_identity_face(synthesis, context, formation, face);
}

struct pg_synthesis_job *pg_synthesis_identity_formation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer)
{
	if (!producer || producer->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {producer};
	return pg_synthesis_work_request(synthesis, FORMATION_JOB, 1, inputs);
}

struct pg_synthesis_job *pg_synthesis_identity_face_job(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *formation,
	const struct pg_dimension_map *face)
{
	if (!face || face->source >= face->target || !face->coordinates) return NULL;
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, formation, face};
	return pg_synthesis_work_request(synthesis, FACE_JOB, 3, inputs);
}

static int face_formation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return 0;
	if (!pg_evidence_owned_by(formation, synthesis->typing)) return 0;
	if (pg_evidence_context(context) != pg_evidence_context(formation)) return 0;
	switch (pg_evidence_judgement(formation)) {
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: return 1;
	default: return 0;
	}
}

struct pg_synthesis_job *pg_synthesis_permutation_source_face(struct pg_synthesis *synthesis,
	struct pg_dimensions *dimensions, const struct pg_evidence *context,
	struct pg_synthesis_job *formation, const struct pg_dimension_map *permutation,
	const struct pg_dimension_map *target_face, const struct pg_dimension_map **intrinsic)
{
	if (!dimensions || dimensions->graph != synthesis->typing->graph) return NULL;
	if (!intrinsic || !permutation || !target_face) return NULL;
	if (permutation->source != permutation->target) return NULL;
	if (target_face->source >= target_face->target) return NULL;
	permutation = pg_dimension_face(dimensions, permutation);
	if (!permutation) return NULL;
	const struct pg_dimension_map *composed = pg_dimension_compose(dimensions, permutation, target_face);
	const struct pg_dimension_map *ordered, *orientation;
	if (pg_dimension_face_factor(dimensions, composed, &ordered, &orientation) != 0) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_identity_face_job(synthesis, context, formation, ordered);
	if (!job) return NULL;
	*intrinsic = orientation;
	return job;
}

struct pg_synthesis_job *pg_synthesis_identity_face(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!face_formation(synthesis, context, formation)) return NULL;
	return pg_synthesis_identity_face_job(synthesis, context, pg_synthesis_evidence(synthesis, formation), face);
}

static void formation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct formation_work *local = pg_synthesis_work_state(job, FORMATION_JOB);
	if (!local->formation) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[0];
		if (pg_synthesis_await(synthesis, job, producer)) return;
		const struct pg_evidence *proof = producer->result;
		if (!pg_evidence_owned_by(proof, synthesis->typing)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_formation(synthesis, pg_synthesis_evidence(synthesis, proof));
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		local->formation = pg_identity_formation_init(synthesis->typing, proof);
		if (!local->formation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	int status = pg_identity_formation_advance(local->formation, 1);
	if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
	job->result = pg_identity_formation_result(local->formation);
	pg_identity_formation_destroy(local->formation);
	local->formation = NULL;
	pg_synthesis_finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void face_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct face_work *local = pg_synthesis_work_state(job, FACE_JOB);
	if (!local->face) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[1];
		if (pg_synthesis_await(synthesis, job, producer)) return;
		if (!face_formation(synthesis, job->inputs[0], producer->result)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_face(synthesis,
			job->inputs[0], producer->result, job->inputs[2]);
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		if (!local->formation) local->formation = pg_synthesis_identity_formation(synthesis, producer);
		if (pg_synthesis_await(synthesis, job, local->formation)) return;
		canonical = pg_synthesis_identity_face(synthesis, job->inputs[0], local->formation->result, job->inputs[2]);
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		local->face = pg_identity_face_init(synthesis->typing,
			job->inputs[0], local->formation->result, job->inputs[2]);
		if (!local->face) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	int status = pg_identity_face_advance(local->face, 1);
	if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
	job->result = pg_identity_face_result(local->face);
	pg_identity_face_destroy(local->face);
	local->face = NULL;
	pg_synthesis_finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void instance_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct instance_work *local = pg_synthesis_work_state(job, INSTANCE_JOB);
	const struct pg_evidence *context = job->inputs[0];
	if (!local->family) {
		const struct pg_evidence *values[3];
		for (size_t i = 0; i < 3; ++i) {
			struct pg_synthesis_job *producer = (void *)job->inputs[i + 1];
			if (pg_synthesis_await(synthesis, job, producer)) return;
			values[i] = producer->result;
			if (!pg_evidence_owned_by(values[i], synthesis->typing)) goto rejected;
			if (pg_evidence_judgement(values[i]) != PG_JUDGEMENT_VALUE) goto rejected;
			if (pg_evidence_context(values[i]) != pg_evidence_context(context)) goto rejected;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_instance(synthesis, context,
			pg_synthesis_evidence(synthesis, values[0]), pg_synthesis_evidence(synthesis, values[1]),
			pg_synthesis_evidence(synthesis, values[2]));
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		local->family = pg_synthesis_normalize_classifier(synthesis, context, values[0]);
	}
	if (pg_synthesis_await(synthesis, job, local->family)) return;
	size_t i = local->next;
	if (!local->endpoints[i]) {
		enum pg_evidence_rule side = i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE;
		const struct pg_evidence *type = pg_prove_identity_endpoint_type(synthesis->typing,
			local->family->result, side);
		if (!type) goto rejected;
		local->endpoints[i] = pg_synthesis_expect(synthesis, (void *)job->inputs[i + 2],
			pg_synthesis_evidence(synthesis, type));
	}
	if (pg_synthesis_await(synthesis, job, local->endpoints[i])) return;
	if (++local->next < 2) { pg_synthesis_enqueue(synthesis, job); return; }
	job->result = pg_prove_identity_instance(synthesis->typing, local->family->result,
		local->endpoints[0]->result, local->endpoints[1]->result);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static void formation_destroy(struct pg_synthesis_job *job)
{
	struct formation_work *local = pg_synthesis_work_state(job, FORMATION_JOB);
	pg_identity_formation_destroy(local->formation);
}

static void face_destroy(struct pg_synthesis_job *job)
{
	struct face_work *local = pg_synthesis_work_state(job, FACE_JOB);
	pg_identity_face_destroy(local->face);
}

/* Action uses the accepted term in its source Context, not the destination
 * of a family substitution. Classifier requests remain shared across owners. */
static const struct pg_evidence *action_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *context,
	struct pg_synthesis_job *producer, struct pg_synthesis_job **classifier)
{
	if (pg_synthesis_await(synthesis, job, producer)) return NULL;
	const struct pg_evidence *input = producer->result;
	if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return NULL; }
	if (!pg_synthesis_typed_input(synthesis, context, input)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL;
	}
	if (pg_evidence_judgement(input) == PG_JUDGEMENT_VALUE_TYPE)
		input = pg_prove_type_value(synthesis->typing, input);
	if (!*classifier) *classifier = pg_synthesis_classifier_formation(synthesis,
		pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, input));
	if (pg_synthesis_await(synthesis, job, *classifier)) return NULL;
	return input;
}

static void reflexivity_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct reflexivity_work *local = pg_synthesis_work_state(job, REFLEXIVITY_JOB);
	const struct pg_evidence *input = action_input(synthesis, job, job->inputs[0],
		(void *)job->inputs[1], &local->classifier);
	if (!input) return;
	job->result = pg_prove_reflexivity(synthesis->typing, local->classifier->result, input);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static int family_paths(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct family_work *state = pg_synthesis_work_state(job, FAMILY_ACTION_JOB);
	const struct pg_evidence *left = job->inputs[0], *right = job->inputs[1];
	if (!state->maps[0]) {
		size_t arity = pg_evidence_context_map(left)->count, count = job->input_count - 3;
		if (count > arity || pg_evidence_context_map(right)->count != arity) goto rejected;
		if (pg_evidence_context(left) != pg_evidence_context(right)) goto rejected;
		if (pg_evidence_context(pg_evidence_premise(left, 0)) != pg_evidence_context(pg_evidence_premise(right, 0))) goto rejected;
		state->count = count;
		state->common = arity - count;
		state->declarations = pg_alloc(synthesis->typing->graph, count * sizeof(*state->declarations));
		state->paths = pg_alloc(synthesis->typing->graph, count * sizeof(*state->paths));
		if (count && (!state->declarations || !state->paths)) goto error;
		const struct pg_evidence *prefix = pg_evidence_premise(left, 0);
		for (size_t i = count; i; --i) {
			state->declarations[i - 1] = prefix;
			prefix = pg_context_parent_input(synthesis->typing, prefix);
		}
		for (size_t side = 0; side < 2; ++side) {
			const struct pg_evidence *map = job->inputs[side];
			state->maps[side] = pg_prove_substitution_compose(synthesis->typing,
				pg_prove_substitution_projection(synthesis->typing, prefix, pg_evidence_premise(map, 0)), map);
		}
		if (!state->maps[0] || !state->maps[1]) goto rejected;
	}
	if (state->next == state->count) return 1;
	size_t i = state->next;
	if (!state->checked) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[i + 3];
		if (pg_synthesis_await(synthesis, job, producer)) return 0;
		const struct pg_evidence *path = producer->result;
		if (!pg_evidence_owned_by(path, synthesis->typing)) goto rejected;
		if (pg_evidence_judgement(path) != PG_JUDGEMENT_VALUE) goto rejected;
		if (pg_evidence_context(path) != pg_evidence_context(left)) goto rejected;
		const struct pg_evidence *type = pg_prove_family_identity_type(synthesis->typing,
			pg_context_declared_input(synthesis->typing, state->declarations[i]), state->maps[0], state->maps[1], i, state->paths,
			pg_substitution_image_at(synthesis->typing, left, state->common + i),
			pg_substitution_image_at(synthesis->typing, right, state->common + i));
		if (!type) goto rejected;
		state->checked = pg_synthesis_expect(synthesis, producer, pg_synthesis_evidence(synthesis, type));
	}
	if (pg_synthesis_await(synthesis, job, state->checked)) return 0;
	state->paths[i] = state->checked->result;
	for (size_t side = 0; side < 2; ++side)
		state->maps[side] = pg_prove_substitution_pair(synthesis->typing, state->maps[side], state->declarations[i],
			pg_substitution_image_at(synthesis->typing, job->inputs[side], state->common + i));
	if (!state->maps[0] || !state->maps[1]) goto rejected;
	state->checked = NULL;
	++state->next;
	pg_synthesis_enqueue(synthesis, job);
	return 0;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return 0;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 0;
}

static void family_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct family_work *local = pg_synthesis_work_state(job, FAMILY_ACTION_JOB);
	const struct pg_evidence *input = action_input(synthesis, job,
		pg_evidence_premise(job->inputs[0], 0), (void *)job->inputs[2], &local->classifier);
	if (!input || !family_paths(synthesis, job)) return;
	job->result = pg_prove_family_action(synthesis->typing, local->classifier->result, input,
		job->inputs[0], job->inputs[1], local->count, local->paths);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}
