#ifndef A_PROGRAM_ARTIFACT_DERIVATION_H
#define A_PROGRAM_ARTIFACT_DERIVATION_H

#include "program.h"
#include <stdio.h>

struct pg_artifact_derivations;

/* Inert continuation metadata for imported derivation workers.
 * Raw inputs use the enclosing artifact's ordinary shared descriptor codec;
 * this payload contains only their ordinals and cursors. Direct plain-rule
 * DAGs use the parameter-only header codec without input adapter jobs. External
 * premises are mapped jobs owned/restored by the enclosing importer, never
 * replaced by equivalent derivations. They occupy the returned mapping's prefix;
 * this owner stores neither their continuations nor their completion status.
 * Imported-input roots cannot use external dependencies. Neither form accepts
 * an open effect parameter. Pending
 * plain rules must not yet have started comparison/effect/reduction work.
 * Unsupported input closures return NULL, never a recomputed checkpoint.
 * Metadata and the mapping array belong to storage; inputs and mapped jobs
 * borrow the source Program. The enclosing artifact owns the ONE shared
 * schedule, including other owner kinds, and transports it separately. */
const struct pg_artifact_derivations *pg_artifact_derivations_capture(
	struct pg_graph *storage, const struct pg_synthesis *, size_t,
	struct pg_synthesis_job *const *roots, struct pg_synthesis_job ***mapping);
const struct pg_artifact_derivations *pg_artifact_derivations_capture_with_dependencies(
	struct pg_graph *storage, const struct pg_synthesis *, size_t,
	struct pg_synthesis_job *const *roots, size_t dependency_count,
	struct pg_synthesis_job *const *dependencies, struct pg_synthesis_job ***mapping);
size_t pg_artifact_derivations_job_count(const struct pg_artifact_derivations *);
const struct pg_derivation_input *const *pg_artifact_derivations_inputs(
	const struct pg_artifact_derivations *, size_t *count);
/* Select pg_derivation_headers_{write,read} for direct rules; otherwise use
 * the full derivation-input DAG codec. No premises[] exist in header mode. */
int pg_artifact_derivations_are_headers(const struct pg_artifact_derivations *);
int pg_artifact_derivations_write(FILE *, const struct pg_artifact_derivations *);
const struct pg_artifact_derivations *pg_artifact_derivations_read(FILE *, struct pg_graph *, size_t limit);

/* Prepare in an unpublished import owner. Inputs are in capture's order, decoded
 * by the enclosing shared codec. Only ordinary factories run; no Solve or
 * evidence admission. Any failure requires discarding the unpublished Program.
 * The returned mapping is graph-owned. The enclosing importer separately
 * installs its validation schedule and eventually the saved pending schedule. */
int pg_artifact_derivations_prepare(struct pg_synthesis *,
	const struct pg_artifact_derivations *, size_t,
	const struct pg_derivation_input *const *, struct pg_effect_inference *,
	struct pg_synthesis_job ***jobs);
int pg_artifact_derivations_prepare_with_dependencies(struct pg_synthesis *,
	const struct pg_artifact_derivations *, size_t,
	const struct pg_derivation_input *const *, struct pg_effect_inference *,
	size_t dependency_count, struct pg_synthesis_job *const *dependencies,
	struct pg_synthesis_job ***jobs);
/* Enumerate plain-rule slots whose saved completion requires rechecking.
 * out has job_count capacity. No duplicate verification or status authority. */
size_t pg_artifact_derivations_validation(const struct pg_artifact_derivations *, size_t *out);
/* Spend at most min(budget, validation_limit), through the same Solve path.
 * Repeated calls continue its existing work. Imported DONE assertions select
 * checks, never supply their answers. spent must be deducted from total fuel. */
enum pg_synthesis_status pg_artifact_derivations_revalidate(struct pg_program *,
	const struct pg_artifact_derivations *, struct pg_synthesis_job *const *,
	uint64_t budget, uint64_t validation_limit, uint64_t *spent);
/* Attach only after the saved completed premises have actually been checked.
 * No checking or trust fallback here. Pending rules retain their normal kernel
 * checks. The enclosing import still owns the provenance of unfinished state. */
int pg_artifact_derivations_attach(struct pg_synthesis *,
	const struct pg_artifact_derivations *, struct pg_synthesis_job *const *);
struct pg_synthesis_job *pg_artifact_derivations_root(
	const struct pg_artifact_derivations *, struct pg_synthesis_job *const *, size_t);

#endif
