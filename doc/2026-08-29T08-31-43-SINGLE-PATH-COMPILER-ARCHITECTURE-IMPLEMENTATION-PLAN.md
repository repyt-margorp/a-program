# A Program Two-Layer Simplification and Deletion Plan

Date: 2026-08-31
Status: stopped by user on 2026-09-07; failed refactoring archived
Last revised: 2026-09-03T14:18:00+09:00
Scope: `src/prototype/` only

The user stopped this implementation. Do not resume its remaining checklist as
an active goal. See `2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md` for the
preserved failure state. The successor is
`2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md`; it is a new implementation,
not another incremental migration of these stores.

This document replaces the former 10,981-line incremental migration plan. That
plan preserved the calculation/typing distinction, but implemented too many
intermediate stores, handoff objects, copied result forms, state machines, and
micro-tests. The result was a larger and less controllable compiler.

The replacement objective is not another compatibility layer. It is to retain
the two semantic layers required by A Program while deleting physical
boundaries that do not correspond to distinct mathematics.

### 2026-09-03 13:04 seed/current Authority correction

The retention correction is stronger than merely adding two writer switches.
The current Solver still materializes some answers into records that also carry
source topology: resolved Match constructor owners, rebuilt Match Core terms,
branch-refinement status, checked occurrence classifiers, and generated binder
facts. A solved image therefore cannot yet emit a truthful `RECOMPUTE` file by
serializing those records as-is.

No full zero-progress snapshot will be retained beside the live image. That
would make retention policy responsible for a second semantic store. Instead,
the one image has the following ownership rule:

```text
immutable seed topology
  Term/Context/Substitution/declaration/source-occurrence records
  source labels, binding identities, source Core endpoints
  root classifier recipes and unresolved Match-resolution goals

canonical progress
  one solution cell per generated Layer T equation
  current Context projection answer
  accepted Claim/Derivation evidence

derived publication/cache
  rebuilt occurrence Core, checked classifier projection, diagnostics,
  indexes, SCCs, work frontier
```

Fields in the last group may remain cached in memory only when their exact
equation answer validates them. They are never replay seed and are omitted by
`RECOMPUTE`. Match constructor correspondence and branch refinement must be
read through their canonical equation solutions; mutating a source case cannot
be their Authority. Generated binder facts are expansion products and cannot
be appended to `typing_pipeline.seed` after the open-image boundary.

The `.a` writer therefore consumes explicit seed/current read-only views, not a
raw dump of `compile_context`, `TypedOccurrence`, or `ConstraintDB`. The reader
relocates section-local references into the ordinary stores, rebuilds all
indexes, and enters the same `solve(image, steps)` entry as source lowering.
`RECOMPUTE` and `CHECKPOINT` remain writer-only retention profiles.

### 2026-09-03 14:18 current implementation checkpoint

The immutable `typing_pipeline.seed` is now guarded by a field-wise digest at
the open-image boundary and after every bounded solve. The guard exposed two
real writes that had previously been hidden as implementation convenience:

1. generated Match binder classifiers were appended to the immutable binder
   input list;
2. rebuilt evidence classifiers overwrote source classifier seeds.

Both are now equation progress. A generated binder classifier is merged into
the existing Context binder equation. A generated occurrence classifier is
retained by the owning `PROJECTED_CLASSIFIER` solution as an unprojected source
candidate until its Context action becomes available. Reindexing and answer
publication then happen in that one aggregate equation. This is required for
resumable indexed Match solving: dropping the candidate while the Context is
pending loses a legitimate answer, while writing it into the seed makes
`RECOMPUTE` depend on how far the Solver already ran.

The exact representative of endpoint-indexed evidence is also current equation
state. When a rebuilt `Terminates` witness changes only to a DefEq Core endpoint,
the aggregate still replaces its old representative; otherwise the published
evidence type and witness can name different, merely convertible endpoints.
The IF8 fuel-free QuickSort integration suite passes after this correction.

The next Authority cut is narrower than the earlier document suggested:

- immutable source topology is serialized from source fields and seed records;
- generated Core, Match constructor correspondence, branch refinement,
  equation answers, and evidence are checkpoint progress;
- no full second zero-progress image is retained in memory;
- the writer receives explicit read-only seed and current views and decides
  which current sections to retain;
- the loader always reconstructs one ordinary open image, then optionally
  validates and installs retained checkpoint progress.

Current checkpoint:

- [x] Source lowering reaches an open image with zero semantic transitions.
- [~] `solve(image, steps)` is the only semantic advancement entry inside the
  current image implementation. The ordinary compiler still performs external
  reference rewriting and Universe closure after destroying that image; the
  whole-compiler invariant is therefore not complete.
- [x] `seal` is read-only and root roles are explicit.
- [x] Match-resolution diagnostics have been removed from semantic Authority.
- [ ] Move Match constructor correspondence out of mutable source-case fields.
- [x] Stop solve-time writes/appends to `typing_pipeline.seed` and enforce this
  with the open-image seed digest.
- [ ] Make all in-memory derived occurrence publication validate against its
  canonical equation solution.
- [ ] Add field-wise section-local `.a` encoding and relocation.
- [ ] Rebuild an open/partial image without AST/source-plan allocations.
- [ ] Verify zero/partial/solved round trips under both retention profiles.
- [ ] Cut over `.ao` and `.apo` together only after the preceding checks pass.

### 2026-09-03 current `.a` implementation order

The latest code audit fixes the persistence boundary more precisely. Source
lowering and resumed loading must not construct different image kinds. Both
produce the same open `prototype_compilation_image`; only their seed decoder is
different. The image has no retention mode. `RECOMPUTE` and `CHECKPOINT` are
arguments to the writer alone.

The Match source/progress split is now represented explicitly:

```text
declared_constructor_owner/id  immutable source correspondence
constructor_owner/id           current solved correspondence
branch-refinement solution     current equation answer
```

An unresolved source case has an invalid declared pair and an immutable
`match_resolution_goal`. A solved current pair is legal only when it equals the
declared pair or is justified by that goal's canonical equation. A v90 accepted
artifact has no local resolution history, so its decoded declared and current
pairs are identical. This compatibility rule is confined to the old reader and
does not define the new `.a` format.

Implementation now proceeds in this order:

1. [x] Separate generated occurrence classifier candidates from
   `typing_pipeline.seed`; the owning `PROJECTED_CLASSIFIER` solution retains
   the candidate until its Context action can reindex it.
2. [x] Add immutable declared Match constructor fields without introducing a
   second occurrence graph.
3. [x] Enforce pair validity and declared/current agreement in generic
   occurrence-graph validation; require one immutable resolution goal for every
   source Match case at the image boundary.
4. [ ] Move the remaining post-image semantic passes into the image lifecycle.
   External-reference rewriting and Universe closure currently run in
   `compiler_session.c` after `prototype_ast_compile_pending_with_imports()` has
   already solved, sealed, and destroyed its image. The image must own the
   UniverseDB reference, and these operations must be deterministic Solver work
   before final accepted publication. Reduction-environment projection is
   derived read-only state and belongs to seal/publication validation.
5. [ ] Expose one read-only image storage view to persistence code. It contains
   const references and immutable seed prefix/count information, not copied
   databases and not a second Authority.
6. [ ] Add a source-independent image constructor that attaches already-decoded
   semantic stores, initializes the ordinary compile context, rebuilds indexes,
   and enters phase 1 without AST, source schedule, or source result buffers.
7. [ ] Define a field-wise sectioned `.a` schema. Mandatory sections encode the
   replay-complete seed; optional checkpoint sections encode canonical generated
   equations, exact dependencies, replay-valid answers, and accepted evidence.
   Raw C structure dumps and embedded `.p` source are forbidden.
8. [ ] Implement `RECOMPUTE` writing from immutable/source fields only. It must
   produce identical seed sections from zero-progress, partial, and solved
   instances of the same image.
9. [ ] Implement `CHECKPOINT` writing as the same seed bytes plus current
   canonical progress. Never persist work queues, retry counts, traversal order,
   caches, diagnostics, or superseded candidates.
10. [ ] Load mandatory sections, relocate exact references, rebuild hash indexes,
   dependency indexes, SCCs, and the ready frontier, then validate and install
   any optional checkpoint answer against its equation key and direct-input
   snapshot.
11. [ ] Verify zero/partial/solved round trips for both writer profiles and the
    deterministic partition law for `solve(a + b)` versus
    `solve(a); solve(b)`.
12. [ ] Replace `.ao` and `.apo` together with `.a` in compile, import, inspect,
    link, aggregate, replay, backend, documentation, and tests. No compatibility
    suffix branch remains after this cut.

The writer may omit a seal when its selected retention profile omits evidence
covered by that seal. It may never weaken or reinterpret a seal. Loading always
returns an open, paused, or solved image according to retained validated state;
it never silently recompiles source or invokes an alternate Solver.

The post-image audit changes the persistence gate: a `.a` writer must not be
declared complete while `prototype_link_external_refs()` or
`prototype_universe_build_closed()` can still change the program after the
image has been sealed. Persisting before those operations would create an image
that is neither the compiler input nor its accepted output; persisting after
them would capture mutations not represented by the image's equation graph or
seal. This must be corrected before schema stabilization.

### 2026-09-03 `.a` lifecycle and current-progress correction

The implementation target is one compilation image and one advancement path:

```text
.p
  -> structurally lower to an in-memory .a image with zero semantic progress
  -> solve(image, steps) zero or more times
  -> optionally seal(image, accepted_roots) without mutation
  -> write(image, RECOMPUTE | CHECKPOINT)
```

The old `.ao` and `.apo` files do not become two variants of the new format.
They are both deleted when the sectioned `.a` reader/writer is ready. Before
that cutover, v90 remains only the current solved-publication compatibility
boundary and is not evidence that open or partial `.a` persistence exists.

`RECOMPUTE` and `CHECKPOINT` are writer retention profiles, never Solver modes:

- `RECOMPUTE` writes the replay-complete immutable seed and omits every
  reconstructible generated equation, answer, evidence record, work frontier,
  and cache. The loaded image recomputes that work through the ordinary Solver.
- `CHECKPOINT` writes the identical seed plus canonical generated equations,
  dependency edges, and at most the current replay-valid answer/evidence for
  each equation. It never writes superseded candidates, retries, queue order,
  or another solution Authority.
- A seal certificate is retained only when the selected sections retain every
  accepted root, answer, and evidence dependency covered by that certificate.
  Otherwise the writer omits the certificate and the loaded image is open.

`steps` is a deterministic Solver cost, not wall-clock time. Parsing,
structural lowering, hash interning, and seed well-formedness checks consume no
Solver steps. The implementation already exposes `--solve-effort STEPS`; the
remaining persistence work must record its cost-model version and verify the
partition law that `solve(a + b)` and `solve(a); solve(b)` reach the same
canonical image state.

The current implementation has completed the in-memory cut: `lower` is
solver-free, `solve` is the sole semantic-progress entry, successful solving is
`SOLVED`, `seal` is read-only, and the four root roles are explicit sorted
reference sets over the existing Authority stores. Classifier replay seeds now
belong to `typing_pipeline.seed`; the duplicate mutable input-fact table and its
never-written suppression flag have been deleted. The
`prototype_typing_pipeline_legacy_state` wrapper is now deleted: immutable
termination-witness roots are seed references, accepted proof state belongs to
`JudgementDB`, and proof reification, usage analysis, classifier cursors, and
queues are explicitly disposable workspaces. The former legacy adapter is now
the ordinary `typing_publication` module. Remaining work is persistence-stable
semantic keys, the `.a` container, load-time replay, and legacy suffix removal.
Those must not be approximated by renaming v90, serializing `compile_context`,
or copying invocation workspaces into another image record.

### 2026-09-03 12:37 persistent-state extraction checkpoint

Item 13 is complete for the present semantic fragment:

- source replay inputs, including exact termination-witness occurrences, are
  immutable records in `typing_pipeline.seed`;
- current classifier/effect/refinement answers live in canonical ConstraintDB
  solution cells, while accepted theorem evidence lives in `JudgementDB`;
- Context projection lookup tables, dependency indexes, SCCs, usage vectors,
  proof reification caches, active equation IDs, and retry queues are
  reconstructible or disposable and are not serialization Authorities;
- authority-neutral evidence lookup now reads the current `JudgementDelta` and
  committed DB through one path, so a `RETURN`/`THUNK` can consume a literal
  proof emitted earlier in the same materialization round;
- `compile_context` retains source pointers only as lowering ownership and
  optional diagnostics. They are disconnected before `solve` and are not part
  of the `.a` replay contract.

The next gate is item 7(e): persistence-safe identities for every reference in
the replay seed. This does **not** require replacing the fast in-memory IDs by
global content hashes. The wire representation assigns canonical section-local
IDs, relocates every cross-section edge explicitly, and uses a stable semantic
hash only to select comparison candidates. Exact decoded record/graph equality
remains Authority after a hash hit. Only after the reference-kind audit and
round-trip tests pass may item 14 define the sectioned `.a` reader/writer.

### 2026-09-03 12:44 persistence identity and retention correction

The `.a` format is an event/graph image, not a memory snapshot and not a renamed
v90 publication artifact. In particular:

- image-local `TermId`, `ContextId`, `SubstitutionId`, `TypedOccurrenceId`,
  `TypedProjectionId`, `EquationId`, `ClaimId`, and declaration IDs remain the
  efficient in-memory representation;
