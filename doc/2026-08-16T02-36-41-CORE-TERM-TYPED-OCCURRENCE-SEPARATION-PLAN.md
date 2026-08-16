# Core Term and Typed Occurrence Separation Plan

Date: 2026-08-16

Status: implementation complete

Repository baseline:

- branch: `main`;
- commit: `977f174`;
- implementation root: `src/prototype/`;
- active artifact format: v75; and
- implementation and public-header size: 122,083 lines.

This plan is a semantic data-ownership refactor. It is not a source-layout-only
change.

Related documents:

- `2026-08-05T07-00-00-CURRENT-GRAPH-LAYERS-AND-CBPV-EFFECT-CORE.md`;
- `2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`;
- `2026-08-11T08-10-57-PROTOTYPE-SEMANTICS-PRESERVING-CODEBASE-ORGANIZATION-PLAN.md`;
- `2026-08-11T08-19-24-CURRENT-COMPILER-FUNCTIONAL-AND-THEORY-REVIEW.md`; and
- `src/prototype/spec/artifact_v75.schema`.

## 1. Objective

Make the Core Term graph the sole authority for computational topology while
retaining a smaller typed-occurrence layer that records where and under which
static assumptions a Core Term appears.

The target separation is:

```text
Core Term graph
    context-free computational topology and reduction identity

Typed Occurrence graph
    Core occurrence, Context, classifier, Context action, and source provenance

ContextDB / SubstitutionDB
    static dependency Contexts and CwF morphisms

JudgementDB
    accepted Propositions, Claims, and Derivations over typed occurrences

Runtime machine
    Core code plus runtime environment, continuation, handlers, and resources
```

The current `OperationGraph` is larger than this boundary. It repeats most Core
syntax topology and is also consumed as runtime code. This plan replaces it
with `TypedOccurrenceGraph` without erasing the occurrence identity required by
dependent typing, proof replay, resource usage, or diagnostics.

## 2. Normative Decisions

The following decisions are fixed for this migration.

### 2.1 Core Term authority

- `TermDB` is the sole authority for APP, Lambda, Match, Return, Thunk, Force,
  OperationRequest, and ComputationFold topology.
- Core reduction does not inspect a typed occurrence, classifier, Context,
  Claim, Derivation, source AST node, or source name.
- Multiple typed occurrences may reference one Core Term.
- Reduction may intern computed Core nodes into `TermDB`, as it does today.
- Object Identity evidence must not add equations to Core reduction or
  conversion.

`TermDB` may continue to contain terms that denote types. A Program uses terms
for type-level computation. The required boundary is that Core reduction of a
program term does not consult its classifier or proof, not that type-denoting
terms must live in a physically separate arena.

### 2.2 Typed occurrence authority

- A typed occurrence is identified independently of its shared Core Term.
- The occurrence owns its static Context and exact occurrence Binding
  identities.
- A frozen occurrence owns either one solved classifier selected by the
  compiler or one explicit residual-verification obligation. It never retains
  an ambiguous pending solver state.
- Solver input hints and mutable solution state do not live in the frozen
  occurrence record.
- Occurrence edges record correspondence between a parent occurrence role and
  a child occurrence. They do not redefine Core computational topology.
- Match refinement, induction ownership, and fold-clause source binders remain
  occurrence-level side data where they cannot be reconstructed from an
  alpha-shared Core node.

### 2.3 Context and proof authority

- Context cannot be reconstructed from a shared Core Term.
- `ContextDB` and `SubstitutionDB` remain separate semantic stores.
- A principal occurrence typing Proposition may be projected from one frozen
  typed occurrence through one checked API.
- Conversion, weakening, reindexing, cumulativity, alternate derivations, and
  scoped premises remain explicit Judgement evidence.
- No proof-irrelevance or canonical-proof assumption is introduced.

### 2.4 Runtime authority

- The runtime machine dispatches on Core Term tags, not occurrence tags.
- Runtime bindings are keyed by Core `BindingId`, not AST binder identity.
- Runtime resources, resumptions, environments, and continuation frames remain
  runtime-only structures. They are not static Context entries.
- Runtime verification is a separate boundary service. Ordinary Core stepping
  does not inspect compile metadata or JudgementDB.
- A residual verification plan may refer to a typed occurrence when the
  dependent CBPV contract requires runtime result checking.

### 2.5 Artifact migration

- The artifact format advances from v74 to v75.
- The v75 wire form stores typed occurrences and occurrence-specific side
  tables, not a second computational syntax graph.
- No v74 compatibility reader, permanent field alias, forwarding API, ID
  remap layer, or dual-write path remains in the completed implementation.
- Existing Operation IDs become Typed Occurrence IDs directly during the
  migration. A second ID namespace and relocation table are not introduced.

### 2.6 Semantic reading of a typed occurrence

The target model must not be described as a typed copy of the Core graph. Its
basic judgement-shaped record is instead:

```text
o = (Gamma, t, A, sigma, source provenance)

where
    t       is a context-free Core Term;
    Gamma   is the static Context in which this occurrence is checked;
    A       is the frozen classifier, when statically solved; and
    sigma   is an optional checked Context action explaining reindexing.
```

Consequently, the same Core Term `t` may occur as distinct `o0` and `o1` under
different Contexts, classifiers, source names, or proof premises. Sharing `t`
does not merge those occurrences. Conversely, creating another occurrence does
not copy APP/Lambda/Match topology into the typed layer.

A principal typing Proposition is a checked projection of a frozen occurrence:

```text
typed_occurrence(o) = (Gamma, t, A, ...)
                          |
                          v
                    Gamma |- t : A
```

Derivations justify that Proposition but are not fields of `t` or `o`.
Alternate Derivations may justify the same Claim. Evaluation of `t` neither
selects nor mutates any of them.

Static `ContextDB` must also remain distinct from the runtime environment. The
former states assumptions and dependencies used by typing and proof; the latter
maps Core `BindingId`s to runtime values. They may be connected by compilation,
but neither is reconstructed from or stored inside the other.

## 3. Current-State Audit

### 3.1 Current Core Term graph

`src/prototype/include/a_program/core/term.h` already stores the CBPV and
effect-calculus constructors:

- `APP` and `LAMBDA`;
- `MATCH` and `INDUCTION_HYPOTHESIS`;
- `RETURN`, `THUNK`, and `FORCE`;
- `OPERATION_REQUEST`; and
- `COMPUTATION_FOLD`.

It also owns Core Match cases, case binders, IH scopes, computation-fold
clauses, canonical identity, substitution, normalization, and normalization
caches.

This is the intended computational authority.

### 3.2 Current OperationGraph duplication

`src/prototype/include/a_program/graph/typed_occurrence_model.h` defines Operation
tags that repeat most executable Term tags. Each operation node also stores:

- `function`, `argument`, `body`, and `scrutinee` child Operation IDs;
- Match and computation-fold spans;
- source and projected Core Terms;
- source, known, mutable, and solved classifier-related fields;
- Context and Context-action references;
- category, computation kind, and application role;
- AST, name, and binder provenance; and
- exact Match/IH ownership coordinates.

The graph therefore performs four jobs:

