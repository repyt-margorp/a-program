# Post-Surface Issues and Authority Refactoring

Date: 2026-09-25
Updated: 2026-09-26
Status: P1-P3 closed; P4 Identity-boundary/Lambda-scope and P5 effect-owner/shared
work-header/Handler slices verified by clean publication-tree acceptance and sanitizers.
Broader P4 typed-proof migration and P5 synthesis modularity remain open.
Initial review baseline: `40375d734896a456f5ad2827ad8fe0f10ff61517`.
Verified implementation milestones: `60bde88` (P4), `dcc58ec` (first P5 slice),
`84a54e2` (shared work/private state), and `c89edb7` (Handler owner);
the latest clean gate covers all four.
Inherited implementation edits: Context/IADT relocation changes in
`evidence.[ch]`, `iadt.[ch]`, two unit tests and two derived-LT files. These
are excluded from the publication candidate and preserved locally, together
with the inherited September 18 IADT-priority plan edits.

This is the next active work list. The [surface-removal plan](2026-09-25-FUNCTION-WITNESS-SURFACE-REMOVAL-COMPLETION-PLAN.md)
records the completed milestone. The [authority plan](2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md)
retains historical evidence and its unfulfilled R2-R5 criteria; it does not
provide a second execution order. The order below is the agent's proposal.

## Problem List

| ID | Problem | Issue / PR | Status |
| --- | --- | --- | --- |
| P1 | Align open reports with published implementation | #32; PRs #20, #35-#38 | Verified; closed |
| P2 | Reassess the old computation-result report against current contracts | #13 | Closed by user-approved scope decision |
| P3 | Resolve the remaining MergeSort report against existing regressions | #28 | Verified; closed |
| P4 | Make typed proof structure authoritative; remove redundant evidence dependency | Existing R2-R5 plans | Identity-boundary and Lambda-scope slices verified; broader migration pending |
| P5 | Separate synthesis by semantic owner without duplicating shared machinery | User follow-up on synthesis.c | Effect-owner/shared-header/Handler slices verified; legacy source state remains |

## P1. Issue and PR Disposition

### Subjective (User)

English paraphrase, this conversation, 2026-09-25: move on from the resolved
syntax work; inspect Issues and PRs and make the next plan. Preserve the user's
requirements separately from AI decisions and keep progress records concise.
Earlier direction in this conversation: remove global `*f` without introducing
a replacement accessor; retain local IH `*arg` and ordinary proof terms.
Follow-up, 2026-09-25: close Issues and PRs that are clearly resolved.

### Objective (Code)

