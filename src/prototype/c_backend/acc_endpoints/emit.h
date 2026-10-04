#ifndef __ACC_ENDPOINTS_EMIT_H__
#define __ACC_ENDPOINTS_EMIT_H__

#include "../acc_transport/emit.h"

/* Borrow admitted endpoint operands. Emit only the selected actual successor's
	* finite nominal index projections, without issuing source evidence. */
int pg_c_acc_endpoints_emit(FILE *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