- each persisted section uses dense local IDs, and every reference declares its
  target section. Loading interns or appends records through the owning DB and
  records relocation before decoding dependent sections;
- recursive equation groups are serialized as graph edges and reconstructed as
  SCCs. A recursive content hash is therefore not used as identity;
- hashes are acceleration data only. They cannot authorize a typing answer,
  prove equality, or replace exact key comparison;
- `source_ast`, source schedule order, retry order, queue order, wall-clock
  measurements, and invocation workspaces are not replay seed data.

The mandatory seed sections are the minimum data needed to recreate the same
open image without a `.p` file: symbols and namespace identity, Layer C graph,
Context/Substitution and declaration authorities, immutable typed-occurrence
and typed-projection topology, and `typing_pipeline.seed`. A `RECOMPUTE` writer
stops there. A `CHECKPOINT` writer may additionally retain canonical generated
equations, exact direct dependencies, current replay-valid solution cells, and
accepted Claim/Derivation evidence. The reader has one path: load mandatory
seed, validate it, load whichever optional progress sections exist, rebuild
indexes/SCC/frontier, then expose the same `solve(image, steps)` operation.

Retention is deliberately absent from the in-memory image and from Solver
configuration. `--a-retention=recompute|checkpoint` is consumed only by the
writer. Consequently the required semantic test is not byte equality between
the two profiles, but convergence to the same accepted roots and seal after the
same total deterministic Solver-step budget.

### 2026-09-03 12:27 indexed-motive identity correction

The persistent image work exposed one remaining order-dependent Layer T rule.
An expected result equation previously constructed every Match motive as a
single value Lambda. For an indexed family this could capture the current
index, so a motive valid only at the parent index was later reused to type an IH
at a recursive child index. The generated QuickSort boundary then expected a
`SizedList A (succ tailSize)` where the recursive result supplies
`SizedList A lowerSize`.

The corrected rule is now part of the persistence prerequisite:

- whether a Match owns an IH is read from immutable `TypedOccurrenceGraph`
  ownership, never inferred from whichever IH constraint happens to be active;
- an indexed Match without an owned IH may retain a one-argument constant
  motive that captures its Context index;
- when an owned IH can instantiate the motive at another index and the result
  depends on a family index, motive construction abstracts the complete index
  telescope plus the scrutinee value before any answer is published;
- the dependency test is read-only. It must not allocate speculative Binding or
  Term identities for paths that retain the ordinary constant motive;
- the two-recursive-IH QuickSort test is the permanent regression boundary.

This closes the concrete dependent-motive contradiction encountered while
extracting Layer T state. Match/IH semantic answers now reside in canonical
ConstraintDB cells; their indexes and active traversal state are rebuildable
workspace and are not `.a` sections.

### 2026-09-03 08:51 execution checkpoint

The current implementation has advanced beyond the earlier accepted-classifier
failure and now has the in-memory open-image boundary, but it has not reached
the persistent `.a` boundary:

- generated QuickSort completes source solving, checked-Core validation,
  accepted typed-publication construction, and accepted proof publication;
- hash-consed classifier RHS nodes for literal, static endpoint, value APP,
  type APP, accepted substitution, thunk, and `Terminates` now live in the
  common ConstraintDB operand arena;
- static endpoint leaves retain the selected source occurrence, so two typed
  uses of one erased Layer C Term are no longer identified by Core shape alone;
- the owning Pi classifier equation stores its RHS root directly. The former
  occurrence-to-recipe root map and separate classifier-expression database
  have been deleted;
- one structural RHS evaluator is used by source Pi solving and accepted
  classifier projection. A dependent generated function graph proves that a
  source binder which has left the lowering stack is recovered through its
  Context-owned BindingId rather than by recompiling scoped AST;
- candidate publication is checked against the sealed accepted
  `TypedProjection` tuple rather than a stale source occurrence tuple;
- the ordinary dependent-Match fixture still accepts, proving that accepted Pi
  reconstruction does not overwrite an explicit dependent classifier;
- classifier-equation operands no longer use the former fixed four-slot array.
  They are immutable variadic edges with explicit target kind, semantic role,
  target, and qualifier in the common constraint graph storage;
- classifier answer writes now enter through
  `operation_solver_projection_answer_transition()`. The former bind/replace
  writer APIs are deleted, and unchanged DefEq answers are no-ops rather than
  numeric-Term-ID representative changes;
- the central ConstraintDB now records semantic transition counts transactionally,
  both in total and by solution domain. The classifier writer increments the
  classifier count only when its authoritative answer or validation state
  actually changes; worklist visits and unchanged DefEq proposals do not count;
- the premise lifecycle API now takes `ConstraintDB + EquationId` rather than
  an unowned solution pointer. All six central constraint domains
  therefore account state/reason changes through the owning equation. The
  remaining gap is semantic progress held outside ConstraintDB, especially the
  occurrence-keyed usage solution refresh in legacy typing state;
- the separate lifecycle `reopen` mutation has subsequently been deleted.
  One transition now performs direct state/reason replacement, clears evidence
  only when entering `PENDING`, and is a no-op for an unchanged lifecycle.
  Final aggregation no longer manufactures `final -> pending -> final` progress;
- `PENDING` and `INCOMPLETE_BUDGET` are now explicitly frontier/effort states,
  not semantic progress. Lifecycle accounting counts only transitions to
  `SOLVED`, `RESIDUAL`, or `CONTRADICTION`; classifier answer changes remain
  separately counted. Transition counts are retained execution history, not
  semantic output Authority: bounded and uninterrupted solving may visit a
  different number of intermediate answers while their final canonical
  equation topology and solution cells must agree;
- immutable constraints now use one structured lookup-or-intern path. Exact
  semantic payload and operand-list duplicates reuse their EquationId,
  provenance-only source AST differences do not split identity, transaction
  rollback rebuilds the index, and effect generation no longer performs a
  separate linear duplicate scan. This key is canonical only inside the
  current image until its allocation-local references are replaced by stable
  semantic keys;
- the public creation APIs now say `intern`, not `append`. This makes the
  contract explicit: rediscovering one semantic equation returns its existing
  identity, while only a genuinely new key extends topology;
- classifier answer reuse no longer trusts a 64-bit operand fingerprint.
  Each answer records the exact producer-equation input revision and owning
  Context-projection revision. The hash remains diagnostic/index data only;
- input invalidation requeues only its owning equation. It does not wake the
  transitive dependent closure until reevaluation changes the semantic output.
  Refreshing the snapshot of an unchanged DefEq answer is not semantic progress;
- motive materialization wakes IH/fold dependents exactly on the transition to
  `MATERIALIZED`, not on every fixed-point visit. This closes the projection 5/7
  SCC loop exposed when exact revisions replaced the former digest heuristic;
- Context projection completion now enqueues the immutable premise equations
  of the owning aggregate. Readiness is represented by dependency/SCC
  scheduling rather than a foreign answer write;
- ordinary recursive ADT Match no longer becomes guarded-recursive merely
  because a constructor refinement has not yet supplied its motive seed. The
  dependent-Match and artifact-flow boundary tests pass after this correction.
- `PI_DOMAIN_RELATION` is now an immutable Layer T equation. It validates the
  APP argument against the callable domain in its own solution cell and never
  writes an argument projection answer. Each `TypedProjection` also has one
  aggregate `PROJECTED_CLASSIFIER` equation. Its semantic premise set is the
  owner partition of non-aggregate classifier equations; a rebuildable index
  and append-only dependency edges schedule that partition without copying it
  into a second operand snapshot. Only the aggregate mutates the projection
  answer.
- zero-clause fold sequencing is now represented by an immutable
  `SEQUENCE_BINDER_RELATION` equation. Expected classifier refinement is routed
  to the unique canonical `BINDER_TYPE` premise and the aggregate re-derives
  its answer. Match/IH/fold recursion is scheduled by derived equation SCCs;
  relation equations no longer publish into foreign answer cells.
- the public incremental owner is now `prototype_compilation_image`, not a
  producer session. `prototype_compilation_image_lower()` stops after Layer C,
  Layer T projection topology, and replayable root-equation expansion, and
  asserts that every equation remains pending with zero ConstraintDB semantic
  transitions. `prototype_compilation_image_solve()` is the sole entry that
  starts usage, imported classifier, Match, classifier, effect, and proof
  progress;
- ConstraintDB is now one interleaved append-only equation arena. A generic
  per-domain linked index replaces domain-contiguous first/count assumptions.
  Re-running expansion uses lookup-or-intern, retains existing answers, and
  appends only genuinely new equations or dependencies after topology growth;
- source-derived Match resolution goals, Match typing goals, imported
  constructor goals, binder inputs, and declaration inputs now belong to the
  persistent Layer T `typing_pipeline.seed`. They are no longer arrays owned
  by the transient `compile_context`; imported goals retain an import-table
  index rather than a process-local interface pointer;
- top-level publication and `::` expectation inputs now also belong to that
  seed. Expectation checking and label publication no longer rediscover their
  semantic inputs by scanning the AST or source-result tables;
- focused regressions currently pass for dependent Match refinement, explicit
  indexed families (Acc, Box, and Perfect), IF8 fuel-free QuickSort, classifier
  RHS Authority, constraint Authority, computation-block sequencing, and
  issue-23 dependent motives.

This is partial CG0.5, not completion. The separate expression store and
publication-only structural evaluator are gone, and source binder annotations
are now materialized once into persistent Layer T classifier-RHS seed roots at
the lowering boundary. Synthesized Match/Pi/reindex/computation
classifiers do not yet all preserve endpoint operands, and checkpoint resume
and sealing do not yet invoke the common evaluator through the future image
API.

The current implementation must therefore be read as the following progress
matrix. This matrix supersedes older prose that described strict artifact
publication as the active blocker:

| Capability | Current state | Architectural meaning |
| --- | --- | --- |
| canonical Layer C arena | implemented | retained as the one erased computation graph |
| immutable variadic Layer T operand edges | implemented | replaces the former fixed four-slot operand array |
| accepted executable closure and strict Match validation | implemented in v90 | prototype of the sealed-publication role only |
| common classifier RHS equation representation | implemented for the migrated annotation fragment | one ConstraintDB arena; source annotations materialize immutable seed roots and equations reference them; synthesized propagation remains |
| one evaluator for source, resume, projection, and seal | partial | source Pi solving and accepted projection share it; resume and seal remain |
| canonical Layer T equation interning | implemented in-image | one structured lookup-or-intern path; persistent semantic identity remains |
| canonical classifier answer write | implemented for current classifier equations | PI-domain and sequence-binder relations constrain canonical premises; one aggregate equation owns each projected answer |
| domain-wide transition accounting | partial | every central ConstraintDB lifecycle and classifier answer transition is counted; legacy usage/effect stores plus universe/equality/proof progress remain outside this accounting |
| solver-free `.p -> open image` boundary | implemented in memory | one non-copying image owner has explicit `lower` and `solve` entry points; source tables are disconnected from the Solver context; root equations exist but remain unsolved; persistent `.a` sections are pending |
| independent seed/provenance/checkpoint/publication roots | implemented in memory | four sorted reference sets point into existing Authority databases; persistent relocation/canonical section encoding is pending |
| `.a` persistence and legacy `.ao`/`.apo` cutover | not implemented | no storage profile may be treated as complete yet |

Malformed indexed decrease evidence now distinguishes rigid APP-domain
contradictions at the owning APP equation. Both a missing proof argument that
places the following List value in the proof slot and a forged rigid witness
are rejected as `classifier-equation-contradiction`; completion no longer
misreports either rigid mismatch as merely unsolved.

The related external-witness convergence defect was isolated by a 2026-09-03
trace. It found one base binder projection whose answer was alternately
proposed by a PI-expected premise and a zero-clause fold-result premise. Making
APP domain checking read-only removes that oscillation and the fixture reaches
the exact APP contradiction. It also exposes a real cyclic dependency in the
positive Acc eliminator: `Match motive -> IH result -> sequence binder -> APP
domain -> Match motive`. The former foreign expected write had silently acted
as the cycle seed. The completed repair represents PI-domain and fold/binder
relationships as equations, routes refinement to the canonical binder premise,
and schedules the recursive component through its derived SCC.

The diagnostic answer fingerprint no longer includes
`projected_binder_classifier`, and hashes do not authorize reuse: exact producer
and Context revisions do. That repair remains valid, but cannot make foreign
answer ownership valid. CG0.1 has now replaced PI-expected and fold-result
foreign publication with immutable relation equations and aggregate/SCC
ownership. Final-state aggregation and persistence-stable semantic keys remain.

Transition accounting is deliberately observational, not a second answer
Authority. The counters are restored by ConstraintDB transaction rollback and
are incremented inside the canonical answer transition before each real
classifier mutation or terminal lifecycle change. Frontier reopening and
effort exhaustion are excluded, and the counters are not used to decide an
answer. Once every domain
uses the same accounting rule, the `.p -> open .a` boundary can assert zero
semantic progress without inspecting phase names or procedural call paths.

The open-image cut is now explicit. Source lowering returns after Layer C,
Layer T occurrence/projection topology, and topology-only root-equation
expansion. Usage solving, input-fact seeding, imported classifier publication,
pending Match resolution, and every answer transition run only through
`prototype_compilation_image_solve()`. Expansion no longer clears ConstraintDB,
effect metas, or classifier answers: repeated discovery is lookup-or-intern and
topology growth appends new equations. This is now a valid in-memory seed
section; persistent semantic keys and the `.a` reader/writer remain.

