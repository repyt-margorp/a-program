# Current Authority and Semantic Duplication Re-Audit Plan V2

Date: 2026-08-24

Status: deferred post-implementation audit; not executed in the current change

Implementation root: `src/prototype/`

## 1. Purpose

This document was drafted while the correction implementation was already in
progress. It is retained as the next clean-baseline re-audit procedure, not as
an authority competing with
`2026-08-24T18-24-59-AUTHORITY-AND-SEMANTIC-DUPLICATION-CORRECTION-IMPLEMENTATION-PLAN.md`.
The current change completes and validates that implementation plan first.

This plan defines a fresh, repository-wide review of:

1. semantic facts with more than one mutable authority;
2. immutable projections that are not clearly separated from their authority;
3. repeated algorithms or storage machinery that can be implemented once;
4. concepts that are represented separately even though one more general
   construction can derive them; and
5. superficially similar concepts that must remain separate because they prove
   different statements.

The review produces evidence and a correction plan. It does not modify
implementation code while findings are still being collected.

## 2. Why V2 Is Required

The completed audit at
`2026-08-24T17-57-04-CURRENT-AUTHORITY-AND-SEMANTIC-DUPLICATION-COMPREHENSIVE-AUDIT-PLAN.md`
used commit `14b51b1` and artifact v84 as its baseline. The current repository
has since changed materially:

- Universe closure moved to artifact v85;
- classifier and effect solution ownership is being changed;
- normalization cache validity has changed;
- accepted replay and driver storage call sites have changed; and
- the working tree contains implementation changes that must be audited as
  existing input, not silently rewritten by this review.

The previous report and correction plan are leads, not current truth. Every old
finding must be marked `CONFIRMED`, `RESOLVED`, `PARTIALLY_RESOLVED`,
`SUPERSEDED`, or `UNSUPPORTED` against current code.

## 3. Audit Discipline

During this audit:

- do not fix a finding immediately after discovering it;
- do not modify implementation files;
- do not discard or rewrite pre-existing working-tree changes;
- do not retain duplicate authority merely to reduce migration size;
- do not infer duplication from matching names or struct layouts alone;
- do not merge distinct kernel rules only because their C control flow is
  similar;
- do not use diagnostics, counters, caches, or snapshots as acceptance input;
- do not treat lower LOC as stronger evidence than a clearer authority model;
- do not propose compatibility adapters or remap layers as the final design;
  and
- exclude general QuickSort property-proof design unless it exposes a shared
  authority defect in existing infrastructure.

Implementation begins only after all sections have been reviewed and one
integrated correction plan has been accepted.

## 4. Project Invariants

The review evaluates code against these A Program invariants.

1. Compilation and type checking are bounded earlier computation; runtime is a
   later computation phase, not a second language semantics.
2. `TermDB` owns context-free computation identity.
3. Context, source occurrence, classifier, effect, usage, and proof provenance
   belong above the context-free Term graph.
4. Value/computation polarity must not duplicate the common computation graph
   vocabulary into parallel Value and Computation node families.
5. One semantic fact has one mutable authority.
6. A derived copy must name its authority, construction point, validity key,
   invalidation rule, and disagreement behavior.
7. A cache can be deleted and reconstructed without changing accepted meaning.
8. A Constraint, phase residual, accepted Claim, and diagnostic are distinct
   states and must not become interchangeable records.
9. `::` is post-synthesis expected-type verification, not an elaboration input.
10. Names, namespaces, TypeViews, and labels may distinguish operations but do
    not redefine context-free computation identity.
11. Artifact replay must reconstruct or validate the same evidence required by
    source compilation.
12. Resource-sensitive extensions must be able to expose weakening,
    substitution, and usage evidence without inventing a parallel Context
    model.

## 5. Classification and Proof Standard

Every candidate receives one primary classification.

| Classification | Meaning |
| --- | --- |
| `SEMANTIC_AUTHORITY` | Unique mutable or persistent owner of a fact |
| `DERIVED_PROJECTION` | Immutable view computed from named authority |
| `ALGORITHM_WORKSPACE` | Disposable state local to a solve or traversal |
| `REBUILDABLE_CACHE` | Performance state with a complete validity rule |
| `DIAGNOSTIC_SNAPSHOT` | Reporting-only data that cannot affect acceptance |
| `WIRE_EVIDENCE` | Serialized evidence independently validated on replay |
| `PHASE_RESIDUAL` | Explicit obligation transferred between phases |
| `PRESENTATION_DATA` | Names/readback/source information without kernel authority |
| `DUPLICATE_AUTHORITY` | Multiple mutable owners can answer one semantic question |
| `SEMANTIC_DUPLICATION` | The same rule or construction is implemented independently |
| `PHYSICAL_DUPLICATION` | Repeated C machinery with no duplicated semantic authority |
| `LEGITIMATE_DISTINCTION` | Similar representation, but a different theorem or role |
| `UNSUPPORTED_SUSPICION` | Current code does not support the proposed criticism |

