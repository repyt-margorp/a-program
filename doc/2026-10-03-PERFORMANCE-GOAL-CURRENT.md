# Performance Goal: Current Speed and Memory Work

Date: 2026-10-03
Status: active; E9+E10 published; MEM1 v1 rejected, conservative stateless v2 qualification underway.
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

2026-10-03 13:49 UTC, Root superseding review at Main `b461ace`: MEM1 v1
patch `698e97e2` is rejected. Fresh inline-frame-state control gives parent0,
v1 signal6, conservative v2 0: a later demand still borrows state held by a
retired frame. Passing v1 acceptance and its lower sampled capacity do not
qualify that deletion or establish a gain. Keep its capacity values only as
rejected-candidate evidence. V2 `b2c39601`, source128 `8f6ea298`, retains
every non-NULL state frame and pools stateless frames after callback return.
Qualification and fresh memory evidence remain pending; v1 values do not apply.
Callback report `e742cb81` verifies current owners retain the machine arena
through return; public borrowing prose does not establish an arbitrary custom
callback guarantee. No new prohibition is adopted. Root controls and exact
evidence are linked by [mem1-root-review.json](../src/prototype/performance_followup/mem1-root-review.json).
Diagnosis40 still verifies sampled180M machine capacity1,685,012,480 bytes and
zero completed-machine scratch; this is capacity, not peak/live/RSS/time.
Job's header/scheduler work and correctedE11 joint qualification stay separate.

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
