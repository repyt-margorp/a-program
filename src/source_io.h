#ifndef A_PROGRAM_POINTER_SOURCE_IO_H
#define A_PROGRAM_POINTER_SOURCE_IO_H
#include "program.h"
#include <stdio.h>

/* APGSRC70 preserves available typed construction at each producer.
 * It shares relocation with source/rule inputs and nominal declarations.
 * Reading exposes descriptive results through pg_synthesis_materialized,
 * not accepted evidence. Local Solve still checks source and annotations;
 * most pending continuations are not yet retained by this prototype.
 * Definition entry links belong to their lexical namespace, shared by all
 * selections, including namespaces without an exported module root.
 * Each namespace retains committed indexing/activation cursors and name-only
 * completion. Reading restores these links in lexical order, not typed results.
 * Failed items are rechecked by Solve. Started definition bodies reconnect
 * through their existing lexical request keys; a start bit shares the producer
 * completion byte, without a second body graph. Neither child checking progress
 * nor the pending schedule is restored by this bit. The namespace record also
 * retains its whole-module traversal ordinal. It is descriptive: ordinary Solve
 * checks every skipped prefix owner before using it, without imported acceptance.
 * Inputs-only profiles retain ordinal zero for normal recomputation.
 * Older formats reject; rebuild them from source.
 *
 * Reconstructible source environments and source/definition/rule roots, including pending modules.
 * The image retains source/handler binding symbols and family index telescopes, preserves
 * already suspended values under surface &, admits checked
 * termination requests, and uses TOTAL source RETURN/arrow contracts with F
 * totality contracts, context-authoritative Pi premises and lexical
 * result-to-graph binder associations in the ordinary
 * context-binding records. These are names, not imported graph certificates.
 * Pending/refused normalization retains context/term producer edges with
 * WHNF/NF mode and optional one-time closed thunk demand, without a claimed
 * endpoint or evaluator progress. Completed normalization shares its checked
 * input closure with direct result roots; loading still grants no acceptance.
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
 * registration attachment checks each retained producer's source recipe.
 * Reuses immutable syntax, lexical parents, namespaces and import bindings.
 * Rule evidence is stored as unaccepted derivation inputs through the existing
 * codec, with one Core table for all rule roots. Loading relocates the full
 * input DAG to canonical ordinary rule requests in one shared dependency walk;
 * it does not schedule a second DAG-expansion job for each input. This is graph
 * assembly, not proof checking: no Solve, endpoint computation or evidence
 * admission runs. Ordinary rules check the imported premises after loading.
 * Effect contributions must be
 * complete (workers sealed); solutions are recomputed by ordinary Solve.
 * Source declarations retain their nominal reference in the shared graph,
 * not an allocation-only formation theorem. Source synthesis checks fields
 * and result images against that immutable declaration; nested alpha renaming
 * uses fresh field evidence, not acceptance of loaded annotations.
 * A descriptive completion byte accompanies each producer, including modules
 * without expression results. Only explicit artifact trust may rely on it;
 * it never grants kernel acceptance. Unsupported producer/scope
 * kinds fail explicitly instead of being omitted. Source roots are recomputed;
 * retaining annotation recipes is not a complete CHECKPOINT codec.
 * Roots borrow the same checked/pending inputs as ordinary Solve consumers.
 * Checked roots allocate no scheduler node; they export unaccepted rule inputs.
 * Streams are borrowed. */
int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, const struct pg_synthesis_input *roots);
/* Same format and writer, but omit materialized producer results and completion. Preserve
 * reconstruction inputs/obligations; solving this image restarts computation.
 * This explicit recompute profile is not a progress-preserving checkpoint. */
int pg_sources_write_inputs(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, const struct pg_synthesis_input *roots);
/* Owns the reconstructed stores in the returned program. root is the first
 * selected job, or NULL for no selections. Additional roots are graph-owned.
 * Source producers retain their own restored lexical environments. The Program
 * convenience scope is empty; no second standard namespace is synthesized.
 * limit bounds scope/root/name data and syntax data separately. Loading never
 * advances Solve; on failure, destroys all storage and leaves outputs alone.
 * Only APGSRC70 is accepted. It contains no evaluator-history archive. */
struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots);

/* Source plus owner continuation, sharing the retained image's Core/object
 * table. This is transport composition, not a complete checkpoint or a backend
 * extension. materialized selects existing retained-results (1) or inputs (0).
 * A non-NULL callback selects APGRET5 inside the source envelope; ordinary
 * readers reject that form. NULL preserves the existing standalone bytes.
 * Callbacks use the supplied codec/object owner and keep referenced objects
 * alive until the write returns. Read callbacks run before source restoration
 * and boundary validation; discard their provisional outputs if reading fails.
 * The returned Program owns restored terms; loading performs no Solve. */
struct pg_graph_codec;
int pg_sources_write_with(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, const struct pg_synthesis_input *roots, int materialized,
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *), void *owner);
struct pg_program *pg_sources_read_with(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots,
	int (*continuation)(FILE *, struct pg_typing *, size_t, size_t, const struct pg_graph_codec *, void *, void *),
	void *owner);
#endif
