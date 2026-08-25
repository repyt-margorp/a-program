# PR22 Effort Compilation: Critical Review and Implementation Plan

Date: 2026-08-25 JST

Status: reviewed and planned; implementation not started

PR: #22, `Document effort compilation and from-scratch artifact checking`

Merged revision: `de52b75943cc3a330969f9ce969b1c86d73cfb1b`

Reviewed implementation revision: `de52b75943cc3a330969f9ce969b1c86d73cfb1b`

Current artifact format: v86

Source design:

- `2026-08-25T17-26-30-EFFORT-COMPILATION-PARALLEL-ELABORATION-AND-FROM-SCRATCH-ARTIFACT-CHECKING-DESIGN.md`

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

No implementation source is changed by this review.

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

### 4.7 Normalization can stop but cannot resume

Normalization distinguishes `COMPLETE`, `BLOCKED_EFFECT`, and `EXHAUSTED`, and
it has profile-aware caches. It does not expose a serializable machine state
containing the evaluation stack and continuation. Persisting only the input
Term and spent fuel would restart, not resume, expensive reductions.

An explicit normalization machine is a prerequisite for persistent resume, but
it is not a prerequisite for the first from-scratch checker.

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

### EC0: Freeze checked-Core authority - PENDING

- [ ] Inventory every field of TermDB, TypeDeclaration semantic schema,
      ContextDB, SubstitutionDB, TypedOccurrenceGraph, VerificationDB, export
      interface, UniverseDB, and Identity roots.
- [ ] Complete the retain/reconstruct/debug/remove decision for every occurrence
      field.
- [ ] Complete the authority replacement matrix for every v86 proof kind.
- [ ] Specify checking and local-inference mode for every Core/occurrence tag.
- [ ] Specify exact resource-usage reconstruction.
- [ ] Specify exact dependency-closure reconstruction.
- [ ] Specify object, storage, and incremental identities.
- [ ] Freeze `ElaboratedModuleView` and opaque `CheckedModuleView` headers.
- [ ] Add a dependency rule preventing checker headers from including
      candidate-publication or accepted-replay APIs.

Exit criteria:

- no checked-Core field remains described as merely "explicit or
  reconstructible";
- every field has one exact authority rule; and
- no Claim ID appears in the target semantic schema.

### EC1: Build semantic projections in memory - PENDING

- [ ] Add read-only views over TermDB, ContextDB, SubstitutionDB, semantic type
      schema, and intrinsic environment.
- [ ] Add the immutable semantic occurrence projection.
- [ ] Project one completed current compile into an untrusted
      `ElaboratedModuleView`.
- [ ] Keep source provenance, diagnostics, solver state, and performance counters
      outside this view.
- [ ] Validate all local IDs, ranges, dense references, and fingerprints before
      semantic checking.
- [ ] Add mutation tests proving that debug/provenance changes do not affect the
      semantic projection.

Exit criteria:

- current source compilation produces one immutable untrusted module view;
- the view can be destroyed independently of solver work state; and
- its semantic digest is stable when only diagnostics or source spans change.

### EC2: Implement the foundational from-scratch checker - PENDING

Planned files, subject to EC0 naming:

- `src/prototype/include/a_program/checker/module.h`;
- `src/prototype/include/a_program/checker/session.h`;
- `src/prototype/src/checker/module.c`;
- `src/prototype/src/checker/context.c`;
- `src/prototype/src/checker/substitution.c`;
- `src/prototype/src/checker/occurrence.c`; and
- focused tests under `src/prototype/tests/checks/`.

Tasks:

- [ ] Check fingerprints and imported checked bases.
- [ ] Check empty and extended Contexts in parent order.
- [ ] Check identity, empty, projection, extension, and composition
      Substitutions directly.
- [ ] Check Universe variables and basic type formation.
- [ ] Check variables, Pi, Lambda, APP, literals, intrinsics, constructors, and
      constructor spines.
- [ ] Recompute DefEq with the pure normalization API.
- [ ] Return `COMPLETE`, `PAUSED`, or `REJECTED`; only `COMPLETE` mints a checked
      module view.
- [ ] Compare each reconstructed judgement with v86 Claims only in the test
      harness, never inside checker implementation.

Exit criteria:

- the checker consumes no Claim, Derivation, or solver state;
- forged classifiers, Context endpoints, and Substitution assignments fail;
- ordinary examples agree with v86 replay; and
- removing the accepted proof graph from the checker input has no effect on the
  covered fragment.

### EC3: Check dependent Match and CBPV - PENDING

- [ ] Check explicit Match motives without motive synthesis.
- [ ] Check branch telescope Contexts and constructor ownership.
- [ ] Check solved and constant branch refinements through exact Substitutions.
- [ ] Check induction-hypothesis occurrence rules.
- [ ] Check `RETURN`, `THUNK`, `FORCE`, operation request, and computation fold.
- [ ] Check effect rows structurally without host dispatch.
- [ ] Check totality labels only from admitted explicit constructions.
- [ ] Keep `EXHAUSTED` and `BLOCKED_EFFECT` distinct from conditional contracts.
- [ ] Add wrong-motive, wrong-owner, wrong-refinement, underreported-effect, and
      false-totality mutation tests.

