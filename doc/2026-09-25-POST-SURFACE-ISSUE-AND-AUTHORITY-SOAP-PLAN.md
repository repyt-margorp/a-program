# Post-Surface Issues and Authority Refactoring

Date: 2026-09-25
Updated: 2026-09-26
Status: P1-P3 closed. P4 Identity-boundary/Lambda-scope/typed-only function,
result-formation and typed-map/variable-image slices verified. P5 effect/shared-work/
Handler/Operation/CBPV/function/Context/IADT-scope/basic-Identity owners verified.
Verification uses clean publication trees, full acceptance and affected sanitizers.
Broader P4 typed-proof migration and P5 synthesis modularity remain open.
Layout milestone (user-authorized 2026-09-26): see
[the promotion plan](2026-09-26-POINTER-SOURCE-PROMOTION-SOAP-PLAN.md).
After promotion, old `src/prototype/pointer/*.c` paths resolve to `src/*.c`,
its `tests/` suffix resolves to top-level `tests/`, and the remaining legacy
`src/prototype/` tree resolves under `archive/legacy/`. Historical measurements
below keep their original revision paths. This is not completion of P4/P5;
resume their semantic work separately after the unchanged-source layout gate.
Initial review baseline: `40375d734896a456f5ad2827ad8fe0f10ff61517`.
Published epochs and their verification are recorded below. The latest
Identity-owner comparison baseline is `4aecd9c`; its clean gate covers all
previous slices without the inherited trials.
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
| P4 | Make typed proof structure authoritative; remove redundant evidence dependency | Existing R2-R5 plans | Typed-only functions and result-formation history removal verified; broader migration pending |
| P5 | Separate synthesis by semantic owner without duplicating shared machinery | User follow-up on synthesis.c | Effect-owner/shared-header/Handler/Operation/CBPV-adapter/function-formation slices verified; legacy source state remains |

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

Publication instruction, 2026-09-26 (English paraphrase): push coherent verified
milestones to main. Reject unsuitable implementation trials, record why in
Markdown, and publish that decision without shipping the rejected code.

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
| Pi / Lambda | Pi domain/codomain operands, scoped Context and Core binder; Lambda type/body/annotation | Ordinary binders can now establish their scope from the retained domain during structural checking; family declaration inputs remain incomplete |
| APP | Function/argument operands and instantiated result `type` | One ordinary APP rule constructs or checks the retained result; substitution is shared and only bound-pointer renaming is accepted. `formed_classifier` still returns Evidence |
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

Context-contract review at `44587b2`, 2026-09-26 (agent findings):

- `tests/core.c:structural_scope_admission_test` now checks that the same
  described variable is unavailable before Context admission and accepted
  afterwards; a wrong classifier remains rejected. These are different reasons
  for a NULL structural-check result. Existing typed queries instead retain a
  terminal status. Caching that NULL as permanent failure would be a regression,
  not a current production bug demonstrated by this review.
- The dependent-application test now constructs two accepted extensions with
  the **same raw Context** but different retained declaration formations and
  Universe upper bounds. Pi formation preserves the supplied domain and its
  bound. Thus adding a single mutable formation pointer to `pg_context`, taking
  the first accepted receipt, or including a new formation pointer in its
  current interning key would not preserve this contract automatically.
- Family-domain Pi retains a scoped variable, not its telescope formations.
  A closed ordinary-Pi example alone cannot establish that the generic checker
  has enough input. `context_payload.c` transports raw declarations only.

Assessment: reject the proposed direct expansion of structural checking into
permanent subject-keyed typed queries **before** specifying these inputs. No
such implementation was installed. Also reject a parallel mutable Context
formation cache and proof-count-based retry invalidation. These would hide the
missing dependency instead of making the typed construction self-sufficient.
Keep raw scope identity separate from the particular typed declaration used
in a formation, just as erased Core and typed occurrences remain separate.
This does not require another object-language tag, a new proof axiom, or a
new source-replay path. The replacement representation is still a design task,
not a user-approved schema or a completed migration.

At `f8c1224`, a narrower variable-only repair was also reviewed statically:
`typing.c:context_variable`, `pg_occurrence_weaken` and `pg_occurrence_unproject`
construct/cancel variable images from raw declarations. Adding a selected
formation only in `pg_prove_variable` would therefore make checked variables
disagree with their map images, or lose that selection during weakening.
Do not install this partial change or compensate with a fallback proof search.
Declaration uses, variable images, scope action and image transport must migrate
together. No Context/variable representation change is included in the
Context-action synthesis-owner extraction below.

Follow-up at `382a0a6`, 2026-09-26 (agent findings): distinguish regularity
from preservation of a selected formation. `pg_classifier_request` establishes
that an already synthesized classifier is a type; its contract does not select
a particular Universe upper bound. In contrast, `pg_prove_pi` must preserve
the supplied declaration's bound. Thus a first-receipt lookup alone is not
evidence of an incorrect classifier. Do not add formation metadata to every
variable or split raw Context identity merely to remove all such lookups.

There is, however, an unnecessary reconstruction in `variable_frame`: a map
already retains a typed image, but a binder-shaped image is discarded and its
variable reintroduced through the destination's first Context receipt. The new
`tests/iadt.c:retained_variable_image` constructs narrow/wide formations of
the same raw Context, retains only the wide variable introduction, and follows
it through a mapped F-type. On the baseline, nominal inspection introduces an
additional narrow-variable receipt before reporting that the open type has no
known IADT declaration. This is a reproduced duplicate checking path, **not**
a demonstrated acceptance of an ill-typed term.

Agent implementation decision: consume the exact map image through the existing structural
checker for every image shape. Keep the separate restriction case: unlike a
map, it has no stored image. No Context schema, new cache, object proof rule or
wire change is needed. The focused IADT suite and full publication gates pass
(recorded below). This does not settle the missing family
declaration input or complete P4.2/P4.3.

Verification of the earlier review: clean `44587b2` plus its two test additions passes
the full Core executable under `-O2 -Wall -Wextra -Werror` and ASan/UBSan
(`-O1`, non-PIE). Implementation/build/wire changes: **0 lines**; tests:
**+51/-0**. The full acceptance gate was not rerun for this test/document-only
checkpoint; the preceding Operation publication gate remains the compiler
baseline, not a fresh result for this review. P4.2/P4.3 remain unchecked.

Typed-only checking trial `f2b112b..270c271`, 2026-09-26 (agent decision): permit
the ordinary-binder slice without a new Context representation. The existing
structural dependency walk checks the retained Pi domain before entering its
codomain scope. Function and CBPV checking are owner-local; all acceptance uses
the ordinary rules. Empty scope has its ordinary introduction. Other open
scopes still need admitted declarations, and family signatures are not guessed.
This supersedes neither the missing-input finding nor the rejection of
permanent negative caching: no new query cache/retry generation was introduced.

