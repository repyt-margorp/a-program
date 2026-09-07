#ifndef A_PROGRAM_POINTER_ACTION_H
#define A_PROGRAM_POINTER_ACTION_H

#include "dimension.h"
#include "evidence.h"

/* Construct a checked context substitution along a strict face. The binding
 * array follows source declaration order; NULL entries preserve that binder.
 * This preserves supplied typing, but does not assert a higher Identity type
 * for an ordinary variable merely because it belongs to a binding cube. */
const struct pg_evidence *pg_context_restrict(struct pg_typing *typing,
	struct pg_dimensions *dimensions, const struct pg_evidence *source,
	const struct pg_dimension_map *face, size_t count,
	const struct pg_binding_face *const *bindings);

#endif
