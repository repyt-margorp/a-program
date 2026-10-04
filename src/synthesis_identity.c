#include "synthesis_source.h"
#include "action.h"
#include "derivation.h"
#include "typed_query.h"

struct formation_work { struct pg_typed_query *query; };
struct face_work { struct pg_typed_query *query; };
struct reflexivity_work { struct pg_typed_query *classifier; };
struct instance_work {
	struct pg_synthesis_job *family, *endpoints[2];
	size_t next;
};
struct family_work {
	struct pg_synthesis_job *checked;
	struct pg_typed_query *classifier, *prefixes[2];
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations, **paths;
	size_t count, common, next;
};

static void formation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void face_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void reflexivity_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void family_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void instance_step(struct pg_synthesis *, struct pg_synthesis_job *);

static struct pg_synthesis_input formation_output(const struct pg_synthesis_job *);
static struct pg_synthesis_input face_output(const struct pg_synthesis_job *);

static const void *instance_input(const struct pg_synthesis_job *job, size_t i)
{
	if (!i) return job->inputs[0];
	return i % 2 ? pg_synthesis_input_result(pg_synthesis_work_dependency(job, i)) : NULL;
}

static const struct pg_synthesis_work_class FORMATION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct formation_work), .advance = formation_step,
	.output = formation_output}};
static const struct pg_synthesis_work_class FACE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct face_work), .advance = face_step, .output = face_output}};
static const struct pg_synthesis_work_class REFLEXIVITY_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct reflexivity_work), .advance = reflexivity_step}};
static const struct pg_synthesis_work_class FAMILY_ACTION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct family_work), .advance = family_step}};
static const struct pg_synthesis_work_class INSTANCE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct instance_work), .advance = instance_step,
	.resolved_input = instance_input}};

static struct pg_synthesis_input formation_output(const struct pg_synthesis_job *job)
{
	const struct formation_work *local = pg_synthesis_work_state(job, FORMATION_JOB);
	return (struct pg_synthesis_input){.pending = local->query ? &local->query->pending : NULL};
}

static struct pg_synthesis_input face_output(const struct pg_synthesis_job *job)
{
	const struct face_work *local = pg_synthesis_work_state(job, FACE_JOB);
	return (struct pg_synthesis_input){.pending = local->query ? &local->query->pending : NULL};
}

struct pg_synthesis_job *pg_synthesis_identity_instance(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_input family,
	struct pg_synthesis_input left, struct pg_synthesis_input right)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_synthesis_input_owned(synthesis, family)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, left)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, right)) return NULL;
	const void *inputs[] = {context, family.checked, family.pending,
		left.checked, left.pending, right.checked, right.pending};
	return pg_synthesis_work_request(synthesis, INSTANCE_JOB, 7, inputs);
}

struct pg_synthesis_job *pg_synthesis_reflexivity(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_input input)
{
	if (!pg_synthesis_input_owned(synthesis, input)) return NULL;
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	const void *inputs[] = {context, input.checked, input.pending};
	return pg_synthesis_work_request(synthesis, REFLEXIVITY_JOB, 3, inputs);
}

struct family_key {
	const struct pg_evidence *left, *right;
	struct pg_synthesis_input input;
	const struct pg_synthesis_input *paths;
};

static const void *family_operand(const void *data, size_t i)
{
	const struct family_key *key = data;
	if (i < 2) return i ? key->right : key->left;
	struct pg_synthesis_input input = i < 4 ? key->input : key->paths[(i - 4) / 2];
	return i % 2 ? (const void *)input.pending : input.checked;
}

struct pg_synthesis_job *pg_synthesis_family_action(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_synthesis_input *paths)
{
	if (!pg_synthesis_input_owned(synthesis, input)) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(right_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(left_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (pg_evidence_judgement(right_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (count && !paths) return NULL;
	if (count > (SIZE_MAX / sizeof(const void *) - 4) / 2) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_synthesis_input_owned(synthesis, paths[i])) return NULL;
	struct family_key key = {left_substitution, right_substitution, input, paths};
	return pg_synthesis_work_request_key(synthesis, FAMILY_ACTION_JOB, 4 + 2 * count, &key, family_operand);
}

struct pg_synthesis_job *pg_synthesis_family_transport(struct pg_synthesis *synthesis,
	struct pg_synthesis_input family, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_synthesis_input *paths, struct pg_synthesis_input value,
	enum pg_identity_direction direction)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	if (!pg_synthesis_input_owned(synthesis, family)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, value)) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_rule(left_substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	struct pg_synthesis_job *type_value = pg_synthesis_plain_rule_inputs(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &family);
	struct pg_synthesis_job *action = pg_synthesis_family_action(synthesis,
		(struct pg_synthesis_input){.pending = pg_synthesis_pending(type_value)}, left_substitution, right_substitution, count, paths);
	if (!action) return NULL;
	struct pg_synthesis_input context = {.checked = pg_evidence_premise(left_substitution, 1)};
	action = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, context,
		(struct pg_synthesis_input){.pending = pg_synthesis_pending(action)}).pending);
	if (!action) return NULL;
	struct pg_synthesis_job *left = pg_synthesis_plain_rule_inputs(synthesis, PG_IDENTITY_LEFT_TYPE, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(action)});
	struct pg_synthesis_job *right = pg_synthesis_plain_rule_inputs(synthesis, PG_IDENTITY_RIGHT_TYPE, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(action)});
	struct pg_synthesis_job *checked = pg_synthesis_expect_inputs(synthesis, value,
		(struct pg_synthesis_input){.pending = pg_synthesis_pending(direction == PG_IDENTITY_RIGHT ? left : right)});
	struct pg_derivation_input transport = {.rule = PG_IDENTITY_TRANSPORT,
		.count = 3, .parameters.direction = direction};
	struct pg_synthesis_input premises[] = {
		{.pending = pg_synthesis_pending(direction == PG_IDENTITY_RIGHT ? right : left)}, {.pending = pg_synthesis_pending(action)}, {.pending = pg_synthesis_pending(checked)}
	};
	return pg_synthesis_rule_inputs(synthesis, &transport, premises, NULL, NULL);
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