The fresh-process test exposed a real reconstruction limitation: substituting
into a Pi result can allocate bound pointers different from those saved in the
typed graph. Reconstruct-and-compare-by-pointer rejected the valid dependent
APP. The shared APP/codomain rules now validate the supplied result allocation:
the computed classifier must be alpha-equivalent, and the selected origin,
argument, scope, Universe bound and remaining occurrence fields must match
exactly. Free/nominal references are not renamed; no Core nodes are interned
by alpha equality. APP/codomain receipt keys now include the typed conclusion
instead of discarding it as a premise-determined output. This is checking, not
expected-type feedback into synthesis.

This slice does **not** yet delete the newly generated checking-history records,
replace the default derivation image, or make synchronous structural/map checks
fuel-bounded. Those remain P4.3/P4.4 work, not completed by a passing typed-only
image test. Family declarations, general normalization boundaries and IADT
nominal inputs still require their explicit typed contracts.

Image boundary: `occurrence_io.c` already transports typed operands, maps and
scopes without accepting proofs; it does not yet replace `derivation_io.c`'s
premise DAG. `retained_io.c` writes rule inputs, effect equations, optional
reduction receipts and term roots. In `tests/derivation_io.c`, the constructor
roundtrip explicitly requires two receipts with the same typed subject and
context map to remain distinct. That is a legacy history-retention contract,
not two distinct object proof Terms. Review that contract in P4.3; retain the
test's nominal declaration, IH allocation, zero-fuel and fresh-check assertions.

At `d09120d`, `pg_prove_data_result_formation` additionally clones an accepted
Constructor/Match/IH receipt solely to replace its result-formation receipt
with another having the **same exact typed subject**. Both constructor and
eliminator already retain that subject in `pg_occurrence.type`. This narrower
duplication is independent of the map/declaration contract above.

At `3266fa8`, `pg_context_map.images` already owns every typed image, including
weakened prefix images. `substitution_image_range` nevertheless traverses a
second prefix graph of Evidence and rebuilds projection receipts. The bulk
accessor additionally returns raw premise arrays for flat maps. Tests currently
require alternative receipts of identical typed images to remain distinguishable
through these accessors; they also check genuinely distinct conversion origins,
scopes and classifiers, which must remain distinguishable.

Therefore R1 consolidated conclusions, but did **not** remove dependence on
the derivation graph. Renaming Evidence, splitting its file, or adding another
subject accessor would not finish this part of the refactor.

Function-graph baseline regression, 2026-09-26 (`2f16bec`, with the same C code
at `7f25362`): accepting the exact same Pi through `THUNK_CONTENT` and
introducing a Lambda with that receipt makes graph
preparation report unsupported. The direct Pi case succeeds. Nested premise
indexing assumes `LAMBDA_INTRO -> PI_FORM -> CONTEXT_EXTEND`; a valid alternative
Pi receipt violates that assumption. The new regression introduces no earlier
Lambda whose ordinary receipt could mask the failure.

Agent decision: graph consumers read the retained Lambda body/Pi inputs through
the existing typed queries. Specialization retains its checked context action
instead of constructing another Lambda. Independent Pi/Lambda substitutions can
freshen different bound pointers; an ordinary one-binder substitution connects
the codomain to the body scope. The indexed Context accessor supplies admission
only, never the selected Universe bound. Domain inversion keeps its checks but
reuses an already checked exact child rather than adding a `PI_DOMAIN` receipt.
Eight direct/specialized, ordinary/alternative-Pi and narrow/wide formation cases
pass. Parameter traversal retains its existing function cursor across typed-input
yields rather than revisiting the prefix. The publication gates below pass.
Missing family-declaration inputs, other derivation consumers and synchronous
inversion costs remain P4 work.

#### Remaining Work, With Current Evidence

1. **Incremental callers still drain synchronous helpers.**
   Before the IADT-owner slice, `synthesis_context.c:lift_step` called
   `pg_prove_substitution_lift`, draining structural lifting in one quantum.
   The slice now advances the existing lift worker once per quantum;
   final declaration admission still invokes synchronous checking. Composition
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

Agent decision for P4.3c, 2026-09-26: delete result-formation receipt cloning.
Validate the supplied formation against the resulting typed Term's `type`
edge, after the ordinary rule has checked its semantic inputs. This preserves
the exact Context, nominal identity, classifier/Universe and typed structure,
not merely erased Core equality. No change to DefEq or object Identity is
involved. Reject a blanket "same conclusion means interchangeable premise"
policy: Context declaration choices, maps and directional/action rules have
separate retained-input contracts. Keep those unchanged in this slice.

Agent decision for the following map-image migration, 2026-09-26: image lookup
must select the typed map entry and use the existing structural checker, not
select a particular acceptance history. Change the legacy receipt-selection
tests, not their typed-image/Context/nominal checks. This does not identify
different typed occurrences merely because their erased Core agrees, and does
not replace the explicit Context declarations needed to admit a map. Reject
adding another image cache or retaining two lookup algorithms for flat versus
extended maps; verify the allocation/performance consequences before adoption.

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
  Before replacing Context admission/history in P4.3, specify the retained
  declaration edge for ordinary binders and the typed telescope/terminal
  Universe for family binders;
  preserve the tested raw Context identity and distinguish its formation uses.
  Trace that edge through variable lookup, Pi bounds, lift and image loading.
  A checking request must receive those dependencies explicitly, so unavailable
  input is not cached as a false theorem and no global retry counter is needed.
  The narrower P4.3b slice uses Pi's existing ordinary-domain input; it does not
  infer missing declarations for arbitrary open Contexts or family binders.
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
- [x] **P4.3b Typed-only ordinary-function slice:** check fresh-process images
  containing dependent Lambda/APP, F/U, context action and an Identity boundary
  without old derivations. Preserve two typed Lambdas over the same Core and
  caller-retained alpha-renamed Pi results; reject changed free variables,
  classifiers, scope, selected origin and Universe bound. The publication
  gates pass. This is a prerequisite, not completion of
  the history-removal or shared budgeted-work requirements below.
- [x] **P4.3c Result-formation history:** remove receipt cloning done solely
  to retain an alternative acceptance of the exact same Constructor/Match/IH
  result type. Check the supplied formation's complete typed occurrence;
  preserve nominal identity, Context, Universe bounds and other rule inputs.
  Verify allocation reuse, fresh-process images, old/new cross-reading and
  existing direct/Solve rejection tests before publication. This does not
  replace family-declaration inputs or all derivation history.
- [x] **P4.3d Typed map images:** remove prefix-receipt traversal from single
  and bulk image lookup. The map's exact typed entries are authoritative;
  reuse/check those entries through the existing structural checker. Test
  flat/extended/projected maps, equal-image positions, distinct conversion
  origins, selected Context formations, long prefixes/lifts and no allocation
  on repeated access. Recheck source/retained images, general Sorted and the
  common sanitizer/performance gates. Context admission and full map-history
  removal remain explicit later work, not implicit in this accessor change.
