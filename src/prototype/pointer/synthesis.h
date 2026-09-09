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
	/* Arena-owned initialization identity for pending jobs and source scopes.
	 * Accepted evidence has the independent lifetime of its typing store. */
	const void *owner_key;
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
 * Name spelling matters; token source offsets do not. Exact parent, context producer,
 * binder, evidence producer and export-scope pointers remain distinct keys.
 * This never compares Core by alpha/conversion or merges typed evidence. */
const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis);
/* A '*' token can name an explicitly supplied type assumption. Reading it
 * requires a universe-classified variable and retains extended_context in
 * the result. This does not create/discharge a recursive datatype signature;
 * ordinary declarations do not implicitly acquire this binding. */
const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context);
/* Reserve a lexical name over a pending context-formation producer. Consumers
 * await it and validate the exact parent/binder with the same check as bind.
 * No new context or accepted proof is fabricated by this reservation. */
const struct pg_source_scope *pg_synthesis_bind_context(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, struct pg_synthesis_job *context);
struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Nominal allocation input for a source declaration, before preparation.
 * Candidate universe inference still runs. Only the matching candidate uses
 * the stored Self binder/schema; no stored formation evidence is trusted.
 * Constructor telescope allocations must be restored separately. */
struct pg_synthesis_job *pg_synthesis_declaration_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_data_declaration *allocation);
/* Borrow immutable source inputs even after Solve. No progress/evidence fields
 * are exported. Only ordinary source-expression producers have this view. */
int pg_synthesis_source_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax);
/* Read-only enumeration of source declaration and Lambda/Pi allocations with
 * completed or retained inputs. The visitor selects its source closure and
 * deduplicates shared jobs; unrelated jobs are not implicitly image roots. */
int pg_synthesis_visit_source_allocations(const struct pg_synthesis *synthesis,
	int (*visit)(void *, struct pg_synthesis_job *), void *owner);
/* Retain the unaccepted formation input as allocation provenance, including
 * across save-before-Solve. Source inference never uses its acceptance status. */
struct pg_synthesis_job *pg_synthesis_allocation_origin(const struct pg_synthesis_job *job);
const struct pg_object *pg_synthesis_allocation_object(const struct pg_synthesis_job *job);
struct pg_synthesis_job *pg_synthesis_restore_declaration(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin);
struct pg_synthesis_job *pg_synthesis_restore_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin);
/* A definition is keyed by its registration producer and expression, not by
 * the registration worker's allocated scope. Borrow its reconstructible source
 * inputs without claiming whole-module acceptance. Available while dormant. */
int pg_synthesis_definition_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **definitions, const struct pg_syntax **expression);
/* Reserve the same definition producer before registration. Ordinary
 * registration supplies its lexical scope and activates it. An expression not
 * registered by that block is rejected, not treated as an independent body.
 * This does not add whole-module checking to a direct definition reference. */
struct pg_synthesis_job *pg_synthesis_definition_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions,
	const struct pg_syntax *expression);
/* Allocate the canonical lexical scope without registering or solving names.
 * Consumers wait for the same registration worker; this is not acceptance. */
const struct pg_source_scope *pg_synthesis_definition_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions);
/* Reconstructible lexical input, not a copied scope or another authority.
 * A parentless empty view denotes the ordinary root. At most one of producer,
 * module, exports, imports, definitions and binding is present. Handler-local
 * scopes require their own source reconstruction, not this view. */
struct pg_source_environment {
	const struct pg_source_scope *parent, *exports, *imports;
	struct pg_token name;
	struct pg_synthesis_job *producer, *module;
	const struct pg_syntax *definitions;
	struct pg_synthesis_job *binding;
};
int pg_synthesis_environment_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_source_environment *input);
/* Reserve the lexical binder before solving its Lambda/Pi domain. Requests
 * share by exact source scope and syntax, and create no context/evidence.
 * Completion yields context-extension evidence; reservation alone never does. */
