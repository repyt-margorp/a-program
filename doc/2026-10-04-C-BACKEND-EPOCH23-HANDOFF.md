# C Backend Epoch23 Handoff

Date: 2026-10-04 local. Parent task
`8b6debfcf836451b9e972aba9f3f624f24b406e9`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: bind selected types in native source calls`.

## Problem List

1. Bind existing selected type constants in private native calls and verify an
   existing admitted source insertion algorithm as usable ordinary C products.

## 1. Private Selected Types and Existing Source Sorting

### Subjective (User)

Existing human scope, concise English paraphrase dated 2026-10-03: prioritize
bounded downstream `.a`/LinkerScript products usable by ordinary C modules; the
backend has no authority over A Program. Implementation lanes may continue
independently; workers may publish task branches while only Merge integrates
Main. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains dated human statements
and the sole issue table. No new source algorithm, representation or erasure
decision is attributed to the user.

### Objective (Code)

Against C22, scalar lowering adds16/removes5 lines: a selected declaration or
represented builtin type reference can remain a private identity token. Known
calls bind it through the existing inline path; private recursive captures check
object identity, represented dependencies remain dense C parameters, and metadata
tokens produce no C definitions. No public Universe/type dictionary ABI exists.
Receipt serialization adds1/removes1 line to identify the profile capability.
Parent build adds8 lines; six new fixture/script/client/inert/harness/build files
contain500 lines. No producer/schema/accepted implementation changes.
Exact13 paths:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-SOURCE-SORT-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH23-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/source_sort/fixture.p
src/prototype/c_backend/source_sort/source.aplink
src/prototype/c_backend/source_sort/client.c
src/prototype/c_backend/source_sort/inert_test.c
src/prototype/c_backend/source_sort/check.sh
src/prototype/c_backend/source_sort/build.mk
```

Owned disk evidence:
`src/prototype/c_backend/.evidence/epoch23-source-sort-20261004/`.
`epoch23-files.sha256`, `epoch23-submitted-snapshot.tar`, `epoch23-freeze.json`
pin the exact submission. `epoch23-runs.sha256` pins commands/exits, initial and
corrected inputs, images, C/header/receipt products, binaries and comparisons.
`qualified-inputs.sha256`/mapping retain207 selected inputs, including the exact
128 qualified runtime files. `verification.json`, `source-deltas.json` and
`style.json` summarize verification, deltas and style. Relevant SHA-256 pins:

- Scalar: `9246bf1c7cd31c64d22158b12b9898855d7be69852787d2b2d41091a7f812977`.
- Receipt driver: `2e576e61a9dd3fd5838353fcab5dfc0628ef9a0f4823a8152db14ee4f126f25b`.
- Rebuilt O2 backend: `aa912a789a154bf823704631402dc524281dc5e639bdcbc00656248582325696`.
- Rebuilt O2 source-sort inert helper: `86428cf8fd475091b1ebf4f4308f143bd63edae988d8e2807fa67d9d3e6e4d47`.
- Rebuilt O2 nested helper: `4bc5def3554db5a1b25e65440bf2f23c4620fee420a775ad65084c3a72453b49`.
- Qualified E8 pointer: `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`.
- Unchanged source provider: `a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`.
- O2/SAN records: `23f9a5b92127c134bd43244663f2e258e1d1570326ad8ed967eb157176ee0e7b`,
  `5e504ab7a19575c831adf8cf97d968ee83c43d80bf253304e60149b63c0d2b11`.
- Parent O2/SAN: `dfa9e0ff713f3d4be9003406ff5a7185dc4d7221f04498b7e9a5e218fc018c45`,
  `deb65ca92192d332c21675e005c59b3201fdf2b99b0dca528976a700763209dc`.
- I/O record: `99bcd31c2aab5dd439abf888990fa24df1108cf89f679d95cceeef54c8db5042`.

Worker producer remains qualified E8 `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`;
all128 runtime inputs match its qualification. Backend/two helpers rebuilt serial
j1, exact pointer reused, no producer-driver rebuild or comparative timing.
The wrapper explicitly imports unchanged `insertBy`, `insertionSortBy`,
`insertionSort` and Nat comparison from `tests/fixtures/sorted-proof-provider.p`.
Its legacy intrinsic spelling requires the existing admission flag; provider
bytes and Source bodies are unchanged. There is no C replacement sorter.

