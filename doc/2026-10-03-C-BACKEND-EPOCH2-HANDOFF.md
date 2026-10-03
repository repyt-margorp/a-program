# C Backend Epoch2 Handoff

Date: 2026-10-03
Status: verified and frozen for Core task-branch publication.
Branch: `parallel/c-backend-20261003`; upstream: `origin/parallel/c-backend-20261003`.
Base: `bb6998319a0236a27a5c7ef01a32a71edb31d76b` plus exactly the files below.
Commit message: `prototype: lower single-tail recursive data to native C`.
Related: [active Goal](2026-10-03-C-BACKEND-GOAL.md),
[published Epoch1](2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md).

## Problem List

1. Hand off verified native single-tail container lowering without producer
   changes, accepted-source promotion or worker integration.

## 1. Native Recursive Containers

### Subjective (User)

2026-10-03, English paraphrase of actual user task scope/authorization: implement
and verify downstream C Backend/Linker refinement in the prototype lane without
target artifact fields. Own-branch publication is authorized; only Core merges
Main. This task does not authorize accepted producer promotion. Sources and
superseding decisions are preserved in the active Goal.

### Objective (Code)

Worker HEAD/upstream are `bb69983`. Core reports Epoch1 was integrated into Main
via `9061be3`, pushed, and passed six current-Core gates; that is coordinator
evidence, not a worker rerun. This follow-up changes only the prototype lane and
its documents. The worker performed no shared Git metadata write, sandbox bypass,
Main push/merge or accepted-source edit.

Exact publication set, 19 files relative to the worker root:

1. `doc/2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md`
2. `doc/2026-10-03-C-BACKEND-GOAL.md`
3. `src/prototype/c_backend/README.md`
4. `src/prototype/c_backend/build.mk`
5. `src/prototype/c_backend/link/driver.c`
6. `src/prototype/c_backend/lower/check.sh`
7. `src/prototype/c_backend/lower/data_check.sh`
8. `src/prototype/c_backend/lower/list.aplink`
9. `src/prototype/c_backend/lower/list_check.sh`
10. `src/prototype/c_backend/lower/list_client.c`
11. `src/prototype/c_backend/lower/list_differential.c`
12. `src/prototype/c_backend/lower/list_differential.p`
13. `src/prototype/c_backend/lower/list_fixture.p`
14. `src/prototype/c_backend/lower/nodes.c`
15. `src/prototype/c_backend/lower/nodes.h`
16. `src/prototype/c_backend/lower/oracle_test.c`
17. `src/prototype/c_backend/lower/representation.c`
18. `src/prototype/c_backend/lower/representation.h`
19. `src/prototype/c_backend/lower/scalar.c`

Private evidence root: `/tmp/a-program-c-backend-20261003.vjxTyc`.
`epoch2-files.sha256` is the final SHA-256 manifest for all 19 files, including
this document. Check it with `sha256sum -c` before publication; it is external to
the document to avoid a self-referential hash. `epoch2-deltas.tsv` records exact
per-file additions/deletions against `bb69983`, including new files.

Fresh verification uses the immutable `2d747cc` snapshot's accepted source
(relevant compiler revision `eb0aad6`) plus its solver/artifact overlay, and the
current worker backend sources. No overlay symlink points to dirty Main.
This is task-baseline producer evidence; Core's new current-owner plus Surface
combination remains its separate verification before any Main merge.

- Seven O2 gates pass: `check-c-list`, `check-c-scalar`, `check-c-enum`,
  `check-c-data`, `check-c-link`, `check-c-backend`, `check-c-sorting-boundary`.
  Final log: `epoch2-final-o2.log`.
- Native scalar/enum/data/list gates pass under ASan/UBSan with leak detection.
  Emitter, producer, raw fixture generator and generated source clients are
  instrumented. Native-tool object/archive/executable products use the driver's
  normal C flags; their caller harnesses are instrumented. Logs:
  `epoch2-sanitizers.log`, plus `epoch2-final-list-san.log` after the final
  conditional-allocation coverage addition.
- The list client checks 511 length/flag combinations; scalar wrapping, ordered
  append/selection fields, composition, borrowing, arena lifetime and destruction.
  Actual malloc failure after one successful allocation, capacity failure,
  recursive depth exhaustion, prior-result survival and rollback preserve output.
  Null nodes/tails, invalid deep tags/enums and self/multiple-node cycles reject.
  A tight capacity gate confirms an unused selection branch does not allocate.
