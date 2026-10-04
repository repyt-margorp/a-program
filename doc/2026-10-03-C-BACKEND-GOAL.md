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

2026-10-04, conveyed via inquiry desk/Merge durable notices09:17/09:24UTC,
English paraphrase, original wording not supplied: why is C modularization still
unfinished, and what prevents Goal completion? Repeated tiny fixes across the
three implementation sessions instead of their main problems are extremely
problematic. Keep the actual Acc QuickSort CODE deadline21:16:56Z and prioritize
its indexed/callable/down/capture gaps; defer List conveniences. This supplements
the already recorded faithful manual mockup clarification, not a Goal reset.

2026-10-04, conveyed human clarification via inquiry desk/Merge, English
paraphrase of the12-hour Acc deliverable: readable realistic C CODE is due,
not full generalized backend completion. A clearly labeled hand-authored
candidate faithfully following actual admitted Acc QuickSort is acceptable.
The substitute-sorter prohibition excludes alternate algorithms or false
generated-success claims; it does not prohibit this mockup. Retain Acc/down
recursion, indices, partition and captured parameters. Deliver executable
extent and gaps, not only correspondence docs; preserve sealed C32.

2026-10-04T09:16:56Z, conveyed human requirement via inquiry desk/Merge, English
paraphrase; supersedes further List-convenience priorities: too much refinement
has moved away from the main problem. Within12 hours (deadline21:16:56Z,
2026-10-05 06:16:56 JST), prioritize a realistic readable C-code mockup lowering
the actual admitted Acc QuickSort. Preserve tested C32 at a safe boundary and
defer more List conveniences. Continue the same Goal/model, prototype C-specific
scope, without .a extensions, invented source erasure or a substitute handwritten
sorter. Show Acc/down, indices, partition and capture correspondence; distinguish
executable candidate, sketches and generalized lowering, and identify remaining
gaps. Merge retains review/integration.

2026-10-03, concise English paraphrase of the direct human workflow change via
inquiry desk `019ebfae`: implementation lanes may continue without waiting for
Merge; Merge resolves integration conflicts. This supersedes blanket review/build
holds. Continue separate prototype epochs within the existing Goal, preserving
submitted evidence and shared Git/Main boundaries. Coordinate genuinely exclusive
benchmark/shared-resource runs. Native Acc/QuickSort remains unfinished; invent
no source erasure policy. Recorded first in the next-boundaries note during C10
freeze and incorporated now after Root released the live files.

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

Fresh C50 [saved-image product command](2026-10-04-C-BACKEND-ACC-COMMAND-PLAN.md)
loads/admit-selects the qualified actual image once and regenerates all seven
bodies plus map/endpoint/frame table, without source advancement during emission.
Ordinary source/object/archive expose unchanged gs_sort and exact898-line C49
module53f01aa8/header246364a1/section provenance79ac358c. Currentfc52755b/runtime120
c206 O2/full driver-emitter-generated-source/client ASan/UBSan/leak44 expected
rows each pass, SAN terminal22:39:14Z/final exact-harness O2 terminal22:47:39Z.
Reused Core341/source output/resources/depth/
rollback, inherited map/endpoint/frame refusals, four I-O2 injections, pending3,
unsupported4, compiler/archive/prior-output/cleanup and late no-replace controls
pass. New caller setup build2/linker1 and source4/diagnostic4 remain retained;
only missing existing Fold TU/target entry names were corrected. This is a fixed
actual mockup product command, not Native LinkerScript/arbitrary-artifact success.
Manual role/action/storage/complete checked Scope/general native remain open.

Consumed Merge operational publication/current-review facts22:00/22:23UTC:
C48 task8a81cefa0899bf89b58963e7d2e88e3701d2807b/prototype Main
784c9bfea5d49cd9161ae51d8b7444f2abf59ef3 and C49 task
e076bc32da59585966a58c0d57075c8178875742/prototype Main
b563796e6a6f6058df5c4c5593e4f407229b5389 are pushed/remote exact with independent
current O2/full affected SAN53 each/code/table/receipts qualification. Their
submitted snapshots/pending-parent statements remain immutable history, not new
human design approval. C50 task/current Merge review/Main remain separate pending.

Fresh C49 [actual frame plan](2026-10-04-C-BACKEND-ACC-FRAME-PLAN.md): admitted
parameter/Fold/callback/constructor binder identities now locate all10/12/12
frame slots; paths agree with existing boundaries. Combined source-derived table
888dbff1 drives target role assembly without manually numbered assignments. Fixed
ordinary898-line C SHA53f01aa8 preserves seven source bodies and public signature.
Currentfc52755b/runtime120c206 strict O2/full emitter-inspector-module-client
source/object/archive SAN53 expected rows each pass; source/reused Core341/resource/
rollback/inertness, inherited16 raw endpoint/20 action and new8 frame refusals pass.
Code/header/receipts/table exact across products/phases/example, no unexpected C49
gate failure. Manual role values/action/checked Scope/general native Acc remain
open. C48 exact18 submitted archive/handoff and other17 live paths preserved;
publication/current Root/Main pending at this observation. Exact15 C49 prepared.

Fresh C48 [actual endpoint plan](2026-10-04-C-BACKEND-ACC-ENDPOINTS-PLAN.md):
existing admitted endpoint Match-on-Nat.succ exposes exact nominal LT/quoted
TOTAL-pure Acc indices. Source-derived table14876c35 drives left LT projection,
quoted Acc force and contravariant down domain; no numeric cancellation or source
typing/checking introduced. Fixed ordinary860-line C SHA0faa1758 preserves seven
actual source bodies/gs_sort signature. Currentfc52755b/runtime120c206 strict O2/
full emitter-inspector-module-client source/object/archive SAN53 expected rows
each pass; source/reused Core341/resources/rollback, six endpoints/16 private raw
refusals and inherited12/new8 action mutations qualify. Initial test-only wrong
right3 expectation/abort134/40 rows retained; constructed prior was right4, no
runtime/source/table change. Manual frame/action/checked Scope/general native
Acc/full61 remain open. Exact18 handoff is prepared; no cost/adoption/Goal claim.
Merge reports C46 task497f2a46/Main205eeccc and C47 task3526b9a3/Main099104cc
pushed/remote exact with independent current qualifications. C47 actual readable
CODE was integrated before21:16:56Z; old archives/handoffs remain immutable.

Fresh C47 [actual transport plan](2026-10-04-C-BACKEND-ACC-TRANSPORT-PLAN.md):
new readonly descriptor emission reads every ordered image and both path binders
from existing admitted successor fields. Actual step6/weaken10/lift10 maps and
six distinct descriptive contexts retained; no producer field needed. Runtime
keeps full frame/left-right vectors/raw down/IH/nominal paths, with manual action
interpretation explicit. Ordinary standalone823-line module SHAca73caaa keeps
all seven actual source bodies and original gs_sort signature. Current accepted
fc52755b/runtime120c206 O2/full emitter-candidate-client SAN source/object/archive
50 matching rows each (44zero/4refusal4/duplicate compiler-linker1/prior-output2).
Actual source/reused Core341/resources/depth/rollback and three branches/12 changed
context-image-path-capture refusals pass; six C/H/three provenance/descriptor exact.
Initial map-client missing stdio.h compiler1 remains with37 original rows; only
required include corrected. Pointer/Core inputs O2; no new cost/general native pass.
C47 publication/current Root/Main were pending at its freeze; later facts above.
C46 exact9 archive remains immutable,
only live owning Goal advances for this distinct epoch. Complete checked Scope/
action/source equivalence, native indexed/callable Acc/full61/Goal stay open.

Fresh C46 [capture query plan](2026-10-04-C-BACKEND-CAPTURE-QUERY-PLAN.md):
owned scalar removes obsolete support include and uses inverse existing readonly
pg_term_independent at both free-capture sites. Current fc52755b/runtime120c206
backend/three helpers build O2/SAN; static-transitive58/recursive114/nested40
matching rows each phase,75 cross-phase generated C/H, source/Core/resources and
static41/transitive340 query-error points qualify. Query errors preserve prior
source/header and input owner counts; no source eval/WHNF/substitution/typed-query
advancement during emission. Whole backend/helpers/source/raw clients SAN;
native object/archive libraries O2 with client-only SAN, pointer producer O2.

Current actual Acc checked/trusted now each reaches lowering refusal4 under
O2/SAN using existing --save-materialized profile. Initial inputs-only trusted
pending3/harness assertion1 and new gate missing done2/repeat receipt cc-ar diff1
remain, corrected setup only. No producer fields/walker/checker/schema/erasure
or ABI authority restored. Standalone actual Acc CODE remains exact; general
native indexed/callable/action equivalence still open. C46 publication/current
Root/Main separate pending, proposed task parent62145905. Exact9 [handoff](2026-10-04-C-BACKEND-EPOCH46-HANDOFF.md) pins280 inputs and retained failures.

Consumed Merge C45 reports2026-10-04: exact13 task
6214590570681e7eae2b84baa03c588822d2e389/prototype Main
3fd490fc89b45d30eff33bbedd2841adf8e425f4 pushed/remote exact. Independent current
standalone component/client O2/SAN42 matching rows each/source708/header/body
products qualify. Historical build2/native NOT_RUN records remain immutable.

Fresh C45 [ordinary-product plan](2026-10-04-C-BACKEND-ACC-C-PRODUCTS-PLAN.md):
[standalone actual Acc C](../src/prototype/c_backend/acc_products/example/component.c)
708 lines SHA49d3a079/header246364a1 packages seven exact source-derived bodies
plus labeled manual target primitives/actions/array boundary. Historical unused
manual sorter excluded; only gs_sort exported. Serial current O2/client-source
ASan/UBSan/leak42 matching rows each (39zero/one compiler-linker1/one existing-
output2/one unqualified-input4) pass source/object/archive clients, actual source
output, reused C44 current-Core341 cases, rollback/depth/resources/duplicate/prior
controls and exact body bytes. Pointer/Core O2; not new independent property counts.

Focused current native backend build2 fails before probing: scalar.c includes
absent support.h and uses pg_support_contains; accepted fc52755b/runtime120c206
removed that metadata. Checked/trusted expectations stay explicit NOT_RUN, not
fresh refusal4. Original build and evidence-key setup1 retained. No producer
request to restore fields is inferred. C45 publication/current Root/Main are
reported above; C46 separately corrects that target dependency. Historical C45
build2/NOT_RUN evidence remains unchanged.

Consumed Merge C44 reports2026-10-04: exact11 task
3f5baf12ba7b86c122d3c88c554bb3e32263d162/prototype Main
8cc0aaeca9f4d4cb387073e00bd0d0bbd76349b7 pushed/remote exact. Independent current
fc52755b/runtime120c206 O2/new emitter-runtime-client SAN29 each/Core341/55-line
product/seven bodies qualify. Original failure retained, worker Git unchanged.

Consumed Merge C43 reports2026-10-04: exact13 task22a5e5ea18ecb79e970f1aff4b4ae682f033cf3e
and prototype Main d0b0281b11e7989fe110901b8b9b94310b6e7533 pushed/remote exact.
Independent current accepted c206 O2/new emitter-runtime-client SAN29 matching
rows each/Core341/37-line product/six parent bodies qualify; original failure
retained. Worker Git stays C31/read-only; no accepted C promotion.

Fresh C44 [capture plan](2026-10-04-C-BACKEND-ACC-CAPTURE-CONSTRUCTION-PLAN.md):
target167/interface11 reuses C43 and emits55-line actual successor/down/Nat/Acc/
quoted callback capture construction, retaining exact37-line down prefix. Support42
now has no manual successor/capture body; bounded action/closure representation,
zero-down/storage/Nat/LT/staging remain explicit manual extent. Currentfc52755b/
runtime120c206 O2/new emitter-runtime-composed-client ASan/UBSan/leak29 matching
rows each/Core341/source/action/captures/resource/inert/repeat/refusal/prior-output
pass;185 source/tool/product inputs retained. Initial emit4 omitted static lambda
application with pending argument; corrected target lowering only, original
source/O2/debug binaries/log/empty output preserved. Exact11 task/Main and
independent Root qualification are reported above; historical handoff stays exact.
General/native/full61/accepted/cost remain open.

Consumed Merge C42 publication/current review2026-10-04: exact10 task
c3ef05cb17cc8e58d1bcb35b9ec0ff2e973ca772 and prototype Main2fb449ab30231ec570a2a724c54eb4bb8b39689c
pushed/remote exact. Root current accepted c206 O2/client SAN19zero each/Core341/
fields/path/maps inspection pass; initial Root argument-order/archive/setup errors
remain retained. No worker Git metadata or accepted implementation changes.

Fresh C43 [action plan](2026-10-04-C-BACKEND-ACC-ACTION-DERIVATION-PLAN.md):
target241/interface13 emits37-line actual successor down branch expressions from
admitted Fold/Match/captures/owned transport inputs. Manual C42 action runtime94/
capture support52 remain explicit, six parent source products exact. Current
fc52755b/runtime120 c206 O2/new emitter-runtime-composed-client SAN29 matching
rows each (25zero/four4),341 current-Core sorts/source/action/resource/inert/
determinism/refusal/prior-output controls pass.171 input pins. Initial emit4
expected two Fold operands; actual four retain both index endpoints, corrected
target expectation only, original source/binaries/debug/empty output retained.
Exact13 task/Main and independent Root qualification are reported above; the
historical handoff stays exact. General action equivalence/capture conversion/
main-native Acc/full61/accepted/cost remain open.

Consumed durable Merge reports2026-10-04: ordered exact C32-C40 task chain at
f26f7c587d22767331fd39b296cef785f0254f05 and prototype Main c3de4aa10205305b0efeece94f0cbd460c61561f
are pushed/remote exact; C41 task35612104bb8193fbec96b2a0169a4fc0f6bcf8a1 and Main89aee51b4f3aadc0f6b2ed51e5ce8ef7d9b913f7
are likewise published. Original worker Git stays C31/read-only. Root current
accepted fc52755b/runtime120c206 C40 O2/SAN33/Core341/comparison256 and C41
O2/SAN35/Core341/source-sort/measure pass; generated pieces exact. Composition/
dependency execution is qualified, not every standalone C32-C39 gate. No accepted
C adoption/full61/Goal completion. Historical pending snapshots stay unchanged.
Reviewed Job interpretation exposes existing typed field/boundary direction,
formation/maps/path inputs; numeric index equality does not justify erasure.
No missing producer field established. C42 redirects from proposed packaging to
actual checked down action/refinement/captures on current accepted source.

Fresh C42 [down plan](2026-10-04-C-BACKEND-ACC-DOWN-PLAN.md): manual scoped action
candidate for actual three accessibleSucc branches retains original Acc/LT,
branch endpoints/nominal edge/map cardinality and quote-before-force/raw-down/
folded-IH captures. Current accepted fc52755b/runtime120 c206 inspection exposes
successor right1/left2 (paths2/maps6,10,10), Nat right4/left2, owner counts inert.
Strict O2/whole candidate-client-source ASan/UBSan/leak each19 zero command rows,
341 current-Core sorting observations/source/action/resource/depth/rollback pass.
Six current C41 products copied exactly; no generation of actions is claimed.
151 final input pins; comment-only map-cardinality wording correction has equal
preprocessed streams. No initial C42 gate failure. Exact10 handoff prepared;
task/current Root C42 qualification/Main pending. Full source maps/general action
equivalence/automatic action generation and main-native Acc remain open.

