# PR22 Effort Compilation: Critical Review and Implementation Plan

Date: 2026-08-25 JST

Status: implementation complete for the admitted checked-Core and the first
static producer fragment through EC14. The v87 checked container, checked-base
imports, typed effort, resumable classifier, residual-graph normalizer,
persistent merge producer, canonical module-set merge, isolated parallel
checking, stable GoalKeys, and checked-module incremental invalidation are
implemented. Full evaluation-stack persistence, producer-local classifier
serialization, source-file scheduling, and Claim-only HOTT migration remain
explicit follow-up work rather than compatibility shortcuts.

PR: #22, `Document effort compilation and from-scratch artifact checking`

Merged revision: `de52b75943cc3a330969f9ce969b1c86d73cfb1b`

Reviewed implementation revision: `fd546e13784c0d7c59f984beeacdac199ddab321`

Current checked artifact format: v87; v86 remains the temporary migration
oracle for Claim-only HOTT roots

Implementation authority schema:

- `2026-08-25T18-50-06-CHECKED-CORE-AUTHORITY-SCHEMA.md`

Source design:

- `2026-08-25T17-26-30-EFFORT-COMPILATION-PARALLEL-ELABORATION-AND-FROM-SCRATCH-ARTIFACT-CHECKING-DESIGN.md`

Post-implementation structural consolidation:

- `2026-08-26T13-55-23-CODE-STRUCTURE-CONSOLIDATION-REFACTOR-IMPLEMENTATION-PLAN.md`

### 2026-08-27 re-audit

PR22 is already merged and its admitted implementation is present on `main`.
The implementation was re-audited against `fd546e1`, not merely against the
merge commit. Focused checked-Core projection, session, example, v87 container,
work-capsule, and compile-producer tests all pass.

The re-audit keeps the original acceptance decision, with these boundaries:

| Area | Current decision |
| --- | --- |
| Checked semantic projection and independent checking | Adopted; the checker consumes no Claim or Derivation authority |
| v87 checked semantic container | Adopted for the admitted fragment; it does not retire v86 yet |
| Typed effort and resumable production | Adopted; effort controls producer progress, never logical truth |
| Parallelism | Adopted only for isolated immutable inputs and deterministic merge |
| Persistent resume | Fragment merge is implemented; classifier and exact evaluator-stack persistence remain pending |
| HOTT and other Claim-only roots | Not migrated by implication; v86 remains the blocking migration oracle |
| Source scheduling | Not implemented and not to be simulated by sharing mutable compiler arenas |

The implementation also exposed structural duplication that must be removed
before expanding the checked fragment. This is not a rejection of PR22's
semantic boundary. It is follow-up consolidation of its physical realization:

1. freeze and test the exact v87 wire schema;
2. define one Core Term semantic field/reference inventory while keeping
   calculus-specific validation rules explicit;
3. use narrow immutable Term, Context, and Substitution readers;
4. consolidate checked closure and relocation over those readers;
5. establish one canonical checked-module image owner;
6. split the large checker aggregate only after its semantic APIs are stable;
7. retain independent accepted replay and checked-Core acceptance until every
   published root has checked object evidence.

These tasks are CR1 through CR8 of the linked consolidation plan. They replace
neither the EC phases below nor the independent checker. They prevent the same
semantic field inventory from remaining separately encoded in projection,
validation, closure, relocation, and serialization.

## 1. Objective

PR22 proposes three connected changes:

1. compilation becomes effort-bounded, resumable, and demand-directed;
2. independent elaboration proposals may be produced in parallel and merged;
3. final artifact authority moves from accepted Derivation replay to checking
   elaborated semantic content from scratch.

The direction is accepted, but the original FC0-FC9 order is too permissive.
It leaves the minimal checked Core, occurrence authority, binder identity, and
checker mode as open questions while already scheduling checker and container
implementation. Those questions determine the data model and must be closed
first.

This plan therefore adopts PR22 with a checker-first execution order and with
explicit boundaries for current A Program semantics.

## 2. Review Scope

The proposal was compared with the current code in:

- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/include/a_program/graph/typed_occurrence_model.h`;
- `src/prototype/include/a_program/graph/compile_metadata.h`;
- `src/prototype/include/a_program/kernel/context.h`;
- `src/prototype/include/a_program/kernel/type_declaration.h`;
- `src/prototype/include/a_program/kernel/judgement/types.h`;
- `src/prototype/include/a_program/kernel/judgement/db.h`;
- `src/prototype/include/a_program/artifact/interface.h`;
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc`;
- `src/prototype/src/kernel/typing/accepted_replay.inc`;
- `src/prototype/src/driver/compiler_session.c`;
- `src/prototype/src/driver/read_file.c`;
- `src/prototype/src/artifact/wire_v86.c`;
- `src/prototype/src/artifact/publication/`;
- `src/prototype/spec/artifact_v86.schema`; and
- the current artifact, Function Graph, HOTT, timing, and integration tests.

This section records the original review scope. Implementation work performed
after that review is tracked by the EC checklists below.

## 3. Executive Verdict

