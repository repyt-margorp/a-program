# Artifact Semantic Persistence Refactor

Date: 2026-09-28
Updated: 2026-09-30
Status: reviewed Job and necessary APGSRC70 Source dependencies verified for accepted promotion;
full persistence/checkpoint and C backend remain prototype work.
Exact resumption and trust/fuel integration remain unfinished.
Baseline: `152b59506e915e18a34f6dc8041e981fb2a82888` (PR #45 documents
imported). Implementation is unchanged from `e716232`; unrelated working-tree
changes are excluded from this audit.

## Problem List

0. **AP0:** audit and remove redundant Job/Evidence construction before adding
   further persistence or backend features (2026-09-30 user instruction).
1. **AP1:** specify what `.a` preserves, without confusing semantic progress
   with evaluator history or making C the owner of A Program semantics.
2. **AP2:** implement shared, inert transport of that data and remove unused
   retention only after its necessary consumers have been accounted for.
3. **AP3:** verify zero-fuel stability, preserved meaning and a downstream-only
   consumption boundary; keep unfinished checkpoint work visible.
4. **AP4:** implement the first downstream C transpiler, now explicitly requested.
5. **AP5:** add the downstream Linker/LinkerScript requested on 2026-09-29
   (#46, PR #48); do not change artifact semantics or admission policy (#47).
6. **AP6:** realize ordinary C arguments/results and target-native calls/data
   from selected typed exports (#49, PR #50), not only structural execution.

This is the active implementation plan, superseding R1-R6 in the
[growth audit](2026-09-28-RETAINED-IMAGE-GROWTH-AUDIT.md). That document retains
measurements, causes and the separate Context/syntax investigations.

## AP0. Simplify Before Extending Persistence

### Subjective (User)

2026-10-04 09:42 UTC, English translation of explicit human adoption via
inquiry desk019ebfae: migrate the current prototype Job into accepted code,
replacing the old broadly expanded Job. This supersedes the earlier
prototype-only boundary for this Job refactor.

2026-10-03, English paraphrase: explore C-backend and surface development in
parallel with the necessary Job/Evidence refactor on one PC, coordinating the
`.a` interface. The earlier serial AP0 prerequisite must not be interpreted as
forbidding independent backend work on an agreed snapshot. The user requests
a workflow proposal; this does not approve `.a` extensions or promotion.

2026-10-03, English paraphrase of the user's follow-up: review the accumulated
GitHub issues to ground the parallel-development proposal in actual pending work.

2026-10-03, English paraphrase: keep this terminal as the primary coordinator;
consider managing additional Codex development sessions with tmux. Determine an
appropriate number of parallel lanes. This asks about feasibility and organization,
not immediate installation or worker launch.

2026-10-03, English paraphrase of the user's selected division: Main owns the
Job/Evidence refactor, Sub1 owns C-backend design, and Sub2 owns system
performance and waste removal. Simpler implementation takes priority over
technical tuning that complicates the system. Comparative investigation should
consider Bend2, Lean, Agda and Rocq; related issues/PRs are forthcoming. This
supersedes the surface/library worker proposal below, not the downstream-only
backend boundary or the existing prototype/promotion rules.

2026-10-03, English paraphrase of the follow-up: Main regularly supervises and
directs the tmux workers; each uses `/goal`, `6.1 Sol` and `xhigh`. The user is
currently submitting performance audit documents/issues. Setup and supervision
are tracked in the three-lane plan, not a second backend or frontier work list.

2026-10-03, English paraphrase of the latest addition: add a Surface Sub3 for
function/function-graph Binder notation and `.p` updates, retaining Main and
Sub1/Sub2. This supersedes the three-session count, not the ownership boundaries.

2026-10-01, English paraphrase of the user's clarification: do not re-expand
Oracle-local semantic structures into a common Job enum/union/dispatcher. The
frontier should reference the actual constraint owners, not a second program
graph. Fewer Jobs or replacement descriptors alone do not satisfy this intent.
Record new user requirements immediately in each affected active plan's
Subjective before further work or context compaction, as now required by
`AGENTS.md`. The linked SE1-SE5 plan remains the single AP0 work list.

2026-10-01, English paraphrase of the user's further instruction: remove the
Job/Evidence representations that flatten Core/Oracle-local structures into
another graph. Resume should build on verified witness Terms and typed
Occurrences plus their unfinished Solve obligations, rather than persist a
duplicate semantic program. Validate this intended architecture; do not count
adapter deletion alone as completion. Detailed requirements remain in SE1-SE5.
The same follow-up requests a balanced audit/implementation loop, not an
audit-only detour or unchecked wholesale deletion.

2026-10-01, English paraphrase of the latest correction: do not substitute
documentation edits for implementation; continue code changes and verification
alongside the SE ownership audit. GitHub connectivity is reported restored.

### Objective (Code): Parallel Work Inspection

2026-10-04, Root reviewed Job promotion at accepted parenta8418715/runtime120
930997b6: final compact-default acceptance coverage passes after targeted
image-CLI harness correction; broad exit2 retained, no full-replay0 claim.
Initial regression and five owner/final18 CLI sanitizer controls pass; strict3 remain.
The same annotation-free finite-permutation materialized image grows
from old APGSRC62 1,365,999 bytes to materialized APGSRC70 20,728,552 bytes.
Final default saves keep the compact/recomputable profile: 1,369,677 bytes, Core
objects21/terms21 and default-cap load0. The larger profile is explicit
`--save-materialized`; refusal and equality under 2,000,000 remain tested.
This is new retention growth and no peak-RAM benefit is claimed. See the
[promotion receipt](../src/prototype/solver_inputs/accepted_promotion/20261004/review.json)
for exact old/new table counts, selected dependencies and verification.

2026-10-03, `eb0aad6` plus local documentation edits: GitHub REST lists seven
open issues (#41, #43, #44, #47, #49, #51, #52). Initially two open PRs were #53
(audit documents) and #54 (generic MergeSort library); a follow-up REST check
also finds #55 (distributed Solve design intent, documentation-only). Earlier
PR bodies/file lists and #55's body were inspected, not complete diffs or fresh
verification. No new Bend2/performance submission was identified. PR-reported tests
are historical to their own revisions. No PR was merged or issue closed here.
Backend `emit.h` accepts borrowed typed Occurrences; `main.c` still obtains
exports through Job-root loading and synthesis results. Build defaults use
shared `/tmp` paths, so concurrent workers must explicitly set their own
`OVERLAY` and `BUILD` and never share mutable assembly/symlink targets.

### Assessment: Proposed Parallel Workflow

2026-10-04, Root implementation decision: select only necessary reviewed Job,
Evidence/query and Source I/O dependencies, preserve unowned changes, and
exclude held Performance, separate Surface syntax and unqualified owner trials.
Accepted promotion is separate from full frontier/codec/cost completion.
Fresh attribution supersedes the initial materialized-default choice: preserve
compact CLI/REPL saves, explicit typed retention and revalidate both profiles.
The Job API does not require larger default images; explicit retained growth
remains open.

User-selected division: Main owns Job/Evidence, Sub1 C-backend design, and
Sub2 system performance through simplification, with the latest instruction
adding Sub3 for surface Binder syntax and `.p` changes. The
[parallel coordination plan](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md)
records ownership and verification; SE/AP remain the actual work lists.
Agent workflow proposal: use separate branches/worktrees and build/overlay/output
directories for the three lanes.
Each lane pushes its own branch; one integrator publishes tested changes to
Main. Do not concurrently edit a shared worktree or a shared plan; lane plans
link the existing authority/persistence work list instead of copying it.

The latest assignment has four development sessions: this coordinator owns Core,
Sub1 owns C lowering, Sub2 performance/simplification and Sub3 surface syntax.
This supersedes the earlier three-session recommendation.
Add a temporary
read-only reviewer when useful, not a fourth overlapping implementation owner.
Independent tmux-managed CLI sessions have separate contexts; task briefs must
name their branch, write scope, baseline and completion tests. Workers report
cross-owner defects instead of repairing another lane's files. Full regression
and performance runs use a coordinated single slot; focused tests may overlap.
Local inspection found `codex-cli 0.159.2` and no `tmux` on PATH. No workers or
worktrees were launched, and no installation was performed. CLI sessions can
use `-C` for an explicit working root; tmux would manage those processes rather
than provide automatic context sharing or proof of their correctness.

Backend lowering already borrows typed structure. Keep the loader/export adapter
coordinated with the Core owner, without another shared IR or shadow acceptance
graph. Pin a committed producer revision, its exact overlay recipe, format
identifier and immutable `.a` fixtures for backend work; unfinished refactor
trials are not that baseline. Agree export meaning, supported Oracle scope,
pending/unsupported behavior and explicit trust/fuel policy, not solver-private
layout. Version incompatible format changes explicitly; no old-format reader
compatibility is required. Source-only syntax changes lower to existing typed
operations and preserve `::` post-synthesis assertion. Integration tests both
pinned fixtures and new producer images, including input-image immutability.
The three current public resume failures remain Core obligations, not backend
waivers. No promotion or additional `.a` fields are authorized by this proposal.

| Lane | Backlog | Dependency |
| --- | --- | --- |
| Main: Core/typing/persistence | SE1-SE5; shared policy in #47; semantics of #52 | Owns admission, resume, artifact transport and integration. |
| Sub1: C lowering/LinkerScript | #44, #49 | Develops supported target profiles against pinned artifacts; coordinates export API changes. |
| Sub2: performance/simplification | #51; review #52 / PR #53; new input awaited | Measures all paths; develops agreed non-overlapping deletion epochs; Main-owned findings return to Main. |
| Sub3: surface syntax / `.p` migration | Function/function-graph Binder changes; exact new design to inspect | Owns a source-layer prototype; typed semantic changes require Main coordination. |

#41/PR #54 and #43 remain backlog, not automatic Sub3 assignments. General
recursion still requires agreed semantic rules rather than parser changes.

Do not block all target work on broad #47 relevance/partiality research. Start
with already checked supported exports. Do not treat all of #52 as backend-only,
or merge #43 as a cosmetic surface change. Review each PR against current code
before adoption; open status alone does not establish a missing implementation.

### Existing Audit and Progress Record

The user's 2026-09-30 follow-up requires auditing Job/Evidence duplication,
revising the plan from that audit, and completing the refactor before continuing.
The [Solver/Evidence audit and SE1-SE5 work list](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md)
now controls the next implementation. It records measured Evidence-only Jobs,
duplicated construction paths, premise overlap and export-time graph copying.
"No recomputation" does not justify keeping an unnecessary wrapper.
The follow-up Job audit distinguishes removable completed/query wrappers from
actual unfinished construction. Retain a work record only for a demonstrated
obligation with no other owner; fewer adapters alone do not complete AP0.
Do not replace the Job graph with a renamed graph or put typing state in Core.
The user's follow-up proposes resuming from the Solve constraint frontier.
Audit canonical constraint ownership and its partial-work cursors, with readiness
derived from those same records rather than a second input/result graph. Keep
checked admission distinct from merely constructing descriptive typed data.

The [direct-input prototype](../src/prototype/solver_inputs/README.md) removes
checked normalization, Context-input, post-check and rule-premise adapters and
passes its regression gates. Rule readers and export use direct references;
structural queries no longer allocate the broad source work state. Remaining
Evidence adapters and duplicate construction paths keep SE1 open. Composition
and lifting now borrow their existing typed queries without a second Job;
their ordinary consumers retain only a query reference. Substitution images
now use direct inputs too; checked family-pair results no longer allocate
completed Jobs. The duplicate known/pending substitution APIs are removed.
Classifier operands now use direct inputs and normalization borrows the existing
typed query, without its former classifier-formation Job. Removing forwarding
alone lost completed-result sharing and was rejected; that ownership remains
open. Nominal IADT lookup also takes direct inputs and retains its typed query
across suspension, without an Evidence adapter or repeated normalization-proof
lookup. These changes do not repair the public split-fuel failures or authorize
a new checkpoint/backend codec.
Logical-family conversion and Lambda-body/Pi-scope contexts now also take direct
inputs; their checking and semantic conversion remain ordinary Solve work.
Application/sequencing/result-context entry points are now unified around those
inputs, including handler and constant-result consumers. Source-scope Contexts,
lookup and environment export now also use direct inputs. The checkpoint
prototype borrows checked external data separately from scheduled workers;
it does not reconstruct Context Jobs to preserve its previous physical layout.
Identity formation/faces/reflexivity/instances and family action/transport now
also take direct inputs, including their source and IADT consumers. Separate
checked/pending APIs and the temporary path-to-Job array are removed and verified.
Operation signatures, handler signature premises and startup Contexts now also
borrow direct inputs through source transport, without additional wire fields.
Remaining IADT/known-result adapters and duplicated query ownership are still
SE1 work. Independent Job-graph removal and public persistence remain open.

Do not extend the binding/domain checkpoint codec or AP6 while this gate is
open. Existing passing checkpoint fixtures remain historical evidence, not a
commitment to preserve every current worker representation. Public split-fuel
failures remain open. This changes task order, not Core/type boundaries or the
requirement to keep Transpiler/Linker-only data out of `.a`.

## AP1. Persistence Contract

### Subjective (User)

User paraphrases, 2026-09-28:
- Incorporate the `.a`-to-C issue/PR when deciding which artifact data to retain.
- The transpiler follows `.a`; it must not change A Program to accommodate C.
  Keep its implementation in a separate directory under `src/`.
- Complete the artifact refactor first. Start C transpilation only after a
  subsequent user instruction. General recursion/loop syntax remains discussion.
- Include quantitative fuel/image tests, especially `--steps 0`.
- Follow-up, same date: equal cumulative fuel (for example 10 twice versus 20
  once) should produce equal-size images; byte-for-byte equality is preferable.
- Import all three research documents from PR #45 into `doc/`, preserving them.
- Follow-up, reaffirmed 2026-09-29: old artifacts need not remain readable. Do not
  maintain backward compatibility. This supersedes the agent's legacy-import
  proposal, not any of the same-format stability requirements.
- Follow-up, 2026-09-29 (user paraphrase): use one total fuel budget with a
  configurable revalidation sublimit. Explicitly trusting prior results may
  skip revalidation. Verification history may differ across reloads and is
  excluded from semantic comparison. Keep trust/reuse policy local to artifact
  I/O; it is not a prerequisite for a C backend.
- Latest follow-up, same date: reject file-size-derived read allowances. Use a
  fixed bound or no bound, and investigate excessive reachable typed data rather
  than treating a larger allowance as compaction. Proceed without further asks
  this morning.
- Follow-up, 2026-09-29 (user paraphrase): continue through C transpilation;
  extend this plan as necessary and push independently verified increments.
  This supersedes the earlier instruction to wait for another backend request.
- Follow-up, same date (user paraphrase): merge the new PR and explicitly add
  Linker/LinkerScript to `.a`-to-C. These are subordinate transpiler facilities,
  not A Program language features or reasons to change `.a`. See AP5.
- Follow-up, same date (user paraphrase): merge the native-lowering PR; a suitable
  LinkerScript must produce modules usable from other C code, without merely
  carrying the source implementation model into C. Close issues whose actual
  completion criteria are met; do not retain them for unrelated later work.
  See AP6 and the issue audit below.

### Objective (Code)

- [Issue #44](https://github.com/repyt-margorp/a-program/issues/44) and
  [PR #45](https://github.com/repyt-margorp/a-program/pull/45) propose a semantic
  view, selected-export readiness and preserving materialized progress. They do
  not implement those facilities or authorize accepting unverified inputs.
- `src/source_io.h:6` and `source_io.c:590` describe APGSRC62 reconstruction;
  `source_io.c:924` reads inputs and `:990` schedules ordinary synthesis.
  This is not a general export of settled typed roots.
- `src/main.c:54` and `reduction_io.c:12` collect whole evaluator stores and old
  archives. The CLI does not consume that archive as a reusable result cache.
- `src/typing.h:30` already defines descriptive occurrences: Core, classifier,
  typed formation, context, scoped operands, maps and induction allocation.
  They are not acceptance receipts. `occurrence_io.h` transports them inertly.
- `occurrence_io.c:152,275` uses name/resolve context I/O only. Full descriptor
  variants already exist in `context_io.h`; nominal declaration transport exists
  in `declaration_io.c`. There is no need for a second C-specific type graph.
- `derivation_io.h` already transports unaccepted rule inputs and effect
  definitions with shared premises. Reduction claims are separate. Neither a
  restored classifier nor a stored status proves validity.
- `program.h` makes named exports whole-module-checked selections. Per-export
  backend readiness must not silently bypass that current language contract.

### Assessment

**Revision to the earlier cleanup proposal:** deleting history and always writing
only APGSRC62 would fix one growth source, but would not satisfy the semantic
progress requirement of #44. Do not declare that limited change the completed
artifact refactor. Conversely, future C output does not justify retaining every
WHNF/NF request. Reachability from meaningful roots, not elapsed work, determines
what must survive.

Agent-proposed data contract, to be refined against concrete owners in AP1.2:

| Data | Retention rule |
| --- | --- |
| Selected exports and already materialized Core/Oracle structure | Retain roots and their dependency closure; preserve aliases and root order |
| Typed occurrences, contexts/maps, classifier formation and nominal declarations | Retain reachable construction, not another flattened copy of the same answers |
| Object-language proof/witness Terms | Ordinary semantic roots/dependencies; never erase merely because they are proofs |
| Inputs needed to validate a retained judgement or transformation | Share the existing rule/premise structure where required; first audit whether occurrence structure already suffices |
| Unfinished elaboration/verification/effect obligations | Retain their proposition/inputs, context and dependency links; no copied scheduler status as authority |
| Source/annotation recipes for unmaterialized parts | Keep enough to continue through the existing Solve path; do not drop pending definitions just because no typed root exists yet |
| Reduction results/certificates | Keep only if a specified reachable consumer needs them; wholesale evaluator snapshots are not semantic exports |
| Hash buckets, redundant indexes and old generations | Reconstruct/discard; not canonical content |
| Unfinished computation frontier / continuation | Retain or deterministically reconstruct the necessary current state if exact fuel-partition resumption requires it; do not confuse it with all past reductions |
| C names, layout, ABI, calling conventions, target profiles and generated code | Downstream only; absent from canonical `.a` |

This table describes result-retaining output. The user's earlier permission to
emit explicitly recomputable images is implemented by the opt-in inputs profile
below; it omits materialized results rather than claiming their progress survives.

Use existing graph identity and shared relocation. Wire ordinals identify records
inside an image, not permanent machine pointers or a new Claim ID hierarchy.
Do not merge nominal families or scoped binders by alpha/WHNF equality.

By default loading restores descriptive content, not trusted acceptance. Inspecting it must
not run synthesis or normalization. Any validation still required is explicit,
budgeted through existing owners, and distinguishable from loading. Preserving
the witness/input is not the same as remembering that a previous process accepted
it. Do not add an independent replay engine or promise free proof checking.

Adopt #44's one-way dependency and semantic export intent. Defer its profile
lattice, general lowered IR, range analysis, CUDA/RTL and assumption manifests.
The exact API names and extra layers in the supplied document are proposals,
not mandatory architecture. A read-only view may borrow existing nodes; it need
not copy them into a second permanent database.

#### Owner Inventory (2026-09-28)

Inspected against `a696276` (`src/` unchanged from `e716232`). This inventories
the existing image payload and identifies missing progress, not a completed
checkpoint implementation. Prototype tests also run separately against the
working tree; unrelated Evidence/IADT changes are not part of this refactor.

| Current owner/data | Consumer and decision |
| --- | --- |
| `source_io.c`: definition policy, ordered selections, source scopes, names/imports, syntax DAG | Source reconstruction and name lookup; required for unresolved source. Selection aliases must remain aliases. |
| Producer DAG: source/rule/definition/operation/normalization inputs, module entry indices | Exact source-to-producer reconnect; retain identity/operands, not another lookup by erased Core. Current producer records have **no materialized typed result** slot. |
| Source binding addresses, declaration/member/match contexts and lexical references | Preserve generative identities when elaboration continues. These are allocation inputs, not proof that a type or branch is valid. |
| `occurrence_io.c`: judgement, context, Core, classifier, formation, annotation, origin/map, selection, operands, construction maps, induction allocation | Typed inspection and ordinary kernel rules. Required reachable semantic structure; currently omitted from the Program writer. Descriptor transport is now prototyped. |
| `context_payload.c` / `declaration_io.c`: telescopes, indices, constructor/result images, nominal descriptors | Shared typed/Core dependency closure; required. Temporary packing arrays and wire ordinals are disposable, not another semantic authority. |
| `derivation_io.c`: rule parameters and premise DAG; comparison endpoints instead of certificate pointers | Existing source rule reconstruction and ordinary `pg_synthesis_derive`; keep necessary validation inputs. Do not equate an occurrence's existence with accepted evidence. Audit which are reconstructible from the retained occurrence before deleting any. |
| `effect_inference.c`: parameter/seed pairs and dependency triples | Input equations for handler/classifier inference. Existing exporter requires complete contributions, then reader leaves them unsealed. Partial contribution discovery and converged results need an explicit progress contract. |
| `program.retained_reductions`; all-store snapshot plus previous archive | CLI writes/reads but does not consume this as a cache. Standalone `pg_reduction_check_*` tests recompute it. Remove this Program retention; keep live evaluation and explicitly needed evidence APIs. |
| `synthesis_work.c`: ready order, wait edges, private owner state, result; `synthesis.steps` | One charged step dispatches one ready worker. Source import recreates work, not these continuations. Saving only producer recipes cannot preserve this progress. |
| Typing queries/actions, substitution and conversion; WHNF/NF machines | Called by private synthesis workers. Some have existing inert codecs, others do not. A codec roundtrip alone does not establish provenance or reconnect them to the owner. Still to inventory for AP1.3. |
| Hash buckets, lookup caches, transport scratch arrays, old archive generations | Rebuild or discard. Neither pointer bucket iteration order nor elapsed history is a semantic root order. |

**Fuel constraint:** the current unit is a scheduler dispatch, not wall time or
one Core beta step. Preserving exact progress requires its unfinished owner
continuation (including nested work), not merely a saved total counter. Import
revalidation must remain explicit and budgeted; how it composes with that
continuation is still an AP1.3 task. No restored `DONE` bit will authorize evidence.

**Resolved fuel/trust direction (2026-09-29):** use total budget B and validation
sublimit R: actual validation v <= min(B,R), and v + useful progress <= B.
User-authorized trust/reuse may avoid v; it must be explicitly selected. Reaching
R in strict mode leaves validation pending, not silently trusted. No separate
replay engine or uncharged verification. Trust must retain provenance and module
obligations; it cannot be implemented as importing an arbitrary DONE flag.

Agent assessment: a hash in the same file establishes neither proof validity nor
prior trusted validation. Hash reuse needs a trusted prior record and must cover
the semantic dependency closure and checker/Oracle assumptions. Imported
assumptions must not be relabeled as fresh kernel proofs. Signing/blockchain is
not required for this work. History is accounting, not semantic graph identity;
inert load/save must append nothing. At equal useful progress, semantic content
must agree; total-fuel comparisons additionally report validation spent. The
existing loss of unfinished work is still a defect, not an accounting difference.

Implementation increment after `6598d76` (2026-09-29, agent decision within
the resolved budget policy): `artifact/file.c:pg_artifact_revalidate` drives the
same Solve owner with at most `min(B,R)` dispatches and reports actual spend v.
It stops at the target's terminal status or an empty ready queue; additional
work receives at most B-v. Zero fuel calls no worker. There is no imported
evidence admission, new validation engine or failure-to-trust fallback.
The C adapter exposes `--revalidate-limit`; its current validation phase includes
whole-module/entry reconstruction, because exact frontier restoration is still
missing. The source CLI, saved validation history and hash/provenance reuse are
not silently claimed implemented by this local policy entry point.
Fresh verification: O2 and ASan/UBSan semantic/CLI gates pass for B=0, R=0,
B<R, R<B, explicit trust, invalid limits, foreign-owner rejection, unchanged
zero-fuel bytes, terminal checks costing zero, and subsequent work charged from
B-v. Standalone QuickSort still passes in both admission modes (O2). These runs
use clean `e716232` plus the existing prototypes, excluding unrelated local
Evidence/IADT edits. Full compiler acceptance was not rerun for this increment.
Delta, excluding docs: `artifact/file.c` +19/-0, `artifact/file.h` +11/-0,
`c_backend/main.c` +9/-6; tests `semantic_test.c` +60/-0 and `check.sh` +14/-0.
AP1/AP2 remain open: a dispatch cap does not supply any missing continuation.

#### Pending Owners (2026-09-29)

Further code inspection against `e716232`, excluding unrelated local edits:

| Owner | Current continuation that recipe-only import loses |
| --- | --- |
| `synthesis_work.c` | Ready order and waiter dependency/preparation flags. `inputs[]` is heterogeneous; serializing its pointer array is not a codec. `result` points to checked evidence, not an importable acceptance bit. |
| `synthesis.c` | Source stage and child producers; definition indexing/activation cursors; block scope/frame/tail; application specialization; Match branch/motive scans, generalization and indexed-boundary cursors. Existing allocation export preserves identity, not this progress. |
| `synthesis_binding.c`, `synthesis_schema.c` | Pending domain/normalization, telescope tail/scope/allocation, constructor registration/check cursors and member producers. Name hash tables can be rebuilt from these semantic entries. |
| `synthesis_function.c`, `synthesis_cbpv.c` | Provisional structures, pending substitution/comparison, result-context work, and Return/Thunk/sequence stages. Raw structure is not a checked result. |
| `synthesis_context.c`, `typing.c` | Substitution's built prefix and next image; occurrence action's next Core/classifier/annotation component and nested substitution; input-map/scope stacks and context-lift cursor/fresh binder. Completed typed graphs do not encode these cursors. |
| `synthesis_derivation.c`, `synthesis_conversion.c` | Next premise; endpoint-comparison stage; normalization owner; current checking term/type. Existing rule inputs retain obligations but restart these workers. |
| `synthesis_handler.c`, `synthesis_operation.c`, `synthesis_effect.c` | Clause scan/collection and open-contribution count; allocated label/binders; pending rule, equation solve/substitution and row-child dependencies. Definition serialization does not preserve the solve queue. |
| `synthesis_iadt.c`, `synthesis_identity.c` | Field/index/endpoint cursors, checked map prefixes, normalization/comparison, typed queries, and formation/face workers. Persisting only the final family object cannot resume them. |
| `typed_query.c` and rule owners | Exact role/input key, dependency stack and private partial construction. Roles are static C descriptors, not portable object identities; checked results need their normal kernel justification. |
| `computation_io.h`, `eval_io.h`, `identity_io.h` | Existing raw state codecs cover parts of nested work. They explicitly do not certify provenance or reconnect a saved state to a source producer; do not mistake codec existence for checked resumption. |

Assessment: copying all arena state would retain dead memoization and machine
pointers; importing only counters would discard work. The required closure starts
at selected results and their unfinished producers, follows live continuations
and necessary checked-premise inputs, and excludes unrelated completed workers.
Fresh acceptance must still be obtained through the existing kernel rules; an
explicit trust import must remain distinguishable from that acceptance. A proposed
continuation codec must specify its exact input link and validation before it can
replace recipe-only reconstruction. Do not add another general replay engine or
mark AP1.2/AP1.3 complete from this ownership table alone.

The focused prototype now exports a checked root's existing derivation inputs
and typed subject into one shared payload, destroys the source Program, then
checks those inputs in a bare graph/typing/Solve owner. No source scope or
intrinsic installation is reconstructed. Universe, Lambda, constructor and
recursive Match/IH cases return the exact imported typed subject in 9, 69, 304
and 930 charged steps respectively (O2 and ASan/UBSan pass). An unrelated saved
typed root remains unaccepted. This demonstrates reuse of existing kernel rules,
not a new replay engine, free validation or integrated source resumption.
With the current dispatch-based fuel, even checking retained inputs consumes
positive fuel. The resolved accounting contract remains separate from whether
those inputs and unfinished owner state can be transported.

### Plan

- [x] **AP1.1:** fetch #43/#44/#45 and import the three PR Markdown files unchanged.
- [ ] **AP1.2:** inventory each persisted field by owner, reader/consumer and
  semantic root. Classify as required, recomputable, or dead. Include typed
  queries, effect equations and reduction consumers; do not infer usefulness
  from a serializer roundtrip test alone.
  Image-payload inventory is above; private pending-owner continuation coverage
  and the occurrence-versus-rule validation-input analysis are still open.
- [ ] **AP1.3:** specify one export/root contract using existing occurrences,
  declarations and residual inputs. Demonstrate parse-only, partly materialized,
  completed and rejected examples. Specify how a saved result reconnects to its
  exact source producer without rediscovering it by unbounded search.
  Define the fuel unit and required current continuation so that splitting a
  budget does not lose charged progress. Inventory queues/cursors individually;
  their implementation layout is not a wire contract, but blanket deletion of
  every continuation is not justified by the history-growth finding.
  Completed term evidence alone is not completed source-module evidence:
  `synthesis_derivation.c:258` exports the term's proof, whereas definition
  `definitions_step` in `synthesis.c:2535` waits for siblings and `::` after
  registration/activation (`synthesis.c:2479`) finishes indexing names.
  Any replacement of a source producer must preserve those obligations and its
  namespace/polarity outputs, not forward only a valid selected term's proof.
- [x] **AP1.4:** current-format-only policy, explicitly requested by the user.
  Prototype writes/reads APGSRC68/APGRET4, with APGOCC8/APGSCP2 typed payloads.
  Earlier formats reject and must be rebuilt from source. Remove compatibility
  branches, reserved legacy archive fields, migration APIs/tests and old-version
  build dependencies. Existing seed tests check rejection of every older header.
  Focused typed-payload tests also reject earlier and unknown occurrence/scoped
  headers without publishing roots or allocating Terms (O2 and ASan/UBSan pass).
  The semantic payload test rejects APGRET versions 0-3 and 5 with unchanged
  outputs and no new Terms/proofs; version 4 succeeds (O2 and ASan/UBSan pass).
  Promotion and complete regression coverage remain separate requirements.

## AP2. Shared Transport and Removal

### Subjective (User)

Retain the separation of computational Core and typed structure, with a single
owner for each concept. Avoid duplicated reconstruction machinery, unnecessary
boundaries and broad retention just to support eventual resumption.

### Objective (Code)

`graph_io.h` already provides an enclosing image with a shared Term/object table.
`context_io.h`, `context_payload.h`, `occurrence_io.h`, `derivation_io.h` and
`declaration_io.h` provide overlapping pieces, not a complete semantic Program
image. `program->retained_reductions` currently owns the raw history on read.
The growth audit also reproduces temporary Ref-wrapper duplication; this is a
physical encoding issue, not evidence that nominal objects should be merged.

### Assessment

Extend the existing transport owners instead of building a backend-specific
serializer. A source adapter may inspect Program/Synthesis to select available
roots. The resulting semantic view must not expose parser nodes or scheduler
handles as its public ABI. Source continuation inputs can remain opaque to that
view; the existing elaborator remains their consumer.

Do not persist both a full source-derived answer graph and a disconnected export
copy. Both references must reach the same materialized nodes. Memo tables remain
in-memory optimizations unless an explicit consumer proves a need for transport.
If a necessary derivation demands a reduction endpoint, keep its dependencies;
this is not permission to save all evaluator jobs.

### Plan

- [ ] **AP2.1:** extend scoped/unscoped occurrence I/O to the existing full
  descriptor codec path. Keep name/resolve APIs as thin adapters only if still
  used. Test nominal declarations and shared binders without source reconstruction.
  Prototype implementation and targeted O2 tests pass: descriptor APIs delegate
  to the same occurrence/context codec; used name/resolve calls are adapters.
  Promotion and final acceptance coverage remain open.
- [ ] **AP2.2:** share root collection and relocation across occurrences,
  contexts, declarations, validation inputs and residual source obligations.
  Reuse `pg_graph_image_*` / `pg_graph_dependencies_init`; fix Ref transport
  duplication without copying or normalizing the borrowed semantic graph.
  Prototype fixes temporary Ref aliases by their identical object pointer in
  the shared dependency collector. Nominal objects and Lambda/Application nodes
  remain distinct by pointer. The prototype now connects available typed roots
  to their existing producer ordinals in that same table, without erased-Core lookup.
  Clean-baseline source, derivation, occurrence and scoped-structure tests pass;
  the added test also passes ASan/UBSan. Completed Example 09: retained bytes
  32,625 (prior baseline) -> 32,463, no duplicate Ref records; ordinary 26,374
  unchanged. Fuel-partition resumption still fails; this is not the history fix.
- [ ] **AP2.3:** implement an inert semantic-root view plus persistence of the
  available construction and residual inputs. Inspecting an absent result returns
  unavailable, not an implicit Solve request. Keep existing module acceptance rules.
  Prototype view borrows `pg_occurrence` directly, without another semantic DB.
  Imported construction is an unaccepted producer input, not a restored result
  receipt. Ordinary Solve still checks its source independently. Validation-input
  reuse and suspended-owner continuation remain open; this is not a checkpoint.
- [ ] **AP2.4:** remove all-store snapshot/previous-generation accumulation and
  dead Program ownership after AP1's consumer inventory. Review `main.c`,
  `program.h`, `source_io.*`, `retained_io.*`, `reduction_io.c`, `eval_internal.h`
  and `computation_io.h`. Preserve live evaluator memoization and required
  evidence APIs. Retire `--retain-reductions` explicitly rather than as a no-op.
  Prototype removes the snapshot API and Program archive field; CLI/REPL now
  have one writer. The retired option errors without creating an image.
  Explicit reduction records/checking and live memoization remain. Source,
  normalization, identity I/O, CLI images and focused ASan/UBSan pass; promotion
  remains open. Full acceptance now passes on the combined candidate below.
- [ ] **AP2.5:** use the current format only; update CLI/REPL writers, docs and
  tests. Older inputs reject, rather than taking a migration path. Same-version
  zero-fuel saving must preserve bytes. No automatic reader-limit escalation.

Implementation location before acceptance: `src/prototype/artifact_persistence/`.
Approved changes can later move to their existing artifact/semantic owners.
The future C implementation is planned under `src/transpile/c/`, separate from
those owners; no transpiler directory, emitter or target IR is needed in this phase.

#### Prototype Decisions And Checks (2026-09-28)

Compared with `f2f6f69`, new patches modify only existing source, synthesis-work
and retained-I/O owners. The reader reconnects results by producer ordinal;
selected roots and their explicit producer dependencies determine retained
results. Lexical-origin discovery supplies allocation inputs, not more exports.
The typed DAG's own closure supplies its Terms, Contexts, maps and annotations.

Rejected trials: traversing every typed/source backreference retained incidental
workers and grew Example 09 by about 12.7 KB per completed generation. Seeding
only materialized Core still retained every constructor-use Context receipt;
an extra member index did not fix that. Both broad traversal and that index were
removed. Existing constructor field addresses already identify the exact binders.
Use those addresses for default fields; retain explicit roots/non-default
allocations separately. Match allocations still need their existing source
origins. No Context, nominal declaration or binder is merged by alpha equality.
Also rejected: treating `pg_evidence_scope(term_proof)` as its term environment;
that API selects Context declarations only, not arbitrary term contexts.

Artifact-only prototype on top of `5eaf3b7`, freshly applied to clean `e716232`:
`check-artifact-history`, `check-artifact-transport` and seed tests pass. They include
source/normalization, explicit reduction checking, CLI/REPL images and fuel curves.
Focused semantic/source tests also pass ASan/UBSan. A test adds 32 unrelated live
evaluation jobs and requires unchanged saved bytes: caches are not image roots.
Read-only inspection still cannot call synthesis/evaluation, and forged roots or
invalid module siblings are not accepted. Exact saved-versus-rechecked Core is
captured before Solve so the test cannot compare a fresh result with itself.

Example 09: 25,234 bytes at step 0; 25,670 at step 100; 50,472 when complete.
Three inert generations at each fuel and three completed generations are byte
identical. The old archive's +444 bytes/generation path no longer exists.
This retains more typed structure than the former source-only image; it is not
a claim that total image size always shrinks. Reports and reproducible gates are
documented in the prototype README. The user's no-compatibility decision removes
the earlier successful migration experiment from the implementation/test suite.

Full acceptance exposed another encoding redundancy: optional occurrence
annotations/classifiers were written as Core placeholders even when absent.
General Sorted retained 263,420 occurrences but only 2,673 annotations; its
Context section exceeded the unchanged one-million-unit bound. APGOCC8/APGSCP2
omit absent reference slots. In the isolated reproducer this saves 2,085,976
bytes (31,467,832 -> 29,381,856) and restores ordinary-result comparison under
the original bound. No proof, field or classifier was erased to fit the limit.
The small transport test checks omitted slots directly and inert roundtrips.
Full acceptance also found a stale seed version assertion (updated) and a
remaining read failure for assertion-free finite permutation views. Full
acceptance/F3/F4 and promotion remain open; do not claim the large case fixed
from the small transport tests or silently increase the reader limit.
Fresh current-format reproduction: compilation completes in 1,475,909 steps;
the 20,875,920-byte image fails step-0 import. The shared Core reader encounters
214,216 objects + 814,001 Terms + 23,963 root references = 1,052,180 units, above
its 1,000,000-unit bound, before reading typed payloads. This is not backward
compatibility. Investigate retained dependency growth under AP1.2/AP2.3 rather
than treating a larger limit as the persistence fix.

**2026-09-29 combined candidate:** `candidate.sh` composes the artifact patches
with the existing readback-sharing and Fold-comparison prototypes, without
copying their implementations into a second owner. Full `check-acceptance`
passes, including the previously failing assertion-free Fin case. This run
precedes the effect-order fix below. Readback sharing alone reduces that Fin
case to 927,681 compile steps and 20,091,101 bytes; its zero-step import passes
the unchanged bound. Sharing typed substitution indiscriminately is not an
equivalent optimization: destination-context binders must remain distinct.

Before the byte-bound prototype below, the separate F3/F4 sorting gate fails.
The full QuickSort case builds
in 5,811,051 steps and writes 32,372,583 bytes; 284,806 objects, 1,062,813 Terms
and 46,350 root references exceed the same reader bound. Its selected Core closure
has only 30,716 Terms; typed headers retain a closure of 1,061,082 Terms and
178,600 typed nodes. MergeSort also fails image loading after successful source
construction. A larger diagnostic API bound allows measurement and exact inert
resave, but is not the CLI policy or a passing regression workaround.

`check-artifact-metrics` now counts Core/typed roots, unique reachable Terms and
objects, typed nodes, rule-premise edges and section bytes. It compares each
zero-step generation with its source image and forbids synthesis, reduction,
substitution and effect solving during inspection. Focused ASan/UBSan passes.
Current-continuation/residual-owner coverage is still needed to complete AP3.2.

**Reproduced and fixed in prototype:** effect definitions were enumerated in
pointer-hash bucket order, changing saved bytes after relocation without Solve.
`effect_inference` now links its existing equation/edge nodes in registration
order; it does not copy constraints or alter solver adjacency/queue ordering.
The new test fails before the fix and passes afterward: 12 inert generations,
duplicate edges/constant aliases, and unchanged definitions across partial/full
effect solving. Focused semantic ASan/UBSan, Core and derivation-I/O tests pass.
The whole acceptance run must be repeated on the eventual final candidate.

A further deterministic test reverses external operation-label addresses while
keeping their names fixed. Both closed effect rows and Fold clause descriptors
previously changed bytes at zero steps. The prototype now retains the first
effect-set enumeration and source clause order, with private pointer-sorted
membership/dispatch indexes. Lookup identity, set union/difference and clause
positions are unchanged; no pointer ordering is exported. The index is derived,
not a second mutable authority or an additional wire table. This costs one
in-memory pointer/index per label; semantic payload size does not increase.
O2/ASan/UBSan focused transport, semantic, metrics and Core tests pass. Full
`check-acceptance` passes with all three ordering fixes on the clean `e716232`
baseline (exit 0). The separate large F3/F4 and fuel-partition failures remain.
The existing source-handler boundary test now compares inert resave bytes, not
only reconstructed behavior. All 4,483 snapshots across four cases pass (including
nested calls, an invalid resumption, and a forwarded unhandled operation).

Fresh `check-artifact-partitions` after the owner increment below still fails
across reload: 100+100 produces 25,851 versus 26,094 bytes; 1600+1600 is pending
at 3,200 steps versus done at 2,942 (31,392 versus 50,508 bytes). In-memory
partitions and 10+10 versus 20 pass. Completed+0 preserves bytes but does not
restore accepted status. Measurements: `/tmp/a-program-normalization-owner-partitions/partitions.tsv`.
Owner-local increment after `8d4206d` (2026-09-29): the prototype connects a
pending WHNF machine to an unstarted normalization producer, sharing the usual
input preparation and continuing through ordinary Solve. It creates no second
evaluator, imported evidence or completed-status override. The internal attach
API requires caller-established provenance; checked input/key agreement alone
does **not** validate an imported intermediate machine. Default source loading
does not call it or gain trust. NF and source-owner stages remain absent.

Fresh O2 and ASan/UBSan tests cover all 300 pending boundaries of nested calls,
an unforced Lambda, recursive Match/IH and host addition. Premise inputs and the
machine share one relocation table. Each test destroys the original Program,
checks premises using existing rules, then resumes its own known-origin state.
Remaining Solve fuel and serialized result match uninterrupted execution;
inert load/resave and step 0 preserve bytes. Foreign owners, wrong inputs/modes,
unresolved premises, duplicate keys and repeat attachment reject. Revalidation
cost is measured separately from the remaining normalization dispatches, not
asserted free. Semantic persistence and CLI file-policy regressions also pass;
O2 C differential gates and Acc QuickSort pass in checked/trusted modes. Full
compiler acceptance was not rerun for this increment. These results use clean
`e716232` plus prototypes, excluding unrelated local Evidence/IADT changes.
Implementation delta: `synthesis_conversion.c` +44/-11, its header +13/-0.

Extension after `035209b`: `artifact/schedule.c` transports ready order and
ordered wait edges, including preparation wakeups, using the enclosing owner's
job ordinals. Decode is inert; attachment is separate, validates the closed
mapping, and publishes only after validation. No role/status/evidence import or
dispatch takes place. Existing scheduler links remain the sole live authority.
Normalization attachment now permits several owners to share the exact pending
WHNF instead of copying it or rejecting its second consumer. A five-job fixture
checks the input premises, restores shared reduction plus ready/wait structure,
and completes in exactly the uninterrupted run's remaining 77 dispatches.
O2/ASan/UBSan also pass the existing 300 boundaries, preparation/wake ordering,
truncated/invalid metadata, failed-attachment atomicity and a dormant wait cycle.
The `.a` source loader is **not** connected to this partial checkpoint yet:
it lacks the role-owned source continuation payloads, not just the queue.
The whole-source partition gate was rerun and still fails at 100+100 and later
partitions; semantic persistence, CLI file-policy, C differential and Acc
QuickSort gates pass. Full acceptance has not been rerun for this extension.
Baseline remains clean `e716232` plus prototypes; unrelated Evidence/IADT
changes are excluded. The new transport is 184 lines in `artifact/schedule.c`
and 27 in its header. Existing runtime changes only move the waiter declaration
to its internal header (+9/-1 there, -7 in `synthesis_work.c`) and permit shared
WHNF attachment (+3/-1 in `synthesis_conversion.c`, +1 header line). This is
additional checkpoint functionality, not a claimed code-size reduction.

Source-owner increments after `4251885` and `21244ae` (2026-09-29): a borrowed definition
registration frontier retains existing entry links and indexing/activation
cursors. Its owner validates source identities, rebuilds only the name hash
index and restores fresh definition scope/activation gates. It does not accept
entry evidence, advance Solve, or copy typed data. Repeated
imports keep the first name's producer; shared definition recipes remain shared.
Invalid attachments do not publish cursor/index changes. The checkpoint caller
must establish provenance and restore child continuations and scheduling before
Solve. Completed name registration additionally requires a fully populated input
table and completed outer registration. APGSRC68 removes the former checked
Context prerequisite from name publication; entries still check it. This is not acceptance
of the module or its entries. Pending definition-body links and module traversal
positions now have owner-local attachment APIs. They cannot skip an unchecked or
failed earlier sibling; named selection still waits for the entire module.
Scheduler replacement tolerates unpublished startup subscriptions to a request
that an owner has already completed. Saved ready/wait endpoints remain pending
and disjoint; replacement does not import acceptance.

Fresh O2/ASan/UBSan tests pass 44 source frontiers: forward names, shared syntax,
ADT/function use, invalid unselected siblings/`::`, duplicate names, unsupported
imports and dormant cycles. Each destroys the original Program, preserves
load/resave and step-0 bytes, then matches remaining dispatches and final source
image bytes. A 64-definition registration gives identical checkpoint bytes for
1+19/10+10/20+0 and 20. This is a test-only composition of existing syntax and
schedule codecs, not a public `.a` checkpoint format. Separate link tests verify
repeated-import sharing and reject a cursor that skips duplicate assignments.
Existing 300-boundary WHNF/scheduler tests pass with sanitizers; semantic, Source
I/O, CLI policy, C differential and checked/trusted Acc QuickSort gates pass.
Results use clean `e716232` plus prototypes, excluding unrelated Evidence/IADT
edits. Full acceptance was not rerun. Implementation delta: `synthesis.c`
+103/-4, `synthesis_source.h` +16/-0, `artifact/schedule.c` +11/-7; the focused
fixture originally added 357 lines. This is continuation support, not a code-reduction claim.

Current focused verification after `21244ae`: O2 and ASan/UBSan pass the 44
registration frontiers and 16 later owner frontiers (8 body edges, 22 module
cursors). The latter explicitly recheck child work through ordinary Solve: 387
dispatches across the fixtures, charged within each test's total 100,000 budget.
Remaining continuation dispatches and final bytes then match the original.
Decode itself stays inert; the test helper's subsequent validation is **not**
part of loading or evidence imported from saved status. This does not preserve
arbitrary in-progress child state or prove equal total fuel after strict checks.
The 1+19/10+10/20+0 equality remains a registration-only, zero-validation test.
New negative tests reject foreign body owners, incomplete registration completion,
skipped pending/rejected siblings and terminal endpoints in a saved schedule.
WHNF/scheduler, semantic, Source I/O, CLI policy, C differential and checked/trusted
Acc QuickSort gates pass on clean `e716232` plus the prototypes. Full acceptance
was not rerun. Incremental implementation delta: `synthesis.c` +89/-4,
`synthesis_source.h` +19/-4, schedule C/header +10/-8; focused tests +247/-14.

Next: AP1.2/AP1.3 must retain namespace producers and the other live child-owner
continuations, then connect them to `.a` under AP1's provenance/fuel policy.
Before the namespace increment below, `check-artifact-namespace-frontier` failed to find a covered late
boundary for `Nat:=@{zero:*;succ:*->*;}; main:=Nat.zero; main::Nat;`: a published
constructor callable remains runnable outside the fixture's module/entry closure.
The program itself compiles. This is missing checkpoint coverage, not an ADT
typing failure; the producer is not disposable history. Preserve its scope,
callable/value child and pending abstracted body rather than dropping the queue
entry. The advanced-body gate below still records the remaining coverage gap.
The normal source loader does not use these hooks. Its fresh partition gate
still fails at 100+100 and later; results are in
`/tmp/a-program-source-lifecycle-final-partitions/partitions.tsv`. These focused
owner gates do not satisfy the whole-source checkpoint completion criterion.

Constructor increment after `44170b8` (2026-09-29, agent implementation decision):
the pending value owner can borrow/reuse its **completed checked field map**,
without retaining the completed scope worker. Attachment checks nominal fields,
parameter context and exact variable images (a constant-field substitution is not
a namespace callable), creates no Terms/proofs and leaves the value pending.
Ordinary Solve still checks Self/parameter agreement and constructs the callable.
This applies before body abstraction starts; provenance and lexical interface
metadata remain the enclosing owner's responsibilities. It is not raw evidence
admission or a replacement for unfinished scope/body continuation transport.
O2/ASan/UBSan pass six fresh-owner, original-Program-destroyed tests: 0/1/2 fields,
with and without a type parameter. Existing rule checking spends 227/263/299 and
349/385/421 fuel respectively; continuation then takes exactly 1/16/28, matching
the original and its Core bytes within one total budget. Inert decode/resave,
step 0, wrong owners/constructors, constant fields and late rewind are covered.
Definition/WHNF/semantic, Source I/O, CLI policy, C differential and Acc QuickSort
also pass; full acceptance was not rerun. Actual incremental C/header: +49/+12,
no removals; fixture: 239 lines. Normal source import is still unconnected; both
namespace and full partition gates still fail on this candidate.

Further inspected consumer, addressed after `fe9c3d0` (2026-09-29, agent decision):
`induction_scope_step` read the completed constructor worker's private `fields[]`
solely to inspect declaration field types. The prototype now indexes existing
checked context-prefix proofs once per schema/constructor. Constructor and
induction owners use the same constant-time accessor; the worker's field array
and count are removed. This is a derived, immutable pointer view, not another
field/type authority or wire payload. Scope results and unfinished work still
need transport; this change alone does not connect the full source checkpoint.
Focused constructor, IADT, Synthesis, semantic image, Source I/O and policy tests
pass. ASan/UBSan also pass IADT, Synthesis and the six constructor cases, including
the new order/bounds/foreign-label checks and allocation-free repeated lookup.
List example 09 produces exactly the same 50,508-byte `.a`
and 2,824 Solve steps as `fe9c3d0`. General QuickSort with the existing proof
provider also matches byte-for-byte: 14,777,063 bytes, 679,228 steps. A single
concurrent-load sample gives before/after peak RSS 268,428/268,776 KiB and elapsed
1.216/1.230 seconds; no memory/speed improvement is claimed from that sample.
C differential/Oracle and standalone Acc QuickSort gates pass. Full
`check-acceptance` passes on clean `e716232` plus these prototypes, including
both LT providers/partition orders; unrelated working-tree edits are excluded.
Log: `/tmp/a-program-schema-field-order-acceptance.log`. The source-definition
checkpoint gate also passes; the namespace frontier still fails at cut 126.
The strict partition gate still fails at 100+100 and later; see
`/tmp/a-program-schema-field-order-partitions/partitions.tsv`. Neither failure
is reclassified as an expected pass or evidence of full checkpoint completion.
Actual C/header changes: `iadt.c` +31/-6, `iadt.h` +6/-0,
`synthesis_iadt.c` +10/-12 (net +29); fixture +28. No accepted source is modified.

Namespace increment after `b15e40a` (2026-09-29, agent implementation decision):
the source-owner fixture now shares nominal/source relocation with the existing
checked-map inputs and reconnects a published, unfinished constructor to the
module cursor and saved queue. No DONE flag grants evidence: ordinary Solve
rechecks the completed source entries/lexical metadata and map premises first.
The original Program is destroyed. Inert decode/resave and step 0 are byte-stable;
after explicit validation, both the partial checkpoint and final `.a` are exact,
and the remaining 17 dispatches agree. The shared allocation entry point also
avoids creating a second field worker when the checked map supplies the same
addresses. This removes 9 redundant validation dispatches in this fixture
(293 -> 284), not 9 continuation steps or a claimed general speedup.
O2 and ASan/UBSan pass this namespace boundary, 44 registration/16 module
frontiers and all six constructor cases. Repeated allocation attachment creates
no additional jobs; conflicting field addresses reject. Synthesis, IADT,
semantic image and Source I/O tests also pass on clean `e716232` plus these
prototypes, excluding unrelated working-tree changes. Logs:
`/tmp/a-program-namespace-check-final.log`, `/tmp/a-program-namespace-asan.log`.
C differential/Oracle, checked/trusted standalone Acc QuickSort and CLI policy
also pass (`/tmp/a-program-namespace-c.log`). List 09 still matches the preceding
candidate byte-for-byte (50,508 bytes, 2,824 Solve steps).
At `331d5f2`, the next failing gate was `check-artifact-namespace-body-frontier`:
declaring `succ` before `zero` reaches an already-started constructor body at
the module boundary. Body/Classifier/Pi/Lambda work was not transported. These
tests still use a known-origin envelope; the public source loader remains
recipe-only for progress. The strict full-source partition gate still fails at
100+100 and later (`/tmp/a-program-namespace-partitions/partitions.tsv`).
Incremental implementation: `synthesis.c` +17/-14; focused tests +177/-1;
build targets +6/-1. Full compiler acceptance
has not been rerun for this small increment; the previous full pass is above.

Started-body increment after `331d5f2` (2026-09-29, agent implementation):
the constructor owner reconnects its checked constructor leaf through the
existing abstraction factory. Body adapter selection and implicit-index callable
metadata share the ordinary factories. The derivation owner restores its next
premise only when every skipped premise is locally checked; active endpoint
conversion/reduction is explicitly outside this cursor interface. No attachment
admits a conclusion or runs Solve. Scheduler order remains separately owned.

Fresh O2 and ASan/UBSan pass 70 started-body boundaries (Nat 16, binary Tree 27,
implicit-index Vec 27), including pending children after module checking has
finished. Each case destroys the original Program, decodes/resaves inertly,
checks step-0 byte stability, revalidates retained premises through ordinary
Solve within one 100,000-step budget, and reproduces the partial checkpoint,
remaining dispatches and final `.a` byte-for-byte. Foreign attachments,
out-of-range premise cursors, skipped pending premises and late rewind reject;
type/value/raw-computation body selection is covered. Six checked-field cases,
44 registration and 16 source-owner frontiers still pass. Logs:
`/tmp/a-program-body-focused.log`, `/tmp/a-program-body-asan.log`.

This closes the started-body fixture gate, not arbitrary source checkpointing.
Unfinished field construction, classifier queries without a retained structural
type, other source/typing owners, and public checkpoint/history/provenance
integration remain open. The strict public partition gate still fails at
100+100 and later (`/tmp/a-program-body-partitions/partitions.tsv`). The fixture
envelope is not adopted as a public file format. Full acceptance and C regression
pass independently of that missing public checkpoint gate.
Semantic image, WHNF/scheduler checkpoint, CLI policy, 21 C differential cases,
C Oracle and checked/trusted standalone Acc QuickSort pass
(`/tmp/a-program-body-artifact.log`, `/tmp/a-program-body-policy.log`,
`/tmp/a-program-body-c.log`). List 09 matches `331d5f2` exactly: 2,824 steps and
50,508 bytes. Full `check-acceptance` passes, including general Sorted,
derived-LT providers/partition orders, indexed constructor sequencing, Identity,
source images and witness isolation (`/tmp/a-program-body-acceptance.log`).
All checks use clean accepted `e716232` (unchanged through `331d5f2`) plus these
prototype overlays; unrelated working-tree edits are excluded.
Actual C/header delta from `331d5f2`: `synthesis.c` +53/-6,
`synthesis_cbpv.c` +28/-7, `synthesis_derivation.c` +26/-0,
`synthesis_source.h` +17/-0 (net +111). The extended fixture is +279/-40;
build-target comments +1/-1. Patch context lines are not implementation growth.

#### Rule Relocation and Preparation (2026-09-29)

Historical increments `aa4a21a` and `75b1f60` added imported-input cursor/edge
accessors and the artifact-owned derivation codec. Their 190 known-origin
boundaries pass O2/ASan/UBSan, including shared rules, charged validation and
rejection of false completion. They are not public source checkpoints. The
earlier versions of this plan at those commits retain per-file deltas and logs.
One enclosing artifact owns its schedule; duplicating it inside each owner
payload was rejected because it prevents composition with other pending owners.

Inspection at `75b1f60`: source import replaces every saved rule DAG root with a
lazy `DERIVATION_INPUT_JOB`; those workers later assemble ordinary canonical
rules. At List-09 seed +100 this produces 28 pending input wrappers, alongside
11 plain rules and 13 other pending workers. This preparation exists because of
the import representation, not because rule formation requires type checking.

Agent revision: `pg_sources_read` now relocates the complete stored DAG directly
through `pg_synthesis_import_rules` into the existing `pg_synthesis_rule` factory.
One iterative, shared dependency walk assembles pointers; ordinary Solve still
checks every rule. No normalization, effect solving, stored-DONE admission or
new checker runs during loading. The lazy API remains for callers explicitly
requesting fuel-accounted input expansion, but the public source importer no
longer creates those wrappers. Do not serialize an unnecessary import-only
preparation machine merely because its isolated codec was implemented first.
This is not a change to which judgements are accepted or a claim of free checking.

Verification: source-I/O tests, semantic zero-step cycles, CLI read policy,
21 C differentials, Oracle tests and checked/trusted standalone Acc QuickSort
pass. Direct import of a depth-2048 shared DAG creates only canonical rule jobs,
preserves aliases, produces no evidence, rejects cycles, and leaves an invalid
RETURN rule for ordinary Kernel rejection. Semantic tests forbid Solve, WHNF,
substitution and effect solving during load. All 0..200-cut inert cycles plus
the completed case pass O2 and ASan/UBSan. Full `check-acceptance` exits 0,
including both LT providers/partition orders, general Sorted/permutation,
QuickSort/MergeSort image consumers and invalid-evidence controls. Logs:
`/tmp/a-program-direct-rule-import-acceptance.log`, `-c.log`, `-asan.log`, and
`/tmp/a-program-direct-rule-reproduced-semantic.log` (fresh patch application).

The namespace fixture initially dropped already-created premise jobs when
installing its validation queue. Unlike lazy wrappers, canonical requests do
not recreate missing queue entries. The fixture now selects the validation
premise closure through the existing DAG walker, preserving its isolation from
unfinished constructor bodies. Remaining dispatches and final images still
match at all 70 started-body boundaries (O2/ASan/UBSan). Definition, constructor,
WHNF/scheduler and explicit lazy-input owner gates also pass. Logs:
`/tmp/a-program-direct-rule-import-owners-fixed.log`, `-namespace-asan.log`.

List-09 loaded-seed completion decreases from 2,942 to 2,824 dispatches; fresh
source remains 2,824. Completed output remains 50,508 bytes. Public split-fuel
comparison still fails at 100+100 and later; terminal+0 retains exact bytes but
remains unaccepted under strict import. See
`/tmp/a-program-direct-rule-import-partitions/partitions.tsv`. Next preserve
canonical rule and source/typing-owner continuations in shared source relocation,
with the existing schedule and explicit validation/provenance policy. AP1-AP3
remain open; removing redundant preparation is not exact resume.

Delta from `75b1f60`, excluding patch context: `synthesis_derivation.c` +51/-0,
`synthesis.h` +7/-0, `source_io.c` +2/-7, `source_io.h` +6/-1 (net +58).
Tests: `semantic_test.c` +71/-1, `definition_checkpoint_test.c` +24/-1
(net +93). Accepted sources and unrelated working-tree edits are unchanged;
all candidates use clean `e716232` plus these prototype overlays.

**2026-09-30, direct-rule checkpoint increment after `cbe6395`:** the owner
codec still accepted only lazy imported-input roots, although the source loader
now constructs plain rules. It now also captures a closed DAG of direct rules,
using the existing shared rule exporter/importer and one common cursor encoder.
No lazy adapter is inserted, and no ready queue or proof authority is duplicated.
Internal owner metadata is APGDRC2; public APGSRC68 and all backend formats stay
unchanged. Mixed source/evidence owners, open effect parameters and started
comparison/reduction state still reject rather than being silently recomputed.

Fresh O2/ASan/UBSan: 281 cuts, including 91 direct-rule cuts, preserve aliases,
inert bytes, remaining dispatches and final proofs after destroying the original
Program. Direct and lazy routes produce identical final proof bytes. Rechecking
saved completed premises uses ordinary Solve and is charged; false completion
still rejects. The new direct-root regression fails on `cbe6395`. Existing
definition, WHNF/scheduler and semantic-image gates pass on clean `e716232` plus
the prototypes; full acceptance is not rerun for this isolated codec change.
Logs: `/tmp/a-program-direct-rule-checkpoint-{before,final,final-asan,suite}.log`.
Implementation/header delta: +109/-33; tests: +79/-22. This expands continuation
coverage, not the claimed completion scope. AP1-AP3 remain open. Next compose
plain rules with source-owned and accepted-evidence premises through one shared
mapping and explicit provenance policy; replacing such premises by newly
constructed rule requests changes identity/fuel and is not an acceptable shortcut.

**2026-09-30, mixed-premise increment after `08a3f3b`:**

- **Subjective (User):** preserve A Program's own progress; do not extend `.a`
  with Transpiler/Linker responsibilities (AP6 boundary remains unchanged).
- **Objective (Code):** the closed-rule exporter cannot preserve pending source
  premises and replaces terminal evidence workers with different rule requests.
  Both change the actual dependency graph used to account for fuel.
- **Assessment:** keep exact premise-job references. The direct-rule checkpoint
  now projects borrowed rule headers and mapped edges, instead of exporting and
  reimporting a reconstructed proof DAG. External jobs belong to their original
  owner; this payload stores neither their status nor a second continuation.
  A header-only envelope shares the existing parameter/Core codec and cannot be
  decoded as a complete proof DAG. Internal metadata is APGDRC3; public APGSRC68,
  full derivation APGDRV16 and backend formats are unchanged.
- **Plan/status:** O2/ASan/UBSan pass the previous 281 cuts and 15 mixed-owner
  cuts with one schedule. Destroy/reload preserves exact dependencies, inert
  bytes, remaining dispatches and final proof bytes. Distinct producers of the
  same checked fact remain distinct jobs; completed assertions still require
  ordinary budgeted checking. Header/full-DAG confusion, duplicate/foreign
  dependencies, cyclic slot references and missing dependencies reject.
  Existing definition, normalization/schedule, semantic-image, derivation-I/O
  and source-I/O gates pass. Full acceptance was not rerun for this codec-only
  increment; no evaluator or kernel rule changed.
  This establishes owner composition, **not** public source checkpointing:
  arbitrary source-private continuations, open effects, started comparison work
  and provenance-policy integration remain open. Do not close AP1-AP3 from this
  gate. Logs: `/tmp/a-program-mixed-rule-final{,-asan}.log` and
  `/tmp/a-program-mixed-rule-{derivation,source}-io.log`.
  Actual source deltas, excluding patch context: `artifact/derivation.c`
  +139/-60, its header +18/-4, `derivation_io.c` +31/-11, its header +11/-0
  (net +124 implementation lines); checkpoint tests +234/-9. This adds supported
  dependency composition, not a claim of overall code reduction.

**2026-09-30, source-owner composition after `cf28c76`:**

- **Subjective (User):** keep target-only responsibilities out of `.a`.
- **Objective (Code):** `artifact/source.[ch]` transports only definition body
  edges, module traversal cursors and literal/@ rule edges. Normal source I/O
  owns syntax, scopes and name registration; the new payload does not duplicate
  them. Literal preparation shares the ordinary synthesis factory. Completed
  targets still require ordinary checking before cursor/schedule attachment.
- **Assessment:** these continuations serve A Program's own Solve, not C
  generation. Public APGSRC68 and backend formats are unchanged. The test uses
  bounded standalone source/header sections because both codecs require EOF;
  relaxing EOF or adopting independent nominal relocation tables is rejected.
  General integration must share relocation, not promote this fixture envelope.
- **Plan/status:** O2/ASan/UBSan pass 116 complete lifecycle cuts across four
  closed literal modules, including shared rules. Inert resaves, reconstructed
  live snapshots, remaining dispatches and final source images match exactly.
  The 0+0, 10+10, 1+19, 0+20 and 20+0 fragment checks pass; rechecking is counted
  in ordinary Solve steps, not a free second allowance. Guards reject omitted
  rule edges, foreign/duplicate inputs, unsupported syntax and unchecked
  completion. Logs: `/tmp/a-program-source-checkpoint-{final,asan}.log`.
  Full O2 `check-acceptance` and the existing definition, derivation,
  normalization/schedule and semantic-image gates pass on clean `e716232` plus
  the prototypes, excluding unrelated working-tree changes. Logs:
  `/tmp/a-program-source-checkpoint-{suite,c-boundary}.log`.
  LinkerScript/native-enum gates preserve the input artifact. On the identical
  List input, parent/candidate public images match at 0/200/completion steps
  (25,369/26,390/50,820 bytes; completion at 2,824 steps).
  The public partition gate still fails: 100+100 differs by 178 bytes and
  1600+1600 remains pending versus single-run completion. Results:
  `/tmp/a-program-source-checkpoint-partitions/partitions.tsv`. AP1-AP3 remain
  open for general source continuations, shared relocation and provenance.
  Actual implementation delta: `synthesis.c` +50/-18, its header +11/-2,
  `artifact/source.c` +216/-0 and its header +32/-0 (net +289); test +404/-0,
  build +8/-0. This adds continuation coverage, not overall code reduction.

**2026-09-30, shared source/rule transport after `5b3685e`:**

- **Subjective (User):** the preceding downstream-only requirement still applies.
- **Objective (Code):** source I/O owns temporary exported effect rows until its
  retained image finishes writing. Moving the terminal table outside that call
  would outlive those objects. Instead, `pg_sources_*_with` lets continuation
  codecs use the existing retained table before its owner releases storage.
  The checkpoint test's two standalone tables and temporary section files are
  removed. APGRET5 identifies this continued prototype envelope; default public
  APGSRC68/APGRET4 bytes and all backend formats are unchanged.
- **Assessment:** the consumer is Solve's source/rule continuation, not a target
  backend. No C representation, ABI, link setting or acceptance flag is added.
  Readers still enforce the exact enclosing boundary and reject a missing or
  unexpected continuation. Callback outputs remain provisional until success.
- **Plan/status:** O2 and ASan/UBSan pass the 116-cut checkpoint gate and a new
  nominal-sharing case: two distinct same-shape ADTs and a Lambda share exact
  Core references between source results and unaccepted rule parameters,
  survive inert resaving, and pass ordinary source rechecking. Source/identity
  I/O, semantic/history/metrics, definition/derivation checkpoint and derivation
  I/O gates pass. LinkerScript/native-enum tests preserve input images. Logs:
  `/tmp/a-program-source-shared-{suite,asan,derivation,c-boundary}.log`.
  List images match the parent at 0/200/completion (25,369/26,390/50,820 bytes).
  The public partition gate still fails with the same 100+100 and 1600+1600
  discrepancies (`/tmp/a-program-source-shared-partitions/partitions.tsv`).
  General source continuations and provenance remain open; the preceding full
  acceptance result is historical, not rerun for this transport-only increment.
  Actual source deltas: `retained_io.c` +40/-6, header +20/-0; `source_io.c`
  +19/-8, header +18/-0 (net +83). Tests +128/-58; metrics +8/-6; build +1/-1.

**2026-09-30, source-reference continuation after `15ce2de`:**

- **Subjective (User):** keep Transpiler/Linker-only information out of `.a`.
- **Objective (Code):** plain identifiers retain a producer/projection edge or
  a scope-bound VARIABLE rule in `source_work`. The restricted checkpoint lacked
  these links. Reference preparation now shares the ordinary source factory;
  `artifact/source` stores two mapped job links, not another binding/name table.
  Internal metadata becomes APGSRCW2; public APGSRC68 and backend formats stay
  unchanged. No target representation, ABI or link configuration is added.
- **Assessment:** restoring these links is justified by Solve's remaining work.
  Attachment requires the exact lexical producer/binder and checked definition
  producers; it neither accepts saved status flags nor performs computation.
  Qualified/import/namespace continuations remain outside this owner payload.
- **Plan/status:** O2/ASan/UBSan pass 204 complete lifecycle cuts, including
  backward/forward aliases; inert/live images, remaining dispatches and final
  bytes match. Rechecking costs 1,166 ordinary Solve steps across those tests.
  Baseline: clean `e716232` plus the prototype overlays and this delta;
  unrelated Evidence/IADT working-tree changes are excluded.
  Full O2 `check-acceptance`, definition/derivation checkpoints and the
  semantic/history/source/Identity I/O gates pass on that same candidate.
  Guards reject shadowed-binder substitution, wrong producer/rule, foreign
  ownership, unchecked definitions and repeated attachment. Linker/native-enum/
  scalar gates pass, including unchanged images, source graph/proof counts and
  no hidden evaluation in lowering. Parent/candidate List images match exactly
  at 0/200/completion (25,369/26,390/50,820 bytes; 2,824 completion steps).
  The public partition gate remains failing, unchanged; see
  `/tmp/a-program-source-reference-partitions/partitions.tsv`. This is owner
  coverage, not AP1-AP3 completion. Logs: `/tmp/a-program-source-reference-asan.log`,
  `/tmp/a-program-source-reference-suite.log`, `/tmp/a-program-source-reference-c-boundary.log`.
  Actual implementation deltas: `synthesis.c` +55/-13, header +15/-0;
  `artifact/source.c` +27/-4, header +4/-2 (net +82). Tests +92/-1.

#### Rejected Read Policy and Retention Audit (2026-09-29)

The user rejected the agent's byte-derived allowance. That prototype and its
historical passing results (`/tmp/a-program-byte-bounded-*.log`) do not establish
the policy for adoption. In particular, the earlier description of every
reachable typed dependency as "required" was too strong.

Agent implementation decision within the requested fixed-or-unbounded choice:
keep a fixed default of 1,000,000 decoder units, expose `--image-limit N|none`, and
use the same explicit bound for files and pipes. No size-based retry or inferred
allowance. The follow-up after `2a11208` implements `none` as `SIZE_MAX`, after
separating policy quota checks from actual array counts. Four readers previously
rejected a large quota even for a small file. They now use one checked wire-array
allocator; count representability and allocation failure remain enforced.
O2 history/transport/semantic/C gates and focused ASan/UBSan pass, including
sanitized emitted C. File and pipe step-0 roundtrips remain byte-identical; the
32,372,583-byte QuickSort image is also unchanged under the unbounded option.
Invalid modules and zero-fuel C exports remain rejected/pending. This increment
does not rerun the entire acceptance suite or resolve checkpoint/fuel reuse.
The limit is per-payload records/references/name units, not bytes, memory or fuel.
The large sorting tests explicitly select 10,000,000; this is **not compaction**.

Prototype `artifact/file.c` owns file/pipe adaptation and atomic publication;
the codecs still own graph transport. Pipe staging exists because the shared
relocation table needs seeks, not to measure an allowance. Later trust/history
policy belongs here; Kernel and a future backend do not acquire file policies.

Fresh, inert measurements on the 32,372,583-byte QuickSort image:

| Diagnostic closure | Typed nodes | Unique header Term dependency closure |
| --- | ---: | ---: |
| Materialized producer Core roots only | not applicable | 30,716 |
| Operands/maps, omitting type and origin edges | 9,314 | 341,950 |
| Add type edges, omit origin | 25,834 | 344,220 |
| Add origin edges, omit type | 59,481 | 905,258 |
| Full retained typed closure | 178,600 | 1,061,082 |

The last row has 28,784 mapped constructions, 14,616 derived constructions and
198,805 map-image edges. Headers include Core/classifier/annotation; the closure
also follows nominal descriptor dependencies. Context/allocation roots add more
records to the complete image. These diagnostic omissions are not valid codecs
or size-saving claims. Measurements forbid Solve and verify unchanged resave.
Reproduce with `artifact_metrics IMAGE 10000000 --retention`.

Assessment: `source_io.c` retains each reachable producer's materialized result,
not just explicitly selected exports. `occurrence_io.c:operand` then retains
origin/type/map dependencies. Therefore absence of evaluator history does not
mean absence of construction-history retention. `typing.c` input selection and
context action still consume these origins, so blind deletion would lose typed
construction. Separate public result structure, remaining work's exact inputs,
and optional reconstruction/checking aids by **consumer**, not by reachability
alone. Avoid adding a second parallel type graph or serializing all workers.

- [x] Withdraw byte-derived allowance; add explicit fixed limits and localized
  file I/O in the prototype. Tests: exact threshold/larger-bound agreement,
  file/pipe zero-step identity, large-file explicit override, and atomic failed
  replacement. Existing current-format rejection remains unchanged.
- [x] Add reproducible edge-attribution metrics; do not mutate the graph to
  inspect it or call an ablation a valid reduced artifact.
- [ ] AP1.2/AP2 continuation: classify retained producer roots by export, pending
  consumer and verification provenance. Record which can be omitted in a
  recompute profile and which a progress-preserving profile actually consumes.
- [x] Connect the pending WHNF normalization owner to ordinary Solve and test
  exact remaining dispatches at every boundary of the focused cases above.
  `check-artifact-normalization-checkpoint` is separate from, and does not
  replace, the still-failing whole-source partition gate.
- [x] Transport scheduler order/subscriptions separately from acceptance and
  connect it to shared WHNF consumers; check exact resumed dispatches and wakes.
  Source-owned private state and full checkpoint/provenance integration remain open.
- [x] Restore the pending source-definition registration frontier without
  replaying registration work; preserve names, activation gates and sharing.
  `check-artifact-definition-checkpoint` covers this owner and the next item.
- [x] Restore completed name registration, pending body edges and module-check
  cursors without accepting child evidence; test charged child verification and
  rejection of skipped obligations. This is not private child-state restoration.
- [x] Connect the checked-field/pre-body namespace boundary to module restoration;
  `check-artifact-namespace-frontier` preserves the original queue and final image.
- [x] Restore started namespace constructor abstractions through the ordinary
  Body/Classifier/Pi/Lambda workers and premise cursors; pass
  `check-artifact-namespace-body-frontier`, including implicit-index metadata.
- [x] Restore imported derivation preparation and its canonical rule edge,
  preserving shared workers and preparation subscriptions without checking on
  decode; `check-artifact-derivation-checkpoint` is an owner-level gate only.
- [x] Compose source-input transport, definition/module/literal owners, direct
  rules and one schedule for complete closed-literal lifecycles;
  `check-artifact-source-checkpoint` remains a restricted fixture envelope.
- [x] Share source and continuation Core/object relocation in that envelope;
  retain temporary-object lifetimes and exact read boundaries. Check nominal
  separation and shared Lambda binders without granting imported acceptance.
- [x] Restore plain identifier producer/rule edges using the ordinary reference
  factory; test forward aliases across every cut and exact lexical binder guards.
- [ ] After AP0/SE1-SE5, compose Lambda binding/domain annotation and normalization owners
  with that shared closure and schedule. `synthesis_binding.c:domain_step`
  currently creates checked type adapters before its normalization request;
  preserve its semantics without freezing the current wrapper representation
  or running kernel checks during inert decode.
  Do not treat a lexical VARIABLE-owner test as a complete Lambda checkpoint.
- [x] Verify direct source-image rule relocation through ordinary canonical
  factories, without per-input preparation wrappers. Retain byte-identical
  step-0 resaves and ordinary rejection; full acceptance passes for this
  prototype revision. This does not waive remaining continuation/provenance work.
- [x] Align retained entry ownership with the **lexical namespace** before
  restoring its progress. APGSRC67 stores each entry table once, plus each
  producer's prepared namespace edge. The ordinary source owner consumes those
  same links; no synthetic parent export or second name table is created.
  `shared_registration_inputs` fails at `6cb1887` (four entries for two
  selections of a two-entry namespace) and passes with two shared entries.
  Parent-omitted/child-first roots, semantic O2/ASan/UBSan and source/CLI history
  gates pass. This does not yet persist indexed/activated cursors or scheduling.
  Example-09 completed image grows 50,508 -> 50,796 bytes from explicit owner
  edges; do not report whole-image compaction. The public partition gate still
  fails: 100+100 has 26,010 vs 26,366 bytes; 1600+1600 remains pending vs done.
  Fresh full O2 `check-acceptance`, definition/namespace-body checkpoints and
  C/Linker/Acc QuickSort gates also pass (2026-09-29, clean accepted sources plus
  prototype overlays; unrelated working-tree edits excluded). Logs:
  `/tmp/a-program-lexical-registration-{history,asan,acceptance,c,checkpoints}.log`.
  Actual source delta from `6cb1887`: `source_io.c` +51/-21, `source_io.h`
  +3/-1, `synthesis.c` +5/-6, `synthesis.h` +3/-1 (net +33). Tests:
  `semantic_test.c` +43/-1, `tests/source_io.c` +8/-8, `tests/seed.c` +1/-1
  (net +42). Patch-file churn and documentation are counted separately.
- [x] Connect registration progress through the retained **lexical namespace**
  dependencies, not only exported module producers. Reuse existing entry links,
  recipe validation and scope order. Keep proof acceptance separate; no extra
  name/proof authority or silent cursor reset. Test parent-omitted and child-first
  roots, forward names, assertions, aliases and inert bytes before adopting it.
  Do not claim full checkpointing without child continuations and scheduling.
  **APGSRC68 after `02895e5` (2026-09-29, agent implementation):** retain the
  existing indexed/activated cursors and name-only completion, 24 bytes per
  namespace. Rebuild through the existing owner API, parents before children.
  Advance cursors only after successful publication; failed items re-enter
  ordinary Solve rather than importing a negative verdict. Name publication
  no longer requires typed Context acceptance; entry/module checks still do.
  `registration_progress` covers all 0..120 cuts of forward-name, duplicate,
  missing-name, invalid annotation and invalid sibling fixtures; it fails on
  `02895e5`. Parent-omitted/child-first and shared-selection tests remain.
  Corrupt cursors reject without publication; a completed registration in an
  invalid Context does not accept its module or entries.
  Wrong entry recipes now reject during structural attachment, before Solve;
  the older test requiring successful decode of that malformed edge is updated
  to require rejection and unchanged output arguments, not weakened acceptance.
  Example-09 ordinary completion remains 2,824 steps; completed size is
  50,796 -> 50,820 bytes. The public partition gate still **fails**: 100+100 is
  26,212 vs 26,390 bytes, and 1600+1600 remains pending vs done. Inert resaves
  are exact. This preserves registration progress, not the missing whole-source
  checkpoint, authenticated acceptance or fuel history.
  Fresh verification (2026-09-29): full O2 `check-acceptance`, artifact history,
  semantic/prepared-module/constructor ASan/UBSan, owner checkpoints and
  C differential/Linker/checked-and-trusted Acc QuickSort gates pass. Tests use
  clean accepted sources plus these prototype overlays, excluding unrelated
  working-tree Evidence/IADT edits. Logs are
  `/tmp/a-program-registration-final-{acceptance,history,semantic,asan,asan-source,asan-constructors,checkpoints,c}.log`;
  the separately failing public partition gate is in `-partitions.log`.
  Actual implementation delta from `02895e5`: `source_io.c` +30/-2,
  `source_io.h` +9/-5, `synthesis.c` +24/-13, `synthesis_source.h` +4/-3:
  **+67/-23, net +44**. Tests: `semantic_test.c` +122/-1,
  `definition_checkpoint_test.c` +2/-2, `tests/source_io.c` +39/-37,
  `tests/seed.c` +1/-1: **+164/-41, net +123**. These are applied C deltas;
  patch-file context churn and documentation are separate.
  The earlier module-root-only trial below remains rejected.
  **Rejected trial after `239b380` (2026-09-29, agent assessment):** persisting
  `(module producer, indexed, activated)` passed ordinary roundtrips but failed
  when only a child module was exported. Its outer-name assertion still needs
  the lexical parent's registration, although that parent is not a module root.
  Restoring parents first fixed export ordering, not this missing dependency.
  `lexical_module_roots` now preserves both counterexamples as permanent tests,
  using the existing source API (not invented nested-block surface syntax).
  That trial codec/format change was withdrawn; it did not replace APGSRC66.
  The subsequent APGSRC67 ownership change above does not adopt its cursors.
  Adopted owner contract: collect registration from live lexical scopes, share its
  entry links once, and relocate outer namespaces before local assertions. Do
  not invent a public module root, scan all historical jobs, or drop a charged
  cursor merely because its parent was not an export.
- [x] Fix the constructor-use ordering dependency exposed by shorter name
  registration. A pending allocation must not serve as checked field structure.
  Reject incompatible field counts before sharing an allocation, then await the
  existing field worker and compare its synthesized result. No new cache or
  authority. `member_prefix_recheck` now puts each of four good/bad uses first;
  saved type annotations are rederived, while wrong binders/kinds/counts reject.
  The uncorrected trial failed the existing good-use assertion. This is a source
  owner fix, not a weakening of that test or a C-backend special case.
- [x] Verify the independent FIFO fix exposed by that trial: re-enqueueing a
  still-queued worker must not truncate the ready list. Use its existing
  link/tail, not another flag. `early_wake` reproduces head/middle/tail reuse;
  it fails against `239b380`. Fresh verification (2026-09-29): O2 semantic and
  full `check-acceptance`, ASan/UBSan semantic, normalization/namespace-body
  checkpoints, C differential/LinkerScript and checked/trusted Acc QuickSort
  gates pass. The clean accepted-source snapshot plus the current prototype
  overlays excludes unrelated working-tree Evidence/IADT edits. Actual source
  delta from `239b380`: `synthesis_work.c` +3/-0; `semantic_test.c` +68/-0.
  Patch-file context and documentation are not implementation line changes.
  Logs: `/tmp/a-program-registration-reviewed-{semantic,asan,acceptance,c,checkpoints}.log`.
  The public partition gate still **fails**: 100+100 uses 25,770 versus 26,094
  bytes, and 1600+1600 remains pending versus completion at 2,824 steps.
  This fix does not implement source-frontier resumption or complete AP1-AP3.
- [ ] Expand continuation transport to unfinished fields, classifier queries
  lacking a retained structural type, and other source/typing owners. Integrate
  the completed owner interfaces into ordinary `.a` checkpoint transport;
  known-origin fixture envelopes do not satisfy the public partition gate.
- [x] Reuse the completed field map before constructor body abstraction, through
  checked typed inputs; `check-artifact-constructor-checkpoint` tests this boundary.
- [x] Remove induction's read of completed scope-worker `fields[]` in favor of
  the checked schema's shared field order; namespace integration remains above.
- [ ] Replace any confirmed redundant origin/type retention with the existing
  typed owner's shared construction or an explicitly selected recompute policy.
  Gate on open scopes, distinct nominal families, ordinary Sorted results,
  module siblings/`::`, and step-0 identity; never waive obligations to shrink.
- [ ] Implement the single-fuel validation sublimit and explicit trust/import
  provenance from AP1. Record history separately; no false checkpoint claim.
  Artifact-local revalidation cap and C adapter integration are implemented;
  source-resume integration, retained history and provenance reuse are pending.

#### Rejected Partial Body Transport (2026-09-29)

**Subjective (User):** `.a` must not accumulate Transpiler/Linker features.
The same restraint applies to persistence experiments: retaining more objects
is not progress unless there is a demonstrated A Program consumer.
The latter sentence is an agent application of the user's constraint.

**Objective (Code):** at `43bbc0a`, a disposable prototype added the pending
definition-to-body edge to the existing producer table, using the existing
`pg_synthesis_definition_resume_body` API. It did not change the kernel,
Transpiler or trust policy. Public partition measurements on
`examples/09_list_induction.p` gave:

| Measurement | Current APGSRC68 | Rejected body-edge trial |
| --- | ---: | ---: |
| Single 2,000 steps, bytes | 31,696 | 32,061 |
| Reload after 1,000, then 1,000, bytes | 30,573 | 31,011 |
| Single 3,200 budget | done at 2,824 | done at 2,824 |
| Reload after 1,600, then 1,600 | pending at 3,200 | pending at 3,200 |
| Zero-step image, bytes | 25,369 | 25,369 |

The existing `registration_progress` regression also caught a new failure:
`main := #1; main :: #Text;`, saved after 108 steps, became permanently pending
after reload (206 new dispatches; empty ready queue), instead of rejected.
`definition_step` expects an existing child link to have its corresponding wait
subscription. The trial restored the link but not that scheduling state.
This is a defect in the attempted partial restoration, not a new finding that
ordinary `::` checking accepts this program.

**Assessment:** reject the trial. A larger image and an extra pointer are not a
checkpoint. Do not fix this by trusting stored completion, swallowing the failed
assertion, or adding a backend-specific path. The owner-level checkpoint tests
already restore scheduling separately; their success does not establish that an
isolated owner API is safe to insert into the public loader.
APGSRC68 remains current; the experimental format is not adopted.

**Plan:** the next AP1-AP3 integration must restore a reachable suspended-owner
closure and its scheduling invariants together, through the existing shared
relocation and Solve machinery. Start with one small source fixture across
every cut, including `::` rejection, rather than adding isolated fields to the
public format. Preserve exact aliases, inert step-0 cycles, and account for
validation separately within the user's single total budget. Measure bytes and
useful progress; unrelated completed workers are not retention roots. The full
partition gate stays failing/open until its actual obligations are satisfied.

Reproduction: `check-artifact-partitions` with clean accepted `e716232` plus
the committed prototypes at `43bbc0a`; current and trial TSVs are
`/tmp/a-program-body-frontier-before/partitions.tsv` and
`/tmp/a-program-body-link-partitions/partitions.tsv`. Trial semantic failure is
in `/tmp/a-program-body-link-semantic.log`. No trial implementation is promoted.
Fresh O2 `check-artifact-semantic` on an unmodified candidate passes, including
the same 0-120 cut/rejection tests and inert byte-equal cycles; log:
`/tmp/a-program-artifact-boundary-verified.log`. The full partition test still
fails as reported above; no full compiler regression rerun is claimed here.

#### Source Dependency Waits (2026-09-29)

**Objective:** following `ea621fe`, an integration regression reconstructs
pending definition bodies and the current module-entry cursor through existing
owner APIs, then runs ordinary revalidation. It does not install saved evidence
or a checkpoint format. The previous implementation strands even the valid
`main := #1; main :: #Int;` at cut 108: pending root, empty ready queue.
The invalid `#Text` annotation must also reach rejection instead of stalling.

**Assessment/implementation:** `definition_step` and `definitions_step` called
`pg_synthesis_finish(..., child->status)` when a child was unfinished. A pending
status is not a completion: it needs a subscription. Both now use the existing
`pg_synthesis_await`. The request-creation stages and ordinary completed-child
paths are unchanged. No new scheduler, acceptance rule, artifact field or
Transpiler dependency is introduced; implementation delta is +2/-4 lines.

**Verification:** clean accepted `e716232` plus prototype overlays at `ea621fe`
and this change; unrelated working-tree edits are excluded. Focused O2 and
ASan/UBSan semantic tests pass, including 242
valid/invalid reconstruction cuts, no import-time Solve/proof admission, and the
existing inert byte-equal cycles. Definition and started-constructor-body
checkpoint gates also pass, preserving their remaining dispatch counts.
O2 C emission, LinkerScript, native enum and checked/trusted Acc QuickSort
differentials pass, including input-image immutability. Permanent tests add 52
C lines; patch-file context changes are not implementation line growth.
Logs: `/tmp/a-program-pending-parent-{before,after,final,final-asan}.log`.
Backend log: `/tmp/a-program-pending-parent-c.log`.
Full O2 `check-acceptance` also exits 0: source compatibility 63/63,
all four derived-LT/partition-order variants, universal sorting properties,
Local/Strong Sorted, finite views and optional internal witness packets.
Log: `/tmp/a-program-pending-parent-acceptance.log`.
The full public partition test still fails with the same measurements as the
APGSRC68 baseline; this is not permission to adopt the rejected body-edge wire
extension. AP1-AP3 closure/schedule/provenance integration remains open.

#### Explicit Recompute Profile (2026-09-29)

Agent implementation within the user's earlier requirement to allow either
recomputation or retained intermediate results, controlled by an option:

- `--save FILE.a` retains available typed results as before.
- `--save-inputs FILE.a` preserves producer/source/rule inputs and obligations but
  omits materialized producer results. It restarts Solve on import, deliberately.
  This is neither a checkpoint nor a backend-ready typed export.

Both use one writer/reader and APGSRC65; no second graph, solver or compatibility
format. Only result-root selection differs. Nominal allocation dependencies
needed by retained inputs still use the existing closure collector. Omitted
results do not waive module siblings, annotations or later proof checking.
The choice is per save; `:save` keeps its existing result-retaining behavior.

Measured conversion at zero steps: QuickSort 32,372,583 -> 3,070,587 bytes (90.5%
smaller). Materialized typed nodes go from 178,600 to 0; shared typed/rule Core
table becomes 21 Terms. The remaining file includes source syntax and producer
inputs, not just this shared table. The default fixed allowance reads this image.
This measures an explicit space/recomputation tradeoff, **not** a 90.5% reduction
while preserving all typed progress. The result-retaining profile is unchanged.

- [x] Prototype the input profile without mutating source state. Small tests
  cover parse-only/partial/completed recursive Match/IH, host arithmetic, alias
  roots, and a valid selected name beside an invalid sibling. Zero-step resave
  is identical; re-solving preserves the eventual done/rejected status.
- [x] Complete large Quick/Insertion recomputation and negative-proof gates,
  plus current fixed-limit regression/sanitizer checks. Fresh candidate
  `/tmp/a-program-input-profile` on clean `e716232` plus prototype: history/CLI,
  examples 01-09/results, focused O2/ASan/UBSan and file-policy tests pass.
  Five-backend sorting passes: Quick/Insertion 143s, Merge 90s, Tree 127s,
  Bubble 175s, plus concrete value transport. Input-only general QuickSort
  and invalid-sibling/shape rechecking pass; other large backends exercise the
  result-retaining profile. Reports: `/tmp/a-program-input-profile-{history,
  asan,sorting,file-policy}.log`. Full acceptance has not been rerun for this
  increment; no production promotion or main push is claimed.
- [ ] Continue AP1/AP2 reduction of redundant **retained-result** dependencies;
  inputs-only output does not discharge that work or checkpoint preservation.

#### Restore Existing Inputs Only (2026-09-29)

Inspection found that `pg_sources_read` called the source constructor, which
installed a second intrinsic namespace and scheduled its typing work before
restoring the image's own namespace. The prototype now shares owner allocation
through `pg_program_allocate_empty`; source construction adds its standard
namespace, while image loading uses only the saved lexical inputs. The imported
Program convenience scope is empty; source/REPL extensions use the restored
producer's lexical environment, not another prelude. No kernel rule is skipped.

The new zero-root image test fails before the fix: loading an empty image leaves
unrelated runnable work. It passes afterward, with no runnable work or consumed
steps. Restored `#Int`/`#int_add` and subsequent source extension, semantic tests,
history/CLI tests and focused ASan/UBSan pass. Full `check-acceptance` passes on
this candidate (exit 0). Example 09 from the same zero-fuel image completes in 2,942 rather
than 3,129 steps; completed image bytes remain 50,472. Inert and completed-cycle
gates remain exact. The strict partition gate still fails: 100+100 gives 25,830
versus 26,070 bytes; 1600+1600 is pending versus done at 2,942. This removes
duplicate initialization, not the missing continuation or validation fuel cost.
Reports: `/tmp/a-program-restore-inputs-{history,asan,partitions,acceptance}.log`.

The five-backend sorting gate passes on this candidate: Quick/Insertion 135s,
MergeSort 98s, TreeSort 139s, BubbleSort 192s, plus concrete value transport.
This does not discharge the separate partition/resumption gate.

The split-budget probe now measures a terminal boundary rather than assuming
one, and tests both `terminal+0` and `0+terminal`. It reports byte/size agreement,
local status and consumed fuel separately. At `2942+0`, the image and cumulative
fuel match exactly but local status changes from `done` to `pending`: zero-step
import intentionally does not admit evidence. This is distinct from the earlier
`100+100` lost-progress failure. The strict gate still reports failure; no
acceptance condition was silently relaxed. This predates the AP1 user decision;
the revised gate must report validation/trust provenance separately from progress.
Report: `/tmp/a-program-restore-inputs/terminal-partition-fields/partitions.tsv`.

## AP3. Verification and Handoff

### Subjective (User)

Measure growth versus fuel, with zero-step cases mandatory. Finish this refactor
before taking up C emission. The user will continue investigating #43 separately.
The user additionally requests equal-size outputs for equivalent fuel partitions,
and preferably identical bytes, not merely absence of growth.

### Objective (Code)

The existing prototype `image_audit/fuel_curve.sh` records bytes, deltas, hashes,
reported fuel and exit status. Example 09 passes ordinary cycles but detects
retained growth of 444 bytes per completed generation. The large ordinary
prototype matrix is stable at 3,065,779 bytes; retained mode hits the default
reader bound. These are prior targeted results, not tests of the planned format.

Fresh split-budget probe on the same clean `e716232` compiler (implementation
unchanged at `df250f3`), Example 09, starting every path from the same zero-fuel
image: 10+10 versus 20 is byte-identical. At 100+100 versus 200, ordinary save/load
produces **25,266 versus 25,598 bytes** despite both reporting 200 used steps and
pending status. Splitting in memory, with or without an intermediate save but no
reload, matches the single run. Retained mode also fails across reload and has
some same-size byte differences. `partition_fuel.sh` records these as failures,
not accepted deviations. Logs: `/tmp/a-program-partition-*`.
At 1600+1600 versus 3200, the ordinary images are even byte-identical (26,374
bytes), but the single run is done at 3,129 steps while the reloaded split remains
pending at 3,200. Thus byte equality alone cannot certify preserved Solve progress.

Rechecked against the `f2f6f69` clean-baseline transport overlay on 2026-09-28:
both failures persist. The prototype `check-artifact-partitions` target now runs
the strict byte gate as well as size/status/consumed-fuel checks. It deliberately
fails rather than accepting these cases as expected failures; its reports remain
available for comparison. This is not yet a production-registered or passing gate.

### Assessment

Size stability alone can hide loss of useful progress. Require both inert
stability and preservation of already materialized semantic roots. Reading a
view without Solve is testable now without implementing a C emitter. An unfinished
export may remain unavailable; a malformed/rejected input must not become accepted.

The desired composition law is `advance(advance(S, a), b) = advance(S, a+b)`;
the serialized-resume variant inserts save/load between the two advances.
Fix compiler version, input image, selected roots, policy and requests when
testing it. Exclude format migration and host execution from this pure gate.
Baseline APGSRC62/63 and prototype APGSRC68 reconstruction do not preserve the full running frontier;
the equality of supplied or reported fuel therefore does not establish equality
of progress. A recompute fallback must be reported as such, not counted as passing
this resume gate. Count any required revalidation explicitly; do not hide its cost
or silently trust imported claims merely to make the equation pass. An explicitly
selected trust mode is covered by AP1, not an automatic test workaround.
In particular, local acceptance after importing an image is not a serialized fact:
distinguish revalidation work from a lost computation frontier when investigating
these diagnostics. Do not fix the counter/status mismatch by restoring a trusted
`done` flag. The intended structural gate concerns saved progress, not that flag.

Minimum gate: same materialized structure/residual frontier and byte count.
Stronger gate: identical bytes under deterministic serialization, without alpha
interning or normalization during save. Record both outcomes separately. Fixed
padding, retained dead records or omitting useful results cannot repair a failure.
CLI status/step/byte equality alone cannot prove the structural part: the planned
semantic-root instrumentation remains necessary, even when this prototype passes.

### Plan

- [x] **AP3.0:** retain the existing quantitative prototype and baseline results.
- [x] **AP3.0b:** add/run the split-budget CLI probe, with separate size and exact
  byte verdicts and fresh source/host/invalid-assertion controls. No production fix.
- [x] **AP3.1:** extend it for the chosen format: source fuel 0/1/100/completion;
  three same-format zero-step rewrites of each state; partial resume and three
  completed generations, each followed by zero-step rewrites. Require exact byte
  identity at zero, no host output/acceptance, and no prior-history accumulation.
  Prototype history/metrics gate passes on the byte-bound candidate. Reopening
  then finishing is still recomputation, not preserved partial progress; the
  stricter continuation requirement remains open under AP3.1b.
- [ ] **AP3.1b:** compare 20 with 10+10, 1+19, 0+20 and 20+0, plus 0 versus 0+0;
  include larger splits at actual construction/evaluation suspension boundaries
  and after completion. Compare single run, in-memory split, split with an inert
  save, and split across a fresh-process reload. Record supplied/used cumulative
  fuel, validation spent, trust provenance, status, semantic roots/residuals,
  counts, bytes and digests. Under the updated AP1 contract, require size and
  structural agreement at equal useful progress; exclude verification history
  from that comparison, but test history accounting separately. Keep total-fuel
  comparisons visible, including their actual revalidation cost. Run semantic
  byte equality as a separately visible stricter
  gate. Neither smaller files nor equal-sized but changed files count as full
  equivalence. Compare actual consumed fuel as well as budgets when work finishes
  early; do not require consuming the unused budget after completion.
- [ ] **AP3.2:** count semantic roots, unique reachable Terms/objects, typed nodes,
  obligation edges and section bytes in addition to total bytes. Attribute
  positive-fuel growth to newly retained dependencies. Do not impose universal
  bytes-per-step proportionality or call every fresh binder a duplicate.
  Prototype metrics cover the current semantic payload and rule edges; nested
  pending-owner continuation/residual counts remain open.
- [ ] **AP3.3:** verify save/load preserves existing materialized results and
  pending obligations without advancement. Instrument that inspecting the view
  makes no synthesis/evaluator calls, not merely that a CLI counter reports zero.
  Rechecking retained material must use existing kernel rules under explicit fuel.
- [ ] **AP3.4:** test same erased Core with different annotations, distinct nominal
  families, open scopes/maps, selected-root aliases, IADT/Identity witnesses,
  effect equations, invalid `::`, and normalization requests. Keep required
  negative/trust tests when removing duplicate ordinary/retained matrices.
- [x] **AP3.5:** add a read-only semantic-view consumer test with no C codegen.
  It must not include syntax/synthesis headers or change the image on inspection.
  Test a materialized export beside an unfinished sibling without waiving any
  whole-module proof needed to execute the selected root.
  Prototype O2/ASan/UBSan passes: a consumer reads a suspended function's Pi,
  formation, binder, scoped body and result classifier without forcing it.
  An invalid unfinished sibling still rejects the module/checked selection;
  inspection leaves zero steps, no new evidence and identical saved bytes.
  The consumer test is not a backend-readiness or acceptance API.
- [ ] **AP3.6:** run source/occurrence/scoped/derivation I/O, execution, F3/F4 and
  full acceptance tests plus focused ASan/UBSan on the candidate. Promote only
  authorized pieces; register the small fuel test permanently and the large one
  with sorting. Report per-file additions/deletions for implementation, tests
  and docs separately. Record remaining unavailable cases, not false completion.
  The earlier result-retaining candidate passed these regression suites and
  focused sanitizers against clean `e716232`. The newer fixed-limit/input-profile
  increment has its separate results above; do not inherit an unrun full-suite
  verdict from the previous candidate. Promotion, permanent registration and the
  separate failing partition gate remain open; this is not completion of AP1-AP3.
  Latest run (2026-09-29): the fixed-limit/input-profile candidate passes full
  `check-acceptance` (exit 0, about 18m35s including builds, `-j2`). This run
  includes the pre-existing uncommitted Evidence/IADT changes and their tests;
  they are not included in the persistence/backend commits. Isolated prototype
  transport/semantic and C gates also pass without those changes. This result
  does not change the still-open frontier/trust/partition requirements above.

Handoff: the agreed semantic content survives `.a` roundtrips, reading/inspection
is inert, unused history no longer accumulates, and existing language behavior
remains unchanged. The user has now requested C transpilation (AP4). #44 remains
open until its implementation and verification criteria are met.
#43 remains open and receives no syntax or termination-rule changes in this work.

## AP4. First C Transpiler

### Subjective (User)

2026-09-29, paraphrase: implement at least `.a` to C, keeping artifact work in
scope, and push at appropriate verified milestones. The earlier one-way backend
dependency and separation of computation from typing remain requirements.

### Objective (Code)

At `5eaf3b7` plus the AP1-AP3 prototype, loading is inert and materialized typed
roots are inspectable. Whole-module acceptance still requires ordinary Solve;
an imported occurrence is not accepted evidence. No C emitter exists. Core has
Lambda/Application/Reference; Oracle owners already expose inert host, CBPV and
constructor/matcher views. These views need not be copied into an artifact IR.

AP4 implementation after `966df23`: `c_backend/emit.c` consumes the existing
occurrence/Core/Oracle views. Its command adapter alone imports images and
requests whole-module checking. Generated executables link only the target
runtime and libc. O2 and ASan/UBSan differential gates pass for captures, partial
applications, repeated suspensions, deep handlers, recursive ADTs, generic List,
host overflow/formatting, NUL, pending/rejected inputs and atomic output.
The raw Oracle gate covers two distinct operation clauses and all ten host
arithmetic/formatting functions; emission is checked for zero evaluator steps
and unchanged graph/proof counts. Repeated imports emit identical C and do not
change input-image digests.

Focused O2 and ASan/UBSan gates also pass against clean `e716232` plus the committed persistence,
readback and conversion prototypes, without the unrelated Evidence/IADT edits.
The separate full acceptance run passes on the current working-tree candidate,
including the pre-existing uncommitted Evidence/IADT changes (AP3.6); those
changes are excluded from this work's commits.

The existing Acc QuickSort source checks in 67,229 steps and image selection in
67,377 steps. Interpreter execution prints `FFTT` in 18,495 runtime steps.
Before the AP4.6 increment below, C emission refused its reachable Identity
Oracle. At `4e338b1` plus the 2026-09-29 backend changes, the generated standalone
executable now prints `FFTT` in both checked and explicitly trusted modes.

### Assessment

Agent implementation decision: build a separate prototype backend consuming a
closed typed root, not parser nodes or scheduler state. A thin command driver
loads `.a`, obtains a checked named export through existing Solve, and passes the
borrowed occurrence downstream. Explicitly report reconstruction fuel: this
first driver does not pretend to preserve the unfinished solver frontier or to
implement trusted import. In this initial default path, zero fuel cannot emit.

Increment after `1bc5e18` (2026-09-29, agent implementation decision under AP1's
explicit user-authorized trust policy): add `--trust-image` at the artifact/C
adapter boundary. APGSRC66 records one completion byte on each existing producer,
without creating evidence, restoring scheduler status, or duplicating the typed
graph. `pg_artifact_trusted_export` requires a completed source module, every
entry's completion (including imports and assertions), and a saved closed local
definition. It cannot authorize an isolated result beside unfinished obligations.
The caller explicitly trusts an **unauthenticated file assertion**; this is not
strict validation or a hash-provenance scheme. No checking, allocation or source
elaboration occurs on this path. Inputs-only and incomplete images refuse it.
There is no failure-to-trust fallback. Default checking remains unchanged.

This bounded increment addresses already-completed downstream consumption, not
AP1/AP3's missing pending frontier. The API is only available before any Solve;
inert resaves retain statements, while a save after strict recomputation starts
reports only local completion. This deliberately conservative temporary policy
does not solve partial revalidation/resumption or the validation sublimit/history.
Those remain open, as does authenticated external prior-record reuse. Refusing
unsupported compiler-wide reuse is preferable to fabricating accepted receipts.

Emit C functions/closures from the structural DAG, with a backend-local runtime
for the supported Oracles. No embedded source parser, Solver, kernel or `.a`
reader in the generated executable; no compile-time execution of host effects.
Use exact fixed-width unsigned arithmetic for host wrapping operations, exact
Text bytes and structural ADTs, not a heuristic Nat-to-machine-int conversion.
Recognize nominal objects by pointer identity, never source spelling. Unsupported
reachable Oracles fail before publishing output. Do not erase proof Terms on
the assumption that every proof is computationally irrelevant.

Rejected shortcut: treating Identity transport as an unconditional no-op, or
normalizing the entire QuickSort at translation time to hide missing runtime
support. `identity.c:field_answer` has a diagonal transport rule, but the general
U/Pi transport also transforms inputs/results and lifting. That needs its own
faithful target realization; the mere presence of checked typing does not erase
those computations. The former refusal test is now a differential execution
test; general Identity coverage must still be distinguished from this example.

AP4.6 implementation decision (2026-09-29): generated closures accept a target
projection mode rather than installing an additional source evaluator or proof
database. A related binder carries both endpoints and its chosen center;
ambient captures remain fixed. The runtime follows one-direction Lambda/APP,
constructor Match, diagonal and scoped U/F/Pi transport equations from
`identity.c` and `iadt.c`. Conservative endpoint comparison only enables the
explicit reflexive-action equation. It does not establish typing or reflect
object equality into conversion. Review caught eager evaluation of transported
Return payloads; they now stay delayed, with a discard regression fixture.
Known Identity operations with unsupported demanded shapes fail at runtime;
unknown Oracle owners still fail emission before publication. This changes the
backend's support boundary, not the kernel, artifacts or source acceptance.

### Plan

- [x] **AP4.1:** add `src/prototype/c_backend/` with emitter, runtime, command
  adapter and focused tests; keep accepted build untouched during trial.
- [x] **AP4.2:** cover Lambda/Application, Return/Thunk/Force, Fold/request,
  host integers/Text/print; retain effect order and delayed execution.
- [x] **AP4.3:** add structural constructors and Match, including recursive
  Lambda encoding, without changing IADT identity or typechecking rules.
  This is the structural runtime fragment, not all Identity-bearing IADT uses.
- [x] **AP4.4:** compile emitted C independently and compare execution to the
  interpreter; test overflow, embedded NUL, higher-order captures, repeated
  thunks, multiple handler clauses, invalid siblings, pending input, unsupported
  Oracles, output failure and unchanged input artifacts. Run sanitizers.
- [x] **AP4.5:** document the exact supported subset, fuel/trust limitations,
  generated runtime ownership and per-file line changes. Push verified chunks;
  do not close #44 or mark AP1-AP3 complete from a limited backend milestone.
  Published: persistence increment `966df23`, first C backend `92704c6`, both on
  `origin/main`. Neither commit promotes prototype code into accepted `src/`.
- [ ] **AP4.6:** implement the needed Identity transport/action/lifting target
  equations using exact Oracle contracts, then replace QuickSort's explicit
  rejection gate with actual generated-C differential execution. Include
  non-diagonal and dependent U/Pi cases; no unconditional proof erasure.
  Increment after `2187d92`: compile diagonal transport only for `Act A` with
  a known inert reference `A`, in either direction. O2 and ASan/UBSan backend
  tests pass, including checked Text transports and rejection of lift, variable
  families/actions and function actions. Unknown payloads are not erased.
  QuickSort's rejection boundary still passes; general Identity and actual C
  QuickSort execution remain unfinished. No Kernel, source graph or acceptance
  rule changes, and no claim that a syntactic `Act` alone is always diagonal.
  Update after `4e338b1`: actual C QuickSort works. Raw differential cases cover
  chosen (not inferred reflexive) centers, Match fields, capture/shadowing,
  partial actions, constant families ignoring divergence, U/F/Pi maps in both
  directions, diagonal lifting and lazy discarded payloads. Non-reflexive loops
  and mismatching endpoints remain neutral in the kernel and refuse C execution.
  These raw fixtures test Oracle equations, not acceptance of arbitrary triples.
  **Still open:** general dependent thunk lifting and iterated higher actions;
  do not infer full Identity support from the sorting gate.
  Fresh O2 and ASan/UBSan: backend command tests, raw Oracle differentials and
  standalone QuickSort pass, including sanitizers on the generated executables.
  Tests use clean `e716232` plus committed overlays and this backend increment,
  excluding unrelated working-tree Evidence/IADT edits. Emission has zero
  evaluator/substitution calls and unchanged graph/proof counts; repeated C is
  deterministic within each admission mode and input artifact hashes are stable.
  Full compiler acceptance was not rerun because this increment changes only
  the separate target backend; earlier full-suite results are not reattributed.
- [x] **AP4.7:** verify explicit trusted completed-export consumption; publish as
  its own prototype increment after `1bc5e18`.
  Keep default checking, zero-step byte identity and no local evidence admission.
  Test invalid/pending siblings, `::`, imports, inputs-only refusal, old/invalid
  completion encodings, and differential C execution with effects. No separate
  replay engine. Report its narrow scope without closing AP1/AP3/#44.
  Fresh O2: artifact history/transport/semantic, seed and C differential gates
  pass. ASan/UBSan: semantic, transport, C differential (including emitted C),
  file policy and QuickSort unsupported-boundary gates pass. Fresh clean
  `e716232` overlays exclude unrelated working-tree Evidence/IADT changes.
  `step 0` cycles remain byte-identical; imported statements do not become
  local proofs. Import-provider failures and forged completion still reject
  under ordinary checking. Full acceptance was not rerun for this increment;
  AP3.6's prior result is not reattributed to this version.

AP4.6 backend increment relative to `4e338b1` (documentation excluded):

| File in `src/prototype/c_backend/` | Added | Removed |
| --- | ---: | ---: |
| `emit.c` | 45 | 36 |
| `runtime.c` | 291 | 26 |
| `runtime.h` | 11 | 5 |
| `oracle_test.c` | 89 | 22 |
| `oracle_check.sh` | 9 | 1 |
| `sorting_check.sh` | 14 | 16 |

Implementation +347/-67 (net +280); tests +112/-39 (net +73).
Accepted `src/`, the Kernel, artifact format and build remain unchanged.
This is additional target semantics, not code compaction or finished resumption.

AP4.7 source delta relative to `1bc5e18` (actual patched source, not diff-file
context/header churn; accepted implementation files are unchanged):

| File (prototype overlay unless prefixed) | Added | Removed |
| --- | ---: | ---: |
| `synthesis.h` | 7 | 0 |
| `synthesis_work.h` | 1 | 0 |
| `synthesis_work.c` | 17 | 0 |
| `source_io.c` | 17 | 7 |
| `source_io.h` | 6 | 4 |
| `artifact/file.c` | 27 | 0 |
| `artifact/file.h` | 9 | 0 |
| `c_backend/main.c` | 21 | 3 |
| `c_backend/emit.h` | 3 | 2 |
| `semantic_test.c` | 108 | 0 |
| `tests/source_io.c` | 6 | 2 |
| `tests/seed.c` | 1 | 1 |
| `c_backend/check.sh` | 28 | 4 |
| `c_backend/sorting_check.sh` | 7 | 0 |

Implementation: +108/-16 (net +92). Tests: +150/-7 (net +143).
Documentation is separate in this plan and the two prototype READMEs. This is
new explicit policy support, not claimed code compaction or frontier completion.

Initial backend line delta at `92704c6` (new files; no accepted implementation changed):

| File/group | Added | Removed |
| --- | ---: | ---: |
| `c_backend/emit.c` | 230 | 0 |
| `c_backend/main.c` | 102 | 0 |
| `c_backend/runtime.c` | 273 | 0 |
| `c_backend/emit.h` | 10 | 0 |
| `c_backend/runtime.h` | 42 | 0 |
| `c_backend/oracle_test.c` | 149 | 0 |
| `check.sh` / `oracle_check.sh` / `sorting_check.sh` | 85 / 11 / 26 | 0 |
| `build.mk` | 20 | 0 |
| Eight `.p` fixtures | 74 | 0 |
| Backend README | 87 | 0 |

Implementation: +657. Tests/fixtures/build: +365. Backend documentation: +87.
This is a new backend feature, not a claim of code reduction or completed AP1-AP3.

## AP5. Downstream Linker and LinkerScript

### Subjective (User)

2026-09-29, English paraphrase: incorporate the new PR, revise this active plan,
and definitely include a Linker and LinkerScript in `.a`-to-C. Keep them
subordinate to A Program; they must not alter `.a` or its semantics.
The user has not approved the research report's illustrative syntax, foreign
ABI, blanket proof erasure or default acceptance of pending proofs.

### Objective (Code)

PR [#48](https://github.com/repyt-margorp/a-program/pull/48) merged at `a1a3321`;
its sole change is the preserved
[research report](2026-09-29-LINKABLE-COMPILATION-UNITS-AND-TARGET-LINK-MANIFEST-DESIGN.md).
Historical inspection at `ea287c2`, unchanged by that merge:

- `c_backend/main.c` reads one artifact and selects one name, using ordinary
  whole-module checking or explicit trusted saved completion. Its atomic output
  guard already prevents replacing the input artifact.
- `c_backend/emit.c:pg_c_emit` collects one Core DAG, assigns emission-local
  object ordinals, emits private `tN` functions and unconditionally adds `main`.
  It performs no Solve or normalization. `entry_mode` rejects unapplied Pi.
- `c_backend/runtime.h` ABI 2 exposes internal allocation and `setjmp` state.
  It is not a public library ABI; object ordinals are not component identities.
- There is no LinkerScript reader, multi-export interface or native link driver.
  #46 is therefore an extension, not an established failure of single-entry C.
  #47 separately concerns relevance and partial-artifact admissibility.

Paths above are under `src/prototype/`. Those observations describe the state
before the implementation below. Merging the report closed neither issue.

### Assessment

Adopt the component/export separation and shared dependency closure from #46.
Implement one **declarative LinkerScript**, provisionally `.aplink`, as the
downstream input describing artifact roots, public aliases, product, optional
entry, target/ABI and target bindings. A parsed script is the link plan; do not
create separate mutable databases for script, manifest and resolved answers.
Borrow selected typed occurrences; keep only target-local lookup/closure data.

```text
unchanged .a + external LinkerScript
  -> existing artifact admission + named-root selection
  -> one shared reachable DAG -> private C + public wrappers/header
  -> target compiler/archiver/linker -> library or executable + link receipt
```

The dependency is exclusively from transpilation to artifact/semantic APIs.
No C symbols, section placement, ABI layout, linker flags, link receipts or
target acceptance state enter canonical `.a`. A script cannot supply typing,
prove termination, synthesize an Oracle contract or authorize trust. Keep the
existing explicit admission controls outside script data. Charge validation
against one invocation budget across all selected roots, not a fresh budget
per export; share loaded modules and ordinary Solve work.

Distinguish this script from a native GNU/LLD `.ld` layout script and an export
map. Native files are downstream inputs/products of the target driver. A real
native link step must consume them where requested; merely listing a filename
in a receipt is insufficient. Support explicit native-script selection in the
C driver, with profile validation and clear unsupported diagnostics. Automatic
embedded memory-layout generation and non-C targets remain later work.

Initial agent proposal: export several already-supported closed returning
roots through run/status wrappers. Each call owns and destroys its runtime and
failure boundary; no boxed value, closure or stale `jmp_buf` escapes. This is a
useful library milestone, not a claimed general function ABI. A named A Program
export is not necessarily a separate object file. Emit the roots' union once;
`main` becomes an optional small wrapper calling the same public entry path.

Before permitting boxed calls between independently emitted components, define
owner-qualified nominal/effect identity and a shared-declaration mapping. Equal
local ordinals, spellings or structural hashes cannot establish that identity.
For the initial isolated-call ABI, explicitly reject handle exchange instead of
silently equating families. Callable Pi/boxed and restricted flat ABIs follow.

Defer #47's general relevance/erasure policy: no naming-based "proof-only"
classification, pending proof admission, unsafe narrowing or unconditional
Identity erasure is introduced by linking. Existing supported Identity runtime
equations remain in use. Also defer a new source-language interface syntax,
generic multi-target framework and duplicate permanent target IR. These are not
prerequisites for script-driven C components.

### Plan

AP1-AP3 remain unfinished. This independent requested backend increment proceeds
without making their failed partition gate a prerequisite or calling it repaired.
AP5 follows the existing supported
AP4 fragment; it need not wait for every higher Identity equation in AP4.6 or
for #47. Implement and push each verified increment, initially only under
`src/prototype/c_backend/`; use `link/` there for script/driver ownership.
Future promotion to `src/transpile/` is a separate approved change. Do not put
the LinkerScript parser in the source parser or artifact codecs.

- [x] **AP5.0:** merge PR #48 intact; inspect #46/#47 against code and record
  adopted/deferred proposals here. This is planning, not linker completion.
- [x] **AP5.1:** specify and implement the minimal versioned script parser in
  `link/`, with matching `--link` driver input. Define quoting, paths relative
  to the script, ordered exports, alias validation, product/entry rules and ABI
  version. Initially one artifact/component suffices; reject unsupported imports
  or target fields, never ignore them. Existing CLI shorthand builds the same
  plan rather than maintaining a second emission path.
- [x] **AP5.2:** resolve selected names via existing artifact APIs once, retain
  their admission policy, and extend `emit.h/.c` to one shared multi-root DAG.
  Extract unconditional `main`; keep `tN` private. Publish `.c`, `.h` and a
  receipt only after validation; reject conflicting aliases, reserved symbols,
  input/output path aliases and unsupported entry shapes before publication.
- [x] **AP5.3:** define/run the first closed-export public ABI with per-call
  allocation, status and cleanup; preserve the private runtime ABI separately.
  Test repeated calls and failure followed by another call. Do not export
  internal runtime structures or imply persistent/reentrant handles are ready.
- [x] **AP5.4:** add source/object/static-library/executable products and an
  actual native link driver consuming the same plan. Invoke tools with explicit
  argument vectors. Support target-native script input and test its actual
  linker effect. C-static private functions and prefixed public wrappers control
  object/archive symbols; shared-library export maps belong to AP5.6. Keep
  native archive `.a` and A Program `.a` distinct by explicit product roles and
  non-overlapping paths, not suffix guessing. Record tool/profile, exports and
  input/script references in the receipt, never as Kernel evidence.
- [x] **AP5.5:** run a C client calling at least two script-selected exports;
  compare it and executable mode with interpreter effects/results. Check shared
  dependency reuse, private symbols, no duplicate `main`, deterministic emission,
  invalid scripts/unresolved bindings/tool failure, ABI mismatch and sanitizer
  lifetimes. Link two isolated components with colliding local ordinals and
  verify no unintended identity/handle exchange. Check input bytes unchanged
  after success/failure and zero Solve/graph growth in emission itself. Re-run
  existing backend, QuickSort and artifact zero-step/partition gates; report
  still-open partition failures rather than attributing their repair to linking.
- [ ] **AP5.6 (follow-up in AP6/#49):** specify callable Pi arguments,
  owned boxed handles, component/shared nominal identity and restricted flat
  types before enabling them. Test distinct-family collisions and shared-family
  controls. Foreign symbol bindings require an existing type/effect contract;
  unresolved bindings may be listed for objects but must resolve for executables.
  Shared-library visibility and further target profiles follow explicit tests.
- [x] **AP5.7:** close #46 against its initial isolated-library completion
  criterion, with verified admission/identity/lifetime limits and an explicit
  handoff of AP5.6 to AP6/#49. Closed on 2026-09-29 after the audit below.
  Keep #47 open until its separate admissibility criteria pass. Report implementation,
  test and documentation line deltas separately; do not count a merged report
  as implemented functionality or complete AP1-AP3/#44 from a backend pass.

#### Initial Linker Implementation (2026-09-29)

Agent implementation against `b6bbb0b`: `c_backend/link/plan.c` reads one
declarative `aplink 1` plan; the existing driver admits its ordered exports from
one loaded Program under one B/R budget. `emit.c` collects their union once,
keeps tN functions static, and implements both the old CLI and component output
through the same emitter. No additional target IR, checker, source syntax,
artifact field or acceptance rule was added. All changes remain prototypes.

The agent-selected public convention is `ap_export_ALIAS(void)` returning only
execution status. Prefixing all validated aliases avoids main/C-keyword/runtime
collisions without maintaining a reserved-symbol list. Every call owns its
runtime and jump target; it cannot export boxed values, callbacks or persistent
handles. Values are discarded, not serialized or returned as unowned pointers.
This permits independent components' local nominal ordinals to overlap without
comparing or exchanging them. It does **not** supply shared-family interchange.

The source/object/archive/executable driver stages a new output directory;
existing paths reject. It writes C/header/runtime/JSON receipt and invokes host
cc/ar with argument vectors where requested. `native_script` is consumed by
the executable link using `-Xlinker -T`, tested through a real added ELF section.
Publication occurs only after emission/tools succeed. Toolchain/ABI/admission
metadata are receipt data, not canonical artifact identity or proof evidence.
The grammar, product contents, ownership and explicit limitations are in the
[backend README](../src/prototype/c_backend/README.md#linkerscript).

Fresh O2 and ASan/UBSan gates pass: multi-export client, shared-root reuse,
deterministic source bundle, two distinct components including a nominal ADT,
failure then successful invocation, header ABI mismatch, native tool/script
failure, aggregate fuel exact/one-short/zero-cap checks, invalid siblings and
pending-trust rejection. Existing 21 C differentials and Oracle/Identity gates
pass; O2 checked/trusted Acc QuickSort still returns FFTT. Logs:
`/tmp/a-program-linker-all.log` (including QuickSort), `-final.log` and
`-final-asan.log` (final driver and expanded link/Oracle gates).
Emission-only Oracle tests also check unchanged graph/proof counts and zero
evaluator/substitution calls across multiple roots. Input digests stay unchanged.
No full compiler acceptance rerun is claimed for this backend-only increment.

Artifact zero-step semantic gates pass; the public partition gate still fails
with the same values as `b6bbb0b`: 100+100 gives 25,770 versus 26,094 bytes,
and 1600+1600 is pending versus completion at 2,824 steps. Reports:
`/tmp/a-program-linker-semantic.log`, `/tmp/a-program-linker/partitions/partitions.tsv`.
AP1-AP3 and the overall goal remain open. AP5.6 is also unimplemented; the first
linkable-library milestone is not a general foreign-function ABI.

Implementation delta (excluding tests/docs/build): `emit.c` +76/-6, `emit.h`
+13/-0, `main.c` +74/-38, `link/driver.c` +175/-0, `link/plan.c` +157/-0,
`link/plan.h` +28/-0: net +479. Tests: `oracle_test.c` +22/-0,
`link/check.sh` +167/-0, `link/client.c` +30/-0, `link/fixture.p` +5/-0,
`link/other.p` +2/-0 (net +226). `build.mk` +7/-2 (net +5).
Documentation is excluded from these counts. No accepted implementation or
unrelated working-tree changes are included.

## AP6. Target-Native Public Modules

### Subjective (User)

2026-09-29, English paraphrase: incorporate the new Issue/PR and make suitable
LinkerScripts produce A Program-derived modules that people can use as ordinary
C modules. The present output appears to preserve too much of the A Program
implementation model. Keep the backend subordinate to `.a` and the language.
The user requests the capability, not the report's exact profile grammar or a
hand-written QuickSort replacement. The following staging is an agent proposal.

Later follow-up on 2026-09-29, English paraphrase of the user: readable C
transpilation is **culture**, just as writing theory in 2020s natural language
or in classical C notation is cultural. The user's original name for this
concept was **Tradition**: conventionally choosing a way of expressing things.
Preserve this motivation for future design, rather than treating C's customary
form as intrinsic to A Program's theory.

Further clarification on 2026-09-29, English paraphrase of the user: refactoring
artifact functionality can be appropriate, but continually extending `.a` for
transpilation is a bad pattern. Transpiler/Linker responsibilities must remain
downstream. Do not persist target-only representations, ABI decisions, lowering
analysis or link configuration in `.a`. A missing backend view is not by itself
a reason to add an artifact field; first derive it from existing semantic data.
The user reaffirmed this boundary on 2026-09-30.

Implementation review criterion (agent): every proposed artifact field must name
an A Program consumer independent of target generation, and explain why existing
semantic nodes do not suffice. C names, native layouts, capture/liveness analysis,
ABI and link choices belong to the downstream arena/script/output manifest.
If a profile cannot lower an existing artifact, diagnose or reject that profile;
do not mutate the image to make it acceptable. Changing profiles must leave the
input image unchanged on both successful and rejected requests.
Rechecked at `43bbc0a`: O2 `check-c-link` and `check-c-enum` pass, including
the input-image immutability checks (`/tmp/a-program-boundary-link-check.log`).
Rechecked after `15ce2de`: native representation entries belong to
`c_backend/lower/scalar.c`'s temporary module arena (`m.order.storage`), not the
Program graph or artifact codec. Linker choices remain in `link/plan`/`driver`.
Both input-image immutability gates pass again with the source-reference delta.
The scalar/enum Oracle gate also preserves input Term/object/occurrence/proof
counts and forbids evaluator/substitution advancement during emission.

### Objective (Code)

PR [#50](https://github.com/repyt-margorp/a-program/pull/50) merged at `1a047e3`,
adding only [the native-lowering audit](2026-09-29-NATIVE-QSORT-LOWERING-AUDIT.md).
Implementation inspection at `baabea8`, unchanged by that merge:

- `c_backend/emit.c:entry_mode` rejects exported unapplied Pi computations.
  `pg_c_emit_header` exposes only `int ap_export_ALIAS(void)`; returned values
  are discarded. Linkability is implemented; an ordinary data/function API is not.
- `pg_c_emit_exports` emits one `tN` function per reachable Core node. Lambda
  creates a closure; APP calls `ap_apply` with a delayed operand. Its independent
  runtime preserves supported semantics without embedding the compiler, but is
  not direct native realization of those computations.
- `link/plan.c` accepts only `isolated_v1`/`host-c11`; it has no lowering or
  representation choice. `link/driver.c` always copies/compiles `runtime.c`,
  even if a future native component would need none.
- The read-only input is already `pg_occurrence` plus owner views, not a new
  semantic IR. `main.c` owns admission; no lowering pass should duplicate it.
- Fresh O2 Linker/C/Acc QuickSort gates and full compiler `check-acceptance`
  passed at `baabea8` on clean accepted sources plus prototype overlays.
  These do not test any proposed native profile.
- The report's 53-line Bool sorter is a hand-derived candidate selected by a
  fixed template and provenance hashes. Its artifact was not decoded. The
  reported finite tests are external-session results, not rerun upstream CI or
  a checked compiler transformation. Preserve that distinction.

### Assessment

Adopt the selected-boundary and native-realization direction. Separate three
contracts without creating three mutable authorities:

1. Existing accepted/trusted typed input: what the source computation means.
2. One downstream LinkerScript: which public inputs/results, representation,
   ownership, target, observations and unsupported policy are requested.
3. One target-local lowering result: implemented transformations, required
   dependencies, emitted ABI and diagnostics/receipt. It cannot grant evidence.

The script selects implemented transformations; it cannot assert an arbitrary
source/C equivalence. Keep `native_script` exclusively for native linker layout.
Agent interpretation of Tradition: lowering profiles choose target conventions;
they do not establish another source semantics or acceptance authority. This
does not require a new Core former, source keyword or framework rename.
No target profile, C symbol, buffer contract or transformation result enters
the canonical `.a`, Core, source syntax or kernel. Reuse the parsed link plan;
do not add a parallel manifest database, solver, permanent C type graph or replay.

**Two independent requirements:** a flat public API could still marshal into
closures internally; direct calls could still expose unusable internal values.
Neither alone completes this request. First obtain a small genuinely native
fixed-width function boundary, then extend calls/constructors and container
representations. Keep the structural backend as the differential reference and
an explicitly selected fallback, never a silent replacement for a native-only
request or a different ABI.

Agent proposal for the first profile: closed first-order functions over existing
`#Int32`/`#Int64`, native parameters and a status/result contract. For example,
the already-valid source `add := \x : #Int32 => \y : #Int32 => #int_add x y;`
should support an ordinary C client calling
`int ap_export_add(int32_t x, int32_t y, int32_t *out)` without any `ap_value`,
environment or compiler headers. Derive/check widths and arity from the typed
Pi/result, not from the alias or script assertions. Preserve wrapping arithmetic
without C signed-overflow undefined behavior. This is an agent-selected target
contract, not a newly accepted mathematical integer model. The initial subset
is now implemented in the unpromoted prototype, as recorded below.

Do not eagerly evaluate every Core APP operand merely to use a C call: the
current runtime deliberately delays it. Use checked value operands and explicit
sequencing; preserve captures, unused arguments, repeated thunks and effects.
Reject unsupported dependent/higher/Identity cases in the native profile while
the structural profile retains its existing support. Scalar native lowering
needs no blanket proof erasure; do not make all of #47 its prerequisite.

For containers, identify the selected nominal family and constructors through
typed owner views. Specify `Rep(source_value, target_buffer)` and input extent,
ownership/aliasing, length and failure conditions before List-to-slice lowering.
Finite C buffers are an explicit foreign input domain, not a replacement of all
unbounded source values. Preserve persistent source inputs via copy-out unless
ownership justifies mutation. Acc can drive recursion and Identity can affect
runtime behavior: source Sortedness alone authorizes erasing neither. An Acc
lowering needs a recurrence/refinement argument, separately from #47 relevance.

Reject adopting the report's template by export spelling or hash. Also reject
mandatory whole-program NF, compile-time print, a generic multi-target framework
before C works, and a large IR hierarchy merely to rename Core nodes. Introduce
only the C-local functions/blocks/operands needed by actual transformations.
Share analysis by appropriate graph/environment/profile keys, not Term alone
where captures or representation differ. Justified A Program precomputation
must use existing mechanisms under the invocation's explicit budget, not a
hidden extra normalization pass.

### Plan

This is #49's sole active implementation checklist; the imported report remains
research/provenance. AP1-AP3 checkpoint work and #47 remain independently open.
New implementation stays under `src/prototype/c_backend/`, with `lower/` only
when needed; promotion remains a separate approved change.

- [x] **AP6.0:** merge PR #50 intact; inspect current emitter, adapter, link
  parser/driver and issue completion boundaries. Do not adopt its template.
- [x] **AP6.1:** extend `link/plan.h/.c` with a versioned lowering/ABI contract
  and explicit unsupported policy, distinct from `native_script`. Reuse one
  plan and named-root resolution. Document native-only versus explicit fallback;
  reject unknown profiles, incompatible signatures and unsupported directives.
- [x] **AP6.2:** derive the first fixed-width callable boundary from admitted
  occurrences in `emit.h` and C-local lowering. Implement native scalar
  constants/functions and checked direct arithmetic/sequence, with actual C
  inputs/results and no structural runtime dependency for supported examples.
  Compare independent C calls with the interpreter, including overflow extrema,
  input-dependent outputs, repeated calls and two modules in one client.
- [ ] **AP6.3:** generalize known saturated calls using shared application-spine
  and capture analysis; retain partial/higher-order adapters only when the
  requested ABI/profile supports them. Test unused arguments, nested captures,
  shadowing, partial application, delayed failure and effect order. No evaluation
  of user effects during compilation or function replacement by name.
  Shared scalar callees, lifted scalar captures, function-returning Fold and
  remaining first-order Pi parameters are now implemented (milestone below).
  Higher-order/dynamic operands and non-scalar capture representations remain.
- [ ] **AP6.4:** lower constructor/match control and define nominal
  representation contracts; test length, append and partition before QuickSort.
  Distinguish semantic justification dependencies from residual executable
  dependencies. Drop only those whose permitted use/realization is established;
  retain necessary private callees, effects and callbacks. Check distinct versus
  intentionally shared families and reject incompatible cross-module exchange.
  Closed nullary enum selections and conditional Match are verified below;
  fieldful/recursive/container representations and shared-family exchange remain.
- [ ] **AP6.5:** implement finite List-to-slice copy-out and justified
  specialization/recurrence lowering for ordinary QuickSort inputs, not a closed
  printed example. Tie Acc/relevance transformations to checked applicability
  and #47 where needed. Record source/target relation and resource/failure
  contract; require independent lowering evidence in addition to source Sorted.
- [x] **AP6.6 (current profiles):** update `link/driver.c` to include only required runtime/helpers
  and emit ABI/profile/transformation/assumption/fallback diagnostics in receipts.
  Preserve staged publication and existing cc/ar/link-script invocation.
  Public headers must expose only the chosen C contract, not compiler internals.
  Failed native requests must not publish a structurally incompatible library.
- [ ] **AP6.7:** add durable upstream native-client/differential/sanitizer gates
  to `c_backend/build.mk`. Cover closed results, open functions, effectful entries,
  private dependencies, unsupported profiles, unchanged `.a`, deterministic
  output and no hidden Solve. Compare sizes/performance only for the same ABI,
  inputs and observations; the 53-vs-10,873 report is not such a comparison.
- [ ] **AP6.8:** push each verified boundary, report separate implementation,
  test and documentation deltas, and update #49 with supported/unsupported cases.
  Close only on reusable native lowering, not on scalar marshalling alone or a
  manual sort template. Other targets are later profiles, not blockers for C.

### Initial Scalar Milestone (2026-09-29)

Objective at `81c830a` plus this prototype change: `scalar_direct_v1` /
`c_scalar_v1` emits actual Int32/Int64 parameters/results, unsigned wrapping
arithmetic, scalar sequencing and known saturated scalar calls. Each export
gets a temporary target DAG; identical aliases share the private function.
One parsed link plan remains authoritative for target policy. No new source
Term former, classifier, admission path, `.a` field or accepted-code edit.
Product receipts identify native/structural lowering; only structural products
include the runtime. See [the executable contract and tests](../src/prototype/c_backend/README.md#native-scalar-profile).

Historical assessment at `3c0ce77`, superseded for scalar calls below: actual lowering
of `mul (add x y) (sub x y)` includes a function-returning Fold and rejects;
the explicit scalar block counterpart succeeds. Retain that rejection test
until AP6.3 handles negative/function results coherently. Known Lambda calls
currently specialize at use sites; reusable private callees and cross-call
analysis are still needed. No QuickSort template, C buffer representation or
Acc/proof erasure was introduced. #49 stays open.

Fresh O2 `check-c-scalar`, `check-c-link`, `check-c-backend` and
`check-c-sorting-boundary` pass on clean accepted `e716232` plus the artifact
overlays through `baabea8` and this backend change. Scalar coverage includes
400 arithmetic cases against the existing evaluator, independent C inputs,
extrema32/64, captures, repeated calls, two components, all four native products,
unchanged artifact hashes and deterministic output. The raw emitter test forbids
evaluator/substitution calls, checks source/proof counts and traverses a shared
4,096-level DAG. Fresh ASan/UBSan `check-c-scalar` also passes, including the
adapter, compiler admission, raw generator and generated C source clients.
AP6.7 is still open for general calls/ADT/effect semantics.
The full compiler acceptance pass at `baabea8` is historical, not rerun for this
backend-only change. AP1-AP3 split-fuel/checkpoint work remains unfinished.

Implementation deltas excluding documentation: `lower/scalar.c` +331/-0,
`lower/scalar.h` +12/-0, `link/driver.c` +30/-15, `link/plan.c` +27/-4,
`link/plan.h` +4/-0: net +385. Tests: `lower/check.sh` +111/-0,
`lower/client.c` +51/-0, `lower/fixture.p` +19/-0,
`lower/oracle_test.c` +148/-0: net +329. `build.mk` +11/-2: net +9.
This adds a native capability rather than removing the reference backend;
it is not a code-reduction milestone. Unrelated dirty files are excluded.

### Shared Native Calls (2026-09-29)

Objective at `3c0ce77` plus this prototype change: the former scalar inlining
path is replaced, not retained in parallel. Pending application operands carry
their original lexical scope through zero-clause Fold. Exported partial/Pi
expressions receive native parameters from their admitted classifier without
requiring a syntactic Lambda at the export root. Known Lambda bodies compile
once per source pointer, supplied widths and free-binder/capture widths;
callers pass actual scalar captures as private C parameters. The same iterative
DAG collector orders expressions and callee dependencies. It rejects cycles.

Assessment: this is target-local specialization and scalar capture lifting,
not source WHNF, a second evaluator or a new typed authority. The read-only
`pg_support_contains` API on the existing support trie avoids scanning each
callee body again for each ambient binder. Absent support metadata refuses
capture analysis rather than treating an open term as closed. C-local variable
numbers count only emitted definitions; extra administrative source nodes do
not perturb names. Do not use source nominal IDs as C data representations.

The formerly rejected `mul (add x y) (sub x y)` now works unchanged, as do
partial applications, nested captures, lexical shadowing and a Fold whose
continuation returns a function. Source/interpreter and independent native C
clients exercise them. The shared-call regression builds a 96-level binary-call
family and requires exactly 97 callee definitions plus one entry, under 100 KB;
runtime execution of that exponential recurrence is deliberately not the test.
The 400 integer comparisons, 4,096-level shared DAG and no-evaluation/source
mutation gates remain. Effectful ignored arguments and higher-order signatures
reject without executing their effects or publishing a bundle.

Fresh O2 `check-c-scalar`, `check-c-link`, `check-c-backend`,
`check-c-sorting-boundary` and `check-support` pass; ASan/UBSan `check-c-scalar`
passes as well. Baseline is clean `e716232` plus the artifact overlays through
`baabea8`, backend `3c0ce77` and this change, excluding unrelated dirty files.
Support tests also check membership after union/binder removal against 32-bit
reference sets and distinguish unknown hand-built metadata from the empty set.
No full compiler acceptance rerun is claimed for this backend/read-only-query
change; the preceding full run remains the `baabea8` result.

Per-file non-documentation delta: `lower/scalar.c` +295/-123,
`lower/scalar.h` +2/-2, `link/driver.c` +1/-1,
`readback_support/support.c` +14/-0 and `support.h` +3/-0 (implementation
net +189). Tests: `lower/check.sh` +11/-5, `client.c` +7/-0,
`fixture.p` +8/-0, `oracle_test.c` +32/-0, `differential.c` +15/-0,
`differential.p` +15/-0 and `readback_support/support_test.c` +6/-0
(test net +89). No accepted implementation/build files changed.

AP6.3 remains open for representations outside scalar known calls; AP6.4/AP6.5
must address ADT/control/container/Acc semantics before native QuickSort is
claimed. #49 remains open, as do the independent AP1-AP3 checkpoint requirements.

### Nullary Representation Milestone (2026-09-29)

AP6.4 implementation decision (agent): start the representation
boundary with explicitly selected, closed, unindexed nullary ADTs. A downstream
`enum32 SOURCE ALIAS` directive selects an admitted nominal type, not just its
erased layout. The extended native profile exposes a distinct C struct tag
with a checked uint32 constructor position and lower Match into selected-branch
control flow. Invalid foreign positions must fail before calling source code.
No eager execution of every branch. Keep the existing scalar contract unchanged.
Do not infer equivalence of nominal families from matching constructor counts;
ambiguous reused erased layouts must reject until typed-edge-directed selection
exists. Independent generated headers do not yet declare shared nominal type
identity. This is a finite representation relation, not ADT erasure, a List
buffer contract or an Acc transformation.

Objective at `7dedefa` plus this prototype change: `native_direct_v1` /
`c_native_v1` reuses scalar lowering with target representation pointers in
callee keys; equal integer widths cannot merge distinct selected families.
`lower/representation.c` borrows existing declaration views and stores its index
only in the temporary backend arena. `main.c` admits type selections through the
same Program/budget/policy as function exports. No new admission authority.
Nullary Match branches reuse private callees/capture lifting and execute only
in their selected switch arm. Function-returning Match and subsequent scalar
arguments are supported. Public enum arguments are range-checked before entry.
The contract is documented in [the backend guide](../src/prototype/c_backend/README.md#native-nullary-adts).

Assessment: a declaration can retain an ambient parameter prefix even when its
selected type occurrence is closed. Checking `parameters == NULL` incorrectly
rejected ordinary source Bool. Instead inspect the existing declaration's index
and field extensions relative to its prefix, plus the admitted closed type.
No artifact repair, schema rewrite, new Core object or persistent C type cache
was needed. Reused erased layouts remain deliberately ambiguous and reject when
two nominal selections try to assign them different representations. The script
and `link.json`, not `.a`, own all representation/ABI choices.

Fresh O2 `check-c-scalar`, `check-c-enum`, `check-c-link`, `check-c-backend` and
`check-c-sorting-boundary` pass; ASan/UBSan scalar/enum gates also pass. Baseline:
clean accepted `e716232` plus artifact overlays through `baabea8` and backend
`7dedefa` plus this change, excluding unrelated worktree edits. Native tests
compare independent inputs with the interpreter, all constructors in two/three
case types, same-shaped distinct families, captures/Fold/curried Match, invalid
tags, unsupported selections, all four products and deterministic checked/trusted
output. Raw fixtures forbid evaluator/substitution calls during emission, assert
source graph/evidence counts unchanged, and reject ambiguous shared layouts.
Input `.a` digests are unchanged after successful and rejected link requests.
The source differential initially lacked imports and assumed C-style `\n`
escaping; these test mistakes were corrected without changing source semantics.
Full compiler acceptance is not rerun for this backend-only milestone.

Non-documentation delta: `lower/scalar.c` +158/-46, `scalar.h` +5/-0,
`representation.c` +92/-0, `representation.h` +25/-0, `link/plan.c` +26/-8,
`plan.h` +4/-1, `driver.c` +16/-3, `main.c` +6/-4: implementation net +270.
Tests: `lower/oracle_test.c` +53/-3, `check.sh` +3/-1, `enum_check.sh` +86/-0,
`enum_client.c` +38/-0, `enum_fixture.p` +23/-0, `enum_differential.c` +22/-0,
`enum_differential.p` +20/-0: test net +241. `build.mk` +5/-1: net +4.
This is a new native representation capability, not a code-reduction milestone.
AP6.3-AP6.5/#49 and independent AP1-AP3 checkpoints remain open. The existing
structural Acc QuickSort test is not evidence of native QuickSort support.

### Issue Audit (2026-09-29)

| Issue | Decision and remaining reason |
| --- | --- |
| #46 | Closed: initial isolated multi-export C library/executable milestone passes. General native arguments/results and AP5.6 continue in AP6/#49; they are not reported as implemented. |
| #44 | Keep open: structural C and checked/trusted QuickSort work, but selected-export admissibility beside unresolved obligations and general dependent/higher Identity target coverage remain incomplete (AP4.6/#47). Failed fuel partitioning is separate, not by itself the reason to retain a C issue. |
| #47 | Keep open: no general checked relevance or partial-artifact admission rule; a link script does not provide either. |
| #49 | Keep open: shared scalar calls and closed nullary ADT/Match representations are verified; higher-order calls, fieldful/recursive data and native QuickSort remain. PR #50 remains a design record, not an implemented sorter template. |
| #41 | Keep open: F1/F2 are accepted and F3/F4 prototypes have passed their gates, but F3/F4 promotion and accepted regression integration remain open in the finite-sorting plan. No new five-backend verification is claimed here. |
| #43 | Keep open: loop/state/result synthesis and logical boundary choices are still undecided. No evidence that the request is unnecessary or disproved. |

No remaining issue is classified as invalid merely because a narrower milestone
works. The #46 closure and this handoff supersede the earlier decision to keep
that packaging issue open for future ABI extensions. Overall goal stays active.

## Research Records

The following PR #45 files are imported verbatim. Their supplied research remains
available; this plan neither rewrites it nor claims to have revalidated its whole
bibliography. The decisions above govern this narrower implementation phase.

- [Artifact/backend audit](2026-09-28-artifact-backend-lowering-audit.md)
- [General recursion audit](2026-09-27-GENERAL-RECURSION-CBPV-AUDIT.md)
- [Empty type, abort and divergence](2026-09-28_cbpv_empty_type_abort_divergence.md)

PR #48 is also preserved as supplied research:
[Linkable components and target link manifest](2026-09-29-LINKABLE-COMPILATION-UNITS-AND-TARGET-LINK-MANIFEST-DESIGN.md).
AP5 is the active implementation checklist for #46. Its assessment, not the
report's illustrative ABI/CLI or pending-proof defaults, governs this increment.

PR #50 is preserved as supplied research:
[Native QuickSort lowering audit](2026-09-29-NATIVE-QSORT-LOWERING-AUDIT.md).
AP6 adopts its target-native direction, not its hand-derived template or an
unverified source-to-target refinement claim.
