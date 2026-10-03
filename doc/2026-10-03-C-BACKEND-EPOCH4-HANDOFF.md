# C Backend Epoch4 Handoff

Date: 2026-10-03
Status: verified and frozen for Core task-branch publication.
Branch: `parallel/c-backend-20261003`; upstream: `origin/parallel/c-backend-20261003`.
Base: `720f92a7103229996a8013847bdf1eca2246c9e9` plus exactly the files below.
Commit message: `prototype: add transactional C array-to-List boundary`.
Related: [active Goal](2026-10-03-C-BACKEND-GOAL.md),
[published Epoch3](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md).

## Problem List

1. Make existing native List APIs easier to call from ordinary C arrays while
   retaining a concrete native Acc/QuickSort refusal and downstream authority.

## 1. Finite Array Input and Native Boundary

### Subjective (User)

2026-10-03, English paraphrase of the latest direct user clarification: emit C
readily usable from other C modules through `.a` and LinkerScript. Keep the
backend subordinate to A Program, avoid excessive deep or scope-expanding
investigation, and concentrate performance/Job/Evidence verification jointly.

2026-10-03, English paraphrase of actual user authorization: workers may commit
and push their own task branches; only Core merges results or updates Main.
No worker self-merge, accepted promotion or issue closure. Deliver verified,
reviewable epochs with evidence within the prototype scope. Sources and earlier
requirements are retained in the active Goal.

### Objective (Code)

Fresh HEAD/upstream inspection confirms the full base above. Core reports eight
Epoch3 combined gates and code audit passed; fresh Main history contains prototype
merge `0fc0c0b`. That integration is separate from this new worker epoch.
Historical handoffs/manifests remain unchanged. No accepted implementation,
artifact schema, source producer, checking authority or shared interface changes.

Exact publication set, 11 files relative to the worker root:

1. `doc/2026-10-03-C-BACKEND-EPOCH4-HANDOFF.md`
2. `doc/2026-10-03-C-BACKEND-GOAL.md`
3. `src/prototype/c_backend/README.md`
4. `src/prototype/c_backend/link/driver.c`
5. `src/prototype/c_backend/lower/nodes.c`
6. `src/prototype/c_backend/lower/array_client.c`
7. `src/prototype/c_backend/lower/numeric_check.sh`
8. `src/prototype/c_backend/lower/oracle_test.c`
9. `src/prototype/c_backend/sorting_check.sh`
10. `src/prototype/c_backend/fixtures/native_quicksort.p`
11. `src/prototype/c_backend/fixtures/native_quicksort.aplink`

Evidence root: `/tmp/a-program-c-backend-epoch4-current`.
`epoch4-files.sha256` records these exact hashes, including this handoff; verify
with `sha256sum -c` before publication. It is external to avoid a self-referential
document hash. `epoch4-deltas.tsv` records all per-file additions/deletions.
Implementation: `nodes.c` +24/-0, `driver.c` +3/-1 (net +26 lines).
Tests: `array_client.c` +115/-0, `numeric_check.sh` +3/-0,
`oracle_test.c` +3/-0, `sorting_check.sh` +18/-1, both fixtures +11/-0 each
(net +160 lines). Documentation is counted separately in the delta file.

Fresh builds use a detached, dereferenced copy of Core's combined family/Surface
overlay, not symlinks into Main. Snapshot: `core-snapshot`; manifest digest
`c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`.
Build log: `final-build.log`, C11/O2/Wall/Wextra/Werror, at most `-j2`.
Backend binary SHA256:
`ff32d3b6f7280d9dc7a87cd729b377985ec060921146479d8cce8361e58d4be1`.
Raw Oracle generator SHA256:
`914b45b0dd5b1570ae49c311c7e87eb94cb27a5b21cd33bd95b1331a625f7ad1`.
Producer: `/tmp/a-program-core-c3-combined/build/pointer-check`, SHA256
`4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27`.

Fresh verification, all exit 0:

| Gate | Log under evidence root |
| --- | --- |
| Numeric/List O2 | `final-numeric-o2.log` |
| Scalar plus raw Oracle O2 | `final-scalar-o2.log` |
| Recursive List O2 | `final-list-o2.log` |
| Linker products/negative controls O2 | `final-link-o2.log` |
| Structural sorting/native refusal O2 | `final-sorting-o2.log` |
| Numeric/List ASan+UBSan+leak detection | `final-numeric-sanitizer.log` |
| Scalar/raw native ASan+UBSan+leak detection | `final-scalar-sanitizer.log` |

