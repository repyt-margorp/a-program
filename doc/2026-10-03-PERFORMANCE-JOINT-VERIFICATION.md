# Coupled Performance and Job Verification

Date: 2026-10-03
Status: corrected E11/E12 qualification frozen; V3 measured gain reviewed; MEM2 IADT empty-tail source9 published, cost slot granted/results pending; Goal active.
Active Goal: [Current Performance Goal](2026-10-03-PERFORMANCE-GOAL-CURRENT.md).
Historical parent: [Performance Goal](2026-10-03-PERFORMANCE-GOAL.md), frozen at
task-branch commit `05390513bca302b4a219881994c5bbd69d5b733a`.
This supplemental brief records the separate joint gate, without changing that
44-file frozen publication or creating another Job/Evidence implementation plan.

## Problem List

1. Verify coupled head/readback and Job storage deletion on one current producer,
   with unchanged checking and separate storage/traversal/time attribution.

## 1. Common Producer and Joint Gates

### Subjective (User)

2026-10-03, English paraphrase of the latest direct user coupling clarification,
already recorded in the parent Goal: Performance and Job/Evidence reduction/
deletion require concentrated joint verification. C is the downstream `.a` plus
LinkerScript bridge to C modules, not a reason to expand A Program. Keep the full
performance Goal active.
2026-10-03, English paraphrase of explicit branch authorization: workers may
commit/push their task branches; only Core merges or updates Main. Stay within
prototype scope, without self-promotion or competing acceptance authority.
2026-10-03, English paraphrase of the latest user coordination clarification:
Core waits for a notification OR a six-hour timer and inspects every six hours
even without a notice. This adds the timer fallback to the earlier notification
requirement; continuous polling is not required.
2026-10-03, English paraphrase of the latest human clarification: accumulating
issues make progress difficult to see. Each worker should provide concrete,
issue-linked feedback showing which subproblem has advanced and how much.
2026-10-03, English paraphrase of the direct human workflow replacement: the
visible parent is the inquiry/report desk. Separate Merge/audit session
`01a100b3-1d83-7090-bbf6-62544c39ec4b` inherits coordination, exact task publication
and Main integration. Continue this Goal and frozen handoffs unchanged; report
through the existing outbox to Merge, which owns measurement slots and reports
upward. Read the central schedule at Main `4d1d941`. The new #59/PR #60 audit lane
is read-mostly; do not delete accepted tests, relax outcomes or add authority.
Keep one issue-linked status table, do not load/resume/fork another session, and
do not treat notification evidence as new user approval. This replaces Core's
operational coordination role without changing implementation authorization.
2026-10-03, English paraphrase of actual human approval relayed by inquiry desk
`019ebfae`: verified speed improvements may be promoted from Prototype into
accepted `src`. This supersedes the earlier no-promotion boundary only for that
verified scope. Merge remains the sole Main/accepted-promotion owner; this worker
does not edit accepted code.
2026-10-03, dated English paraphrase of the human priority relayed by inquiry
desk `019ebfae`: AP tree peak memory of about1847MiB after optimization remains
excessive beside native examples around97–529MiB. Require software/design
improvements for both speed and memory; storage counts alone do not satisfy
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

### Objective (Code)

### Issue Status

Updated 2026-10-03. This is the single current issue-status table for this active
brief; detailed evidence remains in the linked manifests and reports below.

