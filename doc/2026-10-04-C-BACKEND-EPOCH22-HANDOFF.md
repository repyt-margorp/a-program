# C Backend Epoch22 Handoff

Date: 2026-10-04 local. Parent task
`eaf5755914a44a855069321b4c64330eb4ca5b03`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: retain private IH captures in nested recursion`.

## Problem List

1. Lower admitted nested folds capturing known private induction results under
   the existing native ABI, preserving dynamic and callable/indexed refusals.

## 1. Nested Private Induction Captures

### Subjective (User)

Existing human scope, concise English paraphrase dated 2026-10-03: prioritize
bounded downstream `.a`/LinkerScript products usable by other C modules; the
backend has no authority over A Program. Implementation lanes may continue
independently; workers may publish task branches while only Merge integrates
Main. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains the dated human
record and sole issue table. No new representation or source-erasure decision.

### Objective (Code)

Scalar lowering adds7/removes2 lines: nested recursive callees now retain known
delayed IHs and recursive target identities privately, as ordinary private
callees already do. Recursive calls check captured target identity; represented
dependencies retain dense native parameters. Existing thunk creation still
admits only known lambdas or direct recognized IH calls. Parent build adds8 lines;
six new fixture/client/inert/harness/script/build files contain430 lines.
README, owning Goal and nested plan record behavior, limits and evidence.
Exact12 paths:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-NESTED-CAPTURE-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH22-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/nested_capture/fixture.p
src/prototype/c_backend/nested_capture/nested.aplink
src/prototype/c_backend/nested_capture/client.c
src/prototype/c_backend/nested_capture/inert_test.c
src/prototype/c_backend/nested_capture/check.sh
src/prototype/c_backend/nested_capture/build.mk
```

Owned disk evidence:
`src/prototype/c_backend/.evidence/epoch22-nested-captures-20261004/`.
`epoch22-files.sha256`, `epoch22-submitted-snapshot.tar` and `epoch22-freeze.json`
pin the submitted files; `epoch22-runs.sha256` pins retained commands, exits,
initial/corrected inputs, products, generated source/headers/receipts and binaries.
`qualified-inputs.sha256` and its mapping retain192 selected inputs, including
all128 qualified runtime files. `source-deltas.json`, `style.json` and
`verification.json` give per-file deltas, checks and gate summaries. Relevant pins:

- Scalar source: `fa2e939c31f604c8d92062bde87eb59dd48d5a5851ed441a2610e30e0ad433f7`.
- Rebuilt O2 backend: `7da254af8867afebd87bac9563cfee0d3eb166493e5094dac16ff3c1fd640cc4`.
- Rebuilt O2 nested inert helper: `adedfddddce9dcfc967b056a5af98f83f21d22842253c4eccbc2d8704cc0d1dd`.
- Rebuilt O2 parent inert helper: `6fe6da7bf2c1826b027cc2604c66f37553fec199486a2f19b45f03348beb66ee`.
- Qualified E8 pointer: `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`.
- O2 record: `cc818dadab8bf2f216af46ab29532a0483d5fb8facee58e70b780ac0295a3eba`.
- SAN record: `6eb53f5ce1bf5173d46044d43a30a6bffbf55959bf09da70e858210075d0604a`.
- Parent O2/SAN records: `66f33686c4b4e8504fb266c6ad2cadab7906e7c45cbe8048c849ce077a2a8f32`,
  `f00a3e16bbb1491765e0a1221321cb0b1f85adeecc96dee823a65893117da06c`.

Worker producer remains qualified E8 `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`;
all128 runtime files match its existing qualification. Backend and two helpers
rebuilt serial j1; exact pointer reused, no producer-driver rebuild or timing.
Focused nested O2/client-source SAN40 expected rows each pass
(31zero/1no-fuel3/8refusal4). Three products and raw clients match eight source/
readback observations, `3,3,0,6,6,5,5,1`. Native clients cover198 finite Nat calls
and85 signed List cases, plus separate resource/reference calls. Signed seed32/64
extrema, curried/repeated IH, lexical capture order, input preservation and depth
failure/output/arena preservation pass. An unused IH succeeds even at native
`UINT32_MAX`, showing it is not forced eagerly. Counts repeat across products and
phases; they are not independent properties or source-unbounded execution.

Guarded raw emission calls no evaluator/substitution/WHNF/query routines and
changes no source graph/proof counts. Separate existing Core then supplies120
finite Nat/seed/List comparisons against generated C. Int64 APIs use an existing
typed parameter; descriptive Core comparisons construct typed parameter values,
without adding a source literal or changing synthesis. Closed source observations
remain Int32/Nat. Callable/indexed declarations ordinarily admit but their native
representations refuse4, as do dynamic functions and demanded effects, checked/
trusted without products. No-fuel3, determinism, equal headers, unchanged input
image and no staging residue pass. Affected C21 capture O2/client-source SAN114
expected rows each also pass (74zero/2no-fuel3/38refusal4).

Initial source/readback `3,3,0,6,6` and checked/trusted nested refusal4 remain
retained; count/nonrecursive branch controls emit0. The scratch source advanced
after those pins; original fixture/script were retained byte-exact against their
initial SHA-256. Expanded scratch and final source have distinct pins/results;
none is claimed to equal the initial fixture. Exact C21 task blobs/remote were
independently verified before live lowering advanced. Root reports C21 prototype
Main `8c00c80f97f7a7413b4b088efc94999fe58d33ff` and current E18+MEM9 joint gates
passed, raw `942062b6`; this is Root evidence, not C22 joint qualification.
Historical C20/C21 handoffs and submitted snapshots remain unchanged.

SAN covers O1 ASan/UBSan/leak clients and generated source/raw/oracle bodies;
emitted object/archive/backend/helpers/producer remain O2. Tabs, English/ASCII
comments/docs, no typedef/ML comments, shell syntax, whitespace and build gate
registration are checked. No active unexpected gate failure remains.

### Assessment

Agent bounded correction reuses existing target capture storage and admitted
source views. No public ABI, `.a` field/schema, producer, private checker/IR,
dynamic callable representation or source-erasure policy changes. Known private
thunks remain suspended until source use; static recursive identity is not a
public function pointer or a new ownership contract.

Existing target limits remain: borrowed nodes must be valid/readable and outlive
shared results; input/output/arena storage must not overlap. Recursive depth
defaults/maxes at256; Nat magnitude is uint32 with target-local overflow/depth
failure. General dynamic functions, callable/indexed Acc fields, native QuickSort,
higher Identity/effects/boxed ownership and full #61 remain unfinished/refused.
No accepted adoption, cost claim, issue closure or Goal completion follows.

Root operational workflow: exact task publication is delegated from this snapshot
because shared Git is read-only. Root owns current-producer review and Main
integration; task publication alone is not integration or accepted promotion.

### Plan

- [x] Reproduce the nested private IH refusal and implement the target correction.
- [x] Verify focused products/source/readback/raw inert/Core/O2/client-source SAN
  and affected C21 gates, with initial pins and unsupported controls retained.
- [x] Prepare exact12 code/test/docs and terminal evidence for frozen handoff.
- [ ] Root exact task publication/current-producer audit/Main integration.
- [ ] Continue only bounded justified downstream work; full #61 remains open.
