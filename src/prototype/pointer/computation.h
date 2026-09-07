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
/* Pure CBPV semantic WHNF, sharing beta steps with pg_eval. The output graph
 * owns materialized demanded arguments. Effect requests are not implemented. */
void pg_computation_eval_init(struct pg_eval *machine, struct pg_graph *output,
	const struct pg_term *term);

#endif
