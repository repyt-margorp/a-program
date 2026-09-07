# Resumable Compilation Graph and Checkpoint Plan

Date: 2026-09-01
Status: active; in-memory image/root boundary implemented, persistent-state extraction pending
Last revised: 2026-09-03T11:06:00+09:00

## Current Implementation State

As of 2026-09-03:

- [x] Layer C TermDB and Context-indexed Layer T structures exist separately.
- [x] Classifier, computation, effect, and related constraints have canonical
  IDs in the central typing pipeline for the migrated fragment.
- [x] Source occurrence and accepted publication projections are distinct.
- [x] Source-derived unresolved Match/import goals and binder/declaration
  inputs are image-owned Layer T seed data. The transient compile invocation no
  longer owns duplicate arrays, and persisted import references use an import
  table index rather than a raw pointer.
- [x] Top-level publication and `::` expectation inputs are also image-owned
  seed data. Final expectation checking and label publication consume those
  records rather than searching the AST/source-result tables again.
- [x] Artifact v90 serializes and replays a strict solved closure.
- [ ] Artifact v90 is not an open `.a` image. It does not persist the
  replay-complete Layer T seed, canonical Solver expansion, or resumable answer
  cells. The source lowerer can now stop before its first Solver transition,
  but that in-memory boundary has no `.a` reader/writer yet.
- [x] The in-memory open-image boundary disconnects AST, source schedule,
  source-lowering plan, source Core handoff, and source typing-result pointers
  from the Solver context. Lambda annotation replay recipes and top-level
  publication/expectation roots are retained in Layer T seed records instead.
- [x] Context source equations are the Authority; reindex equations are derived
  projections in the corrected indexed-family path.
- [x] A zero-clause computation fold over an induction hypothesis records the
  IH recursive argument as an explicit equation dependency. The Solver no
  longer relies on a hidden transitive rescan to seed the indexed motive from
  the continuation domain.
- [x] Classifier answers are semantically owned by the aggregate equation for a
  `TypedProjectionId`. PI-domain and sequence-binder relations retain their own
  lifecycle, expected refinement targets the canonical binder premise, and no
  relation equation publishes into a foreign projection answer.
- [x] Destructive transitive classifier invalidation and
  `stale_result_term` have been removed. Answer changes wake direct equation
  dependents instead of erasing the dependent closure.
- [x] Every physical classifier-answer write uses one equation transition API.
  The former bind/replace writer APIs and direct classifier result writes are
  removed. Publish, input-change, and discard now share the owning equation
  transition, and unchanged DefEq answers do not select a representative by
  allocation-local numeric Term ID.
- [ ] Semantic transition accounting is only partially image-wide. The
  ConstraintDB owns transactional total/per-domain counters; every central
  premise lifecycle transition/reopen is keyed by its owning EquationId, and
  the classifier transition counts actual answer or validation-state changes.
  Occurrence-keyed usage answers, effect metas, universe work, equality work,
  and proof publication still need to enter the image-wide progress manifest.
  The source boundary already proves zero ConstraintDB transitions and invokes
  none of those solvers; the remaining work is to make later progress reportable
  and persistable through one manifest.
- [x] Premise lifecycle mutation has one API. The former `reopen` operation is
  deleted; direct final-state replacement counts once, and entering `PENDING`
  clears stale evidence in the same transition.
- [x] The compilation-image boundary test distinguishes semantic state from
  execution history. `PENDING` and `INCOMPLETE_BUDGET` are scheduler/frontier
  states and do not increment progress. Bounded and uninterrupted runs must
  converge to identical equation topology and solution cells; their count of
  visited intermediate answers is not itself semantic Authority.
- [x] Immutable Layer T constraints now use one structured lookup-or-intern
  path. Re-discovering the same semantic payload and operand list returns the
  existing EquationId; source AST provenance is not part of identity. The old
  effect-equation linear duplicate scan is deleted, rollback rebuilds the hash
  index, and sealed topology still permits lookup of an existing node while
  rejecting creation of a new one.
- [x] Equation creation APIs use the `intern` contract. No migrated caller may
  treat equation discovery as append-only storage or perform a private linear
  duplicate scan.
- [ ] The current constraint hash is an in-image canonical index, not yet a
  persistent semantic key. It still contains allocation-local
  TypedProjection, origin Equation, classifier-RHS, and operand IDs. These must
  be replaced or relocation-validated by recursively stable keys before a
  `CHECKPOINT` answer may trust the equation identity.
- [ ] Transition identity is not yet persistence-ready. Remaining producers must
  use exact direct operand and Context-action semantic keys; raw equation IDs,
  result Term IDs, a generic Solver revision, and a solution cell's own derived
  output are not stable replay inputs. Final lifecycle aggregation is also
  incomplete.
- [x] Projected binder and zero-clause fold propagation terminate on the
  incompatible external-witness path after APP domain comparison is made
  read-only. Exact answer snapshots correctly exclude the solution cell's
  derived output and unchanged direct inputs are no-ops. The same cut exposes
  the positive Acc eliminator's cyclic motive/IH/fold/domain dependency. The
  PI-domain and fold/binder relations are now explicit operands in the derived
  SCC. Missing and forged rigid arguments produce
  `classifier-equation-contradiction`; a correctly typed but opaque external
  decrease term reaches the more precise `motive-equation-mismatch` because it
  cannot supply the required structural recursive relation.
- [x] Recursive Match/IH/fold dependencies form explicit SCCs. The former
  QuickSort answer-loss, stale totality-endpoint, and raw branch-classifier
  failures are fixed, and SCC topology is derived from immutable equation
  dependencies. A pure kernel
  projection can derive the common nominal family of a generated neutral Match,
  but that projection is not yet owned by an equation with explicit branch
  dependencies.
- [x] Constructor occurrences retain their immutable declaration classifier.
  A later Context projection therefore does not recover a nominal TypeView from
  shared Layer C shape or from a previous Solver answer.
- [ ] A live all-impossible Match can derive its motive/result classifier from
  its parent expected-classifier equation. The PI-introduction/Match path now
  propagates this expectation and passes the former projection 1052 blocker,
  but the dependency is still procedural rather than an immutable equation
  operand. Impossible branch bodies and the Match result therefore remain only
  partially separated.
- [ ] Projection liveness is an equation domain derived from exact projected
  branch-refinement equations. Enclosing branch-refinement prerequisites are
  now indexed from the immutable Context action chain, but live/dead answers
  are not yet their own equation domain.
- [x] A solved Context projection has a dependency-keyed memo over its parent
  Context/substitution and exact branch-refinement answer. Proof/evidence reads
  use a read-only answer API. This memo is derived state, not a second Context
  Authority.
- [x] Accepted proof publication distinguishes executable publication roots
  from projection-local proof Authority. A dependent Match premise is checked
  against the frozen Layer T equation answer for its exact occurrence,
  Context, subject, and classifier; selecting one executable projection no
  longer discards another solved projection needed by the proof DAG.
- [x] The temporary `exact_result_term` mirror has been removed. One equation
  cell now has one physical classifier-answer field again.
- [x] The generated QuickSort path no longer overwrites one immutable source
  binding equation with projection-specific classifiers. Its source contract,
  typed projections, dependent Match motive, and checked-Core conversion now
  reach accepted publication.
- [x] Generated QuickSort now reaches accepted publication with a projected
  subject, projected classifier, and accepted proof. The earlier occurrence 455
  source-bearing classifier failure has been crossed by the first
  classifier-expression recipe and accepted evaluator.
- [x] The separate classifier-expression database and occurrence-root map have
  been removed. Supported classifier RHS nodes use the ConstraintDB's immutable
  variadic operand arena, and the owning Pi equation stores the root directly.
- [ ] Common RHS migration is not complete. Source Pi solving and accepted
  projection share one evaluator, but synthesized Match/Pi/reindex/computation
  classifiers do not yet all propagate endpoint operands and future resume/seal
  entry points are not connected.
- [x] The strict artifact writer separates accepted executable reachability
  from retained source/provenance topology. Generated QuickSort persists one
  dense arena with 52 executable resolved Match cases and 25 non-executable
  ownerless source cases, then passes readback and checked projection. Missing
  roots/children and forged source-case promotion are rejected.
- [x] Replay seed, checkpoint progress, optional source provenance, and accepted
  publication have independent sorted root-reference sets over the same image.
  They do not copy Term, equation, projection, declaration, Claim, or Derivation
  payloads. Their current IDs are allocation-local and still require canonical
  section relocation before persistence.
- [x] Source topology construction and solving are separate entry points.
- [x] The Solver context no longer receives AST, source schedule/lowering plan,
  Core handoff, or source-result pointers after the open-image boundary.
  Their allocations are retained only for current owner lifetime/rollback and
  are not semantic resume inputs.
- [x] Successful solving records `SOLVED`, and a distinct read-only
  `seal(image)` returns a stable certificate without changing equations,
  transitions, or graph revisions. The certificate is bound to the accepted
  root revision/count/digest and the transitive accepted Claim/Derivation DAG.
- [ ] The image still owns `struct compile_context`, whose
  `legacy_typing` member contains mutable classifier, usage, effect, and proof
  state. Some of these fields are semantic inputs or current answers rather
  than disposable scheduling data. A `.a` loader therefore cannot yet rebuild
  the same Solver state solely from `typing_pipeline.seed`, ConstraintDB, and
  JudgementDB. This state must be classified and migrated before CG3; serializing
  the struct would preserve a second Authority.
- [ ] An unresolved Layer T equation graph can be serialized.
- [ ] Solver answers and Derivations can be loaded as an optional checkpoint.
- [ ] Solving can resume from a loaded dependency frontier.
- [ ] `.a` replaces `.apo` for open images, retained checkpoints, and sealed
  export closures.

### Current cutover decision

The target lifecycle is one image, not a family of compiler products:

```text
.p
  -> lower_to_image(source)       // immutable topology, zero Solver transitions
  -> solve(image, steps)          // zero or more calls over the same image
  -> certificate = seal(image)    // optional, read-only validation
  -> write(image, retention, certificate?) // legal at any progress frontier
```

The in-memory image has three orthogonal properties:

| Property | Values | Authority |
| --- | --- | --- |
| semantic progress | zero, partial, fixed point/residual/contradiction | equation graph and current validated answers |
| persisted retention | recompute or checkpoint | writer policy only |
| executable seal | absent or valid for named accepted roots | read-only seal certificate |

No combined stage/profile enum is introduced. In particular, there is no
`RECOMPUTE` Solver and no `CHECKPOINT` Solver. A partial image may be serialized
under either policy. Loading either result reconstructs the same image type and
enters the same ready-frontier builder and Solver.

The old `.ao` and `.apo` suffixes are both replaced by `.a` in one repository-wide
cutover after open-image roundtrip, resume, sealed link, aggregate, inspection,
and backend tests pass. Until that gate, v90 `.apo` remains the current
solved-publication prototype and must not be relabeled as the new open image.

`steps` means a deterministic count of Solver work units. It is not wall-clock
time, CPU speed, or a retention policy. Each rule evaluation, equation
transition attempt, or other explicitly enumerated Solver action consumes a
documented number of steps. Source parsing and structural lowering are not
semantic Solver steps and must not consume `--solve-effort`; if they later need
resource limits, those limits are separate structural limits.

Progress gate:

| Stage | State | Current acceptance gate |
| --- | --- | --- |
| CG0.0 closed answer identity | partial | direct and generated QuickSort pass source solving; persistent semantic identity remains |
| CG0.1 answer transition | partial | one physical classifier writer, PI/fold relation equations, aggregate answer ownership, and derived SCC scheduling are complete; final lifecycle and non-classifier domains remain |
| CG0.2 nominal-family equation | partial | intern the existing pure projection and its dependencies |
| CG0.3 dependent motive | partial | parent expectation reaches Match; intern it as an operand and finish guarded recursive motive |
| CG0.4 projection liveness | partial | enclosing refinement edges are indexed; exact projected refinement must still own live/dead edge enablement |
| CG0.5 publication roles | in-memory complete | seed, provenance, checkpoint, and accepted roots are distinct; persistent relocation and loaded-image evaluation remain |
| CG1-CG2 image boundary | partial | `.p` reaches an in-memory zero-transition open image, advances through one solve API, and has accepted-root-bound read-only sealing; `legacy_typing` extraction and loaded-image resume remain |
| CG2.5 persistent Layer T extraction | not started | semantic inputs/answers still present in `compile_context.legacy_typing` must move to seed/equation/evidence Authority or become rebuildable workspace |
| CG3-CG6 `.a` persistence/resume | not started | no `.a` serializer, loader, or resume CLI exists |

The status words in this table describe implementation progress, not alternate
compiler modes. There is one semantic pipeline. `RECOMPUTE` and `CHECKPOINT`
are write profiles for the same in-memory image and are not phase branches in
the Solver.

The word "artifact" in this plan means a persisted compilation image, not only
a completed compiler result. Immediately after structural lowering, an open
`.a` may contain unresolved equations and no accepted executable closure. The
same image can later contain replay-valid answers and, after read-only sealing,
an accepted executable closure certificate. These are capability differences
caused by present sections, not stage tags and not different object models.

The first `.a` boundary is exact:

1. Parse `.p` and reject only malformed surface or graph structure.
2. Intern the immutable Layer C graph, Context/Substitution topology, semantic
   references, root Layer T equations, and their direct operand edges.
3. Return the open in-memory `.a` image before any classifier, effect, usage,
   totality, universe, or proof equation transition occurs.
4. Advance only through `solve(image, steps)`.
5. Persist at any point using a retention profile; optionally seal a sufficiently
   solved image without mutating or advancing it.

This means `.p` is accepted only as source for constructing the zero-progress
image. It is never a second input form to solving, publication, linking, or
execution. Those operations consume the in-memory image produced by `.p` or the
same image reconstructed from `.a`.

This boundary must be demonstrated by a transition counter in tests. Merely
moving a call named `solve` out of lowering is insufficient if lowering still
queries an answer and creates a derived equation conditionally from it.

Current CG0.1 implementation checkpoint on 2026-09-03:

- The equation solution now records an answer lifecycle, answer revision,
  input revision, and semantic operand fingerprint.
- PI introduction, classifier equality, CBPV boundary refinement, generated
  binder updates, Context projection updates, and zero-clause fold refinement
  have started moving to the common projection-owned transition.
- Direct dependency wake-up is active. Recursive answer clearing and
  `stale_result_term` are no longer present.
- A provisional constant IH classifier can now be refined after its motive is
  solved. Previously the destructive clear path accidentally made this update
  possible; the lifecycle now expresses it directly.
- Direct and generated QuickSort source solving now derive termination evidence
  from the accepted typed-publication projection. The checked-Core boundary
  accepts the resulting graph without an endpoint mirror.
- The generated function graph now passes the former direct-instance/source-hint
  constructor-resolution failure. The kernel derives a common nominal family
  and factors every branch-varying telescope argument as a neutral Match.
  Pending constructor resolution and branch refinement consume this projection;
  the old source classifier hint is no longer an answer-selection Authority.
- Match motive construction now reads each branch classifier through that
  branch's refinement Substitution. A refinement change reopens the one owning
  motive equation and wakes its dependents; it does not clear a transitive
  answer closure. This removes the former `append`-shaped raw branch join
  `match left { nil -> nil; cons -> left }`.
- The guarded-recursive IH graph argument still retains a provisional neutral
  Match in its size, accessibility witness, and input indices after checked
  branch refinement is available. This remains a CG0.3 defect, not a request
  for inductive eta in DefEq.
- Match expected-result refinement now runs before classifier equality or
  compatibility shortcuts. Equal answer Terms do not discharge the motive
  derivation rule. PI introduction also forwards its specialized expected
  codomain to the body even when that body already has a classifier answer.
- A generated binder's Context equation is now treated as the Authority over a
  previously derived Lambda classifier. A changed inferred binder domain
  rebuilds the derived Lambda classifier; an annotated fixed binder still
  reports a contradiction.
- Final aggregation from provisional answers to `VALIDATED`, `RESIDUAL`, or
  `CONTRADICTION` is not complete, and some producers still bypass the common
  transition.
- Branch refinement no longer depends on its own branch classifier projection.
  That self-edge reopened a solved refinement immediately. The dependency
  index now first interns all case equations, then adds only scrutinee, Match,
  and enclosing Context-action prerequisites.
- The completion diagnostic no longer re-solves every motive equation. It
  reads the validation result already owned by the equation cell; otherwise a
  resumed checkpoint and an uninterrupted run would have different semantic
  execution paths.
- Proof and evidence materialization now read an already-solved Context
  projection answer through a read-only API. They no longer invoke the
  projection Solver while constructing a Derivation.
- A Context projection solution now stores the exact dependency fingerprint
  used to derive its concrete Context and Substitution. Identity,
  projection/restriction, and branch-refinement actions return the existing
  answer when that fingerprint still matches. A focused Context projection
  integration test and the projected empty-Match fixture pass after this cut.
- The fingerprint defect above Context materialization is fixed:
  `projected_binder_classifier` was a derived output in the same solution cell,
  not an operand, and has been removed from answer fingerprinting. A later
  trace exposed the deeper defect. The external-witness fixture previously
  alternated one base binder answer between `PI_EXPECTED` and
  `COMPUTATION_FOLD_RESULT`; read-only APP comparison now terminates that path.
  The resulting positive Acc regression proves that both refinements must
  become immutable relation equations and feed the base equation as premises
  in one explicit SCC. Enqueue suppression, fingerprints, choosing one
  producer, or simply deleting the relation would hide rather than repair the
  invalid ownership.
- The same change exposed a proof-publication bug: one occurrence can have
  several solved Context-indexed projections, while the executable publication
  names only one root projection. Candidate replay now checks projection-local
  Claims through a read-only Layer T answer query instead of rejecting every
  non-root Context as Solver history. The dependent function-graph fixture and
  IF8 integration suite pass with this rule. The `.a` design must preserve this
  split: checkpoint progress may retain all canonical projection answers and
  proof evidence, while sealing selects only executable publication roots.
- No `.a` serializer work has started. This is intentional: persisting the
  incomplete producer cutover would freeze the wrong Authority model.

Latest convergence and projection checkpoint on 2026-09-02:

- [x] A projected inferred binder now keeps one established classifier
  representative when a later candidate is classifier-compatible. Distinct
  Universe metavariable Terms remain separate equation operands and generate
  Universe obligations; they no longer replace one shared answer forever.
- [x] Motive constructor specialization reads the concrete branch
  `TypedProjection` Context. The occurrence-level branch Context is source
  topology and may not select another projection's constructor classifier.
- [x] Motive validation now selects its refinement Substitution from that same
  projection-owned motive equation rather than from the accepted occurrence
  case snapshot.
- [x] Generated QuickSort reaches an accepted typed fixed point. The former
  indexed `LT` all-impossible Match at operation 60/projection 1052 now obtains
  its expected result and no longer fails there. The subsequent generated
  binder/derived-Lambda mismatch was also corrected. Replacing enclosing-case
  DFS rediscovery with Context-action dependencies removed the former
  branch-refinement stall. A read-only proof-materialization path and the
  dependency-keyed Context projection memo remove the identified pullback
  recomputation. Earlier runs reached a `200000`-effort residual in about 22
  seconds and exceeded 45 seconds at `500000`; those measurements exposed
  rediscovery but are now historical. The current generated QuickSort reaches
  strict artifact writing in about four seconds. Exact answer-input identity,
  liveness pruning, and projection-growth bounds remain broader audit work.
- [x] Constructor occurrences retain an immutable `declared_classifier` input.
  This is distinct from the accepted `classifier` answer and preserves the
  nominal TypeView selected during lowering when an occurrence was unreachable
  in an earlier transaction but is projected again later. Artifact v90 carries
  this input so replay does not need AST or Core-shape name recovery.
- [ ] Projection liveness is not yet an equation domain. Candidate topology is
  expanded through every Match branch, while branch refinement can later prove
  a particular projected edge impossible. The current seal loop still demands
  solved classifiers and motives from those dead descendants. Direct enclosing
  branch dependencies are now available from Context action expressions; CG0.4
  must reuse those exact edges rather than reintroduce reachability scans.
- [ ] Typed projection materialization exceeded the former fixed Context limit
  of 8192. The prototype allocation is provisionally aligned with the 32768
  Substitution capacity so the semantic failure can be diagnosed. CG0.3 is not
  complete until Context count, intern requests/hits, projection count, and
  repeated materialization are measured and unnecessary Context construction
  is removed. Raising a fixed limit is not the architectural acceptance gate.

Latest focused regression checkpoint on 2026-09-03 before the foreign-write
cut:

- [x] Direct `if8_order_check.p` and `if8_fuel_free_quicksort_check.p` solve.
- [x] Constraint Authority, Context projection, dependent Match refinement,
  and Core/typing boundary integration checks pass.
- [x] Constant-motive seeding uses immutable branch-to-IH topology. It waits
  for constructor correspondence before deciding that a nullary branch is a
  constant motive, and it keeps acyclic seed answers separate from cyclic
  provisional fallbacks.
- [x] The checked-Core Issue 23 test no longer overflows the C stack while
  freezing compile metadata. The existing large snapshot is assigned directly
  instead of first constructing another multi-megabyte stack temporary. The
  snapshot remains an R6 deletion target; this correction does not make it an
  Authority.
- [x] Generated QuickSort output-tree source compilation and checked-Core
  validation pass with immutable source binding equations and
  projection-owned reindexed answers.
- [x] Generated QuickSort accepted publication no longer fails at its exported
  classifier or accepted proof. The exact accepted tuple is constructed before
  sealing and candidate publication is checked against that tuple.
- [x] Generated QuickSort artifact writing and readback pass with an explicit
  executable reachability role. The retained arena contains 52 resolved
  executable Match cases and 25 ownerless source/provenance cases; the latter
  cannot be promoted into the executable closure. Missing executable roots and
  children are rejected.
- [x] The malformed IF8 missing-bound negative fixture is rejected with the
  canonical `classifier-equation-contradiction` diagnostic at the APP whose
  proof slot receives the following List argument.
- [x] The malformed IF8 forged-bound negative fixture is rejected with
  `classifier-equation-contradiction`. The answer transition records the owning
  equation and both incompatible classifiers; no IF8-specific branch is used.
- [ ] No `.a` writer, reader, resume option, or `.apo` suffix cutover has
  started. The current v90 `.apo` path remains a solved publication format and
  is not yet an open compilation image.
- [x] Classifier-equation dependencies and classifier RHS nodes use one
  immutable variadic operand-edge arena. The separate classifier-recipe store,
  occurrence root map, and publication-only structural evaluator are deleted.
- [ ] Propagate these RHS roots through synthesized classifiers and connect the
  same evaluator to checkpoint resume and read-only seal validation.
- [x] Classifier answer publication has one physical transition. Static audit
  rejects reintroduction of the former bind/replace writer APIs.
- [ ] Producer phase 0 is not solver-free. It currently refreshes usage
  solutions, publishes imported classifier facts into mutable pre-solver state,
  attempts pending Match resolution, and only later generates root equations
  inside the classifier Solver. These calls must move behind `solve` or become
  immutable root equations before phase 0 may be named an open `.a` image.