For every candidate, the report must answer:

1. What exact semantic question does it answer?
2. What type and ID identify that fact?
3. Which function creates it?
4. Which functions mutate it?
5. Which functions can accept or reject a program from it?
6. Can the candidate records disagree?
7. Which value wins when they disagree today?
8. Is one reconstructed from another, and is that reconstruction checked?
9. What phase transition freezes it?
10. Is it serialized, published, relocated, or imported?
11. What invalidates it?
12. Can it be deleted without changing accepted Claims or artifact bytes?
13. Would unification expose one invariant or hide several distinct rules?

A `DUPLICATE_AUTHORITY` finding requires at least one concrete disagreement
path or two independent acceptance readers. Similar field names are not enough.

## 6. Required Deliverables

The audit will produce two new documents after review:

1. a comprehensive audit report containing evidence and negative decisions;
2. one integrated correction implementation plan, ordered by dependency and
   furnished with progress checklists, tests, performance gates, artifact
   impact, Issue disposition, and per-file LOC accounting.

The report must contain these ledgers:

### 6.1 Semantic owner ledger

| Fact | Authority | Writers | Acceptance readers | Projections/caches | Persistence | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending | pending | pending |

### 6.2 Duplication ledger

| ID | Candidate | Classification | Concrete evidence | Recommended action |
| --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending |

### 6.3 Physical consolidation ledger

| Mechanism | Implementations | Shared invariant | Rule-specific behavior | Candidate abstraction |
| --- | --- | --- | --- | --- |
| pending | pending | pending | pending | pending |

### 6.4 Negative-decision ledger

| Concepts | Why similar | Required distinction | Reason not to unify |
| --- | --- | --- | --- |
| pending | pending | pending | pending |

## 7. Progress Dashboard

| Phase | Scope | Status | Output |
| --- | --- | --- | --- |
| RA0 | Freeze and describe the current baseline | pending | baseline record |
| RA1 | Term, AST, lowering, and typed occurrences | pending | graph authority matrix |
| RA2 | Constraint generation, solving, and freeze | pending | solution authority ledger |
| RA3 | Context, substitution, binding, and usage | pending | structural evidence ledger |
| RA4 | Type declarations and schema capabilities | pending | schema role matrix |
| RA5 | Proposition, Claim, Derivation, and replay | pending | proof authority matrix |
| RA6 | Evaluation, conversion, profiles, and caches | pending | reduction/cache matrix |
| RA7 | CBPV, effects, host operations, and folds | pending | computation authority matrix |
| RA8 | Universe obligations and closure | pending | Universe evidence matrix |
| RA9 | Identity, parametricity, dimensions, and HOTT | pending | action/freeze matrix |
| RA10 | Indexed families, function graphs, and totality | pending | generated-object matrix |
| RA11 | Artifact, publication, relocation, and import | pending | wire authority matrix |
| RA12 | Namespaces, TypeViews, symbols, and labels | pending | naming-role matrix |
| RA13 | Driver storage, transactions, indexes, and C machinery | pending | physical consolidation ledger |
| RA14 | Tests, diagnostics, performance, and documentation | pending | coverage matrix |
| RA15 | Integrated criticism and correction-plan extraction | pending | report and implementation plan |

## 8. RA0: Current Baseline

- [ ] Record branch, commit, dirty files, artifact version, compiler options,
      and whether each changed file predates this audit.
- [ ] Record implementation, tests, specifications, and documentation LOC
      separately.
- [ ] Run `git diff --check` without changing the worktree.
- [ ] Record the complete integration-suite count and elapsed time.
- [ ] Record focused timings for artifact flow, HOTT, IF8, function graph,
      totality, effects, and Context-resolution tests.
- [ ] Inventory every DB, arena, store, index, cache, snapshot, transaction, and
      persistent ID type.