1. typed source-occurrence identity;
2. a second computational topology;
3. a projection of solver state; and
4. runtime executable code.

Only the first job and occurrence-specific parts of the third job belong in the
replacement graph.

### 3.3 Current runtime dependency

`src/prototype/src/graph/operation/runtime.inc` evaluates
`metadata->operations` directly. It dispatches on Operation tags, follows
Operation child fields, and uses Operation classifiers during verification.

Runtime environments currently retain both AST binder IDs and Binding IDs, and
lookup is source-AST-oriented. This prevents the runtime from being a consumer
of context-free Core code.

### 3.4 Current solver projection

`operation_solver_solution` is documented as the solver authority, but solved
classifiers and phase approximations are copied into mutable Operation fields.
This is a checked projection by convention rather than a state transition
enforced by the C data model.

### 3.5 Current proof and artifact dependency

Operation identity appears in:

- Judgement authority and Proposition headers;
- ordered proof reconstruction and replay;
- HOTT/Identity actions;
- verification obligations;
- exports and selected entry metadata;
- artifact closure and dense publication; and
- artifact v74 `operation_graph` records.

The migration must preserve occurrence identity at all these boundaries while
removing duplicated computational shape.

### 3.6 Checked field-ownership inventory

The CT0 audit classifies every current typed-occurrence field as follows. A
field marked `delete` must not appear in the frozen v75 record.

| Current field(s) | Current role | Target authority |
| --- | --- | --- |
| `tag` | duplicated executable/source-wrapper tag | Core Term tag; source wrapper provenance for name/ascription |
| `category`, `computation_kind` | mutable polarity/result cache | derive from frozen classifier; retain only irreducible elaboration choice |
| `application_role` | typed APP rule selection | frozen occurrence semantics |
| `context_id` | static occurrence Context | typed occurrence |
| `context_action_substitution` | CwF action into current Context | typed occurrence, checked against `SubstitutionDB` |
| `source_core_term`, `source_classifier` | source of a reindexed occurrence | typed occurrence provenance |
| `core_term` | occurrence-to-Core reference | typed occurrence |
| `known_classifier` | lowering hint | solver input fact; delete from frozen occurrence |
| `classifier` | selected principal classifier | frozen typed occurrence |
| `classifier_variable` | mutable solver identity | solver workspace; delete from frozen occurrence |
| `source_ast`, symbol fields, AST binder IDs | diagnostics/source provenance | source map and occurrence side data, never Core/runtime semantics |
| `binding_id`, `binder_classifier` | exact occurrence binder semantics | typed occurrence/Context |
| `first_edge`, `edge_count` | occurrence correspondence topology | typed occurrence graph |
| `function`, `argument`, `body`, `scrutinee` | duplicate Core/occurrence topology | role-indexed occurrence edges; delete |
| IH scope/case/field plus owner | recursive occurrence origin | one IH occurrence side record |
| fold return binder fields | generated binder occurrence identity | fold occurrence side data |
| `fold_return_operation` | duplicate fold child | fold-return edge; delete |
| implicit effect-row binders | classifier generalization evidence | frozen elaboration side data |
| Match/fold spans | occurrence-specific telescopes and binders | typed occurrence side arenas keyed by Core ordinal |

Operation/occurrence IDs currently escape into labels, selected-entry metadata,
diagnostics, verification obligations, classifier constraints, Judgement
Propositions and premises, CwF/Identity actions, runtime traces, artifact roots,
publication relocation, and linker state. These remain occurrence references;
only names and API fields are changed. Effect-operation IDs and Core Term IDs
are separate namespaces and are not renamed.

The Core evaluator currently consults `TypeDeclarationDB` for four different
classes of fact: constructor owner/ordinal dispatch, constructor arity and
recursive-field coordinates, type-family parameter/index saturation, and
pure-type conversion/printing data. CT6 extracts the first three operational
facts into immutable reduction descriptors. Pure type normalization and
diagnostic printing retain explicit static-database dependencies under a
different API contract.

No `origin_occurrence` field is required. The checked triple
`context_action_substitution`, `source_core_term`, and `source_classifier`
fully states the immediate semantic origin of a transformed occurrence.
Persisting an additional historical occurrence ID would duplicate provenance
and make dense publication depend on transformation history.

## 4. Target Data Model

The exact field layout may change during CT0, but every final field must have
one of the roles below.

```c
struct prototype_typed_occurrence {
	uint32_t core_term;
	uint32_t context_id;
	uint32_t context_action_substitution;
	uint32_t classifier;
	int classifier_status;
	uint32_t classifier_verification_obligation;
	uint32_t source_ast;
	uint32_t binding_id;
	uint32_t first_edge;
	uint32_t edge_count;
};

struct prototype_typed_occurrence_edge {
	int role;
	uint32_t ordinal;
	uint32_t child_occurrence;
};
```

The intended meanings are:

- `core_term` is the Core Term valid in `context_id`;
- `context_action_substitution` records a checked reindex action when the
  occurrence is derived from another occurrence;
- `classifier` is valid only when `classifier_status` is `SOLVED`;
- `classifier_status` distinguishes mutable `PENDING`, statically `SOLVED`,
  and deliberately residual `RESIDUAL_VERIFICATION` states;
- `classifier_verification_obligation` is valid only for an explicitly
  residual occurrence and points to its runtime verification contract;
- `binding_id` is the exact typed-occurrence binding when alpha sharing prevents
  recovering it from the selected Core representative; and
- occurrence edges map structural roles to child occurrences.

No `origin_occurrence` field is added. A checked Context action together with
the source Core Term and source classifier states the immediate semantic origin
without making artifact identity depend on transformation history.

### 4.1 Edge roles

The role vocabulary must cover at least:

```text
FUNCTION
ARGUMENT
LAMBDA_BODY
MATCH_SCRUTINEE
MATCH_CASE_BODY
INDUCTION_ARGUMENT
RETURN_VALUE
THUNK_COMPUTATION
FORCE_VALUE
REQUEST_OPERATION
REQUEST_ARGUMENT
REQUEST_CONTINUATION
FOLD_COMPUTATION
FOLD_RETURN_CLAUSE
FOLD_OPERATION_CLAUSE
```

An edge validator must confirm that each role agrees with the referenced Core
Term shape. It must not copy or reinterpret that shape.

### 4.2 Source-only boundaries

Current source-reference and expected-type occurrences are not Core
computations.

- A reference occurrence records that a source name selected a particular
  typed occurrence and classifier.
- An expected-type occurrence records the `::` boundary and its explicit
  exposure Claim.
- These may remain typed-occurrence kinds because they select static rules and
  preserve source provenance. They are never runtime-dispatched Core tags.

Likewise, APP, Lambda, Match, and CBPV-shaped occurrence kinds may remain when
they select a typing rule. Their presence is not a second computational graph:
the executable child topology exists only in `TermDB`, while occurrence edges
record the correspondence needed by typing and proof replay.

### 4.3 Match, IH, and fold side tables

The following records remain because they carry occurrence semantics not
contained in Core topology:

