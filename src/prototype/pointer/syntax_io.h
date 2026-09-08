#ifndef A_PROGRAM_POINTER_SYNTAX_IO_H
#define A_PROGRAM_POINTER_SYNTAX_IO_H
#include "syntax.h"
#include <stdio.h>

/* Unresolved syntax DAG transport, not a typing certificate or a complete
 * program checkpoint. Preserves exact sharing and token bytes/locations.
 * Read owns all reconstructed storage in graph; it never parses or runs Solve.
 * limit bounds the sum of nodes, roots, items and token bytes. Outputs publish
 * only on success. Semantic/syntactic admissibility remains a separate task. */
int pg_syntax_write(FILE *file, size_t count, const struct pg_syntax *const *roots);
int pg_syntax_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t *count, const struct pg_syntax *const **roots);
#endif
