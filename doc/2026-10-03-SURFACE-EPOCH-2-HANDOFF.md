# Surface Harness Migration and Broad Verification Handoff

Date: 2026-10-03
Status: verified; follow-up task-branch publication pending Core.
Parent: `90939fc45f3b9cbe8e6409d6142eeee783822041`, published by Core on
`origin/parallel/surface-20261003`. No Main merge or promotion.
Related: [owning work list](2026-10-03-SURFACE-GOAL.md), Issue #57.

## Problem List

1. Finish the fixture-dependent harness migration and report broad verification,
   including the inherited public-resume failures.

## 1. Harness Migration and Verification Evidence

### Subjective (User)

2026-10-03, user requirements paraphrased from the owning Goal: verify the full
named Binder correction and `.p` migration within the prototype boundary.
Preserve canonical telescopes, synthesis-first assertions and ordinary checking.
Keep actual failures visible. Worker task-branch publication is authorized;
Core alone owns Main integration.

### Objective (Code)

The frozen parent changes four source-layer modules and eight `.p` fixtures.
Full O2 `check-acceptance check-source-compatibility` on its immutable private
overlay exits 2 with exactly one recipe error: `check-witness-isolation` reports
`complete-graph-cases: expected exit 0, got 1`. Its `sed` expression still targets
`@cons { tailLength; }`, so the migrated `{{...}}` fixture retains its missing
case instead of becoming the positive control. This is a Surface migration
omission, not a baseline checker failure.

The separate prototype test patch changes that expression to doubled braces
(one target line added/deleted), retaining both expected outcomes and all
optional witness linkage controls. The existing `overlay.sh` test-patch loop
already applies it. The corrected full witness gate passes for ordinary and
witness-linked compilers; baseline and corrected positive controls both finish
in 1,427 steps. No C/header implementation or `.p` input changes in this epoch.
A fresh `/tmp/ap-surface-20261003-epoch2` assembly matches every tested final
C/header and `.p` input, and its harness matches the tested follow-up byte-for-byte.

| Fresh verification | Actual result |
| --- | --- |
| Full O2 acceptance plus compatibility | Exit 2; only the harness omission above fails; all other independent acceptance gates pass, compatibility 63/63 |
| Corrected full witness isolation | Exit 0; plain/linked source, semantic negatives, IH/Self, general Sorted, images and output equality retained |
| Artifact transport, semantic and history | Pass; inert loads, nominal separation, untrusted-input rejection, source-image reuse and fuel/history cycles |
| Seven checkpoint targets | Pass: normalization, source, derivation, definition, namespace frontier, namespace body frontier and constructor |
| Strict public `.a` partitions | Fail: `1000:1000`, `1600:1600`, `1921:0` reloads; combined artifact Make exits 2 |
| Fresh baseline partition control | Same three failures; all 40 rows match in every column except absolute image bytes (candidate +1,056) |
| ASan/UBSan | `check-surface`, full reader and syntax-I/O tests exit 0; leak detection enabled, sanitizer errors halt; separate build directory |
| Style/assembly | Tabs and English comments retained; corrected shell syntax and fresh overlay assembly pass |

The original full acceptance exit is preserved; a second full run is not
claimed. The complete failing harness gate was rerun after its one-line
migration. The public-resume failures remain real, including `1921:0` where
bytes and fuel agree but completion status differs. No checkpoint repair or
universal persistence pass is claimed. Whole v1 syntax-image rejection remains
the parent epoch's explicit prototype policy, not approved production policy.

Durable evidence: [results](../src/prototype/surface/results/), including
`epoch-2-gates.tsv`, selected acceptance/artifact log excerpts, complete focused
sanitizer/witness logs, both partition tables and their checked comparison.
Full raw logs remain in `/tmp/ap-surface-20261003-final/{acceptance,artifact,sanitizer}.log`;
the baseline and harness controls are in `/tmp/ap-surface-20261003-followup/`.
Inputs, compiler/flags and binary hashes are pinned in `epoch-2-inputs.json`.