Commands use the corresponding lane scripts with `core-build/a-to-c`, the pinned
producer and, for the scalar gate, `core-build/c_scalar_test`. Sanitized target
client flags are `-std=c11 -Wall -Wextra -Werror -O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer -no-pie`, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Generated source bodies and raw native programs
are instrumented; object/archive bodies use the driver's ordinary O2 flags with
instrumented clients. Producer/emitter/Oracle-generator binaries are O2.
This is focused worker verification, not a full compiler acceptance or performance
comparison. Core's later combined integration gate remains separate.

The new ordinary C client checks 511 slices and 5,110 stable partition calls
for each source/object/archive product. It verifies input-array independence,
empty input, Nat32 and reversed-field Int64 values, exact arena capacity, all
four allocation failure positions, rollback and survival of earlier results.
It converts/copies 300 nodes at depth limit 1 while a native recursive length
call explicitly fails 4. The raw Oracle adds seven Int32 array-to-List sums
matching source evaluation, retaining inert emission/graph/store checks.
Existing numeric source differentials, 1,089 comparisons, 27,305 partition cases,
determinism, checked/trusted equality, product and refusal controls still pass.

Retained evidence (`evidence/`):

| Artifact/output | SHA256 |
| --- | --- |
| `fixture.a` (numeric; source Solve 3512, link reconstruction 3530) | `0ef9a51166186353c116b168e5d1b92c46896cdd3d9031f6e5a50d4d0a10251a` |
| `native.a` (open QuickSort; source Solve 52935) | `1a24905ef6d490430a0b56139c3f7256171e361ca0e90348ad96c7255f501c4b` |
| `component/component.c` (840 lines, 26687 bytes) | `1a2cb79232d311998011886b97b8a4253555a3515dfccba023a6efd56315c6c4` |
| `component/component.h` (73 lines, 4141 bytes) | `29fbe8ac2c7554c3e9c330e63a0524012d41b0e218ada85b08c180863fe5b515` |

Checked native QuickSort reconstructs in 52650 steps, then returns 4 with
`representations require unique closed declarations`. Checked/trusted refusal
gates preserve the image and publish no product. The selected `List Nat` is a
generic applied family, outside the closed native selection contract. Indexed
SizedList and callable recursive Acc fields remain unsupported. Earlier artifact
read failure was a task-baseline/current-producer version mismatch: the copied
combined overlay reads the earlier diagnostic artifact and existing typed views
successfully (`acc-inspection.log`). It is not a demonstrated producer defect.
No producer change is requested. Structural QuickSort still yields FFTT separately.

`git diff --check`, shell syntax, leading-tab and English/ASCII comment checks
pass. No ML-style fixture comments or manual generated-file edits. Source/docs
were edited with apply_patch. No shared Git write, sandbox bypass or Main action.

### Assessment

Core's operational reprioritization selects usable C ABI/products and bounded
native lowering. Native priority, exact freeze and combined publication/review
are coordinator workflow, not new direct user requirements. Shared Git metadata
remains read-only; Core can perform the authorized task-branch publication.
Publication, later integration review and accepted-source promotion are distinct.

Agent implementation decision: extend the existing exact two-constructor,
scalar-payload/single-tail contract with `ap_from_ALIAS(arena, array, count, out)`.
Build nodes backwards so their observable order matches the slice. Copy input
elements and allocate the terminal too (`count + 1` nodes). Roll back only new
allocations on failure and preserve output/prior results. Existing declaration
positions determine tags/fields; no named source transformation or sorter exists.
Receipt metadata reuses the emitter-selected copy contract without reconstructing
representations. This is explicit C boundary conversion, not internal slice layout.

Limits: null required pointers return 1, allocation/live-capacity failure 3,
an active arena 4, SIZE_MAX count overflow 6. Caller pointers/extents must be
valid and input/output/arena metadata separate. Conversion helpers are iterative;
source calls retain target depth max/default 256 and Nat32 overflow 5. A successfully
converted large List can still fail a subsequent native source call's resource
bound. No target width/depth/capacity convention supplies source evidence.

Native Acc recurrence/erasure, QuickSort, indexed/dependent data, general trees,
unsupported recursive closure captures, dynamic callbacks/effects, higher Identity
and cross-module nominal exchange remain explicit. Do not claim AP6 completion
or close #44/#49. Further work follows the latest bounded scope, with any genuine
producer/readback/checking dependency routed through Core to job-evidence first.

### Plan

- [x] Implement transactional finite array input and document the C contract.
- [x] Verify source/object/archive clients, raw/source correspondence, resources,
  retained refusals, determinism, immutable images and O2/sanitizers.
- [x] Review exact files, tabs/English comments, source deltas and provenance.
- [x] Freeze these 11 files; no further edits until Core releases the epoch.
- [ ] Core commits/pushes this task branch using the chosen message/manifest.
- [ ] Core reviews combined integration separately; no accepted promotion.
- [ ] After release, coordinate the next bounded downstream increment.
