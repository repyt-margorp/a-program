# Problem List

## 1. Publication context: Historical checker-performance experiments

### Subjective (User)

2026-10-03, English paraphrase: the project owner supplied this document and
requested current-head verification, then explicitly authorized Issues and a
new documentation PR. Implementation changes are outside this submission.

### Objective (Code)

Verification revision: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
The supplied report pins AP experiments to 9146893 plus an experimental overlay.
Fresh inspection at eb0aad6 confirms the main implementation mechanisms remain,
but its benchmark scripts, patches and raw products were not supplied or found in
fetched main. Its timings are not current-head measurements and are not freshly
reproduced by this submission.

### Assessment

The supplied body below is preserved verbatim as author-supplied material.
Its assertions, historical reports and proposals must be read together with
[the revision-pinned current-head review](2026-10-03-NEW-AUDITS-CURRENT-HEAD-REVIEW.md).
That review controls what this submission independently verifies.

### Plan

- [x] Preserve the complete supplied document, without silently rewriting it.
- [x] Attach current-head qualifications and separate implementation boundaries.
- [ ] Complete the follow-up work in [Issue #56](https://github.com/repyt-margorp/a-program/issues/56).

---

# Problem List

1. Recover the checker performance audit after the workspace interruption.
2. Separate measured compiler overheads from remaining hypotheses.
3. Define the next reproducible investigation and preserve progress.
4. Identify remaining evaluation costs with fresh instrumentation and controlled trials.

## 1. Audit Recovery and Evidence Status

### Subjective (User)

- 2026-10-01, user paraphrase: compare A Program's prototype with Bend2, including runtime and theorem-checking time; isolate the compiler costs in descending impact with controlled constant-factor experiments.
- 2026-10-01, user paraphrase: write progress and audit findings to Markdown continuously, ready for later inclusion in `doc/`.
- 2026-10-02 15:35 JST, user paraphrase: report whether the investigation has progressed.

### Objective (Code)

**Evidence status: Sections 1–3 reconstruct historical results. Section 4 records fresh reproduced experiments, raw evidence and explicit validation limits.**

The previous session measured and profiled the checker candidates, but its final documentation update failed when the execution environment disconnected. On this turn, execution access returned. The visible workspace is an earlier snapshot: the original runtime comparison document and QuickSort products exist, but the later checker audit, checker patches, raw checker JSON/profile files, latest AP worktree and temporary candidate builds are not present at their previous locations. Exact-title Library searches did not return either checker audit title as a matching file; unrelated fuzzy results were not used.

This report recovers the observations retained in the conversation and tool receipts. It does not fabricate missing logs, hashes, binaries or patches. The historical numbers below were measured in the prior session, but they cannot currently be independently reproduced from the retained workspace alone. The raw-data and implementation-recovery gap remains open.

Historical measurement revisions:

| Component | Revision / configuration |
| --- | --- |
| A Program checker base | `9146893f07f0fe3f4c8309c370420c1702dfb96d`, artifact/readback/conversion candidate |
| Bend2 | `7d24b8d0235cb9781140512c0f163c48ea84a719`, v2.0.34 |
| AP release compiler flags | `-std=c11 -Wall -Wextra -Werror -O2`, GCC |
| Bend checker runtime | Bun 1.4.2 |
| Previous machine | Linux x86_64, AMD EPYC 9V74; 8-vCPU quota, 8 GiB memory limit |

The recovered local AP checkout is at `729d7d1add161d69a21c15a2954d5eb8de2f4a4b`; it is not the later revision used for the checker experiments. No claim about today's remote HEAD or performance is made by this recovery report.

### Assessment

Agent conclusion: the timing investigation progressed materially in the previous session, but did not continue unattended after that turn ended. Today's new work is recovery and documentation. A durable summary of historical results is useful, but is not a substitute for recovering the experiment patches and rerunning the measurements.

### Plan

- [x] Inspect restored execution access and the surviving workspace.
- [x] Check for a matching persistent audit document.
- [x] Reconstruct a standalone, doc-ready audit with explicit historical provenance.
- [x] Reconstruct equivalent checker patches and source generator at the fixed revision (Section 4); the lost original files remain unavailable.
- [x] Create new raw samples and a residual profile at the fixed revisions (Section 4); these do not replace the missing historical raw evidence.
- [ ] Revalidate against the then-current AP revision as a separate comparison.

## 2. Historical Checker Measurements and Causal Findings

### Subjective (User)

User paraphrase: treat theorem-checking speed as a compiler investigation, separately from generated C improvements; compare corresponding Bend compiler code where necessary and expose the overheads individually.

### Objective (Code)

The matched workload was Bend's official `trees_400`: **400 pairs, 800 concrete equalities**, with the same depth sequence in the AP port. They check `alltrue(full(n)) = true` and `mirror(full(n)) = full(n)` at selected concrete depths. These are computational equality checks, not a universal theorem about all trees. AP uses its own datatypes, induction and equality syntax; helper definitions and proof encodings differ.

Timing includes fresh-process startup, parsing, elaboration and ordinary checking. It excludes generated C emission/compilation, application execution, artifact saving and artifact rechecking. AP received a 10-billion Solve-step budget. Bend's independent BendTT kernel path (`--verdict`) was **not** timed. The other AP official checker families were not ported or timed in this experiment.

Final historical confirmation used three interleaved AP rounds, each ordered base → direct+small → direct+small+input, after separate initial and repeated configuration runs. No heavy builds overlapped timings. Bend had a separate initial process plus five fresh timed processes.

| Configuration | Historical median seconds | Solver transitions |
| --- | ---: | ---: |
| Bend ordinary checker | 1.836936 | Different work-step protocol |
| AP base, interleaved rounds | 28.088683 | 226901949 |
| AP direct fields + small allocations | 13.493432 | 213140829 |
| AP above + borrow retained normalization input | 12.011445 | 213140829 |

Per-round base/best ratios were **2.460, 2.299, 2.252**, median **2.299**. The borrowing-only conditional ratios were **1.081, 1.153, 1.123**, median **1.123**. The best AP median remained approximately **6.54 times** Bend's ordinary-checker median. This remaining ratio was not causally decomposed.

An earlier sequential batch used one separate initial process plus three fresh timed processes per configuration:

| Configuration | Median seconds | Timed range seconds | Base/variant |
| --- | ---: | --- | ---: |
| Base | 27.247801 | 26.943–28.525 | 1.00 |
| Small first arena block | 19.485716 | 19.144–21.035 | 1.40 |
| Direct match fields | 23.313585 | 22.993–23.504 | 1.17 |
| Direct fields + small arena/index | 12.783611 | 12.128–13.508 | 2.13 |
| Above + borrow retained input | 11.693069 | 11.337–12.113 | 2.33 |

An end-of-batch base guard took 24.987897 seconds, demonstrating drift. Therefore the results support an approximately 2.3 improvement locally, not an exact universal speed multiplier or independent multiplication of the single-change factors.

The following correctness checks historically passed for the combined candidate and the combined+input candidate: `core_test`, `iadt_test`, `tests/eval_io.sh`, artifact normalization checkpoint tests and artifact source checkpoint tests. These cover capture, indexed/dependent elimination, lazy fields, split budgets, write/read, lexical relocation, ownership guards and checkpoint boundaries. The source checkpoint suite reported 204 complete lifecycle cuts. The combined+input positive two-equality control accepted in 69135 transitions; false Boolean/tree controls rejected at 69211 and 61525. These are previous test results, not tests rerun in the restored environment.

### Assessment

Agent findings, ordered by measured impact in this workload:

| Priority | Cause / candidate | Historical result and interpretation |
| --- | --- | --- |
| 1 | Oversized first blocks in short-lived arenas | Change the first block from 16 KiB to 1 KiB; retain 16 KiB later blocks. About **1.40** improvement alone, with unchanged transitions. |
| 2 | Administrative lambda/binder bridge for every constructor match | `iadt.c:apply_fields` builds `lambda k. k field_0 ...` and beta-reduces it. Direct argument closures preserve the branch's captured environment while avoiding that bridge. About **1.17** alone, **6.06%** fewer transitions and about **33%** lower peak RSS (approximately 2.86 → 1.92 GiB). The allocator interaction makes the combination substantially faster than either isolated result suggests. |
| 3 | Repeated checking-input lookup during normalization | Once the reduction owner retains its exact input, stop repeatedly accessing `pg_evidence_subject(...)->core`. Interleaved conditional improvement: median **1.123**, range **1.081–1.153**, with unchanged transitions. |

The direct+small patch also changed the initial index capacity from 64 to 8 buckets. Index-only benefit was not established, so the combined allocation factor cannot be attributed entirely to one allocation constant. No new type rule, assumed proof result or equality weakening was introduced by the successful candidates.

Rejected or deferred experiments:

- Exact per-machine closure/environment demand memo: 538720 hits of 11379764 requests (**4.73%**); less than 1% fewer transitions, but about 31.9 seconds alone and higher memory. It also lacked complete persistence integration. This rejects that specific cache for this workload, not every possible sharing design.
- Capture-safe evaluation binder reuse: zero reusable binders and unchanged full-workload transition count. The initially suspected four-million reification-binder cost was actually mostly the administrative match bridge.
- Inline unchanged materialization root: no established speed benefit and higher retained memory. Direct+small+terminal was about 13.28 seconds / 2.27 GiB in one diagnostic sample, versus about 13.02 seconds / 1.92 GiB without terminal.
- Diagnostic `-O3 -flto`: about 12.52 seconds versus 13.02 seconds for direct+small in single scans; not repeated or established as an optimization. A `maybe-uninitialized` warning was made nonfatal for this diagnostic build only; production warning relaxation was not recommended.
- Existing `solver_inputs` candidate: 25.049 seconds median versus historical base 25.533, only 50868 fewer transitions (**0.0224%**); its nominal difference was within sample variation.

### Plan

- [x] Recover the isolated and combined historical factors with their limits.
- [x] Preserve negative results and the corrected binder-cost attribution.
- [x] Restore reviewable equivalent patches under `src/prototype/bend_benchmark/remaining_products/`.
- [x] Rerun correctness checks and interleaved timings; Section 4 identifies original versus adapted test results. No adoption is proposed by this report.

## 3. Residual Costs and Next Investigation

### Subjective (User)

User paraphrase: continue comparing compiler mechanisms until the causes of the verification-time difference are identified, with progress and audit findings recorded along the way.

### Objective (Code)

Historical gprof profiles used separate `-O2 -g -pg` builds. Their instrumented wall times (base 51.446 seconds; best 28.197 seconds) are **not release benchmark times**. Gprof excludes substantial shared-library/profiler cost, so attributed percentages cannot be read as complete wall-time shares or directly compared with Bun CPU sampling percentages.

| Historical profiled calls | Base | Best |
| --- | ---: | ---: |
| `normalization_step` | 226430306 | 212669186 |
| `pg_lambda` | 4044426 | 79866 |
| `pg_binder` | 4037468 | 72908 |
| `intern_support` | 4073335 | 108772 |
| `intern` | 29837760 | 19499680 |
| `pg_evidence_subject` | 226521523 | 96076 |
| `pg_materialize_step` | 96122654 | 92699614 |
| `reify_advance` | 64301483 | 60878443 |
| `pg_eval_demand` | 11379764 | 11379764 |
| `pg_alloc` (arena requests) | 177405433 | 151053193 |

Normalization accounted for about **99.8% of Solve transitions**, not 99.8% of wall time. The combined changes remove 13761120 transitions. Millions of demands and materializations still remain.

Historical prefix checks used complete cycles of both official depth patterns at 30 and 120 pairs:

| Pairs / equalities | Base transitions | Best transitions | Base seconds | Best seconds |
| --- | ---: | ---: | ---: | ---: |
| 0 / 0, helpers only | 4514 | 4514 | 0.010 | 0.010 |
| 30 / 60 | 16983329 | 15953693 | 1.326 | 0.771 |
| 120 / 240 | 67918505 | 63799961 | 6.058 | 2.968 |

These timings are single diagnostic samples. Counts are almost exactly fourfold at the same depth distribution; setup/helper work is tiny relative to repeated normalization. The 400-pair file ends partway through a depth cycle.

At pinned Bend revision `7d24b8d`, `bend2/bend.ts` provides useful source-level counterparts: `term_cell` retains existing lazy cells; the VAR return frame publishes a computed head into its cell; MAT pushes constructor fields directly; `compare_go` unifies equal share cells. AP's demand continuation materializes closure/arguments to immutable Core before resumption. These source differences motivate investigation but do not prove the remaining 6.54 timing ratio's cause.

### Assessment

Remaining hypotheses, without assigned optimization factors:

1. **Closure materialization versus lazy constructor fields:** investigate a resumable head/closure interface that avoids reconstructing fields before they are needed. Preserve lexical capture, immutable dispatch policy, serialization ownership and ordinary checking.
2. **Per-transition dispatch/owner overhead:** about 212.7 million normalization calls remain. Reduce repeated lookups or administrative transitions while preserving fuel charging, scheduler wake order and checkpoint boundaries. Safe grouping has not been implemented or measured.
3. **Temporary allocation/index lifetime:** 151 million arena requests remain. The failed inline-root experiment demonstrates that removing a table allocation can instead enlarge retained frames. Measure lifetime before adding pools or broader caches.

These tree checks do not require floats, unsigned machine integers or nontermination syntax. Those requirements belong to the separate native runtime-suite investigation. Numeric operations and C lowering cannot by themselves explain this ordinary-checker benchmark.

### Plan

- [x] Recover the residual profile and scaling observations with explicit provenance.
- [x] Reconstruct and preserve new experiment source, patches, exact commands and raw outputs.
- [x] Measure lazy-field/materialization changes with positive and false-equation controls.
- [ ] Measure dispatch changes without weakening split-fuel or checkpoint invariants.
- [ ] Port additional AP checker families and measure BendTT admission separately.

## Recovery Progress Record

| Date / stage | Progress | Evidence status |
| --- | --- | --- |
| Previous session, before interruption | Runtime comparison, full tree port, scoped checker trials, repeated/interleaved timings, correctness tests and profiles executed | Historical observations retained in conversation; later checker raw files currently unavailable |
| 2026-10-02, access recovery | Execution access works; earlier runtime files survive; later checker worktree/artifacts are absent at their previous paths | Fresh local inspection |
| 2026-10-02, documentation recovery | This report reconstructs final results, rejected experiments and the remaining work | Recovered summary, not a fresh benchmark |

For future inclusion in `doc/`, retain this recovery/evidence-status section until source patches and raw measurements have been restored. Do not silently relabel historical results as current measurements.

## 4. Remaining Evaluation Cost Investigation

### Subjective (User)

2026-10-02 15:44 JST, user paraphrase: identify what else is slow and locate the concrete problems, rather than only naming broad possible bottlenecks.

### Objective (Code)

The pinned historical AP revision `9146893` was fetched and the historical successful candidate rebuilt. Its two-equality control again accepts in exactly 69135 steps; the full file again accepts in **213140829** steps. New sources/patches/raw results are under `a-program/src/prototype/bend_benchmark/remaining_*`; the old missing raw files have not been fabricated.

Fresh evaluator counters on the full 800 equalities:

| Counter | Count | Interpretation |
| --- | ---: | --- |
| Application traversal | 51105850 | Push argument closure and descend into function |
| Lambda traversal | 24379292 | Includes 2408960 applied lambdas with an unused parameter |
| Evaluator environment lookup hits / misses | 7931520 / 2408960 | Lookup misses are only 1.13% of total Solve steps; they are not the dominant transition category |
| Materialization calls inside demand frames | 92676264 | About 43.5% of total Solve steps; not a measured wall-time percentage |
| Materialization's environment substitution steps | 60109380 | Converts closures into immutable Core |
| Materialization initialization | 11381800 | Many short-lived readback contexts/indexes |
| Readback requests / newly allocated entries | 46424226 / 42249914 | Roughly 91% are new within their local readback context |
| Readback environment lookup misses / hits | 10140677 / 16134324 | Distinct from the evaluator's environment lookup counters |
| Demand argument-prefix copies | 11379764 | These overlap the frame-resume transition count; do not sum them as disjoint steps |

Fresh single diagnostic/release scans: reconstructed best **10.282 s**; closed readback-key canonicalization **10.220 s**, unused-beta-environment omission **10.876 s**, closed-argument environment elision **10.139 s**. No repeated speed benefit is established for those small changes; none explains the large remaining gap. The unused-beta probe keeps the same total 213140829 transitions, while closed-argument elision removes only 4800.

A new owner-resolved head/spine continuation probe avoids eager Core materialization for **recognized constructor matches only**. It retains a fallback to the ordinary materialized callback for neutral/unrecognized heads; callback names, existing frame layout and captured closures remain explicit. It accepts all 800 obligations in **202802749** steps and **9.589 s** in one scan (not a repeated factor). Positive/false controls accept/reject appropriately. Its demand-frame materialization calls fall to **82338184**, but the **60109380** environment substitution steps are unchanged. Therefore matching alone does not explain the expensive open-closure substitutions. The next probe isolates `Fold(Return(v), k)` and total-result projection of `Return(v)`, passing `v` as its actual closure while preserving existing handling of effects, neutral heads and other forms.

### Assessment

Agent finding: the evaluator's linked-list lookup was a concrete candidate but is not the dominant transition count. The measured problem is the demand interface's eager materialization and repeated temporary readback allocation. Constructor-head delivery reduces administrative materialization but not the costly environment-substitution walk, narrowing attention to other demand continuations, especially CBPV Return extraction. Counts locate work; controlled release repetitions and semantic/state tests must establish any claimed factor.

#### Progress: additional CBPV head probes

The `Force(Thunk(M))` head probe enters the actual captured closure `M`; `Fold(Return(v), k)` applies `k` to the captured value closure; constructor matches take actual field closures. All unrecognized, neutral or effectful cases retain the existing materialized fallback. Existing frame storage/codec layouts are unchanged; new owner-resolved callback names coexist with old callback names.

| Fresh single scan | Seconds | Solve steps | Peak RSS KiB |
| --- | ---: | ---: | ---: |
| Reconstructed previous best | 10.282220 | 213140829 | 2012316 |
| Match head only | 9.589244 | 202802749 | 1947704 |
| Match and Return head | 9.683705 | 194395523 | 2000852 |
| Force/Thunk head only | 8.643720 | 205979295 | 1968896 |
| Match, Return and Force/Thunk head | 6.724955 | 183131801 | 1918216 |

The last probe eliminates all **92676264 demand-frame materialization calls** and all **11379764 argument-prefix copies** for this particular workload. Materialization contexts fall from **11381800 to 2036**; environment-substitution traversal falls from **60109380 to 7139616** (88.1% fewer). This directly locates a major avoidable cost. Evaluator application and lambda traversals remain unchanged. Environment hits/misses move to **53840108 / 12044800**, because lazy closures retain substitutions for later lookup; these are a concrete residual cost, not evidence that every environment miss is expensive.

All 800 equalities are accepted, and both false-equation controls are rejected. Original `iadt_test`, evaluation-state I/O, normalization checkpoint and source checkpoint suites pass for the combined head probe. Normalization checkpoint tests exercise 53/7/145/12 split points; source checkpoint tests exercise 204 lifecycle cuts and exact charged steps.

The unchanged `core_test` does **not** yet pass: its Fold frame counter recognizes only the old callback pointer, and the Force budget fixture expects eager intermediate materialization to remain pending after 100 steps. The new lazy WHNF can finish the raw evaluator earlier. These failures are preserved in `force_head_checks_original.json`; they must not be hidden or represented as a passing original suite. A disposable test adapter will preserve semantic assertions, retain coverage of the old materialized interface, and independently test the new closure path's capture and charged final WHNF materialization. Repeated release measurements are also pending; the single-scan 1.53 factor is preliminary.

#### Completed release repetitions and state validation

The preceding paragraph records the intermediate state. Three sequential fresh-process repetitions are now complete, interleaved as `best, force, head_force; head_force, best, force; force, head_force, best`. Initial scans and all counter/profiler builds are excluded. Compiler: GCC 13.3.0, `-std=c11 -Wall -Wextra -Werror -O2`; CPU: AMD EPYC 9V74. All trials use the same full source and 10-billion Solve budget. No heavy builds run concurrently with timed repetitions.

| Candidate | Three wall times (seconds) | Median seconds | Factor versus previous best |
| --- | --- | ---: | ---: |
| Reconstructed previous best | 11.633996, 10.435503, 10.533046 | 10.533046 | 1.000 |
| Force/Thunk head only | 9.163341, 8.419133, 8.906071 | 8.906071 | 1.183 |
| Match, Return and Force/Thunk head | 7.480899, 7.247114, 7.065879 | 7.247114 | **1.453** |

Factors are ratios of medians. Adding Match/Return delivery on top of Force-only yields **1.229**. This latter factor is a combined incremental effect; it is not an isolated factor for either Match or Return. Fresh and historical timing factors must not be multiplied across different sessions/configurations.

The disposable `remaining_test_adapter.py` and separately preserved `head_force_test_adapter.patch` now pass the complete Core suite. Every original semantic assertion remains. Two fixture assumptions are explicitly adapted: the Fold observer recognizes both registered callback versions; the original eager-Force fixture is run using a test-only policy selecting the still-registered old materialized continuation. A new independent lazy-Force test exercises all **8 raw evaluator split cuts**, freshening of a binder that would otherwise capture a free variable, and final WHNF materialization. The raw head completes in 8 steps, but the WHNF owner charges **15014 steps** before publishing a result/certificate. At budget 100 it remains pending with neither result nor certificate; chunks of 7 match the one-shot total/result exactly. Thus the observed speedup does not rely on publishing a certificate before charged final readback.

This is a **passing adapted Core suite, not a passing unchanged Core suite**. Original failure logs remain available. The unchanged IADT, evaluation I/O, normalization checkpoint and source checkpoint suites pass. Positive two-equation control: 56605 steps; false Bool equation: rejected after 56681; false tree equation: rejected after 50195. All full trials accept the 800 equations in exactly 183131801 steps.

#### Fresh corresponding Bend comparison

Bun 1.4.2 was restored from its pinned release archive (archive SHA256 `36368faef7527875d5ffa52e53cd48021741f2a83eb6208a8dd64068d422a913`). `remaining_compare.py` runs the unchanged pinned Bend CLI with `--check-only`, `BEND_NO_TELEMETRY=1`, and the official source. It records command/source/binary/patch hashes and wait4 CPU/RSS. One initial Bend scan is excluded, followed by three interleaved AP/Bend pairs; all processes are sequential.

| Ordinary checker | Three wall times (seconds) | Median seconds | Peak RSS in these trials |
| --- | --- | ---: | ---: |
| Bend2 v2.0.34, Bun 1.4.2 | 1.780318, 2.046328, 1.780815 | **1.780815** | 90616 KiB |
| AP combined head prototype | 6.783083, 6.959428, 7.812357 | **6.959428** | 1918312–1918316 KiB |

The new AP prototype still takes **3.908 times** Bend's ordinary-checker time on these corresponding files and uses about **21.2 times** the measured peak RSS. These are implementation comparisons, including startup/parsing/elaboration/checking and different native proof encodings; they are not isolated per-kernel instruction costs. **BendTT `--verdict` is still unmeasured.** The result applies to the pinned AP revision plus this prototype overlay, not an uninspected current remote HEAD.

Source correspondence at the pinned Bend revision: `bend2/bend.ts:671` `term_cell` reuses existing lazy cells; the `VAR` return frame near line 2897 stores the computed head and lazy constructor fields in its cell; `MAT` near line 2937 pushes actual constructor fields; `compare_go` near line 3057 reuses pointer-equal terms and unifies equal share cells. AP's head probe addresses the eager intermediate readback difference. It does not add Bend-style update cells or conversion-cell unification, so their remaining contribution has no established AP factor.

#### Concrete residual cost ranking

A separate `-O2 -g -pg` build of the combined head candidate accepts exactly the same 183131801-step file. Its gprof results are diagnostic only: the profile samples about 5.38 seconds of attributed executable self time, excluding shared-library/profiler costs. The percentages below are **not wall-time percentages** and do not establish prospective speedup factors.

| Residual mechanism | Fresh evidence | Attribution and status |
| --- | --- | --- |
| One-transition reduction wrapping and scheduler polling | 182.66 million normalization/reduction/WHNF calls; 182.92 million work projections; 185.15 million work-state accesses | `whnf_step`, `normalization_step`, reduction advance, WHNF advance, work projection/state, enqueue and certificate access together account for about 38% of attributed self samples. Source advances the underlying WHNF by budget 1, checks the certificate, re-enqueues pending work, then repeats. Concrete repeated overhead; no safe batching factor established. Preserve exact Solve charging, FIFO/wake behavior and checkpoint cuts before changing it. |
| Evaluator application, beta and environment traffic | 51.11 million application traversals; 24.38 million lambda visits; 53.84 million environment hits and 12.04 million misses | Evaluator `step` accounts for about 20.1% of attributed self samples. Environment hits/misses total 65.88 million transitions after lazy delivery. Missing bindings alone were not the original dominant cost; work shifted from eager substitution to captured-closure resolution. An exact-key cache previously had poor reuse; do not assume broad memoization will help. No separate environment speedup measured. |
| Small allocations and remaining readback indices | 96.75 million `pg_alloc` calls, down from historical best 151.05 million; 4.30 million reify requests, 6.16 million index-candidate calls | Application traversal allocates an argument link, and each applied lambda allocates an environment link. `pg_alloc` is about 5.4% of attributed self samples; reify requests about 7.4%, index-candidate lookup about 5.0%. Final materialization and capture-safe interning remain necessary. The high RSS is measured; its live-object/retention ownership breakdown remains unmeasured. |
| Sharing model versus Bend cells | AP head delivery retains captured closures; Bend updates lazy share cells and merges equal cells during conversion | Source-level difference verified. Its factor has not been isolated; AP has other pure WHNF memo stores, so it would be wrong to call AP wholly uncached. Any new cache must preserve policy, capture and independent checking. |

Prioritize the first two rows for the next controlled experiments, then measure allocation lifetime and retained objects. Do not interpret adding these percentages as a projected total wall-time reduction. Floats, fixed-width unsigned integers and nontermination syntax are not required by this tree checker workload.

#### Reproduction and evidence map

All implementation scripts and generated diffs stay under `a-program/src/prototype/bend_benchmark/`; accepted AP/Bend sources and accepted tests were not edited. Detached AP source revision: `9146893f07f0fe3f4c8309c370420c1702dfb96d`. Bend revision: `7d24b8d0235cb9781140512c0f163c48ea84a719`.

```bash
bash a-program-checker-pin/src/prototype/artifact_persistence/candidate.sh /tmp/ap-remaining-base
python3 a-program/src/prototype/bend_benchmark/remaining_trees.py --bend bend --count 400
python3 a-program/src/prototype/bend_benchmark/remaining_costs.py --base /tmp/ap-remaining-base --destination /tmp/ap-remaining-head_force --feature head_force
python3 a-program/src/prototype/bend_benchmark/remaining_test_adapter.py /tmp/ap-remaining-head_force
make -s -f a-program-checker-pin/src/prototype/artifact_persistence/build.mk OVERLAY=/tmp/ap-remaining-head_force BUILD=/tmp/ap-remaining-head_force/build /tmp/ap-remaining-head_force/build/core_test
/tmp/ap-remaining-head_force/build/core_test
python3 a-program/src/prototype/bend_benchmark/remaining_compare.py
```

The builder deliberately requires a fresh destination; do not rerun it over an existing overlay. Build `best`, `force_only`, `head`, and diagnostic features in separate fresh destinations using the same script. `remaining_compare.py` currently expects the fixed temporary AP/Bun paths recorded in its source. Profiling command: `make -s -f /tmp/ap-remaining-head_force/src/Makefile BUILD=/tmp/ap-remaining-head_force/profile 'CFLAGS=-std=c11 -Wall -Wextra -Werror -O2 -g -pg' /tmp/ap-remaining-head_force/profile/pointer-check`; run the full file from that profile directory, then use `gprof` on the binary and `gmon.out`.

Raw evidence in `remaining_products/`: `interleaved_summary.json` and individual `*_r*_400.json`; `ordinary_fresh_comparison.json`; `diagnostic_400.json`, `head_diagnostic_400.json`, `head_return_diagnostic_400.json`, `head_force_diagnostic_400.json`; `force_head_checks_original.json`, `head_force_audit_checks.json`; `head_force.gprof.txt`; `head_force.patch`, diagnostic/isolated patches and `head_force_test_adapter.patch`; `*_build.json` and `bun_download.json`. Historical raw evidence remains missing; these files contain new experiments only.

### Plan

- [x] Rebuild the fixed revision, tree source port and historical successful changes.
- [x] Count the remaining evaluator/readback transition categories.
- [x] Isolate eager demand materialization and test narrow head/closure changes.
- [x] Validate semantic, negative, state and checkpoint controls; preserve original Core fixture failures and distinguish adapted Core success.
- [x] Record fresh AP factors, corresponding Bend timings, residual profile and new raw evidence separately from historical results.
- [ ] Measure safe reduction batching/environment changes without altering charged fuel or wake/checkpoint semantics.
- [ ] Attribute retained memory by live owner/lifetime and isolate Bend-style sharing.
