#ifndef __ACC_CAPTURE_EMIT_H__
#define __ACC_CAPTURE_EMIT_H__

#include "../acc_actions/emit.h"

/* Actual successor Fold/constructors/captured callback expression emission,
	* composing the sealed branch lowerer with the same manual action runtime. */
int pg_c_acc_successor_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
