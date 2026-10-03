# C Backend Epoch 1 Handoff

Date: 2026-10-03
Status: verified and frozen for Core review/publication; integration review pending.
Baseline: `parallel/c-backend-20261003`, `2d747cc` plus the exact local files below.
Chosen commit message: `prototype: lower selected fieldful data to native C`
Related: [active Goal](2026-10-03-C-BACKEND-GOAL.md), AP4-AP6, #44/#49.

## Problem List

1. Publish the verified fieldful-data epoch without crossing the downstream or shared-Git boundary.

## 1. Fieldful Native Values

### Subjective (User)

2026-10-03, English paraphrase of the user-provided active Goal: implement
prototype-only C/Linker refinement with actual tests, preserve `.a`, and deliver
reviewable epochs. Own task-branch commit/push is authorized; only Core merges
Main (user authorization relayed by Core). Provenance corrected on 2026-10-03:
Core's publication/coverage/freeze workflow belongs in Assessment and supersedes
the mixed attribution formerly here; it is not a direct user quotation.

### Objective (Code)

- `data SOURCE ALIAS` selects closed unindexed nonrecursive nominal declarations
  with Int32/Int64/selected-enum fields. Native C uses tagged structs, direct
  constructors/Match/capture calls and by-value input/output, without structural
  runtime or allocation. The receipt records the selections and ownership contract.
- O2: `check-c-scalar`, `check-c-enum`, `check-c-data`, `check-c-link`,
  `check-c-backend`, `check-c-sorting-boundary` pass against `2d747cc`'s private
  solver overlay plus this epoch. Checked/trusted structural Acc QuickSort emits
  `FFTT`; source checking uses 54,096 steps, image checking 53,809 and execution
  18,495. These counts supersede historical counts for this tested baseline only.
- ASan/UBSan with leak detection: scalar, enum and data gates pass, including the
  instrumented emitter/raw Oracle generator and generated native clients.
- Fieldful clients cover 65 constructor/field combinations (empty; 16 small;
  48 wide), extrema/wrapping, by-value output aliasing, invalid active tags and
  nominal mixing. Source differentials compare 54 observations over 9 packets.
  Raw Oracle differentials compare both scalar widths on 16 pairs and assert
  no evaluator/substitution execution or input graph/occurrence/proof mutation.
- Recursive/dependent/nested/callback field selections, open callback parameters,
  block-local function thunks, recursive exports and effects explicitly refuse.
  Pending zero-fuel admission publishes nothing; checked/trusted C is identical.
  Successful/refused scripts leave the input hashes unchanged. Reused nominal
  layouts refuse before either raw output stream is written.
- Both pinned and current worktree producers emit identical APGSRC68 images and
  identical C/headers. Final fixture SHA-256:
  `34f35120939a80da52777122f034ca294a8c4606d9ca9ff8437ef229529798a0`.
  The 11-export fixture generates 655 C lines / 14,312 bytes and 44 header lines /
  1,772 bytes. This is a size observation, not a performance/equivalent-interface claim.
- The unchanged baseline emitter rejects the new script at its `data` directive
  with status 2 and no product. The new positive and negative gate passes.
- `git diff --check`, shell syntax, tabs and English/C-style comments pass.
  Two newly added block-comment continuation lines initially used spaces and
  were corrected. Initial fixture syntax/type errors were corrected before
  these final gates; no unresolved epoch test failure remains.
- The worker did not attempt an index write, commit/push, self-merge or issue
  closure. No accepted code, shared plan, artifact codec or `.a` field changed.

### Assessment

Core/agent workflow, 2026-10-03: Core offers publication because the worker's
shared Git metadata is read-only and requests the retained negative tests.
Core endorses the agent's Int64 fallback correction, preserving `::` as an
assertion, and requests frozen files/evidence plus notification before further
epoch-file edits. Manual source/docs edits use apply_patch. This handoff requests
task-branch publication; actual integration review and promotion remain separate.

