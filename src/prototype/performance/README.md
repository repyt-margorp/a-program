# Captured-head and direct-IADT experiment

Promotion compatibility, 2026-10-03: overlay assembly recognizes these exact
patches when the accepted implementation already contains them. Accepted decoded
frame cleanup is retained. On that accepted base, `base` and `direct` also retain
the accepted head behavior. Reproduce the original ordinary/head comparison and
original observer failures from the pinned pre-promotion measurement task
`44e479be675aeb59f386372c052f94fe10634dcd`; those historical results are not new
measurements or failures of the promoted accepted tests.

Coherent focused epoch for Issue #56 / PR #58, based on worktree
`parallel/performance-20261003` at `2d747cc` (the full actual revision is
in [results/epoch.json](results/epoch.json)). This is prototype code, not
promotion or completion of the performance Goal. Use the full revision in the
manifest rather than the abbreviated label here.

The direct variant removes `lambda k. k fields...` and uses existing closure
application. The head variant additionally forwards recognized constructor,
Return and Thunk fields as their actual closures. An immutable owner-local
descriptor extends the existing continuation protocol; it adds no per-call
wrapper, cache, frame field, wire layout or scheduling authority. Neutral,
unrecognized and effectful heads retain the existing materialized continuation.
Old versioned descriptors remain registered.

Head readiness is raw evaluator progress. WHNF still performs charged final
readback before publishing its result/certificate. Actual removed transitions
are no longer charged; all remaining evaluation/readback transitions consume
ordinary fuel. Counts differ across implementation epochs. Within an epoch,
one-shot, chunked and checkpoint execution must agree. New continuation names
require an updated decoder; this does not authorize imported progress or
cross-epoch no-rechecking admission. Job/Evidence, scheduling, Identity recovery
and artifact authority stay with Core.

## Reproduction

Run from the repository root with fresh output directories. All outputs remain
outside the repository. The overlay refuses dirty accepted/prerequisite source
and always selects this worktree, not Main's source. `base` is the current
solver_inputs overlay; build accepted code separately using `src/Makefile`.

```bash
experiment=$(mktemp -d /tmp/ap-performance.XXXXXX)
python3 -B src/prototype/performance/fetch_trees.py "$experiment/official"
python3 -B src/prototype/performance/trees.py "$experiment/official/main.bend" "$experiment/trees_2.p" --count 2
bash src/prototype/performance/overlay.sh "$experiment/base" base
bash src/prototype/performance/overlay.sh "$experiment/direct" direct
bash src/prototype/performance/overlay.sh "$experiment/head-original" head
bash src/prototype/performance/overlay.sh "$experiment/head-adapted" head-adapted
bash src/prototype/performance/checks.sh "$experiment/head-adapted" "$experiment/checks"
```

`checks.sh` builds with `-j2` and strict C11 O2 warnings, then runs adapted Core,
unchanged IADT and evaluator I/O, independent direct/head tests, normalization
and source checkpoint tests, nine fresh-process head reloads, and captured
Function Graph result/rejection controls. It is a focused gate, not full
acceptance. It performs no wall/RSS measurement.

Preserve the original Core failures separately:

```bash
make -s -j2 -f "$experiment/head-original/src/Makefile" BUILD="$experiment/head-original/build" "$experiment/head-original/build/core_test"
"$experiment/head-original/build/core_test" > "$experiment/core-original.log" 2>&1
```

The last command currently aborts at the old Fold pointer observer. After
changing only that observer to recognize both names, it aborts at the eager
Force raw-budget assertion. The separately saved [first](results/core-original-failure.txt)
and [second](results/core-observer-failure.txt) failures are not passing tests.
`core_adapter.patch` retains every original assertion, uses a test-only legacy
materialized-Force dispatcher for the original eager fixture, and recognizes
both Fold names. Independent `head_test.c` checks the default policy instead.
Passing adapted Core must never be reported as unchanged Core passing.
These saved failures are fresh reproductions in this epoch. The supplied
historical report's original failures remain separately recorded in that report;
its lost original raw logs have not been recovered or replaced.

For call-count profiling, build the CLI from each overlay with
`CFLAGS='-std=c11 -Wall -Wextra -Werror -O2 -g -pg'`, run from its private profile
directory on the generated source with `--steps 10000000`, then run `gprof`.
The saved flat-profile call tables contain all reported symbols. These short
profiles have too few samples for reliable wall-time percentages; none are
claimed. Reuse `artifact_allocation_audit` via `build.mk`, supplying
`ARTIFACT_TESTS="$overlay/artifact_tests/"` to select the current adapted census.
Wrapped allocation counts exclude calls within graph.c and are cumulative,
not live bytes, block capacity, peak RAM or RSS.

## Focused results

Fresh 2026-10-03 results, GCC 14.2.0, strict O2. Same independently regenerated
two-pair source; four concrete computational equalities. The full 400-pair
comparison has not run here.

