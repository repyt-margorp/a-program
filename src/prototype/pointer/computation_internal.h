#ifndef A_PROGRAM_POINTER_COMPUTATION_INTERNAL_H
#define A_PROGRAM_POINTER_COMPUTATION_INTERNAL_H

#include "computation.h"

struct fold_work {
	struct pg_graph *graph;
	const struct pg_term *head, *resume, *payload, *next;
	const struct pg_object *label, *x;
	const struct pg_object **binders;
	size_t count, index, position;
	enum { FOLD_BINDERS, FOLD_ARGUMENTS, FOLD_CLAUSE, FOLD_ABSTRACT } phase;
};

extern const struct pg_eval_work_operation pg_fold_work_operation;

#endif
