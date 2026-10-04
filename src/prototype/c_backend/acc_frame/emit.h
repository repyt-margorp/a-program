#ifndef __ACC_FRAME_EMIT_H__
#define __ACC_FRAME_EMIT_H__

#include "../acc_endpoints/emit.h"

/* Read existing admitted binders and export bounded target role positions.
	* This neither classifies source fields nor certifies Scope/action semantics. */
int pg_c_acc_frame_emit(FILE *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
