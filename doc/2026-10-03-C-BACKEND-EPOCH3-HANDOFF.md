# C Backend Epoch3 Handoff

Date: 2026-10-03
Status: verified and frozen for Core task-branch publication.
Branch: `parallel/c-backend-20261003`; upstream: `origin/parallel/c-backend-20261003`.
Base: `4987c08360b52b149538c2996a04df62e33166cd` plus exactly the files below.
Commit message: `prototype: lower numeric List partitions and finite copy-out`.
Related: [active Goal](2026-10-03-C-BACKEND-GOAL.md),
[published Epoch2](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md).

## Problem List

1. Hand off native numeric predicates, stable List partitions and finite copy-out
   without producer changes, accepted-source promotion or worker integration.

## 1. Numeric Predicates and List Boundary

### Subjective (User)

2026-10-03, English paraphrase of actual user scope/authorization: implement and
verify downstream C Backend/Linker refinement in the prototype lane without
target artifact fields. Task-branch publication is authorized; only Core merges
Main. This task does not authorize accepted-source promotion. The active Goal
preserves sources and superseding decisions.

2026-10-03, English paraphrase of the explicit user role decision relayed by Core:
Core specializes in design audit, combined verification and Main merges;
Job/Evidence implementation belongs to its sole worker. The target-only C epoch
continues under the existing publication and promotion boundaries.

### Objective (Code)

Fresh Git inspection: worker HEAD/upstream both equal the full base above. Core
reports Epoch2 was integrated prototype-only as `e5d4057`, pushed, after seven
current-owner/family-cursor plus Surface gates passed. That is coordinator
evidence, not a worker rerun. This epoch changes only the prototype lane and its
documents. No shared semantic interfaces, `.a` schema, producer authority,
accepted sources or frozen Epoch2 documents change. The worker has not written
shared Git metadata, bypassed the sandbox, pushed Main or merged.

Exact publication set, 21 files relative to the worker root:

1. `doc/2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md`
2. `doc/2026-10-03-C-BACKEND-GOAL.md`
3. `src/prototype/c_backend/README.md`
4. `src/prototype/c_backend/build.mk`
5. `src/prototype/c_backend/link/driver.c`
6. `src/prototype/c_backend/link/plan.c`
7. `src/prototype/c_backend/link/plan.h`
8. `src/prototype/c_backend/lower/check.sh`
9. `src/prototype/c_backend/lower/nodes.c`
10. `src/prototype/c_backend/lower/numeric.aplink`
11. `src/prototype/c_backend/lower/numeric_check.sh`
12. `src/prototype/c_backend/lower/numeric_client.c`
13. `src/prototype/c_backend/lower/numeric_differential.c`
14. `src/prototype/c_backend/lower/numeric_differential.p`
15. `src/prototype/c_backend/lower/numeric_fixture.p`
16. `src/prototype/c_backend/lower/oracle_test.c`
17. `src/prototype/c_backend/lower/representation.c`
18. `src/prototype/c_backend/lower/representation.h`
19. `src/prototype/c_backend/lower/scalar.c`
20. `src/prototype/c_backend/lower/scalar.h`
21. `src/prototype/c_backend/main.c`

Evidence root: `/tmp/a-program-c-backend-20261003.vjxTyc`.
`epoch3-files.sha256` records every exact file hash, including this document;
verify with `sha256sum -c` before publication. It stays external to avoid a
self-referential document hash. `epoch3-deltas.tsv` records per-file changes
against the full base, including new files.

Fresh verification uses the immutable `2d747cc` snapshot's accepted source
(relevant compiler revision `eb0aad6`) plus its solver/artifact overlay and these
backend changes. It never refers to dirty Main. This is task-baseline producer
evidence; Core's new current-owner combined verification remains separate.

- Eight O2 gates pass: `check-c-scalar`, `check-c-enum`, `check-c-data`,
  `check-c-list`, `check-c-numeric-list`, `check-c-link`, `check-c-backend`,
  `check-c-sorting-boundary`. Log: `epoch3-o2.log`. After final receipt formatting
  and 300-node copy-out coverage, focused numeric/List, Linker and data gates
  pass again: `epoch3-final-o2.log`, `epoch3-final-data-o2.log`.
- Five native scalar/enum/data/list/numeric-list gates pass under ASan/UBSan
  with leak detection: `epoch3-san.log`; the final numeric/List gate passes again
  in `epoch3-final-san.log`. Emitter, producer, raw fixture generator and generated
  source clients are instrumented. Native-tool object/archive/executable bodies
  use the driver's normal flags; their caller harnesses are instrumented.
- The numeric client checks 1,089 comparator pairs and 27,305 input/pivot cases
  with both stable partition outputs. It verifies ordered payloads, unchanged
  inputs, zero allocation for comparison, magnitude overflow, bounded recursion,
  real malloc/capacity failures, rollback and survival of earlier results.
