# C Backend Epoch15 Handoff

Date: 2026-10-04 local / 2026-10-03 UTC. Lane `c-backend`.
Branch: `parallel/c-backend-20261003`.
Parent: `e9d74f70d33f1830f05a5a04cd3d05f8a01467ed`.
Chosen message: `prototype: verify generated C callback module composition`.

## Problem List

1. Make the borrowed callback ABI usable between independently generated C
   components with explicit product, signature and lifetime controls.

## 1. Generated Provider and Consumers

### Subjective (User)

2026-10-03, English paraphrase: prioritize readily usable downstream C modules
through `.a` and LinkerScript, preserve source authority and prototype scope,
and avoid excessive scope expansion. Implementation lanes may continue without
waiting for Merge; only Core/Merge integrates Main. Own task-branch publication
is authorized. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains the dated
human requirements; scheduling, test choices and exact freeze mechanics below
are operational decisions.

### Objective (Code)

Three new test/client files contain 231 lines: a scalar provider source fixture,
C status/output adapters for borrowed callbacks, and a serial composition gate.
The build registers `check-c-callback-modules`. This changes no emitter,
representation, producer, artifact/schema, source erasure or accepted code.
The existing `c_scalar_v1` provider exposes wrapping Int32 add/negate and Int64
add. Two independent `c_callback_v1` consumers use explicit distinct aliases and
the same guarded unary callback structures. Contexts borrow immutable offsets
and provider function pointers; every scalar call uses valid output storage.

Fresh verification uses the qualified E8 pointer, C14 backend and Core checker
without rebuilding them. Products are freshly emitted from ordinary admitted
images; the consumer image exactly matches the retained final C14 image.

| Check | Actual result |
| --- | --- |
| Focused serial O2 gate | 0; 99 rows: 96 zero and three expected link-1 refusals |
| Focused serial client/source ASan/UBSan/leak gate | 0; same 99 rows |
| C products | All sixteen source/object/archive/shared consumer-provider pairs, both header orders |
| Ordinary clients | 32 linked and eight explicitly loaded-provider runs per phase; two consumer modules per client |
| Existing Core evaluator | Freshly evaluates 400 selected-body callback cases per phase; each client checks both consumers against them, 800 comparisons |
| Actual source differential | Twenty Int32 observations per client agree with the one fresh source run per phase |
| Input/context/status controls | Serialized images unchanged; context offsets unchanged; null output/code preserve expected status/output |
| Loaded provider lifetime | Load before borrowing its function pointers; close only after all synchronous calls finish |
| Duplicate public symbols | Source/object/whole-archive link attempts return 1 with actual duplicate exported-symbol diagnostics and no executable |
| Deterministic inputs/expectations | O2/SAN consumer/provider images, Core cases, oracle and selected source/header bytes identical |
| Style and registration | Leading tabs, English comments/docs, shell syntax, diff check and registered gate dry run pass |

The 800 comparisons and twenty observations reuse the same expectations across
product/header combinations; they are not independent property counts. Core test
interpretations use existing host operations on ordinarily admitted consumer
bodies. They confer no Surface formation/equality evidence or new admission
route. Provider formation still comes from ordinary source admission.

Root operational report: C14 immutable23 task `e9d74f70` and prototype Main
`6c36dcb` are integrated/pushed. Current E12 runtime128 `12026914` callback
O2/client SAN39 each, shared58 each/link/I/O19/import4 controls pass. Root's corrected
explicit-import consumers preserve original output/fuel; omitted imports were
fixture setup, not a frontend policy bug. Fresh readback from Main `6c36dcb` of
`core-c14-source-import-review.json` pins the earlier E9+E10 four positive
corrected consumers; its SHA256 is
`d7575c61ff8c979762c807b9fa3c99144a0f7cc06ed0752bc933b24f42e6f5c4`.
Historical failed C14 inputs/images/reproduction remain unchanged. Root's C14
qualification is distinct from the pending current-producer C15 review.