- [x] **P4.3e Variable-image inspection:** remove the binder-only reconstruction
  in nominal lookup; use the exact typed map image as for other images. Keep
  the reproduced alternate-Context regression and existing nominal/dependent
  image tests; complete acceptance, affected sanitizers and paired timings
  before publication. Do not claim a new typing theorem or Context migration.
- [x] **P4.3f CBPV content inspection:** replace RETURN/THUNK receipt shortcuts
  with checked typed-child access; reuse an accepted exact child instead of
  adding an inverse receipt. Preserve converted boundaries, effects, scopes
  and object proof Terms. Verify fresh typed-only images, legacy derivation
  images, full acceptance and paired performance before publication.
- [x] **P4.3g Function-graph typed inputs:** remove nested Pi/Lambda premise
  recovery and specialization's duplicate Lambda construction. Preserve exact
  selected domain bounds, scope correspondence and shared mapped inputs; retain
  regressions for alternate Pi admission, captured binders and allocation reuse.
  Verify full acceptance, image cross-reading, sanitizers and paired timings;
  revise storage-specific test assertions without weakening their typing checks.
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
  Recheck the small timing delta below, including APP/codomain reuse, without
  restoring result-blind interning or adding a parallel acceptance cache.
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

CBPV adapter extraction, baseline `aa151fe`, 2026-09-26 (agent decision):
move BODY/SEQUENCE and Return/Thunk-content requests into `synthesis_cbpv.c`.
These are source/checking adapters, not new Core nodes or another solver.

| Request | Retained inputs | Private progress, replacing 240-byte source state |
| --- | --- | --- |
| Body | Producer, optional checked Context | Selected ordinary rule: 8 bytes |
| Sequence | Context, input, continuation | Normalized input, Fold, selected rule, comparison and stage: 40 bytes |
| Return/Thunk content | Checked Context and typed input | Current normalization dependency and stage: 16 bytes |

The existing descriptor projection publishes provisional rules, not acceptance.
Constructor calling-convention discovery borrows Body's existing input edge.
Use the same work interner, queue, subscriptions, comparison engine and ordinary
checking rules. Comparison cleanup remains on completion and cancellation.
Expose existing source-preparation/typed-input helpers through the private
source interface; do not copy their logic or place CBPV state in the driver.
Remove the four source descriptors and their central dispatch/projection cases.
Function/IADT/Identity preparation and rule-shape inspection remain source-owned
and unfinished. This extraction does not eliminate Evidence history or claim
net code reduction. Source compatibility fallbacks are moved unchanged, not
newly justified as a general dependent sequencing rule.

The parallel P4 review did not establish a sufficient family-declaration
contract. Reject adding one formation pointer to raw Context, or disguising a
Context certificate as a family variable: the former loses selectable Universe
bounds; the latter has unresolved scope/image obligations. No such trial is
included in this slice; P4.2's existing retained-input requirement still applies.

Function formation slice, baseline `64cbc89`, candidate `22661f4`,
2026-09-26 (agent decision): `synthesis_function.c` owns ordinary APP/Lambda
construction, constant-result telescope opening, classifier-formation requests
and Pi-scope allocation/projection. Formation has **zero private progress**:
`pg_classifier_request` already interns and advances the typed query. Pi-scope
state is just its binder and ordinary extension request (16 bytes here), not
the 240-byte source union. Its binder is allocated once by the interning-miss
hook, before domain acceptance. The shared 80-byte header is unchanged.

The pending formation-shape projection moves with that owner. CBPV result
Context construction and its inverse input-edge inspection move together into
`synthesis_cbpv.c`; constructor calling conventions borrow this edge without
reconstructing proofs. The two private roles disappear from the source driver.
These changes do not complete ordinary Pi/APP rule-shape extraction, classifier
normalization, IADT/motive ownership or P4's Evidence-history migration.

Rejected shortcut: unconditionally aliasing `type_structure(formation(t))`
to `classifier_structure(t)` would discard the accepted-formation boundary.
The existing query first reads an accepted typed subject; its fallback can
instead expose an immutable symbolic recipe with unresolved effect parameters
and pre-substitution binders. Tests already distinguish these states. Preserve
this order and exact retained subject, rather than remove the wrapper on the
assumption that both snapshots always have the same pointer. No implementation
of that shortcut was adopted. A new classifier cache or generic structure-kind
dispatch was also unnecessary: use the existing queue and narrow owner queries.

Context-action slice, baseline `f8c1224`, 2026-09-26 (agent decision, verified
below): `synthesis_context.c` owns reindex, checked map pairing and lift.
The first two keep one borrowed worker/request pointer; lift has no private
state. They no longer allocate the source-wide union or enter its role dispatch.
Pairing uses the existing `pg_synthesis_expect` request; its inputs are already
restricted to values, so this does not introduce effect weakening into map
checking. Accepted map endpoints are read from the typed map, not premise
positions. Declaration formation still explicitly comes from the supplied
extension; it is not guessed from a raw Context or a first matching receipt.
Keep the same interner, queue, ordinary evidence rules and typed action worker.
Lift and family pairing remain synchronous at their existing rule boundary;
this is owner localization/post-check reuse, **not** completion of P4.4.

IADT-scope slice, baseline `1e47484`, 2026-09-26 (agent implementation
decision, verified below): move constructor scope, IH scope and
index-result elaboration together with their saved-allocation readers.
Constructor/IH scopes each need 40 bytes here, not the 240-byte source state
plus an overloaded substitution payload. Constructor fields retain only their
declarations; IH progress explicitly holds a Context. Allocation attachment and
lexical binder interning remain single source-owned helpers. Do not expose the
old all-domain union to the new owner or infer nominal/motive inputs from Core.

Index-result elaboration now calls the existing batch-substitution request.
Its private state is one pointer; generic substitution owns 40 bytes and an
array of declaration inputs, reading image producers from immutable request
operands instead of copying them. The independent caller test finds that
same completed request and exact result. No new acceptance cache or rule is
introduced. Structural lift uses the existing worker one step at a time;
synchronous final admission and direct kernel/Identity callers remain P4.4
work. Neither that granularity change nor this extraction removes required
declaration formation history or completes IADT/motive ownership.

Final review caught a dropped source-scope registration dependency in this
extraction. Restore the original wait/failure propagation before index-result
assembly, even for zero indices, and reject an unavailable checked Context.
The regression uses ordinary definition registration, including a duplicate
name; removing the wait makes it fail. Do not treat an empty result map as
permission to bypass its source environment. This fixes the unpublished trial,
not an established defect in `1e47484`.