struct pg_synthesis_job *pg_synthesis_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Same source allocation site, with an optional relocated binder supplied
 * before its first reservation. NULL allocates normally. A different binder
 * cannot replace an existing one, even before Solve. This carries identity,
 * not domain/context evidence, and introduces no new job or Core kind. */
struct pg_synthesis_job *pg_synthesis_binding_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_object *binder);
int pg_synthesis_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax, const struct pg_object **binder);
/* Stable even while pending or rejected; not proof that the binder is typed. */
const struct pg_object *pg_synthesis_binding_binder(const struct pg_synthesis_job *job);
/* Lexical scope is available before proof completion. Expressions requested
 * here await its context producer; no assumed domain certificate is exposed. */
const struct pg_source_scope *pg_synthesis_binding_scope(const struct pg_synthesis_job *job);
/* Open the maximal leading Lambda or Pi telescope (not both mixed), using
 * the same domain synthesis and binding jobs as ordinary expressions. Other
 * heads give an empty telescope. Only domains are synthesized: the remaining
 * body, constructor result or declaration is not checked here. The result is
 * context evidence, not a datatype admission or Pi formation certificate. */
struct pg_synthesis_job *pg_synthesis_telescope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Reserve ordered binder identities from an inert context suffix before
 * telescope traversal begins. Only binders are reused; domains synthesize
 * normally. Source arity must match. The suffix is not accepted evidence. */
struct pg_synthesis_job *pg_synthesis_telescope_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *end);
/* Structural producer shared with the checked telescope above. Completion
 * exposes lexical scope/body even if domain checking is pending or failed;
 * its proof result is always NULL. One binding is opened per transition. */
struct pg_synthesis_job *pg_synthesis_telescope_structure(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax);
/* Available after either telescope producer completes. Structural completion
 * is not context certification. Both retain the same source binder pointers. */
const struct pg_source_scope *pg_synthesis_telescope_scope(const struct pg_synthesis_job *job);
const struct pg_syntax *pg_synthesis_telescope_body(const struct pg_synthesis_job *job);
/* Register an already accepted proof as a completed producer. The exact
 * evidence pointer, including its typed occurrence and premises, is the key.
 * No synthesis, evaluation or proof replay occurs. Only evidence owned by
 * this store's typing arena is accepted; this is not a serialized-proof loader. */
struct pg_synthesis_job *pg_synthesis_evidence(struct pg_synthesis *synthesis,
	const struct pg_evidence *proof);
/* One pending callable producer per exact operation declaration. Ordinary
 * Solve constructs its Lambda/request/RETURN evidence once; names and aliases
 * can refer to this producer before completion. Signature ownership is checked
 * by the same operation function builder, not inferred from the erased Core.
 * The declaration and its signature evidence outlive this synthesis store. */
struct pg_synthesis_job *pg_synthesis_operation(struct pg_synthesis *synthesis,
	const struct pg_operation_declaration *declaration);
/* Independently synthesize a #.return clause as a raw continuation Lambda.
 * The input supplies its result-domain type, not an expected clause codomain.
 * Shared by exact scope/input/clause; input computations are not executed.
 * Recover the resulting Pi classifier to obtain the return clause's carrier.
 * This alone neither infers operation-clause effects nor accepts a handler. */
struct pg_synthesis_job *pg_synthesis_handler_return(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *input,
	const struct pg_syntax *clause);
/* Await the inferred carrier and resolve the operation label through ordinary
 * source aliases, then synthesize the clause in its checked payload/resumption
 * context. The body receives no expected result type. Completion returns its
 * nested Lambda proof; final pg_prove_handler still checks carrier/effect
 * compatibility. This API does not guess or infer a missing carrier. */
struct pg_synthesis_job *pg_synthesis_handler_clause(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *clause);
/* Assemble all clauses through the shared producers and final kernel rule.
 * Exactly one #.return clause is required, in any position. A NULL carrier
 * requests independent return-type and least positive effect inference. The
 * handler owns that equation work until synthesis destruction; all structural
 * contributions precede sealing and final kernel acceptance. Explicit carriers
 * remain post-synthesis bounds, not expected types for clause elaboration. */