Working-tree checkpoint after the read-only APP-domain cut:

- [x] `test_if8_fuel_free_quicksort.sh`, including the incompatible external
  witness, terminates and passes.
- [x] Classifier RHS Authority, Constraint Authority, and computation-block
  sequencing tests pass.
- [ ] Direct Acc eliminator, explicit indexed-family, dependent-Match, and
  dependent function-graph tests are temporarily pending. Their former success
  depended on APP publishing its expected domain into a sequence-result binder
  equation. The exact cycle is `Match motive -> IH result -> sequence binder ->
  APP domain -> Match motive`.
- [ ] Do not restore that foreign publication as the final repair. Add the
  PI-domain and fold/binder relation equations, derive the SCC, and then make
  the APP path permanently read-only.

The remaining work is therefore not an artifact-extension rename. The current
lowering path still interleaves topology generation, solving, evidence
materialization, accepted publication, and finalization. The cut points must be
made explicit before `.a` persistence is added.

The immediate next step is not to implement a new serializer. The resolved
writer failure proves that four persistence roles must not be collapsed:

1. immutable replay seed;
2. optional source/proof provenance;
3. optional replay-validated Solver progress;
4. accepted executable publication.

The earlier liveness finding also remains: an impossible Match-case edge may
disable obligations for that branch body while the Match node itself remains
live and receives its result/motive from an outer expected-classifier equation.

The implementation order is revised to match the current code:

1. [x] Preserve the accepted executable role already enforced by v90 and its
   boundary tests. Do not extend the solved-only closure into a provisional
   `.a` format.
2. [x] Replace PI-domain and zero-clause sequence-binder foreign answer writes
   with immutable relation equations. Only each aggregate projection equation
   owns its classifier answer.
3. [x] Aggregate premises under that owner and derive the recursive
   Match/IH/fold relation SCC from immutable dependency edges.
4. [ ] Complete the replay seed and remove semantic source dependencies.
   Match resolution, Match typing, imported constructors, binders,
   declarations, publication names, and `::` expectations are already seed
   records. Move any remaining Solver/finalizer input out of AST, source plan,
   handoff, and source-result tables; then prove that those source-only objects
   can be destroyed immediately after `lower_to_image`.
5. [ ] Complete stable image identity. Replace allocation-local components of
   every equation key with recursively stable Layer C, Context-action, operand,
   and declaration semantic keys. Finish classifier RHS endpoint propagation
   and image-wide semantic progress accounting without adding another store.
6. [ ] Split solving from sealing. A successful fixed point remains an open
   image. `seal(image, roots)` is read-only, validates existing answers/evidence,
   and returns a certificate; it never generates equations or repairs proofs.
7. [ ] Add one sectioned `.a` writer and loader over the same image stores.
   `RECOMPUTE` writes the replay-complete seed and omits reconstructible Solver
   work. `CHECKPOINT` writes the same seed plus canonical expansion, current
   replay-valid answers/evidence, residuals, and at most one provisional record
   per recursive SCC. Neither profile is visible to lowering or solving.
8. [ ] Rebuild indexes, SCCs, and the ready frontier after load, then prove that
   repeated bounded `solve(image, steps)` and one sufficiently large call reach
   identical canonical equations, diagnostics, answers, and accepted roots.
9. [ ] Replace `.ao` and `.apo` with `.a` in one cutover across compile, import,
   link, aggregate, inspect, replay, backend, tests, and documentation. Delete
   the old suffix branches; do not retain aliases or a compatibility writer.

Persistence before step 4 would serialize transient frontend state. Suffix
renaming before steps 6-8 would relabel the solved-publication prototype rather
than implement the new compilation image. The immediate implementation target
is therefore step 4, followed by stable keys and the read-only seal boundary.

One concrete failure found during this audit is PI elimination in
`classifier_and_computation_propagation.inc`: when an argument projection is
unsolved, it recursively invokes `operation_refine_classifier_from_expected()`.
For a lexical `VAR`, that path can report an immediate classifier-check failure
instead of adding an equality/dependency edge to the binder's Context equation.
This is exactly the control-flow defect targeted here: a Solver rule is trying
to generate topology, solve it, and diagnose contradiction in one branch. The
rigid mismatch path now reports contradiction on the exact APP equation, but
the compatible-yet-unresolved path still publishes an expected classifier into
another equation's answer. That remaining publication is replaced by the
relation-equation work in steps 2-3 above.

The generated QuickSort graph originally exposed destructive transitive
invalidation: one binder refinement erased Match, computation-fold, and outer
Lambda answers, and an incomplete recursive dependency graph could not
bootstrap them again. That clearing route is now deleted. Endpoint publication,
nominal-family selection, and ordinary branch-refined motive rebuilding now
advance. The remaining work is to intern the neutral-family projection as an
ordinary Layer T equation and to replace the guarded-recursive provisional
fallback with a checked abstraction over the original scrutinee. Nominal
declaration identity is fixed. Any telescope argument, including a parameter,
may depend on the eliminated scrutinee; a neutral Match is the residual only
when no checked motive abstraction is available. Uniform parameters remain
mandatory only for recognizing a recursive field as belonging to the same
strictly-positive family.

The later `LT` failure exposed a second missing graph domain. Source lowering
must retain all candidate branch topology because it is solver-free, but a
projection-specific branch-refinement equation may prove one Match-case edge
impossible. Therefore liveness is derived by canonical equations:

```text
Live(root) = true
Live(child) = OR over incoming edges (
  Live(parent) AND EdgeEnabled(edge)
)

EdgeEnabled(MATCH_CASE edge) =
  refinement(edge.match_case_projection) != IMPOSSIBLE
EdgeEnabled(other structural edge) = true
```

An unresolved refinement leaves liveness unresolved; it is not treated as
false. A dead projection remains in `.a` topology for replay and diagnostics,
but classifier, motive, effect, evidence, and seal obligations are required only
for projections whose `Live` equation is true. This is a derived equation
domain, not a second reachability Authority. The current source-case
`refinement_status`, enclosing-Match scans, and seal-all-projections loop must
not survive as competing liveness rules.

This liveness rule does not make an all-impossible Match dead. If an outer
constructor field, Pi codomain, or expectation consumes that Match, the Match
projection remains live. Its result equation receives an expected-classifier
operand from the parent and may synthesize a constant motive even when no case
body is reachable. Only the impossible case edges and their descendants become
dead.

## Decision

Compilation must have one data path:

```text
.p source
  -> lower to one open .a compilation image (no Solver transition)
  -> solve the same image through canonical equation transitions under an effort budget
  -> optionally write recomputable or checkpoint .a at any point
  -> attach a seal certificate after read-only closure validation
  -> execute or link only an image with a valid seal certificate
```

The first arrow has a strict contract: reading and lowering `.p` constructs a
well-formed, zero-progress `.a` and does not run a Solver transition. Parsing and
lowering may reject malformed syntax or structurally invalid graphs, but they
must not require a classifier, effect, usage, totality, universe, or proof
equation to have an answer. All semantic progress after that boundary is made
by applying `solve(image, effort)` to the `.a` image.

The open-image boundary occurs after the immutable topology required to replay
future expansion has been closed, not after all derived equations are known.
Context/Substitution/IH actions, unresolved root goals, ordered source operands,
and their semantic references are seed topology. Derived equations,
answer-dependent Match projections, dependency edges, and SCCs may be interned
later by `solve`; under `CHECKPOINT` they may be retained, while `RECOMPUTE`
recreates them from the seed. Computing a usage answer, importing a classifier
into a mutable solution cell, resolving an answer-dependent Match, or publishing
a Claim is solving and must occur only after this boundary.

Zero progress does not mean an unvalidated byte dump. The parser and structural
lowerer have established canonical Layer C nodes, stable source references,
Context/Substitution topology, replay root goals, and format
well-formedness. It means precisely that no semantic Solver transition has
occurred. The `.p` representation is discarded after this boundary and is never
an alternate input to a later Solver path.

The `.a` image is the compilation state itself, not merely a renamed sealed
artifact. It does not carry the cross-product
`DRAFT | CHECKPOINT | SEALED` by `RECOMPUTE | CHECKPOINT`. `checkpoint` is a
writer profile, not a semantic stage. `sealed` means only that a seal
certificate section is present and validates against the current semantic
roots. An image without that certificate is open regardless of how much Solver
progress it retains.

`RECOMPUTE` always retains the replay-complete seed graph and omits any
reconstructible Solver expansion, derived answers, proof-search progress, and
caches selected by the profile. `CHECKPOINT` retains the same seed graph plus
canonical expanded equations/dependencies and replay-valid current classifier,
effect, usage, totality, universe, and proof answers, residuals, and provisional
SCC candidates. A generated equation is identified by a semantic key, so
omitting it causes the same `lookup-or-intern` operation to recreate it during
solving; retaining it does not create a second Authority.

The eventual command-line choice is therefore a serialization option such as
`--a-retention=recompute|checkpoint`. It is not passed to source lowering or
the Solver. A zero-progress, partial, or solved in-memory image may be written
with either option, and loading either result resumes through the same
`solve(image, effort)` entry point.

Progress, retention, and execution authority are orthogonal:

```text
progress   = zero | partial | fixed-point/residual/contradiction
retention  = RECOMPUTE | CHECKPOINT
seal       = absent | valid for a named accepted-root closure
```

`solve` changes only progress. The writer chooses retention. `seal` validates a
read-only projection. The Solver must not branch on retention or seal presence.
A provisional answer may be refined or rejected, so "monotone" means that
canonical equation identity and validated knowledge are not discarded; it does
not mean that every provisional answer Term is permanent.

The same loader and Solver consume both forms. Optional section presence, not a
stage/profile branch in semantic code, determines how much work must be
reconstructed.

`CHECKPOINT` does not retain every historical answer ever visited. For each
canonical equation it stores at most the current replay-valid answer and, where
an SCC requires it, one explicitly provisional candidate keyed by the current
operand fingerprint. Superseded Term IDs, traversal history, and old constraint
states are disposable. Thus avoiding recomputation does not require preserving
an append-only log of every intermediate type calculation.

The source reader must not independently construct several final-looking DBs.
It constructs one Layer C computation graph and one Context-indexed Layer T
seed equation graph. Solving interns derived equations by semantic key and
advances answers/evidence through the one transition owned by each equation, or
leaves explicit residuals.

Persistence roles do not duplicate those arenas. One `.a` contains one
canonical Layer C arena and one Layer T graph. Seed, provenance, checkpoint,
and accepted publication are independent root sets and optional record tables
over those stores. The seal certificate commits to the accepted root closure;
it does not assert that every unresolved source/provenance node in the arena is
executable.

"Monotone" applies to semantic knowledge, not to one temporary Term ID. A
recursive or projected equation may move from no candidate to a provisional
candidate and then to a validated answer. The transition belongs to that one
equation. It never creates another answer Authority, and it never destroys the
answers of the entire transitive dependent closure.

This preserves the required separation:

- Layer C owns erased computation;
- Layer T owns classifiers, Contexts, constraints, effects, usage, totality,
  equality evidence, and proofs about Layer C terms.

The layers are distinct, but compilation does not have parallel authorities or
parallel control-flow pipelines.

## Persistent Data Classes

The resumable image has one semantic graph with five persistence roles. These
roles are section ownership, not five execution pipelines. An object may be
referenced across roles only through a validated semantic key or projection
edge; physical Term, Claim, or equation IDs are local to one image.

### Replay-complete seed

This is the minimum canonical graph and is always retained:

- interned Layer C terms;
- source occurrences and Context-indexed typed projections;
- Context and Substitution DAGs;
- root typing/effect/usage/totality/universe equations and their immutable
  operands;
- external references, declarations, and immutable source annotations such as
  a constructor occurrence's `declared_classifier`;
