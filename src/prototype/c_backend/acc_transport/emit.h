#ifndef __ACC_TRANSPORT_EMIT_H__
#define __ACC_TRANSPORT_EMIT_H__

#include "../acc_capture/emit.h"

/* Borrow admitted typed structure. Emit finite target descriptors for the
	* selected actual successor's maps; this issues no source evidence. */
int pg_c_acc_transport_emit(FILE *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