- Match-case refined Context;
- certified refinement substitution;
- exact case Binder IDs and source Binder provenance;
- IH owner occurrence, scope, case, and recursive field coordinates;
- fold return and operation-clause Binder identities; and
- occurrence correspondence for generated clause Lambdas.

These records must reference Core case/clause ordinals rather than reproduce
their Term bodies.

### 4.4 Solver state

The mutable solver model remains separate:

```text
occurrence input facts
immutable classifier/effect/usage constraints
one mutable solution cell per occurrence
residual reason and state
```

Finalization performs exactly one of two checked projections:

1. a solved cell becomes a `SOLVED` occurrence with one frozen classifier; or
2. an intentionally dynamic dependent-CBPV boundary becomes a
   `RESIDUAL_VERIFICATION` occurrence with no classifier and one validated
   verification obligation.

All other unresolved cells are compile errors. `PENDING` is a workspace-only
state and must never cross a frozen-module or artifact boundary. Accepted proof
publication independently validates a `SOLVED` classifier. No caller may
publish a lowering hint as a solved classifier, and a residual occurrence must
not be represented as an ordinary solved Claim.

### 4.5 Runtime state

The runtime target is conceptually:

```c
struct prototype_runtime_code_ref {
	uint32_t core_term;
	uint32_t occurrence;
};

struct prototype_runtime_binding {
	uint32_t binding_id;
	struct prototype_runtime_value value;
};
```

`occurrence` may be invalid when no residual verification or source trace is
required. Runtime frames follow Core children. Occurrence edges are consulted
only to preserve source traces or locate a residual verification plan.

## 5. Authority Matrix

| Fact | Sole authority | Allowed projection/cache |
| --- | --- | --- |
| computational node tag and children | `TermDB` | read-only Term semantic view |
| alpha-shared Core identity | `TermDB` canonical key/interning | hashes and validation caches |
| typed occurrence identity | `TypedOccurrenceGraph` | dense artifact relocation |
| occurrence Context | `TypedOccurrenceGraph` referencing `ContextDB` | proof header projection |
| occurrence classifier closure | frozen solved classifier or explicit residual verification obligation | read-only inspection view |
| solver progress/residual | solver solution arena | diagnostics counters |
| Context object | `ContextDB` | lowering stack prefix ID |
| Context morphism | `SubstitutionDB` | certified substitution reference |
| accepted typing fact | Proposition/Claim | principal-occurrence projection audit |
| proof reason | Derivation DAG | reconstruction audit only |
| runtime value binding | runtime environment | trace/debug rendering |
| runtime control | runtime continuation/handler stack | runtime trace |
| persistent wire graph | artifact v75 dense graph | relocation maps during publication |

## 6. Non-Goals

This migration does not:

- split Value and Computation Terms into separate physical databases;
- erase the CBPV distinction between Value and Computation classifiers;
- encode `OPERATION_REQUEST` as ordinary APP;
- restore a separate BIND node beside `COMPUTATION_FOLD`;
- merge runtime Environment with static Context;
- merge substitution morphisms with weakening/reindex Derivations;
- infer Context from an alpha-shared Core Term;
- canonicalize or discard multiple accepted Derivations;
- add Eq, refl, transport, equality reflection, or new HOTT rules;
- change the accepted Index Family, Acc, or IF8 semantics;
- change the surface grammar;
- promote code outside `src/prototype/`; or
- retain v74 compatibility after v75 becomes authoritative.

## 7. Implementation Phases

At most one phase is `IN PROGRESS`. A phase is complete only after its focused
tests and the required structural audit pass.

### CT0. Freeze invariants and complete the field-use inventory

Status: `COMPLETE`

- [x] Record baseline build, focused integration, full integration, and artifact
  determinism results at commit `977f174`.
- [x] Inventory every read and write of every former operation-node field.
- [x] Classify each field as Core topology, occurrence semantics, solver state,
  source provenance, runtime state, proof reference, or derived cache.
- [x] Inventory every former Operation ID stored by Judgement, HOTT, verification,
  export, runtime trace, and artifact records.
- [x] Identify the current calls from Core reduction into `TypeDeclarationDB`
  and state
  the exact operational fact it consumes.
- [x] Decide whether transformed occurrence provenance needs
  `origin_occurrence` or is fully represented by a Context action.
- [x] Add the normative invariants from Section 2 to the current architecture
  documentation.

Deliverable:

- a checked field-ownership table with no `unknown` field; and
- focused regression outputs stored in the implementation progress section of
  this document.

Exit criterion: every current Operation field and every Operation-ID consumer
has one planned target representation.

### CT1. Add one Core structural projection API

Status: `COMPLETE`

- [x] Add a declarative Core child-role vocabulary under `core/term`.
- [x] Add checked APIs for child count, child role, and child Term lookup.
- [x] Cover APP, Lambda, Match, IH, Return, Thunk, Force, OperationRequest, and
  ComputationFold.
- [x] Reuse existing Match-case and fold-clause arenas; do not copy their bodies
  into a descriptor table.
- [x] Convert OperationGraph structural validation to compare against this API.
- [x] Convert graph reachability helpers to use occurrence edges plus Core role
  validation.
- [x] Add table-driven tests that every supported Core tag exposes exactly its
  expected child roles.

Primary files:

- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/src/core/term/storage_and_formation.inc`;
- `src/prototype/src/graph/typed_occurrence/graph_validation.inc`; and
- new focused checks below `src/prototype/tests/checks/`.

Exit criterion: no Operation validator contains an independent switch defining
Core child topology.

### CT2. Make runtime Core-driven

Status: `COMPLETE`

- [x] Replace `current_operation` as the runtime dispatch key with a Core Term
  code reference.
- [x] Convert APP, Match, Return, Thunk, Force, OperationRequest, and
  ComputationFold stepping to Core Term tags and Core child APIs.
- [x] Retain optional occurrence identity only for source trace and residual
  verification lookup.
- [x] Replace runtime AST-binder lookup with exact occurrence `BindingId`
  lookup while runtime traversal still follows typed occurrences.
- [x] Remove AST binder IDs from semantic runtime environment entries.
- [x] Keep source AST IDs only in trace/provenance records.
- [x] Separate ordinary stepping from computation-fold-result verification.
- [x] Define explicit residual verification annotations consumed at the relevant
  runtime boundary.
- [x] Ensure ordinary runtime evaluation does not require complete
  `prototype_compile_metadata`.
- [x] Preserve resumption multiplicity, handler scope, resource generation, and
  host dispatch behavior.

Primary files:

- `src/prototype/src/graph/typed_occurrence/runtime.inc`;
- `src/prototype/src/graph/typed_occurrence/verification.inc`;
- `src/prototype/include/a_program/graph/verification.h`;
- `src/prototype/include/a_program/core/term.h`; and
- runtime-focused integration tests.

Focused gates:

- [x] `test_cbpv_surface.sh`;
- [x] `test_computation_block_sequence.sh`;
- [x] `test_resumption_multiplicity.sh`;
- [x] higher-order operation fixtures;
- [x] `runtime_strict_value_check.p`;
- [x] `runtime_strict_effects_check.p`; and
- [x] `runtime_strict_dependent_check.p`.

Exit criterion: runtime semantic dispatch contains no
`case PROTOTYPE_OPERATION_*`, and runtime environments do not look up values by
AST binder ID.

### CT3. Introduce TypedOccurrenceGraph and occurrence edges

Status: `COMPLETE`

- [x] Replace `prototype_operation_node` with a typed-occurrence record whose
  fields satisfy Section 4.
- [x] Add the occurrence-edge arena and role validation.
- [x] Preserve existing numeric IDs directly as occurrence IDs.
- [x] Replace direct `function`, `argument`, `body`, and `scrutinee` fields with
  role-indexed occurrence edges.
- [x] Rename source-only names and ascriptions to reference and expected-type
  occurrences and exclude them from runtime dispatch.
- [x] Retain exact occurrence Binding identity independently of Core alpha
  representative selection.
- [x] Convert Match cases to reference Core case ordinals and branch
  occurrences without copying branch Core bodies.
- [x] Convert fold clauses to reference Core clause ordinals and generated
  clause occurrences without copying clause Core bodies.
- [x] Convert IH lookup to one authoritative typed-occurrence side record.
- [x] Add validation that occurrence edges agree with Core child roles and
  Context membership.
- [x] Migrate manually assembled HOTT and audit fixtures from deleted child
  fields and classifier hints to occurrence-edge builders and explicit solver
  input storage.

Primary files:

- `src/prototype/include/a_program/graph/typed_occurrence_model.h`;
- `src/prototype/include/a_program/graph/typed_occurrence_graph.h`;
- `src/prototype/src/graph/typed_occurrence/storage.inc`;
- `src/prototype/src/graph/typed_occurrence/graph_validation.inc`; and
- `src/prototype/include/a_program/graph/compile_metadata.h`.

The final names must be `typed_occurrence`, `occurrence_edge`, and
`typed_occurrence_graph` or equally explicit variants. The word `operation`
must remain reserved for language-level effect operations and OperationRequest.

Exit criterion: TypedOccurrenceGraph contains no second executable syntax
union and no duplicated Core child payload.

### CT4. Migrate lowering, classifier solving, effects, and usage

Status: `COMPLETE`

- [x] Change lowering outputs from Operation references to typed-occurrence
  references.
- [x] Preserve shared Core identity for differently typed source occurrences.
- [x] Move lowering classifier hints into compile-context solver input storage.
- [x] Keep one solution cell per typed occurrence.
- [x] Freeze the solved classifier into the occurrence only during
  finalization.
- [x] Distinguish unresolved solver state from an intentional dependent-CBPV
  runtime verification residual.
- [x] Derive and validate category and computation kind from the frozen classifier where
  possible.
- [x] Represent genuinely source-directed polarity as explicit elaboration
  input, not as an undifferentiated mutable occurrence field.
- [x] Remove `known_classifier` and `classifier_variable` from typed
  occurrences.
- [x] Preserve effect-row and resource-usage solver authority established by
  the T4/T5 review work.
- [x] Replace Operation-specific reachability and dependency traversal with
  occurrence-edge traversal.

Primary files:

- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`;
- `src/prototype/src/frontend/lowering/graph_construction.inc`;
- `src/prototype/src/frontend/lowering/constraint_solver.inc`;
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`;
- `src/prototype/src/graph/occurrence_usage.c`; and
- `src/prototype/src/frontend/universe_collection.c`.

Focused gates:

- [x] shared Core `Bool`/`Nat` identity occurrences;
- [x] List type argument instantiation before Match;
- [x] one-level and multi-level recursive `*rest` execution;
- [x] dependent Pi and dependent Match;
- [x] explicit Index Family Acc fixtures;
- [x] IF8 fuel-free QuickSort; and
- [x] resource-usage checks.

Exit criterion: solver state has one producer and the frozen occurrence is a
read-only result, not a mutable solver workspace.

### CT5. Migrate Judgement, CwF, and Identity references

Status: `COMPLETE`

- [x] Rename `PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION` to the typed-occurrence
  authority.
- [x] Rename Proposition and proof-workspace Operation identity fields to
  occurrence identity fields.
- [x] Keep non-occurrence authorities unchanged.
- [x] Provide one checked projection from a sealed occurrence to its principal
  `HAS_TYPE` Proposition.
- [x] Require accepted principal Claims to equal that projection.
- [x] Preserve explicit conversion, expected-type exposure, weakening,
  substitution reindex, effect weakening, and cumulativity Derivations.
- [x] Preserve multiple accepted Derivations of one Claim.
- [x] Update CwF semantic-action references without merging certificates into
  substitutions.
- [x] Update HOTT/Identity actions to reference occurrences only where typed
  source occurrence identity is semantically required.
- [x] Prove accepted replay performs no synthesis and does not reconstruct a
  different classifier.

Primary files:

- `src/prototype/include/a_program/kernel/judgement/types.h`;
- `src/prototype/src/kernel/typing/`;
- `src/prototype/include/a_program/kernel/cwf_certificate.h`;
- `src/prototype/src/identity/`; and
- HOTT and proof-boundary tests.

Exit criterion: proof evidence refers to typed occurrences, but no occurrence
record contains a Derivation tree or an accepted-proof subset.

### CT6. Separate operational reduction descriptors from type schemas

Status: `COMPLETE`

- [x] Classify the current Core evaluator dependencies on `TypeDeclarationDB`.
  Match exhaustiveness is the remaining ordinary-computation dependency;
  type-family saturation belongs to the pure-type reduction profile.
- [x] Define the minimum immutable reduction descriptor for constructor and
  eliminator execution.
- [x] Include only operational facts such as constructor identity, arity,
  recursive-field coordinates, Match dispatch, and selected transparent
  definition bodies.
- [x] Keep constructor classifiers, Context telescopes, Universe evidence, and
  nominal TypeViews in the static type layer.
- [x] Change Core runtime stepping to receive the operational reduction
  environment rather than the complete type schema.
- [x] Keep pure type normalization as a separate profile that may inspect type
  terms and static declarations under its own contract.
- [x] Confirm that Core WHNF and computation WHNF never branch on a classifier
  or proof.
- [x] Add differential tests comparing old baseline and new reduction results,
  step counts, and completion states.

Primary files:

- `src/prototype/src/core/term/evaluation_and_conversion.inc`;
- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/include/a_program/kernel/type_declaration.h`;
- `src/prototype/src/kernel/type_declaration.c`; and
- runtime construction in drivers.

Exit criterion: ordinary Core computation consumes operational descriptors,
not the static typing database as an undifferentiated dependency.

### CT7. Split compile ownership from graph storage

Status: `COMPLETE`

- [x] Make `prototype_compile_metadata` own one explicit
  `prototype_typed_occurrence_graph` rather than parallel raw arrays plus a
  temporary graph facade.
- [x] Separate solver workspace, resolution trace, diagnostics, export data,
  verification plans, and frozen module graph ownership.
- [x] Remove `prototype_compile_metadata_operation_graph*` view/commit helpers.
- [x] Define one seal/freeze boundary after which typed occurrences, Contexts,
  substitutions, and accepted proofs are immutable.
