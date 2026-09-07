#ifndef A_PROGRAM_POINTER_IDENTITY_H
#define A_PROGRAM_POINTER_IDENTITY_H

#include "graph.h"
struct pg_eval;

/* Symbolic one-direction reflexive action. Iteration uses the same reference,
 * not a tag per dimension. These constructors do not establish typing or
 * execute a source computation. Fixed action equations are dispatched below. */
const struct pg_term *pg_identity_action(struct pg_graph *graph, const struct pg_term *source);
int pg_identity_action_view(const struct pg_term *term, const struct pg_term **source);
const struct pg_term *pg_identity_instance(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *left, const struct pg_term *right);
/* Apply a function/family action to a complete boundary triple. This only
 * constructs Core; accepted typing must supply the chosen center witness. */
const struct pg_term *pg_identity_apply(struct pg_graph *graph, const struct pg_term *function,
	const struct pg_term *left, const struct pg_term *right, const struct pg_term *witness);
/* Recognize homogeneous (refl A) x y, not an arbitrary family R x y. */
int pg_identity_view(const struct pg_term *term, const struct pg_term **type,
	const struct pg_term **left, const struct pg_term **right);
/* Fixed pure action equations for curried Lambda/APP, RETURN/THUNK and
 * F/U/Pi family bodies. Boundary triples share one direction; repeated refl
 * remains distinct. Pi endpoints are retained without execution.
 * Unknown sources/families stay neutral. No classifier or proof lookup. */
int pg_identity_dispatch(struct pg_eval *machine);

#endif
