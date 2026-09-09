#ifndef A_PROGRAM_POINTER_COMPUTATION_IO_H
#define A_PROGRAM_POINTER_COMPUTATION_IO_H

#include "eval_io.h"

/* Raw evaluator envelope, not acceptance of saved progress. The fixed policy
 * must match the machine dispatcher. Payload owners preserve the configuration,
 * frames and optional task in one graph using the existing codecs. Reading
 * initializes an unused machine at its final address: work may point into its
 * temporary arena. The callback installs roots/frames and registers the named
 * task without executing anything. On failure attached resources are destroyed;
 * the owner must also release any restored resources not yet attached. Output
 * graph allocations may remain. No WHNF receipt is issued from imported flags. */
int pg_computation_machine_write_with(FILE *file, const struct pg_eval *machine,
	const struct pg_eval_policy *policy,
	int (*write_payload)(FILE *, const struct pg_eval *, void *), void *owner);
int pg_computation_machine_read_with(FILE *file, struct pg_eval *machine, struct pg_graph *output,
	size_t name_limit,
	int (*read_payload)(FILE *, struct pg_eval *, const struct pg_eval_work_operation *, int, void *), void *owner,
	const struct pg_eval_policy **policy);

/* Raw production Demand stack, including named continuations and shared scope
 * ownership. No evaluation or evidence admission. Imported progress requires
 * provenance before execution can support accepted results. The caller retains
 * machine flags, policy and other deferred work separately. Destroy active answer
 * materialization before arena/output; all other payload storage is arena-owned. */
struct action_result_work;
struct action_scope;
/* Owner callbacks embed all supplied scopes and Terms in one relocation table.
 * They retain/restore any live task sharing that storage. Returned scope roots
 * must preserve order. The owner selects its versioned payload and cleans up
 * restored task resources if the outer read fails; no task is run here.
 * Extra configuration roots use the same contract as eval_io: the owning
 * format fixes their count, and their lexical links share the frame forest. */
int pg_computation_frames_write_with(FILE *file, const struct pg_eval_frame *frames,
	const struct pg_eval_configuration *current, size_t extra_count,
	const struct pg_eval_configuration *extra,
	int (*write_owner)(FILE *, size_t, const struct action_scope *const *, size_t,
		const struct pg_term *const *, void *), void *owner);
int pg_computation_frames_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_owner)(FILE *, struct pg_graph *, size_t, size_t, size_t *,
		struct action_scope *const **, size_t *, const struct pg_term *const **, void *), void *owner,
	struct pg_eval_frame **frames, struct pg_eval_configuration *current,
	size_t extra_count, const struct pg_eval_configuration **extra);
int pg_computation_frames_write(FILE *file, const struct pg_eval_frame *frames, const struct action_result_work *result,
	const struct pg_eval_configuration *current, const struct pg_graph_codec *codec, void *owner);
int pg_computation_frames_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_eval_frame **frames, struct action_result_work **result, struct pg_eval_configuration *current);

struct fold_work;
/* The Term owner can retain frame scopes in the same relocation table.
 * It owns cleanup of additional resources if outer validation fails. */
int pg_fold_work_write_with(FILE *file, const struct fold_work *work,
	size_t count, const struct pg_term *const *roots,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *owner);
int pg_fold_work_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *,
		const struct pg_term *const **, void *), void *owner,
	struct fold_work **work, size_t *count, const struct pg_term *const **roots);
/* Raw Fold construction progress and extra owner roots in one Term table.
 * Original polling/resumption use the restored structure; no binder creation
 * or reduction occurs during reading. Same provenance restriction as above. */
int pg_fold_work_write(FILE *file, const struct fold_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_fold_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct fold_work **work, size_t *count, const struct pg_term *const **roots);

struct composition_work;
int pg_symmetry_work_write_with(FILE *file, const struct composition_work *work,
	size_t count, const struct pg_term *const *roots,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *owner);
int pg_symmetry_work_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *,
		const struct pg_term *const **, void *), void *owner,
	struct composition_work **work, size_t *count, const struct pg_term *const **roots);
/* Raw symmetry composition: original owners, argument and constructed axis
 * prefix. Same shared-root/provenance/lifetime contract as Fold work. */
int pg_symmetry_work_write(FILE *file, const struct composition_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_symmetry_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct composition_work **work, size_t *count, const struct pg_term *const **roots);

struct prefix_work;
/* Prefix removal retains a captured argument, so its extra roots are full
 * configurations. Caller and argument environments share one relocation table. */
/* The configuration owner can include a Demand stack in that same forest.
 * On failure it must clean up any restored active frame materialization.
 * No callback runs the task or establishes its provenance. */
int pg_symmetry_prefix_write_with(FILE *file, const struct prefix_work *work,
	size_t count, const struct pg_eval_configuration *roots,
	int (*write_configurations)(FILE *, size_t, const struct pg_eval_configuration *, void *), void *owner);
int pg_symmetry_prefix_read_with(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit,
	int (*read_configurations)(FILE *, struct pg_graph *, size_t, size_t, size_t *,
		const struct pg_eval_configuration **, void *), void *owner,
	struct prefix_work **work, size_t *count, const struct pg_eval_configuration **roots);
int pg_symmetry_prefix_write(FILE *file, const struct prefix_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner);
int pg_symmetry_prefix_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct prefix_work **work, size_t *count, const struct pg_eval_configuration **roots);

#endif
