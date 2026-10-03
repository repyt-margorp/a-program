# C Backend Epoch16 Handoff

Date: 2026-10-04 local / 2026-10-03 UTC. Lane `c-backend`.
Branch: `parallel/c-backend-20261003`.
Parent: `ba5d39d1537ffa93770d1705603e4bac37ab14ea`.
Chosen message: `prototype: lower borrowed binary scalar callbacks`.

## Problem List

1. Realize an admitted fixed binary scalar callback as ordinary C without
   extending source authority or relaxing the original unary profile.

## 1. Fixed Unary/Binary Callback Inputs

### Subjective (User)

2026-10-03, English paraphrase: prioritize bounded downstream `.a`/LinkerScript
products readily usable by ordinary C modules; preserve prototype/source
authority and explicit unsupported cases. Implementation lanes continue
independently; task-branch publication is authorized and only Core/Merge
integrates Main. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains the
complete dated human requirements. Profile/test/freeze choices are agent workflow.

### Objective (Code)

Seven implementation files change +66/-21 against the observed C15 task (their
baseline implementation is C14). The new distinct `c_callback2_v1` /
`callback2_direct_v1` profile permits a thunk of one or two independent Pi
domains, each represented by the same Int32 or Int64 as its pure TOTAL result.
Every domain, independence, result representation and totality comes from
existing admitted views. Callback arity determines flat argument validation and
C invocation. Existing `c_callback_v1`, scalar and native profiles still reject
binary callback inputs with status 4.

The header checks `AP_C_CALLBACK2_ABI` 1 and shares the original guarded unary
structures. New binary structures have signatures `int32_t (*call)(void *,
int32_t, int32_t)` and the corresponding Int64 signature. They borrow code and
context synchronously, return scalar values, validate every callback's code
including unused inputs, and leave output unchanged on null-code status 2.
Null output remains status 1; context may be null if valid for its supplied code.
No structural runtime, producer/schema, source erasure or accepted code changes.

Six new fixture/client/harness files contain 437 lines. The owning prototype
build registers `check-c-callbacks2` and its independent inert/Core checker.
Fresh worker gates reuse qualified E8 runtime128; a strict serial O2 backend and
checker build each return 0. Ordinary fixture admission is 6168 steps; selected
export reconstruction totals 6223 steps. The earlier 4985-step typed-view probe
motivated the candidate and is not binary native-lowering completion evidence.

| Check | Fresh worker result |
| --- | --- |
| Binary O2 and client/source ASan/UBSan/leaks | Both 0; 48 rows each: 30 zero, sixteen expected 4, one expected 2, one expected compiler 1 |
| Four products | Source/object/archive/shared; no structural runtime |
| Manual C cases | 4000 wrapping comparisons/product: five offsets, all 10x10 input pairs, eight exports |
| Independent existing Core evaluator | 400 comparisons/product: 250 Int32, 150 Int64; fifty selected pairs, eight admitted export bodies |
| Actual source observations | Twenty Int32 outputs agree with one ordinary source run per phase; Int64 coverage uses existing Core/host interpretations, not Surface large literals |
| Inert emission | No evaluation/substitution/WHNF/typed-query advancement; graph objects/terms and typing proofs/occurrences unchanged; source/header bytes match driver |
| Borrowed call paths | Direct, repeated, block-local captures, mixed unary/binary and unused inputs |
| Determinism and status | Repeated/trusted source/header bytes identical; null output/code and output preservation; incompatible header version refuses compilation |
| Unsupported callback fixtures | Ternary, mixed domains, mixed result, returned callback and demanded effect: checked/trusted status 4 with no products/temp residue |
| Original profiles | Scalar/native/unary checked/trusted binary refusals remain 4; nominal selection refuses 2 |
| Header interoperability | Original unary/new binary headers compile and run in both include orders, including valid null context |
| Affected unary gate | 0; original 39 rows, 400 Core cases/product, source20 and original binary refusal |
| Affected ordinary modules | O2/client-source SAN both 0; original 99 rows each, all sixteen product pairs and loaded-provider lifetime/duplicate-symbol controls |
| Publication I/O | 0; nineteen controls preserve I/O2, unsupported4, atomic cleanup and prior output |
| Source checks | Tabs, English comments/docs, declarative headers, shell syntax, diff check and registered-gate dry run pass |

Repeated products reuse expectations; these are comparisons, not independent
properties. Test-only well-scoped Core callback interpretations are constructed
after emission from existing host operations. They provide evaluator expectations,
not new Surface formation/equality receipts or a source checker.

The first manual client gate returned 1 because its source client aborted 134 on
a stale output sentinel after a previous successful offset loop. Resetting that
sentinel before the null-unary check fixed the harness; original command/status,
stdout/stderr and failed binary remain pinned. No emitter change or weaker
expectation resolved it. Earlier intermediate 33/43-row successful gates and
current-progress setup evidence remain historical; final acceptance uses 48 rows.