| Proposal | Verdict | Required treatment |
| --- | --- | --- |
| Producer history is not final semantic authority | ACCEPT | Make checked semantic content the target authority |
| Current replay remains during migration | ACCEPT | Keep it as a blocking oracle until exact coverage exists |
| Paused work is not a residual proof | ACCEPT | Use distinct status types and artifact sections |
| Effort does not change typing truth | ACCEPT | Test completed-content and pause invariance |
| Semantic solvers share one orchestration protocol | ACCEPT | Do not merge their payloads or rules |
| Typed occurrences are needed above erased TermDB | ACCEPT WITH CHANGE | Serialize a semantic projection, not the current mixed record |
| A checker may use stored classifiers | ACCEPT WITH CHANGE | Treat every classifier as an assertion, never as its own premise |
| `.a` may contain checked and unfinished state | DEFER PHYSICAL UNIFICATION | First implement separate logical views and independent digests |
| Runtime suspension belongs in the first container | REJECT | Static compilation/checking comes first |
| Parallel workers merge local arenas | ACCEPT WITH CHANGE | No worker may mutate shared TermDB, ContextDB, or SubstitutionDB |
| Stable names identify work across revisions | ACCEPT WITH CHANGE | Goal identity must not become object Binding identity |
| Claims and Derivations become optional | ACCEPT AS FINAL TARGET | Do not erase them before all current authority consumers migrate |
| Canonical bytes are schedule independent | ACCEPT CONDITIONALLY | Require deterministic conflict and proposal selection policies |
| Resume is equivalent to one uninterrupted attempt | ACCEPT CONDITIONALLY | Scope the law to one producer and one cost-model version |

The largest correction is this:

> The first implementation task is not a scheduler or a new file format. It is
> an explicit untrusted elaborated-module schema and a Derivation-independent
> checker for that schema.

## 4. Current Implementation Findings

### 4.1 The current compile budget is not whole-compilation effort

`prototype_compile_metadata` stores separate normalization and solver counters.
The classifier fixed point increments `solver_steps_used`, but graph building,
Context resolution, proof materialization, evidence closure, Universe closure,
Function Graph generation, artifact publication, and accepted replay are not
charged through one effort protocol.

On failure, `prototype_ast_compile_pending_with_imports` frees its stack/heap
workspace and rolls back the typed-occurrence transaction. No resumable solver
frontier survives.

PR22 is correct that the present limit must not be renamed to global effort.

### 4.2 TypedOccurrenceGraph mixes four roles

The current occurrence record contains:

1. semantic occurrence data: Core term, Context, child roles, Binding identity,
   selected classifier, Match refinement, and fold binders;
2. source data: AST ID and source symbols;
3. producer state: pending/solved/residual status and source-side intermediate
   classifier fields; and
4. publication/control state: verification obligation and transaction extents.

A from-scratch checker needs role 1 and selected parts of the transformation
data. It must not accept roles 2-4 as checked semantics. Serializing the current
struct as the checked Core would preserve compiler implementation details and
make the checker circular.

### 4.3 Context and Substitution are structural candidates today

ContextDB and SubstitutionDB already store immutable structural records and
rebuildable indices. This is a good basis for direct checking. However,
certification of an `EXTEND` substitution currently lives separately in
`accepted_substitution_claims` and accepted Derivations.

The target checker must validate `EXTEND` directly:

```text
sigma : Delta -> Gamma
Delta |- a : A[sigma]
---------------------------------
<sigma, a> : Delta -> (Gamma, x:A)
```

It then mints an in-memory checked substitution capability. A numeric
Substitution ID read from a file is never that capability.

### 4.4 Artifact v86 is proof-graph authoritative

The current reader performs, in order, graph readback, occurrence readback,
Universe readback, accepted graph validation, Identity-root validation,
Function Graph association validation, Universe replay, and export Claim
validation.

Current wire authority includes:

- Proposition resource-usage vectors;
- exact Claim-backed exports;
- accepted Substitution evidence;
- Identity roots referring to Claims;
- Universe obligations referring to Claims and Derivations; and
- dependency edges discovered through proof premises.

Deleting Derivations now would remove semantic information. PR22 correctly
keeps v86 replay during migration.

### 4.5 Accepted replay is not a from-scratch checker

`prototype_judgement_validate_accepted_graph` validates selected accepted rule
applications. It receives the exact Proposition, Claim, Derivation, premise,
and rule payload graph chosen by the compiler.

That is valuable validation, but it answers:

> Is this serialized derivation DAG a valid execution of the admitted rules?

The target checker must answer:

> Does this elaborated module type-check, regardless of how the producer found
> it?

The new checker may share Term representation, pure normalization, Context
operations, and small rule predicates. It must not consume accepted Claim IDs,
Derivation order, solver state, or candidate-publication state.

### 4.6 Current stores are not parallel writer stores

Term construction, substitution, normalization, Context interning, and graph
transactions mutate arena-owned indices and caches. There is no thread or
process worker layer in the current compiler.

Parallel elaborators therefore cannot write into one shared TermDB or
CompileMetadata. The first parallel model must use fragment-local stores over
an immutable checked base. Merge performs one typed structural import into a
new candidate image.

### 4.7 Normalization originally stopped without a residual producer

