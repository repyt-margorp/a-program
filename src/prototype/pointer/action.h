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

/* Construct a checked context substitution along a strict face. The binding
 * array follows source declaration order; NULL entries preserve that binder.
 * This preserves supplied typing, but does not assert a higher Identity type
 * for an ordinary variable merely because it belongs to a binding cube. */
const struct pg_evidence *pg_context_restrict(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_dimension_map *face, size_t count,
	const struct pg_binding_face *const *bindings);

#endif
