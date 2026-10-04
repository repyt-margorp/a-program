#ifndef __ACC_NAT_ACCESSIBILITY_EMIT_H__
#define __ACC_NAT_ACCESSIBILITY_EMIT_H__

#include "../indexed_views/emit.h"
#include "evidence.h"
#include <stdio.h>

/* Source Nat Fold recurrence; exact supplied zero/successor bodies still have
	* manual C33 implementations. No Identity erasure or full body lowering. */
int pg_c_nat_accessibility_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_occurrence *, const struct pg_c_indexed_entry *);

#endif
