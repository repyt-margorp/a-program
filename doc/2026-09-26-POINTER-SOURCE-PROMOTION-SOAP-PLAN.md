# Pointer Source Promotion

## Problem List

1. Promote the verified pointer implementation and isolate the legacy tree.

## 1. Source Layout

### Subjective (User)

User request, translated, 2026-09-26: at a coherent milestone, promote the
pointer version into `src/`, relocate the old version into an archive, and
organize `src/`. Publish verified milestones to main. Reject unsuitable trials
with a written explanation rather than including them in a push.

### Objective (Code)

Baseline: `2f16becdb01bff9d54202930cf2a467b477b47af`. Its preceding full
acceptance run passed. The pointer tree has 408 tracked files; the remaining
legacy prototype has 504. Root `make` still selects the legacy implementation.
Pointer tests read the legacy `.p` fixture corpus but do not link that compiler.
`src/handmade/main.c` is user-authored and outside this promotion.

The primary worktree has inherited, uncommitted Context/IADT edits and two
untracked fixtures. These are not accepted by this promotion. A separate
worktree also contains a new function-signature regression trial; it is not
part of this baseline or layout change. Preserve all of them separately.

### Assessment

Agent implementation decision: move files without changing C/header content,
Core tags, typing rules, wire formats or test assertions. Use `src/` for the
current implementation, `tests/` for its verification, and
`archive/legacy/src/prototype/` for the frozen legacy implementation. Keep the
archived fixture corpus as read-only compatibility input instead of copying
it into a second authority. Historical document paths remain revision-relative;
update current entry points and record this mapping in the active plan.

Do not combine this with Oracle-owner refactoring, function-signature changes,
symbol renames or removal of optional witness construction. In particular, a
file move is not completion of P4/P5. Keep the user-authored handmade directory.
Retain the old Makefile under the archived project root; no legacy source is
linked by the new default build.

### Plan

- [x] Inspect the published milestone, dirty worktrees and external test paths.
- [x] Move the current compiler and tests; archive the legacy tree/experiment.
- [x] Switch root build commands and update current documentation/policy.
- [x] Update fixture paths without weakening tests or rewriting historical logs.
- [x] Verify byte-identical C/header and archived payloads against the baseline.
- [x] Run the complete acceptance gate from the new root Makefile and check
  build entry points from another working directory. Compare representative
  source/image results with the baseline binary.
- [x] Report rename-aware file/line changes; prepare only the verified layout.
- [ ] Relocate inherited local edits to their corresponding new paths without
  accepting them; verify their content and push main.

### Verification

Fresh checks against baseline `2f16bec`, 2026-09-26:

- 354 moved C/header files, or 418 including `.inc`, have zero content changes.
  A blob audit verified 882 unchanged moved payloads, including all 504 archived
  prototype files. The handmade source and old root Makefile are preserved.
- Root/direct Makefile entry points and an external working directory select
  the same current source list; public linker symbol sets are identical.
- The generated inventory changes only its source-location column. All 158
  program paths exist; shell syntax checks pass. No semantic expectation changed.
- Nat addition, indexed append, general Sorted and the ordinary-result Sorted
  theorem were saved at 0/100/full fuel, both ordinary and retained: 24 image
  pairs and 48 opposite-version loads passed. Source status/step output matched;
  20 image pairs were byte-identical. Retained binary order is not a canonical
  equality criterion: repeated saves by the unchanged baseline also differed.
  One cross-loaded retained theorem pair differed by one Solve transition while
  both completed; do not claim bitwise or checkpoint-step determinism here.
  Evidence: `/tmp/a-program-layout-cross.log` and its named image directory.
- Root `make check-acceptance` exited 0, including the 158-program syntax
  inventory, 63/63 compatibility cases, ordinary-result Sorted, all four
  LT/partition variants and optional witness isolation/packets. The final layout
  check also passed separately. Logs: `/tmp/a-program-layout-acceptance.log`
  and `/tmp/a-program-layout-acceptance.time`. Plain `make`, external-directory
  build, `--nf` and explicit `#print` execution also passed. Addition and indexed
  append NF output matched the baseline.

### Change Size

Rename-aware comparison against `2f16bec` (Git `--find-renames=10%`): 912
renames. All 783 C/header/`.inc`/`.p` files are unchanged. Nonzero changes are
build entry points, test paths, the generated inventory and documentation:

| File | Added | Removed |
| --- | ---: | ---: |
| `.gitignore` (archived build output) | 1 | 0 |
| `AGENTS.md` | 9 | 0 |
| `CODING_STYLE.md` | 2 | 0 |
| Root `Makefile` | 1 | 9 |
| `README.md` | 27 | 24 |
| `archive/README.md` | 15 | 0 |
| `archive/legacy/Makefile` (unchanged old root file) | 9 | 0 |
| September 25 active SOAP plan | 7 | 0 |
| This promotion plan | 119 | 0 |
| `examples/type-infer-and-check/README.md` | 6 | 0 |
| `src/Makefile` (relocated) | 374 | 360 |
| `tests/README.md` | 10 | 6 |
| `tests/build_layout.sh` | 23 | 0 |
| `tests/cli.sh` | 1 | 1 |
| `tests/compatibility.sh` | 4 | 4 |
| `tests/compatibility.tsv` (regenerated) | 302 | 302 |
| `tests/image_cli.sh` | 8 | 8 |
| `tests/image_origins.sh` | 1 | 1 |
| `tests/inventory.sh` | 3 | 0 |
| `tests/retained_quicksort.sh` | 1 | 1 |
| `tests/syntax_inventory.sh` | 2 | 2 |

Excluding documentation, additions/deletions are +730/-688, net +42: +16 in
build entry points/ignore rules, +26 in shell verification, zero in compiler algorithms or
test programs. Large changed-line counts mostly reflect relocated path strings,
not extra logic. Documentation is +195/-30 (net +165); total +925/-718 (net +207).
