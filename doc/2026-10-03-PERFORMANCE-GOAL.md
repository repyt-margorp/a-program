# Simplification and Performance Goal

Date: 2026-10-03
Status: historical Goal record; active successor is the Current Performance Goal.
Session: `performance`, branch `parallel/performance-20261003`.
Baseline: committed `eb0aad6` compiler/overlay plus the coordination documents.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
#56 / PR #58; measurement/relation boundaries in #51/#52.

## Problem List

1. Identify and remove unnecessary work without complicating the system or
   duplicating Main's Job/Evidence refactor.

## 1. Simpler Performance Path

### Subjective (User)

2026-10-03, English paraphrase of the explicit human workflow change relayed
by inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: implementation lanes may
continue separate prototype epochs without waiting for Merge publication or
review. Merge owns integration conflicts and may implement their resolutions.
Preserve submitted bytes/commits through immutable snapshots; investigate other
stalls and confirm that next instructions were consumed. This supersedes blanket
review holds. Only actual safety/shared-file dependencies or short exclusive
measurement slots justify narrow holds. Existing heavy-run scheduling and
accepted-source promotion boundaries remain; no new representation/erasure
policy follows from this workflow instruction.

2026-10-03, English paraphrase of the later human clarification relayed by
inquiry desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: improve speed by
deleting unnecessary mechanisms/work, without adding tuning complexity. Required
intermediate computation for resume may consume memory; roughly order-of-magnitude
excess versus other systems is unacceptable. Preserve needed progress/checked
facts, identify redundant graphs/copies/dead transient state, include new
index/ownership overhead, and demonstrate net memory/time effects with meaning,
fuel and resume unchanged. No unsafe borrowing, discarded required progress,
hard 10x target or overlapping heavy benchmark is approved.

2026-10-03, English paraphrase of the human requirement relayed by inquiry desk
thread `019ebfae-06be-7b71-974a-b97505daed4a`: AP tree peak memory of about
1847 MiB after optimization versus native checkers' about 97-529 MiB is excessive.
Improve both speed and memory through software/design changes. Coordinate with
Job/Evidence to pin allocation and retained-owner causes and delete redundant
graphs/copies. Require matched wall/RSS plus semantic/fuel/resume gates; storage
counts alone are not measured RAM gains. Do not use unsafe borrowing or add
wrappers merely for profiling. Existing qualified epochs continue; this status
inquiry does not request heavy reruns.

2026-10-03, English paraphrase: Sub2 investigates performance, preferably by
removing waste and simplifying structures before technical tuning. Compare
relevant tasks with Bend2, Lean, Agda and Rocq. The user is submitting audit
documents/issues now. Use `/goal`, `6.1 Sol`, `xhigh`; Main regularly supervises.
2026-10-03, English paraphrase of the active Goal: use the incoming audit to
start this worker, keep its directory separate and hand off verified epochs.
Session names describe work rather than Sub numbers.
2026-10-03, English paraphrase of the resumed Goal (user message): complete
Issue #56 / PR #58 against this worktree, critically verify the supplied audit
and current-head review, and prototype head/readback/direct-IADT simplification
with independent correctness, Effort and persistence gates and qualified
cross-system measurements. Keep coordinator authority intact, use verified
branch epochs and concise SOAP progress, and do not push Main or merge PRs.
2026-10-03, English paraphrase of the direct explicit user clarification: each
worker may commit and push its own task branch. Only Core merges results or
updates Main; do not self-merge, promote code or close issues. Publish verified,
reviewable epochs with test evidence, and send Core the branch/commit, changes,
failures and integration needs. Continue within the prototype write scope.
This direct clarification supersedes the earlier relayed authorization record;
task-branch publication does not accept or promote this prototype.
2026-10-03, English paraphrase of the direct user role clarification relayed by
Core: Core now specializes in design audit, combined verification and Main merges.
Job/Evidence implementation moves to the sole `job-evidence` worker. Performance
retains captured-head/readback/direct-IADT ownership and continues verification/
handoff; do not merge Main or self-promote. This supersedes Core's earlier dual
implementation/coordinator role, not the prototype or authorization boundaries.
2026-10-03, English paraphrase of the latest direct user clarification:
Performance and Job/Evidence reduction/deletion are closely related and require
concentrated joint verification. C backend is an important downstream bridge
through `.a` plus LinkerScript to C modules, not a driver for expanding A Program.
Prioritize shared verification while retaining the full performance Goal. Do not
add competing Job/Evidence or authority implementations; route shared owner
findings through Core to `job-evidence`.

