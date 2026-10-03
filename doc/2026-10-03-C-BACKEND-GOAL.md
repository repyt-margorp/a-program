# C Backend Goal

Date: 2026-10-03
Status: in progress; session `c-backend`, branch `parallel/c-backend-20261003`.
Baseline: committed `eb0aad6` compiler/overlay plus the coordination documents.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
[AP4-AP6](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md), open #61
(supersedes historical #44/#49).

## Problem List

1. Implement and verify a coherent downstream C realization epoch without
   moving target conventions into A Program semantics.

## 1. Downstream Realization

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

2026-10-03, English paraphrase of the human coordination concern relayed by
inquiry desk `019ebfae-06be-7b71-974a-b97505daed4a`: the user asks whether Merge
is already done and says progress/coordination are unclear. Show the completed
backend integration and the remaining publication/release steps distinctly.
This feedback grants no new representation or erasure policy; native QuickSort
remains unfinished. Root's concrete scheduling release belongs in Assessment.

2026-10-03, concise English paraphrase of the direct human workflow replacement:
the visible parent becomes the user inquiry/report desk. Separate Merge/audit
session `01a100b3-1d83-7090-bbf6-62544c39ec4b` inherits coordination, exact task
publication and Main integration. Continue the current Goal and frozen handoffs;
use the existing outbox routed to Merge, read the central schedule at Main
`4d1d941`, and keep one issue-linked status table. The fourth read-mostly audit
lane for #59/PR60 grants no accepted-test deletion, relaxed outcomes or new
authority. Merge owns measurement slots and summarizes upward. Do not load,
resume or fork another session; notification evidence grants no new approval.
This supersedes the earlier parent/Core merge-owner assignment. Recorded first
in the separate routing note during C7 freeze and incorporated after release.

2026-10-03, English paraphrase of the direct human clarification: issues are
accumulating and progress is hard to see. Each worker should give concrete
issue-linked feedback showing which subproblem advanced and how much actual
implementation/verification was done. This does not make a table format or
reporting cadence a user design principle; Core's workflow is in Assessment.

2026-10-03, English paraphrase of the user coordination preference: Core should
be woken by worker events rather than continuous polling. Keep this worker Goal
active and continue downstream ABI/helper work without a pause. The later direct
clarification specifies notification OR a six-hour timer, with a check even
without notice; this supersedes a notice-only interpretation. Payload format,
owner routing and relay mechanics are Core operational decisions, not user design
principles. Recorded during the Epoch4 freeze in a separate addendum and now
incorporated after Core released that freeze.

2026-10-03, English paraphrase of the latest direct user clarification:
performance and Job/Evidence need concentrated joint verification. The C backend
is an important downstream project: use `.a` and LinkerScript to emit C readily
usable from other C modules, connecting A Program to the external world and
eventual generic assembler lowering. It has no authority over A Program. Avoid
excessive deep or scope-expanding investigation. This narrows the earlier full
coverage direction; keep target work bounded and coordinate upstream dependencies.

2026-10-03, English paraphrase of user authorization, relayed by Core: own
task-branch commit/push is explicitly permitted; only Core merges Main.

2026-10-03, English paraphrase of the latest direct user clarification: each
worker may commit/push its own task branch. Only Core merges results or updates
Main; workers must not self-merge, promote or close issues. Publish verified,
reviewable epochs with test evidence and report branch/commit, changes, failures
and integration needs to Core. Continue within the prototype write scope. This
directly confirms the earlier relayed authorization; integration/promotion
boundaries remain. Recorded first in a separate Goal addendum during the Epoch3
freeze and incorporated here immediately after Core released it.

2026-10-03, English paraphrase of the explicit user role decision relayed by
Core: Core now specializes in design audit, combined verification and Main
merges; Job/Evidence implementation belongs to its sole worker. The current
target-only C epoch continues. Task-branch publication remains authorized;
only Core integrates Main, with no accepted-source promotion.

2026-10-03, English paraphrase of the continuation request: implement and
verify AP4-AP6 and critically check #44/#49 against this committed worktree.
Keep code prototype-only and downstream-only, without extending `.a` for target
needs; deliver tested, reviewed branch epochs and concise SOAP evidence to the
core coordinator. Do not push Main or merge/close issues. The full Goal remains
active until its requirements are verified.

2026-10-03, English paraphrase: Sub1 develops C-backend design in a separate
tmux Codex session, with `/goal`, `6.1 Sol` and `xhigh`. Main supervises and
steers. Earlier requirements: human-usable C modules are a target tradition;
LinkerScript owns ABI/layout, and `.a` must not grow for backend-only needs.
2026-10-03, English paraphrase of the active Goal: start C-backend/Linker
refinement now, while the coordinator continues Job/Evidence removal. Use task
names, separate directories and reviewed integration epochs.

### Objective (Code)

2026-10-03 19:29 UTC, fresh Root current-producer review and integration: C14 task
e9d74f7/Main6c36dcb are pushed/remote exact; all implementation/tests are
frozen-byte exact, only Goal provenance reconciled. Source12812026914 callback
O2/client-source SAN39 each, shared58 each, link/IO19 and four corrected
explicit-import source consumers pass. [Root receipt](../src/prototype/c_backend/verification/core-epoch14.json)
records original historical-image IO mismatch, its pre-C14 reproduction and
corrected matched current image. No assertion/runtime was adapted.

2026-10-03 15:29 UTC, superseding Root C14 source-fixture review at Main4853cb6:
the original named/thunk/lambda consumers omitted explicit `import once32;`.
`--imports` supplies provider exports for explicit import declarations, rather
than injecting them into lexical scope. Root verified all16 worker evidence
hashes and freshly ran all four corrected consumers on unchanged runtime128
73fa86c9/pointer407e8f5e/providerd3c37ff5: all0, exact outputs and step counts.
Report: `/tmp/a-program-merge-c14-imported-source-controls-20261003/Root-results.json`
(d7575c61). Original three reject1 results remain historical evidence at
`/tmp/a-program-merge-c14-source-probes-20261003/Root-results.json`; they do not
establish a frontend callback defect or require any source-policy change.
C14 exact23 notice8283bd1b is READY, pending separate Root ABI/current-producer
review. Latest C pane is capacity-stalled; no owner restart or model change.

Fresh C16 at C14 implementation `e9d74f70` plus owned target edits, now on the
observed C15 task `ba5d39d1537ffa93770d1705603e4bac37ab14ea`: distinct
`c_callback2_v1` permits borrowed same-width unary/binary pure-total scalar
inputs using existing independent Pi views. Worker qualified E8 strict O2 build
and O2/client-source SAN48 each pass, with 4000 manual and 400 Core comparisons
per product, source20, inert emission, both header orders and sixteen retained
checked/trusted refusals. Affected unary39/module99 O2+99 SAN/I/O19 pass. Original
unary binary refusal remains 4. Exact detail stays in the
[C16 note](2026-10-04-C-BACKEND-CALLABLE-SHAPE-NOTE.md) and separate handoff;
current-producer qualification and publication remain Root work.

Fresh Root C16 current-E15 review, 2026-10-03: source1283f48e22e, rebuilt
backend and Core/inert controls pass binary48 O2 +48 client/source SAN, unary39
each, module99 each, shared58 each, linker and matched current-image I/O19.
All input pins remain exact. Taska4a2e98 is pushed/remote exact; prototype Main
merge is reviewed separately. [Root receipt](../src/prototype/c_backend/verification/core-epoch16.json)
records limited sanitizer scope and original sentinel134/source-archive limit.
No accepted source, producer/schema/authority, actual cost or general callable
completion claim. Frozen worker E8 evidence above remains separate.

Fresh C15 at exact task `e9d74f70` plus owned edits: a generated scalar provider
supplies borrowed callback interpretations to two independently emitted callback
consumers. Sixteen product pairs/both header orders, forty clients per phase,
800 Core comparisons/client and twenty source observations pass in serial O2
and client/source ASan/UBSan/leak modes. Each phase has 99 expected status rows,
including source/object/whole-archive duplicate-symbol refusals. Explicit loaded
provider handles close after all synchronous calls. No emitter/producer/schema
change or broader/timing run; worker E8 and Root E12 evidence stay separate.

2026-10-03, fresh Root C15 qualification at current E14 Main376fab18:
producer source128abedf677, frozen C14 backend and unchanged E12 Core oracle
pass O2 and client/source SAN99 expected rows each. All sixteen product pairs,
both header orders, provider lifetime and three duplicate refusals pass with
input pins unchanged. Exact8 taskba5d39d and prototype Maind219fca are
pushed/remote exact. [Root receipt](../src/prototype/c_backend/verification/core-epoch15.json)
records producer pins and sanitizer coverage. C16 remains private and excluded.

Frozen worker C14 evidence (its imported-source boundary statement is
superseded by the corrected explicit-import review above):

Root report, 2026-10-03: C13 exact11 task
`96a0308283dad19450c5c56f2ac0ca3463c4838b` is pushed/remote verified; prototype
Main `f7afb04`/status `e552f2c` are pushed/remote exact. Current E9+E10 strict
build, shared O2/client sanitizer 58+58 rows, original linker and nineteen I/O
controls all pass, with exact public definitions 3/4/17/8. Root reconciles Goal
provenance/conflicts separately. Worker C14 live edits are preserved.

Fresh C14 at task `96a0308` plus owned edits: strict O2 callback controls pass
for source/object/archive/shared products, including 400 Core-evaluator
comparisons per product, 20 same-module source observations, inert emission and
twelve checked/trusted refusals. Focused client/source ASan/UBSan/leak controls
also pass; each phase has 39 status rows. Affected shared, original linker and
nineteen I/O controls pass, with 36 old component/header/map files byte-identical
to C13. Explicit same-module thunk arguments admit;
the initial imported differential and three focused named/thunked/inline cases
reject (1) on qualified E8. A further imported existing thunk also rejects.
Root independently reproduces all three initial probes (1) on current E9+E10,
pinned in `/tmp/a-program-merge-c14-source-probes-20261003/Root-results.json`,
and routes readonly review to the sole Job owner. Preserve these unresolved
fixture/frontend boundaries; no producer defect or imported source applicability
is claimed. The initial output comparison failure from literal backslash-n
separators is retained and corrected to actual matching separator bytes.

Fresh C14 private typed-view probe at C12 task plus immutable C13: ordinary
`higher` admission succeeds in 4225 steps on the qualified E8 image. Existing
views show a thunk of unary Int32 Pi, independent codomain and pure TOTAL Int32
result; the exported outer function also has a pure TOTAL result. The probe
build/run both exit 0, retained under
`/tmp/a-program-c-backend-epoch14-callback-probe-20261003`. This proves this bounded
classifier shape exists, not general callback/Acc lowerability. Agent choice:
implement a separate opt-in borrowed same-width unary scalar callback profile;
keep existing profiles/refusals and all source admission unchanged.

Fresh C13 on exact C12 task `97f8d7c` plus owned edits: explicit shared C product,
PIC objects, selected public symbol map and receipt are implemented. Strict
backend O2, new O2/client ASan/UBSan/leak gates, affected existing linker gate and
all nineteen I/O controls pass on qualified E8. Each shared phase has 58 status
rows, sixteen client runs and exact dynamic public definitions. Snapshot/evidence
and client-only sanitizer limits are in the
[shared-library plan](2026-10-03-C-BACKEND-SHARED-LIBRARY-PLAN.md) and
[C13 handoff](2026-10-03-C-BACKEND-EPOCH13-HANDOFF.md). This is bounded #61 target
product progress, not native sorter or whole AP5/AP6 completion.

Root fresh C13 review, 2026-10-03: qualified current E9+E10 runtime12873fa86c9
unchanged before/after; strict O2 backend, shared O2/client ASan/UBSan/leak,
original linker and nineteen I/O controls all pass. Shared phases each58 rows
and exact3/4/17/8 definitions. Backend/library bodies stay O2. Exact11 task
96a0308 pushed/remote verified; no accepted promotion or native sorter completion.

2026-10-03 13:49 UTC, Root review at Main `b461ace`: exact C12 task97f8d7c
and Main370a986 complete, current-E9+E10 fourteen O2/three affected client
sanitizer gates/nineteen I/O controls0; [Root evidence](../src/prototype/c_backend/verification/core-epoch12.json).
The verified audit closes historical #44/#49 as superseded by open #61, retaining
source/native residuals and this owner. Separate C13 exact11 notice49bf8ae9 is
now qualified and integrated as prototype Mainf7afb04; see
[Root C13 evidence](../src/prototype/c_backend/verification/core-epoch13.json). Neither native Acc/QuickSort
completion nor accepted C promotion follows.

2026-10-03, Root current-producer C12 review of exact task `97f8d7c` on
E9+E10 runtime128 `73fa86c9`: all fourteen strict-O2 backend targets pass,
including the new record-List gate and both former negative shapes as explicit
positive clients. Root verifies all128 producer and pinned backend inputs remain
unchanged. Fresh affected client/source sanitizer and I/O controls follow;
worker E8 qualification remains separately attributed.

2026-10-03 13:06 UTC, Root fresh qualification of exact C11 task `56e4ae875`
against Main `09ee775` and current E9+E10 runtime128 `73fa86c9`: all thirteen
strict-O2 backend gates and nineteen publication-I/O status controls pass.
The I/O controls use a freshly generated current-producer image, retaining
unsupported 4, close/receipt status 2, atomic cleanup and prior-output checks.
Worker C9/E8 before/after reproduction and client sanitizer results remain
separate historical-build evidence; no new sanitizer or wall/RSS run is claimed.
Root evidence stays at `/tmp/a-program-merge-c11-e10-gates-20261003`.

2026-10-03 Merge verification at `091669f`: frozen C7 task `d275b75` is published,
all fourteen files match and eleven fresh strict-O2 gates pass on canonical
E6/runtime128. Root log/binary/source pins are in
[core-epoch7.json](../src/prototype/c_backend/verification/core-epoch7.json).
Enum arrays: 875 boundary cases per product/24 source observations. Worker native
sanitizer reports remain separate. Prototype integration is complete; accepted
promotion/native Acc QuickSort and broader contracts remain open. This supersedes
the older pending-C7 publication/joint status below without changing its history.

Fresh independent C12: already-selected finite value records now have the
single-tail List/array target contract. Ordinary admission and old checked/trusted
status-4 refusal were proved before the guard/validator-order change. Focused
record-List, affected value-record and enum-List O2/client sanitizer gates pass;
C11 publication controls still pass. Source Match/sum/append preserve whole
record fields; unsupported record shapes remain explicit. Exact pins are in the
[record-List plan](2026-10-03-C-BACKEND-RECORD-LIST-PLAN.md) and separate handoff.
C11's seven submitted entries remain exact in `epoch11-submitted-snapshot.tar`
SHA256 `af0729f2998856df06a78eb80f5da74368e0f409d0da70ccf2c032e12484c318`;
only this live owning status advances independently. Root's later C10 report
confirms task `fe497956` and prototype Main `2753441`/status `0791e799` pushed
and remote exact. This is coordinator evidence, not a worker Main operation.
Root later reports exact C11 task `56e4ae875a18f336fff2262925472158a0d5d249`
pushed/remote verified with live C12 edits preserved. Root's Main `545baa62` / publication `6988deb` and thirteen current-E9+E10
backend/nineteen I/O controls are complete and remote verified; these are
Root fresh tests, distinct from the worker's old-producer controls.

Fresh C11 at `fe497956c3d0031a0db0c98a13db5c464fcd2a8b` plus owned edits:
source/header/direct publication stream errors now return I/O status 2. The
focused runner passes nineteen status assertions both before (expected old
immediate status 4) and after the fix; close/receipt failures, unsupported 4,
atomic cleanup and prior-output preservation remain explicit. Backend-only
strict O2 uses the exact qualified C9/E8 runtime128; eighteen O2 and eighteen
client ASan/UBSan/leak combinations pass. Generated C/header bytes are unchanged.
Exact files, pinned evidence and sanitizer limits are in the
[C11 handoff](2026-10-03-C-BACKEND-EPOCH11-HANDOFF.md). C10's six submitted hashes
match inspected task `fe497956`; Root independently verified remote/Main
publication at `2753441` / `0791e799`. C11 exact task `56e4ae875` is now pushed
and remote verified; Root's current-producer verification remains pending.

Merge report, 2026-10-03, freshly checked against Git: exact C8 task
`884d5bbe192e128fc98de8f3afcd2d3bce59211d` is committed/pushed, and all 17 old
manifest hashes match that immutable revision. Merge finds the bounded capture
queue/stable order coherent and releases the live freeze for separate work.
That older pending status is superseded: Root twelve current-E8 O2 gates pass,
C8 prototype Main `dea5fc1` is integrated/pushed in `cf8cdf5`;
[core-epoch8.json](../src/prototype/c_backend/verification/core-epoch8.json) pins
fresh source/binary/log evidence. This is prototype integration, not acceptance. C7's exact tested
candidate `091669f` is integrated; Main `66fa707` is pushed/remote verified in
Merge's later report. Historical handoffs/manifests stay unchanged.

Fresh C9 at published C8 plus active prototype edits: the applied selector,
parameter/signature mapping and ordinary C clients are adopted. All thirteen O2
and ten generated-client ASan/UBSan/leak gates are terminal exit 0 on the pinned
worker producer. The new source/test/doc manifest has 34 files, SHA256
`1ceade3b72ffe12c448cd21983e759cc3d67dad004a641abc3ab69e24b83eec6`.
Nat/enum/reversed Lists, a two-parameter value family, mixed C products and admitted
source take/drop/slice are positive; five guarded profiles advance no source work.
Initial incorrect Nat branch arity was corrected in the private fixture without
a producer change. Exact files, source deltas, tests/failures and target limits
are in the [C9 handoff](2026-10-03-C-BACKEND-EPOCH9-HANDOFF.md). This is a separate
task epoch. Root report, 2026-10-03 11:07 UTC: exact task
`60c100c9d06b314eaeb024462e8ce0e3f2da5bab` is pushed/remote verified; thirteen
strict O2 gates pass on qualified E8/runtime128. Prototype Main `48c42eb`
integrates the candidate/reconciled status; Main
`7ca963d4ce27d0cbfa7b31bc598a5dc3ef578b7d` is pushed/remote verified.
Fresh worker readback of that immutable Main finds all 37 code/test/historical
documents identical to the exact C9 freeze; only this owning status differs.
Root's committed `src/prototype/c_backend/verification/core-epoch9.json` has
SHA256 `524a446b189581c17cef7fb694492860a8ab0eec8864c37f18d25d0910c6ea0a`;
its thirteen exit-0 rows, exact E8 runtime128 and frozen38 manifest match were
read, not rerun. Root did not rerun worker sanitizers. No native Acc/QuickSort
completion or accepted C promotion is implied. Historical private evidence stays in the
[continuation note](2026-10-03-C-BACKEND-APPLIED-LIST-NEXT-NOTE.md).

Fresh final C8 at task parent `d275b75` plus the 12-source-file manifest:
the active prototype retains transitive known-function captures, source delta
`+27/-17`. Twelve O2 gates and nine generated-client ASan/UBSan/leak gates are
terminal exit 0. Exact old `nested_three` is positive; original static raw controls
and callback/effect/native Acc refusals remain. Source/product tests, binaries,
style and target limits are pinned in the
[C8 handoff](2026-10-03-C-BACKEND-EPOCH8-HANDOFF.md).
Root published the exact seventeen-file task as `884d5bb`; twelve current-E8
producer O2 gates pass; prototype Main integration follows, not full
Goal completion. Applied-List implementation remains separate private scratch.

Fresh private follow-up at C7 `d275b75` plus isolated scratch edits: selected
applied Nat/enum/reversed Lists and `Choice Nat Bool` pass O2 and generated-client
ASan/UBSan/leak gates for ordinary source/object/archive products. Four admitted
profiles also pass inert emission/unchanged source graph and byte-identical
product checks. The separate capture trial passes 300 raw evaluator comparisons
and the exact former `nested_three` control is positive in its O2 static gate;
callback/effect failures remain. Plans below pin exact counts, hashes, earlier
harness/setup failures and pending regressions. No active lowerer, producer,
schema, task commit or Main change is made by these private trials.

Merge report, 2026-10-03: C7's eleven current-E6 gates are terminal exit 0 on
canonical 128-source exact E6. All 14 candidate files match; native enum arrays
record 875 cases/product and 24 source observations alongside retained native
Acc/QuickSort refusal. Tested private candidate `091669f` is now local prototype
Main; publication/status push follows. Log identifier `02942769` and
`core-epoch7.json` pin Merge's exact source/binary evidence. This is joint O2
evidence, not a worker rerun, independent property count or sanitizer rerun.

2026-10-03, fresh Git inspection confirms C7 task HEAD
`d275b75d246d9795d798ee7771519c0febc6c9bb`; all 14 historical manifest hashes
match that immutable commit. Merge reports completed push, independently matching
remote and live freeze release. That earlier pending status is superseded by the fresh Root C7 result above;
Main publication was completed in pushed `66fa707`. Old C7 handoff and `/tmp` manifests stay unchanged; new Goal edits
belong to the next epoch. The routing note remains historical notice evidence.

2026-10-03, fresh branch/upstream inspection confirms C6 publication
`cea1dc0177d36328d6f4de2a8f151b5952d2c2c5`. All 16 historical manifest hashes match
that Git revision. Core reports push exit 0, independent remote verification and
freeze release. Later Core evidence `fd45c42`: all ten C6 O2 gates pass against
the joint E3 producer (128-runtime manifest abbreviated `e87b8781...`, source
hashes unchanged). Prototype Main merge `53debc870231f415bdf926b537a7c1a8670e81d7`
and push are complete. Worker seven sanitizer gates remain separate evidence.
This is prototype integration, not accepted promotion. The
new issue-feedback note/table is incorporated here after release; the temporary
addendum is removed. Old handoffs/manifests stay unchanged.

Current issue status (2026-10-04 local; detailed pins stay in linked epoch plans):

| Issue/subproblem | Implemented change + exact revision | Local vs joint verification | Task publication vs Main integration | Remaining acceptance/blocker | Next concrete epoch |
| --- | --- | --- | --- | --- | --- |
| #49 native calls/data/List/Nat32 | C1-C5 through `312c2da`: first-order ABI, value data, nodes, numeric partitions, transactional arrays, known local functions. | Local C5: 9 O2/5 native sanitizer gates. Core: 9 combined O2 gates on reviewed 128-runtime-hash snapshot. | Task `312c2da` pushed; prototype Main `435d965` is an inspected ancestor of Core's pushed C6 Main `53debc8`. | Broader native contracts remain; no accepted promotion. | Preserve these contracts in C6/C7. |
| #49 selected value records, C6 | `cea1dc0177d36328d6f4de2a8f151b5952d2c2c5`: whole nested records, active tag validation, emitter receipt metadata. | Local 10 O2/7 native sanitizer gates; 105 cases/product, 16 source observations. Core evidence `fd45c42`: 10 O2 gates on joint E3 producer pass. | Task commit/push/remote verified; Core Main `53debc870231f415bdf926b537a7c1a8670e81d7` integrated and pushed. | Bounded record criterion verified; recursive pointer/aggregate fields and broader AP6 remain open. | Distinct C7 enum List arrays. |
| #49 finite enum List arrays, C7 | `d275b75d246d9795d798ee7771519c0febc6c9bb`: enum copies and validation before arena mutation; all 14 historical hashes match Git. | Local 11 O2/8 native sanitizer gates; 875 cases/product, 24 source observations, 17 inert raw/evaluator observations. Merge reports all 11 joint E6 O2 gates pass; log `02942769`. | Exact task commit/push/remote verified; tested candidate `091669f` integrated, Main `66fa707` pushed/remote verified. | Bounded enum criterion verified; multi-payload/aggregate shapes remain excluded. | Preserve array contracts in the separate applied-List epoch. |
| #49 transitive known-function captures, C8 | `884d5bbe192e128fc98de8f3afcd2d3bce59211d`: finite dependency walk, source delta +27/-17; all 17 historical hashes match Git. | Local 12 O2/9 client sanitizer gates pass; 600 Int32 + seven Int64 cases/product, six source observations and 300 raw comparisons. Root twelve current-E8 O2 gates pass; original raw/callback/effect/native Acc controls remain. | Exact task pushed; prototype Main `dea5fc1` published in `cf8cdf5`. | Bounded capture criterion verified; recursive captures and native Acc remain open. | Preserve capture contracts in later bounded ABI work. |
| #49 selected applied families, C9 | Exact task `60c100c9d06b314eaeb024462e8ce0e3f2da5bab`: selector/parameter/signature wiring; 38-file freeze SHA256 `f0a61e5edd02a55ed8a2cafc2f1aed876537a008ddb68c8c6646cc2a9e9490b8`. | Local 13 O2/10 client sanitizer gates pass; 1093 Nat Lists, 127 enum Lists, 127 reversed Lists, 34 values/product and nine source observations. Five inert profiles/fourteen checked/trusted refusals retained. Root thirteen strict O2 E8/runtime128 gates pass, `core-epoch9.json`. | Exact task pushed/remote verified; prototype Main `48c42eb`, publication `7ca963d` pushed/remote verified by Root. | One instance per erased layout/reference arguments only; indexed/callable shapes excluded. No accepted C promotion. | Preserve unsupported contracts in the focused ordinary-module follow-up. |
| #49 source List slices and ordinary modules, C9 | Exact task `60c100c`: admitted take/drop/slice use generic native Match/known thunks; ordinary C converts arrays between separate nominal products. | Local O2/client sanitizers: 1093 Lists, five take/drop limits and 25 slice pairs/List/product; nine payload comparisons. Nine product pairs/both header orders/shared arena and incompatible pointer refusal pass. Root thirteen E8 gates pass; rows are not independent properties. | Same exact C9 task/Main publication `7ca963d`; historical handoff preserved. | Nat32/depth/storage limits remain; drop reconstructs suffix. Indexed SizedList/callable Acc/native QuickSort remain open. | Preserve these contracts in bounded ordinary-module verification. |
| #49 ordinary C helper aliases, C10 | Exact task `fe497956c3d0031a0db0c98a13db5c464fcd2a8b`; all six submitted hashes match Git. Three test/harness files; no emitter/producer/schema change. | Qualified C9/E8: 18 O2 and 18 client ASan/UBSan/leak combinations; rollback/depth/lifetime and two default-symbol refusals pass. Root verified 6/41/275 hashes, 36 run statuses and 74 argv exits. | Root reports task pushed/remote exact; prototype Main `2753441`/status `0791e799` pushed/remote exact. Owning Goal reconciliation is Root's integration work. | Explicit distinct aliases remain required; no general nominal exchange or fully instrumented object/archive claim. | Preserve historical C10 evidence while advancing independent bounded epochs. |
| #49 publication I/O status, C11 | Exact task `56e4ae875a18f336fff2262925472158a0d5d249`: two publication files save stream errors before closing and prefer I/O status 2. Seven-file historical snapshot preserved. | 19 status assertions before/after; backend-only O2, 18 O2/18 client sanitizer combinations pass on qualified C9/E8. Unsupported 4, cleanup, prior output and generated bytes preserved. | Root reports task pushed/remote exact; Main reconciliation/current E9+E10 review pending. | Linux fault interposition is test-only; backend/object/archive bodies are O2, not sanitizer-instrumented. No broad native completion. | Preserve I/O contracts in the independent C12 epoch. |
| #49 finite value-record List/arrays, C12 (historical milestone) | Exact task `97f8d7c28f3438fb1b36d687e0e7efc175d8c6f3`: List-shaped finite payloads, active validation before traversal/allocation, validators emitted first. | Worker qualified E8: 259 cases/product, 13 source observations, O2/client sanitizers and 24 refusals pass; affected gates and I/O controls pass. Root current E9+E10: 14 O2/3 affected sanitizer gates and 19 I/O controls pass. | Task pushed/remote verified from immutable16; Root reports prototype Main integration complete, status push next. | Recursive aggregates, multi-tail/non-List record shapes, callable/indexed fields and native Acc/QuickSort remain unsupported under #61. No accepted C promotion. | Preserve historical C12 evidence during separate shared-library work. |
| #61 shared C products/visibility, C13 | Exact task `96a0308283dad19450c5c56f2ac0ca3463c4838b`: explicit shared product, PIC objects, selected public symbol map and receipt; separate exact11 snapshot. | Worker E8: shared O2/client SAN each 58 rows, 16 runs, exact 3/4/17/8 definitions; linker/I/O controls pass. Root E9+E10 strict build, shared O2/client SAN58+58, linker/I/O19 all pass. | Task pushed/remote verified; Root prototype Main `f7afb04`/status `e552f2c` pushed/remote exact. | ELF version-script toolchain, code handle lifetime, client-only shared SAN coverage; no install/SONAME policy, general callable/boxed ABI or source authority. | Preserve historical C13 evidence in separate C14 callback work. |
| #61 bounded public callback inputs, C14 | Taske9d74f7/Main6c36dcb exact23 paths; opt-in borrowed synchronous same-width unary pure-total Int32/Int64 ABI, seven implementation files +84/-20. | Fresh Root current-E12 O2/client-source SAN39 each, four products400 Core comparisons each, source20, shared58 each, link/IO19 and explicit-import4 pass. | Task/Main pushed and remote exact; 22 frozen files plus reconciled Goal doc. | Caller purity/totality/lifetime preconditions and limited SAN scope explicit; returned/boxed, mixed-width, multiargument, effects and native Acc excluded. | Continue bounded justified C-module work; no full #61 closure. |
| #61 ordinary generated callback modules, C15 | Parent `e9d74f70` plus three new provider/adapter/harness files, 231 lines; existing callback ABI unchanged, exact8 snapshot. | Worker E8: O2/SAN99 each; 16 product pairs, both header orders, 32 linked +8 loaded-provider clients/phase, 800 Core comparisons/client, source20 and three duplicate refusals pass. Fresh Root E14 source128abedf677: O2/client-source SAN99+99 match with unchanged input pins. | Exact8 taskba5d39d/prototype Maind219fca pushed/remote exact. C14 immutable snapshot preserved. | Valid provider success and synchronous context/code lifetime required; SAN covers clients/source bodies only. No general failure propagation, escaping ownership or nominal exchange. | Preserve qualified C15; continue bounded admitted native/ordinary-C obligations, with Acc/QuickSort open. |
| #61 bounded binary callback inputs, C16 | Parent `ba5d39d` plus target-only edits: separate `c_callback2_v1`/`callback2_direct_v1`, fixed unary/binary same-width Pi contract, flat C signatures and old unary binary refusal4; seven implementation files +66/-21. | Worker E8 O2/client-source SAN48 each; 4000 manual +400 Core comparisons/product, source20, inert emission/header orders; affected unary39/module99+99/I/O19 pass. Initial client sentinel failure retained and fixed without emitter change. | Exact18 taska4a2e98 pushed/remote exact; fresh Root E15 source1283f48e22e binary48+48/unary39each/module99each/shared58each/link/I/O19 pass. Main merge separately reviewed. | Caller pure-total interpretation/lifetime; no arity3/mixed-width/boxed/effect/dependent contract, source authority or native Acc completion. | Root reviews C16 snapshot; continue bounded native/ordinary-C work separately. |
| #61 native Acc/QuickSort (historical #44/#49) | C4 `a3b6bce` pins native refusal; C7-C9 retain it. Applied List resolves only the first representation obligation. No native sorter. | Historical checked/trusted status 4/no product; Root C9 sorting-boundary gate passes. Structural FFTT remains separate. | C4-C9 task/prototype Main published; no native Acc epoch publication. | Indexed SizedList and callable recursive Acc fields remain unsupported; no producer defect established. | Report concrete indexed/callable representation needs through Merge; keep work bounded and downstream. |
| #61 remaining public contracts (historical #49) | General/boxed callbacks, effects, higher Identity, recursive captures and shared nominal exchange remain explicit limits; C16 advances only fixed borrowed unary/binary scalar inputs. | Local/joint gates verify supported cases and refusals, not general completion. | Historical tasks/prototype Main published; no publication of general missing contracts. | Need bounded justified representation/demand contracts. | Preserve refusals; route shared needs through Merge. |

Evidence: historical [C5](2026-10-03-C-BACKEND-EPOCH5-HANDOFF.md),
[C6](2026-10-03-C-BACKEND-EPOCH6-HANDOFF.md) and active
[enum List plan](2026-10-03-C-BACKEND-ENUM-LIST-PLAN.md).

2026-10-03, light native-sort inspection at `d275b75`: selection requires a
`PG_JUDGEMENT_VALUE_TYPE`/reference (`representation.c:57`), signatures require
reference domains/results (`scalar.c:717`, `:726`), indices must equal the
parameter prefix (`representation.c:64`), and fields must be references (`:98`).
The unchanged provider (SHA256
`a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`)
uses applied `List Nat`, indexed SizedList and Acc's callable recursive `down`.
QuickSort executes `*down` for both partitions, so its name/Sortedness is no basis
for erasure. Historical admitted-shape log SHA256
`fcad4a06063d4c0e760be567d2f42b58155ca2940d43d2a1b224b51add7a3425`
shows the applied classifiers; this is not a fresh producer/runtime run during
the slot. Existing declaration parameter/index/field views are present in the
pinned producer. No producer defect or new shared-interface need is established.

2026-10-03, fresh active C7 at `cea1dc0` plus the exact source manifest above:
enum payload Lists now use existing finite array copy helpers. Input tag validation
precedes arena allocation/mutation. Eleven O2 C gates and eight native generated-
client ASan/UBSan/leak gates pass on the pinned worker producer. The initial raw
client's empty-array compiler warning is resolved by its documented NULL input;
the old failed log remains. Scratch files/build overrides are removed. Exact files,
pins/deltas/limitations are in the [C7 handoff](2026-10-03-C-BACKEND-EPOCH7-HANDOFF.md).
No active gate failure, producer change, joint C7 claim or native sort completion.

2026-10-03, fresh Epoch6 at published `312c2da` plus lane edits: selected closed
value data can contain earlier selected nonrecursive value data. Whole structs
survive extraction/capture/return; private C validators check active nested fields,
and receipts borrow emitter-selected contract metadata. Ten O2 C gates and seven
native generated-client sanitizer gates pass on the pinned worker producer.
105 cases per source/object/archive product and 16 source observations cover the
new contract. Missing/later/recursive/function/dependent fields retain refusals;
existing callbacks/effects/capture-chain/native Acc limits remain. Earlier probe
syntax/setup/raw-harness failures are corrected and retained as historical logs.
Exact files/pins/source deltas: [Epoch6 handoff](2026-10-03-C-BACKEND-EPOCH6-HANDOFF.md).
No schema/producer/private checker change or current-Main verification claim.

2026-10-03, fresh Git inspection confirms Epoch5 branch/upstream publication
`312c2da155636da8415880eb674e1fb0455e3b95`, exactly 25 handoff files. Core reports
code/manifest review, nine terminal O2 gate successes on its reviewed committed
E1/performance/family/Surface 128-runtime-hash snapshot, and prototype Main merge
`435d965` complete; Main push is next. This is Core combined/integration evidence,
not a worker rerun or accepted promotion. The Epoch5 handoff/manifest remain
historical evidence; its implementation freeze is released. Separate Job/Evidence
producer E2/E3 integration remains pending according to Core.

2026-10-03, fresh Epoch5 at `a3b6bce` plus lane edits: the verified known-function
Thunk/Lambda change is applied to the active prototype; scratch copies/overrides
are removed. Nine O2 C gates pass and five native generated-client sanitizer
gates pass (static/scalar/data/List/numeric). Source/object/archive ordinary
clients cover lexical captures, shadowing, curried/repeated/unused demand, native
Nat/List resources and two components using distinct aliases/array exchange.
The exact old block controls have positive coverage; dynamic callbacks, demanded
effects, recursive function fields and the specific three-closure chain remain
explicit refusals. Evidence/pins: [Epoch5 handoff](2026-10-03-C-BACKEND-EPOCH5-HANDOFF.md).
Backend/producer/raw generators and driver objects are O2; sanitizer scope is
generated source bodies and clients. This worker run uses the pinned Core
snapshot, not a claim of latest Main combined verification.

2026-10-03, fresh Git inspection: Epoch4 task branch and upstream both point to
`a3b6bce5f2ad1dee80c56e9a4843ec646ba30fc5`, containing exactly its 11 frozen files.
Core reports matching manifest hashes, no blocking transactional array/List code
defect, delegated branch commit/push completed and freeze released. Combined C
gates/Main integration are next; publication is not integration or promotion.
Its committed handoff and old manifest remain immutable evidence.

Later Core report, 2026-10-03: all eight Epoch4 C gates exit 0 against committed
Main E1+performance+family/Surface producer; 128 runtime-source hashes match its
jointly broad-tested snapshot. An initial Surface setup omission was corrected
and the earlier run retained. Core merged exact `a3b6bce` prototype into Main;
Main push follows. No merge revision/push completion is claimed here. This is
Core combined/integration evidence, distinct from worker pinned-snapshot runs.

2026-10-03, fresh Epoch4 verification at `720f92a` plus the exact lane edits in
the [handoff](2026-10-03-C-BACKEND-EPOCH4-HANDOFF.md): five focused O2 gates pass
(numeric/List, scalar/raw Oracle, List, Linker and sorting boundary); numeric/List
and scalar/raw native clients pass ASan/UBSan with leak detection. New coverage:
511 array slices, 5,110 partition calls, Int32 source-sum correspondence, reversed
Int64 fields, empty/300-node conversion, all four allocation failure positions,
capacity rollback and prior-result survival. Final open QuickSort admits in
52,935 steps; checked/trusted native requests reject with status 4 and no product.
No remaining gate failure. Evidence and resource limits are pinned in the handoff.
Fresh Main history contains prototype merge `0fc0c0b`; no worker Main write.

2026-10-03, fresh Epoch4 probe at `720f92a` plus lane fixtures: current Core
producer admits an open Nat-list QuickSort (`53120` steps with diagnostic type
aliases); native linking refuses its selected generic `List Nat` representation
with status 4 before publication. A frozen copy of Core's family/Surface overlay
reads the artifact and its existing typed views successfully. The earlier
task-baseline reader failure was a producer/reader version mismatch, not evidence
of a current producer defect. Snapshot manifest digest:
`c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`;
inspection: `/tmp/a-program-c-backend-epoch4-current/acc-inspection.log`.
No producer, schema or shared-owner change is requested for this bounded epoch.

2026-10-03: fresh Git inspection confirms Epoch3 branch/upstream publication
`720f92a7103229996a8013847bdf1eca2246c9e9`. Core reports all eight combined C
gates passed with current family-cursor plus Surface producer, code audit found
no blocking defect, prototype Main merge completed and Main push was underway.
Log: `/tmp/a-program-core-c3-combined.log`. This is Core integration evidence;
the worker's separate task-baseline tests remain distinguished below. Epoch3
freeze is released. The old exact manifest remains historical evidence.

2026-10-03: fresh Git inspection confirms Epoch2 publication `4987c08` on the
task branch and its upstream. Core reports seven current-owner/family-cursor plus
Surface gates passed, followed by prototype-only Main integration `e5d4057` and
Main push. These combined/integration results are Core reports, not worker reruns.
At `4987c08`, `src/synthesis.c:atomic_rule_step` rejects integer tokens outside
Int32 and synthesizes accepted integers as Int32. Large Int64 surface literals
are unavailable. The published data fixture already uses `fallback : #Int64`
for its Int64 branches; no synthesis or fixture correction is needed.

Fresh Epoch3 verification at `4987c08` plus the exact lane edits in the
[handoff](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md): eight O2 C gates pass and five
native gates pass with ASan/UBSan/leak detection. Numeric coverage includes 1,089
comparison pairs, 27,305 stable partition cases, ten source List observations
and three Bool observations, 17 raw Nat Match evaluator observations, reversed
constructor/field order, checked magnitude overflow and finite copy-out including
300 nodes beyond the recursive execution limit. Final receipt/copy-out coverage
passes focused O2 and sanitizer reruns. No remaining gate failure. This uses the
private task-baseline producer; current-owner combined verification is Core's
separate gate before integration.

Historical Epoch1 inspection, 2026-10-03: branch/upstream both pointed to `bb69983`, the
published Epoch1. Core reports its push completed and its exact archive is under
combined current Identity-boundary producer tests. Later Core-reported evidence:
those combined backend gates passed. This is a coordinator report, not a fresh
worker rerun. Later Core report: Main integration is `9061be3`, Main push
succeeded and six current-Core backend gates passed. Prototype-only integration;
the worker then remained at its own `bb69983` baseline.

Fresh Epoch2 verification at `bb69983` plus the exact lane edits in its
[handoff](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md): seven O2 backend gates pass;
native scalar/enum/data/list gates pass with ASan/UBSan and leak detection.
The final list client covers 511 length/flag cases, 35 source differential
observations and 14 raw recursive sum/append evaluator observations. Raw emission
forbids evaluator/substitution calls and preserves graph/store counts. The
producer is the private immutable task-baseline overlay, not current Main.

Inspected 2026-10-03 at clean `2d747cc`, before lane edits: the baseline provides
`src/prototype/c_backend/` with structural emission,
LinkerScript and scalar/enum profiles. AP4-AP6 records incomplete parts; its
historical results were freshly rechecked for this epoch. O2 native, Linker,
structural and Acc QuickSort gates pass; ASan/UBSan native gates pass. Final
pinned/current producer images and emitted C/headers match byte-for-byte.
Evidence and exact publication files are in the [epoch handoff](2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md).

### Assessment

2026-10-03 19:29 UTC, Root agent decision: bounded callback ABI qualifies on the
current E12 producer and integrates as prototype only. Supersede earlier
READY/pending and imported-fixture rejection conclusions with fresh explicit
import/source and matched-image evidence; preserve original failures/history.
SAN scope and caller purity/totality/lifetime preconditions remain explicit.
Native Acc/QuickSort and broader #61 completion remain open.

2026-10-03, Root operational issue routing under the human-authorized audit:
#44/#49 are superseded by open #61, preserving separate source/native residuals
and the existing C owner. This is an audit disposition, not a new user design
principle. Root retains exact C13 task bytes and resolves only current status/
provenance in this reviewed Main plan.

2026-10-03 12:03 UTC, Root operational decision following the direct human
workflow replacement: all earlier publication/review-only build and implementation
holds below are superseded. C10's exact six submitted files are preserved in an
isolated publication snapshot; live work may advance independently. Continue
the minimal target I/O classification reproduction/correction and bounded
existing-value-record/single-tail List work at serial j1 within the prototype
Goal. Relevant correctness verification may overlap the other lanes; comparative
wall/RSS still needs an exclusive slot. Root resolves integration conflicts.
No producer/schema/erasure expansion, accepted C promotion or native Acc/QuickSort
completion follows. Report consumed release, next action and genuine dependency.

Historical Root releases below are superseded by the 12:03 workflow for live
implementation; their completed C10 execution scope remains historical evidence.

Agent C11 implementation decision within the existing prototype Goal: the
pinned stream faults reproduce status 4 for source/header/direct I/O. Preserve
stream errors before fclose and prefer I/O status 2, retaining unsupported 4,
atomic cleanup and prior-output preservation. The original worker backend-only
build uses qualified C9/E8 runtime128. Root's current E9+E10 producer gates are
separate fresh qualification; they are pending, not inferred from the old build.

Historical worker C14 proposal/qualification record; the corrected explicit-import
review above supersedes fixture-only source-rejection inferences:

Root operational routing after the human issue-disposition assignment: agent
audit consolidates historical #44/#49 residuals into open #61, splitting semantic
and native-target obligations under the same existing C owner. The specific
issue routing/consolidation is agent workflow, not a new direct human design
principle. Preserve milestones, #47 policy and all representation/erasure limits.

2026-10-03, agent next bounded candidate after exact C13: inspect the admitted
fixed-width callback classifier using existing structural owner views. AP5.6/#61
requires an explicit callable/type/resource contract before foreign callbacks.
Consider only a separate profile for borrowed unary Int32/Int64 pure-total
callbacks, with explicit signatures and code/context lifetime; preserve existing
native/scalar callback refusals. First establish whether the retained ordinary
source classifier actually carries the needed totality and domain/result shape.
Do not infer totality from an arrow spelling, fabricate evidence or modify the
producer. If the necessary contract is absent, report the concrete boundary to
Merge rather than weakening admission or rebuilding a checker. No new profile is
enabled merely by this candidate decision. C13's eleven submitted files remain
immutable in its verified tar while live owned status advances independently.

C14 agent implementation decision after the positive classifier probe: use
`c_callback_v1`/`callback_direct_v1` only for borrowed same-width unary Int32/Int64
pure-total function inputs and scalar results. Validate null code before calling;
borrow context synchronously and never return/store foreign closures. Arbitrary C
code's source interpretation and lifetime are explicit caller preconditions, not
backend checking evidence. Ordinary admission remains required; the separate
Core test interpretations use existing host operations and do not establish
Surface formation or equality proofs. Imported-source probes stay reported and
routed to Job; target controls do not excuse their failed admission.

Superseding Root observation, received 2026-10-04 local date: explicit imports
resolve the original callback source probes without changing output/fuel or
frontend policy. Their historical rejected input bytes remain evidence of fixture
setup, not an active producer dependency. Agent C15 decision: verify generated
scalar providers as borrowed callbacks in two independently emitted consumers,
all product pairs/both header orders and synchronous loaded-code lifetime. Keep
the callback ABI unchanged; no escaping ownership or erasure policy follows.

Root scheduling request, 2026-10-03 UTC/2026-10-04 local: proposed Fold-only
matched36 cost window 20:20-20:30 UTC is not a worker launch grant. C15's focused
gates are terminal with no live build/runtime children at 20:06 UTC. Send the
safe-boundary notice and keep heavy work drained only for this window/release;
light source/docs/hash handoff work continues. This is operational scheduling,
not a direct human design statement or blanket implementation hold.

Root operational release: Fold36 collector was terminal at 20:20:46 UTC, all36
completed with exact charged fuel and no heavy children. Focused C16 correctness
resumes; no worker comparative grant follows. C15's exact task blobs/tar are
independently readable, so its immutable submission stays preserved while this
distinct live epoch advances. Parent task readback is not remote/Main evidence.
Agent C16 decision: use a separately named unary/binary profile after the actual
admitted nested Pi view; validate all independent domains and pure TOTAL result
widths with existing views. Test interpretations run after inert emission and
confer no source admission/equality receipts. Source erasure/producer/schema and
native Acc/QuickSort limits remain unchanged; original unary refusals stay tested.

Root operational environment receipt: tmpfs ENOSPC affected other lanes after
C16 gates were terminal. Agent storage decision: preserve every original raw
file and copy all2294 byte-exact to owned disk-backed
`src/prototype/c_backend/.evidence/epoch16-20261004`; final manifests/tar and
future compiler temporaries use that disk. No shared cleanup/evidence deletion
or C16 ENOSPC test failure follows; initial sentinel setup failure stays pinned.

Fresh read-only issue inspection: [#61](https://github.com/repyt-margorp/a-program/issues/61),
open, updated 2026-10-03 13:11:54 UTC, consolidates unfinished selected-export/
Identity and native-target sections under this owner. Its agent assessment keeps
#47 policy and indexed/callable Acc refinement separate; ordinary admission
remains authoritative. This is inspected issue content, not a new human rule or
proof that the missing contracts are implemented.

Root operational release, 2026-10-03 12:03 UTC, supersedes the blanket holds
recorded below: the C10 exact six-file snapshot was copied/hash-verified into
Root's isolated publication worktree/index; live files can advance independently.
Focused correctness at j1 may overlap lanes, while matched timing/RSS remains
exclusive. Agent C11 choice: the pinned publication stream fault reproduces
status 4 for source/header/direct I/O; correct only that target distinction and
retain unsupported status, atomic cleanup and prior output. Reuse qualified
C9/E8 runtime128 for a backend-only build and affected ordinary-client controls.
No producer/schema/source-erasure change or native Acc claim. Root handles
publication/integration conflicts; publication is not an implementation blocker.

Root bounded scheduling release, 2026-10-03 11:15 UTC: the reviewed light
manifest `f29b9b92` authorizes only the prepared distinct NumbersNat/ChoiceNat
ordinary-C source/object/archive combinations, serially at `-j1`, with focused
O2/generated-client ASan/UBSan/leak controls. Reuse exact qualified C9/E8 backend
and products; pin all input/binary/source hashes and preserve duplicate-symbol
refusal/shared-arena failure/depth/lifetime checks. No producer/emitter/schema
edits, broad thirteen-gate rerun, E9/E10 composition or timing. Freeze the bounded
terminal result separately; Root owns shared Git publication. This supersedes
the hold below only for these clients; other heavy work remains held.

Root operational update, 2026-10-03 11:07 UTC: C9 freeze is released for truthful
owned status/light review. The heavy-work hold remains while Performance qualifies
E9 then E9+E10: no new broad/sanitizer execution or timing until scheduled release.
Agent next choice: review the separate [ordinary-module alias note](2026-10-03-C-BACKEND-ABI-ALIAS-NOTE.md)
and scratch client, then run only its focused gates after release. Explicit
existing target aliases suffice for the observed helper-name collision; defer
new namespaces/receipt registries. Keep historical C9 handoff and manifests
immutable; this status update belongs to later work. No shared Git mutation,
producer/schema expansion or accepted C authority follows from integration.

Agent C8 epoch decision, 2026-10-03: after the capture trial's ordinary/raw/static
sanitizer and eight additional native/link/sorting O2 gates pass, adopt only its
small finite dependency walk and portable tests in the active prototype. The
exact old `nested_three` becomes positive; callback/effect/native Acc refusals
and original raw static checks remain. Final twelve O2/nine generated-client
sanitizer gates run sequentially before exact freeze/publication. Applied-List
work stays outside C8; it has private focused evidence, not integration authority.

Merge scheduling release, 2026-10-03: performance's 30 timing samples are terminal
and stopped; audit cost is deferred with no timing phase. The exclusive hold is
explicitly released. Resume private correctness/build work sequentially at
`-j1` while Merge runs immutable C7's eleven qualified-E6 gates and performance
qualifies E7/E8. Merge's private C7 review candidate is `091669f`; Main integration
still waits for all eleven gates. This scheduling report grants no producer,
schema, accepted-source or native sorter authority. Preserve indexed/callable
Acc refusals and inspect the existing admitted view/parameter order first.

Merge scheduling decision, 2026-10-03 after C7 publication: performance's E6
qualification requests a bounded exclusive wall/RSS slot. The current scratch
build and O2/native sanitizer clients are terminal exit 0; fresh owned-process
inspection finds no live heavy C child. Hold all further builds/runtime gates
until explicit release, while light Acc/QuickSort inspection continues. The Goal
remains active. Merge's current-producer C7 gates use immutable `d275b75` after
the slot; this worker does not duplicate them. A short outbox acknowledgment
records the safe boundary. This is operational scheduling, not a user design
principle or evidence of native Acc completion.

Merge operational steering, 2026-10-03: C7 is task-published and the live freeze
is released because combined verification uses its immutable commit. Keep old
frozen evidence unchanged; continue bounded native Acc/QuickSort work from this
task parent as a separate epoch and report concrete representation blockers via
the outbox. No producer/schema expansion or worker Main integration. The central
schedule at `4d1d941` and both briefs were read; correctness gates may overlap at
modest parallelism, while comparative wall/RSS/audit cost needs Merge's exclusive
slot. Producer/admission/frontier needs route through Merge to Job/Evidence.
Current references to Core below are historical; Merge now owns those duties.

Agent next-epoch probe: the existing three-closure refusal is a target capture
dependency limitation. A scratch-only finite dependency walk passes initial
ordinary clients (600 Int32/seven Int64 cases per product and six source
observations), with callback/effect refusals preserved. The
[transitive capture plan](2026-10-03-C-BACKEND-TRANSITIVE-CAPTURE-PLAN.md) pins the
remaining verification. This AP6.3 candidate does not complete native Acc;
inspect applied-family/Acc representation needs before selecting further scope.

Agent native-sort boundary decision from the light inspection: a bounded selected
applied, unindexed List instance is the first representation probe after the
slot; its ordinary identity/constructor/Match contract must precede any claim
about QuickSort. Retain indexed SizedList and callable Acc refusals until a
justified target recurrence/representation exists. Do not erase `down`, rebuild
an upstream IR/checker or change `.a` for this probe. Report these target barriers
through Merge's outbox; request shared-owner changes only if a concrete missing
view is demonstrated. The scratch capture improvement alone resolves none of
the three representation barriers.

Agent follow-up evidence/decision during the measurement hold: constructor values
retain their already instantiated type occurrence, and nullary namespace
constructors return a value directly. The
[applied List plan](2026-10-03-C-BACKEND-APPLIED-LIST-PLAN.md) prepares a small
source fixture and proposes explicit target-only `data_of` selection from that
classifier. This avoids a type-factory evaluator or unbounded typed-query helper.
The selector is not implemented; source admission/actual mapped occurrence
alignment and all target gates remain pending until release. Do not count this
inspection/draft as applied-List or native Acc completion.

The isolated applied-List helper is now drafted (58 C lines/22 declarative header
lines, C SHA256 in the issue table). It reads source-owned classifier/declaration
nodes and maps parameters to existing selected representations using target
storage; it creates no source terms/evidence and calls no checking/evaluation
worker. No active parser/lowerer change or verified product is claimed. The
measurement hold still forbids builds/runtime gates until explicit release.

Core operational reporting decisions, 2026-10-03, superseding their former
Subjective attribution in the temporary feedback note: use one concise table
with exact revision, local/joint evidence, publication/integration, remaining
criteria and next epoch. Update at material milestones/blockers and within six
active hours; do not invent percentages or imply closure/promotion. Send brief
pointer/hash/changed-row notices through the existing outbox, without a reporting
framework or owner code changes. The direct human requirement is concrete
issue-linked feedback because progress is unclear. While C6 was frozen, the user
note/table remained separate; Core's release now permits incorporation here.

Core operational C6 release, 2026-10-03: preserve the exact published historical
handoff and manifest, and continue a separate downstream epoch. Core reports
Job E2 prototype Main `7b27c4c`, 2,007 evidence hashes checked and Main publication
in progress; its new common-producer C6 gates are separate from worker evidence.
Agent next decision: adopt the verified enum-array trial through the existing
copy helpers. No producer/schema/shared authority change is needed.

Core operational Epoch5 release/integration, 2026-10-03: continue distinct bounded
C-module work with explicit refusals; no native Acc completion, producer/schema
extension or private checker. Route any shared-producer need through Core. Agent
next candidate is selected closed value-record fields using existing typed views;
the [record plan](2026-10-03-C-BACKEND-NESTED-RECORD-PLAN.md) owns its probe/tests.

Core operational release, 2026-10-03: later distinct worker edits may proceed
after Epoch4 publication. Keep native Acc/QuickSort and the trial's specific
three-closure capture-chain refusal explicit. Continue bounded C usability work,
with no producer fields or private checker. Static-function scratch was excluded
from Epoch4; implement and verify its next epoch independently.

Core operational routing, 2026-10-03: Job E2 interrupted the existing Core sleep
after 43.156 seconds; this is a Core report, not a C-worker wake experiment.
Workers post one material ready/blocker/conflict/regression notice under their
own `src/prototype/coordination/outbox/*.txt`, newline terminated and at most
8 KiB, with lane/epoch/commit/manifest/gates/failures. Core owns the pointer/hash
relay without socket changes, merge or missing-session launch, and waits in the
existing conversation with interruptible `clock.sleep` plus the six-hour fallback.
Do not load/resume/fork another Core, change the relay or perform Main integration.
The Epoch4 notice was received and reviewed. These are operational decisions,
correcting their earlier mixed Subjective attribution in the temporary addendum.

Agent Epoch5 decision: adopt the verified minimal static Thunk/Lambda handling
through existing direct-call/capture machinery. Move the exact old block controls
to positive product tests while retaining dynamic callback, demanded effect,
recursive function-field and unsupported capture-chain refusals. Test ordinary
native modules with distinct aliases and explicit array exchange/shared arena.
The [bounded follow-up plan](2026-10-03-C-BACKEND-STATIC-FUNCTION-PLAN.md) holds
fresh evidence; no public closure ABI, source authority or native sort completion.

Core operational reprioritization, 2026-10-03: prioritize target ABI, ordinary C
clients, source/object/library products and bounded native lowering of admitted
source. Retain unsupported contracts explicitly; do not pursue wholesale
CBPV/dependent/effect coverage or change `.a` for target convenience. The native
Acc/QuickSort probe may establish a concrete boundary; route producer gaps to
Core rather than rebuilding an upstream checker or IR. This supersedes the
open-ended Epoch4 generalization proposal below.

Agent revised Epoch4 decision: finish the bounded Acc probe, report its actual
reader/representation boundary, then select a small usable C boundary improvement
with ordinary client and source/object/library verification. Keep existing
native numeric/List behavior and historical evidence intact.

Agent Epoch4 ABI decision: extend the already selected two-constructor
scalar-payload List contract with `ap_from_ALIAS(arena, array, count, out)`.
Copy the caller's finite slice into arena-owned nodes, including the terminal;
preserve order and prior results, rolling back this call's allocations on failure.
Pair it with existing copy-out and native source partition exports in ordinary C
clients for source/object/archive products. This is target conversion glue, not
native Acc erasure, QuickSort completion or a change to source semantics.

Core operational Epoch4 steer, 2026-10-03: proceed to native Acc recurrence,
QuickSort and slice requirements. Preserve explicit target resource limits and
downstream-only authority. Structural QuickSort and a hand-coded sorter do not
complete native lowering. Route producer/readback/checking dependencies through
Core to job-evidence before editing shared owners. Plan/status updates are new
epoch edits; do not rewrite old manifests.

Agent Epoch4 decision, 2026-10-03 at `720f92a`: start from the existing generic
sorting provider and a selected open Nat-list sort, not a new sorter. Inspect
admitted family/constructor/index and Acc callable-field structure first.
Investigate uniform target representations with indices/proof fields retained
where used, plus native known function/thunk values; do not assume Acc erasure
from source Sortedness or constructor arity. Use small downstream inspection
probes to identify existing owner views and concrete lowering failures before
changing representation/lowering code. Producer/readback gaps go to Core.

Core operational routing, 2026-10-03: the job-evidence worker uses clean baseline
`5035c7a`, producer `64df10d`; Core no longer edits SE owners concurrently.
Route producer/admission/frontier needs through Core to that worker. This epoch
needs no shared interface change. Continue implementation/tests and exact freeze
handoff; resolved publication authorization and queued Epoch2 notes stay resolved.

Core operational steer, 2026-10-03: release the Epoch2 freeze and prioritize
native numeric-predicate partitioning/List boundary work. Borrow emitter-selected
contract metadata to remove receipt representation reconstruction if simpler.
Keep `.a`, schema and producer authority unchanged; ask Core before shared
interface changes. Do not reopen resolved literal/auth work or publish status-only
commits. The new Goal edits belong to the next epoch; the committed handoff and
its historical manifest stay unchanged.

Core workflow, 2026-10-03: shared Git remains read-only; Core performs the
worker-branch commit/push and verifies emission with its current-owner plus
Surface overlay before any Main merge. This is distinct from publication and
does not authorize worker integration or accepted-source promotion.
Core operational handoff steering, 2026-10-03: provide the exact frozen Epoch2
files/hashes and handoff for worker-branch publication. Older queued literal/auth
notes are resolved. Core's reported `9061be3` merge/push is integration evidence
in Objective, not a new user requirement. This provenance correction changes
documentation only; completed code/tests and verification remain unchanged.
Core's native-container priority, exact freeze and queued-note handling are
operational steering; style checks and apply_patch follow repository/coordinator
instructions. They are not new direct user decisions. This supersedes their
former Subjective attribution and the earlier mixed "Core/user authorization"
label; actual user branch-publication/Core-only-Merge authorization stays there.

Agent Epoch2 implementation decision, 2026-10-03 at `bb69983` plus lane docs:
start with closed unindexed single-tail recursive declarations, represented as
borrowed immutable C node pointers. Constructed nodes belong to an explicit
caller-owned allocation arena. Recognize the existing erased recursive Match
template and statically known induction thunks; do not introduce a recursive
source former or infer a source type from the C representation. Validate finite
input chains and retain explicit refusals for indexed/dependent fields, trees,
callbacks and unsupported thunk shapes. Verify length, append and stable list
selection before attempting slice/QuickSort. This is an agent choice within the
owned downstream scope, not a new user design requirement.
Current implementation: native length/sum/append and stable Bool-field selection,
including scalar results of composed List-producing calls. Numeric-predicate
partitioning is not implemented; do not present flag selection as its completion.
Target arena allocation/depth failures have explicit statuses, preserve output
and roll back only the failing call's new nodes. Default/max recursive depth is
256; this is a target resource bound, not source termination evidence.

Agent Epoch3 decision, 2026-10-03 at `4987c08` plus this plan: the existing
`natLessOrEqual` uses recursive Nat matches; the host API supplies arithmetic,
not an integer comparison primitive. Add an explicit target `nat32` selection
for the exact closed zero/single-successor declaration shape, mapped to uint32
with checked successor overflow. Lower the existing comparator and stable numeric
List partitions through ordinary constructor/Match/call lowering, not a named
comparator replacement. Generalize saturated recursive calls and retain the
existing static IH capture handling. Nested recursive closure captures remain
explicitly refused. Add finite List copy-out
for a structurally checked scalar-payload/single-tail shape, with explicit buffer
capacity and unchanged output on failure. Reuse emitter-selected contract flags
in link receipts. These are backend-local choices; source authority and shared
interfaces remain unchanged. Nat width and recursion depth are target resource
bounds, not source typing or termination evidence.

Core workflow update, 2026-10-03: publication permits the next owned epoch.
Superseding Core steer: the queued Int64 correction was already resolved before
`bb69983`; do not recreate it as a new epoch. Additional Int64 coverage is
optional. Core reports combined current-producer
backend gates passed. The optional Int64 follow-up is retired before publication;
prioritize native recursive/container lowering, without producer
fields or hiding callback/Identity limitations.

Core/agent workflow, 2026-10-03 (supersedes its earlier placement in Subjective):
Core reports Surface's shared index.lock sandbox failure and offers delegated
task-branch publication. The worker supplies exact files, branch, commit message,
test results and CODING_STYLE checks, without a sandbox bypass or self-merge.
Core requests retained unsupported block/callback/recursive tests and endorses
the agent's Int64 fallback correction; that correction preserves `::` as an
assertion. Core requests final evidence, freeze acknowledgement and no further
epoch-file edits during review. Manual source/docs edits use apply_patch.
These are coordination/implementation decisions, not direct user quotations.
Publication, actual integration review and promotion remain distinct.

Assigned scope: C design, lowering/runtime/link prototypes and lane-local tests.
Fresh #44/#49 bodies/comments and PR #45/#50 diffs were inspected. Adopt the
reusable native realization direction; reject the hand-derived sorter as an
implementation and keep whole-module admission/selected-export policy with Core.
Agent Epoch1 decision (historical): implement explicitly selected nonrecursive fieldful data
as C tagged structs with direct constructor/Match calls, scalar/enum fields and
borrow-free value ownership. This advances AP6.4 and non-scalar captures without
claiming recursive List/Acc/QuickSort or higher Identity completion. Recursive,
indexed, dependent and function fields must reject before publication. Epoch2
supersedes the blanket recursive refusal for direct single-tail fields only. No new
shared IR, checker or image fields. The later 2026-10-03 lane assignment permits
this target work while Main continues SE1, superseding the earlier AP6 hold.

### Plan

- [x] Read AGENTS.md, CODING_STYLE.md, the coordination plan and AP4-AP6.
- [x] Inspect relevant GitHub issue/PR bodies and diffs; preserve user Subjective.
- [x] Establish a private clean producer overlay and immutable `.a` fixtures.
  Use `ARTIFACT_SOURCE="$PWD/src"` with `solver_inputs/overlay.sh`; no symlink
  may reference Main's dirty tree. Set private `OVERLAY`, `BUILD` and outputs.
- [x] Select fieldful value data as the next epoch. Gates: public C calls over
  all constructor cases; wrapping scalar fields and enum validation; direct
  constructor/Match, captured values and returned data; interpreter differentials;
  all products, checked/trusted determinism, refused representations/ABI/nominal
  mixing, unchanged input hashes and inert raw emission; O2 and ASan/UBSan.
- [x] Implement this fieldful epoch in `src/prototype/c_backend/`, preserving the downstream boundary.
- [x] Verify this epoch's evaluator/C agreement, C module use, negative controls and input
  image immutability; test committed and current producer compatibility.
- [x] Report per-file implementation/test/doc deltas and measured results in the handoff.
- [x] Core published Epoch1 as `bb69983` on the task branch; merge review is separate.
- [x] Epoch2: implement single-tail recursive nodes, arena ownership, recursive
  Match/known IH thunk lowering and finite input validation. Gates: native
  length/append/selection with ordinary inputs, evaluator differentials, failed
  allocation/output preservation, malformed/cyclic inputs, existing refusal
  coverage, deterministic inert emission and O2/ASan/UBSan checks.
- [x] Core published Epoch2 as `4987c08`; Core reports combined verification and
  prototype-only Main integration `e5d4057`. Freeze released for the next epoch.
- [x] Epoch3: native Nat comparison, stable numeric List partitions and finite
  scalar List copy-out. Verify source/C agreement, boundary/resource failures,
  existing refusals, inert deterministic emission, receipts and O2/sanitizers.
- [x] Hand off frozen Epoch3; Core published/pushed `720f92a` and reports eight
  combined gates, audit and prototype Main merge passed. Freeze released.
- [x] Epoch4: establish the concrete native Acc/QuickSort boundary without a
  generalized compiler rewrite; route owner/readback needs through Core. Deliver
  a bounded C ABI/client improvement with source/object/library verification.
- [x] Core published/pushed Epoch4 as `a3b6bce` and released the freeze.
- [x] Core reports eight combined Epoch4 gates and prototype Main merge passed; preserve
  the historical committed handoff and manifest.
- [x] Epoch5: apply/verify the bounded static-function candidate and ordinary
  native-module composition.
- [x] Hand off frozen Epoch5 through the verified outbox route.
- [x] Core published Epoch5 `312c2da`, reports nine combined gates and prototype
  Main merge `435d965`; implementation freeze released, Main push next.
- [x] Epoch6: verify the bounded selected value-record candidate, preserving
  unsupported shapes and historical exact handoffs.
- [x] Hand off frozen Epoch6 through the outbox; Core publishes/pushes `cea1dc0`
  and releases the freeze. Its current-producer review/integration is separate.
- [x] Core verifies exact C6 against its joint E3 producer and integrates/pushes
  prototype Main `53debc8`; evidence/status `fd45c42`. No accepted promotion.
- [x] Epoch7: adopt/verify selected enum List arrays in a separate bounded epoch.
- [x] Hand off exact frozen C7; Merge publishes/pushes task `d275b75`, verifies
  remote and releases the live freeze. Historical evidence remains unchanged.
- [x] Merge reports eleven current-E6 C7 gates pass and local prototype Main
  candidate `091669f`; keep joint O2 distinct from worker sanitizer evidence.
- [x] Merge's C7 Main publication/status push completed in `66fa707`.
- [x] C8: adopt/verify finite transitive static captures; final twelve O2/nine
  generated-client sanitizer gates and style pass. Exact old refusal is positive.
- [x] Freeze C8 exact source/test/docs; keep applied-List implementation outside it.
- [x] Root publishes/pushes exact C8 task `884d5bb` (seventeen frozen files).
- [x] Root twelve current-E8 O2 gates pass; exact producer/source pins and
  retained setup failure are in `core-epoch8.json`.
- [x] Prototype Main integration `dea5fc1`; Main publication is tracked by Merge.

- [x] Adopt the separately verified applied selector/client candidate and finish
  bounded source take/drop/slice verification before a distinct exact handoff.
- [x] Freeze separate C9 after thirteen O2/ten client sanitizer gates and style;
  preserve exact C8/private historical evidence.
- [x] Exact C9 task `60c100c` pushed/remote verified by Root; fresh immutable
  task bytes and changed path set match all 38 frozen entries.
- [x] Root thirteen strict O2 E8/runtime128 gates pass; prototype Main `48c42eb`
  integrated, publication `7ca963d` pushed/remote verified. Fresh readback keeps
  code/tests/historical records exact; only the owning status is reconciled.
- [x] Prepare a separate two-Nat-module alias/shared-arena client during the hold;
  shell syntax/style and read-only wrapper/header review only.
- [x] After Root's bounded 11:15/11:26 releases, reconstruct only exact C9/E8
  fixture/Numbers/Choice products and verify serial focused O2/client-sanitizer
  combinations. Inputs/source/binary/commands are pinned; no broader execution.
- [x] Freeze exact bounded C10 test/docs handoff for Root publication/review.
- [x] Root reports exact C10 task/Main publication; the direct human workflow
  supersedes blanket holds. Preserve its historical frozen evidence.
- [x] Verify/handoff C11 I/O, C12 finite record Lists and C13 shared C products as
  separate exact snapshots, with task and Main reports distinguished above.
- [x] Probe a bounded callback's actual admitted classifier/Force shape using
  existing views; enable no foreign contract until applicability is established.
- [x] Implement/verify the separate borrowed unary scalar callback profile with
  ordinary admission, Core/source comparisons and focused O2/client/source SAN.
  Preserve old profile refusals and route imported application boundaries to Job.
- [x] Freeze exact C14 source/test/docs; Root task/Main publication and fresh
  current-E12 producer qualification complete, with scope in the linked receipt.
- [x] C15: verify generated-provider callbacks across two consumer modules and
  mixed products/header orders, focused O2/client-source SAN and loaded-code lifetime.
- [x] Freeze/hand off exact C15; distinguish worker E8, Root C14 E12 and fresh
  Root C15 current-E14 qualification/task/Main publication. Preserve all
  historical submitted snapshots.
- [x] C16: implement distinct borrowed unary/binary scalar profile and verify
  Core/manual/source comparisons, inert emission, header orders and old refusals;
  affected unary/module/I/O controls pass with qualified E8.
- [x] Freeze/hand off exact C16 implementation/test/docs for separate Root task
  publication and current-producer/Main review; fresh Root E15 gates pass and
  taska4a2e98 is pushed. Keep C15 immutable evidence.
- [ ] Maintain the single issue table at material events and within six active
  hours; use brief pointer/hash notices and keep Core workflow in Assessment.
- [ ] Coordinate further bounded target ABI/lowering increments under the latest
  user scope. Remaining Acc/QuickSort, callbacks/effects, Identity and shared
  nominal contracts stay explicit; do not expand into upstream authority work.
- Completion: the supported C/Linker refinement described by AP4-AP6 and open #61
  (human-authorized successor of historical #44/#49)
  is implemented and verified within the agreed downstream scope. Epoch results
  go to the coordinator for integration; unsupported constructs and unresolved
  policy choices remain explicit. One small change does not complete the Goal.

## Work Contract

- Write only `src/prototype/c_backend/`, this lane plan and necessary lane-local
  documents. Do not edit accepted code, shared SE/AP plans, Main overlays or
  artifact codecs/checking/fuel policy. Ask Main to transfer scope if required.
- Do not merge PRs, close issues, push Main, promote code or change model.
  Branch-only push is allowed after focused verification.
- No independent Job/Evidence/equality authority, checker or shared program IR.
- Superseding workflow: focused correctness may overlap lanes at `-j1`; matched
  timing/RSS needs Merge's exclusive slot. C11 runs no broad suite or timing.
  Keep generated overlays untracked; detach symlinks before edits.
- Maintain this short SOAP plan in place. Before ending each work turn, record
  current task, actual tests, next action and blockers. Report cross-owner needs
  concretely so Main can steer. Do not claim completion for merely writing a plan.

## Current Epoch

2026-10-03: concrete implementation and verification progress; Epoch1 was
published as `bb69983`; Core reports prototype-only Main integration `9061be3`.
No accepted-source promotion. Its exact archive stays frozen.
The owned lane may continue with separately verified follow-ups. Baseline/current private producer
paths and exact deltas/tests are in the [handoff](2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md).
The worker performed no shared Git index write or sandbox bypass. Core may
commit/push the named task branch under the explicit publication authorization.
Agent code review checked nominal layout refusal, unsigned/signed field mapping,
conditional extraction, captures, output preservation and target-only storage.
Epoch2 adds verified single-tail recursion, arena ownership, length/append and
stable Bool-field selection. Core published it as `4987c08` and reports Main
integration `e5d4057`; the historical [handoff](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md)
and its manifest remain unchanged. Epoch3 was published as `720f92a`; Core
reports combined verification/audit and prototype Main merge passed. Freeze is
released. Its [handoff](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md) and exact manifest
remain historical evidence. The latest user clarification narrows Epoch4 to
usable downstream C boundaries and bounded native lowering. The Acc/QuickSort
probe reports the selected generic-family representation refusal. The verified
array-to-List boundary and ordinary source/object/archive clients are frozen in
the [Epoch4 handoff](2026-10-03-C-BACKEND-EPOCH4-HANDOFF.md), published as `a3b6bce`.
Core released the implementation freeze and reports eight combined gates/Main
prototype merge passed; Main push completion is not yet reported.
No hand-coded replacement or shared-owner edits.
Epoch5 was published as `312c2da`; Core reports nine combined gates and prototype
Main merge `435d965`, with Main push next. Its old exact manifest stays historical
evidence and the implementation freeze is released. The selected value-record
candidate is published as separate Epoch6 `cea1dc0`; Core releases the freeze and
reports ten joint-E3-producer gates passed, Main merge `53debc8` and push complete.
The next
separate epoch adopts verified enum List copying, with no shared-owner changes.
C7 now passes eleven local O2 and eight native sanitizer gates; its exact handoff
is ready for a separate freeze/publication. The single issue table above records
publication/integration and remaining criteria; old handoffs remain unchanged.
The historical three-closure captured chain and old-profile dynamic callbacks
have explicit positive/refusal coverage under their selected profiles; general
boxed callbacks,
tree/indexed/dependent fields, missing/later value children, recursive aggregate
fields and effects remain explicit negative coverage.
Further work includes slice/Acc/QuickSort;
keep relevance/admission decisions with Core. The task has not completed AP6.4/5.
Cross-owner handoff: #44 selected-export admission beside unresolved siblings and
#47 relevance remain Core-owned. The full Goal is active; this epoch does not
complete AP4.6, AP5.6 or AP6.3-AP6.5/7/8 and does not close their successor #61.

C14's historical [handoff](2026-10-03-C-BACKEND-EPOCH14-HANDOFF.md) pins the
borrowed unary scalar callback epoch and worker E8 tests. Root later publishes
task `e9d74f70`/prototype Main `6c36dcb` with E12 qualification and explicit-import
controls; omitted imports resolve the former fixture boundary without producer
policy changes. C15's separate
[handoff](2026-10-04-C-BACKEND-EPOCH15-HANDOFF.md) pins generated-provider/module
composition, worker E8 correctness and the pending current-producer review.
No full Goal/native sorter completion or accepted promotion.
