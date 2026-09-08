#ifndef A_PROGRAM_POINTER_DERIVATION_IO_H
#define A_PROGRAM_POINTER_DERIVATION_IO_H

#include "derivation.h"
#include "graph_io.h"
#include <stdio.h>

struct pg_effect_inference;

/* Unaccepted rule application. source/target are comparison endpoints for
 * conversion, directed endpoints for normalization, otherwise NULL. They are
 * obligations, not receipts. The parameter certificate pointers remain NULL.
 * The ordinary constructors compute the conclusion after checking premises. */
struct pg_derivation_input {
	enum pg_evidence_rule rule;
	struct pg_derivation_parameters parameters;
	/* Unresolved F-row site, mutually exclusive with parameters.effects.
	 * Its graph identity is persistent; the Solve worker is supplied separately. */
	const struct pg_object *effect_parameter;
	const struct pg_term *source, *target;
	enum pg_reduction_kind reduction_kind;
	size_t count;
	const struct pg_derivation_input *premises[];
};
int pg_derivations_write(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
int pg_derivations_write_descriptors(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const struct pg_graph_codec *codec, void *owner);
/* Save ordinary, possibly invalid/unaccepted rule inputs without running Solve.
 * Same record grammar as accepted derivations; no acceptance flag is stored.
 * Certificate pointers are forbidden: retain comparison endpoints instead.
 * Without an inference worker, open-row parameters are rejected. */
int pg_derivation_inputs_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_graph_codec *codec, void *owner);
/* Same format, with immutable effect definitions sharing the Core table.
 * No approximations, queue positions, sealing or acceptance flags are saved.
 * This captures a rule DAG, not yet all pending source-elaboration jobs. */
int pg_derivation_inputs_write_inference(FILE *file, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner);
/* Restores inputs only, with one Core relocation table and shared premise DAG.
 * Limits bound records/edges here and records in the nested Core separately.
 * No proof index is accessed, and no evaluation takes place. Outputs publish
 * only on success. Descriptor contracts are those of graph_io.h. */
int pg_derivations_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);
int pg_derivations_read_descriptors(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);
/* work is empty, initialized with graph, and remains unsealed on success.
 * The caller establishes contribution completeness before sealing and Solve.
 * Any failure poisons work until destroy; no roots are published on failure.
 * A NULL worker accepts only images without effect definitions/open rows. */
int pg_derivations_read_inference(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	struct pg_effect_inference *work, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);

#endif