Identity-owner slice, baseline `4aecd9c`, 2026-09-26 (agent implementation
decision, verified below): move formation, face/endpoint selection,
instantiation, reflexivity and family action/transport requests together into
`synthesis_identity.c`. Each descriptor allocates only
its private state. Formation/face cancellation stays with the existing action
workers. Family paths are values, so their conversion can use the ordinary
shared post-check without computation-effect widening. Preserve the selected
paths, source/destination Contexts and exact request inputs; no new Core tag,
solver, proof cache or wire format. IADT index transport and source Identity
discovery remain explicit callers, not duplicated implementations. This is
owner localization, not completion of typed-history removal or higher Identity.

### Plan

- [x] Confirm that owner-level semantics are partially separated while synthesis
  remains concentrated; distinguish this task from P4's representation change.
- [x] Verify the Context-action owner slice (baseline `f8c1224`): move reindex,
  checked pairing and lift requests out of the source union; pairing must share
  ordinary value post-checking instead of retaining a separate compare path.
  Preserve exact typed images, family binders, producer convergence and worker
  lifetime. Run focused/Debug/sanitizer tests, clean acceptance and paired
  timings before publication. This does not complete P4's budgeted lift or
  typed Context-formation representation.
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
- [x] Verify the IADT-scope slice against `1e47484`: constructor/IH scope and
  index-result elaboration move to `synthesis_iadt.c`; batch substitution
  belongs to `synthesis_context.c`. Remove the overloaded source payload,
  copied image producers and second index-result assembly state. Reuse the
  same substitution request, lexical binder interner and allocation attachment.
  Check cold/restored scopes, rejected motives/types, family fields, zero/single/
  bulk budgets, cancellation and exact completed-request reuse. Advance the
  existing lift worker incrementally, without claiming its synchronous final
  declaration checking is budgeted. Run full acceptance, sanitizers, cross-image
  reading and paired performance before publication.
- [x] Verify the Identity-owner slice against `4aecd9c`: remove five source
  descriptors and the family/face/formation payloads; share ordinary path
  post-checking. Test exact requests, distinct chosen paths, Context rejection,
  higher faces, zero/single/bulk budgets and cancellation. Run focused checks,
  full acceptance, affected sanitizers, cross-image and paired performance
  checks before deciding adoption and publishing this epoch.
- [x] Verify the Handler-owner slice: distinguish structure preparation from
  acceptance, preserve independent/restored and shared nested effect boundaries,
  registration failure, clause binder identities, repeated requests and source
  images. Run clean acceptance, affected sanitizers and paired performance;
  record the surviving Operation/CBPV/IADT ownership work separately.
- [x] Verify the Operation-owner slice: pending term/classifier projections,
  aliases and nested references, single/bulk budgets, exact request reuse,
  restored binder identities and wrong signatures; then clean acceptance,
  affected sanitizers, source-image compatibility and paired performance.
- [x] Verify the CBPV-adapter slice: exact request reuse, owner-sized state,
  provisional versus checked results, dependent/constant sequencing, wrong
  Contexts, zero/single/bulk budgets and cancellation. Run clean acceptance,
  sanitizers, old/new image cross-reading and paired performance. Keep the
  remaining CBPV rule inspection and ordinary-function extraction open.
- [x] Inspect `CLASSIFIER_FORMATION_JOB` with `type_structure_step` and
  `constructor_callable_origin`: its query progress already belongs to the
  shared typed-query engine. Move its provisional consumers with the request;
  do not introduce another classifier cache or extract entry points alone.
- [x] Verify `22661f4`: pending/accepted formation structure, exact request
  reuse and early Pi binder identity, result-Context input edges, cancellation,
  source images, clean acceptance and paired performance. Broad owner
  extraction and history removal remain separate unfinished criteria.
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
| 2026-09-26 | P4.3b | `270c271` checks retained ordinary functions without prior derivations; the shared APP rule validates retained result binders | Clean acceptance, Debug, sanitizers and cross-reading pass; history removal and budgeted checking remain open |
| 2026-09-26 | P5 | `12a0e1d` localizes four CBPV adapters and replaces their 240-byte private source state with 8/16/40-byte owner state | Clean acceptance, Debug, sanitizers, cross-reading and paired timings pass; broader P4/P5 remain open |
| 2026-09-26 | P5 | `22661f4` localizes function formation, Pi scopes and their pending consumers; no extra classifier cache | Clean acceptance, Debug, sanitizers, cross-reading and paired timings pass; broader P4/P5 remain open |
| 2026-09-26 | P4.3c | `c341229` validates result formation through the typed Term's `type` edge and deletes receipt cloning | Clean acceptance, Debug, sanitizers, legacy alternate-history images and paired timings pass; broader P4/P5 remain open |

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

### Typed-Only Function Verification

Candidate `270c271` against `f2b112b`, 2026-09-26, excluding inherited trials:
optimized Core/Identity and fresh-process typed-only tests pass. The latter
also passes Debug and ASan/UBSan; sanitized Core/Identity pass with the preceding
non-PIE flags. Core retains 238,032 terms and Identity's maximum comparison is
180,603 steps. The existing 100-repeat dependent-APP test adds no terms/proofs;
the new test also checks exact accepted-subject reuse with no allocation.
Clean `check-acceptance` exits zero: wall 25m36.015s, user 23m43.575s,
system 1m51.514s, including builds. This covers the four LT/partition variants,
general Sorted, images and optional-witness isolation/packets. Log:
`/tmp/a-program-typed-check-acceptance.log`. Paired performance is recorded below.

All eight derivation fixture modes cross-read with the preceding publication
at budgets 1/64 in both directions. The old and new compiler sources differ
only by this slice (`3397e52..f2b112b` has no compiler changes). Byte comparison
is not a general reproducibility test: effect/producer payload bytes differ,
and repeated effect-fixture writes by the baseline binary itself also differ. Cross-reading,
ordinary checking and effect results pass; no image-format change was made.

After acceptance, six paired `generic_sorted.sh` samples use the same script,
inputs and strict-O2 builds, with the order reversed for the last three pairs.
Baseline median: 5.740s (5.722-5.780); candidate: 5.8005s (5.742-5.909), about
1.1% slower. Normalized output and reported Solve counts match in every pair.
This small local difference is recorded, not claimed performance-neutral or
attributed to a specific helper without profiling. Logs:
`/tmp/a-program-typed-check-bench.70kJnt/`. Adopt the checked-structure slice;
P4.4 retains the timing/reuse follow-up, and full P4/P5 remain open.

Source delta `f2b112b..270c271`; documentation remains separate:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 70 | 15 | +55 |
| `evidence.h` | 7 | 4 | +3 |
| `evidence_function.c` | 36 | 0 | +36 |
| `evidence_cbpv.c` | 32 | 0 | +32 |
| `evidence_structure.h` | 22 | 0 | +22 |
| `tests/typed_structure.c` | 177 | 0 | +177 |
| `tests/typed_structure.sh` | 6 | 0 | +6 |
| `Makefile` | 8 | 0 | +8 |

