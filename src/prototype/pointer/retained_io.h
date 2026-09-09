#ifndef A_PROGRAM_POINTER_RETAINED_IO_H
#define A_PROGRAM_POINTER_RETAINED_IO_H

#include "derivation_io.h"
#include "computation_io.h"

/* Unaccepted rule inputs and reduction records share one Core/object table.
 * Reading performs no Solve or evidence admission. The caller owns effects
 * and descriptor state, and destroys them after any failure (including an
 * enclosing boundary failure). Output pointers publish only on success. */
int pg_retained_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects, const struct pg_reduction_archive *reductions,
	const struct pg_graph_codec *codec, void *owner);
int pg_retained_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	const struct pg_reduction_archive **reductions);

#endif
