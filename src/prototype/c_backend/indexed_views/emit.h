#ifndef __C_INDEXED_EMIT_H__
#define __C_INDEXED_EMIT_H__

#include "view.h"
#include <stdio.h>

struct pg_c_indexed_entry {
	const char *name;
	struct pg_c_indexed_view view;
};

/* Private descriptor declarations, not a Linker profile or source admission.
	* Output borrows source identities only while this function runs. */
int pg_c_indexed_emit(FILE *, struct pg_graph *, size_t, const struct pg_c_indexed_entry *);

#endif
