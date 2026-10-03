# C Backend Epoch8 Handoff

Date: 2026-10-03
Status: exact frozen epoch; delegated task publication requested.
Branch: `parallel/c-backend-20261003`.
Parent: `d275b75d246d9795d798ee7771519c0febc6c9bb`.
Chosen commit message: `prototype: retain transitive captures in native C`.

## Problem List

1. Retain native dependencies through statically known lexical function chains.

## 1. Bounded Static Capture Epoch

### Subjective (User)

2026-10-03, concise English paraphrase of the direct human requirements in the
[owning Goal](2026-10-03-C-BACKEND-GOAL.md): use `.a` and LinkerScript to produce
C usable by ordinary C modules; keep implementation bounded, prototype-only and
downstream. Own task-branch commit/push is allowed. The separate Merge session
owns coordination/Main integration. Workers do not self-merge, promote or close
issues; unsupported contracts remain explicit. Concrete issue-linked feedback
must show actual progress.

### Objective (Code)

The former `h -> g -> f` captured-offset chain rejected during native lowering.
The finite target binding queue now follows reachable known delayed bodies and
recursive target capture metadata. Each binder enters once; output remains in
stable lexical order. Existing direct-call/capture rebinding is reused. The only
implementation delta is `lower/scalar.c`, `+27/-17` against the parent. No source
evaluation, substitution, checking, graph/evidence construction or callback ABI
is introduced. The exact old `nested_three` source/client control is positive.
Original static raw controls and unsupported negative controls remain.

Final verification uses the immutable worker producer snapshot, not a fresh
current-Main/E6 joint claim. Twelve O2 targets and nine generated-client sanitizer
targets run sequentially at `-j1`, all terminal exit 0. Detailed commands/output:
`/tmp/a-program-c-backend-epoch8/o2.log` and `san.log`.

- O2: backend, linker, scalar, enum, data, List, numeric/List, static functions,
  value records, enum List arrays, sorting boundary and new transitive functions.
- ASan/UBSan/leak: the nine native scalar/enum/data/List/numeric/static/records/
  enum-array/transitive targets. Generated source bodies and all clients are
  instrumented; producer/backend/raw generators and driver object/archive bodies
  are O2. No fully instrumented compiler claim.
- New transitive gate: 600 Int32/seven Int64 cases per source/object/archive
  product, six source observations and 300 descriptive raw evaluator comparisons.
  Three/four/eight chains, shadowing, repeated demand, unused effectful bodies,
  extrema, null outputs, checked/trusted determinism and unchanged `.a` pass.
- Existing static gate: 360 Int32/five Int64 cases/product and nine source
  observations; original 20 raw static comparisons retained. Native Nat/List,
  two-module clients, overflow/depth/allocation rollback controls pass.
- Raw wrappers forbid evaluator/substitution/WHNF/typed-query advances during the
  new emitter probe; source graph/evidence counts stay unchanged. Descriptive raw
  occurrences are not presented as source admission.

No active final failure. Historical private raw-generator build lacked a
typed-query declaration header; corrected warning-free build passes, old log
retained. A transient exec service `Text file busy` rejection prevented one
read; retry succeeded without any sandbox change. Failed manual patch contexts
wrote no matching edit; they were corrected. These are setup/editing failures,
not producer defects or relaxed tests. The baseline native refusal is retained.
Private applied-List harness/setup failures and positive evidence are separately
recorded in its progress plan; that implementation is excluded from this epoch.

Evidence pins (SHA256):

| Item | Hash |
| --- | --- |
| 12 source-file manifest | `f114d8d31c94803d972a0419f69d23615d654b3966a3a7e10a1d72e20ad9afff` |
| Active scalar lowerer | `dbeb9d53c30ca38ae8734a0c1c7c3bd5aa42303a935c47de331997ac6e0fcb54` |
| Backend binary | `399a6112ea33337efc103598bdf62d829071c2a3eb4cc817b2fa7c99b7154b64` |
| Producer binary | `4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27` |
| New raw generator | `df3dc7101e6a2740a25a9486e69634e1bdaf5ea92846f6e13422b0582a3344c6` |
| Admitted transitive image | `1e7888b088f19d57deb2895fc1f1080f44137e08b186f8e28e81461465d9c576` |
| O2 log | `44eb4f7ea128f5ca51def3dd8efc5317f60923399267c2d3142efb3178ba72bd` |
| Sanitizer log | `f02c49d86f7b28eed69e07788cba692e5445643097c731648fe78d41b4677762` |
| 1,025-file producer snapshot manifest | `c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45` |