- Five source lists produce 35 scalar observations matching native C. The raw
  recursive fixture compares 14 sum/append observations with the existing
  evaluator, wraps evaluation/substitution entry points during emission and checks
  unchanged term/object/occurrence/proof counts. Raw fixtures are correspondence
  tests, not admission evidence; source admission runs separately.
- Checked/trusted repeated emission, all native products and unchanged input
  image hashes pass. O2 and sanitizer emitters produce identical final C/header:
  `epoch2-emission-compatibility.log`.
- `git diff --check`, shell syntax, tabs, C-style fixture comments and manual
  English-comment review pass. No generated source was edited by hand.

Frozen read-only image `epoch2-final-list.a`:
`a93c6f8c07a6555a01350959198715309568a0a6bba65dc4c2d128dd206eec68`.
It checks in 3,744 source Solve transitions; loaded reconstruction takes 3,767.
`epoch2-final-module/component.c` is 670 lines/21,640 bytes, SHA-256
`e5a83ee4f7c54cd7943a0be8da3787402b220c1dec6834d7279b8d4234d46c78`;
`component.h` is 45 lines/2,258 bytes, SHA-256
`4db428bbb04c039c130044d32ce2a67c2ffdb41ca6087b6da6157a6b007ced51`.

Preparatory failures were corrected before freeze: an incomplete static-capture
cache key made repeated emission vary; the key now compares closure identity as
well as binder/type. A raw declaration fixture omitted retained prefix images,
and the sanitizer build diagnosed a fixture's initialization order; both fixtures
now express valid inputs. Initial source branch parentheses and Self-prefix
inspection were corrected during implementation. Final gates have no unresolved
failure. Older queued Int64/auth notes are not new work in this epoch.

### Assessment

Agent implementation decisions within owned scope: selected closed unindexed
single-tail data uses immutable C node pointers; construction uses a zero-initialized
caller-owned arena. Public validation walks finite chains before computation.
Failed calls roll back only their own allocations and preserve output; prior
successful results stay live. Append copies its left chain and borrows its right
input; identity can return its borrowed input. Caller nodes must remain readable,
and result storage is separate from input nodes/arena metadata.

Existing erased recursive Match templates become direct C recursion. Existing
admitted induction classifiers determine represented inner results in composition;
known IH thunks carry lexical fields/recursive targets/captures and force only at
their source use. No evaluator, producer field or independent typing authority is
introduced. The depth bound defaults to and never exceeds 256; return 4 denotes
target resource exhaustion, not a source termination result. Allocation failure is
3, invalid input 2, null arena/output 1, success 0.

Retained limits: general trees/multiple Self fields, recursive thunk/function
fields, indexed/dependent/nested aggregates, block-local function thunks, callbacks
and effects reject in native mode. Single-tail recursion moves from the former
blanket negative to positive list coverage; unselected recursive exports still
reject. Stable selection by a stored Bool flag is implemented, not numeric-predicate
partitioning. Slice copyout, Acc/native QuickSort, demanded Identity, unsupported
nested recursive closure captures and cross-module nominal contracts remain open.
No machine-checked refinement theorem or full AP6.4/AP6.5 completion is claimed.

Core workflow: publication is delegated because shared Git metadata is read-only.
Core will verify its current-owner plus Surface overlay before any Main merge.
Core prioritizes native container/recursive lowering, requests exact frozen
files/hashes and this handoff, and treats old queued Int64/auth notes as resolved.
The single-tail milestone is the agent's implementation choice within that scope.
Tabs/English comments, style checks and apply_patch follow repository/coordinator
instructions, not new direct user decisions. Core's integration report belongs
in Objective. Both owning documents receive this provenance correction in one
refreeze, with no code/test change or repeated gate run.
These are coordinator workflow decisions, separate from direct user authorization
for own-branch publication and the requirement that only Core integrates Main.

### Plan

- [x] Implement and verify the listed prototype-only epoch; check coding style.
- [x] Freeze the exact 19 files and external checksum/delta evidence for Core.
- [ ] Core commits/pushes this task branch with the chosen message.
- [ ] Core performs combined-overlay verification and separate integration review.
- Next after publication handoff: predicate partitioning and slice/Acc/QuickSort,
  subject to Core-owned relevance/admission decisions. The full Goal stays active.
- Freeze acknowledgement: no further edits to these files until Core releases
  this epoch for continuation. Publication does not imply integration or promotion.