That repair exposed and closed a second Authority mismatch. A dependent Match
proof may need the same source occurrence under several solved
`TypedProjection` Contexts. The accepted executable publication selects one
root projection per occurrence, but it is not the Authority for every
projection-local premise. Candidate publication now uses a read-only lookup
over the frozen Layer T equation graph and accepts a premise only when its
occurrence, Context, subject, and classifier match a solved projection answer.
The principal publication tuple remains the executable root. This distinction
must be preserved in `.a`: checkpoint answers/evidence range over canonical
equations, while the optional seal certificate names only accepted executable
roots.

The former strict artifact failure is fixed at its first required role boundary.
Generated QuickSort now writes and rereads one dense arena containing 52
accepted executable Match cases and 25 source/provenance-only Match cases. An
explicit executable reachability certificate covers 4,633 of 5,459 persisted
Terms and only the 52 resolved cases. The retained ownerless cases remain
readable but cannot become executable roots.

This did not by itself complete `.a` role persistence. The later in-memory image
work now gives replay seed, checkpoint progress, optional source provenance, and
accepted publication independent root sets. The current v90 representation
still does not encode those roles as independent sections. CG3 must relocate
the explicit roots; it must not infer them from mere presence in an arena.

The repair is not to guess nominal owners for those cases or weaken the strict
writer. Open `.a` seed/provenance sections may contain unresolved topology.
Only an accepted projected closure may be executable, and every Match in that
closure must have resolved nominal correspondence. Section reachability must be
computed independently for these roles.

Role separation does not create a second TermDB. Seed and accepted projected
Terms remain hash-consed in one canonical Layer C arena. The `.a` directory
stores role-specific root sets, optional retained records, and a seal
certificate over the accepted closure. A Term reachable from multiple roles is
serialized once and referenced by semantic relocation; unresolved source Match
nodes are valid arena members but can never belong to a sealed executable root
closure.

### 2026-09-03 `.a` model correction

The `.a` file is not only the final answer of compilation. It is the serialized
form of the same compilation image at any amount of completed work:

```text
.p
  -> parse and structurally lower
  -> open .a seed image              // zero Solver transitions
  -> solve(image, effort)            // advance the same image
  -> optional seal(image)            // read-only accepted-root certificate
  -> write .a at any point           // choose retained sections/certificate
```

There are therefore no separate "unsolved artifact", "checkpoint", and
"final artifact" data models. One sectioned `.a` container carries one Layer C
arena and one Layer T equation graph. Missing answer/proof/cache sections mean
that work must be recomputed. Their presence means that replay-valid current
work may be reused. A seal certificate means that a strict accepted executable
closure was validated; it does not change the meaning of the underlying graph.

The zero-progress image is solver-free, not structurally unchecked. Parsing,
name resolution needed to construct immutable references, canonical interning,
and graph well-formedness checks have completed. No classifier, effect, usage,
totality, universe, equality, or proof equation has been advanced. This is the
only semantic output of reading `.p`; every later operation consumes `.a`.

`RECOMPUTE` and `CHECKPOINT` are writer profiles only:

- `RECOMPUTE` stores the replay-complete immutable seed and deliberately omits
  reconstructible equation expansion, answers, evidence, and caches;
- `CHECKPOINT` stores the identical seed plus canonical generated equations,
  their current replay-valid answers/evidence, residual obligations, and at
  most one explicitly provisional candidate per recursive SCC;
- neither profile stores superseded answers, worklist history, retry flags, or
  another mutable solution Authority.

The replay-complete seed is not an AST dump. It includes every immutable root
equation and operand needed to generate later equations deterministically.
`RECOMPUTE` may omit generated equations only when their semantic keys and
operands are reproducible from that seed. `CHECKPOINT` may retain those
canonical generated equations, but loading them must intern the same keys that
recomputation would produce. Thus the profiles may differ in file size and
resume effort, but never in equation identity, accepted answers, or diagnostics.

This resolves the apparent tradeoff between recomputation and permanently
retaining every intermediate type result. Recompute images keep only inputs
needed to derive those results again. Checkpoints keep only the current answer
owned by each canonical equation, not every historical candidate. The profile
changes file size and resumed compile time, never typing or execution semantics.

The storage choice is independent from how far the live image has advanced. A
zero-progress, partially solved, fully solved, or sealed image may be written
under either profile whenever the required referenced sections are present. A
sealed `RECOMPUTE` projection may retain the accepted executable closure and its
certificate while omitting unrelated reconstructible Solver progress. If the
writer also omits evidence referenced by that certificate, the on-disk image is
open by construction; it does not carry a stale seal.

The implementation must therefore keep three independent axes instead of one
stage enum:

```text
semantic progress: zero | partial | fixed-point/residual/contradiction
retention policy:  RECOMPUTE | CHECKPOINT
execution seal:    absent | present and valid for named accepted roots
```

Only the first axis is changed by `solve(image, effort)`. The second is selected
by the writer. The third is produced by read-only validation and is invalidated
in an on-disk projection whenever any referenced answer, evidence, or accepted
root is omitted. No `if (profile == ...)` branch may occur in equation
generation, answer selection, or Solver scheduling.

Solving is monotone in canonical topology and validated knowledge, not in the
literal contents of a provisional answer slot. A provisional answer may be
refined or rejected under the same equation key. A validated answer is retained
only with the exact operand fingerprint that justifies it. Historical answers
and the order in which they were tried are not semantic state.

### 2026-09-03 source/image cutover decision

The migration target is now fixed more narrowly than the former artifact plan:

1. Reading `.p` constructs one structurally complete, zero-Solver-progress
   in-memory `.a` image.
2. `solve(image, effort)` is the only operation that advances semantic
   equations. Repeated calls advance the same image; they do not rebuild a
   compiler result object.
3. The image may be written while it is zero-progress, partially solved, at a
   fixed point with residuals or contradictions, or together with a valid seal
   certificate.
4. `RECOMPUTE` and `CHECKPOINT` select only which replay-valid sections the
   writer retains. They are not image states, elaboration modes, or Solver
   branches.
5. Both legacy suffixes, `.ao` and `.apo`, are replaced by `.a` in one cutover.
   Their current distinction between per-source and aggregate publication has
   no semantic successor.

"Zero progress" is measured by semantic equation transitions, not by an empty
image. Parsing, name binding, Layer C interning, Context/Substitution/IH-scope
topology, root Layer T equation interning, dependency edges, and structural
validation have already happened. Classifier, effect, usage, totality,
universe, equality, and proof answers have not advanced.

The in-memory image type must be introduced before the source stop boundary is
exposed. It is a non-copying owner/view over the existing Layer C and Layer T
stores, not another database or handoff representation. This corrects the old
CG1/CG2 ordering that first proposed returning an open image and only later
introducing its data type.

The current v90 `.apo` path is intentionally unchanged until this boundary is
real. It still serializes a solved accepted-publication closure. Renaming that
file today would falsely claim that unresolved topology and resumable progress
are already supported.

The in-memory boundary is still transitional in one concrete way. The image
owner retains allocations for the parsed AST, source schedule/lowering plan,
and source-result arrays until destruction, although the open-image boundary
disconnects all of them from `compile_context` and `solve()` rejects an image
whose Solver context still exposes one. These allocations are lifetime overhead
rather than semantic input and may be freed once rollback ownership is
separated. Successful solving now records `SOLVED`; independent read-only
`seal(image)` validation exists and does not mutate the graph.

### 2026-09-03 current-code alignment for the image cut

The public API now exposes the intended in-memory boundary. In
`finalization_and_entrypoints.inc`, `prototype_compilation_image_create()`
constructs one non-copying owner over the existing semantic stores,
`prototype_compilation_image_lower()` performs structural lowering, and
`prototype_compilation_image_solve()` is the only public entry that advances
semantic solving. The ordinary compiler calls those two entries in order; it
does not retain a private direct-to-publication route.

The current split is:

| Solver-free image construction | `solve(image, steps)` |
| --- | --- |
| build and validate the source schedule/lowering plan | usage solution advancement |
| intern the erased Layer C graph and seal its source handoff | imported-constructor classifier inference |
| build Context/Substitution/IH-scope and rooted TypedProjection topology | pending-Match resolution from inferred classifiers |
| lookup-or-intern replayable root equations, dependencies, and SCCs | seed immutable input facts and advance equation answers |
| install the read-only occurrence provider | classifier/effect/usage input-fact evaluation |
| assert zero ConstraintDB semantic transitions | equation expansion, answer advancement, proof publication, accepted-root projection |

This is a real in-memory boundary, but it is not yet the complete persistent
seed contract. Immutable replay roots and role roots are not yet serialized.
Root classifier and branch equations are now expanded before the boundary by a
topology-only lookup-or-intern pass; first solve no longer clears ConstraintDB,
effect metas, classifier answers, or dependencies. Type-representation
rebuilding remains Solver-side mutable work. It must become either a
deterministic projection of declaration schema or a disposable cache, never
classifier Authority.

`operation_solver_generate_constraints()` is now the topology half of the seed
builder. It initializes only rebuildable helper state, interns equations and
dependencies, derives indexes/SCCs, and validates topology. Usage solving and
input-fact registration occur only after entry to `solve(image, steps)`:

```text
build_seed_topology(image)
  = intern root equations and immutable input operands exactly once

solve(image, steps)
  = intern deterministic derived equations by semantic key
  + advance the one answer cell owned by each equation
```

Neither operation clears or regenerates already interned topology. Loading a
`.a` image and lowering `.p` must both finish at the same
`build_seed_topology` postcondition. The only difference is whether the graph
was decoded or constructed from source.

The non-copying image owns or views the existing Layer C
arena, TypeDeclaration schema, Context/Substitution/IH topology, canonical
Layer T equation graph, Judgement evidence graph, and role roots. It does not
copy them into a new database. Its phase cursor is temporary driver state, not
serialized semantic progress. Persisted progress consists only of canonical
topology, validated current answers/evidence, residual obligations, and an
optional seal certificate.

The implementation cut is complete only when all of the following tests pass:

1. `.p -> open image` produces non-empty structural topology and zero semantic
   transitions in every solution domain.
2. Repeated bounded `solve(image, steps)` calls and one unbounded call produce
   the same canonical equations, accepted answers, diagnostics, and seal.
3. Zero-progress, partial, solved, and sealed images can be written using both
   retention profiles whenever the retained sections satisfy their references.
4. Loading `RECOMPUTE` and `CHECKPOINT` projections and solving to closure gives
   the same accepted executable roots and proof replay result.
5. Only after those tests pass, `.ao` and `.apo` are removed together and all
   source compile, aggregate/link, replay, and execution paths consume `.a`.

This is the current progress point: canonical relation/equation ownership, the
solver-free in-memory source return, four independent root roles, and an
accepted-root-bound read-only seal are implemented for the current fragment.
Persistent Match/import/binder/declaration/publication seed records have
replaced the corresponding transient pending arrays, and semantic Solver reads
of AST/source-result storage are removed. Persistent-state extraction is
complete for the current fragment. Stable relocation identity, writer profiles,
the `.a` loader, resume, and suffix cutover remain.

## 1. Product Model

A Program is both:

1. a theorem-proving system that constructs and checks typed derivations; and
2. an implementation language whose programs are compiled and executed.

Compilation, type checking, proof search, normalization, and optimization are
forms of computation performed before runtime. Runtime is the residual
computation that was not discharged before publication. This does not collapse
typing into execution: the two layers answer different questions and must
remain separate.

### Layer C: calculation graph

Layer C owns only computational syntax and reduction:

- hash-consed Core terms;
- binding identity referenced directly by graph nodes;
- substitution;
- beta, iota, CBPV, and operation reduction;
- normalization profiles and their caches;
- residual executable computation.

Layer C must not own Context, classifier solutions, effect judgements, usage,
totality, proof claims, or evidence.

### Layer T: typing and theorem graph

Layer T owns Context-indexed judgements and their derivations:

- Context and Context morphisms;
- subject references to opaque Layer C term IDs;
- classifier, conversion, effect, usage, totality, and universe equations;
- dependencies between equations;
- proof/evidence nodes justified by solved equations;
- residual verification obligations.

Layer T may request a pure calculation from Layer C. It must not contain a
reducer, duplicate Core graph, or mutable copy of a Core answer.

### Exactly two semantic layers

The source reader, elaboration traversal, artifact encoder, and runtime driver
are processes, not additional semantic layers. A handoff packet is a transient
function argument, not a database or authority.

## 2. Measured Failure

Baseline on 2026-08-31:

| Item | Measured size |
|---|---:|
| Production C/header/include code | 221,626 lines |
| Tests | 48,062 lines |
| Old frontend lowering route | 41,622 lines |
| New `typing_*.c` route | 24,193 lines |
| New `typing_*.h` route | 5,188 lines |
| Boundary/state/topology/handoff modules | 82 files / 35,644 lines |
| Standalone C test executables | 84 |
| Shell test scripts | 60 |

Current deletion checkpoint on 2026-08-31:

| Item | Current size |
|---|---:|
| Production C/header/include code | 196,416 lines |
| Test C/header/include/shell code | 36,716 lines |

The deleted code includes the unused typing agenda, its scheduler-support state
islands, test-only T0 topology/state stores, and the semantic-index authority
that production built and validated without consulting. The production build
passes at this checkpoint. Type-inference examples also pass after removing two
mixed-authority paths:

