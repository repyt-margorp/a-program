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
};

int pg_dimensions_init(struct pg_dimensions *dimensions, struct pg_graph *graph);
/* Frees the index; maps remain owned by graph. */
void pg_dimensions_destroy(struct pg_dimensions *dimensions);
const struct pg_dimension_map *pg_dimension_map(struct pg_dimensions *dimensions,
	size_t source, size_t target, const struct pg_coordinate *coordinates);
const struct pg_dimension_map *pg_dimension_identity(struct pg_dimensions *dimensions, size_t dimension);
/* outer : m -> n, inner : l -> m; result : l -> n. */
const struct pg_dimension_map *pg_dimension_compose(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *outer, const struct pg_dimension_map *inner);

#endif
