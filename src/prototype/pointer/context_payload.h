#ifndef A_PROGRAM_POINTER_CONTEXT_PAYLOAD_H
#define A_PROGRAM_POINTER_CONTEXT_PAYLOAD_H
#include "typing.h"

/* Temporary relocation data, never context formation evidence. Metadata is
 * node count, selected count, (parent ID, binder judgement, indices ID) triples,
 * selected IDs. Family index telescopes share the same context DAG.
 * IDs are local, 1-based; zero is empty. Only value/family judgements are
 * declaration kinds. Terms are binder/type pairs followed by additional roots.
 * Pack borrows nodes and allocates arrays in storage. Unpack interns contexts
 * and publishes all outputs together only on success. */
int pg_contexts_pack(struct pg_graph *storage, size_t count,
	const struct pg_context *const *contexts, size_t term_count,
	const struct pg_term *const *terms, size_t *metadata_count,
	const uint64_t **metadata, size_t *root_count, const struct pg_term *const **roots);
int pg_contexts_unpack(struct pg_typing *typing, size_t metadata_count,
	const uint64_t *metadata, size_t root_count, const struct pg_term *const *roots,
	size_t *count, const struct pg_context *const **contexts,
	size_t *term_count, const struct pg_term *const **terms);
#endif
