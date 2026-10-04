# Job/Evidence Implementation Handoff

Date: 2026-10-03
Status: bounded E1-E4/E6-E23 prototypes verified/Main integrated; E24 private broad pending, SE1-SE5 unfinished.
Branch: `parallel/job-evidence-20261003`.
Producer checkpoint: `64df10dfb6caf69226cebb401b88714541334768`.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
[single SE1-SE5 work list](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md).

## Problem List

1. Complete actual Job/Evidence duplication removal without rebuilding another
   program/derivation graph, changing meaning or postponing implementation for logs.

## 1. Single Implementation Owner

### Subjective (User)

2026-10-04, English translation of human workflow clarification relayed by
inquiry desk019ebfae-06be-7b71-974a-b97505daed4a after C24 publication:
"I will speak directly with that session; tell me how to enter it." The desk
identifies the original Job/Evidence session and supplies navigation. This was
immediately recorded in central coordination Problem1 Subjective.

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

2026-10-04, E23 READY675501ba exact33/a19b595a/transporte9faf892 verified,
source156/actual471/raw28/control7/canonical2/public52/all35-fourcuts exact.
Worker private E22 broad0/C0/strict1 retained, original private-parent label
corrected without source changes. Fresh current E22+MEM9 source128c78b71d7:
30 expected producer records, two current parent retention assertions134, four
other owner guards0, candidate full affected O2/SAN0. Public52/full TSV exact
E22/strict3 retained. C28 downstream registration0/two373 phases/48 C-H exact.
Taskc3f3bcd7/prototype Main2e1561c4 pushed/all33 bytes exact,
E19/E20/E21/MEM9/C27/C28 and original Job HEAD/index/pane preserved.
[E23 receipt](../src/prototype/solver_inputs/joint_verification/e23-main-integration-review.json).

2026-10-04, E22 READYf4b884b6/correctionca3f2c98 exact31/233fa25a/transport813b2762
verified on separate E18 task. Source156/actual471/raw28/control7/canonical2/
public52/all35-fourcut census exact, isolated broad0/C0/strict1 retained.
Fresh currentE21+MEM9 source1285235ee3c:30 expected producer and33 C27 records,
381 C-H/public52/full TSV exact E21. Task69ceadfa/Mainb060684f
pushed/all31 bytes exact, original Job HEAD/index/no input unchanged.
[E22 receipt](../src/prototype/solver_inputs/joint_verification/e22-main-integration-review.json).
E23 focused original11/corrected12 payloads and three source156/raw23/delta2
verify: two repeated1024 copied-map arena assertions134, candidate full Source/
six boundaries O2SAN0; initial helper-name setup1 and label correction retained.
[E23 focus](../src/prototype/solver_inputs/joint_verification/e23-focused-failed-capture-root-review.json)
is isolated private E22/source-only inspection, full READY/current pending.

2026-10-04, superseding E21 READYf1cf58f0: immutable exact33/02fb2e4a
transport061c0618 independently verified; all four claimed canonical digests
reverse published E17 and reconstruct tested files exactly, resolving earlier
default-diff representation mismatch. Source156/actual471/raw15/full isolated
broad0/C0/five all35-fourcut census/public52 exact E17, strict3 retained. Fresh
current E20+MEM9 source128b532b759 fifteen expected O2/SAN semantic/synthesis/
Source/normalization/denied-allocation/public gates pass; C26 downstream22/0,
public52/full TSV exact E20. Task221cd5f9/Mainead378b3 pushed/remote
exact33; original Job HEAD/index unchanged, separate E20 canonical preserved.
[E21 receipt](../src/prototype/solver_inputs/joint_verification/e21-main-integration-review.json).
E22 separate E18 focused14/source156/raw23 verified: five parent134 assertions,
completed prepare existing rejection0; full Source/six copied-owner boundaries
O2SAN0, all156 reconstructed from delta. Initial missing migration preflight1
retained. [E22 focused receipt](../src/prototype/solver_inputs/joint_verification/e22-focused-source-admission-root-review.json);
full READY/current qualification pending, no Root Job input/resume.

2026-10-04, notice04aa22ec confirms prior E18 release receipt consumption;
E19/E20 exact READY freezes unchanged and independently already Root delivered.
E21 isolated broad0 is newly reported, not a full frozen/current composition
claim. No new Root Job input/resume; supplementary/final transport pending.

2026-10-04, Root E20 corrected26 taskf4f18402/prototype Mainfd91e8e0 pushed
and remote exact on separate E17 branch; original Job HEAD/index preserved.
Worker source156/actual471/raw11 and five all35/fourcut census verify; isolated
unskipped broad0/C0 and public52/full TSV equal E17, original strict3 retained.
Fresh current E18+MEM9+E19+E20 source12851020c04: nine expected records, full
semantic/synthesis/source-checkpoint O2/SAN0; public52/full TSV equal qualified
E19. C25 downstream20/0, rebuilt backend/four helpers and affected O2/client-
source SAN/link/I-O19; prior generated C/H bytes exact.
[Integration receipt](../src/prototype/solver_inputs/joint_verification/e20-main-integration-review.json)
records exact25 Main metadata plus synthesis canonical context reconciliation;
existing named-field bridge vectors/E19/MEM9 remain. Original scope1, first25
freeze/transport, notice-only digest correction and Root private reverse/context
setup failures retained. E21 focused10/source156/raw6 parent assertion134 versus
candidate O2/SAN0 verified at isolated E17 only. Four Root projections reproduce
tested files, but reported new patch digests need final transport verification.
[E21 receipt](../src/prototype/solver_inputs/joint_verification/e21-focused-owner-counterexample-root-review.json).