- Initial inspection found open Issues [#13](https://github.com/repyt-margorp/a-program/issues/13),
  [#28](https://github.com/repyt-margorp/a-program/issues/28), and
  [#32](https://github.com/repyt-margorp/a-program/issues/32).
- Commit `975bd56` removed global witness syntax. The completion plan records
  a clean publication-tree acceptance run, optional-generator isolation, and
  ordinary-result QuickSort proofs. Those are historical test results, not a
  fresh run of the inherited dirty worktree. This pass additionally compared
  all 895 source/test blobs in the publication tree with Main (zero differences)
  and reran the related tests using that tree and its existing strict-O2 build.
- The then-open PRs #35-#38 add audit documents against `a72cda3`, targeting the old
  `rewrite/pointer-core-hott` branch. Issues #31, #33, #34 are already closed.
  Their documents are not present under the proposed paths in current Main.
- PR #20 concerns the older compiler; its [critical review](2026-08-23T23-40-38-PR20-MEANING-BOUNDARY-CRITICAL-REVIEW-AND-IMPLEMENTATION-PLAN.md)
  records implementation and verification in August. That is not a current
  pointer-compiler audit.

### Assessment

Agent assessment of the reviewed PRs:

| PR | Adopted insight | Agent disposition (closed as superseded) |
| --- | --- | --- |
| [#20](https://github.com/repyt-margorp/a-program/pull/20) | Exact ownership of residuals, graph associations and Identity evidence matters | Historical review; do not transplant old artifact structures into the pointer compiler |
| [#35](https://github.com/repyt-margorp/a-program/pull/35) | `::` cannot select a dependent motive; prove the generic partition obligation | #31's completed proof/explicit-motive tests supersede the original failure report |
| [#36](https://github.com/repyt-margorp/a-program/pull/36) | Removing syntax does not remove the need for checked proofs | Current direct theorem resolves its motivating use case; general automatic graph adequacy is still not established |
| [#37](https://github.com/repyt-margorp/a-program/pull/37) | Separate IADT induction from the predicate expressing a checked totality contract | #33's tested retain decision stands; deleting native rules is not an outstanding requirement |
| [#38](https://github.com/repyt-margorp/a-program/pull/38) | Compare derived LT lifting and proof-representation dependence | #34 completed the experiment; preserve its measured tradeoff, not mandatory constructor deletion |

#32 is resolved as the surface-design audit: the motivating general theorem and
interface are checked. Its closure explicitly excludes automatic graph adequacy
for every function; no such new feature was silently declared implemented.

### Plan

- [x] Map #32's criteria to published tests and remaining nonclaims; verify
  `check-quick-result` and `check-witness-isolation` on the clean candidate.
- [x] Record the [#32 disposition](https://github.com/repyt-margorp/a-program/issues/32#issuecomment-5833072998)
  and close its satisfied scope with explicit limitations.
- [x] Comment on and close PRs #20, #35, #36, #37, #38 as superseded, unmerged
  historical proposals. Their source reports remain accessible in the PRs.
- Completion: every open review has a current, attributed disposition; no
  historical suggestion is silently promoted to a user requirement.

## P2. Reassess the Legacy Computation-Result Issue

### Subjective (User)

Earlier user direction in this conversation (English paraphrase): establish
general Sorted for the result of the ordinary existing QuickSort, with no
privileged function-witness spelling. Follow-up, 2026-09-25: the user points out
that #13 is an old Issue. Its positional-sortedness checklist comes from that
historical report, not a new user request to expand the library.
Further user concern, 2026-09-25 (English paraphrase): automatic equality
retention at `:=` may be unnecessary; the intended Higher Observational system
should allow equality proofs to be conveyed explicitly. The user asks why this
additional mechanism is needed, rather than requesting its implementation.
Final user decision, 2026-09-25 (English paraphrase): record the explicit
Equality/transport reasoning in #13 and close it. Automatic binding equations
and the positional-sortedness corollary are not required for this closure.

### Objective (Code)

- [generic-quick-sorted-result.p](../src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p)
  defines `quick_correct_existing`, concluding
  `general_sorted A R (quickSort A (&le) xs)` for open inputs, with explicit
  order/comparator hypotheses. It does not assume `partition_spec`.
- [sorted-proof-provider.p](../src/prototype/pointer/tests/fixtures/sorted-proof-provider.p)
  defines `general_sorted` through head-to-tail bounds and recursive sortedness.
  This is not literally #13's `At`/`SortedByIndex` presentation.
- Inspection of the current sorting fixtures and name searches found no
  positional-sortedness conversion theorem. This is a coverage finding, not a
  demonstrated kernel rejection of that theorem.
- [quick_result.sh](../src/prototype/pointer/tests/quick_result.sh) covers direct
  proofs, wrong result/motive checks, and ordinary/retained saved-image loads.
  #13's August comments describe the superseded `Returns` implementation.
- Fresh probes on the pinned publication build: the original direct `choose`
  post-check passes (1,471 steps); changing true to false rejects the incorrect
  nil proof (1,519). The report's block-bound variant rejects (1,503).
  Even `{ result := NatList.nil; Sorted.nilSorted :: Sorted result; }` rejects
  (670), whereas the same block checking against literal `NatList.nil` passes
  (565). Thus the remaining spelling difference is not specific to Match.
- `synthesis.c:block_step` creates a fresh result Context and checks the
  continuation before constructing the sequence. `source_expect_step` checks
  the resulting classifier; it does not substitute the earlier RHS for that
  abstract result binder during the post-check.
- `prelude.c:pg_identity_library` constructs equality, reflexivity, congruence
  and transport terms. `evidence.c:pg_prove_identity_transport` consumes an
  explicit Universe Identity and checks the value's endpoint and Context.
  This is existing proof transport, not automatic evidence of a block binding's
  value. A complete surface equality proof for the block probe has not been run.

### Assessment

The old claim that no supported route exists for post-hoc properties is
superseded by the current direct result projection and general theorem. Do not
schedule another compiler feature merely because #13 remained open historically.

Correction of this plan's initial agent proposal, 2026-09-25: making a new
positional-sortedness lemma the next mandatory implementation was premature.
Such a lemma is a plausible ordinary library corollary; it is not yet checked,
and its absent spelling does not establish a compiler defect. The user's final
closure decision leaves it as optional library work.

The block probe exposes an abstract-binder checking boundary. Under the agreed
closure scope, a supported result-proof route satisfies the original capability;
the unassisted block spelling is not a required positive test. Its rejection
does not justify equality assumptions, expected-type synthesis, or general
dependent sequencing.

User-approved direction, following the earlier agent assessment: do not schedule automatic equality
retention for `:=`. It is a possible elaboration convenience, not a prerequisite
for explicit equality reasoning or the already checked QuickSort theorem.
Given evidence `e : nil = result`, a checked action on the Sorted family and
transport can carry `Sorted nil` to `Sorted result`. The equality evidence must
itself be constructed or passed in scope; adding Higher Identity rules does not
make an abstract binder equal to its earlier source expression automatically.
Keep this explanatory route separate from claims of current surface coverage.

### Plan

- [x] Recheck the original direct and block examples, with positive/negative
  controls; identify the currently working ordinary-result theorem.
- [x] Record the supported ordinary-result theorem and current tests, the
  rejected bare block spelling, and the unimplemented positional corollary.
  The latter two are explicitly excluded from the agreed closure requirements;
  this does not claim every historical checklist item was implemented.
- [x] Publish the [Equality explanation and closure decision](https://github.com/repyt-margorp/a-program/issues/13#issuecomment-5833394020)
  and close #13 as requested. Use checked explicit evidence/transport for future
  proof connections; no implicit binding equations are scheduled.
- Deferred: a source proof of `general_sorted -> SortedByIndex`, by ordinary
  induction over sortedness and lookup/order evidence. No new kernel rule or
  change to QuickSort is proposed, and this is not currently the next code task.
- Completion: an accurate disposition of the old Issue, without resurrecting
  `Returns`, replay architecture, or silently enlarging the requested scope.

## P3. MergeSort Report Follow-up

### Subjective (User)

Earlier user direction (English paraphrase): fix the reported MergeSort behavior
within the same general system, retaining theoretical consistency and existing
programs. No new request for a linear-time merge algorithm was made here.

### Objective (Code)

- #28's comments correct its original `*tail right` overapplication and record
  later captured/indexed/helper-graph fixes. The original contamination claim
  is not a confirmed remaining bug.
- [merge_composition.sh](../src/prototype/pointer/tests/merge_composition.sh)
  covers both valid IH forms, declaration order, captures, helpers, indexed
  inputs, wrong applications and images.
- The current provider contains `mergeBy`, `mergeSortFuel` and `mergeSort`.
  [sort-merge-property.p](../src/prototype/pointer/tests/acceptance/sort-merge-property.p)
  also has `merge_result_sorted` for the ordinary result; `sort_insertion.sh`
  runs its consumers. The September 14 audit's missing-full-program statement
  therefore no longer describes all current coverage. Equality with the
  reporter's exact external source has not been established.

### Assessment

Agent decision after verification: the existing tests cover the reported
limitations, so close #28 with the current implementation evidence.
The current insertion-based merge is not linear-time; that cost is a separate
algorithmic limitation, not a reason to label these typing fixes incomplete.

### Plan

- [x] Run merge composition and the complete sort-proof assembly on the clean
  candidate; check ordinary results, general proof, named graph exports and images.
- [x] Check existing indexed-capture, branch-name collision and canonical-leaf
  regressions. The original `@lessEqual` request also passes (10,623 steps).
- [x] Identify the permanent full-program source and ordinary-result theorem;
  state that an unattached external version and linear-time merging are not verified.
- [x] Comment on and close #28 against its remaining criteria at the pinned
  revision. No compiler repair or additional regression was needed in this pass.

## P4. Resume the Authority Refactor

### Subjective (User)

Earlier user direction (English paraphrase): after the IADT/sorting repairs,
remove repeated reconstruction and consolidate ownership. Keep erased Core
computation separate from typed evidence; prefer a consistent design over a
small migration. Report code, test and documentation line changes separately.

Follow-up, 2026-09-25 (English paraphrase): review the refactoring interrupted
by the issues in the thirties, establish what has actually progressed and how
far functional separation has gone, and identify necessary remaining work.

Later clarification, same date (English paraphrase): the concern is not merely
the size of `evidence.c`. Proof structure should be carried by the constructed,
typed Terms in the Curry-Howard sense, rather than retaining all proof reasons
in an additional Evidence structure. This supersedes the agent's initial
proposal to start primarily with synchronous-helper scheduling.

Latest direction, same date (English paraphrase): proceed while inspecting the
code, revising the approach where necessary.

### Objective (Code)

Static review at `40375d7`, with the inherited diff inspected separately.
Scope: the active pointer build, typed structure/acceptance, synthesis requests,
source persistence, graph/witness split, relevant tests and the interrupted
plans. This is not a fresh whole-repository correctness or performance audit.
Fresh GitHub inspection finds #29 and #31-#34 closed, PR #30 merged and
#35-#38 closed without merge. Issue closure does not close the broad refactor.

#### Completed Consolidation

| Area | Current owner and evidence | Status |
| --- | --- | --- |
| Erased computation | `graph.h:pg_term` has Lambda/Application/Reference; `eval.c` performs computation. Structural interning does not run conversion | Preserved; no request to merge this with typed data |
| Typed conclusion | `typing.h:pg_occurrence` owns context/Core/classifier, typed inputs and retained maps; `evidence.c:pg_evidence` points to the subject rather than storing a second term conclusion | R1 implemented; alternative derivations remain separate |
| Context action | `pg_occurrence_action_request`, `pg_context_lift_request` and typed-input requests share work in `pg_typing`; `reindex_step` converges producers before advancing the same action | Substantial R2 migration implemented, not an untouched task |
| Checked map reuse | Exact binder-image lookup, checked prefixes, constructor parameters/instances and IH weakening retain maps rather than reconstructing them | Published epochs include `bf7f8b2`, `60d7edb`, `d5d7da5`, `7e3e560`; do not repeat these migrations |
| Accepted structure and pending requests | `synthesis.c:accepted_structure` reads the accepted subject directly; requests are interned by role/inputs; `intern_rule_input` shares immutable rule headers | Accepted fast path and request/header sharing implemented (`607058c`, `ad3d94c`, `b2c8574`) |
| Identity discovery | `source_has_identity` requests shared resumable formation and skips repeated lexical contexts | The old synchronous-formation restart diagnosis was addressed by `d2b23f6` |
| Source persistence | Allocation discovery uses retained contexts/declarations and indexed source references; `source_io.c:index_origin_reference` resumes dependencies without a global rescan | Old saved-formation allocation dependency removed (`121d6e5`); source indexing and transport epochs published |
| Traversal scratch | `pg_typing_wait_push/pop/clear` shares recyclable frame storage without sharing live stacks | Implemented in `a9b3e66` |
| Function graph / generated witness | `function_graph.c` constructs the graph; `function_witness.c` optionally constructs packets. Ordinary `pointer-check` excludes `WITNESS_SOURCE` | Executable-level separation implemented in `975bd56`; ordinary QuickSort-result proof no longer requires global `*f` |

The last split deliberately shares the source plan in
`function_graph_internal.h`; optional packet/witness progress fields also remain
in that state. The two modules are not independent data models. This avoids
rediscovering call sites and is not evidence of competing authority.

#### Typed Structure Is Not Yet Self-Sufficient

Object-language witnesses already exist as Terms: `pg_prove_reflexivity`
constructs an Identity action Term with an Identity classifier. Nevertheless,
each introduction also records a C-side rule and its premise Evidence pointers.
For example, `pg_prove_lambda` builds a typed occurrence with its Pi type and
body operand, then records `PG_LAMBDA_INTRO` with separate Pi/body derivations.
`pg_prove_identity_transport` likewise stores family/value typed operands and
an additional target/family/value premise list. These are distinct logical
roles, but storing both dependency graphs permanently is an implementation
choice, not a requirement of proof-as-Term representation.

Consumers at HEAD still require the additional graph:

- `action.c:identity_structure` enumerates accepted Evidence for a typed subject
  to obtain its Identity boundary.
- `pg_prove_structural_subject` returns existing Evidence or reconstructs
  selected mapped/variable uses. It is not a general checker that can accept
  every Lambda, constructor or Identity term from typed structure alone.
- At HEAD, `derivation.c:transport_direction` reads a target derivation's rule, and
  `derivation_io.c:premise` / `write_dag` persist rule/premise dependencies.

Local first slice, 2026-09-25: `pg_identity_field_view` now reads transport/lift
operands and direction from their exact Oracle application, without evaluation
or Evidence. Serialization parameter extraction uses it instead of the removed
`transport_direction` premise traversal. Admission and persisted premises remain
unchanged; this is not yet a history-independent typed checker.

The next local slice moves `pg_identity_boundary_view` into `action.c/h`, with
borrowed typed operands/maps rather than Evidence premises. Formation, face
selection, indexed transport and derivation loading now use that view. Direct
boundary checking reuses exact accepted typing or checks the retained inputs;
it no longer enumerates parent derivations. `pg_prove_lambda` likewise reads
the Pi's scoped codomain input instead of requiring a `PG_PI_FORM` receipt.
The same Pi accepted through U-content elimination is usable without rebuilding
its scope. Both changes retain ordinary classifier/scope/owner validation.

Current input/consumer inventory (partial P4.2, not a completed checker design):

| Construction | Retained semantic inputs | Remaining migration |
| --- | --- | --- |
| Pi / Lambda | Pi domain/codomain operands, scoped Context and Core binder; Lambda type/body/annotation | Lambda now uses that scope, but initial Pi/context admission and generic structural checking still require migration |
| APP | Function/argument operands and instantiated result `type` | `formed_classifier` and dependent codomain checking still return Evidence; retain the shared context action, not a new substitution engine |
| Constructor | Typed field operands and result `type`; nominal declaration lives with IADT | `constructor_instance` still consumes schema/parameter/instance receipts. Audit the declaration link and maps before deleting them; constructor Core alone cannot choose a nominal type |
| Match / IH | Scrutinee, branches, motive, formation, parameter map and IH allocation are retained | Elimination still checks its rule/schema through Evidence. Preserve motive telescopes and IH binder allocation rather than infer them from erased branch code |
| Identity | Family, endpoints, maps and paths, now read in its owner | Direct boundary checking works without parent history; children/maps still need accepted typing. Origin recovery and full recursive typed checking remain unfinished |
| Conversion / normalization | Origin, new type boundary and reusable computation certificates | These are checked computations, not object Equality proofs. Do not discard validation or infer conversion from equal Core tags |

Reinspection at `6f2b466`, 2026-09-26: `pg_context` retains a raw declared
classifier and a family telescope, not the typed formation of that declaration.
`binding_level` and classifier lookup still recover formation from context
Evidence premises. Ordinary Pi retains a typed domain, but a family-domain Pi
retains the scoped family variable instead. Therefore reject extending
`pg_prove_structural_subject` with a Core-tag switch that guesses these missing
inputs. Before the P4.3 checker migration, specify the owner of ordinary and
family declaration formation, including Universe bounds and image transport.
Do not replace old receipts with another copied universal proof payload or
claim that file extraction makes the typed structure self-sufficient.

Image boundary: `occurrence_io.c` already transports typed operands, maps and
scopes without accepting proofs; it does not yet replace `derivation_io.c`'s
premise DAG. `retained_io.c` writes rule inputs, effect equations, optional
reduction receipts and term roots. In `tests/derivation_io.c`, the constructor
roundtrip explicitly requires two receipts with the same typed subject and
context map to remain distinct. That is a legacy history-retention contract,
not two distinct object proof Terms. Review that contract in P4.3; retain the
test's nominal declaration, IH allocation, zero-fuel and fresh-check assertions.

Therefore R1 consolidated conclusions, but did **not** remove dependence on
the derivation graph. Renaming Evidence, splitting its file, or adding another
subject accessor would not finish this part of the refactor.

#### Remaining Work, With Current Evidence

1. **Incremental callers still drain synchronous helpers.**
   `synthesis.c:step` handles `LIFT_JOB` by calling
   `pg_prove_substitution_lift`, whose implementation drains
   `pg_context_lift_advance(..., 1024)` in a loop. Substitution composition
   similarly iterates all images and drains occurrence actions with
   `UINT64_MAX`. These helpers are called from synthesis, graph construction
   and Identity action. `pg_synthesis_advance` charges one step per dispatched
   job, not per nested helper operation. This is a confirmed granularity gap,
   not proof that completed work is recomputed or that it dominates runtime.
   The ordinary `reindex_step` already advances its shared action one step at
   a time; its final checked wrapper is not a second independent traversal.

2. **Pending structural inspection remains intertwined with elaboration.**
   `term_structure_step`, `declared_type_step`, `classifier_structure_step`
   and `type_structure_step` still follow source/rule recipes. Shared
   `source_rule`, `compound_structure_step` and `type_rule_structure_step`
   already remove parts of the older duplication. The four query roles ask
   different questions; deleting them merely because there are four is not a
   valid consolidation. Pending handlers require structure before all proofs
   are accepted (`tests/synthesis.c:pending_effect_contexts`). The remaining
   audit must identify an actual repeated construction or dispatch, preserve
   this dependency ordering, and keep `::` out of synthesis inputs.

3. **Inherited experimental edits have no production integration.**
   `pg_prove_context_rename` is called by the new `pg_data_schema_relocate`
   and tests; `pg_data_schema_relocate` itself is called only by tests.
   Production `schema_result_context` still uses checked context-alpha and
   telescope correspondence. The six tracked implementation/header/test
   edits are +127/-3 and are outside the published verification. Two untracked
   derived-LT `.p` files are also present; the committed `derived_lt.sh` instead
   builds variants from `sorted-proof-provider.p` and `derived-lt.patch`.
   Direct comparison shows the untracked pair predates moving shared Sorted
   predicates into the provider; it is not an exact duplicate of the current
   generated pair. Do not silently add that old nominal split to acceptance.
   These files must not silently become accepted code or regression coverage.
   The older retained-QuickSort failure alone does not justify adopting them:
   the later published regression passes. No inherited edits were removed.

4. **Functional ownership is clearer than physical organization.**
   At the initial baseline, `synthesis.c` is 10,160 lines and `evidence.c` 5,499 lines
   (5,512 with the inherited edit). Synthesis still contains lexical scopes,
   pending requests, IADT/motive/transport logic and structural inspection.
   Evidence still contains rule admission and the typed-query machinery.
   By contrast `typing.c` (1,075), `derivation.c` (240), graph construction
   (1,418) and witness construction (341) are separate modules. File length
   identifies review pressure, not duplicate semantics. Extracting files
   alone would not satisfy the code-reduction objective.

5. **The original completion/performance gates are still open.**
   Parent R0/R1 are checked; R2/R3/R4 are partial and R5 remains open.
   Old A1-A4 findings cannot be replayed as current failures: many named
   paths above have changed. Historical full acceptance and the targeted
   reruns below support the published milestones, not every inherited edit
   or the final cleanup. This review runs no new full suite or profiler.

For scale, `git diff --numstat 4657cc6 HEAD -- src/prototype/pointer`, grouped
by path at the initial baseline, gives implementation/headers **+10,379/-5,631, net +4,748**;
tests/fixtures **+15,781/-2,701, net +13,080**; Makefile **+101/-12**.
This excludes documentation and the working diff. It includes IADT/sorting
repairs and witness-removal work as well as refactoring, so it is not the
isolated cost of consolidation. The original net-negative gate is not met;
neither moving code nor cutting tests can be counted as semantic simplification.

### Assessment

Agent assessment: the interrupted refactor is **partly implemented and
published**, but its conclusion-sharing step is weaker than the user's desired
proof-as-typed-Term architecture. Make that ownership contract the first design
task. The synchronous-helper gap remains real, but is subordinate, not a reason
to expand the existing proof-reconstruction machinery.

Distinguish (a) an object proof `p : P`, including higher proofs, (b) the checks
that establish well-typedness, and (c) the history of those checks. Keep (a) and
perform (b); do not require permanent (c) merely because it exists today.
The target is a self-sufficient typed construction with ordinary checking and
shared in-progress work, not acceptance of arbitrary annotations. Different
proof Terms must survive; multiple checker derivations of the same typed Term
need not become different object proofs. Review old blanket premise-retention
tests against that distinction before changing their expectations.

Core stays erased; required motive, scope, index and Identity-boundary inputs
belong to typed construction or their existing semantic owner. Do not copy the
entire Evidence DAG into renamed occurrence fields. Conversion checks and
normalization receipts need an explicit recompute/retain policy; eliminating
typing history does not eliminate those checks or introduce equality reflection.
Keep App/Match/IH/Fold rules and nominal checks even if their permanent receipt
representation changes. No new acceptance bug is established by this review.

P4/P5 are the **single execution work list** for the old R2-R5 work. Semantic
regressions, checking, performance and honest LOC reporting remain obligations;
exact derivation-storage assertions are subject to the explicit review above.
Historical unchecked substeps are not independent tasks. #13's closed scope,
surface witness syntax and a complete higher-Identity theory are not added back.

Inherited-trial disposition, 2026-09-26: do not adopt Context/schema relocation
in this plan. No production use or current failing regression establishes its
need, and clean acceptance at `dcc58ec` passes without it. The older untracked
LT variants also remain excluded in favor of the script-generated current
variants. This rejects their inclusion in this milestone, not their logical
soundness; preserve all inherited files and edits locally for their author.

### Plan

- [x] **P4.0 Static rebaseline:** reconcile actual owners, published epochs,
  inherited changes and superseded diagnoses. No implementation changes here.
- [x] **P4.1 Clean starting point:** reproduce any claimed need for the inherited
  relocation trial against HEAD. Either adopt it with a real production caller
  and regression, or explicitly retire it with the user's work preserved.
  Compare the untracked LT variants before deciding whether they are redundant.
- [ ] **P4.2 Typed-proof contract (R2/R3):** for Lambda/Pi/APP, constructor/Match/
  IH and Identity, list each semantic input, its existing typed owner and each
  remaining rule/premise reader. Classify information as object proof structure,
  necessary checking input, reusable computation receipt, or optional history.
  Include ordinary/family context declaration formation and Universe bounds;
  the current raw Context payload alone does not retain those typed inputs.
  Specify how a typed construction can be checked without searching for its
  old derivation. Inventory image roots and tests that assume exact premise
  retention; preserve distinct object proofs, not necessarily checker histories.
- [x] **P4.2a Owner-local Identity metadata:** remove direction recovery from
  Evidence premises. The information already exists in the Term, so this small
  prerequisite can precede the full Lambda/APP migration without changing the
  inherited Context/IADT trial. Exact-shape inspection allocates nothing and
  establishes no typing; test both directions and malformed/noncanonical shapes.
- [x] **P4.2b Typed Identity boundary:** migrate the boundary view and all its
  consumers from premise arrays to typed inputs; check direct homogeneous,
  selected and family boundaries with no pre-existing parent receipt. Reuse
  already accepted subjects without rebuilding them. Reject wrong classifiers,
  paths and foreign typing ownership; no new Core tags or boundary database.
- [x] **P4.3a Lambda scope input:** replace the Pi-receipt rule test with its
  retained scoped input. Test alternative acceptance of the same Pi, identical
  typed Lambda output, no new scope/Core allocation, and wrong scope/type cases.
- [ ] **P4.3 One end-to-end slice:** start with typed Lambda/APP plus context
  action, then one Identity boundary consumer. Build, check, inspect, serialize
  and load the typed structure through the same Solve mechanism. Remove the
  superseded Evidence-based structural recovery in that slice. Prove by tests
  that missing old history does not remove the proof Term or its checkability,
  while wrong classifiers, scopes and nominal inputs still reject. Extend to
  Match/IH only after their motives and allocation inputs have explicit owners.
- [ ] **P4.4 Work reuse and images (R2/R4):** reuse the same typed construction
  and checking requests for accepted and pending consumers, keeping open-handler
  progress and `::` non-feedback. Measure synchronous lift/composition helpers
  including nested validation, then adapt callers to existing budgeted workers
  where needed. Check chunks 0/1/64, cancellation, repeated queries and zero/
  partial/complete retained/recompute images in fresh processes. Do not replace
  permanent history with repeated full-source checking or a second scheduler.
- [ ] **P4.5 Acceptance (R5/A5):** focused tests per slice, then full
  `check-acceptance` for each publication milestone and affected Debug/ASan/
  UBSan ownership/context coverage. Compare identical input/compiler flags
  against HEAD and, where compatible, the old R0 matrix (append, length,
  function-field, QuickSort); separate Solve from evaluation. Report per-file
  additions/deletions/net for implementation, tests, build and docs. Close
  R2-R5 individually against their criteria; if overall code growth remains,
  explain scope changes and seek explicit revision of the reduction target
  rather than silently checking it off. Publish verified epochs under the
  established Main-push policy.

## P5. Synthesis by Semantic Owner

### Subjective (User)

English paraphrase, 2026-09-25: independently of Evidence ownership, synthesis
has become too large. Separate it by feature/layer for locality and future
refactoring: dependent types, CBPV Value/Computation and effects belong above
the Lambda/Application/Reference Core, with their semantic-oracle owners.

Further explicit requirement, same date (English paraphrase): owner-local
Oracle modules are mandatory. Do not grow a global `TERM_LAMBDA`,
`TERM_APPLICATION`, `TERM_THUNK`, `TERM_FORCE`, etc. enumeration and a universal
union. A structure containing every domain's fields is unreadable to the user;
moving that structure into a header would not address the requirement.

Publication direction, 2026-09-26 (English paraphrase): inspect code and revise
the plan as needed; push coherent, verified milestones to Main rather than wait
for the whole refactor. A trial that proves poorly designed may be rejected:
preserve the reason in Markdown and publish without that implementation. This
does not authorize deleting another contributor's unaccepted work.

### Objective (Code)

The separation exists for parts of representation and evaluation, not yet for
most source synthesis. `classifier.c` builds Pi/F/U and rows;
`computation.c` implements pure computation dispatch; `iadt.c` owns data schemas;
`identity.c`/`action.c` implement Identity operations and formation;
`effect_inference.c` already owns its equation work. However, `synthesis.c`
still contains declaration/motive/constructor/index transport, body/sequence
adaptation, handler/effect collection, Identity discovery and shared scheduling.
Its private `pg_synthesis_job` and scope structures connect these responsibilities.
At the initial baseline, `synthesis.c:job_role` lists the domains centrally and
`pg_synthesis_job` contains a nested union of conversion, function-graph,
Identity, index-transport, substitution, block/application/Match, declaration
and handler state. This is the concrete unfinished part, even though Core
itself already has only three node kinds. `evidence.h:pg_evidence_rule` and
`derivation.h:pg_derivation_parameters` are additional centralized rule and
parameter representations to review with P4, not exempt from this requirement.

Implementation update, `dcc58ec`, 2026-09-26: effect inference, contribution, row contribution
and substitution now live in `synthesis_effect.c`, with private state and static
request descriptors. The existing index/queue still owns their work; export and
checking obtain the row worker through an owner accessor. Completion and
preparation notification now dispatch through descriptors as well. The remaining
source descriptors still share the legacy driver and wide job union. This is
an implemented extraction prerequisite, **not** the completed small-header design.
At that milestone, GDB reported `sizeof(struct pg_synthesis_job) == 336`
before and after extraction; migrated effect states had additional allocations.

Next slice, `84a54e2` after `bf2b4f7`, 2026-09-26: the shared header is
80 bytes on this build, containing request identity, queue/dependency links,
status and a borrowed checked result. State, header and immutable operands
share one aligned arena allocation; the state position follows from its static
owner size rather than a separately stored pointer. Effect requests no
longer allocate the source owner's union. The remaining source state is 264
bytes, still private in `synthesis.c`; this is not an overall-memory-reduction
claim. `synthesis_work.c` now owns the original interner, scheduling and
lifetime code, including its preparation notifications. Source-specific eager
Evidence admission and lazy definition activation live in the source start hook,
not branches in the shared allocator. No second queue or serialized work kind
was added. Checking rules, object proofs and image formats are unchanged.

Handler slice at `c89edb7`, 2026-09-26: `synthesis_handler.c`
owns Handler/return-clause/operation-clause states, their effect registration,
failure propagation, scope restoration and provisional rule inspection. The
three descriptors leave the central source driver/union. Exact scope/name/binder
interning stays shared through `synthesis_source.h`; that header exposes lexical
inputs, not private owner payloads. Nested handlers still share the existing row
worker until all contributions are registered. No per-handler solver is added.
The common projection is an ephemeral view of an owner's existing rule,
preparation state and known value/computation shape, not a cached classifier or
acceptance flag. Clean acceptance, source images and affected sanitizer tests
pass. Boundary review found a regression in the
extraction candidate: source name resolution read private source state from a
direct Handler producer. The new alias test failed with exit 139 at `880f2bf`;
`c89edb7` centralizes optional lexical-export access and passes that test both
before and after effect closure. Other owner-private reads were checked for
their role/factory preconditions. The superseded candidate's full test run was
stopped; only the fixed clean tree counts as the publication gate.

Dependency map checked against the current worktree, 2026-09-25:

| Existing path | Shared mechanism to retain | Owner-specific dependency to remove from the driver |
| --- | --- | --- |
| `request_inputs_with_jobs`, `enqueue`, `subscribe`, `wake` | Exact-input interning and one ready/dependency queue | Global role dispatch and allocation sized for every domain's union |
| `finish`, `pg_synthesis_destroy` | Completion notification and lifetime teardown | Handler failure propagation, row-worker destruction and domain scratch cleanup |
| Effect inference/contribution/substitution requests | Existing row solver and shared term substitution | Row phases, binding buffers and child-request state |
| Handler preparation and `pg_source_scope` | Lexical scope/binder identity and preparation dependencies | `effect_owner`, registration count and clause/carrier state |
| `pg_synthesis_export_rule_closure`, `derivation_step` | Shared dependency discovery and ordinary checked rule inputs | Direct `effects->inputs[0]` / `worker->inputs[0]` access to the row worker |
| Structural queries and source images | Accepted typed inputs and existing source allocation references | Domain recipe inspection must move with its construction, not remain a second driver switch |

### Assessment

User-mandated invariants:

- Core retains only Lambda/Application/Reference. A new upper-layer operation
  does not require extending the Core enum or payload union.
- Each semantic owner keeps its representation and synthesis/checking behavior
  local. Do not reproduce the all-domain union in a common synthesis, typing,
  Evidence or serialization structure under another name.
- Common graph/request infrastructure shares mechanics, not every owner's
  private state. Small owner-local phase distinctions are not additional Core
  node kinds and do not become a global inventory of language concepts.

Agent proposal: keep a small common request header and owner-local request
structures, using statically defined owner operations to advance/dispose them.
Reuse the existing request index and dependency queue, rather than introduce a
parallel framework. Typed checking remains over the existing shared graph;
modules must not gain separate proof databases, context substituters or
conversion engines. Type/effect dependencies remain in one pending graph, not
artificially ordered compiler passes. Owner descriptors must not introduce a
dependency from erased Core to source synthesis.

Proposed responsibility map; filenames are not yet an implementation decision:

| Responsibility | Synthesis work to place with that owner | Shared dependencies |
| --- | --- | --- |
| Driver / source naming | Request interning, ready queue, scope/name/import handling and entry dispatch | Existing typed store and immutable source associations |
| Dependent functions / families | Pi formation, Lambda/APP typing, binder and dependent codomain handling | Common context action and conversion |
| CBPV | Value/computation adaptation, Return/Thunk/Force, sequencing and block lowering | Function rules and shared typed constructors |
| Effects | Operation signatures, request/handler clauses and effect contributions | Existing `effect_inference.c`; no copied row solver |
| IADT | Declarations, inferred constructor indices, Match/motives/IH and indexed branch preparation | Common context action; explicit calls to Identity transport |
| Identity / higher action | Formation, boundary construction and transport synthesis | Existing `identity.c` and `action.c`, not a new equality engine |
| Function graphs | Graph request and source naming adapter | Existing shared graph plan; optional witness module remains separate |

These are code ownership groups, not new Core tags or extra public language
constructs. A semantic owner is not permission for an arbitrary host callback
to certify typing. Checked equations and rule validation remain the trust
boundary. CBPV and dependent function rules interact; define explicit calls
rather than duplicate APP in each module. Statically defined owner dispatch
does not require a dynamic plugin registry or an all-domain switch in the
driver. The current pure evaluator's delegation is not yet evidence that all
synthesis/codec dispatch has this shape.

Agent refinement from that map: begin with effect inference/contribution/
substitution requests, then their handler integration. Their existing public
request APIs already delimit a useful slice; their private state need not enter
a common header. The common allocator should key requests by a static owner
descriptor plus immutable inputs and allocate the owner's requested size.
Completion/destruction use the same descriptor. Preserve exactly one indexed
request per existing input tuple, not a wrapper job plus a second owner job.
Expose the row worker to existing export/checking consumers through an
owner-local accessor; do not preserve knowledge of its input offsets there.
Before extracting handlers, retain their open-equation registration and source
allocation contracts explicitly. These are real dependencies, not redundant
states to delete merely to shorten the driver.

The next extraction must preserve three inspected dependencies: `intern_scope`
inherits/interns the exact effect-owner pointer; `pg_synthesis_environment_input`
exports that owner's source boundary; `prepare_handler`/`handler_step` share a
registration count and seal only after all nested contributions are registered.
Keep that state private to the handler owner and expose borrowed scope/input
views. Do not copy the row solver into scopes or recreate a separate inference
request for each clause. Before migrating the handler, separate the generic job
header from legacy source state; moving the union to `synthesis_work.h` is not
an acceptable shortcut. Common completion publishes status and wakes consumers;
owner hooks retain scratch cleanup, allocation registration and failure handling.

Milestone decision, 2026-09-26: adopt the first extraction because effect state,
advancement and cleanup now have one local owner while retaining the same work
index and dependencies. Do not mistake that for the completed representation
refactor: the legacy allocation remains large and aggregate LOC increases.
Reject moving the universal union into a public header as a shortcut; it would
publish the same coupling instead of removing it.

Small-header decision, 2026-09-26: retain the original single interner and
dependency queue, but put them in `synthesis_work.c` and keep all legacy domain
fields private to `synthesis.c`. Reject the trial's stored private-state
pointer: its position is derivable from the immutable owner size. Source state
remains transitional; do not claim that extracting the scheduler completes
Handler/IADT ownership or Evidence migration.

Handler extraction decision, 2026-09-26: reject requiring accepted classifiers
at the module boundary; open carriers need provisional structure to close their
effect equations. Move that inspection with construction instead. Keep lexical
registration dependencies before Handler steps and one shared binder interner.
Remove redundant scope/syntax copies from Handler state, reading immutable
request operands instead. Do not split or duplicate `effect_inference.c`.
Do not add lexical-export fields to the common header just to preserve an old
source-state assumption: ordinary Handler producers have no lexical namespace.
The source owner alone projects its optional exports; normal typed membership
resolution remains unchanged.

Operation extraction, 2026-09-26 (agent decision): put signature/request
construction, provisional projection, reference resolution and allocation
restoration in `synthesis_operation.c`. Source naming exposes only an existing
lexical-alias edge. Reuse the common work index/queue and ordinary checking
rules. The fixed two-binder Operation allocation needs only its existing
Context pointer; remove the extra generic allocation array and its copied
prefix/count/cursor from this owner. Retained allocation supplies identities,
not trusted field types. Keep P4's general checker migration open.

### Plan

- [x] Confirm that owner-level semantics are partially separated while synthesis
  remains concentrated; distinguish this task from P4's representation change.
- [ ] Map private job fields, entry points and cross-owner calls; choose the
  smallest shared interface consistent with P4.2. Keep domain-specific state
  private. Moving the entire job union into a widely included header is not
  sufficient modularization. Replace the existing all-domain role/union design
  incrementally, not with a second permanent request representation.
- [x] Record the first effect-owner dependency map, including completion,
  destruction, source-scope and image/checking consumers. This does not complete
  the all-owner field inventory or change the job representation yet.
- [x] Extract the four effect requests and their private state; replace central
  effect-role dispatch and direct worker-input access. Reuse the original
  request index, dependency queue and substitution engine. Other source work
  remains explicitly transitional; no second scheduler or serialized role IDs.
- [x] Verify this first extraction with synthesis, source/derivation images,
  nested handlers, sanitizer cancellation and ordinary-result/generic Sorted
  regressions. Check exact request reuse before and after completion, including
  zero-equation substitution. Record mechanical movement separately from deletion.
- [x] Verify the small common-header slice: owner-sized aligned storage, one
  initialization on an interning miss, same prefix/operand keys and shared
  lifecycle, no source-union access from other owners. Run targeted checks,
  clean acceptance, affected sanitizers and a paired performance comparison.
  Source-owner field relocation is mechanical, not deleted typing behavior;
  generic lifecycle extraction must not introduce source rules into the driver.
- [ ] After the common contract is clear, extract one owner per reviewable epoch,
  initially handler/effect contribution code around the existing effect worker,
  then IADT/motive work. Place cross-cutting Identity transport deliberately;
  do not duplicate it. CBPV/function preparation must preserve pending cycles.
  Refine this order from the dependency map before code changes.
- [x] Verify the Handler-owner slice: distinguish structure preparation from
  acceptance, preserve independent/restored and shared nested effect boundaries,
  registration failure, clause binder identities, repeated requests and source
  images. Run clean acceptance, affected sanitizers and paired performance;
  record the surviving Operation/CBPV/IADT ownership work separately.
- [x] Verify the Operation-owner slice: pending term/classifier projections,
  aliases and nested references, single/bulk budgets, exact request reuse,
  restored binder identities and wrong signatures; then clean acceptance,
  affected sanitizers, source-image compatibility and paired performance.
- [ ] Move each owner's provisional structural inspection with its typed
  construction/checking interface; the central driver must not keep a second
  domain switch describing the same terms. Adapt to P4's reduced Evidence
  dependency rather than fossilizing the old premise-reader APIs.
- [ ] Route retained typed inputs and owner-specific image payloads through the
  same owners. Preserve checked loading and portable identity; do not move the
  universal rule/payload union into a codec. Reuse the shared relocation and
  source/Solve machinery instead of introducing an owner-specific Replay path.
- [ ] Keep move-only and representation-changing diffs identifiable. Use the
  affected owner tests plus source/image and pending-handler regressions at
  each epoch, then the common acceptance/performance gates. Report moved lines
  separately from genuinely deleted duplicate code; file splitting alone does
  not satisfy the net-reduction target.

## Progress

| Date | Problem | Material result | Evidence / next step |
| --- | --- | --- | --- |
| 2026-09-25 | P1/P3 | Agent verified the published source and closed #28/#32 and five superseded PRs under the user's request | Fresh targeted checks below |
| 2026-09-25 | P2 | User approves closure using explicit Equality/transport; agent records its limits and closes #13 | No automatic binding equations or mandatory positional lemma added |
| 2026-09-25 | P4/P5 | Static rebaseline and subsequent user clarifications recorded: typed proof ownership first; synthesis modularity is a separate task | Next: inherited-trial disposition and typed-input/consumer contract; implementation and final gates remain unchecked |
| 2026-09-25 | P4.2a | Identity field metadata moved to its semantic owner; Evidence-premise recovery deleted | Targeted optimized and sanitizer checks passed; full typed-proof and synthesis migrations remain open |
| 2026-09-25 | P4.2b/P4.3a | Typed Identity boundaries and Lambda scopes no longer depend on parent proof history; input inventory added | Targeted/sanitizer tests and full acceptance pass. Generic checking, image-history removal and P5 remain open |
| 2026-09-26 | P5 | Four effect requests moved to owner-local state and behavior using the same queue; shared completion uses owner hooks | Targeted, image, Handler, QuickSort and sanitizer checks pass; clean publication candidate excludes inherited trials |
| 2026-09-26 | P4/P5 publication | Clean `dcc58ec` passes full acceptance and affected sanitizer checks; inherited trials not adopted | Publish the two implementation commits plus this record; broad typed-proof and common-state migrations remain open |
| 2026-09-26 | P5 shared work | `84a54e2` separates the 80-byte common header from private owner state and moves the original scheduler | Clean full acceptance, affected sanitizers, image cross-reading and paired comparison complete; remaining owner extraction/P4 stay open |
| 2026-09-26 | P5 Handler owner | `c89edb7` localizes Handler state and pending structure; a direct-alias regression found during extraction is fixed and tested | Clean acceptance, sanitizers, cross-version images and paired timing complete; Operation/CBPV/IADT and broader P4 remain open |

P4.2a verification (fresh, current worktree including the inherited Context/IADT
edits): optimized `identity_test`, `derivation_io.sh` and `identity_io.sh` pass;
ASan/UBSan `identity_test` and `derivation_io.sh` pass. The publication binary and
the new `derivation_io_test write` produce byte-identical images (`cmp` passes).
Builds: `/tmp/a-program-owner-field-view{,-asan}`. No full acceptance rerun or
publication in this slice; passing these checks does not adopt the inherited trial.

Implementation changes through P4.2b/P4.3a, `40375d7..60bde88`; inherited
Context/IADT edits excluded:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `action.c` | 119 | 57 | +62 |
| `action.h` | 29 | 2 | +27 |
| `derivation.c` | 11 | 23 | -12 |
| `evidence.c` | 4 | 26 | -22 |
| `evidence.h` | 0 | 16 | -16 |
| `identity.c` | 18 | 0 | +18 |
| `identity.h` | 5 | 0 | +5 |
| `synthesis.c` | 15 | 14 | +1 |
| `tests/core.c` | 14 | 0 | +14 |
| `tests/identity.c` | 162 | 24 | +138 |
| `tests/derivation_io.c` | 1 | 0 | +1 |

Implementation/headers: +201/-138, net +63; tests: +177/-24, net +153. This
removes history-dependent inspection but adds direct typed-boundary checking;
it does not meet the overall reduction gate.
Documentation changes are separate from these code counts.

P4.2b/P4.3a fresh checks: optimized Core/Identity tests and ASan/UBSan Core,
Identity, synthesis and derivation-IO tests pass. QuickSort ordinary-result and
generic Sorted/image tests pass, as do Identity/occurrence image checks. The
derivation writer's image remains byte-identical to the publication binary's.
Single-run generic Sorted comparison with the same script: publication 5.422 s,
candidate 5.462 s; matching reported Solve counts. This is a regression smoke
comparison, not a statistically established speedup. Builds are in
`/tmp/a-program-typed-boundary{,-debug,-asan}`. Full `check-acceptance` exited
zero: real 21m44.109s, user 19m56.641s, system 1m47.075s. This includes examples
01-09, ordinary-result QuickSort proofs, the four LT/partition-order variants,
source/retained images, witness-generator isolation and optional packet tests.
Log: `/tmp/a-program-typed-boundary-acceptance.log`. The tested worktree includes
the inherited Context/IADT trial; this verifies the current tree, not adoption
of that trial or a clean publication tree. No commit/push had occurred at that
checkpoint; the clean publication result below supersedes that local-only gate.

P5 first-slice verification, 2026-09-26: optimized synthesis, source/derivation
images, examples 01-09 present in the suite (eight files), nested Handler/save
boundaries, ordinary-result QuickSort and generic Sorted checks pass. ASan/UBSan
synthesis and source-image checks pass, including cancellation. Additional tests
check owner access, zero-equation substitution and request/step reuse after
completion. Builds: `/tmp/a-program-effect-owner{,-asan}`; these targeted builds
still include inherited trial edits. Cross-reading with the pre-extraction
build passes for ordinary, pending-effect and producer images. Effect/producer
byte ordering varies on repeated writes even with the baseline binary; this
slice does not claim byte-canonical images or change their wire format.

Three paired generic-Sorted script runs, same flags and alternating build order:
pre-extraction median 5.770 s (5.765-5.787), candidate median 5.816 s
(5.710-5.879). This small sample does not establish a speed change. Logs:
`/tmp/a-program-effect-owner-bench.E7dYzS/`. Relative to the preceding P4 slice:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 701 | 812 | -111 |
| `synthesis_effect.c` | 199 | 0 | +199 |
| `synthesis_effect.h` | 10 | 0 | +10 |
| `synthesis_work.h` | 32 | 0 | +32 |
| `tests/synthesis.c` | 20 | 0 | +20 |
| `Makefile` | 18 | 17 | +1 |

Implementation/headers: +942/-812, net +130. Much of the diff is code movement
and exported scheduler-call renaming; the driver shrinking by 111 lines is not
111 lines of eliminated duplicate behavior. The total reduction gate remains
unmet. Tests and build changes above are separate from implementation and docs.

Combined `40375d7..dcc58ec`: implementation/headers +1,143/-950 (net +193),
tests +197/-24 (net +173), build +18/-17 (net +1). The per-file tables above
separate the two implementation milestones. Documentation is counted separately
in its publication commit, not included as implementation growth.

P1/P3 checks, 2026-09-25: `make -f src/prototype/pointer/Makefile BUILD=<publication-build>
check-quick-result check-witness-isolation check-sort-insertion` exited zero
(129.759 seconds total). `tests/merge_composition.sh` with the same checker and
program driver exited zero (2.462 seconds). Both graph-name fixtures passed at
chunks 1/64. Logs: `/tmp/a-program-soap-issue-review-{sorting,merge}.log`.
These were targeted reruns against the pre-refactor publication build, not
verification of the later P4/P5 changes.

Clean publication verification, 2026-09-26, detached tree at `dcc58ec`:
`make -s -f src/prototype/pointer/Makefile
BUILD=/tmp/a-program-soap-publication-build check-acceptance` exited zero.
Wall 25m44.541s, user 23m47.364s, system 1m56.294s. This includes the clean
build and ran alongside sanitizer compilation; it is not a comparative runtime
benchmark. Log: `/tmp/a-program-soap-publication-acceptance.log`.
The same clean tree passes ASan/UBSan `core_test`, `identity_test`,
`synthesis_test` and `tests/source_io.sh`, using
`/tmp/a-program-soap-publication-asan` with strict warnings, `-O1 -g`,
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
These results exclude all inherited trial code and supersede the dirty-tree
publication caveat above. No parser/Core/wire semantics were intentionally
changed. P4/P5 overall and the net-code-reduction gate remain open.

P5 common-work slice, `bf2b4f7..84a54e2`, 2026-09-26:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 1435 | 1476 | -41 |
| `synthesis_work.c` | 188 | 0 | +188 |
| `synthesis_work.h` | 27 | 2 | +25 |
| `tests/synthesis.c` | 73 | 0 | +73 |
| `Makefile` | 1 | 1 | 0 |

Implementation/headers: +1650/-1478, net +172; tests: +73/-0; build: +1/-1.
The large diff mostly relocates field access into private source state and moves
the original scheduler, rather than adding or removing typing rules. This is an
ownership prerequisite, not a code-reduction milestone. Documentation is separate.
Permanent tests cover aligned/zeroed owner storage, exact key reuse, one start
hook per request, completion/pending destruction and foreign-owner access.

Clean `84a54e2` passes ASan/UBSan synthesis and source-image tests, with the same
strict sanitizer flags as the preceding milestone. Ordinary, pending-effect and
producer images cross-read in both directions with the preceding clean build
(six writer/reader pairs). These checks preserve the existing ordinary Solve
path; they do not establish byte-canonical output. Logs:
`/tmp/a-program-owner-work-asan-{synthesis,source-io}.log`.
Clean publication-tree `check-acceptance` exited zero: wall 25m56.804s,
user 24m0.611s, system 1m55.351s, including build. This covers ordinary-result
QuickSort, all four LT/partition variants, resumed images, wrong-proof rejection
and optional-witness isolation/packets. Log:
`/tmp/a-program-owner-work-publication-acceptance.log`. This is a publication
gate, not a timing comparison with earlier whole-suite runs.

Three alternating paired `tests/generic_sorted.sh` runs after acceptance, using
the same strict-O2 builds: preceding code median 5.738s (5.657-5.754), candidate
median 5.786s (5.762-5.850), about +0.8%. Reported comparison Solve counts match
in all six runs. This small sample does not establish a speedup or rule out
small regressions. Logs: `/tmp/a-program-owner-work-bench.ymrkIh/`.
Publish this verified slice; inherited trial edits remain excluded. The next
implementation still requires owner-by-owner extraction and P4 typed-proof
work; neither the complete Oracle split nor overall code reduction is achieved.

### Handler Owner Verification

Candidate `c89edb7`, excluding inherited trials. Clean-tree ASan/UBSan synthesis
and source-image tests pass with the preceding milestone's flags, including
pending-work cancellation. Handler nesting and all four save-boundary cases
also pass: 1,365/1,515/705/898 snapshots preserve acceptance/rejection, effect
rows and binder identity. Ordinary, pending-effect and producer images cross-read
with `84a54e2` in both directions, at single and bulk budgets. Logs:
`/tmp/a-program-handler-owner-asan-*.log` and
`/tmp/a-program-handler-cross.7iFzVi/`. Clean `check-acceptance` exited zero:
wall 25m24.720s, user 23m30.471s, system 1m53.402s, including rebuilds and early
sanitizer compilation overlap. This is a verification gate, not a comparative
runtime measurement. It includes all four LT/partition variants, ordinary-result
Sorted/permutation proofs, resumed images, negative cases and optional-witness
isolation/packets. Log:
`/tmp/a-program-handler-owner-publication-fixed-acceptance.log`.

After the full gate, three alternating paired `generic_sorted.sh` runs against
`84a54e2` give baseline median 5.840s (5.816-5.889), candidate median 5.787s
(5.757-5.793), about -0.9%. Test output and reported comparison Solve counts
match after replacing temporary path names. The small sample does not establish
a speedup or exclude small regressions. Logs:
`/tmp/a-program-handler-owner-bench.X7dvJs/`.
Adopt and publish this verified slice; no extra work engine, classifier authority,
Core tag or wire format was introduced. The rejected shortcuts and the remaining
P4/P5 work above stay explicit.

Source delta `2d7ebe8..c89edb7` (documentation excluded):

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 312 | 874 | -562 |
| `synthesis_handler.c` | 626 | 0 | +626 |
| `synthesis_handler.h` | 14 | 0 | +14 |
| `synthesis_source.h` | 39 | 0 | +39 |
| `synthesis_work.c` | 23 | 1 | +22 |
| `synthesis_work.h` | 11 | 2 | +9 |
| `tests/synthesis.c` | 25 | 0 | +25 |
| `Makefile` | 1 | 1 | 0 |

Total +1051/-878, net +173: implementation/headers +148, tests +25, build zero.
This moves ownership out of the driver, not a net code-reduction milestone.
The three Handler requests no longer allocate the legacy source union or copy
immutable lexical inputs. Debug-symbol sizes on this x86-64 build: common header
80 bytes unchanged; source-private state 264 to 248; return/clause/Handler
private states 32/40/32 bytes instead of 264 each. These sizes exclude immutable
operands and separately allocated state, so they are not a peak-memory claim.
Operation preparation, CBPV adaptation, IADT and
general typed-proof authority still require their own inspected migrations.

### Operation Owner Verification

Clean candidate `3397e52`, 2026-09-26, excluding inherited trials:
`check-acceptance` exits zero (wall 25m55.984s, user 24m0.880s, system 1m54.263s,
including builds). General ordinary-result QuickSort, all four LT/partition
variants, negative/image cases and optional-witness isolation/packets pass.
ASan/UBSan synthesis, source-image, Handler nesting and all four save-boundary
tests pass with the preceding flags. Snapshot counts remain
1,365/1,515/705/898. Source, pending-effect, producer and Operation images
cross-read in both directions with `c89edb7` at budgets 1/64. Logs:
`/tmp/a-program-operation-owner-{acceptance,asan-*}.log` and
`/tmp/a-program-operation-cross.ELy19S/`.

After acceptance, three alternating paired `generic_sorted.sh` runs against
`c89edb7` give baseline median 5.784s (5.706-5.849), candidate median 5.785s
(5.783-5.798). Normalized output and reported Solve counts match. This sample
does not establish a speed change. Logs: `/tmp/a-program-operation-bench.NalE2B/`.
Adopt this slice; P4 and the remaining P5 migrations stay open.

Source delta `6f2b466..3397e52`; documentation is separate:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 4 | 155 | -151 |
| `synthesis_operation.c` | 190 | 0 | +190 |
| `synthesis_source.h` | 2 | 0 | +2 |
| `tests/synthesis.c` | 35 | 0 | +35 |
| `Makefile` | 1 | 1 | 0 |

Total +232/-156, net +76: implementation/headers +41, tests +35, build zero.
Most code moved; the Operation's redundant generic allocation array was removed.
This is not overall code reduction. Debug-symbol sizes on x86-64: shared header
80 bytes unchanged; source state 248 to 240; Operation/reference private state
24/16 instead of 248 each, excluding operands and other allocations. No new
Core tag, checker authority, scheduler or wire format was introduced.