Fresh C41 [measure plan](2026-10-04-C-BACKEND-ACC-MEASURE-PLAN.md): target328/
interface17 emits53-line actual measure List Fold/Measured Match/indexed cons and
outer measurement/Match/accessibility/Acc call expressions. Support38 lines
retain target array construction/copy-out/lifetime; source outer calls now emit.
WorkerE8 O2/full new/reused emitters-runtime-composed-client ASan/UBSan/leak35
matching rows each;341 Core/source/full measured fields lengths0..32/resource/
inert/depth/refusal pass.169 pins/runtime128 exact, parent C36-C40 C/phase images
exact. Initial outer carrier mismatch4 corrected via static pure projection;
measurement success/debug/source evidence retained, original O2 helper identity
not claimed after rebuild. Exact12 archive preserved; later current qualification/
task/Main results are recorded above and in the sole issue table.
Manual checked down transports and Nat32/storage/Nat/LT/array staging remain;
main-native/generalized/full #61/accepted/cost/Goal completion unclaimed.

Fresh C40 [comparison plan](2026-10-04-C-BACKEND-ACC-COMPARISON-PLAN.md):
target206/interface13 emits38-line actual natLessOrEqual callable Nat Fold.
Left constructor/pred snapshots, right Match before deferred left IH and actual
Bool clauses compose with byte-exact sealed C36-C39 products. WorkerE8 terminal
O2/full new/reused emitters-runtime-composed-client ASan/UBSan/leak33 matching
rows each:341 Core sorts/256 Core comparisons/source/direct/lazy extreme/depth/
malformed private tag/inert/refusal/resource controls pass.166 pins/runtime128
exact; initial SAN setup make2 retained/corrected command only. Exact13 ready;
task/current-producer/Main pending. Manual checked accessibility/down transports,
storage/Nat/LT/measure/outer remain explicit; main-native/generalized/refusal/full
#61/Goal/cost/accepted limits unchanged.

Fresh C39 [accessibility plan](2026-10-04-C-BACKEND-ACC-ACCESSIBILITY-PLAN.md):
admitted accessibility/down LT clauses contain explicit right/left Identity
transports; read-only source/view boundary reported to Merge without a producer
bug/new-field claim. Actual natAccessible recurrence now emits Nat Fold/Acc zero
construction/ordered recursive IH/supplied accessibleSucc call. Target241/
interface14 lines emit22 lines; sealed C36 Acc/C37 partition/C38 append bytes
unchanged. Complete selected Core alpha identity with supplied reference prevents
an imported provenance Fold from being mistaken for a different executable.
Changed callback retains refusal4; unsafe initial successful output retained.
WorkerE8 terminal O2/full emitters-runtime-composed-client ASan/UBSan/leak29
matching rows each; actual source sort/subjects0,1,3|,341 Core observations,
resources/inert/depth/manual raw-down branches/source-association/refusal controls
pass.160 input/runtime128 pins exact, phase C/images identical; initial header
build2/premature missing-binary127/alias134/negative-control failures retained.
Manual accessibility successor/zero-down transports, storage/Nat/LT primitives,
comparison/measure/outer entry remain explicit. Exact13 ready; task/current-
producer/Main review pending, no main-native/generalized/accepted/cost completion.

Fresh C38 [actual-append plan](2026-10-04-C-BACKEND-ACC-APPEND-PLAN.md): actual
source List Fold now emits its callable clauses, captured constructor fields and
deferred recursive IH application to the same right. Target243/interface12 lines
emit42 lines; sealed C36 Acc63/C37 partition86 bytes remain exact. The composed
gs_sort entry calls generated Acc/partition/append; accessibility/raw-down,
comparison/measure/Nat32 storage/primitives/outer entry remain manual. WorkerE8
terminal O2/full emitters-runtime-composed-client ASan/UBSan/leak36 expected rows
each match:341 existing-Core/source sorting observations, source append/mutation,
field snapshots/borrowed suffix/deferred malformed-tail/resource/inert/refusal4/
prior-stream controls.157 inputs/runtime128 exact; initial omitted-alias probe
abort6 and strict indentation build2 retained/corrected. Separate exact13 ready;
task/current-producer/Main review pending. No main backend/public profile/ABI/
producer/schema/erasure/generalized native/accepted/cost/full #61 completion.

Fresh C37 [actual-partition plan](2026-10-04-C-BACKEND-ACC-PARTITION-PLAN.md):
existing typed instances suffice to emit actual partition Fold and inline source
decision/lower/upper expressions. Target379/interface13 lines produce86 lines of
C and compose with immutable C36's63-line Acc clause. Captured type/comparison/
pivot, comparison-before-tail order, Partition bound/SizedList indices and every
LT proof field remain explicit; constructor parameters come from exact retained
source classifiers, with no expected-type fallback. WorkerE8 O2/full emitters/
runtime/composed-client SAN32 expected rows each match:341 Core observations,
actual sort/partition/mutation outputs, capture/order/early failure/malformed
Bool, resource/inert/deterministic/refusal4/prior-stream controls.156 input/
runtime128 pins exact; initial strict build2 and composition macro build1 retained
and corrected. Separate exact13 ready; task/current-producer/Main review pending.
Accessibility/append/comparison/measure/storage/outer entry remain manual; no main
backend/public ABI/producer/schema/erasure/generalized native completion claim.

Fresh C36 [actual-clause plan](2026-10-04-C-BACKEND-ACC-CLAUSE-PLAN.md): admitted
Acc branch terms now generate executable SizedList/Partition Match, ordered SEQ,
both child callable-IH applications, pivot constructor and append calls. Captured
A/le/current/down stay explicit. Target339/interface15 lines emit63 lines of C;
manual C33 helper/runtime/outer entry remains separately labeled. WorkerE8 O2
and full emitter-runtime/composed-client SAN25 expected rows each match:341 Core
observations, allocation/depth/rollback, actual source result and pivot-omitting
source mutation (different emitted C/result), inert/deterministic and helper/
motive refusal4/no-partial controls.152 input/runtime128 pins exact; original
context.depth build2 retained/corrected. Separate exact13 handoff ready; task/
current-producer/Main review pending. Main backend/public ABI and native Acc
refusal unchanged; no full automatic helper/runtime/generalized QuickSort claim.

Fresh C35 [Acc Fold plan](2026-10-04-C-BACKEND-ACC-FOLD-PLAN.md): actual admitted
QuickSort occurrence fields/motives/captures produce native Fold/IH plumbing:
original down obtains child Acc, child Fold yields a callable, then apply receives
SizedList. Exact source index/binder associations and A/le captures retained.
New target implementation202/interface13 lines; no main backend/ABI/producer
changes. WorkerE8 O2/full helper-runtime-client SAN11 expected rows each match;
source empty/singleton observations0,1,1/inert/foreign failure staging/depth/
capture/motive/refusal controls pass,151 inputs/runtime128 exact. Initial setup
failures retained. Actual clause expressions are still supplied/manual (sizes0/1
unit only, larger cases refuse4); no generated QuickSort/body completion. Sealed
C32/C33/C34 remain exact; task/current-producer/Main review separate.

Fresh C34 [indexed-view plan](2026-10-04-C-BACKEND-INDEXED-VIEWS-PLAN.md): existing
admitted views directly produce private Nat/LT/Acc/SizedList/Measured/Partition/
List C declarations, retaining source parameters/indices/fields/result images and
Acc's borrowed U(Pi y:A,Pi edge:R y x,F{}Self y) signature. New target code296
lines; no main backend/Linker/ABI/producer/schema changes. WorkerE8 strict O2
and full helper-runtime/client SAN12 expected rows each match; inert/no machine
advances/source counts, unknown/missing/mismatched family and wrong C relation
controls pass. Initial build/comment-image defects retained and corrected. This
extracts representation, not a source algorithm/Fold/motive/capture body; C33
remains the faithful hand-authored executable deliverable. Exact sealed C32/C33
archives remain unchanged; task/current-producer/Main review is separate.

Human-priority C33 delivers readable hand-authored executable
[actual Acc QuickSort C](../src/prototype/c_backend/acc_quicksort_mockup/mockup.c)
and [mockup plan](2026-10-04-C-BACKEND-ACC-QUICKSORT-MOCKUP-PLAN.md). Explicit LT
constructors/both indices/prior, SizedList indices, Partition sizes/bounds,
natAccessible/Acc raw down and distinct folded callable down, type/comparison/
pivot captures are retained. Source original algorithm only; no target substitute
sorter. WorkerE8 strict O2 and candidate/client SAN9 each0, actual source3/Core341
finite comparisons, all down/partition branches and resource rollback pass.
Closed-list native selection still refuses expression/argument/capture4 in both
admission modes. Initial C warnings/helper-link/alias-selection setup failures
remain. Manual executable candidate is not automatic/generalized native lowering,
source erasure, full #61/cost/adoption or Goal completion. Sealed C32 unchanged.

Fresh C32 [array-call plan](2026-10-04-C-BACKEND-ARRAY-CALLS-PLAN.md): five
target owners +97/-12 add optional native_array_calls_v1 under existing native
ABI1, wrapping actual List exports with array conversion/copy-out/local-arena
cleanup. Worker E8 new139/query63/source-sort46 expected rows each O2/client-source
SAN, linker/I-O19/new-profile faults5 pass. Three families/four products/raw,
five payloads/List121/slice4356/sort341/source3+6/510 separate Core fingerprint
comparisons/inert/statuses1-6 pass;84 new/68 prior C/H and314 pins/runtime128 exact.
Initial missing-provider admission4 and symbol-expectation setup1 are retained;
terminal controls pass. New wrappers preserve source algorithms/ABI/producer/
schema/checker/erasure boundaries. Exact19 separate handoff follows.

Fresh C31 exact18 task `cc3cba3495b81628e0a68ec8b45aee4f5447d188` archive/commit/
changed set/parent/message/remote independently match; report `60d27747`.
Current-producer/Main remains unreported; historical submitted bytes unchanged.

Fresh readonly C29 exact16 `dae637f3b9c0b70229214514eb72176718c0f158` / C30
exact13 `ab14e6940fa92a9f5205232e98f6896cb9d6f221` task publications independently
match all blobs/changed sets/parents/messages/remote; report `cac4b432`. Current-
producer/Main results remain unreported; submitted archives unchanged. C31's
[buffer query plan](2026-10-04-C-BACKEND-BUFFER-QUERY-PLAN.md) proves old copy-query6
and source depth4 on300 nodes, then adds opt-in allocation-free validated size_t
List extents under existing native ABI1 (+73/-6 in seven owners). Worker E8 strict
O2 backend/helper0; new63/callback76/record41 each O2/client-source SAN, linker/
I-O19/query faults5 pass. Five payload forms/121 Lists/4356 source slice pairs,
four observations/inert/no allocation/active-arena/long-chain controls pass.
New28/prior60 C/H and300 input/runtime128 pins exact; no unexpected failures.

C30 adds nine ordinary C-module test/example files under unchanged frozen C29
backend: admitted scalar offset-negate/subtract provider, native consumer, separate
declarative API/factories/client. Worker E8 O2/client-adapter-source SAN124 each
(118zero/6compiler-linker-one) pass:16 mixed pairs/both header orders,32 linked+
8 loaded-provider clients,source/readback9,121 Lists/width/two contexts/three
operands/seeds and93 finite shared Trees/context. Public definitions4/18,
32 generated C/H and291 input/runtime128 pins exact; no unexpected failures.
C29 archive remains immutable; live owning docs now advance independently under
the human workflow change. Both task/current-producer/Main reviews remain pending.
See the [native callback module plan](2026-10-04-C-BACKEND-NATIVE-CALLBACK-MODULES-PLAN.md).

Fresh C28 readonly publication verification: all13 task/then-live blobs, changed
set/parent/message and independent remote match `0c852ce6995cbb9a933910fda6d37490291fdcbb`;
report `c28-publication.json` SHA `6ede04c7`. Immutable snapshot unchanged,
current-producer/Main unreported. Distinct [C29 native callback plan](2026-10-04-C-BACKEND-NATIVE-CALLBACK-PLAN.md)
combines existing native carriers with borrowed same-width scalar descriptors
through target mode/dispatch only (+43/-11 in five owners), preserving old profiles.
Worker E8 strict O2 backend/three helpers build0; new76/binary48/signed74 expected
rows each O2/client-source SAN pass. Four products/raw cover781 Lists/width and655
Trees;3432 separate Core full-value map/combine/right-fold comparisons and five
source/readback observations pass. Linker/I-O19/new-profile faults5 pass,74 parent
C/H exact,282 inputs/runtime128 exact. Initial source syntax/intrinsic failures
and two verifier setup errors retained; terminal gates have no unexpected failures.
No producer/schema/checker/erasure/accepted changes or native Acc completion.

Fresh readonly C27 publication observation: all20 committed/then-live blobs,
changed set, parent/message and remote match task
`4b9c10941418fd431db380ead13dd4a68732f825`; report SHA
`0dd8614998385f751ae6ba824888803cc4029bc3a3fb62555c54b6f3e7d831da`.
Historical submitted archive remains exact; current-producer/Main unreported.
Distinct [signed module plan](2026-10-04-C-BACKEND-SIGNED-MODULES-PLAN.md) uses
that unchanged backend:373 expected rows each O2/client-adapter-source SAN pass,
64 mixed triples/both orders/128 linked+32 loaded clients per phase,16 source
observations/client,121 Lists/width and five compiler/linker refusals. Provider
implements admitted constant flags, not a source comparator; no new ABI/producer.

Fresh read-only C26 publication observation: all14 committed/live blobs, changed
set, parent, message and independent remote match task
`d8f8f27e5f028b109bf7d40204becfad375d423c`; retained report `c26-publication.json`
SHA `f79abab6bdbbe6c4773512c095b8eb9cdbae2324e2c2261af854409a0820b569`.
Historical freeze/tar unchanged; Root current-producer/Main remains unreported.
Separate [signed-predicate plan](2026-10-04-C-BACKEND-SIGNED-PREDICATE-PLAN.md)
now implements an opt-in signed callback profile using unchanged admitted source.
At exact C26 + C27 local edits, strict O2 backend/two helpers rebuild0; signed74,
affected Nat69 and signed-static61 expected rows each O2/client-source SAN pass.
Four products/raw verify781 Lists per width and18744 foreign partition selections
per product; admitted constant callbacks separately provide120 Core tags/1248
lengths and eight source observations. Sixty-four affected C/H bytes match C26;
linker/I-O19/five signed faults pass,262 inputs/runtime128 exact. Initial setup
failures retained. No current-producer/native Acc/accepted/cost/full Goal claim.

