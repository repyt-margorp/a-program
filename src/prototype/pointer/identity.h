#ifndef A_PROGRAM_POINTER_IDENTITY_H
#define A_PROGRAM_POINTER_IDENTITY_H

#include "graph.h"
struct pg_eval;

enum pg_identity_direction { PG_IDENTITY_RIGHT, PG_IDENTITY_LEFT };
/* One-dimensional fields of a selected universe identification. These build
 * pure value expressions, not ordinary Pi functions or effect requests.
 * Only checked typing can authorize the family's field contract. */
const struct pg_term *pg_identity_transport(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction);
const struct pg_term *pg_identity_lift(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction);

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
/* Fixed pure action equations for curried Lambda/APP, RETURN/THUNK/FORCE and
 * F/U/Pi family bodies. Boundary triples share one direction; repeated refl
 * remains distinct. Pi/U endpoints are retained without execution.
 * Diagonal value transport returns its input; diagonal lifting acts on it.
 * For an acted U(F A) family, all four fields on THUNK(RETURN(v)) reduce
 * to THUNK(RETURN(the corresponding field of the acted A on v)). Other
 * quoted computations are not forced to expose this canonical equation.
 * Unknown sources/families stay neutral. No classifier or proof lookup. */
int pg_identity_dispatch(struct pg_eval *machine);

#endif
