# Surface Epoch 1 Handoff

Date: 2026-10-03
Status: focused verification passed; files frozen for Core task-branch publication.
Combined gates are running on the immutable final overlay with private outputs.
Baseline: `parallel/surface-20261003`, `2d747ccfec844e8afc72d408385ceb79a7c01808`.
Local edits: exact [file list](../src/prototype/surface/epoch-files.txt).
Related: [active work list](2026-10-03-SURFACE-GOAL.md), Issue #57, PR #58.

## Problem List

1. Deliver a reviewable named-binder correction and migration prototype without
   editing accepted source or crossing the shared Git sandbox boundary.

## 1. Named Binder Prototype Epoch

### Subjective (User)

2026-10-03, user paraphrase of the active Goal: implement and verify the named
Binder correction and `.p` migration; critically read all three audits. Preserve
canonical telescopes, synthesis-first `::` and Core checking authority. Prototype
local `:=` source and brace alternatives while distinguishing agent decisions
from user-approved syntax. Use the isolated Surface worktree and requested model.

2026-10-03, user authorization relayed by Core: worker task-branch commit/push is
allowed, with Main merges reserved to Core.

2026-10-03, explicit user clarification: each worker may commit/push its own task
branch; only Core merges or updates Main. Do not self-merge, promote or close
issues. Publish verified epochs with evidence and send Core the branch/commit,
changes, failures and integration needs; retain the prototype write scope.

2026-10-03, user paraphrase of the released-slot direction: run the combined
acceptance/compatibility/persistence/sanitizer obligations with private outputs
and at most two jobs; notify Core when Surface releases the slot. Older queued
permission/status notes are resolved.

### Objective (Code)

Fresh strict O2 builds use this committed baseline plus the listed prototype
files. Final clean overlay: `/tmp/ap-surface-20261003-final`; tested binaries:
`/tmp/ap-surface-20261003-e2/build`. All four changed source files in the final
overlay match the tested implementation. A byte comparison of every source/header
against the baseline overlay found exactly `syntax.c`, `syntax.h`, `syntax_io.c`
and `synthesis.c` changed. Accepted files and shared `solver_inputs` patches are
unchanged. The only copied test changes are the lane reader patch and eight
named-selector fixtures.

Implementation: explicit positional/ordered/unordered clause mode; LHS local
and RHS source item contract; both lexical scans corrected; RHS owner/case
resolution; ordered subsequence validation; existing complete telescope and
graph/IH associations retained; syntax wire version 2 with mode validation.

Migration: eight active `.p` files, 13 clauses, with old/new hashes in
[migration.tsv](../src/prototype/surface/migration.tsv). The parsed-token migrator
requires `--legacy-source`, preserves other bytes, and rejects doubled input.
Overlay assembly verifies both input and replacement hashes before copying.
Two inline parser fixtures migrate through `test_patches/reader.c.patch`.
The lane README supplies grammar, proof-facing naming and compatibility policy;
accepted README, examples and historical audits remain untouched.

| Fresh gate | Result / scope |
| --- | --- |
| Strict C11 O2 build | `-Wall -Wextra -Werror`; focused binaries built with at most `-j2` |
| `check-surface` | Passed typed evidence, both lexical scans, migrator and source/image matrix |
| Unnormalized evidence | Ordered, unordered, permuted, positional, omitted fields, identity and shorthand terms/classifiers alpha-equivalent; each has four canonical fields plus IH |
| Scope and roles | Distinct aliases, both-valid names, legal shadowing, lexical RHS exclusion, wrong-case selector, duplicate origin/local, invalid `@`/`*`, shadowed graph/IH and wrong proofs covered |
| Synthesis-first assertions | Assertion-free proof passes; wrong `::` rejects; checking code is unchanged |
| Persistence | Exact syntax bytes; structural mode rejection; unsolved/solved v2 image reload and rejected-input reload; chunks 1 and 64 |
| Actual old images | Both pending and completed baseline v1 images reload on baseline and reject on prototype (exit 2) |
| Migrated `reader_test` | Entire reader test passes, including positional/ordinary block/definition parsing |
| `syntax_io_test` | Entire syntax test passes, plus ordered/permuted `.p` inputs; truncation and deep sharing retained |
| Ordinary source smoke | `04_match.p` and `09_list_induction.p` pass: 719 and 1,921 steps |
| Style | New source indentation uses tabs; English/ASCII comments inspected; C-style comments in C/`.p`; shell syntax and diff whitespace checks pass |

