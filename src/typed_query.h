#ifndef A_PROGRAM_POINTER_TYPED_QUERY_H
#define A_PROGRAM_POINTER_TYPED_QUERY_H

#include "evidence.h"

/* Kernel-side checked queries share identity and dependency scheduling, not
 * domain state or a second acceptance table. Keys borrow immutable inputs;
 * owner-private progress precedes this header in the typing arena. */
struct pg_typed_query {
	struct pg_index_entry index;
	struct pg_typing *typing;
	const struct pg_typed_query_class *role;
	const struct pg_evidence *result;
	struct pg_typed_query *dependency;
	struct pg_typing_wait *waiting;
	size_t ordinal;
	uint64_t steps;
	int status;
	const void *inputs[];
};

struct pg_typed_query_class {
	size_t size, input_count;
	int (*advance)(struct pg_typed_query *);
};

struct pg_typed_query *pg_typed_query_request(struct pg_typing *typing,
	const struct pg_typed_query_class *role, size_t ordinal,
	const void *const *inputs, int *created);
void *pg_typed_query_state(const struct pg_typed_query *query);

#endif
