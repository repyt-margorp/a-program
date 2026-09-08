#include "action.h"

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

const struct pg_evidence *pg_identity_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(formation, typing)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	switch (pg_evidence_judgement(formation)) {
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: break;
	default: return NULL;
	}
	const struct pg_evidence *map = NULL;
	for (;;) {
		const struct pg_evidence *step;
		enum pg_evidence_rule rule = pg_evidence_rule(formation);
		if (rule == PG_PURE_NORMALIZATION) {
			formation = pg_evidence_premise(formation, 0);
			continue;
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
	enum pg_evidence_rule rule = pg_evidence_rule(formation);
	switch (rule) {
	case PG_IDENTITY_FORM: case PG_IDENTITY_INSTANCE: case PG_FAMILY_IDENTITY_FORM: break;
	default: return NULL;
	}
	if (!map) return formation;
	if (rule != PG_FAMILY_IDENTITY_FORM) {
		const struct pg_evidence *family = pg_prove_reindex(typing, map, pg_evidence_premise(formation, 0));
		const struct pg_evidence *left = pg_prove_reindex(typing, map, pg_evidence_premise(formation, 1));
		const struct pg_evidence *right = pg_prove_reindex(typing, map, pg_evidence_premise(formation, 2));
		return rule == PG_IDENTITY_FORM ? pg_prove_identity_type(typing, family, left, right)
			: pg_prove_identity_instance(typing, classifiers, family, left, right);
	}
	size_t count = pg_evidence_premise_count(formation) - 5;
	struct pg_graph temporary = {0};
	const struct pg_evidence **paths = pg_alloc(&temporary, count * sizeof(*paths));
	if (count && !paths) { pg_graph_destroy(&temporary); return NULL; }
	for (size_t i = 0; i < count; ++i)
		paths[i] = pg_prove_reindex(typing, map, pg_evidence_premise(formation, i + 3));
	const struct pg_evidence *left = pg_prove_reindex(typing, map, pg_evidence_premise(formation, count + 3));
	const struct pg_evidence *right = pg_prove_reindex(typing, map, pg_evidence_premise(formation, count + 4));
	const struct pg_evidence *ls = pg_prove_substitution_compose(typing, pg_evidence_premise(formation, 1), map);
	const struct pg_evidence *rs = pg_prove_substitution_compose(typing, pg_evidence_premise(formation, 2), map);
	const struct pg_evidence *result = pg_prove_family_identity_type(typing,
		pg_evidence_premise(formation, 0), ls, rs, count, paths, left, right);
	pg_graph_destroy(&temporary);
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
