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
	PG_IDENTITY_TRANSPORT, PG_IDENTITY_LIFT, PG_INDUCTIVE_FORM, PG_CONSTRUCTOR_INTRO,
	PG_MATCH_ELIM, PG_INDUCTION_ELIM, PG_EFFECT_SUBSUMPTION, PG_REQUEST_INTRO, PG_HANDLER_ELIM,
	PG_CONTEXT_FAMILY_EXTEND, PG_TYPE_FAMILY_APP, PG_TYPE_FAMILY_ABSTRACT, PG_TYPE_CASE,
	PG_TERMINATION_FORM, PG_TERMINATION_INTRO, PG_TOTAL_PURE_VALUE,
	PG_HOST_TYPE_FORM, PG_HOST_VALUE_INTRO, PG_HOST_FUNCTION_INTRO };
struct pg_evidence;
/* Closed host descriptors are checked against the fixed host contract. A raw
 * semantic reference or arbitrary signature cannot establish these premises. */
const struct pg_evidence *pg_prove_host_type(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_object *type);
const struct pg_evidence *pg_prove_host_value(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_object *value);
const struct pg_evidence *pg_prove_host_function(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_object *function);
/* Formation accepts a checked suspended computation, without requiring TOTAL.
 * Introduction additionally requires a TOTAL result contract for that same
 * suspended term in that context. Neither rule runs the computation or treats
 * an empty operation row as a termination proof. TOTAL is conditional on
 * returning operation interpretations, not a guarantee about a host handler. */
const struct pg_evidence *pg_prove_termination_type(struct pg_typing *typing,
	const struct pg_evidence *type,
	const struct pg_evidence *suspended);
const struct pg_evidence *pg_prove_termination(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *suspended);
/* A scoped family hypothesis over a nonempty value/family telescope. The signature
 * is an ordinary Pi-shaped Core term ending in Universe, not a CBPV value
 * function type. Family evidence cannot be RETURNed, FORCEd or coerced to a
 * Universe inhabitant until all indices have been supplied. */
const struct pg_evidence *pg_prove_family_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *indices, const struct pg_evidence *universe);
const struct pg_evidence *pg_prove_family_application(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *index);
/* Abstract a checked type/family over one checked value or family binder, retaining
 * its dependent signature. No computation-to-type extraction is performed. */
const struct pg_evidence *pg_prove_family_abstraction(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *body);
/* Nonrecursive elimination into value-type universes. Each branch is a
 * checked type family over exactly its constructor fields (a value type for
 * nullary constructors). Uses ordinary Match Core/iota, not Comp-to-value
 * inversion. The result universe bounds all branch universes. */
const struct pg_evidence *pg_prove_type_case(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	size_t count, const struct pg_evidence *const *branches);
/* Expected branch classifier, formed by the same constructor/motive
 * substitution as Match/induction. Does not synthesize or check a body. */
const struct pg_evidence *pg_prove_match_branch_type(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *fields);
struct pg_data_schema;
struct pg_data_declaration;
struct pg_operation_declaration;
/* Fresh inert label with immutable raw signature terms, not typing evidence.
 * Owned by graph; referenced terms must outlive it. No interning by signature. */
const struct pg_object *pg_operation_label_create(struct pg_graph *graph,
	const struct pg_term *payload, const struct pg_term *response);
int pg_operation_label_types(const struct pg_object *label,
	const struct pg_term **payload, const struct pg_term **response);
/* Check the signature of an existing label using local, closed value-type
 * evidence. Exact label/signature-proof tuples reuse the same declaration. */
const struct pg_operation_declaration *pg_operation_declaration_at(struct pg_typing *typing,
	const struct pg_object *label, const struct pg_evidence *payload_type,
	const struct pg_evidence *response_type);
/* Fresh nominal operation with closed value-type payload/response signatures.
 * Aliases reuse its label; a label alone does not confer typing acceptance.
 * No runtime implementation, handler permission or totality is asserted. */
