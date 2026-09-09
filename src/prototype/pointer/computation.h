#ifndef A_PROGRAM_POINTER_COMPUTATION_H
#define A_PROGRAM_POINTER_COMPUTATION_H

#include "graph.h"
#include "eval.h"

/* Fixed semantic operations, not additional Core tags. Construction does not
 * execute them. The beta-only evaluator keeps these references neutral. */
extern const struct pg_object pg_return_operation;
extern const struct pg_object pg_thunk_operation;
extern const struct pg_object pg_force_operation;
/* Zero-operation-clause fold: applied to M and its raw return continuation. */
extern const struct pg_object pg_fold_operation;
/* Request(label, payload, continuation) is an inert computation description.
 * The label is an exact semantic-object pointer; all term operands are Core
 * edges. These builders/views establish no operation signature or typing. */
extern const struct pg_object pg_request_operation;
const struct pg_term *pg_computation_request(struct pg_graph *graph,
	const struct pg_object *label, const struct pg_term *payload,
	const struct pg_term *continuation);
int pg_computation_request_view(const struct pg_term *term,
	const struct pg_object **label, const struct pg_term **payload,
	const struct pg_term **continuation);
struct pg_operation_clause {
	const struct pg_object *label;
	const struct pg_term *body;
};
/* Raw Fold layout, distinct from a typed handler signature. The immutable
 * array is pointer-sorted for lookup; position identifies the original clause.
 * Transport preserves labels/positions and rebuilds the destination index. */
struct pg_clause_position {
	const struct pg_object *label;
	size_t position;
};
int pg_computation_handler_view(const struct pg_object *object,
	size_t *count, const struct pg_clause_position **positions);
const struct pg_object *pg_computation_handler_restore(struct pg_graph *graph,
	size_t count, const struct pg_clause_position *positions);
/* Raw deep fold. Each clause is a computation over payload and a THUNK of
 * the recursively handled continuation. Clause code remains outside its own
 * handler. Zero clauses use pg_fold_operation. Duplicate labels are invalid.
 * No signature, effect row or accepted typing is inferred by this builder. */
const struct pg_term *pg_computation_fold(struct pg_graph *graph,
	const struct pg_term *source, const struct pg_term *returned,
	size_t count, const struct pg_operation_clause *clauses);
/* Versioned names of the fixed pure operators for graph relocation. These
 * neither execute a computation nor certify a typed use of an operator. */
const char *pg_computation_name(const struct pg_object *object);
const struct pg_object *pg_computation_resolve(const char *name);
/* One structural eta contraction, or NULL. Never evaluates the operands.
 * Shared by semantic evaluation and action under a binder. */
const struct pg_term *pg_computation_eta(struct pg_graph *graph, const struct pg_term *term);
/* Fixed kernel-pure semantics, shared with conversion. No host callbacks or
 * user handler overrides. Unknown references remain neutral. */
extern const struct pg_eval_policy pg_pure_policy;
/* Pure CBPV semantic WHNF, sharing beta steps with pg_eval. The output graph
 * owns materialized demanded arguments. Requests stay inert; zero-clause fold
 * forwards them with its return continuation. No host operation is executed. */
void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term);

/* Resolve the pure dispatcher's known continuations, including its data,
 * Identity and symmetry delegates. State restoration is a separate duty. */
struct pg_eval_continuation;
const struct pg_eval_continuation *pg_computation_continuation_resolve(const char *name);
const struct pg_eval_work_operation *pg_computation_work_resolve(const char *name);

#endif