Total +358/-19, net +339: implementation/headers +148, tests +183, build +8.
This is a checking prerequisite, not history removal or net code reduction.
Plan document: +94/-8 lines, net +86 (separate from source changes).

### CBPV Adapter Verification

Verified `12a0e1d` against `aa151fe`, 2026-09-26; inherited trials excluded:

- Strict-O2, Debug and ASan/UBSan synthesis tests pass, including pending
  effect/Context cycles, yielding and cancellation. Existing cases now assert
  smaller owner state, exact repeated requests and borrowed Body input edges.
- ASan/UBSan source-image suite passes. Old/new binaries cross-read modules,
  annotations, nominal declarations and all 13 retained source fixtures in
  both directions; single/bulk reads and checked/recomputed imports agree.
- General Sorted input has identical old/new outcomes at budgets 0 and 100
  (`pending`), and completion (`done steps=603582`). The five moved semantic
  functions are unchanged after normalizing field and shared-helper names.
- Clean `check-acceptance` passes, exit 0: wall 1555.189s (25m55.189s), user
  1442.026s, system 112.286s. Includes all four LT/partition combinations and
  optional witness packets. Log: `/tmp/a-program-cbpv-owner-acceptance.log`.

After acceptance, six paired `generic_sorted.sh` samples use identical inputs
and strict-O2 builds, with the order reversed for the last three pairs. Baseline
median 5.813s (5.783-5.838); candidate 5.745s (5.685-5.822), about 1.2% faster
locally. All normalized output and reported Solve counts match. This is not a
general speedup claim. Logs: `/tmp/a-program-cbpv-owner-bench.pVWhXv/`.

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 46 | 303 | -257 |
| `synthesis_cbpv.c` | 313 | 0 | +313 |
| `synthesis_source.h` | 8 | 0 | +8 |
| `tests/synthesis.c` | 31 | 0 | +31 |
| `Makefile` | 1 | 1 | 0 |

Total +399/-304, net +95: implementation/headers +64, tests +31, build 0.
Ten existing functions (236 lines) were moved and adapted; the additional
lines provide private descriptors/lifecycle/projection interfaces and tests.
The old dispatch cases and unused `request_typed` helper were removed. This is
owner/state isolation, not evidence-history removal or net code reduction.

### Function Formation Verification

`22661f4` against `64cbc89`, 2026-09-26; inherited edits excluded:
strict-O2, Debug and ASan/UBSan synthesis tests pass, including pending effect
Contexts, immutable structural snapshots, early Pi binder identity and request
reuse. ASan/UBSan source-image tests pass. All 13 retained fixtures plus modules,
annotations and nominal declarations cross-read in both directions with the
previous binary; single/bulk and checked/recomputed imports agree.

Clean `check-acceptance` exits zero: wall 1539.732s (25m39.732s), user
1425.980s, system 112.967s, including rebuilds and early sanitizer-build overlap.
All four LT/partition variants and optional-witness packets pass. General Sorted
with its required provider agrees at budgets 0/100 and completion:
`done steps=603582`. Logs: `/tmp/a-program-function-owner-*.log`;
cross-reading: `/tmp/a-program-function-cross.6bSuQf/`.

After acceptance, six paired `generic_sorted.sh` runs, one warmup each and
reversed order for the last three pairs, give baseline median 5.728s
(5.607-5.772) and candidate 5.7305s (5.710-5.759). All normalized output and
reported Solve counts match. This small local sample shows no material change,
not a general performance guarantee. Logs:
`/tmp/a-program-function-owner-bench.an3UTL/`. Adopt this slice; P4/P5 remain open.

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `Makefile` | 1 | 1 | 0 |
| `synthesis.c` | 33 | 158 | -125 |
| `synthesis_cbpv.c` | 25 | 0 | +25 |
| `synthesis_function.c` | 178 | 0 | +178 |
| `synthesis_source.h` | 11 | 0 | +11 |
| `tests/synthesis.c` | 35 | 0 | +35 |

Total +283/-159, net +124: implementation/headers +89, tests +35, build zero;
documentation excluded. Six construction helpers and two existing work steps
were moved/adapted, not new typing rules. The source driver's role cases were
removed; no second work engine, Core tag or image format was introduced.
This is owner/state isolation, not net code reduction or Evidence-history removal.

### Result Formation Verification

Verified `c341229` against `d09120d`, 2026-09-26; inherited trials excluded.
Strict-O2, Debug and ASan/UBSan IADT and derivation-image tests pass. The tests
require exact existing-result reuse without new Evidence/Core allocation and
reject a changed result type or Context, even with unchanged erased Core.
Acc is covered with both field-totality contracts; existing wrong schema,
nominal constructor, arity and IH allocation checks remain in place.

All eight derivation fixture modes cross-read in both directions at budgets
1/64. The old kernel also builds the updated nominal fixture, which requests
alternate formations; its additional-history images pass the new checker.
The new reuse assertion fails on that old kernel as expected, confirming the
test detects the removed behavior. No wire-format change is involved. Logs:
`/tmp/a-program-result-formation-{asan,debug}-*.log` and
`/tmp/a-program-result-formation-cross.TELgXV/`.
Full `check-acceptance` exits zero: wall 25m46.732s, user 23m54.670s,
system 1m51.199s, including builds and early overlap with the auxiliary checks.
All four LT/partition variants and optional-witness packets pass. Log:
`/tmp/a-program-result-formation-acceptance.log`.

After acceptance, six paired `generic_sorted.sh` runs with a warmup per binary
and reversed order for the final three pairs give median 5.7735s for the
baseline (5.671-5.792) and 5.7345s for the candidate (5.672-5.757). Normalized
output and reported Solve counts match. General Sorted with its required
provider agrees at budgets 0/100 (pending) and completion (603582 steps).
No material regression is observed in this local sample; it is not a general
speedup claim. Logs: `/tmp/a-program-result-formation-bench.mwUVwc/`.
Adopt this slice; no speculative Context/map canonicalization is included.

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `derivation.c` | 15 | 9 | +6 |
| `derivation.h` | 2 | 0 | +2 |
| `evidence.c` | 0 | 24 | -24 |
| `evidence.h` | 0 | 5 | -5 |
| `tests/derivation_io.c` | 48 | 0 | +48 |
| `tests/iadt.c` | 6 | 5 | +1 |

Total +71/-43, net +28: implementation/headers -21, tests +49, build zero;
documentation excluded. One cloning API and its temporary premise-array copy
are removed. Broader P4/P5 and general history-removal gates remain open.

Plan document: +75/-4, net +71, reported separately from source changes.

### Typed Map Image Verification

