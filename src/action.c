#include "action.h"
#include "typed_query.h"
#include <stdlib.h>

static int boundary_argument(const struct pg_term **core, const struct pg_term *argument)
{
	if ((*core)->kind != PG_APPLICATION || (*core)->as.application.argument != argument) return 0;
	*core = (*core)->as.application.function;
	return 1;
}

int pg_identity_boundary_view(const struct pg_occurrence *subject, struct pg_identity_boundary *output)
{
	if (!subject || !output || subject->origin) return 0;
	if (subject->judgement != PG_JUDGEMENT_VALUE_TYPE && subject->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return 0;
	if (subject->operand_count < 3) return 0;
	struct pg_identity_boundary view = {.family = subject->operands[0],
		.left = subject->operands[subject->operand_count - 2],
		.right = subject->operands[subject->operand_count - 1]};
	const struct pg_term *core = subject->core, *source;
	if (!boundary_argument(&core, view.right->core) || !boundary_argument(&core, view.left->core)) return 0;
	if (!subject->map_count) {
		if (subject->operand_count != 3) return 0;
		if (view.family->judgement == PG_JUDGEMENT_VALUE) {
			if (core != view.family->core) return 0;
		} else if (!pg_identity_action_view(core, &source) || source != view.family->core) return 0;
	} else {
		if (subject->map_count != 2) return 0;
		const struct pg_context_map *const *maps = pg_occurrence_maps(subject);
		view.left_substitution = maps[0];
		view.right_substitution = maps[1];
		view.path_count = subject->operand_count - 3;
		view.paths = subject->operands + 1;
		if (maps[0]->count != maps[1]->count || view.path_count > maps[0]->count) return 0;
		size_t common = maps[0]->count - view.path_count;
		for (size_t i = view.path_count; i; --i) {
			if (!boundary_argument(&core, view.paths[i - 1]->core)) return 0;
			if (!boundary_argument(&core, maps[1]->images[common + i - 1]->core)) return 0;
			if (!boundary_argument(&core, maps[0]->images[common + i - 1]->core)) return 0;
		}
		if (!pg_identity_action_view(core, &source)) return 0;
	}
	*output = view;
	return 1;
}

int pg_identity_substitution_images(struct pg_typing *typing,
	const struct pg_evidence *substitution,
	const struct pg_evidence *left, const struct pg_evidence *right,
	size_t count, const struct pg_evidence *const *paths, size_t image_count,
	const struct pg_evidence **images)
{
	if (!pg_evidence_owned_by(substitution, typing)) return -1;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return -1;
	size_t total = pg_evidence_context_map(substitution)->count;
	if (image_count > total || (image_count && !images)) return -1;
	if (image_count > SIZE_MAX / sizeof(*images)) return -1;
	struct pg_graph temporary = {0};
	const struct pg_evidence **results = pg_alloc(&temporary, image_count * sizeof(*results));
	int status = -1;
	if (image_count && !results) goto done;
	const struct pg_evidence *context = pg_evidence_premise(substitution, 1);
	for (size_t i = 0; i < image_count; ++i) {
		const struct pg_evidence *value = pg_substitution_image_at(typing, substitution,
			total - image_count + i);
		const struct pg_evidence *type = pg_prove_classifier(typing, context, value);
		results[i] = pg_prove_family_action(typing, type, value, left, right, count, paths);
		if (!results[i]) goto done;
	}
	for (size_t i = 0; i < image_count; ++i) images[i] = results[i];
	status = 0;
done:
	pg_graph_destroy(&temporary);
	return status;
}

static int boundary_binders(struct pg_dimensions *dimensions,
	const struct pg_binding_face *center, const struct pg_object **binders)
{
	if (!center) return -1;
	center = pg_binding_face(dimensions, center->cube, center->face);
	if (!center) return -1;
	size_t dimension = center->face->source;
	if (!dimension || dimension > SIZE_MAX / sizeof(struct pg_coordinate)) return -1;
	struct pg_graph temporary = {0};
	struct pg_coordinate *coordinates = pg_alloc(&temporary, dimension * sizeof(*coordinates));
	int result = -1;
	if (!coordinates) goto done;
	for (size_t i = 0; i + 1 < dimension; ++i) coordinates[i] = (struct pg_coordinate){PG_AXIS, i};
	for (size_t side = 0; side < 2; ++side) {
		coordinates[dimension - 1] = (struct pg_coordinate){side ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
		const struct pg_dimension_map *map = pg_dimension_map(dimensions, dimension - 1, dimension, coordinates);
		const struct pg_binding_face *endpoint = pg_binding_restrict(dimensions, center, map);
		if (!endpoint) goto done;
		binders[side] = &endpoint->variable;
	}
	binders[2] = &center->variable;
	result = 0;
done:
	pg_graph_destroy(&temporary);
	return result;
}

static int origin_step(struct pg_typing *typing,
	const struct pg_occurrence **current, const struct pg_evidence **substitution,
	struct pg_typed_query **query)
{
	if (!*query) *query = pg_construction_origin_request(typing, pg_prove_structural_subject(typing, *current));
	int status = pg_typed_query_advance(*query, 1);
	if (status <= 0) return status;
	const struct pg_evidence *result = pg_typed_query_result(*query);
	if (!result) return -1;
	const struct pg_evidence *step = pg_construction_origin_environment(*query);
	if (step) {
		*substitution = *substitution ? pg_prove_substitution_compose(typing, step, *substitution) : step;
		if (!*substitution) return -1;
	}
	*current = pg_evidence_subject(result);
	*query = NULL;
	if ((*current)->selection || !(*current)->origin) return 1;
	/* Identity formation may cross a type/value boundary. Ordinary program
	 * construction access stops here; it must not mistake extraction for origin. */
	*current = (*current)->origin;
	return 0;
}

struct formation_origin {
	const struct pg_occurrence *term, *family;
	const struct pg_evidence *map, *family_map;
	struct pg_typed_query *query;
};

static int formation_origin_step(struct pg_typing *typing, struct formation_origin *origin)
{
	if (!origin->family) {
		int status = origin_step(typing, &origin->term, &origin->map, &origin->query);
		if (status <= 0) return status;
		if (origin->term->map_count || origin->term->operand_count != 3 ||
			origin->term->operands[0]->judgement != PG_JUDGEMENT_VALUE) return 1;
		origin->family = origin->term->operands[0];
	}
	return origin_step(typing, &origin->family, &origin->family_map, &origin->query);
}

/* Formation and action share the same selected boundary inputs. The optional
 * map changes that boundary's destination, not its original family scope. */
static const struct pg_evidence *selected_family(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_identity_boundary *boundary,
	const struct pg_evidence *map, const struct pg_evidence *left, const struct pg_evidence *right,
	const struct pg_evidence *term)
{
	size_t count = boundary->path_count;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *) || (count && !boundary->paths)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **paths = pg_alloc(&temporary, count * sizeof(*paths));
	if (count && !paths) { pg_graph_destroy(&temporary); return NULL; }
	for (size_t i = 0; i < count; ++i) {
		paths[i] = pg_prove_structural_subject(typing, boundary->paths[i]);
		if (map) paths[i] = pg_prove_reindex(typing, map, paths[i]);
	}
	const struct pg_evidence *ls = pg_prove_context_map(typing, boundary->left_substitution);
	const struct pg_evidence *rs = pg_prove_context_map(typing, boundary->right_substitution);
	if (map) {
		ls = pg_prove_substitution_compose(typing, ls, map);
		rs = pg_prove_substitution_compose(typing, rs, map);
	}
	const struct pg_evidence *result = term ? pg_prove_family_action(typing, type, term, ls, rs, count, paths)
		: pg_prove_family_identity_type(typing, type, ls, rs, count, (struct pg_evidence_inputs){.owner = paths}, left, right);
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_identity_boundary_type(struct pg_typing *typing,
	const struct pg_occurrence *subject)
{
	struct pg_identity_boundary boundary;
	if (!pg_identity_boundary_view(subject, &boundary)) return NULL;
	if (!boundary.left_substitution) return pg_prove_structural_subject(typing, subject);
	const struct pg_evidence *accepted = pg_evidence_for_subject(typing, subject, NULL);
	if (accepted) return accepted;
	const struct pg_evidence *family = pg_prove_structural_subject(typing, boundary.family);
	const struct pg_evidence *left = pg_prove_structural_subject(typing, boundary.left);
	const struct pg_evidence *right = pg_prove_structural_subject(typing, boundary.right);
	const struct pg_evidence *result = selected_family(typing, family, &boundary, NULL, left, right, NULL);
	return result && pg_evidence_subject(result) == subject ? result : NULL;
}

const struct pg_evidence *pg_identity_boundary_action(struct pg_typing *typing,
	const struct pg_identity_boundary *boundary, const struct pg_evidence *type,
	const struct pg_evidence *term)
{
	if (!boundary || !term) return NULL;
	return boundary->left_substitution ? selected_family(typing, type, boundary, NULL, NULL, NULL, term)
		: pg_prove_reflexivity(typing, type, term);
}

static const struct pg_evidence *formation_from_origin(struct pg_typing *typing,
	const struct formation_origin *origin)
{
	const struct pg_evidence *formation = pg_identity_boundary_type(typing, origin->term), *map = origin->map;
	if (!formation) return NULL;
	struct pg_identity_boundary boundary;
	if (!pg_identity_boundary_view(pg_evidence_subject(formation), &boundary)) return NULL;
	/* Retained construction inputs survive classifier conversion; the current
	 * classifier need not be the one used to build a family action. */
	if (boundary.family->judgement == PG_JUDGEMENT_VALUE) {
		const struct pg_evidence *family_map = origin->family_map;
		const struct pg_occurrence *family = origin->family;
		if (!family) return NULL;
		const struct pg_term *source;
		if (family->operand_count == 1 && pg_identity_action_view(family->core, &source) && source == family->operands[0]->core) {
			const struct pg_evidence *type = pg_prove_value_type(typing, pg_prove_structural_subject(typing, family->operands[0]));
			if (family_map) type = pg_prove_reindex(typing, family_map, type);
			formation = pg_prove_identity_type(typing, type,
				pg_prove_structural_subject(typing, boundary.left), pg_prove_structural_subject(typing, boundary.right));
			if (!formation || !pg_identity_boundary_view(pg_evidence_subject(formation), &boundary)) return NULL;
		} else if (family->operand_count == 2 && family->operands[1]->map_count == 2) {
			struct pg_identity_boundary action;
			if (!pg_identity_boundary_view(family->operands[1], &action)) return NULL;
			const struct pg_evidence *term = pg_prove_structural_subject(typing, family->operands[0]);
			const struct pg_evidence *checked = pg_identity_boundary_action(typing, &action,
				pg_prove_structural_subject(typing, action.family), term);
			if (!checked || pg_alpha_equal(pg_evidence_subject(checked)->core, family->core) != 1) return NULL;
			const struct pg_evidence *type = pg_prove_value_type(typing, term);
			formation = selected_family(typing, type, &action, family_map,
				pg_prove_structural_subject(typing, boundary.left), pg_prove_structural_subject(typing, boundary.right), NULL);
			if (!formation || !pg_identity_boundary_view(pg_evidence_subject(formation), &boundary)) return NULL;
		}
	}
	if (!map) return formation;
	if (!boundary.left_substitution) {
		const struct pg_evidence *family = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, boundary.family));
		const struct pg_evidence *left = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, boundary.left));
		const struct pg_evidence *right = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, boundary.right));
		return boundary.family->judgement == PG_JUDGEMENT_VALUE ? pg_prove_identity_instance(typing, family, left, right)
			: pg_prove_identity_type(typing, family, left, right);
	}
	const struct pg_evidence *left = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, boundary.left));
	const struct pg_evidence *right = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, boundary.right));
	return selected_family(typing, pg_prove_structural_subject(typing, boundary.family), &boundary, map, left, right, NULL);
}