Fresh C26 at C25 task `728d04b1` + lane edits: representation.c +3/-3 permits
complete already selected finite records by value in the existing branching node
contract. New52/affected branching52/record-List41 expected rows each O2/client-
source SAN, linker/I-O19 pass on qualified worker E8; four observations,222 finite
Trees/layout,888 descriptive Core comparisons and22 parent C/H exact. Original
source syntax rejection, C25 target refusals and first receipt-command mismatch
are retained. Final private native QuickSort probe still refuses body lowering4
while properly selected List identity emits0; SizedList indices and callable Acc
fields are exposed by existing views, with no producer defect/erasure inferred.
See the [recursive record plan](2026-10-04-C-BACKEND-RECURSIVE-RECORD-PLAN.md).

Fresh C25 at exact task `728d04b12c3901c2193cd3232347dfdb544e6fe5`: native unindexed data admits at most
two direct Self fields under the existing borrowed node/caller-arena ABI. Iterative
all-child validation distinguishes finite shared subtrees from cycles and forwards
temporary allocation status3 without output/arena mutation. Final worker E8 strict
O2 build0; Tree46/applied50/List21/value-record41/numeric26 expected rows each
O2/client-source SAN, enum arrays, linker/I-O19 pass. Six observations,202 Trees/
layout and1616 separate Core comparisons pass;18 prior C24 C/H bytes exact.
Runtime128/retained240 inputs and original failures stay pinned. Root current-
producer/Main review is separate; native Acc/QuickSort/full #61 remain open.
See the [multi-tail plan](2026-10-04-C-BACKEND-MULTI-TAIL-PLAN.md).

Fresh read-only C24 publication observation, 2026-10-04 local: exact16 committed
and then-live blobs, changed-file set, message and independent remote match task
`9af7f27a05cae856b5a1da010b1741883a6ba44e`; immutable C24 archive stays exact.
Root C23 receipt reports prototype Main `9d55106409fbc70235deec26cbdcebd748005fec`
pushed/remote exact, current E18+MEM9 source-sort48/nested40 each O2/client-source
SAN plus linker/I-O19 pass. Root preserved its initial relative-image setup failure;
receipt SHA `3cc29374c370a27de040677583a560d9b04985458923aeab8e514a51a24418ec`.
Root C24 receipt: prototype Main `72f4ff47ffe14a574733483a5a33ec0031d2ce01`
pushed/remote exact; fresh E18+MEM9 runtime128 `c414`, all10 records0 including
applied50/source-sort46/nested40 each O2/client-source SAN, linker/I-O19 and26
parent nested C/H exact. Receipt `core-epoch24.json` prefix `310b3ce9`.
Fresh read-only C25 verification independently matches all26 task/live blobs,
changed-file set, parent, message and remote; report `c25-publication.json` SHA
`064cbcc514eccdb32642ec7d3bd35657c28080410a2159fc6bbc6fee52ac45c2`.
C25 current-producer/Main review remains unreported. Separate next work is tracked
in the [recursive record plan](2026-10-04-C-BACKEND-RECURSIVE-RECORD-PLAN.md).

Fresh C24 worker observation, 2026-10-04 local: all13 C23 task/then-live blobs and
independent remote match `1a1e2614427397a79bad7f9a857b24b9d5bdb43c`; C23 Main/
current-producer qualification is unreported. Immutable submission stays exact.
C24 recognizes exact selected retained classifier Terms through the existing
target instance lookup; guarded declaration/builtin heads, no pending operands,
scalar +13/-9 including pending operand phase, no Source evaluation or public generic ABI.
Scratch O2 build0; fixed/alias List/Pair exports0 checked/trusted, unselected/open/
public type controls4. Expanded ordinary-admitted fixtures isolated a generic-call-
under-fold refusal4 in direct/sequenced forms; both failures remain retained.
Preserving the checked pending operand phase corrects both forms. Final worker E8
O2/client-source SAN50 each pass (31zero/1no-fuel3/18refusal4), three products/raw,
eight observations and445 separate existing-Core comparisons. Affected source-
sort46 each (637 Core comparisons), nested40 each and I-O19 controls pass.
Exact16 handoff is separate from Root current-producer/Main review.
See the [applied type plan](2026-10-04-C-BACKEND-APPLIED-TYPE-BINDING-PLAN.md).

Root operational C22 receipt: exact task `8b6debfcf` and prototype Main
`a0a9dc38dde82d8ef410251b1362bb2e7757091c` pushed/remote exact. Current
E18+MEM9 runtime128 `c414202a` rebuilt O2 backend/two helpers; nested40,
recursive114 and integer61 each O2/client-source SAN pass,84 parent C/H bytes
exact. Root checked retained192/runtime128/raw1603 and preserved1839 live files;
only Main Goal provenance reconciled. Receipt prefix699afb1d/rawce041c85.
These are Root reports, not worker C23/C24 current-producer qualification.

Historical C23 worker observation, 2026-10-04 local: all12 C22 task blobs and independent
remote match `8b6debfcf836451b9e972aba9f3f624f24b406e9` and the submitted manifest.
C22 Main/current-producer qualification remains unreported here. Separate C23
binds known selected nominal/host types privately through the existing known-call
path: scalar +16/-5, receipt capability +1/-1, unchanged public ABI/producer/schema.
The unchanged existing source `insertionSortBy`/`insertionSort` now emits ordinary
source/object/archive C products. Fresh worker E8 O2/client-source SAN48 expected
rows each pass (31zero/1no-fuel3/16refusal4), nine source/readback observations and
552 separate existing-Core comparisons. Each client tests341 nonconstant Lists
with two sorts, four insert pivots and identity, signed/Nat/enum identities,
persistent input and transactional resource controls. Affected C22 O2/SAN40 each,
original linker O2 and publication-I/O19 controls pass. Initial representation
selection failures and old fixed-type/sort refusal4 remain retained. See the
[source sort plan](2026-10-04-C-BACKEND-SOURCE-SORT-PLAN.md).
This is native existing-source insertion sorting, not native Acc/QuickSort or
full #61 completion; the latter retains checked/trusted refusal4 coverage.

Historical C22 worker observation, 2026-10-04 local: exact16 C21 task
`eaf5755914a44a855069321b4c64330eb4ca5b03` and independent remote match its
submitted manifest. Root reports C20/C21 prototype Main publication and fresh
E18+MEM9 joint results recorded in the single issue table below; these are Root
results, not worker reruns. Separate C22 demonstrates and corrects nested private
IH captures with scalar delta +7/-2 and no public ABI/producer/schema change.
Strict E8 build and focused O2/client-source SAN40 expected rows each pass
(31zero/1no-fuel3/8refusal4), with eight source/readback observations and120
separate existing-Core comparisons. Affected C21 O2/SAN114 each pass. Initial
direct/known-lambda nested refusal4 logs and original source/script pins remain
retained in C22's published exact snapshot. See the
[nested capture plan](2026-10-04-C-BACKEND-NESTED-CAPTURE-PLAN.md).
Native Acc/QuickSort/full #61 remain open; no producer defect or new authority.

Historical C21 worker observation, 2026-10-04 local: C20 exact10 task
`7b883a8eb123faab0e288661229196ffdebfe232` is committed and remote task ref
matches. All ten committed blobs match its submitted manifest; no Main/current-
producer C20 result is inferred. Immutable C20 evidence remains unchanged while
live prototype files advance independently in C21 under the existing human
workflow. C21 retains known private lambda captures in recursive targets and
inspects known Force/Thunk/application heads without source evaluation. Eight
historical C20 static refusals now have positive source/export/client coverage;
dynamic/effect/callable cases remain refused. Fresh E8 serial O2/client-source
SAN114 expected rows each pass (74zero/2no-fuel3/38refusal4), for standalone and
explicit-source-reexport families. Sixteen source/readback observations, raw inert
emission and 200 separate existing-Core Nat comparisons/family pass. Transitive,
numeric and borrowed native-predicate O2 gates also pass. No ABI/producer/schema
change; native Acc/QuickSort/full #61 remain open. Detailed C21 evidence is in
the [recursive capture plan](2026-10-04-C-BACKEND-RECURSIVE-CAPTURE-PLAN.md).

Root operational receipt, 2026-10-04 local: C19 exact11 task
`0388736ad3e5b7fca30ff2added9d558c0cef96a` and prototype Main
`b8fb56b196f04aecd4f41c9f5e941f652843afd6` are pushed/remote exact. Root reused
qualified E18 runtime128 `efcdeaa4` and backend without rebuilding or broad rerun;
separate-unit119 expected rows each O2/client-adapter-source SAN match
(116zero/3expected compiler-linker-one). Reported raw `fdf44ac0` is
`merge-storage-20261003/c19-current-e18-gates/Root-results.json`. Root verified
48 generated C18 source/header files, all pins and retained149/raw975. Main's
other10 frozen paths are exact; only Goal provenance was reconciled. Prior1807
tracked live bytes were preserved and C20 excluded. Historical C19 snapshot
remains immutable; bounded C20 inspection uses the existing ABI.

Fresh worker C20 evidence, 2026-10-04 local, on task `0388736ad3` plus separate
fixture/client/harness/docs edits: eight direct recursive Int32/Int64 List forms
emit under existing native ABI; eight source-equivalent known/captured
function-argument forms remain refusal4. Dynamic/unsupported controls also remain.
Retained qualified E8 tools/runtime128 are reused, not rebuilt. Final O2/client-
source SAN77 expected rows each (26zero/1no-fuel3/50refusal4) pass; source,
loaded-image execution and three products match six observations. Initial fixture
IH error and receipt-tool-path harness mismatch are retained. No emitter, ABI,
producer or schema change; signed comparator/native Acc/full #61 remain open.
See [C20 handoff](2026-10-04-C-BACKEND-EPOCH20-HANDOFF.md).

Root operational receipt, 2026-10-04 local: C18 exact13 task
`ecd659995e867a997cec0f48ec3d1fe61ea16036` and prototype Main
`7c977d387e53403e8bae045d14ac8996cd95aca5` are pushed/remote exact. Fresh worker
read independently matches all13 task blobs to the submitted manifest and the
remote task ref. Root current-E18 runtime128 `efcdeaa4` rebuild passes module103
and parent predicate69 each O2/client-source SAN; reported raw `51cc33c4` is
`merge-storage-20261003/c18-current-e18-gates/Root-results.json`. All pins and
retained146/raw2082 verify per Root; Main's other12 paths are exact and only Goal
provenance was reconciled. Prior1797 tracked live bytes were preserved and C19
excluded. Original C18 evidence stays immutable; live freeze is released. These
joint results qualify C18, not the separate worker C19/E8 run.

Root operational receipt, 2026-10-04 local: exact C17 task
`d39632b8adc277aa65a5a23669ca6b99d3663e33` and prototype Main
`8dba3ecb801c36b4d0eb92c23f2c13204476922a` are pushed/remote exact. Root's fresh
current E17 runtime128 `0a15914b` rebuild passes predicate69/binary48/unary39
each O2 and client/source SAN, shared58 O2, linker, current-image I/O19,
numeric O2/SAN and five predicate I/O faults. Reported raw evidence `e9fd805d`
is `merge-storage-20261003/c17-current-e17-gates/Root-results.json`; worker C18
does not independently rerun or claim these joint results. All18 non-Goal frozen
paths and 76 legacy C16 generated source/header bytes remain exact. Root reconciled
only the Main Goal; prior1788 live bytes and C18 trials were preserved. Historical
C17 handoff remains immutable; the live freeze is released for distinct C18.

Fresh C16 at C14 implementation `e9d74f70` plus owned target edits, now on the
observed C15 task `ba5d39d1537ffa93770d1705603e4bac37ab14ea`: distinct
`c_callback2_v1` permits borrowed same-width unary/binary pure-total scalar
inputs using existing independent Pi views. Worker qualified E8 strict O2 build
and O2/client-source SAN48 each pass, with 4000 manual and 400 Core comparisons
per product, source20, inert emission, both header orders and sixteen retained
checked/trusted refusals. Affected unary39/module99 O2+99 SAN/I/O19 pass. Original
unary binary refusal remains 4. Exact detail stays in the
[C16 note](2026-10-04-C-BACKEND-CALLABLE-SHAPE-NOTE.md) and separate handoff;
Root later reports exact C16 task `a4a2e988f4b916784b7f3f6dc7b05d04c78692d9`
and prototype Main `0585663c6430ab44929770724bf15d78680d0b48` pushed. Current-E15
runtime128 `3f48e22e` rebuilds backend/Core/inert: binary48 O2+48 SAN, unary39
each/modules99 each/shared58 each/link/I/O19 pass. Original C16 snapshot stays
immutable; distinct C17 proceeds under the independent-lane workflow.

Read-only task observation: all eight C15 committed blobs at `ba5d39d` match its
immutable submitted manifest/tar. Later Root receipt reports exact task
`ba5d39d1537ffa93770d1705603e4bac37ab14ea` and prototype Main
`d219fca70099befb96ce5770f9379be00d3c8a67` pushed; fresh current-E14 runtime128
`abedf677` callback-modules O2/client-source SAN99+99 pass on frozen C14 backend
sources. Root previously reported current-qualified/pushed E13 Main `81f76b3`.
These do not qualify private C16. Historical C15 submission remains unchanged
while distinct C16 live plan/build/docs advance under the independent-lane rule.

Root operational report, received 2026-10-04 local date: exact C14 task
`e9d74f70d33f1830f05a5a04cd3d05f8a01467ed` and prototype Main `6c36dcb` are
integrated/pushed. Current E12 runtime128 `12026914` passes callback39 O2 +39
client SAN, shared58 each, original linker, I/O19 and four import controls.
Explicit-import consumers preserve original output/fuel: omitted imports caused
the earlier fixture rejection, with no frontend policy bug established. Root
preserves historical image mismatch/pre-C14 reproduction and all frozen
implementation/test bytes; Goal provenance is separately reconciled. Worker C15
uses independent test/client files and reuses the qualified E8 binaries.

Fresh C15 at exact task `e9d74f70` plus owned edits: a generated scalar provider
supplies borrowed callback interpretations to two independently emitted callback
consumers. Sixteen product pairs/both header orders, forty clients per phase,
800 Core comparisons/client and twenty source observations pass in serial O2
and client/source ASan/UBSan/leak modes. Each phase has 99 expected status rows,
including source/object/whole-archive duplicate-symbol refusals. Explicit loaded
provider handles close after all synchronous calls. No emitter/producer/schema
change or broader/timing run; worker E8 and Root E12 evidence stay separate.

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

Root report, 2026-10-03: exact C12 task
`97f8d7c28f3438fb1b36d687e0e7efc175d8c6f3` is pushed/remote verified from the
immutable archive, with live bytes preserved. Fourteen current E9+E10 O2 gates,
three affected client/source sanitizer gates and nineteen I/O controls pass.
Prototype Main integration is complete; status publication is pending in this
report. These are Root's combined results, separate from worker qualified-E8
evidence. Current bounded shared-library work routes to #61; older table rows
retain their historical issue IDs.

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
pushed/remote verified with live C12 edits preserved. Main Goal reconciliation
and current E9+E10 gates are Root's separate pending review, not worker evidence.

