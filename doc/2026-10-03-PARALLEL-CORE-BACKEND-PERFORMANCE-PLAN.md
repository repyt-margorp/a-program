# Parallel Core, Backend, Performance and Surface Work

Date: 2026-10-03
Status: Core coordinates; Job/Evidence, C and performance Goals active; Surface delivered.
Accepted semantic baseline: `eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
Reviewed prototype Main checkpoint: `64df10d`; original worker baseline: `2d747cc`.
Local state: unrelated accepted-source and test edits remain excluded. This plan
changes no implementation or promotion rules.
Related: [SE1-SE5](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md),
[AP0-AP6](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md).
The latest five-session assignment supersedes earlier ownership assignments,
not the existing owner work lists.

## Problem List

| ID | Problem / owner | Related | Status |
| --- | --- | --- | --- |
| 1 | `core`: coordination, design audit, verification and Main integration | All lanes | Sole integration owner; no parallel SE implementation |
| 2 | `c-backend`: downstream C design | #44, #49; shared policy #47 | Single-tail List epoch integrated; numeric partition work in progress |
| 3 | `performance`: simplify wasteful paths and measure performance | #56 / PR #58; #51/#52 | Focused epoch pushed; Main integration awaits broad gates |
| 4 | `surface`: function/function-graph Binder notation and `.p` migration | #57 / PR #58 | Verified prototype integrated; worker available for further scope |
| 5 | `job-evidence`: typed ownership, duplicate deletion and exact resume | SE1-SE5, AP0 | Separate worker Goal active at `5035c7a` |

## 1. Core Ownership and Integration

### Subjective (User)

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

User-selected replacement assignment: Core coordinates and reviews; the single
Job/Evidence implementation owner moves to `job-evidence`. Core must not edit
the same implementation concurrently. Separate worktrees prevent accidental
edits, but do not eliminate semantic conflicts. Core performs reviewed Main
integration and cross-lane verification; workers publish only their task branches.
Supervision proposal: while Core is active, inspect worker panes and worktree
diffs at least every ten minutes and at epoch boundaries. Read status/blockers
and relevant tests, then send concrete steering when needed. An inactive Main
session provides no automatic supervision; tmux alone does not supply it.

### Plan

- [x] Transfer the single SE1-SE5 work list and committed prototype recipe to
  `job-evidence`; do not duplicate its implementation checklist here.
- [x] Publish the committed compiler/overlay recipe used by all workers;
  do not copy an unfinished trial or unrelated local changes into their baseline.
- [x] Assign separate worktrees, branches, overlay/build/output paths and task briefs.
- [x] Verify the requested model and `/goal` support, then launch the three tmux
  workers.
- [x] Launch `job-evidence` in its own worktree/window using the requested model,
  verify an active Goal and actual code inspection, and notify all lanes.
- [ ] Verify concrete deletion/test activity at the next supervision checkpoint;
  launch and inspection alone are not implementation completion.
- [ ] Periodically review worker status, diffs, tests and blockers and issue
  directions; record material decisions in the owning SOAP plan.
- [ ] Review cross-owner findings; transfer file ownership for an explicit epoch
  when needed, rather than permanently excluding a necessary large refactor.
- [ ] Integrate each completed epoch, run relevant combined regression gates,
  record unresolved failures, and publish only reviewed commits to Main.
- Completion: workers can deliver independent changes without a second Core
  authority; SE completion remains governed by its original criteria.

## 2. C Backend

### Subjective (User)

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

2026-10-03, English paraphrase: delegate Job/Evidence implementation to a
separate Codex session and specialize this Core session in merges and audits.
Earlier requirements retain Oracle locality, typed construction as authority,
concrete duplication deletion, correct resume and meaningful verification.

### Objective (Code)

Main `64df10d` contains the verified family-cursor epoch and previously reviewed
SE prototype patches. SE1-SE5 is unfinished; the three public split-fuel failures
in its verification report remain unwaived. Unrelated dirty accepted-source and
test changes are not part of the worker baseline.

### Assessment

Agent implementation-workflow decision within the user's scope: create one
`job-evidence` worktree/window and keep SE1-SE5 as its sole active work list.
Its Goal brief is a handoff, not a replacement architecture or second checklist.
Core reviews owner changes, interfaces, tests and resulting deletions before
merging; the performance worker does not independently modify the same owners.

### Plan

- [x] Start from committed Main in `parallel/job-evidence-20261003`; provide
  separate build/output paths and the existing prototype overlay recipe.
- [ ] Read the current owner code, select a concrete deletion epoch and implement
  it alongside focused verification, rather than postponing coding for more logs.
- [ ] Keep semantic, scope, effect, synthesis-first `::`, fuel, ordinary-result
  proof and persistence gates; report inherited failures without waiving them.
- [ ] Hand off frozen tested epochs with exact files, applied source/test deltas
  and known blockers. Core alone decides Main integration.
- Completion: governed by SE1-SE5, not by worker launch or one small deletion.

## Coordination

- Worker Goal briefs: [C](2026-10-03-C-BACKEND-GOAL.md),
  [performance](2026-10-03-PERFORMANCE-GOAL.md),
  [surface](2026-10-03-SURFACE-GOAL.md),
  [Job/Evidence](2026-10-03-JOB-EVIDENCE-GOAL.md). The latest five-session
  assignment supersedes older Main/Sub numbering; this session is coordination
  `core`, while `job-evidence` owns SE1-SE5 implementation.
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
