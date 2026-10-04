#ifndef __ACC_RECIPE_EMIT_H__
#define __ACC_RECIPE_EMIT_H__

#include "../acc_frame/emit.h"

/* Borrow admitted Acc/down and field endpoints. The finite target recipe
	* does not establish source action equivalence or checked Scope. */
int pg_c_acc_recipe_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);

#endif