Fresh C11 at `fe497956c3d0031a0db0c98a13db5c464fcd2a8b` plus owned edits:
source/header/direct publication stream errors now return I/O status 2. The
focused runner passes nineteen status assertions both before (expected old
immediate status 4) and after the fix; close/receipt failures, unsupported 4,
atomic cleanup and prior-output preservation remain explicit. Backend-only
strict O2 uses the exact qualified C9/E8 runtime128; eighteen O2 and eighteen
client ASan/UBSan/leak combinations pass. Generated C/header bytes are unchanged.
Exact files, pinned evidence and sanitizer limits are in the
[C11 handoff](2026-10-03-C-BACKEND-EPOCH11-HANDOFF.md). C10's six submitted hashes
match inspected local task `fe497956`; remote/Main publication is not inferred.

Merge report, 2026-10-03, freshly checked against Git: exact C8 task
`884d5bbe192e128fc98de8f3afcd2d3bce59211d` is committed/pushed, and all 17 old
manifest hashes match that immutable revision. Merge finds the bounded capture
queue/stable order coherent and releases the live freeze for separate work.
That pending status is superseded: Root reports twelve current-E8 O2 gates pass;
C8 prototype Main `dea5fc1` is integrated/pushed in `cf8cdf5`. The reconciled
owning status at published Main `7ca963d` records this integration. This is
prototype integration, not accepted C promotion. C7's exact tested
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
The epoch is frozen for delegated task publication, not Main integration or full
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
remote and live freeze release. Its eleven current-producer gates/Main integration
are pending. Old C7 handoff and `/tmp` manifests stay unchanged; new Goal edits
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
| #61 bounded public callback inputs, C14 | Exact task `e9d74f70d33f1830f05a5a04cd3d05f8a01467ed`: opt-in borrowed synchronous same-width unary Int32/Int64 pure-total callback ABI; seven implementation files +84/-20, immutable23 snapshot. | Worker E8 O2/client-source SAN39 each, 400 Core cases/product, source20 and twelve refusals pass. Root E12 `12026914`: callback39+39/shared58each/link/IO19/import4 pass; explicit imports resolve fixture omissions, original output/fuel retained. | Task/prototype Main `6c36dcb` integrated/pushed by Root; no accepted promotion. | Caller purity/totality/interpretation/lifetime preconditions remain. Returned/boxed/multiargument/mixed-width/dependent/effectful callbacks and native Acc remain excluded. | Separate C15 ordinary generated-provider/module composition tests. |
| #61 ordinary generated callback modules, C15 | Exact task `ba5d39d1537ffa93770d1705603e4bac37ab14ea`: three provider/adapter/harness files, 231 lines; existing callback ABI unchanged, all8 blobs match immutable snapshot. | Worker E8: O2/SAN99 each; 16 product pairs, both header orders, 32 linked +8 loaded-provider clients/phase, 800 Core comparisons/client, source20 and three duplicate refusals pass. Root current-E14 `abedf677`: frozen-C14 backend module O2/client-source SAN99+99 pass. | Root reports task/prototype Main `d219fca70099befb96ce5770f9379be00d3c8a67` pushed. C14/C15 submitted snapshots preserved; no accepted promotion. | Valid provider success and synchronous context/code lifetime required; SAN covers clients/source bodies only. No general failure propagation, escaping ownership or nominal exchange. | Preserve published C15; distinct C16 binary profile handoff. |
| #61 bounded binary callback inputs, C16 | Exact task `a4a2e988f4b916784b7f3f6dc7b05d04c78692d9`: separate `c_callback2_v1`/`callback2_direct_v1`, unary/binary same-width Pi contract, flat C signatures and old unary binary refusal4; seven implementation files +66/-21. | Worker E8 O2/client-source SAN48 each; 4000 manual +400 Core comparisons/product, source20, inert/header controls; affected unary39/module99+99/I/O19 pass. Root current-E15 `3f48e22e`: rebuilt binary48+48/unary39each/modules99each/shared58each/link/I/O19 pass. | Root reports exact task/prototype Main `0585663c6430ab44929770724bf15d78680d0b48` pushed; immutable18 evidence retained. | Caller pure-total interpretation/lifetime; no arity3/mixed-width/boxed/effect/dependent contract, source authority or native Acc completion. | Separate C17 native predicate/container boundary. |
| #61 selected native predicates, C17 | Exact task `d39632b8adc277aa65a5a23669ca6b99d3663e33`, immutable19 [handoff](2026-10-04-C-BACKEND-EPOCH17-HANDOFF.md): opt-in Nat32 unary/binary predicates returning selected two-case enums, transactional filtering/partitioning and unambiguous alias names. | Worker E8 O2/client-source SAN69 each, affected controls pass; initial failures retained. Root current-E17 `0a15914b`: rebuilt predicate69/binary48/unary39 each O2/SAN, shared58 O2/link/current-image I/O19/numeric O2/SAN/five predicate I/O faults pass; raw `e9fd805d`. | Root reports task and prototype Main `8dba3ecb801c36b4d0eb92c23f2c13204476922a` pushed/remote exact. Only Main Goal reconciled; historical evidence preserved. | Borrowed pure-total interpretation/lifetime/readable storage and target Nat32/depth/allocation bounds; limited SAN. No native Acc/QuickSort, indexed/callable fields or accepted promotion. | Preserve published predicate contracts in distinct C18 ordinary-module composition. |
| #61 generated native predicate modules, C18 | Exact task `ecd659995e867a997cec0f48ec3d1fe61ea16036`, immutable13 [handoff](2026-10-04-C-BACKEND-EPOCH18-HANDOFF.md): reversed provider enum, two nominal consumers, explicit C tag/array adapters and callback-local arena; emitter/ABI unchanged. | Worker E8 O2/client-source SAN103 each, 96zero/7expected compiler-one; 32 linked+8 loaded clients/phase, source93/client and resource/type/symbol controls. Initial setup/depth-test failures retained. Root current-E18 `efcdeaa4`: rebuilt module103 and parent69 each O2/SAN match, raw `51cc33c4`. | Task blobs/remote independently match; Root reports prototype Main `7c977d387e53403e8bae045d14ac8996cd95aca5` pushed/remote exact, other12 frozen paths exact and Goal provenance reconciled. | Provider success, interpretation and synchronous storage/context/code lifetime; limited SAN. Native Acc/QuickSort/full #61 remain open. | Preserve published C18 contracts in distinct C19 translation-unit clients. |
| #61 public predicate translation units, C19 | Exact task `0388736ad3e5b7fca30ff2added9d558c0cef96a` + [unit plan](2026-10-04-C-BACKEND-PREDICATE-UNITS-PLAN.md): declarative C adapter header, separate factories/client and unchanged callback descriptors returned by value; no backend/producer/schema change. | Worker E8 O2/client-adapter-source SAN119 each (116zero/3expected compiler-linker-one); opposite header orders, 32 linked+8 loaded clients/phase and source93/client. Root reused qualified E18 `efcdeaa4`: same119 rows each O2/SAN, raw `fdf44ac0`; 48 generated C18 C/header files and all pins exact. | Root reports task and prototype Main `b8fb56b196f04aecd4f41c9f5e941f652843afd6` pushed/remote exact; other10 frozen paths exact, Goal provenance reconciled. | Borrowed provider API/code/storage lifetime and valid provider success; limited SAN. No A Program returned-callback ABI, native Acc/QuickSort or accepted promotion. | C20: inspect admitted known/captured integer predicates and dynamic refusal under existing native ABI; no new ABI activation. |
| #61 integer predicate/List boundary, C20 | Exact task `7b883a8eb123faab0e288661229196ffdebfe232` + [inspection plan](2026-10-04-C-BACKEND-INTEGER-PREDICATE-PLAN.md): admitted direct Bool forms, signed List clients and parameterized refusals at that revision; emitter/ABI unchanged. | Historical worker77 expected rows each; Root E18+MEM9 `c414202a`: integer77 each O2/client-source SAN and parentunit119 each pass,48 C/H exact; raw `3a412138`. Initial failures retained. | Exact task independently verified; Root prototype Main `f8134616dd29ff9dca11d307a302f27f4bc3a958` pushed/remote exact. C20 snapshot unchanged. | Static captures advanced by C21; dynamic signed predicates advance only in C27's separate opt-in profile. Original native ABI retains refusals; admitted signed comparison/indexed/callable Acc/native QuickSort/full #61 remain open, with limited SAN/borrowed/depth contracts. | Preserve historical C20; live integer61 gate tests eight static wrappers positively and retains17 checked/trusted refusals. |
| #61 recursive known captures, C21 | Exact task `eaf5755914a44a855069321b4c64330eb4ca5b03`, scalar SHA `30acb4b3`, [capture plan](2026-10-04-C-BACKEND-RECURSIVE-CAPTURE-PLAN.md): private known thunk heads and dense native capture parameters under existing ABI. | Worker E8 O2/client-source SAN114 each, sixteen source/readback observations and200 Core Nat comparisons/family; integer61 each and three affected O2 gates pass. Root E18+MEM9 `c414202a`: recursive114/integer61/parentunit119 each O2/SAN +three affected O2 gates pass;48 C/H exact, raw `942062b6`. | All16 task blobs/remote independently verified; Root prototype Main `8c00c80f97f7a7413b4b088efc94999fe58d33ff` pushed/remote exact. Historical snapshot unchanged. | C21 nested-IH refusal advanced only by separate C22; dynamic/callable/indexed contracts and native Acc/QuickSort/full #61 remain open. | Preserve C21 evidence; separate bounded nested private IH capture correction and fresh freeze. |
| #61 nested private IH captures, C22 | Exact task `8b6debfcf836451b9e972aba9f3f624f24b406e9`, scalar +7/-2, [nested plan](2026-10-04-C-BACKEND-NESTED-CAPTURE-PLAN.md): private IH/recursive identities, dense represented captures; existing ABI. | Worker E8 O2/SAN40 each, eight observations/120 Core comparisons and affected114 each. Root E18+MEM9 `c414202a`: nested40/recursive114/integer61 each O2/SAN pass,84 parent C/H exact; rawce041c85. Initial refusals retained. | All12 task blobs/remote independently verified; Root prototype Main `a0a9dc38dde82d8ef410251b1362bb2e7757091c` pushed/remote exact. Historical evidence unchanged. | Dynamic/effect/callable/indexed contracts and native Acc/QuickSort/full #61 remain open; borrowed/depth/limited SAN. | Preserve C22 in later separate known-type/native-call work. |
| #61 private selected types/native source insertion sort, C23 | Exact task `1a1e2614427397a79bad7f9a857b24b9d5bdb43c`, scalar +16/-5/receipt +1/-1, [source sort plan](2026-10-04-C-BACKEND-SOURCE-SORT-PLAN.md): private type bindings let unchanged admitted insertion sorting emit C; ABI unchanged. | Worker E8 O2/SAN48 each, nine observations/552 Core comparisons;341 Lists/client/two sorts/four pivots/identity/resources. Root E18+MEM9 `c414202a`: source-sort48/nested40 each O2/SAN,26 C/H exact, linker/I-O19 pass; initial image-token setup failure retained, reportd69730eb. | All13 task/then-live blobs/remote independently verified; Root prototype Main `9d55106409fbc70235deec26cbdcebd748005fec` pushed/remote exact. Submitted snapshot unchanged. | Exact selected applied arguments advance separately in C24. Dynamic/effect/indexed/callable Acc/QuickSort, borrowed/depth/limited SAN remain. | Preserve C23; bounded downstream representation work stays separate from native Acc/QuickSort. |
| #61 selected applied types/value-phase calls, C24 | Exact task `9af7f27a05cae856b5a1da010b1741883a6ba44e`, scalar +13/-9, [applied type plan](2026-10-04-C-BACKEND-APPLIED-TYPE-BINDING-PLAN.md): exact selected classifier identity and pending operand value phase; no Source evaluation/public generic ABI. | Worker E8 O2/SAN50 each, eight observations/445 Core comparisons,341 Lists/client/finite Pair/captured type/fold/resource controls. Affected source-sort46 each/637 Core, nested40 each/I-O19 pass. Root E18+MEM9: all10 records0, applied50/source-sort46/nested40 each O2/SAN and linker/I-O19,26 nested C/H exact; original failures retained. | All16 task/then-live blobs, changed-file set and remote independently verified; Root prototype Main `72f4ff47ffe14a574733483a5a33ec0031d2ce01` pushed/remote exact, receipt310b3ce9. Submitted snapshot unchanged. | Exact selected classifiers only; arbitrary open/dependent/indexed/callable contracts, native Acc/QuickSort/full #61 remain open; borrowed/depth/limited SAN. | Preserve C24; separate bounded recursive finite-record probe. |
| #61 native Acc/QuickSort (historical #44/#49) | C4 `a3b6bce` pins native refusal; C7-C9 retain it. C23 separately advances existing-source insertion sort. C26 private probe corrects selection to `data_of empty List`. | Worker E8 final six rows: identity emits0 checked/trusted, actual source QuickSort body4 and indexed SizedList4 checked/trusted; image unchanged/no failed products. Existing views expose List indices0, SizedList/Acc indices1 and callable Acc field. Initial faulty object-index scan retained. Structural FFTT remains separate. | Earlier task/prototype Main published; no native Acc/QuickSort epoch publication or current-producer C26 qualification. | Actual indexed/callable applicability/erasure contract unresolved; no producer defect or missing field established. | Concrete source-semantic need routed through Merge; retain native4 and avoid checker/erasure invention. |
| #61 two direct Self fields, C25 | Exact task `728d04b12c3901c2193cd3232347dfdb544e6fe5`: representation +17/-4, nodes +48 and status/receipt metadata; [multi-tail plan](2026-10-04-C-BACKEND-MULTI-TAIL-PLAN.md). Existing borrowed arena ABI, iterative active/completed graph validation; no producer field. | Worker E8 Tree46/applied50/List21/value-record41/numeric26 each O2/client-source SAN; enum arrays875/product/linker/I-O19 pass. Six observations,202 finite Trees/layout,1616 separate Core comparisons;18 C24 C/H exact. Initial setup failures retained. Root joint qualification pending. | All26 committed/live blobs, changed set/message/remote independently verified; task pushed. Main review separate; historical C25 snapshot unchanged. | Readable immutable borrowed nodes, O(V) temporary storage/quadratic validation, depth256; three-tail/callable/indexed/non-List record refusals. Native Acc/QuickSort/full #61 open. | Root current-producer review; C26 finite-record candidate and concrete indexed/callable routing remain separate. |
| #61 finite records in branching nodes, C26 | Exact task `d8f8f27e5f028b109bf7d40204becfad375d423c`: representation.c +3/-3; existing finite-value validators/borrowed node ABI. [Record-tree plan](2026-10-04-C-BACKEND-RECURSIVE-RECORD-PLAN.md); former C25 RecordTree refusal explicitly positive. | Worker E8 new52/branching52/record-List41 each O2/client-source SAN, linker/I-O19 pass; four observations,222 finite cases/layout,888 separate Core comparisons,22 parent C/H exact. Original source/target/receipt failures retained. | All14 task/then-live blobs, changed set/parent/message/remote independently verified; task pushed. Root current-producer/Main review separate. Historical C26 archive unchanged. | Complete earlier selected finite records only; recursive aggregates, non-branching non-List records, missing/later/callable/indexed/effect contracts refused. Borrowed/depth/temporary storage/limited SAN; native Acc/QuickSort/full #61 open. | Root review; separate C27 signed predicate ABI probe under existing authority. |
| #61 borrowed signed predicates/List partitioning, C27 | Exact task `4b9c10941418fd431db380ead13dd4a68732f825`; [signed plan](2026-10-04-C-BACKEND-SIGNED-PREDICATE-PLAN.md). Opt-in same-width unary/binary Int32/Int64 to selected two-case enum, with no-arena private status propagation. | Worker E8 signed74/Nat69/signed-static61 each O2/client-source SAN, linker/I-O19/signed faults5 pass;64 parent C/H exact,262 inputs/runtime128 exact. Four products/raw:781 Lists/width,18744 foreign selections/product;120 admitted Core tags/1248 lengths, eight source observations. Initial setup failures retained. | All20 task/then-live blobs, changed set/parent/message/remote independently verified. Current-producer/Main review pending. Submitted snapshot unchanged. | Borrowed pure-total provider interpretation/lifetime/nonoverlap, depth256/limited SAN;13 checked/trusted refusal pairs. Foreign signed comparisons are ABI controls, not admitted source comparator or native Acc/QuickSort/full #61 completion. | Root review from immutable snapshot; separate C28 generated-provider units. |
| #61 ordinary signed predicate translation units, C28 | Exact task `0c852ce6995cbb9a933910fda6d37490291fdcbb`; [signed module plan](2026-10-04-C-BACKEND-SIGNED-MODULES-PLAN.md). Native generated provider, two nominal consumers and separate declarative C factories/client; ABI/backend/producer unchanged. | Worker E8 reused exact C27 backend/pointer:373 rows each O2/client-adapter-source SAN (368zero/5expected-one),64 mixed triples/both orders/128 linked+32 loaded clients;121 Lists/width,16 source/readback observations/client, definitions4/15/15.271 inputs/runtime128/48 generated C/H exact; no unexpected failures. | All13 task/then-live blobs, changed set/parent/message/remote independently verified. Current-producer/Main review unreported; immutable13 preserved. | Provider success/constant-source interpretation/API/context/code lifetime, nonoverlap/depth256 and limited SAN; no source comparator/native Acc/QuickSort/full #61/adoption/cost completion. | Root review from immutable snapshot; separate C29 native scalar callbacks. |
| #61 native scalar callbacks/maps/reductions, C29 | Exact task `dae637f3b9c0b70229214514eb72176718c0f158`, five target owners +43/-11 and seven fixture/client/helper/gate files; [native callback plan](2026-10-04-C-BACKEND-NATIVE-CALLBACK-PLAN.md). Separate profile reuses same-width unary/binary descriptors/native carrier/arena/call code. | Worker E8 new76/binary48/signed74 each O2/client-source SAN, linker/I-O19/faults5;781 Lists/width,655 Trees,3432 Core/source5,74 parent C/H/282 pins/runtime128 exact. Initial failures retained. | All16 task blobs/changed set/parent/message/remote ancestry independently verified. Current-producer/Main unreported; immutable snapshot unchanged. | Borrowed pure-total interpretation/lifetime/nonoverlap/depth256/limited SAN;17 checked/trusted refusal pairs. No foreign error/returned closure/arity3/mixed widths/effects/indexed/callable Acc/QuickSort/full #61/cost/adoption completion. | Root current-producer/Main review; preserve published contract during C31 size-query work. |
| #61 generated scalar/native callback translation units, C30 | Exact task `ab14e6940fa92a9f5205232e98f6896cb9d6f221`, nine test/example files436 lines, [module plan](2026-10-04-C-BACKEND-NATIVE-CALLBACK-MODULES-PLAN.md). Generated scalar provider/native consumer/separate declarative factories/client; no backend/ABI/producer change. | Worker E8 O2/SAN124 each (118zero/6expected-one);16 pairs/both orders/32 linked+8 loaded clients,source9,121 Lists/width/two contexts/right-folds/93 Trees per context;32 C/H/291 pins/runtime128 exact; no unexpected failures. | All13 task blobs/changed set/parent/message/remote independently verified. Current-producer/Main unreported; immutable snapshot preserved. | Successful pure-total provider interpretation/API/context/code/storage lifetime, nonoverlap/depth256/limited SAN; no foreign error protocol/source closures/native Acc/QuickSort/full #61/cost/adoption completion. | Root current-producer/Main review; preserve source/client evidence during distinct bounded downstream work. |
| #61 finite foreign copy-buffer size query, C31 | Exact task `cc3cba3495b81628e0a68ec8b45aee4f5447d188`, seven target owners +73/-6 and seven example/test files379 lines; [query plan](2026-10-04-C-BACKEND-BUFFER-QUERY-PLAN.md). Opt-in native_buffer_query_v1/existing native ABI1 adds allocation-free validated size_t extent for existing selected Lists; old profiles unchanged. | Worker E8 strict O2 backend/helper0; new63/callback76/record41 each O2/SAN, linker/I-O19/query faults5 pass. Five payloads,121 Lists/4356 slice pairs/source4/300 nodes/no allocation/inert/status controls;28 new/60 prior C/H and300 pins/runtime128 exact. No unexpected failures. | All18 task blobs/changed set/parent/message/remote independently verified; current-producer/Main unreported. Submitted snapshot unchanged. | Readable immutable finite inputs/output nonoverlap; overflow6 guard inspected, SIZE_MAX-node case unexecuted; limited SAN. Node-based copy boundary only; native indexed/callable Acc/QuickSort/full #61/cost/adoption open. | Root current-producer/Main review; preserve query contract during separate array-call work. |
| #61 ordinary array calls to source List functions, C32 | C31 `cc3cba34` + five target owners +97/-12 and ten example/test files; [array-call plan](2026-10-04-C-BACKEND-ARRAY-CALLS-PLAN.md). Optional native_array_calls_v1/existing native ABI1 wraps actual exports with temporary-arena conversion/copy-out/cleanup. | Worker E8 new139/query63/source-sort46 each O2/client-source SAN, linker/I-O19/faults5 pass. Three families/four products/raw/five payloads/List121/slice4356/sort341/Core510/source3+6/inert/statuses1-6;84 new/68 prior C/H/314 pins/runtime128 exact. Initial setup failures retained. | Root reports exact task `10929483918c882e33570eae7cadfbd0c2afa3c4` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Borrowed immutable lifetime/nonoverlap/default depth256/allocation/Nat32/capacity contracts;13 checked/trusted refusal pairs; limited SAN, output SIZE_MAX overflow unexecuted. No zero-copy slices/native indexed-callable Acc/QuickSort/full #61/cost/adoption. | Seal for Root publication/current-producer review; human09:16:56Z priority supersedes further List conveniences with actual Acc QuickSort mockup by21:16:56Z. |
| #61 human-priority actual Acc QuickSort C mockup, C33 | New hand-authored closed-Nat executable candidate [mockup.c](../src/prototype/c_backend/acc_quicksort_mockup/mockup.c) SHA762f264c plus declarative header/tests; actual LT/Acc/down/index/partition/capture correspondence, no alternate sorter or generated claim. | WorkerE8 fresh actual source admission/reference3, O2/full candidate-client SAN9 each0;341 existing-Core fingerprint/length/range observations and all down/partition/resource branches. Native closed-list controls checked/trusted still4. Initial setup failures retained; joint qualification pending. | Root reports exact task `53f51769c328150258edc9e0dbad8405e6ce1028` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Manual Nat/LT only; automatic expression/motive/capture lowering and arbitrary type/comparator closure conversion remain gaps. Scoped arena/depth256/node65536/Nat32/borrowed lifetime; unreachable zero-down foreign metadata refuses. No full #61/cost/adoption/Goal closure. | Root review the submitted executable CODE; preserve archive and defer List conveniences. |
| #61 actual indexed/callable declaration extraction, C34 | C31 cc3cba34 plus new private indexed_views/ code/interfaces/helpers; [plan](2026-10-04-C-BACKEND-INDEXED-VIEWS-PLAN.md). Existing admitted descriptors yield seven C declarations, exact parameter/index/field/result images and Acc.down signature; target code296 lines, no main backend/ABI change. | WorkerE8 O2/full helper-runtime/client SAN12 expected rows each match; original runtime128 exact/inert counts/no machine work/lexical factory/source-index/missing LT/Bool-Nat mismatch/wrong C relation controls. Initial failures retained; no Root qualification inferred. | Root reports exact task `a375a25144e8cd33e2bc139683f7477ae6e25da6` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Header only; borrowed code/context/data lifetime, symbolic index consistency and callback interpretation remain preconditions. No emitted sorting/Fold/IH/motive/capture body, generalized instance/checker/erasure/full #61/cost/adoption claim. | Root review declarations; next bounded step inspect actual Fold/IH/capture body views, retain automatic native refusal and manual C33 distinction. |
| #61 actual Acc Fold callable-IH adapter, C35 | New private acc_fold/ [plan](2026-10-04-C-BACKEND-ACC-FOLD-PLAN.md): target202/interface13 lines derive af_fold/af_force_down/af_apply from actual admitted QuickSort clause/field/IH/capture associations. Original child Acc versus callable Fold remains explicit; no main backend/ABI change. | WorkerE8 O2/full helper-runtime-client SAN11 expected rows each match; empty/singleton actual source0,1,1, both IH calls/capture staging/depth/motive and wrong C IH refusal;151 inputs/runtime128 exact. Initial failures retained; no Root qualification inferred. | Root reports exact task `6e401b2d65305ce4b1cade3766619b74551df042` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Automatic plumbing only; clause expressions supplied/manual, unit sizes0/1 only/larger refusal4. Borrowed/index/interpretation/closure lifetime preconditions; depth covers nested clause construction, no callback side-effect rollback. No generated full sorting/accepted/cost/full #61 completion. | Root review sealed adapter; separate C36 emits actual clause expressions using C33 manual helper/runtime composition. |
| #61 actual Acc clause expression lowering, C36 | New private acc_clause/ [plan](2026-10-04-C-BACKEND-ACC-CLAUSE-PLAN.md): target339/interface15 lines inspect actual branch terms and emit63-line executable Match/SEQ/partition/both-IH/pivot/append body; manual support32 lines includes immutable C33 helpers. | WorkerE8 O2/full emitter-runtime/composed-client SAN25 expected rows each match;341 existing-Core observations, actual source and admitted pivot-omitting mutation, inert/deterministic/refusal4/no-partial/resource controls;152 inputs/runtime128 exact. Initial build2 retained. No Root qualification inferred. | Root reports exact task `68b8885fa5077008e1050ff74668ed3458cd28ac` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Historical C36 clause generated; its accessibility/partition/append/comparison/measure/outer entry and closed Nat32 representation remain manual. Audited helper mapping, depth256/node65536/Nat32/borrowed lifetime limits; unsupported effect/Identity/dynamic/motives/layouts explicit. No full generalized/accepted/cost/#61/Goal completion. | Root review sealed clause; separate C37 replaces its manual partition path with actual source-generated helper expressions. |
| #61 actual Acc partition/helper expression lowering, C37 | New private acc_partition/ [plan](2026-10-04-C-BACKEND-ACC-PARTITION-PLAN.md): target379/interface13 lines emit86-line actual Fold/decision/lower/upper body with exact constructor-instance parameters; sealed C36 clause63 unchanged. | WorkerE8 O2/full emitters-runtime/composed-client SAN32 expected rows each match;341 Core/source/split/mutation/capture-order/early failure/malformed Bool/resource/inert/refusal4/prior-stream controls;156 inputs/runtime128 exact. Initial build2/macro composition build1 retained. No Root qualification inferred. | Root reports exact task `e0c7f26ded44b93a8c9662afcb0d92b576a8a2a6` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Historical C37 append remains manual; separate C38 advances it. Nat32/storage/LT primitives/accessibility/comparison/measure/outer entry remain manual. Static classifier-instance availability, depth256/node65536/Nat32/borrowed lifetime/provider interpretation limits; generic/dynamic/effect/Identity/motives and main native Acc refusal remain explicit. No full generalized/accepted/cost/#61/Goal completion. | Root review sealed partition; separate C38 actual callable append composition. |
| #61 actual Acc callable append lowering, C38 | New private acc_append/ [plan](2026-10-04-C-BACKEND-ACC-APPEND-PLAN.md): target243/interface12 emits42-line actual callable Fold, copied original fields and deferred tail; existing gs_sort declaration in module.h, C36/C37 products unchanged. | WorkerE8 terminal O2/full emitters-runtime-composed-client SAN36 matching rows each;341 Core/source/resource/capture/deferred-tail/suffix-identity/inert/mutation/refusal4/prior-stream controls;157 inputs/runtime128 exact. Initial alias-probe abort6/build2 retained. No Root current qualification inferred. | Root reports exact task `66cb550e9b71b83b4c3e3fffd844d07943bb8498` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Historical C38 accessibility remains manual; separate C39 emits Nat recurrence while successor/raw-down still manual. Nat32/storage/Nat/LT/comparison/measure/outer entry and original-whole-left/open/dynamic/effect/Identity/motive/main-native refusals persist; borrowed/depth256/node65536/limited SAN. No generalized/accepted/cost/#61/Goal completion. | Root review sealed append; actual accessibility transport boundary routed separately, source recurrence advances in C39. |
| #61 actual Nat accessibility recurrence/transport boundary, C39 | New private acc_accessibility/ [plan](2026-10-04-C-BACKEND-ACC-ACCESSIBILITY-PLAN.md): target241/interface14 emits22-line actual Nat Fold/Acc construction/SEQ/IH/supplied helper call; complete Core/reference guard; sealed C36-C38 bytes unchanged. | WorkerE8 terminal O2/full emitters-runtime-composed-client SAN29 matching rows each;341 Core/source subjects/resources/inert/manual down branches/depth300/refusal controls;160 pins/runtime128 exact. Header2/premature127/alias134/unsafe provenance selection retained and corrected. Readonly typed transport/changed-Core probes O2 only; no Root current qualification. | Root reports exact task `be023665ef408398e554969b4be48db1747865c4` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | accessibleSucc/zero-down transport bodies remain manual: step right transport, weaken/lift left transport; routed readonly through Merge/Job, no producer bug/field claim. Exact supplied source root only; different wrappers/callbacks4. Nat32/storage/Nat/LT/comparison/measure/outer entry, borrowed/depth256/node65536/limited SAN/main-native refusal remain. No generalized/accepted/cost/full61/Goal completion. | Root review Nat recurrence; next emit actual source Nat Boolean comparison without changing transport/producer/erasure authority. |
| #61 actual Acc Nat comparison clauses, C40 | New private acc_comparison/ [plan](2026-10-04-C-BACKEND-ACC-COMPARISON-PLAN.md): target206/interface13 emits38-line actual callable Nat Fold, right Match and deferred IH; sealed C36-C39 products unchanged. | WorkerE8 O2/full emitters-runtime-composed-client SAN33 matching rows each;341 Core sorts/256 Core comparisons/source/lazy extreme/malformed tag/depth/resource/inert/refusal4 controls;166 pins/runtime128 exact. Initial SAN setup make2 retained/corrected invocation; no Root current qualification. Root current accepted fc52755b/runtime120 c206: fresh O2/SAN33 each, Core341 sorts/256 comparisons, generated products exact; dependency execution qualified, not all standalone gates. | Root reports exact task `f26f7c587d22767331fd39b296cef785f0254f05` in ordered C32-C40 publication; branch head `f26f7c58` pushed/remote exact. Root reports prototype Main `c3de4aa10205305b0efeece94f0cbd460c61561f` pushed/remote exact; original worker Git remains C31/read-only. Historical snapshots unchanged. | Source-specific complete Core/reference and actual field/branch checks, not general selection. Manual successor/raw-down/zero-down checked transport, Nat32/storage/Nat/LT/measure/outer; borrowed/depth256/node65536/limited SAN/main-native refusal remain. No generalized/accepted/cost/full61/Goal completion. | Root review actual comparator; next bounded actual measure/outer source gap, transport interpretation already routed through Merge/Job. |
| #61 actual Acc measurement/outer source calls, C41 | New private acc_measure/ [plan](2026-10-04-C-BACKEND-ACC-MEASURE-PLAN.md): target328/interface17 emits53-line List Fold/Measured Match/indexed construction and source measure/accessibility/Acc sequencing; target array staging38 lines, sealed C36-C40 products exact. | WorkerE8 O2/full emitters-runtime-composed-client SAN35 matching rows each;341 Core/source/full measured fields0..32/malformed tail/parent-depth/resource/inert/refusal4 controls;169 pins/runtime128 exact. Initial quoted carrier refusal4/debug retained and corrected; original first O2 binary identity unavailable, Root current accepted fc52755b/runtime120 c206 O2/SAN35 each/Core341/source-sort and measure pass; parent generated pieces exact. | Root reports exact task `35612104bb8193fbec96b2a0169a4fc0f6bcf8a1` and prototype Main `89aee51b4f3aadc0f6b2ed51e5ce8ef7d9b913f7` pushed/remote exact. Historical exact12 archive preserved; original worker Git stays C31. | Source-specific reference/view/known carrier binding, not general selection. Manual checked successor/raw-down/zero-down transport and Nat32/storage/Nat/LT/array staging; depth256/node65536/borrowed/finite SAN/main-native refusals. No generalized/accepted/cost/full61/Goal completion. | C42 supersedes packaging with actual checked down/refinement actions; ordinary modules are already usable. Keep historical manual/generated distinction. |
| #61 actual scoped Acc down actions, C42 | New private acc_down/ [plan](2026-10-04-C-BACKEND-ACC-DOWN-PLAN.md): hand-authored step/right quote-force, weaken/left/raw-down, lift/left/folded-IH with retained original proof/value/private refinement. Six C36-C41 generated bodies unchanged. | Current accepted fc52755b/runtime120 c206 strict O2/client-source SAN19 zero command rows each;341 current-Core sorts/source/action/capture/refusal2/resource/depth/rollback pass. Inspector fields successor3/Nat6 inert;151 pins. No initial worker C42 failure; final comment projection exact. Root current C42 independently repeats O2/client SAN19zero/Core341/fields; pointer/Core/inspector O2, initial Root setup failures retained. | Root reports exact task `c3ef05cb17cc8e58d1bcb35b9ec0ff2e973ca772` and prototype Main `2fb449ab30231ec570a2a724c54eb4bb8b39689c` pushed/remote exact after current qualification. Historical exact10 preserved. Original Git remains C31/read-only; C32-C41 archives exact. | Private closed Nat/LT token retains branch endpoints/edge/map cardinality, not serialized maps or proof of general action equivalence. Zero-down refusal2/manual primitives/staging; Nat32/depth256/node65536/borrowed lifetime/finite SAN/main-native limitations. No full61/accepted/cost/Goal completion. | Derive actual scoped action code automatically from admitted fields and qualify broader correspondence; preserve target/source authority boundary. |
| #61 actual successor down branch emission, C43 | New private acc_actions/ [plan](2026-10-04-C-BACKEND-ACC-ACTION-DERIVATION-PLAN.md): target241/interface13 derives37-line right quote-force/left raw-down/left folded-IH sequencing from actual admitted branch/capture/transport views. Manual C42 action runtime94/capture support52 and six parent source products unchanged. | Current fc52755b/runtime120 c206 strict O2/new emitter-runtime-client SAN29 matching rows each (25zero/four4);341 Core/source/action/resource/inert/determinism/refusal/prior-output checks pass.171 pins; pointer/Core O2 not SAN. Initial emit4 assumed two operands; actual four retain index endpoints, corrected target guard and initial evidence kept. Root independent current O2/new emitter SAN29/Core341/down37 byte-exact qualified. | Root reports exact task `22a5e5ea18ecb79e970f1aff4b4ae682f033cf3e` and prototype Main `d0b0281b11e7989fe110901b8b9b94310b6e7533` pushed/remote exact after independent current qualification. Exact13 history preserved. Historical C32-C42 archives exact; original worker Git C31/read-only. | Manual bounded action semantics/reconstructed Acc/capture/zeroDown/storage/staging; no serialized-map/general equivalence/closure proof. Nat32/depth256/node65536/borrowed/private token/nonoverlap/finite SAN/main-native general barriers persist. No full61/accepted/cost/Goal completion. | Qualify broader actual source action/capture correspondence and bounded native profile without upstream checker/erasure/producer expansion. |
| #61 actual successor Acc/callback capture construction, C44 | New private acc_capture/ [plan](2026-10-04-C-BACKEND-ACC-CAPTURE-CONSTRUCTION-PLAN.md): target167/interface11 reuses C43, emits55-line actual successor/down/Nat/Acc/callback capture product with37-line exact parent prefix. Support42 no longer hand-constructs successor/captures; action layout remains explicit target code. | Current fc52755b/runtime120c206 O2/new emitter-runtime-client SAN29 matching rows each (25zero/four4),341 Core/source/action/capture/resource/inert/repeat/refusal/prior-output pass;185 input pins. Initial emit4 missing static APP with pending argument; bounded target lowering corrected, original source/O2/debug/log/empty result retained. Pointer/Core O2 not SAN. | Root reports exact task3f5baf12ba7b86c122d3c88c554bb3e32263d162/prototype Main8cc0aaeca9f4d4cb387073e00bd0d0bbd76349b7 pushed/remote exact; independent current O2/new emitter SAN29 each/Core341/55-line body qualify. Exact11 history and C32-C43 archives unchanged; worker Git C31/read-only. | Manual bounded action/closure semantics/original Acc representation/zeroDown/storage/Nat/LT/staging; no serialized-map/general correspondence proof. Nat32/depth256/node65536/borrowed lifetime/private token/nonoverlap/finite SAN; arbitrary closures/main-native/general Identity limits persist. No full61/accepted/cost/Goal closure. | Test actual-source composition as ordinary C products and identify concrete main-native boundary, keeping broader correspondence/source authority separate. |
| #61 actual Acc standalone C products, C45 | New private acc_products/ [plan](2026-10-04-C-BACKEND-ACC-C-PRODUCTS-PLAN.md) packages one708-line actual-source module SHA49d3a079/header246364a1/provenanceb7a663b6 with existing gs_sort signature and no unused manual sorter. Seven source bodies exact; target action/storage/array extent labeled. | Current fc52755b/runtime120c206 serial O2/client-source SAN42 matching rows each, source/object/archive/source output/reused Core341/resource/rollback/depth/single symbol/duplicate-symbol1/prior-output2/unqualified-body4 pass. Native expectations explicitly NOT_RUN after current main backend build2 lacks support.h; original build/evidence-key setup1 kept. | Root reports exact task6214590570681e7eae2b84baa03c588822d2e389/prototype Main3fd490fc89b45d30eff33bbedd2841adf8e425f4 pushed/remote exact; independent current O2/full component-client SAN42 each qualified. Exact13 history/C32-C44 archives preserved; original Git C31/read-only. | Bounded manual action/closure representation and general source equivalence unproved; Nat32/depth256/node65536/borrowed immutable/nonoverlap/limited SAN persist. At C45 freeze, general-native capture discovery relies on removed producer support metadata; C46 corrects this separately. No full61/adoption/cost/Goal completion. | Correct stale target capture discovery in a distinct bounded epoch without producer fields/checker/erasure authority; then run current checked/trusted native controls. |
| #61 current native capture-discovery compatibility, C46 | Owned scalar removes stale support include and uses inverse public readonly pg_term_independent at two sites; [plan](2026-10-04-C-BACKEND-CAPTURE-QUERY-PLAN.md). No producer support metadata or new checker/walker. Test-only current materialized profile adapter records transformed argv. | Currentfc52755b/runtime120c206 O2/SAN backend/helpers build; static-transitive58/recursive114/nested40 matching rows each,75 C/H exact, source/Core/resources/inert/static41-transitive340 query faults pass. Actual Acc checked/trusted each lowering4 with existing materialized profile. Initial inputs-only trusted pending3/harness1/shell2/receipt-only diff1 preserved. Whole backend/helpers/source/raw SAN; O2 objects/archive with client-only SAN. | Root exact task497f2a460582b55209b93b8576de3e4328e3c714/prototype Main205eeccccb8ffeb9cc34b80c38f77858e412e898 pushed/remote exact; independent current13 commands/O2-SAN212 each/75 C-H/materialized native4 qualify. Exact9 history preserved; worker Git read-only. | General native indexed/callable Acc and action/closure/source-equivalence remain open; target query work unmeasured. Existing depth/allocation/overflow/borrowed/nonoverlap and unsupported contracts persist. No full61/accepted/cost/Goal completion. | Review exact current correction and source boundary; continue actual-source C critical path without producer/checker/erasure/ABI expansion or List conveniences. |
| #61 actual Acc ordered transport maps/captures, C47 | New acc_transport/ [plan](2026-10-04-C-BACKEND-ACC-TRANSPORT-PLAN.md): source-derived step6/weaken10/lift10 descriptors5646ca54 retain every image, six descriptive context tokens and both path binders; target full frame/left-right vectors keep original down/IH separate. Standalone823-line C SHAca73caaa preserves seven actual source bodies/signature. | Currentfc52755b/runtime120c206 O2/full emitter-candidate-client source/object/archive SAN50 matching rows each, source/reused Core341/resources/depth/rollback/inert/three branches/12 target refusals pass; six C/H/three provenance/descriptor exact. Initial missing stdio.h compiler1/37 rows preserved, include-only correction. Pointer/Core inputs O2. | Root exact task3526b9a3c46abe0fa92ed93bf0c691a73a372605/prototype Main099104ccf3568c3532a59e6215dd730ca665edb9 pushed/remote exact; independent current five commands/O2-full-source SAN50 each/code-table-receipts exact. Delivered before deadline; immutable16 history retained. | Descriptive tokens are not checked Scope certificates; complete context-declaration/action/source equivalence and general native indexed/callable Acc remain open. Bigger action/stack records/cost unmeasured; Nat32/depth256/node65536/borrowed/nonoverlap/overflow/finite SAN persist. No accepted/full61/Goal completion. | Preserve C47 history; C48 derives actual endpoint-type projections, still bounded/manual. |
| #61 actual Acc endpoint projections, C48 | New acc_endpoints/ [plan](2026-10-04-C-BACKEND-ACC-ENDPOINTS-PLAN.md): exact admitted nominal endpoint indices drive LT/quoted Acc/down-domain actions;860-line ordinary module0faa1758/seven actual source bodies/signature unchanged. | Currentfc52755b/runtime120c206 O2/full affected SAN53 matching rows each; source/reused Core341/resources/inert/six endpoints/16 private structural refusals/inherited12/new8 action refusals pass; code/headers/receipts/table exact. Initial wrong test index/abort134 retained. | Merge exact task8a81cefa0899bf89b58963e7d2e88e3701d2807b/prototype Main784c9bfea5d49cd9161ae51d8b7444f2abf59ef3 pushed/remote exact; independent current O2/full affected SAN53 each/code/table qualify. Exact18 history remains immutable. | Manual frame/action/complete checked Scope/source-equivalence/general native Acc remain open; native4/input3 inherited C46. Nat32/depth256/node65536/borrowed/nonoverlap/finite SAN/overflow/cost limits persist; no accepted/full61/Goal completion. | Source field-to-frame binding delivered separately in C49; preserve actual algorithm without List/producer/checker/erasure expansion. |
| #61 actual Acc binder-derived frames, C49 | New acc_frame/ [plan](2026-10-04-C-BACKEND-ACC-FRAME-PLAN.md): admitted parameter/Fold/callback/constructor binders locate all10/12/12 slots; table888dbff1 replaces manually numbered frame assignments. Fixed898-line module53f01aa8/source bodies/public ABI unchanged. | Currentfc52755b/runtime120c206 O2/full affected SAN53 expected rows each; source/reused Core341/resources/inertness/inherited endpoint16-action20/new frame8 refusals pass; products/table exact. No unexpected C49 failure. | Merge exact taske076bc32da59585966a58c0d57075c8178875742/prototype Mainb563796e6a6f6058df5c4c5593e4f407229b5389 pushed/remote exact; independent current O2/full affected SAN53 each/code/table/input pins qualify. Exact15 submitted history immutable; shared Git read-only. | Target role values/action/complete checked Scope/source equivalence/general native Acc remain open. Native4/input3 inherited C46, not new execution. Nat32/depth256/node65536/lifetime/nonoverlap/finite SAN/overflow/cost limits persist; no accepted/full61/Goal claim. | Saved-image ordinary product command delivered separately in C50, preserving fixed/manual extent and ordinary native refusal. |
| #61 actual Acc saved-image C products, C50 | New acc_command/ [plan](2026-10-04-C-BACKEND-ACC-COMMAND-PLAN.md): one admitted-image driver regenerates seven bodies/table and packages ordinary source/object/archive; code/header/ABI exact C49. Owned disk streams/staging and no-replace directory publication. | Currentfc52755b/runtime120c206 O2/full driver-emitter-generated-source/client SAN44 expected rows each/source/reused Core341/resources/inertness/parent refusals/four I-O2/pending3/unsupported4/compiler/prior-output/cleanup/late publication pass. Original dependency build2/linker1 and target-name source4/diagnostic4 retained. | Exact9 handoff prepared after published C49; delegated task/current Merge review/Main integration pending separately. C32-C49 archives and C49 other14 live paths preserved; no shared Git mutation. | Fixed actual mockup with manual role/action/storage/complete checked Scope/source equivalence; main-native4/input3 inherited C46, general native/full61 open. Nat32/depth256/node65536/borrowed/nonoverlap/overflow/finite SAN/cost/accepted limits persist. | Use the readable executable candidate and reproducible ordinary products; identify one concrete remaining manual action/capture boundary with existing source views or route the dependency before upstream edits. |
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

