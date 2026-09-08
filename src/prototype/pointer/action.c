#include "action.h"
#include <stdlib.h>

int pg_identity_substitution_images(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *substitution,
	const struct pg_evidence *left, const struct pg_evidence *right,
	size_t count, const struct pg_evidence *const *paths, size_t image_count,
	const struct pg_evidence **images)
{
	if (!pg_evidence_owned_by(substitution, typing)) return -1;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return -1;
	size_t total = pg_evidence_premise_count(substitution) - 2;
	if (image_count > total || (image_count && !images)) return -1;
	if (image_count > SIZE_MAX / sizeof(*images)) return -1;
	struct pg_graph temporary = {0};
	const struct pg_evidence **results = pg_alloc(&temporary, image_count * sizeof(*results));
	int status = -1;
	if (image_count && !results) goto done;
	const struct pg_evidence *context = pg_evidence_premise(substitution, 1);
	for (size_t i = 0; i < image_count; ++i) {
		const struct pg_evidence *value = pg_evidence_premise(substitution, 2 + total - image_count + i);
		const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, value);
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

static const struct pg_evidence *projection_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination)
{
	size_t count = 0;
	for (const struct pg_context *c = pg_evidence_context(source); c; c = c->parent) ++count;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	if (count && !images) { pg_graph_destroy(&temporary); return NULL; }
	const struct pg_context *c = pg_evidence_context(source);
	for (size_t i = count; i; --i, c = c->parent)
		images[i - 1] = pg_prove_variable(typing, destination, c->binder);
	const struct pg_evidence *result = pg_prove_substitution(typing, source, destination, count, images);
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *retained_origin(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence **substitution)
{
	const struct pg_evidence *map = NULL;
	for (;;) {
		const struct pg_evidence *step;
		enum pg_evidence_rule rule = pg_evidence_rule(formation);
		switch (rule) {
		case PG_PURE_NORMALIZATION:
		case PG_TYPE_CONVERSION:
		case PG_TYPE_FROM_VALUE: case PG_VALUE_FROM_TYPE:
			formation = pg_evidence_premise(formation, 0);
			continue;
		default: break;
		}
		if (rule == PG_REINDEX) {
			step = pg_evidence_premise(formation, 0);
			formation = pg_evidence_premise(formation, 1);
		} else if (rule == PG_CONTEXT_PROJECTION) {
			const struct pg_evidence *destination = pg_evidence_premise(formation, 0);
			formation = pg_evidence_premise(formation, 1);
			const struct pg_evidence *source = destination;
			while (pg_evidence_context(source) != pg_evidence_context(formation))
				source = pg_evidence_premise(source, 0);
			step = projection_substitution(typing, source, destination);
		} else break;
		if (!step) return NULL;
		map = map ? pg_prove_substitution_compose(typing, step, map) : step;
		if (!map) return NULL;
	}
	*substitution = map;
	return formation;
}

static const struct pg_evidence *rebuild_family(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_identity_boundary *boundary,
	const struct pg_evidence *map, const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (!map) return pg_prove_family_identity_type(typing, type,
		boundary->left_substitution, boundary->right_substitution,
		boundary->path_count, boundary->paths, left, right);
	size_t count = boundary->path_count;
	struct pg_graph temporary = {0};
	const struct pg_evidence **paths = pg_alloc(&temporary, count * sizeof(*paths));
	if (count && !paths) { pg_graph_destroy(&temporary); return NULL; }
	for (size_t i = 0; i < count; ++i) paths[i] = pg_prove_reindex(typing, map, boundary->paths[i]);
	const struct pg_evidence *ls = pg_prove_substitution_compose(typing, boundary->left_substitution, map);
	const struct pg_evidence *rs = pg_prove_substitution_compose(typing, boundary->right_substitution, map);
	const struct pg_evidence *result = pg_prove_family_identity_type(typing, type, ls, rs, count, paths, left, right);
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_identity_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(formation, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	switch (pg_evidence_judgement(formation)) {
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: break;
	default: return NULL;
	}
	const struct pg_evidence *map = NULL;
	formation = retained_origin(typing, formation, &map);
	if (!formation) return NULL;
	enum pg_evidence_rule rule = pg_evidence_rule(formation);
	struct pg_identity_boundary boundary;
	if (!pg_identity_boundary_view(formation, &boundary)) return NULL;
	/* A checked refl A selects the ordinary Identity family of A. Recover it
	 * from its premise, never from the shape of the family Core application. */
	if (rule == PG_IDENTITY_INSTANCE) {
		const struct pg_evidence *family_map = NULL;
		const struct pg_evidence *family = retained_origin(typing, boundary.family, &family_map);
		if (!family) return NULL;
		if (pg_evidence_rule(family) == PG_REFLEXIVITY) {
			const struct pg_evidence *type = pg_prove_value_type(typing, pg_evidence_premise(family, 1));
			if (family_map) type = pg_prove_reindex(typing, family_map, type);
			formation = pg_prove_identity_type(typing, type, boundary.left, boundary.right);
			if (!pg_identity_boundary_view(formation, &boundary)) return NULL;
			rule = PG_IDENTITY_FORM;
		} else if (pg_evidence_rule(family) == PG_FAMILY_ACTION) {
			struct pg_identity_boundary action;
			if (!pg_identity_boundary_view(pg_evidence_premise(family, 0), &action)) return NULL;
			const struct pg_evidence *type = pg_prove_value_type(typing, pg_evidence_premise(family, 1));
			formation = rebuild_family(typing, type, &action, family_map, boundary.left, boundary.right);
			if (!pg_identity_boundary_view(formation, &boundary)) return NULL;
			rule = PG_FAMILY_IDENTITY_FORM;
		}
	}
	if (!map) return formation;
	if (rule != PG_FAMILY_IDENTITY_FORM) {
		const struct pg_evidence *family = pg_prove_reindex(typing, map, boundary.family);
		const struct pg_evidence *left = pg_prove_reindex(typing, map, boundary.left);
		const struct pg_evidence *right = pg_prove_reindex(typing, map, boundary.right);
		return rule == PG_IDENTITY_FORM ? pg_prove_identity_type(typing, family, left, right)
			: pg_prove_identity_instance(typing, classifiers, family, left, right);
	}
	const struct pg_evidence *left = pg_prove_reindex(typing, map, boundary.left);
	const struct pg_evidence *right = pg_prove_reindex(typing, map, boundary.right);
	return rebuild_family(typing, boundary.family, &boundary, map, left, right);
}

struct endpoint_frame {
	struct endpoint_frame *previous;
	const struct pg_evidence *context;
	struct pg_identity_boundary boundary;
};

struct pg_identity_endpoint_work {
	struct pg_graph temporary;
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	const struct pg_evidence *context, *formation, *result;
	struct endpoint_frame *stack;
	size_t depth;
	enum pg_identity_direction side;
	int failed;
};

struct pg_identity_endpoint_work *pg_identity_endpoint_init(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, size_t depth, enum pg_identity_direction side)
{
	if (!pg_evidence_owned_by(context, typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_evidence_owned_by(formation, typing)) return NULL;
	if (pg_evidence_context(context) != pg_evidence_context(formation)) return NULL;
	if (side != PG_IDENTITY_LEFT && side != PG_IDENTITY_RIGHT) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	struct pg_identity_endpoint_work *work = calloc(1, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->classifiers = classifiers;
	work->context = context;
	work->formation = formation;
	work->depth = depth;
	work->side = side;
	return work;
}

static int endpoint_step(struct pg_identity_endpoint_work *work)
{
	struct pg_typing *typing = work->typing;
	if (!work->result) {
		const struct pg_evidence *formation = pg_identity_formation(typing, work->classifiers, work->formation);
		struct pg_identity_boundary boundary;
		if (!pg_identity_boundary_view(formation, &boundary)) return -1;
		if (!work->depth) {
			work->result = work->side == PG_IDENTITY_LEFT ? boundary.left : boundary.right;
			return 0;
		}
		if (pg_evidence_rule(formation) == PG_IDENTITY_INSTANCE) return -1;
		struct endpoint_frame *frame = pg_alloc(&work->temporary, sizeof(*frame));
		if (!frame) return -1;
		*frame = (struct endpoint_frame){work->stack, work->context, boundary};
		work->stack = frame;
		if (boundary.left_substitution) work->context = pg_evidence_premise(boundary.left_substitution, 0);
		work->formation = boundary.family;
		--work->depth;
		return 0;
	}
	const struct endpoint_frame *frame = work->stack;
	const struct pg_evidence *type = pg_prove_classifier(typing, work->classifiers, work->context, work->result);
	const struct pg_identity_boundary *boundary = &frame->boundary;
	work->result = boundary->left_substitution
		? pg_prove_family_action(typing, type, work->result, boundary->left_substitution,
			boundary->right_substitution, boundary->path_count, boundary->paths)
		: pg_prove_reflexivity(typing, type, work->result);
	if (!work->result) return -1;
	work->context = frame->context;
	work->stack = frame->previous;
	return 0;
}

int pg_identity_endpoint_advance(struct pg_identity_endpoint_work *work, uint64_t fuel)
{
	if (!work || work->failed) return -1;
	while (!work->result || work->stack) {
		if (!fuel) return 0;
		--fuel;
		if (endpoint_step(work) < 0) { work->failed = 1; return -1; }
	}
	return 1;
}

const struct pg_evidence *pg_identity_endpoint_result(const struct pg_identity_endpoint_work *work)
{
	return work && !work->failed && !work->stack ? work->result : NULL;
}

void pg_identity_endpoint_destroy(struct pg_identity_endpoint_work *work)
{
	if (!work) return;
	pg_graph_destroy(&work->temporary);
	free(work);
}

const struct pg_evidence *pg_identity_face_endpoint(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, size_t depth, enum pg_identity_direction side)
{
	struct pg_identity_endpoint_work *work = pg_identity_endpoint_init(typing, classifiers, context, formation, depth, side);
	while (pg_identity_endpoint_advance(work, UINT64_MAX) == 0) {}
	const struct pg_evidence *result = pg_identity_endpoint_result(work);
	pg_identity_endpoint_destroy(work);
	return result;
}

struct pg_identity_face_work {
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	const struct pg_evidence *context, *formation, *layer, *result;
	const struct pg_dimension_map *face;
	struct pg_identity_endpoint_work *endpoint;
	size_t checked, axes, next, retained, remaining;
	int failed;
};

struct pg_identity_face_work *pg_identity_face_init(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, const struct pg_dimension_map *face)
{
	if (!face || face->source >= face->target || !face->coordinates) return NULL;
	if (!pg_evidence_owned_by(context, typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_evidence_owned_by(formation, typing)) return NULL;
	if (pg_evidence_context(context) != pg_evidence_context(formation)) return NULL;
	struct pg_identity_face_work *work = calloc(1, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->classifiers = classifiers;
	work->context = context;
	work->formation = work->layer = formation;
	work->face = face;
	work->next = face->target;
	work->remaining = face->target - face->source;
	return work;
}

static int face_step(struct pg_identity_face_work *work)
{
	const struct pg_dimension_map *face = work->face;
	if (work->checked < face->target) {
		struct pg_coordinate coordinate = face->coordinates[work->checked];
		switch (coordinate.kind) {
		case PG_AXIS:
			if (coordinate.axis != work->axes++) return -1;
			break;
		case PG_ENDPOINT_ZERO: case PG_ENDPOINT_ONE: break;
		default: return -1;
		}
		const struct pg_evidence *layer = pg_identity_formation(work->typing, work->classifiers, work->layer);
		struct pg_identity_boundary boundary;
		if (!pg_identity_boundary_view(layer, &boundary)) return -1;
		work->layer = boundary.family;
		++work->checked;
		return 0;
	}
	if (work->axes != face->source) return -1;
	if (!work->endpoint) {
		if (!work->next) return -1;
		enum pg_coordinate_kind kind = face->coordinates[--work->next].kind;
		if (kind == PG_AXIS) { ++work->retained; return 0; }
		work->endpoint = pg_identity_endpoint_init(work->typing, work->classifiers,
			work->context, work->formation, work->retained,
			kind == PG_ENDPOINT_ZERO ? PG_IDENTITY_LEFT : PG_IDENTITY_RIGHT);
		if (!work->endpoint) return -1;
		return 0;
	}
	int status = pg_identity_endpoint_advance(work->endpoint, 1);
	if (status <= 0) return status;
	const struct pg_evidence *result = pg_identity_endpoint_result(work->endpoint);
	pg_identity_endpoint_destroy(work->endpoint);
	work->endpoint = NULL;
	if (!--work->remaining) { work->result = result; return 1; }
	work->formation = pg_prove_classifier(work->typing, work->classifiers, work->context, result);
	return work->formation ? 0 : -1;
}

int pg_identity_face_advance(struct pg_identity_face_work *work, uint64_t fuel)
{
	if (!work || work->failed) return -1;
	if (work->result) return 1;
	while (fuel--) {
		int status = face_step(work);
		if (status < 0) work->failed = 1;
		if (status) return status;
	}
	return 0;
}

const struct pg_evidence *pg_identity_face_result(const struct pg_identity_face_work *work)
{
	return work && !work->failed ? work->result : NULL;
}

void pg_identity_face_destroy(struct pg_identity_face_work *work)
{
	if (!work) return;
	pg_identity_endpoint_destroy(work->endpoint);
	free(work);
}

const struct pg_evidence *pg_identity_proper_face(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, const struct pg_dimension_map *face)
{
	struct pg_identity_face_work *work = pg_identity_face_init(typing, classifiers, context, formation, face);
	while (pg_identity_face_advance(work, UINT64_MAX) == 0) {}
	const struct pg_evidence *result = pg_identity_face_result(work);
	pg_identity_face_destroy(work);
	return result;
}

const struct pg_evidence *pg_identity_context_extend(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *family, const struct pg_object *left,
	const struct pg_object *right, const struct pg_object *center)
{
	const struct pg_evidence *left_type = pg_prove_identity_endpoint_type(typing,
		classifiers, family, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *right_type = pg_prove_identity_endpoint_type(typing,
		classifiers, family, PG_IDENTITY_RIGHT_TYPE);
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
		classifiers, family, x0, x1);
	return pg_prove_context_extension(typing, context, center, center_type);
}

const struct pg_evidence *pg_identity_pi_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *pi, const struct pg_evidence *left,
	const struct pg_evidence *right, const struct pg_object *x0,
	const struct pg_object *x1, const struct pg_object *path)
{
	const struct pg_evidence *identity = projection_substitution(typing, context, context);
	return pg_identity_family_pi_type(typing, classifiers, pi, identity, identity,
		0, NULL, left, right, x0, x1, path);
}

const struct pg_evidence *pg_identity_family_pi_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *pi,
	const struct pg_evidence *left_substitution, const struct pg_evidence *right_substitution,
	size_t count, const struct pg_evidence *const *paths,
	const struct pg_evidence *left, const struct pg_evidence *right,
	const struct pg_object *x0, const struct pg_object *x1, const struct pg_object *path)
{
	if (!pg_prove_family_identity_type(typing, pi, left_substitution, right_substitution,
		count, paths, left, right)) return NULL;
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
	const struct pg_evidence *prefix = projection_substitution(typing, context, boundary);
	const struct pg_evidence *ls = pg_prove_substitution_compose(typing, left_substitution, prefix);
	const struct pg_evidence *rs = pg_prove_substitution_compose(typing, right_substitution, prefix);
	for (size_t i = 0; i < count; ++i) centers[i] = pg_prove_projection(typing, boundary, paths[i]);
	const struct pg_evidence *center_type = pg_prove_family_identity_type(typing, domain, ls, rs,
		count, centers, pg_prove_variable(typing, boundary, x0), pg_prove_variable(typing, boundary, x1));
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
	prefix = projection_substitution(typing, context, boundary);
	ls = pg_prove_substitution_pair(typing,
		pg_prove_substitution_compose(typing, left_substitution, prefix), source, l);
	rs = pg_prove_substitution_pair(typing,
		pg_prove_substitution_compose(typing, right_substitution, prefix), source, r);
	body = pg_prove_family_identity_type(typing, codomain, ls, rs, count + 1, centers,
		pg_prove_application(typing, pg_prove_projection(typing, boundary, left), l),
		pg_prove_application(typing, pg_prove_projection(typing, boundary, right), r));
	for (size_t i = 0; body && i < 3; ++i) {
		body = pg_prove_pi(typing, classifiers, pg_evidence_premise(boundary, 1), boundary, body);
		boundary = pg_evidence_premise(boundary, 0);
	}
done:
	pg_graph_destroy(&temporary);
	return body;
}

const struct pg_evidence *pg_identity_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *type,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (!pg_prove_identity_type(typing, type, left, right)) return NULL;
	const struct pg_evidence *content = pg_prove_thunk_content(typing, type);
	const struct pg_evidence *identity = pg_prove_identity_type(typing, content,
		pg_prove_force(typing, left), pg_prove_force(typing, right));
	return pg_prove_thunk_type(typing, classifiers, identity);
}

static void project_boundary(struct pg_typing *typing, const struct pg_evidence *context,
	size_t count, const struct pg_evidence **left, const struct pg_evidence **right,
	size_t path_count, const struct pg_evidence **paths)
{
	for (size_t i = 0; i < count; ++i) {
		left[i] = pg_prove_projection(typing, context, left[i]);
		right[i] = pg_prove_projection(typing, context, right[i]);
	}
	for (size_t i = 0; i < path_count; ++i) paths[i] = pg_prove_projection(typing, context, paths[i]);
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
	size_t common = arity - count;
	struct pg_graph temporary = {0};
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_evidence **centers_proof = pg_alloc(&temporary, count * sizeof(*centers_proof));
	const struct pg_evidence **li = pg_alloc(&temporary, arity * sizeof(*li));
	const struct pg_evidence **ri = pg_alloc(&temporary, arity * sizeof(*ri));
	const struct pg_evidence *result = NULL;
	if (count && (!extensions || !centers_proof)) goto done;
	if (arity && (!li || !ri)) goto done;
	const struct pg_evidence *context = source;
	for (size_t i = count; i; --i) {
		extensions[i - 1] = context;
		context = pg_evidence_premise(context, 0);
	}
	const struct pg_evidence *ls = projection_substitution(typing, context, context), *rs = ls;
	if (!ls) goto done;
	for (size_t i = 0; i < common; ++i) li[i] = ri[i] = pg_evidence_premise(ls, i + 2);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_object *binders[3];
		if (boundary_binders(dimensions, centers[i], binders) != 0) goto done;
		const struct pg_evidence *type = pg_evidence_premise(extensions[i], 1);
		const struct pg_evidence *lt = pg_prove_reindex(typing, ls, type);
		const struct pg_evidence *rt = pg_prove_reindex(typing, rs, type);
		context = pg_prove_context_extension(typing, context, binders[0], lt);
		context = pg_prove_context_extension(typing, context, binders[1], pg_prove_projection(typing, context, rt));
		if (!context) goto done;
		project_boundary(typing, context, common + i, li, ri, i, centers_proof);
		const struct pg_evidence *prefix = pg_evidence_premise(extensions[i], 0);
		ls = pg_prove_substitution(typing, prefix, context, common + i, li);
		rs = pg_prove_substitution(typing, prefix, context, common + i, ri);
		const struct pg_evidence *l = pg_prove_variable(typing, context, binders[0]);
		const struct pg_evidence *r = pg_prove_variable(typing, context, binders[1]);
		const struct pg_evidence *center_type = pg_prove_family_identity_type(typing, type, ls, rs, i, centers_proof, l, r);
		context = pg_prove_context_extension(typing, context, binders[2], center_type);
		if (!context) goto done;
		project_boundary(typing, context, common + i, li, ri, i, centers_proof);
		li[common + i] = pg_prove_variable(typing, context, binders[0]);
		ri[common + i] = pg_prove_variable(typing, context, binders[1]);
		centers_proof[i] = pg_prove_variable(typing, context, binders[2]);
		ls = pg_prove_substitution(typing, extensions[i], context, common + i + 1, li);
		rs = pg_prove_substitution(typing, extensions[i], context, common + i + 1, ri);
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

static const struct pg_evidence *cube_action(struct pg_typing *typing, struct pg_classifiers *classifiers,
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
		prefix = pg_evidence_premise(prefix, 0);
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
		context = pg_evidence_premise(context, 0);
	}
	/* Rename the entire dependent suffix to zero vertices with one checked
	 * substitution. Later declarations use the preceding renamed images. */
	const struct pg_evidence *map = projection_substitution(typing, prefix, context);
	const struct pg_dimension_map *zero = pg_dimension_map(dimensions, 0, dimension, coordinates);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_binding_face *vertex = pg_binding_face(dimensions, cubes[i], zero);
		if (!vertex) { context = NULL; goto done; }
		const struct pg_evidence *previous = context;
		context = pg_prove_context_extension(typing, context, &vertex->variable,
			pg_prove_reindex(typing, map, pg_evidence_premise(extensions[i], 1)));
		if (!context) goto done;
		map = pg_prove_substitution_compose(typing, map, projection_substitution(typing, previous, context));
		map = pg_prove_substitution_pair(typing, map, extensions[i], pg_prove_variable(typing, context, &vertex->variable));
		if (!map) { context = NULL; goto done; }
	}
	if (term) {
		term = pg_prove_reindex(typing, map, term);
		if (!term || !pg_prove_classifier(typing, classifiers, context, term)) { context = NULL; goto done; }
	}
	for (size_t d = 1, faces = 1; context && d <= dimension; ++d) {
		size_t active = count * faces;
		for (size_t i = 0; i < active; ++i) {
			const struct pg_binding_cube *cube = cubes[i / faces];
			size_t axes = 0, divisor = faces / 3;
			for (size_t j = 0; j + 1 < d; ++j, divisor /= 3) {
				size_t digit = (i / divisor) % 3;
				coordinates[j] = digit == 2 ? (struct pg_coordinate){PG_AXIS, axes++}
					: (struct pg_coordinate){digit ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
			}
			coordinates[d - 1] = (struct pg_coordinate){PG_AXIS, axes++};
			centers[i] = pg_binding_face(dimensions, cube, pg_dimension_map(dimensions, axes, cube->dimension, coordinates));
			centers[i] = pg_binding_permute(dimensions, centers[i], order);
		}
		const struct pg_evidence *type = term ? pg_prove_classifier(typing, classifiers, context, term) : NULL;
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
	return cube_action(typing, NULL, dimensions, source, count, cubes, order, NULL);
}

const struct pg_evidence *pg_identity_cube_action(struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_dimensions *dimensions,
	const struct pg_evidence *source, const struct pg_evidence *term,
	size_t count, const struct pg_binding_cube *const *cubes, const struct pg_dimension_map *order)
{
	if (!classifiers || !pg_evidence_owned_by(term, typing)) return NULL;
	return cube_action(typing, classifiers, dimensions, source, count, cubes, order, term);
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
		prefix = pg_evidence_premise(prefix, 0);
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
