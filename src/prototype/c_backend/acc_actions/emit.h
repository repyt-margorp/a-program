#ifndef __ACC_ACTIONS_EMIT_H__
#define __ACC_ACTIONS_EMIT_H__

#include "../indexed_views/emit.h"
#include "evidence.h"
#include <stdio.h>

/* Emit the actual selected successor's three down branches into the bounded
	* C42 action representation. No general Identity semantics are established. */
int pg_c_acc_actions_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
