#ifndef A_PROGRAM_C_SCALAR_H
#define A_PROGRAM_C_SCALAR_H

#include "../emit.h"

/* Borrow admitted roots. Share temporary C-local functions and scalar DAGs;
 * do not normalize, mutate source graphs or grant typing evidence.
 * Both streams remain unwritten until every selected function is supported. */
int pg_c_emit_scalar(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, const char **error);

/* Explicit target-side representations; data selections are nonrecursive
	* tagged values with scalar/enum fields. The scalar profile has no selections. */
int pg_c_emit_native(FILE *source, FILE *header, size_t count,
	const struct pg_c_export *exports, size_t entry, size_t enum_count,
	const struct pg_c_export *enums, size_t data_count,
	const struct pg_c_export *data, const char **error);

#endif
