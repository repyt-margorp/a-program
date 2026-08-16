# Semantic Store Authority and Physical Consolidation Plan

Date: 2026-08-16

Status: complete

Repository baseline:

- branch: `main`;
- commit: `71b04ee`;
- implementation root: `src/prototype/`;
- active artifact format: v76; and
- implementation and public-header size: 125,308 lines.

This plan follows the completed Core Term and Typed Occurrence separation. It
does not merge semantic stores merely to reduce their number. It removes
duplicate mutable authority, separates authoritative schema from derived and
presentation data, and shares physical storage machinery only after the
semantic boundaries are explicit.

Related documents:

- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T17-05-44-POST-SC1-PHYSICAL-COMPACTION-AUDIT-AND-PLAN.md`;
- `2026-08-09T17-13-30-V3-PC1-PERSISTENT-PROPOSITION-REFERENCE-NORMALIZATION-PLAN.md`;
- `2026-08-11T08-19-24-CURRENT-COMPILER-FUNCTIONAL-AND-THEORY-REVIEW.md`;
- `2026-08-16T02-36-41-CORE-TERM-TYPED-OCCURRENCE-SEPARATION-PLAN.md`; and
- `src/prototype/spec/artifact_v76.schema`.

## 1. Objective

Establish one mutable authority for each semantic fact while preserving the
stores that are required by the theory and compiler boundary.

The following stores remain semantically distinct:

```text
TermDB
    context-free Core computational topology and canonical Term identity

TypedOccurrenceGraph
    source and elaboration occurrences of Core Terms under static Contexts

ContextDB / SubstitutionDB
    Context objects and explicit Context morphisms

TypeDeclarationDB
    generative IADT nominal schema and constructor telescope authority

JudgementDB
    accepted Propositions, Claims, and Derivations used for replay

ConstraintDB
    mutable elaboration equations, dependencies, states, and residuals
```

The refactor must eliminate these current forms of duplication:

1. classifier and effect constraints represented and mutated in more than one
   owner;
2. semantic type schema, readback data, materialized classifiers, and intern
   caches stored in one TypeDeclarationDB record family;
3. candidate and accepted proof management copying complete Proposition data
   between representations; and
4. repeated arena, interning, transaction, and program-storage plumbing across
   otherwise distinct semantic stores.

## 2. Non-Goals

This migration does not:

- merge TermDB with TypedOccurrenceGraph;
- reconstruct Context from a Core Term;
- merge ContextDB with SubstitutionDB;
- erase TypeDeclarationDB or encode generative IADT schema only as ordinary
  Terms;
- merge Proposition, Claim, and Derivation into one semantic record;
- merge classifier, effect-row, Universe, and resource-usage theories into one
  untyped solver;
- encode APP, Match, induction, or ComputationFold proof rules through one
  generic eliminator validator;
- add proof irrelevance or select one canonical Derivation per Claim;
- preserve compatibility aliases, dual-write paths, remap layers, or readers
  for an obsolete artifact version; or
- change the surface language, reduction rules, accepted programs, or runtime
  effect semantics.

Reducing line count is a secondary result. The primary success criterion is
that every mutable semantic fact has one identifiable owner.

## 3. Current-State Findings

### 3.1 Classifier constraint ownership

`operation_classifier_solver` currently owns occurrence solutions, classifier
constraints, constraint results, motive candidates, dependencies, and a
worklist in:

```text
src/prototype/src/frontend/lowering/context_and_type_lowering.inc
```

Judgement processing also owns computation constraints with occurrence,
Context, classifier, computation, continuation, residual-effect, solved-state,
and projected-classifier fields in:

```text
src/prototype/include/a_program/kernel/judgement/types.h
src/prototype/include/a_program/kernel/judgement/db.h
```

These are not always the same mathematical constraint. An elaboration
classifier equation and a kernel-rule obligation may legitimately be separate.
The defect is that their relationship is expressed through copied solution
fields and implicit phase ordering rather than an explicit constraint or
projection edge.

### 3.2 Effect constraint ownership

Effect rows currently have three mutable representations:

1. `prototype_judgement_effect_row_constraint` in Judgement delta state;
2. `prototype_occurrence_effect_constraint` in compile metadata; and
3. `operation_effect_solver` meta and state arrays in the compiler workspace.

Constraint creation writes an equation into compile metadata and initializes
solver state elsewhere. Solving updates the separate state and later copies it
back into metadata. The API therefore does not enforce which representation is
authoritative.

This is a genuine duplicate-authority defect, not merely a diagnostic cache.

### 3.3 TypeDeclarationDB responsibility mixture

The current declaration and constructor records combine:

- nominal declaration identity;
- parameter and index Contexts;
- constructor field Context and result classifier;
- source/readback type-expression spans;
- `curried_classifier_cache`;
- structural representation identities;
- representation fingerprints and intern indexes; and
- generated Identity/IADT validation logic.

The constructor registration API receives semantic schema, readback metadata,
and a derived curried classifier in one operation. This makes it possible for a
consumer to accidentally treat readback or a stale materialization as semantic
authority.

The structural `representation_id` is not itself disposable: Core Type Former
Terms use it to identify a structural representation. Fingerprint tables,
dirty flags, and materialized classifier lookup are rebuildable caches and must
not have the same ownership status.

### 3.4 Judgement management duplication

Proposition, Claim, and Derivation have different meanings and remain separate:

```text
Proposition
    the immutable judgement statement

