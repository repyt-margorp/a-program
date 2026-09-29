#ifndef A_PROGRAM_C_SCALAR_H
#define A_PROGRAM_C_SCALAR_H

#include "../emit.h"

/* Borrow admitted roots. Build one temporary C-local scalar DAG per distinct
 * root; do not normalize, mutate source graphs or grant typing evidence.
 * Both streams remain unwritten until every selected function is supported. */
int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error);

#endif
