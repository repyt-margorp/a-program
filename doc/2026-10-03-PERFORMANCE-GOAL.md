# Simplification and Performance Goal

Date: 2026-10-03
Status: in progress; focused publication epoch frozen for protocol review.
Session: `performance`, branch `parallel/performance-20261003`.
Baseline: committed `eb0aad6` compiler/overlay plus the coordination documents.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
#56 / PR #58; measurement/relation boundaries in #51/#52.

## Problem List

1. Identify and remove unnecessary work without complicating the system or
   duplicating Main's Job/Evidence refactor.

## 1. Simpler Performance Path

### Subjective (User)

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
Fresh task-branch publication attempt on the same local epoch: `git add` of the
exact manifest exits 128 because the shared worktree `index.lock` path is on a
read-only filesystem. No files were staged; no commit/push or sandbox bypass
occurred. Core's delegated publication path remains necessary.
The committed baseline has exact Core interning, an evaluator/readback, typed
queries and measurement prototypes. Existing SE/AP findings are historical
until verified at this baseline. #56 / PR #58 now identify current demanded-head
readback, argument-prefix copying and administrative IADT branch application.
Their historical Bend ratios and missing raw bundle are not fresh measurements.

### Assessment

Assigned scope: whole-path measurement, comparative research and one measured
deletion/refactor epoch outside Main's current owners. Do not select a hot path
from intuition alone, or call a smaller scheduler count a memory/time win.
Incoming recommendations need critical code verification, not automatic adoption.
Agent decision: retain existing allocation minima and scheduler/projection paths
for the first isolation. Profile completed tree checks, then delete only the
administrative IADT bridge using existing closure application. No broad cache
or changed acceptance authority is needed. Heavy timing/full regression waits
for an exclusive coordinator slot; focused builds/profiles are diagnostic.
Head-interface decision: use immutable owner-local continuation metadata and
the existing Demand frames/codecs. Recognized Match/Return/Thunk heads forward
actual closures; declined heads enter the unchanged materialized algorithm.
Keep old versioned descriptors for legacy state/tests. No new per-invocation
wrapper, scheduling store, cache or evidence authority is introduced. Actual
removed evaluator transitions are no longer charged; retained transitions and
final readback still consume the selected policy's ordinary Effort. Cross-epoch
counts differ; within-epoch partition/checkpoint agreement remains mandatory.
Coherent focused epoch is ready for Core commit/push review, not Goal completion.
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
The exact 45-file set in `results/files.tsv` is frozen; notify Core before any
changes. Tabs/English comments were checked. Shared Git sandbox restrictions
remain; do not bypass them or self-merge. Core may perform the authorized branch
commit/push separately from integration review. Original acceptance/observer
failures remain separate even where an explicit adapter is independently tested.
Core reports earlier heavy gates finished and Surface owns the next regression
slot; focused Core Identity/Synthesis O2 tests now run with at most two build
processes. Continue protocol/research/focused correctness/profiling work; no
comparative wall/RSS runs or publication until the coordinator releases the slot.
Report when an exclusive measurement slot is needed. Older queued slot/permission
notes are superseded, not fresh tasks. These Core decisions/reports belong in
Assessment, correcting the earlier attribution of the latest publication/slot
notes to direct User messages. The latest direct explicit user branch/Main
authorization is recorded in Subjective. None of these reports proves this
prototype's unfinished broad gates; the overall Goal remains active.
Coordinator handoff: this worker may prototype captured-head/closure delivery,
direct IADT field application and their diagnostics/tests in its own subtree.
Job/query scheduling, accepted Evidence and artifact frontier remain coordinator
scope. Work from the current overlay, not by blindly applying old patches.

### Plan

- [x] Read AGENTS.md, CODING_STYLE.md, coordination and relevant SE/AP boundaries.
- [x] Check GitHub for the incoming performance audit/issue/PR. Read and link
  it when available; do not merge, invent its recommendations or wait idle.
- [x] Pin clean parent/current builds using private overlays and output paths;
  use `ARTIFACT_SOURCE="$PWD/src"`, not Main's dirty tree.
- [x] Inventory repeated construction/scan/copy paths; reuse existing measurement
  tools rather than introduce another persistent results/Job/Evidence store.
- [ ] Establish completed-result baseline measurements and profile the cost.
  Coordinate an exclusive timing slot with Main before heavy runs.
- [ ] Research primary sources for exact tool versions and comparable tasks;
  separate proof/checking/evaluation/native execution and CPU/GPU conditions.
- [x] Propose one simpler measured implementation epoch. Main-owned findings
  are handed off; other changes need an agreed scope before implementation.
- [x] Implement handed-off direct-IADT and captured-head prototypes and focused
  positive/negative tests. Full regression, profiling, sizing/projection isolation,
  public reload comparison and cross-system measurement remain required.
- [x] Prepare and freeze the exact focused publication set, chosen message and
  separate pass/failure evidence. Notify Core before any later file changes.
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
- Main owns Job/Evidence, `synthesis*`, `typed_query*`, `action*`, admission,
  Identity boundary recovery and artifact/frontier work. Report those findings.
- The head/readback/IADT prototype epoch is handed off above; report dependencies
  outside that scope before editing. Do not reset fuel accounting, cache effects or add
  blanket proof irrelevance/reflection. Preserve post-synthesis-only `::`.
- Do not merge PRs, close issues, push Main, promote code or substitute model.
  Branch-only push is allowed after focused verification.
- Heavy timing/regression runs must not overlap other heavy work. Focused builds
  use `-j2` at most; keep outputs untracked and never edit through symlinks.
- Update this concise SOAP work list in place; report actual evidence, next
  action and blockers before ending each turn. Avoid audit-only indefinite loops.