Exit criteria:

- examples 01-09 and current dependent/CBPV fixtures agree with v86;
- no effect operation is executed by the checker; and
- compile effort exhaustion never becomes `MAY_DIVERGE` or a verification
  obligation.

### EC4: Reconstruct resources, exports, and dependencies - PENDING

- [ ] Recompute resource usage from semantic occurrences and Context structure.
- [ ] Compare recomputed vectors with current Proposition vectors.
- [ ] Check exports from exact checked occurrences and transparency.
- [ ] Recompute import dependencies from Core references, Contexts,
      Substitutions, type schemas, contracts, and object proof Terms.
- [ ] Prove that proof-only dependencies formerly reached through Derivation
      premises remain present.
- [ ] Replace test-path export Claim lookups with checked export capabilities.
- [ ] Add missing/extra dependency, forged usage, and opaque-unfolding tests.

Exit criteria:

- covered exports no longer need Claim IDs;
- dependency sets match v86 on all current artifacts; and
- future ONE/MANY resource behavior has one explicit authority.

### EC5: Reconstruct declarations and Universes - PENDING

- [ ] Check only `prototype_type_semantic_schema_db` as declaration authority.
- [ ] Treat readback, representation lookup, and constructor classifier caches
      as optional projections.
- [ ] Check parameter and index Contexts, constructor telescopes, and result
      classifiers.
- [ ] Reconstruct Universe constraints from checked semantic content.
- [ ] Solve and compare declared levels without Claim/Derivation provenance.
- [ ] Retain an optional closure certificate only as an acceleration hint.
- [ ] Add positive-cycle, omitted-constraint, changed-level, and forged-schema
      tests.

Exit criteria:

- Universe closure and type declarations check without accepted replay; and
- deleting readback/cache data does not change checked meaning.

### EC6: Check Function Graph and Identity/HOTT boundaries - PENDING

- [ ] Check generated Function Graph IADTs and functions as ordinary content.
- [ ] Validate association and selector metadata only after their referenced
      declarations and exports are checked.
- [ ] Inventory each admitted Identity/HOTT root as object syntax, deterministic
      Core rule, or unsupported target content.
- [ ] Replace Claim-only roots one family at a time.
- [ ] Keep current replay authoritative for any unsupported HOTT family.
- [ ] Add forged association, selector swap, forged dimension action, and forged
      Identity-root tests.

Exit criteria:

- every admitted root has an explicit checker rule or ordinary checked Term;
- compiler-local HOTT plans are not object evidence; and
- no unsupported root is silently accepted as checked.

### EC7: Complete dual validation and authority erasure test - PENDING

- [ ] Run current v86 replay and the from-scratch checker for every passing
      integration fixture.
- [ ] Treat any acceptance, export, dependency, usage, Universe, or contract
      disagreement as blocking.
- [ ] Produce a proof-graph-erased in-memory image.
- [ ] Recheck it and compare checked export fingerprints.
- [ ] Measure replay and checker time separately.
- [ ] Add a static source audit that the checker does not call accepted replay,
      candidate publication, or solver selection.

Exit criteria:

- the complete currently admitted fragment passes without Claim/Derivation
  input; and
- v86 remains only a migration oracle, not a hidden fallback.

### EC8: Introduce the checked `.a` container - PENDING

- [ ] Define independently hashed semantic, conditional-contract, producer,
      and debug sections.
- [ ] Exclude runtime suspension from the first format.
- [ ] Parse all sections structurally before checking semantic content.
- [ ] Make producer and debug sections optional and non-authoritative.
- [ ] Make a checked-only `.a` valid.
- [ ] Require fresh checking on import; never deserialize checked capabilities.
- [ ] Add canonical writer ordering and byte-stability tests.
- [ ] Complete one final format bump and remove the v86 reader without a
      compatibility parser.

Exit criteria:

- erasing producer/debug sections preserves checked exports;
- corrupting those sections cannot grant semantic authority; and
- corrupting semantic content is rejected by from-scratch checking.

### EC9: Add typed effort accounting - PENDING

- [ ] Add a scheduler-owned effort budget with versioned cost model.
- [ ] Allocate typed credits to classifier, normalization, motive, proof search,
      Function Graph generation, and checking.
- [ ] Keep object totality entirely separate from compile effort.
- [ ] Preserve per-phase counters for diagnostics and performance comparison.
- [ ] Return `Paused` without publishing or rolling back the last checked base.
- [ ] Add deterministic split-budget tests.

Exit criteria:

- every bounded phase reports consumed credits;
- a paused producer preserves the checked base byte-for-byte; and
- changing effort changes discovery progress, not the meaning of completed
  content.

### EC10: Make producers resumable in memory - PENDING

- [ ] Move classifier fixed-point worklists and cells out of invocation-local
      stack state into an owned producer session.
- [ ] Convert normalization to an explicit resumable machine before claiming
      normalization resume.
- [ ] Add typed capsules independently for motive, effect, Universe, proof, and
      Function Graph producers only where interruption is useful.
