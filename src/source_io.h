#ifndef A_PROGRAM_POINTER_SOURCE_IO_H
#define A_PROGRAM_POINTER_SOURCE_IO_H
#include "program.h"
#include <stdio.h>

/* Reconstructible source environments and source/definition/rule roots, including pending modules.
 * APGSRC62 retains source/handler binding symbols and family index telescopes, preserves
 * already suspended values under surface &, admits checked
 * termination requests, and uses TOTAL source RETURN/arrow contracts with F
 * totality contracts, context-authoritative Pi premises and lexical
 * result-to-graph binder associations in the ordinary
 * context-binding records. These are names, not imported graph certificates.
 * It retains normalization requests as context/term producer edges with
 * WHNF/NF mode and optional one-time closed thunk demand, without a claimed
 * endpoint or evaluator progress.
 * A shared producer DAG also retains prepared source annotations and their
 * source/rule operands. Selected roots retain order and aliases independently
 * of dependency order. Loading recreates annotations with the usual factory.
 * Prepared constructor members retain formation/parameter producer edges and
 * optional complete field allocations. Ordinary Solve rechecks field types;
 * stored contexts supply binder identities, not accepted typing evidence.
 * Source declaration members use the same context payload and reconnect their
 * allocations before publication, including after an unsolved resave.
 * Application, Lambda/Pi, Match sequencing and Handler addresses retain syntax, enclosing lexical binders
 * and slot, independently of generated names and checking jobs. Their graph
 * objects use the same relocation table; no context proof or type annotation
 * is reconstructed solely to recover a binder. Source typing runs normally.
 * Lambda/Pi environments retain their binder reference, not a Context theorem.
 * Their annotations are synthesized again; explicit proof roots stay checked.
 * Default constructor field bindings use the same address format, with the
 * constructor pointer instead of syntax and the parameter destination's
 * binder sequence. Explicit field allocations remain independent inputs.
 * Qualified constructor uses retain field Context references in the same
 * allocation payload, without a substitution proof. Recomputed constructor
 * parameters and field types are still checked normally.
 * Recursive source Match retains motive, source-branch and erasure Contexts
 * through the shared context payload, without an origin theorem. These inputs
 * supply lexical symbols only; source types and branches are synthesized anew.
 * Handler clause bindings retain their source owner, operation producer and
 * three binders. Reading rebuilds their contexts with the newly inferred open
 * carrier through the ordinary clause builder, without a saved carrier answer.
 * Handler/Fold allocation does not retain or reconstruct a Handler proof.
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
 * Source declarations retain their nominal reference in the shared graph,
 * not an allocation-only formation theorem. Source synthesis checks fields
 * and result images against that immutable declaration; nested alpha renaming
 * uses fresh field evidence, not acceptance of loaded annotations.
 * No search state or acceptance flag is retained. Unsupported producer/scope
 * kinds fail explicitly instead of being omitted. Source roots are recomputed;
 * retaining annotation recipes is not a complete CHECKPOINT codec.
 * Streams are borrowed. */
int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots);
/* APGSRC63 optionally retains raw reduction records in the same Core table.
 * Reading leaves them in program->retained_reductions, without checking or
 * installing results. NULL reductions selects ordinary APGSRC62 RECOMPUTE.
 * Origin selection includes retained endpoints and intermediate phase Terms. */
int pg_sources_write_retained(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots, const struct pg_reduction_archive *reductions);
/* Owns the reconstructed stores in the returned program. root is the first
 * selected job, or NULL for no selections. Additional roots are graph-owned.
 * limit bounds scope/root/name data and syntax data separately. Loading never
 * advances Solve; on failure, destroys all storage and leaves outputs alone. */
struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);
#endif
