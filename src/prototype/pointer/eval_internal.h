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

struct pg_eval_task {
	const struct pg_eval_work_operation *operation;
	void *state;
};

/* The evaluator and its state codec share these actual job layouts. The
 * intrusive request prefix indexes exact input/policy, never normal forms.
 * Reduction receipts remain local evidence, not externally accepted records. */
struct pg_reduction_certificate {
	const struct pg_term *source;
	const struct pg_term *target;
	const struct pg_eval_policy *policy;
	enum pg_reduction_kind kind;
	const struct pg_reduction_phase *phases;
	const struct pg_reduction_certificate *normality;
};

/* Raw relocated records. No public accessor exposes these roots as accepted
 * certificates; an owner must validate their derivations before admission. */
struct pg_reduction_archive {
	size_t count;
	const struct pg_reduction_certificate *const *roots;
	size_t phase_count;
	const struct pg_reduction_phase *const *phases;
};

struct pg_reduction_request {
	struct pg_index_entry index;
	struct pg_whnf_work *work;
	const struct pg_term *input;
	const struct pg_eval_policy *policy;
};

struct pg_whnf_job {
	struct pg_reduction_request request;
	struct pg_eval machine;
	const struct pg_reduction_certificate *certificate;
	struct materialization output;
	enum pg_eval_status status;
	uint64_t steps;
};

struct pg_nf_job {
	struct pg_reduction_request request;
	const struct pg_term *body;
	const struct pg_reduction_certificate *certificate;
	const struct pg_reduction_phase *phases;
	struct pg_whnf_job *head;
	struct pg_nf_job *children[2];
	struct pg_nf_job **stack;
	size_t depth, capacity;
	uint64_t steps;
	enum pg_nf_status status;
	enum { NF_HEAD, NF_CHILDREN, NF_RECHECK } stage;
};

/* Attach an owner-validated, unregistered pending job at its final address.
 * This maintains the exact-key index only, not imported-progress provenance.
 * The job storage must outlive work; work destroys its machine/readback. */
int pg_whnf_job_attach(struct pg_whnf_work *work, struct pg_whnf_job *job);

/* Rebuild one NF phase from already justified head/child reductions and an
 * already checked predecessor. Checks congruence, sequence and policy, not
 * the validity of imported leaves.
 * No children means a head-only phase; that alone does not establish NF. */
const struct pg_term *pg_reduction_phase_rebuild(struct pg_graph *graph,
	const struct pg_reduction_phase *previous,
	const struct pg_reduction_certificate *head,
	const struct pg_reduction_certificate *left, const struct pg_reduction_certificate *right);
/* Given justified WHNF and a checked predecessor, does a head-only phase
 * finish NF? References are terminal; rebuilt parents require unchanged WHNF. */
int pg_reduction_nf_terminal(const struct pg_reduction_phase *previous,
	const struct pg_reduction_certificate *head);

/* Index maintenance only. Does not establish the validity of saved results. */
int pg_readback_index(struct readback_context *context, struct readback_entry *entry);
void pg_readback_destroy(struct readback_context *context);
int pg_materialize_step(struct materialization *work, struct pg_graph *graph,
	struct pg_closure closure, const struct pg_argument *arguments);
void pg_materialize_destroy(struct materialization *work);
struct pg_argument *pg_eval_frame_copy_argument(struct pg_eval_frame *frame, struct pg_graph *arena);

#endif