C50 agent result: actual-source body regeneration and ordinary C products now
share one reproducible command rather than historical body-file stitching. The
fixed pack guards and identical code/header keep its manual representation/native
extent explicit. Public admission precedes readonly emission; no new checker or
source-erasure policy is inferred. Next inspect a concrete remaining manual
action/capture boundary using existing admitted views; route a demonstrated
shared dependency before edits. Do not rebuild general source theory or turn
finite passing controls into native/accepted/cost/full Goal completion.

C49 agent result: actual admitted binder positions now determine the target frame,
while role value/action representation and complete source-equivalence remain
explicit manual limits. No new source classification/checking or producer field
is needed. Next consolidate the existing actual-source emitters into one bounded
saved-image product command using the same ordinary signature, keeping its fixed
candidate scope and main-native refusal explicit. This is an agent implementation
choice within prototype scope, not new user approval or generalized native success.
Route a concrete missing shared view before source-owner edits; no List convenience,
new source erasure, cost or full Goal claim.

C48 agent result: affected indices now come from actual typed endpoint operands;
manual frame binding/action representation and complete source-equivalence remain
open. Explicit known constructor dispatch is a bounded target reader, not a new
checker or numeric equality/cancellation principle. No producer dependency is
demonstrated. Keep actual-source Acc/down/capture work and ordinary modules central,
preserve frozen evidence, and route a concrete shared need before source-owner
edits. Finite passing controls and timely C47 publication do not complete #61 or
the Goal; no cost measurement or accepted adoption is inferred.