struct pg_pending *pg_synthesis_identity_endpoint(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!endpoint_selector(face)) return NULL;
	return pg_synthesis_identity_face(synthesis, context, (struct pg_synthesis_input){.checked = formation}, face);
}

struct pg_pending *pg_synthesis_identity_formation(struct pg_synthesis *synthesis,
	struct pg_synthesis_input producer)
{
	if (!pg_synthesis_input_owned(synthesis, producer)) return NULL;
	const void *inputs[] = {producer.checked, producer.pending};
	struct pg_synthesis_job *discovery = pg_synthesis_work_find(synthesis, FORMATION_JOB, 2, inputs);
	if (discovery) return pg_synthesis_pending(discovery);
	const struct pg_evidence *proof = pg_synthesis_input_result(producer);
	if (proof) {
		struct pg_typed_query *query = pg_identity_formation_request(synthesis->typing, proof);
		if (query) return &query->pending;
	}
	return pg_synthesis_pending(pg_synthesis_work_request(synthesis, FORMATION_JOB, 2, inputs));
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

struct pg_pending *pg_synthesis_permutation_source_face(struct pg_synthesis *synthesis,
	struct pg_dimensions *dimensions, const struct pg_evidence *context,
	struct pg_synthesis_input formation, const struct pg_dimension_map *permutation,
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
	struct pg_pending *pending = pg_synthesis_identity_face(synthesis, context, formation, ordered);
	if (!pending) return NULL;
	*intrinsic = orientation;
	return pending;
}

struct pg_pending *pg_synthesis_identity_face(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_input formation,
	const struct pg_dimension_map *face)
{
	if (!face || face->source >= face->target || !face->coordinates) return NULL;
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_synthesis_input_owned(synthesis, formation)) return NULL;
	if (formation.checked && !face_formation(synthesis, context, formation.checked)) return NULL;
	const void *inputs[] = {context, formation.checked, formation.pending, face};
	struct pg_synthesis_job *discovery = pg_synthesis_work_find(synthesis, FACE_JOB, 4, inputs);
	if (discovery) return pg_synthesis_pending(discovery);
	const struct pg_evidence *proof = pg_synthesis_input_result(formation);
	if (proof) {
		struct pg_typed_query *query = pg_identity_face_request(synthesis->typing, context, proof, face);
		if (query) return &query->pending;
	}
	return pg_synthesis_pending(pg_synthesis_work_request(synthesis, FACE_JOB, 4, inputs));
}

