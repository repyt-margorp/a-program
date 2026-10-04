# C Backend Epoch24 Handoff

Date: 2026-10-04 local. Parent task
`1a1e2614427397a79bad7f9a857b24b9d5bdb43c`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: bind selected applied types in native calls`.

## Problem List

1. Bind exact selected applied classifiers in private native calls and preserve
   checked operand value phase through partial private inlining.

## 1. Selected Applied Types and Pending Native Values

### Subjective (User)

Existing human scope, concise English paraphrase dated 2026-10-03: prioritize
bounded downstream `.a`/LinkerScript products usable by ordinary C modules;
the backend has no authority over A Program. Implementation lanes continue
independently; workers may publish task branches while only Merge integrates
Main. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains dated human requirements
and the single issue table. No new generic-type or erasure decision is attributed
to the user.

### Objective (Code)

Against exact C23, scalar adds13/removes9 lines. A private selected type token
borrows the exact retained classifier Term, including a selected closed applied
List or Pair, through the existing instance identity index. Declaration/builtin
guards and no pending operands prevent arbitrary head acceptance. No type
normalization, substitution, inferred instance, runtime dictionary or public
Universe ABI is added. Private recursive captures compare exact token identity.

The second correction preserves the checked carrier expression when forwarding
unconsumed inline-call operands. The old path stored the underlying C computation
expression instead; its computation marker reappeared after the value-phase
check and refused valid generic calls inside a fold. Both direct and explicitly
sequenced Source forms now have positive native/Core controls; no Source demand
or effect rule is changed. Source-sort tests reconcile the former C23 applied
List refusal explicitly (+11/-5). Parent build adds8 lines; six new controls464
lines. No producer/schema/accepted code or shared interface change.
Exact16 files:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-APPLIED-TYPE-BINDING-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH24-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/source_sort/source.aplink
src/prototype/c_backend/source_sort/client.c
src/prototype/c_backend/source_sort/inert_test.c
src/prototype/c_backend/source_sort/check.sh
src/prototype/c_backend/applied_types/fixture.p
src/prototype/c_backend/applied_types/types.aplink
src/prototype/c_backend/applied_types/client.c
src/prototype/c_backend/applied_types/inert_test.c
src/prototype/c_backend/applied_types/check.sh
src/prototype/c_backend/applied_types/build.mk
```

Owned disk evidence:
`src/prototype/c_backend/.evidence/epoch24-applied-type-binding-20261004/`.
`epoch24-files.sha256`, `epoch24-submitted-snapshot.tar` and `epoch24-freeze.json`
pin the exact submission. `epoch24-runs.sha256` pins commands/exits, initial/
expanded inputs, diagnostics, images, generated source/header/receipt products,
binary hashes and comparisons. `qualified-inputs.sha256`/mapping retain218 selected
inputs, including all128 qualified runtime files. `verification.json`,
`source-deltas.json`, `style.json` and `parent-generated-bytes.json` summarize
terminal gates, deltas, style and26 unchanged parent C/header files. Relevant pins:

- Scalar: `a884444784855e4d0306987a9c138680e688fdd5a5cdedebfc1f6439ba504704`.
- Rebuilt O2 backend: `f835425f96308f459954585127b4c7dbafe43362fc218415b126eff2f3a8256f`.
- Rebuilt O2 applied-type helper: `1bf03a5e58c7fabc6fecddeefcc4809938bf0abd78ce9c0885d6b37c3e9c9b2e`.
- Rebuilt O2 source-sort helper: `bf24948e64f54a0d2c6a5179afee3b1fc3b26eb3bda002ae4f8d2fc44dcccf44`.
- Rebuilt O2 nested helper: `da0b8e06d069debf7cacae290779b0b2956789f9b33cc98261f4f64eb7d1a950`.
- Qualified E8 pointer: `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`.
- New O2/SAN records: `96c2a0086b7e3e699722b724e8007c8e33caade0810404712337b49cc33897fd`,
  `400bd4973318b3b1fb700f78302f0196ea95569c95598877d5bede4e63c2364f`.
- Affected source-sort O2/SAN: `866500970cc211fb4fe70cc1532471ea75ac9731e562a12ec512f4e430db129e`,
  `8615fd9eb9373ec5db2cac5a01e91a6ddf2de261afb26a275a60abd5a2914bc3`.
- Nested O2/SAN: `1423c7b043126431b5f12a1b0b63e78fec3f8988fc6f9b656c3f19bced851aaa`,
  `c687bd3eb476f8d2ae34edb94714c15a5e269a9a212b0aa9dd175c6647b52764`.
- I/O record: `008af0033cc14cd865438aa6b480e6d721174e1b5041db7709ab1c1bfe7190e4`.

