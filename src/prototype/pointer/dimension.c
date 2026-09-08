#include "dimension.h"
#include "eval.h"

#include <stdlib.h>
#include <string.h>

struct pg_map_entry {
	struct pg_index_entry index;
	struct pg_dimension_map map;
};

struct pg_binding_entry {
	struct pg_index_entry index;
	struct pg_binding_face binding;
};

int pg_dimensions_init(struct pg_dimensions *dimensions, struct pg_graph *graph)
{
	memset(dimensions, 0, sizeof(*dimensions));
	dimensions->graph = graph;
	if (pg_index_init(&dimensions->maps) != 0) return -1;
	if (pg_index_init(&dimensions->binding_faces) != 0) {
		pg_index_destroy(&dimensions->maps);
		return -1;
	}
	return 0;
}

void pg_dimensions_destroy(struct pg_dimensions *dimensions)
{
	pg_index_destroy(&dimensions->maps);
	pg_index_destroy(&dimensions->binding_faces);
	memset(dimensions, 0, sizeof(*dimensions));
}

static int validate(size_t source, size_t target, const struct pg_coordinate *coordinates)
{
	if (target && !coordinates) return -1;
	unsigned char *used = calloc(source ? source : 1, 1);
	if (!used) return -1;
	int result = 0;
	for (size_t i = 0; i < target; ++i) {
		struct pg_coordinate coordinate = coordinates[i];
		switch (coordinate.kind) {
		case PG_ENDPOINT_ZERO:
		case PG_ENDPOINT_ONE:
			if (coordinate.axis != 0) result = -1;
			break;
		case PG_AXIS:
			if (coordinate.axis >= source) {
				result = -1;
				break;
			}
			if (used[coordinate.axis]) result = -1;
			used[coordinate.axis] = 1;
			break;
		default:
			result = -1;
		}
		if (result != 0) break;
	}
	free(used);
	return result;
}

static uint64_t mix(uint64_t hash, size_t value)
{
	return (hash ^ value) * UINT64_C(1099511628211);
}

static int map_equal(const struct pg_dimension_map *map, size_t source,
	size_t target, const struct pg_coordinate *coordinates)
{
	if (map->source != source) return 0;
	if (map->target != target) return 0;
	for (size_t i = 0; i < target; ++i) {
		if (map->coordinates[i].kind != coordinates[i].kind) return 0;
		if (map->coordinates[i].axis != coordinates[i].axis) return 0;
	}
	return 1;
}

