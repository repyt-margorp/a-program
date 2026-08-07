# Typed HOTT Goals V2-S1 Implementation Plan

Date: 2026-08-07

Status: complete

Baseline commit: `7780fdd` (`main`, synchronized with `origin/main`)

Artifact format at baseline: `A_PROGRAM_ARTIFACT 61`

Parent plan:
`doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Normative input:
`doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`

Preconditions completed before this plan:

- V2-C1: ContextDB and SubstitutionDB ownership extraction;
- V2-T1/T2: shared-TermDB HOTT and dependent-CBPV fragment freeze;
- V2-C2: direct binding-object graph action;
- V2-B1: proof assumptions use direct binding-object identity.

## 1. Objective

V2-S1 introduces the typed goal substrate required before A Program can
construct Higher Observational Type Theory witnesses.

The phase must separate four concepts that the current compiler either mixes
or does not represent:

1. classifier metavariable unification;
2. kernel conversion requests;
3. HOTT object-construction goals;
4. persistent residual obligations.

The immediate result is not an equality type, a witness term, or an
observational reducer. The result is a typed, context-indexed and replayable
goal boundary on which V2-P1 and V2-O1 can safely build.

The required ordering remains:

```text
V2-B1 -> V2-S1 -> V2-P1 -> V2-O1 -> V2-A1
```

## 2. Codebase Trace and Restated Problem

### 2.1 Current compilation path

The current classifier path is concentrated in `src/prototype/ast.c`:

```text
AST lowering
  -> OperationGraph occurrence nodes
  -> operation_solver_generate_constraints
  -> operation_solver_index_constraints
  -> worklist propagation and Match motive solving
  -> operation_solver_materialize_judgements
  -> JudgementDelta validation
  -> JudgementDB commit
```

Computation and effect constraints then follow partially separate paths:

```text
OperationGraph
  -> JudgementDelta computation constraints
  -> computation constraint solver

OperationGraph
  -> prototype_operation_effect_constraint records
  -> effect-row solver
