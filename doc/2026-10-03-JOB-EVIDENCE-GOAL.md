# Job/Evidence Implementation Handoff

Date: 2026-10-03
Status: active implementation; epoch 1 verified, SE1-SE5 unfinished.
Branch: `parallel/job-evidence-20261003`.
Producer checkpoint: `64df10dfb6caf69226cebb401b88714541334768`.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
[single SE1-SE5 work list](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md).

## Problem List

1. Complete actual Job/Evidence duplication removal without rebuilding another
   program/derivation graph, changing meaning or postponing implementation for logs.

## 1. Single Implementation Owner

### Subjective (User)

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

2026-10-03, English paraphrase of the later human clarification relayed by
inquiry desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: delete unnecessary
mechanisms/work for speed, without additional tuning complexity. Memory needed
for intermediate computation and resume is acceptable; roughly order-of-magnitude
excess versus other systems is not. Preserve indispensable frontier/checked
facts while identifying redundant graphs/copies/dead transient state. Account
new index/owner overhead and demonstrate net memory/time effects with meaning,
fuel and resume preserved. No unsafe borrowing, discarded necessary progress,
hard 10x target or overlapping heavy benchmark is approved.

2026-10-03, English paraphrase of the human requirement relayed by inquiry desk
thread `019ebfae-06be-7b71-974a-b97505daed4a`: improve both speed and peak memory
through software/design changes. The reported AP tree peak of about 1847 MiB
versus native checkers' about 97-529 MiB is excessive. Coordinate with performance
to pin allocations and retained owners and delete redundant graphs/copies.
Require matched wall/RSS and semantic/fuel/resume gates; record/file sizes do not
establish RAM gains. Preserve safe lifetimes; do not use unsafe borrowing or add
wrappers merely for profiling. Continue qualified epochs; no heavy rerun is
requested by this status inquiry.

2026-10-03, English paraphrase of the latest direct user clarification:
performance and Job/Evidence reduction/deletion are tightly coupled and need
concentrated joint verification. C remains important downstream: `.a` plus
LinkerScript emits C usable from other C modules, without undue scope expansion.
Preserve checking when measuring improvements.

2026-10-03, English paraphrase of the latest explicit decision: specialize this
Core session in merges and audits; move Job/Evidence simplification into another
Codex session because implementation has progressed too slowly here.
This supersedes Core's previous implementation assignment, not the design goals.

Earlier user requirements, English paraphrases: retain Lambda/Application/Ref
Core and Oracle-local structures rather than enumerate them again in Job or
Evidence. Prefer existing typed construction/witness Terms as accepted authority.
Unfinished checking may need a cursor, but resume does not justify duplicate
Terms, derivation graphs or wrappers. Audit and concrete deletion must progress
together. Preserve Core/type separation, post-synthesis-only `::`, effect order,
ordinary function-result proofs and Solve/fuel meaning. Keep backend conventions
downstream; do not extend `.a` for Transpiler/Linker needs.

Earlier workflow requirements: independent worktrees, `/goal`, `6.1 Sol` at
`xhigh`, periodic Core supervision. Workers may publish their task branches;
only Core merges Main. Prototype integration is not accepted-source promotion.

### Objective (Code)

2026-10-03, Root review of corrected E11 task `d22be9f` (exact25, pushed and
remote verified): independent assembly matches all156 runtime/test records;
three fresh lifetime/real escaped-frame O2/SAN controls pass, with the original
unsafe counterexample retained. Worker full unskipped O2 acceptance/semantic/
seven-checkpoint terminal0 is separate from focused qualification. Root now
verifies all469 inputs relative to the tested corrected overlay and all14
inherited migrated scripts equal E9. The initial Root check used the repository
root instead of this tested overlay; it did not establish input drift.
Current E9+E10+corrected E11 joint qualification remains pending. Corrected-parent
E12 proceeds independently; original unsafe E11/E12 remain unqualified.

2026-10-03, worker epoch 1 on `5035c7a` / producer `64df10d`: actual module
export/stage and module/reference result copies removed. Full O2 acceptance,
semantic/seven checkpoints, focused ASan/UBSan/leaks and baseline C gates pass;
fresh assembly matches the tested sources. Corrected ordinary QuickSort theorem
census is DONE on both producers at 766,477 dispatches. Preserve the earlier
REJECTED 61,489-step workload/fixture and failed incomplete-provider retry.
Three original strict public resume failures and all parent image bytes remain
unchanged. Epoch 2 is separately verifying further owner-copy deletion; SE4
policy/progress diagnostics use ordinary Solve and inert untrusted loading.

Committed Main `64df10d` includes reviewed SE patches, the family scratch-pool
epoch, Surface epochs and C-backend epochs 1/2. The accepted semantic baseline
remains `eb0aad673dd0fb5219eb0d720d45a819cc50edba`; worktree `src/` supplies the
clean committed inputs, not Core's unrelated dirty source/tests.

The [family verification](../src/prototype/solver_inputs/family_cursor_verification.tsv)
records full strict O2 acceptance, focused/sanitizer and combined gates passing.
SE1-SE5 is not complete. Three unwaived public partition failures remain:
1000+1000 and 1600+1600 reloads stay pending unlike uninterrupted completion;
terminal 1921+0 has matching bytes but still reports pending. Restricted
checkpoint success does not establish public frontier correctness.

Existing `synthesis_work.h` is already a shared scheduling header with private
class state, not the original giant semantic union. Evidence already borrows
many typed inputs. Do not presume every remaining receipt or cursor redundant.

