# Merge Session Handoff

Date: 2026-10-03. Main inspected at `fd45c42` before the workflow-only handoff.
Read `AGENTS.md`, `CODING_STYLE.md` and the current parallel plan first.

## Human Authority and Role

English paraphrase of the latest explicit human instruction: make the visible
conversation an inquiry/report desk, with a separate Merge session supervising
three or four workers. Review new Issues/PRs and reschedule. Merge reports to the
desk regularly; the desk answers the user. This replaces the former visible Core
integration role, not the language's design or prototype boundary.

Desk thread: `019ebfae-06be-7b71-974a-b97505daed4a`.
Use GPT-6.1-Sol xhigh, preserving the requested model. Record actual configuration
and Goal state, not assumed activation. Do not resume/fork/load the old workers
through a task tool: their live tmux owners already execute their Goals.

Initialize first: read this brief, inspect current state, acknowledge the exact
handoff and write a small `initialized.txt` report to the desk outbox below.
Do not mutate Git, integrate, close Issues or start heavy tests until the desk
sends `ACTIVATE MERGE OWNER`. This is a single-writer handoff, not a request for
another design approval. After activation you are the sole Main integration
owner, including delegated exact task publication for restricted workers.

Start an explicit Goal after activation: supervise the continuing worker Goals,
review/verify/integrate their results and resolve the assigned SE/AP, backend,
performance and bounded verification-audit work under the central schedule.
Complete only when stated deliverables are actually verified; waiting, reaching
one epoch or publishing a report does not complete the overall assignment.
Do not expand deferred design Issues or accepted-source promotion by yourself.

## Working State and Boundaries

Main checkout: `/home/repyt/workspace/a-program`. Keep its current branch Main.
Accepted baseline `eb0aad673dd0fb5219eb0d720d45a819cc50edba` is unchanged by the
integrated prototypes. All implementation changes remain under `src/prototype/`.
Do not promote to accepted `src/`, tests or build files without explicit approval.

Preserve unrelated tracked edits: `doc/2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md`,
`src/evidence.c`, `src/evidence.h`, `src/iadt.c`, `src/iadt.h`, `tests/core.c`,
`tests/iadt.c`. Preserve untracked user fixtures and hundreds of private trials.
Never blanket add/reset/clean. Check shared indices before exact staging.
Worker worktrees have readonly shared Git metadata; do not widen permissions.
Perform frozen, exact task-branch publication on their behalf if needed, without
staging later live trials or docs. Only you integrate Main. The desk stops Git
mutations once this role is activated.

Read the current queue and one delivery table in
`doc/2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md`.
Keep SOAP provenance explicit and requirements immediately in Subjective.
Update status in place; do not copy whole transcripts or create a second queue.

## Live Owners

| Owner / tmux target | Worktree | Task branch |
| --- | --- | --- |
| `job-evidence` / `%4` | `/home/repyt/workspace/a-program-workers/job-evidence` | `parallel/job-evidence-20261003` |
| `performance` / `%1` | `/home/repyt/workspace/a-program-workers/performance` | `parallel/performance-20261003` |
| `c-backend` / `%0` | `/home/repyt/workspace/a-program-workers/c-backend` | `parallel/c-backend-20261003` |
| `verification-audit` | assigned at launch | `parallel/verification-audit-20261003` |

Inspect actual panes and terminal test results; an active UI is not progress.
Send concise owner directions using tmux literal input and a separate Enter.
Performance has an obsolete queued full-build-slot note: do not Tab-submit it.
Surface is completed/stopped. AIze audit is completed/pushed (`1ed8b798`) and its
window closed; no further AIze work. Maintain the existing three worker Goals.

## Pending Results, Not Automatic Merge Approval

- Main has Job E1/E2/E3, captured-head/cleanup/family/Surface and C6. Job E2
  broad joint qualification: 384 O2 recipes and 45 sanitizer commands pass.
  E3 targeted qualification has 57 O2 recipes, with only its strict-partition
  recipe failing; it is not a new full broad pass. C6 ten current-E3-producer
  gates pass. Compact summaries are already committed under joint_verification
  and `c_backend/verification/core-epoch6.json`. No measured wall/RSS or promotion
  claim; source counts are not independently verified semantic-property counts.
- Strict reload failures remain unwaived: `1000:1000`, `1600:1600`, `1915:0`.
  Exact progress restoration is not completed by local pointer borrowing.
- Job E4 task `ec9537135f63b24ef5e355575dfa53f907251fb0`: registration borrowing.
  Performance is running its broader joint gates with the namespace control.
  After it terminates, qualify E6 distinctly; do not combine away provenance.