2026-09-26, implemented in `5e967a4` against `3266fa8`: single and bulk substitution image
access now checks/reuses `pg_context_map.images` through the existing structural
checker. It no longer walks Evidence prefixes or rebuilds their projection
receipts. Bulk access consistently requires scratch storage; production callers
already provide it. No new cache, rule, Core tag or wire format is introduced.
Context/map admission still retains its explicit premises. This is not the
completion of P4's typed declaration contract or general history removal.

Tests deliberately stop requiring the particular acceptance receipt originally
supplied for an identical typed image. This is not object-level proof
irrelevance: distinct conversion origins, classifiers, scopes and chosen
Identity paths remain tested. The first full run stopped at the corresponding
legacy receipt-identity assertion in `tests/identity.c`; only that assertion and
its explanation were changed, not the adjacent path/coherence checks.

Strict O2 Core/IADT/derivation tests and affected Core/IADT/Identity/derivation
tests under Debug and ASan/UBSan (leak detection enabled) pass. All eight
derivation fixture modes cross-read at budgets 1/64 in both directions. The
baseline reader uses the updated test driver because the older driver requires
distinct receipts for two roots now allowed to share; the baseline kernel and
wire reader are unchanged. Logs: `/tmp/a-program-typed-map-images-cross.xrmzKd/`
and `/tmp/a-program-typed-map-images-{asan,debug}-*.log`.
Full `check-acceptance` exits zero in 23m52.130s, including the updated test
driver build and early auxiliary-check overlap. All four LT/partition variants
and optional-witness separation/packet tests pass. Log:
`/tmp/a-program-typed-map-images-acceptance-final.log`.

After acceptance, six paired strict-O2 runs per input, with warmup and reversed
order for the last three pairs, give the following wall-time medians. The script
includes several checks and image round trips; individual source rows measure
Solve, not execution alone. Outputs and reported Solve counts agree exactly.

| Workload | Baseline seconds | Candidate seconds |
| --- | ---: | ---: |
| `generic_sorted.sh` | 5.738 | 5.716 |
| `explicit_index_family_append_check.p` | 0.006 | 0.0055 |
| `function_graph_certified_length_model.p` | 0.0035 | 0.004 |
| `function-field-induction.p` | 0.004 | 0.004 |
| `generic-quick-sorted.p` with provider | 0.8115 | 0.811 |
| `generic-quick-sorted-result.p` with provider | 0.739 | 0.7375 |

No material local regression is observed; millisecond-sized samples do not
establish a speedup. In three fresh-process RSS pairs, the last two rows have
medians 224032 -> 223916 KiB and 174600 -> 174792 KiB respectively. This is not
a whole-program memory bound. Logs: `/tmp/a-program-typed-map-images-bench.ZQdRJD/`
and `/tmp/a-program-typed-map-images-memory.log`. Adopt P4.3d; keep P4/P5 open.

Context-action owner verification, 2026-09-26, clean `f8c1224` plus this slice:

- Full `check-acceptance`: exit 0, **1546.382 s** (25m46s), including ordinary
  QuickSort-result/general Sorted proofs, four LT/partition variants, negative
  cases, partial/retained images and optional-witness isolation. Synthesis also
  passes Debug and ASan/UBSan with leak detection. No inherited local trial was
  included. Logs: `/tmp/a-program-context-owner-{acceptance,debug,asan}.log`.
- Old/new source-image cross-reading passes both directions for typed and
  reduction-retained lambda/family/append/function-field images, using both
  retained checking and recomputation. Evidence: `/tmp/a-program-context-owner-cross.QielOh`.
- Six paired timings after warmup, reversed order for the last three pairs:
  generic-sorted script **5.7205 -> 5.7210 s**; single Sorted source
  **0.8095 -> 0.8020 s**; ordinary-result theorem **0.7250 -> 0.7335 s**.
  Per-run medians of 100-process batches: explicit-index Vec append
  **5.605 -> 5.600 ms**, certified-length candidate **6.325 -> 6.200 ms**,
  function-field induction **3.730 -> 3.640 ms**. No material local timing
  regression; these small variations are not a speedup claim.
- Shared post-check scheduling adds steps: Sorted **603582 -> 604432**;
  ordinary-result **1117652 -> 1118238**. Accepted outputs/statuses agree.
  Three fresh-process RSS pairs give medians **223504 -> 223372 KiB** and
  **174984 -> 174948 KiB**, respectively. Evidence:
  `/tmp/a-program-context-owner-bench.u1pd1s`, `/tmp/a-program-context-owner-memory.log`.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 3 | 127 | -124 |
| `synthesis_context.c` | 130 | 0 | +130 |
| `tests/synthesis.c` | 15 | 0 | +15 |
| Prototype `Makefile` | 1 | 1 | 0 |
| Code/tests/build total, excluding this document | 149 | 128 | +21 |
| This plan document | 72 | 0 | +72 |

Most movement is owner localization, not deleted functionality. Implementation
alone grows by 6 lines; tests add 15. Adopt this verified slice; P4's typed
declaration contract/history migration and the remaining P5 owners stay open.

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 8 | 40 | -32 |
| `evidence.h` | 5 | 4 | +1 |
| `tests/core.c` | 35 | 24 | +11 |
| `tests/derivation_io.c` | 5 | 4 | +1 |
| `tests/iadt.c` | 2 | 1 | +1 |
| `tests/identity.c` | 4 | 3 | +1 |

Source total +59/-76, net -17: implementation/headers -31, tests +14,
build zero. Documentation delta is reported separately below.

Plan document delta against `3266fa8`: +92/-0, net +92, excluded from source totals.

### IADT Scope Verification

2026-09-26, baseline `1e47484` plus this slice. Full `check-acceptance` exits
zero in **1551.067 s** (25m51s), including 63/63 source compatibility cases,
ordinary QuickSort-result/general Sorted proofs, all four LT/partition variants
and optional-witness isolation/packets. This run preceded the final two-line
source-registration/Context guard restoration described above. After it,
strict-O2 and Debug synthesis, ASan/UBSan synthesis and source images (leak
detection enabled), source-origin/CLI images and the following comparisons
pass. The final guard regression fails when the wait is removed; the whole
25-minute suite was not repeated after that local correction. Cancellation
includes all 1,696 boundaries of the new indexed-Vec/IH example.

Final old/new image checks pass in both directions for six fixtures and two
retention modes: 24 reader cases, using checked and recompute paths. All 12
writer fixture/mode pairs are byte-identical. Evidence:
`/tmp/a-program-iadt-owner-{acceptance,regression-red}.log`,
`/tmp/a-program-iadt-owner-guard-*.log`,
`/tmp/a-program-iadt-owner-final-cross.Rxr7P9`.

Final quiet strict-O2 comparison: warmup plus six pairs, reversing order for
the last three. Small inputs use 100-process batches; table entries are median
wall seconds per invocation. This measures Solve, not execution alone.