struct pg_synthesis_job *pg_synthesis_handler(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *syntax);
struct pg_effect_inference;
struct pg_effect_equation;
/* Source sequencing with an independently synthesized raw continuation.
 * Pending inputs are shared. Computations use the checked fold rule; values
 * use application. If a computation has a dependent result, only a checked
 * pure RETURN may supply the argument. No host effects are executed here. */
struct pg_synthesis_job *pg_synthesis_sequence(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *input,
	struct pg_synthesis_job *continuation);
/* Recover a callable's classifier and remove the given Pi prefix with the
 * ordinary constant-codomain rules. Dependent results are not generalized.
 * Structural consumers may inspect the result before evidence acceptance;
 * use one parameter for return clauses, two for operation clauses. */
struct pg_synthesis_job *pg_synthesis_constant_result(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *callable, size_t parameters);
/* Recover a constant result value type from an independently synthesized
 * return continuation, then form F G C using the converged equation G.
 * The context is a producer and may be unresolved; acceptance awaits it.
 * Work outlives synthesis; notify sealing through effect_inference below. No clause body
 * is checked against an expected type here; final handler checking remains
 * required. Dependent/raw-Pi codomains are not coerced to F G C. */
struct pg_synthesis_job *pg_synthesis_handler_carrier(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *returned,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation);
/* Borrow a positive effect-equation graph into ordinary budgeted Solve.
 * Completion has no proof result: closed rows are read from work and must still
 * be checked by typing rules. Work outlives this synthesis store. Unsealed work
 * parks without polling. After sealing (or construction failure), call this
 * function again to notify this store; it wakes the same producer once.
 * Rules may register dependencies before sealing, without accepting a row. */
struct pg_synthesis_job *pg_synthesis_effect_inference(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work);
/* Register (the structural F row of formation minus mask) in target. This
 * awaits structural information, not accepted evidence or row convergence.
 * Completion has no proof result. The owner must await every contribution
 * before sealing work, and retain/check the original formation producer.
 * Work and target outlive synthesis. Non-F/foreign rows are never guessed. */
struct pg_synthesis_job *pg_synthesis_effect_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, struct pg_synthesis_job *formation);
/* Same registration from a raw row-expression DAG: closed/parameter leaves
 * and union nodes. Each exact row/target/mask request shares work. No row is
 * accepted as a type; await completion before sealing, as above. A rejected
 * expression may already have registered valid leaves; these are not proof
 * that the whole expression succeeded. Consumers must check the job status. */
struct pg_synthesis_job *pg_synthesis_row_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, const struct pg_term *row);
/* Substitute selected converged equation parameters in an unaccepted term.
 * Uses ordinary capture-avoiding substitution, without normalization or proof
 * acceptance. Work must outlive synthesis; notify sealing as above. The equation array is
 * copied; unselected parameters remain unchanged. Completed terms live in graph. */
struct pg_synthesis_job *pg_synthesis_effect_substitution(struct pg_synthesis *synthesis,
	const struct pg_term *term, struct pg_effect_inference *work, size_t count,
	const struct pg_effect_equation *const *equations);
const struct pg_term *pg_synthesis_effect_substitution_result(const struct pg_synthesis_job *job);
/* Resolve nominal operation identity through completed lexical aliases,
 * definition storage, quotation and successful source expectations. This is
 * shared budgeted work over producer links, not Core recognition or function
 * evaluation. Arbitrary functions and bare evidence registrations do not
 * acquire operation identity. Failed/pending producers retain their status.
 * Completion has no proof result and validates the original producer chain. */
struct pg_synthesis_job *pg_synthesis_operation_reference(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer);
/* Structural declaration lookup, possibly before reference validation. NULL
 * means unavailable, not a negative proof. Cyclic producer chains stop without
 * evaluation. Consumers must still await reference completion before accepting
 * a clause; knowing the signature does not validate an alias or expectation. */