Normalization now distinguishes `COMPLETE`, `BLOCKED_EFFECT`, `EXHAUSTED`, and
`INVALID` and exposes a process-local machine whose exhausted result is a valid
residual Core graph. Completed beta/iota/CBPV rewrites therefore survive the
next advance. The first machine deliberately does not serialize an evaluator
stack, so traversal of an unreduced outer context may repeat. Persistent exact
stack resume remains separate from semantic checking and must not be emulated
by storing an input term plus a replay cursor.

## 5. Corrected Semantic Boundary

### 5.1 Three distinct object types

The implementation must use distinct C types for:

```text
ElaboratedModuleView  untrusted explicit semantic assertions
CheckedModuleView     checker-minted read-only capability
WorkCapsule           non-authoritative producer continuation
```

No `accepted` Boolean may convert the first or third into the second.

### 5.2 Minimal elaborated module

The untrusted module must contain only data needed to check meaning:

```text
ElaboratedModule
  calculus and intrinsic fingerprints
  exact imported checked-interface fingerprints
  Core Term arena
  TypeDeclaration semantic schema
  Context arena
  Substitution arena
  semantic typed occurrences and child edges
  explicit classifiers
  explicit dependent Match motives/refinements
  explicit effect and totality classifiers
  canonical resource-usage input or reconstructible occurrence data
  object proof terms and admitted Core evidence forms
  conditional contracts
  exact exports and transparency
  Function Graph associations/selectors
  Identity/HOTT roots representable at the admitted object/Core boundary
```

It must not contain semantic dependence on:

- source AST IDs or spans;
- display symbols except qualified exported names;
- solver metas, states, reasons, queues, or revisions;
- selected Claim IDs or Derivation IDs;
- normalization traces or cache entries;
- transaction state; or
- a stored checker-success bit.

### 5.3 Semantic occurrence projection

Do not serialize `struct prototype_typed_occurrence` directly for the target
format. Define a separate immutable projection. Its initial field inventory is:

```text
SemanticOccurrence
  kind
  category
  application_role
  context
  core_term
  asserted_classifier
  child role edges
  exact Binding identity where Core alpha-sharing erased it
  binder classifier where required by the rule
  Match constructor owner, telescope Context, and refinement Substitution
  fold return/argument/continuation Binding identities
  admitted Context action when the occurrence is a semantic reindexing
```

Each current source/intermediate field must receive one written decision:

- retained as semantic input;
- deterministically reconstructed;
- moved to debug/provenance; or
- removed.

No field may remain merely because current replay reads it.

### 5.4 Checking mode

A Program surface synthesis remains producer work. The target checker is
bidirectional only at the explicit checked-Core boundary:

```text
infer_local(Gamma, occurrence) -> classifier
check(Gamma, occurrence, asserted_classifier)
```

`infer_local` is allowed only for syntax-directed rules such as local variable
lookup, Pi application, constructor spine formation, and explicit motive
application. It must not perform:

- overload or unqualified-name resolution;
- metavariable search;
- expected-type-driven surface elaboration;
- higher-order unification;
- Match motive synthesis;
- proof search; or
- Function Graph generation.

Every non-syntax-directed choice is explicit in `ElaboratedModule`.

This preserves the A Program invariant that `::` is post-synthesis checking.
The artifact checker validates an already elaborated expected-type boundary; it
does not use that boundary to reconstruct the source program.

### 5.5 Stable identity

Three identities must remain distinct:

1. object identity: generative Context and Binding identity within one checked
   module;
2. storage identity: dense arena indexes local to one serialized module; and
3. incremental matching identity: `GoalKey` and dependency keys across
   revisions.

An incremental key must never force two alpha-similar Context bindings to
become one object binding. Cross-revision work matching may use a qualified
declaration identity plus a stable producer-local path, but checked Contexts
remain generative.

Fragment import uses one schema-driven relocation pass from local storage IDs
to candidate storage IDs. This is not a semantic `Remap` layer and must not be
retained as ongoing authority. All local references are rewritten once through
typed visitors, then the relocation table is discarded.

### 5.6 Checked capabilities

After validation, the checker may mint opaque process-local handles such as:

```text
checked_context_ref
checked_substitution_ref
checked_occurrence_ref
checked_export_ref
```

These handles are outputs of checking. They are never deserialized directly.

## 6. Authority Replacement Matrix

| Current authority | Required target source | Removal gate |
| --- | --- | --- |
| HAS_TYPE/IS_TYPE Claim | checked semantic occurrence or object proof Term | All occurrence kinds checked without Claim lookup |
| Accepted Derivation | syntax-directed checker rule or checked object evidence | Erasure test gives same checked exports |
| `accepted_substitution_claims` | direct Context/Substitution checker capability | Every substitution kind checked independently |
| Proposition usage vector | deterministic occurrence usage or checked explicit vector | Recomputed vector matches v86 on all fixtures |
| Export Claim ID | exact checked occurrence and classifier | Export closure has no Claim dependency |
| Identity root Claim tuple | checked object Term or deterministic Core identity rule | HOTT root checker requires no accepted graph |
| Universe Claim/Derivation provenance | constraints reconstructed from semantic content | Same closed levels and rejection behavior |
| Proof-premise dependency slicing | exact semantic reference closure | Same dependency set without Derivation traversal |
| Function Graph proof association | checked generated declarations plus validated interface projection | Corruption tests pass without Claim IDs |
| Verification residual edge | explicit typed conditional contract | Incomplete producer work cannot create a contract |