const struct pg_operation_declaration *pg_operation_declaration(struct pg_typing *typing,
	const struct pg_evidence *payload_type, const struct pg_evidence *response_type);
const struct pg_object *pg_operation_label(const struct pg_operation_declaration *declaration);
const struct pg_operation_declaration *pg_evidence_request_declaration(const struct pg_evidence *evidence);
const struct pg_evidence *pg_operation_payload_type(const struct pg_operation_declaration *declaration);
const struct pg_evidence *pg_operation_response_type(const struct pg_operation_declaration *declaration);
/* Derived raw Lambda a. request op a (Lambda b. RETURN b). Like constructor
 * wrappers, allocates fresh lexical binders; schedule once per named producer. */
const struct pg_evidence *pg_prove_operation_function(struct pg_typing *typing,
	const struct pg_operation_declaration *declaration);
/* Request op a k, with k : Pi(B,F E C) independent of its response binder.
 * Produces F ({op} union E) C without executing the operation. */
const struct pg_evidence *pg_prove_request(struct pg_typing *typing,
	const struct pg_operation_declaration *declaration,
	const struct pg_evidence *payload, const struct pg_evidence *continuation);
struct pg_handler_clause {
	const struct pg_operation_declaration *operation;
	const struct pg_evidence *body;
};
/* Ordered nominal labels only; signature evidence and bodies are premises. */
struct pg_handler_signature;
const struct pg_handler_signature *pg_handler_signature(struct pg_graph *graph,
	size_t count, const struct pg_object *const *labels);
size_t pg_handler_signature_count(const struct pg_handler_signature *signature);
const struct pg_object *pg_handler_signature_label(
	const struct pg_handler_signature *signature, size_t index);
const struct pg_handler_signature *pg_evidence_handler_signature(const struct pg_evidence *evidence);
const struct pg_term *pg_handler_signature_reference(struct pg_graph *graph, const struct pg_handler_signature *signature);
const struct pg_handler_signature *pg_handler_signature_view(const struct pg_term *term);
/* Extend the carrier's context by payload:A and resume:U(Pi(B,carrier)).
 * Only binder types are supplied; a clause body must still synthesize its
 * own classifier and pass pg_prove_handler. No handler term is accepted here. */
const struct pg_evidence *pg_prove_handler_context(struct pg_typing *typing,
	const struct pg_operation_declaration *operation, const struct pg_evidence *context,
	const struct pg_evidence *carrier, const struct pg_object *payload, const struct pg_object *resume);
/* Nondependent deep handler at a checked F G C carrier. Clauses are raw
 * Lambda payload. Lambda (U(Pi(response,F G C))). computation.
 * Input effects not handled here and all clause/return effects must fit G. */
const struct pg_evidence *pg_prove_handler(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *returned,
	const struct pg_evidence *carrier, size_t count, const struct pg_handler_clause *clauses);
/* Closed-row computation-type formation. The row is a syntactic set of labels,
 * not evidence of an operation's signature, execution, or termination. */
const struct pg_evidence *pg_prove_effect_type(struct pg_typing *typing,
	const struct pg_effect_row *effects,
	const struct pg_evidence *value_type);
const struct pg_evidence *pg_prove_computation_type(struct pg_typing *typing,
	enum pg_totality totality,
	const struct pg_effect_row *effects, const struct pg_evidence *value_type);
/* RETURN has a total derivation regardless of whether the returned value is
 * itself a thunk. Selecting UNSPECIFIED explicitly forgets that guarantee. */
const struct pg_evidence *pg_prove_return_contract(struct pg_typing *typing,
	enum pg_totality totality,
	const struct pg_evidence *value);
/* Directed closed-row widening and TOTAL-to-UNSPECIFIED weakening, retaining
 * the already synthesized computation. Never strengthens an unknown contract.
 * Target formation and result-type agreement are premises, not inference hints. */
const struct pg_evidence *pg_prove_effect_subsumption(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *target_type);