- [x] Replace in-place incremental thawing with an explicit compile
  transaction: a mutable workspace may import a frozen module snapshot, but
  must publish a new snapshot rather than mutate the old one.
- [x] Ensure a second REPL definition extends the workspace without making a
  previously published module graph mutable.
- [x] Ensure runtime receives only frozen Core code, runtime descriptors, and
  residual verification plans.

Primary files:

- `src/prototype/include/a_program/graph/compile_metadata.h`;
- `src/prototype/src/graph/typed_occurrence/graph_validation.inc`;
- frontend entry points; and
- driver compile/run entry points.

Exit criterion: ownership is explicit and no count is copied back and forth
between compile metadata and a graph facade.

### CT8. Replace artifact v74 with v75

Status: `COMPLETE`

- [x] Write `artifact_v75.schema` before changing wire code.
- [x] Rename wire operation authority and sections to typed occurrences.
- [x] Store one Core Term reference, one Context reference, classifier closure
  status, either its solved classifier or residual-verification obligation,
  occurrence edges, and required side-table data.
- [x] Do not store solver hints, solution worklists, runtime environments,
  normalization caches, source-only wrapper tags, or duplicate Core children.
- [x] Update closure marking to trace occurrence edges and proof references.
- [x] Update dense publication and relocation for occurrence IDs directly.
- [x] Replace `wire_v74.c/.h` with v75 implementation and archive only the v74
  schema document.
- [x] Reject v74 artifacts at the version boundary without fallback parsing.
- [x] Update schema fingerprints, README references, fixtures, and artifact
  determinism tests together.

Primary files:

- `src/prototype/spec/artifact_v75.schema`;
- `src/prototype/include/a_program/artifact/interface.h`;
- `src/prototype/include/a_program/artifact/wire_v75.h`;
- `src/prototype/src/artifact/wire_v75.c`;
- `src/prototype/src/artifact/publication/`; and
- `src/prototype/tests/integration/test_artifact_flow.sh`.

Exit criterion: v75 is the only accepted artifact version and contains no
Operation syntax graph independent of TermDB.

### CT9. Delete legacy structures, audit boundaries, and measure the result

Status: `COMPLETE`

- [x] Audit typed-occurrence kinds. Keep only kinds that select a static typing
  rule or preserve source provenance; do not require a one-for-one Core tag
  mirror and never use these kinds for runtime dispatch.
- [x] Delete direct Operation child fields and stale classifier phase fields.
- [x] Delete OperationGraph runtime dispatch and AST-binder runtime lookup.
- [x] Delete old operation graph facade APIs and v74 wire code.
- [x] Rename remaining files and APIs from OperationGraph to
  TypedOccurrenceGraph where they describe typed occurrences.
- [x] Keep language-level effect-operation and OperationRequest terminology.
- [x] Run searches proving no permanent compatibility/remap vocabulary remains.
- [x] Run focused, full integration, public-header, source-manifest, and schema
  consistency tests.
- [x] Record per-file added/deleted/net line counts.
- [x] Record total implementation and test line-count changes.
- [x] Record build and full-suite runtime before and after the migration.

Exit criterion: all target invariants are executable checks, the legacy model
is absent, and the final report explains every material code-size increase or
decrease.

## 8. Test Matrix

### 8.1 Required commands

```sh
make clean
make
make -f src/prototype/Makefile test-type-infer-and-check
make -f src/prototype/Makefile test-integration
```

### 8.2 Required focused boundaries

| Boundary | Required coverage |
| --- | --- |
| shared Core, distinct typing | `test_shared_core_occurrences.sh` and Bool/Nat identity fixture |
| Core CBPV execution | `test_cbpv_surface.sh`, `test_cbpv_boundary.sh` |
| sequencing and handler fold | computation-block and effect-handler fixtures |
| runtime Binding identity | nested Lambda, Match binders, handler resumptions |
| dependent runtime verification | strict dependent CBPV fixture |
| Match/IH occurrence identity | recursive IH identity and outer IH insertion sort |
| indexed families | explicit Acc surface/eliminator fixtures |
| well-founded recursion | IF8 fuel-free QuickSort |
| proof boundary | P0 certificate boundary and HOTT goal checks |
| Context/Substitution | Context category, conversion scope, resource usage |
| artifact | read, append, link, republish, deterministic publication |
| repository boundary | public headers, source manifest, spec consistency |

### 8.3 Structural assertions

The final tests or audit script must assert:

- runtime code does not dispatch on typed-occurrence tags;
- runtime semantic bindings do not contain AST binder IDs;
- TypedOccurrence records do not contain Core child fields;
- frozen occurrences do not contain solver variables or lowering hints;
- principal occurrence Claims equal the checked occurrence projection;
- Context and substitution IDs remain valid after dense publication;
- multiple typed occurrences may share one Core Term;
- multiple Derivations of one Claim remain representable; and
- v74 input is rejected without entering graph readback.

## 9. Risks and Stop Conditions

### 9.1 Alpha-shared binder identity

A Core representative may use canonical Binding IDs that differ from source
occurrence Binding IDs. Do not remove occurrence Binding identity until runtime,
proof replay, Match refinement, and source diagnostics have separate tested
consumers. Core execution must use Core Binding IDs; static evidence must use
typed-occurrence/Context Binding IDs.

There are two valid traversal regimes, and they must not be mixed:

1. a Core-only evaluator follows Core child Terms and uses the binders stored
   by those same Core parents; or
2. an occurrence evaluator follows occurrence edges and uses the exact
   occurrence `BindingId` stored by the occurrence parent and side tables.

Using a canonical parent Core binder while following a child occurrence from a
different alpha representative is invalid. The CT2 transition therefore uses
regime 2 without AST binder IDs. Its final Core-only runtime must switch the
parent, child, and binder together to regime 1.

### 9.2 Dependent Match and IH

If an occurrence edge cannot distinguish two branches or IH fields that share a
Core node, stop and extend the occurrence side table. Do not put source Context
identity back into Core canonical keys.

### 9.3 Runtime residual verification

If a dependent fold requires classifier substitution after an effectful result,
retain an explicit verification-plan reference. Do not make ordinary Core
stepping inspect all compile metadata, and do not pretend the residual is an
accepted static Claim.

### 9.4 Proof reconstruction

If deleting an Operation field would require selecting one arbitrary
Derivation, stop. The field is not a duplicate of proof identity. Preserve the
accepted proof DAG and remove only the duplicated principal header projection.

### 9.5 Reduction environment

If a supposed operational descriptor requires a typing proof to choose the next
runtime step, stop and identify the missing compilation/elaboration decision.
Runtime execution may consume a compiled dispatch decision but not perform type
search.

## 10. Implementation Progress

Progress recorded on 2026-08-16 against the working tree based on `977f174`:

Completion update:

- Core child roles are the only executable-topology projection used by typed
  occurrences and runtime.
- Match case owner comparison now observes Core representation shape; nominal
  TypeView selection remains occurrence/classifier evidence.
- Match branch occurrence validation maps all case BindingIds and the explicit
  IH-owner frame, including alpha-shared recursive branches and computation
  blocks.
