#ifndef A_PROGRAM_POINTER_TYPED_QUERY_H
#define A_PROGRAM_POINTER_TYPED_QUERY_H

#include "evidence.h"
#include "pending.h"

/* Kernel-side checked queries share identity and dependency scheduling, not
 * domain state or a second acceptance table. Keys borrow immutable inputs;
 * owner-private progress precedes this header in the typing arena. */
struct pg_typed_query {
	struct pg_pending pending;
	const struct pg_evidence *result;
	struct pg_typed_query *dependency;
	struct pg_typing_wait *waiting;
	size_t ordinal;
	uint64_t steps;
	int status;
	const void *inputs[];
};

struct pg_typed_query_class {
	struct pg_pending_class pending;
	size_t size, input_count;
	int (*advance)(struct pg_typed_query *);
	/* Release private scratch only; the typed answer stays on its owner. */
	void (*destroy)(struct pg_typed_query *);
};

extern const struct pg_pending_ops pg_typed_query_pending_ops;
const struct pg_typed_query_class *pg_typed_query_role(const struct pg_typed_query *);
struct pg_typing *pg_typed_query_typing(const struct pg_typed_query *);
int pg_typed_query_owned_by(const struct pg_typed_query *, const struct pg_typing *);

struct pg_typed_query *pg_typed_query_request(struct pg_typing *typing,
	const struct pg_typed_query_class *role, size_t ordinal,
	const void *const *inputs, int *created);
void *pg_typed_query_state(const struct pg_typed_query *query);
void pg_typed_query_destroy(struct pg_typing *typing);

#endif