- branch pullback already replaced the affected occurrence cells, but a second
  global classifier/motive reset discarded every solved equation;
- IH evidence was constructed in a materialized Context while effect-row
  evidence was independently searched in the old candidate Context.

The first path was deleted. The second now chooses the materialized Context once
and uses it for every premise of that judgement. No replacement phase flag or
compatibility branch was introduced.

The first R1 attempt added a standalone `kernel/typing/equation_graph` beside
the production `frontend/typing_constraint_state`. Audit showed that the new
graph had no production caller and represented the same Layer T equations a
second time. Its 846 implementation/test lines were deleted before R2. R1 now
means simplifying the production ConstraintDB in place; an isolated model graph
does not satisfy R1.

The in-place R1/R2 classifier cutover is now active:

- the base classifier constraint solution is the sole mutable classifier answer
  for an occurrence;
- the old occurrence classifier, stale-classifier, evidence, and state fields
  were deleted rather than retained as a mirror;
- the full-scan solution digest/revision bridge and its layout test were deleted;
- all constraint solution creation now uses one initializer, fixing a real bug
  where omitted Term IDs silently became term `0`;
- classifier, branch-refinement, computation, and effect constraints now enter
  through one append operation that records their owner TypedProjection and
  Context;
- dependency adjacency is now keyed by actual TypedProjection IDs rather than
  fields named TypedProjection that contained occurrence IDs;
- the remaining `classifier_solver` occurrence array contains binder solutions
  only and is named `binder_solutions` accordingly.

The C/T boundary test and the standard type-inference example manifest pass.
Dependent constructor tests still expose a missing Context-projection dependency
edge; no retry flag was added.

The next audit established that this is not a local missing wake-up. Three
mutable mechanisms represent one Context refinement:

1. candidate-to-materialized Context relocation plus dirty binding revisions;
2. Context projection solutions that may be sealed before binder equations
   close; and
3. final mutation/rebasing of occurrence, constraint, proposition, and proof
   Context roots.

A local attempt to keep projection answers provisional advanced the dependent
fixture past the original stale-projection failure, but then exposed the same
overlap in proof publication. That attempt was rolled back. The accepted repair
must remove candidate-to-materialized Context identity, not synchronize its
three copies.

Compound control conditions found by static scan:

| Logical operators in one `if`/loop condition | Production | Tests |
|---|---:|---:|
| 4 or more | 1,590 | 450 |
| 8 or more | 358 | 136 |
| 16 or more | 71 | 25 |
| 24 or more | 20 | 9 |
| 32 or more | 8 | 6 |

These are architecture findings, not formatting findings. The principal causes
are:

- one function validates several unrelated authorities;
- topology, mutable state, copied answers, and publication snapshots overlap;
- phases are represented by combinations of booleans and counters;
- parallel arrays require repeated cross-validation;
- a new route was added before the old route was deleted;
- test programs restate entire object layouts in long boolean expressions.

Moving a compound expression into a named predicate is not an accepted fix when
the underlying states remain independent.

## 3. Target Data Model

### 3.1 Layer C store

Keep one hash-consed Core graph. A constructor validates a node once and either
returns the existing ID or interns a new node. Consumers dispatch on the node
tag. They must not infer typing facts from Core shape.

### 3.2 Layer T equation graph

Replace facet-specific mutable stores with one graph whose node key is:

```text
EquationKey = {
    context,
    subject,
	judgement_kind
}
```

`subject` is a Context-indexed typed projection referencing an opaque Core term.
`judgement_kind` includes classifier, conversion, effect row, resource usage,
totality, universe, and proof obligation. These are distinct judgements, not
distinct solver databases. The rule kind and operands are the derivation recipe
owned by that one equation; they are not part of judgement identity. The current
synthetic fragment rejects two different recipes for one equation. When proof
alternatives are introduced, they become multiple derivation nodes under the
same equation and answer cell.

Each interned equation has exactly one answer cell:

```text
UNSEEN -> READY -> SOLVED
                 -> RESIDUAL
                 -> REJECTED
```

There is one dependency adjacency graph, one ready queue, and one SCC mechanism
for recursive equations. Match motives and computation-fold carriers are
ordinary cyclic equation components. They do not receive dedicated state DBs or
special classifier tags.

Rule evaluators remain explicit for `APP`, `MATCH`, induction hypotheses, and
computation folds because they justify different typing rules. They share graph
storage and scheduling; they are not encoded into one opaque generic rule.

### 3.3 Context

Use one persistent, hash-consed Context DAG. Extension is an intern operation.
Weakening, projection, substitution, and reindexing are Context morphism edges.
Materialization may be a temporary stack/local cache, but not a persistent
authority with its own answer lifecycle.

A Context extension stores the classifier equation ID for its binder, not a
copy of the current classifier answer. Therefore solving or refining that
equation never changes Context identity. A morphism is keyed by stable source
and target Context IDs plus its immutable action operands. Checking a morphism
may depend on classifier equations, but it does not create a replacement
Context or rewrite every root that mentioned the old one.

The binding equation owns the classifier family in its source Context. A
reindex action derives a classifier under one substitution, but that concrete
classifier is an input to the consuming typed-projection equation; it is not a
replacement answer for the binding equation. Thus two projections may own
`Partition A size` and `Partition A (succ size)` without mutating one shared
Context binding back and forth.

### 3.4 Evidence

Evidence is produced from solved equations and may exist in an open checkpoint.
Candidate and accepted forms must share one immutable premise representation.
Accepted replay checks the same evidence DAG and must not rebuild a second
candidate-shaped premise array. Sealing does not create or repair evidence; it
selects accepted roots and validates that every referenced equation/evidence
node is already replay-valid.

### 3.5 Compilation image and current publication prototype

The current v90 artifact is a **sealed publication prototype**, not the target
`.a` compilation image. It serializes:

- the retained Layer C arena and the accepted executable closure required by
  exports;
- accepted typing/proof publication records required to replay that closure;
- immutable diagnostics and publishable unresolved obligations.

It does not yet serialize the replay-complete Layer T seed, canonical generated
equations, or resumable current answer cells. It must not be reinterpreted as an
open checkpoint by adding optional fields around its solved-only assumptions.

The current `.ao`/`.apo` publication-only boundary is replaced, not wrapped. Reading
`.p` first lowers, without invoking the Solver, into one well-formed open `.a`
container containing the canonical Layer C graph and a replay-complete Layer T
seed equation graph. Solving advances that same container through canonical
equation transitions and may
intern further equations by semantic key. `--a-retention=recompute` retains the
seed while permitting canonical Solver expansion and answers to be regenerated;
`--a-retention=checkpoint` retains the same seed plus replay-validated expanded
equations, one current answer per equation, and derivations. A `.a` with a valid seal
certificate replaces the old publication artifact in one format
cutover; no dual-write compatibility path is retained. These are writer
profiles of one format and one Solver, not separate compiler paths.
Normalization caches, indexes, ready queues, and historical traversal cursors
are optional derived data and never become Authority. See
`2026-09-01T23-05-53-RESUMABLE-COMPILATION-GRAPH-AND-CHECKPOINT-PLAN.md`.

The checkpoint profile stores only the current replay-valid answer of each
canonical equation, plus an explicitly provisional current SCC candidate where
required. It does not retain superseded type calculations or a Solver history.
The recompute profile may drop reconstructible expanded equations together with
their answers, but never drops the seed required to intern them again.
Consequently both profiles resume through the same path; their only difference
is how much verified progress must be recomputed.

## 4. Control-Flow Rules

A long condition is an audit signal, not the defect by itself. The architectural
defect is that one branch decides among facts owned by different authorities,
phases, or lifecycles. For example, one condition must not simultaneously ask
whether a classifier cell is solved, a Context projection was rebuilt, a motive
revision changed, and publication already copied the answer. That condition is
evidence that the same semantic transition has several representations.

Conversely, a boundary constructor may reject several malformed fields in one
guard. Null, range, tag, and length checks over one input record are one
invariant and are not by themselves a reason to invent more modules or states.
The review therefore classifies operands by owner before changing code:

```text
operand -> mathematical fact -> unique authority -> lifecycle
```

If the operands name more than one authority or lifecycle, the repair must
remove an authority, add the missing equation dependency, or make the invalid
state unrepresentable. Extracting a predicate or splitting the expression is
not a repair.

Every current compound condition with four or more logical operators requires
this ownership review. Conditions with fewer operators are still defects when
they mix authorities. The accepted replacement depends on its cause:

| Cause | Required replacement |
|---|---|
| Invalid input combinations | validated constructor; invalid values cannot enter storage |
| Tag alternatives | exhaustive `switch` over one enum |
| Multiple phase flags | one state enum and one transition table |
| Parallel-array consistency | one row object or one graph node keyed by stable ID |
| Cache/source equality | one authority plus revisioned derived cache |
| Repeated lookup branches | hash-consed lookup-or-intern operation |
| Allocation rollback | one owner object and one transactional mark |
| Test expectation chain | data-driven expectation table with indexed failure report |
| Artifact field chain | schema-driven section decoder and per-record constructor |

Splitting one large condition into several adjacent `if` statements is only a
temporary diagnostic step. A phase is complete only when the invalid state
combination is structurally unrepresentable or belongs to one boundary parser.
Likewise, replacing `a && b && c` with `is_ready(x)` is forbidden unless the
underlying independent states have also disappeared.

Completion gate:

- every solver/elaborator branch has an ownership record in the static audit;
- no solver/elaborator branch reads mutable lifecycle state from two
  authorities;
- no solver/elaborator condition encodes a product of independent boolean
  phases;
- one semantic transition is represented by one enum transition or one
  equation answer, never a conjunction of copied state flags;
- loop bodies advance one work graph and do not rediscover the same state
  through a second scan;
- no function both mutates solver state and publishes artifact/evidence state;
- remaining four-or-more-term conditions are confined to validated input,
  schema decoding, or test expectation boundaries and are listed explicitly.

Operator counts remain a regression metric, but reaching zero by predicate
extraction does not satisfy this gate.

For solver, elaborator, reducer, Context, evidence, and publication code, every
`if`, `while`, or `for` condition containing `&&` or `||` is therefore an audit
item, regardless of its operator count. The default outcome is deletion of the
state distinction that requires the compound condition. A compound condition
may remain only when all operands validate one immutable input record owned by
one boundary constructor. The audit record must state that record and its sole
owner. In particular, the following are not acceptable terminal changes:

- extracting the expression into `is_ready`, `is_valid`, or a similar helper;
- splitting one mixed-authority expression into adjacent guards;
- adding a phase flag that makes one branch easier to select;
- adding a copied cache field so two authorities happen to agree;
- retrying the same semantic operation after mutating Context or classifier
  state.

The intended steady-state control flow is:

```text
parse/construct immutable input
    -> lookup-or-intern one graph node
    -> register dependency edges
    -> evaluate one explicit rule when its operands are ready
    -> commit one equation answer/evidence transition
```

Missing data is represented by an unsolved dependency, not by taking another
search path. Previously interned data is found by its canonical key, not by a
fallback scan followed by structural guesses.

## 5. Deletion-First Migration

The migration is performed by vertical semantic slices. A slice is not complete
until its previous implementation and micro-tests are deleted. Parallel old/new
routes are forbidden after the slice lands.

### R0. Reset the migration

- [x] Remove the rejected dedicated `CarrierState` module and its tests.
- [x] Delete the unused `typing_agenda` solver route and agenda-only tests.
- [x] Delete test-only topology, publication, materialization, and scheduler
  state islands that have no production caller.
- [x] Reduce `TypingPipeline` to state used by the production lowering route.
- [x] Remove the unused constraint semantic-index authority.
- [x] Delete the test-only standalone effect-equation solver superseded by the
  common T equation graph.
- [ ] Freeze additions to `typing_*_state`, handoff, schedule, and topology DBs.
- [ ] Record per-directory LOC, module count, test executable count, and compound
  condition count in a generated audit report.
- [ ] Identify all code introduced solely to keep old and new routes live.

Exit: no new compatibility boundary may land without deleting its predecessor in
the same change.

### R1. Build the minimal T equation graph

- [ ] Reduce the production `prototype_typing_constraint_db` itself to the
  equation key, immutable rule operands, one answer cell, dependency edges,
  ready queue, and SCC metadata. Do not construct a parallel graph.
- [ ] Use hash interning for equation identity.
- [ ] Provide one transaction mark for graph construction.
- [x] Do not add classifier-, effect-, Match-, carrier-, usage-, or universe-
  specific storage modules.
- [ ] Replace storage-layout tests with focused production-path tests for
  interning, dependency wake-up, SCC formation, residualization, and rejection.

Temporary allowance: R1 may add at most 3,000 production lines. R2 must remove
at least twice the R1 addition before another subsystem is added.

### R2. Cut over basic typing and delete duplicates

Move atoms, variables, intrinsic references, Pi, Lambda, APP, and CBPV boundaries
to the T equation graph.

- [ ] Replace Context classifier term/solution alternatives with one binder
  classifier equation reference. Context IDs must remain stable while that
  equation moves from pending to solved.
- [ ] Intern Context extensions by `(parent, binding, classifier_equation,
  extension_kind, producer)` and intern morphisms by stable Context IDs.
- [ ] Delete candidate-to-materialized Context relocation, dirty-binding lists,
  Context revision rescans, and mutable root rebasing in the same slice.