No C15 gate failure remains. The provider fixture was briefly created outside
the prototype subtree and moved into the lane with apply_patch before execution;
no outside implementation file remains. A documentation patch context mismatch
made no file changes and was corrected. No producer change, weaker supported or
refusal expectation, broad suite or timing run was used.

Retained evidence: `/tmp/a-program-c-backend-epoch15-callback-modules-20261004`.

| Pin | Digest/revision |
| --- | --- |
| Qualified producer | `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2` |
| Qualified runtime128 manifest | `9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740` |
| Reused C14 backend | `a8e08be59fde399290cc7096f8a8455a7e6d61446952f3e6b5d0bb19afbf6424` |
| Reused inert/Core checker | `61eff75ba12cf120b9513a7bad20f095de42822c2d6531ddd77cde4ed50248f4` |
| Qualified pointer | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| Consumer image | `4ff54c4377472eb4451d3e31db155b22fc8a25dcefd7bdac50e061f0b564011c` |
| New provider image | `48a75e7d02b38d826375631879e1cbbd89ca25a5a61f1f9687fbc749df10ba3b` |
| `verification.json` | `2ff9838e18be16ebd4bce04f1b3361d3aaa831ab4a796d0325ca3cd81c7da4be` |
| `qualified-inputs.sha256` (184 files) | `2e8ff68a532bb60ff07e9f762d7cdd2bd49fe2c8bbd92b39e3c7ff87ee66cafa` |

All 128 current runtime source hashes match qualification. Detailed argv,
per-command stdout/stderr/statuses, generated sources/headers/objects/libraries,
client/oracle binaries and committed Root import report are retained and pinned.
`epoch15-files.sha256`, `epoch15-deltas.tsv`, `epoch15-runs.sha256`,
`epoch15-freeze.json` and the immutable submitted tar identify this distinct
eight-file snapshot. C14's submitted tar remains SHA256
`4a3b8f67a01b095ab8f2822549d87628c9fc4b5d7aefdf9a14a32e83ad386798`.

### Assessment

The adapters demonstrate ordinary generated C clients within the existing
borrowed unary scalar contract. A valid generated scalar provider must succeed
with valid parameters/output; this adds no foreign failure propagation. Code and
immutable context must outlive synchronous calls. No context/foreign closure
escapes; explicit loaded providers are closed after all calls. Source semantic
nominal identity is not inferred from compatible C header spellings.

Sanitizer mode instruments the clients and every source-product body at O1.
Generated object/archive/shared bodies and qualified backend/producer/Core checker
remain O2. Loaded code remains O2. Archive duplicate refusal explicitly forces
both members with whole-archive; ordinary archive extraction or shared-library
symbol interposition is not claimed to reject duplicate aliases automatically.

Root's proposed matched36 Fold-only 20:20-20:30 UTC window is scheduling, not a
launch grant. Focused gates are terminal with no heavy C15 children at the
20:06 safe boundary. The outbox records that fact; light documentation/hash work
continues while heavy work remains drained for this specific window/release.
No blanket implementation hold or comparative workload follows.

General/boxed/multiargument/mixed-width/effectful/dependent callbacks, escaping
ownership, general nominal exchange, higher Identity and native Acc/QuickSort
remain open or unsupported. C15 advances AP5.6/AP6.2 ordinary C usage only; it
does not complete the full Goal/#61, authorize accepted promotion or close issues.

Exact eight-file snapshot:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-CALLBACK-MODULES-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH15-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/callback_modules/provider.p
src/prototype/c_backend/callback_modules/client.c
src/prototype/c_backend/callback_modules/check.sh
```

### Plan

- [x] Implement generated-provider adapters and two ordinary C consumer modules.
- [x] Verify all product pairs/header orders, Core/source agreement and lifetime.
- [x] Preserve input/status/duplicate-symbol controls and pin instrumentation scope.
- [x] Check tabs/English, register the gate and retain all source/binary evidence.
- [x] Freeze this distinct epoch and notify Root through the existing outbox.
- [ ] Root publishes the task and separately reviews current-producer/Main integration.
- [ ] Continue the active bounded downstream Goal; native Acc/QuickSort remains open.