### Objective (Code)

Fresh launch inspection: branch `parallel/performance-20261003` at `2d747cc`,
initially clean; its parent/compiler baseline remains `eb0aad6`. No earlier
goal-turn outcome is available to classify; this launch begins with inspection.
GitHub REST inspection on 2026-10-03: #56 is open, without comments; PR #58
was already merged at 01:57 UTC, with documentation head `61e724d`. This worker
made no remote mutation. Read the complete supplied performance report and
current-head review. The missing historical raw bundle remains missing.
Private current overlay `/tmp/ap-performance-2d747cc-base-20261003`, built with
`ARTIFACT_SOURCE="$PWD/src"`, strict O2, passes its unchanged Core suite.
Fresh diagnostic gprof: List completes in 1,921 steps; generic sorted proof
with its required provider completes in 602,945. The latter samples primarily
interning/typed construction, so the tree report does not describe every task.
The independently regenerated two-pair tree port completes in 1,442,531 steps;
its obligations/depths are parsed from pinned Bend `7d24b8d` official source.
Local epoch (base `2d747cc` plus `src/prototype/performance/` patches): direct
IADT completes that input in 1,354,087 steps; combined captured-head delivery
in 1,170,277. False Boolean/tree controls reject on the combined prototype at
54,737 / 41,789 steps. These are completed-result counts, not release timings.
Direct IADT passes unchanged Core/IADT, normalization/source checkpoints and
independent capture, field-order, lazy-field, branch-function and neutral cuts.
Combined head delivery passes unchanged IADT and normalization/source checkpoints;
the original Core fixture fails at its old Fold callback observer, then at its
eager-Force budget assumption. Both failures are preserved. An explicit adapter
retains every assertion, tests legacy materialized Force, and recognizes both
Fold names; its Core suite passes. Independent default-policy Force testing:
8 raw transitions, 3,014 final-readback-inclusive WHNF transitions; budget 100
has no result/certificate, chunks of 7 agree, all 9 raw cuts reload in fresh
processes. Source checkpoint exercises 154 lifecycle cuts / 484 verification
steps here, rather than copying the historical 204-cut report.
Fresh canonical final assembly at
`/tmp/ap-performance-2d747cc-final-20261003` passes the reproducible focused
`checks.sh` gate; raw outputs, hashes, deltas and exact handoff files are in
[the prototype bundle](../src/prototype/performance/README.md). ASan/UBSan/leak
checks passed the two independent tests before Core reserved its slot. Captured
Function Graph results agree at chunks 1/64 and wrong proofs reject. Completed
two-pair `.a` images are byte-identical across parent/direct/head (109,937 bytes)
and reload through ordinary rechecking. The parent/head census has unchanged
typed/Job/query counts and 82,578/15,548 retained Core terms. Cumulative wrapped
arena requests are 1,119,482/671,918; these exclude graph.c internals and are
not live memory or RSS. No wall/RSS comparison has been run or published.
Before publication, this worker's `git add` of the exact manifest exited 128
at the read-only shared worktree `index.lock`; no files were staged or sandbox
bypass attempted. Core published the 45-file epoch as
`f3c35558ca8ce2b1c5fac4c7ce3eba63da75b858`. Fresh verification finds all committed
paths/hashes match the frozen manifest, and local HEAD and
`origin/parallel/performance-20261003` both resolve to that commit. This proves
task-branch publication only, with no Main merge or promotion.
Fresh broad strict O2 verification at published implementation `f3c3555`:
the hash-pinned baseline finishes `check-acceptance` at j2 with all 380 recorded
recipes returning zero (342 checks, 19 compiles and 19 directory recipes).
Per-recipe exits and original output are retained under
`/tmp/ap-performance-broad-f3c3555-20261003/`; aggregate Make zero alone is not
used as acceptance. The adapted-head target finds a fresh additional failure:
`identity_io.sh` exits 134 at `identity_io.c:2085`, where `total_result_machine`
requires seeing only the old `computation/total_result/v1` descriptor. The full
run finishes with this sole nonzero recipe; its aggregate Make zero is not a
pass. Original artifact/public partition and broad sanitizer outcomes are
reported separately below; no wall/RSS run occurred.
Independent TotalResult tests now pass on both pinned policies: 279 baseline
and 70 head raw cuts across captured results, trailing arguments, neutral-Force
Fold fallback, neutral projection and overapplied Return. Each cut survives two
inert save/load rounds with exact status/head readiness/task/frame presence and
total steps, then also a separate writer/reader process. The new descriptor and
captured environment are observed on head cuts; neutral Force exercises charged
fallback readback. Depth-1,000 final WHNF readback remains charged, budget 100
has no result/certificate, and chunks of 7 agree at 3,017/3,014 transitions.
The minimal descriptor-only Identity IO adapter now passes its complete script
separately, including all deferred work kinds, fresh writer/resaver/reader and
WHNF transport. `machine_resave` is byte-for-byte unchanged (SHA-256
`6c7a2f721881b5a3152ed5c8c83306b9e9f4b431bf2ae9327fffbbfeac6b6dab`).
Artifact transport, semantic and seven checkpoint gates pass; original history
fails solely at the same unadapted Identity observer. Strict public partitions
freshly reproduce three reload failures on each policy: 1,000+1,000, 1,600+1,600
and completion+0 (baseline 1,921; head 1,915). Both gates remain failed; they are
not expected-failure passes. Broad ASan/UBSan/leak checks finish with 7/44
commands failed: restored-frame readback scratch leaks in TotalResult, normalization
checkpoint, adapted Identity IO, TotalResult fresh-process qualification and Force
read cuts 4/5/6. Reports allocate at `eval_io.c:readback_read`; successful captured
head delivery drops the restored frame without destroying its readback scratch.
The original sanitizer report remains failed under the pinned published epoch.
The strict sanitized baseline TotalResult control passes all 279 cuts without
leaks. The separate corrective overlay changes only `eval.c` among runtime C/
headers; the published implementation and its source hashes remain unchanged.
The corrective broad ASan/UBSan/leak batch is now terminal: all 45 commands pass,
including explicit head success/error/invalid-return cleanup, return-2 fallback,
all 70 TotalResult fresh-process cuts and all nine fresh Force images. The
original seven failed commands remain separate. Corrective full strict O2
acceptance is terminal with all 384 recorded recipes returning zero, using both
explicit adapters; original unadapted failures remain failed.
Corrective strict O2 transport/semantic/history and all seven checkpoints now
pass; strict public partitions retain the same three failures and identical
table. All 70 O2 TotalResult images and nine Force images match the published
head byte for byte, with identical output and raw/final-readback fuel. Two-pair
trees remain 1,170,277 steps with the same 109,937-byte completed ordinary image.
Fresh read-only alignment of Core's combined source manifest verifies all 128
records against Main `341261d`'s disposable trial. Twelve owner/family/Surface
files differ from this pinned producer; evaluator/head/cleanup source hashes
match. Detailed differences and Core-reported log provenance are retained in
`performance_verification/results/combined-source-alignment.json`.
Private cross-system preparation at `/tmp/ap-performance-cross-7d24b8d` pins
Bend 2.0.34/Bun 1.4.2, Lean 4.34.0, Agda 2.7.0.1 and Rocq 9.0.1 with OCaml
4.14.2/Stdlib 9.0.0. All four accept the official four-equality prefix and reject
false Boolean/tree controls: 12 fresh qualified results. Separate
`src/prototype/performance_cross/` helpers reproduce the fixtures/commands and
retain source/tool/result hashes without collecting time/RSS metrics. Lean's
archive and Rocq's private OCI layers match publisher digests; Rocq's incompatible
Debian candidate and library-relocation failures remain separate setup evidence.
Full workload checking, BendTT admission and comparative timings remain open.
The committed baseline has exact Core interning, an evaluator/readback, typed
queries and measurement prototypes. Existing SE/AP findings are historical
until verified at this baseline. #56 / PR #58 now identify current demanded-head
readback, argument-prefix copying and administrative IADT branch application.
Their historical Bend ratios and missing raw bundle are not fresh measurements.

