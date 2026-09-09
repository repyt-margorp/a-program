#ifndef A_PROGRAM_POINTER_EVAL_IO_H
#define A_PROGRAM_POINTER_EVAL_IO_H

#include "eval.h"
#include "graph_io.h"

/* A configuration fragment, not a complete evaluator checkpoint. No status,
 * policy, work cursor, continuation or accepted reduction is transported. */
struct pg_eval_configuration {
	struct pg_closure head;
	const struct pg_argument *arguments;
};
/* Preserve shared environment/argument links using the ordinary Term codec.
 * Reading is inert: it neither evaluates nor issues normalization evidence.
 * Source links are borrowed; the destination graph must be initialized.
 * All restored storage belongs to graph. On failure
 * outputs are empty, though graph may contain unused allocations. limit bounds
 * configuration roots, link records and the embedded graph's record counts. */
int pg_eval_configurations_write(FILE *file, size_t count,
	const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner);
int pg_eval_configurations_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_eval_configuration **roots);

/* Compose an owning work payload around the configuration's Term roots. The
 * callback must return the same ordered roots through one shared relocation
 * table, not serialize each configuration separately. It owns the embedded
 * stream format. On outer read failure the caller must release any callback
 * state already restored; configuration outputs themselves remain empty. */
int pg_eval_configurations_write_with(FILE *file, size_t count,
	const struct pg_eval_configuration *roots,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *state);
int pg_eval_configurations_read_with(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, size_t *count, const struct pg_eval_configuration **roots,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *, const struct pg_term *const **, void *), void *state);

/* Raw substitution/readback work, including partial results and traversal.
 * Restored progress is NOT accepted evidence that these results follow from
 * the original input. Use only as an inert work representation until that
 * provenance is established. The existing substitution executor resumes it;
 * no alternate evaluator or certificate admission is provided here.
 * Read into an unused work handle; destroy it before destroying graph. */
int pg_substitution_write(FILE *file, const struct pg_substitution *work,
	const struct pg_graph_codec *codec, void *owner);
int pg_substitution_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_substitution *work);

struct materialization;
/* Internal materialization state and its initial configuration use one shared
 * relocation table. The same raw-work/provenance restriction applies. */
int pg_materialization_write(FILE *file, const struct materialization *work,
	const struct pg_eval_configuration *input, const struct pg_graph_codec *codec, void *owner);
int pg_materialization_read(FILE *file, struct pg_graph *graph, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct materialization *work, struct pg_eval_configuration *input);

/* Frame DATA only: caller, target, cursor and answer readback share one graph.
 * Parent, continuation/state, machine flags and policy are owned by the outer
 * checkpoint and are not encoded. Restored frame has no continuation installed.
 * Its private prefix is recreated in arena; answer state must be destroyed via
 * pg_materialize_destroy before arena/output. No computation is advanced. */
int pg_eval_frame_payload_write(FILE *file, const struct pg_eval_frame *frame,
	const struct pg_eval_configuration *current, const struct pg_graph_codec *codec, void *owner);
int pg_eval_frame_payload_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_eval_frame **frame, struct pg_eval_configuration *current);

/* Parent-stack data: only the active frame may have started answer readback.
 * Parent order and all caller links share the active answer's relocation table.
 * As above, continuations/state and machine flags must be restored by owners. */
int pg_eval_frames_payload_write(FILE *file, const struct pg_eval_frame *frames,
	const struct pg_eval_configuration *current, const struct pg_graph_codec *codec, void *owner);
int pg_eval_frames_payload_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_eval_frame **frames, struct pg_eval_configuration *current);

/* Owning payload composition, using the same callback contract as configuration
 * forests above. Callback state needs cleanup even after an outer read failure.
 * All frame, readback and owner Term roots must share one relocation table. */
int pg_eval_frames_payload_write_with(FILE *file, const struct pg_eval_frame *frames,
	const struct pg_eval_configuration *current,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *owner);
int pg_eval_frames_payload_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *, const struct pg_term *const **, void *), void *owner,
	struct pg_eval_frame **frames, struct pg_eval_configuration *current);

#endif
