#ifndef A_PROGRAM_POINTER_SYNTHESIS_H
#define A_PROGRAM_POINTER_SYNTHESIS_H

#include "syntax.h"
#include "evidence.h"

enum pg_synthesis_status { PG_SYNTHESIS_PENDING, PG_SYNTHESIS_DONE,
	PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_UNSUPPORTED, PG_SYNTHESIS_ERROR };
struct pg_source_scope;
struct pg_synthesis_job;
enum pg_definition_policy { PG_DEFINITION_IMPLICIT_THUNK, PG_DEFINITION_EXPLICIT_THUNK };
struct pg_synthesis {
	struct pg_typing *typing;
	struct pg_classifiers *classifiers;
	struct pg_whnf_work *normalization;
	struct pg_index jobs;
	struct pg_index scopes;
	struct pg_synthesis_job *ready;
	struct pg_synthesis_job *ready_tail;
	uint64_t steps;
	enum pg_definition_policy definition_policy;
};

/* Syntax, source buffers, typing, classifiers and normalization work outlive this store.
 * Requesting an expression only creates pending work; advance performs it. */
int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_whnf_work *normalization,
	enum pg_definition_policy definition_policy);
void pg_synthesis_destroy(struct pg_synthesis *synthesis);
/* Scope construction interns immutable binding inputs within this store.
 * Name spelling matters; token source offsets do not. Exact parent, context,
 * binder, evidence producer and export-scope pointers remain distinct keys.
 * This never compares Core by alpha/conversion or merges typed evidence. */
const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context);
struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Register an already accepted proof as a completed producer. The exact
 * evidence pointer, including its typed occurrence and premises, is the key.
 * No synthesis, evaluation or proof replay occurs. Only evidence owned by
 * this store's typing arena is accepted; this is not a serialized-proof loader. */
struct pg_synthesis_job *pg_synthesis_evidence(struct pg_synthesis *synthesis,
	const struct pg_evidence *proof);
/* Publish a checked term or formation under an ordinary lexical name. This
 * does not extend the typing context or insert THUNK/RETURN/FORCE. The proof
 * must be available in the parent context (prefix projection is permitted).
 * Names borrow their text as other source scopes do. The new scope shadows
 * its parent without modifying it; references share the accepted producer.
 * Serialized results must be checked before reaching this API. */
const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_evidence *proof);
/* Publish an immutable closed source scope as a namespace, not a Core term.
 * Every name in exports is public; construct it from a fresh root to select
 * exactly the exports wanted. Member lookup never falls back to the importing
 * scope. Both scopes belong to this store. A namespace alias can reuse exports.
 * name is an identifier or the intrinsic root token '#'; no names are reserved
 * inside exports. This does not load files or validate serialized evidence. */
const struct pg_source_scope *pg_synthesis_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports);
/* Publish a closed definition-root job, including a pending one. Member
 * references await the shared registration and whole-module checking jobs;
 * only that module's own names are exported, not its ambient source scope.
 * A selected root is canonicalized to its whole-definition job. Registration
 * requests work but never advances it. This does not read files or execute
 * exported computations. The module and parent belong to this store. */
const struct pg_source_scope *pg_synthesis_module_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *module);
/* Diagonal action of an input job, after its own synthesis has succeeded.
 * Both jobs belong to this store. The supplied context must be exactly the
 * input judgement's context; no expected classifier guides the producer.
 * Results use the existing checked reflexivity rule, not a Core-only lookup.
 * This scheduler API does not introduce a new surface keyword. */
struct pg_synthesis_job *pg_synthesis_reflexivity(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *input);
/* Act on a synthesized input along checked substitutions and selected paths.
 * Input belongs to this store; its source context comes from the substitutions.
 * The request does not synthesize or act immediately. Boundary evidence must
 * outlive the store. All selected paths participate in the immutable job key. */
struct pg_synthesis_job *pg_synthesis_family_action(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths);
struct pg_data_schema;
/* Wait for independent body synthesis, then post-check its classifier against
 * the constructor's pulled-back index motive and abstract the checked case.
 * Requests only record immutable inputs; conversion advances on the shared
 * work queue. This adds no surface syntax or whole-Match typing rule. */
struct pg_synthesis_job *pg_synthesis_data_case(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *body, const struct pg_data_schema *schema,
	const struct pg_object *constructor, const struct pg_evidence *motive);
/* Shared suspended typed substitution; the checked substitution/proof pair
 * determines a job. Uses the existing reindex machine, not a second traversal. */
struct pg_synthesis_job *pg_synthesis_reindex(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
/* Extend a checked substitution with an independently typed value. The
 * expected dependent field type is reindexed and compared using shared work;
 * only completed conversion evidence reaches the ordinary pairing rule. */
struct pg_synthesis_job *pg_synthesis_substitution_pair(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_evidence *image);
/* Pure checked computation -> returned value, using the same job table and
 * scheduler. The immutable context/evidence pair is the key, never bare Core.
 * Requests do not reduce; unsupported neutral heads are not negative proofs.
 * Evidence outlives this store. Primitive rule traversal is not yet budgeted. */
struct pg_synthesis_job *pg_synthesis_return(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *computation);
/* Normalize an accepted term or formation using shared pure WHNF work, then
 * retain typing through directed subject-reduction evidence. The immutable
 * typed input, not its erased Core alone, is the evidence-job key. */
struct pg_synthesis_job *pg_synthesis_normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* Strong pure normalization of the same typed inputs, with a distinct job
 * key but shared evaluator/subterm work and subject-reduction evidence.
 * This may normalize under THUNK; it is not a runtime execution request. */
struct pg_synthesis_job *pg_synthesis_nf(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* Retain the term, normalize its derived classifier and explicitly convert
 * its typing evidence. No target type is supplied to synthesis or guessed
 * from Core. An unchanged classifier preserves the original proof. */
struct pg_synthesis_job *pg_synthesis_normalize_classifier(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* Expose checked THUNK code through shared normalization and typed inversion.
 * This does not execute the stored computation or cache an effect result. */
struct pg_synthesis_job *pg_synthesis_unthunk(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *value);
/* FIFO scheduling: each ready job takes one transition before rejoining the
 * tail. Finite primitive transitions do not starve other ready work. This is
 * not a wall-clock bound on individual kernel rules or allocation. */
void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget);
enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job);
const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job);
/* Inspect the active subscription, not historical premises. A cycle query
 * returns one job on a reachable waiting cycle, or NULL. It neither advances
 * work nor rejects recursion, and an absent cycle does not prove progress. */
const struct pg_synthesis_job *pg_synthesis_dependency(const struct pg_synthesis_job *job);
const struct pg_synthesis_job *pg_synthesis_cycle(const struct pg_synthesis_job *job);
/* After a definition root has been indexed, retrieve its producer job without
 * resynthesizing it. NULL means not indexed or no such local definition.
 * Selections and their common definition root expose the same producers.
 * A completed unselected library root has no expression result of its own.
 * Registration/activation is shared by AST and scope; each selection waits
 * for whole-definition checking after resolving its member name. */
struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name);

#endif