- Source differentials match ten List observations and three Bool observations.
  The raw Oracle compares 17 Nat Match observations with the existing evaluator,
  checks 17 successor calls and overflow, and checks signed Int32 List copy-out.
  Emission forbids evaluator/substitution calls and preserves source graph/store
  counts. Raw descriptions test correspondence, not admission authority.
- Copy-out covers empty input, null buffer/output, exact/insufficient capacity,
  unchanged destination/length on failure, untouched unused elements, malformed
  deep tags/tails, cycles, reversed Int64 constructor/field order and 300 nodes.
  Copy-out succeeds beyond the recursive execution limit; source length over
  those same 300 nodes returns resource status 4 with unchanged output.
- Source/object/archive/executable publication, checked/trusted C determinism,
  repeat receipts, unchanged input `.a`, exhausted admission fuel and failed
  publication are covered. Nat-only receipts correctly omit node ownership.
  Invalid Nat shapes and unselected nested fields reject; existing tree/thunk,
  indexed/dependent, block-local thunk, callback and effect refusals remain.
- `git diff --check`, tabs, English/C comments and shell syntax checks pass.
  Manual source/docs edits used apply_patch. No remaining code/test gate failure.

Retained final fixture: `epoch3-final-numeric.a`, source Solve 3,512 steps,
load/reconstruction 3,530 steps. SHA-256:
`d3935e02642a1ce88dc1e6f7ca759fd89aaeb57df7070b44a4811e5d5a6f37f0`.
`epoch3-final-module/component.c`: 786 lines / 24,251 bytes, SHA-256
`649277892f4513ca9f6434451636026a79f7c0676c9c72aa624b0152d2fe31ed`.
Header: 63 lines / 3,320 bytes, SHA-256
`9d11fc8c745f039e0e22bf241e6f99317a563e7f29b6ddcb30fc3c35bdb195ca`.
These are measurements for this contract, not a performance comparison.

### Assessment

Core operational steering released Epoch2 and prioritized actual native numeric
partitioning/List boundary work plus removal of receipt representation repetition.
Exact freeze/handoff, current-owner combined checks and routing through Core to
the job-evidence worker are coordination workflow, not new direct user decisions.
The latter worker's reported baseline is `5035c7a`, producer `64df10d`. This epoch
needs no producer/admission/frontier or shared interface change. Shared Git stays
read-only, so Core performs the authorized task-branch commit/push. Publication
and later Main integration review remain distinct.

Agent implementation choices: explicit `nat32` selects the exact closed
zero/single-successor declaration shape and maps it to uint32 magnitude, checked
successor and conditional predecessor extraction. Constructor names are not
recognition keys. The existing recursive comparator lowers through saturated
calls with extra first-order arguments and existing static IH handling; no named
comparator replacement or host primitive is added. Nested recursive closure
captures remain refused. Selected natural payloads act as finite scalar fields
inside the existing node representation.

For the exact two-constructor scalar-payload/single-tail List shape, generate
`ap_copy_ALIAS` from declaration positions, validate/count before writing, then
copy in source order. Source inputs remain persistent; output buffer extent,
non-overlap, length and statuses are documented in the backend guide/header.
Magnitude overflow (5), depth (4) and capacity (6) are target resource contracts,
not source evidence. Receipts consume copied flags from the successful emitter
after its temporary representations are destroyed, eliminating reconstruction.

Implementation files: plan/header/main add Nat selection and existing root
resolution; representation/header add structural Nat mapping and List shape
recognition; scalar/header add recursive arity, native expressions and copied
contract metadata; nodes emit transactional finite copy-out; driver consumes
metadata once. Test files/build add source/native/raw correspondence and resource
gates. README/Goal/this handoff document scope, provenance, evidence and limits.

Native slice-input sorting, Acc recurrence/erasure, QuickSort, dynamic callbacks,
effects, indexed/dependent data, trees, unsupported recursive closure captures,
higher Identity and general cross-module nominal exchange remain incomplete.
Structural Acc QuickSort still passes separately; it is not native sorting
evidence. #44/#49 and the full Goal remain open. Resolved literal/authentication
notes are not reopened; historical manifests are unchanged.

### Plan

- [x] Implement native numeric partitioning and finite List copy-out downstream.
- [x] Verify ordinary C clients, source/raw correspondence, determinism, immutable
  inputs, resource/refusal controls, receipts and O2/sanitizers.
- [x] Review exact file set, tabs/English comments and publication provenance.
- [x] Freeze these 21 files; do not edit them until Core releases the freeze.
- [ ] Core commits/pushes this task branch using the chosen message and manifest.
- [ ] Core runs current-owner combined verification and separate Main review.
- [ ] After release, continue remaining Acc/QuickSort and other owned obligations;
  route producer/admission/frontier needs through Core to job-evidence.
