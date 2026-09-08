#ifndef A_PROGRAM_POINTER_SYMMETRY_H
#define A_PROGRAM_POINTER_SYMMETRY_H

#include "dimension.h"
#include "eval.h"

/* Formal permutation action, not a typing certificate. Construction does not
 * reduce identity or composition. Maps must be dimension-preserving faces.
 * The operator and its map are owned by graph, not by a dimensions registry.
 * Nonidentity elimination demands the argument's head before composing;
 * divergence remains pending under a finite evaluation budget. Composition
 * consumes one traversal transition per axis. Structural interning and arena
 * allocation remain synchronous; fuel is not a wall-clock bound. */
const struct pg_term *pg_symmetry(struct pg_graph *graph,
	const struct pg_dimension_map *permutation, const struct pg_term *term);
int pg_symmetry_dispatch(struct pg_eval *machine);

#endif
