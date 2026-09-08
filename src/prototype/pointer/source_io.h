#ifndef A_PROGRAM_POINTER_SOURCE_IO_H
#define A_PROGRAM_POINTER_SOURCE_IO_H
#include "program.h"
#include <stdio.h>

/* Closed source environments and source/definition/rule roots, including pending modules.
 * Reuses immutable syntax, lexical parents, namespaces and import bindings.
 * Rule evidence is stored as unaccepted derivation inputs through the existing
 * codec, with one Core table for all rule roots. Effect contributions must be
 * complete (workers sealed); solutions are recomputed by ordinary Solve.
 * No search state or acceptance flag is retained. Unsupported producer/scope
 * kinds fail explicitly instead of being omitted. This is RECOMPUTE, not a
 * complete CHECKPOINT codec. Streams are borrowed. */
int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots);
/* Owns the reconstructed stores in the returned program. root is the first
 * selected job, or NULL for no selections. Additional roots are graph-owned.
 * limit bounds scope/root/name data and syntax data separately. Loading never
 * advances Solve; on failure, destroys all storage and leaves outputs alone. */
struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);
#endif