static int formation_advance(struct pg_typed_query *query)
{
	struct pg_typing *typing = pg_typed_query_typing(query);
	struct formation_origin *origin = pg_typed_query_state(query);
	int status = formation_origin_step(typing, origin);
	if (status > 0) {
		query->result = formation_from_origin(typing, origin);
		if (!query->result) return -1;
	}
	return status;
}

static const struct pg_typed_query_class FORMATION[1] = {{
	.pending = {&pg_typed_query_pending_ops},
	.size = sizeof(struct formation_origin), .input_count = 1,
	.advance = formation_advance}};

struct pg_typed_query *pg_identity_formation_request(struct pg_typing *typing,
	const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(formation, typing)) return NULL;
	switch (pg_evidence_judgement(formation)) {
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: break;
	default: return NULL;
	}
	const void *inputs[] = {formation};
	int created = 0;
	struct pg_typed_query *query = pg_typed_query_request(typing, FORMATION, 0, inputs, &created);
	if (created) {
		struct formation_origin *origin = pg_typed_query_state(query);
		origin->term = pg_evidence_subject(formation);
	}
	return query;
}

const struct pg_evidence *pg_identity_formation(struct pg_typing *typing,
	const struct pg_evidence *formation)
{
	struct pg_typed_query *query = pg_identity_formation_request(typing, formation);
	while (pg_typed_query_advance(query, UINT64_MAX) == 0) {}
	return pg_typed_query_result(query);
}