const struct pg_operation_declaration *pg_synthesis_operation_declaration(const struct pg_synthesis_job *job);
struct pg_derivation_input;
/* Structural subject of an unaccepted formation producer. Universe/F/U/Pi
 * inputs can be inspected before row closure. Unknown rule forms await their
 * accepted formation instead. No accepted type certificate is issued;
 * callers must retain/check the original formation producer. Symbolic row
 * parameters remain symbolic even if that producer has already completed. */
struct pg_synthesis_job *pg_synthesis_type_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation);
const struct pg_term *pg_synthesis_type_structure_result(const struct pg_synthesis_job *job);
/* Structural classifier for pending VARIABLE/FORCE/THUNK/LAMBDA/APP rule inputs.
 * Prepared source VARIABLE/quotation rules share this projection too.
 * Uses binder identity through context producers, not names or accepted proof
 * guesses. Explicit classifier-normalization producers use shared pure WHNF;
 * post-check producers expose their target, without proving the check passes.
 * Other producers await acceptance. Read the raw result with the same
 * type_structure_result accessor; it supplies no typing evidence. */
struct pg_synthesis_job *pg_synthesis_classifier_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term);
/* Raw subject of supported pending rules (variables, value/type bridge,
 * projection, Lambda/APP and unary CBPV introductions/elimination). Other rules await
 * accepted subjects. No execution: an effectful computation is not its result.
 * The same type_structure_result accessor returns the structural term. */
struct pg_synthesis_job *pg_synthesis_term_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term);
/* Unaccepted stored rule DAG; request does not traverse or accept it. Inputs
 * outlive synthesis. Uses ordinary dependencies, rules and pure work. */
struct pg_synthesis_job *pg_synthesis_derivation(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input);
/* Expand the same immutable rule DAG with relocated effect parameters resolved
 * in work. Definitions must already be registered; work outlives synthesis.
 * No worker address lives in the stored inputs. Expansion is fuel-accounted;
 * ordinary rule jobs await sealing/convergence before accepting F formation. */
struct pg_synthesis_job *pg_synthesis_derivation_inference(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct pg_effect_inference *work);
/* Same unaccepted rule evaluator, with producer premises instead of loaded
 * input->premises. The input header is copied and structurally keyed together
 * with exact premise/parameter producer pointers; header allocation identity
 * does not distinguish otherwise identical rule calls. If work/equation
 * are supplied, await their closed result as the F-formation row parameter;
 * input->parameters.effects must then be NULL. No provisional proof is made.
 * The supplied array has input->count entries and is copied into the job key. */
struct pg_synthesis_job *pg_synthesis_rule(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct pg_synthesis_job *const *premises,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation);
/* Read-only export of stored input DAGs, prepared rule DAGs or completed proof jobs.
 * 0 ready, 1 an input is not prepared, -1 unsupported/error. No Solve occurs.
 * Unfinished source elaboration is not replaced by its provisional term.
 * Stored derivation inputs retain their original DAG before, during and after
 * Solve, including rejected inputs. Export does not force their expansion.
 * storage owns the transport inputs, which borrow Core objects from synthesis.
 * effects is empty and initialized with storage; it receives immutable
 * definitions from all reached workers, never their solutions/sealing flags.
 * require_closed rejects still-extensible workers with result 1. Use it for
 * self-contained images which do not retain contribution-generating work.
 * On failure effects is poisoned and roots is unchanged. This is not a whole
 * module checkpoint: callers must retain all other source obligations too. */
int pg_synthesis_export_rules(const struct pg_synthesis *synthesis, size_t count,
	struct pg_synthesis_job *const *jobs, struct pg_graph *storage,
	struct pg_effect_inference *effects, int require_closed,
	const struct pg_derivation_input *const **roots);
/* Append-only root closure for transport. observe may append to the leaf DAG
 * roots, but may not run or mutate synthesis. Exactly one of header/worker is
 * non-NULL. Headers are borrowed during the callback; premises are visited
 * separately. Outputs follow final root IDs; failure poisons effects. */