### Assessment

2026-10-03, Root status reconciliation at Main `c2bab35b`: the active successor
is [Current Performance Goal](2026-10-03-PERFORMANCE-GOAL-CURRENT.md), published
from exact two-document task `73d8229`. Its single current issue table and work
list live in [Joint Verification](2026-10-03-PERFORMANCE-JOINT-VERIFICATION.md).
This supersedes the pending-MEM1 schedule and active checklist below; retain
them as historical requirements/progress, with the original frozen worker
Goal at `0539051` unchanged. E9+E10 is qualified and pushed; MEM1 diagnoses
sampled evaluator retention and qualifies a frame-reuse prototype independently.
Actual new wall/RSS gains remain unmeasured and the full Goal remains open.

2026-10-03, Merge operational schedule: MEM1 remains pending after distinct E9
then E9+E10 qualification. Peak1847 MiB is a historical E6 measurement; final
Core10636362->298214 is a retained-count result and does not explain that peak.
Attribute peak/live/cumulative allocation and retained capacity by actual owner,
including new index/ownership overhead. Use existing census/allocator tools,
representative size scaling and a simplest safe deletion with Job/Evidence.
Preserve checked facts and resumable frontiers; no blanket graph deletion follows
from similar records. Current qualification continues; an exclusive later slot
owns matched wall/RSS. No new profiling-only wrapper or authority is planned.