/* Strictly-positive inductive formation. The schema parameter context ends
 * in Self : Universe_l or a scoped Self-family signature over value indices.
 * Discharges precisely Self; other parameters remain explicit in the Core.
 * Indexed declarations yield TYPE_FAMILY evidence, not a CBPV value or
 * computation; a saturated family application proves a fiber is a type.
 * Datatype higher computation is not implemented here. */
const struct pg_evidence *pg_prove_inductive_type(struct pg_typing *typing,
	const struct pg_data_schema *schema);
const struct pg_data_declaration *pg_evidence_inductive_declaration(const struct pg_evidence *evidence);
const struct pg_object *pg_evidence_constructor(const struct pg_evidence *evidence);
struct pg_inductive_instance {
	const struct pg_data_schema *schema;
	const struct pg_evidence *formation;
	const struct pg_evidence *parameters;
	/* Saturated index substitution, including the parameter/Self prefix.
	 * NULL for an unapplied family or a non-indexed declaration. */
	const struct pg_evidence *indices;
};
/* Shared nominal lookup by accepted typed subject. Traversal fuel includes
 * application-body dependencies; individual kernel/selected-Pi checks retain
 * their own cost. The completed instance is borrowed from the typing store. */
struct pg_typed_query *pg_inductive_request(struct pg_typing *typing,
	const struct pg_evidence *type);
const struct pg_inductive_instance *pg_inductive_query_result(const struct pg_typed_query *work);
/* Recover nominal formation and its parameter map from typed construction,
 * including context maps, selected components and pure computation inputs.
 * The nominal declaration still requires its exact accepted formation.
 * No global search by Core. Returns zero and leaves output unchanged when
 * provenance is unavailable; this is not evidence of a non-inductive type. */
int pg_inductive_instance(struct pg_typing *typing, const struct pg_evidence *type,
	struct pg_inductive_instance *output);
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
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters);
/* Fresh instantiated field telescope, returned as a checked substitution into
 * the conditional schema context. Its destination extends parameters'
 * destination by exactly the fields. Shared by introduction and elimination. */
const struct pg_evidence *pg_prove_constructor_scope(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters);
/* Retain field binder identities only. The context suffix must have the field
 * arity over the parameter target; ordinary lifting recomputes field types. */
const struct pg_evidence *pg_prove_constructor_scope_at(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, const struct pg_context *allocation);
/* Derived ordinary context: destination, generic indices, z:Family indices.
 * Shares telescope lifting with constructor scopes; creates no new rule. */
const struct pg_evidence *pg_prove_inductive_motive_context(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_object *binder);
int pg_inductive_motive_context_valid(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context);
/* Expose a substituted Match/induction using the same elimination rule.
 * Lift its generic motive telescope, not the scrutinee's fixed fiber.
 * Returns ordinary evidence, with no new conversion or reduction rule. */
const struct pg_evidence *pg_prove_elimination_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution,
	const struct pg_evidence *elimination);
/* Select a checked typed field by its nominal schema binder, including scoped
 * and congruently normalized inputs. Preserve its classifier; do not retag a
 * dependent field after normalizing an earlier field. Computed constructions
 * still require exposure. Neutral/unexposed values return NULL. No new rule. */
const struct pg_evidence *pg_prove_constructor_field(struct pg_typing *typing,
	const struct pg_evidence *value, const struct pg_object *field);
/* Expose Match/induction at a retained constructor introduction. Apply fields
 * and suspended recursive eliminations through ordinary typed substitution.
 * Function fields sequence their returned child before induction. This is one
 * step, not recursive normalization. Neutral scrutinees and unsupported
 * dependent Fold results return NULL. No new proof rule. */
const struct pg_evidence *pg_prove_elimination_body(struct pg_typing *typing,
	const struct pg_evidence *elimination);
/* Factor instance through a constructor refinement. Recover images of the
 * refinement's variables from the retained constructor and source images,
 * then check the entire composite. No inversion of reduction or unification. */
const struct pg_evidence *pg_prove_refinement_factor(struct pg_typing *typing,
	const struct pg_evidence *refinement, const struct pg_evidence *instance,
	const struct pg_object *scrutinee);
