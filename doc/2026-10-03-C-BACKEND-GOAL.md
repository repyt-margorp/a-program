# C Backend Goal

Date: 2026-10-03
Status: ready for launch; session `c-backend`, branch `parallel/c-backend-20261003`.
Baseline: committed `eb0aad6` compiler/overlay plus the coordination documents.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
[AP4-AP6](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md), #44/#49.

## Problem List

1. Implement and verify a coherent downstream C realization epoch without
   moving target conventions into A Program semantics.

## 1. Downstream Realization

### Subjective (User)

2026-10-03, English paraphrase: Sub1 develops C-backend design in a separate
tmux Codex session, with `/goal`, `6.1 Sol` and `xhigh`. Main supervises and
steers. Earlier requirements: human-usable C modules are a target tradition;
LinkerScript owns ABI/layout, and `.a` must not grow for backend-only needs.
2026-10-03, English paraphrase of the active Goal: start C-backend/Linker
refinement now, while the coordinator continues Job/Evidence removal. Use task
names, separate directories and reviewed integration epochs.

### Objective (Code)

The baseline provides `src/prototype/c_backend/` with structural emission,
LinkerScript and scalar/enum profiles. AP4-AP6 records incomplete parts; its
test results are historical until rerun. No new success is established here.

### Assessment

Assigned scope: C design, lowering/runtime/link prototypes and lane-local tests.
First critically inspect current #44/#49 and PR input against code. Choose a
reviewable missing target epochs; actual implementation is required, not endless
documentation or a new wrapper IR. Cross-owner defects go to Main.

### Plan

- [ ] Read AGENTS.md, CODING_STYLE.md, the coordination plan and AP4-AP6.
- [ ] Inspect relevant GitHub issue/PR bodies and diffs; preserve user Subjective.
- [ ] Establish a private clean producer overlay and immutable `.a` fixtures.
  Use `ARTIFACT_SOURCE="$PWD/src"` with `solver_inputs/overlay.sh`; no symlink
  may reference Main's dirty tree. Set private `OVERLAY`, `BUILD` and outputs.
- [ ] Select and record one coherent missing target epoch and its acceptance tests.
- [ ] Implement it in `src/prototype/c_backend/`, preserving the downstream boundary.
- [ ] Verify evaluator/C agreement, C module use, negative controls and input
  image immutability; test committed and current producer compatibility.
- [ ] Report per-file implementation/test/doc deltas and measured results.
- [ ] Commit only this lane's changes on its branch when feasible; report to Main.
- Completion: the supported C/Linker refinement described by AP4-AP6 and #44/#49
  is implemented and verified within the agreed downstream scope. Epoch results
  go to the coordinator for integration; unsupported constructs and unresolved
  policy choices remain explicit. One small change does not complete the Goal.

## Work Contract

- Write only `src/prototype/c_backend/`, this lane plan and necessary lane-local
  documents. Do not edit accepted code, shared SE/AP plans, Main overlays or
  artifact codecs/checking/fuel policy. Ask Main to transfer scope if required.
- Do not merge PRs, close issues, push Main, promote code or change model.
  Branch-only push is allowed after focused verification.
- No independent Job/Evidence/equality authority, checker or shared program IR.
- Heavy regression/performance runs require Main's machine slot; focused builds
  use `-j2` at most. Keep generated overlays untracked; detach symlinks before edits.
- Maintain this short SOAP plan in place. Before ending each work turn, record
  current task, actual tests, next action and blockers. Report cross-owner needs
  concretely so Main can steer. Do not claim completion for merely writing a plan.