| Issue/subproblem | Implemented change and revision | Local versus joint verification | Task publication versus Main integration | Remaining criterion/blocker | Next concrete epoch |
| --- | --- | --- | --- | --- | --- |
| #56 / PR #58: captured head, readback and cleanup | Owner-local head `f3c3555`, cleanup `0539051`; Root accepted promotion `7631e5a` | Joint E7 full O2 384/0 and sanitizer45/0; fresh accepted candidate386/0 and eight sanitizer groups0. All13 promoted source/build/test blobs exactly match that verified candidate; original failures separate | Fresh remote main `cf8cdf5` contains accepted `7631e5a`; task publication remains `0539051`/measurement `44e479b`; Root performed promotion | Strict3, accepted-only unmeasured cost and human-required further actual memory/time improvement remain; full Goal is open | Finish current E9+E10, then pending joint MEM1 under Root scope/slot |
| #56: coupled Job E2 storage borrowing | Job-owned module/ref borrowing `44b03895a0f7ac51de68a25f2cc68aa97c227b1b`; no implementation edits by this lane | Owner-local results are Core reports. This lane's joint E2: full O2 384/0, sanitizer 45/0, transport/semantic/all7 checkpoints pass; strict3 fail | Task publication verified; Core reports prototype Main integration `7b27c4c`, pushed with evidence/status `37de1e8`; source/evidence2007 reverified by Core | Strict3 remain with owner; preserve frozen evidence, no accepted promotion | Separate subsequent epoch qualification |
| #56: coupled E3 body-frontier persistence | Job-owned SourceIO codec `51c476d78730ea56d96978ae53a0dfd1d688b096` | Joint O2 codec batch 57 recipes, only strict target fails; 13 sanitizer commands pass. Inert resave/invalid markers/old-format controls pass; body9/semantic parent negatives qualify. Core reverified all 1,318 records | Task retry/remote verified; Core reports Main prototype `48364b013c27066b0ac5efc2955f3236b5ab34a3`, published with `fd45c42` | Strict3 retained; initial wrong diagnostic-message predicate preserved as setup; no accepted promotion | Separate E4/E6 qualification |
| #56: coupled E4 registration deletion and namespace control | Job-owned `ec9537135f63b24ef5e355575dfa53f907251fb0`; exact parent-control patch adds 17 test-only lines | Joint E4 terminal: full O2 384/0, sanitizer build38/commands45 zero, focused O2 23/0, cross-build140/0 and all7 checkpoints pass. Artifact41 only strict target fails; exact same3 | E4 task publication verified; Merge reports exact source/gate review and Main prototype integration `3a8c024`, metadata `be0446d` pushed | Strict3 retained, E5 runtime excluded, no accepted promotion or Goal closure | Distinct E6 review, then matched measurements |
| #56: coupled E6 namespace-owner borrowing | Job-owned `b2d66682be75c21a3c2a8c5138db9d3f0c5e0eb2`, exact E4 parent, binding-scope borrowing and namespace test | Joint E6 terminal: relevant O2 408 recipes only strict target fails; sanitizer build38/commands45 zero; all7 checkpoints, focused23 and cross140 pass. Namespace once, exact same strict3 | Task publication verified; joint evidence1,939 frozen and Merge readiness/slot notice sent; no Main E6 integration claimed | Merge review; strict3 and setup failure retained; no new E6 full-suite claim or accepted promotion | Matched E6 baseline/head measurements after slot grant |
| #56: E7 canonical source-metadata borrowing | Job-owned `3ad03033708135895352e3b19ddef64169c62c22`; exact E6 parent, five runtime files | Source128 `26a57bde`, tested156 `c63ba010`; full O2 384/0, sanitizer38/45, focused23 and cross140 pass. Canonical Surface-context repair is Root-owned; old failure retained | Task published; frozen joint `79e42921`; fresh remote main `cf8cdf5` contains prototype integration `efc3ff0` | Strict3 and both original Surface-context failures remain; no blanket owner acceptance | Exact E9/E10 qualification after Root handoff |
| #56: E8 Graph-reference output borrowing | Job-owned `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`; semantic E7 parent, only synthesis.c changes | Source128 `9ba2b33c`, tested156 `aefbc96f`; relevant O2 408 only strict target fails; sanitizer38/45, focused23/cross140 and Graph controls pass. Fresh Root canonical assembly matches all128 runtime/six tests | Task published; joint1,956 frozen `253a36c0`; fresh remote main `cf8cdf5` contains prototype integration `f6d7cfe` | Strict3 and reporting-helper/owner setup failures preserved; frozen producer retained | Distinct E9 qualification now running |
| #56: E9 descriptive namespace traversal restoration | Job-owned `bd1ddf3a0ac86c365c1b1bf5992eb254cf752a99`, exact E8 parent, four runtime files | Source128 `2766822a`, publication23+manifest/tested156 aligned; Root independently verified all1,989/source128. Terminal O2 81 only strict target fails, sanitizer38/45 plus extra5 pass, focused23 plus extra8 and cross140 pass;590 module snapshots/build and forged prefix/source claim pass | Task published; joint manifest `0a8b46ec`, summary `d8cb5334`; Root reports prototype Main merge `6a48aaeb`/status `1459fbb8` pushed and remote verified | Strict3 and all original worker split/harness/observer failures retained; rejected E11/runtime E12 excluded; no new full-suite claim | Finish distinct exact E9+E10 qualification, then pending MEM1 |
| #56: E10 unary receipt ordinal deletion | Job-owned isolated `6715af2d91f9024fd85e122db57a2415b3c0d747`, semantic E8 parent | Distinct joint source128 `73fa86c9`, publication18+manifest/tested156 aligned; Core adapter disjoint. Terminal O2 81 only strict target fails, sanitizer38/45+extra5 and focused23+extra8/cross140 pass; public52 images/full verdict-fuel TSV byte-identical E9. Root independently verified all2,002/source128/seven test hashes against current Main assembly | Separate task branch published; joint2,002 frozen `de7b28cb`, summary `c8f0b63b`; Root independently verifies prototype Main `5e466381`/status `09ee775` pushed and remote exact. Isolated E8+E10 evidence separate | Strict3 and private publication-directory setup failure preserved; raw deletion has no worker-ABI aligned extent gain and no actual RAM claim | Continue independent MEM1 existing-tool attribution and next owned prototype work |
| #56 with #51/#52 measurement boundaries: cross-system evidence | Official source `7d24b8d0235cb9781140512c0f163c48ea84a719`; collector `9c50c62d`, config `390adf41` | E6 measurements30/0; tree wall17.23→5.40s/RSS−36.3%, QuickSort medians equal. All114 task blobs match `0f017454`; initial zero-sample failure retained | Task `44e479be` pushed; measurement Main merge `99efcf7` is an ancestor of freshly observed remote `cf8cdf5` | Joint E6 timings do not establish accepted-only values; proof/helper/startup/RSS limits and strict3 remain | Future cost scope/slot only after coupled qualification and Root grant |
| #56: same-input retained state and completed artifacts | Diagnostic helper + compact report, exact measured E6 ordinary/head sources and three pinned inputs | Six DONE censuses/source saves and12 full fresh reloads pass;12 zero-step resaves preserve bytes while CLI reports pending3. All three completed pairs byte-identical. Trees retain10,338,148 fewer Core terms; typed/Evidence/Job counts equal | Separate65-record evidence frozen `eb4e7542`; frozen2 bytes freshly match live/task `ac6a8b2420051b06d41c1b7032526c360d1ae690` and remote task branch. Published114/E7/E8 immutable; no Main integration claim | Preserve obsolete metrics-helper build2; semantic-object/complete allocator census and additional time/RSS gains are not claimed | Preserve publication while qualifying distinct E9/E10; no extra broad runs |
| #56: lifetime and allocation attribution | Read-only E6 graph/evaluator ownership inspection; runtime unchanged | Interned terms remain in Program graph until destruction; evaluator/readback scratch frees separately. Both measured variants retain the same arena/index defaults | Findings recorded separately; no new owner implementation or tuning patch | Per-owner RAM and independent arena/index or head/direct timing factors remain unmeasured | Preserve these limits in future agreed cost scope after exact coupled qualification |
| #56 / MEM1: delete unnecessary work/state for actual memory/time | V1 inline-state and V2 materialized-callback UAF rejected; V3 recycles only stateless captured-head frames after parent cleanup | Root exact128/1944 verified, fresh callback3 O2/SAN0; current-E12 candidate1816/base2064 and exact36 cost samples/expected steps/raw hashes verified, no censored samples | V3 exact18 taskc861728 pushed/remote exact; prototype Maincc69f52 same bytes; cost111/addendum taske561030/Main895ac67 exact, original109 unchanged; accepted/default selection unchanged | Bounded tree400 median wall5.34021->4.92598s, peakRSS1891928->1001764KiB; LocalSorted RSS+492KiB, smallList startup dominated. Strict3 and full Goal/accepted adoption remain open | Exact cost evidence published; integrate current corrected Job layers separately, then continue owned deletions |
| #56 / MEM2: preserve captured lexical state | Private beta environment elisionc3c91bd3 rejected on qualified V3/E12 source1280deb36a7 | Root verifies exact30 freeze0573b9be and original results; fresh four O2/SAN builds0, fourteen executions reproduce six semantic failures. Parent local/images pass; candidate retained binding becomes unbound, fuel2->1; no sanitizer diagnostics | Root report5ac6c445; no rejected runtime integration or current-parent defect | Public retained closure/configuration contents and lifetime must be preserved; no physical-layout or full-machine checkpoint claim | Direct-IADT no-trailing-argument field-spine lifetime research continues privately; qualify live/restored/retained nodes before reuse |
| #56 / MEM2: delete copied IADT field spines | Source1282e87fd84/patchb19dd27b; reuse only empty-tail immutable captured fields, nonempty tail copies preserved | Root all1964/source128 exact; fresh O2/SAN builds/ten controls0; SAN45/focus23/cross140, public52/history28/fuel TSV equal references. Twelve census/fuel controls verified | Exact9 task6b0ed93 on parente561030/prototype Main6ebf453 pushed/remote exact; accepted/default selection unchanged | Cumulative tree400 -2408960 requests/-77086720bytes is not actual RAM/time; strict3 and original helper/failure history preserved | Existing-scope exclusive grant18:00-18:10UTC, exact36 jobs/config8cc0b753/279 pins; terminal raw expected-step/pin review and release pending |

