# C Backend Epoch6 Handoff

Date: 2026-10-03
Status: verified and frozen for delegated task-branch publication.
Branch: `parallel/c-backend-20261003`
Base: `312c2da155636da8415880eb674e1fb0455e3b95`
Chosen commit message: `prototype: lower selected nested value records to native C`
Related: [Goal](2026-10-03-C-BACKEND-GOAL.md),
[record plan](2026-10-03-C-BACKEND-NESTED-RECORD-PLAN.md).

## Problem List

1. Publish bounded native value-record composition for ordinary C modules,
   preserving admission authority and explicit unsupported contracts.

## 1. Selected Value Records

### Subjective (User)

2026-10-03, English paraphrase of direct user decisions recorded in the owning
Goal: produce readily usable C modules through `.a` and LinkerScript, keep the
backend downstream and bounded, and avoid excessive deep investigation. Do not
extend `.a` for target convenience. Workers may publish their task branches;
only Core integrates Main, with no worker promotion or issue closure. Keep the
Goal active. Core waits for notification OR a six-hour timer and checks even
without notice. Selection order, freeze, payload and implementation choices
below are agent/Core operational decisions, not new user design principles.

### Objective (Code)

At the base plus these exact edits, closed nonrecursive `data` selections can
contain earlier selected nonrecursive value data. Existing C tagged structs and
native constructor/Match/call machinery carry whole values through field
extraction, captures, private calls and public results. Private generated C
validators check only active constructor fields, including nested tags. Public
status 1 remains null output, status 2 invalid input and status 0 success; failures
preserve output. By-value input/output aliasing remains supported. Record-only
products need no allocator, structural runtime or compiler library.

Representation selection still uses its existing single pass. Missing/later
child selections, recursive aggregate payloads and value records containing
recursive pointers refuse with status 4 before publication. The simple selected
Chain alone succeeds; ChainBox refuses. Function/dependent fields, indexed/tree
containers, dynamic callbacks, demanded effects and the specific three-closure
capture chain retain explicit negative controls. The old data Nested refusal
still omits its Packet selection and remains unchanged. No `.a`, schema, producer,
source synthesis, shared owner or private A Program checker was changed.

Receipts copy the successful emitter's nested-value-field flag into
`nested-value-fields` / `selected-value-data`; they do not reconstruct a second
representation table. This metadata is private to the downstream C implementation.
The public ABI version and storage/ownership conventions are unchanged.

Fresh final verification passes all ten O2 C gates: backend plus structural
Oracle, Linker, scalar/raw Oracle, enum, data, List, numeric/List, static functions,
sorting boundary and new value records. Seven native generated-client gates pass
ASan/UBSan with leak detection: records, scalar/raw, enum, data, List, numeric/List
and static functions. No remaining gate failure.

The record gate verifies 105 Int32/Int64/enum cases per source/object/archive
product, 16 source/C observations, whole-value extraction/capture/return, output
aliasing, nested invalid tags, null output and inactive poisoned union fields.
Repeated products are deterministic; checked/trusted C and headers agree, input
image hashes remain unchanged and every unsupported control publishes nothing.
The final source admits in 2,950 steps and checked linking reconstructs in 2,968.
Int64 inputs come from existing typed fields supplied by the C client; surface
integer literals still synthesize Int32 and reject larger values, and `::` remains
post-synthesis assertion.

The extended raw Pair/Box correspondence fixture emits seven exports while
forbidding evaluator/substitution calls and preserving source graph/store counts.
Sixteen Int32/Int64 extrema combinations exercise whole-value box/unbox and field
round trips; 16 boxed scalar answers are compared with the existing evaluator.
Invalid nested tags preserve output, and ambiguous reused layouts still refuse
before either output stream is written. Raw descriptions are correspondence
tests, not admission receipts; the surface record fixture is admitted separately.

Retained resolved failures: the initial surface Bool Match lacked parentheses
(admission rejected at 1,889 steps); a generated script was missing after that
early stop; and the first raw harness used a nonexistent clause member, so its
build failed. The first scalar run used the earlier raw binary and does not count
as new raw evidence. Corrected fixture/setup/build and final rebuilt gates pass.
Earlier logs remain under the evidence root. These errors establish no producer
bug; no producer or lowering workaround was used to repair them.

Target limitations remain: native Acc/QuickSort is unsupported for its applied
family/representation; structural FFTT execution is separate. Nat32 successor
overflow returns 5, recursive Match defaults/maxes at 256 active calls with depth
failure 4, and allocation/capacity failure returns 3. Finite List/array conversion
is iterative. Caller storage/lifetime obligations remain, and aggregate size/C
stack resources still bound by-value products. These are target contracts, not
source totality evidence. This epoch does not complete AP6/#44/#49 or the Goal.

#### Evidence and Pins

Evidence root: `/tmp/a-program-c-backend-nested-trial`.
Final logs are `final-*-o2.log`, `final-*-san.log`, `final-build.log`,
`final-admission.log`, `final-emission.log` and `final-trusted-emission.log`.
Earlier probe logs and the failed/corrected raw build logs are retained separately.
Backend/producer/raw generators are O2. Sanitizers instrument generated source
bodies and every client; driver-produced object/archive bodies remain O2. No
comparative wall/RSS or fully instrumented compiler claim.