const struct pg_dimension_map *pg_dimension_map(struct pg_dimensions *dimensions,
	size_t source, size_t target, const struct pg_coordinate *coordinates)
{
	if (target > SIZE_MAX / sizeof(*coordinates)) return NULL;
	if (target && !coordinates) return NULL;
	uint64_t hash = mix(mix(UINT64_C(14695981039346656037), source), target);
	for (size_t i = 0; i < target; ++i) {
		hash = mix(mix(hash, coordinates[i].kind), coordinates[i].axis);
	}
	for (struct pg_index_entry *candidate = pg_index_candidates(&dimensions->maps, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_map_entry *entry = (const struct pg_map_entry *)candidate;
		if (map_equal(&entry->map, source, target, coordinates)) return &entry->map;
	}
	if (validate(source, target, coordinates) != 0) return NULL;
	struct pg_map_entry *entry = pg_alloc(dimensions->graph, sizeof(*entry));
	if (!entry) return NULL;
	struct pg_coordinate *copy = pg_alloc(dimensions->graph, target * sizeof(*copy));
	if (!copy) return NULL;
	if (target) memcpy(copy, coordinates, target * sizeof(*copy));
	entry->map = (struct pg_dimension_map){source, target, copy};
	if (pg_index_insert(&dimensions->maps, &entry->index, hash) != 0) return NULL;
	return &entry->map;
}

const struct pg_dimension_map *pg_dimension_identity(struct pg_dimensions *dimensions, size_t dimension)
{
	if (dimension > SIZE_MAX / sizeof(struct pg_coordinate)) return NULL;
	struct pg_coordinate *coordinates = calloc(dimension ? dimension : 1, sizeof(*coordinates));
	if (!coordinates) return NULL;
	for (size_t i = 0; i < dimension; ++i) coordinates[i] = (struct pg_coordinate){PG_AXIS, i};
	const struct pg_dimension_map *result = pg_dimension_map(dimensions, dimension, dimension, coordinates);
	free(coordinates);
	return result;
}

const struct pg_dimension_map *pg_dimension_compose(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *outer, const struct pg_dimension_map *inner)
{
	if (!outer || !inner) return NULL;
	if (outer->source != inner->target) return NULL;
	outer = pg_dimension_map(dimensions, outer->source, outer->target, outer->coordinates);
	inner = pg_dimension_map(dimensions, inner->source, inner->target, inner->coordinates);
	if (!outer || !inner) return NULL;
	struct pg_coordinate *coordinates = calloc(outer->target ? outer->target : 1, sizeof(*coordinates));
	if (!coordinates) return NULL;
	for (size_t i = 0; i < outer->target; ++i) {
		coordinates[i] = outer->coordinates[i];
		if (coordinates[i].kind == PG_AXIS) coordinates[i] = inner->coordinates[coordinates[i].axis];
	}
	const struct pg_dimension_map *result = pg_dimension_map(dimensions, inner->source, outer->target, coordinates);
	free(coordinates);
	return result;
}

const struct pg_binding_cube *pg_binding_cube(struct pg_dimensions *dimensions, size_t dimension)
{
	struct pg_binding_cube *cube = pg_alloc(dimensions->graph, sizeof(*cube));
	if (cube) cube->dimension = dimension;
	return cube;
}

const struct pg_dimension_map *pg_dimension_face(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *face)
{
	if (!face) return NULL;
	face = pg_dimension_map(dimensions, face->source, face->target, face->coordinates);
	if (!face) return NULL;
	size_t used_axes = 0;
	for (size_t i = 0; i < face->target; ++i) {
		if (face->coordinates[i].kind == PG_AXIS) used_axes++;
	}
	return used_axes == face->source ? face : NULL;
}

const struct pg_binding_face *pg_binding_face(struct pg_dimensions *dimensions,
	const struct pg_binding_cube *cube, const struct pg_dimension_map *face)
{
	if (!cube || !face) return NULL;
	if (cube->dimension != face->target) return NULL;
	face = pg_dimension_face(dimensions, face);
	if (!face) return NULL;
	uint64_t hash = mix(mix(UINT64_C(14695981039346656037), (uintptr_t)cube), (uintptr_t)face);
	for (struct pg_index_entry *candidate = pg_index_candidates(&dimensions->binding_faces, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_binding_entry *entry = (const struct pg_binding_entry *)candidate;
		if (entry->binding.cube != cube) continue;
		if (entry->binding.face == face) return &entry->binding;
	}
	struct pg_binding_entry *entry = pg_alloc(dimensions->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->binding = (struct pg_binding_face){{.kind = PG_BINDER}, cube, face};
	if (pg_index_insert(&dimensions->binding_faces, &entry->index, hash) != 0) return NULL;
	return &entry->binding;
}

const struct pg_binding_face *pg_binding_restrict(struct pg_dimensions *dimensions,
	const struct pg_binding_face *binding, const struct pg_dimension_map *face)
{
	if (!binding) return NULL;
	const struct pg_dimension_map *composite = pg_dimension_compose(dimensions, binding->face, face);
	return pg_binding_face(dimensions, binding->cube, composite);
}

const struct pg_binding_face *pg_binding_permute(struct pg_dimensions *dimensions,
	const struct pg_binding_face *binding, const struct pg_dimension_map *permutation)
{
	if (!binding || !permutation) return NULL;
	if (permutation->source != binding->cube->dimension || permutation->target != binding->cube->dimension) return NULL;
	permutation = pg_dimension_face(dimensions, permutation);
	if (!permutation) return NULL;
	return pg_binding_face(dimensions, binding->cube,
		pg_dimension_compose(dimensions, permutation, binding->face));
}

const struct pg_term *pg_term_restrict_bindings(struct pg_dimensions *dimensions,
	const struct pg_term *term, const struct pg_dimension_map *face,
	size_t count, const struct pg_binding_face *const *bindings)
{
	if (!term || !face) return NULL;
	face = pg_dimension_face(dimensions, face);
	if (!face) return NULL;
	if (count && !bindings) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_binding_value)) return NULL;
	struct pg_binding_value *values = calloc(count ? count : 1, sizeof(*values));
	if (!values) return NULL;
	size_t changed = 0;
	const struct pg_term *result = NULL;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_binding_face *restricted = pg_binding_restrict(dimensions, bindings[i], face);
		if (!restricted) goto done;
		if (restricted == bindings[i]) continue;
		const struct pg_term *value = pg_reference(dimensions->graph, &restricted->variable);
		if (!value) goto done;
		values[changed++] = (struct pg_binding_value){&bindings[i]->variable, value};
	}
	result = pg_term_substitute(dimensions->graph, term, changed, values);
done:
	free(values);
	return result;
}
