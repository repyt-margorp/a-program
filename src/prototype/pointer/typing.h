#ifndef A_PROGRAM_POINTER_TYPING_H
#define A_PROGRAM_POINTER_TYPING_H

#include "graph.h"

/* Immutable declared telescope. Its existence is not a typing certificate. */
struct pg_context {
	const struct pg_context *parent;
	const struct pg_object *binder;
	const struct pg_term *declared_type;
};

/* Elaboration input, not accepted evidence. Operands retain typed occurrences
 * where erasure shares their Core. Source locations belong to diagnostics. */
struct pg_occurrence {
	const struct pg_context *context;
	const struct pg_term *core;
	const struct pg_term *annotation;
	size_t operand_count;
	const struct pg_occurrence *operands[];
};

struct pg_typing {
	struct pg_graph *graph;
	/* Fresh arena-owned identity per initialization, never a reusable address
	 * of this mutable index container. Not a serialized identifier. */
	const void *owner_key;
	struct pg_index contexts;
	struct pg_index occurrences;
	struct pg_index proofs;
};

int pg_typing_init(struct pg_typing *typing, struct pg_graph *graph);
void pg_typing_destroy(struct pg_typing *typing);
/* NULL is the empty context. Binder freshness is a scope-construction duty;
 * this operation records a declaration, not its well-formedness proof. */
const struct pg_context *pg_context_bind(struct pg_typing *typing,
	const struct pg_context *parent, const struct pg_object *binder,
	const struct pg_term *declared_type);
const struct pg_context *pg_context_lookup(const struct pg_context *context,
	const struct pg_object *binder);
/* Count declarations after an exact prefix. Returns -1 for unrelated contexts
 * or a missing output pointer. This inspects structure, not proof validity. */
int pg_context_extension_size(const struct pg_context *context,
	const struct pg_context *prefix, size_t *count);
/* annotation is a declaration supplied by syntax, never a mutable solver
 * answer or an expected-type check. NULL means no explicit annotation. */
const struct pg_occurrence *pg_occurrence(struct pg_typing *typing,
	const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands);

#endif
