# Native Static Function Boundary

Date: 2026-10-03
Status: verified active prototype; frozen Epoch5 handed off for publication.
Baseline: task branch `parallel/c-backend-20261003`, published Epoch4 `a3b6bce`.
Related: [owning Goal](2026-10-03-C-BACKEND-GOAL.md), AP6.3/#49.
This is a follow-up candidate, not a revision of Epoch4's frozen publication set.

## Problem List

1. Lower admitted block-local known functions through the existing native call
   machinery while preserving lexical captures and explicit unsupported APIs.

## 1. Static Function Calls

### Subjective (User)

2026-10-03, English paraphrase of the user coordination preference and latest
clarification: Core waits for worker notification OR a six-hour timer and checks
even without notice, rather than continuously polling. Keep this worker Goal
active; current downstream ABI/helper work continues without a pause. The timer
supersedes a notice-only interpretation. Payload format and owner routing belong
to Core operational decisions, corrected in Assessment. The owning Goal's frozen
document was preserved during the freeze; the preference is now incorporated
there after release, and the temporary addendum is removed.

2026-10-03, English paraphrase of the latest direct user clarification (source in
the owning Goal): produce C readily usable from other C modules, keep the backend
downstream, and avoid excessive deep or scope-expanding investigation. Task-branch
publication is allowed; Core alone integrates Main. Static-function support is an
agent candidate within that scope, not a new user-requested ABI design.

### Objective (Code)

At `720f92a` plus frozen Epoch4, `lower/scalar.c:lower` only retains a native
thunk operand when its body directly invokes the known recursive IH. A local
function definition instead produces a syntactic Thunk/Lambda operand. Existing
`force`, `inline_call`, `callee` and capture handling already retain known IH
closures in target-local state. Public dynamic callback parameters remain
unsupported. No producer defect or missing artifact field is established.

Fresh inspection of the current-Core numeric image revalidates `block_callback`
in 3519 steps and exposes this Thunk/Lambda shape. Evidence:
`/tmp/a-program-c-backend-epoch4-current/next-static-call-inspection.log`.
The existing numeric gate retains its native refusal. New lane fixture
`fixtures/static_functions.p` isolates closed, captured, shadowed, repeated,
curried and unused known functions beside dynamic-callback/effect controls.
Its admission/lowering results must be recorded before implementation.

Initial probe with the pinned Epoch4 binaries: the initial source fixture admits in
2466 steps. All six known-function exports refuse native lowering with status 4
(`unsupported native expression, argument or capture representation`); dynamic
callback and demanded-effect controls refuse the public signature with status 4.
No probe publishes a product. Results:
`/tmp/a-program-c-backend-epoch4-current/static-functions-current.tsv`.
Image SHA256: `9b4c710957ed747bd615e1cc49e937ce0bf07292b020f4192ddc497c47e852d1`;
source SHA256: `d3e9a588a3523ec6982912999ad87d634115014d151e3325027147d5ab52c15f`.
These are current unsupported target cases, not source-admission failures.

Fresh isolated trial at the same base, with only `scratch/static_functions/scalar.c`
selected through its private build: a +7/-4 line change retains syntactic
Thunk/Lambda values alongside the existing IH case. Active `lower/scalar.c` is
unchanged (SHA256 `2ef1e59bec83a972ff17d5e65b850ae36e2609bb67396de6249a19efb124f00f`).
Trial SHA256: `bf719285e6a05a2df32e33b46b815270453b31545e04036e2db8f78412d54022`;
reviewable diff: `/tmp/a-program-c-backend-epoch4-current/static-trial.patch`.
The trial backend SHA256 is
`c5121ca65495789db0542bf860c55719d4312d84cb09423f34a7f031d2e2ec1b`.

Fresh trial verification passes:

- Source/object/archive C clients: 260 Int32 and five Int64 cases per product,
  null outputs, eight source observations, checked/trusted equality, deterministic
  output and unchanged input images. Logs: `static-functions-trial-o2.log` and
  `static-functions-trial-san.log` under the Epoch4 evidence root. Source target
  bodies/clients are ASan/UBSan/leak checked; native-tool objects retain O2 flags.
