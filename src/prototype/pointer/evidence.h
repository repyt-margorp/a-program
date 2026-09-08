#ifndef A_PROGRAM_POINTER_EVIDENCE_H
#define A_PROGRAM_POINTER_EVIDENCE_H

#include "typing.h"
#include "classifier.h"
#include "conversion.h"
#include "identity.h"

enum pg_evidence_rule { PG_CONTEXT_EMPTY, PG_CONTEXT_EXTEND, PG_UNIVERSE_FORM, PG_VARIABLE,
	PG_TYPE_FROM_VALUE, PG_RETURN_TYPE_FORM, PG_THUNK_TYPE_FORM, PG_PI_FORM,
	PG_RETURN_INTRO, PG_THUNK_INTRO, PG_FORCE_ELIM, PG_LAMBDA_INTRO, PG_APP_ELIM,
	PG_VALUE_FROM_TYPE, PG_TYPE_CONVERSION, PG_CONTEXT_PROJECTION,
	PG_CONTEXT_SUBSTITUTION, PG_REINDEX, PG_THUNK_CONTENT, PG_PI_CODOMAIN, PG_PI_DOMAIN,
	PG_RETURN_CONTENT, PG_PI_CONSTANT_CODOMAIN, PG_FOLD_ELIM,
	PG_IDENTITY_FORM, PG_IDENTITY_INSTANCE, PG_REFLEXIVITY,
	PG_IDENTITY_LEFT_TYPE, PG_IDENTITY_RIGHT_TYPE, PG_FAMILY_IDENTITY_FORM, PG_PURE_NORMALIZATION,
	PG_RETURN_VALUE, PG_THUNK_COMPUTATION, PG_FAMILY_ACTION,
	PG_IDENTITY_TRANSPORT, PG_IDENTITY_LIFT, PG_INDUCTIVE_FORM, PG_CONSTRUCTOR_INTRO };
enum pg_evidence_judgement { PG_JUDGEMENT_CONTEXT, PG_JUDGEMENT_VALUE_TYPE,
	PG_JUDGEMENT_COMPUTATION_TYPE, PG_JUDGEMENT_VALUE, PG_JUDGEMENT_COMPUTATION,
	PG_JUDGEMENT_SUBSTITUTION };
struct pg_evidence;
struct pg_data_schema;

/* Zero-index strictly-positive inductive formation. The schema parameter
 * context must end in the distinguished Self : Universe_l assumption.
 * Discharges precisely Self; other parameters remain explicit in the Core.
 * Indexed formation and datatype higher computation are not implemented here. */
const struct pg_evidence *pg_prove_inductive_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema);
/* Instantiate Gamma, then Self with the admitted family, then all fields.
 * No caller-provided result type or expected-type-guided field inference. */
const struct pg_evidence *pg_prove_constructor(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *fields);
/* Derived curried constructor computation: Lambda fields. RETURN constructor.
 * Zero fields yields RETURN directly. Uses fresh lexical field binders, not
 * a value-side Pi or a new proof rule. Schedule once per wrapper request. */
const struct pg_evidence *pg_prove_constructor_function(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters);

/* Borrowed view of an explicit Identity formation's immutable premises.
 * family is a formation for IDENTITY_FORM/FAMILY_IDENTITY_FORM and a selected
 * universe identification value for IDENTITY_INSTANCE. Substitutions/paths
 * occur only for FAMILY_IDENTITY_FORM; path_count is not a cube dimension. */
struct pg_identity_boundary {
	const struct pg_evidence *family;
	const struct pg_evidence *left, *right;
	const struct pg_evidence *left_substitution, *right_substitution;
	size_t path_count;
	const struct pg_evidence *const *paths;
};
/* No traversal, allocation or conversion. Returns zero without modifying
 * output for any other derivation. Reindex recovery belongs to action.h. */
int pg_identity_boundary_view(const struct pg_evidence *formation,
	struct pg_identity_boundary *output);

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
/* Homogeneous Identity formation has the base formation's polarity. It does
 * not prove the endpoints equal or execute computational endpoints. */
const struct pg_evidence *pg_prove_identity_type(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *left,
	const struct pg_evidence *right);
/* R : Id Universe_i A B, x : A, y : B yield R x y : Universe_i.
 * This is family instantiation, not Pi elimination. R is retained explicitly;
 * an arbitrary function/relation is not a universe Identity witness. */
const struct pg_evidence *pg_prove_identity_instance(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *left, const struct pg_evidence *right);
/* Fibrant value-universe Identity fields. RIGHT sends x:A to B and lifts to
 * R x (trr R x); LEFT sends y:B to A and lifts to R (trl R y) y.
 * No raw computation is used as an endpoint or coerced to a universe value. */