Claim
    acceptance of one Proposition, including closure information

Derivation
    one rule application proving a Claim from ordered premises
```

The large rule-replay implementation is not, by itself, evidence that these
three concepts should be merged. APP, Match, induction, and ComputationFold
validate different rules and must remain visible in the kernel.

There is nevertheless physical duplication:

- a derivation candidate stores a conclusion Proposition ID and copied
  conclusion fields;
- candidate premises embed complete Proposition records;
- accepted replay reconstructs a candidate-shaped record by copying the
  accepted conclusion and premise Propositions; and
- publication wrappers repeat initialization, append, rollback, and intern
  bookkeeping.

The correction is a common immutable rule-application view, not a combined
Proposition/Claim/Derivation record.

### 3.5 Repeated physical storage machinery

Term, Context, Substitution, TypeDeclaration, Judgement, Universe, AST, and
compiler-workspace code repeatedly implement variants of:

- count/capacity and checked growth;
- append-only publication;
- hash heads, hash-next arrays, and intern statistics;
- transaction marks and rollback;
- frozen-prefix or seal state;
- dense artifact closure and relocation; and
- repeated initialization of a complete compiler program state.

The repeated machinery can be shared, but semantic IDs and typed accessors must
not be replaced with an untyped mega-arena.

## 4. Target Ownership Model

### 4.1 Persistent semantic stores

```text
TermDB
TypedOccurrenceGraph
ContextDB
SubstitutionDB
TypeDeclarationDB       authoritative nominal and telescope schema only
TypeRepresentationDB    persistent structural representation identities
JudgementDB              accepted Proposition/Claim/Derivation DAG
UniverseDB               accepted level facts and constraints required by artifacts
```

### 4.2 Compiler-only mutable workspaces

```text
ASTDB
ConstraintDB
DerivationCandidateDB
HOTT/Identity action queues and candidates
diagnostic trace state
```

### 4.3 Rebuildable caches

```text
normalization caches
type-representation fingerprint indexes
curried-constructor-classifier materialization
reindex lookup caches
hash indexes and intern statistics
```

### 4.4 Readback and presentation data

```text
TypeReadbackDB
source type expressions
source names and locations
diagnostic rendering metadata
```

Readback data may refer to semantic IDs. Semantic validation must never depend
on reconstructing schema from readback data.

## 5. Normative Invariants

### 5.1 Constraint authority

- Every pending or solved elaboration constraint has exactly one
  `ConstraintId` in `ConstraintDB`.
- A constraint record owns its lifecycle state and optional result/evidence.
- Compile metadata does not own mutable solver state.
- A frozen typed occurrence contains only a final checked projection or one
  residual-obligation reference.
- Judgement rules may emit typed constraint nodes but do not maintain a second
  mutable copy of the same equation.
- Artifact publication stores accepted evidence and explicit residual
  obligations, not solver queues, mutable states, or solved temporary nodes.

### 5.2 Typed constraint separation

The common constraint header contains only generic scheduling information:

```text
ConstraintId
kind
origin Context/occurrence/source
dependency span
state
failure or residual reason
optional evidence/result reference
```

Classifier equations, effect-row equations, computation obligations, Universe
inequalities, and later resource constraints retain typed payloads and
kind-specific validation. Shared storage must not permit reading one payload as
another.

### 5.3 Type schema authority

- TypeDeclarationDB is the sole authority for generative declaration identity,
  parameter/index Contexts, constructor field Contexts, and result classifiers.
- Recursive constructor occurrences continue to use the declaration's `*`
  family/index mechanism; readback does not reconstruct recursive schema.
- TypeRepresentationDB owns persistent structural representation identity.
- TypeReadbackDB is never queried by conversion, classifier synthesis, or
  reduction.
- A curried constructor classifier is derived from the field Context and result
  classifier through one checked memoized query.
- Cache invalidation is revision-based and cannot alter the semantic result.

### 5.4 Judgement authority

- A Proposition has one interned immutable statement identity.
- A Claim refers to a Proposition and records accepted closure state.
- Zero or more Derivations may prove one Claim.
- Candidate proof objects refer to Proposition IDs instead of embedding copied
  Proposition records.
- Accepted and candidate replay use the same read-only `RuleApplicationView`.
- Accepted replay performs no synthesis and mutates no ConstraintDB state.
- Rule-specific validators remain explicit and independently testable.

### 5.5 Physical storage boundary

- Shared arena helpers manage bytes, extents, allocation failure, and rollback;
  typed stores retain typed IDs and accessors.
- One `prototype_program_storage` owns the backing storage for one compiler
  session and initializes semantic stores once.
- `prototype_program` remains a semantic view over those stores rather than a
  generic object database.
- No persistent ID is inferred from a raw pointer or array address.
- No compatibility forwarding API remains after each migration phase.

## 6. Target Constraint Model

The intended dependency structure is:

```text
source occurrence facts
          |
          v
