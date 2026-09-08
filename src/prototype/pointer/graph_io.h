#ifndef A_PROGRAM_POINTER_GRAPH_IO_H
#define A_PROGRAM_POINTER_GRAPH_IO_H

#include "graph.h"
#include <stdio.h>

/* Diagnostic shared-DAG listing. IDs are local display labels, not semantic
 * identities or addresses. No reduction, descriptor execution or graph edits. */
int pg_graph_print(FILE *file, const struct pg_term *root);

/* Raw acyclic Core graph transport, not typing evidence or a program image.
 * Plain binders are relocated freshly. Owned binders and semantic objects need
 * stable, versioned descriptor names supplied by the owner. The codec never
 * interprets a name as a host address. Names must identify distinct objects;
 * callbacks and referenced objects must outlive the call/output graph.
 * No evaluation or alpha interning takes place. Streams are caller-owned. */
int pg_graph_write(FILE *file, size_t count, const struct pg_term *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *context);
/* graph must be initialized. Limits bound record count and descriptor length.
 * Output roots are owned by graph and published only on success. Failure may
 * leave unused arena allocations but cannot publish evidence. Unknown descriptors fail closed.
 * Distinct object records resolving to one pointer are rejected, not merged.
 * Recursive declaration payloads and accepted evidence are not encoded here. */
int pg_graph_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *context,
	size_t *count, const struct pg_term *const **roots);

#endif