struct pg_dag;
int pg_synthesis_export_rule_closure(const struct pg_synthesis *synthesis,
	struct pg_dag *roots, struct pg_graph *storage, struct pg_effect_inference *effects,
	int require_closed,
	int (*observe)(void *, const struct pg_derivation_input *, const struct pg_effect_inference *),
	void *owner, const struct pg_derivation_input *const **result);
/* Publish a checked term or formation under an ordinary lexical name. This
 * does not extend the typing context or insert THUNK/RETURN/FORCE. The proof
 * must be available in the parent context (prefix projection is permitted).
 * Names borrow their text as other source scopes do. The new scope shadows
 * its parent without modifying it; references share the accepted producer.
 * Serialized results must be checked before reaching this API. */
const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_evidence *proof);
/* Bind an independently synthesizing producer, without guessing its type or
 * advancing it. References subscribe and project its eventual term evidence
 * into their context. Failed/pending producers cannot supply an accepted term;
 * a completed non-term job is unsupported. Scope compatibility is checked when
 * the result becomes available. A selected module export can be imported this
 * way without treating its symbol name as a namespace or a file name. */
const struct pg_source_scope *pg_synthesis_name_job(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *producer);
/* Supply the driver-selected symbol bindings for source import statements.
 * This closed scope is not made lexically visible: only explicit imports
 * introduce names. Its producers may be pending. No provider search, ambiguity
 * choice or filesystem access happens here. An absent configuration remains
 * unsupported; a missing name in a supplied complete binding set is rejected.
 * Local declarations shadow imports; imports are not implicitly re-exported. */
const struct pg_source_scope *pg_synthesis_import_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_source_scope *bindings);
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
 * outlive the store. All selected paths participate in the immutable job key.
 * After synthesis, check each path against its prefix-dependent family using
 * ordinary conversion evidence; expected families never guide input synthesis. */
struct pg_synthesis_job *pg_synthesis_family_action(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths);
/* Same request with pending path producers. Completed evidence enters through
 * pg_synthesis_evidence; it has no separate action/checking implementation. */
struct pg_synthesis_job *pg_synthesis_family_action_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	struct pg_synthesis_job *const *paths);
struct pg_data_schema;
/* Assemble field telescopes and result maps for a parsed @{...} or @\i:T=>
 * declaration in its already opened parameter scope. Constructors are checked
 * independently; no schema is exposed until every map has succeeded. The
 * result is an immutable layout/schema, not a value or an admitted datatype.
 * Recursive Self fields and nominal fibrancy still require admission rules. */
struct pg_synthesis_job *pg_synthesis_data_schema(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration);
/* Restore the nominal allocation before schema work begins. All fields and
 * result maps still synthesize normally and must validate against allocation.
 * No layout-based equality or cached formation proof is substituted. A
 * conflicting allocation or late attachment is rejected without replacing work.
 * NULL uses ordinary fresh declaration allocation. */
struct pg_synthesis_job *pg_synthesis_data_schema_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration,
	const struct pg_data_declaration *allocation);
const struct pg_data_schema *pg_synthesis_schema_result(const struct pg_synthesis_job *job);
/* In an already synthesized constructor field scope, check the syntactic
 * result '* i ...' against the declared index context. Both field and index
 * contexts extend parameters. Parameter images are fixed binder references;
 * index expressions synthesize independently before ordinary substitution
 * pairing checks their types. Returns substitution evidence, not membership,
 * positivity, fibrancy or a declaration admission certificate. */
struct pg_synthesis_job *pg_synthesis_data_result(struct pg_synthesis *synthesis,
	const struct pg_source_scope *fields, const struct pg_evidence *parameters,
	const struct pg_evidence *indices, const struct pg_syntax *result);
/* Wait for independent body synthesis, then post-check its classifier against
 * the constructor's pulled-back index motive and abstract the checked case.
 * Requests only record immutable inputs; conversion advances on the shared
 * work queue. This adds no surface syntax or whole-Match typing rule. */