Latest Core inspection, agent hypothesis for revalidation: selected modules and
source references still copy a final rule result, and modules copy export-scope
metadata. Existing projected-output readers may support borrowing those owners.
Import projection, namespace-without-Term behavior, pending discovery, unselected
definition checks, failures and real quotation proofs must remain correct.
This is a candidate deletion, not user approval of a solution or the full Goal.

### Assessment

2026-10-03, Merge operational schedule: coordinate the pending MEM1 attribution
and scaling epoch with performance after current qualification. Existing SE
owners retain responsibility for the resulting safe deletion; this adds no
second owner/work list. Account new index/ownership storage, preserve needed
frontier/checked facts and distinguish cumulative/retained layout from peak RAM.
The rejected E11 lifetime control remains a required safety boundary. Current
corrected E11 focused work continues; matched wall/RSS needs a later exclusive
slot and is unmeasured here.

2026-10-03, Core operational coordination: deliver exact tested canonical
snapshots for joint captured-head/readback integration on common producer, inputs
and progress. Core is assembling family/Surface/performance gates. This worker
does not edit eval/readback; retained-copy deletion differs from peak memory,
traversal and time. Joint cost measurements need an exclusive slot. Surface is
intentionally stopped; its completed branch is preserved.

Core operational assignment: this worker is the sole SE1-SE5 implementation
owner. Core performs design review, combined verification and Main integration;
it no longer edits these owners concurrently. Keep the existing SE work list;
this document is a handoff and scope contract, not a second architecture plan.

Choose a concrete deletion after reading current producers and consumers, then
implement focused controls promptly. Report unsupported architectural assumptions
with evidence; do not preserve a duplicate merely to limit migration size, or
delete a real checking obligation because its result resembles a Term edge.
Larger coherent changes are allowed within producer scope, with tested epochs.

### Plan

Use the existing SE1-SE5 checkboxes and completion criteria. Before each handoff,
report exact frozen files/hashes, applied implementation/test/doc deltas, tests,
inherited/new failures, next concrete action and cross-owner needs. A small epoch
is progress, not full Goal completion; finish only against the SE criteria.

Route MEM1 owner/copy/lifetime findings from performance into that same SE list
after current qualification. Implement the simplest safe deletion and include
new overhead; matched time/RSS plus semantic/fuel/step-zero/split-resume evidence
is required before claiming a memory/time gain. The performance Goal owns its
pending measurement work list; current storage counts alone do not complete it.

Focused verification should cover the removed storage's actual owner/readers,
pending and checked inputs, errors, warm sharing, zero/split fuel and cancellation.
Before integration run relevant regression/acceptance, sanitizer, ordinary-result
QuickSort/general Sorted, persistence and split-fuel controls. Keep inherited
public failures visible until fixed; do not turn failures into expected passes.
Coordinate heavy runs; correctness builds use at most `-j2`, wall/RSS comparisons
need an exclusive slot. Do not rerun every unrelated gate after every edit;
broaden verification at the stated reviewable epoch boundaries.

## Work Contract

- Write implementation/tests only under `src/prototype/solver_inputs/`, including
  detached disposable trial trees. Update the existing SE plan and this brief
  concisely in place. Use `apply_patch` for manual edits and repository C style.
- Producer persistence dependencies may require `src/prototype/artifact_persistence/`;
  report the specific ownership need to Core before touching a shared module.
  This is coordination, not a prohibition against a necessary larger refactor.
- Do not edit accepted `src/`, `tests/`, Makefile, handmade or other lanes' files.
  Do not edit through symlinks to accepted inputs or use Core's uncommitted trials.
- Performance owns its captured-head/readback/direct-IADT epoch; backend owns
  target lowering/runtime/LinkerScript; Surface owns its grammar/migration epoch.
  Route cross-owner findings through Core instead of solving them in both lanes.
- Do not merge, close issues, push Main, promote source or substitute models.
  Task-branch commit/push is allowed after verification. If shared Git metadata
  is sandbox read-only, give Core the exact frozen handoff for delegated publication;
  continue independent implementation rather than attempting a sandbox bypass.
- Record user requirements in Subjective immediately, Core steering/agent
  decisions in Assessment, fresh code/tests in Objective with their revisions.
  Preserve failures and superseded decisions; avoid repeating command transcripts.

## Reproducible Starting Point

Run from the task worktree. Keep generated outputs private and untracked:

```sh
ARTIFACT_SOURCE="$PWD/src" bash src/prototype/solver_inputs/overlay.sh \
  "$PWD/src/prototype/solver_inputs/job_evidence_baseline"
make -f src/prototype/solver_inputs/job_evidence_baseline/src/Makefile \
  BUILD="$PWD/src/prototype/solver_inputs/job_evidence_baseline/build" \
  -j2 "$PWD/src/prototype/solver_inputs/job_evidence_baseline/build/synthesis_test"
src/prototype/solver_inputs/job_evidence_baseline/build/synthesis_test
```

The overlay creates symlinks for unchanged inputs. Detach the particular file
before a manual edit; do not modify accepted sources through them. Regenerate
canonical patch files mechanically from their documented base after trial tests.
Artifact gates use `src/prototype/artifact_persistence/build.mk`, private
`OVERLAY`/`BUILD`, and that overlay's `checkpoint_tests/` and `artifact_tests/`.
The SE README and pinned verification reports identify the exact existing gates.

For Goals lifecycle reference, see the
[official OpenAI documentation](https://developers.openai.com/cookbook/examples/codex/using_goals_in_codex).
Actual model/Goal activity must be observed at launch, not inferred from this file.