- [ ] Make Context projection an immutable morphism expression whose readiness
  is represented only by dependencies on equations. Do not give it a second
  answer lifecycle.
- [ ] Store an optional Context projection memo only with the exact parent and
  branch-refinement answer fingerprint that justified it. Answer lookup must
  not recursively rebuild a solved pullback. The memo is disposable and never
  becomes a Context or classifier Authority.
- [ ] Make propositions, proof candidates, occurrences, and constraints refer
  to the same stable Context IDs; finalization seals equations and evidence but
  does not rewrite Context roots.
- [ ] Replace ProjectionSolution classifier/effect/usage/totality answer copies.
  Classifier answer copies are removed; effect, usage, and totality remain.
- [x] Delete corresponding agenda phases and copied classifier revision fields.
- [ ] Delete migrated old-lowering fixed-point paths immediately.
- [ ] Preserve Layer C/T capability tests.

Exit: one semantic answer has one equation and one answer cell, and one source
typing occurrence has one stable Context-indexed identity from construction to
artifact publication.

### R3. Cut over ADT, IADT, Match, and induction

- [ ] Represent motive, index refinement, branch result, and IH premises as
  ordinary equations and dependencies.
- [ ] Use SCCs for recursive dependencies.
- [ ] Delete `typing_match_state` transactions, copied field results, replacement
  arrays, and pullback phase storage after equivalence tests pass.
- [ ] Keep explicit Match/IH rule evaluators.

Exit: Match has no private solver or answer authority.

### R4. Cut over effects, folds, usage, totality, and universes

- [ ] Represent effect unions and rows as interned terms/equations.
- [ ] Represent fold carrier recursion as an SCC, not `CarrierState`.
- [ ] Move usage, totality, and universe equations onto the common graph.
- [ ] Delete the corresponding state/solution modules and metadata answer copies.
- [ ] Preserve distinct operation semantics and visibility policy.

Exit: these theories have explicit rule kinds but no separate lifecycle engine.

### R5. Collapse Context machinery

- [ ] Complete any Context cleanup not already required by the R2 blocker.
- [ ] Verify Context extension remains lookup-or-intern.
- [ ] Verify weakening/projection/substitution/reindex remain morphism edges.
- [ ] Verify no persistent context materialization authority was reintroduced.
- [ ] Verify Context records contain equation references, not copied Core binder
  answers.
- [ ] Rebuild indexes once per committed transaction, not per query.

Exit: one Context DAG and one morphism graph remain.

### R6. Evidence and artifact cutover

- [ ] Generate evidence directly from solved equations through the common
  evaluator; permit replay-valid evidence in open checkpoints.
- [ ] Remove candidate-to-accepted premise reconstruction.
- [ ] Serialize replay seed and optional checkpoint progress independently from
  accepted executable roots; serialize a seal certificate only when all of its
  referenced C/T/evidence roots are retained.
- [ ] Delete snapshot structs that duplicate equation answers.
- [ ] Retain strict artifact replay and forgery tests.

Exit: publication and sealing cannot rediscover, solve, or mutate typing facts.

### R7. Remove the obsolete frontend route

- [ ] Delete migrated files under `frontend/lowering/constraint/`.
- [ ] Replace source schedule, lowering plan, handoff, and topology DB chains with
  one source traversal plus transient local work records.
- [ ] Delete legacy adapters and revision bridges.
- [ ] Reduce public frontend headers to semantic inputs and sealed outputs.

Exit: there is one elaboration path from source to C/T graphs.

### R8. Consolidate tests

- [ ] Replace one-executable-per-struct tests with table-driven suites for Core,
  typing equations, Context, evidence/artifact, and end-to-end examples.
- [ ] Keep boundary and negative-compilation tests as separate suites.
- [ ] Print fixture name and failed invariant rather than one giant boolean.
- [ ] Delete tests that assert private array offsets, counters, or migration-only
  phases.
- [ ] Preserve semantic tests for shared Core/different annotation, dependent
  Match, IADT/Acc, CBPV, effects, artifact replay, and higher identity fragments.

Exit targets: at most 20 C test executables, at most 15 shell drivers, and at
most 25,000 test lines.

## 6. Quantitative Gates

The target is simplification, not merely passing tests.

- Production code target after R7: at most 120,000 lines.
- Stretch target after artifact cleanup: at most 110,000 lines.
- Test code target after R8: at most 25,000 lines.
- Every phase after R1 must be net-negative in production LOC.
- Every migrated semantic slice must delete at least one obsolete module.
- No new persistent DB is accepted without identifying the unique mathematical
  object it owns and the old authority it deletes.
- Report added, deleted, and net lines by file and directory after every phase.
- Report file count, public-header count, test executable count, and compound
  condition counts after every phase.

LOC is not the semantic objective, but failure to reduce LOC indicates that the
old authority or migration scaffolding remains.

### 6.1 Preliminary module disposition

This table is a deletion map, not permission to keep wrappers with new names.
Each row is rechecked at the start of its vertical slice.

| Current cluster | Planned disposition |
|---|---|
| `typing_constraint_state`, `typing_agenda`, `typing_pipeline` | replace by the common T equation graph and a small elaboration owner |
| `typing_projection_state`, `typing_classifier_publication` | delete after answers live only in equation cells |
| `typing_effect_equation`, `typing_expected_effect_rows` | absorb equations into T; keep only effect-term constructors/rule evaluator |
| `typing_match_state`, `typing_match_specialization` | delete mutable transaction/result stores; keep explicit Match rule evaluator |
| `typing_universe_state` | absorb lifecycle and queue into T; keep universe arithmetic/solver algorithm |
| `typing_context_materialization`, both context-projection builders, `typing_substitution_action` | replace with Context DAG and morphism graph operations |
| `source_epoch`, `source_schedule`, `source_lowering_plan`, `source_core_handoff`, driver `typing_handoff` | delete persistent staging DBs; retain only local traversal records where measured necessary |
| `typing_publication` | retain as the read-only accepted/publication projection module; the former legacy adapter/state names are deleted |
| nominal/function/intrinsic/import/declaration state modules | merge immutable name/declaration records into one T environment; do not merge their distinct semantic record kinds |
| APP, Match, IH, computation-fold kernel rule files | retain as explicit theorem rules; simplify shared graph plumbing only |
| Layer C term graph and reducer | retain separately from T |

The largest immediate review targets are `typing_agenda.c` (6,365 lines),
`typing_pipeline.c` (3,063), `typing_match_state.c` (1,822),
`typing_expected_effect_rows.c` (1,712), `source_lowering_plan.c` (1,699), and
the old lowering route (41,622 total). Their size is not itself proof of
incorrectness, but they contain the highest concentration of overlapping phase
and validation logic.

## 7. Verification Order

For each vertical slice:

1. Run the static authority and compound-condition audit.
2. Run focused table-driven tests.
3. Run Layer C/T boundary tests.
4. Run examples 01 through 09.
5. Run dependent Match, IADT/Acc, CBPV/effect, artifact replay, and identity
   regression groups affected by the slice.
6. Compare performance and allocation counters with the previous commit.
7. Delete the superseded route and rerun the same tests.
8. Record LOC and module-count reduction.

The full slow suite runs only at a slice boundary, not after every small edit.

## 8. Explicit Rejections

The following approaches are rejected:

- one semantic graph that mixes calculation and typing;
- a dedicated DB for each judgement facet;
- dedicated carrier classifier tags or `CarrierState`;
- preserving old and new routes until the end of migration;
- adding handoff/snapshot objects to avoid changing an incorrect API;
- copying mutable answers into compile metadata;
- treating tests of private storage layout as semantic coverage;
- hiding compound conditions behind predicates without changing the data model;
- reducing APP, Match, IH, and computation-fold rules to one opaque generic rule;
- accepting code growth merely because each added boundary is locally coherent.

## 9. Immediate Next Work

R1/R2 is no longer at its initial cutover. Relation equations, aggregate answer
ownership, SCC scheduling, append-only interning, and the zero-transition
in-memory boundary are implemented. The authoritative detailed queue is
section 11.6; its current order is summarized here:

1. **Complete persistent identity.** Finish classifier RHS propagation, stable
   Context/action/equation keys, and image-wide semantic progress accounting.
2. **Separate solve from seal.** `solve(image, steps)` alone advances equations;
   `seal(image, roots)` is a read-only optional certificate operation.
3. **Add one sectioned `.a` format.** `RECOMPUTE` and `CHECKPOINT` are writer
   retention policies over the same image, not Solver or lifecycle modes.
4. **Verify resume equivalence.** Bounded repeated solving and uninterrupted
   solving must converge to the same canonical semantic graph from either
   retention profile.
5. **Cut over once.** Replace both `.ao` and `.apo` with `.a` across compile,
   import, link, aggregate, inspect, replay, backend, tests, and documentation;
   delete the legacy suffix paths.

The older detailed R2/R3 findings below remain requirements, but they do not
override this ordered queue. In particular, `.a` persistence may not begin by
serializing the current flat v90 closure, and the current artifact failure may
not be hidden by assigning arbitrary owners to source/provenance Matches.

The old `.apo` route is replaced by `.a`; it is not retained as a parallel
Authority. `RECOMPUTE` and `CHECKPOINT` serialize the same replay-complete seed.
They differ only in whether canonical Solver expansion, replay-valid current
answers, and provisional SCC candidates are retained. This is one writer
projection over one compilation image, not two in-memory solution stores or two
Solver entry points.

The `.p` reader always first produces that unsolved in-memory `.a` image. The
root equation/constraint graph is present in both storage profiles because it
is the replay-complete seed. `RECOMPUTE` may drop reconstructible expanded
equations and current answers and intentionally pays their computation cost
again; `CHECKPOINT` retains only canonical expansion, replay-valid current
answers, evidence, and provisional SCC candidates. It does not retain every
superseded type Term or the historical order of Solver steps.

The in-memory image does not have separate `RECOMPUTE` and `CHECKPOINT`
states. It always contains the progress made by the current Solver invocation.
Those names select only which optional sections a writer retains:

- `RECOMPUTE` writes the replay-complete seed and omits reconstructible
  expansion, answers, evidence, and caches;
- `CHECKPOINT` writes that identical seed plus canonical expanded equations,
  replay-valid current answers, accepted Derivations, residual obligations,
  and replay-checkable provisional SCC candidates;
- sealing is an independent read-only validation that may add a certificate
  and accepted publication closure. It is not a third Solver path.

Thus a checkpoint avoids recomputation without turning every historical type
candidate into permanent state. At most the current replay-valid answer per
canonical equation, and one explicitly provisional candidate where an SCC
requires it, may be retained. Superseded answers and worklist history are
disposable.

### 8.4 CG0.5 correction after the generated QuickSort publication trace

The 2026-09-02 implementation trace narrows item 13 further. The unresolved
classifier is not produced by the final Match alone. It originates in the
binder annotation

```text
graph : @quickSortAcc Nat @le size access input output
```

where the static value endpoint `quickSortAcc` is lowered to a Core Term before
its accepted projection exists. The current Match `HAS_TYPE` equation therefore
has zero operands, while the enclosing Pi equations retain a binder classifier
whose embedded endpoint has no edge back to the accepted `quickSortAcc`
projection. Re-running the surface type-expression AST at publication can repair
some top-level declarations, but cannot replay synthesized classifiers and is
not a valid `.a` authority.

CG0.5 is consequently split into these mandatory steps:

- [ ] **CG0.5a, typed static endpoints:** while building the solver-free seed,
  lower every value expression used inside a classifier through the ordinary
  typed occurrence/projection substrate. A static endpoint is not an opaque Term
  leaf. Its root is a typed operand with an exact Context action. Local variables
  remain Context bindings; top-level names retain the selected assignment
  occurrence. No Solver transition is allowed while these edges are emitted.
  **Current:** endpoint leaves and binder-annotation lowering exist. Propagation
  through every synthesized classifier and proof that this construction occurs
  before all Solver transitions remain incomplete.
- [ ] **CG0.5b, classifier expression equations:** represent APP, Pi/family,
  computation envelope, `Terminates`, accepted substitution, and reindex as
  interned Layer T equation constructors. Constructors are binary or unary, so
  arbitrary classifier expressions do not require a fixed-width list of hidden
  endpoints. The existing four-entry `operand_typed_projections` storage must
  not be enlarged as a workaround. Either migrate it to an interned variadic
  edge arena or let classifier-expression nodes supply ordinary bounded-arity
  edges; one physical representation must own dependency identity.
  **Current:** the fixed four-slot equation operand array and the separate
  recipe arena are gone. Supported bounded-arity RHS nodes are hash-consed in
  the common ConstraintDB arena and the owning equation carries one root.
  Computation envelopes and endpoint propagation through every synthesized
  classifier remain.
- [ ] **CG0.5c, one evaluator:** source solving, accepted projection, checkpoint
  resume, and sealing evaluate those same equations. Source Terms are immutable
  seed inputs, current answers are optional progress, and only accepted projected
  answers may become executable publication roots.
  **Current:** source Pi solving and accepted publication use the same
  evaluator. Resume and read-only seal validation do not yet enter through an
  image API, so this item remains open.
- [x] **CG0.5d, remove scaffolding:** delete
  `compile_type_expr_has_static_endpoint`,
  `compile_materialize_accepted_assignment_classifiers`,
  `static_endpoint_publication`, and the publication-only recursive classifier
  evaluator after the common equation evaluator covers their tests. These
  symbols and the separate classifier-expression store are absent.

