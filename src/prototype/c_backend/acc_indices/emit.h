#ifndef __ACC_INDEX_EMIT_H__
#define __ACC_INDEX_EMIT_H__

#include "../acc_recipe/emit.h"

/* Borrow existing constructor field/result images. These finite target recipes
	* do not check or admit a declaration, or establish source action equivalence. */
int pg_c_acc_index_emit(FILE *, struct pg_graph *, const struct pg_typing *,
	const struct pg_occurrence *, const struct pg_occurrence *,
	const struct pg_c_indexed_entry *);
int pg_c_lt_index_recipe_emit(FILE *, struct pg_graph *,
	const struct pg_c_indexed_entry *);

#endif