/* Reassemble constructor-refined branches in the original context. Each map
 * must be the corresponding constructor refinement, up to typed binder
 * renaming below its shared prefix. Dependent suffix declarations are abstracted
 * in the Match motive and supplied after elimination. This is derived
 * Match/Pi/APP, not a rule. */
const struct pg_evidence *pg_prove_refined_match(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *scrutinee, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *refinements,
	const struct pg_evidence *const *branches);
/* Eta-expand a declared, parameter-instantiated indexed family into an
 * ordinary raw Pi computation returning its type as a Universe value.
 * Uses checked index scopes, family application, RETURN and Lambda; not a
 * new admission rule or a coercion of an arbitrary computation to a family. */
const struct pg_evidence *pg_prove_inductive_family_function(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters);
/* Checked motive substitution shared by Match, IH construction and Solve.
 * The value's fiber evidence supplies its indices; no expected type input. */
const struct pg_evidence *pg_prove_inductive_motive_substitution(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *source,
	const struct pg_evidence *destination, const struct pg_evidence *value);
/* Specialize a context at a constructor of a variable scrutinee. Its indices
 * must be distinct variables. Replace those bindings together, and lift the
 * remaining dependent suffix over the constructor fields. The returned map
 * targets context; it is not equality evidence for an arbitrary scrutinee. */
const struct pg_evidence *pg_prove_constructor_refinement(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *scrutinee, const struct pg_object *constructor);
const struct pg_evidence *pg_prove_inductive_motive_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters,
	const struct pg_evidence *source, const struct pg_evidence *motive,
	const struct pg_evidence *destination, const struct pg_evidence *value);
/* IH classifier from the field's checked type. Function fields open their Pi
 * telescope and sequence its pure returned recursive value into the motive.
 * The motive may depend on indices/arguments, but not on that returned value.
 * Direct fields retain fully dependent motive substitution. No value is
 * extracted from a neutral computation; this builds type formation only.
 * Optional allocation supplies Pi binders, never trusted type formation. */
const struct pg_evidence *pg_prove_inductive_hypothesis_type(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive, const struct pg_evidence *context,
	const struct pg_evidence *field, const struct pg_term *allocation);
/* Dependent case elimination (no recursive IH). Motive is a computation-type
 * formation in destination,indices,z:Family indices. Branches are already
 * synthesized computations in destination, ordered by the schema, curried
 * over each constructor's fields. Check their classifiers against the motive
 * instantiated at that constructor; no branch synthesis or conversion search.
 * The result substitutes the scrutinee's indices and value into the motive,
 * including a raw Pi when appropriate. Zero indices uses the same rule. */
const struct pg_evidence *pg_prove_match(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches);
/* Conditional induction branch context for supported recursive field shapes.
 * Returns the constructor field substitution projected into a destination
 * extended by one IH value per recursive field, in field order. Direct fields
 * use U(motive[field/z]); function fields use the checked mapped Pi telescope.
 * IHs are assumptions, not proven inhabitants or a completed induction rule.
 * Unsupported recursive shapes return NULL, never omit an IH. */
const struct pg_evidence *pg_prove_induction_scope(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive);
/* Same rule with retained field/IH binder identities. The exact suffix length
 * is derived from the declaration; all field and IH types are recomputed. */
const struct pg_evidence *pg_prove_induction_scope_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_context *allocation);
/* Indexed induction. Branches abstract fields, then the IH values
 * from induction_scope. No unrestricted recursive function enters their
 * typing context. Function fields sequence their pure result into recursion. */
const struct pg_induction_allocation *pg_evidence_induction_allocation(const struct pg_evidence *evidence);
const struct pg_evidence *pg_prove_induction(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches);
/* Reconstruct with explicit allocation. Distinct valid allocations produce
 * distinct constructions; no existing derivation is replaced. Default
 * allocation requests are memoized separately from accepted derivations. */
const struct pg_evidence *pg_prove_induction_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches,
	const struct pg_induction_allocation *allocation);
