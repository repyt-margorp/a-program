#ifndef A_PROGRAM_C_EMIT_H
#define A_PROGRAM_C_EMIT_H
#include "typing.h"
#include <stdio.h>

/* Borrow a closed export admitted by the caller's checking/trust policy.
 * This read-only pass neither certifies an imported occurrence nor calls
 * Solve/normalization. The driver
 * owns acceptance. Unsupported Oracle/entry shapes reject before output. */
int pg_c_emit(FILE *output, const struct pg_occurrence *root, const char **error);
#endif
