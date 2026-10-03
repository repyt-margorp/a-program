# C Backend Epoch7 Handoff

Date: 2026-10-03
Status: verified and frozen for delegated task-branch publication.
Branch: `parallel/c-backend-20261003`
Base: `cea1dc0177d36328d6f4de2a8f151b5952d2c2c5`
Chosen commit message: `prototype: add transactional enum List array boundary`
Related: [Goal and single issue table](2026-10-03-C-BACKEND-GOAL.md),
[enum List plan](2026-10-03-C-BACKEND-ENUM-LIST-PLAN.md), AP6.5/#49.

## Problem List

1. Publish bounded finite enum array/List conversion for ordinary C clients,
   preserving source authority, failure transactions and explicit limitations.

## 1. Enum Array Boundary

### Subjective (User)

2026-10-03, English paraphrase of direct user scope recorded in the owning Goal:
emit C readily usable by other C modules through `.a` and LinkerScript, keep work
bounded and downstream, and avoid excessive deep investigation. Do not extend
`.a` for target convenience. Workers may publish their task branches; only Core
integrates Main. Keep the full Goal active.

2026-10-03, English paraphrase of the direct human concern: issues accumulate and
progress is hard to see; give concrete issue-linked feedback showing which
subproblem advanced and what implementation/verification was done. Table fields,
report cadence, freeze and publication mechanics below are Core workflow decisions.

### Objective (Code)

At the base plus these exact edits, the existing two-constructor, single-payload
List shape accepts an explicitly selected enum32 payload for finite copy-in/out.
It uses arrays of the selected `struct ap_enum_ALIAS`, not integer/boxed handles.
Constructor order and Self/payload field order remain independent of spelling.
The ordinary generated API is `ap_from_LIST(arena, array, count, out)` and
`ap_copy_LIST(input, buffer, capacity, written)`; public ABI version is unchanged.

`representation.c` changes one predicate (+1/-1). `nodes.c` adds enum tag checks
before allocation/arena mutation and documents status 2 (+8/-3). Copies retain
existing typed field assignment, finite-node validation, output preservation and
rollback of only the failing call's new nodes. Empty arrays accept NULL input and
still allocate the terminal node. Invalid enum elements preserve output, prior
allocations and arena state without calling malloc. Inputs may change or expire
after successful copying. Copy-out validates/counts before writing; failed calls
preserve buffer and written length. Storage overlap remains forbidden.

Fresh active verification passes all eleven O2 C gates: backend plus structural
Oracle, Linker, scalar/raw Oracle, enum, data, List, numeric/List, static functions,
value records, sorting boundary and new enum List arrays. Eight native generated
source/client ASan/UBSan/leak gates pass: enum List, scalar/raw, enum, data, List,
numeric/List, static functions and value records. No active gate failure.

The new gate checks 875 cases per source/object/archive product (511 two-state
arrays and 364 three-state arrays), 24 source/C observations, filter/flip/append,
both constructor/field orders, copied-input independence, invalid tags in every
array position, five allocation failure positions, capacity rollback preserving
prior results, null/empty/length-overflow controls and malformed/cyclic inputs.
300-node conversion succeeds independently of recursive execution depth; an actual
native length call then returns depth status 4 with unchanged output. Distinct enum
arrays fail the incompatible C argument compile control. Repeated products,
checked/trusted C equality, exhausted checking fuel without publication and input
image immutability pass. Source admission is 5,347 steps; checked reconstruction
is 5,378. Existing multi-payload nodes have no array helper; aggregate/tree fields,
dynamic callbacks and demanded effects retain status-4/no-product controls.

The scalar raw harness adds a reversed-layout enum chain: 17 evaluator observations
of selected heads, ordered whole-enum copy-in/out, borrowed identity and invalid
input preservation. Emission forbids evaluator/substitution advancement and
preserves source Term/object/occurrence/proof counts. Raw fixtures describe
correspondence, not source acceptance; the surface fixture is admitted separately.
The first generated raw client hit GCC's maybe-uninitialized warning for a count-0
array pointer. Its empty case now passes the documented NULL input; rebuilt active
O2/sanitizer gates pass. The failed log is retained, with no producer/lowering
workaround. Scratch copies/build override are removed and excluded from publication.

Explicit limits remain: no aggregate/multi-payload List array specialization,
native Acc/QuickSort, indexed/dependent data, public callback/effect ABI, general
shared nominal exchange or higher Identity completion. The three-closure capture
chain remains a refusal. Structural Acc FFTT is separate from native sorting.
Nat32 overflow status 5, allocation/capacity status 3 and default/max recursive
depth 256/status 4 are unchanged target contracts. Source integer literals remain
Int32-only with `::` as assertion. This is a C boundary increment, not full AP6,
#44/#49 closure, source acceptance authority or accepted-code promotion.

The owning Goal now contains one current issue table. The direct human feedback
concern stays concise in Subjective; table/cadence/freeze decisions are in
Assessment/Plan. The temporary freeze feedback note is removed after incorporation.
Historical C1-C6 handoffs/manifests remain unchanged. Fresh archive verification
confirms all 16 C6 hashes at `cea1dc0`. Core reports C6's ten O2 gates against joint
E3 (128-runtime manifest abbreviated `e87b8781...`), prototype Main merge
`53debc870231f415bdf926b537a7c1a8670e81d7` and push complete, evidence `fd45c42`.
These are Core results for C6, not C7 joint evidence; worker sanitizers stay separate.

#### Evidence and Pins

