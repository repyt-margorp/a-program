# Performance Goal: Current Speed and Memory Work

Date: 2026-10-03
Status: active; V3 bounded measured gain reviewed; MEM2 beta environment elision rejected; IADT empty-tail spine prototype published, matched costs pending.
This is the active successor to the frozen
[Performance Goal at0539051](2026-10-03-PERFORMANCE-GOAL.md). Its published bytes
and historical handoffs remain immutable. The single current issue-status table
and work list remain in [Joint Verification](2026-10-03-PERFORMANCE-JOINT-VERIFICATION.md).

## Problem List

1. Improve checker speed and actual memory use while preserving checking,
   charged effort and persistence semantics.

## 1. Speed and Memory Recovery

### Subjective (User)

2026-10-03, dated English paraphrase of the human priority relayed by inquiry
desk `019ebfae`: AP tree peak memory of about1847MiB after optimization remains
excessive beside native examples around97–529MiB. Require software/design
improvements for both speed and memory. Storage counts alone do not satisfy
this requirement. Keep the full performance work active.
2026-10-03, dated English paraphrase of the further human clarification relayed
by desk `019ebfae`: improve speed by deleting unnecessary mechanisms/work,
without adding tuning complexity. Needed intermediate state for resume is
acceptable; roughly order-of-magnitude overhead versus other systems is not
excused by resume history. Distinguish indispensable frontier/checked facts
from redundant graphs, copies and dead transient state. Count new index/owner
overhead and require net memory/time improvement with unchanged meaning, fuel
and resume. This is not a hard10x target or permission to discard required
progress, use unsafe borrowing or overlap heavy benchmarks.
2026-10-03, dated English paraphrase of the direct user workflow change via
inquiry desk `019ebfae`, confirmed by Root at12:03UTC: each implementation lane may continue without waiting
for Merge; Merge resolves integration conflicts. Finish live E9+E10 gates,
preserve exact submitted epochs, and continue independent MEM1 allocation/
lifetime attribution and next prototype work within the existing Goal instead
of waiting only for publication. Exclusive wall/RSS scheduling remains. Do not
use unsafe borrowing, weaken checks, duplicate authority or restart completed
lanes. This supersedes blanket review/publication holds.

Earlier user requirements and approvals retain their meaning in the frozen
parent Goal and the current Joint Verification Subjective. In particular,
verified speed improvements may be promoted by Merge; this worker stays in
its prototype scope and does not edit accepted code or merge Main.

### Objective (Code)

2026-10-03 15:29 UTC, fresh Root verification at Main4853cb6: independent
SAN builds/runs confirm materialized-failure parent0 versus V2 signal6,
heap-use-after-free in retire_frame; reportd5155b89. Accepted eval.c/h,
Makefile and tests have no diff from e552f2c, and prototype overlay/build have
no MEM1 selection reference. Published rejection4853cb6 changes status/evidence
only, not the implementation selection. V3 notice700fcaaa is frozen READY,
with Root qualification pending; no safe memory/time gain is established.

Published measurements `44e479be` establish the exact E6 tree workload's
wall17.23→5.40s and peak RSS median1891724KiB, about1847MiB. Native proof/helper
work differs, so these are concrete measurements rather than a universal
ranking. The state/image follow-up `ac6a8b2` shows final Core terms
10636362→298214, byte-identical completed artifacts and unchanged final
typed/Evidence/Job counts. It does not decompose the head peak or establish
live/peak/cumulative bytes by owner; counted layout is not RAM.

Separate E9 is terminal/frozen: source128 `2766822a`, evidence1989 `0a8b46ec`;
targeted O2 and relevant sanitizer/persistence controls pass except the original
strict3. Root reports all evidence independently verified and prototype Main
merge `6a48aaeb`/status `1459fbb8` pushed/remote verified. E9+isolated E10
qualification is terminal/frozen at source128 `73fa86c9`, evidence2002
`de7b28cb`: targeted81 only strict target fails, sanitizers45+extra5 and
focused23+extra8/cross140 pass; all52 public images/full verdict-fuel table
exactly match E9. No new actual memory improvement is claimed from E10.
Root now reports independent verification of all2,002/source128 and seven
relevant test hashes against assembled current Main after accepted promotion;
prototype Main `5e466381`/status `09ee775` pushed and remote verified by Root.
The private `73fa86c9` snapshot remains the qualification anchor.
Original observer, sanitizer, strict partition and setup failures remain
separate. No E11/E12 runtime is included.

### Assessment

