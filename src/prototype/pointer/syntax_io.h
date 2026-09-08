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
/* Check structural source contracts before submitting externally reconstructed
 * syntax to synthesis. Does not resolve names, classify terms, or assert that
 * a node is legal in every lexical position. Returns 0 or -1. */
int pg_syntax_validate(size_t count, const struct pg_syntax *const *roots);
#endif
