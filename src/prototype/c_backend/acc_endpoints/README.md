# Actual Acc QuickSort endpoint projections

This is a fixed, executable C candidate for the admitted Acc QuickSort. It
preserves the actual partition, pivot append, two recursive calls, Acc/down and
folded IH, nominal LT values and captured parameters. Seven source-derived bodies
are reused byte for byte from the qualified C44 products. The action/closure/frame,
Nat32 storage and array boundary are hand-authored and explicitly bounded. This
is not a substitute sorting algorithm or a general native-backend success.

The readonly reader borrows the existing typed Identity endpoints. Each actual
endpoint is an explicit Match on Nat.succ. Selecting that existing constructor's
clause and retaining its lexical binding exposes these nominal index operands:

| Branch | Left endpoint | Right endpoint | Target action |
| --- | --- | --- | --- |
| LT step | quoted TOTAL pure Acc(Nat,LT,slot2) | quoted TOTAL pure Acc(Nat,LT,slot7) | Right transport, then FORCE; down domain acts contravariantly. |
| LT weaken | LT(slot7,slot2) | LT(slot7,slot8) | Left transport into the captured down domain. |
| LT lift | LT(slot7,slot2) | LT(slot7,slot8) | Left transport before invoking the folded IH. |

The emitted table supplies these actual operands to the C action. It retains the
parent's full ordered maps, six descriptive contexts, two nominal branch paths,
raw down and folded IH separately. Equal integers cannot authorize a foreign
descriptor. The reader does not evaluate source code, synthesize a classifier,
check Scope, infer predecessor cancellation, create equality evidence or change
.a. Unknown scrutinees and the unselected zero shape refuse. Quoted forms require
the actual TOTAL grade and empty effect row. Raw negative test shapes are private
structural inputs, not new admitted source contracts.

`example/component.c` and its declarative header are ordinary standalone C. Only
`gs_sort` is public; an ordinary client can compile the source, link its object or
link a static archive with the same header. The fixed packaging recipe verifies
the sealed seven-body composition and actual endpoint table before emitting a
new product. Its provenance distinguishes source bodies from target runtime.
It does not replace the general backend's explicit native Acc refusal.

`check.sh` takes an emitter, saved materialized image, exact C44 product directory,
retained Core-case source, qualified pointer interpreter and new output directory.
Build the emitter and inspector serially with `build.mk` and the pinned overlay.
The affected gate covers source/object/archive clients, source output, reused Core
comparisons, allocation faults, depth/rollback, prior products, unsupported
selectors/families and private descriptor/domain mutations. Whole emitter,
inspector, module and clients are instrumented in the sanitizer phase; reused
pointer/Core inputs remain O2. The initial wrong test expectation is retained.

Complete source context-declaration semantics, generic Scope/action equivalence,
arbitrary callable/indexed Acc, independent capture/frame derivation and generalized
lowering remain open. Nat32/depth256/node65536, borrowed immutable input/private
token lifetime, nonoverlap, transactional output and finite sanitizer limits
persist; the overflow guard is not exercised by these finite cases. No measured
cost, accepted adoption, full #61 or Goal completion is claimed.