Canonical disk-backed evidence:
`src/prototype/c_backend/.evidence/epoch16-20261004` in this worker worktree.
All 2294 original raw files were copied byte-exact from
`/tmp/a-program-c-backend-epoch16-binary-callbacks-20261004`; those original files
remain untouched. Historical command paths are preserved rather than rewritten.

| Pin | SHA256/revision |
| --- | --- |
| Qualified producer | `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2` |
| Qualified runtime128 manifest | `9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740` |
| Qualified pointer | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| C16 O2 backend | `87a4f5a4c4cbb38e5c7b7fad4f724be5641e624b29145899f8160879df28ce8d` |
| C16 O2 inert/Core checker | `9d99ce72a7ad442fe4de63a268635e224833b412c373277723fdeafc4e09859b` |
| Fresh binary fixture image | `c190e76fea2bd351c894fb369e7b38736a4af2d1b0a8d22a5acf8173e7c28dc4` |
| Native component C | `cebfb77377ddf15cc808a0339ef9c96f8bafb6bb1578777ac32eb2f09d348964` |
| Native component header | `4d28452817b3778f9c390efef27fdd4fb4758d2ffe3a3dd46e3b4152d8028bd3` |
| Independent Core oracle client | `5b1e1f853f8ff34794c9db6012550a299f454846c4fb83d4d70f3dfd5ed6ba4f` |

All128 current qualified worker runtime hashes match. Image, oracle and native
source/header bytes match across final O2/SAN phases. Detailed input/tool/binary
pins, commands, statuses and products are in `verification.json`,
`qualified-inputs.sha256` and `epoch16-runs.sha256`. Exact file hashes/deltas and
immutable tar are recorded in `epoch16-files.sha256`, `epoch16-deltas.tsv`,
`epoch16-freeze.json` and `epoch16-submitted-snapshot.tar`.

Root operational receipt: C15 exact8 task `ba5d39d1537ffa93770d1705603e4bac37ab14ea`
and prototype Main `d219fca70099befb96ce5770f9379be00d3c8a67` are pushed; current-E14
runtime128 `abedf677` frozen-C14 module O2/client-source SAN99+99 pass. Its immutable
tar remains `f6eb5f145a96829b42245952e3841c5b62a68c7e624e8a45d9d0269f756186a0`.
Distinct live C16 plan/build/docs advance; original C15 submission is unchanged.
Root C15 qualification does not qualify this C16 implementation. Root released
Fold36 at terminal 20:20:46 UTC; no worker comparative measurement was launched.
Later Root environment receipt reports tmpfs ENOSPC in other lanes; C16 gates
were already terminal without an ENOSPC failure. Final manifests/tar use owned
disk storage and future compiler temporaries use its `compiler-tmp` directory.
No shared cleanup or evidence deletion occurred.

### Assessment

This is bounded AP5.6/#61 C usability progress, not native Acc/QuickSort or full
Goal completion. External C must implement the admitted pure-total source
interpretation and keep code/context valid throughout every synchronous call;
the backend does not check arbitrary C purity, totality or foreign lifetime.
No foreign closure is returned or stored beyond a call. Mixed widths/results,
arity3, partial/dependent/boxed/effectful callbacks, recursive callable Acc fields,
indexed SizedList, higher Identity and general nominal exchange stay unsupported.
The five negative fixtures and old unary refusals remain in the new gate.

SAN instruments manual/oracle/header clients and source-product bodies at O1;
generated object/archive/shared bodies and backend/producer/Core checker remain
O2. Affected shared-module clients use the same limited instrumentation scope.
No broad backend suite, source promotion, timing or private checker was added.
Task publication and current-producer/Main integration are separate Root work.

Exact eighteen-file snapshot:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-CALLABLE-SHAPE-NOTE.md
doc/2026-10-04-C-BACKEND-EPOCH16-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/link/plan.h
src/prototype/c_backend/link/plan.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/lower/representation.h
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/scalar.h
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/callback2/fixture.p
src/prototype/c_backend/callback2/callback.aplink
src/prototype/c_backend/callback2/client.c
src/prototype/c_backend/callback2/header_client.c
src/prototype/c_backend/callback2/inert_test.c
src/prototype/c_backend/callback2/check.sh
```

### Plan

- [x] Inspect actual ordinary admitted views and implement a distinct target profile.
- [x] Verify manual/Core/source agreement, inert emission and borrowed ABI controls.
- [x] Retain old refusals, initial failure and affected module/I/O evidence.
- [x] Check source style, register the gate, pin exact bytes and notify Root.
- [ ] Root publishes task and separately reviews current-producer/Main integration.
- [ ] Continue the bounded active Goal; native Acc/QuickSort remains unfinished.