classifier/effect constraints -----> kernel computation obligations
          |                                      |
          +--------------+-----------------------+
                         v
                 solved result/evidence
                         |
             +-----------+-----------+
             v                       v
     frozen occurrence        residual obligation
```

`ConstraintDB` is not persisted wholesale. Publication traces only the solved
evidence selected by accepted Claims and residual obligations required at a
later verification boundary.

An explicit edge connects an elaboration classifier constraint to any kernel
computation obligation it generates. Copying the projected classifier into both
records is forbidden.

## 7. Target Type-Declaration Model

```text
TypeDeclarationDB
    declaration identity
    namespace/name and origin
    parameter Context
    index Context
    formation classifier
    constructor schema IDs

ConstructorSchema
    owner declaration and ordinal
    field telescope Context
    result classifier

TypeRepresentationDB
    persistent structural representation ID
    representative schema reference

TypeRepresentationCache
    fingerprints
    intern buckets
    dirty/revision state

TypeReadbackDB
    source type expressions
    parameter/field display records
    diagnostic and interface names

ConstructorClassifierCache
    schema revision
    derived curried classifier Term
```

Generated Identity and IADT validation consumes the same semantic schema API as
source declarations. Its rule-specific implementation moves out of the storage
module.

## 8. Target Judgement Management Model

The validator-facing view is:

```text
RuleApplicationView
    proof kind
    conclusion Proposition reference
    ordered premise Proposition references
    rule payload reference
    semantic action reference