C47 agent result: actual executable transport/capture code now retains all ordered
source map image values and both nominal path slots, rather than cardinality alone.
Descriptive context tokens preserve raw sharing/distinction; they are not checked
Scope certificates or complete declaration semantics. No producer bug or field
need is established. Retention/validation enlarges target action/stack records,
with no measured cost claim. Remaining action interpretation is manual. Next
inspect actual endpoint types to derive affected LT/Acc index projections through
existing typed inputs; do not invent cancellation/erasure from equal Nat32 values,
replace the algorithm or expand a private checker. C46 archive/raw evidence remain
exact while the sole live owning table records this new material result.

C46 agent result: current native compilation and free capture discovery are
restored by a public readonly syntax query, not reconstructed producer metadata.
Errors and source/target ownership boundaries remain explicit. Completed trusted
exports require the already-existing materialized profile; inputs-only pending
is not a native refusal. These distinctions are test setup and agent decisions,
not new human design approval. General Acc lowering still refuses4 while the
requested executable actual-source standalone module remains usable. Preserve
all old evidence, qualify exact correction independently and stay on that main
source/target critical path; no broader source checking or List conveniences.

C45 agent result: realistic readable executable actual Acc C now has standalone
source/object/archive clients without compiler/runtime linkage. Source algorithm
bodies remain exact; manual action/layout/lifetime choices stay labeled. A current
native build failure establishes a stale target support dependency, not a source
rejection or missing producer field. Correct target capture discovery separately;
do not restore upstream metadata, add a checker or claim general completion.
Original failures and unexecuted checked/trusted probes stay visible in the handoff.

