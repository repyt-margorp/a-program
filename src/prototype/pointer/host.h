#ifndef A_PROGRAM_POINTER_HOST_H
#define A_PROGRAM_POINTER_HOST_H

#include "graph.h"

/* Fixed machine contracts, not C's implementation-dependent int/long widths.
 * Text is an exact byte sequence, including embedded zero bytes. No decoding,
 * normalization, arithmetic or external execution occurs in this module. */
const struct pg_object *pg_host_type(const char *source_name);
const char *pg_host_type_name(const struct pg_object *type);
const struct pg_object *pg_host_type_resolve(const char *descriptor);
/* Literal storage is canonical big-endian two's-complement for integers;
 * Text keeps its bytes unchanged. Exact type/payload tuples are interned. */
const struct pg_object *pg_host_literal(struct pg_graph *graph,
	const struct pg_object *type, size_t count, const unsigned char *bytes);
int pg_host_literal_view(const struct pg_object *object,
	const struct pg_object **type, size_t *count, const unsigned char **bytes);
const struct pg_object *pg_host_integer(struct pg_graph *graph,
	const struct pg_object *type, int64_t value);
int pg_host_integer_view(const struct pg_object *object, int64_t *value);

#endif
