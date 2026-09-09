#ifndef A_PROGRAM_POINTER_SOURCE_IO_H
#define A_PROGRAM_POINTER_SOURCE_IO_H
#include "program.h"
#include <stdio.h>

/* Reconstructible source environments and source/definition/rule roots, including pending modules.
 * A shared producer DAG also retains prepared source annotations and their
 * source/rule operands. Selected roots retain order and aliases independently
 * of dependency order. Loading recreates annotations with the usual factory.
 * Named/module environments reference that producer DAG, including prepared
 * annotations. A shared scope/producer dependency order rejects cross-table
 * cycles before invoking the ordinary construction factories.
 * Module entries retain source-item indices and producer references, including
 * unaccepted annotations, across an unsolved read/write cycle. Ordinary
 * registration verifies each retained producer before using it.
 * Reuses immutable syntax, lexical parents, namespaces and import bindings.
 * Rule evidence is stored as unaccepted derivation inputs through the existing
 * codec, with one Core table for all rule roots. Effect contributions must be
 * complete (workers sealed); solutions are recomputed by ordinary Solve.
 * Selected source declaration origins use that same table. Retained
 * origin inputs survive save-before-Solve and do not certify the source.
 * No search state or acceptance flag is retained. Unsupported producer/scope
 * kinds fail explicitly instead of being omitted. Source roots are recomputed;
 * retaining annotation recipes is not a complete CHECKPOINT codec.
 * Streams are borrowed. */
int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots);
/* Owns the reconstructed stores in the returned program. root is the first
 * selected job, or NULL for no selections. Additional roots are graph-owned.
 * limit bounds scope/root/name data and syntax data separately. Loading never
 * advances Solve; on failure, destroys all storage and leaves outputs alone. */
struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);
#endif