- [x] **CG0.5e.1, accepted executable closure:** the accepted executable subset
  is independently rooted and validated inside the dense v90 arena. QuickSort
  write/read, checked projection, and malformed promotion/removal/child tests
  pass without promoting ownerless source Match cases.
- [x] **CG0.5e.2, open-image root roles:** complete the
  replacement of the flat closure walk with
  independent reachability for replay seed, retained checkpoint progress,
  optional source provenance, and accepted executable publication. In
  particular, `all-occurrence-metadata` must not root `source_core_term`,
  `source_classifier`, or source Context classifiers into the executable graph.
  A reference needed for replay belongs to seed/provenance; a reference needed
  for execution must be reached from an accepted projected Claim/evidence root.
  The strict unresolved-Match invariant applies to the latter only. The four
  in-memory root sets are now explicit sorted references over existing stores;
  section-local persistent relocation remains CG3 work.

The implementation must not add a Match-only reconstruction rule. A Match whose
motive is a classifier-expression equation will become reproducible by the same
dependency path; treating it specially would preserve the lost binder endpoint
and create another publication mode.

The acceptance boundary for CG0.5 is now explicit:

1. generated QuickSort writes and rereads an artifact without unresolved Match
   cases;
2. the ordinary dependent-Match fixture remains accepted, proving that inferred
   Pi reconstruction did not overwrite an explicit dependent classifier;
3. lowering the same source to an unsolved image and solving it in-place yields
   the same accepted publication as uninterrupted compilation;
4. no publication step reads the surface AST or mutates a source equation.

Current blockers to preserve as R2/R3 boundary tests:

- neutral nominal-family projection is still recomputed as a kernel query
  instead of being owned by an interned Layer T equation;
- generated dependent QuickSort output reaches the recursive graph constructor
  field with a guarded-recursive provisional motive whose indices still contain
  neutral Matches;
- the earlier indexed `LT` Match no longer fails at operation 60/projection
  1052 after parent expectation is propagated, but that relationship is not yet
  owned by equation topology. Its impossible descendants still need owned
  liveness equations;
- generated binder Context equations now outrank stale derived Lambda
  classifiers. Context-action dependency indexing removes the former enclosing
  branch DFS stall. Proof/evidence reads no longer invoke the Context Solver,
  and solved Context actions reuse an exact dependency-keyed projection memo.
  Generated QuickSort now reaches strict artifact writing in about four
  seconds. Projection-growth and repeated transition metrics remain gates for
  the later common-equation cutover, but they are no longer the current writer
  blocker;
- typed projection materialization can exceed the former fixed 8192 Context
  allocation. The temporary 32768 allocation is diagnostic headroom, not
  completion: the final gate measures Context/projection counts and intern
  reuse and removes redundant materialization.
- generated QuickSort now materializes the accepted projected classifier and
  proof. The former generic-metadata publication failure is fixed by explicit
  executable reachability: 25 ownerless auxiliary/source Match cases remain in
  the dense retained arena but stay outside the 52-case executable closure.
  This proves the accepted-publication role split, not the still-unimplemented
  seed/provenance/checkpoint section split;
- the first partial CG0.5 implementation separated publication construction
  from sealing and added checked classifier completion plus changed-tuple proof
  reification. It also proved that walking only surface classifier ASTs is not
  sufficient: generated QuickSort export occurrence 455 belongs to an ordinary
  synthesized assignment with no classifier type expression, while its inferred
  classifier transitively contains the source QuickSort endpoint. Static
  endpoint provenance must therefore be propagated by classifier equations.

None of these blockers is resolved by another boolean state or retry flag.
Missing dependencies become edges, and role-specific reachability replaces the
flat metadata walk.

The publication blocker establishes four roles that the single `.a` image
must represent explicitly:

1. **replay seed**: immutable Layer C topology, source equations, Context and
   Substitution actions, and source identity required to reproduce solving;
2. **source provenance**: optional source Claims/Derivations and unresolved
   explanatory topology, never an executable root;
3. **current Solver answer**: optional replay-validated checkpoint progress;
4. **sealed accepted projection**: concrete projected subject, classifier,
   Claim, and evidence eligible to become an executable root.

These roles may refer to one another through checked equation/projection keys,
but they are not interchangeable Authorities. In particular, a source-bearing
current answer may be valid in an open/checkpoint `.a` and still be unsuitable
as a sealed executable classifier.

The `.a` seed must also retain immutable inputs that are required to replay
an equation but are not answers. Constructor `declared_classifier` is the first
concrete case: an unreachable accepted occurrence has no published classifier,
yet a later Context projection may make it relevant. Recovering that nominal
classifier from shared Layer C Core shape is forbidden. It is serialized as an
occurrence input and consumed only when the constructor projection has no
accepted answer.

## 10. 2026-09-01 R2/R3 Checkpoint

The current implementation pass confirmed that compound control flow was
masking duplicated semantic routes rather than merely poor expression style.
The following changes are complete:

- Context extensions retain the binder classifier equation reference; frontend
  binder metadata no longer owns a copied classifier answer.
- Every source occurrence in a fresh typing pipeline receives one canonical
  equation projection, including accepted-prefix occurrences. Transaction
  boundaries limit new topology work but do not redefine equation identity.
- Contextual Match use-sites retain separate topology projections. Canonical
  equation identity and contextual branch placement are no longer selected by
  the same fallback rule.
- Empty source graphs are accepted as the unit of lowering. An import-only
  module no longer requires a fake term/type slot or a separate import path.
- Artifact publication relocates planned Context IDs to the actual interned
  compact Context IDs before serialization.
- Branch membership is read from immutable Layer T occurrence edges. The old
  reachability traversal revalidated each edge against the currently projected
  Layer C Core shape; after branch refinement, parent and child projection
  publication can occur at different times, so that second authority rejected a
  valid generated function graph. Core shape is no longer consulted to answer a
  Layer T topology question.

The last item fixed
`function_graph_dependent_output_ih_check.p`: a RETURN occurrence inside a
nested Match was a valid child in the source occurrence graph, while its current
Core term had not yet been republished with the parent Match projection. The
previous code treated this temporary cross-layer timing difference as malformed
branch topology.

The following work remains open and must not be hidden behind predicates:

- `operation_solver_reindex_in_enclosing_indexed_branches` still scans every
  Match and reconstructs enclosing refinements from Context ancestry plus
  occurrence reachability. The typed projection action chain already records
  this route. R3 must make that chain the sole source and delete the global scan,
  branch reachability matrix, and reindex revision cache together.
- `operation_occurrence_materialized_context` still has accepted-publication,
  occurrence-action, and candidate-materialization branches. These represent
  migration-era Context routes and remain part of the R2 deletion target.
- Several callers retry classifier application after ad hoc branch reindexing.
  Those retries must become ordinary equation dependencies and wake-ups before
  the old fixed-point path can be deleted.

Verification completed at this checkpoint:

- production prototype build;
- constraint authority checks;
- Layer C/T boundary checks;
- shared-Core/different-occurrence checks;
- Context projection and substitution immutability checks;
- incremental Context resolution checks;
- dependent Match source/publication/readback/runtime/negative checks;
- HOTT compile/publication/readback/forgery/link checks;
- shared-term HOTT substrate and alpha-slot checks;
- artifact flow, including replay and linking.

This checkpoint does not satisfy R3. It removes one false cross-layer authority
and establishes the regression tests needed to replace the remaining global
branch rediscovery route.

### 10.1 Binding identity correction

The indexed-family append regression exposed a data-model error in recursive
Match frames. One `ih_scope_id` had been used for two different objects:

1. the lexical identity of the recursive hypothesis binder; and
2. the operational Match frame specialized by substitution.

Substitution must create a distinct operational frame because its Match term is
specialized, but it must preserve the lexical binder identity. Comparing
canonical Match shapes is not a valid substitute: two independently declared,
same-shaped recursive Matches remain different binders.

The current correction adds `binding_scope_id` to the frame record. New frames
initially bind themselves; substitution clones retain the source
`binding_scope_id`; conversion compares that identity only when the references
are free. Paired and one-sided Match scopes are represented explicitly. A fake
mapping from a valid frame to `INVALID_ID`, and the former canonical-shape
fallback, are rejected.

The reference is now carried through Core import, structural reading, artifact
v90, checked-container encoding, checker validation, reachability marking, and
artifact compaction. Focused conversion-scope, indexed-family append, and
artifact-flow tests pass.

### 10.2 Control-flow defects found by the focused regressions

Post-synthesis expectation checking contained this order:

```text
require Core slot FORMED
then skip unchecked assignments during function-graph preflight
```

The skip decision necessarily precedes the normal-path state requirement. The
compound condition hid the reversed transition order. It has been changed to a
linear sequence: obtain the source result, apply the preflight transition once,
then validate the normal compiled state. The nominal-index constant-motive
fixture now compiles again. Surface `::` remains a post-synthesis check and does
not replace the synthesized classifier authority.

The next function-graph regression exposed a more fundamental violation.
`function_graph_expand_named_match_cases` mutates Match cases, appends binders,
rewrites role references, and sets `selectors_expanded` while compilation is
already consuming a sealed source-lowering plan. The source fingerprint check
correctly rejects this mutation. It must not be disabled or taught to ignore
the changed fields.

Required repair before continuing R3:

- [x] Resolve named-case selector interfaces before the generated source epoch
  is sealed, or store the resolved binder/role projection solely in Layer T.
- [x] Do not mutate source AST nodes, cases, binders, or role-reference tags from
  the compiler rule evaluator.
- [x] Make Match lowering consume one immutable elaborated source case. A second
  Layer T resolution record was rejected because it would duplicate the
  resolved binder/role projection.
- [x] Remove `selectors_expanded` as a compile-time phase flag after all named
  cases use the single resolution route.
- [x] Move direct certified-elimination expansion to the same pre-seal source
  elaboration route; it currently performs the same forbidden mutation.
- [x] Add a boundary test that fingerprints the generated source epoch before
  and after Layer C/T construction and requires exact equality.

This repair replaced the lazy mutation route without adding a preflight
exception to `prototype_source_lowering_plan_validate`. The generated epoch is
now fingerprinted after source elaboration and again after Layer C/T
construction. `test_function_graph_certified_execution.sh` passes local,
imported, named-case, dependent-output, and QuickSort graph cases.

### 10.3 Authority defects exposed after source immutability

Making source construction immutable exposed three later order dependencies.
All three were duplicate-authority defects rather than missing retry branches.

1. A solved classifier can be represented by a different interned Term ID that
   is DefEq to the former answer. DefEq validates semantic equality but does not
   make the new Term ID a second Authority. The owning equation records the
   representative transition and wakes direct dependents. It must not clear and
   rebuild the transitive closure merely because an interned representative
   changed.
2. An EXTEND substitution stores its classifier as part of its immutable
   identity. Evidence materialization had recomputed a current expected
   classifier and used that result as a second Claim authority. It now uses the
   stored classifier for the Claim and uses recomputation only as a coherence
   check.
3. Accepted substitution evidence was rediscovered by scanning all Claims.
   Equivalent derivations could therefore select a different Claim ID from the
   one already attached to the substitution. The accepted substitution Claim
   reference is now authoritative and is reused after certificate validation.

Dependent classifier formation now follows one path: record the declaration
head from `TypeDeclarationDB::formation_classifier`, then fold the APP spine
outward with `STATIC_FAMILY_APP_ELIM`. It no longer assumes that an unrelated
occurrence happened to materialize the declaration head first. Local generated
source reads constructor shape from its not-yet-compiled source declaration;
an imported association reads the already-accepted semantic schema. These are
different compile epochs and never compete as simultaneous authorities.

The artifact forgery test no longer hard-codes Proposition or Core Term IDs. It
starts from the exported certified term's evidence Claim, follows the stored
Claim-to-Proposition reference, and mutates that Proposition's Context.

### 10.4 Recursive Match binding is Layer C authority

The eager insertion regression exposed a cross-layer masking defect. A recursive
Match binds its source scrutinee name in every case body. At each IH unfold that
name denotes the current recursive argument. It is not an ordinary free use of
the enclosing Lambda binder.

Core substitution previously made this decision with two booleans derived from
the post-substitution scrutinee shape. Replacing the scrutinee with a concrete
value also replaced the recursively bound occurrences in every case body and
set the cloned IH frame's subject binder to `INVALID_ID`. Branch pullback in
Layer T later happened to specialize those bodies again, so valid evaluation
depended on typing order.

The Layer C rule is now explicit:

- a variable-to-variable substitution renames the recursive subject binder and
  its bound body occurrences together;
- a variable-to-value substitution changes the Match scrutinee but preserves
  the recursive subject binder in case bodies;
- free-binding analysis treats that subject as bound in case bodies and free in
  the scrutinee expression;
- same-store and cross-store alpha comparison, plus canonical hashing, enter
  the same subject-binder scope only after hashing the scrutinee;
- IH frame clones retain the lexical binding identity while owning their
  specialized operational Match.

`eager_insertion_check.p` again normalizes in 81 steps without relying on
branch-specific Core specialization. Recursive dependent-motive artifact
publication also passes with the same binding rule.

The permanent `term_identity_frame_check` boundary now verifies concrete
specialization, variable renaming, free-binding analysis, canonical closedness,
and Lambda closure from the same frame authority. Artifact proof relocation
dispatches once on `proof_kind`; one proof record no longer traverses duplicate
partial branches for its Match and motive references.