The image admits in 6,757 steps. All producer snapshot hashes and 12 source hashes
verify unchanged after gates. Source deltas are generated in
`/tmp/a-program-c-backend-epoch8/epoch8-deltas.tsv`: six new portable fixture/test
files, three existing static-control updates, lowerer, prototype build and README.
Tabs, English comments/docs, no typedefs/ML comments, LF/final newline and
`git diff --check` are checked before the manifest freeze. Existing comment-star
alignment is preserved. All manual source/docs edits use `apply_patch`.

### Assessment

Agent decision: finite transitive capture retention is a small bounded usability
improvement. It does not add dynamic closures or establish arbitrary recursion.
Tests cover named finite chain shapes, not a universal nesting theorem. Unsupported
recursive captures/function fields, demanded effects, public callbacks, higher
Identity and general nominal module exchange retain limits. Native Acc/QuickSort
still rejects in checked/trusted boundary gates; no structural FFTT/native sorter
completion inference. Indexed SizedList/callable recursive Acc remain open.

Existing target Nat32 overflow status 5, recursive depth status 4 (default/max
256), invalid input status 2, allocation status 3 and transactional output/arena
contracts remain target-local. No `.a`, schema, producer, accepted source, private
checker or shared interface change; no cross-owner need is established.

Merge operational publication: shared Git metadata is read-only. Merge can commit
and push this exact task-branch snapshot under existing authorization, then review
it with the current producer before any Main integration. Publication, integration
and accepted promotion stay distinct. Freeze all listed files until delegated
publication/release. Goal remains active. Applied-List scratch code is a separate
candidate; its progress document is included only to preserve owning-plan links.

### Plan

- [x] Implement/verify bounded capture change and portable tests; retain exact
  former refusal positively and existing raw/negative controls.
- [x] Finish twelve O2/nine native-client sanitizer gates and style checks.
- [x] Freeze exact 17-file code/test/docs epoch and send manifest/evidence to Merge.
- [ ] Delegated task-branch commit/push and independent remote verification.
- [ ] Merge current-producer combined audit/gates, then separate Main integration.
- [ ] Continue bounded applied-List work after release; native Acc remains open.

Exact frozen files (the full manifest includes this handoff; its hash is in the
outbox notice rather than this self-referential document):

1. `doc/2026-10-03-C-BACKEND-GOAL.md`
2. `doc/2026-10-03-C-BACKEND-TRANSITIVE-CAPTURE-PLAN.md`
3. `doc/2026-10-03-C-BACKEND-APPLIED-LIST-PLAN.md`
4. `doc/2026-10-03-C-BACKEND-MERGE-ROUTING-NOTE.md`
5. `doc/2026-10-03-C-BACKEND-EPOCH8-HANDOFF.md`
6. `src/prototype/c_backend/README.md`
7. `src/prototype/c_backend/build.mk`
8. `src/prototype/c_backend/lower/scalar.c`
9. `src/prototype/c_backend/lower/static_function_check.sh`
10. `src/prototype/c_backend/lower/static_function_client.c`
11. `src/prototype/c_backend/fixtures/static_functions_differential.p`
12. `src/prototype/c_backend/fixtures/transitive_functions.p`
13. `src/prototype/c_backend/fixtures/transitive_functions.aplink`
14. `src/prototype/c_backend/fixtures/transitive_functions_differential.p`
15. `src/prototype/c_backend/lower/transitive_function_check.sh`
16. `src/prototype/c_backend/lower/transitive_function_client.c`
17. `src/prototype/c_backend/lower/transitive_oracle_test.c`