- Raw static capture/shadowing emission forbids evaluator/substitution calls and
  preserves graph/store counts; 20 generated C answers agree with the evaluator.
  Generated code passes O2 and ASan/UBSan/leak checks. Log: `static-raw-o2.log`.
- Native List/Nat captures: borrowed-tail identity, array copy-out, unused successor
  at UINT32_MAX, checked overflow, recursive depth and allocation rollback pass
  O2/sanitizers in source, object and archive clients. Logs:
  `static-native-products-o2.log`, `static-native-products-san.log` supersede the
  earlier source-only `static-native-o2.log` and `static-native-san.log` scope.
  Deterministic emission, checked/trusted C equality, exhausted checking fuel
  without publication and unchanged input images pass. The reusable gate is
  `lower/static_native_check.sh`; that initial gate hash is superseded by the
  two-module extension below.
  Source target bodies and all clients are sanitizer checked; driver-produced
  object/archive bodies retain ordinary O2 flags.
- The exact old data/List/numeric `block_callback` fixtures now have nine positive
  C observations, including Bool-selected Packet branches. Logs:
  `static-legacy-o2.log`, `static-legacy-san.log`. An initial scratch-client expected
  value missed Packet's true-field increment; source inspection corrected that
  harness error to 45/51. No lowering change was needed; reruns pass.

The expanded source fixture admits in 4127 steps; current SHA256 is
`fc0451127ce712a1c01f68de606494219dc7735121899e008ca13e7a5459b4a3`.
It includes a two-closure captured-offset case that passes, and a specific
three-closure captured-offset chain that refuses with status 4/no product. This
is not a general nesting-depth bound. Dynamic callbacks and demanded effects
still refuse. An unused known closure with an effectful body does not run that
body; its source/C differential passes. No public effect/callback ABI is added.
Native fixture SHA256:
`74c3fe8993a4984bd0fb0f6443679706ddf329fd18063e04f92cda93ba3dac38`.

Fresh two-module source/object/archive verification passes O2 and ASan/UBSan/leak
checks using the same trial binaries: distinct type/export/helper aliases,
explicit array exchange, shared caller arena, rollback preserving the other
module's earlier result and both destructors. Logs: `static-native-modules-o2.log`
and `static-native-modules-san.log` under the same evidence root. Gate SHA256:
`c9fb577ee9951c6b7b267971c882cff9d1e1944ca43cf68e0ed23f172a7c945f`;
client SHA256: `b8c2ff8a1273f7ead87e6de38c400a8b221c1a62e2ee0b016ea214b584f28c46`.
Generated source bodies/all clients are sanitizer checked; driver object/archive
bodies retain O2. No general cross-module nominal identity is asserted.

Epoch4 now also passes the remaining enum, data and structural backend/Oracle
gates against its pinned producer/backend pair. Supplemental logs are under
`/tmp/a-program-c-backend-epoch4-current/supplemental-*-o2.log`.
All eight C gates pass; all 11 Epoch4 manifest hashes remain unchanged. This is
fresh worker evidence, separate from Core's publication/integration review.

Fresh active Epoch5 at `a3b6bce` plus the named lane edits: the minimal diff is
applied to `lower/scalar.c` (SHA256
`bf719285e6a05a2df32e33b46b815270453b31545e04036e2db8f78412d54022`).
The temporary lowerer copy/build override/legacy harness are removed; raw static
correspondence now lives in `lower/static_oracle_test.c` and the prototype build's
`check-c-static-functions` target. All nine O2 C gates pass, including the updated
data/List/numeric gates: their exact former block controls now have 130 Packet,
511 List and two numeric observations per C product, alongside retained callback,
effect and unsupported representation controls. Static/scalar/data/List/numeric
generated C clients pass ASan/UBSan/leak checks. Backend, producer and raw fixture
generators are O2; driver-produced object/archive bodies are O2. Evidence root:
`/tmp/a-program-c-backend-epoch5`, logs `*-o2.log`/`*-san.log`.
No active gate failure. The producer/reader snapshot is the pinned Epoch4
copy; latest Main/current-owner combined verification remains Core's separate
integration gate. Exact pins and retained artifacts are in the Epoch5 handoff.