static void formation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct formation_work *local = pg_synthesis_work_state(job, FORMATION_JOB);
	struct pg_synthesis_input producer = pg_synthesis_work_dependency(job, 0);
	if (pg_synthesis_await_input(synthesis, job, producer)) return;
	const struct pg_evidence *proof = pg_synthesis_input_result(producer);
	if (!pg_evidence_owned_by(proof, synthesis->typing)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
	if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	local->query = pg_identity_formation_request(synthesis->typing, proof);
	pg_synthesis_finish(synthesis, job, local->query ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void face_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct face_work *local = pg_synthesis_work_state(job, FACE_JOB);
	struct pg_synthesis_input producer = pg_synthesis_work_dependency(job, 1);
	if (pg_synthesis_await_input(synthesis, job, producer)) return;
	const struct pg_evidence *proof = pg_synthesis_input_result(producer);
	if (!face_formation(synthesis, job->inputs[0], proof)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	local->query = pg_identity_face_request(synthesis->typing, job->inputs[0], proof, job->inputs[3]);
	pg_synthesis_finish(synthesis, job, local->query ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void instance_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct instance_work *local = pg_synthesis_work_state(job, INSTANCE_JOB);
	const struct pg_evidence *context = job->inputs[0];
	if (!local->family) {
		const struct pg_evidence *values[3];
		for (size_t i = 0; i < 3; ++i) {
			struct pg_synthesis_input producer = pg_synthesis_work_dependency(job, 1 + 2 * i);
			if (pg_synthesis_await_input(synthesis, job, producer)) return;
			values[i] = pg_synthesis_input_result(producer);
			if (!pg_evidence_owned_by(values[i], synthesis->typing)) goto rejected;
			if (pg_evidence_judgement(values[i]) != PG_JUDGEMENT_VALUE) goto rejected;
			if (pg_evidence_context(values[i]) != pg_evidence_context(context)) goto rejected;
		}
		if (pg_synthesis_forward(synthesis, job, pg_synthesis_work_resolve(synthesis, job))) return;
		local->family = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, (struct pg_synthesis_input){.checked = context}, (struct pg_synthesis_input){.checked = values[0]}).pending);
	}
	if (pg_synthesis_await(synthesis, job, local->family)) return;
	size_t i = local->next;
	if (!local->endpoints[i]) {
		enum pg_evidence_rule side = i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE;
		const struct pg_evidence *type = pg_prove_identity_endpoint_type(synthesis->typing,
			local->family->result, side);
		if (!type) goto rejected;
		local->endpoints[i] = pg_synthesis_expect_inputs(synthesis, pg_synthesis_work_dependency(job, 3 + 2 * i),
			(struct pg_synthesis_input){.checked = type});
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

/* Action uses the accepted term in its source Context, not the destination
 * of a family substitution. Classifier requests remain shared across owners. */
static const struct pg_evidence *action_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *context,
	struct pg_synthesis_input producer, struct pg_typed_query **classifier)
{
	if (pg_synthesis_await_input(synthesis, job, producer)) return NULL;
	const struct pg_evidence *input = pg_synthesis_input_result(producer);
	if (!input) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return NULL; }
	if (!pg_synthesis_typed_input(synthesis, context, input)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL;
	}
	if (pg_evidence_judgement(input) == PG_JUDGEMENT_VALUE_TYPE)
		input = pg_prove_type_value(synthesis->typing, input);
	if (!*classifier) *classifier = pg_classifier_request(synthesis->typing, context, input);
	if (pg_synthesis_yield_query(synthesis, job, *classifier)) return NULL;
	if (!pg_typed_query_result(*classifier)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return NULL;
	}
	return input;
}

static void reflexivity_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct reflexivity_work *local = pg_synthesis_work_state(job, REFLEXIVITY_JOB);
	const struct pg_evidence *input = action_input(synthesis, job, job->inputs[0],
		pg_synthesis_work_dependency(job, 1), &local->classifier);
	if (!input) return;
	job->result = pg_prove_reflexivity(synthesis->typing, pg_typed_query_result(local->classifier), input);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static int family_paths(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct family_work *state = pg_synthesis_work_state(job, FAMILY_ACTION_JOB);
	const struct pg_evidence *left = job->inputs[0], *right = job->inputs[1];
	if (!state->prefixes[0]) {
		size_t arity = pg_evidence_context_map(left)->count, count = (job->input_count - 4) / 2;
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
			state->prefixes[side] = pg_substitution_compose_request(synthesis->typing,
				pg_prove_substitution_projection(synthesis->typing, prefix, pg_evidence_premise(map, 0)), map);
		}
	}
	if (!state->maps[0]) {
		for (size_t side = 0; side < 2; ++side)
			if (pg_synthesis_await_query(synthesis, job, state->prefixes[side])) return 0;
		for (size_t side = 0; side < 2; ++side) state->maps[side] = pg_typed_query_result(state->prefixes[side]);
	}
	if (state->next == state->count) return 1;
	size_t i = state->next;
	if (!state->checked) {
		struct pg_synthesis_input producer = pg_synthesis_work_dependency(job, 4 + 2 * i);
		if (pg_synthesis_await_input(synthesis, job, producer)) return 0;
		const struct pg_evidence *path = pg_synthesis_input_result(producer);
		if (!pg_evidence_owned_by(path, synthesis->typing)) goto rejected;
		if (pg_evidence_judgement(path) != PG_JUDGEMENT_VALUE) goto rejected;
		if (pg_evidence_context(path) != pg_evidence_context(left)) goto rejected;
		const struct pg_evidence *type = pg_prove_family_identity_type(synthesis->typing,
			pg_context_declared_input(synthesis->typing, state->declarations[i]), state->maps[0], state->maps[1], i, (struct pg_evidence_inputs){.owner = state->paths},
			pg_substitution_image_at(synthesis->typing, left, state->common + i),
			pg_substitution_image_at(synthesis->typing, right, state->common + i));
		if (!type) goto rejected;
		state->checked = pg_synthesis_expect_inputs(synthesis, producer,
			(struct pg_synthesis_input){.checked = type});
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
		pg_evidence_premise(job->inputs[0], 0), pg_synthesis_work_dependency(job, 2), &local->classifier);
	if (!input || !family_paths(synthesis, job)) return;
	job->result = pg_prove_family_action(synthesis->typing, pg_typed_query_result(local->classifier), input,
		job->inputs[0], job->inputs[1], local->count, local->paths);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}
