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
	struct pg_synthesis_job *ready;
	uint64_t steps;
	enum pg_definition_policy definition_policy;
};

/* Syntax, source buffers, typing, classifiers and normalization work outlive this store.
 * Requesting an expression only creates pending work; advance performs it. */
int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_whnf_work *normalization,
	enum pg_definition_policy definition_policy);
void pg_synthesis_destroy(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis);
const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context);
struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
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
/* Retain the term, normalize its derived classifier and explicitly convert
 * its typing evidence. No target type is supplied to synthesis or guessed
 * from Core. An unchanged classifier preserves the original proof. */
struct pg_synthesis_job *pg_synthesis_normalize_classifier(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* Expose checked THUNK code through shared normalization and typed inversion.
 * This does not execute the stored computation or cache an effect result. */
struct pg_synthesis_job *pg_synthesis_unthunk(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *value);
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
 * A completed unselected library root has no expression result of its own. */
struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name);

#endif