### Assessment

Core operational decisions, 2026-10-03: use one short lane/epoch/commit/manifest/
failure notice for a material event and retain freeze/owner precautions. Core
waits with interruptible `clock.sleep` in its existing conversation; Performance
tests the worker-to-Core route. No duplicate Core, scheduler or worker Main
integration. The notice format, route mechanics and owner precautions are not
user design principles, correcting their earlier Subjective attribution.

Later Core operational report/routing: Job E2 woke the existing Core sleep after
43.156 seconds. Use this worker's own `src/prototype/coordination/outbox/*.txt`
for a material notice (newline-terminated, at most 8 KiB, with lane/epoch/commit/
manifest/gates/failures). Core owns the pointer/hash relay; no socket permission
change, merge or missing-session launch. The six-hour fallback remains. Epoch4's
ready notice uses this route; scratch work stays outside its frozen manifest.

Agent candidate: recognize only syntactically known function thunks and reuse
the existing direct call/lexical environment machinery. No public closure layout,
foreign callback contract, evaluator pass or shared IR is proposed. Check capture
rebinding and unused/repeated demand explicitly before adopting a change. Keep
dynamic callbacks, demanded effects and unsupported recursive capture shapes as
refusals. This bounded improvement can make ordinary first-order APIs written
with local functions usable without source workarounds; it is not native Acc or
QuickSort completion.

Agent trial decision (historical): use a separate lowerer copy under
`src/prototype/c_backend/scratch/static_functions/`, selected only through a
private build override. Keep the active lowerer, default build and every frozen
Epoch4 file unchanged. This allows concrete implementation evidence during
publication without mixing it into Core's epoch review. Apply a verified change
to the active prototype only after release; the scratch copy is not a publication
candidate. No shared owner changes or extra normalization pass.

Agent trial outcome: adopt this minimal candidate for the next owned epoch after
release. Move the exact old block controls into positive verification when
updating their gates; retain callbacks, effects, recursive function fields and the
new unsupported capture-chain control explicitly. The original eight gates had
stale expected refusals for those now-supported block exports; their frozen
Epoch4 run remains valid. Updated active gates now pass as recorded above.
The scratch copy/override is excluded from any publication set; apply the small
verified diff to the active prototype and rerun the updated gates after release.

Core publication/release, 2026-10-03: exact Epoch4 was published/pushed as
`a3b6bce`; Core found no blocking defect and released the implementation freeze.
Combined gates/Main integration are separate and next. The scratch trial was
outside that publication set. Adopt its small active-lowerer diff now, retain the
specific three-closure refusal, and keep historical Epoch4 evidence unchanged.
Shared Git metadata remains read-only; publication stays delegated to Core.

Agent ABI verification decision: test two separately generated native products in
one ordinary C client, with distinct export/type/helper aliases and the existing
guarded arena ABI. Exchange List contents through explicit array copy-out/copy-in;
do not infer cross-module nominal equivalence from equal layouts. Check that one
module's failed allocation preserves earlier results allocated by the other and
that either generated arena destructor releases the shared allocation chain.
This is a bounded C composition check, not a new representation or source rule.

### Plan

- [x] Verify the probe's source admission and exact current native failures.
- [x] Test a minimal static Thunk/Lambda extension in the isolated scratch copy.
- [x] After release, implement only the supported static-function shape using
  existing native calls; abandon the proposal if it needs shared owner changes.
- [x] Verify the isolated trial's C extrema/captures/shadowing/repetition,
  differentials, immutable images, inert emission, resources and refusals.
- [x] Verify two native modules with distinct aliases, explicit array conversion
  and shared caller-arena rollback across source/object/archive products.
- [x] Verify the active prototype and updated existing gates after release.
- [x] Hand off a separate verified epoch; exact files/pins are in the
  [Epoch5 handoff](2026-10-03-C-BACKEND-EPOCH5-HANDOFF.md). Keep its set frozen
  until Core release; publication/current-owner integration are separate.
- Completion: represented first-order exports containing the admitted known local
  functions agree with source execution; unsupported public callback/effect APIs
  still refuse before publication. This does not complete the full Goal.
