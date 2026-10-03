# C Backend Goal

Date: 2026-10-03
Status: in progress; session `c-backend`, branch `parallel/c-backend-20261003`.
Baseline: committed `eb0aad6` compiler/overlay plus the coordination documents.
Related: [coordination](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
[AP4-AP6](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md), #44/#49.

## Problem List

1. Implement and verify a coherent downstream C realization epoch without
   moving target conventions into A Program semantics.

## 1. Downstream Realization

### Subjective (User)

2026-10-03, English paraphrase of user authorization, relayed by Core: own
task-branch commit/push is explicitly permitted; only Core merges Main.

2026-10-03, English paraphrase of the explicit user role decision relayed by
Core: Core now specializes in design audit, combined verification and Main
merges; Job/Evidence implementation belongs to its sole worker. The current
target-only C epoch continues. Task-branch publication remains authorized;
only Core integrates Main, with no accepted-source promotion.

2026-10-03, English paraphrase of the continuation request: implement and
verify AP4-AP6 and critically check #44/#49 against this committed worktree.
Keep code prototype-only and downstream-only, without extending `.a` for target
needs; deliver tested, reviewed branch epochs and concise SOAP evidence to the
core coordinator. Do not push Main or merge/close issues. The full Goal remains
active until its requirements are verified.

2026-10-03, English paraphrase: Sub1 develops C-backend design in a separate
tmux Codex session, with `/goal`, `6.1 Sol` and `xhigh`. Main supervises and
steers. Earlier requirements: human-usable C modules are a target tradition;
LinkerScript owns ABI/layout, and `.a` must not grow for backend-only needs.
2026-10-03, English paraphrase of the active Goal: start C-backend/Linker
refinement now, while the coordinator continues Job/Evidence removal. Use task
names, separate directories and reviewed integration epochs.

### Objective (Code)

2026-10-03: fresh Git inspection confirms Epoch2 publication `4987c08` on the
task branch and its upstream. Core reports seven current-owner/family-cursor plus
Surface gates passed, followed by prototype-only Main integration `e5d4057` and
Main push. These combined/integration results are Core reports, not worker reruns.
At `4987c08`, `src/synthesis.c:atomic_rule_step` rejects integer tokens outside
Int32 and synthesizes accepted integers as Int32. Large Int64 surface literals
are unavailable. The published data fixture already uses `fallback : #Int64`
for its Int64 branches; no synthesis or fixture correction is needed.

Fresh Epoch3 verification at `4987c08` plus the exact lane edits in the
[handoff](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md): eight O2 C gates pass and five
native gates pass with ASan/UBSan/leak detection. Numeric coverage includes 1,089
comparison pairs, 27,305 stable partition cases, ten source List observations
and three Bool observations, 17 raw Nat Match evaluator observations, reversed
constructor/field order, checked magnitude overflow and finite copy-out including
300 nodes beyond the recursive execution limit. Final receipt/copy-out coverage
passes focused O2 and sanitizer reruns. No remaining gate failure. This uses the
private task-baseline producer; current-owner combined verification is Core's
separate gate before integration.

Historical Epoch1 inspection, 2026-10-03: branch/upstream both pointed to `bb69983`, the
published Epoch1. Core reports its push completed and its exact archive is under
combined current Identity-boundary producer tests. Later Core-reported evidence:
those combined backend gates passed. This is a coordinator report, not a fresh
worker rerun. Later Core report: Main integration is `9061be3`, Main push
succeeded and six current-Core backend gates passed. Prototype-only integration;
the worker then remained at its own `bb69983` baseline.

Fresh Epoch2 verification at `bb69983` plus the exact lane edits in its
[handoff](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md): seven O2 backend gates pass;
native scalar/enum/data/list gates pass with ASan/UBSan and leak detection.
The final list client covers 511 length/flag cases, 35 source differential
observations and 14 raw recursive sum/append evaluator observations. Raw emission
forbids evaluator/substitution calls and preserves graph/store counts. The
producer is the private immutable task-baseline overlay, not current Main.

Inspected 2026-10-03 at clean `2d747cc`, before lane edits: the baseline provides
`src/prototype/c_backend/` with structural emission,
LinkerScript and scalar/enum profiles. AP4-AP6 records incomplete parts; its
historical results were freshly rechecked for this epoch. O2 native, Linker,
structural and Acc QuickSort gates pass; ASan/UBSan native gates pass. Final
pinned/current producer images and emitted C/headers match byte-for-byte.
Evidence and exact publication files are in the [epoch handoff](2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md).

### Assessment

Core operational routing, 2026-10-03: the job-evidence worker uses clean baseline
`5035c7a`, producer `64df10d`; Core no longer edits SE owners concurrently.
Route producer/admission/frontier needs through Core to that worker. This epoch
needs no shared interface change. Continue implementation/tests and exact freeze
handoff; resolved publication authorization and queued Epoch2 notes stay resolved.

Core operational steer, 2026-10-03: release the Epoch2 freeze and prioritize
native numeric-predicate partitioning/List boundary work. Borrow emitter-selected
contract metadata to remove receipt representation reconstruction if simpler.
Keep `.a`, schema and producer authority unchanged; ask Core before shared
interface changes. Do not reopen resolved literal/auth work or publish status-only
commits. The new Goal edits belong to the next epoch; the committed handoff and
its historical manifest stay unchanged.

Core workflow, 2026-10-03: shared Git remains read-only; Core performs the
worker-branch commit/push and verifies emission with its current-owner plus
Surface overlay before any Main merge. This is distinct from publication and
does not authorize worker integration or accepted-source promotion.
Core operational handoff steering, 2026-10-03: provide the exact frozen Epoch2
files/hashes and handoff for worker-branch publication. Older queued literal/auth
notes are resolved. Core's reported `9061be3` merge/push is integration evidence
in Objective, not a new user requirement. This provenance correction changes
documentation only; completed code/tests and verification remain unchanged.
Core's native-container priority, exact freeze and queued-note handling are
operational steering; style checks and apply_patch follow repository/coordinator
instructions. They are not new direct user decisions. This supersedes their
former Subjective attribution and the earlier mixed "Core/user authorization"
label; actual user branch-publication/Core-only-Merge authorization stays there.

Agent Epoch2 implementation decision, 2026-10-03 at `bb69983` plus lane docs:
start with closed unindexed single-tail recursive declarations, represented as
borrowed immutable C node pointers. Constructed nodes belong to an explicit
caller-owned allocation arena. Recognize the existing erased recursive Match
template and statically known induction thunks; do not introduce a recursive
source former or infer a source type from the C representation. Validate finite
input chains and retain explicit refusals for indexed/dependent fields, trees,
callbacks and unsupported thunk shapes. Verify length, append and stable list
selection before attempting slice/QuickSort. This is an agent choice within the
owned downstream scope, not a new user design requirement.
Current implementation: native length/sum/append and stable Bool-field selection,
including scalar results of composed List-producing calls. Numeric-predicate
partitioning is not implemented; do not present flag selection as its completion.
Target arena allocation/depth failures have explicit statuses, preserve output
and roll back only the failing call's new nodes. Default/max recursive depth is
256; this is a target resource bound, not source termination evidence.

Agent Epoch3 decision, 2026-10-03 at `4987c08` plus this plan: the existing
`natLessOrEqual` uses recursive Nat matches; the host API supplies arithmetic,
not an integer comparison primitive. Add an explicit target `nat32` selection
for the exact closed zero/single-successor declaration shape, mapped to uint32
with checked successor overflow. Lower the existing comparator and stable numeric
List partitions through ordinary constructor/Match/call lowering, not a named
comparator replacement. Generalize saturated recursive calls and retain the
existing static IH capture handling. Nested recursive closure captures remain
explicitly refused. Add finite List copy-out
for a structurally checked scalar-payload/single-tail shape, with explicit buffer
capacity and unchanged output on failure. Reuse emitter-selected contract flags
in link receipts. These are backend-local choices; source authority and shared
interfaces remain unchanged. Nat width and recursion depth are target resource
bounds, not source typing or termination evidence.

Core workflow update, 2026-10-03: publication permits the next owned epoch.
Superseding Core steer: the queued Int64 correction was already resolved before
`bb69983`; do not recreate it as a new epoch. Additional Int64 coverage is
optional. Core reports combined current-producer
backend gates passed. The optional Int64 follow-up is retired before publication;
prioritize native recursive/container lowering, without producer
fields or hiding callback/Identity limitations.

Core/agent workflow, 2026-10-03 (supersedes its earlier placement in Subjective):
Core reports Surface's shared index.lock sandbox failure and offers delegated
task-branch publication. The worker supplies exact files, branch, commit message,
test results and CODING_STYLE checks, without a sandbox bypass or self-merge.
Core requests retained unsupported block/callback/recursive tests and endorses
the agent's Int64 fallback correction; that correction preserves `::` as an
assertion. Core requests final evidence, freeze acknowledgement and no further
epoch-file edits during review. Manual source/docs edits use apply_patch.
These are coordination/implementation decisions, not direct user quotations.
Publication, actual integration review and promotion remain distinct.

Assigned scope: C design, lowering/runtime/link prototypes and lane-local tests.
Fresh #44/#49 bodies/comments and PR #45/#50 diffs were inspected. Adopt the
reusable native realization direction; reject the hand-derived sorter as an
implementation and keep whole-module admission/selected-export policy with Core.
Agent Epoch1 decision (historical): implement explicitly selected nonrecursive fieldful data
as C tagged structs with direct constructor/Match calls, scalar/enum fields and
borrow-free value ownership. This advances AP6.4 and non-scalar captures without
claiming recursive List/Acc/QuickSort or higher Identity completion. Recursive,
indexed, dependent and function fields must reject before publication. Epoch2
supersedes the blanket recursive refusal for direct single-tail fields only. No new
shared IR, checker or image fields. The later 2026-10-03 lane assignment permits
this target work while Main continues SE1, superseding the earlier AP6 hold.

### Plan

- [x] Read AGENTS.md, CODING_STYLE.md, the coordination plan and AP4-AP6.
- [x] Inspect relevant GitHub issue/PR bodies and diffs; preserve user Subjective.
- [x] Establish a private clean producer overlay and immutable `.a` fixtures.
  Use `ARTIFACT_SOURCE="$PWD/src"` with `solver_inputs/overlay.sh`; no symlink
  may reference Main's dirty tree. Set private `OVERLAY`, `BUILD` and outputs.
- [x] Select fieldful value data as the next epoch. Gates: public C calls over
  all constructor cases; wrapping scalar fields and enum validation; direct
  constructor/Match, captured values and returned data; interpreter differentials;
  all products, checked/trusted determinism, refused representations/ABI/nominal
  mixing, unchanged input hashes and inert raw emission; O2 and ASan/UBSan.
- [x] Implement this fieldful epoch in `src/prototype/c_backend/`, preserving the downstream boundary.
- [x] Verify this epoch's evaluator/C agreement, C module use, negative controls and input
  image immutability; test committed and current producer compatibility.
- [x] Report per-file implementation/test/doc deltas and measured results in the handoff.
- [x] Core published Epoch1 as `bb69983` on the task branch; merge review is separate.
- [x] Epoch2: implement single-tail recursive nodes, arena ownership, recursive
  Match/known IH thunk lowering and finite input validation. Gates: native
  length/append/selection with ordinary inputs, evaluator differentials, failed
  allocation/output preservation, malformed/cyclic inputs, existing refusal
  coverage, deterministic inert emission and O2/ASan/UBSan checks.
- [x] Core published Epoch2 as `4987c08`; Core reports combined verification and
  prototype-only Main integration `e5d4057`. Freeze released for the next epoch.
- [x] Epoch3: native Nat comparison, stable numeric List partitions and finite
  scalar List copy-out. Verify source/C agreement, boundary/resource failures,
  existing refusals, inert deterministic emission, receipts and O2/sanitizers.
- [ ] Hand off the frozen Epoch3 files/hashes for Core task-branch publication;
  current-owner combined checks and Main review remain separate.
- [ ] Continue the remaining AP4.6/AP5.6/AP6.3-AP6.8 work against their shared
  criteria: recursive data/length/append/partition, List/slice/Acc/QuickSort,
  callback/effect and Identity coverage, explicit cross-module nominal contracts.
  These remain incomplete; no admission or relevance authority is assumed here.
- Completion: the supported C/Linker refinement described by AP4-AP6 and #44/#49
  is implemented and verified within the agreed downstream scope. Epoch results
  go to the coordinator for integration; unsupported constructs and unresolved
  policy choices remain explicit. One small change does not complete the Goal.

## Work Contract

- Write only `src/prototype/c_backend/`, this lane plan and necessary lane-local
  documents. Do not edit accepted code, shared SE/AP plans, Main overlays or
  artifact codecs/checking/fuel policy. Ask Main to transfer scope if required.
- Do not merge PRs, close issues, push Main, promote code or change model.
  Branch-only push is allowed after focused verification.
- No independent Job/Evidence/equality authority, checker or shared program IR.
- Heavy regression/performance runs require Main's machine slot; focused builds
  use `-j2` at most. Keep generated overlays untracked; detach symlinks before edits.
- Maintain this short SOAP plan in place. Before ending each work turn, record
  current task, actual tests, next action and blockers. Report cross-owner needs
  concretely so Main can steer. Do not claim completion for merely writing a plan.

## Current Epoch

2026-10-03: concrete implementation and verification progress; Epoch1 was
published as `bb69983`; Core reports prototype-only Main integration `9061be3`.
No accepted-source promotion. Its exact archive stays frozen.
The owned lane may continue with separately verified follow-ups. Baseline/current private producer
paths and exact deltas/tests are in the [handoff](2026-10-03-C-BACKEND-EPOCH1-HANDOFF.md).
The worker performed no shared Git index write or sandbox bypass. Core may
commit/push the named task branch under the explicit publication authorization.
Agent code review checked nominal layout refusal, unsigned/signed field mapping,
conditional extraction, captures, output preservation and target-only storage.
Epoch2 adds verified single-tail recursion, arena ownership, length/append and
stable Bool-field selection. Core published it as `4987c08` and reports Main
integration `e5d4057`; the historical [handoff](2026-10-03-C-BACKEND-EPOCH2-HANDOFF.md)
and its manifest remain unchanged. Epoch3 is verified and frozen for Core
publication: native numeric-predicate partitioning and finite List copy-out.
Its [handoff](2026-10-03-C-BACKEND-EPOCH3-HANDOFF.md) defines the exact file set
and evidence; no further edits in those files until Core releases the freeze.
Unsupported block-local thunks, callbacks,
tree/indexed/dependent/nested fields and effects remain explicit negative coverage.
Further work includes slice/Acc/QuickSort;
keep relevance/admission decisions with Core. The task has not completed AP6.4/5.
Cross-owner handoff: #44 selected-export admission beside unresolved siblings and
#47 relevance remain Core-owned. The full Goal is active; this epoch does not
complete AP4.6, AP5.6 or AP6.3-AP6.5/7/8 and does not close #44/#49.