## 7. Container Decision

The first implementation does not physically combine all state.

1. Implement `ElaboratedModuleView`, `CheckedModuleView`, and `WorkCapsule` as
   separate in-memory structures.
2. Add an independently checkable checked-content wire section only after the
   checker covers the current admitted fragment.
3. The first new `.a` format may package checked content, conditional contracts,
   producer state, and debug data, but each section has an independent digest
   and typed parser.
4. Import reads and checks only semantic content and contracts. It does not load
   producer state into the checked environment.
5. Runtime suspension is excluded from the first `.a` version.
6. At final cutover, bump the artifact format once, remove the v86 parser, and
   keep no compatibility implementation. During development, v86 remains the
   authority oracle rather than becoming a second permanent format.

## 8. Revised Implementation Sequence

### EC0: Freeze checked-Core authority - COMPLETE FOR ADMITTED FRAGMENT

- [x] Inventory every field of TermDB, TypeDeclaration semantic schema,
      ContextDB, SubstitutionDB, TypedOccurrenceGraph, VerificationDB, export
      interface, UniverseDB, and Identity roots.
- [x] Complete the retain/reconstruct/debug/remove decision for every occurrence
      field.
- [x] Complete the authority replacement matrix for every v86 proof kind.
- [x] Specify checking and local-inference mode for every admitted
      Core/occurrence tag; unsupported future tags reject or pause explicitly.
- [x] Specify exact resource-usage reconstruction.
- [x] Specify exact dependency-closure reconstruction.
- [x] Specify object, storage, and incremental identities.
- [x] Freeze `ElaboratedModuleView` and opaque `CheckedModuleView` headers.
- [x] Add a dependency rule preventing checker headers from including
      candidate-publication or accepted-replay APIs.

Exit criteria:

- no checked-Core field remains described as merely "explicit or
  reconstructible";
- every field has one exact authority rule; and
- no Claim ID appears in the target semantic schema.

### EC1: Build semantic projections in memory - COMPLETE

- [x] Add read-only views over TermDB, ContextDB, SubstitutionDB, semantic type
      schema, and intrinsic environment.
- [x] Add the immutable semantic occurrence projection.
- [x] Project one completed current compile into an untrusted
      `ElaboratedModuleView`.
- [x] Keep source provenance, diagnostics, solver state, and performance counters
      outside this view.
- [x] Validate current local IDs, ranges, dense Context/Substitution/Symbol
      references, and calculus/intrinsic fingerprints before
      semantic checking.
- [x] Add mutation tests proving that debug/provenance changes do not affect the
      semantic projection.

Exit criteria:

- current source compilation produces one immutable untrusted module view;
- the view can be destroyed independently of solver work state; and
- its semantic digest is stable when only diagnostics or source spans change.

### EC2: Implement the foundational from-scratch checker - COMPLETE FOR ADMITTED FRAGMENT

Planned files, subject to EC0 naming:

- `src/prototype/include/a_program/checker/module.h`;
- `src/prototype/include/a_program/checker/session.h`;
- `src/prototype/src/checker/module.c`;
- `src/prototype/src/checker/context.c`;
- `src/prototype/src/checker/substitution.c`;
- `src/prototype/src/checker/occurrence.c`; and
- focused tests under `src/prototype/tests/checks/`.

Tasks:

- [x] Check calculus and intrinsic fingerprints and imported checked-base
      capabilities.
- [x] Check empty and value-extended Contexts in parent order for the initial
      zero-indexed fragment.
- [x] Check identity, empty, projection, extension, and composition
      Substitutions directly, including non-allocating reindex comparison.
- [x] Check Universe-variable assertions and basic type formation for the
      initial fragment.
- [x] Check variables, Pi, Lambda, APP, literals, intrinsics, constructors, and
      constructor spines for the currently admitted zero-indexed fragment.
- [x] Recompute the admitted DefEq fragment with checker-local, non-allocating
      beta/type-family comparison. The mutable producer normalization cache is
      deliberately not a checker dependency.
- [x] Return `COMPLETE`, `PAUSED`, or `REJECTED`; only `COMPLETE` mints a checked
      module view.
- [x] Compare each reconstructed judgement with v86 Claims only in the test
      harness, never inside checker implementation.

Exit criteria:

- the checker consumes no Claim, Derivation, or solver state;
- forged classifiers, Context endpoints, and Substitution assignments fail;
- ordinary examples agree with v86 replay; and
- removing the accepted proof graph from the checker input has no effect on the
  covered fragment.

### EC3: Check dependent Match and CBPV - COMPLETE FOR ADMITTED FRAGMENT

- [x] Check explicit Match motives without motive synthesis.
- [x] Check a motive body under its own binders and require the producer's
      constant-motive candidate to be the same normalized classifier whose
      branch-binder independence was validated. This prevents a checked
      classifier from retaining an unscoped branch-local term hidden by
      normalization.