| Workload | Baseline | Candidate |
| --- | ---: | ---: |
| `generic_sorted.sh` | 5.721000 | 5.732000 |
| General Sorted source | 0.808000 | 0.813500 |
| Ordinary QuickSort-result theorem | 0.742000 | 0.737500 |
| Explicit-index Vec append | 0.005525 | 0.005510 |
| Certified-length candidate | 0.003905 | 0.003955 |
| Function-field induction | 0.003630 | 0.003705 |

Statuses/results agree after excluding step counts and temporary paths.
Budgeted lift changes Sorted steps **604432 -> 623532** and ordinary-result
steps **1118238 -> 1123411**. Three fresh-process RSS pairs give medians
**223840 -> 223064 KiB** and **174824 -> 175260 KiB**, respectively.
No material local regression is observed; these variations are not a speedup
or global memory-bound claim. Logs: `/tmp/a-program-iadt-owner-bench.FP1VOl`
and `/tmp/a-program-iadt-owner-memory.log`.

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 24 | 370 | -346 |
| `synthesis_iadt.c` | 278 | 0 | +278 |
| `synthesis_context.c` | 115 | 1 | +114 |
| `synthesis_source.h` | 15 | 0 | +15 |
| `synthesis.h` | 4 | 3 | +1 |
| `tests/synthesis.c` | 60 | 6 | +54 |
| `Makefile` | 1 | 1 | 0 |
| Code/tests/build total | 497 | 381 | +116 |

Implementation/headers grow by 62 lines; tests by 54. Adopt this owner split
and exact-request reuse, not a claim of net code reduction. Do not replace
explicit declaration inputs with a new family cache, Context formation pointer
or exported all-domain union. P4's representation/budgeting work and remaining
P5 owners stay open. Documentation changes are excluded from the table.
This plan changes by +100/-9, net +91, against `1e47484`.

### Identity Owner Verification

2026-09-26, clean `4aecd9c` plus this slice. Full `check-acceptance` exits zero
in **1553.701 s** (25m54s), including 63/63 compatibility cases, the ordinary
QuickSort-result theorem, all four LT/partition variants and optional-witness
isolation/packets. The code was frozen before this full run. Debug and ASan/UBSan
synthesis pass; sanitized source-image tests pass with leak detection enabled.
The new cancellation test covers all 60 boundaries of formation, face,
instantiation, reflexivity and family action. An independent caller finds the
same completed path post-check with no additional request or accepted result.

All 13 retained fixtures cross-read with the previous binary in both retention
modes and directions, using checking and recomputation: 52 reader cases and
26 byte-identical writer pairs. The general QuickSort-result theorem also
cross-loads in both directions after saving at budgets 0, 100 and 5,000,000.
Logs: `/tmp/a-program-identity-owner-{acceptance,debug,asan,asan-io}.log`,
`/tmp/a-program-identity-owner-cross.lvak9v/` and
`/tmp/a-program-identity-owner-theorem-cross.MNkKOm/`.

After acceptance, strict-O2 paired runs use one warmup and six samples per
binary, reversing order for the final three pairs. Small workloads use batches
of 100 processes. Entries are median wall seconds per invocation (Solve, not
execution alone); normalized outputs agree after excluding step counts/paths.

| Workload | Baseline | Candidate |
| --- | ---: | ---: |
| `generic_sorted.sh` | 5.734000 | 5.722500 |
| General Sorted source | 0.823500 | 0.825500 |
| Ordinary QuickSort-result theorem | 0.740500 | 0.754000 |
| Explicit-index Vec append | 0.005585 | 0.005565 |
| Certified-length candidate | 0.003935 | 0.003985 |
| Function-field induction | 0.003730 | 0.003725 |

The result-theorem median is 1.8% slower; ranges overlap (0.729-0.764 versus
0.735-0.780s). No speedup or strict performance-neutrality claim follows from
this sample. Shared post-check scheduling changes Sorted steps 623532 ->
623736 and result-theorem steps 1123411 -> 1123535. Three fresh-process RSS
pairs give medians 223516 -> 223096 KiB and 174788 -> 174368 KiB. Logs:
`/tmp/a-program-identity-owner-bench.YybAUc/` and
`/tmp/a-program-identity-owner-memory.log`.

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `synthesis.c` | 2 | 405 | -403 |
| `synthesis_identity.c` | 423 | 0 | +423 |
| `tests/synthesis.c` | 75 | 0 | +75 |
| `Makefile` | 1 | 1 | 0 |
| Code/tests/build total | 501 | 406 | +95 |

Implementation grows by 20 lines and tests by 75; this is not net code
reduction. The source driver falls from 8444 to 8041 lines. Private states are
8/16/32/8/72 bytes for formation/face/instance/reflexivity/family action, versus
the former 240-byte source state (plus a separate family payload). The shared
80-byte header is unchanged. Remove the old classifier adapter and five driver
branches; retain the same request index, action workers and checking rules.
Adopt this verified owner-localization slice. P4's declaration contract/history
migration and the remaining P5 owners/codec consumers stay open.

Documentation: +84/-9, net +75 lines, excluded from the source totals above.

Variable-image follow-up verification, 2026-09-26, baseline `382a0a6` plus
this slice (inherited trials excluded):

- The new regression fails on the baseline at its post-inspection receipt
  assertion and passes after the change. Optimized/Debug IADT and ASan/UBSan
  IADT/Core checks pass, including leak detection.
- Full `check-acceptance`: exit 0, wall **1548.342s**, user 1438.850s, system
  108.604s. Source compatibility is **63/63**; ordinary-result QuickSort, four
  LT/partition variants and optional-witness isolation/packets pass.
- Thirteen fixtures in two retention modes give 26 byte-identical old/new
  image pairs and 52 bidirectional check/recompute cases. The ordinary-result
  theorem cross-loads at zero, 100 and complete Solve in both directions.
- Quiet paired measurements: one warmup, six pairs, reversed final three;
  small cases batch 100 processes. Median seconds follow. Normalized output
  and reported Solve steps agree. The small differences are not a speedup claim.

| Workload | `382a0a6` | Candidate |
| --- | ---: | ---: |
| `generic_sorted.sh` | 5.745215 | 5.736955 |
| General Sorted source | 0.816259 | 0.804133 |
| Ordinary QuickSort-result theorem | 0.743240 | 0.741066 |
| Explicit-index Vec append | 0.005502 | 0.005492 |
| Certified length | 0.003907 | 0.003934 |
| Function-field induction | 0.003659 | 0.003659 |

Three fresh-process RSS pairs give medians 223172 -> 222996 KiB (Sorted) and
174680 -> 174476 KiB (result theorem). Logs are under
`/tmp/a-program-variable-image-{acceptance,debug,asan-iadt,asan-core}.log`,
`/tmp/a-program-variable-image-{bench,memory}.jsonl`,
`/tmp/a-program-variable-image-cross.k40vtf/` and
`/tmp/a-program-variable-image-theorem-cross.XqphB8/`.

