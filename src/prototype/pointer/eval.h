#ifndef A_PROGRAM_POINTER_EVAL_H
#define A_PROGRAM_POINTER_EVAL_H

#include "graph.h"

struct pg_environment;
struct pg_argument;
struct pg_eval_frame;
struct pg_eval_task;
struct pg_closure {
	const struct pg_term *term;
	const struct pg_environment *environment;
};
/* Shared lexical links. Once referenced by a closure/configuration, callers
 * must preserve their contents and lifetime. They carry no typing evidence. */
struct pg_environment {
	const struct pg_object *binder;
	struct pg_closure value;
	const struct pg_environment *parent;
};
struct pg_argument {
	struct pg_closure value;
	const struct pg_argument *next;
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
	struct pg_eval_task *task;
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
/* State is borrowed until resume or machine destruction; arena storage is
 * suitable. It is not serialized by pending readback. */
int pg_eval_demand(struct pg_eval *machine, size_t index,
	int (*resume)(struct pg_eval *, const struct pg_term *, const void *), const void *state);
/* Evaluate auxiliary work on this machine, preserving the caller's arguments.
 * Resume must incorporate the answer. Pending readback retains the caller;
 * use for pure work, not an effect whose result could be discarded. */
int pg_eval_demand_closure(struct pg_eval *machine, struct pg_closure value,
	int (*resume)(struct pg_eval *, const struct pg_term *, const void *), const void *state);
/* Pure auxiliary traversal. Each poll consumes one machine transition:
 * 0 pending, 1 ready, -1 error. Poll must preserve the caller configuration.
 * Resume uses the dispatcher protocol and runs after detaching the task.
 * After successful registration, destroy runs once, including on error or
 * destruction while pending. Failure to register leaves ownership with caller.
 * Pending readback retains the caller; state must outlive the task. */
/* One immutable descriptor per auxiliary algorithm, not per invocation.
 * It must outlive every registered task. Neither this pointer nor arbitrary
 * state addresses are portable image identities. */
struct pg_eval_work_operation {
	int (*poll)(void *);
	int (*resume)(struct pg_eval *, void *);
	void (*destroy)(void *);
};
int pg_eval_defer(struct pg_eval *machine,
	const struct pg_eval_work_operation *operation, void *state);

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
/* Original lexical input, borrowed until work destruction. No reduction. */
const struct pg_closure *pg_substitution_input(const struct pg_substitution *work);
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
struct pg_reduction_certificate;
enum pg_reduction_kind { PG_REDUCTION_WHNF, PG_REDUCTION_NF };
/* Keys are (input term, policy pointer), with empty environments. Captured
 * environments remain inside jobs. All referenced graphs outlive the store. */
struct pg_whnf_work {
	struct pg_graph *graph;
	struct pg_graph storage;
	struct pg_index jobs;
	struct pg_index normal_forms;
};
int pg_whnf_work_init(struct pg_whnf_work *work, struct pg_graph *graph);
void pg_whnf_work_destroy(struct pg_whnf_work *work);
/* Requesting a job neither evaluates nor compares normal forms. A materialized
 * result is also cached as its own WHNF under the same policy; source/result
 * jobs and receipts remain distinct unless their exact input pointers agree. */
struct pg_whnf_job *pg_whnf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input);
/* Includes materialization: WHNF and result are published only after readback.
 * Steps count evaluation, shared traversal and spine reconstruction transitions. */
enum pg_eval_status pg_whnf_advance(struct pg_whnf_job *job, uint64_t budget);
enum pg_eval_status pg_whnf_status(const struct pg_whnf_job *job);
uint64_t pg_whnf_steps(const struct pg_whnf_job *job);
/* NULL until WHNF has been reached and read back successfully. */
const struct pg_term *pg_whnf_result(const struct pg_whnf_job *job);
/* Immutable directed reduction receipt, shared by WHNF and NF. It certifies
 * the computation from source to target, not a serialized "normal" flag.
 * Issued only after evaluation/materialization or congruent NF work completes.
 * Owned by work->graph, so it survives job-store destruction. Core records the
 * policy; the typing layer decides whether that policy preserves typing. */
const struct pg_reduction_certificate *pg_whnf_certificate(const struct pg_whnf_job *job);
const struct pg_term *pg_reduction_source(const struct pg_reduction_certificate *certificate);
const struct pg_term *pg_reduction_target(const struct pg_reduction_certificate *certificate);
const struct pg_eval_policy *pg_reduction_policy(const struct pg_reduction_certificate *certificate);
enum pg_reduction_kind pg_reduction_kind(const struct pg_reduction_certificate *certificate);
/* Completed NF phases, newest first. Children certify congruent rebuilding;
 * phases without children retain the final head reduction. These are local
 * immutable dependencies, not permission to accept imported endpoint claims. */
struct pg_reduction_phase {
	const struct pg_reduction_phase *previous;
	const struct pg_reduction_certificate *head;
	const struct pg_reduction_certificate *children[2];
	const struct pg_term *rebuilt;
};
const struct pg_reduction_phase *pg_reduction_phases(const struct pg_reduction_certificate *certificate);
/* A reflexive cache entry can inherit normality from a completed reduction
 * whose target is its source. This edge never points back to the cache entry. */
const struct pg_reduction_certificate *pg_reduction_normality(const struct pg_reduction_certificate *certificate);

enum pg_nf_status { PG_NF_PENDING, PG_NF_DONE, PG_NF_ERROR };
struct pg_nf_job;
/* Strong normalization shares the same policy-keyed WHNF jobs and exact
 * pointer-keyed subterm results. It descends beneath Lambda/THUNK, so it is
 * for pure normalization, not runtime execution of a suspended computation.
 * Rebuilt parents are reduced again: child reduction may expose an eta rule.
 * Requests do no evaluation. Fuel bounds traversal/evaluator transitions;
 * a term without a normal form can remain pending. No recursive C traversal. */
struct pg_nf_job *pg_nf_request(struct pg_whnf_work *work,
	const struct pg_eval_policy *policy, const struct pg_term *input);
enum pg_nf_status pg_nf_advance(struct pg_nf_job *job, uint64_t budget);
enum pg_nf_status pg_nf_status(const struct pg_nf_job *job);
const struct pg_term *pg_nf_result(const struct pg_nf_job *job);
const struct pg_reduction_certificate *pg_nf_certificate(const struct pg_nf_job *job);
/* Transitions charged to advances of this root, including dependencies;
 * shared work performed by another root is not charged a second time. */
uint64_t pg_nf_steps(const struct pg_nf_job *job);

#endif
