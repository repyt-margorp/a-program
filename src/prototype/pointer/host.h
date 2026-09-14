#ifndef A_PROGRAM_POINTER_HOST_H
#define A_PROGRAM_POINTER_HOST_H

#include "graph.h"

/* Fixed machine contracts, not C's implementation-dependent int/long widths.
 * Text is an exact byte sequence, including embedded zero bytes. Literal
 * construction is inert; only the explicit dispatcher below runs arithmetic.
 * Neither path performs encoding conversion or external effects. */
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
/* Fixed modular arithmetic. Enumerate until NULL; no user callbacks. */
const struct pg_object *pg_host_function(size_t index);
const char *pg_host_function_name(const struct pg_object *function);
const char *pg_host_function_descriptor(const struct pg_object *function);
const struct pg_object *pg_host_function_resolve(const char *descriptor);
int pg_host_function_view(const struct pg_object *function,
	const struct pg_object **type, size_t *arity);
struct pg_eval;
struct pg_eval_continuation;
int pg_host_dispatch(struct pg_eval *machine);
const struct pg_eval_continuation *pg_host_continuation_resolve(const char *name);

#endif