```

A candidate adapter resolves candidate Proposition IDs. An accepted adapter
resolves Claim IDs to their Proposition IDs. Neither adapter copies complete
Proposition records.

The view does not erase the persistent distinction between:

- a candidate not yet accepted;
- a Claim accepted into the closure DAG; and
- one of several Derivations of that Claim.

## 9. Implementation Phases

### DA0. Freeze the authority inventory

Status: complete

- [x] Enumerate every classifier, effect-row, computation, Universe, and usage
  constraint structure and its writers/readers.
- [x] Classify each field as semantic input, mutable solver state, checked
  result, residual obligation, diagnostic trace, or cache.
- [x] Enumerate TypeDeclarationDB fields and consumers under schema,
  representation, readback, and cache ownership.
- [x] Enumerate candidate and accepted proof records and every full-Proposition
  copy path.
- [x] Record baseline per-file and subsystem line counts.
- [x] Record full-suite commands and runtime at commit `71b04ee`.
- [x] Add compile-time comments stating the current temporary authority before
  moving any field.

Exit condition:

- every migrated field has one documented current owner and one target owner;
- no implementation change has yet altered accepted behavior.

### DA1. Introduce one ConstraintDB

Status: complete

- [x] Add typed `ConstraintId`, common lifecycle state, origin, dependency span,
  result/evidence reference, and residual reason.
- [x] Add kind-specific payload storage for classifier equations, effect-row
  equations, and computation obligations.
- [x] Move dependency edges and worklist membership to ConstraintDB.
- [x] Preserve separate classifier and effect solving functions over typed
  payloads.
- [x] Add checked APIs for creating dependencies between elaboration and kernel
  obligations.
- [x] Add state-transition validation: pending to solved, residual, incomplete,
  or rejected; terminal states cannot silently return to pending.
- [x] Add transaction marks so failed speculative elaboration removes all
  associated constraints atomically.
- [x] Keep the existing external behavior while tests compare old and new
  solver projections during this phase only.

Temporary comparison instrumentation must be deleted at DA2 completion. It is
not a compatibility layer.

### DA2. Remove duplicate classifier and effect authority

Status: complete

- [x] Replace `operation_effect_solver.constraint_states` and metadata-owned
  mutable states with ConstraintDB state.
- [x] Replace compile-metadata effect equations with Constraint IDs or immutable
  diagnostic snapshots.
- [x] Convert Judgement effect-row constraint generation to emit or reference
  ConstraintDB nodes.
- [x] Link computation obligations to their classifier/effect premises by ID.
- [x] Remove copied solved-classifier fields from intermediate constraint
  records where the result can be resolved through a checked edge.
- [x] Freeze one final classifier/effect projection into each solved typed
  occurrence.
- [x] Freeze one residual-obligation ID instead when static solving stops.
- [x] Ensure diagnostic rendering can read a stable snapshot without becoming
  solver authority.
- [x] Delete old dual-write, copy-back, and comparison instrumentation paths.

Required boundary tests:

- [x] one classifier equation used by multiple dependent constraints;
- [x] one effect union shared by nested APP and ComputationFold occurrences;
- [x] residual dependent-CBPV verification;
- [x] rejected effect inclusion does not leave a solved metadata copy;
- [x] speculative constraint rollback removes dependency edges and results; and
- [x] artifact closure excludes solved compiler-only constraint work state.

### DA3. Split TypeDeclarationDB responsibilities

Status: complete

- [x] Retain `prototype_type_declaration_db` as authoritative nominal and
  constructor schema storage only.
- [x] Introduce explicit TypeRepresentationDB storage for persistent structural
  representation identities.
- [x] Move fingerprint buckets, intern links, dirty flags, and revision state to
  a rebuildable representation-cache module.
- [x] Move source/readback type expressions and field/result display metadata to
  TypeReadbackDB.
- [x] Replace stored authoritative `curried_classifier_cache` fields with one
  memoized query derived from field Context and result classifier.
- [x] Add a validator proving a cached classifier equals fresh schema
  materialization before artifact publication.
- [x] Move generated Identity/IADT validation out of the declaration storage
  implementation and onto the public semantic schema API.
- [x] Audit conversion, synthesis, reduction, HOTT, artifact, and linker code to
  prove none reconstruct semantic schema from readback records.
- [x] Delete the old all-in-one constructor registration API.

Artifact decision:

- semantic schema and persistent representation identity are serialized;
- readback is serialized only when required by interface or diagnostics;
- caches are reconstructed or serialized only as checked acceleration data;
- cache presence never changes acceptance.

Required boundary tests:

- [x] `List A` parameter schema;
- [x] Sigma-like dependent constructor field telescope;
- [x] explicit Index Family `Acc`;
- [x] generated Identity declaration;
- [x] two nominal TypeViews sharing one structural representation;
- [x] corrupted readback cannot change semantic acceptance; and
- [x] deleting all rebuildable caches preserves classifier results.

### DA4. Normalize Judgement proof management

Status: complete

- [x] Add the immutable `RuleApplicationView` and typed premise iterator.
- [x] Convert candidate records to refer to conclusion and premise Proposition
  IDs instead of copying complete Proposition records.
- [x] Add candidate and accepted adapters for the common view.
- [x] Change validators to consume the view without changing rule-specific
  validation logic.
- [x] Remove accepted-to-candidate reconstruction and copied conclusion tuples.
- [x] Consolidate candidate builder initialization, append, rollback, and intern
  transaction code.
- [x] Preserve multiple Derivations per Claim and ordered premise identity.
- [x] Preserve Claim closure rank and artifact-root behavior.
- [x] Prove accepted replay performs no synthesis, normalization search, or
  mutable constraint solving.
- [x] Delete obsolete publication wrapper variants after all callers migrate.

The following validators remain separate even if they share view plumbing:

- [x] APP elimination;
- [x] Match elimination;
- [x] induction-hypothesis elimination;
- [x] ComputationFold elimination;
- [x] conversion and weakening; and
- [x] observation/Identity rules.

Required boundary tests:

- [x] two Derivations for one Claim remain distinguishable;
- [x] premise order changes the derivation identity where the rule is ordered;
- [x] candidate rollback leaves no accepted Claim or intern entry;
- [x] artifact replay validates without constructing candidate records; and
- [x] malformed premise references fail before rule-specific field access.

### DA5. Extract shared physical storage infrastructure

Status: complete

- [x] Introduce a small checked arena-extent and growth utility used through
  typed store wrappers.
- [x] Introduce shared append-only transaction marks and rollback helpers.
- [x] Introduce a reusable intern-index helper where key/hash semantics are
  supplied by each typed store.
- [x] Introduce `prototype_program_storage` as the owner of backing arrays for
  one compiler session.
- [x] Keep `prototype_program` as a semantic composition view.
- [x] Replace repeated compiler-session initialization and destruction paths.
- [x] Share artifact closure/relocation traversal mechanics only where a typed
  descriptor preserves each store's validation rules.
- [x] Keep TermId, ContextId, SubstitutionId, TypeDeclarationId, PropositionId,
  ClaimId, and DerivationId as distinct C types and namespaces.
- [x] Reject an untyped `void *` mega-arena or one global object-tag namespace.
- [x] Remove old per-store helpers only after their typed callers use the common
  mechanism.

DA5 deliberately follows DA1 through DA4. Physical infrastructure must be
extracted from the corrected ownership model, not from the current accidental
duplication.

### DA6. Artifact migration, deletion audit, and measurement

Status: complete

- [x] Determine the exact persistent schema after DA1 through DA5.
- [x] Write `artifact_v76.schema` before changing wire code.
- [x] Serialize residual obligations without compiler worklist or mutable state.
- [x] Serialize Type schema, representation identity, and Judgement DAG through
  their new ownership boundaries.
- [x] Update dense closure, relocation, linker validation, and schema
  fingerprints.
- [x] Reject v75 without a fallback reader once v76 becomes active.
- [x] Delete temporary field aliases, dual-write paths, adapters used only for
  migration, and obsolete v75 wire implementation.
- [x] Run focused, full integration, public-header, source-manifest, artifact,
  and deterministic replay tests.
- [x] Record per-file additions, deletions, and net line changes.
- [x] Record subsystem totals for constraints, type declarations, Judgement
  management, artifact code, tests, and documentation.
- [x] Record before/after clean build and full-suite runtime.
- [x] Update README and prototype documentation to v76 and the final ownership
  model.

## 10. Test Strategy

### 10.1 Differential behavior

Before deleting the previous paths, run the old and new implementations over
the same checked fixtures and compare:

- frozen Core Term ID;
- typed occurrence Context and classifier;
- solved effect row and residual status;
- accepted Proposition and Claim identity;
- ordered Derivation premises;
- normalized output under every existing reduction profile; and
- published artifact closure.

Differential comparison code is temporary migration instrumentation and must be
removed before completion.

### 10.2 Existing semantic boundaries

The suite must continue to cover:

- same Core lambda under Bool and Nat typed occurrences;
- generic `List Nat` instantiation and Match;
- one-level and multi-level recursive `*rest` execution;
- dependent Pi and dependent Match;
- explicit Index Family `Acc`;
- IF8 equality/transport and fuel-free QuickSort;
- effect-row union, residual, forwarding, and handling;
- artifact publication, relocation, linking, and replay;
- non-DefEq object Identity evidence; and
- resource-usage validation.

### 10.3 Structural assertions

Add audit tests or assertions proving:

- one mutable constraint state exists per Constraint ID;
- no readback field is read by semantic type validation;
- no candidate embeds a complete copied Proposition;
- accepted replay allocates no candidate;
- no cache field is required for semantic acceptance;
- every frozen occurrence has either a solved projection or residual reference;
- no artifact contains worklist, speculative candidate, or mutable cache state;
  and
- no deprecated compatibility/remap symbol remains.

## 11. Risks and Stop Conditions

### 11.1 Constraint theories are not interchangeable

Stop if the shared ConstraintDB requires casts between classifier, effect-row,
Universe, or usage payloads. Correct the typed payload boundary before
continuing.

### 11.2 Residual verification loses provenance

Stop if replacing metadata constraints prevents a residual obligation from
identifying its originating occurrence, Context, dependencies, and expected
result. Add explicit references; do not restore copied mutable records.

### 11.3 Type representation becomes cache-dependent

Stop if removing a fingerprint or curried-classifier cache changes declaration
identity, conversion, constructor typing, or artifact acceptance. Persistent
representation identity and rebuildable lookup cache have been confused.

### 11.4 Judgement rules become hidden

Stop if `RuleApplicationView` turns rule validation into a generic switch that
cannot state each rule's premises and conclusion explicitly. Share transport
and storage plumbing only.

### 11.5 Physical consolidation erases typed identities

Stop if shared arena work permits a Term ID to be passed as a Context ID or
requires one global object-kind tag. Restore typed wrappers before migration.

### 11.6 Artifact replay starts solving

Stop if v76 replay needs a worklist, proof search, classifier synthesis, or
cache-dependent reconstruction. Artifacts must contain accepted evidence or an
explicit residual verification contract.

## 12. Progress Dashboard

| Phase | Status | Main result |
| --- | --- | --- |
| DA0 | complete | Complete authority and field-ownership inventory |
| DA1 | complete | One typed ConstraintDB and dependency graph |
| DA2 | complete | Frozen occurrence results and versioned residual obligations |
| DA3 | complete | Type schema, representation, readback, and cache separated |
| DA4 | complete | Proposition-ID-based common Judgement rule-application view |
| DA5 | complete | Shared checked storage primitives and compiler-session owner |
| DA6 | complete | Artifact v76, deletion audit, full suite, and metrics |

Current overall status: 7 of 7 phases complete.

When implementation begins, update each phase status immediately after its exit
condition is met. Do not mark all phases complete only at the end.

## 13. Required Change Report

The final report must include:

```text
baseline commit
final commit
artifact version