- The compiler publishes mutable, sealed, and frozen graph states explicitly.
  Accepted proof replay requires a sealed graph and runtime obtains a frozen
  module snapshot rather than mutable compile metadata.
- Artifact v75 is the sole reader/writer format. Version 74 has no parser and
  survives only as an archived schema.
- The explicit Index Family, HOTT, IF8 fuel-free QuickSort, artifact flow,
  resource usage, runtime, and transaction focused gates pass.

The following bullets record the implementation path; superseded interim
limitations are called out where relevant.

- Core exposes checked child count and role-indexed child lookup for all current
  executable constructors.
- Typed-occurrence validation and reachability use that Core vocabulary.
- Typed runtime dispatch reads Core Term tags. Semantic runtime environments no
  longer store or search AST binder IDs.
- Ordinary Core evaluation now has an entry point that requires no compile
  metadata. Trace occurrences and residual verification are supplied through
  an optional `prototype_runtime_annotations` boundary.
- The typed-occurrence and side-table C types have been renamed without adding
  a compatibility alias or a second ID namespace.
- A role-indexed occurrence-edge arena is allocated by compiler drivers and
  HOTT test support.
- Source lowering now materializes each occurrence edge when the occurrence is
  formed. A computation fold is materialized after its clause side table is
  complete. Source compilation no longer performs a whole-graph edge rebuild
  during finalization.
- Direct occurrence child fields have been deleted. APP, Lambda, Match, IH,
  Return, Thunk, Force, request, and fold consumers obtain child occurrences by
  checked role lookup.
- Lowering classifier hints now live in compile-context solver storage;
  `known_classifier` and `classifier_variable` are absent from the occurrence
  record.
- `prototype_compile_metadata` directly owns the typed-occurrence graph. The
  temporary view/commit facade and parallel ownership counters have been
  removed.
- Classifier finalization now distinguishes statically solved occurrences from
  explicit dependent-CBPV residual verification. An unresolved ordinary
  occurrence remains an error; a residual fold records the exact verification
  obligation instead of pretending that solver incompleteness is a classifier.
- Incremental compilation uses an explicit append-only transaction. A frozen
  snapshot fixes its visible prefixes; extension publishes a new frozen
  snapshot and rollback restores the previous sealed/frozen boundary.
- Dense artifact publication copies the occurrence-edge arena together with
  occurrences. Artifact linking offsets both child occurrence IDs and each
  parent edge span; omitting either operation produces an in-range graph whose
  topology refers to the wrong module.
- Edge validation accepts the three intentional Context-action shapes: current
  parent/current child, projected source parent/projected source child, and an
  unchanged parent with a projected child. This is a projection check, not a
  relaxation of Core topology authority.
- Judgement candidate replay and the principal accepted-replay validators now
  consume role-indexed occurrence edges for APP, Lambda, Match, CBPV, request,
  and computation-fold structure.
- Judgement authority and Proposition fields now use typed-occurrence
  terminology. Language-level effect-operation IDs remain operation IDs.
- No whole-graph edge rebuild or v74 migration facade remains.
- Ordinary runtime reduction no longer receives `TypeDeclarationDB`. Core
  `TYPE_FORMER` carries the erased constructor cardinality needed by a neutral
  exhaustive Match, while host Nat/Text conversion receives a compiler-frozen
  `prototype_term_reduction_environment`. Static declarations remain available
  only to pure-type normalization and optional runtime verification.
- Constructor cardinality is operational annotation, not TypeFormer identity.
  Recursive/generated ADTs materialize their own former before all constructors
  exist, so cardinality updates the one representation anchor and is excluded
  from canonical equality and hashing.

Focused checks recorded as passing during this migration:

- `test_term_child_roles.sh`;
- `test_cbpv_surface.sh`;
- `test_computation_block_sequence.sh`; and
- `test_resumption_multiplicity.sh`;
- `test_resource_usage.sh`;
- `test_context_category.sh`;
- `test_artifact_flow.sh`.

The manually assembled HOTT fixtures now use occurrence-edge builders and no
longer initialize deleted child or classifier-hint fields. The HOTT goal and
shared-Term HOTT substrate checks pass after that migration. The final full
suite, including these fixtures, completed successfully.

## 11. Progress Dashboard

| Phase | Status | Commit | Focused tests | Full integration | Notes |
| --- | --- | --- | --- | --- | --- |
| CT0 field/authority inventory | COMPLETE | working tree | PASS | PASS | ownership table and consumers audited |
| CT1 Core structural API | COMPLETE | working tree | PASS | PASS | child-role boundary covered |
| CT2 Core-driven runtime | COMPLETE | working tree | PASS | PASS | runtime uses Core and frozen annotations |
| CT3 TypedOccurrenceGraph | COMPLETE | working tree | PASS | PASS | role edges and alpha frame checks active |
| CT4 lowering and solver | COMPLETE | working tree | PASS | PASS | Index Family and IF8 pass |
| CT5 proof/CwF/Identity | COMPLETE | working tree | PASS | PASS | sealed principal projection audited |
| CT6 reduction descriptors | COMPLETE | working tree | PASS | PASS | ordinary runtime is TypeDB-free |
| CT7 compile ownership | COMPLETE | working tree | PASS | PASS | transaction and frozen snapshot covered |
| CT8 artifact v75 | COMPLETE | working tree | PASS | PASS | v74 rejected without fallback |
| CT9 deletion and final audit | COMPLETE | working tree | PASS | PASS, 454 seconds | structural and measurement audit complete |

### 11.1 Completion record

The final terminology cleanup was followed by a reader build, the type example
suite, and one uninterrupted full integration run. No compatibility alias,
temporary ID remap, v74 reader, or old OperationGraph API remains. This record
describes the reviewed scope prepared for publication on `main`.

## 12. Final Change Report

The following counts are generated against baseline commit `977f174`. Renames
are included in Git's added/deleted counts.

| File | Added lines | Deleted lines | Net lines | Responsibility change |
| --- | ---: | ---: | ---: | --- |
| `core/term` files | 508 | 115 | +393 | checked child-role API and Core-only reduction authority |
| typed-occurrence/runtime files | 1,975 | 581 | +1,394 | role edges, graph states, transactions, frozen runtime snapshot |
| frontend lowering files | 3,517 | 2,009 | +1,508 | edge construction, separated solver state, explicit finalization |
| kernel/Judgement files | 1,231 | 877 | +354 | checked occurrence projections and replay through role edges |
| Identity files | 33 | 33 | 0 | migrated to typed-occurrence graph without semantic expansion |
| artifact files | 536 | 401 | +135 | v75 occurrence wire form and relocation |
| other implementation/build files | 463 | 1,022 | -559 | deleted legacy runtime/graph plumbing and updated manifests |
| tests | 1,076 | 418 | +658 | permanent authority, transaction, artifact, and regression gates |
| documentation/specification | 1,503 | 53 | +1,450 | normative plan, v75 schema, and current README vocabulary |
| total | 10,842 | 5,509 | +5,333 | 114 changed paths |

Line-count measurements use C/header/include/parser sources under
`src/prototype/src` and `src/prototype/include`, with tests measured separately:

- implementation/public-header LOC: 122,083 before, 125,308 after (+3,225);
- test LOC: 17,335 before, 17,973 after (+638);
- fourteen legacy `PROTOTYPE_OPERATION_*` tag symbols were removed; all fourteen
  surviving semantic selectors are now explicitly typed-occurrence kinds and
  are not runtime dispatch tags;
- eleven duplicate or stale fields were deleted: five direct node-child fields,
  one Match body field, three fold-clause topology fields, and two solver-state
  fields; and
- the remaining typed-occurrence record has 33 fields: 4 rule selectors,
  2 Context/action fields, 2 Core/source projections, 5 frozen-classifier
  fields, 5 source/binding provenance fields, and 15 occurrence-side edge,
  Match/IH, fold, and effect-row fields.

Timing on the same workspace, rounded to whole seconds:

| Gate | Baseline `977f174` | Final |
| --- | ---: | ---: |
| clean compiler and reader build | 6 s, PASS | 6 s, PASS |
| type inference/check examples | 4 s, PASS | 5 s, PASS |
| full integration | stopped after 8 s, FAIL | 454 s, PASS |

The baseline full-suite duration is not comparable: it failed in
`test_artifact_flow.sh` at `import resolution must not expose a
TypeCodeShapeKey lookup path`. The final 454-second result is therefore a new
complete-suite measurement, not an asserted runtime regression from eight
seconds.

The exact per-path line report follows in Section 12.1. It can be regenerated
with:

```sh
git diff --numstat 977f174 -- README.md src/prototype \
  doc/2026-08-16T02-36-41-CORE-TERM-TYPED-OCCURRENCE-SEPARATION-PLAN.md
```

### 12.1 Per-path line report

<details>
<summary>114 changed paths</summary>