```

Artifact v61 serializes solved OperationGraph data, effect constraints,
aggregate solver counters, and the existing computation-fold verification
obligations. It does not serialize classifier solver goals.

### 2.2 Positional classifier constraint payload

`struct operation_classifier_constraint` currently contains:

```c
uint32_t target;
uint32_t left;
uint32_t right;
uint32_t aux;
```

The meaning of these fields changes with `kind`:

| Current kind | `target` | `left` | `right` | `aux` |
| --- | --- | --- | --- | --- |
| `HAS_TYPE` | result operation | unused/child | unused/child | unused/child |
| `EQUAL` | result operation | referenced operation | invalid | flag-like zero |
| `CONVERTIBLE` | ascription operation | body operation | expected TermDB classifier | zero |
| `PI_EXPECTED` | result operation | body/function operation | domain argument or classifier | Lambda/App flag |
| `MOTIVE_EQUATION` | Match operation | branch body operation | case index | scrutinee operation |
| `IH_EXPECTED` | IH operation | recursive argument operation | case-related value | zero |
| `CBPV_BOUNDARY` | Return/Thunk/Force operation | child slot | child operation | body slot |
| fold/request result | result operation | input child | second child | third child |

This is not merely a naming issue. Dependency indexing and solving must repeat
the same positional interpretation in independent switches. Adding HOTT
endpoints to this tuple would make an operation ID, TermDB ID, case index,
boolean role, Context ID, and substitution ID indistinguishable at the C type
level.

### 2.3 Context is absent from classifier goals

The record stores `source_operation` and `source_ast`, but not `context_id`.
Consumers recover context indirectly from the current OperationGraph node.

That is insufficient for HOTT because a construction goal is a judgement in a
specific Context, and its endpoint substitutions have domains and codomains
that must be checked against that Context. A shared erased core term or a
relocated operation ID is not a substitute for a CwF object.

### 2.4 Solver state is an approximation, not rule evidence

`operation_solver_refresh_constraint_states` currently marks most classifier
constraints solved when the target operation has any classifier binding. The
`CONVERTIBLE` case checks whether its left operation has a classifier. It does
not retain the conversion result, normalization profile, budget, reason, or
the exact validation rule that discharged the constraint.

Consequences:

- `SOLVED` does not identify which equation was checked;
- `RESIDUAL` is inferred by matching a verification obligation's operation;
- two constraints targeting one operation can inherit the same state even
  when they require different premises;
- a future HOTT witness cannot cite this state as evidence.

### 2.5 Match motive equations are duplicated outside the generic worklist

Match typing currently uses all of the following:

- `OPERATION_CONSTRAINT_MOTIVE_EQUATION`;
- `struct operation_motive_equation` in a separate arena;
- motive constant candidates;
- symbolic motive applications for IH;
- materialized motive terms.

The separate equation record carries semantic fields such as constructor
owner, constructor ordinal, case index, and IH scope. The generic constraint
contains positional IDs for the same branch relation. This creates two records
whose agreement is maintained procedurally rather than structurally.

V2-S1 must place the equation's semantic payload in one tagged classifier-goal
record. Symbolic motive expressions and solved motive terms may remain
solver-local expression state; they are not object equality witnesses.

### 2.6 Conversion is structured but not submitted as a goal

Term conversion already returns `struct prototype_term_conversion_result`,
including:

- status and reason;
- normalization profile;
- left and right endpoints and observations;
- step limit and steps used;
- graph revision.

However, `prototype_judgement_classifier_conversion` always selects
`PURE_TYPE_WHNF` and `UINT64_MAX`. Most callers immediately test only whether
the status is `EQUAL`. The current compiler therefore has a structured result
but no context-indexed conversion request carrying the caller's deterministic
budget or optional carrier.

This must be corrected before HOTT construction. Conversion can be a checked
premise of `OBS_DIAGONAL` or `OBS_CONVERT`; it is not itself an object witness.

### 2.7 Purity has the required trichotomy

`prototype_term_effect_row_purity` already distinguishes:

```text
PURE
EFFECTFUL
UNRESOLVED
```

V2-S1 must use this API. It must not use `classifier_view.effects == 0` or a
failed `prototype_term_effect_row_closed_bits` call as evidence of purity.

### 2.8 Existing VerificationDB is not a HOTT residual database

`struct prototype_verification_obligation` currently describes one runtime
condition: a computation-fold result family. Its payload contains operation
IDs, a continuation binder, input classifier, classifier family, effect row,
normalization profile, and schema version.

It does not contain the HOTT residual contract:

- Context and carrier;
- left and right endpoints;
- bridge Context;
- endpoint substitutions;
- deterministic step limit and steps used;
- conversion/residual reason;
- source provenance and calculus fingerprint.

It also has runtime discharge behavior. A HOTT residual is an unclosed static
construction obligation. Reusing the current record would conflate runtime
conditional verification with object-theory construction.

### 2.9 No HOTT object syntax exists yet

There is no Eq/Obs TermDB tag, HOTT witness proof kind, or surface equality
syntax. This is correct for V2-S1. The absence means S1 can establish the goal
schema and solver boundaries without prematurely choosing the V2-O1 object
representation.

## 3. Design Decisions

### 3.1 One shared TermDB remains mandatory

V2-S1 adds no value-side or computation-side copies of Pi, Lambda, APP, Match,
or any other graph constructor. It adds no HOTT-only copy of ordinary terms.

Context, carrier, endpoints, and substitutions are typed indices over the
existing shared TermDB and CwF substrate.

### 3.2 A solver equation is not object equality

The following remain distinct:

```text
classifier metavariable assignment   compiler elaboration fact
kernel conversion result             meta-level checked result
HOTT construction goal               request to construct object evidence
HOTT witness term                     future object-language term
```

Solving the first or second record never inserts a PropEq/HOTT witness and
never expands global DefEq.

### 3.3 Goal IDs are arena identities, not graph identities

Goal IDs identify compiler-local records. They are not TermDB IDs, binding
objects, proof IDs, or stable artifact rule IDs.

Stable observational rule IDs remain deferred to V2-P1. S1 may use local enums
whose names match the frozen semantic rule names, but those numeric values must
not be serialized as a public artifact contract.

### 3.4 Context and substitutions are authoritative

For a bridge goal over `Gamma`, endpoint substitutions must have the CwF shape:

```text
pi0 : GammaR -> Gamma
pi1 : GammaR -> Gamma
```

Therefore validation requires:

```text
left_substitution.source_context  == bridge_context
left_substitution.target_context  == context
right_substitution.source_context == bridge_context
right_substitution.target_context == context
```

No De Bruijn depth, AST binder name, or classifier match may replace these
checks.

### 3.5 S1 does not migrate artifact format

Artifact v61 remains unchanged and byte-stable for programs without existing
runtime verification obligations. HOTT construction goals are compiler-local
in S1.

The persistent HOTT residual structure is implemented and validated as an
in-memory candidate, but serialization is deferred to V2-P1/V2-A1 and artifact
v62. A v61 writer must never silently serialize or discard a live HOTT
residual.

### 3.6 Existing effect and universe solvers remain separate

V2-S1 does not force classifier, effect-row, and universe constraints into one
catch-all arena. It defines typed references and status boundaries between
them. Their algorithms remain owned by their current solvers.

## 4. Target Records

Exact C names may be adjusted during implementation, but field semantics must
not regress to positional tuples.

### 4.1 Classifier unification goal

Compiler-local, owned by the OperationGraph classifier solver:

```text
prototype_classifier_goal
    id
    kind
    state
    reason
    source_operation
    source_ast
    context_id
    classifier_variable
    tagged payload
