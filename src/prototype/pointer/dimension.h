#ifndef A_PROGRAM_POINTER_DIMENSION_H
#define A_PROGRAM_POINTER_DIMENSION_H

#include "graph.h"

enum pg_coordinate_kind { PG_ENDPOINT_ZERO, PG_ENDPOINT_ONE, PG_AXIS };
struct pg_coordinate {
	enum pg_coordinate_kind kind;
	size_t axis;
};

/* m -> n has n coordinates in the m-dimensional source. Axes cannot repeat. */
struct pg_dimension_map {
	size_t source;
	size_t target;
	const struct pg_coordinate *coordinates;
};

struct pg_dimensions {
	struct pg_graph *graph;
	struct pg_index maps;
	struct pg_index binding_faces;
};

struct pg_binding_cube {
	size_t dimension;
};

/* One variable of a cube's dependent boundary telescope. */
struct pg_binding_face {
	struct pg_object variable;
	const struct pg_binding_cube *cube;
	const struct pg_dimension_map *face;
};

int pg_dimensions_init(struct pg_dimensions *dimensions, struct pg_graph *graph);
/* Frees the index; maps remain owned by graph. */
void pg_dimensions_destroy(struct pg_dimensions *dimensions);
const struct pg_dimension_map *pg_dimension_map(struct pg_dimensions *dimensions,
	size_t source, size_t target, const struct pg_coordinate *coordinates);
const struct pg_dimension_map *pg_dimension_identity(struct pg_dimensions *dimensions, size_t dimension);
/* Validate/intern a map and require every source axis: no degeneracy. */
const struct pg_dimension_map *pg_dimension_face(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *map);
/* outer : m -> n, inner : l -> m; result : l -> n. */
const struct pg_dimension_map *pg_dimension_compose(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *outer, const struct pg_dimension_map *inner);

const struct pg_binding_cube *pg_binding_cube(struct pg_dimensions *dimensions, size_t dimension);
/* Faces use every source axis. Degeneracies are actions, not new variables. */
const struct pg_binding_face *pg_binding_face(struct pg_dimensions *dimensions,
	const struct pg_binding_cube *cube, const struct pg_dimension_map *face);
const struct pg_binding_face *pg_binding_restrict(struct pg_dimensions *dimensions,
	const struct pg_binding_face *binding, const struct pg_dimension_map *face);

/* Substitute the listed free boundary bindings along a strict face. Other
 * references are unchanged. This is syntactic restriction, not typed Act,
 * transport, or restriction of an opaque semantic object's internal data. */
const struct pg_term *pg_term_restrict_bindings(struct pg_dimensions *dimensions,
	const struct pg_term *term, const struct pg_dimension_map *face,
	size_t count, const struct pg_binding_face *const *bindings);

#endif
