# Simplification and Performance Goal

Date: 2026-10-03
Status: ready for launch; session `performance`, branch `parallel/performance-20261003`.
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

### Objective (Code)

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
Coordinator handoff: this worker may prototype captured-head/closure delivery,
direct IADT field application and their diagnostics/tests in its own subtree.
Job/query scheduling, accepted Evidence and artifact frontier remain coordinator
scope. Work from the current overlay, not by blindly applying old patches.

### Plan

- [ ] Read AGENTS.md, CODING_STYLE.md and the coordination/SE/AP plans.
- [ ] Check GitHub for the incoming performance audit/issue/PR. Read and link
  it when available; do not merge, invent its recommendations or wait idle.
- [ ] Pin clean parent/current builds using private overlays and output paths;
  use `ARTIFACT_SOURCE="$PWD/src"`, not Main's dirty tree.
- [ ] Inventory repeated construction/scan/copy paths; reuse existing measurement
  tools rather than introduce another persistent results/Job/Evidence store.
- [ ] Establish completed-result baseline measurements and profile the cost.
  Coordinate an exclusive timing slot with Main before heavy runs.
- [ ] Research primary sources for exact tool versions and comparable tasks;
  separate proof/checking/evaluation/native execution and CPU/GPU conditions.
- [ ] Propose one simpler measured implementation epoch. Main-owned findings
  are handed off; other changes need an agreed scope before implementation.
- [ ] Implement an agreed removal/refactor and its focused positive/negative tests.
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