Evidence root: `/tmp/a-program-c-backend-epoch7`, logs `*-o2.log`, `*-san.log`,
`build.log`, `raw-rebuild.log`, `admission.log`, `emission.log` and
`trusted-emission.log`. `scalar-first-o2.log` retains the resolved warning.
Earlier isolated proof/patches are under `/tmp/a-program-c-backend-enum-list-trial`.
Backend/producer/raw generators are O2. Sanitizers instrument generated source
bodies and every client; driver-produced object/archive bodies remain O2. No
comparative performance or fully instrumented compiler claim.

Worker build uses the immutable dereferenced Core family/Surface snapshot
`/tmp/a-program-c-backend-epoch4-current/core-snapshot`. Its 1,025-file manifest
`core-snapshot.sha256` verifies unchanged, digest
`c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`.
This pinned producer differs from latest joint E3; Core must verify C7 separately.

| Tool | Path | SHA256 |
| --- | --- | --- |
| Backend | `/tmp/a-program-c-backend-epoch7/build/a-to-c` | `8d5875aa4c874279e38784a68014e897eee0c349fd8f875e4b8999352450bd1d` |
| Producer | `/tmp/a-program-core-c3-combined/build/pointer-check` | `4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27` |
| Scalar Oracle | `/tmp/a-program-c-backend-epoch7/build/c_scalar_test` | `f03585adefb494ba5f9383ccc8cf2ebc4a3dd4eafcf5235082b0eb80f0f71df0` |
| Static Oracle | `/tmp/a-program-c-backend-epoch7/build/c_static_oracle_test` | `991c9fc16ebb6ccf5f0347049721ae5c2f147da704dbafa8ca64a9ba696ba93b` |
| Structural Oracle | `/tmp/a-program-core-c3-combined/build/c_oracle_test` | `d4d5b2d8506fbbe8ff8fa7635d1e350879341e03da4c98a07f386fd409024052` |

Retained examples under `evidence/`:

| File | Lines/bytes where applicable | SHA256 |
| --- | --- | --- |
| `fixture.a` | admitted 5,347 steps | `576a5c7c3965ccbe0bcb269ab91a3ad50745880030fe0dfde1bfe0f024243377` |
| `source/component.c` | 1,206 / 39,298 | `d2d7759dbe9f91dd4f3580b35fe877bd24faacec8efa2654b0bfb9fab0a0e8de` |
| `source/component.h` | 77 / 4,656 | `7bcb62bea275f0b0d19fdfe32650e4c6a6de4a452d38bf6294bfba19681434f5` |
| `source/link.json` | existing copy-in/out contract flags | `51e7a2da2bf7b05b5b8ea0188f5ed19c779b51c45b80626283ccc6477449e016` |
| `raw/enum-list.c` | 668 / 22,415 | `51884e06bdaabca609eccce417494b0b18f75d1f82d2134853256dd6ad2d9e9e` |

Fixture source SHA256:
`071ba6775442efe22c29826484d31ede6124a0e00ad43e959b1cceb1ba6acf87`.
The ten code/test/build files are separately pinned by
`epoch7-source-files.sha256`, digest
`2420d6affa99f52a3913265c55e6d3737859ab9f358a7ca65e763876bc91e6e3`.

#### Exact Publication Files

Manifest: `/tmp/a-program-c-backend-epoch7/epoch7-files.sha256`.
The exact 14-file set is:

```text
doc/2026-10-03-C-BACKEND-EPOCH7-HANDOFF.md
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-ENUM-LIST-PLAN.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/nodes.c
src/prototype/c_backend/lower/oracle_test.c
src/prototype/c_backend/lower/check.sh
src/prototype/c_backend/lower/enum_list_check.sh
src/prototype/c_backend/lower/enum_list_client.c
src/prototype/c_backend/fixtures/enum_list.aplink
src/prototype/c_backend/fixtures/enum_list.p
src/prototype/c_backend/fixtures/enum_list_differential.p
```

Per-file additions/deletions are in `epoch7-deltas.tsv`. Only the +1/-1 predicate
and +8/-3 node changes alter lowering/ABI helper logic. Build adds four lines;
raw harness/gate and five fixture/client/gate files provide durable evidence.
Documentation records the actual boundaries, Core publication states and the
single current issue table. Outbox notices, temporary/generated evidence, removed
scratch files and all accepted/shared-owner code are excluded.

Final checks cover tabs on added source lines, English/ASCII comments/docs,
C-style fixture comments, shell syntax and `git diff --check`. Unchanged baseline
comment-star alignment is retained. Keep these 14 files fixed until Core release.

### Assessment

Agent decision: reuse the existing finite copy relation for selected enum payloads,
with runtime tag-domain validation before any transaction. This improves ordinary
C module inputs without replacing source execution, inferring nominal equality
or creating a new checker/IR. All producer/schema/admission authority is unchanged.

Core operational reporting/provenance correction: the human requested concrete
issue-linked feedback. The single table's fields, six-hour active cadence, freezes
and avoiding invented percentages are Core workflow decisions. A brief pointer/
hash/changed-row notice at this material milestone updates that feedback through
the existing outbox. No new reporting subsystem or shared-owner edits.

Core operational publication: shared Git metadata is read-only; Core performs
authorized task-branch commit/push from the exact manifest. Review all eleven
gates with its current common producer before separate Main integration. Worker
does not merge Main, promote accepted code, close issues or declare full Goal done.

### Plan

- [x] Implement/verify only selected enum List array conversion in the prototype.
- [x] Pass eleven O2 and eight native sanitizer gates, including raw inert checks.
- [x] Record exact files, code/test deltas, pins and the resolved raw-client warning.
- [x] Update one concise owning-Goal issue table with distinct publication states.
- [ ] Core publishes/reviews the frozen task epoch and independently verifies
  all eleven gates before prototype Main integration.
- Completion: this bounded array contract is verified; native Acc/QuickSort and
  remaining full-scope requirements stay open. The full Goal remains active.