struct endpoint_frame {
	struct endpoint_frame *previous;
	const struct pg_evidence *context;
	struct pg_identity_boundary boundary;
};

struct endpoint_work {
	struct pg_graph temporary;
	const struct pg_evidence *context, *formation, *value;
	struct endpoint_frame *stack;
	size_t depth;
};

static int endpoint_step(struct pg_typed_query *query)
{
	struct pg_typing *typing = pg_typed_query_typing(query);
	struct endpoint_work *work = pg_typed_query_state(query);
	if (!work->value) {
		if (!query->dependency) query->dependency = pg_identity_formation_request(typing, work->formation);
		if (!query->dependency) return -1;
		if (!query->dependency->status) return 0;
		const struct pg_evidence *formation = pg_typed_query_result(query->dependency);
		query->dependency = NULL;
		struct pg_identity_boundary boundary;
		if (!formation || !pg_identity_boundary_view(pg_evidence_subject(formation), &boundary)) return -1;
		if (!work->depth) {
			enum pg_identity_direction side = *(const enum pg_identity_direction *)query->inputs[2];
			work->value = pg_prove_structural_subject(typing, side == PG_IDENTITY_LEFT ? boundary.left : boundary.right);
			if (!work->value) return -1;
			if (!work->stack) { query->result = work->value; return 1; }
			return 0;
		}
		if (boundary.family->judgement == PG_JUDGEMENT_VALUE) return -1;
		struct endpoint_frame *frame = pg_alloc(&work->temporary, sizeof(*frame));
		if (!frame) return -1;
		*frame = (struct endpoint_frame){work->stack, work->context, boundary};
		work->stack = frame;
		if (boundary.left_substitution)
			work->context = pg_evidence_premise(pg_prove_context_map(typing, boundary.left_substitution), 0);
		work->formation = pg_prove_structural_subject(typing, boundary.family);
		if (!work->formation) return -1;
		--work->depth;
		return 0;
	}
	const struct endpoint_frame *frame = work->stack;
	const struct pg_evidence *type = pg_prove_classifier(typing, work->context, work->value);
	const struct pg_identity_boundary *boundary = &frame->boundary;
	work->value = pg_identity_boundary_action(typing, boundary, type, work->value);
	if (!work->value) return -1;
	work->context = frame->context;
	work->stack = frame->previous;
	if (!work->stack) { query->result = work->value; return 1; }
	return 0;
}