| Configuration | Completed Solve steps | Live Core terms | `.a` bytes |
| --- | ---: | ---: | ---: |
| Accepted source | 1,634,155 | Not measured | Not measured |
| Current solver_inputs parent | 1,442,531 | 82,578 | 109,937 |
| Direct IADT | 1,354,087 | Not measured | 109,937 |
| Combined head/direct | 1,170,277 | 15,548 | 109,937 |

The three overlay images are byte-identical, SHA256
`dfbe0cf0a17fc80bb55cfb788618de038bcbb98a1c3f523f1951546ca8ff4717`.
Each reload completes through ordinary rechecking; this does not establish
the separate public exact-resume gate. The allocation census keeps the same
typed records/Job/query counts; wrapped requests decrease from 1,119,482 to
671,918, aligned requested bytes from 73,129,488 to 41,420,688. Retained-memory
owner/lifetime attribution remains required.

The [profile summary](results/profile-summary.tsv) records 73,040 prefix-copy
calls in the parent and zero in the combined probe; materialization calls fall
from 600,511 to 26,055, while demands remain 73,040. Lambda/binder calls decrease
by exactly 25,714, matching the removed IADT bridge. Final materialization
remains. These are call counts, not speed factors.

Independent Force: 8 raw transitions, all nine split cuts; final WHNF charges
3,014 transitions. At budget 100 there is neither result nor certificate.
Chunks of 7 agree with one-shot. All nine raw cuts reload in fresh processes.
Independent direct tests cover distinct captured scopes, field order, an unused
divergent field/branch, a branch returning a function, zero-field selection and
neutral fallback, at every raw cut. ASan/UBSan with leak detection passed these
two focused tests before Core reserved the measurement slot.

Unchanged IADT/evaluator I/O and normalization/source checkpoints pass. The
source checkpoint has 154 lifecycle cuts / 484 charged verification steps here;
normalization covers 53/7/145/12 cuts. Captured Function Graph outputs agree at
chunks 1 and 64; wrong graph and wrong-proof controls reject. Boolean/tree false
port controls reject at 54,737 / 41,789 steps. Full O2 acceptance, broader
sanitizers and public reload partitions remain pending in this lane.

## Cross-system boundary and remaining work

`fetch_trees.py` retrieves and hash-checks the unchanged official Bend, Lean,
Agda and Rocq tree sources at Bend revision `7d24b8d0235cb9781140512c0f163c48ea84a719`.
`trees.py` parses the actual depth sequence and checks its source hash. The AP
port uses induction and an indexed equality datatype checked through
post-synthesis `::`. Helper definitions and proof encodings differ; unrelated
Bend copy/Sigma helpers are not ported. This is corresponding computation,
not an identical kernel workload or a universal tree proof.

Primary references: the [pinned official workload](https://github.com/bendlang/bend/blob/7d24b8d0235cb9781140512c0f163c48ea84a719/bench/checker/trees_400/main.bend),
[Lean proof checking](https://lean-lang.org/theorem_proving_in_lean4/Propositions-and-Proofs/),
[Agda interface controls](https://agda.readthedocs.io/en/v2.7.0.1/tools/command-line-options.html)
and [Rocq conversion strategies](https://rocq-prover.org/doc/v9.0/refman/proofs/writing-proofs/equality.html).
Exact runtime/tool versions and process/interface/cache conditions must be
pinned before measurement. Bend ordinary checking and BendTT admission must
be separate runs; no tactic/native evaluation should silently replace conversion.
None of Bun/Lean/Agda/Rocq is installed on this lane's PATH at inspection.
No current cross-system speed ratio is claimed. Historical missing products
remain missing; this is a new reconstructed current-head bundle.

Core reports its heavy gates finished; Surface holds the next full-regression
slot. Do not run or publish wall/RSS comparisons until Surface releases it.
This slot update does not establish this prototype's broad gates. Next obligations
remain in [the active SOAP plan](../../../doc/2026-10-03-PERFORMANCE-GOAL.md):
full gates/public reload characterization, reproducible interleaved completed
measurements, live owner/lifetime census, independent arena/index isolation,
projection/scheduler findings handed to Core, and qualified cross-system runs.

## Core handoff

Branch: `parallel/performance-20261003`. Chosen commit message:
`Prototype captured-head delivery and direct IADT branch application`.
No commit, push, Main merge, PR merge or promotion was performed by this lane.
Core may commit/push the exact [file list](results/files.tsv) on its behalf;
merge review remains separate. The goal remains active after this focused epoch.

Applied implementation delta against the current parent: eval.c +27/-1,
eval.h +13/-0, iadt.c +43/-7, computation.c +50/-4: +133/-12, net +121 lines.
This deletes repeated runtime work while adding a bounded closure protocol;
it is not a net source-code deletion claim. Test, tooling, evidence and doc
additions are listed separately in [results/delta.tsv](results/delta.tsv).
Canonical final assembly and focused checks passed from fresh directories;
profile/count sources differ only in subsequent English comment formatting.
Tabs/English comments, patch application and ordinary whitespace were checked.
