#ifndef A_PROGRAM_POINTER_EVAL_H
#define A_PROGRAM_POINTER_EVAL_H

#include "graph.h"

struct pg_environment;
struct pg_argument;
struct pg_eval_frame;
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
	/* Optional fixed semantic dispatcher. NULL preserves beta-only policy. */
	int (*dispatch)(struct pg_eval *machine);
	struct pg_graph *output;
	struct pg_eval_frame *frames;
	int head_ready;
};

void pg_eval_init(struct pg_eval *machine, const struct pg_term *term);
enum pg_eval_status pg_eval_advance(struct pg_eval *machine, uint64_t budget);
/* Reify the current state without further reduction; may allocate in graph. */
const struct pg_term *pg_eval_readback(struct pg_eval *machine, struct pg_graph *graph);
void pg_eval_destroy(struct pg_eval *machine);
/* Dispatcher protocol: 0 progressed, 1 neutral, -1 failure. Demand evaluates
 * one argument on the same machine; resume receives its materialized WHNF.
 * Demand readback and argument-prefix reconstruction consume evaluator steps;
 * the callback is invoked once, only after the complete answer is available.
 * The output graph must outlive the machine. This machine does not memoize
 * invocations; the separate WHNF store is only for immutable pure policies. */
const struct pg_closure *pg_eval_argument(const struct pg_eval *machine, size_t index);
/* Read-only argument cursor. Start at machine->arguments; each call advances
 * one link without evaluating. Returned closures live until machine destroy. */
const struct pg_closure *pg_eval_next_argument(const struct pg_argument **cursor);
int pg_eval_enter(struct pg_eval *machine, struct pg_closure value, size_t consume);
int pg_eval_apply(struct pg_eval *machine, struct pg_closure function,
	struct pg_closure argument, size_t consume);
int pg_eval_demand(struct pg_eval *machine, size_t index,
	int (*resume)(struct pg_eval *machine, const struct pg_term *answer));

struct pg_binding_value {
	const struct pg_object *binder;
	const struct pg_term *value;
};
struct pg_substitution_state;
struct pg_substitution { struct pg_substitution_state *state; };
enum pg_substitution_status { PG_SUBSTITUTION_PENDING, PG_SUBSTITUTION_DONE, PG_SUBSTITUTION_ERROR };
/* Initialization copies binding entries but does not traverse the term.
 * Fuel counts traversal transitions, including environment lookup links.
 * Allocator/hash-table work is not a wall-clock bound. Inputs and output graph
 * must outlive the job; completed output survives destroy. */
int pg_substitution_init(struct pg_substitution *work, struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings);
enum pg_substitution_status pg_substitution_advance(struct pg_substitution *work, uint64_t budget);
enum pg_substitution_status pg_substitution_status(const struct pg_substitution *work);
uint64_t pg_substitution_steps(const struct pg_substitution *work);
const struct pg_term *pg_substitution_result(const struct pg_substitution *work);
void pg_substitution_destroy(struct pg_substitution *work);
/* Capture-avoiding simultaneous substitution, without reduction. Later entries
 * shadow earlier entries for the same binder. Images are not resubstituted.
 * Input nodes and images must outlive the returned graph, as with readback. */
const struct pg_term *pg_term_substitute(struct pg_graph *graph,
	const struct pg_term *term, size_t count, const struct pg_binding_value *bindings);

/* Immutable pure policies outlive their jobs. Core does not decide which
 * policies are admissible as conversion evidence; that is the checker's job.
 * Runtime invocations with effects must not be put in this memo store. */
struct pg_eval_policy { int (*dispatch)(struct pg_eval *machine); };
extern const struct pg_eval_policy pg_beta_policy;
struct pg_whnf_job;
struct pg_whnf_certificate;
/* Keys are (input term, policy pointer), with empty environments. Captured
 * environments remain inside jobs. All referenced graphs outlive the store. */
struct pg_whnf_work {
	struct pg_graph *graph;
	struct pg_graph storage;
	struct pg_index jobs;
};
int pg_whnf_work_init(struct pg_whnf_work *work, struct pg_graph *graph);
void pg_whnf_work_destroy(struct pg_whnf_work *work);
/* Requesting a job neither evaluates nor compares normal forms. */
struct pg_whnf_job *pg_whnf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input);
/* Includes materialization: WHNF and result are published only after readback.
 * Steps count evaluation, shared traversal and spine reconstruction transitions. */
enum pg_eval_status pg_whnf_advance(struct pg_whnf_job *job, uint64_t budget);
enum pg_eval_status pg_whnf_status(const struct pg_whnf_job *job);
uint64_t pg_whnf_steps(const struct pg_whnf_job *job);
/* NULL until WHNF has been reached and read back successfully. */
const struct pg_term *pg_whnf_result(const struct pg_whnf_job *job);
/* Immutable directed evaluation receipt, issued only after readback completes.
 * Owned by work->graph, so it survives job-store destruction. Core records the
 * policy; the typing layer decides whether that policy preserves typing. */
const struct pg_whnf_certificate *pg_whnf_certificate(const struct pg_whnf_job *job);
const struct pg_term *pg_whnf_source(const struct pg_whnf_certificate *certificate);
const struct pg_term *pg_whnf_target(const struct pg_whnf_certificate *certificate);
const struct pg_eval_policy *pg_whnf_policy(const struct pg_whnf_certificate *certificate);

#endif