Fresh Git inspection verifies HEAD and tracking branch at `0539051`; every
committed/live file matches the corrective 44-file frozen manifest. Core's exact
current-Main `341261d` joint snapshot is
`/home/repyt/workspace/a-program/src/prototype/solver_inputs/core_job_performance_surface_trial`,
pinned by `/tmp/a-program-core-job-performance-combined-source.sha256`.
Core's initial joint focused report was 11/11 zero; the fresh broader results
below supersede that initial incomplete status. Included adapters remain explicit.
Fresh reads of Core's five-input/four-variant census verify all roots DONE.
Imported LocalSorted takes 766,477 baseline/Job-only and 761,848 head/combined
steps. Job E1 removes 3,753 stored result references and 32 counted Job bytes,
with all other final columns unchanged, including 114,388 typed occurrences and
94,974 Evidence. Detailed arithmetic/source pins are retained in
`/tmp/ap-performance-joint-cost-341261d-20261003/four-variant-matrix.json`.
These are freshly inspected Core-run records, not this worker's measurements.
The fully dereferenced private copy at
`/tmp/ap-performance-current-joint-341261d-0539051-20261003` matches all 128
manifest records. Core's original records also still match after copying.
`snapshot.json` pins runtime sources, both Makefiles and included Core/Identity/
Synthesis/Source checkpoint test adapters. No Core or frozen files changed.
The bounded read-only task probe exposes only this worker's `/root` entry;
available task tools cannot inspect the external Core thread. Its status is
unavailable, not established as `notLoaded`; no wake message or load occurred.
The initial O2 syntax inventory recipe fails because the private copy omitted
four `training/*.p` inputs present in Core's snapshot. The unchanged script
correctly changes into the private root. All 158 original inventory paths exist
in Core's tree; only those four are missing here. Preserve this copy/setup
failure separately from runtime failures and the corrected gate result.
After dereferencing the omitted inputs, the unchanged syntax inventory passes
all 158 entries. Its original failed recipe remains in the O2 acceptance record;
`syntax-inventory-copy-diagnosis.json` pins the correction and separate rerun.
The joint ASan/UBSan/leak build terminates with 38 recipes, zero failures. All
45 subsequent commands pass, including seven checkpoint controls, all 70 raw
TotalResult separate-process cuts, nine Force write/read images, success/error
head cleanup and return2 fallback. Source hashes match before/after. Exact
evidence: private `asan-checks/checks.json`, with original seven failures separate.
O2 artifact/checkpoint batch is terminal: 41 recipes, with only the strict public
partition target failing. Transport, semantic, history and all seven checkpoint
controls pass. The three reload failures remain: 1000+1000 and 1600+1600 reloads
stay pending with different bytes/fuel; completion1915+0 stays pending despite
equal bytes/fuel. Current producer images are 51,876 bytes, distinct from the
historical older producer's 50,820 bytes. Exact private `o2-partitions/partitions.tsv`
retains every zero-step comparison. All 23 separate O2 focused commands pass.
O2 and sanitizer unit outputs agree, including 3,014 charged WHNF steps; every
TotalResult image also loads with the other build's reader in 140 successful
exact-fuel/readback controls. Four image hashes differ across instrumentation
(case2 cuts18/19, case4 cuts8/9); this byte observation remains visible in
`raw-cut-coherence.json`, without claiming cross-instrumentation byte equality.
Existing `machine_resave` assertions and public strict byte controls are unchanged.
O2 acceptance is terminal: 384 recipes, one initial syntax inventory copy/setup
failure. That failed recipe and the unchanged successful rerun stay separately
reported; all runtime acceptance gates pass after the input copy correction.
`o2-acceptance-result.json`, `o2-artifact-result.json` and `summary.json` record
actual recipe exits; diagnostic make's aggregate zero is not an acceptance pass.
Every Core/private source hash still matches all 128 manifest records. All 44
frozen files match both live bytes and published `0539051` blobs. HEAD and origin
are unchanged; no evaluator, owner, test assertion or frozen file was edited.
The joint evidence is frozen in private `verification.sha256`, 1,033 records,
SHA256 `ae05927ed56cb773e9513cbcf5e2ff1b5d687ede76ec0d0bd89930ab74fe7460`.
The 10.8 MB evidence tar excludes compiled binaries; exact archive metadata is
`/tmp/ap-performance-current-joint-341261d-0539051-20261003-evidence.json`.
Separate primary-source qualification now passes all 800 concrete equalities in
each unchanged pinned Bend/Lean/Agda/Rocq source. Bend's `--verdict` runs the
privately compiled pinned BendTT kernel; direct kernel Boolean/tree false-endpoint
controls reject with proof errors. The corresponding AP port reaches DONE on
the E1 joint producer in 183,507,626 charged steps, and ordinary completed-image
reload reaches DONE in 183,501,946. These are fresh correctness/step observations,
without wall/RSS comparisons; helper declarations and proof encodings differ.
Exact commands, hashes and results are linked in the unfrozen separate prototype
[`performance_cross/README.md`](../src/prototype/performance_cross/README.md).
This does not alter the frozen 44-file performance publication or E1 joint evidence.
The separate committed Main `898463e` plus E2 `44b0389` assembly is now terminal:
384 full O2 acceptance recipes, zero failures; 38 sanitizer build recipes and
45 ASan/UBSan/leak commands, zero failures; 23 focused O2 and 140 cross-build
raw-cut readbacks, zero failures. O2 artifact batch has 41 recipes, only its
strict partition recipe fails with the exact same three reload records as E1.
Transport/semantic/history and all seven checkpoints pass. Original E2's 40
freeze records, committed 11 lean/five canonical blobs and private 128 runtime
hashes match before/after. The 44 performance and 1,033 E1 evidence records are
unchanged. All manual test helpers use tabs and English comments.
E2 source manifest SHA256:
`e615a1e091f6193b40dd9d4cfaeaabc0776e2f775195edea7e28843438846ba2`.
Frozen evidence has 2,007 records at
`/tmp/ap-performance-current-joint-898463e-44b0389-20261003/verification.sha256`,
SHA256 `690363d184c9c2cdef7c051561bf784bc6df3c841c2b51bb638f83c4693b1907`.
Its exact summary/archive hashes are in the adjacent `-evidence.json` metadata.
The private setup-column failure, preliminary dependency-count helper assertion,
two E1/E2 O2 byte observations and 24 successful repeated writer/reader controls
remain in that bundle. No runtime or test assertion was adapted for these results.
E3's separate assembly verifies all 20 committed publication records and applies
its exact codec/test patch without fuzz. Only `source_io.c`/`source_io.h` runtime
hashes differ from the frozen E2 parent. Source manifest SHA256 is
`e87b8781d160d48b961e2091640058aa21dccb92a4a76f50eea548519764622e` in
`/tmp/ap-performance-current-joint-898463e-51c476d-20261003/`.
Its SourceIO files and seed test match committed E3 tested hashes. Dedicated O2
codec/acceptance/checkpoint batch is terminal: 57 recipes, only strict partition
fails with exact E2's three records. Relevant sanitizer build18/commands13 pass.
Both E2 parent negative controls terminate at the intended missing-body assertions
(-6), with the policy control observing nine started bodies before saving.
Seed rejects versions 0 through 68; the separate parent68/current69 image control
has all four expected exits and inert zero-step reads. The initial diagnostic
helper expected the wrong error text; its failed predicate/report is retained
separately, without a runtime or test-assertion change. E2 remains immutable;
all source hashes match after. E3 evidence is frozen at
`/tmp/ap-performance-current-joint-898463e-51c476d-20261003/verification.sha256`,
1,318 records, SHA256 `9bd47236a0582ba37a243baacb92e6ba03cdec781938ab7e72399ed520595261`.
Summary SHA256 `3421fed9c8ac1dc81281201f231ce6a538df0092cb8976d7a36bdb6f62797b36`;
archive metadata is in the adjacent `-evidence.json`. This targeted SourceIO
qualification uses retained E2 full acceptance evidence; it does not claim a
new E3 whole-suite run. E4 final broad qualification is next.
E4 is assembled separately at
`/tmp/ap-performance-current-joint-898463e-ec95371-20261003`, with runtime manifest
SHA256 `f68fe0a5d4ea303795d241d5bc2975ef892bc318dfd8e1b61f4b1f6ca4bf66df`.
Full O2 acceptance384, sanitizer build38/commands45, focused O2 commands23 and
cross-build readbacks140 are terminal with zero failures. Artifact41 fails only
the strict target, retaining the exact E1/E2/E3 three reload records. Synthesis
namespace control, owner/environment/import/resume and all seven checkpoints
pass. All 128 runtime and included test hashes match before/after; the E3 parent
and 44 performance files remain unchanged. E4 evidence is frozen with 1,945
records, manifest SHA256
`0aeef32911e1b9980b003d361ba3a71ffe2f06d6a0fa77c6241675998a8ef726`;
summary SHA256 `9af487988a528f6fbfe2e93b5f708e0dcef171f9502d6e59d34641aca5a55f65`.
The 17-line test-only augmentation is pinned separately in
`namespace-parent-control.json`; no rejected E5 runtime was imported.
E6's separate private assembly is
`/tmp/ap-performance-current-joint-898463e-b2d6668-20261003`; its 128-record source
manifest SHA256 is `c3ff50a9745bcca76809f68b7a435e2faeb1e20ea609469353522ef73bd509eb`.
Only `src/synthesis_binding.c` differs from E4, SHA256
`77a7af11536020817ca62ed7dd172f80dad768dcd134cef05fa694320ee7271b`.
The Synthesis test exactly matches committed E6's tested hash
`d4502b5b2bf68182825b8092f3711a3647a464d023f7e44c800e2b3a64bd74ad`.
E4's augmentation was reversed only in the new copy, then committed E6's test
applied once. The first helper incorrectly required one global `Alias.zero`
string; an older positive request fixture also uses that string. Preserve the
failed partial copy, trace and JSON separately. Exact committed test hashes
already matched; correcting the helper to count the precise rejection statement
changes no runtime or test assertion. E6 relevant O2 is terminal with 408 recipes,
only the strict partition target failing at the exact E4 three records. Sanitizer
build38/commands45, focused O2 commands23 and cross-build reads140 pass; transport,
semantic/history and all7 checkpoints pass. Full E4 acceptance384 is retained
separately rather than relabelled a new E6 whole-suite run. E6 is frozen with
1,939 evidence records, manifest SHA256
`ab3d3f2e1428de29d9c3457b55e1f35c2688bbcc60495c2b88e1fa926b753e22`;
summary SHA256 `ec36a57245cfc5110562af46cffc9717fbca5d8895c53fea4e5f2f6968ef4f2b`.
The E4 parent and performance44 remain unchanged. The matched ordinary-evaluator
baseline at `/tmp/ap-performance-measurement-baseline-e6-20261003` differs in only
`computation.c`, `eval.c`, `eval.h` and `iadt.c`, keeping all owner hashes fixed.
Its 128-record manifest SHA256 is
`047e1dcb1ff4188a0ec0977e09a0b87a568e8e4a2146400685e7bca357d2824e`.
Private baseline tests reverse only the earlier performance observer adapters.
Its build10 recipes and four original-observer/IADT/SourceIO commands pass, plus
all6 corresponding positive/false endpoint controls across baseline and head.
The two-pair positive charges 1,442,531 baseline versus 1,170,277 head steps;
both false endpoints reject. These are fresh correctness/fuel results, not
timings. Qualification JSON SHA256 is
`9924d23aa968a3771b76fb8c63896351ac40813d76a1f29fbfd0864a8396c58e`.
Fresh Main `4d1d941` accepted inputs and Surface/performance match `898463e`;
pinned E2/E3/E4/E6 owner layers remain explicit. No measurement has run.
Light preparation also qualifies exact List and imported LocalSorted inputs on
both E6 variants: List DONE1921/1915 and imported QuickSort DONE766477/761848
(baseline/head). Initial QuickSort CLI commands omitted the frozen provider's
required `--legacy-intrinsic-dot` option and failed parsing `#.Name` on both
variants. Preserve `matched-input-correctness.json` and logs; only the command
option changed. Corrected report SHA256 is
`2e1521940b69734a47feef8f85dfe8cc6c7674afcd035c76c5fc558dca1a699d`
under `/tmp/ap-performance-measurement-e6-preparation-20261003`. The inputs,
runtime and tests are unchanged; no wall/RSS values were collected.

