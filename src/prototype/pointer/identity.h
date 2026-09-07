#ifndef A_PROGRAM_POINTER_IDENTITY_H
#define A_PROGRAM_POINTER_IDENTITY_H

#include "graph.h"

/* Symbolic one-direction reflexive action. Iteration uses the same reference,
 * not a tag per dimension. These constructors do not establish typing or
 * execute a source computation. Type-directed action equations are separate. */
const struct pg_term *pg_identity_action(struct pg_graph *graph, const struct pg_term *source);
int pg_identity_action_view(const struct pg_term *term, const struct pg_term **source);
const struct pg_term *pg_identity_instance(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *left, const struct pg_term *right);
/* Recognize homogeneous (refl A) x y, not an arbitrary family R x y. */
int pg_identity_view(const struct pg_term *term, const struct pg_term **type,
	const struct pg_term **left, const struct pg_term **right);

#endif