- exact semantic identity inputs needed for replay. Source text, spans, and
  diagnostic-only occurrence history belong to the optional provenance role.

This seed is sufficient to reproduce solving without retaining the frontend
AST. It is not required to enumerate every equation that unification,
refinement, recursive motive construction, or proof search will derive later.

### Canonical Solver expansion

Solving may intern additional equations, dependency edges, derived classifier
Terms, and proof goals. These are progress, but they are not an alternate
representation of the seed. Every generated object has a semantic key derived
from its rule and operands. Re-encountering that key returns the same object;
it does not append a duplicate equation or solution cell.

A recomputable file may omit this reconstructible expansion. A checkpoint file
retains it so solving need not regenerate intermediate types and constraints.
Loading retained expansion validates every key and dependency against the seed
before it becomes part of the in-memory graph.

### Verified checkpoint

These records may be retained to avoid recomputation:

- the canonical Solver expansion;
- solved equation answers;
- accepted Claims and Derivations;
- explicit residual or rejected states with their verification obligations;
- the effort policy and completed semantic transitions.

They are not a second Authority. Each record names its equation ID, and loading
it revalidates the answer or certificate against that equation before reuse.
A minimal image may omit this section and return those equations to `READY`.

For a recursive SCC, a checkpoint may also retain provisional approximants and
their operand fingerprints. Such an approximant is resumable work, not an
accepted typing fact. It must be replay-checked before it can re-enter the
worklist, and sealing cannot use it until the SCC reaches a validated fixed
point.

### Accepted publication and source provenance

An accepted typed publication is a projection of the source/equation graph,
not a replacement for that graph. The two forms must remain distinguishable:

- source provenance records the original occurrence, unresolved source
  topology, source Claims, and the Derivations that led to a result;
- accepted publication records the concrete Context, projected subject,
  projected classifier, accepted Claim, and evidence roots that may enter a
  sealed executable closure.

The current v90 writer recursively follows an accepted Claim into source
Derivations and then copies every referenced Term into one flat executable
graph. Generated QuickSort proves this is invalid: valid source provenance can
contain unresolved Match constructor correspondence after a concrete typed
projection has already been accepted. The writer must not make either of these
repairs:

- mutate source Match cases to the projection's constructor IDs;
- permit unresolved Match cases in a sealed executable graph.

Instead, publication must reify or reference a projection-owned proof DAG. A
checkpoint may also retain the source proof DAG in a provenance section whose
terms are not executable roots. The independent checker validates the bridge
from source occurrence and equation evidence to the accepted projection, then
validates the sealed projected closure. Canonical term keys may prefilter this
bridge, but cannot select an arbitrary equal-looking Term in another Context.

### Disposable cache

These records may be omitted without changing meaning:

- WHNF/NF results keyed by reduction profile and graph revision;
- lookup indexes rebuilt from canonical keys;
- dependency scheduling indexes;
- ready queues and performance counters.

Cache entries carry the digest of their semantic inputs. A missing or stale
entry causes recomputation, never a fallback semantic path.

### Progress manifest

Every `.a` records enough structural metadata to explain how far compilation
has advanced without making that metadata an answer Authority:

- source/import fingerprint and format version;
- equation-topology, solution, accepted-evidence, and publication revisions;
- hashes and presence of optional progress, provenance, cache, and seal
  sections;
- completed invariant checks, expressed as independently replayable
  certificate references rather than one mutable stage enum;
- deterministic Solver effort units consumed for diagnostics and the requested
  effort limit.

Wall-clock time and machine speed are never semantic inputs. Two machines may
stop with different residual frontiers, but every retained answer states which
equation key and exact direct-input revisions it validates. With sufficient effort, both
must produce the same accepted semantic roots.

## One `.a` Format

The old `.ao` and `.apo` extensions are replaced by `.a`. This is one format
cutover, not a permanent compatibility route or a second writer. The current
code uses `.ao` for individual artifacts and `.apo` for aggregate examples, but
that spelling difference has no semantic role worth preserving. The v90 sealed
closure may be used as input to the new sealed sections, but accepted commands
must not keep independent legacy and `.a` semantic pipelines. One sectioned
`.a` format supports the complete lifecycle.

The format does not store a `DRAFT/CHECKPOINT/SEALED` stage enum. Section
bounds, references, graph well-formedness, and equation-key uniqueness are
validated for every image. Classifier, effect, usage, totality, universe, and
proof obligations may remain unresolved in an open image. A valid seal
certificate section is the only marker that the strict export closure required
for linking or execution has been established.

The format also supports two storage profiles without two compilers:

1. **Recomputable image**: the replay-complete seed, imports, and policy, but no
   reconstructible Solver expansion or retained equation answers. Solving
   recreates them through canonical `lookup-or-intern` operations.
2. **Checkpoint image**: the same seed plus canonical Solver expansion,
   verified current answers, Derivations, and optionally disposable caches.
   Solving resumes from the remaining dependency frontier.

Both forms must produce the same sealed semantic graph after sufficient effort.
Different budgets may produce checkpoints with different progress, but cannot
change the meaning of a verified answer.

These profiles are legal at every solve frontier. They do not name stages of
the in-memory image. A sealed recomputable write may keep the accepted
publication closure and certificate while omitting unrelated reconstructible
progress. Conversely, omitting any answer/evidence referenced by the
certificate necessarily omits the certificate and yields an open image.

The section policy is explicit:

| Section role | `RECOMPUTE` | `CHECKPOINT` | Required for execution |
| --- | --- | --- | --- |
| replay-complete source seed | required | required | required indirectly |
| canonical generated equations/dependencies | omitted when reconstructible | retained | no |
| current validated answers/evidence | omitted when reconstructible | retained | required for referenced exports |
| source proof provenance | optional | optional/retained by policy | no |
| accepted projected publication closure | retained only after sealing | retained only after sealing | yes |
| seal certificate | retained only when valid for retained roots | retained only when valid for retained roots | yes |
| disposable cache | omitted by default | optional | no |

A `RECOMPUTE` write of a solved image may therefore deliberately omit its
current answers and seal certificate and become open again on disk. A caller
that wants a directly executable file requests a sealed write; sealing selects
validated publication roots, while `recompute|checkpoint` selects how much
non-publication progress is retained. These options remain orthogonal writer
projections over the same live image and do not add Solver branches.

The intended straight-line API is:

```text
image = lower(source)             // topology only
image = solve(image, effort)      // advances owning equations
write(image, RECOMPUTE)           // omit reproducible progress
write(image, CHECKPOINT)          // retain replay-valid progress
image = load(path)                // rebuild one ready frontier
image = solve(image, more_effort) // the same Solver in both cases
certificate = seal(image)         // read-only validation
```

`write(..., RECOMPUTE)` is a projection of an image, not a destructive reset of
the live graph. It omits optional progress sections while preserving the
replay-complete seed. `write(..., CHECKPOINT)` stores current
replay-valid progress, not every historical answer. Neither option creates a
second in-memory solution store or a storage-specific Solver branch.

The profiles differ only in retained progress:

- `RECOMPUTE` retains every seed node, root equation, source reference, and
  policy input needed to reproduce solving, but may omit generated equations,
  derived answers, evidence, and certificates.
- `CHECKPOINT` retains that identical seed and additionally keeps canonical
  generated equations/dependencies and validated intermediate classifier,
  effect, usage, totality, universe, and proof answers. It may retain
  replay-checkable provisional SCC candidates as non-authoritative work
  records. Every record carries its semantic key and operand fingerprint.

Therefore a checkpoint is not a richer source language and a recomputable
image is not a lossy AST dump. They are two serializations of the same `.a`
graph with different optional progress sections.

## Compiler Options

The planned command boundary is:

```text
--write-a IMAGE.a
--a-retention=recompute|checkpoint
--resume-a IMAGE.a
--solve-effort STEPS
--seal
```

`recompute` writes the replay-complete seed and omits reconstructible expansion,
answers, evidence, and disposable caches. It retains a seal certificate only
when the accepted publication closure and every answer/evidence record named by
that certificate are also retained by the independent sealing choice. `checkpoint`
additionally writes the current canonical expanded equations/dependencies,
every replay-valid answer, intermediate classifier/effect/usage/totality result,
Derivation, residual obligation, and a valid seal certificate when one exists.
It may also write digest-keyed caches.

The options select serialization policy only. They must not select different
lowering or solving algorithms. Resuming either image enters the same equation
frontier and may produce either storage profile on the next write.

No ready queue or traversal cursor is persisted as semantic state. Loading
rebuilds the ready frontier from equation states and explicit dependency edges.
This makes scheduling reproducible without making one historical worklist an
Authority.

The checkpoint may preserve an SCC candidate set, but not the historical order
in which its members were visited. The SCC and its dependencies are semantic
topology; queue order and traversal cursor are disposable scheduling state.

The storage profile is a writer option, not a second in-memory image tag. The
reader discovers optional progress and seal sections through one section table,
validates each present section, and then invokes the same frontier builder.
There are no stage/profile combinations to dispatch inside the Solver.

## Sealed Boundary

Artifact v90 describes the current strict solved closure, but `.apo` is not a
second long-term product. The cutover replaces it with a `.a` containing a valid
seal certificate and the Layer C and Layer T closure required by its exports.
Sealing must not become an alternate solver or silently accept unresolved
Context classifiers.

The certificate commits to the exported semantic roots, canonical equation
keys, current validated direct-input snapshots, and evidence roots. Any change to
an owning equation revision makes the certificate invalid without a special
invalidation pass. A recomputable write omits the certificate together with the
answers it certifies.

The distinction is therefore:

```text
open .a:    unresolved equations are valid; no valid seal certificate
sealed .a:  every exported dependency is solved or an explicit publishable
            obligation permitted by the format, and the certificate validates
```

## Authority Rule Exposed by Indexed Family `map`

The failing `map` artifact exposed a concrete violation. A projected Context
equation received a classifier while its source equation remained unresolved.
The projected equation was a derived view, not an Authority.

The corrected rule is:

- keep the binding classifier family in the source Context equation;
- derive every reindexed classifier through its Substitution;
- publish that derived classifier only as the answer of the consuming typed
  projection's classifier equation;
- if no source-context preimage is valid, leave the equation residual;
- never write a projection-specific classifier back into the source Context
  equation, and never make a reindex-only Context equation a second answer
  Authority.

This rule applies to all future checkpoint serialization: derived projections
may be cached, but only source equations own answers.

## Implementation Stages

### CG0: Finish single-Authority cutover

- [x] Stop the indexed-family PI solver from publishing a classifier directly
  into a reindex-derived Context equation.
- [x] Keep the source Context equation and derive projected classifiers by
  Substitution in the corrected indexed-family path.
- [x] Restore strict reachable-Context validation at the sealed boundary.
- [x] Add indexed-family artifact round-trip coverage to the Authority audit.
- [x] Add the IH recursive-argument premise to zero-clause computation-fold
  equations, so motive seeding is requeued by an explicit graph edge.
- [ ] Route every remaining Context-classifier write through the same canonical
  source-equation API.
- [ ] Make every consuming typed-projection equation own its reindexed answer
  without replacing the source binding equation.
- [ ] Delete direct answer replacement and restart paths that bypass monotone
  equation transitions.

### CG0.1: Replace destructive invalidation with equation transitions

- [x] Add one answer lifecycle representation for classifier equations:
  `UNSOLVED -> PROVISIONAL -> VALIDATED | RESIDUAL | CONTRADICTION`.
- [x] Store exact producer-input and Context-projection revision snapshots on
  the owning equation answer. Operand fingerprints remain diagnostic/index
  values only and never authorize reuse. Input change requeues the owner but
  does not erase or immediately wake its transitive dependent closure.