Worker build uses the immutable dereferenced Core family/Surface snapshot
`/tmp/a-program-c-backend-epoch4-current/core-snapshot`. Its 1,025-file manifest
`core-snapshot.sha256` verifies unchanged, with digest
`c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`.
This is the pinned worker producer, not latest Main combined verification. Core
must verify its current producer independently before integration.

| Tool | Path | SHA256 |
| --- | --- | --- |
| Backend | `/tmp/a-program-c-backend-nested-trial/build/a-to-c` | `2696c4e55ece3244c7e957c271317699ce9949cf2a3e93df975c86377061eac1` |
| Producer | `/tmp/a-program-core-c3-combined/build/pointer-check` | `4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27` |
| Scalar Oracle | `/tmp/a-program-c-backend-nested-trial/build/c_scalar_test` | `af41bd498685d5068699b1e513e9bdf1834b2b3c8404367eacc41bda95b6455d` |
| Static Oracle | `/tmp/a-program-c-backend-nested-trial/build/c_static_oracle_test` | `f414f23d21fe0b67223c2f7791f0bce199219ef1a4b609ebefe1f6dfe108e114` |
| Structural Oracle | `/tmp/a-program-core-c3-combined/build/c_oracle_test` | `d4d5b2d8506fbbe8ff8fa7635d1e350879341e03da4c98a07f386fd409024052` |

Final retained products under `evidence/final/`:

| File | Lines/bytes where applicable | SHA256 |
| --- | --- | --- |
| `nested-records.a` | admitted 2,950 steps | `f5d29ce9fc5e19b6dba5cae1e85f53445b7814dced2b8ae0254efec7f16871de` |
| `source/component.c` | 476 / 10,957 | `ae94f7ac2a68c4533877f3b3f0a46f8e58b12df9c6f4bef16920ac395acdef7c` |
| `source/component.h` | 45 / 1,743 | `e14763eabbfcf205d1c5c1970072ced26cb09df7fbff4d84ef732bfea5452fc6` |
| `source/link.json` | emitter-selected nested contract | `a5fe360a666a7a2773499bc7cd9f4d3451df10edd71dbcbab475a050f49375b5` |

Final fixture source SHA256:
`38f1e5f24c8b8a5443acc7c3f9407a6afca9d9e6ef3467416eacb2ac8c548789`.

#### Exact Publication Files

Manifest: `/tmp/a-program-c-backend-nested-trial/epoch6-files.sha256`.
The exact 16-file set is:

```text
doc/2026-10-03-C-BACKEND-EPOCH6-HANDOFF.md
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-NESTED-RECORD-PLAN.md
doc/2026-10-03-C-BACKEND-STATIC-FUNCTION-PLAN.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/lower/oracle_test.c
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/lower/scalar.h
src/prototype/c_backend/lower/nested_record_check.sh
src/prototype/c_backend/lower/nested_record_client.c
src/prototype/c_backend/fixtures/nested_records.aplink
src/prototype/c_backend/fixtures/nested_records.p
src/prototype/c_backend/fixtures/nested_records_differential.p
```

Per-file additions/deletions are retained in `epoch6-deltas.tsv`. Lowering changes
are `representation.c` +9/-1, `scalar.c` +40/-14; private metadata is `scalar.h`
+1/-1 and `link/driver.c` +2/-0. Build wiring adds four lines, raw coverage is
`oracle_test.c` +31/-4, and five new fixture/client/gate files cover the contract.
Documentation records substantive evidence and the prior Core publication report.
Old handoffs/manifests, outbox notices and generated temporary products are excluded.
No accepted-source or shared-owner file is part of this publication set.

Final repository checks cover tabs on added source lines, English/ASCII
comments/docs, C-style fixture comments, shell syntax and `git diff --check`.
Unchanged baseline comment-star alignment is retained. Keep these 16 files fixed until Core
releases this epoch; the full Goal remains active.

### Assessment

Agent decision: use existing complete C structs and native call machinery for
closed selected value fields. Child-before-parent selection and explicit recursive
field refusal bound the change without a general type/dependency pass. Runtime
active-tag validation protects ordinary C clients; source admission stays with
the existing producer. No shared-producer request is needed for this epoch.

Core operational evidence: Epoch5 `312c2da` passed nine combined O2 gates on the
reviewed E1/performance/family/Surface 128-runtime-hash snapshot and was merged
into prototype Main as `435d965`, with push next. This does not verify Epoch6.
Old evidence stays unchanged; Core released Epoch5 for these distinct edits.

Core operational publication/routing: shared Git metadata remains read-only.
Core performs authorized task-branch commit/push from this exact manifest and
reviews integration separately. Send one newline-terminated, at-most-8-KiB ready
notice through the worker outbox, with manifest pointer/hash, gate results and
remaining limits. Do not retry Git writes, merge Main or promote accepted code.

### Plan

- [x] Implement only bounded selected closed value-record composition.
- [x] Verify ordinary C source/object/archive products, raw correspondence,
  source observations, immutable artifacts, output preservation and refusals.
- [x] Pass ten O2 and seven native generated-client sanitizer gates.
- [x] Record exact code/test/docs files, source deltas, pins and resolved failures.
- [ ] Core publishes the frozen task-branch epoch and verifies all ten gates with
  its current combined producer before separate Main integration review.
- Completion: this bounded record contract is verified. Remaining native Acc,
  callback/effect/Identity/shared nominal obligations and the full Goal stay open.