```

The tagged payload has rule-specific members, for example:

```text
REFERENCE
    referenced_operation

CONVERSION
    body_operation
    expected_classifier

PI_INTRO
    body_operation
    domain_classifier

PI_ELIM
    function_operation
    argument_operation

MOTIVE_CASE
    match_operation
    branch_operation
    scrutinee_operation
    case_index
    constructor_owner
    constructor_index
    ih_scope_id

IH_APPLICATION
    owning_match_operation
    recursive_argument_operation
    ih_scope_id

CBPV_BOUNDARY / FOLD_RESULT / REQUEST_RESULT / CONSTRUCTOR_FORMATION
    explicitly named operation references
```

Every operation reference is range-checked. Every TermDB classifier is checked
against `term_count`. Every Context is checked against ContextDB.

### 4.2 Kernel conversion goal

Compiler-local unless later retained as a V2-P1 proof premise:

```text
prototype_kernel_conversion_goal
    id
    context_id
    carrier_classifier_or_invalid
    left_term
    right_term
    normalization_profile
    step_limit
    result
```

`result` is the existing structured conversion result. The execution API must
call `prototype_term_compare_for_conversion` with the stored profile and budget
rather than route through the unlimited convenience wrapper.

Rules:

- carrier is mandatory for a HOTT-facing conversion goal;
- carrier may be invalid only for ordinary compiler classifier conversion;
- `EQUAL` discharges conversion only;
- `NOT_EQUAL` is a contradiction for a required conversion;
- `RESIDUAL`, `BLOCKED_EFFECT`, and `EXHAUSTED` remain distinct;
- `INVALID` is malformed input, not a residual;
- a conversion result never creates an object witness by itself.

### 4.3 HOTT construction goal

Compiler-local until V2-O1 supplies object constructors:

```text
prototype_hott_goal
    id
    kind
    state
    residual_reason
    source_ast_or_invalid
    context_id
    carrier_classifier
    left_endpoint
    right_endpoint
    bridge_context_id
    left_endpoint_substitution
    right_endpoint_substitution
    parent_goal_id_or_invalid
    local_type_former_rule
    normalization_profile
    step_limit