static void endpoint_cleanup(struct pg_typed_query *query)
{
	struct endpoint_work *work = pg_typed_query_state(query);
	pg_graph_destroy(&work->temporary);
	work->stack = NULL;
}

static const struct pg_typed_query_class ENDPOINT[1] = {{
	.pending = {&pg_typed_query_pending_ops}, .size = sizeof(struct endpoint_work),
	.input_count = 3, .advance = endpoint_step, .destroy = endpoint_cleanup}};

static int boundary_inputs(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(context, typing)) return 0;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return 0;
	if (!pg_evidence_owned_by(formation, typing)) return 0;
	if (pg_evidence_context(context) != pg_evidence_context(formation)) return 0;
	enum pg_evidence_judgement kind = pg_evidence_judgement(formation);
	return kind == PG_JUDGEMENT_VALUE_TYPE || kind == PG_JUDGEMENT_COMPUTATION_TYPE;
}

struct pg_typed_query *pg_identity_endpoint_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	size_t depth, enum pg_identity_direction side)
{
	if (!boundary_inputs(typing, context, formation)) return NULL;
	if (side != PG_IDENTITY_LEFT && side != PG_IDENTITY_RIGHT) return NULL;
	static const enum pg_identity_direction directions[] = {PG_IDENTITY_RIGHT, PG_IDENTITY_LEFT};
	const void *inputs[] = {context, formation, &directions[side]};
	int created = 0;
	struct pg_typed_query *query = pg_typed_query_request(typing, ENDPOINT, depth, inputs, &created);
	if (created) {
		struct endpoint_work *work = pg_typed_query_state(query);
		work->context = context;
		work->formation = formation;
		work->depth = depth;
	}
	return query;
}

const struct pg_evidence *pg_identity_face_endpoint(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *formation, size_t depth, enum pg_identity_direction side)
{
	struct pg_typed_query *query = pg_identity_endpoint_request(typing, context, formation, depth, side);
	while (pg_typed_query_advance(query, UINT64_MAX) == 0) {}
	return pg_typed_query_result(query);
}

struct face_work {
	const struct pg_evidence *formation, *layer;
	size_t checked, axes, next, retained, remaining;
};

static int face_step(struct pg_typed_query *query)
{
	struct pg_typing *typing = pg_typed_query_typing(query);
	const struct pg_evidence *context = query->inputs[0];
	const struct pg_dimension_map *face = query->inputs[2];
	struct face_work *work = pg_typed_query_state(query);
	if (work->checked < face->target) {
		if (!query->dependency) query->dependency = pg_identity_formation_request(typing, work->layer);
		if (!query->dependency) return -1;
		if (!query->dependency->status) return 0;
		struct pg_coordinate coordinate = face->coordinates[work->checked];
		switch (coordinate.kind) {
		case PG_AXIS:
			if (coordinate.axis != work->axes++) return -1;
			break;
		case PG_ENDPOINT_ZERO: case PG_ENDPOINT_ONE: break;
		default: return -1;
		}
		const struct pg_evidence *layer = pg_typed_query_result(query->dependency);
		query->dependency = NULL;
		struct pg_identity_boundary boundary;
		if (!layer || !pg_identity_boundary_view(pg_evidence_subject(layer), &boundary)) return -1;
		if (!work->checked) work->formation = layer;
		work->layer = pg_prove_structural_subject(typing, boundary.family);
		if (!work->layer) return -1;
		++work->checked;
		return 0;
	}
	if (work->axes != face->source) return -1;
	if (!query->dependency) {
		if (!work->next) return -1;
		enum pg_coordinate_kind kind = face->coordinates[--work->next].kind;
		if (kind == PG_AXIS) { ++work->retained; return 0; }
		query->dependency = pg_identity_endpoint_request(typing,
			context, work->formation, work->retained,
			kind == PG_ENDPOINT_ZERO ? PG_IDENTITY_LEFT : PG_IDENTITY_RIGHT);
		if (!query->dependency) return -1;
		return 0;
	}
	if (!query->dependency->status) return 0;
	const struct pg_evidence *result = pg_typed_query_result(query->dependency);
	query->dependency = NULL;
	if (!result) return -1;
	if (!--work->remaining) { query->result = result; return 1; }
	work->formation = pg_prove_classifier(typing, context, result);
	return work->formation ? 0 : -1;
}