Worker producer remains qualified E8 `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`;
runtime128 exactly matches its qualification. Backend/three helpers rebuilt
serial j1; exact pointer reused. No producer-driver rebuild, broad suite or timing.
Final new O2/client-source ASan/UBSan/leak gates each pass50 expected rows
(31zero/1no-fuel3/18refusal4). Three products/raw clients match eight Source/
readback observations `12222204`; seven exports cover fixed/aliased/captured
selected List identity, both generic fold forms, finite Pair and Nat identity.
Each client covers341 Lists (length0..4, values0..3), five forms/List:1705 calls,
plus scalar/reference/resource controls. Identity preserves the input pointer;
folds preserve input payloads. Finite Pair active/inactive fields, selected two
parameters and uint32 extrema pass. Maximum-Nat List mapping succeeds without
Nat comparison. Malformed/null/cyclic input, partial capacity failure, depth
failure and insufficient copy buffer preserve output/arena/prior input.

Guarded raw emission invokes none of evaluator/substitution/WHNF/query advancement
and changes no graph/proof counts. After emission, existing Core supplies445
finite List/Pair/Nat comparisons against generated C. Fingerprints include finite
length/payloads unambiguously; these descriptive comparisons are not a new checker
or transformation evidence authority. Counts repeat across products/phases and
are not independent properties or unbounded Source execution.

Seven ordinary-admitted negative exports each refuse4 checked/trusted without
products/stdout: public generic identity, public type result, unused unselected
List Bool, open family argument, other Pair instance, public unselected List Bool
and demanded effect. Second selected instance of one erased layout and indexed
selection also refuse4 in both modes. No-fuel3, deterministic/trusted bytes,
equal headers, unchanged images and no staging residue pass. Dynamic/native
Acc/QuickSort remain checked/trusted refusals in the affected source-sort gate.

Affected source-sort O2/client-source SAN46 each pass
(31zero/1three/14four), nine observations and637 separate Core comparisons. Former
`fixed_list_generic` refusal is an explicit export/client/Core positive; unchanged
source insertion algorithm/provider bodies remain exact. Nested O2/SAN40 each
pass, and26 generated parent C/header files exactly match the C23 worker run.
Publication-I/O19 controls pass (4zero/13I-O2/2refusal4), including atomic cleanup,
unsupported status and prior output; retained exact E8 image/script reused.

Initial computed Pair `data_of` selection failure is retained as fixture setup;
an admitted nullary constructor classifier resolves it. Original selected applied
List/Pair refusal4, exact-term trial positives, both fold refusal4 variants and
target diagnostic output are retained with distinct source/script/image pins.
The diagnostic identifies loss of operand phase after checked private inlining;
the carrier correction resolves both original forms. Final source inputs match
pre-build pins. No unexpected current gate failure remains.

Exact13 C23 task/then-live blobs and independent remote were verified at parent
`1a1e261` before live files advanced; immutable C23 archive stays exact. Root
reports C22 prototype Main `a0a9dc38`/current E18+MEM9 nested40/recursive114/
integer61 each O2/SAN passed, rawce041c85. These are Root reports; C23/C24 current-
producer/Main qualification is unreported here. No accepted promotion follows.

SAN instruments O1 clients/generated source/raw/oracle bodies; emitted object/
archive/backend/helpers/producer remain O2. Tabs, English/ASCII comments/docs,
no typedef/ML comments, shell syntax, whitespace and gate registration pass.

### Assessment

Agent bounded decisions reuse selected classifier identity and existing private
operand storage. No Source evaluation/substitution, new Terms/evidence, `.a`
fields/schema, producer/private checker/IR, dynamic type dictionary or public
generic ABI is added. Source value/computation phase and admitted selected
classifier remain authoritative; arbitrary unselected/open types stay refused.
No Source proof/Acc field is silently erased or replaced by a manual sorter.

Existing target limits remain: readable borrowed nodes outlive shared results;
input/output/arena storage must not overlap; recursive default/max256 and uint32
Nat magnitude are target-local, with transactional allocation/depth failures.
Callable/indexed Acc, native QuickSort, general Identity/effects/boxed ownership
and full #61 remain open. No cost/adoption/issue closure or Goal completion claim.

Root operational workflow: shared Git is read-only, so exact task publication
is delegated from this snapshot. Current-producer qualification and Main
integration remain separate Root work. Notification evidence grants no new approval.

### Plan

- [x] Reproduce selected applied-type and pending value-phase refusals; implement
  bounded corrections and retain original setup/diagnostic/failure evidence.
- [x] Verify ordinary products/Source/Core/inert/O2/client-source SAN/resource/
  refusal controls and affected source-sort/nested/I-O gates.
- [x] Prepare exact16 code/test/docs and terminal evidence for frozen handoff.
- [ ] Root exact task publication/current-producer qualification/Main review.
- [ ] Continue bounded downstream work; native Acc/QuickSort/full #61 remain open.