- [x] Check branch telescope Contexts and constructor ownership.
- [x] Check solved and constant branch refinements through exact Substitutions.
- [x] Check induction-hypothesis occurrence rules.
- [x] Check `RETURN`, `THUNK`, `FORCE`, operation request, and computation fold,
      including multiple clauses and effect-row-polymorphic operations.
- [x] Check effect rows structurally without host dispatch; union comparison is
      order-independent and handled clauses recurse through latent rows.
- [x] Check totality labels only from admitted explicit constructions.
- [x] Keep checker effort pause distinct from conditional contracts; explicit
      blocked-effect producer-state projection remains to be audited.
- [x] Add wrong-motive, wrong-owner, wrong-refinement, underreported-effect,
      false-totality, wrong-handler-operation, and wrong-continuation mutation
      tests.

Exit criteria:

- examples 01-09 and current dependent/CBPV fixtures agree with v86;
- no effect operation is executed by the checker; and
- compile effort exhaustion never becomes `MAY_DIVERGE` or a verification
  obligation.

### EC4: Reconstruct resources, exports, and dependencies - COMPLETE

- [x] Recompute resource usage from semantic occurrences and Context structure.
- [x] Compare recomputed vectors with current Proposition vectors in the
      migration test harness; the checker itself consumes no Proposition data.
- [x] Check exports from exact checked occurrences and transparency and mint
      opaque checked-export capabilities.
- [x] Recompute direct import dependencies from reachable qualified Core
      `EXTERNAL_REF` nodes and reject both missing and extra declarations.
- [x] Extend dependency closure beyond the selected entry: semantic compaction
      roots Contexts, Substitutions, type schemas, contracts, every occurrence,
      Match/fold payloads, and object proof Terms before dependencies are
      reconstructed from the resulting `EXTERNAL_REF` closure.
- [x] Prove by construction that dependencies reachable only through compiler
      Claim/Derivation premises are excluded. Object proof Terms remain normal
      semantic roots; proof-search provenance is not object dependency data.
- [x] Replace checked-Core test-path export Claim lookups with checked export
      capabilities.
- [x] Add extra-dependency and independently reconstructed usage tests.
- [x] Add missing and extra semantic dependency tests.
- [x] Make forged stored usage and proof-only dependency inputs impossible in
      the checked schema: usage is checker output, and Claim/Derivation premises
      are absent from `ElaboratedModuleView`.
- [x] Add imported opaque-definition non-unfolding tests. Providers with the
      same exact classifier and different opaque bodies are interchangeable;
      missing, ambiguous, and classifier-forged imports are rejected.

Exit criteria:

- covered exports no longer need Claim IDs;
- dependency sets match v86 on all current artifacts; and
- future ONE/MANY resource behavior has one explicit authority.

### EC5: Reconstruct declarations and Universes - COMPLETE FOR ADMITTED FRAGMENT

- [x] Check only `prototype_type_semantic_schema_db` as declaration authority.
- [x] Exclude readback and constructor-classifier caches from the elaborated
      module; retain only operational representation identity in Core terms.
- [x] Complete the checked-Core audit: `representation_id` is read only as the
      operational erased-algebra identity needed by `TYPE_FORMER`; no readback,
      constructor-classifier cache, or representation cache API is reachable
      from the checker.
- [x] Check parameter and index Contexts, constructor telescopes, and result
      classifiers.
- [x] Reconstruct Universe constraints from checked semantic content.
- [x] Solve and compare declared levels without Claim/Derivation provenance.
- [x] Do not admit a closure certificate in the first checker. Declared levels
      are untrusted expected output and all constraints are reconstructed; an
      optional certificate remains a future acceleration that must be checked.
- [x] Add changed-level, forged-schema, and indexed-result-classifier mutation
      tests, plus a permanent indexed `Vec` acceptance boundary.
- [x] Add a positive-cycle mutation test. Omitted producer constraints are
      covered structurally because neither the elaborated-module input nor the
      checker API contains producer Universe constraints.
- [x] Generalize recursive-field IH classifier lifting for higher-order indexed
      fields, with permanent abstract, concrete, and eliminator `Acc`
      acceptance boundaries.

Exit criteria:

- Universe closure and type declarations check without accepted replay; and
- deleting readback/cache data does not change checked meaning.

### EC6: Check Function Graph and Identity/HOTT boundaries - COMPLETE FOR ADMITTED FRAGMENT

- [x] Check generated Function Graph IADTs and functions as ordinary content,
      including generated length, two recursive calls, and dependent spines.
      This required exact Context-action provenance and multi-beta computation
      classifier views rather than Function Graph exceptions.
- [x] Validate association and selector metadata only after their referenced
      declarations and exports are checked. The semantic interface uses export,
      declaration, constructor, and field ordinals; assignment IDs and AST
      Binder IDs are excluded.
- [x] Inventory each admitted Identity/HOTT boundary. `RELATION_*` and witness
      object Terms are ordinary checked-Core content; current artifact Identity
      roots are Claim tuples created only by the HOTT publication/test path and
      are not projected as object evidence; dimension content remains an
      explicit unsupported checker family.
- [x] Do not translate Claim-only Identity roots into a second semantic record.
      A root enters the future checked container only after its family and
      witness are represented by checked object Terms and exports.