- [x] Hash-cons immutable constraints by domain, kind, owner projection,
  semantic payload, and ordered operand edges. Provenance-only `source_ast`
  does not split an equation. Effect generation uses this common path instead
  of scanning all prior effect constraints.
- [ ] Replace the remaining allocation-local components of that key with
  stable semantic identity suitable for compaction and reload. Exact equality
  after a hash hit remains mandatory; the hash itself is never Authority.
- [ ] Complete one update operation that distinguishes unchanged DefEq answer,
  legitimate provisional refinement, final validation, residual, and
  contradiction. Propose/refine/input-change/discard are implemented;
  final-state aggregation remains open.
- [ ] Migrate PI introduction/elimination, CBPV boundary, equality/reference,
  Match motive, IH, computation fold, binder projection, branch refinement, and
  effect/usage/totality producers to that update operation. Physical classifier
  answer writes are complete; final lifecycle and other domains remain.
- [x] Wake direct dependency edges only when reevaluation changes the owning
  equation's semantic output. Input invalidation and unchanged DefEq snapshot
  refresh remain local to the owner. The dependent-output IH fixture covers
  recursive convergence under this rule.
- [ ] Replace the transitional unanswered-classifier wake after Context closure
  with an explicit readiness/validation operand. It currently wakes only a
  dependent with no answer, which preserves Acc construction without reviving
  the recursive SCC loop, but it is not persistence-ready equation identity.
- [ ] Count semantic transitions in every image domain through the owning
  equation or progress-manifest mutation. All central ConstraintDB lifecycle
  transitions and classifier answers are covered; legacy usage/effect answers,
  universe/equality work, and proof publication remain. Scheduler visits, cache
  fills, and unchanged proposals are intentionally excluded.
- [ ] Build SCCs from explicit equation edges and solve recursive
  Match/IH/fold components as units. Do not infer recursion by a reachability
  exception inside invalidation.
- [ ] Add every hidden recursive premise as an equation operand. In particular,
  the generated QuickSort output-tree graph must include the IH-to-Match/fold
  back-edge needed to resume after a binder classifier refinement.
- [x] Delete recursive classifier clearing, `stale_result_term` as a semantic
  fallback, and projection-reachability invalidation exceptions.
- [ ] Delete remaining semantic reopen callers after their input changes and
  refinements are represented by the common transition. Legacy classifier
  bind/replace adapters are already deleted.
- [ ] Keep an old candidate only as a digest-keyed disposable/checkpoint record;
  it must not be returned as the active classifier answer after its inputs
  change.

The destructive route has now been deleted, so CG0.1 may not regress by adding
an invalidation exception. Completion requires migrating the remaining
producers, deriving endpoint-bearing classifiers from current equation
operands, validating final states, and making recursive components explicit
SCCs.

The immediate implementation order is:

1. [x] Repair `Terminates` endpoint derivation so a rebuilt Layer C occurrence
   requeues and refines its classifier equation. This repaired the wake-up but
   did not make a pre-closure answer checkpoint-valid.
2. [x] Stop projected Lambda-binder inference from replacing the binding's
   source Context equation. The source equation retains its classifier family;
   `Partition A size` and `Partition A (succ size)` belong to distinct typed
   projection equations derived through their substitutions.
3. [ ] Intern and close the Context/Substitution/IH-scope action graph before
   any answer-producing classifier conversion. Closure may intern action and
   reindex equations, but may not publish classifier answers.
4. [ ] Give every typed projection one closed action fingerprint containing
   its parent action, substitution, branch refinement, and IH-scope identity.
   A change selects a different equation key; it never mutates the meaning of
   an accepted answer in place.
5. [ ] Remove the implicit dependency from every classifier constraint to its
   own `owner_typed_projection`. The owner is an output. A rule that consumes
   the current owner answer must declare an explicit operand instead of using
   a universal self-edge. The direct removal experiment exposed hidden motive
   reads and was rolled back until those operands and SCCs are present.
6. [x] Delete the temporary `exact_result_term` field and its proof-freeze
   promotion path. Do not restore it while completing closure.
7. [ ] Validate and publish the one `result_term` only under the closed action
   fingerprint. Remove least-Term-ID selection across Contexts: alpha/DefEq
   conversion checks candidates in one closed projection Context; it does not
   choose a representative from different Contexts.
8. [ ] Build the projection-equation SCC graph from explicit operands. Solve
   the condensation DAG in dependency order and solve Match/IH/fold cycles as
   units. Motive materialization is an SCC output; do not destructively reopen
   a materialized motive because its own recursive branch changed.
9. Finish ownership of the common nominal family and factored telescope spine
   as one canonical Layer T equation. The pure projection exists; its operands,
   dependencies, answer lifecycle, and residual result still need migration.
10. Finish adding the parent expected-classifier to the live Match result/motive
   equation. The current PI/Match control path passes the original
   all-impossible blocker, but the expectation must be a semantic-keyed operand
   and dependency rather than mutable helper state.
11. Add canonical projection-liveness equations over exact projected edges and
   branch-refinement equations. Stop requiring answers from dead projections.
12. Complete the recursive IH Match motive. Ordinary branch classifiers now use
   the checked projection-owned branch refinement action and reopen their
   owning motive. Finish partial indexed-family motive application and the
   guarded-recursive abstraction so generated QuickSort stops retaining
   pending or neutral Match indices after its inputs are ready.
13. Separate Context projection transition from answer lookup. A solved
   projection memo carries a per-projection revision and is reusable only when
   its exact parent projection and branch-refinement inputs are unchanged. Do
   not use one global revision, a digest as equality Authority, recursively
   recompute pullbacks on reads, or make the memo a second Authority.
14. Make proof reification consume the validated equation/evidence reference
   directly and memoize projected proof reification by `TypedProjectionId`.
   Delete repeated variable-authority classifier conversion from the recursive
   proof walk.
15. [x] Remove direct classifier result writes and legacy bind/replace adapters.
    Non-classifier domain transitions remain part of items 16 and 18.
16. Aggregate producer outcomes into final equation lifecycle states.
17. Separate accepted projected publication from source proof provenance so a
   strict closure never treats an unresolved source Match as executable.
18. [x] Introduce solver-free `.p -> open in-memory image` construction. The
    remaining work is to make its replay seed persistent and its first equation
    expansion reset-free.

#### Current cut to the open-image boundary

The former producer API has been replaced by the non-copying
`prototype_compilation_image` owner. Its Layer C and Layer T stores remain in
`prototype_program_storage`; the image owns the source schedule, lowering
results, Solver workspace, and resumable phase cursor without copying those
stores.

`prototype_compilation_image_lower()` now returns immediately after Layer C and
typed-projection topology construction. It does not refresh usage, infer
imported constructor classifiers, resolve pending Match items, generate
constraints, or publish an answer, and it asserts zero ConstraintDB semantic
transitions. `prototype_compilation_image_solve()` continues that same image
and is the sole semantic-progress entry.

The current and target cuts are:

```text
lower(source)
  = parse/name-resolve
  + intern Layer C
  + build Context/Substitution/TypedProjection topology
  + assert semantic_transition_count == 0 in every domain

solve(image, effort)
  = expand canonical equations from immutable roots when absent
  + rebuild disposable frontier
  + evaluate immutable input-fact equations
  + solve usage/classifier/effect/totality/universe/equality/proof equations
  + intern deterministic derived equations by semantic key
  + retain residuals or validated answers on their owning equations
```

The in-memory split is complete, but replay-complete equation expansion is not.
`operation_solver_generate_constraints()` still initializes ConstraintDB,
resets legacy classifier state, and refreshes usage. Before persistence, its
immutable root inputs must be retained in the image and expansion must become a
lookup-or-intern operation that can run after either source lowering or `.a`
loading without clearing checkpoint answers. A source annotation or imported
interface fact becomes an immutable leaf/operand; its evaluated classifier is
not preinstalled as a second answer Authority.

Two physical invariants must be removed before that function can become
append-only:

1. ConstraintDB currently assumes domain-contiguous storage through
   `first_branch_refinement_constraint`, `first_computation_constraint`,
   `first_effect_constraint`, and their counts. A newly discovered classifier
   equation cannot be appended after effect equations without violating those
   ranges. Replace all such ranges with one per-domain index/next-link scheme
   over the canonical interleaved equation arena. This index is rebuildable
   acceleration data, not another equation store.
2. `PROJECTED_CLASSIFIER` currently interns its complete premise array into the
   equation key. If later topology adds a premise, that creates a second
   aggregate instead of extending the one answer owner. Key the aggregate by
   stable projection identity, retain its premise relations as append-only
   canonical edges, and record a dependency-set revision on its answer. Adding
   an edge requeues that equation and invalidates only the answer justified by
   the previous direct-input revision.

After these cuts, equation expansion may scan immutable roots and execute only
lookup-or-intern plus append-edge operations. Running it twice on unchanged
roots is a strict no-op; running it after topology growth adds only the newly
reachable equations/edges. It never clears ConstraintDB, effect metas, current
answers, or semantic transition history.

After that remaining cut, both `lower(.p)` and `load(.a)` return the same image
contract. The only semantic-progress entry point is `solve(image, effort)`, and
the only retention decision is `write(image, retention)`. `RECOMPUTE` may store
the earliest lowered frontier and omit reproducible expansion/progress;
`CHECKPOINT` stores the identical immutable roots plus replay-valid canonical
equations and current answers. These are writer profiles, never Solver modes.

The projected-binder/zero-clause-fold idempotence defect and the v90
accepted-executable/source-remainder role boundary are now closed for their
focused paths. The common classifier RHS representation and evaluator are also
in place for source Pi solving and accepted projection. Explicit Match/IH/fold
SCC ownership is also complete. The next implementation is reset-free,
replay-complete equation expansion, stable equation/action fingerprints, final
lifecycle ownership, and projection liveness. Independent
seed/checkpoint/provenance/accepted root sets follow; only then may `.a`
persistence start. This order prevents `.a` from freezing allocation-local IDs,
a reset-based expansion pass, or v90's retained-remainder convention as the
resumable format.

#### 2026-09-02 implementation audit

The generated QuickSort graph exposed two distinct defects that must not be
collapsed into a larger effort limit:

- 14,509 generated constraints caused about 150,000 worklist pops because each
  producer had been indexed as a dependent of its own output projection.
  Removing that universal output edge reduced the run to about 121,000 effort
  and allowed the worklist to close.
- closure then exposed a non-monotone endpoint: a branch classifier containing
  binder `#53` was provisionally DefEq to its projected `#54` endpoint before
  IH-scope completion, but not after final Context closure. The materialized
  motive retained `#53` while occurrence freeze promoted `#54` from a separate
  `exact_result_term` field.

The accepted correction is architectural:

```text
lower .p
  -> close Context/Substitution/IH-scope actions
  -> intern root Layer T equations
  -> build Live and classifier/motive/effect SCC topology
  -> open .a with immutable Layer C and zero-transition Layer T equations
  -> solve SCC condensation graph with an effort budget
  -> validate one answer per equation against its closed input fingerprint
  -> freeze publication/evidence
  -> optionally write RECOMPUTE or CHECKPOINT .a
```

The following are explicitly rejected:

- retaining the universal owner self-edge;
- selecting the least Term ID as a projection answer across Contexts;
- promoting an `exact_result_term` only after the Solver worklist has closed;
- reopening a recursive motive one edge at a time;
- raising QuickSort effort limits to hide rediscovery;
- giving `RECOMPUTE` and `CHECKPOINT` different solving semantics.

Updated verification status: the endpoint mirror remains deleted, direct and
generated QuickSort source solving pass, and checked-Core accepts the generated
graph. Serialization work still waits for CG0.5 because current v90 closure
mixes unresolved source proof provenance into the accepted executable graph.
Persisting that flat closure would freeze a different Authority/role defect
into `.a`.

