#ifndef A_PROGRAM_POINTER_COMPUTATION_H
#define A_PROGRAM_POINTER_COMPUTATION_H

#include "graph.h"

/* Fixed semantic operations, not additional Core tags. Construction does not
 * execute them. The beta-only evaluator keeps these references neutral. */
extern const struct pg_object pg_return_operation;
extern const struct pg_object pg_thunk_operation;
extern const struct pg_object pg_force_operation;

#endif
