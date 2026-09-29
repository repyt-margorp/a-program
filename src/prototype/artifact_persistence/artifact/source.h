#ifndef A_PROGRAM_ARTIFACT_SOURCE_H
#define A_PROGRAM_ARTIFACT_SOURCE_H

#include "program.h"
#include <stdio.h>

struct pg_artifact_source;

/* Source-owned cursors/edges in an enclosing shared job mapping. The enclosing
 * image transports syntax/scopes and reconstructs request identities first.
 * Supported owners: definitions, module traversal and
 * literal/@ expressions. Other source continuations reject, never recompute.
 * Records borrow no live pointers after capture. No source results, proof flags
 * authorizing acceptance, or independent scheduler are stored here. */
const struct pg_artifact_source *pg_artifact_source_capture(struct pg_graph *,
	const struct pg_synthesis *, size_t job_count, struct pg_synthesis_job *const *jobs,
	size_t owner_count, struct pg_synthesis_job *const *owners);
int pg_artifact_source_write(FILE *, const struct pg_artifact_source *);
const struct pg_artifact_source *pg_artifact_source_read(FILE *, struct pg_graph *, size_t limit);
/* Unpublished import only. Lexical name registration is restored by source I/O,
 * not duplicated here. Uses the normal factories to reconnect body/rule edges,
 * but never accepts a result.
 * Failure requires discarding the unpublished Program. After this, recheck the
 * saved completed targets through ordinary Solve and attach pending cursors.
 * The enclosing image supplies provenance and finally restores ONE schedule. */
int pg_artifact_source_prepare(struct pg_synthesis *, const struct pg_artifact_source *,
	size_t job_count, struct pg_synthesis_job *const *jobs);
size_t pg_artifact_source_validation(const struct pg_artifact_source *, size_t *slots);
int pg_artifact_source_attach(struct pg_synthesis *, const struct pg_artifact_source *,
	size_t job_count, struct pg_synthesis_job *const *jobs);

#endif
