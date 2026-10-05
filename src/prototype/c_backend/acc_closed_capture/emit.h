#ifndef __ACTUAL_ACC_CLOSED_CAPTURE_EMIT_H__
#define __ACTUAL_ACC_CLOSED_CAPTURE_EMIT_H__

#include "../acc_measure/emit.h"
#include <stdio.h>

/* Read admitted closed/parameter association, known comparator operands and
	* an optional ground Bool capture with both original matcher clauses.
	* No source advancement, checking, expected-type fallback or effect erasure. */
int pg_c_acc_closed_capture_emit(FILE *, struct pg_graph *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