/* Weaken an independently synthesized fields-only case function with unused
 * IH arguments. Derived projection/application/abstraction, not resynthesis. */
const struct pg_evidence *pg_prove_induction_case(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *branch);

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
	const struct pg_evidence *context, uint64_t level);
const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder);
/* A value in a value-type universe determines a value type. This operation
 * cannot turn a computation-type formation into a value-type formation. */
const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value);
/* Russell-style value universes. Computation formation has a universe bound,
 * but is not a value inhabiting that universe. */
const struct pg_evidence *pg_prove_type_value(struct pg_typing *typing, const struct pg_evidence *type);
const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	const struct pg_evidence *value_type);
const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	const struct pg_evidence *computation_type);
/* The checked final context binding determines the parameter contract, including
 * logical families. The result is a computation type, never a value-side Pi.
 * The first structural input is the value-domain formation in the parent
 * scope, or the declared family variable in the extended scope. The second
 * input is the checked codomain in that extended scope. */
const struct pg_evidence *pg_prove_pi(struct pg_typing *typing,
	const struct pg_evidence *extended_context,
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
	const struct pg_evidence *family,
	const struct pg_evidence *left, const struct pg_evidence *right);
/* Fibrant value-universe Identity fields. RIGHT sends x:A to B and lifts to
 * R x (trr R x); LEFT sends y:B to A and lifts to R (trl R y) y.
 * No raw computation is used as an endpoint or coerced to a universe value. */
const struct pg_evidence *pg_prove_identity_transport(struct pg_typing *typing,
	const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction);
const struct pg_evidence *pg_prove_identity_lift(struct pg_typing *typing,
	const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction);
/* Regularity of R : Id Universe_i A B gives A/B : Universe_i. The
 * accepted R, not an untyped family spine, is the premise. */
const struct pg_evidence *pg_prove_identity_endpoint_type(struct pg_typing *typing,
	const struct pg_evidence *family,
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
	const struct pg_evidence *value);
const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	const struct pg_evidence *computation);
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
	const struct pg_evidence *prefix,
	const struct pg_evidence *context, const struct pg_evidence *body);
const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
/* Shared typed queries, keyed by exact typed subjects and query arguments,
 * not receipt identity. Advances traverse construction and context maps;
 * individual kernel certification operations retain their own cost. No host
 * request is executed, nor is Core normalization used as an interning key.
 * Status: 0 pending, 1 finished, -1 error/no supported checked body. A failure
 * is not a proof of inequality. Jobs belong to typing and are not serialized. */
struct pg_typed_query;
struct pg_typed_query *pg_application_body_request(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
/* One Match/IH unfolding, preserving the selected branch's computation.
 * Constructor exposure and successive applications are shared dependencies. */
struct pg_typed_query *pg_elimination_body_request(struct pg_typing *typing,
	const struct pg_evidence *elimination);
/* Return extraction follows typed beta, zero-clause Fold and Match/IH
 * construction. Computed scrutinees share budgeted body dependencies;
 * a root's step count includes the dependency transitions it advances.
 * Requests remain opaque: only an actual RETURN resumes the continuation.
 * A normalized input exposes its checked source recipe, not normalized fields. */
struct pg_typed_query *pg_return_body_request(struct pg_typing *typing,
	const struct pg_evidence *computation);
/* Checked construction inputs, including exposed beta/iota heads and
 * congruent normal forms. A finished
 * query with no result means that this input is not exposed, not ill-typing.
 * Descriptive-only subjects cannot seed this cache. Child/map dependencies
 * and congruence spines advance on the same budget as body dependencies. */
struct pg_typed_query *pg_typed_input_request(struct pg_typing *typing,
	const struct pg_evidence *source, size_t index);
/* Checked restriction/projection of a typed input to a requested context.
 * Shared by subject and target scope, not by erased Core. An unavailable
 * restriction is not permission to assign removed binders arbitrary values. */
struct pg_typed_query *pg_rebase_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *source);
int pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget);
const struct pg_evidence *pg_typed_query_result(const struct pg_typed_query *work);
uint64_t pg_typed_query_steps(const struct pg_typed_query *work);
/* Synchronous adapter to the same shared work. */
const struct pg_evidence *pg_prove_application_body(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument);
/* Certify a retained computation/family construction and its accumulated context
 * map through ordinary rules. Reads typed inputs, not receipt wrappers.
 * Normalized subjects expose their checked source recipe, not current WHNF.
 * The returned classifier may precede conversion/effect subsumption. */