Assigned scope: whole-path measurement, comparative research and one measured
deletion/refactor epoch outside Main's current owners. Do not select a hot path
from intuition alone, or call a smaller scheduler count a memory/time win.
Incoming recommendations need critical code verification, not automatic adoption.
Agent decision: retain existing allocation minima and scheduler/projection paths
for the first isolation. Profile completed tree checks, then delete only the
administrative IADT bridge using existing closure application. No broad cache
or changed acceptance authority is needed. Heavy timing waits for an exclusive
coordinator slot; correctness concurrency follows the latest Core scheduling
decision below. Focused profiles are diagnostic.
Head-interface decision: use immutable owner-local continuation metadata and
the existing Demand frames/codecs. Recognized Match/Return/Thunk heads forward
actual closures; declined heads enter the unchanged materialized algorithm.
Keep old versioned descriptors for legacy state/tests. No new per-invocation
wrapper, scheduling store, cache or evidence authority is introduced. Actual
removed evaluator transitions are no longer charged; retained transitions and
final readback still consume the selected policy's ordinary Effort. Cross-epoch
counts differ; within-epoch partition/checkpoint agreement remains mandatory.
Core published the coherent focused epoch for later integration review; this
is not Goal completion.
Applied runtime delta is +133/-12 lines, net +121, with 25,714 fewer Lambda/binder
calls on the two-pair workload; adding the bounded closure protocol is justified
by deleting repeated runtime construction/readback, not by claiming fewer code
lines. Prefix-copy calls fall from 73,040 to zero, demands stay 73,040, and final
materialization remains. Full public reload/regression, owner-lifetime census,
isolated allocator/projection work and cross-system repetitions remain open.
Current Core operational decisions/reports, 2026-10-03: protocol review finds
owner-local captured head/spine delivery and immutable Oracle descriptors retain
Lambda/App/Ref Core, without another Term enum or acceptance authority. Core
considers focused branch publication appropriate; Main integration still waits
for broader gates. This is Core's review report, not acceptance or promotion.
The published 45-file set in `results/files.tsv` remains the frozen code/evidence
epoch; Core was notified before this status-only plan update. Notify Core before
further changes. Tabs/English comments were checked. Shared Git sandbox
restrictions remain; do not bypass them or self-merge. Original acceptance/observer
failures remain separate even where an explicit adapter is independently tested.
The new Identity IO failure needs a separate independent TotalResult semantic,
fuel and persistence test before adopting any observer migration. Agent decision
within prototype scope: add this test under `performance_verification/`, preserving
the original failure. The descriptor adapter was deferred until independent
baseline/head semantic and all-cut gates passed, then tested separately.
Latest Core review requires unchanged TotalResult readback and every continuation
save/load split, including captured heads and neutral Force, before proposing
that adapter. Keep `machine_resave` and every assertion, add no schema authority,
and send Core the smallest pinned diagnosis/patch without editing its overlay.
Core was notified before these additional files; the published 45-file code/
evidence epoch remains unchanged.
Sanitizer decision: first verify the baseline control, then test a separate
evaluator-only cleanup patch under `performance_verification/` on a private
overlay. Destroy detached frame materialization on accepted/error head returns;
keep scratch on return 2 for the ordinary fallback. Do not change codec/schema
authority or silently replace the published sanitizer failure record. Core was
notified before this corrective work.
Core's live review confirms this evaluator-local correction is within owned
scope and requires explicit success/error cleanup, return-2 fallback, all-cut
readback/fresh-process/checkpoint, sanitizer/leak and unchanged fuel/output
verification. Freeze an exact corrective handoff for review before integration;
do not replace `f3c3555` or its seven failed sanitizer commands. The Job worker
now runs j2 correctness acceptance; no exclusive measurement slot is granted.
Surface's Goal/window finished at the user's explicit request, retaining its
worktree. These operational updates do not change Performance's success criteria.
Core's earlier audit observed 24 broader commands passing before this worker's
terminal 45-command result. Its combined focused gates are now reported terminal:
Main `341261d` canonical producer/family/Surface plus `f3c3555`, separate cleanup
and descriptor-only adapters passes Core/IADT/Synthesis, full Source IO/Identity
IO, head/TotalResult/cleanup units, normalization checkpoint and all 70 separate-
process TotalResult cuts; charged WHNF remains 3,014. Logs are under
`/tmp/a-program-core-performance-combined-*`. Current Identity query/owner,
family `evidence_function` and Surface files differ from this worker's pinned
producer. Core retained its initial failed assembly/build, caused by overwriting
the newer `eval.h` with the accepted header; it reports no runtime defect there.
These are Core reports, not this worker's fresh broader combined verification.
Combined broader gates remain open. This neither changes frozen files nor grants
timing exclusivity. Submit the exact correction after terminal local broad gates;
the full performance Goal remains active.
Historical Core reports: Surface's corrected witness/seven checkpoints/sanitizers
passed, with three public reload failures, and Core merged its prototype as
`7a9a672`. Family-cursor O2/sanitizer/full acceptance and combined Surface/C gates
passed; its parent pooling-boundary control failed as intended. Five censuses and
completed images stayed unchanged; QuickSort requests fell by 592 calls / 28,416
aligned bytes, cumulative counts rather than RAM/time. These reports do not
establish this prototype's gates. The scratch-pool source delta was confined to
`evidence_function.c`, without evaluator/schema/interface changes.
Current scheduling: the machine has 22 logical CPUs, correcting Core's earlier
24+ report. Job and Performance correctness may overlap at j2; superseded
sequential/queued slot notes are historical. Wall/RSS requires an agreed exclusive
slot pausing all checks. Reuse private outputs and avoid duplicate extraction.
These operational decisions belong in Assessment; direct user branch/Main
authorization remains in Subjective. The full Goal remains active.
Coordinator handoff: this worker may prototype captured-head/closure delivery,
direct IADT field application and their diagnostics/tests in its own subtree.
Job/query scheduling, accepted Evidence and artifact frontier remain coordinator
scope. Work from the current overlay, not by blindly applying old patches.
Operational routing after the role change: `job-evidence` has its own worktree
at baseline `5035c7a` (producer `64df10d`); Core will not concurrently edit SE
owner patches. Route producer/query/frontier needs through Core to that worker.
Performance's pinned `f3c3555` gates do not verify the newer combined producer.
Concurrent correctness at j2 remains allowed; old queued Core slot notes are
superseded, while wall/RSS still requires an agreed exclusive slot.
Core's latest operational action is to align baseline, inputs and progress with
`job-evidence` for combined correctness and cost attribution, separating owner
storage removal from traversal counts and time/RSS. Align the next measurement
baseline with the current published producer and Job's tested epoch, rather than
measuring this old producer as the current combined implementation. Exact frozen
epochs remain the integration units. Timing will be coordinated after live
correctness runs finish; no comparative timing slot is granted now.
Agent measurement decision: use common published producer/inputs and isolated
baseline, head/cleanup, tested Job deletion and combined variants where supported.
Qualify completed outputs/fuel/images and combined correctness before timing;
report owner storage and traversal/cumulative-allocation counts separately from
wall/RSS. Core now reports coupling hash-verified frozen Job Epoch 1 with Main
`341261d` and this head/cleanup epoch, applying the Job patch without fuzz. Joint
j2 focused gates are now reported terminal, 11/11 zero, including Source
checkpoint and all 70 separate-process TotalResult cuts, under
`/tmp/a-program-core-job-performance-combined`. The paired genuinely DONE imported
QuickSort census is live; combined broader gates remain open. Preserve the old
rejected fixture separately. Core independently confirms this worker's terminal
384 recipes / zero failures. Use the exact frozen
Job snapshot/publication instead of inferring it from its worktree base or live
epoch-2 files. This is an attribution plan, not a second
Job/Evidence implementation or a claim that unrun combined broad gates passed.
The corrective publication scope is this owning plan plus
`src/prototype/performance_verification/`, as pinned by the external frozen
manifest in its handoff. Published `performance/` and private cross-system files
are excluded. Notify Core before changing any file in the frozen set; delegated
task-branch publication remains separate from integration review and Goal completion.