struct pg_synthesis_job *pg_synthesis_data_case(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *body, const struct pg_data_schema *schema,
	const struct pg_object *constructor, const struct pg_evidence *motive);
/* Synthesize a clause under checked field/IH assumptions from an already
 * established motive. No expected result guides body synthesis; whole
 * induction checks the returned branch function separately. */
struct pg_synthesis_job *pg_synthesis_induction_branch(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_syntax *clause);
/* Recover the selected nominal declaration and parameter map, never by Core
 * lookup. Pending producers converge on the same accepted-evidence request. */
struct pg_synthesis_job *pg_synthesis_inductive_instance(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *type);
int pg_synthesis_inductive_instance_result(const struct pg_synthesis_job *job,
	struct pg_inductive_instance *output);
/* Abstract an independently synthesized body over an accepted context suffix.
 * Builds ordinary Lambda producers without waiting for the body. Context
 * binders are retained; no fresh binders or provisional proofs are created. */
struct pg_synthesis_job *pg_synthesis_abstract(struct pg_synthesis *synthesis,
	const struct pg_evidence *prefix, const struct pg_evidence *context,
	struct pg_synthesis_job *body);
/* Derive a constant computation motive from an independent branch producer.
 * The field context must extend the destination. Each removed binder needs
 * checked codomain independence; dependent results remain unsupported rather
 * than being filled from an expected classifier. */
struct pg_synthesis_job *pg_synthesis_constant_motive(struct pg_synthesis *synthesis,
	const struct pg_evidence *destination, const struct pg_evidence *fields,
	struct pg_synthesis_job *body);
/* Shared suspended typed substitution; the checked substitution/proof pair
 * determines a job. Uses the existing reindex machine, not a second traversal. */
struct pg_synthesis_job *pg_synthesis_reindex(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof);
/* Record dependencies before either input completes. The evidence API above
 * uses this same request with evidence producers; no second reindex machine.
 * Completed incompatible inputs are rejected without exposing partial proof. */
struct pg_synthesis_job *pg_synthesis_reindex_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *substitution, struct pg_synthesis_job *proof);
/* Post-check independently produced term/type evidence with matching context
 * and polarity. Explicit closed F rows permit directed effect widening after
 * result-type conversion. No expectation reaches the producer, and no Core
 * coercion is inserted. Surface :: performs its exposure before this step. */
struct pg_synthesis_job *pg_synthesis_expect(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term, struct pg_synthesis_job *type);
/* Raw CBPV application of independent producers. Exposes the computation
 * classifier, post-checks the value argument and uses ordinary APP evidence.
 * No implicit force, thunk, return or sequencing; no expected type flows
 * into either producer. Accepted inputs reuse normalization and comparison;
 * rule scheduling may remain pending even when those computations are cached. */
struct pg_synthesis_job *pg_synthesis_application(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument);
/* Same rule graph with a pending context producer. */
struct pg_synthesis_job *pg_synthesis_application_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument);
/* Prepare Gamma,x:A from a pending M:F E A, without executing M or supplying
 * its result. The caller owns the stable binder; the result is a context
 * formation producer, usable by pg_synthesis_bind_context. */
struct pg_synthesis_job *pg_synthesis_result_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *computation,
	const struct pg_object *binder);
/* Shared source/continuation Lambda construction. Context already contains
 * the binder with the supplied domain. Value bodies are lifted by RETURN;
 * raw computation bodies remain raw. All three producers may be pending. */
struct pg_synthesis_job *pg_synthesis_lambda_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *domain, struct pg_synthesis_job *context,
	struct pg_synthesis_job *body);
/* Instantiate an independently produced Universe Identity family at two
 * value endpoints. Post-check each against its own endpoint type; never
 * replace the chosen family with a homogeneous or inferred relation. */
struct pg_synthesis_job *pg_synthesis_identity_instance(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *family,
	struct pg_synthesis_job *left, struct pg_synthesis_job *right);