C44 agent result: actual successor Nat/Acc/captured callback construction derives
from source clause bindings and expression operations. Static source application
with a pending argument carries target values without executing/checking source.
The manual bounded action/closure semantics remain explicit, and emitted fields
do not prove general correspondence. Next exercise this actual-source composition
as ordinary C products and pin the concrete main-native boundary, preserving
all source/producer/ABI/refusal limits rather than broadening checking authority.

C43 agent result: source sequencing now determines right quote-force, left/raw
and left/folded-IH calls, rather than manually reproducing those branch bodies.
The bounded target action/refinement semantics and captured immutable Acc
representation remain manual; no general serialized-map/witness/action proof or
automatic closure conversion follows. Keep actual source admission distinct from
target representation validity and finite tests. Broader native/source-capture
correspondence remains the next critical-path obligation, not List conveniences.

C42 agent correction: ordinary modules are already usable, so packaging does not
remove the actual indexed/callable/down barrier. Consume the human critical-path
concern by prioritizing checked Acc/LT transport action/refinement representation.
Reviewed Job interpretation is operational evidence, not new user design approval.
Use existing admitted typed owners/maps/path inputs and current accepted source;
no extra producer field, source erasure or independent checker. Existing C33
manual and C36-C41 expression results remain meaningful partial CODE, not full
native/generalized/main backend completion.

C42 agent result: a manual closed Nat/LT token now retains original values,
nominal branch edge, endpoints and map cardinality through quoted Acc/forced
callback and acted LT. It does not retain full serialized maps or prove arbitrary
witness/action equivalence. Focused current-source evidence qualifies only that
candidate, with immutable source-valid data/token/lifetime preconditions and
extra allocations within the same target bounds. Next derive the actual scoped
actions from admitted fields automatically; broader semantic/main-native
correspondence remains separate. No source checker/erasure/producer authority.

C41 agent decision: retain the measured Nat and SizedList indices as data, read
constructor parameters from exact retained classifiers, and emit source Match/
callee sequencing. Pure-carrier projection binds admitted known helpers without
source evaluation; first refusal was target binding mismatch, not a producer gap.
The array boundary remains explicit target code and the emitted outer call adds
one guarded depth frame. Next actual-Acc milestone is usable ordinary C-module
packaging of this composition; checked down transport interpretation is already
owner-routed. No main-native/generalized/source-erasure authority follows.

C40 agent decision: lower the actual callable Nat Fold clauses with constructor
snapshots and deferred IH only after right Match. Preserve private source Bool
interpretation/status convention and old Acc/partition/append/accessibility bytes.
Test-only mathematical comparison is not the executable sorting comparator.
Unrelated same-reference source keeps refusal; complete reference identity is a
source-specific binding, not general association authority. The remaining actual
algorithm gaps are measure/outer and already-routed checked down transports.
Keep the faithful manual C33 executable distinguished from this increasingly
generated composition; no full native/main/checker/erasure completion claim.

C39 agent correction: provenance edges may retain imported Fold allocations next
to a different selected executable. Context binder/type agreement alone was
insufficient, so require complete selected Core alpha identity with the supplied
admitted source reference before emitting its actual clauses. Keep the changed
callback's original expected refusal and preserve unsafe initial output. This
source-specific binding is not general Fold selection or a private checker.
Actual accessibleSucc includes checked scoped transport operations; do not infer
erasure/proof irrelevance from Nat32 endpoint equality. Read-only interpretation
was routed through Merge/sole Job owner; the faithful manual C33 successor/zero-
down bodies remain supplied. Nat recurrence generation advances the real Acc
path without claiming those bodies/whole source algorithm automatically lowered.
Next source comparison is another actual algorithm gap, not List convenience.

C38 agent decision: actual source Fold captures tag/head/original-tail at callable
construction, while recursive tail Fold remains deferred until the IH force.
Emit actual clause terms and bind the same borrowed right at each application.
The head-discard mutation is a direct-helper test, never a proposed sorter.
Whole-left references require additional base capture threading and refuse4;
the actual append does not use them. Keep sealed Acc/partition bytes and public
ABI unchanged via separate call-site bindings. Next real source-body gap is
accessibility/raw-down, not another List convenience; no private checker/source
erasure inference or generalized closure/ownership policy is authorized.

C37 agent decision: lower actual partition and inline decision/lower/upper helper
terms, including pending operands/SEQ. Existing constructor classifiers retain
the required bound/index parameters, so no new producer view or expected-type
inference is needed. Missing/unsupported/conflicting instance metadata refuses.
Captures and lazy tail recurrence remain explicit; the separate function-like
composition binding redirects only C36's partition call. Source mutation is a
noncandidate direct-partition test. Accessibility/append/comparison/measure/
storage/outer entry remain manual, and generalized closure/ownership/source
erasure policy is still outside this bounded experiment.

C36 agent decision: lower the actual captured clause expressions, preserving
typed source field order and dependent Match's original-down/IH reabstraction.
Use the immutable manual C33 runtime/helpers explicitly; generated body recursion
calls its own gs_apply, not the manual sorter. An admitted pivot-omitting mutation
proves terms/arguments affect generated execution; it is a test, not an alternate
proposed algorithm. This candidate still does not emit helper bodies/outer source
expressions, define public indexed ABI or grant generalized erasure/ownership.
Retain the existing native refusal and defer more List conveniences. C35 full-Nat
pointer component and C36 closed Nat32 composition remain separately classified.

C35 agent decision: use actual admitted Acc clause allocation and IH classifiers
to generate native traversal/sequence plumbing. Preserve source current/index/
type/relation/capture correspondence; original down returns Acc and folded IH
returns a callable. The source-driver admission remains sole authority; target
shape inspection adds no checking/normalization/erasure. Manual unit clauses are
supplied, so their source comparisons prove only observed component composition
at sizes0/1. Full captured source clause/body lowering remains the concrete next
gap; no alternative sorting algorithm or more List conveniences are substituted.
Staged outer result and nested-clause depth controls do not promise rollback of
provider state/allocation or a bound on arbitrary callback execution.

C34 agent decision: derive the actual indexed/callable representations from
existing descriptor telescopes and exact static lexical factories, preserving
indices/type/family/value parameters/proofs. This establishes that no new
producer fields are needed for this representation milestone. Keep the private
header separate from public lowering; C33 sorting body is still manual and
automatic Fold/IH/motive/capture conversion remains open. No new source checking,
normalization, erasure or instance authority. The tiny C client checks layout
usability, not general proof membership or generated sorting. Preserve sealed
archives while the live owning Goal advances; publication remains Root-owned.

C33 agent decision under the human clarification: transcribe the actual admitted
closed-Nat algorithm to executable readable C with all proof data and original/
folded recursive callables visible. Do not claim generated success or infer an
erasure policy. Finite source/Core comparisons qualify this candidate's observed
extent; general automatic representation/motive extraction and source completeness
remain distinct. C32's immutable archive remains the publication source as the
live owning Goal advances with the priority switch. No publication hold applies.

C32 agent decision: remove repeated client arena orchestration through a separate
first-order array-call profile. Reuse existing input conversion, actual generated
native body and atomic copy-out; destroy local nodes before returning. Source
insertion sorting remains the admitted source algorithm, not a handwritten target
sorter. Keep scalar/non-List/callback/recursive non-List signatures explicit
refusals and original profiles byte-exact. Fixed depth256 and real conversion
costs prevent any slice/performance/general recurrence completion claim.

C31 agent decision: expose the already validated finite count before buffer
allocation in an explicit profile, preserving copy-out's unchanged-on-failure
contract. Reuse target validators without a source length evaluator/checker or new
representation. Count is a foreign extent, not source evidence; two-call storage
immutability/nonoverlap remain caller preconditions. Do not claim zero-copy slice
execution or native Acc recurrence from this buffer convenience.

C30 agent decision: exercise generated arithmetic providers through ordinary C
factories rather than cast incompatible output-pointer signatures into descriptors.
Provider calls require no arena and stay usable while the consumer arena is active.
Use one consumer/provider pair and focused16 product combinations; do not repeat
unchanged C29 Core/refusal/broad gates. Success/interpretation/lifetime remain
explicit preconditions; no new status propagation or source equality is inferred.

C29 agent decision: combine existing scalar-return callback descriptors and native
carrier machinery in a separate opt-in profile after ordinary source admission.
Reuse target call/validation/rollback code instead of new ownership or error
semantics. Exact admitted negate/subtract justify finite full-value Core evidence;
offset contexts/Tree tests are separately labeled manual controls. Root review
must qualify its current producer; local E8 evidence grants no upstream authority.

C28 agent decision: verify ordinary generated signed providers through a declarative
API and separately compiled descriptor factories/client. Constant-source providers
permit meaningful cross-module comparison without inventing a signed primitive or
erasure policy. Explicit enum adapters and array copies relate different nominal
products; no Source type equality is inferred. The provider is scalar-only, so it
needs no callback-local arena; consumer rollback/status remains unchanged. Reuse
exact C27 backend/qualified producer and do not duplicate unchanged broad gates.

C27 agent decision: reuse admitted parameter/representation views for a separate
signed predicate profile rather than broaden old contracts. Fixed-width callback
names avoid alias dereferences for builtin types. Carry target status privately
without an arena so invalid enum tags cannot turn into successful Match/record
results; public output remains unchanged on failure. Foreign signed comparisons
exercise existing source recurrences only under caller interpretation/purity
preconditions; admitted constant-source/Core evidence is separately labeled.
No source signed-comparator/erasure/producer authority or native Acc completion.

C26 agent decision: reuse existing active-field validators for complete selected
value records under the existing all-child branching contract, with no owned graph
or new source authority. Actual Acc inspection establishes an indexed/callable
source-semantic decision need, not a producer defect; route through Merge and
retain native refusal. Do not substitute insertion sorting or structural QuickSort
for native Acc completion. Historical C25 record negatives are reconciled only
in this new epoch with positive ordinary clients.

C25 agent decision: support at most two dedicated direct Self fields using existing
admitted views and target node fields. Validation visits all active edges, accepts
shared finite subtrees and rejects cycles with temporary active/completed records.
Forward status3 before any arena/output mutation; preserve chain code and native
depth256. Quadratic validation work is explicit, with no cost/adoption claim.
Historical Tree refusals receive positive successor controls; three-tail, nat32
Tree, callable/indexed/non-List record contracts remain explicit. This is separate
from native Acc/QuickSort and grants no source erasure or producer authority.

C24 agent decision: borrow an exact selected classifier Term through the existing
target applied-instance index, with declaration/builtin guards and no pending
operands. Preserve value-phase carriers when forwarding unconsumed inline-call
arguments; the prior code reused their underlying C computation expression and
reintroduced a computation marker after the value-phase check. This is target
operand bookkeeping, not Source argument evaluation or new type/erasure authority.
Former selected List refusals and both Source fold forms need explicit positive
Core/native coverage; public/unselected/open/indexed cases remain negative.
Root reports C22 integration; independently verified C23 task publication releases
live implementation while its exact historical archive stays unchanged.

C23 agent implementation decision: keep a represented declaration/builtin type
reference as a private identity token, bind it through existing known-lambda
inlining and compare identity in recursive captures. No runtime Universe/type
dictionary, public generic ABI, source normalization or producer/checker change.
Computed/applied types and unselected constants remain unsupported. The receipt's
existing transformations list describes profile capabilities, not a per-call
trace; append the selected-type capability consistently with its current entries.
Use the unchanged admitted source insertion algorithm as an intermediate usable-C
milestone. Native Acc/QuickSort is a separate acceptance obligation. Root owns
exact publication and current-producer/Main review; local E8 results do not imply
that review. The single table distinguishes those states.

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
- [x] Merge reports C7 Main `66fa707` pushed/remote verified, including evidence.
- [x] C8: adopt/verify finite transitive static captures; final twelve O2/nine
  generated-client sanitizer gates and style pass. Exact old refusal is positive.
- [x] Freeze C8 exact source/test/docs; keep applied-List implementation outside it.
- [x] Delegated exact C8 task `884d5bb` committed/pushed; all 17 immutable hashes
  freshly verify. Merge releases the live freeze for separate work.