### Plan

The checklist below is historical. Use the successor's Joint Verification
work list for all current work; publication did not complete the Goal.

- [ ] **MEM1, pending after current qualification:** pin accepted/current-prototype
  producers, identical completed workloads and existing tool inputs; attribute
  peak/live/cumulative bytes and retained capacity among Core, typed, Evidence,
  Job, query, index and scratch owners. Check representative size scaling and
  include all new index/ownership overhead; final counts do not explain peak RSS.
- [ ] Use that attribution with Job/Evidence to select the simplest deletion of
  redundant work/copies/dead transient state. Preserve necessary checked facts,
  suspended frontier and safe lifetime; account implementation additions/deletions.
- [ ] In a later exclusive slot, demonstrate net matched wall/RSS effects with
  semantic, fuel, step-zero and split-resume controls. Report implemented versus
  measured versus pending, retaining pre-existing failures without waivers.

- [x] Read AGENTS.md, CODING_STYLE.md, coordination and relevant SE/AP boundaries.
- [x] Check GitHub for the incoming performance audit/issue/PR. Read and link
  it when available; do not merge, invent its recommendations or wait idle.
- [x] Pin clean parent/current builds using private overlays and output paths;
  use `ARTIFACT_SOURCE="$PWD/src"`, not Main's dirty tree.
