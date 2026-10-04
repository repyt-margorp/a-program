#ifndef __ACC_COMPARISON_EMIT_H__
#define __ACC_COMPARISON_EMIT_H__

#include "../indexed_views/emit.h"
#include "evidence.h"

/* Actual supplied Nat comparator source; private existing qs_compare callback
	* convention, no public profile or generalized closure conversion. */
int pg_c_acc_comparison_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