static const struct pg_typed_query_class FACE[1] = {{
	.pending = {&pg_typed_query_pending_ops}, .size = sizeof(struct face_work),
	.input_count = 3, .advance = face_step}};

struct pg_typed_query *pg_identity_face_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!face || face->source >= face->target || !face->coordinates) return NULL;
	if (!boundary_inputs(typing, context, formation)) return NULL;
	const void *inputs[] = {context, formation, face};
	int created = 0;
	struct pg_typed_query *query = pg_typed_query_request(typing, FACE, 0, inputs, &created);
	if (created) {
		struct face_work *work = pg_typed_query_state(query);
		work->formation = work->layer = formation;
		work->next = face->target;
		work->remaining = face->target - face->source;
	}
	return query;
}

const struct pg_evidence *pg_identity_proper_face(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *formation, const struct pg_dimension_map *face)
{
	struct pg_typed_query *query = pg_identity_face_request(typing, context, formation, face);
	while (pg_typed_query_advance(query, UINT64_MAX) == 0) {}
	return pg_typed_query_result(query);
}

const struct pg_evidence *pg_identity_context_extend(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *family, const struct pg_object *left,
	const struct pg_object *right, const struct pg_object *center)
{
	const struct pg_evidence *left_type = pg_prove_identity_endpoint_type(typing,
		family, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *right_type = pg_prove_identity_endpoint_type(typing,
		family, PG_IDENTITY_RIGHT_TYPE);
	if (!left_type || !right_type) return NULL;
	context = pg_prove_context_extension(typing, context, left, left_type);
	if (!context) return NULL;
	right_type = pg_prove_projection(typing, context, right_type);
	context = pg_prove_context_extension(typing, context, right, right_type);
	if (!context) return NULL;
	family = pg_prove_projection(typing, context, family);
	const struct pg_evidence *x0 = pg_prove_variable(typing, context, left);
	const struct pg_evidence *x1 = pg_prove_variable(typing, context, right);
	const struct pg_evidence *center_type = pg_prove_identity_instance(typing,
		family, x0, x1);
	return pg_prove_context_extension(typing, context, center, center_type);
}

const struct pg_evidence *pg_identity_pi_type(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *pi, const struct pg_evidence *left,
	const struct pg_evidence *right, const struct pg_object *x0,
	const struct pg_object *x1, const struct pg_object *path)
{
	const struct pg_evidence *identity = pg_prove_substitution_projection(typing, context, context);
	return pg_identity_family_pi_type(typing, pi, identity, identity,
		0, NULL, left, right, x0, x1, path);
}

const struct pg_evidence *pg_identity_family_pi_type(struct pg_typing *typing,
	const struct pg_evidence *pi,
	const struct pg_evidence *left_substitution, const struct pg_evidence *right_substitution,
	size_t count, const struct pg_evidence *const *paths,
	const struct pg_evidence *left, const struct pg_evidence *right,
	const struct pg_object *x0, const struct pg_object *x1, const struct pg_object *path)
{
	if (!pg_prove_family_identity_type(typing, pi, left_substitution, right_substitution,
		count, (struct pg_evidence_inputs){.owner = paths}, left, right)) return NULL;
	const struct pg_evidence *domain = pg_prove_pi_domain(typing, pi);
	if (!domain || count >= SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **centers = pg_alloc(&temporary, (count + 1) * sizeof(*centers));
	const struct pg_evidence *body = NULL;
	if (!centers) goto done;
	const struct pg_evidence *context = pg_evidence_premise(left_substitution, 1);
	const struct pg_evidence *source_context = pg_evidence_premise(left_substitution, 0);
	const struct pg_evidence *boundary = pg_prove_context_extension(typing, context, x0,
		pg_prove_reindex(typing, left_substitution, domain));
	boundary = pg_prove_context_extension(typing, boundary, x1,
		pg_prove_projection(typing, boundary, pg_prove_reindex(typing, right_substitution, domain)));
	if (!boundary) goto done;
	const struct pg_evidence *prefix = pg_prove_substitution_projection(typing, context, boundary);
	const struct pg_evidence *ls = pg_prove_substitution_compose(typing, left_substitution, prefix);
	const struct pg_evidence *rs = pg_prove_substitution_compose(typing, right_substitution, prefix);
	for (size_t i = 0; i < count; ++i) centers[i] = pg_prove_projection(typing, boundary, paths[i]);
	const struct pg_evidence *center_type = pg_prove_family_identity_type(typing, domain, ls, rs,
		count, (struct pg_evidence_inputs){.owner = centers}, pg_prove_variable(typing, boundary, x0), pg_prove_variable(typing, boundary, x1));
	boundary = pg_prove_context_extension(typing, boundary, path, center_type);
	if (!boundary) goto done;
	const struct pg_term *a, *c;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(pi)->core, &a, &binder, &c)) goto done;
	const struct pg_evidence *source = pg_prove_context_extension(typing, source_context, binder, domain);
	const struct pg_evidence *codomain = pg_prove_pi_codomain(typing,
		pg_prove_projection(typing, source, pi), pg_prove_variable(typing, source, binder));
	if (!codomain) goto done;
	const struct pg_evidence *l = pg_prove_variable(typing, boundary, x0);
	const struct pg_evidence *r = pg_prove_variable(typing, boundary, x1);
	for (size_t i = 0; i < count; ++i) centers[i] = pg_prove_projection(typing, boundary, paths[i]);
	centers[count] = pg_prove_variable(typing, boundary, path);
	prefix = pg_prove_substitution_projection(typing, context, boundary);
	ls = pg_prove_substitution_pair(typing,
		pg_prove_substitution_compose(typing, left_substitution, prefix), source, l);
	rs = pg_prove_substitution_pair(typing,
		pg_prove_substitution_compose(typing, right_substitution, prefix), source, r);
	body = pg_prove_family_identity_type(typing, codomain, ls, rs, count + 1, (struct pg_evidence_inputs){.owner = centers},
		pg_prove_application(typing, pg_prove_projection(typing, boundary, left), l),
		pg_prove_application(typing, pg_prove_projection(typing, boundary, right), r));
	for (size_t i = 0; body && i < 3; ++i) {
		body = pg_prove_pi(typing, boundary, body);
		boundary = pg_context_parent_input(typing, boundary);
	}
done:
	pg_graph_destroy(&temporary);
	return body;
}

