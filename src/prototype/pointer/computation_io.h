#ifndef A_PROGRAM_POINTER_COMPUTATION_IO_H
#define A_PROGRAM_POINTER_COMPUTATION_IO_H

#include "eval_io.h"

/* Raw production Demand stack, including named continuations and shared scope
 * ownership. No evaluation or evidence admission. Imported progress requires
 * provenance before execution can support accepted results. The caller retains
 * machine flags, policy and deferred work separately. Destroy active answer
 * materialization before arena/output; all other payload storage is arena-owned. */
int pg_computation_frames_write(FILE *file, const struct pg_eval_frame *frames,
	const struct pg_eval_configuration *current, const struct pg_graph_codec *codec, void *owner);
int pg_computation_frames_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_eval_frame **frames, struct pg_eval_configuration *current);

#endif