### CG0.2: Make neutral Match family resolution an equation

- [x] Add a kernel projection operation that inspects a classifier computation
  and derives a common nominal type declaration only when every reachable
  branch has the same declaration identity.
- [x] Preserve each branch-varying parameter or index as a neutral Match over
  that telescope argument. Do not confuse this dependent elimination rule with
  the uniform-parameter condition used to recognize recursive fields.
- [ ] Intern the derived family/schema result as a Layer T equation answer with
  dependencies on every contributing branch classifier equation.
- [ ] Make pending Match constructor resolution and branch refinement consume
  that one equation answer. Both paths now use the same pure kernel projection
  and the source hint has been removed from answer selection, but repeated
  projection calls remain until the equation is interned.
- [ ] Leave mixed families, non-uniform parameters, or unavailable branch
  classifiers residual. Do not select the first branch, rescan globally, or
  add a generated-function special case.
- [ ] Include this equation in the recursive Match/IH/fold SCC when any branch
  depends on the Match result.
- [ ] Add positive tests for a generated IADT whose branch-varying indices share
  one nominal family, and negative tests for mixed family and parameter spines.

CG0.2 is complete when generated QuickSort advances through the neutral Match
without a source hint, and changing branch insertion order produces the same
equation key, family answer, and diagnostic.

### CG0.3: Complete dependent Match motive abstraction

- [x] Record, for each Match branch classifier equation, the branch refinement
  action that specialized the scrutinee to its constructor spine.
- [x] Reopen the owning motive when a branch refinement action changes and wake
  direct dependents without transitive clearing.
- [x] Synthesize one ordinary motive family from refined branch equations and
  apply it to the original scrutinee. For `append`, this derives the direct
  result family rather than the former raw classifier containing
  `match left { nil -> nil; cons -> left }`.
- [ ] Apply the same checked abstraction to guarded-recursive/IH motives. The
  generated QuickSort `lowerGraph` classifier must contain the direct recursive
  graph index rather than neutral Matches in its size, accessibility witness,
  and input arguments.
- [ ] Intern motive construction as an equation whose operands include every
  branch classifier, branch refinement action, recursive IH premise, and
  original scrutinee projection. The current code still rebuilds part of this
  information procedurally.
- [ ] Add an expected-result operand from the parent projection. The current
  PI-introduction/Match wiring proves the rule is sufficient for the former
  projection 1052 blocker, but it is not complete until the edge is interned in
  equation topology. If all exact projected case refinements are impossible,
  synthesize a constant motive from that operand while leaving every impossible
  branch body unconstrained.
- [ ] Keep a neutral Match in a telescope argument only when motive abstraction
  cannot solve that argument. It is a residual expression, not an implicit
  inductive eta proof.
- [ ] If two residual endpoints are propositionally but not definitionally
  equal, retain an explicit Layer T equality premise. Do not add a general
  inductive eta or observational equality rule to kernel conversion.
- [ ] Make branch-local classifier specialization consume the immutable typed
  projection action chain. Delete the full-Match enclosing-refinement scan and
  its reindex revision cache instead of adding another call site.
- [ ] Add a focused generated `append` result-package test where the endpoint is
  neutral, plus a negative test whose graph index is a genuinely different
  output.

CG0.3 is complete when the generated QuickSort graph accepts the recursive
`lowerGraph` field without eta conversion, source hints, or a global Match
scan, and rebuilding after a branch refinement yields the same motive key
independently of worklist order.

### CG0.4: Make projection liveness an equation domain

- [x] Replace enclosing-case DFS rediscovery in branch-refinement solving with
  direct dependencies extracted from the immutable Context action chain.
- [x] Remove the branch-refinement equation's dependency on its own branch
  classifier projection; that edge reopened the equation after its own answer.
- [ ] Intern one `Live(TypedProjectionId)` equation for each projection.
- [ ] Make a root live by construction and derive child liveness from incoming
  typed-projection edges.
- [ ] Enable a Match-case edge only from that exact case projection's
  branch-refinement equation. Do not read source occurrence case state.
- [ ] Represent an unresolved branch refinement as unresolved liveness, not as
  dead or live by default.
- [ ] Gate classifier, motive, effect, usage, evidence, and seal obligations on
  the same liveness answer.
- [ ] Delete source-case liveness checks, enclosing-Match scans, and
  seal-all-projections exceptions after consumers migrate.
- [ ] Keep a consumed Match node live even when every case edge is impossible;
  its result/motive remains constrained by its parent expected-result edge.
- [ ] Add boundary tests for one dead branch, all-impossible cases with an
  externally constrained result, and unresolved branch refinement.
- [x] Add and run a focused all-impossible projected Match fixture; it accepts
  the parent expected result without assigning classifiers to dead bodies.

CG0.4 is complete when branch insertion order and source occurrence state do
not affect the live projection set, and every liveness answer is reconstructible
from persisted equation keys and dependencies.

Context projection memoization is adjacent to liveness but is not a liveness
Authority. The immutable action expression and its equation operands determine
the answer. An in-memory memo may retain the concrete Context and Substitution
together with an exact dependency fingerprint. `RECOMPUTE` may omit this memo;
`CHECKPOINT` may retain it only when replay validates that fingerprint. A cache
miss evaluates the same action equation and republishes the memo. No storage
profile may introduce a separate projection-solving path.

### CG0.5: Separate publication projection from source provenance

- [x] Make term exports name the exact accepted typed-publication tuple rather
  than replacing it with an arbitrary Term sharing a canonical key.
- [x] Make termination-evidence publication read its child computation,
  Context, subject, and classifier from the accepted typed projection rather
  than the source occurrence snapshot.
- [x] Reproduce the remaining boundary failure with generated QuickSort: source
  compilation and checked-Core validation pass, then v90 graph writing rejects
  an unresolved source Match case.
- [x] Locate the first invalid executable edge exactly. The accepted export
  Claim has the resolved projected subject, but marking its classifier changes
  the unresolved-case count from zero to twenty-five. No Derivation traversal
  is needed to expose the defect.
- [x] Split typed-publication construction from sealing. Publication tuples can
  now be completed and checked before the occurrence graph and publication
  view become immutable.
- [x] Add a checked publication-classifier completion API. It refuses updates
  unless the exact source occurrence, `TypedProjectionId`, and previous source
  classifier match; it does not mutate the equation answer.
- [x] Make the dedicated accepted-proof pass visit only tuples whose accepted
  subject or classifier differs from the source answer. Unchanged tuples reuse
  their existing accepted evidence instead of creating a second proof route.
- [x] Intern classifier RHS nodes for literal, endpoint, value APP, type APP,
  accepted substitution, thunk, and `Terminates` in the ConstraintDB operand
  arena. Endpoint leaves retain source occurrence identity.
- [x] Materialize generated QuickSort's accepted classifier and accepted proof
  far enough that strict closure no longer fails directly from its exported
  classifier.
- [ ] Keep classifier endpoint provenance in Layer T. A type-level static
  endpoint such as `Terminates(&quickSort)` must retain the source
  assignment/occurrence edge that selected it; lowering it immediately to a
  bare Core Term loses the information required when the same Layer C shape is
  shared by differently typed occurrences.
- [ ] Represent each static endpoint as an operand edge of the owning
  classifier equation. Do not build a second classifier AST and do not infer
  the edge later from a bare Term ID. Direct surface annotations, generated
  contracts, inferred APP/Match/Pi classifiers, and reindexed projections must
  all propagate the same edge kind through the common Layer T equation DAG.
- [ ] Lower a static value expression in classifier position to an ordinary
  typed endpoint root during solver-free seed construction. In particular,
  `@quickSortAcc Nat @le size access input output` must retain an edge to the
  exact `quickSortAcc` assignment occurrence instead of becoming only a bare
  Core APP spine. This lowering emits topology and performs no solving.
- [ ] Represent the surrounding classifier expression as bounded-arity interned
  equations for APP, Pi/family, computation envelope, `Terminates`, accepted
  substitution, and reindex. Do not increase the fixed four-operand array as an
  arbitrary endpoint limit. If the existing constraint operand representation
  cannot host this uniformly, migrate all equation operands to one interned
  variadic edge arena before adding endpoint edges. **Current:** the common
  variadic arena and supported RHS constructors are implemented; remaining
  computation envelopes and synthesized propagation keep this item open.
- [x] Replace `prototype_typing_constraint.operand_typed_projections[4]` with
  one immutable variadic operand-edge arena shared by every Layer T equation
  domain. The arena records operand role, target kind, and target semantic ID;
  topology digests and dependency indexes read the same edge slice.
- [x] Intern the supported classifier-expression constructors directly as
  Layer T RHS nodes in the common ConstraintDB and attach their root to the
  owning classifier equation. The former classifier-expression store and
  occurrence-to-recipe root map are deleted.
- [ ] Complete the remaining constructors and synthesized propagation. Do not
  introduce another classifier-only store while adding computation envelopes,
  reindex actions, and inferred Match/Pi classifiers.
- [ ] Make the equation answer consist logically of the interned classifier
  Term plus its immutable endpoint-dependency set. `result_term` remains a
  disposable current answer; the dependency edges are replay input. Operand
  construction must union and rebase endpoint dependencies as it constructs a
  classifier, so an unannotated synthesized assignment cannot lose provenance.
- [x] Evaluate the same classifier equation DAG under an accepted
  `TypedProjectionId` to obtain the executable classifier. The AST-only
  projection. The AST-only recompilation path and publication-only evaluator
  are deleted. Resume and seal integration are tracked separately.
- [ ] Materialize one accepted classifier per sealed `TypedProjectionId` by
  evaluating those endpoint edges against accepted projected subjects and
  accepted Match-case constructor correspondence. Intern the result and store
  it in `typed_publication`; do not overwrite the source equation or its
  checkpoint answer.
- [ ] Reify the accepted typing Claim against that projected subject and
  accepted classifier. Derive it through checked conversion/projection
  evidence from the source equation; do not manufacture a Claim by copying IDs.
- [ ] Classify every remaining Claim/Derivation edge used by artifact closure
  as source provenance, projected evidence, or both. This follows classifier
  materialization and must not be used to hide a source-bearing accepted
  classifier.
- [x] Root the sealed executable closure only from the accepted projected
  subject/classifier/evidence DAG. Keep source equations, their source-bearing
  answers, Claims, and Derivations in a checkpoint provenance section or omit
  reconstructible entries under `RECOMPUTE`.
- [x] Make the independent checker validate the source-to-projection bridge
  when provenance is present, but never execute unresolved source topology.
- [x] Keep the strict writer rejection for unresolved Match cases in the
  executable closure. A passing test must result from correct roots and roles,
  not a relaxed invariant.
- [ ] Add a boundary fixture where two typed projections share one Layer C
  Match shape but resolve to different Context-indexed constructor identities.
- [ ] Add a synthesized-classifier fixture with no `source_classifier_type_expr`
  whose result transitively contains `Terminates` or another static endpoint.
  This is the permanent regression boundary for QuickSort occurrence 455.
- [x] Pass generated QuickSort artifact write/read/replay and checked backend
  projection with no publication-time solving or canonical-representative
  rewrite. The full suite remains a separate final gate.

- [x] Split closure marking into independently rooted in-memory roles:
  replay-complete seed, optional checkpoint progress, optional source
  provenance, and accepted executable publication. A Term may occur in more
  than one section only through a validated semantic reference; physical IDs do
  not make the roles interchangeable. Persistent section relocation remains a
  CG3 concern.
