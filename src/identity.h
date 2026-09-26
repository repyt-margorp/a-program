#ifndef A_PROGRAM_POINTER_IDENTITY_H
#define A_PROGRAM_POINTER_IDENTITY_H

#include "graph.h"
struct pg_eval;

enum pg_identity_direction { PG_IDENTITY_RIGHT, PG_IDENTITY_LEFT };
/* Fixed owner-controlled graph descriptors, not Identity evidence. Unknown
 * names/objects fail; names are borrowed static strings with explicit versions. */
const char *pg_identity_name(const struct pg_object *object);
const struct pg_object *pg_identity_resolve(const char *name);
/* One-dimensional fields of a selected universe identification. These build
 * pure value expressions, not ordinary Pi functions or effect requests.
 * Only checked typing can authorize the family's field contract. */
const struct pg_term *pg_identity_transport(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction);
const struct pg_term *pg_identity_lift(struct pg_graph *graph,
	const struct pg_term *family, const struct pg_term *value, enum pg_identity_direction direction);
/* Read an exact transport/lift application without evaluation or Evidence.
 * Outputs are optional and unchanged on failure. This establishes no typing. */
int pg_identity_field_view(const struct pg_term *term,
	const struct pg_term **family, const struct pg_term **value,
	enum pg_identity_direction *direction, int *lift);

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
 * Complete supplied triples can simplify a curried prefix without waiting
 * for later arguments; an incomplete triple is not consumed.
 * When an outer action receives a complete triple and its source is an
 * iterated action of known leading Lambdas, expose all their boundary
 * arguments by eta expansion and reuse ordinary scoped action. Construction
 * is suspended per binder/application; opaque sources stay neutral. This
 * does not infer a function from the number of supplied APP arguments alone.
 * Diagonal value transport returns its input; diagonal lifting acts on it.
 * For an acted U(F A) family, all four fields on THUNK(RETURN(v)) reduce
 * to THUNK(RETURN(the corresponding field of the acted A on v)). Other
 * thunks transport by THUNK(FOLD(FORCE(u), lambda x. RETURN(tr A x))).
 * Quoted computations are never forced while constructing this map;
 * general lifting on non-returned thunks remains neutral.
 * Unknown sources/families stay neutral. No classifier or proof lookup. */
int pg_identity_dispatch(struct pg_eval *machine);
/* FORCE's demanded answer: unfold U(Pi x:A. C) transport only when an
 * application argument is present. Bare transported functions stay neutral. */
int pg_identity_force(struct pg_eval *machine, const struct pg_term *value);

/* Resolve only this owner's known, versioned evaluator continuations. */
struct pg_eval_continuation;
const struct pg_eval_continuation *pg_identity_continuation_resolve(const char *name);
const struct pg_eval_work_operation *pg_identity_work_resolve(const char *name);

#endif