- [x] Inventory repeated construction/scan/copy paths; reuse existing measurement
  tools rather than introduce another persistent results/Job/Evidence store.
- [ ] Establish completed-result baseline measurements and profile the cost.
  Coordinate an exclusive timing slot with Main before heavy runs.
- [ ] Align pinned baseline/inputs and verification with Core/`job-evidence`;
  attribute owner storage removal separately from traversal counts and time/RSS,
  and route shared owner findings without competing implementations.
- [ ] Research primary sources for exact tool versions and comparable tasks;
  separate proof/checking/evaluation/native execution and CPU/GPU conditions.
  Four checker commands now have reproducible positive/negative qualification;
  this does not establish full workloads or BendTT admission.
- [x] Propose one simpler measured implementation epoch. Main-owned findings
  are handed off; other changes need an agreed scope before implementation.
- [x] Implement handed-off direct-IADT and captured-head prototypes and focused
  positive/negative tests. Full regression, profiling, sizing/projection isolation,
  public reload comparison and cross-system measurement remain required.
- [x] Prepare and freeze the exact focused publication set, chosen message and
  separate pass/failure evidence. Notify Core before any later file changes.
- [x] Finish strict O2 baseline and adapted-head full acceptance, auditing every
  recorded recipe exit; retain original head Core failures separately.
