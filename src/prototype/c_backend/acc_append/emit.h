#ifndef __C_ACC_APPEND_EMIT_H__
#define __C_ACC_APPEND_EMIT_H__

#include "../acc_fold/emit.h"

/* Actual admitted closed-Nat append callable Fold experiment. Source fields,
	* IH/motive/captures are inspected; no source-machine/checker/producer work.
	* Unsupported shapes preserve the stream; final-copy I/O is caller-owned. */
int pg_c_acc_append_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