- [x] Remove the current `all-occurrence-metadata` behavior from executable
  closure. `source_core_term`, `source_classifier`, mutable/current answers,
  and source Context classifiers are replay/provenance inputs unless an
  accepted projected Claim/evidence root independently reaches them.
- [x] Retain strict owner/constructor validation for every Match serialized in
  the accepted executable Layer C closure. Ownerless Match topology is allowed
  only in an open replay/provenance section and may not be linked or executed.
- [x] Add a closure-role regression that proves generated QuickSort has 52
  resolved accepted Match cases and that its 25 auxiliary source Matches do not
  enter the executable section. The test must validate roles, not those exact
  counts as a permanent format invariant.

Historical implementation checkpoint at 2026-09-03T03:00:00+09:00
(superseded where noted by the 03:20 progress table above):

1. Source solving, checked-Core, unsealed publication construction, accepted
   classifier materialization, and accepted proof publication succeed.
2. A partial accepted subject/classifier evaluator proves that APP, Lambda,
   CBPV boundary, zero-clause fold, and termination-witness propagation can be
   computed from exact projections. It deliberately recomputes an equation only
   when a direct operand changed; unconditional Pi reconstruction overwrote a
   valid explicit dependent classifier in the ordinary fixture and was reverted.
3. The former QuickSort classifier chain is now resolved far enough to reach
   strict artifact publication. Candidate publication validates ordinary
   accepted tuples against the sealed typed-publication view, while a
   `CONTEXT_WEAKEN` candidate remains an explicit derived tuple validated by its
   Substitution premise.
4. Artifact v90 now records accepted executable term/case reachability
   separately from the retained dense arena. Generated QuickSort writes 52
   resolved executable Match cases while retaining 25 ownerless
   source/provenance cases outside that closure. Readback, checked backends, and
   malformed-role boundary tests pass.
5. This completed the executable/remainder role cut but did not yet provide the
   `.a` section model. The later in-memory image work has now added independent
   replay seed, checkpoint progress, optional provenance, and accepted
   executable root sets; their allocation-local references still require CG3
   relocation.
6. The earlier exact chain was diagnostically relevant: projections 1647
   through 1666 reach
   Match occurrence 319, whose base equation has no operands and therefore
   cannot observe the accepted `quickSortAcc` endpoint captured in an enclosing
   binder annotation. A Match-only publication rule is forbidden. The missing
   classifier-expression topology had to be emitted during seed lowering. The
   common endpoint/RHS representation now crosses this blocker; propagation
   through every synthesized classifier remains.
7. The AST-only accepted-assignment reconstruction, occurrence recipe map, and
   partial publication evaluator have since been removed. Source Pi solving and
   accepted publication share the common evaluator; resume and sealing remain.

CG0.5's in-memory role condition is now complete: the source equation, current
Solver progress, source provenance, and accepted executable projection are
explicit roles in one image. Source/checkpoint state can remain replayable
without becoming an executable Term root, while the seal certificate covers
only concrete accepted typed-publication roots. CG3 must encode this role split
as independently validated `.a` sections; it is not a temporary v90 exception.

### CG1: Complete the working image and solver-free source boundary

- [x] Introduce one `prototype_compilation_image` aggregate/owner view over the
  existing TermDB, TypeDeclarationDB, Context/Substitution DAGs, equation graph,
  evidence stores, role roots, and progress manifest. It must not copy their
  payloads or create a new answer store.
- [x] Replace the former producer-session public API with the image owner. The
  image borrows semantic stores and owns lowering schedule/results, workspace,
  transaction, and progress cursor without copying those stores.
- [x] Add a source-lowering entry point whose result is that valid in-memory
  open `.a` seed image and which cannot call the Solver.
- [x] Implement that entry point as an explicit stop boundary. Ordinary compile
  calls `lower(image)` and then `solve(image, steps)` on the same
  stores; no duplicate direct-to-publication route is retained.
- [x] End source lowering after canonical Layer C terms, source occurrences,
  Context/Substitution topology, semantic references, and root Layer T
  equations are interned. Equation expansion is topology-only and preserves
  existing equations/answers when later Solver work grows projected topology.
- [x] Represent source-derived type-dependent unresolved lowering decisions as equations or
  resolution goals instead of immediately solving them in the AST traversal.
  Match resolution/typing goals, imported constructor goals, binder inputs,
  declaration inputs, publication names, and expectations are now persistent
  seed records. Solver-time semantic AST reads have been removed; remaining AST
  access is optional diagnostics/lifetime state only.
- [x] Move the former phase-0 usage solve, imported classifier fact
  publication, and answer-dependent pending Match resolution behind the image
  boundary. Structural constructor/name resolution needed to create immutable
  references remains before it.
- [~] Add a domain-wide semantic transition counter. At the boundary, root
  equation and operand counts must be nonzero for a nonempty program while all
  classifier, effect, usage, totality, universe, equality, and proof transition
  counts are zero. **Current:** the open-image check requires nonempty equations
  and verifies every current solution is pending/unsolved with no result or
  evidence; non-ConstraintDB universe/equality/proof progress still needs one
  image-wide manifest.
- [x] Replace recursive PI-argument expected refinement with an interned
  equality edge from the argument projection to its binder Context equation;
  contradiction is reported only when that equation is solved incompatibly.
- [x] Define the in-memory result of this entry point as the canonical
  open `.a` compilation image, even when the caller does not immediately write
  it to disk. `.p` is no longer an implicit alternate Solver input after this
  boundary.

CG1 is complete only when lowering the same `.p` twice interns the same
semantic equation keys regardless of solve effort, and lowering can stop before
the first Solver transition while still producing a valid in-memory
open `.a` image.

The seed migration does not authorize serializing `compile_context`. That
object remains an invocation workspace and still contains diagnostics and
legacy scratch. The persistent `.a` boundary serializes the Layer C/Layer T
stores and explicit seed records only. Semantic Solver reads of AST,
source-lowering results, and process-local imported-interface pointers have
been removed after the open-image boundary. Remaining source allocations are
owner lifetime/rollback overhead, not `.a` content.

Current extraction points in the code are concrete:

- `src/frontend/lowering/finalization_and_entrypoints.inc` now exposes the
  open-image return point after immutable Layer C/Layer T projection topology
  and before the first usage or classifier answer transition.
- `src/frontend/lowering/constraint/model_generation_and_index.inc` now
  performs topology-only lookup-or-intern. Usage and input-fact advancement are
  solve-side operations.
- `prototype_compilation_image_lower()` and
  `prototype_compilation_image_solve()` now form the one in-memory route. Their
  phase cursor remains temporary driver state and is not persisted image
  semantics.
- `src/driver/compiler_session.c` currently treats source compilation as an
  operation that returns a completed graph. It must instead hold the same image
  before and after each explicit solve call.
- `src/driver/read_file.c` still recognizes `.ao` and `.apo` in
  `artifact_path_has_supported_suffix()` and exposes solved-artifact commands.
  Those branches remain until CG5, then change together to `.a` after the new
  reader/writer and all boundary tests exist.

The `.p` reader therefore has exactly one semantic output: an in-memory `.a`
image at the unsolved boundary. A caller may immediately write it, solve it,
or both, but no caller may feed `.p` directly to a second Solver route. The
root equations and immutable operands remain in every `.a`; they are the
replay-complete seed, not a cache. `--a-retention=recompute` may omit canonical
Solver expansion and its current answers so both are regenerated, while
`--a-retention=checkpoint` retains replay-valid expanded equations, answers, and
evidence so solved work is reused. Neither profile stores a history of
superseded answers.

The indexed-family dependency repair is the concrete model for CG1 topology:
if `COMPUTATION_FOLD(IH(r), K)` needs the classifier of recursive argument `r`,
that premise is an operand of the fold equation in `.a`. It must not be
rediscovered by recursively inspecting the current Solver answer.

### CG2: One advancement, sealing, and driver path

- [x] Add one explicit `solve(image, steps)` entry point consuming a rebuilt or
  existing ready frontier. All compile effort after structural lowering enters
  here, including usage, imported classifier, pending Match, effect, totality,
  universe, equality, and proof work.
- [x] Add one explicit `seal(image)` validator that never invokes the Solver or
  mutates an equation answer. Solver completion is now `SOLVED`; read-only seal
  validation returns a stable certificate over the current accepted image and
  binds it to the accepted root set and closed evidence DAG.
- [ ] Remove finalization branches that reconstruct missing equations or
  answers while publishing.
- [ ] Remove entry points that construct accepted publication state directly.
- [ ] Make source continuation and loaded-image resume call this exact Solver
  entry point; neither path may rebuild topology through a private compile
  routine.
- [x] Assign independent replay-seed, source-provenance, checkpoint-progress,
  and accepted-publication root sets over the same stores. These are reachability
  roles, not stage objects. The seal is bound to the accepted roots and their
  closed evidence graph; encoding those roots with persistent section IDs is
  deferred to CG3.
- [~] Add the compilation-image CLI controls through one driver path.
  `--solve-effort STEPS` now controls only solving. Retention remains to be
  added only to writing, and sealing remains callable only through the
  read-only validator.

CG2 does not yet write `.a`. It proves that source-created and eventually
loaded images share one API and one semantic pathway before a wire format is
allowed to freeze the boundary.

### CG2.5: Extract persistent Layer T state from `compile_context`

This stage is a newly identified prerequisite to CG3. The in-memory image is
self-contained only because it owns `struct compile_context`. That object still
contains `struct prototype_typing_pipeline_legacy_state`, and that struct mixes
semantic input, current answers, derived indexes, and disposable work cursors.
Serializing it would freeze the transitional mixed lowerer as a second Layer T
Authority. Omitting it today would make loaded-image resume incomplete.

Every field must have exactly one disposition:

| Current legacy state | Semantic class | Required destination |
| --- | --- | --- |
| `classifier_seeds`, suppression flags, solver input facts | immutable replay input | **completed:** seeds live in `typing_pipeline.seed`; the duplicate input-fact table and never-written suppression flag are deleted |
| expected APP codomain binders, motive applications, IH owner/case arrays | semantic relations plus lookup indexes | relation/equation operands are persistent; occurrence-to-equation maps are rebuilt from those edges |
| classifier initialization flags, `solution_revision`, active constraint ID | runtime cursor/cache | fresh Solver workspace; never serialized or included in equation identity |
| occurrence usage solutions and usage entry slices | current semantic answers | canonical usage-domain equation solution/evidence; occurrence lookup is derived |
| effect meta owner/placeholder/evidence links | immutable equation topology | canonical effect equation and operands |
| effect atoms, materialized row, and solved state | current semantic answer | the one answer cell owned by the effect equation |
| effect occurrence map | derived lookup index | rebuilt after lower/load |
| termination-evidence occurrence links | typed/evidence dependency | immutable typed-projection/equation edge or accepted Claim dependency |
| reification cache, pending IDs, synchronization counters, materialization booleans | runtime cache/frontier | rebuilt Solver workspace; never persisted as meaning |

Implementation work:

- [ ] Add a static field inventory test so every member of
  `prototype_typing_pipeline_legacy_state` is assigned one of the four classes:
  replay input, canonical answer/evidence, rebuildable index, or disposable
  workspace.
- [~] Migrate immutable classifier facts and all motive/IH relations to seed or
  equation operands. Classifier seeds are migrated, classifier input facts are
  read directly from immutable binder/declaration seed sets, and the
  never-written `classifier_seed_suppressed` flag is deleted. Motive/IH relation
  and index cleanup remains.
- [ ] Move usage results to usage equations in ConstraintDB. A usage answer has
  one owner equation and one validation fingerprint; `usage_solutions[]` is not
  retained as a second answer table.
- [ ] Split effect meta topology from effect answers. Intern owner, placeholder,
  and evidence dependencies; store the materialized row only in the owning
  equation solution. Rebuild `meta_for_occurrence` from canonical topology.