### Assessment

2026-10-03 17:58 UTC, Root current MEM2 IADT review: exact9 task6b0ed93/
prototype Main6ebf453 published; all1964 raw evidence and source1282e87fd84
independently verified, only iadt.c differs from qualified parent0deb36a7.
Fresh Root patch reconstruction, O2/SAN builds and ten controls0; public52/
history28/fuel TSV exact, inherited tests unchanged and original strict3 retained.
[Root source/lifetime/grant review](../src/prototype/performance_followup/mem2-spine-root-review.json)
records all279 pins and original reporting/setup failures. Reuse requires an
empty selected caller tail and preserves immutable environments; node forests
outlive head cleanup under existing machine/output-graph contracts. Census
tree400 -2408960 requests/-77086720 cumulative bytes is not RAM/time. Root's
existing-scope exclusive grant18:00-18:10 UTC covers only36 configured sequential
jobs; actual results and terminal release remain pending. No accepted promotion
or full Goal completion; rejected beta environment deletion stays excluded.
2026-10-03 16:47 UTC, Root superseding current review at Main3b09a98:
V3 source18 and measured cost111 are published prototypes; later current
qualification/measurement in the issue table supersedes the historical V3
pending paragraph below. MEM2 constant-body beta elision is rejected separately:
all30 frozen files0573b9be and source1280deb36a7 verified; fresh Root four
O2/SAN builds and fourteen executions reproduce six semantic failures. Parent
local/cross-reader configurations pass; candidate code/data lose a retained
binding and one charged step. No sanitizer diagnostics or current-parent bug.
[Root review](../src/prototype/performance_followup/mem2-capture-root-review.json)
records unchanged V3/E12 and the result/fuel control. Configuration fragments
do not establish full-machine persistence. Candidate runtime is not integrated;
independent field-spine lifetime research continues with no publication hold.

2026-10-03 14:36 UTC, Root superseding review: V1 and V2 are rejected.
New materialized-failure callback control93da2cfa lets the callback destroy its
machine arena and return-1. Parent SAN0; V2 SAN signal6 with heap-use-after-free
in retire_frame, report28649b93. The public header does not explicitly prohibit
this action. No invented callback ban or test waiver replaces the failing case.
Frozen V2 task1297e0b/Mainf48ab5e and1969 c0734afa evidence remain exact:
their passing384/45 and Root focused controls did not cover this path. Those
results and V2 selected capacity/cumulative deltas are historical rejected-
candidate evidence, not a safe deletion or current improvement.
V3 remains private/unqualified: delete materialized/fallback retirement and
pool only stateless captured-head frames after the existing parent cleanup.
Retain non-NULL state and return2 behavior. Fresh correctness/capacity is
required; V2 values do not establish V3 memory/time. Actual peak/RSS/speed,
exclusive cost grant and accepted promotion remain absent.
[Root lifetime review](../src/prototype/performance_followup/mem1-root-review.json)
preserves both rejections and earlier evidence. CorrectedE11/E12 joint work
stays separate, excluding MEM1. The full performance Goal remains active.

2026-10-03 MEM1: continue on a private runtime-identical qualified `73fa86c9`
copy, without publication/review hold. Existing allocation audit counts
cumulative external requests and excludes graph.c internal calls; it is not
live/peak attribution. GDB reads actual DWARF arena/index/query layouts at
selected cuts, without inferior calls or runtime wrappers. Initial completed
list/tree-prefix controls pass, with completed shared-substitution/WHNF scratch
already freed; scaling and nested-query accounting remain open. Preserve the
observer setup error (missing max_align_t DWARF typedef), corrected by reading
the actual aligned block type. Capacity snapshots do not establish peak RSS.
Root's E10 review/publication report updates status only; immutable source and
evidence snapshots remain unchanged. Preserve a separate exact two-doc status
snapshot for later publication, while these active docs remain mutable.