- [x] Keep current v86 replay authoritative for unsupported HOTT families while
      preventing the independent checker from minting a capability for them.
- [x] Add forged association, selector, and dimension mutations. Claim-only
      Identity roots are absent by type from `ElaboratedModuleView`, so they
      cannot be presented as checked input; object relation Terms remain subject
      to ordinary occurrence checking as their surface producer is admitted.

Exit criteria:

- every admitted root has an explicit checker rule or ordinary checked Term;
- compiler-local HOTT plans are not object evidence; and
- no unsupported root is silently accepted as checked.

### EC7: Complete dual validation and authority erasure test - COMPLETE FOR ADMITTED FRAGMENT

- [x] Run current v86 replay and the from-scratch checker for every admitted
      checked-Core fixture. The harness rejects a compile that skipped accepted
      replay during migration.
- [x] Treat acceptance and independently reconstructed resource disagreement as
      blocking; forged exports, dependencies, Universe levels, schema, and
      runtime contracts are permanent negative tests.
- [x] Produce a proof-graph-erased in-memory image by deep-projecting semantic
      content, destroying the entire producer storage, and rechecking it.
- [x] Recheck the erased image and require the same checked export set. Checked
      capabilities reference only the immutable projected interface.
- [x] Measure replay and checker time separately in the permanent fixture run.
- [x] Add a static source audit that the checker does not call accepted replay,
      candidate publication, or solver selection.

Exit criteria:

- the complete currently admitted fragment passes without Claim/Derivation
  input; and
- v86 remains only a migration oracle, not a hidden fallback.

### EC8: Introduce the checked `.a` container - IN PROGRESS

- [x] Define v87 with independently hashed semantic and conditional-contract
      sections, plus recognized optional producer and debug sections.
- [x] Exclude runtime suspension from the first format.
- [x] Parse and hash-check all sections before checking semantic content.
- [x] Make producer and debug sections optional and non-authoritative.
- [x] Make a checked-only `.a` valid.
- [x] Require fresh checking on import; never deserialize checked capabilities.
- [x] Add canonical writer ordering and byte-stability tests.
- [ ] After every published root has checked object evidence, complete one final
      format bump and remove the v86 reader without a compatibility parser.

The v87 writer accepts only an opaque `prototype_checked_module`; it cannot
serialize an untrusted elaborated view. The reader reconstructs a fresh owned
`prototype_elaborated_module`, validates its structure, and invokes the
independent checker before minting a process-local checked capability. Semantic
payload corruption and section hash corruption are permanent negative tests.
Adding a correctly hashed debug section does not change acceptance.

The remaining v86 reader is not a fallback for v87. It remains the migration
oracle for Claim-only HOTT roots that are deliberately outside the admitted
checked-Core input type. Removing it before those roots become object Terms
would silently drop accepted semantics rather than complete EC8.

Exit criteria:

- erasing producer/debug sections preserves checked exports;
- corrupting those sections cannot grant semantic authority; and
- corrupting semantic content is rejected by from-scratch checking.

### EC9: Add typed effort accounting - COMPLETE

- [x] Add a scheduler-owned effort budget with versioned cost model.
- [x] Allocate typed credits to classifier, normalization, motive, proof search,
      Function Graph generation, and checking.
- [x] Keep object totality entirely separate from compile effort.
- [x] Preserve per-phase counters for diagnostics and performance comparison.
- [x] Return `Paused` without publishing or rolling back the last checked base.
- [x] Add deterministic split-budget tests, including atomic multi-phase debit.

The cost model is `PROTOTYPE_EFFORT_COST_MODEL_VERSION == 1`. Graph and
Function Graph construction, inference normalization/motive setup, and final
proof publication are charged atomically before mutating their phase. The
classifier worklist is charged one constraint at a time. This makes every
actual pause a transaction-safe producer frontier instead of a deep helper
return that has lost invocation-local state.

Exit criteria:

- every bounded phase reports consumed credits;
- a paused producer preserves the checked base byte-for-byte; and
- changing effort changes discovery progress, not the meaning of completed
  content.

### EC10: Make producers resumable in memory - COMPLETE FOR CLASSIFIER AND NORMALIZER

- [x] Move classifier fixed-point worklists and cells out of invocation-local
      stack state into an owned producer session.
- [x] Convert normalization to an explicit residual-graph machine. Each
      exhausted advance returns a valid resumable Core term; a separate
      evaluation stack is intentionally not claimed by this first version.
- [ ] Add typed capsules independently for motive, effect, Universe, proof, and
      Function Graph producers only where interruption is useful.
- [x] Keep raw pointers only in the process-local session, never in a capsule.
- [ ] Reject resume under changed calculus, intrinsics, imports, producer
      version, or cost-model version.
- [x] Add a permanent split-credit session boundary that pauses before graph
      mutation, repeatedly resumes the same classifier worklist, and completes
      the same frozen transaction.

`prototype_compile_producer_session` owns the lowering workspace and mutable
solver frontier. It borrows candidate-local source and semantic arenas
exclusively for its lifetime. Destroying an unfinished session rolls back its
occurrence transaction; pausing does not. The Core normalizer can resume from a
residual graph independently. Compiler call sites that still precharge an
atomic inference subphase are not falsely described as preserving an evaluator
stack, and must be migrated only together with a producer-local transaction for
the enclosing constraint.