Per-file source delta: `evidence.c` **+1/-7, net -6**;
`tests/iadt.c` **+49/-0**. Total code/tests **+50/-7, net +43**;
documentation is excluded. Adopt this deletion of a redundant inspection
path. Context-formation representation and broader P4/P5 remain open.
Documentation: **+70/-2**, net +68 lines.

CBPV content inspection, 2026-09-26, baseline `173a550` (agent assessment):
`pg_prove_return_value` and `pg_prove_thunk_computation` currently select direct
inputs by introduction-rule/premise position. Other routes query typed children
but add inverse receipts even when the exact child is already accepted. The
trial uses the existing typed-input query for both routes and reuses checked
children after validating the canonical head and classifier boundary. Changed
classifiers still require inversion; nonempty RETURN effects still reject.
No new fast-path cache, scheduler, tag or wire version is introduced. This is
checking-history consolidation, not proof irrelevance or equality reflection.

Two tests formerly demanded redundant extraction receipts; they now require
the exact accepted child. The new typed-only image test checks RETURN and
THUNK after an alternative conversion history without another receipt. It
fails on the baseline at `result == child`, not at a typing/soundness check.

Fresh publication checks (inherited worktree trials excluded):
- Full `check-acceptance`: exit 0; wall **1535.391s**, user 1426.887s, system
  107.710s. Includes source compatibility **63/63**, ordinary-result QuickSort,
  all four LT/partition variants and optional-witness isolation/packets.
- Optimized Core/Synthesis/Identity and typed-only tests pass; Debug covers
  Core/Synthesis/typed-only. All four pass ASan/UBSan with leak detection.
- Thirteen fixtures, two retention modes: **104** old/new cross-check/recompute
  invocations pass. Of 26 image pairs, 24 are byte-identical. Append shrinks
  44078 -> 42672 bytes (ordinary) and 75422 -> 73184 (typed retention).
  The ordinary-result theorem also cross-loads at 0/100/complete Solve.

Quiet timings: warmup plus six pairs, reversed final three, 100-process batches
for small cases. Median seconds below; outputs and source Solve steps agree.
Append is 2.1% slower; sampled ranges overlap. Do not claim a speedup or exact
performance neutrality. Three-pair RSS medians (KiB) are 223368 -> 223476 for
Sorted, 174560 -> 174616 for its result theorem, 546052 -> 545992 for derived LT.

| Workload | `173a550` | Candidate |
| --- | ---: | ---: |
| Generic Sorted suite | 5.784387 | 5.796864 |
| General Sorted source | 0.825115 | 0.824464 |
| Ordinary-result theorem | 0.745069 | 0.750251 |
| Derived LT plus content proof | 2.559877 | 2.528485 |
| Vec append | 0.005416 | 0.005528 |
| Certified length | 0.003904 | 0.003925 |
| Function-field induction | 0.003687 | 0.003654 |

Per-file added/removed/net under `src/prototype/pointer/`: `evidence.c` 2/2/0,
`evidence.h` 2/1/+1, `tests/core.c` 1/1/0, `tests/synthesis.c` 3/2/+1,
`tests/typed_structure.c` 38/4/+34. Code/tests total **+46/-10, net +36**;
documentation excluded. Runtime checking-history duplication decreases, but
this is not source-line reduction. Adopt P4.3f; broader P4/P5 remain open.
Logs: `/tmp/a-program-unary-input-{acceptance,debug-*,asan-*}.log`,
`/tmp/a-program-unary-input-{bench,memory}.jsonl`, cross-images in
`/tmp/a-program-unary-input-cross.tJRPXn/` and
`/tmp/a-program-unary-input-theorem.E21goA/`.
Documentation: **+58/-0**, net +58 lines.

### Function-Graph Typed-Input Verification

2026-09-26, clean candidate against `7f25362`; inherited Context/IADT trials
excluded. This adopts P4.3g, not the remaining P4/P5 work. No parser, Core tag,
wire version, separate scheduler or acceptance cache was added.

- Full `make check-acceptance`: exit 0; wall **1590.654s**, user 1478.643s,
  system 111.213s. Includes compatibility **63/63**, ordinary-result Sorted,
  all four LT/partition variants, witness isolation and packets.
- Debug and ASan/UBSan with leak detection: Core, Program, IADT, Identity,
  Synthesis and fresh-process typed-only images pass. The eight new graph
  cases cover alternative Pi admission, specialization and selected bounds;
  repeated source lookup allocates no new proofs or occurrences.
- Six source fixtures at 0/100/full fuel, ordinary and retained: **36 image
  pairs, 72 opposite-version loads** pass. Partial/family function images also
  check results at chunks 1/64. Occurrence/derivation images pass **8** reads
  across the two versions. Neither image bytes nor Solve steps are canonical.

Quiet comparison: one warmup plus six pairs, last three reversed, same `-O2`
build flags. Small cases use 100-process batches; table reports median seconds
per process. All include source loading; the last also includes NF. Source
statuses and length NF agree; Solve transition counts change. Sample ranges
overlap for every case. Partial functions are 2.0% slower by median; no general
speedup or exact performance neutrality is claimed.

| Workload | `7f25362` | Candidate |
| --- | ---: | ---: |
| Certified length | 0.008879 | 0.008805 |
| Partial function | 0.007875 | 0.008033 |
| Function-field induction | 0.011067 | 0.010922 |
| Indexed append | 0.005511 | 0.005509 |
| General Sorted | 0.802607 | 0.803444 |
| Ordinary-result theorem | 0.725799 | 0.728126 |
| Derived LT and content proof | 2.534412 | 2.504927 |
| Certified length plus NF | 0.009285 | 0.009184 |

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/evidence.c` | 7 | 1 | +6 |
| `src/evidence.h` | 4 | 0 | +4 |
| `src/function_graph.c` | 72 | 32 | +40 |
| `tests/core.c` | 72 | 1 | +71 |
| `tests/iadt.c` | 2 | 3 | -1 |
| `tests/program.c` | 3 | 3 | 0 |

Implementation **+83/-33, net +50**; tests **+77/-7, net +70**. Duplicate
Lambda reconstruction and inverse receipts decrease, but source code does not.
The broad reduction criterion remains open. Selected scopes/bounds require
explicit checking; retaining a raw Context admission is not enough.

Evidence: `/tmp/a-program-function-typed-final-{acceptance,profiles,cross}.log`,
`/tmp/a-program-function-typed-final-acceptance.time`,
`/tmp/a-program-function-typed-bench.jsonl` and cross-images in
`/tmp/a-program-function-typed-cross.PkLaFp/`. These are local execution records,
not portable fixtures; the new boundary regressions are in the tracked tests.
This plan: **+84/-0**; combined implementation/tests/docs: **+244/-40, net +204**.