The latest direct workflow replacement supersedes prior Core routing for future
material notices and measurement requests: use the existing outbox relay to
Merge. Preserve historical Core notices and frozen evidence unchanged. Current
E8 gates use separate private j1 processes, at most two concurrently; no further
wall/RSS measurements are granted during correctness work.
Fresh read of Main `4d1d941`'s central schedule confirms E4/E6 qualification
first, followed by matched runtime/memory and audit cost in an exclusive slot
owned by Merge. The read-mostly #59/PR #60 lane owns its static invocation/debt
inventory; this lane does not create a second inventory or delete tests. Keep
storage and fuel evidence separate and share later measurement artifacts with
that lane through Merge, avoiding duplicate runs.
Merge reports active and E4 integrated as prototype `3a8c024`, with metadata
`be0446d` pushed; E6 and the slot proposal are received for distinct review.
Job/C are asked to finish current heavy gates safely before pausing. This is
Merge's scheduling decision, not a granted slot or new human design approval.
Continue light preparation; coordinate overlapping audit cost in the same slot.
Merge now grants exclusive slot `e6-shared-cost-20261003-0812`, ending absolutely
at 2026-10-03 08:52:05 UTC. Job hold `5289e8a3` and C hold `de394122` are reviewed;
Merge reports independent inspection found no heavy children. Run only the 30
pinned `dbfc0ce5` jobs sequentially, three repetitions and at most 180 seconds
per run. Derive a private config changing only `maximum_seconds` to the remaining
window and record both hashes. No extra E4 pair, builds or broad gates. Send an
immediate terminal/all-children-stopped outbox notice; start no other heavy work
until Merge hands off/releases. Audit timing is ungranted until this notice.
Published E7 `3ad0303` and E8 `50cd56b` qualification waits for release. This is
an operational slot grant under existing user authority, not a new design rule.
The granted attempt is terminal before sample0: `/usr/bin/time` is absent and
Popen fails exit1 before launching any benchmark child. Zero of30 samples ran;
no wall/CPU/RSS result was collected. Preserve the unchanged partial report,
trace, original/derived config hashes and separate terminal setup JSON under
`/tmp/ap-performance-exclusive-e6-shared-cost-20261003-0812`. Immediate all-children-
stopped outbox notice sent. Hold heavy work until Merge hands off/releases;
do not alter the pinned runner or infer a new grant from this failure notice.
Merge receives terminal notice `abc0e73f` and accepts no benchmark samples.
Its next operational direction prefers a distro GNU time executable extracted
privately under `/tmp`, with package/tool hashes and provenance, a trivial
metrics smoke test, and exact revised collector/config pins preserving all30
job argv/input pins. This supersedes the unexecuted wait4 instrumentation
proposal; preserve that proposal and the original failed attempt. No benchmark
restart under the old grant. Shared absolute hard stop stays 08:52:05 UTC;
audit remains ungranted and heavy correctness held.
Merge supplies its extracted Debian trixie `time` 1.9-0.2 amd64 package/tool.
Independent hashes, package metadata, libc/loader dependencies and original GNU
time plus timeout/true smoke pass. The original collector remains untouched;
private runner `9c50c62d` changes only its literal executable path. Proposed
config `e89b39e6` preserves all30 argv and all285 original pins, adds seven
instrumentation/runtime pins and derives maximum_seconds=1174. Latest safe
launch is 08:32:29 UTC under the same hard stop; if review exceeds that time,
derive only the remaining window again. Exact provenance/pins are in private
`gnu-time-instrumentation-proposal.json`, SHA256 `70126435bf4ca21e9fe37470102058a3e7a2d681e0fd4c5f751f98822e917cf2`.
Send the exact ready key through the existing outbox and await a revised grant.
No benchmark has restarted; preserve the old failure and superseded proposal.
Merge freshly reviews all292 pins and grants
`e6-shared-cost-gnu-time-20261003-0833`. At actual launch derive only
maximum_seconds=min(2400,floor(remaining absolute seconds)-2), preserving
reviewed config `e89b39e6` and recording the launch derivation. Run only its30
sequential fresh samples, at most180s/run, stopping all children by08:52:05 UTC.
No rebuild/extra workload/E7/E8; other heavy lanes remain held. Immediate
terminal notice must distinguish incomplete/censored samples. Audit remains
ungranted until a separate handoff with at least eight minutes left. The private
pre-grant v2 window refresh is superseded by this actual-launch derivation.
Actual launch 08:35:04.485250 UTC derives 1018s from 1020.51475s remaining;
config SHA256 `390adf4137e93708bf8ff580241975fee6edb52057bae10fd8aff317fb8f7e64`.
All 30 samples pass with zero censoring and all 292 pins match before/after.
Runner/all waited wrappers terminate 08:36:55.938837 UTC; terminal outbox
`ac01d166` shares raw measurements SHA256
`887a96d14275da9ade55d5a42428ea77248c63403b8c60f6d9f55a06761abc1f`.
Matched tree medians 17.23→5.40s and 2,971,260→1,891,724 KiB show 68.7% wall
and 36.3% peak-RSS reductions in this experiment. Imported LocalSorted wall
medians remain 0.63s; List rounds to 0.00s. Native values and qualified boundaries
are in `performance_cross/results/e6-exclusive/README.md`.
Merge independently verifies every sample and pin, accepts the exact workload
improvement with overlapping QuickSort ranges and startup-dominated List, and
requests a compact frozen report. Heavy work remains held during its separate
four-job audit review/possible sequential phase; no E7/E8 correctness yet.
Release follows audit terminal or window expiry explicitly. A bounded reread of
Merge's central brief identifies the queued old Core full-build-slot note as
obsolete; it does not replace this fresh grant or terminal status.
Merge explicitly ends the shared measurement window after terminal30/stopped
samples. Its audit four-job review missed the eight-minute start boundary;
audit cost is deferred and no audit run occurred. Heavy correctness is released:
qualify exact E7 `3ad0303` first, then E8 `50cd56b` atop that exact layer, keeping
namespace lifetime, lexical names, borrowed-output guards and producer-pinned
strict3 visible. Exclude E9 runtime. Use at most j2 with modest concurrency
alongside Merge C7 j1; no further wall/RSS while correctness overlaps. Freeze
the measurement report separately. These are Merge operational directions.
Merge requests a light four-file accepted-baseline advisory for captured-head,
direct-IADT and cleanup, with exact dependencies/tests and unrelated owner,
Surface and C work separate. Merge owns isolated accepted promotion and its
full regression while preserving dirty files and strict failures. Prepare the
reviewable diff privately; no accepted-source edit or worker integration action.
Private E7 original delta dry-run fails on existing Surface context before any
runtime mutation; the failed copy and fresh read-only reproduction stay separate.
Assembly verifies canonical E7 bytes after reversing only Surface in a private
E6 copy, then composes disjoint changed-line ranges. All17 committed records and
canonical test bytes match; source128 `26a57bde` and tested156 alignment
`c63ba010` retain every inherited difference. Direct ordinary Surface-after-E7
patch applicability is not claimed; Merge independently reviews that pipeline.
ASan38 build recipes and45 commands pass. Five paired DONE censuses preserve
checking counts; two captured Graph owners add16 Job bytes, separate from
removed metadata storage. E7 is now terminal: full O2 384/0, artifact41 only strict
target fails, sanitizer38/45 zero, focused23 and cross140 pass. Its 1,977-record
freeze `79e42921` is immutable; Merge received readiness `1098d08c` and will
independently assemble canonical E7 before Surface.
Distinct E8 is terminal on that exact parent: source128 `9ba2b33c`, canonical
Synthesis test `5889b2f9`, tested156 `aefbc96f`, all13 committed publication blobs
freshly verified. Relevant O2 408 only strict target fails; sanitizer38/45,
focused23 and cross140 pass, with WHNF3014 unchanged. The unchanged committed
Graph-output control passes captured/callable O2 and sanitizer runs (borrowed9/5,
one Binder VARIABLE leaf), including pending cuts0/1/32/100/1000/completion. E7
fails that control at the own-VARIABLE assertion because its completed Graph
proof has no borrowed output. The reporting helper expected the other assertion
and fails1; original helper/logs/checks are preserved. Corrected reporting only
rechecks the original seven child records without rerunning or changing tests.
All52 public images and the whole strict TSV match E7; strict3 remain failed.
E8 freezes1,956 records in private `verification.sha256`, SHA256
`253a36c017d91a7270da0907519257c6178fb278ec1b43ce35d03e10be1c8749`;
summary `4151814f5ece3f09ef288ce135d1667c464dfd5d5bd31c718b812dc092c7c5dd`.
E7's full384 acceptance is retained separately, without relabelling E8's targeted
batch a new full run. No E9, wall/RSS, owner edits or accepted writes occurred.
Merge reports exact114 task publication `44e479be` pushed and Main merge99efcf7
verified, with Main push still in progress at receipt. Published bytes remain
immutable after freeze release. Separate four-file advisory `c80d1660` is read;
Merge owns isolated accepted build/promotion. Its operational scope permits
that verified improvement independently of unfinished SE/AP issues; this is
not additional human design policy or this worker's Goal completion.
Completion audit of the frozen parent work list identifies one evidence gap:
same-input object/retained-state counts and completed `.a` comparisons at the
measured E6 producer. Existing full-source measurements cover wall/RSS/fuel;
older E1 censuses and prefix image equality cannot establish this larger scope.
Use the existing committed census and ordinary artifact API in new private
outputs for the three measured inputs, sequentially at j1. Do not collect time/
RSS, mutate frozen bundles, add owner logic or duplicate accepted promotion.
Check completion, charged steps, artifact bytes/hashes and ordinary fresh reload
explicitly; retain any failure and producer-specific strict3 without a waiver.
At the next safe boundary Root requests light work/wait until exact E9/E10
freezes and qualification order arrive, without duplicate broad gates or timing.
The E6 follow-up source/census/readback children are already terminal. Freeze
their evidence only; do not ingest E9/E10 or restart heavy work. Root reports
accepted-only four-file independent gates pass and its full accepted suite is
still running; this is a report, not a completed full-suite claim. Root received
E8ready `8d35fcd2`; E7 canonical pipeline initially failed only on obsolete
Surface context, then its regenerated context preserved added/removed semantic
vectors and exact qualified hashes. Preserve both failure histories and every
published measurement/E7/E8 byte. Main E7 local integration/push forthcoming is
reported; remote publication remains unverified here.
The bounded E6 follow-up is terminal: six DONE censuses/source saves, twelve full
fresh ordinary reads and twelve inert zero-step resaves. All zero-step reads
report pending3 with steps0; resave equality does not accept their proof or waive
strict3. Ordinary/head completed images match at51,876 bytes (List),7,154,482
(imported LocalSorted) and2,988,853 (trees400). Trees preserve61,047 typed
occurrences,85,764 Evidence and114,887 Jobs while interned Core terms fall from
10,636,362 to298,214. Imported LocalSorted removes1,145 Core terms; List's count
is unchanged. Every other reported final census column except charged steps is
equal. These are interned-state counts and computed storage extents, not another
RAM/time measurement or a complete allocator/semantic-object census.
Initial artifact metrics build2 selects the old export interface; its error/log
are preserved. Explicit selection of the already frozen E6 adapted helper builds
successfully, but its binary is not executed after the safe boundary hold.
The65-record private manifest is `eb4e7542ed648b6ed80665687f87303550b04cb89fca8e61376a2e0b01597b75`;
the two-file follow-up publication manifest is
`e8f3c59520987fec852944b9c16726e4ad4d86cc5f552c785d5f99b5b3255148`.
Chosen task message: `Record matched E6 state and completed image parity`.
Publication remains separate/proposed; all prior published/frozen bytes match.
Fresh read-only Root accepted inspection now supersedes its earlier running
report: `/tmp/a-program-merge-accepted-promotion-evidence-20261003/accepted-terminal.json`
terminates at10:11:41.955673 UTC,386 recipes, zero failed. Independently inspect
all386 JSONL exits, log SHA4478f785 and all13 candidate source/build/test hashes;
all match. The four runtime hashes exactly match the earlier private accepted
advisory. This proves that private O2 candidate gate, not Main publication,
accepted-only speed or a completed full performance Goal. No duplicate run.
Fresh Root sanitizer terminal now also verifies build0/eight commands0 and all
eight raw log hashes, including head/TotalResult/cleanup, Core, Identity/eval/
SourceIO/IADT/Synthesis. This is the private accepted candidate's ASan/UBSan gate;
it does not replace the original seven failed prototype commands or prove Main
publication. The worker launches no duplicate sanitizer gate.
Light source inspection of the measured E6 pair pins identical graph.c/h,
program.c and support.c, with only the approved evaluator difference. `intern`
allocates a pg_entry into the graph arena; individual terms are not reclaimed.
Program's WHNF work uses Program graph, released at `pg_program_destroy`; final
readback writes its immutable Core there while scratch has separate destruction.
Thus the10,338,148 fewer final interned terms are reduced Program-lifetime state,
not fewer typed/Evidence/Job records or a smaller completed proof image. This
supports the mechanism behind the measured RSS reduction but does not assign
RSS bytes to an owner. Both variants keep `512*sizeof(max_align_t)` minimum
arena blocks and64 initial index buckets, so historical small-arena/index factors
are not part of the current3.19 ratio. Independent arena/index and head/direct
wall/RSS attribution remain unmeasured/deferred until an agreed scope/slot.
Source/function pins and explicit limits are in
`/tmp/ap-performance-e6-owner-lifetime-readonly-20261003.json`; no run or edit to
runtime/frozen evidence is needed for this ownership conclusion.
Readonly lifetime report SHA256:
`16e46c88edeaf327f7771b725834fc7480dabacaf3d27af35d525083125ebc4b`.
2026-10-03 fresh read-only publication check: remote `refs/heads/main` is
`cf8cdf5d74fd3e54f8209cb512642f4e16b38941`. Its accepted `7631e5a` contains all13
candidate source/build/test blobs byte-for-byte; E7 `efc3ff0` and E8 `f6d7cfe`
are ancestors. Root's separately assembled canonical E8 matches every128 worker
runtime and six included test hashes, despite its top manifest listing only120
flat source records. This independently checks the eight artifact files too.
The Job task branch remains50cd56b; E9/E10 directory presence is not publication
or qualification authority. No exact new order has arrived; keep the light hold.
Detailed check is `/tmp/ap-performance-root-promotion-publication-check-20261003.json`.
Latest Merge operational order: exact E9 `bd1ddf3a0ac86c365c1b1bf5992eb254cf752a99`
(24 published files) and isolated E10 `6715af2d91f9024fd85e122db57a2415b3c0d747`
(19 files, separate branch) both retain semantic E8 parent50cd56b. Qualify E9
first on the same frozen898463e/family/Surface/head/cleanup/adapters; only after
terminal E9 compose exact E10 in another named producer. Preserve isolated
E8+E10 worker evidence separately. Use j1, no duplicate broad suite or timing,
no live E11/E12. Prefix cursor must await every claimed child. Keep original
split/harness failures and strict3; Job ordinary1921 and performance1915 are
different producers. Surface context repair586579fd uses patch748250cd with
identical added/removed semantic lines. Fresh source/patch alignment precedes
gates; post-promotion helpers58981a7 do not replace the historical producer.
Root verified the bounded65 state-image records/36 outcomes; optional two-file
publication is next. This releases the light-only wait for the specified j1
qualification, not a timing slot or new user design rule.
Core's reporting workflow implements the human feedback request using the single
concise issue-status table above: change/revision, local versus joint verification,
task publication versus Main integration, remaining criterion/blocker, and next
epoch. Core sets milestone/blocker updates and a six-hour active-work reporting
cadence, brief pointer/hash/changed-row notices, exact freezes and no invented
percentages or closure/promotion claims. These are operational decisions, not
direct human design statements. This provenance correction changes only the
unfrozen active brief; preserve all historical frozen handoffs unchanged.
Core delegates testing this exact snapshot: copy it dereferenced into private
Performance output; never edit Core's trial or frozen `0539051` files. Verify all
128 source hashes before/after strict O2 acceptance, transport/semantic, seven
checkpoints and relevant ASan/UBSan/leak gates. Existing Core and Identity observer
adapters are included; do not adapt new failures silently. Keep the three strict
public reload failures and original rejected input/observer/sanitizer records.
This is test delegation, not a second Job/Evidence implementation.
Shared owner findings go through Core to `job-evidence` under Core's operational
routing; this is separate from the direct user's coupling requirement.
Job E1's census reduction is stored references/class extent, not additional
dispatch reduction, arena retention, RSS or time. Use the common current producer
and frozen Job E1 for subsequent measurement; older producer results remain
historical. Timings await Job E2 completion and an agreed exclusive slot.
Core operational routing: probe thread `019ebfae-06be-7b71-974a-b97505daed4a`
read-only through existing task tools. If active/idle on the same owner, send one
`[worker-notification performance wake-probe]`; if notLoaded, report it and do
not resume/load/fork/send a turn. Prepare a short lane/epoch/commit/manifest/
failure notification only for a ready epoch, blocking decision or conflict, not
routine tool outputs. Core keeps its Goal with interruptible `clock.sleep` and
no second scheduler. This worker's Goal and delegated broad tests remain active.
Core reports its six-hour interruptible Wait fallback recorded in pushed Main
`9cb96a9`. The unavailable route probe is finished; further transport/docs
research is outside this bounded probe. Correct the private input copy and rerun
the unchanged syntax gate, without changing tests or any pinned runtime source.
The delegated gates are now terminal and this worker's broad correctness slot
is released. The evidence supports reviewing the exact combined snapshot;
the three strict reload failures still prevent claiming all broad gates passed.
Core now reports its guarded relay verified against the exact sleeping Core
thread using the Job E2 outbox. This supersedes unavailable task messaging for
material handoffs: write one owning-lane `src/prototype/coordination/outbox/*.txt`
notice with `apply_patch`, newline-terminated and at most 8 KiB. Include epoch,
commit, manifest, gate results and known failures. The Core-owned relay delivers
only a pointer/hash; it does not merge or resume a missing Core. This route is a
Core operational decision, not an additional user design principle. This worker
writes one terminal joint E1 notice at
`src/prototype/coordination/outbox/performance-joint-341261d-0539051-ready-20261003.txt`;
no routine output notification is needed. Core now confirms relay receipt and
fresh verification of all 1,033 evidence entries and the summary. The earlier
read-only task probe remains unavailable; no new transport research or direct
task-thread load is needed.
Further performance work uses the tested current producer/Job epoch and awaits
Job E2 plus an exclusive timing slot; this handoff does not complete the Goal.
Latest Core operational delegation: after `job-evidence` supplies the exact
rebased E2 freeze, assemble E2 plus current family/Surface/performance in another
detached private copy and run focused, full acceptance, persistence and sanitizer
gates. Core is integrating reviewed E1/performance prototypes, without claiming
accepted promotion or wall speed. Do not edit SE owners or infer E2 completion;
the exact freeze has not arrived here. Keep the parent research work list active
while waiting. There is no exclusive timing slot and no current stale full-build
slot restriction. Core now reports prototype E1 `b31d7a5`, performance `f27bd13`
and C4 `b0422d7` integrated/pushed; committed sources plus Surface freshly match
all 128 joint runtime hashes. These are Core verification reports, not accepted
promotion or a wall speed claim. E2 remains next after its exact rebased freeze.
Private pinned BendTT admission and full-source qualification continue within
the parent research work list, without a competing SE implementation.

