# Open Issues 11, 13, 14, 16, 17, and 18 Integrated Authority Implementation Plan

Date: 2026-08-19

Status: Authority and indexed-family implementation complete; first
Returns/Terminates fragment complete; general #13/#14 proof obligations remain

Baseline commit: `54d33d9a13841c8e48c685d3da8022d4af268420`

Current artifact format: v80

## 1. Purpose

This plan resolves the currently open repository Issues without adding another
parallel source of semantic truth.

The implementation order is intentionally:

1. consolidate mutable and accepted authority boundaries;
2. finish the general indexed-family semantic boundary;
3. define computation-result and totality evidence;
4. persist only the final accepted representations.

The order is based on the current code, not only on Issue numbering. IADT work
depends on TypeDeclaration and constraint-solver authority. Computation-result
Claims depend on a prior distinction between effects, successful results,
termination, divergence, and residual compile-time knowledge.

## 2. Issue Set

| Issue | Current conclusion | Planned close gate |
|---|---|---|
| [#11](https://github.com/repyt-margorp/a-program/issues/11) | implemented: exact head/tail/map/append, multi-index families, indexed IH, Acc, and fuel-free QuickSort survive source/artifact/replay/runtime checks | close after final GitHub audit |
| [#13](https://github.com/repyt-margorp/a-program/issues/13) | partial: object-level `#.Returns`, closed evaluation evidence, dependent consumers, and v80 replay exist | remains open until the open QuickSort sortedness theorem is constructed rather than assumed |
| [#14](https://github.com/repyt-margorp/a-program/issues/14) | partial: `#.Terminates` is derived from accepted Returns evidence in the deterministic finite fragment | remains open for general open totality, partiality/divergence, effectful postconditions, and UNKNOWN semantics |
| [#16](https://github.com/repyt-margorp/a-program/issues/16) | implemented: permanent semantic matrix, concrete Box/Perfect/Matrix, constant/residual split, and corruption rejection | close after final GitHub audit |
| [#17](https://github.com/repyt-margorp/a-program/issues/17) | implemented: immutable replay views, narrow Type stores, cache-independence checks, and responsibility-based solver split | close after final GitHub audit |
| [#18](https://github.com/repyt-margorp/a-program/issues/18) | implemented: equation lifecycle, result payload, effect substitution, occurrence solution, diagnostic snapshot, and frozen projection have distinct owners | close after final GitHub audit |

No Issue is closed merely because a nearby example compiles. Each Issue has an
independent permanent close gate in section 13.

## 3. Verified Current State

### 3.1 Existing successful foundation

The following work is already present and must not be repeated:

- TermDB exact/alpha interning and normalization indexes;
- one initial global source constraint generation pass;
- immutable Context and Substitution rebasing;
- zero ordinary source-compilation Context/Substitution index rebuilds;
- explicit `@\` index telescope parsing and lowering;
- Vec, Fin, Acc, dependent Match refinement, recursive IH, and IF8 tests;
- accepted Claim/Derivation DAG publication and artifact replay;
- runtime-only VerificationDB obligations for residual computation-fold result
  checks and effect-row equations.

The previous complete run reported 39/39 integration tests passing. This is a
historical baseline and must be rerun before the first implementation change.

### 3.2 Confirmed remaining authority debt

The current source sizes include:

| File | Lines at baseline |
|---|---:|
| `frontend/lowering/constraint_solver.inc` | 12,386 |
| `kernel/typing/accepted_replay.inc` | 7,421 |
| `kernel/type_declaration.c` | 2,487 |
| `kernel/type_declaration.h` | 469 |

Size alone is not a defect. The relevant findings are:

1. `prototype_judgement_rule_application_view` points at
   `prototype_judgement_candidate_premise`, so accepted replay reconstructs a
   candidate-shaped stack array and casts resolved Propositions away from
   `const`.
2. TypeDeclarationDB physically combines semantic schema, readback,
   representation identity/indexing, and constructor classifier cache.
3. `operation_constraint_db` owns constraint lifecycle while
   `operation_solver_solution[]` also stores classifier/effect states and
   reasons copied from constraints.
4. computation solving copies one invocation-local `solved_classifier` into
   `ConstraintDB.result_term` before publishing it.
5. effect equations are copied into `compile_metadata.effect_constraints[]`;
   that snapshot is then read to count unresolved work and generate
   VerificationDB obligations, so it is not currently diagnostic-only.
6. TypedOccurrence classifier fields are mostly checked final projections, but
   all pre-freeze reads still need an audit.

### 3.3 Context/Substitution rebuild status

The earlier report of 31 Context and Substitution index rebuilds is obsolete for
ordinary source compilation. The current permanent tests require:

```text
context_index_rebuilds=0
substitution_index_rebuilds=0
```

Bulk artifact readback, relocation, and dense publication may rebuild a runtime
index once after direct population. The new work must preserve this distinction.

## 4. Normative Authority Model

The implementation must converge on this table.

| Semantic fact | Single authority | Non-authoritative forms |
|---|---|---|
| Core term structure | TermDB | normalized/reindexed Terms are ordinary interned results |
| source typed occurrence identity | TypedOccurrenceGraph | Core Term ID alone |
| local Context object | ContextDB | resolver worklist entries |
| Context morphism | SubstitutionDB | reindex caches |
| mutable classifier/effect equations | OperationConstraintDB | counters and diagnostic snapshots |
| per-occurrence solved classifier/effect row | OperationSolutionTable | frozen TypedOccurrence projection |
| effect metavariable substitution | OperationEffectMetaDB | materialized row Term |
| computation rule input | immutable JudgementDelta payload | ConstraintDB lifecycle record |
| computation rule output | one invocation-local result arena | no copied `result_term` |
| solver scheduling/lifecycle | OperationConstraintDB | aggregate status view |
| accepted typing evidence | Judgement Proposition/Claim/Derivation | candidate state and replay adapters |
| type/constructor meaning | TypeSemanticSchemaDB plus ContextDB/TermDB | readback and materialization caches |
| source reconstruction | TypeReadbackDB | never a kernel premise |
| cross-artifact representation identity | TypeRepresentationTable | hash index/fingerprint lookup cache |
| residual runtime contract | VerificationDB | solver residual counters |
| object-level result/totality proof | ordinary Term plus accepted Claim/Derivation | VerificationDB obligation |

The word "single" does not require one physical C structure for all roles.
Immutable input, mutable lifecycle, and output may be separate records. It does
require that the solved fact itself is stored once and referenced by checked ID.

## 5. Non-Negotiable Invariants

- `term :: T` remains a post-synthesis expectation check. It never supplies a
  classifier to synthesis.
- Candidate state is solver-local. Accepted replay never depends on candidate
  storage identity.
- A cache, readback record, diagnostic snapshot, or TypedOccurrence projection
  cannot rescue missing semantic evidence.
- Effects and termination are orthogonal. An empty effect row does not prove
  termination; an effectful computation may terminate.
- `UNKNOWN` or exhausted compile budget is not evidence of divergence.
- `#.Returns (&M) v` does not make `M` or `v` definitionally equal to another
  Term and does not add an equation to global conversion.
- No Equality, observation, or result proof is reflected into DefEq.
- APP, Match, IH, computation fold, and effect rules remain explicit kernel
  rules even if they share storage plumbing.
- No old/new compatibility facade is retained to reduce migration size.
- Ordinary lowering performs no full Context/Substitution index rebuild.
- Each phase preserves IF8 single-compile performance within normal measurement
  noise, unless a measured and approved tradeoff is recorded.

## 6. Dependency Order

```text
OA0 authority inventory
  -> OA1 mutable solver authority (#18)
  -> OA2 accepted replay premise view (#17)
  -> OA3 Type semantic schema boundary (#17)
  -> OA4 constraint module decomposition (#17, #18)
  -> IF0 IADT conformance harness (#16)
  -> IF1 constructor specialization (#16)
  -> IF2 Match/refinement/positivity boundary (#16)
  -> IF3 remaining general indexed-family matrix (#11)
  -> RT0 result/totality normative contract (#14 prerequisite)
  -> RT1 object Returns relation (#13)
  -> RT2 post-hoc result Claims and certified QuickSort (#13)
  -> RT3 totality/partiality evidence and replay (#14)
  -> CL final artifact, performance, and Issue closure audit
```

OA1, OA2, and OA3 may be developed on separate commits, but OA4 starts only
after all three interfaces are stable. IF and RT work must not start on top of
parallel mutable authority paths.

## 7. OA: Authority Consolidation

### OA0. Freeze the baseline and write the authority matrix

Primary inputs:

- `src/prototype/include/a_program/kernel/judgement/types.h`
- `src/prototype/src/kernel/typing/accepted_replay.inc`
- `src/prototype/include/a_program/kernel/type_declaration.h`
- `src/prototype/src/kernel/type_declaration.c`
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`
- `src/prototype/src/frontend/lowering/constraint_solver.inc`
- `src/prototype/include/a_program/graph/compile_metadata.h`
- `src/prototype/include/a_program/graph/verification.h`

Tasks:

- [ ] Run all integration tests and the IF8 benchmark at the baseline commit.
- [ ] Record every read and write of classifier/effect state, reason,
      `result_term`, computation result, effect snapshot, and TypedOccurrence
      classifier.
- [ ] Classify each field as authority, immutable input, output, lifecycle,
      checked projection, cache, diagnostic, or residual obligation.
- [ ] Add compile-time or C/API checks for the intended const boundaries before
      deleting fields.
- [ ] Record artifact v78 sections that currently serialize readback,
      representation, or constructor classifier materialization.

Exit criteria:

- [ ] Every field in Issues #17 and #18 has one documented owner.
- [ ] No deletion is justified only by similar spelling or line count.
- [ ] Baseline performance and artifact hashes are reproducible.

### OA1. Consolidate mutable solver state

#### OA1.1 Classifier and per-occurrence status

Keep two distinct concepts:

```text
ConstraintDB:
    each equation's state, reason, dependencies, evidence

OperationSolutionTable:
    one solved classifier and effect row per occurrence
```

Tasks:

- [ ] Remove writable `classifier_state`, `effect_state`, and generic `reason`
      from the solution cell unless a field is proven to be an independent
      semantic analysis result.
- [ ] Derive aggregate occurrence status from indexed dependent constraints at
      freeze/diagnostic time.
- [ ] Keep O(1) lookup of the solved classifier/effect row.
- [ ] Treat quantitative usage as its own analysis with its own authority; do
      not force it into the classifier/effect lifecycle lattice.
- [ ] Make stale/provisional solution replacement revision-checked.

#### OA1.2 Computation constraints

Retain the legitimate separation:

```text
JudgementDelta computation payload   immutable kernel input
ComputationResultArena               invocation-local kernel output
ConstraintDB                         scheduling and lifecycle
```

Tasks:

- [ ] Remove `operation_constraint.result_term` for computation constraints.
- [ ] Store a checked result ID/revision if lifecycle needs to reference an
      output; never copy the solved classifier.
- [ ] Publish directly from the sole result payload after premise-snapshot
      freshness validation.
- [ ] Reset or replace output slots explicitly between kernel invocations.
- [ ] Add corruption tests for wrong delta ID, stale result revision, wrong
      occurrence, and mismatched premise snapshot.

#### OA1.3 Effect rows and residual obligations

Tasks:

- [ ] Keep effect metavariable substitution in `OperationEffectMetaDB`.
- [ ] Keep effect equation state in ConstraintDB.
- [ ] Keep only the final solved effect row in the occurrence solution table.
- [ ] Generate VerificationDB residual obligations directly from the
      authoritative ConstraintDB freeze.
- [ ] Stop using `compile_metadata.effect_constraints[]` as an input to
      unresolved counts or obligation generation.
- [ ] Remove that snapshot if no external diagnostic consumer remains;
      otherwise freeze it after all semantic decisions and rename it as an
      explicit diagnostic projection.

#### OA1.4 TypedOccurrence projection

Tasks:

- [ ] Audit every pre-freeze read of `occurrence.classifier`.
- [ ] Require mutable solving to call the solution-table API.
- [ ] Freeze TypedOccurrence classifiers once after solver completion.
- [ ] Validate frozen occurrence fields against accepted evidence before
      publication.
- [ ] Add a test that poisons an unfrozen projection and proves solving does not
      consume it.

OA1 exit criteria:

- [ ] No solved Term is copied between mutable authorities.
- [ ] No synchronization pass exists solely to keep duplicate states equal.
- [ ] Constraint and solution lookup remains indexed.
- [ ] Issue #18 acceptance criteria have permanent tests.

### OA2. Replace candidate-shaped accepted replay

Introduce a storage-neutral immutable premise view:

```c
struct prototype_judgement_premise_view {
	const struct prototype_judgement_proposition* proposition;
	int semantic_action_kind;
	uint32_t semantic_action_id;
};
```

The exact name may change. It must not carry candidate store kind, candidate
Proposition ID, or a mutable Proposition pointer.

Tasks:

- [ ] Change `prototype_judgement_rule_application_view` to use immutable
      premise views.
- [ ] Add a candidate adapter that resolves candidate IDs into caller-owned
      immutable views.
- [ ] Add an accepted adapter that resolves Claim/scoped Proposition edges into
      the same view.
- [ ] Remove accepted replay construction of
      `prototype_judgement_candidate_premise[]`.
- [ ] Make candidate premise resolution pointers `const`; separate mutable
      builder storage from validator input.
- [ ] Remove all replay-only casts that discard Proposition constness.
- [ ] Keep rule validators explicit and storage-neutral.
- [ ] Add a test that clears/poisons candidate arenas after accepted graph
      freeze and still replays accepted evidence.

Exit criteria:

- [ ] Accepted replay reads only Proposition/Claim/Derivation and immutable
      semantic stores.
- [ ] Candidate allocation counters do not change during accepted replay.
- [ ] Malformed accepted premise graphs remain rejected.

### OA3. Narrow TypeDeclaration authority

Restructure the owner without retaining top-level aliases:

```text
TypeDeclarationDB owner
  schema                 semantic authority
  readback               source reconstruction only
  representations.table  persistent link identity
  representations.index  rebuildable lookup
  classifier_cache       rebuildable materialization
```

Tasks:

- [ ] Move semantic revision, type declarations, and constructor declarations
      into an explicit `TypeSemanticSchemaDB` subobject.
- [ ] Change kernel and solver queries to accept `const TypeSemanticSchemaDB*`.
- [ ] Pass readback only to parser/lowering diagnostics, interface rendering,
      and source reconstruction.
- [ ] Split persistent representation table identity from its hash/index cache.
- [ ] Pass classifier cache explicitly only to materialization code.
- [ ] Remove representation-count checks from semantic schema queries unless
      the representation table is an explicit premise of that query.
- [ ] Derive curried constructor classifiers from Context paths and
      `result_classifier`; never validate fields from readback metadata.
- [ ] Add cache-clearing tests for source compile, artifact replay, linking, and
      dimension-action schema views.

Artifact decision:

- [ ] Introduce artifact v79 if v78 optional constructor-cache payload cannot
      be removed without changing the grammar.
- [ ] Archive v78 and update current fixtures/readers/writers together.
- [ ] Do not keep a v78 semantic fallback path in the current reader.
- [ ] Persist schema, required readback/interface data, and representation
      identity explicitly; do not persist runtime lookup indexes.

Exit criteria:

- [ ] Kernel compilation cannot access readback/cache fields through its API
      type.
- [ ] Clearing all rebuildable Type caches preserves accepted Claim replay.
- [ ] Artifact determinism and link identity tests pass.

### OA4. Split the constraint solver by authority

Physical decomposition starts only after OA1-OA3.

Recommended modules:

```text
frontend/lowering/constraint/
  model_and_index.inc
  generation.inc
  classifier_propagation.inc
  computation_propagation.inc
  effect_propagation.inc
  branch_refinement.inc
  context_projection.inc
  fixed_point.inc
  diagnostics_and_freeze.inc
```

Stable storage/index code that no longer needs the private `compile_context`
should move into a normal `.c` file with a declarative header. Orchestration
that directly needs `compile_context` may remain private `.inc` code.

Tasks:

- [ ] Extract by the authority table, not by arbitrary line ranges.
- [ ] Keep one definition for state transition and rollback.
- [ ] Keep one dependency index and one worklist implementation.
- [ ] Remove duplicate projection/synchronization helpers made dead by OA1.
- [ ] Keep Context resolution and bulk rebuild APIs physically distinct.
- [ ] Preserve diagnostics and source spans byte-for-byte where behavior is not
      intentionally changed.
- [ ] Record line changes for every extracted and deleted file.

Exit criteria:

- [ ] #17 and #18 checks pass.
- [ ] `constraint_solver.inc` becomes orchestration rather than the owner of all
      models and algorithms.
- [ ] No new catch-all utility module or compatibility wrapper remains.
- [ ] IF8 algorithmic counters and median compile time do not regress.

## 8. IF: General Indexed Families

### IF0. Build a semantic conformance matrix

Replace grep-only confidence with source, C/API, artifact, replay, and runtime
checks where applicable.

Required rows:

- [ ] parameter-only List;
- [ ] Vec and Fin with one index;
- [ ] a family with two indices such as Matrix;
- [ ] a later index whose classifier depends on an earlier index;
- [ ] Diagonal/index-equality family;
- [ ] Box with a type-valued index and concrete `pack` construction;
- [ ] Perfect with concrete leaf/node construction;
- [ ] Acc with parameter and relation specialization;
- [ ] constant-motive neutral Match;
- [ ] equality-dependent neutral Match;
- [ ] nested-positive family classified as valid-but-unsupported if still not
      implemented;
- [ ] genuinely negative recursion rejected;
- [ ] malformed `@\` syntax and parameter-after-index rejected;
- [ ] self declaration names inside constructor result syntax rejected.

Each row records:

```text
formation | concrete construction | synthesis | post-hoc :: | elimination
IH | artifact | replay | runtime | expected support classification
```

### IF1. Unify constructor specialization

The semantic source is:

```text
type parameter Context
  -> type index Context
  -> constructor field Context
  -> constructor result classifier in field Context
```

Tasks:

- [ ] Introduce one checked constructor-schema specialization API used by
      lowering, classifier solving, kernel formation, replay, and schema views.
- [ ] Specialize uniform parameters before constructor-local fields.
- [ ] Check index result arguments only after all field binders are in scope.
- [ ] Preserve distinct parameter, index, and field binder identities.
- [ ] Fix Box and Perfect concrete constructor failures.
- [ ] Ensure diagnostics retain valid expected/actual Term IDs, owner view,
      constructor ordinal, field ordinal, and Context.
- [ ] Delete local ad hoc specializers replaced by the shared semantic API.

### IF2. Match refinement and positivity boundary

Tasks:

- [ ] Permit a constant motive when no unresolved index equation is needed.
- [ ] Keep equality-dependent neutral motives residual until explicit equality
      evidence is available.
- [ ] Distinguish impossible branch, residual equation, unsupported nested
      positivity, and invalid negative recursion.
- [ ] Ensure branch substitutions are exact Context morphisms and remain
      immutable.
- [ ] Verify recursive IH receives the constructor-specialized index, not a
      guessed expected type.
- [ ] Preserve `::` as a later check.

### IF3. Complete #11 after #16

Tasks:

- [ ] Add constructor application, head, tail, map, and append coverage for
      indexed families.
- [ ] Add multiple-index construction and elimination.
- [ ] Add exact indexed IH and Acc/well-founded recursion coverage.
- [ ] Retain IF8 fuel-free QuickSort source, artifact, replay, runtime, decrease,
      and transport tests.
- [ ] Compare #11's original acceptance list with the final matrix line by line.

Close order:

- [ ] Close #16 when the audit matrix is complete and every row has a stable
      supported/unsupported/invalid classification with permanent tests.
- [ ] Close #11 only when its original general indexed-family acceptance list is
      complete, or explicitly split any remaining independent feature into a
      new Issue before closing.

## 9. RT: Computation Results and Totality

### RT0. Fix the terminology and proof contract first

Before adding a wire tag or surface form, specify:

```text
Returns (&M) v
    a finite accepted evaluation derivation returns value v

MayTerminate (&M)
    at least one accepted execution path reaches an outcome

MustTerminate (&M)
    every operational branch reaches an accepted finite outcome

PureTotal (&M)
    MustTerminate (&M) plus an empty effect row

UNKNOWN
    solver did not establish a proposition within its budget
```

For the current deterministic fragment, `Returns (&M) v` establishes a finite
successful execution of that closed computation. The names must still remain
ready for nondeterministic operations, where may- and must-termination differ.

Tasks:

- [ ] Define successful return, handled operation, unhandled operation,
      exception-like outcome, abort, and nondeterministic branch status.
- [ ] Define which propositions are object types and which statuses are only
      solver results.
- [ ] State that effect rows are not termination evidence.
- [ ] State that one closed run is not a theorem about an open function.
- [ ] State that budget exhaustion produces residual/UNKNOWN, never a forged
      negative proof.
- [ ] Specify partial correctness separately from total correctness.
- [ ] Specify least-fixed partial recursion separately from greatest-fixed
      coinduction.

### RT1. Introduce object-level `#.Returns`

Recommended CBPV-correct surface type:

```text
#.Returns (&M) v
```

where `M : Comp(E, A)` and `v : A`. `&M` is a value naming the computation; the
type checker does not execute an effectful computation merely to form the type.

Implementation direction:

- `#.Returns` is an intrinsic semantic relation because it refers to the
  operational semantics of Core computations, which are not reflected as an
  ordinary source ADT.
- It is represented as an ordinary TermDB application spine and may appear in
  Pi domains and other object types.
- Its witnesses are ordinary Terms with accepted HAS_TYPE Claims and explicit
  Derivations.
- Users cannot forge a witness merely by naming a constructor. Each witness
  rule replays the corresponding operational rule.

Required witness rules for the first fragment:

- [ ] `RETURN(v)` returns `v`;
- [ ] pure APP/Lambda beta steps preserve Returns;
- [ ] constructor/Match iota steps preserve Returns;
- [ ] zero-clause computation sequencing composes Returns evidence;
- [ ] handled computation fold composes computation, return-clause, and
      operation-clause result evidence;
- [ ] unresolved requests produce no closed Returns witness;
- [ ] normalization budget exhaustion produces no witness and no negative fact.

The implementation must not use the existing VerificationDB obligation as the
object proof. VerificationDB records a conditional runtime contract; JudgementDB
records accepted object evidence.

### RT2. Resolve post-hoc dependent Claims (#13)

Tasks:

- [ ] Reproduce the exact closed `choose`/`Sorted.nilSorted` Issue case as a
      permanent negative-before/positive-after test.
- [ ] Let a later theorem or expectation consume explicit
      `r : #.Returns (&choose) v` evidence.
- [ ] Keep the original computation Term unchanged; attach a proof graph rather
      than synthesizing a new classifier from `::`.
- [ ] Add a dependent sequencing rule whose result family is instantiated by
      the value named in Returns evidence.
- [ ] Add an open QuickSort theorem taking explicit result evidence and proving
      the result-indexed property.
- [ ] Add negative tests for forged result witnesses, wrong values, unhandled
      effects, and exhausted normalization.
- [ ] Persist Returns type/witness Claims and Derivations in the next artifact
      format and replay them without candidate state.

#13 closes only after both the exact closed repro and the open certified
QuickSort result theorem pass source, artifact, and replay checks.

### RT3. Implement the totality/partiality axis (#14)

Recommended ownership:

- object propositions and witnesses live in TermDB plus JudgementDB;
- structural recursion and Acc recursion generate totality Derivations;
- solver status `UNKNOWN` remains compiler-local/residual;
- a future unrestricted recursion former defaults to unknown or partial, never
  total merely because its effect row is empty;
- VerificationDB carries runtime obligations, not proofs of open termination.

Tasks:

- [ ] Add structural-recursion totality evidence without changing current total
      programs or requiring a `Div` wrapper.
- [ ] Add Acc-driven open-function totality evidence.
- [ ] Define may-diverge/partial computation classifiers before adding general
      recursion.
- [ ] Prohibit partial computation unfolding in DefEq.
- [ ] Permit terminating effectful computations to have postconditions without
      running effects during conversion.
- [ ] Add artifact roots and replay for accepted termination evidence.
- [ ] Add positive tests for closed pure Match, Acc QuickSort, terminating
      effectful computation, and replay.
- [ ] Add negative tests for empty-effect loops, budget exhaustion, one closed
      run used as open totality, forged evidence, and partial-as-total claims.

#14 closes only after its normative, positive, negative, compatibility, and
artifact criteria all pass.

## 10. Artifact Strategy

Artifact versions change only at semantic boundaries:

1. v79 may remove Type cache authority and serialize the narrowed schema after
   OA3.
2. the next version after v79 adds Returns/totality Term and Derivation grammar
   after RT rules are stable.

For each bump:

- [ ] archive the previous schema;
- [ ] update enum consistency checks;
- [ ] update reader, writer, relocation, dense publication, link, and replay;
- [ ] reject absent/hole references and malformed proof edges;
- [ ] regenerate test artifacts from source;
- [ ] do not maintain a current compatibility reader for the old semantic
      representation;
- [ ] verify deterministic bytes from two fresh compilations.

## 11. Test and Performance Plan

### Focused tests after every commit

- `test_constraint_authority.sh`
- `test_context_resolution_incremental.sh`
- `test_context_substitution_immutability.sh`
- `test_p0_certificate_boundary.sh`
- `test_artifact_flow.sh`
- `test_explicit_index_family_surface.sh`
- `test_dependent_match_refinement.sh`
- `test_if8_fuel_free_quicksort.sh`
- `test_issue_boundary_audit_manifest.sh`

Add dedicated tests for:

- [ ] accepted replay independent of candidate storage;
- [ ] Type cache/readback clearing;
- [ ] mutable solver authority corruption;
- [ ] IADT conformance matrix;
- [ ] Returns and totality evidence;
- [ ] artifact result/termination replay.

### Full gates

- [ ] all integration tests pass;
- [ ] examples 01-09 continue to pass;
- [ ] artifact publication/readback/link/determinism pass;
- [ ] accepted replay rejects malformed graphs;
- [ ] IF8 median single compile remains below 10 seconds on the current machine;
- [ ] constraint generation remains one global pass;
- [ ] ordinary Context/Substitution rebuild counters remain zero;
- [ ] no new whole-store scan appears in the IF8 profile;
- [ ] peak memory and artifact size changes are recorded.

## 12. Change Accounting

At the end of every phase, record:

```text
git diff --numstat <phase-base>..<phase-head>
```

The report must separate:

- implementation added/deleted/net lines;
- permanent test added/deleted/net lines;
- documentation added/deleted/net lines;
- generated files, which are excluded from hand-written source totals.

The final report must identify:

- fields and duplicate synchronization paths deleted;
- modules moved without semantic change;
- new semantic rules added;
- any file whose line count increased despite consolidation, with the reason;
- benchmark and artifact-size deltas.

Line reduction is desirable but subordinate to explicit authority. A phase that
adds a new projection while retaining the old writable owner is not complete.

## 13. Issue Closure Gates

### #18

- [x] OA0 and OA1 complete.
- [x] OA4 physical boundaries reflect the final ownership.
- [x] no duplicate writable classifier/computation/effect state remains.
- [x] performance and corruption tests pass.

### #17

- [x] OA0, OA2, OA3, and OA4 complete.
- [x] accepted replay is candidate-independent and const-correct.
- [x] semantic Type consumers cannot access readback/cache authority.
- [x] Context/Substitution zero-rebuild regression remains green.

### #16

- [x] IF0-IF2 complete.
- [x] Box and Perfect concrete construction pass.
- [x] constant/residual/unsupported/invalid cases have distinct diagnostics.
- [x] conformance matrix is permanent and included in the Issue manifest.

### #11

- [x] IF3 complete after #16.
- [x] every original acceptance item is either implemented or explicitly split
      into an independently justified Issue.
- [x] source, artifact, replay, runtime, IH, and diagnostics pass.

### #13

- [ ] RT0-RT2 complete.
- [ ] exact closed repro passes through explicit result evidence.
- [ ] open certified QuickSort result theorem passes.
- [ ] no `::`-driven synthesis or DefEq reflection was added.

### #14

- [ ] RT0 and RT3 complete.
- [ ] effects, may/must termination, partial correctness, total correctness,
      divergence, and UNKNOWN are distinct.
- [ ] artifact replay and all positive/negative tests pass.

An Issue receives a closing comment containing commit IDs, focused/full test
results, performance results, and links to the corresponding plan completion
section. Do not close an Issue from a green neighboring test alone.

### 13.1 Issue lifecycle protocol

Issue handling is part of implementation, not an administrative follow-up.

After every phase:

- [ ] identify every Issue whose acceptance boundary was touched;
- [ ] compare the implementation against that Issue's complete current body and
      comments, not only this plan's summary;
- [ ] post a progress comment with commit, tests, performance, completed
      criteria, remaining criteria, and any finding disproved by the code;
- [ ] update this plan's dashboard and completion log;
- [ ] leave the Issue open when any material acceptance criterion remains.

At the end of the integrated plan, assign exactly one disposition to every
Issue:

1. **Implemented and closed**: all valid criteria have permanent evidence.
2. **Audited and closed as not applicable**: the reported concern is false or
   based on an obsolete model; the closing comment gives code references and a
   permanent test where useful.
3. **Partially implemented and open**: remaining work is still part of the same
   coherent Issue and is listed precisely.
4. **Implemented core and split**: the original Issue mixed independent work;
   the completed portion is documented, a new Issue owns the genuinely
   independent remainder, and closure does not hide that remainder.

Rules:

- [ ] Do not close an Issue merely to reduce the open count.
- [ ] Do not keep an Issue open after every valid concern has been implemented
      or rigorously disproved.
- [ ] Do not silently edit an acceptance criterion to match the implementation.
- [ ] When a proposed consolidation is wrong, preserve the theoretically
      distinct records and explain why their similar fields are not duplicate
      authority.
- [ ] When a proposed consolidation is valid, remove the old writable path; a
      compatibility mirror does not count as completion.
- [ ] Closing comments must distinguish adopted findings, rejected findings,
      and intentionally deferred independent work.

### 13.2 Required phase-to-Issue checkpoints

| Completed phase | Issues to re-audit and comment |
|---|---|
| OA0 | #17, #18 |
| OA1 | #18; #17 constraint-authority portion |
| OA2 | #17 accepted-replay portion |
| OA3 | #17 TypeDeclaration portion; #11/#16 dependency impact |
| OA4 | #17, #18 |
| IF0 | #16, #11 |
| IF1 | #16, #11 |
| IF2 | #16 and possible close; #11 remaining matrix |
| IF3 | #11 and possible close |
| RT0 | #14; #13 representation dependency |
| RT1 | #13; #14 result/termination distinction |
| RT2 | #13 and possible close; #14 remaining totality work |
| RT3 | #14 and possible close |
| CL | every still-open Issue in this plan |

## 14. Progress Dashboard

| Phase | Status | Blocks | Completion evidence |
|---|---|---|---|
| OA0 authority inventory | Complete | none | authority table, static guards, baseline and final measurements |
| OA1 mutable solver authority | Complete | none | no copied computation result Term; ConstraintDB owns lifecycle; solution table owns final occurrence result |
| OA2 accepted replay view | Complete | none | immutable storage-neutral premise view and distinct mutable builder storage |
| OA3 Type schema boundary | Complete | none | semantic schema/readback/representation/cache stores are physically distinct; cache-independence checks pass |
| OA4 physical decomposition | Complete | none | six responsibility modules; `constraint_solver.inc` is 8 orchestration lines |
| IF0 conformance matrix | Complete | none | `tests/audit/iadts_conformance.tsv` |
| IF1 constructor specialization | Complete | none | concrete Box, Perfect, Matrix, dependent-index construction and post-checks |
| IF2 Match/refinement boundary | Complete | none | constant motive accepted; equality-dependent residual and nested-positive unsupported remain distinct |
| IF3 general indexed-family completion | Complete | none | exact head/tail/map/append, multi-index elimination, Acc IH, IF8, and artifact corruption tests |
| RT0 result/totality contract | Partial | RT3 | deterministic finite success is specified; general nondeterministic/abort/divergence contract is deferred |
| RT1 Returns relation | Complete for deterministic finite fragment | RT2 | ordinary Terms, Claims, Derivations, negative forgery checks, and v80 replay |
| RT2 post-hoc Claims | Partial | #13 | closed dependent result and open explicit result binder pass; open QuickSort sortedness construction remains |
| RT3 totality/partiality | Partial | #14 | Terminates-from-Returns and v80 replay pass; general open/partial/effectful totality remains |
| CL final audit | Complete for implemented scope | GitHub disposition | 40/40 integration tests and final measurements recorded below |

## 15. Completion Log

Append one row after each phase. Do not rewrite historical measurements.

| Date | Phase | Commit | Tests | IF8 median | Artifact | Added | Deleted | Net | Notes |
|---|---|---|---|---:|---|---:|---:|---:|---|
| 2026-08-19 | Planning | `54d33d9` | previous 39/39 | previous 3.69 s median sample | v78 | - | - | - | #17 and #18 opened; no implementation change |
| 2026-08-20 | OA0-OA4, IF0-IF3, RT0-RT3 fragment, CL | this implementation commit | 40/40, 111.773 s | 5.975 s final single compile; 6.102 s suite sample | v80, IF8 456,207 bytes | 23,545 | 17,485 | +6,060 | Includes tests and version replacement; docs are +936 lines. General #13/#14 remain open. |

### 15.1 Implemented authority result

- `OperationConstraintDB` owns equation lifecycle, reason, dependencies, and
  evidence. `OperationSolutionTable` retains only final O(1) occurrence values
  plus the independent usage analysis.
- computation kernel input remains an immutable JudgementDelta payload; its
  invocation-local result arena is the sole result payload and ConstraintDB no
  longer copies `result_term`.
- effect metavariable substitution, equation lifecycle, occurrence result,
  VerificationDB residual contracts, and diagnostic snapshots are separate.
  Verification obligations are generated from ConstraintDB, not from the
  diagnostic snapshot.
- accepted replay consumes immutable premise views. Candidate construction has
  a separate mutable builder pointer; accepted Proposition constness is not
  cast away.
- TypeDeclaration semantic schema, readback, representation identity/index,
  and constructor classifier cache are separate stores. Kernel schema access is
  read-only and cache-clearing regression checks preserve replay.
- the former 12,386-line solver owner is split by semantic responsibility into
  six modules. The 8-line include file is orchestration only; no compatibility
  implementation remains.

### 15.2 Implemented IADT result

The permanent matrix covers List, Vec, Fin, Matrix, dependent indices,
Diagonal, Box, Perfect, Acc, indexed append, constant and dependent neutral
motives, nested-positive unsupported recursion, invalid negative recursion, and
surface syntax errors. Artifact replay rejects forged indexed constructor
result classifiers, field Contexts, and lifted Acc IH payloads.

### 15.3 Result and termination boundary

`#.Returns (&M) v` and `#.Terminates (&M)` are ordinary object Terms backed by
accepted Claims and Derivations. `#.terminates` requires accepted Returns
evidence for the same computation. These rules survive v80 publication and
replay and never feed conversion, classifier synthesis, or global DefEq.

This is deliberately not described as complete #13/#14 support. The current
open-result fixture accepts Returns evidence and a result-indexed proof as
explicit premises; it does not construct the full QuickSort sortedness proof.
Likewise, Terminates-from-Returns proves a finite deterministic execution, not
general open totality, may/must divergence, or effectful postconditions.

### 15.4 Final verification and accounting

- integration: 40/40 passed, total 111.773 s;
- examples 01-09: passed through the permanent manifest;
- IF8: source equality 6.102 s, publication 5.830 s, second deterministic
  compile 6.084 s, complete fixture 20.769 s;
- standalone IF8 artifact: 456,207 bytes;
- ordinary Context/Substitution rebuild regression: zero, as required;
- implementation plus permanent tests: +23,545 / -17,485 / net +6,060 lines;
- permanent tests within that total: +882 / -137 / net +745 lines;
- documentation: +936 lines including this completion update;
- peak RSS was not recorded because `/usr/bin/time` is unavailable in this
  environment; no substitute estimate is reported as a measurement.

## 16. First Implementation Step

The next code change is OA0 instrumentation and authority tests, followed by
OA1. Do not begin by moving chunks of `constraint_solver.inc`.

The first semantic deletion target is the copied computation result:

```text
computation_constraint_results[i].solved_classifier
    -> ConstraintDB.result_term
    -> occurrence solution
```

The intended replacement is:

```text
ComputationResultArena[i].solved_classifier
    -> checked publication into OperationSolutionTable

ConstraintDB
    stores only lifecycle plus the checked result ID/revision
```

This is narrow enough to test, establishes the single-authority pattern, and
does not require changing IADT, equality, totality, or source syntax.