```

Initial kinds are exactly:

```text
VALUE_OBSERVATION
COMPUTATION_OBSERVATION
TYPE_ACTION
TERM_ACTION
```

Initial states are exactly:

```text
PENDING
SOLVED
RESIDUAL
CONTRADICTION
UNSUPPORTED
```

The parent edge expresses recursively higher structure. There is no dimension
integer and no maximum HOTT level.

S1 validates the record and its boundaries. It does not mark a goal `SOLVED`
without a rule-specific construction result. Before V2-O1, ordinary generated
goals therefore remain `PENDING`, `RESIDUAL`, or `UNSUPPORTED` except in
focused tests using an explicit validated stub result.

### 4.4 Persistent residual candidate

Separate from the runtime `prototype_verification_db`:

```text
prototype_hott_residual_obligation
    obligation_id
    validation_rule_name/local_id
    context_id
    carrier_classifier
    left_endpoint
    right_endpoint
    bridge_context_id
    left_endpoint_substitution
    right_endpoint_substitution
    normalization_profile
    step_limit
    steps_used
    residual_reason
    source_ast_or_resolved_source_span
    calculus_fingerprint_candidate
```

In S1 this record is not serialized. Its validator must nevertheless reject
invalid Context/substitution orientation, invalid terms, unsupported profiles,
unknown reasons, and cyclic parent/residual references.

The final numeric validation rule and calculus fingerprint format are deferred
to V2-P1/V2-A1. S1 must not invent a permanent artifact number for them.

## 5. Implementation Phases

### S1.0 Baseline characterization

- [x] Record the implementation-start commit and confirm artifact version 61.
- [x] Run a warning-free clean build.
- [x] Run all `src/prototype/test_*.sh` scripts.
- [x] Run examples 01-07 and 09.
- [x] Capture deterministic `identity.p` artifact output for byte comparison.
- [x] Record current classifier/effect/residual solver counts for representative
  Bool, Nat, Match, List/IH, CBPV, and computation-fold sources.

Exit criteria:

- the pre-S1 behavior is reproducible;
- no implementation begins from an unexplained failure;
- baseline artifact bytes and solver counts are recorded in the progress log.

### S1.1 Replace positional classifier tuples

- [x] Replace `operation_classifier_constraint` with a semantically named
  classifier-goal record.
- [x] Replace `target/left/right/aux` with a tagged union of named payloads.
- [x] Store `context_id` explicitly at generation time.
- [x] Distinguish `source_operation` from `classifier_variable`.
- [x] Give state transitions a rule-specific reason/result field.
- [x] Rewrite dependency indexing to inspect tagged payloads.
- [x] Rewrite each solver dispatch to inspect the same tagged payload.
- [x] Fold `operation_motive_equation` semantic fields into the `MOTIVE_CASE`
  payload.
- [x] Remove the duplicate motive-equation arena and lookup path.
- [x] Retain symbolic motive applications as solver expressions, not goals.
- [x] Add structural validation over every generated classifier goal.

Compatibility requirement:

- the same existing examples must synthesize the same classifiers;
- this phase changes no TermDB tag, proof kind, artifact field, or accepted
  source syntax.

Exit criteria:

- no classifier goal has generic positional operands;
- each dependency edge is derivable from one tagged record;
- each solved state identifies the rule that validated it;
- Match motive generation has one authoritative equation record.

### S1.2 Add deterministic kernel conversion goals

- [x] Define the kernel conversion goal and its validator.
- [x] Add an execution API accepting context, optional carrier, profile, and
  step limit.
- [x] Preserve every status and reason from
  `prototype_term_conversion_result`.
- [x] Reject host execution and host effects in all admitted profiles.
- [x] Convert the classifier solver's `CONVERSION` payload to submit this goal.
- [x] Convert the future HOTT-facing validation path to require a carrier.
- [x] Do not mechanically replace all existing kernel conversion calls; audit
  and migrate only calls that need replayable status or deterministic budget.
- [x] Add tests for equal, not-equal, neutral residual, blocked effect,
  exhausted budget, and malformed graph results.

Implementation note: the current normalizer does not itself emit
`PROTOTYPE_TERM_CONVERSION_RESIDUAL`; all supported neutral forms are compared
structurally. The HOTT boundary nevertheless tests and preserves a neutral
residual result explicitly, so a future unsupported/opaque normalization rule
cannot collapse it into inequality.

Exit criteria:

- a caller can replay the same conversion request from its record;
- finite step limits are observable and deterministic;
- residual and inequality are never collapsed;
- no conversion result enters TermDB or JudgementDB as an Eq witness.

### S1.3 Add Context-indexed HOTT goal storage

- [x] Define HOTT goal kind, state, residual reason, and local rule enums.
- [x] Add a compiler-local HOTT goal arena/DB with stable IDs within one run.
- [x] Validate Context, carrier, endpoint, bridge, and substitution IDs.
- [x] Validate both endpoint substitutions as `bridge -> context`.
- [x] Reject a missing carrier for all HOTT construction kinds.
- [x] Reject a parent goal from another goal DB.
- [x] Detect self-parent and longer parent cycles.
- [x] Permit recursive higher chains without a numeric dimension bound.
- [x] Store source AST provenance when one exists.
- [x] Add mark/rewind behavior so failed elaboration does not leak goals.
- [x] Add relocation helpers only for real artifact/link relocation needs; do
  not introduce binder remap tables or De Bruijn identities.

No relocation helper was added in S1: the arena is compiler-local and has no
artifact/link relocation use. Parent IDs are constrained to earlier records,
which makes the parent graph acyclic by construction.

Exit criteria:

- a well-formed first-fragment typed goal can be represented exactly;
- malformed endpoint boundaries fail before solving;
- nested higher goals are represented by parent edges, not level numbers;
- the arena remains separate from TermDB and JudgementDB.

### S1.4 Implement purity and admission classification

- [x] Route computation goals through `prototype_term_effect_row_purity`.
- [x] Classify closed empty rows as `PURE`.
- [x] Classify closed nonempty rows as `EFFECTFUL`.
- [x] Classify row variables, forall rows, and unresolved rows as
  `UNRESOLVED`.
- [x] Admit `Comp(empty,A)` only when the first-fragment carrier is admitted.
- [x] Residualize neutral pure computations that do not expose a result within
  budget.
- [x] Residualize effectful and unresolved computation observations.
- [x] Mark operation-request and computation-fold observational equality
  `UNSUPPORTED`/deferred; do not reuse runtime handler semantics.
- [x] Mark host primitive equality beyond DefEq literals deferred.
- [x] Mark TypeView nominal equality rejected/unsupported.
- [x] Mark universe equality, univalence, HITs, quotients, and IADT index action
  deferred according to the frozen matrix.

Exit criteria:

- zero cached effect bits are never interpreted as purity;
- every frozen matrix row has one executable admission result;
- deferred constructs cannot fall through to ordinary DefEq.

### S1.5 Add the residual boundary without changing v61

- [x] Define a HOTT residual obligation separate from VerificationDB.
- [x] Copy all typed indices and conversion budget/result reasons from the
  originating HOTT goal.
- [x] Validate residual records independently of runtime obligations.
- [x] Keep existing computation-fold VerificationDB behavior unchanged.
- [x] Ensure artifact v61 writing fails explicitly if a live HOTT residual is
  accidentally presented for serialization.
- [x] Do not add v61 compatibility fields or encode HOTT data in unused slots.
- [x] Document fields that V2-P1/V2-A1 must assign stable rule IDs and a
  calculus fingerprint before v62 serialization.

The v61 artifact writer cannot accept this compiler-local DB by type. The
`prototype_hott_residual_db_require_artifact_v62` gate fails for every nonempty
DB and is the required handoff check when V2-P1 connects residual storage to
artifact writing.

Exit criteria:

- no HOTT residual is lost or mistaken for a successful proof;
- no runtime verification obligation is mistaken for a HOTT residual;
- artifact v61 schema and representative bytes remain unchanged.

### S1.6 Integrate lifecycle and diagnostics

- [x] Generate classifier goals before solving and validate them once.
- [x] Preserve generated goals across fixed-point restarts without duplicating
  identities.
- [x] Keep solver blueprints semantic rather than copying stale state/results.
- [x] Derive aggregate solver counters from actual goal states.
- [x] Report contradiction, unsupported, residual, and exhaustion separately.
- [x] Include source span diagnostics through `source_ast` lookup.
- [x] Ensure strict compilation rejects unresolved required classifier goals.
- [x] Ensure residual-enabled compilation retains typed residuals rather than
  weakening a judgement.
- [x] Ensure JudgementDB commit occurs only after required classifier goals are
  solved and proof rules validate.

Implementation correction: the old blueprint copied mutable expected
classifiers and became stale between graph-changing lowering passes. S1 removes
that cache. Goals are regenerated deterministically from the current
OperationGraph for each solver invocation; IDs remain stable for an unchanged
graph, while stale results are deliberately not carried into a changed graph.
Within one invocation, state and structured conversion results remain attached
to the semantic goal records.

Exit criteria:

- fixed-point restart does not erase a contradiction or duplicate a goal;
- aggregate counters agree with an explicit scan of goal arenas;
- diagnostics identify both the source occurrence and semantic goal kind.

### S1.7 Focused law and adversarial tests

Add a focused prototype test program and shell driver covering at least:

- [x] classifier goal tagged-payload validation;
- [x] dependency extraction for every classifier goal kind;
- [x] Match motive case ownership, constructor ordinal, and IH scope;
- [x] same core term under different typed occurrence Contexts;
- [x] exact binding identity after Context reindexing;
- [x] conversion with finite budgets and all structured outcomes;
- [x] bridge endpoint substitution orientation;
- [x] invalid endpoint substitution target;
- [x] invalid carrier and endpoint TermDB IDs;
- [x] parent-goal chains and cycle rejection;
- [x] PURE/EFFECTFUL/UNRESOLVED classification;
- [x] operation request, handler fold, host primitive, TypeView, universe, HIT,
  quotient, and IADT deferred/rejected classifications;
- [x] no HOTT goal becomes a JudgementDB Eq conclusion;
- [x] no new value/computation graph tag appears;
- [x] v61 writer refuses live HOTT residual serialization.

### S1.8 Full acceptance

- [x] `make clean all reader` passes without warnings.
- [x] All `src/prototype/test_*.sh` scripts pass.
- [x] Examples 01-07 and 09 pass.
- [x] Artifact flow and append tests pass.
- [x] Representative artifact v61 output is byte-identical to baseline.
- [x] Deterministic repeated compilation produces identical output.
- [x] `git diff --check` passes.
- [x] No generated parser/lexer file is edited manually.
- [x] Parent V2 plan marks V2-S1 complete and V2-P1 as next.

## 6. Expected Code Surface

Implementation remains in `src/prototype/` unless separately approved.

| File | Planned responsibility |
| --- | --- |
| `src/prototype/ast.c` | Replace positional classifier constraints, integrate typed goal lifecycle, preserve fixed-point behavior |
| `src/prototype/ast.h` | Expose only compile metadata needed for diagnostics/counters; do not expose private classifier payloads unnecessarily |
| `src/prototype/judgement.h` | Declare conversion and HOTT goal contracts without adding a Judgement Eq kind |
| `src/prototype/typing.c` | Validate/run deterministic conversion goals and typed HOTT goal boundaries |
| `src/prototype/context.h` / `context.c` | Reuse Context/Substitution validation; change only if a narrow goal-boundary query is demonstrably missing |
| `src/prototype/term.h` / `term.c` | Reuse conversion result, normalization profile, and purity APIs; no Eq/HOTT TermDB tags in S1 |
| focused `*_check.c` and `test_*.sh` | Typed-goal law, malformed-input, budget, and regression tests |

A new implementation module should be introduced only if it establishes a
clear ownership boundary and its accepted-build integration is separately
approved. S1 must not create several thin helper modules around one solver.

## 7. Invariants to Preserve

1. TermDB stores computation graphs, not runtime environments.
2. OperationGraph stores typed source occurrences, not a second computation
   calculus.
3. ContextDB binding identity is direct and non-positional.
4. Substitution orientation remains `sigma : Delta -> Gamma`.
5. DefEq/kernel conversion remains meta-level and profile-dependent.
6. Solver equations do not become PropEq/HOTT witnesses.
7. JudgementDB retains only `HAS_TYPE` and `IS_TYPE` conclusions.
8. Value/computation polarity remains typed occurrence information over one
   shared graph.
9. Host effects never run in type conversion.
10. Effectful or unresolved dependency is residual/unsupported, never silently
    treated as pure.
11. No numeric HOTT dimension or finite truncation is introduced.
12. Artifact keys, canonical hashes, and TypeView identity do not establish
    object equality.

## 8. Explicit Non-goals

V2-S1 does not implement:

- surface `==` syntax;
- Eq/Obs type formation in TermDB;
- reflexivity, UA, transport, or rewrite terms;
- observational action over Pi, ADT, Match, Return, or Thunk;
- universe observation or univalence;
- quotient or HIT declarations;
- handler/resumption observational semantics;
- IADT index-refining action;
- proof payload v62;
- artifact v62;
- global equality reflection;
- a union-find populated by user equality proofs.

Those belong to V2-P1, V2-O1, or later explicitly frozen phases.

## 9. Risks and Required Responses

### Risk 1: Renaming fields without changing semantics

Merely replacing `left` with `operand` leaves the positional problem intact.
Use a tagged payload and central validation/dependency extraction.

### Risk 2: Treating any classifier binding as goal success

Each goal kind must run its own validation and retain its result/reason before
entering `SOLVED`.

### Risk 3: Turning conversion into object equality

Keep conversion goals outside TermDB and ordinary object witness storage. A
future witness rule may cite a successful conversion as a premise only.

### Risk 4: Reusing VerificationDB

Do not generalize the computation-fold runtime record into a catch-all HOTT
record. The lifecycle and validation semantics differ.

### Risk 5: Serializing unstable local rule enums

Artifact v61 must not serialize S1 HOTT goals. V2-P1 assigns stable rule IDs;
V2-A1 performs the coordinated v62 migration.

### Risk 6: Reintroducing positional binding identity

HOTT goals refer to Contexts and substitutions. They do not store binder depth,
De Bruijn levels, or a parallel binding remap.

### Risk 7: Unlimited conversion hides residual behavior

The HOTT-facing path must always carry an explicit step limit. Convenience
wrappers using `UINT64_MAX` cannot discharge replayable HOTT goals.

### Risk 8: Goal arena becomes a second proof database

Goal state is solver state. Successful object evidence is added only in V2-O1
and validated through the V2-P1 proof schema.

## 10. Completion Criteria

V2-S1 is complete only when:

1. classifier solver records use tagged, context-indexed semantic payloads;
2. Match motive equations have one authoritative solver representation;
3. conversion requests can be replayed with explicit profile and budget;
4. structured conversion outcomes remain distinct;
5. HOTT construction goals carry Context, carrier, endpoints, bridge Context,
   endpoint substitutions, parent goal, profile, and budget;
6. endpoint substitutions are validated by CwF orientation;
7. purity uses the explicit three-way API;
8. HOTT residuals are distinct from runtime VerificationDB records;
9. no object equality syntax, term, witness, or Judgement Eq has been added;
10. artifact v61 remains unchanged;
11. focused adversarial tests and the full regression suite pass;
12. the parent V2 plan names V2-P1 as the next implementation phase.

## 11. Progress Log

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-07 | Codebase audit | complete | Traced AST -> OperationGraph -> classifier/effect/computation solvers -> JudgementDelta -> JudgementDB -> artifact v61. |
| 2026-08-07 | Problem restatement | complete | Identified positional classifier payloads, context-free goals, approximate state accounting, unlimited conversion wrappers, duplicate motive equations, and incompatible runtime verification obligations. |
| 2026-08-07 | S1 design | complete | Selected four separate records and prohibited Eq witnesses, duplicate graph tags, v61 schema changes, and VerificationDB reuse. |
| 2026-08-07 | S1.0 baseline | complete | Baseline `7780fdd`; warning-free build, all 14 prototype scripts, examples 01-07/09, deterministic identity artifact test, and representative v61 checksums/counts recorded below. |
| 2026-08-07 | S1.1 classifier goals | complete | Tagged payloads, explicit Context/source/variable identity, semantic motive-case goals, dependency validation, and solver dispatch are in place. The stale classifier-goal blueprint cache was removed after it retained an obsolete ascription classifier across lowering passes. All 14 prototype scripts and examples 01-07/09 pass. |
| 2026-08-07 | S1.2 conversion goals | complete | Added validated context/profile/budget records, retained structured results, and connected ascription conversion without creating object witnesses. |
| 2026-08-07 | S1.3-S1.5 typed HOTT boundary | complete | Added compiler-local Context-indexed goal and residual arenas, bridge-substitution validation, parent DAGs, purity admission, deferred construct classification, and the explicit v62 residual gate. |
| 2026-08-07 | S1.6 lifecycle | complete | Goal states use rule-specific premise checks and reasons. Graph-changing passes regenerate semantic goals instead of replaying stale positional blueprints. |
| 2026-08-07 | S1.7-S1.8 verification | complete | Warning-free build, all 15 prototype scripts, examples 01-07/09, deterministic recompilation, and artifact v61 byte baselines pass. No TermDB/Judgement Eq tag was introduced. |
### 11.1 S1.0 baseline evidence

Artifact v61 baseline measurements:

| Source | SHA-256 | Solver steps | Constraints | Solved | Residual | Incomplete |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `examples/01_bool.p` | `0c9d128a4be26ce206d6ab7d3ffe9e01af23e883ae5428036a65534277ad660f` | 81 | 20 | 20 | 0 | 0 |
| `examples/02_nat.p` | `bb1844c597edcbae862432a98a1e5ca5dacba49988796b655ac9606bde148e3f` | 3 | 1 | 1 | 0 | 0 |
| `examples/04_match.p` | `e0824e33b82d5c9a84a0a4818db0caf1f873558439934ae4f5891b420d9db1b7` | 168 | 42 | 42 | 0 | 0 |
| `examples/07_add.p` | `395d998b10005ec8c4b6c526bc0d40ddbd7b63342d30ca6e7ae9b36f8588598a` | 230 | 55 | 55 | 0 | 0 |
| `examples/09_list_induction.p` | `bb29ee57b8a07506bb49a265c60a4d96730de666e8a72ad01f8153c68d78c5e4` | 417 | 95 | 95 | 0 | 0 |

All measurements used solver limit 100000 and reported `solver_exhausted=0`.
`test_artifact_flow.sh` independently compiles its generated `identity.p`
twice under the same budget and requires byte-identical artifacts.

## 12. Implementation Start Checklist

Before changing code:

- [x] confirm `main` and `origin/main` still point to the recorded baseline or
  record the newer implementation-start commit;
- [x] confirm the parent and normative documents have not changed the S1
  contract;
- [x] rerun the baseline suite;
- [x] inspect all newly added solver or verification records since this audit;
- [x] update this plan if code movement changes ownership or file paths;
- [x] keep at most one implementation phase marked `in progress` in this log.