| Path | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `README.md` | 16 | 15 | +1 |
| `doc/2026-08-16T02-36-41-CORE-TERM-TYPED-OCCURRENCE-SEPARATION-PLAN.md` | 1218 | 0 | +1218 |
| `src/prototype/README.md` | 35 | 37 | -2 |
| `src/prototype/build/sources.mk` | 3 | 3 | +0 |
| `src/prototype/calculus.h` | 2 | 2 | +0 |
| `src/prototype/include/a_program/artifact/interface.h` | 5 | 5 | +0 |
| `src/prototype/include/a_program/artifact/{wire_v74.h => wire_v75.h}` | 3 | 3 | +0 |
| `src/prototype/include/a_program/core/term.h` | 76 | 1 | +75 |
| `src/prototype/include/a_program/frontend/universe_collection.h` | 3 | 3 | +0 |
| `src/prototype/include/a_program/graph/compile_diagnostic.h` | 1 | 1 | +0 |
| `src/prototype/include/a_program/graph/compile_metadata.h` | 39 | 23 | +16 |
| `src/prototype/include/a_program/graph/occurrence_usage.h` | 41 | 0 | +41 |
| `src/prototype/include/a_program/graph/operation_graph.h` | 0 | 85 | -85 |
| `src/prototype/include/a_program/graph/operation_runtime.h` | 0 | 37 | -37 |
| `src/prototype/include/a_program/graph/operation_usage.h` | 0 | 41 | -41 |
| `src/prototype/include/a_program/graph/runtime.h` | 51 | 0 | +51 |
| `src/prototype/include/a_program/graph/typed_occurrence_graph.h` | 134 | 0 | +134 |
| `src/prototype/include/a_program/graph/{operation_model.h => typed_occurrence_model.h}` | 73 | 54 | +19 |
| `src/prototype/include/a_program/graph/verification.h` | 19 | 29 | -10 |
| `src/prototype/include/a_program/kernel/judgement/classifier_solver.h` | 2 | 2 | +0 |
| `src/prototype/include/a_program/kernel/judgement/db.h` | 20 | 16 | +4 |
| `src/prototype/include/a_program/kernel/judgement/rules.h` | 20 | 20 | +0 |
| `src/prototype/include/a_program/kernel/judgement/types.h` | 14 | 14 | +0 |
| `src/prototype/include/a_program/kernel/kernel_view.h` | 3 | 3 | +0 |
| `src/prototype/include/a_program/kernel/type_declaration.h` | 8 | 0 | +8 |
| `src/prototype/spec/archive/README.md` | 1 | 1 | +0 |
| `src/prototype/spec/{ => archive}/artifact_v74.schema` | 0 | 0 | +0 |
| `src/prototype/spec/artifact_v75.schema` | 233 | 0 | +233 |
| `src/prototype/src/artifact/artifact_graph_internal.h` | 1 | 1 | +0 |
| `src/prototype/src/artifact/artifact_internal.h` | 4 | 4 | +0 |
| `src/prototype/src/artifact/interface.c` | 19 | 19 | +0 |
| `src/prototype/src/artifact/link.c` | 151 | 75 | +76 |
| `src/prototype/src/artifact/publication/closure_marking_and_slices.inc` | 22 | 30 | -8 |
| `src/prototype/src/artifact/publication/dense_publication.inc` | 76 | 69 | +7 |
| `src/prototype/src/artifact/publication/section_writers.inc` | 1 | 1 | +0 |
| `src/prototype/src/artifact/publication/wire_primitives.inc` | 2 | 2 | +0 |
| `src/prototype/src/artifact/publication/writer.inc` | 95 | 78 | +17 |
| `src/prototype/src/artifact/relocation.c` | 3 | 4 | -1 |
| `src/prototype/src/artifact/{wire_v74.c => wire_v75.c}` | 154 | 110 | +44 |
| `src/prototype/src/core/term/canonicalization.inc` | 6 | 2 | +4 |
| `src/prototype/src/core/term/declarations.inc` | 284 | 0 | +284 |
| `src/prototype/src/core/term/evaluation_and_conversion.inc` | 103 | 109 | -6 |
| `src/prototype/src/core/term/storage_and_formation.inc` | 26 | 3 | +23 |
| `src/prototype/src/core/term/substitution.inc` | 13 | 0 | +13 |
| `src/prototype/src/driver/compiler_session.c` | 25 | 7 | +18 |
| `src/prototype/src/driver/diagnostics.c` | 2 | 2 | +0 |
| `src/prototype/src/driver/read_file.c` | 252 | 262 | -10 |
| `src/prototype/src/driver/repl.c` | 40 | 22 | +18 |
| `src/prototype/src/frontend/lowering/constraint_solver.inc` | 1720 | 1155 | +565 |
| `src/prototype/src/frontend/lowering/context_and_type_lowering.inc` | 377 | 145 | +232 |
| `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` | 404 | 182 | +222 |
| `src/prototype/src/frontend/lowering/graph_construction.inc` | 970 | 503 | +467 |
| `src/prototype/src/frontend/universe_collection.c` | 18 | 14 | +4 |
| `src/prototype/src/graph/compile_metadata.c` | 56 | 20 | +36 |
| `src/prototype/src/graph/{operation_usage.c => occurrence_usage.c}` | 223 | 83 | +140 |
| `src/prototype/src/graph/operation/graph_validation.inc` | 0 | 367 | -367 |
| `src/prototype/src/graph/operation/storage.inc` | 0 | 293 | -293 |
| `src/prototype/src/graph/operation_graph.c` | 0 | 6 | -6 |
| `src/prototype/src/graph/typed_occurrence/graph_validation.inc` | 492 | 0 | +492 |
| `src/prototype/src/graph/{operation => typed_occurrence}/runtime.inc` | 445 | 268 | +177 |
| `src/prototype/src/graph/typed_occurrence/storage.inc` | 555 | 0 | +555 |
| `src/prototype/src/graph/{operation => typed_occurrence}/verification.inc` | 6 | 7 | -1 |
| `src/prototype/src/graph/typed_occurrence_graph.c` | 6 | 0 | +6 |
| `src/prototype/src/identity/action_certificate_validation.inc` | 6 | 6 | +0 |
| `src/prototype/src/identity/action_execution.inc` | 5 | 5 | +0 |
| `src/prototype/src/identity/context_bridge.inc` | 2 | 2 | +0 |
| `src/prototype/src/identity/object_term_action.inc` | 1 | 1 | +0 |
| `src/prototype/src/kernel/cwf_certificate.c` | 2 | 2 | +0 |
| `src/prototype/src/kernel/judgement.c` | 2 | 2 | +0 |
| `src/prototype/src/kernel/kernel_view.c` | 3 | 3 | +0 |
| `src/prototype/src/kernel/rules/cbpv.inc` | 156 | 97 | +59 |
| `src/prototype/src/kernel/rules/elimination_app.inc` | 10 | 10 | +0 |
| `src/prototype/src/kernel/rules/formation_early.inc` | 8 | 8 | +0 |
| `src/prototype/src/kernel/rules/formation_recording.inc` | 14 | 14 | +0 |
| `src/prototype/src/kernel/rules/introduction/relation_witness.inc` | 24 | 24 | +0 |
| `src/prototype/src/kernel/rules/introduction/structural.inc` | 23 | 23 | +0 |
| `src/prototype/src/kernel/rules/match/expansion_rule_emission.inc` | 72 | 35 | +37 |
| `src/prototype/src/kernel/rules/match/motive_rule_emission.inc` | 17 | 17 | +0 |
| `src/prototype/src/kernel/type_declaration.c` | 59 | 0 | +59 |
| `src/prototype/src/kernel/typing/accepted_replay.inc` | 511 | 367 | +144 |
| `src/prototype/src/kernel/typing/candidate_publication.inc` | 56 | 56 | +0 |
| `src/prototype/src/kernel/typing/candidate_replay.inc` | 158 | 115 | +43 |
| `src/prototype/src/kernel/typing/classifier_solver.inc` | 7 | 7 | +0 |
| `src/prototype/src/kernel/typing/conversion.inc` | 17 | 21 | -4 |
| `src/prototype/src/kernel/typing/judgement_db.inc` | 23 | 19 | +4 |
| `src/prototype/src/parametricity/relation_action.inc` | 19 | 19 | +0 |
| `src/prototype/tests/checks/cbpv_boundary_check.c` | 1 | 1 | +0 |
| `src/prototype/tests/checks/context_category_check.c` | 68 | 45 | +23 |
| `src/prototype/tests/checks/hott/adt_identity.inc` | 16 | 14 | +2 |
| `src/prototype/tests/checks/hott/artifact_roots.inc` | 1 | 1 | +0 |
| `src/prototype/tests/checks/hott/forgery.inc` | 2 | 2 | +0 |
| `src/prototype/tests/checks/hott/higher_identity.inc` | 4 | 4 | +0 |
| `src/prototype/tests/checks/hott/pi_identity.inc` | 16 | 13 | +3 |
| `src/prototype/tests/checks/hott/test_support.inc` | 204 | 152 | +52 |
| `src/prototype/tests/checks/hott/universe_scaffold.inc` | 73 | 42 | +31 |
| `src/prototype/tests/checks/resource_usage_check.c` | 1 | 1 | +0 |
| `src/prototype/tests/checks/spec_enum_check.c` | 25 | 24 | +1 |
| `src/prototype/tests/checks/term_child_role_check.c` | 399 | 0 | +399 |
| `src/prototype/tests/checks/type_inspection_projection_check.c` | 11 | 9 | +2 |
| `src/prototype/tests/checks/typed_occurrence_transaction_check.c` | 125 | 0 | +125 |
| `src/prototype/tests/checks/whnf_profile_cache_check.c` | 3 | 3 | +0 |
| `src/prototype/tests/integration/test_artifact_flow.sh` | 52 | 52 | +0 |
| `src/prototype/tests/integration/test_cbpv_boundary.sh` | 3 | 3 | +0 |
| `src/prototype/tests/integration/test_cbpv_surface.sh` | 10 | 10 | +0 |
| `src/prototype/tests/integration/test_computation_block_sequence.sh` | 2 | 2 | +0 |
| `src/prototype/tests/integration/test_constructor_polarity.sh` | 5 | 5 | +0 |
| `src/prototype/tests/integration/test_definition_block.sh` | 3 | 3 | +0 |
| `src/prototype/tests/integration/test_explicit_index_family_surface.sh` | 3 | 3 | +0 |
| `src/prototype/tests/integration/test_recursive_ih_identity.sh` | 21 | 23 | -2 |
| `src/prototype/tests/integration/test_shared_core_occurrences.sh` | 4 | 4 | +0 |
| `src/prototype/tests/integration/test_spec_consistency.sh` | 1 | 1 | +0 |
| `src/prototype/tests/integration/test_term_child_roles.sh` | 11 | 0 | +11 |
| `src/prototype/tests/integration/test_type_inspection.sh` | 1 | 1 | +0 |
| `src/prototype/tests/integration/test_typed_occurrence_transaction.sh` | 11 | 0 | +11 |

</details>

Code reduction is desirable but not the primary acceptance criterion. The
primary criterion is one semantic authority per fact with checked projections
between Core computation, typed occurrence, static proof, runtime state, and
wire representation.

## 13. Completion Definition

This plan is complete only when all of the following hold:

1. TermDB is the sole computational-topology authority.
2. TypedOccurrenceGraph contains Context and typing occurrence information but
   no second executable syntax graph.
3. Runtime evaluation is Core-driven and Binding-ID-driven.
4. Solver state is not stored in frozen occurrences.
5. Principal typing headers are checked occurrence projections while explicit
   proof distinctions remain in JudgementDB.
6. Core runtime stepping does not inspect classifiers, Contexts, or proofs.
7. Artifact v75 persists the new authority split without compatibility layers.
8. Existing CBPV, effects, dependent Match/IH, Index Family, Acc, IF8, resource,
   HOTT, and artifact tests pass.
9. Legacy OperationGraph syntax and runtime dispatch are deleted.
10. The final per-file and total line-count report is recorded in this document.
11. Core Term records and Core reduction APIs contain no classifier, static
    Context, Claim, Derivation, or typed-occurrence dependency.
12. Static Contexts and runtime environments remain distinct checked layers;
    neither is used as a compatibility representation for the other.