- [ ] Store no raw pointers in a capsule.
- [ ] Reject resume under changed calculus, intrinsics, imports, producer
      version, or cost-model version.
- [ ] Compare uninterrupted and pause/resume proposals canonically.

Exit criteria:

- resume continues the saved frontier instead of restarting it; and
- no capsule can be imported as checked authority.

### EC11: Persist producer state - PENDING

- [ ] Serialize only typed, versioned capsule payloads.
- [ ] Keep capsule-local dense arenas self-contained.
- [ ] Record positive dependency observations.
- [ ] Define negative observations only for sealed name/candidate spaces.
- [ ] Add malformed, stale, and incompatible capsule tests.
- [ ] Confirm capsule deletion affects performance only.

Exit criteria:

- a fresh process resumes supported producers; and
- deleting all producer state still permits checking completed semantic content.

### EC12: Add fragment merge before parallel execution - PENDING

- [ ] Define fragment-local Term, Context, Substitution, declaration, and
      occurrence stores over one immutable checked base.
- [ ] Implement one schema-driven structural importer.
- [ ] Discard relocation tables after import.
- [ ] Detect duplicate, alternative, and conflicting declarations explicitly.
- [ ] Check mutually recursive declarations as one declared SCC.
- [ ] Canonically sort diagnostics and publication order.
- [ ] Recheck every merged candidate before publication.

Exit criteria:

- fragment arrival order cannot silently select an implementation;
- merge does not mutate the checked base; and
- canonical conflict-free merges produce identical checked bytes.

### EC13: Add parallel producers - PENDING

- [ ] Start with process- or fragment-isolated workers, not shared mutable DBs.
- [ ] Schedule independent declaration/proof goals against immutable checked
      snapshots.
- [ ] Support speculative dependency closures only as one combined unchecked
      proposal.
- [ ] Run schedule permutations and stress tests.
- [ ] Measure merge, check, and duplicated-work cost.

Exit criteria:

- parallel and sequential production yield the same checked semantic content;
- no shared-store race exists; and
- speedup is measured separately from checker overhead.

### EC14: Add edit incrementality - PENDING

- [ ] Introduce stable GoalKeys distinct from object Binding IDs.
- [ ] Track exact positive dependencies.
- [ ] Track negative dependencies only against sealed candidate-space
      fingerprints.
- [ ] Invalidate reverse dependency closure after edits.
- [ ] Preserve alternative producer supports until the last valid support is
      removed.
- [ ] Compare every incremental result with a clean elaborate-and-check mode.

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

- [ ] classifier assertion cannot prove itself;
- [ ] Context extension requires its classifier to be a checked type;
- [ ] Substitution extension checks the assigned term after exact reindexing;
- [ ] APP checks Pi domain and instantiated codomain;
- [ ] Match checks the explicit motive and every branch;
- [ ] effect rows are recomputed without dispatch;
- [ ] totality is never inferred from compile completion;
- [ ] resource usage is recomputed exactly; and
- [ ] unsupported HOTT evidence is rejected, not treated as absent proof.

### 10.2 Authority erasure

- [ ] remove Derivations;
- [ ] remove Claims;
- [ ] remove solver state;
- [ ] remove normalization caches;
- [ ] remove debug/source data;
- [ ] retain identical checked exports and contracts.

### 10.3 Resume and transactionality

- [ ] pause before and after each producer step class;
- [ ] reject stale revision capsules;
- [ ] preserve checked base after pause/rejection;
- [ ] split effort in several partitions;
- [ ] compare with uninterrupted production.

### 10.4 Parallel merge

- [ ] permute worker completion order;
- [ ] merge equal fragments;
- [ ] reject unequal same-name fragments;
- [ ] check declared recursive SCCs atomically;
- [ ] reject dependencies on unpublished speculative fragments;
- [ ] produce canonical bytes.

### 10.5 Performance

- [ ] record projection time;
- [ ] record from-scratch checker time by rule family;
- [ ] record current replay time during migration;
- [ ] record merge/import time;
- [ ] record capsule serialization and resume time;
- [ ] record sequential and parallel wall time on the same input;
- [ ] retain QuickSort and Function Graph stress inputs.

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
| EC0 checked-Core authority | PENDING | plan approval |
| EC1 semantic projection | PENDING | EC0 |
| EC2 foundational checker | PENDING | EC1 |
| EC3 Match/CBPV checker | PENDING | EC2 |
| EC4 resources/exports/dependencies | PENDING | EC3 |
| EC5 declarations/Universes | PENDING | EC4 |
| EC6 Function Graph/HOTT | PENDING | EC5 |
| EC7 dual validation | PENDING | EC6 |
| EC8 `.a` container | PENDING | EC7 |
| EC9 typed effort | PENDING | EC7 |
| EC10 in-memory resume | PENDING | EC9 |
| EC11 persistent capsules | PENDING | EC10 |
| EC12 fragment merge | PENDING | EC8, EC11 |
| EC13 parallel production | PENDING | EC12 |
| EC14 edit incrementality | PENDING | EC13 |

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
