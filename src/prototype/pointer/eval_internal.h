#ifndef A_PROGRAM_POINTER_EVAL_INTERNAL_H
#define A_PROGRAM_POINTER_EVAL_INTERNAL_H

#include "eval.h"

/* Shared implementation layout for the evaluator and its inert state codec. */
struct readback_entry {
	struct pg_index_entry index;
	struct pg_closure input;
	const struct pg_term *result;
	struct readback_entry *next, *left, *right;
	const struct pg_object *binder;
	unsigned stage;
	const struct pg_environment *cursor;
};
struct readback_context {
	struct pg_graph *output;
	struct pg_graph temporary;
	struct pg_index results;
	struct readback_entry *pending;
	uint64_t steps;
};
struct pg_substitution_state {
	struct readback_context context;
	struct readback_entry *root;
	enum pg_substitution_status status;
};

/* Index maintenance only. Does not establish the validity of saved results. */
int pg_readback_index(struct readback_context *context, struct readback_entry *entry);

#endif