2026-10-04, Root E19 exact29 task02052bfc/prototype Mainaea14873 pushed/remote
exact on separate E16 branch; original Job HEAD/index unchanged. Frozen156/
overlay471/raw11/canonical migration/public52 and five all35/fourcut census
verify. Current E18+MEM9+E19 source128add4e096: parent134 retained, candidate
full semantic/synthesis/source-checkpoint O2/SAN0; public52/full TSV exact,
original strict3. Fresh C24 downstream10/0, applied50/source46/nested40 each
O2/client-source SAN and link/I-O19;26 parent C/H bytes exact.
[Integration receipt](../src/prototype/solver_inputs/joint_verification/e19-main-integration-review.json)
supersedes prior bounded E19 freeze/current/publication pending status.
Historical E17 owner codec private38/copied128/Root138/17 semantic-build0
records verify;16 resaves/inert3 failures recomputed, no latest causal repair.
[Codec receipt](../src/prototype/solver_inputs/joint_verification/codec-mem6-e17-owner-counterexample-root-review.json)
is durably routed to original Performance, preserving reusable results.

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

2026-10-04, Root E23 bounded integration decision: delay permanent Source
transport allocation until caller jobs[] membership/uniqueness passes. Runtime
6+/3-, regression6+/2-; repeated1024 rejected pending/completed mappings preserve
graph block head. Later owners[]/record failure allocations remain outside scope.
Early count bounds preserve prior refusal, no new index/graph/pool/authority/
schema/fuel/evaluator/readback. Quick cumulative-18384/captured-48 are observations,
not attributed whole-workload RSS/peak/time gains. Full SE/AP/current broad/
acceptance/strict3/public frontier/codec/actual costs/adoption remain; E24 private
broad work distinct/unreviewed. No Root Job inbox/input/resume.

2026-10-04, Root E22 bounded integration decision: reuse existing canonical
pending-owner membership for Source capture/mapping, replacing duplicate tag
checks. Runtime6+/2-, permanent tests47+, standalonecontrol15+ (original16+
notice superseded). No new index/graph/factory/authority/fuel/evaluator/readback.
Five current parent admission gaps reproduced; completed prepare already refuses.
External Quick cumulative-88320 is observed, not causal/net/peak/time gain.
E23 defers permanent record allocation until mappings pass, only covers that
failure path; full/current qualification pending. Strict3/codec/SE/AP/full current
acceptance/cost/adoption remain. No new original Job inbox/pane input or resume.

2026-10-04, Root E21 bounded integration decision: release detached waits into
existing free_waiters and reserve only shortfall before validated schedule
publication; same wake/preparation order, no new allocator/index/field/authority.
Current denied-allocation preserves headers/queue/proofs/steps/saved bytes;
retry/256 reattachments allocate no owned-arena storage and retain five charged
steps. Quick cumulative+18000/captured+48 are retained, not net RAM/time gains.
All33 Main bytes exact because old four canonical patches equal isolated E17;
E20/context bridges unchanged. E22 replaces tag-only Source gates with existing
canonical owner membership; focused result alone gives no continuation/codec/
public-frontier/full SE-AP/cost/adoption closure. Original failures retained.

2026-10-04, Root agent E20 integration decision within existing scope:
remove two persistent Match scan counters, store their16 bytes in existing active
scratch header/FAM. Same24-byte frames/order/growth/pop and zero/split/cancel/
checked receipt lifetime; overflow guard includes header, failed realloc retains
owner. Source only+19/-15, no new pool/index/graph/authority. Branch168->152 is
layout, not net/live/peak/time gain; five external cumulative aligned deltas are
-32/0/-96/+104336/0, retain QuickSort increase. Main canonical uses E18 projection
plus E20, with already-tested named-field bridges separate and unchanged; task
canonical remains isolated E17 provenance. Full current broad/acceptance/strict3/
codec/SE/AP/actual costs stay open. E21 frees detached waits into existing pool;
256 inert reattachments check block-head identity/steps/proofs/bytes then five-step
trace. Focused qualification does not establish current defect or readiness;
final exact transport/canonical digests and wider/current checks remain required.
Keep original Job pane free for direct human conversation, no new Root input.

2026-10-04, Root bounded E19 decision: existing canonical pending-owner
membership replaces weaker tag-only target admission before any revalidation
mutation; permanent copied pending/completed atomic controls pass. Runtime +2/-1,
test27+ plus header, no new authority/index/fuel/codec. Separate E16 task keeps
submitted bytes and current Main retains E17/E18/MEM9/C24. Isolated broad/C/
census is distinct from fresh current focused/affected gates; full current broad/
acceptance and SE1-SE5/AP/strict3/codec/actual costs remain open. External Quick
cumulative aligned+27696 is retained, not a net live/peak/time claim. E20 separate.
Leave original Job pane free for the human; no resume/input or consumption claim.

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
