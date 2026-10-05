#ifndef __ACC_CREATE_EMIT_H__
#define __ACC_CREATE_EMIT_H__

#include "../acc_indices/emit.h"

/* Read the actual recursive prior classifier for finite target creation.
	* Source admission and general action equivalence remain upstream matters. */
int pg_c_acc_create_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
