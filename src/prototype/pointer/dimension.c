#include "dimension.h"

#include <stdlib.h>
#include <string.h>

struct pg_map_entry {
	struct pg_index_entry index;
	struct pg_dimension_map map;
};

int pg_dimensions_init(struct pg_dimensions *dimensions, struct pg_graph *graph)
{
	memset(dimensions, 0, sizeof(*dimensions));
	dimensions->graph = graph;
	return pg_index_init(&dimensions->maps);
}

void pg_dimensions_destroy(struct pg_dimensions *dimensions)
{
	pg_index_destroy(&dimensions->maps);
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
	if (validate(source, target, coordinates) != 0) return NULL;
	uint64_t hash = mix(mix(UINT64_C(14695981039346656037), source), target);
	for (size_t i = 0; i < target; ++i) {
		hash = mix(mix(hash, coordinates[i].kind), coordinates[i].axis);
	}
	for (struct pg_index_entry *candidate = pg_index_candidates(&dimensions->maps, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_map_entry *entry = (const struct pg_map_entry *)candidate;
		if (map_equal(&entry->map, source, target, coordinates)) return &entry->map;
	}
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