### Plan

Earlier desk operational schedule: finish E9+E10 unchanged, then an explicit
joint memory epoch with Job. Pin peak/live/cumulative bytes by Core/typed/
Evidence/Job/query/index/scratch and representative size scaling using existing
tools first; choose the simplest safe owner/copy/lifetime deletion. A later
agreed exclusive slot is needed for matched wall/RSS. No immediate heavy reruns,
unsafe borrowing, new authority/graph or profiling-only wrappers. These are
operational Assessment/Plan decisions, distinct from the human priority above.
Latest direct user workflow supersedes a blanket scope/review/publication hold:
after the live gates, continue independent existing-tool allocation/lifetime
attribution and next owned prototype work. Coordinate shared owner changes;
retain exclusive scheduling for comparative wall/RSS only. Exact submitted
epochs remain immutable.

- Maintain the single issue-status table and send brief material updates through
  the existing outbox, at milestones/blockers and at least every six hours of
  active work per Core's workflow; do not rewrite frozen historical handoffs.
- [x] Copy/pin the exact joint snapshot and included tests into private output.
- [x] Audit every O2 acceptance recipe exit and all artifact/checkpoint gates;
  retain original copy failure and three strict reload failures.
- [x] Complete relevant ASan/UBSan/leak and all-cut fresh-process controls.
- [x] Verify all 128 source and 44 frozen-file hashes after terminal gates.
- [x] Freeze the joint evidence manifest and write the concise owning-lane outbox handoff.
- [x] Receive/pin the exact rebased E2/current family/Surface/performance snapshot.
- [x] Assemble it privately and run focused, acceptance, persistence and sanitizer gates;
  retain setup/observer/strict reload failures and verify source hashes before/after.