This does not finish the C/T separation. Typed occurrences still store both a
source Core term and a Context-reindexed subject Core term, and branch pullback
still publishes the latter by mutation. R3 must leave executable Core semantics
under the source Layer C graph and represent `t[σ]` as a Layer T subject action
or checked projection. It must not use Context pullback as an evaluator and must
not add a flag selecting source versus projected execution.

### 10.5 Compound control flow is an architecture failure signal

The current audit found approximately 13,600 source lines containing `&&` or
`||`. The count is not itself the acceptance criterion. It shows that local
rewriting cannot finish this migration. In solver, elaborator, Context,
evidence, and publication code, a compound condition is presumed to expose an
uncontrolled product of states until its operands are assigned to one immutable
record and one Authority.

The following change is explicitly insufficient:

```text
if (a && b && c) ...

becomes

if (!a) ...
if (!b) ...
if (!c) ...
```

That rewrite may improve an error message, but it does not complete any R-stage
unless `a`, `b`, and `c` are fields of one boundary input owned by one validated
constructor. Likewise, moving the expression into `is_ready()` or `is_valid()`
is not progress when the independent states remain representable.

The production classifier route currently demonstrates the rejected shape:

```text
solve branch-refinement state
    -> mutate projected fields on source occurrences
    -> refresh every classifier constraint payload and answer state
    -> restart the classifier solver
```

The branch decision must disappear, not be simplified. The accepted R2/R3
replacement is:

```text
intern TypedProjection equation
    -> intern projected child dependency edges
    -> solve branch-refinement dependency
    -> wake only dependent equations
    -> advance each owning equation through provisional and validated states
```

No source occurrence, Core node, or existing equation payload is rewritten by
that transition. There is no refresh pass and no solver restart path.

The 2026-09-02 QuickSort trace sharpened this rule. A Context projection update
currently calls recursive dependent invalidation and clears a complete
Match/IH/computation-fold/Lambda chain. The dependency graph lacks a complete
recursive back-edge, so the component cannot reconstruct itself. A temporary
reachability exception is not the fix. R3 must construct the missing equation
edges, derive SCCs from them, retain provisional candidates inside an SCC, and
wake direct dependents through one answer-transition API.

The first in-place cut is now partially complete:

- [x] Key the classifier answer lookup by `TypedProjectionId`, not occurrence.
- [x] Remove the duplicate `classifier_meta_id` payload; source occurrence is
  provenance, not a second classifier identity.
- [x] Delete the classifier-specific branch refresh function and topology-count
  reuse branch. Branch changes currently use the sole constraint constructor.
- [x] Remove copied `context_id` from immutable constraint edges. Context is
  read from the source/projection Authority at the point where a rule needs it.
- [x] Reattach derived computation lifecycle entries from the existing
  JudgementDelta computation record through one lookup-or-attach operation.
- [ ] Generate classifier rule operands under each owning TypedProjection.
- [ ] Resolve wrapped/child/Match-case operands through projected topology
  edges, never by falling back to an occurrence's canonical projection.
- [ ] Replace branch pullback mutation with a branch-refinement dependency and
  targeted equation wake-up.
- [ ] Replace `bind/replace/clear/reopen` answer mutations with one equation
  lifecycle transition and migrate every classifier producer to it.
  **Current:** classifier answer writes, bind/replace, and the separate premise
  reopen API are deleted. Answer final-state aggregation and non-ConstraintDB
  semantic domains remain.
- [ ] Record semantic transitions transactionally for every solution domain.
  **Current:** every central ConstraintDB lifecycle transition and every
  classifier answer mutation is covered. The open-image zero-transition
  invariant still cannot be asserted while occurrence-keyed usage answers,
  effect metas, universe work, equality work, and proof publication progress
  live outside that transition surface.
- [ ] Delete transitive classifier clearing; schedule explicit dependency SCCs
  without discarding already-derived recursive candidates.
- [ ] Delete `operation_solver_refresh_classifier_constraints`, global branch
  reachability caches, reindex revision scans, and solver restart labels.
- [ ] Remove the source/projected Core pair from persistent typed occurrences;
  retain one Layer C Core term and one Layer T action expression.

### 10.1 R3 authority finding on 2026-09-01

The indexed-family regressions exposed a stronger invariant than the original
checklist stated. `TypedProjection` topology already permits several Context
actions for one source occurrence, but classifier constraint generation still
selects one `base_typed_projection_for_occurrence`. Branch refinement then
rewrites that one solution and restarts generation. Consequently, projection
intern order can change which classifier is treated as canonical.

This is the direct cause of the remaining compound state predicates. They are
reconciling the product of these independently changing facts:

```text
source occurrence
base/canonical projection
branch-selected projection
Context classifier equation answer
restart/reset revision
```

The repair must be performed in the following order. Reordering these steps is
not allowed, because deleting a fallback before every projection owns an
equation makes graph insertion order observable.

- [ ] Generate one classifier equation for every reachable
  `TypedProjectionId`; delete `operation_solver_canonical_typed_projection` as
  an answer-selection operation.
- [ ] Store rule operands as projected topology edges. This includes CHILD,
  WRAPPED, MATCH_CASE, and the producer of a sequence-result binder.
- [ ] Index wake-up dependencies by operand `TypedProjectionId`, never by
  source occurrence followed by canonical projection lookup.
- [ ] Define a sequence-result binding classifier as the result projection of
  its producer equation. `Context.classifier_equation.answer` must not be a
  copied second answer for that binding.
- [ ] Remove `operation_type_metas_reset_after_context_action`, mutable answer
  replacement, branch restart, and all scans that publish VAR classifiers from
  source Context answers.
- [ ] Add a deterministic-order test that constructs the same projection graph
  with different root and intern orders and requires identical equation keys,
  answers, diagnostics, and artifacts.
- [ ] Add a generated QuickSort SCC test that refines a binder classifier after
  Match/fold candidates exist and requires targeted reevaluation without losing
  the outer Lambda classifier.

Constructor formation exposed a separate duplicate responsibility. The
frontend validates every field in its projected argument Context, while the
replay-facing kernel helper validates a complete constructor spine. The
frontend must call result formation after validation; replay retains the strict
complete-spine API. This distinction does not create two classifier
Authorities: both compute the result from the declaration telescope, while
only the caller responsible for untrusted input repeats domain validation.

- [x] Split checked constructor result formation from strict replay validation.
- [x] Remove the owner-domain/argument-domain reconciliation branch from the
  frontend constructor path.

Every deletion in this slice must have a focused semantic regression. The
minimum set is shared Core with different annotations, dependent Match,
recursive IH substitution, indexed-family append, generated dependent output,
QuickSort, and artifact replay. Static checks must reject reintroduction of the
deleted occurrence-keyed classifier answer and refresh/restart route.

Boundary record validation remains allowed to use a compound guard when all
operands validate one immutable record owned by that constructor. This exception
does not apply inside a solver work loop or an elaboration transition.

The refresh deletion temporarily regenerates the complete constraint graph when
branch pullback mutates an occurrence. This is one construction Authority, but
it is not the target algorithm: the QuickSort dependency-closure phase increased
to approximately 14.3 seconds in the focused integration run. R3 is not complete
until branch pullback mutation and the restart label are replaced by targeted
dependency wake-up, recovering the prior performance or improving it. The
static constraint-authority test rejects reintroduction of the deleted refresh
and topology-count routes.

## 11. 2026-09-01 Compound-Control-Flow Gate

Complex `&&` and `||` expressions in this codebase are treated as an
architecture audit signal, not primarily as a formatting problem. A static
baseline found 2,941 physical source lines containing at least two logical
operators. The largest concentrations are in accepted replay, graph
construction, function-graph construction, Identity actions, checker sessions,
Context/type lowering, motive solving, and conversion.

The count is deliberately broader than the set of defects. A compound condition
is accepted only after it has one of the following explicit dispositions:

1. **Boundary validation.** Every operand validates fields of one immutable
   input record owned by one constructor. Ordered guards may be used to retain
   precise diagnostics.
2. **Variant discrimination.** The condition is replaced by exhaustive dispatch
   over one semantic variant. Boolean products must not encode an implicit sum
   type.
3. **State transition.** The condition becomes one transition of the interned
   equation/dependency graph. Retry, revision, refresh, and fallback flags are
   removed rather than renamed.
4. **Lookup fallback.** Repeated scans and path-dependent discovery are replaced
   by lookup-or-intern on the semantic key.
5. **Cross-Authority reconciliation.** One answer store is deleted. A predicate
   asserting that two mutable answers currently agree does not establish a
   single Authority.

Extracting `is_ready()`, splitting a condition into several `if` statements, or
adding another phase enum does not satisfy this gate while the original product
of independently mutable states remains representable.

### 11.1 Accepted publication correction

The artifact/replay path exposed a concrete instance of the fifth category.
Raw occurrence Context and accepted publication Context had been stored through
the same occurrence field. Replay then applied the accepted substitution to an
already projected subject and classifier. Correctness depended on which path
last overwrote the occurrence.

The model is now:

```text
SourceOccurrence
    immutable source topology
    source Context

AcceptedPublication
    publication Context
    source-to-publication Substitution
    published subject
    published classifier
```

The accepted publication judgement is the sole accepted typing answer. Source
Context remains provenance and is never overwritten by acceptance. Wire v90,
dense publication, artifact closure, reader replay, and linked replay preserve
both roles explicitly.

- [x] Preserve immutable source occurrence and Match-case Contexts.
- [x] Publish accepted Context through the typed publication judgement.
- [x] Derive publication Context from the publication substitution source.
- [x] Stop replay from reindexing an already published classifier.
- [x] Mark source and publication Contexts independently in artifact closure.
- [x] Add both Context roles to artifact v90 and update its fingerprint.
- [x] Remove parallel accepted-replay Context/subject/classifier arrays in favor
  of one publication view.

### 11.2 Remaining R3 failures are not fallback requests

Focused source/publication boundary tests pass, including Context projection,
shared erased Core occurrences, generated dependent function graphs, direct
IF8/Acc solving, generated QuickSort source solving, checked-Core validation,
strict artifact write/read, and checked backend projection. Executable closure
now excludes retained unresolved source/provenance Match cases through an
explicit reachability role; forged promotion of those cases is rejected.

The remaining R3 failure is above that physical role boundary. Static endpoint
RHS nodes, source equation solving, and accepted publication now share the
common representation and evaluator, but synthesized classifiers still lose
some endpoint dependencies and loaded-image resume does not yet exist. This
must be repaired by completing projected operands under their owning
`TypedProjectionId`. It must not be
repaired by restoring global refresh, solver restart, canonical-projection
fallback, AST recompilation, or source/publication field mixing.

### 11.3 Completion criterion

No R-stage is complete merely because its compound-condition count decreased.
Completion requires all of the following:

- each semantic answer has one Authority;
- each state is interned or derived, not rediscovered through alternate paths;
- each dependency wake-up is targeted and monotone;
- Layer C retains one erased computation term;
- Layer T retains Context-indexed typing, proof, and projection facts;
- unsupported states remain explicit residuals instead of entering fallback
  branches;
- focused semantic tests and the full accepted test suite pass without a retry
  or compatibility route.

### 11.4 Closed Context actions precede Layer T fixed points

The 2026-09-02 generated QuickSort audit found that classifier conversion was
being used while IH-scope identity was still mutable. The same pair of
projection endpoints could compare equal before scope completion and unequal
after it. Therefore the single path is now fixed as:

```text
Layer C/source graph construction
  -> close seed Context/Substitution/IH-scope action topology
  -> intern root Layer T equations, dependencies, and SCC identity
  -> one open, zero-transition in-memory .a image
  -> solve the same image under an effort budget
  -> optional RECOMPUTE or CHECKPOINT .a write at any point
  -> optional read-only accepted-root seal certificate
```

Layer C and Layer T remain separate graphs. This ordering does not merge type
information into Core computation. It only prevents Layer T from treating a
mutable Context action as a stable conversion premise.

The topology-closing lines are part of solver-free image construction. They
intern immutable actions, equations, dependencies, and SCC identity; they do
not advance an answer lifecycle. Any Context action whose result requires type
inference remains an unsolved Layer T equation. This distinction prevents
"solver-free" from being misread as "omit the equation graph" or as "run a
small hidden pre-solver during lowering".

- [ ] Remove universal constraint-owner self-dependencies after every hidden
  owner read is an operand; owner is output. The early removal experiment was
  rolled back because dependent motive tests exposed those hidden reads.
- [ ] Declare every genuine owner read as an explicit operand.
- [ ] Seal Context/Substitution/IH-scope action identity before final DefEq.
- [ ] Replace per-edge destructive motive reopening with Match/IH/fold SCCs.
- [x] Remove the temporary exact/provisional classifier dual Authority.
- [x] Stop generated QuickSort from overwriting one source binding equation
  with projection-specific `Partition A size` and `Partition A (succ size)`
  answers. Its typed publication now passes checked-Core validation.
- [ ] Finish the same closed Context-action fingerprint rule for every remaining
  answer producer; the generated path is evidence that the rule works, not a
  proof that all producers have migrated.
- [ ] Require checkpointed answers to carry and replay-validate the closed
  Context-action key and exact direct-input revision snapshot. Hashes may index
  lookup, but never validate an answer by themselves.

