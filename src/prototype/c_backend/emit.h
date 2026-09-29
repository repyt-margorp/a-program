#ifndef A_PROGRAM_C_EMIT_H
#define A_PROGRAM_C_EMIT_H
#include "typing.h"
#include <stdio.h>

/* Borrow a closed export admitted by the caller's checking/trust policy.
 * This read-only pass neither certifies an imported occurrence nor calls
 * Solve/normalization. The driver
 * owns acceptance. Unsupported Oracle/entry shapes reject before output. */
int pg_c_emit(FILE *output, const struct pg_occurrence *root, const char **error);

/* Public isolated-call ABI 1: ap_export_<alias>(void) returns execution status.
 * No runtime value/handle crosses the boundary; every call owns its allocations.
 * Exports share one reachable Core DAG. entry=SIZE_MAX omits main. The caller
 * admits every subject before emission. C names/ABI never enter the input graph. */
struct pg_c_export {
	const char *alias;
	const struct pg_occurrence *subject;
};
int pg_c_export_alias(const char *);
int pg_c_export_names(size_t, const struct pg_c_export *);
int pg_c_emit_exports(FILE *, size_t, const struct pg_c_export *, size_t entry, const char **error);
int pg_c_emit_header(FILE *, size_t, const struct pg_c_export *);
#endif
