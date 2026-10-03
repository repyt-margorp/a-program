# Parallel Development: Inquiry Desk, Merge and Workers

Date: 2026-10-03
Status: Merge owns Main; Performance runs; C and Job are capacity-stalled at the latest review. Test-suite, issue disposition, static audit and finite-function bounded Goals are complete; full development issues remain open.
Accepted semantic baseline: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
Reviewed prototype Main checkpoint: `fd45c42`; original worker baseline: `2d747cc`.
Local state: unrelated accepted-source and test edits remain excluded. This plan
changes no implementation or promotion rules.
Related: [SE1-SE5](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md),
[AP0-AP6](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md).
The tree assignment below supersedes earlier ownership assignments, not the
existing implementation work lists. Historical references to Core describe the
previous coordinator; its integration duties now transfer to Merge, not the desk.

## Problem List

| ID | Problem / owner | Related | Status |
| --- | --- | --- | --- |
| 1 | Inquiry desk -> `merge`: reporting vs coordination/integration | All lanes | Desk answers the user; only Merge integrates after verified handoff |
| 2 | `c-backend`: downstream C design | #61 (supersedes #44/#49); shared policy #47 | C1-C14 bounded prototypes integrated and current-E12 qualified; native Acc/QuickSort remains open |
| 3 | `performance`: simplify wasteful paths and measure performance | #56 / PR #58; #51/#52 | Accepted performance7631e5a; V3 bounded measured tree gain reviewed; MEM2 beta environment elision rejected; IADT source9 published, bounded tree RSS gain/mixed timing measured; full Goal active |
| 4 | `surface`: function/function-graph Binder notation and `.p` migration | #57 / PR #58 | Verified prototype integrated; completed session stopped |
| 5 | `job-evidence`: typed ownership, duplicate deletion and exact resume | SE1-SE5, AP0 | E1-E4/E6-E13 corrected prototypes Main integrated; current source/lifetime/resume verified; E14 frozen/current qualification next, strict3 remain |
| 6 | `verification-audit`: independent read-mostly suite/debt inventory | #59 / PR #60, #51 | Bounded static inventory/pilot complete; measured cost deferred, full #59 remains open |
| 7 | `test-suite`: implement coverage-preserving test reductions | #59 / PR #60 | Bounded Goal complete; exact7 taskbc16df9 pushed/remote verified, Root raw/assembly review complete, prototype Main36a27be integrated |
| 8 | `issue-audit`: evidence-backed GitHub disposition | All open Issues/PRs | Bounded Goal complete; verified dispositions, task8bb5018/Mainb461ace; #61 retains C residuals |

### Current Delivery Status

2026-10-03 15:29 UTC material review at local/remote Main `4853cb6`, exactly
verified. All seven retained panes inspected: Performance pursues its Goal;
C and Job show model-capacity errors and Goal stalled; four bounded Goals remain
achieved. Root Goal metadata reads blocked although this evidence review is
proceeding. No owner resumed, duplicated or model changed. Deliverables below
remain distinct from full issue completion and accepted promotion.

15:51 UTC superseding measurement update: the exact36 paired current-E12/V3
samples finished0 at15:49:08 UTC, before the hard deadline; all raw hashes,
argv/expected steps and280 pins freshly verified, no censored samples or live
benchmark children. Root released the slot. V3 exact18 taskc861728 is published
and prototype Maincc69f52 preserves its exact bytes. No accepted/default producer
replacement. Root recent-thread record is active/inProgress; its preceding two
turns failed with a service content flag. This explains an interruption but does
not prove the exposed Goal metadata's exact state-transition cause. Root's
blocked Goal label is not a present code-review blocker or an achieved result.

16:20 UTC measurement handoff: exact111 cost files (original109 plus separate
interpretation addendum2) published at taske561030 and prototype Main895ac67.
Root verified all frozen raw samples and recomputed the committed medians/ranges;
the addendum supersedes an incorrect list-launcher overlap sentence without
rewriting the original109 bytes. C/Job still capacity-stalled. Performance has
consumed release and now inspects argument/environment ownership; this is
observed owner research, not a new verified deletion or Goal completion.

16:47 UTC MEM2 review at Main3b09a98: Root verified all30 rejected-environment
freeze files0573b9be and original records; independent four O2/SAN builds and
fourteen executions reproduce six semantic failures. Parent local and both
fresh parent-image readers pass; candidate code/data lose a retained binder
value and one charged step. No sanitizer diagnostics or current V3/E12 defect;
qualified source/cost freezes remain unchanged. Reject runtime patchc3c91bd3.
[Root fresh review](../src/prototype/performance_followup/mem2-capture-root-review.json).
Performance now develops a private direct-IADT field-spine reuse experiment
with no trailing arguments; lifetime/semantic qualification remains pending.
C/Job remain capacity-stalled on the same owners; no resume or model change.

17:58 UTC MEM2 IADT publication: exact9 task6b0ed93 (parente561030) and
prototype Main6ebf453 pushed/remote exact. Root all1964/source128/279 cost pins
verified and fresh reconstructed O2/SAN builds/ten controls0; public52/history28/
fuel TSV exact. Only iadt.c changes, immutable field spines reused with empty
caller tail under existing node lifetimes; no ABI/index/owner/default change.
[Root review](../src/prototype/performance_followup/mem2-spine-root-review.json).
Tree400 census -2408960 requests/-77086720 cumulative bytes, not RAM/time.
Existing-scope exclusive grant18:00-18:10 UTC covers only exact36 sequential
configured samples, remaining-derived cap and stopped children before deadline;
raw/expected-step/pin review and release pending. Root runs no heavy work in
the slot; same C/Job capacity stalls and original failures remain separate.

18:22 UTC MEM2 costs: expired worker grant refused with zero samples/children;
Root executes identical36 proposal in a separate Root-only18:15-18:25 slot.
Collector0 terminal18:16:12, all36 raw expected steps/metrics/279 pins exact,
no incomplete/censored rows; release18:19 consumed. Four tree RSS ranges are
disjoint, tree4001001516 ->976512KiB (-2.50%,978.04 ->953.63MiB). All six timing
ranges overlap: tree400 median4.93199 ->4.89900s; LocalSorted median time+2.85%.
No clear extra speed gain; retain slower third repetitions, startup limits and
LocalSorted RSS+24KiB. [Root review](../src/prototype/performance_followup/mem2-spine-cost-root-review.json)
and raw84 freeze88c45801 preserve results/refusal. Performance consumed release
and now prototypes separate Fold spine deletion; no new cost grant. Source9/
accepted/default selection unchanged; full Goal and Main E11/E12 work remain.

18:42 UTC publication/focused review: cost88 task57a94cc on source6b0ed93 and
prototype Main5ad5536 are pushed/remote exact. All87 records, raw36 byte-equal
to Root raw84, recomputed12 summaries and279 pins verify; no new samples.
Worker index-only publication preserves2011 prior tracked live files and Main
protected9. [Publication review](../src/prototype/performance_followup/mem2-cost-publication-root-review.json).
Private Fold source12881c3ad27 differs only computation.c from2e87fd84;
Root verifies worker focused410/0, all67 cuts/cross268 and affected18/0 records.
[Focused review](../src/prototype/performance_followup/mem3-fold-root-review.json)
is source/raw-evidence inspection, not fresh Root execution or READY publication.
Subsequent persistence44 has only the existing strict-partition recipe1; full
verdict/fuel TSV and all52 public/28 history images equal MEM2 parent. Retain
strict reloads, original parent fixture/setup failures; final qualification/census
pending, no actual Fold RAM/time claim or new comparative grant. Fresh18:45 all
seven panes confirm C/Job capacity stalls, Performance progress and four bounded
owners achieved/stopped. GitHub still nine open issues/zero PRs; no new criteria
or disposition. Timer1674731 alive/unchanged, next19:44:22 UTC.

2026-10-03 19:29 UTC, Root implementation checkpoint: corrected E11/E12 and C14 are
prototype Main integrated/pushed at ea39262,684d47c,6c36dcb. Fresh lifetime,
full Core, public-resume and current-producer callback/shared/link/IO controls
are recorded in [Job integration review](../src/prototype/solver_inputs/joint_verification/e11-e12-main-integration-review.json)
and [C14 review](../src/prototype/c_backend/verification/core-epoch14.json).
E12 Core conflict preserves both receipt and Context checks; C14 only Goal
documentation needed reconciliation. Public52/full TSV remain exact, strict3
unwaived. New Fold exact12 READY notice d04f6635 and separate cost-request
16704347 are evidence; runtime/publication review and cost scheduling pending.
No accepted/default promotion, full Goal or issue closure. Earlier pending
statements above are historical and superseded by this checkpoint/table.

2026-10-03 19:38 UTC, Root Fold qualification/publication: exact12 task86a0699 on57a94cc
and prototype Main9d5cda2 are pushed/remote exact. All1257 pins/source12881c3ad27
verify; Root freshly reconstructs patch and passes parent/candidate O2/SAN
Fold67-cut and cleanup controls, four builds/eight executions. Worker index-only
advancement preserves2099 prior tracked live files; protected9 and old freezes
remain exact. [Root publication review](../src/prototype/performance_followup/mem3-fold-publication-root-review.json)
supersedes historical READY/runtime-review pending fields; strict3 remain.
Separate cost-request16704347 config4dab6b88 has36 exact jobs/289 verified pins,
reviewed but ungranted under the implementation-first delegation. Visible-roots
17-pin debug evidence verifies; deliberate200000-node Tree16 cap/driver1 is
retained as incomplete lower bounds, without dead/peak/live-memory inference.
No actual Fold RAM/time, accepted promotion or full Goal/issue completion.

2026-10-03 19:50 UTC, scheduled19:44 Root progress review: all seven live worker panes
and Surface completion state inspected; original C/Job capacity stalls persist,
Performance consumed Fold publication and continues lifetime attribution, four
bounded owners remain achieved/stopped. All worker indices/protected9 and
remote Maina0b8856 exact. Fresh GitHub criteria/body/state unchanged: nine open
issues/zero PRs; no new closure. New allocation-site9 freeze720aab8b verified;
Tree4 prefixes10K/100K count16975/64913 requests, including graph.c internals.
Counts guide ownership review, not actual/dead-memory claims; scratch-index
hypothesis rejected with no patch. E13 Root review and Fold cost scheduling
remain pending independently of worker capacity. [Scheduled review](../src/prototype/coordination/reviews/20261003T194422.json)
pins every owner/issue boundary and original failures. No resume/new hold/grant/
promotion; Root Goal service still reports blocked. Existing timer unchanged,
next2026-10-04 01:44:22 UTC.

2026-10-03 20:05 UTC, Root recovery result supersedes passive-stall labels: C/Job terminal
capacity retries ended at15:03/15:18 UTC and persisted blocked with zero later
sampling. No exclusive/test/profile workload was active at preflight. Exactly
one original-TUI `/goal resume` each19:58:11 restores the same thread/Goal/PID
and gpt-6.1-sol xhigh; fresh tool/code evidence shows C cross-component callback
implementation and Job E14 supplement/E15 qualification. This is operational
recovery within existing scope, not new human approval or permanent capacity
guarantee. [Recovery evidence](../src/prototype/coordination/reviews/owner-recovery-20261003.json).
Completed owners remain stopped, Root service Goal metadata unchanged blocked.

2026-10-03 20:35 UTC, Root E13/current-producer and Fold cost checkpoint: exact28
task7425ccb/Main81f76b3 pushed,876 frozen pins/13 raw reports and fresh parent/
candidate O2/SAN12 controls pass. Current public52/full TSV exact E12, strict3
remain. [E13 Root review](../src/prototype/solver_inputs/joint_verification/e13-main-integration-review.json)
records independent canonical reconstruction and initial disposable baseline
setup failures. Protected9 and1889 worker tracked files preserved.
Root-only Fold36 slot20:20-20:30 launched20:20:06 with591s absolute cap, all289
pins/no heavy children; terminal0/all36/exact fuels20:20:46 and explicit release
20:21:15 consumed. [Cost review](../src/prototype/performance_followup/mem3-fold-cost-root-review.json)
pins immutable raw88. Tree400 modest wall/RSS improvement has disjoint ranges;
five other timing ranges overlap. No universal or accepted-only ranking follows.
[E14/C15/MEM4 handoff review](../src/prototype/coordination/reviews/handoffs-20261003T2030.json)
verifies distinct freezes/inputs/raw outputs and schedules fresh qualification.
C16 changed live dependencies match historical C14 blobs; no original evidence
was rewritten. Three original owners progress after release; completed owners
stay stopped. No new human design approval, accepted promotion or Goal closure.

