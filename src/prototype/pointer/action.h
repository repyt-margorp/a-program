#ifndef A_PROGRAM_POINTER_ACTION_H
#define A_PROGRAM_POINTER_ACTION_H

#include "dimension.h"
#include "evidence.h"

/* Extend Gamma by x0 : A, x1 : B, x01 : R x0 x1 for a supplied checked
 * R : Id Universe_i A B in Gamma. Binders may be binding-cube faces, but
 * their pointers alone supply no typing. No transport or new R is inferred. */
const struct pg_evidence *pg_identity_context_extend(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *family, const struct pg_object *left,
	const struct pg_object *right, const struct pg_object *center);
/* Form the expanded homogeneous Identity of a raw computation Pi through
 * ordinary Pi formation over its dependent value boundary. Conversion from
 * the symbolic Identity is a separate check using the fixed pure reducer. */
const struct pg_evidence *pg_identity_pi_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *pi, const struct pg_evidence *left,
	const struct pg_evidence *right, const struct pg_object *x0,
	const struct pg_object *x1, const struct pg_object *path);
/* The same expansion for a Pi family in Gamma,Delta along checked boundary
 * substitutions and the selected paths for Delta. Endpoints inhabit the two
 * substituted Pi types; the generated argument path retains that same family.
 * This composes existing formation rules, not a new Identity axiom. */
const struct pg_evidence *pg_identity_family_pi_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *pi,
	const struct pg_evidence *left_substitution, const struct pg_evidence *right_substitution,
	size_t count, const struct pg_evidence *const *paths,
	const struct pg_evidence *left, const struct pg_evidence *right,
	const struct pg_object *x0, const struct pg_object *x1, const struct pg_object *path);
/* Id_(U C) v0 v1 expands to U(Id_C (FORCE v0) (FORCE v1)).
 * FORCE forms observations; this function does not execute endpoints. */
const struct pg_evidence *pg_identity_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *type,
	const struct pg_evidence *left, const struct pg_evidence *right);

/* Act on the final count declarations, keeping the ambient prefix fixed.
 * Each supplied center is a cube face with a last intrinsic axis to vary.
 * Generate its two endpoints and center in dependency order. On success the
 * outputs are checked substitutions into source and count center variables
 * in the returned context. This declares assumptions, not closed fillers.
 * The output path array is caller-owned; outputs are written only on success. */
const struct pg_evidence *pg_identity_context(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	size_t count, const struct pg_binding_face *const *centers,
	const struct pg_evidence **left, const struct pg_evidence **right,
	const struct pg_evidence **paths);
/* Replace the last declaration of source by the entire boundary telescope of
 * cube in the specified axis order, including its assumed center.
 * order must be a permutation of the cube axes. Repeats the ordinary context action;
 * no filler, transport or dimension-specific proof rule is introduced. */
const struct pg_evidence *pg_identity_cube_context(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_binding_cube *cube, const struct pg_dimension_map *order);

/* Act on the final image_count images of sigma : Delta -> Gamma along a
 * checked boundary into Delta. Each result is ordinary family-action evidence
 * for that image, not a datatype fibrancy certificate or an equality of maps.
 * The caller owns an image_count output array; it is written only on success.
 * -1 includes unsupported regularity rules and allocation failure. */
int pg_identity_substitution_images(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *substitution,
	const struct pg_evidence *left, const struct pg_evidence *right,
	size_t count, const struct pg_evidence *const *paths, size_t image_count,
	const struct pg_evidence **images);

/* Construct a checked context substitution along a strict face. The binding
 * array follows source declaration order; NULL entries preserve that binder.
 * This preserves supplied typing, but does not assert a higher Identity type
 * for an ordinary variable merely because it belongs to a binding cube. */
const struct pg_evidence *pg_context_restrict(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_dimension_map *face, size_t count,
	const struct pg_binding_face *const *bindings);

#endif