per-file:
    added lines
    deleted lines
    net lines

per-subsystem:
    constraint code
    type declaration/schema code
    Judgement management code
    common storage code
    artifact code
    tests
    documentation

totals:
    implementation additions
    implementation deletions
    implementation net
    test additions/deletions/net
    documentation additions/deletions/net
    clean build runtime before/after
    full suite runtime before/after
```

Line-count reduction is desirable but is not an acceptance condition. A larger
result is acceptable only when the report identifies the new invariant or test
coverage responsible for the increase.

## 14. Completion Definition

This plan is complete only when all of the following hold:

- ConstraintDB is the sole mutable owner of classifier/effect elaboration
  constraints and their lifecycle state;
- typed occurrences contain only checked final projections or residual
  references;
- TypeDeclarationDB contains authoritative schema rather than readback and
  mutable cache state;
- persistent structural representation identity is separated from rebuildable
  representation indexes;
- candidate and accepted proof validation share a copy-free view while
  Proposition, Claim, and Derivation remain semantically distinct;
- shared arena/storage machinery does not erase typed IDs or theory-specific
  validators;
- artifact v76 replays accepted evidence without synthesis;
- v75 compatibility and all temporary migration paths have been deleted;
- focused and complete regression suites pass; and
- the final per-file and subsystem change report is recorded in this document.

## 15. Implementation Record

### 15.1 DA0 baseline

DA0 was completed against commit `71b04ee` before semantic implementation
changes.

Build and suite baseline:

```text
clean build command:
    make -f src/prototype/Makefile clean all reader