- [x] Root twelve strict O2 current-E8 C8 gates pass; prototype Main `dea5fc1`
  is published in `cf8cdf5`. Worker sanitizer evidence remains separate.
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
- [x] Freeze exact C14 source/test/docs and hand off through the existing outbox;
  Root task publication and current-producer/Main review remain separate.
- [x] Root reports C14 task/Main publication and fresh E12 gates/import controls;
  preserve historical omitted-import fixtures without an active frontend bug claim.
- [x] C15: verify generated-provider callbacks across two consumer modules and
  mixed products/header orders, focused O2/client-source SAN and loaded-code lifetime.
- [x] Freeze/hand off exact C15; distinguish worker E8, Root C14 E12 and pending
  Root C15 current-producer review. Preserve all historical submitted snapshots.
- [x] C16: implement distinct borrowed unary/binary scalar profile and verify
  Core/manual/source comparisons, inert emission, header orders and old refusals;
  affected unary/module/I/O controls pass with qualified E8.
- [x] Freeze/hand off exact C16 implementation/test/docs for separate Root task
  publication and current-producer/Main review; keep C15 immutable evidence.
- [x] Root reports exact C16 task/Main publication and current-E15 affected gates;
  preserve original snapshot while distinct C17 source/view work proceeds.
- [x] Implement/verify the separate [native predicate boundary](2026-10-04-C-BACKEND-NATIVE-PREDICATE-NOTE.md),
  with existing selected Nat32/enum/List contracts, explicit result validation and
  unchanged source authority; keep native Acc/QuickSort unfinished.
- [x] Freeze/hand off C17 exact19 with pinned local E8 evidence and initial
  failures; Root task publication/current-producer/Main review remain separate.
- [x] Record Root C17 exact task/Main publication and current-E17 gates; preserve
  its historical evidence while updating this single live issue table.
- [x] C18: verify ordinary generated native predicate providers with two consumers,
  distinct products/header orders, source observations and focused client SAN;
  retain setup failures and correct the active-arena depth probe.
- [x] Prepare separate C18 exact-file handoff under the unchanged C17 backend;
  Root publication/current-producer review remain separate from local verification.
- [x] Record independently verified C18 task/remote and Root-reported Main/current-
  E18 module/parent gates; preserve all original snapshot and failure evidence.
- [x] C19: separate ordinary C factories/client across translation units, verify
  opposite header orders and existing descriptors by value with focused O2/SAN,
  retain type/missing-object/duplicate-object refusals and C18 observations.
- [x] Prepare separate C19 exact-file handoff; current-producer/task/Main review
  remain Root-owned and do not complete full #61/native Acc/QuickSort.
- [x] Record Root C19 task/Main publication and reused-E18 joint verification;
  preserve original handoff/evidence and continue separate bounded C20 inspection.
- [x] C20: admit signed predicate/List source, retain parameterized refusals and
  verify direct forms/source/readback/three products/O2/client-source SAN.
- [x] Prepare separate C20 exact-file handoff; no emitter/ABI expansion and no
  data-dependent signed comparator/native Acc/QuickSort/full Goal completion.
- [x] Independently verify C20 exact task/remote publication; preserve its snapshot
  while updating live tests to positive static capture coverage in C21.
- [x] C21: retain known private lambda captures in native recursion, verify signed
  maps/transitive/shadow captures/genuine Nat partitions and explicit re-exports;
  O2/client-source SAN/raw inert/separate Core/affected controls pass.
- [x] Prepare C21 exact-file handoff with pinned code/test/docs and failures;
  exact task/remote independently verified and Root reports current-producer
  qualification/prototype Main publication. Historical snapshot stays exact.
- [x] C22: reproduce nested private IH refusal, apply bounded target correction
  and verify focused40 O2/SAN each plus affected C21 capture114 O2/SAN each.
- [x] Prepare C22 exact files/evidence for freeze and Root notification;
  separate publication/qualification and native Acc/QuickSort/full #61 remain.
- [x] Independently verify exact12 C22 task/remote publication; preserve its
  immutable submitted archive as live code advances in a distinct C23 epoch.
- [x] C23: reproduce selected-type argument refusal, implement private known-type
  binding and verify existing source insertion sort, three products/Core/inert,
  O2/SAN48 each, affected nested40 each, linker and I/O19 controls.
- [x] Freeze exact C23 code/test/docs and notify Root for separate publication,
  current-producer qualification and Main review; full native Acc/QuickSort open.
- [x] Independently verify exact13 C23 task/remote and record Root C22 Main/current
  results without rewriting historical snapshots.
- [x] C24: reproduce exact selected applied-type refusal and target value-phase
  loss; implement the bounded corrections with initial failure evidence retained.
- [x] Finish C24 ordinary List/Pair/Core/inert/O2/SAN/resource/refusal and affected
  gates before exact handoff; native Acc/QuickSort remains open.
- [x] Freeze exact16 C24 code/test/docs and send Root the retained terminal
  evidence; task publication/current-producer/Main review remains separate.
- [x] Independently verify exact16 C24 task/remote publication and record Root
  C23 Main/current results in the sole issue table; old snapshots remain unchanged.
- [x] C25: reproduce direct two-Self refusal, add bounded iterative graph validation
  and verify products/Core/inert/resource/refusal plus affected chain controls.
- [x] Prepare exact26 C25 code/test/docs with terminal pinned evidence for Root;
  current-producer qualification/Main integration and full native Acc remain separate.
- [x] Independently verify C25 exact task/remote publication and record Root C24
  Main/current results in the sole issue table without changing historical archives.
- [x] C26: pin and route actual native Acc indexed/callable boundary; implement
  bounded finite-record branching reuse and terminal product/Core/inert/resource/
  refusal plus affected O2/client-source SAN and linker/I-O gates.
- [x] Prepare a separate exact14 C26 code/test/docs epoch for delegated review;
  native Acc/QuickSort/full #61 and accepted adoption remain unfinished.
- [x] Independently verify exact14 C26 task/remote publication before distinct
  live canonical edits; historical snapshot and current-producer review separate.
- [x] C27: implement bounded signed predicate ABI and no-arena status propagation;
  terminal74/affected69/61 each O2/client-source SAN,64 prior C/H exact and linker/
  publication-I/O controls pass with runtime128/input pins and refusals retained.
- [x] Freeze separate exact20 C27 source/test/docs and notify Merge; current-producer/
  task/Main review and native Acc/QuickSort/full #61 remain separate.
- [x] Independently verify C27 task/remote exact20 before material live Goal/README
  reconciliation; preserve its immutable submitted snapshot.
- [x] C28: implement separate signed provider/adapter/client gate, terminal373 rows
  each O2/client-adapter-source SAN;64 mixed triples/two header orders/source/readback,
  shared-arena/status/lifetime and five C type/link refusal controls pass.
- [x] Freeze separate exact13 C28 new files/docs and notify Merge; current-producer/
  task/Main review and native Acc/QuickSort/full #61 remain separate.
- [x] Independently verify exact13 C28 task/remote publication before distinct
  live Goal/README updates; preserve its submitted snapshot.
- [x] C29: admit/pin native scalar callback maps/right-fold reductions/Tree fold,
  implement separate target profile and pass new76/binary48/signed74 each O2/SAN,
  full-value Core/source/resource/refusal,74 parent C/H and publication-I/O controls.
- [x] Prepare separate exact16 C29 code/test/docs handoff with pinned failures,
  source deltas/style; task/current-producer/Main review remains separate.
- [x] C30: verify generated scalar providers/unchanged native consumers through
  separate ordinary C factories/client,124 expected rows each O2/SAN with product/
  header/loaded lifetime/source/full-value/resource/type/symbol controls.
- [x] Prepare separate exact13 C30 handoff; preserve C29 dependency snapshot and
  distinguish ordered task publication/current-producer/Main review.
- [x] Independently verify C29/C30 exact task publication/remote and preserve both
  submitted snapshots before live owning-doc reconciliation.
- [x] C31: reproduce nonempty size-query/depth boundary, implement explicit target
  extent helper and pass new63/callback76/record41 each O2/SAN, byte/inert/source/
  allocation-free/active-field/refusal/I-O controls with pinned inputs.
- [x] Prepare separate exact18 C31 source/test/docs handoff; no current-producer/
  Main/native indexed-callable Acc/QuickSort/full #61/adoption/cost completion.
- [x] Independently verify C31 exact task publication/remote against its immutable
  snapshot before advancing live owning docs.
- [x] C32: prove admitted native/absent array-wrapper boundary and implement
  target-only array calls; new139/query63/source-sort46 each O2/client-source SAN
  plus inert/Core/source/rollback/refusal/I-O/byte/pin controls pass.
- [x] Prepare separate exact19 C32 handoff; preserve old snapshots and distinguish
  task publication/current-producer/Main from Goal/adoption/cost completion.
- [x] Consume/record human12-hour actual Acc QuickSort CODE priority and clarified
  faithful hand-authored scope; seal C32 and defer further List conveniences.
- [x] C33: implement readable actual-Acc candidate, retaining proof indices/down/
  partition/captures, pass source/Core/O2/candidate SAN and explicit native-refusal
  controls; generalized automatic native lowering remains open.
- [x] C34: extract actual indexed/callable C declarations from existing admitted
  views; pass focused O2/full helper-client SAN/inert/source-index/refusal controls
  and prepare separate exact15 handoff without main backend/producer/ABI changes.
- [ ] Continue actual Acc expression/Fold/IH/capture lowering as a separate bounded
  candidate after concrete view inspection; preserve manual/generated distinctions.
- [x] C35: derive actual Acc Fold/callable-IH native plumbing, preserving original
  down/child Fold/result application and captures; focused O2/full helper-client
  SAN/source/inert/refusal/resource controls pass, exact13 handoff prepared.
- [x] Lower actual captured clause expressions and IH applications in C36;
  C35's historical supplied/manual bodies remain a separate component result.
- [x] C36: generate actual captured clause expressions and both child IH calls;
  composed manual helpers execute actual source/Core/resource/mutation controls,
  focused O2/full emitter-runtime/client SAN pass, exact13 handoff ready.
- [x] C37: lower actual partition Fold and inline decision/lower/upper helper
  expressions with source constructor parameters; focused O2/full emitter-runtime/
  composed-client SAN/source/Core/capture/order/resource/refusal controls pass.
- [x] C38: lower actual source callable append Fold with captured fields/deferred
  recursive IH; terminal O2/full emitters-runtime-composed-client SAN/source/Core/
  capture/suffix/resource/refusal/inert controls pass, exact13 handoff prepared.
- [x] C39: pin and report actual accessibility transport boundary; emit Nat
  accessibility recurrence with supplied/manual successor/zero-down explicit;
  terminal O2/full emitters-runtime-composed-client SAN29/Core/source/resource/
  inert/refusal controls pass, unsafe selector corrected, exact13 handoff ready.
- [ ] Lower remaining actual accessibility/raw-down/measure/outer
  expressions; generated Acc/partition/append is not full native Acc completion.
- [x] C40: emit actual Nat callable comparison clauses and lazy right-before-IH;
  terminal O2/full emitters-runtime-composed-client SAN33/Core sorting341/
  comparison256/source/refusal/resource/inert controls pass, exact13 handoff ready.
- [ ] Lower remaining owner-routed checked down transport
  expressions; manual/generated/public-main distinctions remain explicit.
- [x] C41: emit actual measure Fold/Measured Match/indexed construction and outer
  source sequencing; O2/full emitters-runtime-composed-client SAN35/Core341/
  source/fields/resource/inert/refusal controls pass, exact12 handoff ready;
  later Root current C41 qualification/task/Main publication consumed.
- [x] C42: inspect current owned transport/refinement inputs; execute a labeled
  manual three-branch down-action candidate with original values/captures retained;
  O2/client-source SAN19zero each/Core341/action/resource controls pass, exact10
  handoff prepared. Preserve C32-C41 archives and current input pins.
- [x] Consume C42 exact task/Main/current Root qualification; historical submitted
  snapshots remain immutable, original worker Git unchanged.
- [x] C43: emit actual successor down branching from admitted field/Match/capture
  inputs; O2/new emitter-runtime-client SAN29 matching rows each/Core341 pass,
  initial layout refusal retained/corrected, exact13 handoff prepared.
- [x] Consume C43 exact task/Main and current independent Root qualification;
  historical snapshots unchanged, original worker Git read-only.
- [x] C44: emit actual successor Nat/Acc/capture construction from admitted clause
  expressions; parent down exact, O2/new emitter-runtime-client SAN29 each/Core341
  pass, initial static-application refusal retained/corrected, exact11 prepared.
- [x] Consume C44 exact task/Main and independent current qualification; preserve
  the submitted snapshot while advancing this live owning plan separately.
- [x] C45: one readable standalone actual Acc module/header, source/object/archive
  O2/client-source SAN42 matching rows each and exact parent bodies. Current
  native build2 and original setup1 remain; source probes explicitly NOT_RUN.
- [x] Consume C45 exact task/Main and independent current O2/SAN42 qualification;
  keep historical source/binary/pending/failure evidence immutable.
- [x] C46: correct stale target capture dependency with existing readonly query;
  current backend/helpers O2/SAN and affected58/114/40 gates/query faults pass.
  Actual Acc checked/trusted materialized probes reach lowering4; prior inputs
  trusted pending3 and setup failures retained. Exact9 handoff ready.
- [x] C47: retain actual ordered source map vectors/frame/captures/nominal paths;
  current O2/full emitter-candidate-client product SAN50 each pass, source/Core/
  resources/inertness and12 target refusals qualified. Initial compiler1 retained;
  exact16 handoff/standalone example ready, keeping C46 submitted snapshot exact.
- [x] Consume C46/C47 exact task/Main and independent current qualifications;
  preserve submitted history, no accepted/general-native/Goal completion.
- [x] C48: actual endpoint-driven LT/quoted Acc/domain projections; strict O2/full
  affected SAN53 rows each, source/Core/resources/private refusals pass; test-only
  initial assertion retained. Fixed860-line module and exact18 handoff prepared.
- [x] C49: actual binder-derived10/12/12 frame positions; current O2/full affected
  SAN53 expected rows each/source/Core/resources/inertness/new8 frame refusals pass.
  Fixed898-line module and exact15 handoff prepared; C48 submitted history exact.
- [x] Consume C48/C49 exact task/Main and independent current qualifications;
  preserve all historical archives/pending statements, no accepted promotion.
- [x] C50: consolidate existing actual-source emitters into one bounded saved-image
  product command with ordinary source/object/archive; current O2/full-source SAN
  focused44 expected rows each, original failures and manual/native limits retained.
- [ ] C50 exact task publication, independent current review and Main integration
  remain Merge operations; original Goal/model stay active.
- [ ] Review broader actual-source action/capture correspondence and general
  indexed/callable native Acc; accepted adoption/actual costs/full Goal separate.
- [ ] Automatically derive actual scoped down actions from admitted fields and
  qualify broader correspondence; main-native generalized Acc remains open.
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