struct pg_typed_query *pg_construction_origin_request(struct pg_typing *typing,
	const struct pg_evidence *source);
/* Available only for a completed origin query. NULL also denotes identity. */
const struct pg_evidence *pg_construction_origin_environment(const struct pg_typed_query *work);
/* Synchronous adapter, sharing the same completed origin/map result. */
const struct pg_evidence *pg_prove_construction_origin(struct pg_typing *typing,
	const struct pg_evidence *proof,
	const struct pg_evidence **environment);
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
/* Expose a construction input after pure reduction through the shared query.
 * Head-changing receipts require a matching checked result construction;
 * congruent child receipts normalize its inputs before retained maps act.
 * Unsupported heads/rebuilding phases return NULL, never historical inputs.
 * No new inference rule or erased-Core proof lookup is used. */
const struct pg_evidence *pg_prove_normalization_input(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_reduction_certificate *certificate,
	size_t index);
/* Weakening is pullback along a prefix projection. Core and the premise DAG
 * stay shared; this creates only the conclusion in the extended context. */
const struct pg_evidence *pg_prove_projection(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *proof);
/* sigma : destination -> source. Images of source binders are values in
 * destination, in declaration order (outermost first). Dependent declaration
 * types are checked after simultaneous substitution of preceding images.
 * This rule admits structural alpha equality, not implicit beta conversion. */
/* Borrow the accepted image of an exact source binder; no proof or term is
 * rebuilt. An absent binder or a foreign/non-substitution proof returns NULL. */
const struct pg_evidence *pg_substitution_image(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_object *binder);
/* Checked substitution conclusion; distinct derivations may share this map. */
const struct pg_context_map *pg_evidence_context_map(const struct pg_evidence *evidence);
const struct pg_evidence *pg_prove_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, const struct pg_evidence *const *images);
/* Prefix projection destination -> source (identity when contexts coincide).
 * Uses ordinary variable and substitution evidence, without another rule. */
const struct pg_evidence *pg_prove_substitution_projection(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination);
/* Recover the same images in another context through their retained typing
 * origins. Fails if any image or its classifier cannot be reconstructed there.
 * Uses ordinary substitution evidence, never erased free-variable admission. */
const struct pg_evidence *pg_prove_substitution_rebase(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *substitution);
/* Extend a substitution into a prefix to the supplied full source context.
 * Exactly the remaining declarations receive values, checked by the ordinary
 * simultaneous substitution rule; the accepted prefix is not rechecked.
 * No new derivation rule or normalization. */
const struct pg_evidence *pg_prove_substitution_extend(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_evidence *source,
	size_t count, const struct pg_evidence *const *values);
/* Certify context action with the supplied premise derivations. Budgeted
 * callers first advance pg_occurrence_action_request on the same map/subject;
 * this rule reuses that completed work without replacing either premise. */
const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
/* Invert accepted RETURN v : F A or THUNK M : U C judgements with canonical
 * heads. Retains the input proof; never executes M or guesses a type from Core.
 * Symbolic heads must first be normalized with evidence and converted. */
const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation);
/* A symbolic result, not eager evaluation. The checked computation must have
 * both TOTAL and an empty operation row. */