- [x] Finish artifact transport/semantic/history and seven checkpoint gates;
  reproduce public strict-byte partitions on baseline and head without treating
  reported baseline failures as passes.
- [x] Audit broad ASan/UBSan/leak gates on the published head policy and verify
  the separate cleanup correction; preserve the original seven failed commands.
- [x] Finish corrective full O2 acceptance and freeze an exact separate handoff
  with original/adapted failures, source hashes and unchanged fuel/output evidence.
- [ ] Compare results, time, memory, fuel, object counts and `.a` bytes; report
  implementation/test/doc additions/deletions per file and any tradeoff.
- [ ] Deliver verified branch changes and unresolved findings to Main.
- Completion: the #56 current-head recovery/simplification obligations are
  verified, measurements/comparisons are reproducible and qualified, and no
  duplicate authority or weaker checking is added. Reviewable epochs are
  intermediate results, not replacement success criteria.
  If no valid simplification is supported, provide evidence and rejected paths;
  do not change code merely to satisfy a speed target.

## Work Contract

- Initial writes: this lane plan and a new `src/prototype/performance/` subtree
  for small diagnostics/patches/tests. Do not edit accepted code, shared SE/AP
  plans, Main's solver_inputs patches or C-backend files.
- The sole `job-evidence` worker owns Job/Evidence, `synthesis*`, `typed_query*`,
  `action*`, admission, Identity boundary recovery and artifact/frontier work;
  route findings through Core, which audits/verifies/integrates rather than
  concurrently editing those owner patches.
- The head/readback/IADT prototype epoch is handed off above; report dependencies
  outside that scope before editing. Do not reset fuel accounting, cache effects or add
  blanket proof irrelevance/reflection. Preserve post-synthesis-only `::`.
- Do not merge PRs, close issues, push Main, promote code or substitute model.
  Branch-only push is allowed after focused verification.
- Heavy timing requires an exclusive slot. Core permits concurrent correctness
  with Job's j2 gates; this supersedes sequential regression scheduling.
  This worker uses `-j2` at most; keep outputs private and never edit through
  symlinks.
- Update this concise SOAP work list in place; report actual evidence, next
  action and blockers before ending each turn. Avoid audit-only indefinite loops.
