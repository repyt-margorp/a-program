#ifndef __C_ACC_FOLD_EMIT_H__
#define __C_ACC_FOLD_EMIT_H__

#include "../indexed_views/emit.h"
#include "program.h"

/* One source-selected Acc Nat LT Fold with SizedList Nat -> List Nat motive.
	* Emits native Fold/IH machinery only; no supplied clause is treated as
	* automatically lowered source code. It runs no source checking machinery. */
int pg_c_acc_fold_emit(FILE *, FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, size_t, const struct pg_c_indexed_entry *);

#endif