- [ ] Make accepted Claims/Derivations the evidence Authority. Keep proof
  reification and materialization queues as rebuildable workspace only.
- [ ] Replace `compile_context.legacy_typing` with a narrowly named Solver
  workspace containing no semantic input or accepted/current answer. Allocate
  and rebuild it after `.p` lowering and after `.a` loading through the same
  function.
- [ ] Add a drop-and-rebuild test: pause a solve, discard every runtime index,
  cursor, and cache, rebuild them from the image, and prove that continued solve
  reaches the same equation keys, answers, root sets, diagnostics, and seal as
  uninterrupted solve.
- [ ] Add a negative static audit that rejects semantic writes to the removed
  legacy fields and rejects serialization APIs accepting `compile_context` or a
  raw `prototype_typing_pipeline_legacy_state`.
- [ ] Delete `typing_pipeline_legacy_state.h` and its adapter once all production
  reads have migrated. Do not retain an adapter to reduce the edit size.

CG2.5 is complete only when `solve(image, steps)` can start from canonical image
stores plus a freshly rebuilt runtime workspace. At that point a writer may
omit all disposable process state without changing what can be solved.

The next CG2.5 cut is the occurrence-keyed Match/IH state. Its semantic owner is
the immutable `IH_EXPECTED`/motive equation and its typed-projection operands.
Any owner-to-equation acceleration structure is rebuilt from those equations;
it is not a checkpoint section and cannot be consulted as an answer Authority.
After that cut, classifier cursors/caches, usage answers, effect topology and
answers, and evidence workspace are migrated in that order.

### CG3: Canonical equation persistence

- [ ] Freeze a section contract with one mandatory replay seed and optional
  provenance, canonical expansion, current answers/evidence, caches, and seal.
  Do not serialize `compile_context`, phase cursors, queues, or retry history.
- [ ] Define the mandatory seed sections as Layer C Terms, declarations and
  imports, Context/Substitution/IH topology, typed projections, root Layer T
  equations and operands, and replay-seed roots. Define generated equations,
  current answers, evidence, residual obligations, source provenance, accepted
  publication, seal, and caches as separately reference-checked optional
  sections. Absence of an optional section has one meaning: rebuild or omit that
  capability; it never selects another Solver.
- [ ] Define stable semantic keys for every Layer T equation domain.
- [ ] Build keys from rule/domain and operand semantic keys, never from
  allocation order or transient physical IDs.
- [ ] Use section-local wire ordinals only as relocated addresses. A hash may
  select an intern bucket but must never establish equality; exact decoded
  payload and edge comparison remains authoritative. Recursive equations are
  validated as an SCC subgraph with exact external references rather than by a
  hash of process-local IDs.
- [ ] Serialize replay-complete root equations independently of answers.
- [ ] Serialize canonical Solver-expanded equations/dependencies as optional
  checkpoint progress.
- [ ] Replay-validate that each retained expanded equation can be generated by
  its recorded rule from its recorded operands; arbitrary injected constraints
  are invalid checkpoint data.
- [ ] Rebuild lookup indexes and the ready frontier from the seed plus any
  validated retained expansion on load.
- [ ] Reject duplicate keys with different equation IDs.
- [ ] Canonically compact/sort persisted IDs at write time when deterministic
  byte output is required; runtime physical IDs are not semantic identity.
- [ ] Serialize SCC membership or deterministically rebuild it from persisted
  dependency edges; never persist an ad hoc recursion flag.

### CG4: Checkpoint sections

- [ ] Implement `RECOMPUTE` as a writer projection that emits the replay-complete
  seed and omits all reconstructible Solver progress.
- [ ] Implement `CHECKPOINT` as a writer projection over that identical seed
  plus canonical generated equations and each equation's one current
  replay-valid state. It retains all canonical intermediate type/effect/usage/
  totality/equality constraints and current results needed to continue without
  recomputing completed work; it is not an append-only answer history.
- [ ] Serialize answers and Derivations as optional equation-ID-indexed records.
- [ ] Validate every loaded answer before marking its equation solved.
- [ ] Persist each retained answer with equation key/ID, semantic operand
  fingerprint, lifecycle state, and evidence reference.
- [ ] Permit provisional SCC candidates only in checkpoint progress, never as sealed
  facts; replay them as work hints after validation.
- [ ] Record residual obligations and effort policy without serializing queues.
- [ ] Record the Solver cost-model version and consumed deterministic steps;
  reject or requeue retained progress when its validation version is
  unsupported, without changing the meaning of the replay seed.
- [ ] Add optional revision/digest-keyed normalization cache sections.
- [ ] Prove by load-time validation that omitting checkpoint sections merely
  changes recomputation cost, while retaining them never bypasses equation or
  Derivation verification.

### CG5: Resume and sealing

- [ ] Resume solving from unsolved dependency-frontier equations only.
- [x] Make in-memory sealing a read-only validation/projection of solved
  Authority and bind the certificate to accepted roots and closed evidence.
- [ ] Apply the same read-only seal validation after `.a` load and reject a
  retained certificate whose referenced sections or digests are absent.
- [ ] Remove publication-time reconstruction of solver answers.
- [ ] Ensure artifact compaction never becomes an alternate solving stage.
- [ ] Preserve source provenance and accepted publication as distinct `.a`
  sections. Only accepted projection roots participate in executable closure;
  source provenance is inspectable/replayable but not executable.
- [ ] Replace the current `.ao`/`.apo` suffix aliases in
  `artifact_path_has_supported_suffix()` with `.a`. They currently identify the
  same wire kind, so retaining them would preserve naming branches without a
  semantic distinction.
- [ ] Rename artifact paths in integration tests and documentation in the same
  cutover. Do not keep dual-write or dual-read compatibility code. Per-source,
  linked, and aggregate outputs all use `.a`.
- [ ] Route source compile, resume, link, aggregate, interface read, graph read,
  and backend execution through one `.a` loader and section validator.
- [ ] Remove legacy publication-only command names only after their operations
  are expressed as solve, seal, link, or inspect over the same compilation
  image; do not preserve a hidden `.apo` route behind renamed commands.

### CG6: Verification

- [ ] Recomputable and checkpoint images publish equal semantic artifacts.
- [ ] Recomputing from the seed recreates the same semantic equation keys as a
  retained checkpoint expansion, independent of physical equation IDs.
- [ ] Loading a checkpoint does not add duplicate equations or answers.
- [ ] Loading a checkpoint after changing any semantic operand rejects or
  requeues the affected answer without recursively deleting unrelated answers.
- [ ] Removing all disposable cache sections changes performance only.
- [ ] A stale cache is rejected or ignored by digest.
- [ ] An invalid checkpoint answer is rejected by replay.
- [ ] Indexed-family `map`, dependent Match, Acc/QuickSort, CBPV/effects, and
  Higher Observational fragments pass after resume.
- [ ] Generated QuickSort reaches the same classifier fixed point from a fresh
  `RECOMPUTE` image and from checkpoints taken before and inside its recursive
  Match/IH/fold SCC.
- [ ] Static audit rejects direct publication to reindex-derived Context
  equations.
- [ ] The two storage profiles are verified to differ only in optional
  checkpoint/cache sections and compile effort.
- [ ] The same reader and Solver handle absent/present progress sections and an
  optional seal certificate without duplicated semantic code.
- [ ] The source-to-image boundary performs zero Solver transitions, and both
  storage profiles expose identical seed keys and eventually intern identical
  expanded equation keys, operands, and dependencies.
- [ ] Removing the optional source-provenance section leaves the same sealed
  executable roots; retaining it validates the same source-to-projection bridge
  without introducing unresolved source Terms into the executable closure.
- [ ] Context/projection allocation remains below an explicit measured budget;
  increasing a compile-time capacity alone does not satisfy this gate.

Last fully green focused checkpoint before the read-only APP-domain cut:

- [x] `explicit_index_family_acc_eliminator_check.p` solves without a residual.
- [x] computation-block sequencing passes.
- [x] direct IF8 order and fuel-free QuickSort fixtures solve.
- [x] dependent Match refinement passes its focused integration suite.
- [x] the CBPV surface suite passes with dependency-keyed reindex caching;
  cache expansion before Context closure remains forbidden.
- [x] generated function-graph solving passes neutral Match constructor
  resolution without consulting a source classifier hint.
- [x] generated QuickSort output-tree source solving and checked-Core
  validation preserve the source/reindex/typed-projection ownership boundary.
- [x] generated QuickSort materializes its accepted classifier and accepted
  proof; strict publication no longer fails directly from that exported tuple.
- [x] generated QuickSort sealed writing keeps the 25 ownerless source/
  auxiliary Match cases outside the 52-case executable closure. Readback and
  backend projection pass without weakening the Match invariant or mutating
  source cases into projected cases.

Current working-tree checkpoint after the read-only APP-domain cut:

- [x] the malformed IF8 missing-bound fixture rejects with the canonical
  terminal `classifier-equation-contradiction` diagnostic at its APP equation.
- [x] the malformed IF8 forged-bound fixture rejects with the canonical
  `classifier-equation-contradiction` diagnostic emitted at the generic answer
  transition boundary.
- [x] the incompatible external decrease-evidence fixture terminates with
  `motive-equation-mismatch`: its surface APP domain is valid, but the recursive
  motive cannot derive structural decrease evidence from the opaque term.
- [x] PI-domain and sequence-binder relations plus derived Match/IH/fold SCCs
  close the direct Acc, dependent-Match, and issue-23 recursive-IH fixtures
  without foreign answer publication.
- [x] the in-memory `.p -> open image` boundary returns non-empty Layer C/Layer
  T topology with zero semantic transitions; bounded solve and uninterrupted
  solve converge to identical canonical equations and current solutions, and
  both paths solve after source-state pointers have been disconnected.
- [x] Lowering no longer accepts or consumes Solver effort. Solver completion
  and sealing are separate: repeated sealing preserves transition counts and
  Core/Layer T revisions and returns an identical certificate.
- [x] `--solve-effort STEPS` reaches the same public solve entry point; a zero
  budget pauses a nonempty source program instead of advancing it during lower.
- [ ] the full accepted suite has not been rerun after these focused changes.
- [ ] `.a` persistence and the `.apo`/`.ao` suffix cutover remain unimplemented.

The remaining pre-persistence work is extraction of semantic state from
`compile_context.legacy_typing`, stable replay identity, and complete domain
accounting. Explicit seed/provenance/checkpoint/accepted root roles are now
implemented in memory, and solved state is separate from read-only sealing.
Semantic AST/source-plan reads after the open-image boundary have been removed;
remaining AST access is optional diagnostic provenance. Reset-free root-equation
expansion is complete. None of the remaining work may become a stage-specific
exception in the `.a` reader or Solver.

The next implementation order is:

1. execute CG2.5 and remove semantic inputs/answers from
   `compile_context.legacy_typing`;
2. finish stable semantic keys and closed Context/Substitution/IH action
   identity without replacing fast in-memory IDs;
3. [done] four explicit role-root sets and accepted-root-bound read-only seals;
4. implement one sectioned `.a` writer/loader with `RECOMPUTE` and `CHECKPOINT`
   writer projections;
5. perform load-time replay validation and rebuild the ready frontier, SCCs,
   and disposable indexes;
6. run resume/seal equivalence tests and perform the repository-wide
   `.ao`/`.apo` to `.a` cutover.

## Non-goals

- Do not merge Layer C computation with Layer T typing/proof data.
- Do not preserve frontend AST objects merely to resume solving.
- Do not serialize branch-specific retries, global refresh flags, or work queues.
- Do not hide duplicated authorities behind consistency checks.
- Do not let seal validation mutate the working graph or solve equations.