Agent implementation decision and review: map exact selected constructor
positions and scalar/enum fields, retaining source nominal identities in the
selection table and refusing ambiguous shared erased layouts. Conditional
field extraction stays inside case calls; capture types participate in callee
sharing. Signed fields convert through unsigned arithmetic without overflow or
out-of-range signed casts. Input validation precedes output storage and reads
only active fields. This adds useful AP6.4 data/capture coverage without a new
checker, permanent program IR, source normalization or blanket proof erasure.

The correspondence argument and finite tests are independent of source Sortedness;
no Kernel-checked compiler refinement is claimed. #44 still needs Core's
selected-export admission policy and broader AP4.6 Identity coverage. #49/AP6
still needs recursive/container data, length/append/partition, finite List/slice,
justified Acc/QuickSort, dynamic callbacks/effects and cross-module nominal sharing.
Those requirements are not satisfied by the scalar/fieldful milestone.

### Plan

- [x] Implement fieldful selections, representation, native lowering and receipt.
- [x] Run positive/negative/differential/immutability/inertness and sanitizer gates.
- [x] Compare pinned/current producers; review code and CODING_STYLE.
- [ ] Core commits/pushes only the exact files below on `parallel/c-backend-20261003`.
- [ ] Core reviews integration separately; update public issue progress after publication.
- Completion for this epoch: branch publication of the verified files. Full Goal
  completion remains governed by AP4-AP6 and the active lane plan.

Private evidence root: `/tmp/a-program-c-backend-20261003.vjxTyc/`.
Logs: `epoch-o2.log`, `epoch-sanitizers.log`, `data-final-o2.log`,
`data-final-sanitizers.log`, `epoch1-producer-compatibility.log`,
`baseline-data-refusal.log`. Immutable fixtures: `epoch1-pinned.a`,
`epoch1-current.a`. The original overlay's 55 symlinks all resolve into its
immutable baseline snapshot; the current overlay resolves into this worktree,
never Main's dirty tree. No full compiler acceptance or performance run claimed.

Exact files and additions/deletions relative to `2d747cc` follow.

<!-- DELTAS -->

| Kind | Exact file | + | - |
| --- | --- | ---: | ---: |
| Documentation | `doc/2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md` | 130 | 0 |
| Documentation | `doc/2026-10-03-C-BACKEND-GOAL.md` | 76 | 14 |
| Documentation | `src/prototype/c_backend/README.md` | 56 | 0 |
| Build | `src/prototype/c_backend/build.mk` | 4 | 0 |
| Implementation | `src/prototype/c_backend/link/driver.c` | 12 | 2 |
| Implementation | `src/prototype/c_backend/link/plan.c` | 14 | 5 |
| Implementation | `src/prototype/c_backend/link/plan.h` | 3 | 0 |
| Tests | `src/prototype/c_backend/lower/check.sh` | 3 | 1 |
| Tests | `src/prototype/c_backend/lower/data_check.sh` | 87 | 0 |
| Tests | `src/prototype/c_backend/lower/data_client.c` | 77 | 0 |
| Tests | `src/prototype/c_backend/lower/data_differential.c` | 29 | 0 |
| Tests | `src/prototype/c_backend/lower/data_differential.p` | 24 | 0 |
| Tests | `src/prototype/c_backend/lower/data_fixture.p` | 37 | 0 |
| Tests | `src/prototype/c_backend/lower/oracle_test.c` | 74 | 4 |
| Implementation | `src/prototype/c_backend/lower/representation.c` | 62 | 13 |
| Implementation | `src/prototype/c_backend/lower/representation.h` | 9 | 2 |
| Implementation | `src/prototype/c_backend/lower/scalar.c` | 108 | 29 |
| Implementation | `src/prototype/c_backend/lower/scalar.h` | 4 | 2 |
| Implementation | `src/prototype/c_backend/main.c` | 5 | 3 |

Separate totals: build +4/-0 (net +4), documentation +262/-14 (net +248), implementation +217/-56 (net +161), tests +331/-5 (net +326).