const struct pg_evidence *pg_prove_identity_transport(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction);
const struct pg_evidence *pg_prove_identity_lift(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction);
/* Regularity of R : Id Universe_i A B gives A/B : Universe_i. The
 * accepted R, not an untyped family spine, is the premise. */
const struct pg_evidence *pg_prove_identity_endpoint_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	enum pg_evidence_rule side);
/* C type in Gamma,Delta; the substitutions agree on Gamma and paths contains
 * one checked center for each declaration of Delta, in declaration order.
 * Each center uses that declaration's family acted along preceding centers.
 * All centers share one direction, not iterated refl. Zero centers means
 * diagonal action after the common substitution. Preserves C's polarity. */
const struct pg_evidence *pg_prove_family_identity_type(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths,
	const struct pg_evidence *left, const struct pg_evidence *right);
/* t : C in Gamma,Delta acts along the same checked boundary as C. Endpoints
 * are t[left]/t[right], not caller-supplied witnesses. Preserves polarity. */
const struct pg_evidence *pg_prove_family_action(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *term,
	const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths);
/* Symbolic diagonal action; no endpoint conversion is registered globally. */
const struct pg_evidence *pg_prove_reflexivity(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *term);

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
/* Abstract exactly context's suffix after prefix, inside out, using ordinary
 * Pi formation and Lambda introduction. Body must already be a computation
 * in context; no expected type, implicit RETURN or new proof rule is used.
 * A zero-length suffix preserves body. All contexts/proofs belong to typing. */
const struct pg_evidence *pg_prove_abstract(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *prefix,
	const struct pg_evidence *context, const struct pg_evidence *body);
const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
/* The certificate and its endpoint graphs must outlive typing->graph. */
const struct pg_evidence *pg_prove_conversion(struct pg_typing *typing,
	const struct pg_evidence *term, const struct pg_evidence *target_type,
	const struct pg_conversion_certificate *certificate);
const struct pg_conversion_certificate *pg_evidence_conversion(const struct pg_evidence *evidence);
/* Subject reduction for the fixed kernel-pure rules. Requires an accepted
 * source and a directed completed WHNF/NF reduction from exactly that Core.
 * A symmetric conversion certificate cannot justify an arbitrary expansion.
 * The receipt's graph/policy and source evidence must outlive the result. */
const struct pg_evidence *pg_prove_normalization(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_reduction_certificate *certificate);
const struct pg_reduction_certificate *pg_evidence_normalization(const struct pg_evidence *evidence);
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
/* Extend a substitution into a prefix to the supplied full source context.
 * Exactly the remaining declarations receive values, checked by the ordinary
 * simultaneous substitution rule; the accepted prefix is not rechecked.
 * No new derivation rule or normalization. */
const struct pg_evidence *pg_prove_substitution_extend(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_evidence *source,
	size_t count, const struct pg_evidence *const *values);
const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
struct pg_reindex_state;
struct pg_reindex { struct pg_reindex_state *state; };
enum pg_reindex_status { PG_REINDEX_PENDING, PG_REINDEX_DONE, PG_REINDEX_ERROR };
/* Same rule as pg_prove_reindex, with suspended term/classifier substitution.
 * No evidence is exposed before all outputs are constructed. Typing and input
 * proofs must outlive the work; accepted evidence survives work destruction. */
int pg_reindex_init(struct pg_reindex *work, struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
enum pg_reindex_status pg_reindex_advance(struct pg_reindex *work, uint64_t budget);
enum pg_reindex_status pg_reindex_status(const struct pg_reindex *work);
uint64_t pg_reindex_steps(const struct pg_reindex *work);
const struct pg_evidence *pg_reindex_result(const struct pg_reindex *work);
void pg_reindex_destroy(struct pg_reindex *work);
/* Invert accepted RETURN v : F A or THUNK M : U C judgements with canonical
 * heads. Retains the input proof; never executes M or guesses a type from Core.
 * Symbolic heads must first be normalized with evidence and converted. */
const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation);
const struct pg_evidence *pg_prove_thunk_computation(struct pg_typing *typing,
	const struct pg_evidence *value);
/* first : Delta -> Gamma, second : Theta -> Delta; result : Theta -> Gamma. */
const struct pg_evidence *pg_prove_substitution_compose(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second);
/* Pair sigma : Delta -> Gamma with a : A[sigma] in Delta, producing
 * (sigma,a) : Delta -> Gamma,x:A. The result uses the ordinary checked
 * substitution representation; no function application or computation runs. */
const struct pg_evidence *pg_prove_substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *image);
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
/* Storage provenance only; this does not validate a rule-specific premise. */
int pg_evidence_owned_by(const struct pg_evidence *evidence, const struct pg_typing *typing);
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence);
/* Context formation has no term subject or classifier. */
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence);
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence);
size_t pg_evidence_premise_count(const struct pg_evidence *evidence);
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index);

#endif