const struct pg_evidence *pg_prove_total_pure_value(struct pg_typing *typing,
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
/* Lift Delta -> Gamma to Delta,y:A[sigma] -> Gamma,x:A, with a fresh y.
 * Family declarations transport their dependent signature using the same
 * checked substitution rules. Signature-local binders are freshly allocated;
 * schedule once per producer when stable allocation is required. */
const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder);
/* Solve T[pattern] = body for a computation-type formation T. The checked
 * substitution must fix prefix; variable images in its suffix are distinct.
 * Whole nonvariable images can also abstract nominal type arguments, through
 * F/U wrappers, Pi results and type-valued parameters. Their fields supply no
 * inverse binding. Reconstructed applications and parameter substitutions
 * are checked, including dependencies between arguments. A
 * candidate must instantiate back to the original body under the full map.
 * Unused destination fields are discharged by ordinary Pi formation and
 * constant-codomain inversion. NULL includes nonpatterns and escaping fields;
 * this is a proof-producing partial solver, not a new kernel rule. */
const struct pg_evidence *pg_prove_pattern_type(struct pg_typing *typing,
	const struct pg_evidence *prefix,
	const struct pg_evidence *pattern, const struct pg_evidence *body);
/* Formation inversion retains the parent's universe upper bound. It does not
 * equate universe levels or claim to recover a minimal bound. */
const struct pg_evidence *pg_prove_thunk_content(struct pg_typing *typing,
	const struct pg_evidence *thunk_type);
const struct pg_evidence *pg_prove_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument);
/* Prefer the retained domain formation, transported through the same proof
 * maps as nominal recovery. Without an introduction origin, retain the Pi's
 * bound; no universe lowering axiom is introduced. */
const struct pg_evidence *pg_prove_pi_domain(struct pg_typing *typing,
	const struct pg_evidence *pi);
/* F E A formation implies A formation, even for nonempty E. This does not
 * extract a value from an effectful computation. */
const struct pg_evidence *pg_prove_return_content(struct pg_typing *typing,
	const struct pg_evidence *return_type);
const struct pg_evidence *pg_prove_pi_constant_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi);
/* Closed-row sequencing unions source/returning-continuation effects.
 * A raw Pi result is currently admitted only for an empty source row. */
const struct pg_evidence *pg_prove_fold(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *continuation);
/* Recover formation of an already synthesized classifier, not an expected
 * type. NULL also covers rules whose regularity action is not implemented. */
/* Shared by typed subject. Budgets cover context action, not the final kernel
 * certification. The typing store owns pending and completed requests. */
struct pg_typed_query *pg_classifier_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *term);
const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *term);
enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence);
/* Storage provenance only; this does not validate a rule-specific premise. */
int pg_evidence_owned_by(const struct pg_evidence *evidence, const struct pg_typing *typing);
/* Retain an alternative formation of the exact constructor/Match/IH result
 * classifier. Schema, motive and substitution premises stay fixed. This is not
 * an operation on directional/action premises, even if conclusions coincide. */
const struct pg_evidence *pg_prove_data_result_formation(struct pg_typing *typing,
	const struct pg_evidence *proof, const struct pg_evidence *formation);
/* Enumerate existing receipts for this exact typed subject, in acceptance
 * order. NULL after starts the enumeration. No rule runs and no acceptance is
 * inferred from an erased Core. Receipt selection must not determine semantic
 * structure; inspect the typed subject instead. */
const struct pg_evidence *pg_evidence_for_subject(const struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_evidence *after);
/* Reuse an exact accepted subject or certify its structural substitution using
 * ordinary variable/substitution/reindex rules. Descriptive nodes alone never
 * authorize a judgement. Map checking below may establish a lifted destination;
 * normalized construction is not inferred here. NULL means no derivation. */
const struct pg_evidence *pg_prove_structural_subject(struct pg_typing *typing,
	const struct pg_occurrence *subject);
/* Check a descriptive map by ordinary substitution rules. The source must be
 * accepted. An exact lift can establish its destination from accepted prefix
 * and declaration premises; other maps need an accepted destination. */
const struct pg_evidence *pg_prove_context_map(struct pg_typing *typing,
	const struct pg_context_map *map);
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence);
/* Context formation has no term subject or classifier. */
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence);
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence);
size_t pg_evidence_premise_count(const struct pg_evidence *evidence);
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index);

#endif
