#ifndef A_PROGRAM_POINTER_CONTEXT_IO_H
#define A_PROGRAM_POINTER_CONTEXT_IO_H

#include "typing.h"
#include "graph_io.h"
#include <stdio.h>

/* Declaration transport, not context formation evidence. All context binders,
 * declared types and extra Core roots share one relocation table. NULL context
 * roots denote the empty telescope. Descriptor contracts are graph_io.h's. */
int pg_contexts_write(FILE *file, size_t count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms,
	const char *(*name)(void *, const struct pg_object *), void *owner);
int pg_contexts_write_descriptors(FILE *file, size_t count,
	const struct pg_context *const *contexts, size_t term_count,
	const struct pg_term *const *terms, const struct pg_graph_codec *codec, void *owner);
/* Outputs are graph-owned and published together only on success. limit bounds
 * metadata records and, separately, the embedded Core section's records.
 * Reconstruction uses ordinary context interning, never proof acceptance. */
int pg_contexts_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms);
int pg_contexts_read_descriptors(FILE *file, struct pg_typing *typing,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms);

#endif
