# Verified Performance Promotion

Date: 2026-10-03. Status: authorized, scope selection and accepted verification pending.
Related: [central schedule](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
Issues #56/#51. This is a separate promotion epoch and its sole work list.

## Problem List

1. Promote the verified speed improvements and necessary dependencies into the
   accepted implementation while preserving unrelated local work.

## 1. Accepted Performance Change

### Subjective (User)

2026-10-03, English translation of explicit human approval relayed by inquiry
desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: the speed improvements may be
promoted from Prototype into `src/`. Source: the desk's message explicitly
labelled new human approval, quoting the user's original Japanese instruction.
Merge alone owns Main. Select verified changes and necessary dependencies,
compare accepted code, preserve unrelated dirty files/fixtures, update required
tests/docs, verify accepted regressions/parity and commit/push a separate epoch.
Do not blanket-adopt unrelated Surface/C/design work or unverified trials,
waive strict resume failures, or claim a universal speedup.

### Objective (Code)

At receipt, local Main `b33279e` includes reviewed prototype C7 and PR #54;
last pushed Main is `517f05a`. Accepted semantic baseline remains `eb0aad6`.
Unrelated accepted edits in evidence.c/h, iadt.c/h and tests/core.c/iadt.c,
the protected priority plan and two user fixtures have been preserved with
unchanged raw hashes. The existing supervision Goal is active; its original
no-promotion wording is superseded by this explicit approval for selected
performance work only.

Fresh E6 matched baseline/head measurement evidence has 30 passing samples,
three repetitions per variant, exact source/tool/binary pins and no timeouts.
AP trees400 median wall 17.235 -> 5.407 seconds and wrapped-command peak RSS
2971260 -> 1891724 KiB. QuickSort ranges overlap; tiny List includes startup.
Both measured variants share the E6/family/Surface producer; those prototype
dependencies are not automatically authorized for wholesale adoption. Accepted
baseline applicability and the minimal dependency set remain to be inspected.

### Assessment

Agent decision within the new approval: finish publication of already verified
prototype epochs separately, then use an isolated accepted-code review checkout
to select performance changes and inspect dependencies. Preserve dirty Main
files without staging them. Resolve any promotion overlap against their actual
content; do not silently accept unverified local trials or overwrite user work.
Accepted full regression and affected-layer checks must establish the promoted
scope; prototype timing alone does not establish accepted parity.

### Plan

- [x] Record explicit promotion approval and its limits before investigation.
- [ ] Compare verified performance changes with committed accepted code and
  identify the minimal justified dependency set and local-edit overlaps.
- [ ] Implement the selected promotion in an isolated review checkout; update
  meaningful accepted regression tests/docs only as needed.
- [ ] Run affected checks and accepted full regression/parity; preserve actual
  failures and producer-specific strict-resume observations.
- [ ] Review the exact diff, preserve all unrelated dirty bytes, publish the
  separate accepted epoch and report issue-linked scope and verification.
- Completion: the approved performance scope is accepted and verified, with
  necessary dependencies explicit and unrelated local work preserved.
