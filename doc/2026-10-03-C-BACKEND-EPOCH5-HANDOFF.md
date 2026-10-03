# C Backend Epoch5 Handoff

Date: 2026-10-03
Status: verified and frozen for delegated task-branch publication.
Branch: `parallel/c-backend-20261003`
Base: `a3b6bce5f2ad1dee80c56e9a4843ec646ba30fc5`
Chosen commit message: `prototype: lower known local functions to native C`
Related: [Goal](2026-10-03-C-BACKEND-GOAL.md),
[bounded follow-up](2026-10-03-C-BACKEND-STATIC-FUNCTION-PLAN.md).

## Problem List

1. Publish the verified bounded static-function/C-module usability epoch while
   preserving downstream authority and explicit unsupported contracts.

## 1. Native Local Functions

### Subjective (User)

2026-10-03, English paraphrase of direct user decisions recorded in the owning
Goal: emit C readily usable from other C modules through `.a` and LinkerScript;
keep target work bounded and subordinate to A Program. Do not extend `.a` for
target convenience. Each worker may publish its task branch; only Core integrates
Main, with no worker promotion or issue closure. Keep this Goal active. Core waits
for notification OR a six-hour timer, checking even without notice. Relay/payload,
freeze and implementation choices below are operational/agent decisions.

### Objective (Code)

At the base plus these exact lane edits, `lower/scalar.c` changes by +7/-4 lines:
retain syntactic Thunk/Lambda values alongside the existing direct IH thunks.
Existing force/direct-call/capture handling lowers their uses. There is no public
closure ABI, source evaluation during emission, producer field, private checker
or new shared IR. The prototype build adds the static-function/raw Oracle gate.
Temporary scratch copies/build overrides/legacy harness were never published
and are removed. The original Epoch4 handoff is unchanged.

Fresh O2 verification passes all nine C gates: backend plus structural Oracle,
Linker, scalar/raw Oracle, enum, data, List, numeric/List, sorting boundary and
static functions. Five native generated-client ASan/UBSan/leak gates pass:
static, scalar, data, List and numeric/List. No active gate failure.

The static gate verifies 260 Int32 and five Int64 cases per source/object/archive
product, nine null-output controls, eight source/C observations, checked/trusted
C equality, deterministic products, exhausted checking fuel without publication
and unchanged image hashes. Raw emission forbids evaluator/substitution calls,
preserves graph/store counts and compares 20 generated C answers with the existing
evaluator. These raw descriptions are correspondence tests, not admission evidence;
the surface fixtures are admitted separately (4,127 and 2,205 steps).

Native static clients verify borrowed List captures, lexical shadowing, finite
copy-out/copy-in, unused successor at UINT32_MAX, checked overflow, depth refusal
and allocation rollback. Two modules with distinct aliases coexist in one C
client: explicit array conversion exchanges List contents, failed allocation
preserves the other module's earlier results and either generated destructor
releases their shared arena. This does not establish shared nominal identity.

The exact former `block_callback` refusals are retained as positive coverage in
the existing data/List/numeric products: 130 Packet constructor/Bool observations,
511 List observations and two numeric observations per source/object/archive
product, plus invalid-input/null/depth controls. Dynamic callbacks, demanded
effects, recursive function fields, tree/indexed/dependent shapes and unselected
recursive representations retain explicit negative gates.

Source limitations remain: surface integer literals synthesize Int32 and reject
larger integers; `::` is an assertion. Int64 local-function tests use existing
Int64 parameters, with no synthesis change. The specific three-closure captured
chain `h -> g -> f` admits but refuses native lowering with status 4/no product;
the two-closure counterpart succeeds. This is not a general numeric depth bound.
An unused known function's effectful body is not run; demanded effects still
refuse. Native Acc/QuickSort remains status-4 refusal for its unsupported applied
family/representation; structural FFTT execution is separate evidence.

Target resource limits are unchanged: Nat32 successor overflow returns 5;
recursive Match defaults/maxes at 256 active calls and depth failure returns 4;
allocation/live-capacity failure returns 3. Public failures preserve output and
roll back only new nodes. Finite array/List conversion is iterative. Caller
inputs/outputs/arena must obey the documented storage/lifetime contract; destroying
a shared arena retires every module's result stored there. These are C target
contracts, not source totality or typing rules.

#### Evidence and Pins

Evidence root: `/tmp/a-program-c-backend-epoch5`; logs `*-o2.log` and `*-san.log`.
Build log: `/tmp/a-program-c-backend-epoch5-build.log`. Backend/producer/raw
generators are O2. Sanitizers instrument generated source bodies and every client;
driver-produced object/archive bodies remain O2. No comparative performance claim.

Build source is the dereferenced immutable Epoch4 Core family/Surface snapshot:
`/tmp/a-program-c-backend-epoch4-current/core-snapshot`. Its 1,025-file manifest
`core-snapshot.sha256` has digest
`c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`.
This is the pinned worker baseline, not the latest Main producer. Core must run
its current-owner combined gate separately before integration.