2026-10-03 17:58 UTC, Root MEM2 IADT review: frozen9 task6b0ed93 on exact
parente561030 is pushed/remote verified; prototype Main6ebf453 contains only
the same9 frozen files. All1964 evidence hashes, source1282e87fd84 versus
parent0deb36a7 (only iadt.c changed), inherited tests and all279 cost pins verify.
Root reconstructs the frozen patch privately: fresh O2/SAN builds and ten direct/
callback controls0; actual raw gates45/23/cross140, public52/history28 and fuel
TSV match. Source gates retain only original strict3; no new full384 claim.
Reusing immutable validated field nodes with an empty selected caller tail
preserves node/environment order; live machine-arena and decoded output-graph
lifetimes survive existing head cleanup/frame retirement. Nonempty tail copies
remain. No new owner/index/ABI or accepted/default selection change. Original
beta environment rejection remains separate. [Root review](../src/prototype/performance_followup/mem2-spine-root-review.json)
records initial Root setup/predicate corrections separately from successful
checks. Twelve census raw logs/fuel and six deltas verify: tree400 removes
2408960 requests/77086720 cumulative bytes; LocalSorted1053/47264 and the failed
32-byte reporting assumption are preserved. These counts are not peak RAM/time.
Root grants operational exclusive slot MEM2IADT-E12-20261003T180000Z-600,
18:00-18:10 UTC: only exact36 sequential jobs/config8cc0b753, remaining-derived
cap and all children stopped before the absolute deadline. C/Job are stalled,
no workload children observed; Root runs no heavy work during the slot.
Actual results, expected-step/pin/stopped-child review and release remain pending.
This is existing-scope coordination, not new human approval or Goal completion.

2026-10-03 16:47 UTC, Root MEM2 review at Main3b09a98: reject semantic-body
beta environment elision (private patchc3c91bd3). All30 frozen files0573b9be
and original four builds/fourteen executions verified. Root independently
reconstructs the patch on qualified source1280deb36a7: four O2/ASan/UBSan builds
succeed, and fourteen executions reproduce all six semantic failures. The
ordinary caller constant/fuel matches, but a closure retained at cut2 returns
an unbound binder in1 step instead of its value in2. Both fresh readers pass
parent-produced two-root configurations and fail candidate-produced ones.
Public lexical links require preserved contents/lifetime; the original machine
stays alive in the local control. This tests result/fuel, without physical-layout
assertions or a full-machine checkpoint claim. No sanitizer diagnostics, test
adapter or current V3/E12 defect. [Root fresh review](../src/prototype/performance_followup/mem2-capture-root-review.json)
records the rejection; candidate runtime is not integrated. Original V3 source18
and cost111 remain unchanged. Performance now investigates direct-IADT field
spine reuse only with no trailing caller arguments; this is observed prototype
work, not a qualified deletion. Prove live, restored and retained-node lifetimes
before reuse and route shared owners through Root/Job. No new cost samples or
full Goal closure follows.

2026-10-03 14:36 UTC, Root superseding review: V1 and V2 are rejected.
New materialized-failure callback control93da2cfa lets the callback destroy its
machine arena and return-1. Parent SAN0; V2 SAN signal6 with heap-use-after-free
in retire_frame, report28649b93. The public header does not explicitly prohibit
this action. No invented callback ban or test waiver replaces the failing case.
Frozen V2 task1297e0b/Mainf48ab5e and1969 c0734afa evidence remain exact:
their passing384/45 and Root focused controls did not cover this path. Those
results and V2 selected capacity/cumulative deltas are historical rejected-
candidate evidence, not a safe deletion or current improvement.
V3 deletes materialized/fallback retirement and
pool only stateless captured-head frames after the existing parent cleanup.
Retain non-NULL state and return2 behavior. Fresh correctness/capacity is
now independently reviewed by Root: exact source128353ed498 and all1944 hashes,
fresh O2/SAN builds and all three callback units each0 (report6a09e5a5), including
the public materialized-failure control. Own V3 capacity arithmetic is checked;
V2 values do not establish V3 memory/time. Current corrected-E12 candidate
0deb36a7 differs only by the exact frozen eval.c/h patch; all1816 candidate and
2064 baseline evidence hashes verified. Configa3335c5e/all280 pins and36 paired
jobs were reviewed before Root's existing-scope operational exclusive grant
MEM1V3-E12-20261003T154600Z-600, earliest15:46 UTC/hard stop15:56 UTC.
The collector finished0 at15:49:08 UTC; Root verified all36 raw logs/metrics,
argv, exact expected charged steps and280 pins afterward, with no failed,
incomplete or censored samples. Root released the slot at15:51 UTC. Results:

| Same workload / steps | Median wall seconds, baseline -> V3 | Median peak RSS KiB, baseline -> V3 |
| --- | --- | --- |
| List /1915 | 0.00716 ->0.00469 | 2524 ->2580 |
| Imported LocalSorted /761848 | 0.62892 ->0.60939 | 169612 ->170104 |
| Trees4 /2318248 | 0.08646 ->0.07936 | 34740 ->21216 |
| Trees16 /7608511 | 0.24246 ->0.23752 | 91592 ->50900 |
| Trees64 /29780510 | 0.83454 ->0.74352 | 316944 ->169984 |
| Trees400 /183507626 | 5.34021 ->4.92598 | 1891928 ->1001764 |

These are three repetitions per variant/case, matched source/proof/helper work,
reversing pair order in repetition2. Trees400 wall median falls7.76%, RSS47.05%
(1847.59 ->978.29MiB). LocalSorted RSS rises492KiB; list timing is dominated by
startup/clock resolution. GNU process RSS includes allocator/startup effects;
this is a bounded measured tree gain, not universal speed/memory or native
ranking. [Root complete medians/ranges and pins](../src/prototype/performance_followup/mem1-v3-cost-root-review.json).
Exact18 V3 taskc861728 is pushed/remote verified; prototype Maincc69f52 contains
the same frozen bytes. Accepted/default producer selection was not replaced.
2026-10-03 16:20 UTC Root publication review: original109 measured files plus
the separate2-file interpretation addendum are exact at taske561030 and
prototype Main895ac67. All111 file sets/hashes, all36 frozen argv/expected steps/
raw hashes, twelve Root summaries and280 input pins independently verified.
The table above uses launcher wall time; the [frozen report](../src/prototype/performance_mem1/epochs/current_e12_cost_20261003/report.md)
uses GNU time (tree400 median5.33 ->4.92s). Read it with the
[interpretation addendum](../src/prototype/performance_mem1/epochs/current_e12_cost_review_20261003/README.md):
list launcher ranges are disjoint despite the original frozen analysis sentence;
GNU list wall values round to0.00s. Startup dominance and overlapping list RSS
remain, so no evaluator/list-speed or list-memory gain follows. Original109
bytes stay unchanged. The frozen terminal hold flag records the pre-release
stage; the worker consumed Root release and resumed argument/environment owner
research. No new comparative samples ran for this publication.
No accepted promotion or Goal completion. Initial Root omitted-Makefile setup error stays
separate from the successful fresh builds/runs.
[Root lifetime review](../src/prototype/performance_followup/mem1-root-review.json)
preserves both rejections and earlier evidence. CorrectedE11/E12 joint work
stays separate, excluding MEM1. The full performance Goal remains active.

MEM1 uses a private runtime-identical `73fa86c9` copy. Existing allocation audit
counts cumulative external requests and omits graph.c internal calls, so it
cannot explain actual peak/live memory. Read-only GDB snapshots use actual DWARF
layouts without inferior calls or runtime profiling wrappers. Initial completed
list/tree-prefix controls pass; completed shared substitution and WHNF scratch
are already released. Extend owner capacity attribution and bounded scaling
before choosing a deletion; current snapshots are not peak RSS measurements.

Root's MEM1 work list follows current E9+E10 qualification. The latest direct
user workflow change supersedes waiting for Merge scope/publication: independent
attribution and next owned prototype work may proceed after the live gates;
only comparative wall/RSS still needs exclusive scheduling. First distinguish
peak, live, cumulative and retained capacity bytes for Core, typed objects,
Evidence, Job, queries, indexes and scratch using existing census/allocation tools. Include bounded
representative size scaling, then choose the simplest safe owner/copy/lifetime
deletion supported by those observations. Coordinate shared owner findings
through Merge/Job; do not create competing owner implementations, unsafe
borrowing, a new authority/graph or profiling-only wrappers.

Measured final term reduction is evidence of retained-state reduction, not an
explanation of the remaining peak. Further wall/RSS comparison requires a
later agreed exclusive slot with matched sources and inputs. Preserve
semantic/fuel/step0/split-resume controls. No immediate heavy reruns are needed
for this inquiry.

### Plan

Use the one current issue-status table and work list in Joint Verification.
Finish the current qualification and exact handoff first, then continue without
waiting for publication. The next explicit
epoch is owner-attributed memory diagnosis and a reviewable design deletion,
jointly verified with Job; matched timing/RSS follows only with an exclusive
grant. Completion requires demonstrated software/design improvement in both
speed and actual memory, with required correctness and persistence evidence;
storage census alone cannot close this Goal.
