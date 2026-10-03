# C Backend Epoch14 Handoff

Date: 2026-10-03. Lane `c-backend`, branch `parallel/c-backend-20261003`.
Parent: `96a0308283dad19450c5c56f2ac0ca3463c4838b`.
Chosen message: `prototype: lower borrowed unary scalar callbacks`.

## Problem List

1. Make a bounded admitted pure callback usable from ordinary C without changing
   source authority or widening the existing scalar/native profiles.

## 1. Borrowed Unary Scalar Inputs

### Subjective (User)

2026-10-03, concise English paraphrase of the direct human clarification:
prioritize readily usable downstream C modules through `.a` and LinkerScript,
with bounded lowering and no authority over A Program. Avoid excessive scope
expansion. Implementation lanes may continue independently; only Core/Merge
integrates Main. Own task-branch publication is authorized; accepted promotion
and native Acc/QuickSort completion are separate. The owning
[Goal](2026-10-03-C-BACKEND-GOAL.md) preserves the dated source and superseding
workflow decisions.

### Objective (Code)

Seven prototype implementation files change by +84/-20 lines against the exact
parent. `plan.[ch]` adds `c_callback_v1` / `callback_direct_v1`; representation
types and scalar lowering add borrowed unary callback parameters, Force calls
and public null-code validation. `driver.c` selects the new emitter and records
its contract. The build registers one focused gate; fixtures, ordinary clients
and inert/Core-evaluator controls are separate test files. There is no producer,
schema, artifact codec, private checker or accepted-source edit.

Existing structural views establish a thunk of an independent unary Pi with
same-width Int32/Int64 domain and pure TOTAL result. Ordinary selected-export
revalidation still supplies owned evidence. The target emitter only borrows
those views; an inert control forbids evaluator/substitution/WHNF/typed-query
advancement and preserves graph, object, proof and occurrence counts.

The generated header exposes `struct ap_c_callback_i32` and
`struct ap_c_callback_i64`, each containing `void *context` and a same-width unary
function pointer. Scalar outputs remain output pointers. Status 1 means null
output; status 2 means null code, including unused callback arguments. Failures
leave output unchanged. Context may be null if accepted by the callback.
All four products contain ordinary C without the structural runtime.

Fresh serial worker verification on qualified E8:

| Check | Actual result |
| --- | --- |
| Backend/inert checker strict O2 builds | 0; producer reused without rebuilding |
| New callback O2 gate | 0; 39 status rows: 25 zero, 12 unsupported-4, one script-2, one expected compiler-1 |
| New callback client/source ASan/UBSan/leak gate | 0; same 39 rows |
| Existing Core evaluator comparisons | 400 per source/object/archive/shared product: 250 Int32 and 150 Int64 |
| Ordinary manual C client cases | 400 per product, with five context offsets and signed extrema; not independent property counts |
| Actual same-module source differential | 20 Int32 observations, explicit thunk arguments, exact output agreement |
| Inert, repeated/trusted controls | Source/header identical; serialized images unchanged |
| Shape/return/effect refusals | Mixed-width, binary, returned callback and demanded effect: checked/trusted 4, no product |
| Old scalar/native profiles | Checked/trusted 4 retained; no implicit fallback |
| Affected shared gate | 0; 58 rows, exact public definitions 3/4/17/8 |
| Old profile byte comparison | 36 component/header/map files identical to C13 |
| Original linker gate / publication I/O controls | 0 / all nineteen expected statuses pass |
| Style, shell syntax, registered gate dry run | Tabs/English comments, diff check, syntax and dry run pass |

The Core comparison constructs explicit well-scoped host-operation input
interpretations after inert emission, then evaluates the ordinarily admitted
export bodies using the existing Core evaluator. It establishes target agreement
for those interpretations, not Surface formation/equality evidence or a new
admission route. Int64 inputs come from those explicit Core interpretations and
C parameters; no Surface large-literal or expected-type workaround is used.

Source boundary and historical failures are retained:

- The initial imported differential and named/thunked/inline callback probes
  reject (1) on worker E8. A later imported existing thunk also rejects (1).
  Root independently reports the three original probes reject (1) on current
  E9+E10, runtime128 manifest `73fa86c94b805b768d2c8493ce6f6a46c51983e556974c4920d805b59eefc743`,
  pointer `407e8f5e395f14f825b50d92b6787a87bac51408240fa5c1f5be206804e4823e`.
  Readonly syntax/type review is routed to the sole Job owner. This remains a
  fixture/frontend boundary; neither a producer bug nor imported applicability
  is established. Root's report pins its older provider bytes; the exact prefix
  is preserved separately as `source-probes-provider.p`, SHA256
  `d3c37ff580c5a6ac796accece5e474966e03e8c4cb4fd71179d2d8c1b5bcbf5f`.
