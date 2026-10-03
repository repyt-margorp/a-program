# C Backend Epoch13 Handoff

Date: 2026-10-03
Status: bounded shared C product verified; exact snapshot ready for Root.
Branch: `parallel/c-backend-20261003`.
Parent: `97f8d7c28f3438fb1b36d687e0e7efc175d8c6f3`.
Chosen message: `prototype: publish bounded shared C libraries`.

## Problem List

1. Produce an ordinary shared C module from existing admitted exports and target
   ABIs, with explicit public visibility and preserved publication failures.

## 1. Shared C Libraries

### Subjective (User)

2026-10-03, English paraphrase of the human scope recorded in the owning
[Goal](2026-10-03-C-BACKEND-GOAL.md): `.a`/LinkerScript should produce readily
usable ordinary C modules through bounded downstream work. The backend owns no
source semantics or erasure policy. Independent lanes may continue without
waiting for Merge; Merge resolves conflicts and owns Main. Preserve submitted
evidence and shared Git boundaries; coordinate exclusive measurements. The
human-authorized audit disposition supersedes historical #44/#49 with open #61,
under the same C owner. Native Acc/QuickSort remains unfinished.

### Objective (Code)

The exact C12 parent supports source/object/archive/executable products. This
epoch adds only the explicit `shared` product to the prototype plan/driver:

- Reject shared entry/native-layout-script combinations using existing rules.
- Compile component/structural runtime with `-fPIC`; link `library.so` through
  the selected host C compiler with `-shared` and an anonymous ELF version map.
- Select `ap_export_ALIAS` and existing native arena/List-helper names from the
  target ABI/plan. `local: *` hides other definitions. Unmatched helper names add
  no definitions; no representation reconstruction or producer field is added.
- Publish objects, `library.so`, `symbols.map`, sources/header and receipt only
  after all steps succeed. Map stream/close/tool failures are I/O status 2 and
  remove staging files. The receipt identifies visibility/toolchain explicitly.

Fresh serial verification on qualified C9/E8, all terminal exit 0:

- Strict backend-only O2 build; new `shared/check.sh` O2 and client
  ASan/UBSan/leak runs. Each shared phase records 58 expected/actual status rows:
  45 success, eleven status 2, two checked/trusted callback refusals status 4.
- Four ordinary linked and four dlopen clients, plus eight repeated/trusted
  dlopen calls per phase. Scalar Int32/Int64 wrapping/identity/null-output,
  finite record Lists with nested active tags and Int64 extrema, applied Nat
  arrays including `UINT32_MAX`, and isolated repeated effects are positive.
  Record clients use lengths 0 through 9 and retain capacity, cycle, depth,
  allocation-capacity rollback, unchanged outputs/buffers and prior arenas.
- `nm -D --defined-only` matches exactly 3 scalar, 4 isolated, 17 record-List and
  8 applied Nat-List public functions. Private/runtime definitions stay local;
  dlopen clients also refuse hidden runtime/producer symbol lookup. Calls and
  arena destruction finish before dlclose.
- Actual isolated stdout is `42B4242B42B42`; a `/dev/full` failure returns 2,
  then later calls recover. Repeated checked C/header/maps are identical;
  native trusted C and all trusted public headers/maps/behavior match. O2 and
  sanitizer phases produce byte-identical C/header/object/library/map inputs.
- Map immediate/close stream failures, compiler failure and a link-only failure
  leave no product or staging path. Invalid entry/layout/product profiles return
  2; native callback remains 4 in checked/trusted modes. Existing products and
  all four images remain unchanged.
- Affected existing LinkerScript gate passes for the original four products,
  native layout, budgets, invalid sources, tool errors and ABI checks. All
  nineteen C11 I/O/unsupported/atomic/prior-output controls pass on this backend.
  Registered `check-c-shared` dry-run calls the new gate using the qualified
  pointer binary and needs no producer rebuild. No broad suite or timing run.

Two initial harness failures remain in the retained directory: an incorrect
checked/trusted structural C byte-equality assumption, and an incorrect Nat32
struct declaration. Inspection showed the admitted structural graph can differ
and the public Nat32 type is `uint32_t`. Correct public signatures and actual
trusted library execution resolve these without emitter/producer changes or
weaker supported/refusal outcomes. There is no unresolved gate failure.