- [x] Qualify exact committed E3 SourceIO separately, including inert resave, body9
  parent negative, invalid markers/old format, semantic/seven checkpoints and sanitizers.
- [x] Qualify committed E4 plus the 17-line namespace parent control separately;
  exclude E5 runtime, retain epoch attribution and complete final broad gates.
- [x] Qualify committed E6 on that same producer after E4; include its namespace
  control once, preserve prior layer evidence and retain strict failures.
- [x] Qualify the matched current-Job ordinary evaluator with original observers
  and corresponding positive/false endpoint controls; pin runtime/input hashes.
- [x] Propose a 40-minute exclusive wall/RSS slot through Merge after terminal
  E6 and matched-baseline qualification.
- [x] Run the pinned measurements only after Merge grants the slot; all30 pass.
- [x] Freeze exact measurement evidence with #59 audit ownership;114 files,
  manifest `0f017454`, chosen message and delegated publication notice `ffb9e1a1`.
- [x] Verify all114 task-branch blobs at `44e479be`; Merge reports Main merge
  `99efcf7`. Published bytes stay immutable; future changes are separate epochs.
- [x] Qualify exact committed E7 separately after explicit release, retaining
  namespace lifetime, lexical names, borrowed-output guards and strict3.
- [x] Layer exact committed E8 atop E7; exclude E9 runtime and qualify separately;
  freeze1,956 records `253a36c0` and send the distinct readiness notice.
- [x] Support Merge's completed selected thirteen-file accepted performance
  review; all candidate files match and original failures remain separate.
- [x] Compare same-input interned state and completed artifacts on exact measured
  E6 pins; explicitly exclude semantic-object/complete allocator counts, retain
  storage/timing separation and freeze the bounded two-file evidence handoff.
- [x] Receive exact E9/E10 task publications and qualification order; preserve
  their common E8 parent and the pre-promotion producer pins.
- [x] Qualify committed E9 separately at j1, preserving prefix-child waiting,
  raw split/harness failures, strict3 and original observer/sanitizer failures.
- [x] After E9 terminal, compose committed isolated E10 in a separate producer;
  retain isolated E8+E10 worker evidence and qualify this new combination.
- [ ] Continue MEM1 independently after current qualification, without a Merge
  review/publication hold:
  attribute peak/live/cumulative bytes and scaling using existing tools; jointly
  delete unnecessary work/state with Job, then demonstrate net actual memory/time
  with unchanged meaning/fuel/step0/split-resume in an exclusive matched run.
- [x] Review/reproduce rejected MEM2 beta environment elision separately: retain
  all six semantic failures, parent/current V3 qualification and immutable evidence.
- [x] Review/publish exact9 MEM2 IADT empty-tail source freeze separately, with
  source128/all1964/raw gates, fresh Root O2/SAN controls and owner lifetimes.