- [ ] Inventory artifact roots and all serialized tables.
- [ ] Build a writer/reader index using static search and call-site inspection.
- [ ] Map each previous F01-F10 finding to its current status.

Exit gate: the audit baseline is reproducible, and no audit result is confused
with a pre-existing uncommitted implementation change.

## 9. RA1: Term, AST, Lowering, and Typed Occurrences

Primary files:

- `src/prototype/src/core/term.c` and included term modules;
- `src/prototype/src/frontend/ast.c`;
- `src/prototype/src/frontend/lowering.c` and lowering includes;
- `src/prototype/src/graph/typed_occurrence_graph.c`; and
- `src/prototype/src/graph/compile_metadata.c`.

- [ ] Verify that `TermDB` stores only context-free computational structure.
- [ ] Trace every occurrence-level Context, classifier, effect, usage, source,
      and proof reference.
- [ ] Find any payload copied between Term nodes and TypedOccurrences.
- [ ] Audit APP, LAMBDA, MATCH, constructor, operation request, return, thunk,
      force, and computation fold node construction.
- [ ] Verify binding identity is pointer/ID based and independent of surface
      names or positional remapping.
- [ ] Distinguish temporary AST structure from duplicated semantic graph state.
- [ ] Find repeated graph traversals that can share traversal mechanics while
      retaining node-specific kernel rules.
- [ ] Reject proposals for separate Value-side and Computation-side copies of
      the common Term vocabulary unless current code already requires a
      genuinely different identity relation.

Required output: a node-by-node ownership table and a list of safe traversal
abstractions.

## 10. RA2: Constraints, Solvers, and Freeze

Primary files:

- `src/prototype/src/frontend/lowering/constraint/`;
- `src/prototype/src/frontend/lowering/graph_construction.inc`;
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`; and
- classifier/effect/usage fields in graph and metadata headers.

- [ ] Trace classifier facts from seed generation through equations, meta
      solving, evidence, freeze, and TypedOccurrence publication.
- [ ] Perform the same trace for effect rows and usage.
- [ ] Identify every direct write to occurrence classifier/effect/usage fields.
- [ ] Determine whether seeds, provisional values, solutions, evidence, and
      frozen projections use distinct types and stores.
- [ ] Trace all copies between ConstraintDB records, solver-local cells,
      metavariables, computation results, and compile metadata.
- [ ] Verify one O(1) lookup path exists for each authoritative meta/constraint.
- [ ] Check that incompatible solutions fail instead of being merged by generic
      fallback logic.
- [ ] Check that provisional request classifiers cannot union semantically
      distinct parameterized effect atoms.
- [ ] Verify solved state/reason/result fields do not remain in multiple mutable
      owners.
- [ ] Audit transaction/rollback of solver publications.
- [ ] Measure full scans, copied arrays, repeated fixed-point scheduling, and
      index rebuilds.
- [ ] Identify one shared lifecycle API only where classifier, effect, and usage
      domains have the same state-transition theorem.

Required output: a field-level authority table and a single freeze-boundary
proposal if current code still has multiple writers.

## 11. RA3: Context, Substitution, Binding, and Resource Usage

Primary files:

- `src/prototype/src/kernel/context.c`;
- Context/substitution headers;
- `src/prototype/src/kernel/resource_usage.c`; and
- Context-related lowering and identity bridge code.

- [ ] Keep Context objects and Context morphisms/substitutions distinct.
- [ ] Trace weakening evidence to the exact projection substitution it cites.
- [ ] Verify reindex, pullback, composition, and projection derive from stable
      binding identity rather than name or slot remapping.
- [ ] Find copied Context lists and classify them as persistent structure,
      algorithm workspace, or duplicate authority.
- [ ] Compare Context binding classifier references with frozen occurrence
      classifier authority.
- [ ] Compare occurrence usage, solver usage, Context resource declarations,
      accepted usage evidence, and artifact usage records.
- [ ] Audit index construction, invalidation, rollback, and rebuild failure.
- [ ] Establish which APIs must remain explicit for future linear/resource
      sensitive rules.

Required non-unification: weakening is a Judgement rule; projection is a
Context morphism. Shared evidence linkage is required, but the concepts are not
one record.

## 12. RA4: Type Declarations and Schema Capabilities

Primary files:

- `src/prototype/src/kernel/type_declaration.c`;
- `src/prototype/src/kernel/type_schema_view.c`;
- TypeDeclaration headers; and
- type-schema readers in typing, artifact, identity, and lowering modules.

- [ ] Verify graph-level classifier families/telescopes remain semantic
      authority for dependent constructors.
- [ ] Find semantic decisions still made from readback field metadata or cached
      result types.
- [ ] Classify each field as semantic schema, nominal/index data,
      representation identity, readback data, or cache.
- [ ] Verify structural representation keys derive from authoritative schema.
- [ ] Audit constructor field dependency on preceding binders.
- [ ] Inventory broad mutable TypeDeclarationDB parameters and actual fields
      used by each caller.
- [ ] Determine whether capability views can narrow mutation without creating
      duplicate DB ownership.
- [ ] Compare repeated schema traversal in typing, identity, readback,
      publication, and replay.

Required output: a capability matrix and a precise list of broad APIs to split.

## 13. RA5: Proposition, Claim, Derivation, and Accepted Replay

Primary files:

- `src/prototype/src/kernel/judgement.c`;
- `src/prototype/src/kernel/typing/accepted_replay.inc`;
- kernel rule modules; and
- artifact proof publication/readback code.

- [ ] Confirm Proposition identity, accepted Claim, Derivation DAG, candidate
      construction, and replay are distinct roles.
- [ ] Search for accepted-to-candidate premise copying and const removal.
- [ ] Verify immutable storage-neutral premise views cover all replay paths.
- [ ] Audit whether one relation may have multiple derivations without
      overwriting accepted evidence.
- [ ] Confirm proof identity includes Context and proposition, not Term ID alone.
- [ ] Link every weakening derivation to its substitution evidence.
- [ ] Separate generic premise iteration/storage mechanics from theorem-specific
      validation.
- [ ] Audit Judgement transaction rollback and every derived index.
- [ ] Measure repeated proposition lookup and premise-array reconstruction.

Required non-unification: Proposition, Claim, and Derivation answer different
proof-theoretic questions and must not become one generic proof record.

## 14. RA6: Evaluation, Conversion, Reduction Profiles, and Caches

Primary files:

- `src/prototype/src/core/term/evaluation_and_conversion.inc`;
- `src/prototype/src/kernel/typing/conversion.inc`; and
- normalization/cache tests.

- [ ] Assign beta, iota, return/force, computation-fold, pure host operation,
      and stuck effect-request reduction to one implementation each.
- [ ] Find classifier-only shortcuts that duplicate normalizer semantics.
- [ ] Distinguish reduction profiles as explicit policy, not parallel evaluators.
- [ ] Verify cache keys include profile, semantic schema revision, Context or
      substitution where relevant, and intrinsic policy.
- [ ] Verify Core-only cache entries survive unrelated schema changes where
      valid, while schema-dependent entries do not.
- [ ] Audit cache invalidation and semantic revision writers.
- [ ] Keep DefEq, object Identity, and optimizer rewrite evidence separate.
- [ ] Measure repeated WHNF/conversion calls on representative indexed-family
      programs.

Required output: reduction ownership and cache-validity matrices.

## 15. RA7: CBPV, Effects, Host Operations, and Computation Fold

Primary files:

- operation/effect Term formation and evaluation;
- effect constraint propagation;
- computation-fold typing/runtime code; and
- CBPV/effect integration tests.

- [ ] Verify intrinsic namespace membership does not determine whether a symbol
      denotes a type, pure function, machine operation, or effect operation.
- [ ] Keep operation request as explicit free-algebra structure unless a proof
      shows APP/BIND encoding preserves handler inspection.
- [ ] Verify surface operation application lowers uniquely to explicit request.
- [ ] Audit return clauses and multiple operation clauses end to end.
- [ ] Verify sequencing and handling share computation-fold machinery without
      erasing their different typing obligations.
- [ ] Treat higher-order operation atoms as opaque atoms whose latent effect row
      is preserved, not flattened into the surrounding row.
- [ ] Trace effect-row equations, meta solutions, residual obligations,
      diagnostics, and frozen projections.
- [ ] Verify diagnostic snapshots have no acceptance or residual-generation
      readers.
- [ ] Audit THUNK/FORCE and implicit surface coercions for a unique raw-CBPV
      lowering.

Required output: an effect-fact ownership table and fold abstraction boundary.

## 16. RA8: Universe Obligations and Closure

Primary files:

- `src/prototype/src/kernel/universe.c`;
- Universe formation rules;
- artifact v85 schema/wire/interface code; and
- Universe tests.

- [ ] Separate universe variable identity, inequalities, solver state,
      closure/certificate, accepted Claims, and wire evidence.
- [ ] Verify source compilation and replay reconstruct the same obligations.
- [ ] Verify artifact acceptance never trusts a serialized solved flag or
      unchecked numerical assignment.
- [ ] Verify every Universe-dependent published root is covered by closure
      evidence.
- [ ] Keep DefEq of levels separate from cumulativity evidence.
- [ ] Audit Pi formation/max policy against the current A Program Universe
      model.
- [ ] Search for obsolete v84 paths and compatibility fallbacks.

Required output: a source/replay evidence correspondence table and the current
status of previous findings F01/F02.

## 17. RA9: Identity, Parametricity, Dimensions, and HOTT Actions

Primary files:

- `src/prototype/src/identity/`;
- `src/prototype/src/parametricity/`;
- `src/prototype/src/dimension/`; and
- HOTT artifact/test support.

- [ ] Keep logical relation action distinct from object Identity inhabitance.
- [ ] Identify shared endpoint, Context, telescope, and node traversal.
- [ ] Verify shared traversal does not make relation preservation equivalent to
      Identity proof.
- [ ] Trace semantic epoch/freeze capture for cached actions.
- [ ] Verify action reuse cannot cross mutable schema, Context, substitution,
      or accepted-proof boundaries.
- [ ] Distinguish one-dimensional evidence, higher-dimensional evidence, and
      unsupported coherence.
- [ ] Audit constructor/Lambda/APP/Match action duplication for reusable
      traversal only.
- [ ] Verify artifact roots preserve object evidence, not search history.

Required output: a relation/Identity/action/certificate matrix and explicit
freeze invariant.

## 18. RA10: Indexed Families, Function Graphs, and Totality

Primary files:

- indexed-family lowering and kernel rules;
- function graph generation/certification;
- totality evidence; and
- IF8/function-graph tests.

- [ ] Distinguish source function, generated graph IADT, association metadata,
      totality witness, decrease evidence, and execution certificate.
- [ ] Verify association metadata cannot itself establish functionality or
      totality.
- [ ] Find repeated generated schema construction and validation.
- [ ] Compare first-order and dependent/indexed generation paths.
- [ ] Trace Context resolution, substitution lookup, normalization, and
      accepted-proof lookup counts.
- [ ] Identify reusable schema-building mechanics without merging theorems.
- [ ] Keep future general function-property proofs outside correction scope.

Required output: generated-object ownership matrix and performance hotspots.

## 19. RA11: Artifact, Interface, Publication, Relocation, and Import

Primary files:

- `src/prototype/src/artifact/`;
- `src/prototype/spec/artifact_v85.schema`; and
- artifact integration/mutation tests.

- [ ] Assign every wire field to in-memory authority, validated evidence,
      presentation data, or removable cache.
- [ ] Distinguish byte/bounds validation, semantic interface validation,
      relocation, replay, and link validation.
- [ ] Verify repeated validators have different explicit contracts or share one
      safe primitive.
- [ ] Verify sparse referenced slots require presence, not only range.
- [ ] Verify solver workspaces, caches, diagnostics, fuel, and queues are not
      serialized as authority.
- [ ] Trace conditional dependencies for residual obligations.
- [ ] Search for remap/fallback/legacy paths that reconstruct dependent schema.
- [ ] Verify read-publish-read determinism.

Required output: complete wire authority table and artifact-version impact for
every proposed correction.

## 20. RA12: Namespaces, TypeViews, Symbols, and Labels

- [ ] Classify each symbol-bearing ID as qualified identity, import/export
      identity, operation label, binder display name, or presentation data.
- [ ] Verify external references retain namespace/import identity.
- [ ] Verify structurally identical core graphs can carry distinct Bool/Two or
      similarly named TypeViews without becoming DefEq automatically.
- [ ] Verify aliases and display labels cannot affect Term interning or
      conversion.
- [ ] Find flattened-name and remap workarounds that should be replaced by
      stable identity.
- [ ] Verify operation labels used by handlers are semantic identities, not
      merely pretty-printed names.

Required output: naming-role table and rejected name-based authorities.

## 21. RA13: Driver Storage, Transactions, Indexes, and Common C Machinery

Primary files:

- `src/prototype/src/driver/`;
- `src/prototype/src/support/storage.c`;
- storage/index code in every DB; and
- transaction APIs.

- [ ] Inventory repeated reserve, append, intern, lookup, hash, rebuild,
      rollback, initialization, and cleanup code.
- [ ] Verify each proposed shared helper has identical ownership, allocation,
      rollback, and error semantics.
- [ ] Audit driver program/provider/imported/artifact storage for parallel
      backing arrays.
- [ ] Verify two compiler/reader sessions can coexist without static mutable
      role storage.
- [ ] Trace every ignored or inconsistently handled index rebuild failure.
- [ ] Identify broad mutable DB pointers that can become narrow capabilities.
- [ ] Prefer typed storage helpers over an untyped universal arena.
- [ ] Record large physical modules that can be split without changing
      semantics.

Required output: physical consolidation plan with exact call sites and expected
per-file LOC effects.

## 22. RA14: Tests, Diagnostics, Performance, and Documentation

- [ ] Classify tests as semantic regression, authority disagreement, artifact
      mutation, performance, or source-layout/static audit.
- [ ] Verify each confirmed authority defect has a permanent boundary test.
- [ ] Ensure independent writer/reader tests do not share the implementation
      helper whose correctness they are testing.
- [ ] Verify all timed tests follow one reporting convention.
- [ ] Verify counters observe algorithms and never change acceptance.
- [ ] Search diagnostics for semantic readers.
- [ ] Record stale artifact versions and architecture descriptions.
- [ ] Mark superseded plans rather than rewriting their historical decisions.
- [ ] Establish five-run median and rejection thresholds for corrections that
      touch fixed-point or replay hot paths.

Required output: test-to-invariant matrix and documentation correction list.

## 23. RA15: Integrated Criticism

Only after RA0-RA14 are complete:

- [ ] Merge all semantic owner rows and resolve contradictory classifications.
- [ ] Group findings by root authority defect rather than source file.
- [ ] Separate semantic correction from physical source reorganization.
- [ ] Record every tempting but rejected unification.
- [ ] Order accepted changes by dependency and artifact impact.
- [ ] Assign priorities:
  - `P0`: two mutable authorities can alter accepted meaning;
  - `P1`: persistence/replay or phase transition can lose or invent evidence;
  - `P2`: broad capability or repeated semantic algorithm threatens future
    correctness;
  - `P3`: physical duplication, diagnostics, or documentation only.
- [ ] Produce one correction plan with one work package per root defect.
- [ ] Give every package migration steps, deletion targets, tests, performance
      gates, rollback strategy, artifact version effect, Issue disposition, and
      per-file LOC ledger.
- [ ] Do not implement until the integrated plan has been reviewed.

## 24. Initial Required Non-Unifications

These are audit defaults, not conclusions immune to evidence.

| Concepts | Default decision |
| --- | --- |
| Term and TypedOccurrence | Keep separate: context-free identity versus contextual occurrence |
| Value and Computation Term vocabularies | Keep one graph vocabulary; distinguish by typing/polarity |
| Context and Substitution | Keep separate: object versus morphism |
| Weakening and projection | Link evidence; do not collapse rule and morphism |
| Proposition, Claim, and Derivation | Keep separate proof-theoretic roles |
| Constraint and accepted Claim | Keep separate phase meanings |
| DefEq and object Identity | Never merge or reflect automatically |
| Parametricity relation and Identity | Share traversal only where certificates remain distinct |
| APP, MATCH, IH, and computation-fold elimination rules | Keep theorem-specific rules visible |
| Constructor telescope and Pi | Reuse binding machinery where valid; do not identify data schema with computation |
| Effect request and APP | Keep request observable to handlers |
| Computation fold sequencing and handling | Share fold machinery; retain distinct clauses/typing obligations |
| Source validation and artifact replay | Share immutable views/primitives; retain independent trust boundaries |
| Schema, representation, readback, and cache | Separate capabilities and authority roles |

## 25. Completion Gate

The audit is complete only when:

- [ ] every mutable semantic store has a named owner and writer set;
- [ ] every derived copy has a source and validity rule;
- [ ] every suspected duplicate has a classified verdict with code evidence;
- [ ] every negative unification decision has been recorded;
- [ ] source compilation and artifact replay boundaries have both been covered;
- [ ] fixed-point and performance consequences have been measured;
- [ ] previous audit findings have current statuses;
- [ ] no implementation change was mixed into the audit;
- [ ] the integrated report identifies root causes rather than merely counting
      structs, tags, or lines; and
- [ ] the resulting correction plan can be executed and tracked independently.
