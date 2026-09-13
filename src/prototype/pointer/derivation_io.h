#ifndef A_PROGRAM_POINTER_DERIVATION_IO_H
#define A_PROGRAM_POINTER_DERIVATION_IO_H

#include "derivation.h"
#include "graph_io.h"
#include <stdio.h>

struct pg_effect_inference;

enum { PG_DERIVATION_TERM_SLOTS = 9 };
struct pg_derivation_payload {
	size_t count, metadata_count;
	const struct pg_term *const *terms;
	const uint64_t *metadata;
};
/* Wire-order Core inputs: binder, effects, source, target, operation, handler,
 * declaration, constructor, constant, followed by induction context/binder roots.
 * Optional fixed slots are NULL. Context metadata uses context_payload.h.
 * Arrays/reference wrappers belong to scratch; input objects are borrowed.
 * This does not check an inference rule
 * or accept evidence. Shared by transport and dependency selection. */
int pg_derivation_input_terms(struct pg_graph *scratch,
	const struct pg_derivation_input *input,
	struct pg_derivation_payload *payload);
/* Collect the object dependency closure of an unaccepted premise DAG and its
 * effect definitions. Uses the same parameter packing and descriptor traversal
 * as writing. objects is an initialized leaf DAG; partial output on error must
 * not be used. No Solve, proof acceptance or temporary file is involved. */
int pg_derivation_inputs_collect_objects(struct pg_dag *objects, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner);

/* Nominal rules require the declaration graph codec (declaration_io.h).
 * Family/constructor parameters share the same Core relocation table as all
 * other terms. Their reconstruction is not evidence of rule validity. */
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
 * Raw contexts are interned in typing; no proof index is accessed and no
 * evaluation takes place. Outputs publish
 * only on success. Descriptor contracts are those of graph_io.h. */
int pg_derivations_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);
int pg_derivations_read_descriptors(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);
/* work is empty, initialized with graph, and remains unsealed on success.
 * The caller establishes contribution completeness before sealing and Solve.
 * Any failure poisons work until destroy; no roots are published on failure.
 * A NULL worker accepts only images without effect definitions/open rows. */
int pg_derivations_read_inference(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *work, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots);

#endif
