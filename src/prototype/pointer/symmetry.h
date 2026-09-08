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
int pg_symmetry_dispatch(struct pg_eval *machine);

#endif