/* Recover retained Identity formation after its producer completes. Shared
 * by accepted input evidence, including requests from distinct producers.
 * The output is convertible to the input, not a conversion certificate;
 * callers requiring the original classifier must still post-check it. */
struct pg_synthesis_job *pg_synthesis_identity_formation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer);
/* Share a suspended endpoint derivation on the ordinary work queue. face is
 * an immutable, graph-lived canonical endpoint selector: its first coordinate
 * is fixed, followed by its ordered axes. Those axes count outer Identity
 * directions, not the full dimension of the input. Its pointer, context and
 * formation identify the request. One queue step advances one traversal
 * step, not a bounded-cost primitive proof rule. Unsupported is not rejection.
 * This is a restricted entry to identity_face, sharing its job and result;
 * it adds no surface syntax or typed center symmetry rule. */
struct pg_dimension_map;
struct pg_synthesis_job *pg_synthesis_identity_endpoint(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face);
/* Select an ordered proper face, suspending between retained-family checks
 * and endpoint traversal steps. The same immutable-input lifetime applies.
 * Permutations and hidden Identity dimensions remain unsupported. */
struct pg_synthesis_job *pg_synthesis_identity_face(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face);
/* Wait for a formation producer, then share the accepted-evidence face job.
 * A failed producer propagates its status; a completed non-formation rejects. */
struct pg_synthesis_job *pg_synthesis_identity_face_job(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *formation,
	const struct pg_dimension_map *face);
/* Select the source boundary for a permutation: permutation composed with
 * target_face = ordered composed with intrinsic. The returned producer proves
 * only the ordered source face; the caller must still act by intrinsic.
 * That permutation has strictly smaller dimension. No center is requested,
 * no typed symmetry is admitted, and intrinsic changes only on success.
 * Geometry is interned in dimensions, which must use the same graph. */
struct pg_dimensions;
struct pg_synthesis_job *pg_synthesis_permutation_source_face(struct pg_synthesis *synthesis,
	struct pg_dimensions *dimensions, const struct pg_evidence *context,
	struct pg_synthesis_job *formation, const struct pg_dimension_map *permutation,
	const struct pg_dimension_map *target_face, const struct pg_dimension_map **intrinsic);
/* Extend a checked substitution with an independently typed value. The
 * expected dependent field type is reindexed and compared using shared work;
 * only completed conversion evidence reaches the ordinary pairing rule. */
struct pg_synthesis_job *pg_synthesis_substitution_pair(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_evidence *image);
/* Build a complete substitution from independently synthesized image jobs in
 * source declaration order. Shares the index-result substitution worker: each
 * image is post-checked after preceding images determine its dependent type.
 * Requests never supply expected types to producers or publish a partial map. */
struct pg_synthesis_job *pg_synthesis_substitution(struct pg_synthesis *synthesis,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, struct pg_synthesis_job *const *images);
/* Same worker with pending source/destination contexts. Arity and context
 * validity are checked after those producers finish, before forming a map. */
struct pg_synthesis_job *pg_synthesis_substitution_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *source, struct pg_synthesis_job *destination,
	size_t count, struct pg_synthesis_job *const *images);
/* Lift a checked substitution over one source extension using ordinary
 * reindex, context-extension, variable, projection and substitution producers.
 * The caller supplies the stable destination binder. */
struct pg_synthesis_job *pg_synthesis_substitution_lift(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_object *binder);
/* Instantiate Self, then lift each constructor field. Formation and parameter
 * substitution are pending producers; fresh field binders belong to this job. */
struct pg_synthesis_job *pg_synthesis_constructor_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters);
/* Extend the shared field scope with thunked motives for direct recursive
 * fields. Input producers must ultimately justify the same nominal family. */
struct pg_synthesis_job *pg_synthesis_induction_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, struct pg_synthesis_job *motive_context,
	struct pg_synthesis_job *motive);
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
struct pg_synthesis_job *pg_synthesis_normalize_classifier_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *proof);
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