| Tool | Path | SHA256 |
| --- | --- | --- |
| Backend | `/tmp/a-program-c-backend-epoch5/build/a-to-c` | `c5121ca65495789db0542bf860c55719d4312d84cb09423f34a7f031d2e2ec1b` |
| Producer | `/tmp/a-program-core-c3-combined/build/pointer-check` | `4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27` |
| Scalar Oracle | `/tmp/a-program-c-backend-epoch5/build/c_scalar_test` | `536f8aa4eecea91ec92dcbdbb41a4d7eb5ebdd63e14b3189e37a502a88ff2d3c` |
| Static Oracle | `/tmp/a-program-c-backend-epoch5/build/c_static_oracle_test` | `18a2729b1412aac0b0257bf21c91935d0723460445720af5917c25f840fd5e8f` |
| Structural Oracle | `/tmp/a-program-core-c3-combined/build/c_oracle_test` | `d4d5b2d8506fbbe8ff8fa7635d1e350879341e03da4c98a07f386fd409024052` |

Retained examples under `evidence/`:

| File | Lines/bytes where applicable | SHA256 |
| --- | --- | --- |
| `static-functions.a` | admitted 4,127 steps | `b43001df9ea9fb5c3fbfe6cf74b121fa33306b63c22c154d90e3346db15228a0` |
| `static-native.a` | admitted 2,205 steps | `3f524c58979b3ac56f5df63e6aba5654e314df74abc70a803e1e6e9ec6e5f402` |
| `scalar/component.c` | 342 / 6,741 | `feea6e4cd2c8d4d058e46581685ea12707350df78198a572d0aa0c98dab2dacd` |
| `scalar/component.h` | 23 / 836 | `cfa624f5deba5ce3bbb83467d6a19150eab6ba33db4439d1c7a9886559dcd7c8` |
| `native/component.c` | 516 / 16,147 | `e81a6a0acb0ea74b3671c48908dd7b76b0a233c76abaee9a80fabb0d59343ced` |
| `native/component.h` | 51 / 2,687 | `40974e39b1d5cb89232ebd21ff1fabdd5f77b5916415731f4056239641641a08` |

#### Exact Publication Files

The manifest is `/tmp/a-program-c-backend-epoch5/epoch5-files.sha256`. Its exact
25-file set includes four documentation files, two implementation/build files
and 19 test/fixture files:

```text
doc/2026-10-03-C-BACKEND-EPOCH5-HANDOFF.md
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-STATIC-FUNCTION-PLAN.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/lower/data_check.sh
src/prototype/c_backend/lower/data_client.c
src/prototype/c_backend/lower/list.aplink
src/prototype/c_backend/lower/list_check.sh
src/prototype/c_backend/lower/list_client.c
src/prototype/c_backend/lower/numeric.aplink
src/prototype/c_backend/lower/numeric_check.sh
src/prototype/c_backend/lower/numeric_client.c
src/prototype/c_backend/lower/static_function_check.sh
src/prototype/c_backend/lower/static_function_client.c
src/prototype/c_backend/lower/static_native_check.sh
src/prototype/c_backend/lower/static_native_client.c
src/prototype/c_backend/lower/static_native_modules.c
src/prototype/c_backend/lower/static_oracle_test.c
src/prototype/c_backend/fixtures/static_functions.aplink
src/prototype/c_backend/fixtures/static_functions.p
src/prototype/c_backend/fixtures/static_functions_differential.p
src/prototype/c_backend/fixtures/static_native.aplink
src/prototype/c_backend/fixtures/static_native.p
```

Per-file additions/deletions are retained in `epoch5-deltas.tsv`. Only the +7/-4
scalar change alters lowering logic; build wiring adds eight lines. Outbox notices
and temporary/generated evidence are excluded. No accepted-source or shared-owner
file is part of this publication set.

Fresh final checks pass tabs, English/ASCII comments/docs, C-style fixture comments,
shell syntax and `git diff --check`. The pinned 1,025-file snapshot manifest
verifies unchanged. Keep these 25 files fixed until Core releases this epoch.

### Assessment

Agent decision: adopt the minimal verified static-function case through existing
native calls; preserve unsupported capture shapes instead of a generalized closure
compiler. This advances C usability/AP6.3 without claiming full AP6/#44/#49.
Core operational release permitted these separate edits after publishing Epoch4.
Core reports Epoch4's eight combined gates and prototype Main merge passed;
that report does not verify Epoch5. No producer dependency is requested here.

Core operational publication/routing: shared Git metadata remains read-only, so
Core performs the authorized task-branch commit/push for the exact manifest and
reviews current-owner integration separately. Freeze that set before sending
one newline-terminated, at-most-8-KiB ready notice through this worker's outbox.
The pointer/hash relay does not merge or create a Core session. These are workflow
decisions, not additional user design principles.

### Plan

- [x] Implement the bounded local-function case in the active prototype.
- [x] Verify nine O2 gates, five native client sanitizer gates and retained refusals.
- [x] Complete style/hash checks, freeze the exact set and post the ready notice.
- [ ] Core publishes this task-branch epoch; current-owner combined review and
  Main integration remain separate. Keep the full Goal active.