- [ ] Review exact36 matched samples from grant MEM2IADT-E12-20261003T180000Z-600:
  expected charged steps, raw hashes, all279 pins, censored outcomes and stopped
  children; release slot and report actual gains/limits separately from census.
- [ ] Carry the parent Goal's remaining full-performance criteria here: full
  workload/checker qualification, including BendTT where applicable, and honest
  separate owner/timing attribution. Historical parent checkboxes are superseded;
  this is the sole current work list. Focused delivery does not complete the Goal.

### Current Epoch Assessment

Core supplies the original frozen E2 epoch at
`/home/repyt/workspace/a-program-workers/job-evidence/src/prototype/solver_inputs/epochs/job_evidence_e2/`
(reported manifest prefix `27b298`). Assemble its fully hashed canonical runtime
and tests with committed Main `7819353` family/Surface/performance in another
private detached copy. Inspect exact manifest and patch alignment before use;
do not use live canonical worker files or E3/E4. Core reviewed E2's runtime
`+29/-23` module/ref borrowing and const accessor; this is its review report,
not an implementation decision by this lane. Original frozen code is unchanged.
The initial lean publication delta had a TSV newline mismatch; that original
failed delta remains excluded. Core now verifies corrected `delta_v2`: all 16
staged files, five canonical hashes and 11 lean manifest records match index
bytes, and E2 is published/pushed as `44b0389`. Prefer those committed snapshots
to live worker files; retain frozen epoch files only for semantic hash alignment.
This supersedes HOLD without changing the original semantic freeze used here.
Existing Surface migration and performance cleanup/adapters are required in the
joint assembly, with all original failures and no auto-trust waiver. Committed Main
`898463e` includes reviewed E1/performance/Surface/C4, not E2, superseding the
earlier `7819353` assembly baseline. Run the assigned detached E2 common-producer
gates and report exact runtime/test hashes with historical strict three separate.
Verify hashes before/after all gates and retain strict partition failures without
waiver. No exclusive timing slot is granted. The corrected private assembly now
matches all 128 E1 runtime hashes before applying the committed E2 patch, with no
fuzz. Only the three E2-owned runtime files differ afterward. Applying Surface
independently to the frozen E2 synthesis source produces the same combined bytes.
The 128-record E2 source manifest is
`/tmp/ap-performance-current-joint-898463e-44b0389-20261003/source.sha256`, SHA256
`e615a1e091f6193b40dd9d4cfaeaabc0776e2f775195edea7e28843438846ba2`.
The included Core/Identity adapters and Source checkpoint are unchanged; the
Synthesis test matches the exact frozen E2 test. All 158 inventory inputs exist.
E2 is now qualified for a separate test-only handoff with the exact failures
above; this is not acceptance/promotion or Goal completion. Keep its 2,007-entry
bundle immutable while starting the separately committed E3 epoch. The owning
outbox ready notice is `performance-joint-898463e-44b0389-ready-20261003.txt`.
Two O2 raw TotalResult image hashes differ between E1/E2 (case2 cut18 and case4
cut8), solely swapping repeated descriptor IDs 7/8 and 2/3 at unchanged lengths.
Focused repeated writes reproduce hash variability within the unchanged E1
binary and within E2; all 24 fresh writer/reader commands pass. `eval_io.c` is
byte-identical between epochs and enumerates materialization results by index
buckets. That code and the repeat control support an ordering explanation, not
a new E2 semantic/fuel failure. Preserve exact bytes and reports
`E1-E2-raw-machine-comparison.json` and `raw-byte-repeat-controls/checks.json`;
do not claim cross-process byte identity or waive `machine_resave`/strict gates.
The first E2 assembly helper stopped at a migration TSV column-name mismatch
after applying Surface patches, before any E2 runtime patch or gate ran. This
is a private setup error; preserve that partial copy and correct the helper.
Core's next test delegation is separate E3 after E2 qualification: exact local
`51c476d78730ea56d96978ae53a0dfd1d688b096` codec/test diffs, SourceIO started-body
bit in existing byte/APGSRC69 and ordinary lexical reconnection, without imported
accepted evidence/status. Core reports runtime `+26/-8`, tests `+6/-3`, original
strict three unchanged. Verify inert load/resave, body9 negative control, invalid
markers/old format, semantic/seven checkpoints/sanitizers and relevant acceptance.
Keep the E2 snapshot intact and exclude E4 live edits. Initial E3 task-branch
push was rejected; the later retry exited zero and Core independently verifies
the remote at `51c476d`. Do not erase that historical publication failure. Main's
independent C5 merge `435d965` leaves the same reviewed E1/performance/Surface
128 runtime hashes; retain the current producer pin. Each epoch gets a separate
readiness report. Comparative timing waits until shared correctness is terminal.
Core's latest queued test delegation adds committed E4
`ec9537135f63b24ef5e355575dfa53f907251fb0` after distinct E2 then E3 qualification.
Core reports push/remote verified, registration-key deletion runtime `+37/-30`,
tests `+10`, strict three/images unchanged. Its published manifest has 13 records
plus manifest, SHA256 `74b8ebd563b3f7093462d4ee967217ecb1ee54436fa3cbfba5a2ab895b9c2aa9`;
no absent transport record is required. Use a separate same-producer snapshot,
exclude E5 live files, and verify owner/environment/registration/annotation/import/
resume plus final acceptance/sanitizers. Attribute each epoch separately and avoid
repeating identical whole-suite work without a reason. These are operational
test delegations, not additional user design principles. Correctness j2 may overlap;
the older Core full-build-slot note is superseded. The E2 setup-column failure
and all historical failures remain separate.
Core reports Job E5 rejected: Original `zero`/`succ` and Alias
`nothing`/`successor` share a nominal formation but require distinct source
namespaces. The exact E4 parent plus 17-line control passes; the candidate
latest-typed-metadata lookup fails exit 134. Its rejected freeze manifest is
`218b9550fbec3aaeb8a42ecb83d6897691b4547a7ea4dd2fe4748da201bb87eb` under the
Job worker's `epochs/job_evidence_e5_rejected`; Core reports all ten records and
five logs checked. Exclude E5 runtime. Add only its exact `parent-control.patch`
to final E4 Synthesis coverage and attribute those 17 test-only lines separately.
The candidate rejection is not an E4 regression and does not change E2/E3 pins.
Core accepts this lane's issue-status notice `7972df74...`, freshly verifies E2
source/evidence, and reports E2 prototype Main integration `7b27c4c` pushed with
concise evidence/status `37de1e8`. Update the table in place; this does not change
the frozen private producer or claim accepted promotion. Core next delegates
committed E6 `b2d66682be75c21a3c2a8c5138db9d3f0c5e0eb2` after distinct E3/E4 gates:
exact E4 parent, binding-scope owner borrowing and the namespace regression
already included. Verify the 14 published records plus manifest; do not apply
the test control twice. Preserve all layer evidence and exclude E5 runtime.
Core C6 correctness j2 may overlap this lane's j2; no exclusive timing slot.
The ancient full-build-slot note remains superseded. No private checker or policy
waiver is authorized.
Core now reports freshly verifying all 1,318 E3 evidence hashes and exact codec
diff, then Main prototype integration `48364b013c27066b0ac5efc2955f3236b5ab34a3`
published with `fd45c42`. Independent C6 `cea1dc0` passed ten gates against the
same E3 runtime128 and integrated `53debc8`. These reports update the table's
integration state, not the immutable private E2/E3 bundles. E4/E6 remain pending.
After those layers are terminal and the current baseline is aligned, Core asks
for a bounded exclusive wall/RSS slot proposal through its existing route;
avoid unrelated suite repetitions. No exclusive slot has yet been granted.