| Owner / issue scope | Delivered and verified | Not yet delivered / next boundary |
| --- | --- | --- |
| Job/Evidence; SE1-SE5 / AP0, #51/#47 | E1-E4/E6-E13 corrected prototypes integrated. E13 exact28 task7425ccb/Main81f76b3 pushed; Root source128de0980a5,876 frozen pins/13 raw reports, parent/candidate O2/SAN12 controls0; actual current public52/full TSV exact E12, strict3 retained | E14 exact23/manifesteb2b270e reconstructs on task7425;846 raw input pins reviewed, fresh current-E13 qualification/publication pending. E15 focused O2/SAN0 and live E16 are private; original rejected epochs retained, no full ownership/Goal completion |
| C backend; #61 (historical #44/#49) | C1-C14 bounded prototypes integrated and Root current-E12 callback/shared/link/IO/source-import qualified. Original C owner recovered in place and consumes Fold early release; C15 immutable8/manifest17a4bec0,184 dependencies/934 outputs and99 expected O2/SAN rows each reviewed | C15 fresh current-E13 qualification/publication pending. Seven live C16 dependency changes verify exactly at C14 task blobs; no contamination/regression inferred. C16 typed-view4985 is classifier evidence, not binary lowering completion. Caller lifetime/totality, higher/native Acc/QuickSort remain open |
| Performance; #56/#51/#52 | V3/MEM2/MEM3 Fold source/cost delivered. Root Fold36 exact289 pins/raw72/GNU/TSV/expected fuel pass, collector20:20:46 terminal0, workers early released. Durable raw88/manifest a9dd3945; Tree400 wall4.989->4.918s(-1.43%),RSS976244->957552KiB(-1.91%), disjoint ranges | Other five timing ranges overlap; Tree4/LocalSorted RSS overlap and startup List limited. MEM4 exact11/ddba9930/private1125 reviewed, fresh Root qualification/publication pending; no MEM4 actual cost. Full Goal/retained-memory/recovery criteria remain open |
| Surface; #57 | Verified prototype integrated, completed owner stopped | Accepted policy/promotion and selector diagnostics unresolved; no owner restart |
| Static audit; #59/PR #60 | Task7ed3ad1/Main5683755; Root385 source/114 archived dependency hashes and exact inventory verified; bounded Goal achieved | Dynamic/full #59 and controlled cost remain open; no static-owner restart |
| Sort; #41/F3-F5 | PR54 and finite-function task5e6ed84/Main966c4e1 integrated; bounded Goal achieved, focused gates0 | Accepted Sort/F5 and broader interfaces remain separate |
| Test-suite; #59/PR #60 | Goal achieved. Final exact7 notice4432e3d/manifest0f30bef9, raw177/archive and776 dependencies exact. Root recomputed all distinct records:435 to413 calls; fresh original/reduced helpers reproduce128 runtime/23 C tests/two scripts. Worker47 completed recipes0 and ten injection controls verified | Exact7 taskbc16df9 pushed/remote verified, prototype Main36a27be integrated with corrected agent/user provenance. Net applied tests plus25-line helper is -13 lines; repository additions are patch/docs representation. Unchanged derived-LT600s run incomplete; no wall/RSS or accepted-test claim |
| Issue-audit; all open Issues/PRs | Goal achieved. Root independently reread ten exact comments, successor61 body/open,44/49 closed/not_planned as superseded; task8bb5018/Mainb461ace pushed/remote verified | Nine issues remain open, zero PRs. Superseded means residuals routed to61, not implemented. #59 and user design/adoption decisions remain open |

Owning tables: Job/Evidence's [SE Plan](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md#plan),
performance's worker-local `doc/2026-10-03-PERFORMANCE-JOINT-VERIFICATION.md`,
and C's owning Goal (C6 temporary addendum removed in the C7 freeze), plus
audit's worker-local `doc/2026-10-03-VERIFICATION-AUDIT-PLAN.md`.
Current worker reporting updates remain local until their next exact publication;
this consolidated status is durable Main feedback. #41/#43 are separate design
questions, not silently assigned to these epochs. #47's general relevance policy
and #52's equality-reuse design are not completed by storage deletion or C emission.

### Rescheduled Queue