const struct pg_evidence *pg_identity_thunk_type(struct pg_typing *typing,
	const struct pg_evidence *type,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (!pg_prove_identity_type(typing, type, left, right)) return NULL;
	const struct pg_evidence *content = pg_prove_thunk_content(typing, type);
	const struct pg_evidence *identity = pg_prove_identity_type(typing, content,
		pg_prove_force(typing, left), pg_prove_force(typing, right));
	return pg_prove_thunk_type(typing, identity);
}

static void project_paths(struct pg_typing *typing, const struct pg_evidence *context,
	size_t count, const struct pg_evidence **paths)
{
	for (size_t i = 0; i < count; ++i) paths[i] = pg_prove_projection(typing, context, paths[i]);
}

const struct pg_evidence *pg_identity_context(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	size_t count, const struct pg_binding_face *const *centers,
	const struct pg_evidence **left, const struct pg_evidence **right,
	const struct pg_evidence **paths)
{
	if (dimensions->graph != typing->graph || !source || !left || !right) return NULL;
	if (pg_evidence_judgement(source) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (count && (!centers || !paths)) return NULL;
	size_t arity = 0;
	for (const struct pg_context *c = pg_evidence_context(source); c; c = c->parent) ++arity;
	if (count > arity || arity > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_evidence **centers_proof = pg_alloc(&temporary, count * sizeof(*centers_proof));
	const struct pg_evidence *result = NULL;
	if (count && (!extensions || !centers_proof)) goto done;
	const struct pg_evidence *context = source;
	for (size_t i = count; i; --i) {
		extensions[i - 1] = context;
		context = pg_context_parent_input(typing, context);
	}
	const struct pg_evidence *ls = pg_prove_substitution_projection(typing, context, context), *rs = ls;
	if (!ls) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_object *binders[3];
		if (boundary_binders(dimensions, centers[i], binders) != 0) goto done;
		const struct pg_evidence *type = pg_context_declared_input(typing, extensions[i]);
		const struct pg_evidence *lt = pg_prove_reindex(typing, ls, type);
		const struct pg_evidence *rt = pg_prove_reindex(typing, rs, type);
		context = pg_prove_context_extension(typing, context, binders[0], lt);
		context = pg_prove_context_extension(typing, context, binders[1], pg_prove_projection(typing, context, rt));
		if (!context) goto done;
		project_paths(typing, context, i, centers_proof);
		const struct pg_evidence *prefix = pg_context_parent_input(typing, extensions[i]);
		ls = pg_prove_substitution_extension(typing, prefix, context, ls, 0, (struct pg_evidence_inputs){.owner = NULL});
		rs = pg_prove_substitution_extension(typing, prefix, context, rs, 0, (struct pg_evidence_inputs){.owner = NULL});
		const struct pg_evidence *l = pg_prove_variable(typing, context, binders[0]);
		const struct pg_evidence *r = pg_prove_variable(typing, context, binders[1]);
		const struct pg_evidence *center_type = pg_prove_family_identity_type(typing, type, ls, rs, i, (struct pg_evidence_inputs){.owner = centers_proof}, l, r);
		context = pg_prove_context_extension(typing, context, binders[2], center_type);
		if (!context) goto done;
		project_paths(typing, context, i, centers_proof);
		l = pg_prove_variable(typing, context, binders[0]);
		r = pg_prove_variable(typing, context, binders[1]);
		centers_proof[i] = pg_prove_variable(typing, context, binders[2]);
		ls = pg_prove_substitution_extension(typing, extensions[i], context, ls, 1, (struct pg_evidence_inputs){.owner = &l});
		rs = pg_prove_substitution_extension(typing, extensions[i], context, rs, 1, (struct pg_evidence_inputs){.owner = &r});
		if (!ls || !rs) goto done;
	}
	*left = ls;
	*right = rs;
	for (size_t i = 0; i < count; ++i) paths[i] = centers_proof[i];
	result = context;
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_identity_substitution_context(struct pg_typing *typing,
	const struct pg_evidence *left, const struct pg_evidence *right,
	size_t count, const struct pg_object *const *binders,
	const struct pg_evidence **paths)
{
	if (!pg_evidence_owned_by(left, typing) || !pg_evidence_owned_by(right, typing)) return NULL;
	if (pg_evidence_rule(left) != PG_CONTEXT_SUBSTITUTION || pg_evidence_rule(right) != PG_CONTEXT_SUBSTITUTION) return NULL;
	const struct pg_evidence *source = pg_evidence_premise(left, 0);
	if (pg_evidence_context(source) != pg_evidence_context(pg_evidence_premise(right, 0))) return NULL;
	if (pg_evidence_context(left) != pg_evidence_context(right)) return NULL;
	size_t arity = pg_evidence_context_map(left)->count;
	if (pg_evidence_context_map(right)->count != arity) return NULL;
	if (count > arity || arity > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	if (count && (!binders || !paths)) return NULL;
	size_t common = arity - count;
	for (size_t i = 0; i < common; ++i)
		if (pg_alpha_equal(pg_evidence_context_map(left)->images[i]->core,
			pg_evidence_context_map(right)->images[i]->core) != 1) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(left, 1);
	if (!count) return context;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_evidence **centers = pg_alloc(&temporary, count * sizeof(*centers));
	if (!extensions || !centers) goto done;
	for (size_t i = count; i; --i, source = pg_context_parent_input(typing, source)) {
		if (pg_evidence_rule(source) != PG_CONTEXT_EXTEND) goto done;
		extensions[i - 1] = source;
	}
	const struct pg_evidence *prefix = pg_prove_substitution_projection(typing, source, pg_evidence_premise(left, 0));
	const struct pg_evidence *ls = pg_prove_substitution_compose(typing, prefix, left);
	const struct pg_evidence *rs = pg_prove_substitution_compose(typing, prefix, right);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *l = pg_prove_projection(typing, context, pg_substitution_image_at(typing, left, common + i));
		const struct pg_evidence *r = pg_prove_projection(typing, context, pg_substitution_image_at(typing, right, common + i));
		const struct pg_evidence *type = pg_prove_family_identity_type(typing,
			pg_context_declared_input(typing, extensions[i]), ls, rs, i, (struct pg_evidence_inputs){.owner = centers}, l, r);
		context = pg_prove_context_extension(typing, context, binders[i], type);
		if (!context) goto done;
		project_paths(typing, context, i, centers);
		centers[i] = pg_prove_variable(typing, context, binders[i]);
		if (!centers[i]) goto done;
		if (i + 1 < count) {
			l = pg_prove_projection(typing, context, l);
			r = pg_prove_projection(typing, context, r);
			ls = pg_prove_substitution_extension(typing, extensions[i], context, ls, 1, (struct pg_evidence_inputs){.owner = &l});
			rs = pg_prove_substitution_extension(typing, extensions[i], context, rs, 1, (struct pg_evidence_inputs){.owner = &r});
		}
	}
	for (size_t i = 0; i < count; ++i) paths[i] = centers[i];
	result = context;
done:
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *cube_action(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	size_t count, const struct pg_binding_cube *const *cubes,
	const struct pg_dimension_map *order, const struct pg_evidence *term)
{
	if (!count || !cubes || !order || dimensions->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(source, typing)) return NULL;
	if (term && pg_prove_projection(typing, source, term) != term) return NULL;
	if (order->source != order->target) return NULL;
	const struct pg_evidence *prefix = source;
	for (size_t i = 0; i < count; ++i) {
		if (pg_evidence_rule(prefix) != PG_CONTEXT_EXTEND) return NULL;
		if (!cubes[i] || cubes[i]->dimension != order->source) return NULL;
		prefix = pg_context_parent_input(typing, prefix);
	}
	order = pg_dimension_face(dimensions, order);
	if (!order) return NULL;
	size_t dimension = order->source, capacity = count;
	if (dimension > SIZE_MAX / sizeof(struct pg_coordinate)) return NULL;
	for (size_t d = 1; d < dimension; ++d) {
		if (capacity > SIZE_MAX / 3) return NULL;
		capacity *= 3;
	}
	if (capacity > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_coordinate *coordinates = pg_alloc(&temporary, dimension * sizeof(*coordinates));
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_binding_face **centers = pg_alloc(&temporary, capacity * sizeof(*centers));
	const struct pg_evidence **paths = pg_alloc(&temporary, capacity * sizeof(*paths));
	const struct pg_evidence *context = NULL, *left, *right;
	if ((dimension && !coordinates) || !centers || !paths || !extensions) goto done;
	context = source;
	for (size_t i = count; i; --i) {
		extensions[i - 1] = context;
		context = pg_context_parent_input(typing, context);
	}
	/* Rename the entire dependent suffix to zero vertices with one checked
	 * substitution. Later declarations use the preceding renamed images. */
	const struct pg_evidence *map = pg_prove_substitution_projection(typing, prefix, context);
	const struct pg_dimension_map *zero = pg_dimension_map(dimensions, 0, dimension, coordinates);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_binding_face *vertex = pg_binding_face(dimensions, cubes[i], zero);
		if (!vertex) { context = NULL; goto done; }
		const struct pg_evidence *previous = context;
		context = pg_prove_context_extension(typing, context, &vertex->variable,
			pg_prove_reindex(typing, map, pg_context_declared_input(typing, extensions[i])));
		if (!context) goto done;
		map = pg_prove_substitution_compose(typing, map, pg_prove_substitution_projection(typing, previous, context));
		map = pg_prove_substitution_pair(typing, map, extensions[i], pg_prove_variable(typing, context, &vertex->variable));
		if (!map) { context = NULL; goto done; }
	}
	if (term) {
		term = pg_prove_reindex(typing, map, term);
		if (!term || !pg_prove_classifier(typing, context, term)) { context = NULL; goto done; }
	}
	for (size_t d = 1, faces = 1; context && d <= dimension; ++d) {
		size_t active = count * faces;
		for (size_t i = 0; i < active; ++i) {
			const struct pg_binding_cube *cube = cubes[i / faces];
			size_t axes;
			if (pg_dimension_cube_coordinates(d - 1, i % faces, coordinates, &axes) != 0) {
				context = NULL; goto done;
			}
			coordinates[d - 1] = (struct pg_coordinate){PG_AXIS, axes++};
			centers[i] = pg_binding_face(dimensions, cube, pg_dimension_map(dimensions, axes, cube->dimension, coordinates));
			centers[i] = pg_binding_permute(dimensions, centers[i], order);
		}
		const struct pg_evidence *type = term ? pg_prove_classifier(typing, context, term) : NULL;
		context = pg_identity_context(typing, dimensions, context, active, centers, &left, &right, paths);
		if (term && context) {
			term = pg_prove_family_action(typing, type, term, left, right, active, paths);
			if (!term) context = NULL;
		}
		if (d < dimension) faces *= 3;
	}
done:
	pg_graph_destroy(&temporary);
	return context ? (term ? term : context) : NULL;
}

const struct pg_evidence *pg_identity_cube_context(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	size_t count, const struct pg_binding_cube *const *cubes, const struct pg_dimension_map *order)
{
	return cube_action(typing, dimensions, source, count, cubes, order, NULL);
}

const struct pg_evidence *pg_identity_cube_action(struct pg_typing *typing,
	struct pg_dimensions *dimensions,
	const struct pg_evidence *source, const struct pg_evidence *term,
	size_t count, const struct pg_binding_cube *const *cubes, const struct pg_dimension_map *order)
{
	if (!pg_evidence_owned_by(term, typing)) return NULL;
	return cube_action(typing, dimensions, source, count, cubes, order, term);
}

const struct pg_evidence *pg_context_restrict(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_dimension_map *face, size_t count,
	const struct pg_binding_face *const *bindings)
{
	if (dimensions->graph != typing->graph || !source || !face) return NULL;
	face = pg_dimension_face(dimensions, face);
	if (!face) return NULL;
	if (pg_evidence_judgement(source) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (count && !bindings) return NULL;
	size_t arity = 0;
	for (const struct pg_context *context = pg_evidence_context(source); context; context = context->parent) ++arity;
	if (arity != count || count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_evidence *result = NULL;
	if (count && !extensions) goto done;
	const struct pg_evidence *prefix = source;
	for (size_t i = count; i; --i) {
		extensions[i - 1] = prefix;
		prefix = pg_context_parent_input(typing, prefix);
	}
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	result = pg_prove_substitution(typing, prefix, empty, 0, NULL);
	for (size_t i = 0; result && i < count; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		if (bindings[i]) {
			if (&bindings[i]->variable != binder) { result = NULL; break; }
			const struct pg_binding_face *restricted = pg_binding_restrict(dimensions, bindings[i], face);
			if (!restricted) { result = NULL; break; }
			binder = &restricted->variable;
		}
		result = pg_prove_substitution_lift(typing, result, extensions[i], binder);
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}
