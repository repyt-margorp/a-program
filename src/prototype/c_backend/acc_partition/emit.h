#ifndef __C_ACC_PARTITION_EMIT_H__
#define __C_ACC_PARTITION_EMIT_H__

#include "../acc_fold/emit.h"

/* Private closed Nat32 actual partition expression experiment. Existing
	* admission/classifiers supply constructor parameters; none are inferred.
	* No source-machine work or new producer/checker/erasure/public ABI.
	* Unsupported shapes leave output untouched; final-copy I/O is caller-owned. */
int pg_c_acc_partition_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