2026-10-03 issue-audit terminal snapshot, independently verified by Root:
nine open Issues (#41, #43, #47, #51, #52, #56, #57, #59, #61), zero open PRs.
#44/#49 are closed as superseded by #61; residual source/native requirements
and original provenance remain. Historical reviews below are complete; full
issue criteria remain separate.
New #59 / documentation PR #60 concern verification debt and progress accounting,
not a newly established compiler defect. PR #58 is already merged. PR #60's
672-line supplied audit is an input, not approval of every threshold or manifest.
The Issue's smaller first-epoch scope takes precedence over an automatic full
reporting subsystem. Its audit cover is pinned to `37de1e8`, not current Main.

| Order / owner | Issue or PR | Next deliverable and dependency |
| --- | --- | --- |
| 0 / desk -> Merge | Workflow | Merge reviews evidence; Performance runs, original C/Job resumed once in place with actual tool/code progress. Test-suite/issue-audit/static/Sort bounded Goals achieved and stopped; protected local bytes preserved. Scheduled six-hour report delivered; Root resumes coordination after audit handoff |
| 1 / Job, Root reviews independently | SE/AP, #51 | E13 task7425/Main81f76b3 integration/current lifetime/full synthesis/public persistence complete. Qualify frozen E14 exact23 against current E13, then publish independently; E15/E16 remain worker-owned private. Preserve strict3 and rejected failures; no full layout claim |
| 1 / performance + Job | #56 / #51 | MEM3 Fold source12 and Root cost88 delivered: all36/fuel/raw exact, explicit early release. Preserve modest Tree400 gain/five overlapping timing ranges and original failures. Qualify frozen MEM4 exact11 separately; no automatic new measurement slot. Original Performance owner continues lifetime work |
| Complete bounded / test-suite; Root publishes | #59 / PR #60 | Verified pilot removes22 duplicate calls with all distinct outcomes preserved; Root raw/assembly review complete. Exact7 taskbc16df9/Main36a27be prototype integrated; broader inventory/cost/accepted adoption remain open |
| Complete bounded / issue-audit | All open Issues/PRs | Verified comments/dispositions,44/49 superseded by61; nine issues open. Owner stopped; Root resumes coordination, no repeat audit |
| Complete bounded / verification-audit | #59 / PR #60; #51 | Static inventory/pilot delivered and worker complete; four-job measured cost remains deferred, full #59 open. Do not restart the completed owner automatically |
| 2 / C backend | #61 (historical #44/#49) | C14 current producer delivered. C15 immutable8 snapshot/raw inputs reviewed; qualify against current E13 using frozen C14 backend, then exact publication. C16 private binary callback work excluded; preserve old unary refusals and native/higher/ownership boundaries |
| Complete review / Merge | PR #60, #53, #55 | Documentation reviewed/imported with provenance; no open PRs. Full issue criteria remain independent of publication |
| Complete bounded / Sort library | PR #54 / #41 | Generic MergeSort and finite-function prototype verified/integrated; bounded worker complete. Accepted Sort/F5 and broader containers remain separate; no owner restart |
| Review / Merge | #57 | Audit delivered prototype against Issue criteria; Surface stays stopped. Prototype delivery is not accepted promotion; do not reopen a worker just to repeat completed gates |
| Deferred design / Merge | #47, #52 | Relevance and checked-equality reuse remain separate designs, after stable owner/resume boundaries; no new authority, optimizer store or trust shortcut |
| Deferred design / user decision | #43; remainder of #41 | General recursion/logical boundary and broader finite-container interfaces require explicit design decisions, not inferred approval from scheduling |

This is a dependency order, not an invented calendar or completion percentage.
Merge maintains this one queue and the delivery table; workers keep their existing
single owning work lists. The desk displays issue, actual deliverable, blocker and
next step in a short report. Exact test inventory and detailed evidence stay with
their owners. Add review items only for material changes; no duplicate task graph.


## 1. Inquiry Desk and Merge Ownership

### Subjective (User)

2026-10-03 12:56 UTC, English paraphrase of the explicit human request relayed
by inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: add an issue-audit worker
to examine completion and continued need for all open Issues/PRs. Comment and
close completed or obsolete cases only with evidence; consolidate residual work
into useful successor Issues and cross-link superseded originals. Examine #59
first; the user's suspicion that earlier work is complete is not evidence that
all criteria are met. This is a separate read-only code lane with no Main or
heavy-test authority. Coordinate one GitHub issue writer with that owner; the
desk is arranging its isolated launch. Test-suite implementation succeeds the
completed static audit and does not restart that owner.

2026-10-03 12:50 UTC, English paraphrase of the explicit human request relayed
by inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: launch a dedicated
test-suite worker to plan from PR #60 / #59 audit evidence and implement test
deduplication and line-count reductions. Reuse the completed verification
audit without restarting that owner. The new worker continues independent
prototype coding; Merge resolves shared-test conflicts and integrates Main.
Preserve unique boundary coverage and known failures. Accepted test/build
promotion remains a separate intentional review. The desk is arranging the
isolated worktree and tmux owner; a launch receipt is still pending.

2026-10-03, English paraphrase of the explicit human workflow change relayed
by inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: implementation lanes may
continue separate prototype epochs without waiting for Merge publication or
review. Merge owns integration conflicts and may implement their resolutions.
Preserve submitted bytes/commits through immutable snapshots; investigate other
stalls and confirm that next instructions were consumed. This supersedes blanket
review holds. Only actual safety/shared-file dependencies or short exclusive
measurement slots justify narrow holds. Existing heavy-run scheduling and
accepted-source promotion boundaries remain; no new representation/erasure
policy follows from this workflow instruction.

2026-10-03, English paraphrase of the human coordination concern relayed by
inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: the user asks whether Merge
is already done and says progress/coordination are unclear. Distinguish completed
integration from remaining review and current worker holds, with concrete owner,
next action, blocker and release status. This feedback grants no new source
representation or erasure policy and does not complete native QuickSort.

2026-10-03, English paraphrase of the later human clarification relayed by the
same inquiry desk: improve speed by deleting unnecessary mechanisms/work rather
than adding tuning complexity. Memory needed for intermediate computation and
resume is acceptable; roughly order-of-magnitude excess versus other systems is
not. Distinguish necessary resumable progress/checked facts from redundant
graphs, copies and dead transient state. Account for new index/ownership overhead
and demonstrate net memory/time effects with meaning, fuel and resume preserved.
Resume history does not excuse excess. This does not approve discarding required
progress, unsafe borrowing, a hard 10x target or overlapping heavy benchmarks.

2026-10-03, English paraphrase of the human requirement relayed by inquiry desk
thread `019ebfae-06be-7b71-974a-b97505daed4a`: AP tree peak memory, reported at
about 1847 MiB after optimization versus about 97-529 MiB for native checkers,
is excessive. Require software/design improvements for both speed and memory,
and show that active plans cover both. Performance and Job/Evidence must pin
allocation and retained-owner causes, remove redundant graphs/copies, and use
matched wall/RSS measurements plus semantic, fuel and resume gates. File/record
size reductions do not establish RAM savings; unsafe borrowing and wrappers
added only for profiling are unacceptable. Continue qualified epochs; this
status inquiry does not request heavy reruns.

2026-10-03, English translation of the latest human clarification relayed by
inquiry desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: scheduled waking and
progress inspection belong to the Merge owner. If workers send no completion
notice, Merge must still wake after roughly six to twelve hours. Use a six-hour
interval independent of worker notices, inspect all workers and stalled work,
and report material status to the desk. Reuse the existing loaded-owner relay;
do not launch a duplicate Merge owner or claim host-restart recovery from an
in-process timer. This clarifies the earlier Core timed-wait requirement for
the current Merge role.

2026-10-03, English translation of the new human task relayed by the inquiry
desk: if #41 Sort foundation work can proceed in parallel, launch a worker to
develop it, chiefly `.p` libraries/proofs. Use a named isolated worker/task branch,
the requested GPT-6.1-Sol xhigh and explicit continuing Goal with the existing
tmux/outbox-to-Merge workflow. Merge remains the sole Main owner; this does not
approve accepted Sort source/test/build promotion. Reuse already verified
Fin/List/Vec/common backend work and PR #54, coordinate modest test slots and
report genuine compiler blockers to their existing owners.

2026-10-03, English translation of new explicit human approval relayed by the
inquiry desk (`019ebfae-06be-7b71-974a-b97505daed4a`): verified speed improvements
may now be promoted from Prototype into accepted `src/`. This supersedes the
earlier no-promotion boundary for that selected scope. Merge remains the sole
Main owner. Keep promotion a separate epoch, compare current accepted code,
preserve unrelated dirty files/fixtures, select verified changes and necessary
dependencies, verify accepted regressions/parity, then commit/push. Other
Surface/C/design changes and unverified trials are not blanket-approved; strict
resume failures remain unwaived. The separate
[promotion plan](2026-10-03-VERIFIED-PERFORMANCE-PROMOTION-PLAN.md) owns that work.

2026-10-03, English translation of the latest human concern, relayed by the same
desk: explain why issues have not decreased despite continued implementation.
Give an issue-linked account of unmet criteria, prototype versus accepted
promotion, deferred design, newly added issues and closure-review backlog.
Identify genuinely complete issues awaiting administrative closure. This asks
for an explanation, not new implementation scope or closure to improve counts.

2026-10-03 07:44 UTC, English paraphrase of the explicit inquiry-desk delegation
`ACTIVATE MERGE OWNER`: initialization and both notification hops are verified.
The desk relinquishes Git mutation, exact task publication, review/integration
and closure duties to this Merge session. Start the transferred supervision Goal,
continue the single central queue, update received E4/C7 status and actual routes
in place, and publish those documentation updates at a sensible boundary.
Preserve dirty accepted files and producer-pinned strict failures; no accepted
promotion or deferred design expansion. Short reports go to the desk on material
changes and each six-hour active checkpoint. This completes the ownership handoff
required by the preceding replacement instruction, rather than superseding its
prototype or completion criteria.

2026-10-03, English paraphrase of the latest explicit replacement: this visible
conversation becomes the user's inquiry/reporting desk, not the merge owner.
Use a tree: user-facing desk <- separate Merge/audit session <- three or four
implementation sessions. The Merge session reports progress regularly to this
desk, which displays concise updates and answers the user's questions. Review
new issues/PRs, including the latest parallel-development proposal, and
reschedule work within that structure. This supersedes the earlier instruction
that this visible conversation itself performs Main integration. Existing
prototype boundaries and review requirements remain unchanged; the handoff
must be verified before the new Merge owner starts integration.

2026-10-03, English paraphrase of the latest request: implementation appears to
be running in the workers, but issues are accumulating and progress is hard to
see. Have each worker write feedback showing which issues have advanced and by
how much. Core should make their actual progress visible, not just report that
sessions are active.

2026-10-03, English paraphrase of the latest clarification: after the AIze audit
is complete, push it and finish that assignment. Then prioritize A Program
development. This authorizes publication of the audit, not AIze implementation
changes or inclusion of its unrelated local edits.

2026-10-03, English paraphrase of the latest request: add one separate audit
session for `../aize`. Multi-goal development belongs in that project; inspect
its repository and record useful lessons from this Goal-based workflow under
`../aize/doc`. Its stability problems need investigation, not assumed causes.
This is independent of A Program implementation and does not authorize changing
either system's implementation. Core continues supervising the existing lanes.

2026-10-03, English paraphrase of the latest follow-up: wake/restart/notification
arrangements must not replace supervision of actual implementation progress.
Continue monitoring the workers' code changes and verification, not just the
coordination setup.

2026-10-03, English paraphrase of the explicit clarification to the Core-pause
question: use both notification waiting and a timed Wait state. Even without a
notification, Core should wake every six hours and inspect progress. This
supersedes notification-only waiting and does not ask to stop worker Goals.

2026-10-03, English paraphrase of the latest request: Core itself should be
woken and activated by the worker sessions. Prefer notification-driven
coordination over an always-active polling session. The available wake/resume
mechanism still needs verification; do not assume tmux notifications alone can
resume this conversation.

2026-10-03, English paraphrase of the latest clarification: performance and
Job/Evidence reduction or deletion are tightly related; verify them together
with concentrated attention. C backend follows A Program through `.a` and
LinkerScript, prioritizing C code usable from other C modules rather than
unbounded investigation. It remains important as a bridge to external systems
and eventual generic assembler lowering, not an authority over A Program.

2026-10-03, English paraphrase of the latest explicit instruction: if Surface
is finished, shut down that session. Preserve its delivered work; this does not
instruct deleting its branch/worktree or closing unfinished parts of #57.

2026-10-03, English paraphrase of the current Core Goal: supervise the four
implementation scopes and their Goals, resolve conflicts, review/merge verified
results and finish the whole assigned work. A delivered epoch does not shrink
the remaining scope or establish completion of another lane.

2026-10-03, English paraphrase of the latest explicit decision: this session
becomes coordination Core, specializing in merges and audits. Move actual
Job/Evidence simplification to another Codex session because its implementation
has progressed too slowly here. This supersedes the earlier requirement that
Core itself implement SE1-SE5; existing design and verification requirements
remain in force.

2026-10-03, English paraphrase of the latest authorization: each worker may
commit and push its own work. Only this Core session merges worker results;
continue periodic supervision. This does not authorize worker promotion or
direct pushes to Main. This note applies to all three linked worker Goal briefs.

2026-10-03, English paraphrase of the latest follow-ups: name sessions by
their task rather than Main/Sub numbers. The user now reports tmux installed
and asks to verify it, so both the coordinator and the user can inspect worker
progress. Checking installation does not itself establish a running worker.

2026-10-03, English paraphrase of the active Goal: check issues/PRs roughly every
ten minutes until all three workers have clear scope and are running. Start
C-backend/Linker work now; use incoming issues/PRs to scope performance and
surface. Continue actual Job/Evidence deletion between checks, keep working
directories separate and integrate worker results at suitable verified epochs.

2026-10-03, English paraphrase: keep this session responsible for Job/Evidence;
use tmux-managed additional Codex sessions for backend and performance work.
2026-10-03, English paraphrase of the follow-up: this primary session must
regularly inspect the workers and issue directions as needed. Each tmux worker
should use `/goal` for sustained implementation against a long-term plan, using
the requested `6.1 Sol` model at `xhigh`. The performance audit documents and
issues are currently being submitted; incorporate them when available, not
as already inspected material. Do not silently substitute another model.
2026-10-03, English paraphrase of the latest addition: retain Sub2 for
performance and add Sub3 for surface-language changes and associated `.p`
updates, particularly changes to Binder notation for functions and function
graphs. This adds a fourth total session; it does not replace the C worker.
The exact new Binder syntax is not stated in this message.
2026-10-03, English paraphrase of the latest clarification: the user will
install tmux and questions whether it is the best management choice. Leave
further installation to the user; compare management options before assuming
tmux is required. Existing user-local extraction is not a system installation.
Earlier requirements remain: do not duplicate Term/Oracle structures above the
typed owners; preserve Core/type separation and ordinary Solve semantics.

### Objective (Code)

2026-10-03 Root terminal E9 review at Main `6a48aae`: all1989 frozen records,
runtime128 and exact two-literal policy adaptation verified. Targeted81 recipes
fail only the strict partition target; focused23+8, sanitizer45+5/build38 and
transport/semantic/history/seven checkpoint controls pass. All590 module cuts
per build and forged-prefix/source-claim controls are retained. Fresh accepted-
promotion-compatible canonical assembly reproduces all128 qualified bytes.
[Review](../src/prototype/solver_inputs/joint_verification/e9-relevant-summary.json)
keeps historical full acceptance and ordinary1921/performance1915 distinct.
E9 prototype integrates; E9+E10 remains pending, E11 rejected/correction focused,
MEM1 remains unimplemented/unmeasured after current qualification.

2026-10-03 Root task review: E9 `bd1ddf3` exact24 and E10 `6715af2` exact19
are independently pushed/remote verified, each preserving E8 `50cd56b` parent.
E10 has a distinct isolated task branch; E9 Git metadata/index advancement leaves
later live canonical bytes unchanged. [Review](../src/prototype/solver_inputs/joint_verification/e9-e10-publication-review.json)
pins source-guard and receipt-storage decisions; combined qualification remains
performance-owned. C9 task `60c100c` exact38 is pushed/remote verified; Root all13
current-E8 gates pass at one build job, and prototype Main `48c42eb` integrates
the reconciled candidate and [fresh evidence](../src/prototype/c_backend/verification/core-epoch9.json).
Its frozen older C8-pending status is superseded by Root C8 gates/pushed Main.
C holds safely; Job E11's frozen isolated E9 review found a retained-address
lifetime regression (fresh parent0/candidate1); publication is held for correction.
E12 stays private, unqualified. State-image task `ac6a8b2` exact2 is published/integrated
as `6867359`: all65 pins/36 outcomes freshly checked, three completed image
pairs equal,12 inert resaves pending3 with exact bytes. Counts are not RAM/time.

2026-10-03 fresh Merge publication: Main `e14856f` push succeeded and remote
matches, including accepted performance-only `7631e5a`, E7/E8/C8 and deployed
heartbeat source/evidence. The selected promotion meets its separate criteria;
all original protected local edits survive with exact reverse hashes. Sort task
`5e6ed84` exact11 is independently E6-qualified and prototype Main integrated;
its bounded Goal is complete. #41/F5 and full #56 remain open on their own
remaining criteria; no silent design/promotion or strict-resume waiver.

2026-10-03 09:34 UTC, fresh Merge inspection at Main `99efcf7`: C7's eleven
current-E6 gates and generic PR #54's three focused gates passed; evidence is
committed in `66fa707` and PR #54 is confirmed merged on GitHub. E6 measurement
task `44e479b` and Main merge `99efcf7` preserve all 114 frozen files exactly;
both pushes succeeded. Sort owner `%11`, branch `parallel/sort-library-20261003`,
worktree `/home/repyt/workspace/a-program-workers/sort-library`, thread
`01a10111-a71f-7cd0-8b00-22b9f6f3aa1a`, has an active implementation Goal;
its actual turn_context verifies the requested model/effort. Its first selected
gap is a finite-function API with inverse-map recovery of labelled occurrences,
using already verified Vec and permutation laws. Worker-local progress is not
yet a qualified publication. The bounded verification-audit Goal is complete;
its four-job cost remains deferred and unmeasured.

The ownership prototype remains under `src/prototype/solver_inputs/`. Core's
Identity boundary Query/lifecycle epoch is committed as `a51f9c9`: full O2
regression/examples/acceptance, focused and sanitizer gates passed. Its
[verification report](../src/prototype/solver_inputs/identity_boundary_verification.tsv)
records 451 boundary partitions, 55 owner-cancellation cuts and applied code
delta -33 lines, separately from tests. Three public split-fuel resume failures
remain unchanged and unwaived; this does not complete SE1-SE5.
The family-parameter scratch-pool epoch is verified against the current Main
checkpoint plus its canonical prototype patches. Full O2 acceptance, focused,
sanitizer, checkpoint, current Surface/C combination and explicitly adapted
user-addition Core/IADT/Synthesis checks pass. Its measurements and unchanged
public resume failures are in the
[report](../src/prototype/solver_inputs/family_cursor_verification.tsv).
Accepted `src/evidence.*`, `src/iadt.*` and their tests retain unrelated edits.

### Assessment

2026-10-03 20:05 UTC, Root recovery result supersedes passive-stall labels: C/Job terminal
capacity retries ended at15:03/15:18 UTC and persisted blocked with zero later
sampling. No exclusive/test/profile workload was active at preflight. Exactly
one original-TUI `/goal resume` each19:58:11 restores the same thread/Goal/PID
and gpt-6.1-sol xhigh; fresh tool/code evidence shows C cross-component callback
implementation and Job E14 supplement/E15 qualification. This is operational
recovery within existing scope, not new human approval or permanent capacity
guarantee. [Recovery evidence](../src/prototype/coordination/reviews/owner-recovery-20261003.json).
Completed owners remain stopped, Root service Goal metadata unchanged blocked.

2026-10-03 19:55 UTC, desk operational recovery delegation (not new human authorization):
inspect whether the original C/Job tmux owners retry capacity automatically or
need an explicit same-session/same-model retry. Assess one bounded retry only
when no exclusive workload is active; do not load/fork duplicate owners, change
models or restart completed workers. Keep E13 review and Fold scheduling on the
existing queue. Earlier no-resume status reports remain historical.

2026-10-03 19:50 UTC, Root scheduled review: retain the current single queue. C/Job
capacity stalls do not block Root review of frozen E13. Performance continues
owned argument/environment lifetime research; nine-pin debug attribution is
scoped observation, not a new safe-recycling design. Fold matched cost remains
ungranted under the existing implementation-first delegation. Completed owners
stay stopped and issue criteria remain open; no new human approval is recorded.

2026-10-03 19:29 UTC, Root agent implementation decision: finish delivered corrected
E11/E12/C14 independently of the original C/Job capacity stalls. Exact private
merge/source reconstruction and fresh lifetime/public-resume controls support
the integrations; the single current delivery table records actual boundaries.
Worker notices confer no new design, promotion or cost authority.

2026-10-03 12:03 UTC, Root scheduling decision under the explicit human workflow
replacement: earlier review/publication-only implementation holds in this plan
and lane briefs are superseded. Preserve immutable submitted epochs, then continue
separate live implementation and relevant serial correctness gates. C advances
its minimal I/O-status fix and bounded existing-value-record/single-tail List
work; Performance finishes E9+E10 then MEM1 attribution/deletion; Job finishes
corrected E11 qualification and safe Context-ancestry repair. Root owns conflict
resolution. Original unsafe E11/E12 candidates remain unqualified, while repairs
may continue. Exclusive matched wall/RSS and actual shared-owner conflicts remain
narrow dependencies. Completed Sort/audit workers stay complete. Confirm each
owner's consumed instruction and actual next action, rather than only transport
queueing. No new source erasure policy or accepted promotion is inferred.

2026-10-03 11:12 UTC, Root agent review: reject frozen E11 for publication and
Main integration. All21 files,19 local reports and both156-source trees match
their pins. A fresh focused [control](../src/prototype/solver_inputs/joint_verification/e11_scope_lifetime_control.c)
uses a canonical SourceScope head with a caller-owned ancestor: E9 preserves its
retained Binder address (exit0), E11 changes it after scratch mutation (exit1).
[Review evidence](../src/prototype/solver_inputs/joint_verification/e11-lifetime-review.json)
retains commands/results and the initial wrong-syntax setup failure separately.
Borrow only fully retained canonical ancestry through the existing index;
otherwise keep the old owned array. This is an agent correction within scope,
not new user design approval or a scope-factory policy change. Job may run only
the corrected isolated E11 focused/lifetime/sanitizer controls at one build job;
broad/C/timing hold remains, E12 unqualified. Existing performance E9 then
E9+E10 qualification continues on immutable tasks.

2026-10-03 issue-criteria review (agent assessment): ten open issues reflect
partial prototype deliveries and distinct remaining criteria, not a universal
all-SE/strict-resume gate. #41 still needs F5 accepted adoption; #44 remaining
export/admissibility/dependent/higher-Identity contracts; #49 native Acc/QuickSort;
#56 selected accepted promotion complete, with its separate accepted-current-head
measurements, profiling/arena/index and owner work remaining; #57 accepted language-policy adoption; #51 owner/lifecycle/topology
reproduction gates; #59 measured cost/dynamic coverage. #43/#47/#52 retain separate
recursion/relevance/equality decisions. #56 explicitly permits separately
characterized pre-existing reload failures. No completed issue was found merely
awaiting administrative closure. Four docs/library PRs are merged; last issue
closure #46 on Sep29 and new #59 on Oct3 also explain the stable count.

2026-10-03, Merge operational decision within the clarified wake requirement:
the existing relay `%9` now watches all five retained outboxes and has an
independent 21,600-second heartbeat. Actual process PID `1674731` reports first
due `2026-10-03T13:44:22.084Z`; later notices do not reset that deadline.
Accelerated timer controls pass for active steering, idle turn start, refusal
to resume an unloaded owner and serial/coalesced delivery. Existing six relay
controls also pass. The running revised relay acknowledged actual active
delivery through the unchanged loaded-owner guard. This is an in-process tmux
watcher: a stopped watcher, closed owner or host restart has no verified recovery.
The desk relay receives material reports and has no periodic timer of its own.

2026-10-03 07:44 UTC, fresh Merge activation at Main `4d1d941` plus this local
documentation edit: own rollout verifies `gpt-6.1-sol`/`xhigh`; `create_goal`
returned active for the transferred full supervision objective. The index was
empty before this edit. Worker->Merge route probe and E7/C7/audit/performance
acknowledgments were delivered to loaded thread
`01a100b3-1d83-7090-bbf6-62544c39ec4b`; the desk explicitly confirmed actual
Merge->desk receipt of initialized report version `dfab39f9`. Later reconciliation
updated the same bounded report to `47cdab70`; this is evidence, not extra approval.
Desk remains `019ebfae-06be-7b71-974a-b97505daed4a`. The active relays are tmux
`a-program:merge-notifications` (%9) and `a-program:desk-notifications` (%8);
audit is `a-program:verification-audit` (%10), worktree
`/home/repyt/workspace/a-program-workers/verification-audit`, branch
`parallel/verification-audit-20261003`. Its own notice reports an active #59 Goal,
and the desk independently observed xhigh/Pursuing goal/inventory inspection.
Merge alone now owns integration and delegated exact task publication. Accepted
dirty files, untracked user fixtures and private trials remain excluded.

2026-10-03 Merge review at `4d1d941` plus this local plan: all 1,945 E4 frozen
evidence records and 128 runtime hashes freshly verify; summary digest `9af48798`
matches the notice. Reviewed registration borrowing keeps canonical job inputs
as syntax/count authority, with owner/role checks before scope conversion; local
indexed/activated/next state remains explicit. No E4 runtime defect is established.
C7's 14 exact freeze hashes also verify; reviewed tag validation precedes arena
mutation and allocation, with rollback/invalid-input controls in durable clients.
Separate current-producer qualification is still required before C7 integration.
E8 ready notice `f58a7236` now records a frozen increment against exact E7;
live canonical patches have advanced to E8. Publish E7 from its frozen transport,
then E8; never stage live canonical files as E7 or include private E9. Audit slot
request `2270bcbc` is deferred while E6 and owner correctness gates run: finish
static inventory/pilot first, then agree the producer/binary/scope with performance
for one exclusive cost slot. This defers measurement, not the bounded audit Goal.

2026-10-03 07:55 UTC Merge checkpoint: reviewed E4 terminal recipe-result files,
freshly matched a clean Main+E4 assembly to all 128 qualified runtime hashes and
six selected test hashes with the same 17-line namespace test-only augmentation.
Prototype merge `3a8c024` is on Main; nine protected tracked/untracked file hashes
are unchanged. Initial staged-input guard refusal and omitted Surface assembly
are recorded as setup failures, then corrected; no duplicate broad pass is claimed.
[E4 review](../src/prototype/solver_inputs/joint_verification/e4-merge-review.json)
pins worker terminal gates and fresh source alignment separately. C7's exact
14 staged blobs match its freeze, task commit `d275b75` is pushed and independently
remote verified; worker freeze is released for a distinct next native boundary.
Current-producer C7 qualification remains pending and is done from immutable Git.

2026-10-03 Merge E6 review: all 1,939 frozen records freshly verify. Independent
E4+applied-E6 and full clean Main+canonical-E6 assemblies each match runtime128
and six selected tests exactly. Reviewed Binder borrowing retains explicit Binder
identity/owner checks and the permanent namespace control once. Prototype Main
merge is `01c29c0`; [E6 review](../src/prototype/solver_inputs/joint_verification/e6-merge-review.json)
keeps its 408 relevant O2 recipes/one unchanged strict target failure distinct
from E4's full 384-recipe pass, with worker sanitizer45/focused23/readback140
passes. No new broad rerun, waiver or speed claim. Exact cached transports then
published E7 `3ad0303` (18 files) and E8 `50cd56b` (14 files), preserving raw
CRLF bytes and excluding private E9/live canonical trials. Their Main integration
requires separate joint qualification. Performance's <=40-minute measurement
proposal `570d7af6` is under review; C acknowledged a safe terminal hold, Job's
current gate must finish before grant. All CPU-heavy workers are explicitly
released after the agreed slot; audit gate cost is coordinated within it.

2026-10-03 08:12 UTC exclusive scheduling decision: Job's stopped notice
`5289e8a3` confirms E9's broad command terminal exit 2, unpublished/unqualified;
C's current hold `de394122` confirms no heavy children. Related process inspection
also finds no heavy build/check process. Matched E6 baseline/runtime128 and its
20-record qualification evidence freshly verify; all 285 planned source/tool/
binary pins match. Grant slot `e6-shared-cost-20261003-0812` through 08:52:05 UTC:
performance alone runs its 30 pinned sequential samples, three repetitions and
<=180 seconds each, capped to the remaining absolute window. No extra workload
or broad gate. Preserve warm-filesystem/process-startup/RSS limits and censored
failures. Audit may receive a separate sequential phase only after performance
reports all measurement children stopped and its complete gate key is reviewed;
otherwise cost stays deferred. Merge/C/Job/audit start no other heavy work during
this window. Explicit release follows terminal measurement/expiry. Static review
and docs continue. No comparative result exists at this grant.

2026-10-03 08:26 UTC Merge operational review: performance terminal notice
`abc0e73f` records missing `/usr/bin/time` before sample 0, zero benchmark
children and zero measurements. Preserve that failed attempt and derived config;
do not restart under the obsolete grant. Root privately extracted Debian trixie
GNU time 1.9-0.2 from the configured package source, without installing it.
Package SHA-256 `10257c6f`, executable `efc0d111`; a trivial timeout/true smoke
passes. Prefer the original collector with only this private executable path
changed over proposed wait4 instrumentation: the latter's trivial-command RSS
was 11020 KiB versus GNU time's 1828 KiB, consistent with a launcher inheritance
floor. This is a tool choice, not a benchmark conclusion. Review refreshed pins
before a revised performance-only grant under the unchanged 08:52:05 deadline.
Audit stays ungranted; no heavy correctness work overlaps.

Refreshed audit handoff `576d52e1` corrects coordinator provenance and freezes
four static files. Merge freshly reproduces inventory SHA-256 `62350b05`
byte for byte and verifies all 385 pinned files, including 114 legacy inputs.
Its 385/425/14/21 recipe rows and seven duplicate-command candidates remain
static observations, not independent contracts or gate passes. Request the
provisional cost key as an exact fifth supporting file because the report links
it; preserve the four frozen bytes and old handoffs. Full dynamic/cost coverage
stays open. Job E9 notice `39c47eb6` traces five io_error outcomes to omitted
private fixture links and restores them, without a rerun; E9 remains unqualified.

2026-10-03 08:33 UTC: refreshed performance notice `3e12bc59` supplies private
runner `9c50c62d`, the original collector with exactly one executable-path
change. All 292 pins freshly match; all 30 jobs and 285 original pins remain
unchanged. Revised grant `e6-shared-cost-gnu-time-20261003-0833` permits only those
sequential samples, with maximum_seconds derived again at launch from the same
08:52:05 absolute deadline and two-second cleanup headroom. Preserve both configs,
derivation and original failure. Audit receives no automatic grant; require
performance terminal/stopped children and at least eight minutes remaining.

Audit five-file freeze `fcf2aab0` adds only linked key `a99a7753`, preserving the
four prior bytes. Exact indexed blobs verified; task `7ed3ad1` pushed and remote
verified, separate Main merge `5683755`. Static generator reproduction and source
pins complete its bounded first delivery; full #59 remains open. Worker notice
`76f880b0` reports its Goal blocked on the ungranted cost phase, not an active
measurement or overall Merge impasse. C notice `46691c7f` prepares an applied-List
selector using existing typed views, unbuilt/unqualified; no producer extension
or native Acc/QuickSort completion established.

Documentation PRs #53/#55/#60 are separately integrated as `cbf87e5`, `306a2bf`
and `f89339b`: only four Markdown files, all exact reviewed Git blobs. The #55
and #60 preserved bodies match their recorded original SHA-256 hashes; all nine
relative document links resolve. Historical verification/proposals remain dated,
not today's fresh gates or approved equality/distribution/retirement policies.
Publication closes these documentation deliveries only, not #51/#52/#59.

Merge's #57 criteria review at `5683755`: prototype payload APGSYN2, LHS local
scope, canonical hidden fields/IH, both-valid selectors, distinct renames,
ordered subsequences/unordered permutations, explicit eight-file/13-clause
migration, old-image rejection and combined gate evidence are present. Surface
follow-up `276f4c3` is already task-published and integrated via `7a9a672`;
the owning Goal's old pending-publication status is superseded. The brace/order
and whole-v1-image rejection choices remain agent prototype decisions; accepted
implementation and README grammar are not promoted. Selector diagnostics are
explicitly deferred. Keep #57 open; do not restart completed Surface work.

2026-10-03 08:45 UTC Merge slot close/release: performance terminal notice
`ac01d166` records exit 0 at 08:36:55 UTC, all 30 fresh sequential samples passing,
zero timeouts and stopped children. Merge verifies every output/metrics hash and
all 292 pins; measurements.json SHA-256 `887a96d1`, actual launch config
`390adf41` changes only max_seconds to 1018. Matched AP trees400 medians are
17.235 -> 5.407 seconds and 2971260 -> 1891724 KiB RSS over three runs per variant.
QuickSort ranges overlap; tiny List includes startup. Cross-system measurements
cover their concrete full-source checks with differing helper/proof encodings;
no universal checker ranking or #52 relation-reuse completion. Compact worker
analysis/publication remains pending; preserve the initial zero-sample failure.

Audit config was only just prepared at the 08:44:05 latest eight-minute start;
full exact review did not finish in time. Defer audit cost honestly, without a
partial grant or deadline extension. No audit gate/timing ran. End the shared
window and explicitly release Job/C/performance correctness; audit starts no
heavy work. Root qualifies immutable C7 candidate `091669f` on E6 with eleven
gates and one build job; performance qualifies immutable E7 then E8 distinctly,
excluding private E9/E10. Job checks restored E9 fixture setup and genuinely
omitted broad work before claiming qualification. Modest correctness parallelism
is allowed; further wall/RSS requires a new exclusive slot.

2026-10-03 Merge C7/library review at local Main `b33279e`: immutable C7 candidate
`091669f` passes all eleven O2 targets on freshly verified canonical E6/runtime128.
All fourteen frozen files match. Enum arrays have 875 boundary cases per product
and 24 source observations; existing structural/native/Linker/record/refusal gates
pass. [Root C7 evidence](../src/prototype/c_backend/verification/core-epoch7.json)
keeps fresh O2 separate from worker sanitizer reports and native-sort limits.
PR #54 merge `b33279e` follows three fresh E6 library gates, all ten shared
provider inputs/both binaries exact and source candidates unchanged. Nat legacy
equivalence, Bool/nominal payloads, Local/permutation/Vec/Fin and six rejection
controls, independent synthesis, ordinary images and five backend classifiers
pass. [Library evidence](../src/prototype/finite_sorting/verification/generic-merge-e6.json)
does not claim a full aggregate/sanitizer run, conventional two-front merge,
exact public resume, accepted parity or #41 closure.

Audit completion notice `bcec59db` closes only the bounded first static Goal;
full #59/dynamic/retained coverage and cost remain open. Private cost proposal
`ba86a911`, config `1c6a4ccd`, freshly verifies all 269 pins and four unchanged
keyed leaf commands, with exact environment/cwd wrappers and existing collector.
It was never launched and its expired conditions cannot be reused later.
Performance E7 notice `ab7b541e` records a delta-patch context conflict with
already-applied Surface code. Disjoint changed-line composition is private setup,
not an owner/guard change; preserve the failed attempt and verify canonical
assembly independently before integration. E8 remains subsequent, E9 private.

2026-10-03 agent operational decision within the user's explicit tree request:
the desk records/relays user requirements and answers questions; it does not
review every patch, stage worker changes or merge/push Main after activation.
One new Merge session inherits those duties and the unfinished integration queue.
It reports material deliveries/blockers immediately and a compact status at most
six hours apart while active, using one current report rather than endless logs.
Workers notify Merge, not the desk. Merge filters and reports to the desk; a
worker notice is never user approval. User design questions travel back through
Merge to the relevant owner without creating two implementation owners.

The old visible Goal is currently paused following interruption (fresh
`get_goal` inspection); it has not been achieved. Do not mark it complete or
silently resume it as an integration Goal here. Start the transferred integration
Goal in the new session under the user's existing long-running supervision scope.
This desk has no implementation Goal. Reuse the small guarded notification
helper for both hops; `--new-only` suppresses historical replay at route handoff.
Merge explicitly reconciles existing pending outboxes on startup and each timed
review. A loaded-thread route is checked, not assumed; host/process recovery
remains unverified. Official OpenAI documentation distinguishes thread/turn
delivery from Goal lifecycle ([App Server](https://developers.openai.com/codex/app-server),
[Goals](https://developers.openai.com/cookbook/examples/codex/using_goals_in_codex)).

Core reporting decision within the existing supervision scope: each active
worker maintains one concise issue-status table in its owning plan and sends a
current report now, at material milestones/blockers, and at least every six hours
of active work. Distinguish implemented, locally verified, jointly verified,
task-published and Main-integrated; do not invent completion percentages.
Each row names the issue/subproblem, delivered revision, remaining acceptance
criteria, blocker and next concrete epoch. Routine progress stays in the owning
plan; notify Core of its location and material changes rather than copying logs.
Core consolidates those reports here, separately from issue closure. A delivered
prototype epoch does not close an issue or imply accepted-source promotion.

User-selected replacement assignment: Core coordinates and reviews; the single
Job/Evidence implementation owner moves to `job-evidence`. Core must not edit
the same implementation concurrently. Separate worktrees prevent accidental
edits, but do not eliminate semantic conflicts. Core performs reviewed Main
integration and cross-lane verification; workers publish only their task branches.
The latest user preference supersedes the ten-minute polling proposal:
workers notify Core at a ready epoch, blocking decision, cross-owner conflict
or material regression. Core reviews the reported files/tests, integrates only
verified work and returns directions before waiting again. Ordinary tool output
must not wake Core repeatedly. A notification is worker evidence, not a new user
design approval or permission to merge Main.

Wake transport is verified during active Wait, not after process termination.
Local CLI `0.159.2` uses a managed Unix WebSocket control endpoint. Official
[App Server documentation](https://developers.openai.com/codex/app-server)
distinguishes starting a turn from steering an active turn. Initial raw transport
probes timed out; the correct WebSocket route acknowledged the existing Core.
A worker's direct socket attempt returned EACCES; no permissions were widened.
Core now runs `src/prototype/coordination/watch_core.cjs` in tmux window
`core-notifications`. It relays only file pointers/hashes from the three named
worker outboxes. Newline-terminated regular text files up to 8 KiB are accepted;
partial, oversized, symlink and duplicate events are ignored. The helper refuses
an unloaded Core rather than loading, resuming or forking it. Mock tests pass for
absent/active/idle/stale routing and guarded relay delivery. An actual Job E2
outbox notification ended a 180-second Core Wait after 43.1563 seconds; its hash
matched on inspection. Actual terminated-process recovery remains unverified.
Workers are still controlled through their actual tmux owners, not Core's
`notLoaded` task view. Following the
user's clarification, keep Core's Goal active and use the available interruptible
`clock.sleep` for up to six hours after completing current coordination work.
New input can end this wait early; otherwise the timer returns Core to a status
and handoff review. This is a wait in the existing conversation, not process
termination, Goal completion or a duplicate Core launch. Worker Goals continue.
Automatic recovery after closing the conversation/host restart is not verified;
do not claim an independently installed periodic service.

Core implementation decision following the latest priority: verify current
producer, Job/Evidence deletion, captured-head/readback and their combination
on matching inputs. Preserve results, scope, acceptance and saved frontiers;
report allocation/traversal/fuel separately from exclusive wall/RSS measurements.
Backend work remains downstream C-module realization, not a reason to expand
the producer's semantic schema or require general theory coverage first.

Independent human-requested audit: `a-program:aize-audit` used GPT-6.1-Sol xhigh
with its own audit Goal, now achieved. Its sole document is
`../aize/doc/2026-10-03-GOAL-BASED-MULTI-SESSION-WORKFLOW-AUDIT.md` in that
repository (baseline `5d1072c` plus preserved local edits). Core verified exact
handoff/evidence hashes, inspected three substantive findings, independently reran
nine current/seven HEAD probes and checked all 68 non-audit files unchanged.
Worker unit suites pass 77 current/69 HEAD tests. Per the human clarification,
Core published only the audit as AIze Main `1ed8b79834cc11778221d15810901ee917884ae6`
(push 0; remote verified), then closed the completed audit window and its separate
relay. No implementation or unrelated edits were included. The original three
implementation workers/relay continue; A Program is again the priority. AIze's
findings and proposed repairs stay in its audit, not this implementation work list.

### Plan

- [x] Record the new desk <- Merge <- workers requirement before investigation.
- [x] Inspect current open Issues/PRs and reschedule without closing unfinished work.
- [x] Launch and verify the separate Merge owner and its integration Goal.
- [x] Change worker notifications to Merge and Merge reports to the desk; test
  both hops without duplicate/resumed owners or promotion.
- [x] Launch one bounded read-mostly verification-audit lane for #59; preserve
  accepted tests and arrange measurement only through the common exclusive slot.
- [x] Confirm Merge has acknowledged all pending epochs and unrelated dirty files;
  release the desk from integration, publication and broad verification work.
- [x] Launch the explicitly requested isolated Sort library worker, verify its
  actual model/effort, active implementation Goal and first independent gap.
- [x] Install/test an independent six-hour heartbeat on the existing Merge relay;
  verify actual PID/next due and active delivery without another Merge owner.
- [x] Reconcile obsolete lane review holds; consumed/action receipts C `96e813b9`,
  Performance `6e08d065` and Job `2ea1fb41` read/hash-verified. C11 I/O reproduction,
  MEM1 attribution and corrected-parent Context repair proceed; only safety/shared
  owners or exclusive timing remain dependencies. Material desk report updated.
- [ ] Merge keeps issue-linked reporting current and reports material changes
  and each six-hour active checkpoint to the desk.

- [x] Transfer the single SE1-SE5 work list and committed prototype recipe to
  `job-evidence`; do not duplicate its implementation checklist here.
- [x] Publish the committed compiler/overlay recipe used by all workers;
  do not copy an unfinished trial or unrelated local changes into their baseline.
- [x] Assign separate worktrees, branches, overlay/build/output paths and task briefs.
- [x] Verify the requested model and `/goal` support, then launch the three tmux
  workers.
- [x] Launch `job-evidence` in its own worktree/window using the requested model,
  verify an active Goal and actual code inspection, and notify all lanes.
- [x] Verify concrete deletion/test activity at the next supervision checkpoint;
  launch and inspection alone are not implementation completion.
- [x] Record notification-driven coordination in Subjective and send the
  requirement to all three live workers without pausing their implementation.
- [x] Verify worker-to-current-Core delivery during actual Wait without a new
  Core; active routing and the relay passed. Idle routing is mock-tested only.
- [x] Clarify the fallback: user requests notification OR six-hour timer, not
  notification-only waiting. Keep worker Goals active and do not mark Core done.
- [x] Enter an interruptible six-hour Wait after current handoffs; on notice or
  timeout, inspect worker status/tests/conflicts, coordinate and wait again.
- [ ] Review worker notifications, diffs, tests and blockers and issue directions;
  record material decisions in the owning SOAP plan. Six-hour checks provide the
  requested fallback if notification delivery fails.
- [ ] Obtain current issue-linked feedback from all three active workers, then
  keep their concise status tables current at milestones and six-hour checks.
- [x] Receive and review the initial three issue-linked reports; keep future
  updates in those existing tables, not another reporting subsystem.
- [ ] Review cross-owner findings; transfer file ownership for an explicit epoch
  when needed, rather than permanently excluding a necessary large refactor.
- [ ] Integrate each completed epoch, run relevant combined regression gates,
  record unresolved failures, and publish only reviewed commits to Main.
- [ ] Prioritize coupled Job/Evidence/performance verification on exact tested
  epochs and a common producer; compare isolated and combined costs without
  attributing storage deletion to an unmeasured speedup.
- [x] Complete the common-producer four-variant/five-input census and eleven
  focused combined gates; delegate broad combined gates to performance.
- Completion: workers can deliver independent changes without a second Core
  authority; SE completion remains governed by its original criteria.

## 2. C Backend

### Subjective (User)

2026-10-03, English paraphrase of the latest clarification: keep the C backend
a downstream project using `.a` and LinkerScript. Prioritize usable C code and
interoperation with other C modules without excessive scope expansion. Its
importance is connecting A Program to external systems and generic assembler
lowering; "downstream" does not mean unimportant.

2026-10-03, English paraphrase: assign C-backend design to the first sub-session.
Earlier requirements keep C/LinkerScript subordinate to A Program: target
conventions must not become new `.a` fields or source-semantic requirements.

### Objective (Code)

`src/prototype/c_backend/emit.h:pg_c_emit` borrows typed Occurrences. Its driver
still loads Job-rooted exports and calls `pg_artifact_revalidate`; it is not
fully independent of ongoing Main changes. `build.mk` defaults to shared `/tmp`
paths, so workers must supply private `OVERLAY` and `BUILD` paths.
GitHub inspection finds #44 and #49 open; no fresh backend verification here.

2026-10-03 checkpoint: worker epochs `bb69983` and `4987c08` are pushed to
`parallel/c-backend-20261003`; Core integrated them through `9061be3` and
`e5d4057`. The second epoch adds native single-tail List lowering. All seven
backend gates pass with the current Core family-cursor and Surface prototypes;
worker sanitizer, ABI and immutability controls are in the
[handoff](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md). These are prototype-only
merges, not accepted-source promotion. Broader recursive/container, indexed,
effect and admission contracts remain open; neither #44 nor #49 is complete.

2026-10-03 04:15 UTC checkpoint: Core verified the exact 21-file Epoch3 manifest,
reviewed native Nat32/recursive-call/List copy-out and receipt ownership, published
`720f92a` to the task branch, then ran all eight C gates on the current Core
family-cursor plus Surface producer (exit 0). Prototype Main merge `0fc0c0b` is
pushed. The [handoff](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md) pins worker
O2/sanitizer evidence and remaining resource/refusal contracts. Source changes
are +228/-73; test/fixtures +406/-5; build +4/-0; docs +296/-14. No accepted
source, `.a` schema or producer authority changed. Core released the freeze for
native Acc/QuickSort continuation; structural sorting success is not native
completion, and #44/#49/full worker Goal remain open.

2026-10-03 integrated checkpoint: Core verified all eleven frozen Epoch4 file hashes
against worker base `720f92a` and inspected transactional array-to-List emission,
rollback and ordinary C clients. Worker O2/sanitizer gates are reported in the
[handoff](2026-10-03-C-BACKEND-EPOCH4-HANDOFF.md). Core published `a3b6bce` and
freshly passed all eight C gates against Main `f27bd13` with family/Surface,
Job E1 and performance cleanup. The 128 assembled runtime hashes match the
joint broad-tested snapshot exactly. Main prototype merge is `b0422d7`.
An initial archive lacked Git metadata and a first assembly omitted Surface;
both setup failures are retained. Corrected assembly and terminal gates are
pinned in [Core evidence](../src/prototype/c_backend/verification/core-epoch4.json).
Native open QuickSort explicitly refuses
its unsupported representation; neither structural success nor array conversion
establishes native Acc/QuickSort completion.

Epoch5 `312c2da` is published and Core's nine current-producer C gates pass,
including known local functions, captures, two-module array/arena exchange and
the new raw static-function gate. Main prototype merge: `435d965`.
Lowering changes +7/-4; build +8/-0; tests +488/-4; documentation +492/-8.
The former supported block cases have positive coverage, not deleted tests;
dynamic callbacks, demanded effects and the specific three-closure capture shape
still refuse. [Core evidence](../src/prototype/c_backend/verification/core-epoch5.json)
pins the combined gate; worker sanitizer results remain in the
[handoff](2026-10-03-C-BACKEND-EPOCH5-HANDOFF.md).

Epoch6 `cea1dc0` is exact task-branch published (16 frozen files, push 0,
independent remote verified). Core's ten O2 C gates pass on the jointly tested
E3/family/Surface/head producer; its 128 runtime hashes are unchanged after the
run. Native value records cover 105 cases per C product and 16 source observations.
Core integrated this prototype as `53debc8`; no producer/schema or accepted-source
change. [Core evidence](../src/prototype/c_backend/verification/core-epoch6.json)
pins the fresh combined run, not a rerun of the worker's seven sanitizer gates.

### Assessment

Agent proposal: Sub1 owns target realization, ABI and LinkerScript design and
their prototypes/tests. Use a pinned committed producer and immutable `.a`
fixtures while the producer changes internally. `job-evidence` owns SE-related
producer/admission/transport changes; route needs through Core for an explicit
scope transfer without creating a private checker or shadow IR.
Start with supported checked exports, not completion of all relevance research.

### Plan

- [ ] Recheck #61 residuals (superseded #44/#49) against the pinned producer and specify one target epoch.
- [ ] Work under `src/prototype/c_backend/`; use a lane-specific SOAP work list
  linked from AP4-AP6 rather than duplicate those lists here.
- [ ] Agree the selected typed-export interface and explicit pending/unsupported
  behavior with Main; keep target data/layout and realization policy downstream.
- [ ] Verify emitted C as a usable C module, evaluator agreement, ABI/effect
  behavior, negative controls and input `.a` immutability for the chosen subset.
- [ ] Test both pinned fixtures and newly produced images before integration.
- Completion: a reviewed target epoch passes its stated gates without extending
  `.a` for backend-only needs or claiming completion of unsupported constructs.

## 3. Simplification Before Tuning

### Subjective (User)

2026-10-03, English paraphrase of the later human clarification relayed by
inquiry desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: improve speed by
deleting unnecessary mechanisms/work, without adding tuning complexity. Retaining
intermediate computation for resume is acceptable; roughly order-of-magnitude
excess versus other systems is not. Preserve necessary progress/checked facts,
delete redundant graphs/copies/dead transient state, count any new index/owner
overhead, and demonstrate net memory/time effects without changing meaning,
fuel or resume. No unsafe borrowing, discarded required progress, hard 10x target
or overlapping heavy benchmark is approved.

2026-10-03, English paraphrase of the human requirement relayed by the same
inquiry desk: improve both speed and peak memory through software/design changes.
The reported AP tree peak of about 1847 MiB versus native checkers' about
97-529 MiB is excessive. Pin allocation/retained-owner causes jointly with
Job/Evidence and delete redundant graphs/copies. Validate matched wall/RSS and
semantic/fuel/resume behavior; record/file-size reductions alone are not measured
RAM gains. Do not use unsafe borrowing or add wrappers merely for profiling.
Existing qualified epochs continue; no heavy rerun is requested by this inquiry.

2026-10-03, English translation of explicit human approval relayed by the inquiry
desk: promote verified speed improvements from Prototype into accepted `src/`.
This supersedes the former no-promotion boundary for selected performance work,
with necessary dependencies reviewed and unrelated dirty files preserved. The
[separate promotion plan](2026-10-03-VERIFIED-PERFORMANCE-PROMOTION-PLAN.md)
owns implementation and accepted regression/parity verification. Other prototype
lanes and deferred design choices are not blanket-approved.

2026-10-03, English paraphrase of the latest clarification: performance work
and Job/Evidence reduction or deletion are closely coupled and need focused
joint verification. Separate sessions must not obscure their shared costs or
combined correctness.

2026-10-03, English paraphrase: assign system performance, including waste in
other modules, to the second sub-session. Prioritize a simpler implementation
and removal of unnecessary work over technical tuning that complicates it.
Consider comparisons with Bend2, Lean, Agda and Rocq. Related issues/PRs are
forthcoming; this is not approval to weaken checking or introduce a new engine.

### Objective (Code)

The initial GitHub inspection listed seven issues and three PRs (#53-#55), before
the arrival of #56/#57 and PR #58. #51 concerns ownership/size
measurement, #52 selected checked relation reuse, #53 audit documents, and #55
long-term distributed Solve intent, not an immediate scheduler implementation.
PR #58's documentation was subsequently inspected and merged; publication is
not acceptance of all recommendations or fresh verification of speed claims.

Existing tools include `artifact_persistence/state_audit.c`,
`allocation_audit.c`, `image_audit/fuel_curve.sh`, and the readback construction
benchmark. `src/graph.c` already has exact interning and scoped comparison;
`src/eval.h` separates reduction, readback and substitution. Their existence
does not establish which is currently dominant. Current cross-system timings
and fresh bottleneck measurements have not been obtained.

2026-10-03 checkpoint: focused captured-head/direct-IADT epoch `f3c3555` is
pushed on `parallel/performance-20261003`, not merged into Main. Its evidence
distinguishes call/fuel/allocation counts from unmeasured wall/RSS, and preserves
the two original Core failures separately from adapted tests. Fresh baseline
O2 acceptance records 380 recipes with zero failures. The broad adapted-head
run found an additional `identity_io_test:total_result_machine` failure at its
`saw_projection` observer. This remains under diagnosis, not waived or claimed
passing. Broader persistence/sanitizer and combined current-Core verification
remain required before integration. No comparative wall/RSS run has occurred.

2026-10-03 04:24 UTC supervision: the worker distinguishes the obsolete
descriptor-only Identity observer from seven real sanitizer failures in the
published head epoch. Core inspected the separate `eval_frame_cleanup.patch`:
successful/error head delivery releases decoded empty-frame scratch, while
return 2 preserves charged fallback. The worker initially reported 14 focused
corrective checks passing. Subsequent Core inspection confirms all 45 broader
sanitizer commands and all 384 recorded O2 acceptance recipes exit 0 in the
corrective reports. Core checked the 44-file frozen manifest, committed and
pushed `05390513bca302b4a219881994c5bbd69d5b733a` on the worker branch. Applied
runtime correction is `eval.c` +5/-1; return 2 retains charged fallback. The
original failed commands are preserved; this is not Main integration, full
worker completion or broad verification of the current joint producer.

2026-10-03 04:37 UTC Core verification: Main `341261d` canonical producer with
family/Surface, published performance `f3c3555`, separate frame cleanup and
explicit observer adapters passes ten checks: Core/IADT/Synthesis, full Source
IO/Identity IO, head/TotalResult/cleanup units, normalization checkpoint and all
70 fresh-process TotalResult cuts. Charged depth-1,000 WHNF remains 3,014 steps.
Logs: `/tmp/a-program-core-performance-combined-*`; source manifest:
`/tmp/a-program-core-performance-combined-source.sha256`. Core retained its
initial failed build: assembly accidentally replaced the newer prototype
`eval.h` with the accepted header. Correcting that assembly yields exit 0; this
is not a runtime regression. Full combined acceptance and actual timing remain
open. Job deletion is not included in this ten-check result.

2026-10-03 joint terminal checkpoint: the performance worker completed the exact
128-source current-producer/Job E1/head-cleanup snapshot. Core read its frozen
summary: 45 sanitizer and 23 O2 focused commands pass; transport, semantic,
history and seven checkpoints pass. The 384-recipe acceptance run originally
failed because its private copy omitted four training inputs; the unchanged
158-entry syntax gate passes after restoring those inputs. That original failure
is retained, not relabelled passing. The strict partition target still fails at
the same three cases. Evidence: private
`/tmp/ap-performance-current-joint-341261d-0539051-20261003/verification.sha256`
(1033 records, SHA256 `ae05927ed56cb773e9513cbcf5e2ff1b5d687ede76ec0d0bd89930ab74fe7460`).
Four cross-instrumentation raw image hashes differ; all 140 cross-build reads
pass. No cross-instrumentation byte equality or wall/RSS speedup is claimed.

### Assessment

Agent proposal: Sub2 measures the whole path but owns only an agreed,
non-overlapping implementation epoch. First look for repeated traversal,
construction, copying and recoverable state, not extra caches or wrappers.
Job/Evidence/query/admission/frontier findings go through Core to `job-evidence`.
Pure graph,
evaluator or readback changes can be developed separately after checking shared
dependencies. Do not silently change fuel granularity to report fewer steps.

2026-10-03 15:45 UTC, Root operational exclusive grant
`MEM1V3-E12-20261003T154600Z-600`: only the36 configured paired AP samples,
not before15:46 UTC and all children stopped before15:56 UTC. Configa3335c5e,
all280 pins, current candidate1816 and corrected-E12 baseline2064 hashes
verified. Frozen V3 reconstructs the current pair exactly, changing eval.c/h
only; Root fresh original V3 O2/SAN builds and three callback units each pass
(report6a09e5a5). Initial Root missing-Makefile setup failure is preserved.
C/Job capacity-stalled with no observed workload children; completed owners stay
stopped; Root runs no heavy work during the slot. Derive only the remaining
maximum_seconds at launch, record hashes/UTC and preserve censored results.
Post-review checks every expected charged-step count; the collector's DONE
prefix alone does not qualify a sample. This is an agent scheduling grant within
the existing scope, not a user design decision, accepted promotion or Goal
completion. See `/tmp/a-program-merge-mem1-v3-e12-exclusive-grant-20261003.json`
and `/tmp/ap-performance-mem1-v3-current-e12-cost-request-20261003/Root-review.json`.

2026-10-03, Merge memory scheduling decision following the human clarification:
MEM1 is explicitly pending after distinct E9/E9+E10 qualification. Its work list
is in the [performance Goal](2026-10-03-PERFORMANCE-GOAL.md#plan); Job/Evidence
owns findings in its existing SE list. E6 head peak RSS is about1847 MiB even
though final interned Core terms fall from10,636,362 to298,214. Those observations
do not attribute the peak: distinguish peak, live and cumulative bytes and
retained capacity by Core/typed/Evidence/Job/query/index/scratch owner, including
all new overhead and representative size scaling. Reuse existing tools and
choose the simplest safe deletion; preserve required checked facts and suspended
frontiers. The native comparison includes different helper/proof work, so no
universal ratio or hard target follows. Correctness-only current epochs continue;
matched time/RSS waits for a later exclusive slot. Rejected E11 demonstrates why
owner lifetime must be checked before counting a borrowing change as a saving.

Cross-system comparison must match results and work performed. Measure source
construction/type checking, proof construction/conversion, evaluation and native
execution separately. Pin versions, semantics, representation, integer behavior,
proof scope, optimization, CPU/GPU and process/cache conditions. If a system
cannot express a chosen proof task, mark it not comparable, not faster. Identify
the exact Bend2 implementation before choosing commands or citing claims.

### Plan

- [x] Inspect the performance issue #56 and PR #58; delegate detailed revalidation
  to the owning worker and record adopted, rejected and
  deferred recommendations without treating reported problems as current bugs.
- [ ] Coordinate pending MEM1 after current qualification; performance owns
  attribution/scaling/matched costs and Job owns the corresponding safe deletion.
  Record implemented, measured and pending status separately; no new heavy rerun
  is triggered by the human plan/status inquiry.
- [ ] Pin the latest committed baseline and its parent with identical workloads,
  including small Core cases, List induction and ordinary-result sort proofs.
- [ ] Reuse existing census/timing tools; record repeated wall/CPU time, peak
  memory, fuel, terms/typed records, allocations and `.a` bytes in a short TSV.
- [ ] Keep runtime sorting cost separate from its checking/proof cost; use
  completed results, not equal fuel alone, for end-to-end speed comparisons.
- [ ] Profile before selecting one deletion/refactor epoch; document the owner,
  redundant work and simpler replacement. Do not build another program graph.
- [ ] Hand Job/Evidence-owned changes through Core to that worker; prototype
  other agreed changes in a new `src/prototype/` subtree, not accepted files or
  the SE overlay patches.
- [ ] Research primary sources and run genuinely comparable cross-system cases;
  record unavailable tools and unmatched tasks explicitly.
- [ ] Verify meaning, capture/scope, synthesis-first `::`, effect ordering,
  totality and applicable image/fuel invariants; preserve existing failure reports.
- [ ] Report implementation/test/doc additions and deletions separately, per-file
  changes and before/after measurements; reject complexity without demonstrated benefit.
- Completion per epoch: unnecessary work is removed, relevant correctness gates
  pass and measured cost/complexity is reported without changing the task.

## 5. Job/Evidence Implementation Owner

### Subjective (User)

2026-10-03, English paraphrase of the later human clarification relayed by the
same inquiry desk: refactor for speed by deleting unnecessary mechanisms/work.
Resume may retain necessary intermediate computation, but does not justify
roughly order-of-magnitude excess memory. Preserve necessary frontier/checked
facts, distinguish redundant graphs/copies/dead transient state, include new
index/ownership overhead, and demonstrate net memory/time effects with meaning,
fuel and resume intact. No unsafe borrowing, discarded required progress, hard
10x target or overlapping heavy benchmark is approved.

2026-10-03, English paraphrase of the human requirement relayed by inquiry desk
thread `019ebfae-06be-7b71-974a-b97505daed4a`: both speed and peak memory require
software/design improvement. Coordinate with performance to pin allocations and
retained owners and delete redundant graphs/copies. Use matched wall/RSS plus
semantic/fuel/resume gates; storage counts are not RAM measurements. Preserve
safe lifetimes without unsafe borrowing or profiling-only wrappers. Continue
qualified epochs; this status inquiry does not request heavy reruns.

2026-10-03, English paraphrase of the latest clarification: examine Job/Evidence
reduction or deletion together with performance, concentrating verification on
their interaction rather than treating their session boundaries as independent
architectures.

2026-10-03, English paraphrase: delegate Job/Evidence implementation to a
separate Codex session and specialize this Core session in merges and audits.
Earlier requirements retain Oracle locality, typed construction as authority,
concrete duplication deletion, correct resume and meaningful verification.

### Objective (Code)

Main `64df10d` contains the verified family-cursor epoch and previously reviewed
SE prototype patches. SE1-SE5 is unfinished; the three public split-fuel failures
in its verification report remain unwaived. Unrelated dirty accepted-source and
test changes are not part of the worker baseline.

2026-10-03 supervision at worker `5035c7a` plus private epoch edits: Core
inspected removal of copied module/reference results, the module export cache
and redundant stage. Epoch1's frozen manifest verifies all file hashes; its
reported full acceptance, focused sanitizers and C gates pass. Applied runtime
delta is +7/-12; tests +21/-3. Epoch2 extends deletion privately and remains
distinct pending its broad verification. Epoch3 investigates reconnecting
started definition bodies through existing lexical factories, not another graph.

The initial QuickSort census was rejected; its failure records remain. The
corrected provider/import census completes at 766,477 dispatches on both Epoch1
variants, with 3,753 fewer copied result references but identical Term,
Occurrence, Evidence and dispatch counts. Five completed workloads remove
50/154/111/3,753/26 result references. This is storage/reference attribution,
not an observed time or peak-memory improvement. Evidence is frozen under the
worker's `src/prototype/solver_inputs/epochs/job_evidence_e1/`.
Core's current producer plus Surface/performance/cleanup joint Epoch1 build is
verified by eleven terminal exit-0 gates: Core/IADT/Synthesis, full Source IO
and Identity IO, head/TotalResult/cleanup, normalization/source checkpoints and
70 fresh-process TotalResult cuts. The exact 128-source manifest is
`/tmp/a-program-core-job-performance-combined-source.sha256`; relevant logs are
`/tmp/a-program-core-job-performance-combined-*`. Performance is assigned the
broader acceptance/checkpoint/sanitizer gates on a private dereferenced copy;
their terminal results and retained failures are recorded in section 3.
Core published frozen E1 as `7c6a625` and integrated it as `b31d7a5`; the jointly
verified performance correction is integrated as `f27bd13`. E2's 36-entry
manifest passes Core hash verification and its applied
code was reviewed; E2 needs current-producer combined gates before integration.
Later E3/E4 persistence/registration trials remain independent worker epochs.
Core's first six-hour Wait was interrupted after 294.2237 seconds by E2's lean
publication-delta notice. Live supervision then caught the worker's correction:
its generated report patch normalized TSV newlines, breaking exact report hashes.
Publication was held without staging E2. Original runtime freeze is unchanged
and remains available for the separately assigned current-producer joint gates.
Keep the failed delta/report as history and verify corrected exact bytes before
task-branch publication; do not classify this report-assembly error as a runtime bug.
The corrected v2 delta subsequently passes Core's index-level hash check for all
five canonical and eleven lean-manifest records, including exact TSV bytes. E2
was published as `44b0389` on its task branch. Subsequent E2 joint qualification
passes all 384 O2 acceptance recipes, 45 sanitizer commands, 23 focused checks,
140 cross-build readbacks and transport/semantic/history/seven checkpoints.
Core freshly verified all 2,007 frozen evidence records and reviewed the exact
canonical borrowing change, then integrated E2 as `7b27c4c`. The same three
strict reload failures remain failed; two descriptor-ID byte variations do not
establish cross-process byte identity. The
[concise joint result](../src/prototype/solver_inputs/joint_verification/e2-broad-summary.json)
pins the source/evidence hashes. This is prototype integration, not accepted
promotion, full SE completion or a measured wall/RSS improvement.
Separate E3 `51c476d` is committed and pushed after one retained remote rejection
and a successful ordinary retry with independent remote-ref verification. Core
reviewed inert lexical reconnection, checked-owner barriers and the existing
completion byte's separate descriptive/start bits. All 24 published canonical
and report hash records match Git's index. APGSRC69 rejects older formats;
loading restores a body link, not accepted evidence or child computation progress.
Worker gates pass with setup failures retained; the same strict three remain.
E3 subsequently passes its separate current-producer codec/frontier qualification:
57 O2 recipes (only the inherited strict partition target fails), 13 sanitizer
commands, inert resave/invalid-marker controls and paired old-format controls.
The exact E2 parent fails both missing-body assertions while E3 passes them.
Core freshly verified all 1,318 evidence records and integrated E3 as `48364b0`;
this targeted result does not claim a repeated full 384-recipe suite.
[Joint evidence](../src/prototype/solver_inputs/joint_verification/e3-codec-summary.json)
retains the same three strict reload failures and parent/setup evidence.
Its original frozen manifest includes an external transport patch deliberately
not published. Future durable manifests must list published files only; keep
transport hashes separate and avoid copying canonical patches already pinned by
Git. Original freezes are not rewritten.

E4 `ec95371` is task-branch published, with push exit 0 and an independently
matching remote ref. Core reviewed registration-key borrowing and checked all
13 published manifest records plus the manifest against Git's index, and all
11 local verification-log hashes. The lean manifest contains published files
only; external transport and historical full freezes are not copied into Git.
Applied runtime +37/-30, tests +10/-0 remove three duplicated registration fields
(24 bytes per registration), not calculation steps or all Job/Evidence storage.
Worker gates pass with the same strict three failures; Main integration awaits
separate E2/E3/E4 current-producer qualification. Performance is assigned these
layers in order, retaining each snapshot and avoiding needless identical suite
reruns. Its first E2 setup attempt stopped at a migration TSV column-name error
before any gate ran; that failure is retained. C nested-value work remains a
separate live downstream trial.

Private E5 declaration-export deletion is rejected, not published. Core inspected
its code and verified ten frozen records plus five local report hashes (manifest
`218b9550fbec3aaeb8a42ecb83d6897691b4547a7ea4dd2fe4748da201bb87eb`).
Two declarations sharing one nominal formation expose `zero/succ` versus
`nothing/successor`: E4 with the new namespace test passes; E5 reads overwritten
typed-subject metadata and loses `Original.zero` (reported exits 0 versus 134).
This rejects that lookup, not every possible future owner refactor. Preserve
lexical identity through its existing source owner rather than merging names
or adding a shadow Oracle graph. Job owns permanent publication of the 17-line
test-only control; performance includes it in final E4 coverage without E5 code.

E6 exact lexical Binder borrowing is task-pushed as `b2d6668`; Core verified all
14 published hash records plus the manifest and local verification logs. Applied
runtime +12/-12 removes a redundant Binder pointer; worker QuickSort Job payload
falls 7,872 bytes with checking counts unchanged. The permanent E5 namespace
control is included. Generic Git whitespace checking reports patch-context and
CRLF-report formatting, not an applied-C source failure; preserve those exact
frozen bytes. Main integration awaits E4 then E6 joint qualification. E7's
namespace-preserving source-owner metadata borrowing is only locally verified:
reported layout -1,312 QuickSort bytes, no timing or peak-memory claim. E8 remains
a private Graph-output borrowing trial. Full SE1-SE5 completion is not claimed.

The common-producer census completes all twenty variant/input pairs, including
the ordinary imported general LocalSorted QuickSort. Its final QuickSort rows:

| Variant | Dispatches | Terms | Occurrences | Evidence | Job payload bytes | Copied result refs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline | 766477 | 1037917 | 114388 | 94974 | 6764880 | 27902 |
| Job E1 | 766477 | 1037917 | 114388 | 94974 | 6764848 | 24149 |
| Head + cleanup | 761848 | 1036772 | 114388 | 94974 | 6764880 | 27902 |
| Both | 761848 | 1036772 | 114388 | 94974 | 6764848 | 24149 |

[Measurements](../src/prototype/solver_inputs/joint_verification/measurements.tsv)
include step 0, 100 and 1000 checkpoints and the other four inputs; source/input
and report hashes are beside them. This attributes 4629 fewer dispatches to the
head path and 3753 fewer copied references to Job E1, not a measured wall/RSS
speedup. Fixed result slots still occupy their headers; do not convert the
reference count into an assumed byte saving. All 52 public partition images
and all 40 verdict rows are byte-identical with and without Job E1 on this
producer. The same three strict reload failures remain failed and unwaived in
the [partition report](../src/prototype/solver_inputs/joint_verification/combined-partitions.tsv).

### Assessment

Agent implementation-workflow decision within the user's scope: create one
`job-evidence` worktree/window and keep SE1-SE5 as its sole active work list.
Its Goal brief is a handoff, not a replacement architecture or second checklist.
Core reviews owner changes, interfaces, tests and resulting deletions before
merging; the performance worker does not independently modify the same owners.

2026-10-03 15:29 UTC, Root scope review of E15 notice22b47dd7: the proposed
deletion concerns the copied ordinary schema input array in function_graph.c/
internal.h. Its ephemeral borrowed leaf iterator stays with Job; Performance's
direct-IADT ownership remains separate. Retaining charged Graph traversal and
the normalized Lambda key is necessary until identity-preserving replacement is
proved. Source/owner lifetime, reset/repeated reads, nominal sharing and resume
controls remain qualification criteria; this is a scope assessment, not proof
of an implementation or a measured gain. No competing owner was started.

Core operational clarification, 2026-10-03: existing AP1/AP3 already require
one total fuel budget, explicit validation sublimit and optional explicit trust.
Default inert loading does not admit saved completion. Keep the three original
strict failures visible while separating revalidation cost, saved progress and
local acceptance. Core assigns necessary producer-side artifact-persistence
changes to this worker for a named epoch; do not add another replay engine or
use automatic trust to repair a gate.

### Plan

- [x] Start from committed Main in `parallel/job-evidence-20261003`; provide
  separate build/output paths and the existing prototype overlay recipe.
- [x] Read the current owner code, select a concrete deletion epoch and implement
  it alongside focused verification, rather than postponing coding for more logs.
- [ ] Keep semantic, scope, effect, synthesis-first `::`, fuel, ordinary-result
  proof and persistence gates; report inherited failures without waiving them.
- [ ] Hand off frozen tested epochs with exact files, applied source/test deltas
  and known blockers. Core alone decides Main integration.
- Completion: governed by SE1-SE5, not by worker launch or one small deletion.

## Coordination

- Current tree: visible inquiry desk <- one separate Merge session <-
  `job-evidence`, `performance`, `c-backend`, `verification-audit`.
  Surface and AIze are finished/stopped; do not restart them without new scope.
  Only Merge has Main integration, delegated task-publication and closure duties.
  The desk bootstraps this handoff once, then stops Git mutations.
- Merge's initialization brief: `src/prototype/coordination/merge-session-brief.md`.
  Audit lane brief: `src/prototype/coordination/verification-audit-brief.md`.
  Notification routes and live thread IDs are recorded after actual verification,
  not inferred from this proposed assignment.
- Worker Goal briefs: [C](2026-10-03-C-BACKEND-GOAL.md),
  [performance](2026-10-03-PERFORMANCE-GOAL.md),
  [surface](2026-10-03-SURFACE-GOAL.md),
  [Job/Evidence](2026-10-03-JOB-EVIDENCE-GOAL.md). The latest five-session
  assignment supersedes older Main/Sub numbering; this session is coordination
  `core`, while `job-evidence` owns SE1-SE5 implementation.
- Wake service: tmux `a-program:core-notifications`, Core-owned Unix WebSocket
  relay from each lane's `src/prototype/coordination/outbox/*.txt`. Verify helpers
  with `node src/prototype/coordination/test_notify.cjs` (five mock controls pass).
  Core waits interruptibly for at most six hours between reviews. Worker notices
  wake this loaded Core earlier; unavailable delivery stays in the outbox for the
  timed review. Closing Core/host restart is not an independently verified service.
- 2026-10-03: #56/#57 and documentation PR #58 are now available. The
  coordinator read issue bodies and the current-head review; historical supplied
  reports are evidence to revalidate, not accepted patches or fresh speed claims.
- Workers push only their own branches; Main performs reviewed integration.
- Worker sandbox failure at shared worktree Git metadata is now observed on
  `surface` (`index.lock: Read-only file system`). Agent publication decision:
  Core may commit/push a worker's verified epoch on its task branch after the
  worker hands it off. This is delegated publication, not a merge/promotion or
  a workaround granting blanket filesystem access; tests can continue meanwhile.
- One owner edits each implementation/plan per epoch. Shared findings are
  handed off, not solved independently in both lanes.
- Correctness-only Core j1 and worker j2 gates may overlap. Comparative wall/RSS
  measurements require an exclusive agreed slot with other CPU-heavy work paused.
  This supersedes the earlier single-slot restriction for all broad correctness.
- tmux manages independent sessions, not shared chat context. Each brief carries
  the revision, write scope, user Subjective, task/tests and return conditions.
- AGENTS.md's prototype boundary and intentional promotion rule still apply.
- No tmux installation, worker launch, merge, issue closure or new engine is
  implied by the initial plan. The later explicit worker instructions authorize
  setup; record actual launch/model/Goal verification below, not inferred success.

## Setup Evidence

2026-10-03: local CLI `0.159.2` reports `goals` enabled. Official documentation
supports persistent `/goal` and `gpt-6.1-sol` / `xhigh`; account access still needs
a live check. tmux `3.5a` was extracted from Debian packages into the user's
`.local/share/a-program/`, without sudo or modifying system packages. No workers
have been launched at this checkpoint. No compiler tests were run for setup.
Later on 2026-10-03, `/usr/bin/tmux` reports `3.5a` after the user's installation.
A private-socket detached test session was created, listed and removed
successfully; the default socket had no sessions at inspection. A live ephemeral
Codex check succeeded with explicit `gpt-6.1-sol`, `xhigh` and no fallback.
These checks establish tooling, not launched workers or resumed Goals.
[Goals](https://developers.openai.com/cookbook/examples/codex/using_goals_in_codex),
[GPT-6.1 Sol](https://developers.openai.com/api/docs/models/gpt-6.1-sol).

2026-10-03 02:04 UTC: default tmux session `a-program` has live windows
`c-backend`, `performance` and `surface`. Each pane reports `GPT-6.1-Sol xhigh`,
`Pursuing goal` and actual inspection/edit activity. Their separate worktrees
are `/home/repyt/workspace/a-program-workers/<task>` on branches
`parallel/<task>-20261003`, all started from `2d747ccfec844e8afc72d408385ceb79a7c01808`.
That revision merges the three PR #58 documents without implementation changes.
This is the launch checkpoint; later integration supersedes its initial state. View with
`tmux attach -t a-program`; select a window with `Ctrl+b`, then `w`.

2026-10-03 03:39 UTC supervision checkpoint: C/performance Goals remain active;
Surface completed its scoped prototype Goal and is available. Surface epochs
`90939fc`/`276f4c3` are pushed and Core integrated them through `7a9a672`.
The original broad failure was an old-brace `sed` harness migration omission;
the corrected entire failing gate passes. Other independent broad gates passed
before that one-line correction; no second full rerun is claimed. Core separately
verified the current-owner combined Surface/IADT/Synthesis/source-I/O/transport
gates and all seven C gates before merging. #57 remains open for unfinished
policy/diagnostics and accepted adoption; branch publication and prototype Main
integration are not promotion. GitHub was rechecked: no new open issue/PR since
the preceding poll; existing partial issues remain open.

2026-10-03 04:02 UTC role-transfer checkpoint: a fourth worker window,
`a-program:job-evidence`, runs at
`/home/repyt/workspace/a-program-workers/job-evidence`, branch
`parallel/job-evidence-20261003`, starting from clean committed `5035c7a`
(producer `64df10d`). Its pane reports `GPT-6.1-Sol xhigh`, `Pursuing goal`
and actual repository/SE plan reads. Implementation has started with inspection;
no new deletion or passing tests are claimed at launch. C/performance acknowledged
the owner transfer and continue their verification; Surface remains delivered.
Core owns coordination/review/integration only and has stopped parallel SE code
edits. The handoff/role decision is pushed on Main as `5035c7a`; no accepted
implementation, model update or issue closure was performed for setup.
GitHub poll at this checkpoint found no new open issue/PR since the prior poll.

2026-10-03 04:09 UTC: Core verified Surface's pane reported `Goal achieved`,
its published implementation remains `276f4c3`, and only its owning Goal document
has later local edits. At the user's explicit instruction Core closed only
`a-program:surface`; the three other worker windows remain live. The worktree,
branch, frozen evidence and local documentation are retained. No issue closure
or broader source promotion is implied. Job/Evidence has begun actual owner
deletions and focused tests; Core granted its requested O2 acceptance `-j2`
correctness slot. Comparative timing remains exclusive. Performance reports
restored-frame scratch leaks in its sanitizer batch and is verifying a separate
evaluator-only correction; its failed published epoch is not merged or waived.

## 7. Test Suite Implementation Successor

### Subjective (User)

2026-10-03, English paraphrase of the explicit human request via inquiry desk
`019ebfae`: use PR60/#59 audit evidence to investigate test growth/duplication,
write the owning plan and implement coverage-preserving code/line reductions.
Continue independent prototype epochs; Merge resolves conflicts. Accepted
tests/build require a separate intentional promotion.

### Objective (Code)

Final handoff `4432e3d` and manifest `0f30bef9` verify exact seven files.
At13:49 UTC, tmux%12 reports Goal achieved. Root recomputed focused record
equivalence, verified all177 frozen archive records/776 dependencies and all47
completed recipe exits/ten injection outcomes. Fresh original/reduced assembly
matches qualified E9+E10 runtime128, pinned23 C tests and both tested scripts.
No derived-LT terminal pass, comparative cost or accepted implementation claim.
Exact7 task `bc16df9` pushed/remote verified; prototype Main `36a27be` integrated.
Root corrected two documentation provenance labels; other five task files exact.
Completed static audit reused.

### Assessment

Merge routes overlapping Core/SourceIO controls to Job and evaluator/codec
controls to Performance. Existing AP image-policy migration is historical
policy retirement, not new deduplication credit. Preserve known failures and
unique soundness, lifetime, fuel, origin and resume boundaries.

### Plan

Use the one work list in the
[owning plan](2026-10-03-TEST-SUITE-DEDUPLICATION-WORKER-PLAN.md).
Merge reviews frozen code, distinct coverage and old/new evidence before
integration; publication does not block independent implementation.

## 8. Issue Lifecycle Audit

### Subjective (User)

2026-10-03, English paraphrase of the explicit human request via inquiry desk
`019ebfae`: examine all open Issues/PRs for completion and necessity; comment
and close only with evidence. Consolidate useful residuals into successors and
cross-link/close superseded originals. Assess #59 first rather than assume it
complete. This is GitHub disposition work, not code or heavy-test authority.

### Objective (Code)

Final handoff `bfbabb9b`, two-record manifest `b3f86ed7`, task `8bb5018`
and local Main `b461ace` verified. Root independently reread ten comment hashes,
#61 body/open and #44/#49 closed/not_planned. Final nine issues/zero PRs match.
At13:49 UTC, tmux%14 reports Goal achieved; completion accounting confirmed.

### Assessment

Issue-audit completed its sole-writer disposition scope and remains stopped;
Root resumes coordination. Open successor #61 preserves separate source/native
residuals and the existing C owner. Closure of #44/#49 is supersession, not
implementation completion. No accepted #57 policy or strict-reload waiver follows.

### Plan

Use the one work list in the
[owning plan](2026-10-03-ISSUE-LIFECYCLE-CONSOLIDATION-WORKER-PLAN.md).
Comments, links and states verified against the owner's criteria; bounded audit
complete. Retain unfinished issues open; do not restart either completed audit.
