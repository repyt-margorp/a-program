# C Backend Epoch17 Handoff

Date: 2026-10-04 local. Parent task
`a4a2e988f4b916784b7f3f6dc7b05d04c78692d9`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: lower borrowed Nat32 predicates to native C`.

## Problem List

1. Deliver bounded native predicate/List C products with source/Core evidence,
   preserving the source boundary and distinct publication/integration states.

## 1. Selected Native Predicate and Container Contract

### Subjective (User)

2026-10-03, English paraphrase: prioritize bounded downstream `.a`/LinkerScript
C modules usable by ordinary C clients. Preserve source authority and explicit
unsupported contracts; keep prototype scope. Workers may publish their own task
branches and continue independently; only Core/Merge integrates Main. This is
the existing authorization, not a new predicate design or source-erasure decision.
The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains the user record and sole
issue-linked table.

### Objective (Code)

Seven implementation files change by +104/-19, prototype build adds eight lines,
and seven new test/fixture files contain 661 lines. Exact19 paths:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-NATIVE-PREDICATE-NOTE.md
doc/2026-10-04-C-BACKEND-EPOCH17-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/link/plan.h
src/prototype/c_backend/link/plan.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/lower/representation.h
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/scalar.h
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/predicate_native/fixture.p
src/prototype/c_backend/predicate_native/predicate.aplink
src/prototype/c_backend/predicate_native/client.c
src/prototype/c_backend/predicate_native/inert_test.c
src/prototype/c_backend/predicate_native/check.sh
src/prototype/c_backend/predicate_native/alias.aplink
src/prototype/c_backend/predicate_native/alias_client.c
```

Evidence is owned disk-backed
`src/prototype/c_backend/.evidence/epoch17-native-predicates-20261004`.
`epoch17-files.sha256`, `epoch17-submitted-snapshot.tar` and
`epoch17-freeze.json` pin these bytes; `epoch17-runs.sha256` pins raw evidence.
Runtime128 independently matches Root's qualified E8 producer
`50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`, manifest
`9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
All 191 input files have retained byte copies and a mapping. Relevant hashes:

| Input/result | SHA-256 |
| --- | --- |
| `verification.json` | `953eee81c00c55f7f4cee87bccca8d792866dbeab048d0c9040366f8c2ce07b1` |
| `qualified-inputs.sha256` | `4e7c04c28ed1f72ca7b57df3105434744831a9a1d839e92afb1585a2faa66c7c` |
| Backend | `305c521b526f7d5e281c48fdd461869cd174df8347f98c83b5a187d3b90bacfa` |
| Predicate Core/inert checker | `9d39408311827c5a5a12124d2f52c217ae2437e24aab93e083628b788ceccf07` |
| Qualified pointer checker | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| Final saved image | `e7adbdffbde0f907e56e374c5e7c5b47b3b83ea421ca9009d5a04e5916d11b77` |
| Main generated C | `8e2d5e3f191119cae610f70bacde5e972ecee38484f7d300532585c2fd27a1a2` |
| Main generated header | `221f41332be656c757f0e8a7708f3a7a94dd21440e4395530f7a2f90cb8f1a37` |

Strict O2 builds are serial j1 with owned compiler TMPDIR. Final
`o2-release`/`san-release` each exit0 with 69 expected rows: 38zero, 28four,
one each compiler-one/prior-output-two/no-fuel-three. Source/object/archive/shared
products each pass 1365 filters, 13650 stable partition selections, 574 separate
Core comparisons and six source observations (`F|T|T|F|1,2,|0,1,0,|`). Core uses
admitted source `keep`/`less_equal` definitions and existing constructors after
emission; bounded base-five fingerprints identify selected sequences. Emission
calls none of four wrapped advances and preserves graph/proof counts. Reversed
enum order and underscore alias pairs have ordinary C clients; main shared
definitions are exactly thirteen. Repeated products are not independent counts.

Final affected binary48, unary39, numeric/List O2 +client/source SAN and original
I/O19 pass. Five new-profile stream/close/receipt faults return2 with atomic
cleanup. Legacy unary/binary C/header bytes exactly match retained C16. Numeric
gate temporary products are removed by its unchanged cleanup; exact invocation,
terminal log/status are retained, without per-command numeric product hashes.
Tabs, English/ASCII comments/docs, no typedef/ML comments, shell syntax and diff
whitespace checks pass. No broad suite or comparative workload was launched.

Initial receipt grep/setup failure, actual omitted shared helper exports and
actual alias collision are retained. Alias admission/emission0 then compile1
proved the collision; source/helper corrections keep expectations unchanged.
A wrapper retry reused an existing output directory and stopped before child
execution. It overwrote the earlier wrapper console log, but the original
individual failed shared compile stderr/argv/status, products and pre-fix snapshots
remain. The pre-length backend hash is retained; its executable bytes were not
separately copied. Other original evidence remains; no worker ENOSPC is inferred.

Root's C16 current-E15 qualification/task/Main publication is recorded separately.
C17 Root current-producer verification, task publication and Main integration are
pending. This handoff supplies no accepted-source promotion or full Goal claim.

### Assessment

Agent bounded implementation: distinct `predicate_native_direct_v1` /
`c_predicate_native_v1`, using existing selected Nat32 domains and a selected
two-constructor nullary enum result. Arity is one/two independent Pi operands.
Public code/context is borrowed synchronously; invalid code/result tags return2,
preserve output and roll back only new allocations. Existing target validators,
finite array/List copies, capacity/depth limits and successor-overflow5 remain.
Length-qualified callback names/guards distinguish alias pairs; shared products
expose the existing native helpers. No producer/schema/.a/erasure field changes.

Foreign code must implement the declared pure-total source function; purity,
totality, readable storage and code/context lifetime are external preconditions,
not new checking receipts. Target Nat32/depth/allocation bounds are not source
restrictions. SAN instruments O1 clients and source bodies; emitted other products,
backend/producer/Core stay O2. Wrapped malloc injection/allocation observation cover
source/object/archive; shared capacity/depth/transactional controls remain. Native
Acc/QuickSort, indexed/callable recursive fields, general effects/Identity,
returned/boxed/dependent/mixed-domain/arity3 callbacks remain explicitly open or
refused. This advances #61 predicate/container interoperability, not a native sorter.

Core operational publication: shared Git stays read-only; delegate exact task
commit/push to Root from this immutable snapshot. Root owns current-producer audit
and any Main integration. Outbox routing and manifest payload are coordinator
workflow, not human design principles. Publication does not block separate owned
work outside the submitted bytes.

### Plan

- [x] Implement supported target contract and preserve old/unsupported refusals.
- [x] Complete pinned local O2/Core/source/client-SAN and affected controls.
- [x] Freeze exact19, pin evidence and notify Root with the chosen commit message.
- [ ] Root exact task publication/current-producer/Main review, separately recorded.
- [ ] Continue bounded generated native-predicate provider/client composition
  under existing success/lifetime contracts; preserve full Goal residuals.
