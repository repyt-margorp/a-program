#ifndef __ACTUAL_ACC_RUNTIME_CAPTURE_EMIT_H__
#define __ACTUAL_ACC_RUNTIME_CAPTURE_EMIT_H__

#include "../acc_closed_capture/emit.h"

/* Existing admitted Bool parameter/capture views; no source advancement,
	* checking, synthesized expected classifier or source erasure. */
int pg_c_acc_runtime_capture_emit(FILE *, struct pg_graph *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