clean build wall time:
    5.741 seconds

full suite command:
    make -f src/prototype/Makefile test-integration
full suite wall time:
    446.796 seconds
result:
    passed
```

The IF8 fuel-free QuickSort test is the dominant full-suite interval. Later
measurements must retain it rather than comparing a smaller suite.

Relevant baseline line counts:

| Path or subsystem | Lines |
| --- | ---: |
| implementation and public headers | 125,308 |
| frontend lowering subsystem | 24,394 |
| `context_and_type_lowering.inc` | 4,374 |
| `constraint_solver.inc` | 9,612 |
| TypeDeclaration public header and implementation | 4,010 |
| kernel typing `.inc` subsystem | 15,885 |
| `judgement_db.inc` | 620 |
| `candidate_publication.inc` | 1,400 |
| `candidate_replay.inc` | 1,022 |
| `accepted_replay.inc` | 6,791 |
| artifact implementation and public headers | 15,545 |
| `wire_v75.c` | 3,500 |
| `driver/read_file.c` | 5,030 |

### 15.2 DA0 authority inventory

| Fact or data | Current mutable/persistent owner | Current copies or projections | Target owner |
| --- | --- | --- | --- |
| occurrence classifier equation | `operation_classifier_solver.constraints` | Judgement computation constraints and solved occurrence projection | ConstraintDB classifier payload |
| classifier lifecycle/result | `operation_classifier_solver.constraint_results` | solver solution and frozen occurrence classifier | ConstraintDB state/result, then one frozen projection |
| occurrence effect equation | compile metadata `effect_constraints` | v75 artifact and dense publication | ConstraintDB effect payload |
| occurrence effect lifecycle | `operation_effect_solver.constraint_states` | copied into metadata constraint and occurrence solution | ConstraintDB state/result |
| effect-row metavariable solution | `operation_effect_solver.metas/solution_atoms` | materialized Term row and substituted classifiers | ConstraintDB effect-meta result until final materialization |
| kernel CBPV computation obligation | JudgementDelta `computation_constraints` | occurrence solver projected classifier/effect rows | ConstraintDB computation payload with explicit dependencies |
| kernel effect helper equation | JudgementDelta `effect_row_constraints` | imported by structural matching into occurrence equations | ConstraintDB typed dependency or local helper payload |
| Universe inequality | UniverseDB | collected frontend references | UniverseDB semantic payload using shared lifecycle infrastructure only after DA5 |
| resource usage result | occurrence usage solver storage | Proposition resource usage and frozen occurrence solution | existing typed usage theory; generic lifecycle may be shared later |
| residual runtime verification | VerificationDB | v75 artifact | VerificationDB, referenced from terminal ConstraintDB state |

The Universe and resource systems are not folded into classifier/effect
payloads during DA1. Their theories and persistent evidence remain distinct.

### 15.3 DA0 TypeDeclaration ownership inventory

| Field family | Current owner | Classification | DA3 target |
| --- | --- | --- | --- |
| declaration name, namespace, origin, parameter/index Context | TypeDeclarationDB declaration | semantic schema | TypeDeclarationDB schema |
| constructor owner, ordinal, field Context, result classifier | TypeDeclarationDB constructor | semantic schema | TypeDeclarationDB schema |
| `representation_id` | declaration/Term Type Former | persistent structural identity | TypeRepresentationDB |
| representation representative | TypeDeclarationDB representation | persistent structural identity | TypeRepresentationDB |
| representation fingerprint/index/dirty flag | TypeDeclarationDB | rebuildable lookup cache | TypeRepresentationCache |
| parameter and field source expressions | TypeDeclarationDB | readback/presentation | TypeReadbackDB |
| constructor readback result expression | TypeDeclarationDB | readback/presentation | TypeReadbackDB |
| `curried_classifier_cache` | constructor and artifact export | derived semantic materialization | revision-checked ConstructorClassifierCache |
| generated Identity validation functions | `type_declaration.c` | rule-specific semantic consumer | Identity module over schema API |

### 15.4 DA0 Judgement ownership inventory

| Record | Required semantic role | Physical duplication to remove in DA4 |
| --- | --- | --- |
| Proposition | interned immutable judgement statement | candidate premise embeds complete record |
| Claim | accepted Proposition membership and closure rank | none; Claim ID remains |
| Derivation candidate | solver-local rule application | repeats conclusion tuple beside Proposition ID |
| Accepted Derivation | persistent proof DAG edge | accepted replay reconstructs candidate-shaped copies |
| Premise edge | ordered accepted/scoped dependency | candidate and accepted forms need one read-only iterator |

The 6,791-line accepted replay implementation is mostly rule validation. DA4
does not treat its size as evidence that the rules should be encoded into one
generic validator. It removes copied transport records and repeated publication
transactions while retaining rule visibility.

### 15.5 DA1 and DA2 implementation

`operation_constraint_db` is now the single mutable elaboration owner for
classifier equations, effect-row equations, computation obligations,
dependencies, lifecycle state, and solver-local results. Typed occurrences
receive a classifier/effect projection only after the owning constraints reach
a terminal checked state.

The remaining `prototype_occurrence_effect_constraint` records in compile
metadata are immutable diagnostic snapshots. They do not carry solver state,
are not read as semantic authority, and are not published in v76. An unresolved
effect equation is instead frozen as a versioned
`PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION` with its occurrence,
left row, right row, and expected result row. Compiler budgets, worklist state,
and solved compiler-only constraints do not cross the artifact boundary.

### 15.6 DA3 implementation

`prototype_type_declaration_db` remains a composition view for physically
adjacent session storage, but its authority is split into explicit nested
stores:

- declaration and constructor records contain nominal semantic schema;
- `prototype_type_readback_db` contains source reconstruction and diagnostics;
- `prototype_type_representation_db` contains persistent structural identity
  and rebuildable lookup state; and
- `prototype_constructor_classifier_cache` contains revision-checked derived
  materializations.

Constructor classifiers are freshly derivable from the parameter/field Context
path and result classifier. Cache validation compares that derivation with the
stored revision before publication. Generated Identity schema validation is an
independent `identity/generated_schema_validation.c` semantic consumer rather
than declaration-storage implementation.

### 15.7 DA4 implementation

Candidate derivations now identify conclusions and ordered premises through
store-qualified Proposition IDs. The pointer fields exposed to validators are
checked, read-only resolutions and are not identity or persistent authority.
Candidate and accepted replay both expose
`prototype_judgement_rule_application_view`; the individual APP, Match, IH,
ComputationFold, conversion, weakening, and Identity validators remain
separate.

Accepted replay traverses the immutable Claim/Derivation DAG and never creates
a candidate or invokes mutable elaboration solving. Candidate rollback restores
both Proposition and resource-usage extents and rebuilds the intern index.
During final integration, IF8 found an uninitialized resource-usage pointer in
the newly separated transient Proposition array. Initializing the complete
array fixed the lifetime bug and the IF8 fixture remains the permanent boundary
test for this path.

### 15.8 DA5 implementation

`support/storage.c` supplies checked extent reservation, bounded capacity
growth, two-extent append transactions, and intern-index primitives. Typed
stores retain their own IDs, records, hashes, equality, and validation rules.
There is no untyped global object arena.

`prototype_program_storage` owns the typed backing arrays for one ordinary
compiler/REPL session while `prototype_program` remains the borrowed semantic
composition view. `driver/read_file.c` deliberately retains specialized fixture
storage because one process simultaneously owns several provider, import, and
artifact sessions; forcing those independent lifetimes through one single-
session owner would obscure rather than consolidate ownership.

### 15.9 DA6 artifact decision

Artifact v76 removes mutable effect-constraint sections and all normalization
and solver budget/counter fields from `compile_policy`. It publishes accepted
schema, representation identity, Judgement evidence, checked occurrence
projections, and versioned residual verification obligations. The v75 reader
and wire implementation were deleted, and a v75 header is explicitly rejected
without fallback parsing.

### 15.10 Final change report

```text
baseline commit: 71b04ee
final commit: commit containing this implementation record
artifact version: 76

