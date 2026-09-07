#ifndef A_PROGRAM_POINTER_EVAL_H
#define A_PROGRAM_POINTER_EVAL_H

#include "graph.h"

struct pg_environment;
struct pg_argument;
struct pg_closure {
	const struct pg_term *term;
	const struct pg_environment *environment;
};
enum pg_eval_status { PG_EVAL_PENDING, PG_EVAL_WHNF, PG_EVAL_ERROR };
struct pg_eval {
	struct pg_graph temporary;
	struct pg_closure current;
	const struct pg_argument *arguments;
	enum pg_eval_status status;
	uint64_t steps;
};

void pg_eval_init(struct pg_eval *machine, const struct pg_term *term);
enum pg_eval_status pg_eval_advance(struct pg_eval *machine, uint64_t budget);
/* Reify the current state without further reduction; may allocate in graph. */
const struct pg_term *pg_eval_readback(struct pg_eval *machine, struct pg_graph *graph);
void pg_eval_destroy(struct pg_eval *machine);

struct pg_binding_value {
	const struct pg_object *binder;
	const struct pg_term *value;
};
/* Capture-avoiding simultaneous substitution, without reduction. Later entries
 * shadow earlier entries for the same binder. Images are not resubstituted.
 * Input nodes and images must outlive the returned graph, as with readback. */
const struct pg_term *pg_term_substitute(struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings);

struct pg_beta_job;
/* One fixed policy: beta WHNF, all unbound references neutral, no dispatch.
 * Keys are input terms with empty environments. Captured environments remain
 * inside each job. graph and its referenced objects must outlive this store. */
struct pg_beta_work {
	struct pg_graph *graph;
	struct pg_graph storage;
	struct pg_index jobs;
};
int pg_beta_work_init(struct pg_beta_work *work, struct pg_graph *graph);
void pg_beta_work_destroy(struct pg_beta_work *work);
/* Requesting a job neither evaluates nor compares normal forms. */
struct pg_beta_job *pg_beta_request(struct pg_beta_work *work, const struct pg_term *input);
enum pg_eval_status pg_beta_advance(struct pg_beta_job *job, uint64_t budget);
enum pg_eval_status pg_beta_status(const struct pg_beta_job *job);
uint64_t pg_beta_steps(const struct pg_beta_job *job);
/* NULL until WHNF has been reached and read back successfully. */
const struct pg_term *pg_beta_result(const struct pg_beta_job *job);

#endif