Final O2/client-source ASan/UBSan/leak gates each pass48 expected rows
(31zero/1no-fuel3/16refusal4). Source/object/archive and raw clients match nine
source/readback observations:
`|0,1,2,2,3,|0,1,2,3,|0,1,2,2,3,|0,1,2,2,2,3,|0,|3|T|F|`.
Each client tests341 Lists (length0..4, Nat values0..3), two source sorts/four
insertion pivots/identity per List:2387 calls plus scalar/reference/resource calls.
Sort checks nondecreasing order and multiplicities; insertion checks source
behavior even for unsorted input, without claiming a sorted-input guarantee.
Seven Nat identities, Int32/Int64 extrema, both Bool tags,25 comparisons, persistent
input, copy padding/insufficient-buffer preservation and resource controls pass.
Malformed/null/cyclic inputs, partial allocation failure and recursive depth
failure preserve output and restore the arena; earlier successful inputs stay live.
Maximum-Nat singleton sorting succeeds; comparing two maxima fails depth4 with
rollback. Int64 uses a typed parameter, without changing literal synthesis.

Raw emission invokes none of four guarded evaluator/substitution/WHNF/query
routines and changes no graph/proof counts. After emission, existing Core
supplies552 finite sort/insertion/identity/comparison results for generated C.
The finite List fingerprint includes length unambiguously for tested payloads;
these are descriptive comparisons, not new transformation checking authority.
Counts repeat across products/phases and are not independent properties or
unbounded source execution. Headers/determinism/trusted bytes/image preservation
and no staging residue pass; no validation fuel returns3 without a product.

Eight ordinary-admitted negative exports each refuse4 checked/trusted without
products/stdout: public generic identity, public type result, unused Text type,
unused unselected nominal type, dynamic comparator, demanded effect, existing
indexed/callable QuickSort and a computed/applied List type argument. Known
selected Nat/Bool/Int32/Int64 bindings are positive under the same existing ABI.
Affected C22 nested O2/SAN40 expected rows each pass (31zero/1three/8four).
Original linker O2 passes; its harness deletes temporary products, so retained
invocation/xtrace/exit evidence makes no product-byte pin claim. Publication-I/O19
controls pass (4zero/13I-O2/2refusal4), including stream errors/close errors, atomic
cleanup, unsupported status and prior-output preservation.

Initial native failures remain: first computed List selection refused before
lowering; correcting the fixture to existing `data_of empty` made List identity
and comparison positive while fixed-type identity/insertion/sorting still refused4.
The private type-binding correction resolves those latter target cases. Initial
source/scripts/results and corrected trials have distinct pins. All final gate
source inputs remain identical to pre-build pins. Exact12 C22 task/independent
remote were verified before live implementation advanced; its immutable evidence
remains unchanged. No C22 Main/current-producer result is inferred.

SAN covers O1 clients/generated source/raw/oracle bodies; emitted object/archive,
backend/helpers/producer remain O2. Tabs, English/ASCII comments/docs, no typedef
or ML comments, shell syntax, whitespace and gate registration are checked.
No unexpected current gate failure remains; no cost or fully instrumented compiler
claim is made.

### Assessment

Agent bounded target decision: bind only represented known type constants through
existing private call machinery. The receipt transformations list describes
profile capabilities rather than a per-call trace, consistent with existing
arithmetic/capture entries. No `.a` fields/schema, source evaluator/checker/IR,
source graph mutation, public generic type or ownership contract is introduced.
Unselected/computed types and arbitrary proof/erasure policies remain outside scope.

This intermediate native existing-source insertion-sort milestone does not
implement Acc/QuickSort. Indexed SizedList, callable recursive Acc fields,
higher Identity/effects/boxed ownership and full #61 remain open. Borrowed nodes
must be readable and outlive shared results; input/output/arena storage must not
overlap. Nat is target uint32, recursion defaults/maxes at256, allocations belong
to the supplied arena and failures roll back. Source semantics are not bounded by
these target-local limits. No accepted promotion, issue closure or Goal completion.

Root operational workflow: exact task publication is delegated from this frozen
snapshot because shared Git metadata is read-only. Current-producer qualification
and Main integration remain separate Root work; local E8 gates imply neither.

### Plan

- [x] Reproduce selected-type argument refusals and implement private bindings.
- [x] Verify existing source sorting/products/source/Core/inert/O2/client-source
  SAN, affected nested/link/I-O controls and retained initial failure evidence.
- [x] Prepare exact13 code/test/docs and terminal evidence for frozen handoff.
- [ ] Root exact task publication/current-producer qualification/Main review.
- [ ] Continue bounded justified downstream work; native Acc/QuickSort/full #61 open.
