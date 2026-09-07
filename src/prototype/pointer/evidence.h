#ifndef A_PROGRAM_POINTER_EVIDENCE_H
#define A_PROGRAM_POINTER_EVIDENCE_H

#include "typing.h"
#include "classifier.h"
#include "conversion.h"

enum pg_evidence_rule { PG_CONTEXT_EMPTY, PG_CONTEXT_EXTEND, PG_UNIVERSE_FORM, PG_VARIABLE,
	PG_TYPE_FROM_VALUE, PG_RETURN_TYPE_FORM, PG_THUNK_TYPE_FORM, PG_PI_FORM,
	PG_RETURN_INTRO, PG_THUNK_INTRO, PG_FORCE_ELIM, PG_LAMBDA_INTRO, PG_APP_ELIM,
	PG_VALUE_FROM_TYPE, PG_TYPE_CONVERSION, PG_CONTEXT_PROJECTION,
	PG_CONTEXT_SUBSTITUTION, PG_REINDEX, PG_THUNK_CONTENT, PG_PI_CODOMAIN, PG_PI_DOMAIN,
	PG_RETURN_CONTENT, PG_PI_CONSTANT_CODOMAIN, PG_FOLD_ELIM };
enum pg_evidence_judgement { PG_JUDGEMENT_CONTEXT, PG_JUDGEMENT_VALUE_TYPE,
	PG_JUDGEMENT_COMPUTATION_TYPE, PG_JUDGEMENT_VALUE, PG_JUDGEMENT_COMPUTATION,
	PG_JUDGEMENT_SUBSTITUTION };
struct pg_evidence;

/* Checked primitive derivations, owned by typing->graph. NULL means a failed
 * premise check or allocation, not a proof of negation. No mutable proof API. */
const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing);
const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type);
const struct pg_evidence *pg_prove_universe(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context, uint64_t level);
const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder);
/* A value in a value-type universe determines a value type. This operation
 * cannot turn a computation-type formation into a value-type formation. */
const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value);
/* Russell-style value universes. Computation formation has a universe bound,
 * but is not a value inhabiting that universe. */
const struct pg_evidence *pg_prove_type_value(struct pg_typing *typing, const struct pg_evidence *type);
const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value_type);
const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation_type);
const struct pg_evidence *pg_prove_pi(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *domain, const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain);

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence);
const struct pg_evidence *pg_prove_return(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value);
const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation);
const struct pg_evidence *pg_prove_force(struct pg_typing *typing,
	const struct pg_evidence *value);
/* Lambda admits structural alpha renaming of its body classifier (including
 * binders freshened by substitution). Other conversion requires an explicit
 * derivation, not a solver run inside the primitive rule. APP is exact. */
const struct pg_evidence *pg_prove_lambda(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *body);
const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
/* The certificate and its endpoint graphs must outlive typing->graph. */
const struct pg_evidence *pg_prove_conversion(struct pg_typing *typing,
	const struct pg_evidence *term, const struct pg_evidence *target_type,
	const struct pg_conversion_certificate *certificate);
const struct pg_conversion_certificate *pg_evidence_conversion(const struct pg_evidence *evidence);
/* Weakening is pullback along a prefix projection. Core and the premise DAG
 * stay shared; this creates only the conclusion in the extended context. */
const struct pg_evidence *pg_prove_projection(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* sigma : destination -> source. Images of source binders are values in
 * destination, in declaration order (outermost first). Dependent declaration
 * types are checked after simultaneous substitution of preceding images.
 * This rule admits structural alpha equality, not implicit beta conversion. */
const struct pg_evidence *pg_prove_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, const struct pg_evidence *const *images);
const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
/* Expose existing checked image evidence or distribute a reindex through
 * APP/FORCE/FOLD. Original reindex evidence remains immutable and retained. */
const struct pg_evidence *pg_prove_reindexed_variable(struct pg_typing *typing,
	const struct pg_evidence *proof);
const struct pg_evidence *pg_prove_reindexed_elimination(struct pg_typing *typing,
	const struct pg_evidence *proof);
/* Push substitution through structural evidence before demanding computation.
 * For conversion, returns the substituted original term: the caller must
 * restore the converted classifier with ordinary checked conversion. */
const struct pg_evidence *pg_prove_reindexed_premise(struct pg_typing *typing,
	const struct pg_evidence *proof);
/* Derive a beta reduct of a checked APP with a Lambda introduction premise
 * (possibly projected/reindexed). Uses ordinary substitution/reindex evidence, not a
 * new equality axiom. NULL includes unsupported heads and failed premises. */
const struct pg_evidence *pg_reduce_beta(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *application);
/* One checked beta, FORCE/THUNK or zero-clause FOLD/RETURN step, including
 * through projection/reindex evidence. Traversal is not yet budgeted.
 * NULL also includes unsupported evidence, not just irreducible terms. */
const struct pg_evidence *pg_reduce_computation(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *computation);
struct pg_reduction {
	const struct pg_evidence *result;
	const struct pg_evidence *context;
	const struct pg_evidence *input;
};
/* Prepare a direct reduct or a demanded operand without recursively reducing
 * that operand. Nonzero includes unsupported rules and failed premises. */
int pg_prepare_reduction(struct pg_typing *typing, const struct pg_evidence *context,
	const struct pg_evidence *computation, struct pg_reduction *step);
/* Source context and premise of a checked projection/reindex, for either
 * polarity. This decomposes context action, not computation execution. */
int pg_prepare_context_action(struct pg_typing *typing, const struct pg_evidence *context,
	const struct pg_evidence *proof, struct pg_reduction *step);
/* Rebuild the demanded position with checked evidence. This is typing, not a
 * claim that an arbitrary replacement is equal to the original operand. */
const struct pg_evidence *pg_prove_computation_operand(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *operand);
/* Invert a RETURN introduction through checked context actions. This does not
 * execute an arbitrary computation or assert an equation with its result. */
const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation);
const struct pg_evidence *pg_prove_thunk_computation(struct pg_typing *typing,
	const struct pg_evidence *value);
/* first : Delta -> Gamma, second : Theta -> Delta; result : Theta -> Gamma. */
const struct pg_evidence *pg_prove_substitution_compose(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second);
/* Lift Delta -> Gamma to Delta,y:A[sigma] -> Gamma,x:A, with a fresh y. */
const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder);
/* Formation inversion retains the parent's universe upper bound. It does not
 * equate universe levels or claim to recover a minimal bound. */
const struct pg_evidence *pg_prove_thunk_content(struct pg_typing *typing,
	const struct pg_evidence *thunk_type);
const struct pg_evidence *pg_prove_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument);
const struct pg_evidence *pg_prove_pi_domain(struct pg_typing *typing,
	const struct pg_evidence *pi);
const struct pg_evidence *pg_prove_return_content(struct pg_typing *typing,
	const struct pg_evidence *return_type);
const struct pg_evidence *pg_prove_pi_constant_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi);
const struct pg_evidence *pg_prove_fold(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *continuation);
/* Recover formation of an already synthesized classifier, not an expected
 * type. NULL also covers rules whose regularity action is not implemented. */
const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term);
enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence);
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence);
/* Context formation has no term subject or classifier. */
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence);
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence);
size_t pg_evidence_premise_count(const struct pg_evidence *evidence);
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index);

#endif
