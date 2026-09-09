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
struct materialization {
	struct readback_context readback;
	struct readback_entry *entry;
	const struct pg_argument *remaining;
	const struct pg_term *partial;
	int done;
};

struct pg_eval_frame {
	struct pg_closure caller;
	const struct pg_argument *arguments;
	const struct pg_argument *target;
	const struct pg_eval_continuation *continuation;
	const void *state;
	struct pg_eval_frame *parent;
	struct materialization answer;
	const struct pg_argument *cursor;
	struct pg_argument *first, *last;
};

/* Index maintenance only. Does not establish the validity of saved results. */
int pg_readback_index(struct readback_context *context, struct readback_entry *entry);
void pg_readback_destroy(struct readback_context *context);
int pg_materialize_step(struct materialization *work, struct pg_graph *graph,
	struct pg_closure closure, const struct pg_argument *arguments);
void pg_materialize_destroy(struct materialization *work);
struct pg_argument *pg_eval_frame_copy_argument(struct pg_eval_frame *frame, struct pg_graph *arena);

#endif