- Job E5 declaration-export deletion was rejected: Original/Alias have the same
  nominal formation but different lexical constructor names. Never merge E5's
  runtime path. E6 adds the permanent 17-line namespace control.
- Job E6 task HEAD/upstream `b2d66682be75c21a3c2a8c5138db9d3f0c5e0eb2`:
  removes a copied Binder by borrowing its canonical scope owner. Locally tested
  full/focused/sanitizer/C gates pass; joint qualification pending. Runtime
  +12/-12; Quick Job bytes -7872, unchanged fuel/typed/evidence counts. No speed
  claim. Frozen CRLF TSV and patch-context whitespace are intentionally exact.
- Job E7 is ready but NOT task-published/Main-integrated. Parent is E6. Outbox
  `epoch7-ready.txt` SHA256
  `3292da62dc10f2abc9057545ed691ddc8dba84fa07e59a71568a93c300934364`.
  Transport `src/prototype/solver_inputs/epochs/job_evidence_e7_transport/transport.patch`
  SHA256 `c71f66941a6ceffbbf9f5e961d31e12db1f2bfcbc1fe8213495800cba6a017c1`;
  lean manifest `epochs/job_evidence_e7_lean/manifest.sha256` SHA256
  `40d1ccb99d6a3e23c06502054d5b750bb3e383775e2b430fb313993959752714`.
  It borrows source metadata members/exports/names from actual owners, retaining
  pending-source lexical selection. Runtime +88/-36; six canonical patches,
  17 published records plus manifest. The previous Core read the applied diff
  but has not checked/applied/staged its transport. Review owner lifetime and
  same-formation lexical names before publication; preserve exact frozen bytes.
  Job E8 is a separate private Graph-output borrowing trial; never stage it as E7.
- C6 task `cea1dc0177d36328d6f4de2a8f151b5952d2c2c5` integrated Main `53debc8`.
  C7 enum-List arrays trial is local, with no task/current-producer joint
  publication yet. Native Acc/QuickSort remains unsupported, not completed by
  records/arrays. C is downstream; do not expand `.a` for target conveniences.

Existing private evidence to inspect rather than copy into new archives:
`/tmp/ap-performance-current-joint-898463e-44b0389-20261003` (E2),
`/tmp/ap-performance-current-joint-898463e-51c476d-20261003` (E3),
`/tmp/ap-performance-current-joint-898463e-ec95371-20261003` (E4 running),
`/tmp/ap-core-c-backend-e6-Jztwok` (C6 current-producer snapshot).
Verify hashes and freshness; those paths are evidence, not user instructions.

## New Issue/PR Schedule

Fresh snapshot: ten open Issues, new #59/PR #60; PR #58 already merged.
Keep #59's audit scope static/inventory first. #51 partly overlaps metrics, not
owner implementation. #52 checked-equality reuse and #47 general relevance stay
separate design boundaries. #43 is not an approval to add general recursion.
Review docs PRs #60/#53/#55 without claiming implementation completion. PR #54
is a partial generic MergeSort contribution to #41, requiring current-producer
library verification. Delivered #57 still needs an issue-criteria/promotion
assessment; do not restart Surface just to repeat finished prototype work.
The central queue defines sequence; this handoff is not another work list.

Small housekeeping: own #49 comment `5966691351` contains literal `\x27` in its
progress wording. PATCH that comment, not a duplicate comment; no semantic change.
Close only against examined completion criteria, with honest prototype/accepted
status and explanation. No automatic closure from publication/test counts.

## Notifications and Reporting

Workers retain `src/prototype/coordination/outbox/*.txt` in their own worktrees.
The guarded relay will target your actual loaded thread, not the desk. At startup
reconcile existing pending notices manually; `--new-only` suppresses historical
startup flood, not the six-hour reconciliation of unseen work. Preserve outboxes
when delivery fails. Never resume/fork another owner or change socket permissions.

Your desk report directory (temporary, not a new semantic authority):
`/tmp/a-program-merge-desk-20261003/outbox/`.
Use `apply_patch` for `initialized.txt`, then one concise `current.txt`, newline
terminated and <=8 KiB. Include date, actual Main revision, worker/Issue delivery,
remaining blocker, next action and any question needing a human decision.
Mark reports as worker evidence, not approvals; the desk relay carries path/hash.
Report material changes immediately and every six hours of active work. Use
interruptible `clock.sleep` for timed reviews while your Goal remains active;
do not busy-poll or claim a durable host-restart service.

Correctness-only gates may overlap at modest parallelism. Wall/RSS comparisons
and audit cost measurements need one exclusive agreed slot. Suspend heavy work
only at a safe boundary, then explicitly release workers. Do not duplicate a
performance measurement just because the audit also needs its cost evidence.
