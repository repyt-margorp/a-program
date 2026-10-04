#ifndef __C_ACC_CLAUSE_EMIT_H__
#define __C_ACC_CLAUSE_EMIT_H__

#include "../acc_fold/emit.h"

/* Private Nat32 clause experiment. Existing admission owns source typing.
	* Known source-selected partition/append bodies remain manual C33 helpers.
	* No source evaluation, term construction or public native profile.
	* Unsupported shapes leave the stream untouched; destination I/O may fail
	* during final copy. Caller owns ordinary product publication/cleanup. */
int pg_c_acc_clause_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_term *, const struct pg_term *,
	const struct pg_c_indexed_entry *);

#endif