The `.p` reader and the `.a` reader must converge on the same in-memory image
before the first Solver transition. `.p` contributes source-derived immutable
seed topology. `.a` contributes that same seed topology plus zero or more
replay-validated progress sections. Neither route may call a different Solver
or populate a second answer store.

The old `.ao`/`.apo` publication format is replaced by `.a` only after this image
boundary exists. A `RECOMPUTE` write drops reconstructible expanded equations,
answers, evidence, and caches. A `CHECKPOINT` write retains the replay-valid
subset of those records. Both retain the same semantic seed and therefore
produce the same final program when solved with sufficient effort. The choice
is a compiler storage option, not a semantic mode or a control-flow branch in
typing. A seal is another optional section over the current accepted roots; it
does not advance or transform the image.

### 11.5 Source provenance is not executable publication

The 2026-09-02 artifact audit exposed the next cross-Authority boundary. The
v90 closure roots an accepted typed export, recursively follows its accepted
Claim and Derivations, and copies every referenced source Term into the same
graph. Generated QuickSort then fails graph writing because valid source
provenance contains unresolved Match constructor correspondence even though
the accepted typed projection is concrete.

The single-path correction is:

```text
source occurrence/proof provenance
  -> validated source-to-projection bridge
  -> accepted Context-indexed subject/classifier/evidence
  -> sealed executable closure
```

Source provenance and accepted publication remain sections of one compilation
image, but only the accepted projection is an executable root. The implementation
must not mutate source Match cases, weaken sealed graph validation, or replace
the exported Term by another allocation sharing a canonical key.

- [x] Make exports retain their exact typed-publication tuple.
- [x] Make termination publication consume projected children and classifiers.
- [x] Split publication construction from sealing and add checked accepted
  classifier completion.
- [x] Restrict projected proof reification to tuples that differ from their
  source answer.
- [x] Replace the diagnostic AST-only endpoint reconstruction with static
  endpoint operands on the common classifier equation graph. Every classifier
  constructor and reindex rule must preserve these dependencies.
- [ ] Lower classifier-contained value expressions to typed endpoint operands;
  do not retain a bare Core Term as their only replay input.
- [x] Intern bounded-arity classifier-expression equations (or first migrate all
  equation operands to one variadic edge arena). Do not extend the inline
  four-operand array as an endpoint-count limit.
- [ ] Use the common subject/classifier evaluator from resume and seal. The
  publication-only evaluator and separate recipe store are already deleted.
- [ ] Reify projected Claims/Derivations under `TypedProjectionId` for every
  classifier changed by accepted equation evaluation.
- [ ] Keep source Claims in an optional non-executable provenance section.
- [ ] Make sealing a read-only projection and bridge validation, never a Solver.
- [x] Pass generated QuickSort artifact write/read/replay and full artifact flow.

This correction is CG0.5 in the resumable `.a` plan. CG1 starts only after it,
so the new file format does not freeze the current flat-closure mistake.

### 11.6 Revised immediate implementation order

The fixed four-slot constraint operand representation, separate
`classifier_expressions` arena, occurrence root map, and publication-only
evaluator have been removed. Remaining work extends that one representation
rather than introducing another store:

1. [x] Migrate every constraint operand to one immutable variadic edge arena. An
   edge records operand role and referenced Layer T equation/projection; no
   equation kind may retain an inline operand copy.
2. [x] Remove foreign classifier-answer producers as one atomic relation/SCC
   migration. `PI_DOMAIN_RELATION` and `SEQUENCE_BINDER_RELATION` own their
   validation results, expected refinement targets the canonical
   `BINDER_TYPE` premise, `PROJECTED_CLASSIFIER` is the sole answer writer for
   each projection, and Match/IH/fold recursion is scheduled by derived SCCs.
   A relation may wake the owner but never publishes into another equation's
   answer.
3. [ ] Intern literal, endpoint, APP, substitution/reindex, thunk, computation
   envelope, and `Terminates` classifier constructors as ordinary Layer T
   equation nodes in that graph. A static endpoint is a typed occurrence edge,
   never a bare Layer C Term discovered later. **Current:** literal, endpoint,
   value/type APP, accepted substitution, thunk, and `Terminates` are interned;
   computation envelopes and propagation through every synthesized classifier
   remain.
4. [x] Attach the classifier-expression root to the owning classifier equation.
   Remove `binder_classifier_expression_for_occurrence` as a second root map.
5. [ ] Introduce one evaluator over immutable equation nodes. Its read-only
   projection environment selects source/current or accepted endpoint answers;
   its reduction rules and dependency graph are identical in solve, resume,
   accepted projection, and seal validation. **Current:** source Pi solving and
   accepted projection call the same evaluator; resume and seal are pending.
6. [x] Route source Pi answer production and accepted classifier projection
   through the common evaluator, then delete the separate
   `classifier_expressions` store, occurrence root map, and publication-only
   recursive classifier evaluator. Remaining synthesized classifier kinds are
   covered by item 3 rather than by preserving another evaluator.
7. [~] Complete equation semantics before persistence. The one physical
   classifier-answer transition is in place and old bind/replace writers are
   deleted. Immutable constraints now pass through one structured
   lookup-or-intern operation; effect generation no longer has a separate
   linear duplicate scan, provenance does not alter equation identity, and the
   hash index is rebuilt on transaction rollback. Items (a)-(c), relation
   ownership, aggregate premise ownership, and derived SCC scheduling, are
   complete. Remaining before persistence are (d) aggregate final lifecycle
   state and (e) replace allocation-local equation/Term IDs in equation keys
   with stable semantic inputs, close exact Context/Substitution/IH-action
   identity, and add projection-liveness equations. A retained answer must be
   valid under one canonical equation key and one exact direct-input snapshot.
8. [x] The non-copying compilation-image owner and explicit in-memory boundary
   are implemented. `lower(image)` builds Layer C plus Layer T
   occurrence/projection topology and returns an open image with zero semantic
   transitions. `solve(image, effort)` advances the same owner; the ordinary
   compiler calls these two entries in sequence, so no second compiler path was
   added. Replayable root-equation expansion now occurs during lowering as a
   topology-only operation. Input fact seeding and all answer transitions stay
   behind `solve(image, effort)`. A `RECOMPUTE` image may retain only this seed
   frontier; a `CHECKPOINT` image may additionally retain later canonical
   expansion and current answers. Neither profile changes solving semantics.
   **Transitional limitation:** the owner still retains AST/source-plan/result
   allocations until destruction, although Solver access to them is severed.
   The shared Lowering effort argument and the conflated `SEALED` lifecycle
   state have been removed.
9. [x] Make equation expansion append-only and resume-safe. Move
   `prototype_typing_constraint_db_init()` to image/pipeline construction,
   split immutable topology expansion from mutable input evaluation, and
   delete `operation_solver_reset_classifier_state()` as a regeneration
   mechanism. Before doing so, remove the physical `first_* + count` domain
   ranges: they require all classifier equations to precede every branch,
   computation, and effect equation, which is incompatible with interleaved
   append-only growth. Replace them with one general per-domain linked/indexed
   iteration mechanism derived from the canonical equation arena. Also key the
   unique `PROJECTED_CLASSIFIER` owner by projection identity and let new
   premise edges extend its dependency set; do not intern a second aggregate
   merely because a later solve step discovered another premise. Its answer
   records the exact dependency-set revision and is requeued when that set
   grows. Repeating expansion with unchanged roots must reuse every EquationId
   and preserve validated answers.
10. [x] Make the open image self-contained without preserving the frontend
   AST as a semantic Solver input. Source-derived immutable goals and facts now
   live in `typing_pipeline.seed`. Match/import/binder/declaration goals and
   top-level publication/`::` expectation inputs have been migrated. Lambda
   classifier RHS recipes are materialized once into binder seed roots. Match
   binder identity and motive equations use Core/TypedOccurrence records, and
   constructor saturation uses publication seed roots. The open-image boundary
   disconnects AST/source-plan/source-result pointers before solve; remaining
   AST reads are optional diagnostics only. Physical source allocations remain
   owned until image destruction and may be freed earlier after rollback
   ownership is separated.
11. [x] Separate solving, structural limits, and sealing. The effort argument
   has been removed from atomic `lower(image)`, and only
   `solve(image, steps)` adds or consumes compile effort. A successful fixed
   point now records `SOLVED`, not `SEALED`. The read-only
   `seal(image)` operation validates the committed ConstraintDB topology and
   frozen typed-occurrence boundary and returns a certificate over the current
   accepted image without interning equations, advancing answers, or mutating
   lifecycle/revisions. Repeated-seal stability is covered by the compilation
   image test. The CLI now exposes `--solve-effort STEPS`; a sufficient budget
   accepts `examples/01_bool.p`, while zero steps pauses with the canonical
   Solver-limit diagnostic. Item 12 now binds the certificate to accepted roots
   without changing Solver/seal separation.
12. [x] Add independent seed, provenance, checkpoint-progress, and accepted-root
   reachability over that one image. These are root roles, not duplicated
   arenas or in-memory stages. Record the replay seed frontier at the end of
   `lower`; derive checkpoint reachability from retained canonical equations;
   derive accepted reachability from typed publication and accepted Claims;
   keep optional source provenance non-executable. The read-only seal now names
   the accepted-root set and hashes its transitive closed Claim/Derivation DAG.
   The four sets are sorted references into existing Authority databases, not
   copied semantic records. Their numeric IDs remain image-local; the `.a`
   writer must encode the referenced graph canonically before persistence.
13. [x] Remove semantic Solver state from
   `compile_context.legacy_typing` before defining the wire format. Classify
   every field by one of four dispositions and do not serialize the legacy
   struct:

   - immutable replay input moves to `typing_pipeline.seed` or an immutable
     equation operand;
   - one current semantic answer moves to the solution cell of its canonical
     classifier/effect/usage/evidence equation;
   - dependency maps, occurrence indexes, SCCs, and ready-frontier data become
     deterministic rebuildable indexes;
   - reification/materialization cursors, revisions, active IDs, retry state,
     and queues remain disposable invocation workspace.

   Concretely, migrate `classifier_seeds`, solver input facts, expected-codomain
   and motive/IH relations, occurrence usage answers, effect metas, and accepted
   evidence ownership. Delete or reduce `prototype_typing_pipeline_legacy_state`
   to a rebuildable runtime workspace after no semantic result depends on it.
   Loading a `.a` must be able to construct a valid image without allocating a
   source AST or restoring an opaque snapshot of this struct.

   **Completed:** `classifier_seeds`, binder/declaration/import/Match/publication
   inputs, and exact termination-witness roots are immutable replay seed data.
   The derived `input_facts[]` copy and never-written suppression state were
   deleted. Match/IH, classifier, effect, and refinement answers reside in
   canonical equation solution cells. Usage analysis and proof reification are
   rebuildable/disposable workspaces; accepted evidence is owned by
   `JudgementDB`. The `prototype_typing_pipeline_legacy_state` wrapper and its
   legacy-named adapter were removed rather than serialized.
14. [ ] Add one `.a` reader/writer. `RECOMPUTE` and `CHECKPOINT` are retention
   profiles of this writer, never producer/Solver modes. Verify that a zero-progress image, a partial
   image, and a solved image can each be written as either `RECOMPUTE` or
   `CHECKPOINT`, then replace both legacy `.ao`/`.apo` suffix aliases in one
   cutover. Expose the choice as a writer option such as
   `--a-retention=recompute|checkpoint`; neither source lowering nor
   `solve(image, steps)` may branch on it. `RECOMPUTE` emits the replay-complete
   seed only. `CHECKPOINT` emits that same seed plus canonical expansion and one
   replay-valid current state per equation. A missing optional section rebuilds
   work; it never selects a fallback semantic path. Replace the current per-source `.ao`
   and aggregate `.apo` suffix conventions together; do not retain either as a
   second semantic format or compatibility reader. Linking and aggregation
   become operations over one or more `.a` images and write the same `.a`
   container kind.
15. [ ] Cut over all CLI and tests to `.a` once. No suffix or command branch in
   the final implementation may select a different Solver. Delete both legacy
   `.ao` and `.apo` readers/writers after open/partial/sealed roundtrip, resume,
   link, aggregate, inspect, and backend tests pass through the one loader.

The next implementation order is now fixed:

1. [done] finish item 13 by extracting semantic progress from the legacy
   wrapper and classifying every remaining array as seed, canonical answer,
   rebuildable index, or disposable workspace;
2. finish item 7 with persistence-stable keys and exact Context/Substitution/
   IH action identity, without replacing fast in-memory IDs;
3. [done] explicit root roles and accepted-root-bound read-only seals;
4. define and implement the one sectioned `.a` schema, writer, and loader;
5. rebuild indexes, SCCs, and the ready frontier after load, then test bounded
   resume against uninterrupted solving;
6. remove `.ao` and `.apo` together and move compile, import, link, aggregate,
   inspect, replay, and backend tests to `.a`.

The persistent seed records are owned by Layer T, source compilation solves only
after frontend source pointers have been disconnected, root roles are explicit,
and sealing is non-mutating. No legacy wrapper remains as a semantic owner.
Stable keys must now be completed before the section writer is introduced.
Source lowering and `.a` loading must establish the same open-image invariant
before either enters `solve(image, steps)`.

This is an in-place replacement, not an adapter. Temporary dual writes are
allowed only inside one patch while tests are being moved and must not remain at
the completion checkpoint.
