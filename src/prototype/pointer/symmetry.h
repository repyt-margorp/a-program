#ifndef A_PROGRAM_POINTER_SYMMETRY_H
#define A_PROGRAM_POINTER_SYMMETRY_H

#include "dimension.h"
#include "eval.h"

/* Formal permutation action, not a typing certificate. Construction does not
 * reduce identity, fixed prefixes or composition. Maps must be permutations.
 * The operator and its map are owned by graph, not by a dimensions registry.
 * Fixed leading axes are removed during evaluation. Composition extends the
 * shorter permutation with fixed leading axes and demands the argument head;
 * divergence remains pending under a finite evaluation budget. Coordinate
 * copying/composition consumes one traversal transition per axis. Interner
 * lookup and arena
 * allocation remain synchronous; fuel is not a wall-clock bound. */
const struct pg_term *pg_symmetry(struct pg_graph *graph,
	const struct pg_dimension_map *permutation, const struct pg_term *term);
/* Versioned descriptor transport. Names preserve every axis, including fixed
 * prefixes; resolving interns the raw operator, without reduction or typing.
 * The caller supplies name storage. Invalid names do not intern descriptors. */
const char *pg_symmetry_name(const struct pg_object *object, char *buffer, size_t capacity);
const struct pg_object *pg_symmetry_resolve(struct pg_graph *graph, const char *name);
/* Inspect exactly one raw symmetry application, without reduction, allocation
 * or typing. axes is graph-owned and includes fixed prefixes. Outputs remain
 * unchanged on failure. Additional APP arguments are not silently consumed. */
int pg_symmetry_view(const struct pg_term *term, size_t *dimension,
	const size_t **axes, const struct pg_term **argument);
int pg_symmetry_dispatch(struct pg_eval *machine);

#endif