Detailed focused logs are under [results/](../src/prototype/surface/results/).
Baseline and corrected named `graphMain` use 10,048 steps at chunks 1/64;
this is semantic control evidence, not a performance benchmark. Per-file draft
additions/deletions and proposed target deltas are in
[deltas.tsv](../src/prototype/surface/deltas.tsv). The source-layer patches total
56 added / 29 deleted target lines, reader migration 5 / 3, and `.p` migration
13 / 13. Draft fixture copies are counted separately from those semantic deltas.
No accepted example is changed.

Remaining failures/limits: local `git add` fails creating the shared worktree's
`index.lock` (`Read-only file system`). No focused test currently fails. Full
acceptance, compatibility, combined artifact checkpoints and sanitizer gates
are now running on the immutable final overlay after Core released the slot;
their results remain outside this frozen focused epoch. No
public-resume repair or exact checkpoint equivalence is claimed. During test
development, a nested end-to-end sample rejected identically on baseline and
candidate; it was unsuitable for isolating the lexical scan and was replaced
by a direct test of the actual scanner, not by a checker change.

### Assessment

2026-10-03, Core/agent provenance clarification: use `apply_patch` for manual
edits. Delegated publication, slot and freeze procedures are Core/agent
operational decisions. This corrects their earlier attribution in Subjective.

2026-10-03, Core operational directions: shared Git metadata remains read-only;
do not bypass it or self-merge. Core can perform task-branch publication after
receiving exact files, chosen message, tests/failures and branch. Check tabs and
English comments. Core's full regression and short remaining gates are finishing;
Surface's full-regression slot remains next and is not yet released. Hand off the
focused epoch with broad gates open. Agent workflow decision: use the exact file
list and hashes below as the frozen publication snapshot. Freeze the 48 focused
files for Core publication; remaining broad-test outputs stay under `/tmp`.

2026-10-03, superseding Core operational status: Core's broad and short gates
are finished and the full-regression slot is released to Surface. The deferred
combined gates are now running; Core retains final integration and Git work.

Agent prototype decisions: unordered `{{...}}` is the preferred named form;
single braces assert increasing canonical order; nonempty entries and identity
shorthand; one direction only; whole v1 syntax payload rejection rather than
automatic migration. Current lexical shadowing is preserved. These decisions
are explicit and testable, but remain unapproved production syntax policy.

The current-head audit's separation of polarity from optional order restrictions
is adopted. Its scope and persistence obligations are covered by focused gates.
The supplied report's blanket-shadowing prohibition is not established by current
code, so a new prohibition is deferred. Rich selector/migration compiler
diagnostics are deferred rather than adding a Job diagnostic contract in this
lane. The performance report's different revision, missing products and adapted
Core tests justify no optimization or authority change here.

Core must review the `synthesis.c` patch against its current owner refactoring;
it is based on the committed `solver_inputs` overlay, not Core's unfinished
local trials. This epoch changes no shared patch and creates no second Context,
graph or solver. Publication is a worker-branch checkpoint, not promotion or
merge. Overall Goal completion remains unproven until the active plan's combined
gates and epoch delivery are satisfied.

### Plan

The single remaining work list stays in the [active plan](2026-10-03-SURFACE-GOAL.md).
Core publication target: `parallel/surface-20261003`.
Chosen commit message: **`Prototype Function Graph local-source binders and .p migration`**.
Stage exactly the files in `epoch-files.txt` (this handoff, active plan and lane
subtree), verify `manifest.sha256`, and commit/push on that task branch. Do not
stage accepted source, unrelated edits or build products. No Main push or merge.

Focused reproduction:

```sh
ARTIFACT_SOURCE="$PWD/src" bash src/prototype/surface/overlay.sh /tmp/ap-surface-review
make -j2 -f src/prototype/surface/build.mk \
	OVERLAY=/tmp/ap-surface-review BUILD=/tmp/ap-surface-review/build check-surface
make -j2 -f /tmp/ap-surface-review/src/Makefile \
	BUILD=/tmp/ap-surface-review/build \
	/tmp/ap-surface-review/build/reader_test /tmp/ap-surface-review/build/syntax_io_test
/tmp/ap-surface-review/build/reader_test
/tmp/ap-surface-review/build/syntax_io_test \
	src/prototype/surface/fixtures/ordered.p src/prototype/surface/fixtures/permuted.p
```

At the released slot, run the existing overlay acceptance/compatibility/source-image
and artifact checkpoint gates, then build the two focused C tests and migrator
with ASan/UBSan in a distinct build directory. Record actual failures, including
pre-existing public-resume failures; do not adapt tests to hide them.

Completion for this handoff: Core receives the pinned, style-checked epoch and
reproduction inputs. This handoff itself does not mark the overall Goal complete.