Qualified producer: `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`.
Runtime128 manifest: `9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
Candidate O2 backend: `84fe14edcc1ae87f24863341d486ca477357117bd8cad3142851aff814fba81a`.
Qualified pointer: `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`.
All current runtime128 source hashes match that qualified snapshot. Worker
current E9+E10 qualification is not claimed; Root's composition review is separate.

Retained evidence: `/tmp/a-program-c-backend-epoch13-shared-20261003`.

| Evidence | SHA256 |
| --- | --- |
| `verification.json` | `757e27e3fa051d7ef82b6d3317bea6e4cf7ff8ac3d7eab4386e7796b016ad5a7` |
| `qualified-inputs.sha256` (165 files) | `2c3b77eaa2f7912b32553a8aec2d832ad848bc2dd10f0efc989029f4b3f02301` |
| Scalar image | `960ebc1264f943a0eb938befbbac960b771fc584454242c65609203898d5065b` |
| Isolated image | `f172f08f36b52e1cc6e1c7684585ffb3c2017613f8e73ff9617c4c3c9f6f8f94` |
| Record-List image | `3e207ae8fdc6d08c62ea1a0e887b1e8c3f83e2270aa28a81beda7b90a59aa6b3` |
| Applied Nat-List image | `9a40589f1a5c2e1f7838dca83739018621fb24eee48d600688d5fe5df905397b` |

Verification pins source inputs, runtime128, tools/binaries, per-command statuses,
exact dynamic symbols, generated products and client binaries. Detailed command
files/runner stdout/stderr remain with final and failed historical runs. The
separate run manifest pins all retained outputs. `epoch13-files.sha256`,
`epoch13-deltas.tsv` and `epoch13-freeze.json` pin the exact eleven-file snapshot;
the immutable submitted tar preserves it while live owned work later advances.

### Assessment

Product spelling, visibility map, helper naming and tests are agent decisions
within the existing downstream C-module authorization. The
[GNU ld visibility-map documentation](https://sourceware.org/binutils/docs/ld/VERSION.html)
and [GCC shared/PIC options](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html)
describe the chosen host toolchain mechanism. It requires an ELF version-script
compatible host linker. No install/SONAME/version policy or portable loader is
introduced. Libraries retain the existing C ABI/storage and borrowing contracts;
the code handle must outlive every call and arena destructor using it.

Client sanitizer mode instruments clients at O1 only. Producer, backend and
generated object/shared-library bodies remain O2, as separately pinned. Capacity
failure exercises transactional allocation rollback; this gate does not interpose
every shared-body malloc position or claim full shared-body instrumentation.
Existing recursive execution/Nat32/storage limits remain target-local.

Callback/boxed APIs, higher Identity/effects, indexed/callable Acc, recursive
aggregates/captures and general shared nominal exchange remain open or explicitly
unsupported. Native Acc/QuickSort is unfinished. No producer/schema/checker/source
erasure change, accepted C promotion, issue closure or full AP5/AP6 completion.
No cross-owner implementation blocker is established.

Exact eleven-file snapshot:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-SHARED-LIBRARY-PLAN.md
doc/2026-10-03-C-BACKEND-EPOCH13-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/link/plan.h
src/prototype/c_backend/link/plan.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/shared/client.c
src/prototype/c_backend/shared/fault.c
src/prototype/c_backend/shared/check.sh
```

### Plan

- [x] Implement the bounded existing-ABI shared product and visibility.
- [x] Verify linked/dlopen clients, exact exports, lifetime/resource boundaries.
- [x] Preserve original product/refusal/I/O/atomic controls and pin actual results.
- [x] Check tabs, English comments/docs, shell syntax and source/evidence hashes.
- [x] Update one owning issue table and preserve historical submitted snapshots.
- [x] Pin the separate eleven-file handoff for Root's exact task publication.
- [ ] Root publishes/reviews this epoch against the current producer before Main.
- [ ] Continue the active bounded #61 Goal; native Acc/QuickSort remains open.
