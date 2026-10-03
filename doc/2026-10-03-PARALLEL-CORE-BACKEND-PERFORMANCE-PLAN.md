# Parallel Development: Inquiry Desk, Merge and Workers

Date: 2026-10-03
Status: Merge activated after verified handoff; the visible parent is the inquiry desk. Three implementation workers continue and the bounded verification-audit lane is active.
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
| 2 | `c-backend`: downstream C design | #44, #49; shared policy #47 | C6 value records integrated; native Acc/QuickSort remains open |
| 3 | `performance`: simplify wasteful paths and measure performance | #56 / PR #58; #51/#52 | Jointly verified epoch integrated as `f27bd13`; full Goal remains active |
| 4 | `surface`: function/function-graph Binder notation and `.p` migration | #57 / PR #58 | Verified prototype integrated; completed session stopped |
| 5 | `job-evidence`: typed ownership, duplicate deletion and exact resume | SE1-SE5, AP0 | E4/E6 distinct reviewed prototype merges `3a8c024`/`01c29c0`; E7/E8 task-published, joint review pending; strict3 remain |
| 6 | `verification-audit`: independent read-mostly suite/debt inventory | #59 / PR #60, #51 | Active bounded inventory/pilot; no test deletion, acceptance relaxation or broad rerun |

### Current Delivery Status

2026-10-03 07:44 UTC Merge inspection at `4d1d941`, before the local handoff
documentation update: all four workers supplied issue-linked feedback through
the existing outbox. Merge verified notice hashes; E4/C7 reports are newly
received evidence pending review, not independently rerun gates. These are deliverables, not issue
completion scores; worker-local results are not relabelled joint verification.

| Owner / issue scope | Delivered and verified | Not yet delivered / next boundary |
| --- | --- | --- |
| Job/Evidence; SE1-SE5 / AP0, related #51/#47 | E1-E4/E6 prototype Main; E4 `3a8c024` and E6 `01c29c0` each match runtime128 and their distinct frozen joint reports; E7/E8 exact task `3ad0303`/`50cd56b` pushed | E7/E8 joint qualification and Main integration pending; E9 private; strict3 remain |
| C backend; #44/#49 | C4-C6 prototype Main; C6 ten current-E3-producer gates pass; C7 exact 14-file task `d275b75` pushed/remote verified | C7 current-producer eleven-gate qualification and Main integration pending; native Acc/QuickSort unsupported |
| Performance; #56, measurement work #51/#52 | Captured-head cleanup prototype Main; E2/E3/E4/E6 distinct reports reviewed; E6 408 relevant O2 recipes with only unchanged strict target failure, 45 sanitizer/23 focused/140 cross-build passing | Matched E6 baseline qualified; exclusive measurement slot awaiting safe-boundary holds; no wall/RSS comparison or #52 completion claimed |
| Surface; #57 | Delivered prototype integrated; session stopped | Issue-wide closure still requires its recorded remaining criteria; no active worker |
| Verification audit; #59/PR #60, related #51 | Isolated worker launched at `4d1d941`, active bounded Goal and invocation inspection confirmed | Static effective-invocation inventory and QuickSort/persistence/legacy pilot pending; controlled cost later needs the common exclusive slot |

Owning tables: Job/Evidence's [SE Plan](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md#plan),
performance's worker-local `doc/2026-10-03-PERFORMANCE-JOINT-VERIFICATION.md`,
and C's owning Goal (C6 temporary addendum removed in the C7 freeze), plus
audit's worker-local `doc/2026-10-03-VERIFICATION-AUDIT-PLAN.md`.
Current worker reporting updates remain local until their next exact publication;
this consolidated status is durable Main feedback. #41/#43 are separate design
questions, not silently assigned to these epochs. #47's general relevance policy
and #52's equality-reuse design are not completed by storage deletion or C emission.

### Rescheduled Queue

2026-10-03 fresh GitHub snapshot: ten open Issues (#41, #43, #44, #47, #49,
#51, #52, #56, #57, #59); four open PRs (#53, #54, #55, #60).
New #59 / documentation PR #60 concern verification debt and progress accounting,
not a newly established compiler defect. PR #58 is already merged. PR #60's
672-line supplied audit is an input, not approval of every threshold or manifest.
The Issue's smaller first-epoch scope takes precedence over an automatic full
reporting subsystem. Its audit cover is pinned to `37de1e8`, not current Main.

| Order / owner | Issue or PR | Next deliverable and dependency |
| --- | --- | --- |
| 0 / desk -> Merge | Workflow | Handoff verified/activated at 07:44 UTC; Merge Goal active and both hops observed; preserve the four live workers and dirty accepted files |
| 1 / Job + performance | SE/AP, #56 | Finish in-flight E4/E6 joint qualification; review/publish E7 and subsequent borrowing results, then attack exact-resume's three remaining failures rather than treating pointer-size reductions as completion |
| 1 / verification-audit | #59 / PR #60; #51 | Static effective-invocation inventory and QuickSort/persistence/legacy pilot; distinguish unique contracts from repeated mechanics; no suite changes or broad timing pass yet |
| 2 / performance, Merge schedules slot | #56 / #51 | On a pinned jointly qualified producer, measure matched runtime/memory and audit cost in one exclusive slot; coordinate other CPU-heavy work, keep storage/fuel claims separate |
| 2 / C backend | #44 / #49 | Finish current C7 enum-List epoch and current-producer gates; state the next native Acc/QuickSort boundary explicitly; no producer schema expansion for target conveniences |
| 2 / Merge | PR #60, #53, #55 | Review/import documentation separately from implementation; preserve historical provenance, link adopted/deferred recommendations; publication does not complete Issues |
| 3 / Merge -> scoped library review | PR #54 / #41 | Verify generic MergeSort against the then-current producer and existing Local/Strong/permutation contracts; this is partial #41, not arbitrary-container completion |
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

- [ ] Recheck #44/#49 against the pinned producer and specify one target epoch.
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