Exit criteria:

- resume continues the saved frontier instead of restarting it; and
- no capsule can be imported as checked authority.

### EC11: Persist producer state - COMPLETE FOR FRAGMENT MERGE; CLASSIFIER PENDING

- [x] Define and canonically serialize a typed, versioned outer capsule with
      producer kind/version, cost-model version, calculus/intrinsic/base/goal
      fingerprints, positive dependencies, payload-format tag, and integrity
      hash.
- [x] Permit that capsule in the v87 producer section while ordinary checked
      import hash-checks and discards it without changing semantic authority.
- [x] Serialize a typed, versioned fragment-merge payload. The generic outer
      capsule deliberately does not license opaque byte dumps as a producer
      codec.
- [x] Keep merge-capsule arenas self-contained as canonical checked fragment
      bytes plus a pairwise-conflict cursor.
- [x] Record positive observations by stable GoalKey and 256-bit checked-content
      fingerprint; process-local Symbol IDs were removed from capsule v2.
- [x] Define negative observations only for sealed candidate-space keys and
      fingerprints.
- [x] Add malformed, stale, incompatible, canonical round-trip, and fresh-session
      resume tests.
- [x] Confirm ordinary v87 import ignores the producer section and freshly
      checks semantic content.

Malformed, corrupted, and stale outer-capsule tests are permanent. The merge
producer resumes in a fresh session from canonical fragment bytes and the exact
next conflict pair. Rechecking those bytes mints new checked capabilities; it
does not replay merge work. Classifier persistence remains blocked until its
large shared `compile_context` is replaced by a producer-local fragment. A
cursor plus reconstruction of hidden classifier state remains explicitly
unaccepted.

Exit criteria:

- a fresh process resumes supported producers; and
- deleting all producer state still permits checking completed semantic content.

### EC12: Add fragment merge before parallel execution - COMPLETE FOR INDEPENDENT MODULES

- [x] Keep each fragment's Term, Context, Substitution, declaration, and
      occurrence stores module-local over immutable checked bases.
- [x] Replace flattening/import relocation with checked module-graph
      composition. Numeric local IDs never cross the module boundary.
- [x] Eliminate persistent relocation tables from the new merge path.
- [x] Deduplicate byte-identical modules and reject unequal term, type, or
      constructor exports with the same qualified identity.
- [x] Require mutually recursive declarations to be one checked fragment/SCC;
      independent module merge does not invent cross-fragment recursion.
- [x] Canonically sort modules by canonical v87 checked bytes.
- [x] Require each fragment to be freshly checked before it can enter the set.

The original physical-import plan is superseded. Flattening already checked
modules would create a large relocation layer solely to erase module-local
storage identity. A checked module graph preserves the intended semantic
composition while keeping object Binding identity generative and local.

Exit criteria:

- fragment arrival order cannot silently select an implementation;
- merge does not mutate the checked base; and
- canonical conflict-free merges produce identical checked bytes.

### EC13: Add parallel producers - COMPLETE FOR ISOLATED CHECK WORKERS

- [x] Start with fragment-isolated checker workers, not shared mutable DBs.
- [x] Reject duplicate mutable effort accounts across parallel tasks before
      starting workers.
- [x] Schedule independent elaborated modules against immutable checked
      snapshots.
- [x] Support dependencies only through exact checked-base capabilities; a
	  speculative dependency closure must remain one unchecked fragment.
- [x] Run worker-count and arrival-order permutations in the permanent module
      set boundary.
- [ ] Measure parallel speedup and duplicated-work cost on QuickSort-scale
      multi-fragment input.

Exit criteria:

- parallel and sequential production yield the same checked semantic content;
- no shared-store race exists; and
- speedup is measured separately from checker overhead.

### EC14: Add edit incrementality - COMPLETE FOR CHECKED MODULE SETS

- [x] Introduce stable 256-bit GoalKeys distinct from object Binding IDs.
- [x] Track exact positive dependency fingerprints per producer support.
- [x] Track negative dependencies only against sealed candidate-space
      fingerprints.
- [x] Invalidate reverse dependency closure after edits.
- [x] Preserve alternative producer supports until the last valid support is
      removed.
- [x] Integrate the invalidation kernel with canonical checked module sets.
      Every export has a stable GoalKey, a self-content observation, and exact
      positive observations for that module's checked interface dependencies.
- [x] Compare a changed checked-module snapshot with a freshly elaborated and
      checked snapshot. Permanent tests cover an independent export remaining
      valid and an opaque provider edit invalidating its consumer closure.
- [ ] Add a multi-file source scheduler above this checked-module boundary.
      Source parsing and speculative elaboration remain producer work and may
      not bypass fresh checking.

Exit criteria:

- no under-invalidation survives clean comparison; and
- edits cannot reinterpret exact references in an already elaborated module.

## 9. Explicitly Deferred Work

The following work is excluded until EC0-EC8 are complete:

- suspended runtime state in `.a`;
- exactly-once host-effect persistence;
- distributed checking;
- one universal semantic solver;
- speculative cross-module theorem assumptions;
- proof-driven DefEq extension;
- removal of Claim/Derivation storage from the compiler itself; and
- formal verification of the checker implementation.

