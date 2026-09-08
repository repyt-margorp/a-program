#ifndef A_PROGRAM_POINTER_GRAPH_IO_H
#define A_PROGRAM_POINTER_GRAPH_IO_H

#include "graph.h"
#include <stdio.h>

/* Diagnostic shared-DAG listing. IDs are local display labels, not semantic
 * identities or addresses. No reduction, descriptor execution or graph edits. */
int pg_graph_print(FILE *file, const struct pg_term *root);

/* Optional immutable descriptor payloads in the same Term relocation table.
 * child: 1 child, 0 end, -1 error, -2 external object (index zero only).
 * name identifies an external object or, for a payload, its descriptor format.
 * child receives a scratch arena for temporary reference terms; it must not
 * change the source graph. restore constructs an inert object, never accepted
 * evidence or host effects.
 * Repeated format names may create distinct nominal objects. Payload references
 * must be acyclic with the containing Term graph. restore may retain the terms,
 * not its temporary input arrays. scalar enumerates unsigned metadata with
 * 1 item, 0 end, -1 error; NULL means none. Scalars are not Term dependencies
 * or type evidence. Limits include descriptor edges and metadata items. */
struct pg_graph_codec {
	const char *(*name)(void *, const struct pg_object *);
	const struct pg_object *(*resolve)(void *, const char *);
	int (*child)(void *, struct pg_graph *, const struct pg_object *, size_t, const struct pg_term **);
	int (*scalar)(void *, const struct pg_object *, size_t, uint64_t *);
	const struct pg_object *(*restore)(void *, struct pg_graph *, const char *, size_t,
		const struct pg_term *const *, size_t, const uint64_t *);
};
int pg_graph_write_descriptors(FILE *file, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *context);
int pg_graph_read_descriptors(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context,
	size_t *count, const struct pg_term *const **roots);

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
