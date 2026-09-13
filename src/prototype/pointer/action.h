#ifndef A_PROGRAM_POINTER_ACTION_H
#define A_PROGRAM_POINTER_ACTION_H

#include "dimension.h"
#include "evidence.h"

/* Recover an explicit Identity formation through reindex/projection and
 * accepted pure-normalization and type-as-universe-value premises.
 * The input must be a type formation. Following a universe value without a
 * retained formation does not infer an Identity from its Core syntax. An
 * instance of an explicitly proved refl A recovers ordinary Identity on A
 * from that proof's premise, allowing descent through nested instances.
 * Explicit family action likewise recovers its source type, selected maps and
 * paths. Accepted classifier conversions preserve the term's origin; no
 * arbitrary equality witness is treated as reflexivity or family action.
 * Rebuild with the ordinary formation rules and composed substitutions, keeping
 * the selected family and paths. This is not normalization or proof search:
 * unsupported formation/conversion rules return NULL. The rebuilt subject is
 * convertible, not necessarily alpha-equivalent, to the input subject. Callers
 * must retain the input derivation and check conversion when typing at that
 * input; this result does not certify a new equality or a shared Core pointer.
 * Work is synchronous; no new acceptance rule or cached boundary authority. */
const struct pg_evidence *pg_identity_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation);

/* The same recovery suspended between retained origin wrappers. No work at
 * request time and no partial result. Returns 0 pending, 1 done, -1 unsupported
 * or allocation failure; failure does not assert uninhabitance. Primitive
 * proof reconstruction remains synchronous. Destroying work retains evidence. */
struct pg_identity_formation_work;
struct pg_identity_formation_work *pg_identity_formation_init(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation);
int pg_identity_formation_advance(struct pg_identity_formation_work *work, uint64_t fuel);
const struct pg_evidence *pg_identity_formation_result(const struct pg_identity_formation_work *work);
void pg_identity_formation_destroy(struct pg_identity_formation_work *work);

/* Select a codimension-one endpoint of an explicit iterated Identity.
 * depth zero selects the outermost direction; larger depths descend through
 * retained family formations and act the endpoint back through those families.
 * Does not inspect APP arity or manufacture a center. Opaque selected families
 * without an inner formation return NULL. Work is synchronous and uses an
 * explicit stack. The result is checked evidence, not a conversion certificate. */
const struct pg_evidence *pg_identity_face_endpoint(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, size_t depth, enum pg_identity_direction side);

/* The same derivation construction, suspended between descent/unwind steps.
 * advance: 0 pending, 1 complete, -1 unsupported or allocation failure (not a
 * proof of uninhabitance). No partial result is exposed. Retained formation
 * and selected-family wrappers consume one transition each. Projection
 * construction and individual proof-rule applications remain synchronous;
 * fuel does not bound their cost. The typing/classifier stores must outlive
 * this work. Destroying pending work does not retract accepted premises. */
struct pg_identity_endpoint_work;
struct pg_identity_endpoint_work *pg_identity_endpoint_init(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, size_t depth, enum pg_identity_direction side);
int pg_identity_endpoint_advance(struct pg_identity_endpoint_work *work, uint64_t fuel);
const struct pg_evidence *pg_identity_endpoint_result(const struct pg_identity_endpoint_work *work);
void pg_identity_endpoint_destroy(struct pg_identity_endpoint_work *work);

/* Select an ordered proper face of the outer face->target Identity directions.
 * Validate those directions from formation evidence, then compose endpoint
 * selections. No center, degeneracy or nonidentity axis permutation is admitted.
 * An inherited Identity below the selected directions is left intact. */
const struct pg_evidence *pg_identity_proper_face(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, const struct pg_dimension_map *face);

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
/* Given parallel checked substitutions, extend their destination by paths
 * between the final count images. The preceding images must agree by alpha.
 * Each path's family acts along earlier paths; equal endpoint terms do not
 * erase a path declaration. Binders are caller-owned allocation inputs.
 * Returns assumptions, not a proof that either substitution is exhaustive or
 * that any equality holds. Outputs are written only on success, in the final
 * extended context. Uses ordinary context, substitution and Identity rules. */
const struct pg_evidence *pg_identity_substitution_context(struct pg_typing *typing,
	const struct pg_evidence *left, const struct pg_evidence *right,
	size_t count, const struct pg_object *const *binders,
	const struct pg_evidence **paths);
/* Replace the final count declarations by their complete cube telescopes,
 * including assumed centers. count must be nonzero; cubes follow declaration
 * order and share a dimension. order permutes their axes. Repeats context action;
 * no filler, transport or dimension-specific proof rule is introduced. */
const struct pg_evidence *pg_identity_cube_context(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	size_t count, const struct pg_binding_cube *const *cubes, const struct pg_dimension_map *order);
/* Act on a term in source along the same cube construction. Dimension zero
 * reindexes the selected suffix to zero vertices. Each subsequent dimension
 * uses checked family action, retaining polarity and selected boundaries.
 * The source term must have exactly source's context. No center is invented. */
const struct pg_evidence *pg_identity_cube_action(struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_dimensions *dimensions,
	const struct pg_evidence *source, const struct pg_evidence *term,
	size_t count, const struct pg_binding_cube *const *cubes, const struct pg_dimension_map *order);

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

/* Resumable ordered proper-face selection. The immutable face must outlive
 * the worker. Layer-origin wrappers use the shared suspended traversal, and
 * the first recovered formation is reused for endpoint selection. Fuel counts
 * traversal steps; individual proof rules and formation reconstruction remain
 * synchronous. No partial result is exposed; -1 includes unsupported input. */
struct pg_identity_face_work;
struct pg_identity_face_work *pg_identity_face_init(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *formation, const struct pg_dimension_map *face);
int pg_identity_face_advance(struct pg_identity_face_work *work, uint64_t fuel);
const struct pg_evidence *pg_identity_face_result(const struct pg_identity_face_work *work);
void pg_identity_face_destroy(struct pg_identity_face_work *work);

#endif