- The later same-module differential evaluated successfully but its literal
  backslash-n separator did not match C newline bytes. Numeric values matched;
  using actual common `|` separator bytes resolves the comparison. Failed run
  logs remain. No synthesis change or relaxed expected outcome was made.

There is no unresolved target gate failure. Worker tests do not claim current
E9+E10 C14 target verification; Root's fresh target composition remains separate.
No broad suite or timing run was performed.

Retained evidence: `/tmp/a-program-c-backend-epoch14-callback-probe-20261003`.

| Pin | Digest/revision |
| --- | --- |
| Qualified producer revision | `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2` |
| Qualified runtime128 manifest | `9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740` |
| Candidate O2 backend | `a8e08be59fde399290cc7096f8a8455a7e6d61446952f3e6b5d0bb19afbf6424` |
| Candidate inert/Core checker | `61eff75ba12cf120b9513a7bad20f095de42822c2d6531ddd77cde4ed50248f4` |
| Qualified pointer | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| Final O2 callback image | `4ff54c4377472eb4451d3e31db155b22fc8a25dcefd7bdac50e061f0b564011c` |
| `verification.json` | `15467458cc4e36fbf59bf4cc84f99f8015bac0b2970a520a37c0f6d9a8e58c44` |
| `qualified-inputs.sha256` (181 files) | `91bbf26a60b349bf60f1baccdad18cc07b8913a3d2db97a5c4c4b1aff85b4815` |

All 128 current runtime sources match the qualified snapshot. Verification pins
source inputs, binaries/tools, command/status records, generated products and
client/oracle binaries. Detailed command/output/error files, initial failed runs
and copied Root report remain. `epoch14-files.sha256`, `epoch14-deltas.tsv` and
`epoch14-freeze.json` identify the exact submitted bytes; the separate immutable
tar preserves that snapshot for delegated publication.

### Assessment

The new profile and signatures are agent choices within the existing downstream
C-module scope. Borrowed code/context must remain valid throughout the synchronous
call and implement the declared pure-total source function. Arbitrary C code's
purity, totality, interpretation and lifetime are caller preconditions, not facts
checked by the backend. Generated code retains no foreign closure beyond the
call and returns only scalars. Known lexical captures/direct calls still use the
existing target machinery. There is no callback registration or ownership layer
in A Program.

Sanitizer mode instruments the source component and ordinary/manual/Core-oracle
C clients at O1. Emitted object/archive/shared bodies, backend, producer and Core
checker remain strict O2; those bodies are not claimed sanitizer-instrumented.
Target/C resource exhaustion remains outside the admitted pure-total source
contract. Existing depth/Nat32/storage limits are unchanged.

Mixed-width, multiargument, returned/boxed, partial/dependent/effectful callbacks,
nominal selections combined with this profile, recursive captures, higher
Identity, callable/indexed Acc and native QuickSort remain unsupported. Historical
native sorting refusals are preserved, not rerun as a new broad milestone.
The imported-source issue remains routed to Job. No erasure authority, accepted
promotion, issue closure or full AP5.6/#61 completion follows.

Exact 23-file snapshot:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-EPOCH14-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/link/plan.h
src/prototype/c_backend/link/plan.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/lower/representation.h
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/scalar.h
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/callback/callback.aplink
src/prototype/c_backend/callback/check.sh
src/prototype/c_backend/callback/client.c
src/prototype/c_backend/callback/differential.p
src/prototype/c_backend/callback/fixture.p
src/prototype/c_backend/callback/inert_test.c
src/prototype/c_backend/scratch/callback_boundary/typed_view.c
src/prototype/c_backend/scratch/callback_boundary/named.p
src/prototype/c_backend/scratch/callback_boundary/thunked.p
src/prototype/c_backend/scratch/callback_boundary/lambda.p
src/prototype/c_backend/scratch/callback_boundary/same_module.p
src/prototype/c_backend/scratch/callback_boundary/imported_existing.p
```

### Plan

- [x] Establish actual admitted classifier applicability with existing views.
- [x] Implement the bounded opt-in borrowed callback ABI and ordinary C products.
- [x] Verify Core/source comparisons, inert lowering, statuses and retained refusals.
- [x] Run focused O2/client/source sanitizers and affected product/I/O controls.
- [x] Pin inputs/results and preserve unresolved imported-source probes.
- [x] Check style and update the single owning issue table.
- [x] Freeze the exact separate source/test/docs snapshot for Root publication.
- [ ] Root publishes the task and separately reviews current-producer/Main integration.
- [ ] Continue the active bounded downstream Goal; native Acc/QuickSort remains open.