Claim/Derivation may remain useful as producer provenance and diagnostics after
it ceases to be wire authority.

## 10. Required Permanent Tests

### 10.1 Checker boundaries

- [x] classifier assertion cannot prove itself;
- [x] Context extension requires its classifier to be a checked type;
- [x] Substitution extension checks the assigned term after exact reindexing;
- [x] APP checks Pi domain and instantiated codomain;
- [x] Match checks the explicit motive and every branch;
- [x] effect rows are recomputed without dispatch;
- [x] totality is never inferred from compile completion;
- [x] resource usage is recomputed exactly; and
- [x] unsupported HOTT evidence is rejected, not treated as absent proof.

### 10.2 Authority erasure

- [x] remove Derivations from checked input;
- [x] remove Claims from checked input;
- [x] remove solver state from checked input;
- [x] remove normalization caches from checked input;
- [x] remove debug/source data from checked input;
- [x] retain identical checked exports and contracts.

### 10.3 Resume and transactionality

- [ ] pause before and after each producer step class;
- [x] reject stale revision capsules;
- [x] preserve checked base after pause/rejection;
- [x] split effort in several partitions;
- [x] compare with uninterrupted production.

### 10.4 Parallel merge

- [x] permute worker completion order;
- [x] merge equal fragments;
- [x] reject unequal same-name fragments;
- [x] check declared recursive SCCs atomically by requiring one source fragment;
- [x] reject dependencies on unpublished speculative fragments;
- [x] produce canonical bytes.

### 10.5 Performance

- [ ] record projection time;
- [ ] record from-scratch checker time by rule family;
- [ ] record current replay time during migration;
- [ ] record merge/import time;
- [ ] record capsule serialization and resume time;
- [ ] record sequential and parallel wall time on the same input;
- [x] retain QuickSort and Function Graph stress inputs.

## 11. Implementation Constraints

1. All implementation work remains under `src/prototype/` until explicitly
   promoted.
2. No compatibility reader is retained after the final artifact cutover.
3. Migration size is not a reason to preserve duplicate authority.
4. A shared helper is extracted only for an actually identical rule or data
   operation; distinct typing rules remain visible.
5. Current accepted behavior is not weakened while the replacement checker is
   incomplete.
6. No producer result is authoritative before checking.
7. No compiler budget status changes object typing, effects, or totality.
8. No host effect is executed by elaboration or artifact checking.
9. `::` remains post-synthesis expected-type checking.
10. TermDB remains the erased computation graph; occurrence, Context, and proof
    meaning remain above it.

## 12. Progress Summary

| Phase | State | Blocking dependency |
| --- | --- | --- |
| PR22 merge | COMPLETE | none |
| Critical review | COMPLETE | none |
| EC0 checked-Core authority | COMPLETE | authority schema frozen for admitted fragment |
| EC1 semantic projection | COMPLETE | EC0 boundary decisions used by projection |
| EC2 foundational checker | COMPLETE FOR ADMITTED FRAGMENT | future calculus formers require explicit rules |
| EC3 Match/CBPV checker | COMPLETE FOR ADMITTED FRAGMENT | effectful HOTT action remains future calculus work |
| EC4 resources/exports/dependencies | COMPLETE | none |
| EC5 declarations/Universes | COMPLETE FOR ADMITTED FRAGMENT | refined Universe calculus remains separate work |
| EC6 Function Graph/HOTT | COMPLETE FOR ADMITTED FRAGMENT | HOTT object-language expansion is explicit future calculus work |
| EC7 dual validation | COMPLETE FOR ADMITTED FRAGMENT | EC6 |
| EC8 `.a` container | IN PROGRESS; v87 checked fragment complete, v86 retirement deferred | object-evidence coverage for remaining HOTT roots |
| EC9 typed effort | COMPLETE | none |
| EC10 in-memory resume | COMPLETE FOR CLASSIFIER AND RESIDUAL-GRAPH NORMALIZER | exact evaluator-stack persistence remains optional follow-up |
| EC11 persistent capsules | MERGE COMPLETE; CLASSIFIER PENDING | producer-local classifier arena |
| EC12 fragment merge | COMPLETE FOR INDEPENDENT MODULES | cross-fragment SCCs intentionally excluded |
| EC13 parallel production | ISOLATED CHECK WORKERS COMPLETE | source producer scheduler and benchmarks |
| EC14 edit incrementality | CHECKED MODULE-SET DRIVER COMPLETE | multi-file source scheduler |

## 13. Final Decision

PR22 is accepted as an architectural direction, not as a directly executable
FC0-FC9 checklist.

The accepted order is:

```text
freeze checked semantics
  -> project untrusted elaborated content
  -> check it independently in memory
  -> replace every current proof-only authority
  -> prove Claim/Derivation erasure
  -> introduce the new container
  -> add typed effort and resumable producers
  -> add deterministic fragment merge
  -> add parallel and edit-incremental production
```

This order preserves A Program's compiler philosophy: compilation and checking
are ahead-of-time computations, while runtime evaluation is later computation.
The boundary between them is operational timing, not permission for unfinished
compiler work to become logical truth.