subsystem                         added  deleted   net
artifact                            444      534   -90
common storage/session              461      357  +104
constraints/typed occurrences      1328      522  +806
Judgement management               1425     1146  +279
type declaration/schema            1414     1212  +202
other implementation                145       65   +80
tests                               345       77  +268
documentation                        1066       21 +1045

implementation total               5217     3836 +1381
tests total                          345       77  +268
documentation total                  1066       21 +1045
all files total                      6628     3934 +2694

clean build baseline: 5.741 seconds
clean build final:    6.114 seconds
full suite baseline: 446.796 seconds
full suite final:     462.733 seconds
```

Per-file additions, deletions, and net change follow. Renamed artifact files
are measured as renames by Git.

```text
    +9     -9     +0  README.md
 +1043     -0  +1043  doc/2026-08-16T14-43-28-SEMANTIC-STORE-AUTHORITY-AND-PHYSICAL-CONSOLIDATION-PLAN.md
   +13    -11     +2  src/prototype/README.md
   +15     -6     +9  src/prototype/build/sources.mk
    +2     -2     +0  src/prototype/calculus.h
    +1     -1     +0  src/prototype/include/a_program/artifact/interface.h
    +2     -2     +0  src/prototype/include/a_program/artifact/{wire_v75.h => wire_v76.h}
   +19     -0    +19  src/prototype/include/a_program/driver/compiler_session.h
    +7     -7     +0  src/prototype/include/a_program/graph/verification.h
   +12    -12     +0  src/prototype/include/a_program/kernel/judgement/classifier_solver.h
   +46    -27    +19  src/prototype/include/a_program/kernel/judgement/types.h
   +83    -33    +50  src/prototype/include/a_program/kernel/type_declaration.h
   +50     -0    +50  src/prototype/include/a_program/support/storage.h
    +1     -1     +0  src/prototype/spec/archive/README.md
   +22    -11    +11  src/prototype/spec/{artifact_v75.schema => artifact_v76.schema}
   +11    -11     +0  src/prototype/src/artifact/interface.c
  +134   -118    +16  src/prototype/src/artifact/link.c
   +22    -29     -7  src/prototype/src/artifact/publication/closure_marking_and_slices.inc
  +124   -116     +8  src/prototype/src/artifact/publication/dense_publication.inc
   +49    -40     +9  src/prototype/src/artifact/publication/section_writers.inc
    +1     -1     +0  src/prototype/src/artifact/publication/wire_primitives.inc
    +2    -30    -28  src/prototype/src/artifact/publication/writer.inc
   +21    -38    -17  src/prototype/src/artifact/relocation.c
   +77   -148    -71  src/prototype/src/artifact/{wire_v75.c => wire_v76.c}
    +2     -3     -1  src/prototype/src/core/term/declarations.inc
    +1     -1     +0  src/prototype/src/core/term/storage_and_formation.inc
   +27    -52    -25  src/prototype/src/driver/compiler_session.c
    +3     -3     +0  src/prototype/src/driver/diagnostics.c
  +204     -0   +204  src/prototype/src/driver/program_storage.c
   +67    -16    +51  src/prototype/src/driver/read_file.c
   +67   -305   -238  src/prototype/src/driver/repl.c
  +988   -312   +676  src/prototype/src/frontend/lowering/constraint_solver.inc
  +110    -42    +68  src/prototype/src/frontend/lowering/context_and_type_lowering.inc
   +23    -10    +13  src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc
  +187   -158    +29  src/prototype/src/frontend/lowering/graph_construction.inc
    +1     -1     +0  src/prototype/src/frontend/universe_collection.c
   +20     -0    +20  src/prototype/src/graph/typed_occurrence/verification.inc
 +1104     -0  +1104  src/prototype/src/identity/generated_schema_validation.c
   +39     -5    +34  src/prototype/src/identity/identity_computation.inc
    +3     -3     +0  src/prototype/src/internal/ast_common.h
   +20     -9    +11  src/prototype/src/kernel/context.c
  +121    -66    +55  src/prototype/src/kernel/rules/cbpv.inc
   +36    -27     +9  src/prototype/src/kernel/rules/formation_early.inc
   +14    -13     +1  src/prototype/src/kernel/rules/formation_recording.inc
  +112   -104     +8  src/prototype/src/kernel/rules/introduction/relation_witness.inc
   +39    -36     +3  src/prototype/src/kernel/rules/introduction/structural.inc
   +28    -26     +2  src/prototype/src/kernel/rules/match/expansion_rule_emission.inc
  +188  -1174   -986  src/prototype/src/kernel/type_declaration.c
  +537   -434   +103  src/prototype/src/kernel/typing/accepted_replay.inc
  +308    -83   +225  src/prototype/src/kernel/typing/candidate_publication.inc
   +89    -65    +24  src/prototype/src/kernel/typing/candidate_replay.inc
   +40   -219   -179  src/prototype/src/kernel/typing/classifier_solver.inc
   +23    -23     +0  src/prototype/src/kernel/typing/conversion.inc
   +20    -11     +9  src/prototype/src/kernel/typing/judgement_db.inc
    +2     -3     -1  src/prototype/src/kernel/universe.c
   +94     -0    +94  src/prototype/src/support/storage.c
    +8     -4     +4  src/prototype/tests/checks/cbpv_boundary_check.c
   +12     -0    +12  src/prototype/tests/checks/context_category_check.c
    +7     -1     +6  src/prototype/tests/checks/conversion_result_check.c
    +7     -1     +6  src/prototype/tests/checks/conversion_scope_check.c
   +64    -30    +34  src/prototype/tests/checks/core_view_representation_check.c
    +2     -1     +1  src/prototype/tests/checks/hott/adt_identity.inc
   +53    -18    +35  src/prototype/tests/checks/hott/test_support.inc
    +6     -0     +6  src/prototype/tests/checks/lifted_ih_runtime_check.c
    +6     -0     +6  src/prototype/tests/checks/shared_term_reindex_check.c
    +1     -1     +0  src/prototype/tests/checks/spec_enum_check.c
   +39     -0    +39  src/prototype/tests/checks/storage_infrastructure_check.c
    +7     -1     +6  src/prototype/tests/checks/universe_defeq_check.c
    +7     -1     +6  src/prototype/tests/checks/whnf_profile_cache_check.c
   +20    -12     +8  src/prototype/tests/integration/test_artifact_flow.sh
    +4     -1     +3  src/prototype/tests/integration/test_cbpv_boundary.sh
    +5     -2     +3  src/prototype/tests/integration/test_cbpv_surface.sh
    +2     -2     +0  src/prototype/tests/integration/test_computation_block_sequence.sh
   +65     -0    +65  src/prototype/tests/integration/test_constraint_authority.sh
    +1     -1     +0  src/prototype/tests/integration/test_definition_block.sh
    +1     -1     +0  src/prototype/tests/integration/test_spec_consistency.sh
   +28     -0    +28  src/prototype/tests/integration/test_storage_infrastructure.sh
```