### Assessment

Core released the full-regression slot after finishing its gates. Surface ran
the combined suite with private outputs and at most two jobs, then notified
Core that all scheduled tests and controls were finished and the slot released.
Core published the repaired 48-file parent separately; this follow-up does not
rewrite that commit or promote its source patches.
Core's operational handoff steering requests exact files/hashes/evidence for
branch publication and combined prototype review, without a duplicate full
run; unchanged public-resume failures remain unwaived. Publication and slot
directions are Core operational decisions, not new user requirements.

Agent decision: correct the harness's syntax-dependent fixture transformation,
preserving the existing acceptance expectations. The fresh baseline establishes
that the three public-resume failures precede the Surface change; defer their
repair to the Core owner without weakening tests or changing proof authority.
The final C/header inputs are unchanged, so a complete rerun of the failed gate
plus assembly verification is sufficient for this test-only correction.

Core reports its current family-parameter scratch-frame change is confined to
`evidence_function.c`, with no Surface file overlap. Core retains combined
prototype integration checks; accepted syntax/compatibility policy remains a
later promotion decision. Core assigns the next broad correctness slot to
Performance; ask Core before new CPU-heavy work. Shared Git metadata remains
read-only here; Core performs authorized task-branch follow-up publication.

### Plan

The remaining work list stays in the owning Goal. Publish only the files in
[epoch-2-files.txt](../src/prototype/surface/epoch-2-files.txt), checked by
[epoch-2-manifest.sha256](../src/prototype/surface/epoch-2-manifest.sha256), to
`parallel/surface-20261003` on parent `90939fc45f3b9cbe8e6409d6142eeee783822041`.
Chosen message: **`Migrate witness isolation harness and record Surface regression evidence`**.
Per-file deltas are in `epoch-2-deltas.tsv`; accepted source/tests/examples are
untouched. The original manifest must be verified against its published parent
commit; this follow-up manifest covers the current changed files.

Reproduce on a new private overlay:

```sh
ARTIFACT_SOURCE="$PWD/src" bash src/prototype/surface/overlay.sh /tmp/ap-surface-followup-review
make -j2 -f /tmp/ap-surface-followup-review/src/Makefile \
	BUILD=/tmp/ap-surface-followup-review/build check-acceptance check-source-compatibility
make -k -j2 -f src/prototype/artifact_persistence/build.mk \
	OVERLAY=/tmp/ap-surface-followup-review BUILD=/tmp/ap-surface-followup-review/build \
	CHECKPOINT_TESTS=/tmp/ap-surface-followup-review/checkpoint_tests/ \
	ARTIFACT_TESTS=/tmp/ap-surface-followup-review/artifact_tests/ \
	check-artifact-transport check-artifact-semantic check-artifact-history \
	check-artifact-normalization-checkpoint check-artifact-source-checkpoint \
	check-artifact-derivation-checkpoint check-artifact-definition-checkpoint \
	check-artifact-namespace-frontier check-artifact-namespace-body-frontier \
	check-artifact-constructor-checkpoint check-artifact-partitions
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
make -j2 -f src/prototype/surface/build.mk \
	OVERLAY=/tmp/ap-surface-followup-review BUILD=/tmp/ap-surface-followup-review/asan \
	CFLAGS='-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' \
	check-surface /tmp/ap-surface-followup-review/asan/reader_test \
	/tmp/ap-surface-followup-review/asan/syntax_io_test
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
	/tmp/ap-surface-followup-review/asan/reader_test
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
	/tmp/ap-surface-followup-review/asan/syntax_io_test \
	src/prototype/surface/fixtures/ordered.p src/prototype/surface/fixtures/permuted.p
```

The strict partition target is expected to reproduce a reported **failure**;
that observation is not a passing expected-failure test. No full slot is held
by this handoff. Later Main review remains Core's separate task.
