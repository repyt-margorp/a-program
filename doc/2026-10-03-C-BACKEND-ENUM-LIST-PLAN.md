# Native Enum List Boundary

Date: 2026-10-03
Status: active bounded Epoch7 verified; exact handoff ready for freeze.
Baseline: task branch `parallel/c-backend-20261003`, published C6 `cea1dc0`.
Related: [Goal](2026-10-03-C-BACKEND-GOAL.md), AP6.5/#49.

## Problem List

1. Test finite C array conversion for an already supported selected enum List.

## 1. Enum Payload Arrays

### Subjective (User)

2026-10-03, English paraphrase of the direct human clarification: give concrete
issue-linked feedback because issues accumulate and progress is hard to see.
Show which subproblem advanced and what was implemented/verified. Reporting
format/cadence and freeze mechanics are Core operations, corrected in Assessment.

2026-10-03, English paraphrase of direct user scope recorded in the owning Goal:
emit readily usable C modules through `.a` and LinkerScript, keep implementation
bounded and downstream, and avoid excessive deep investigation. Do not extend
`.a` for target convenience. Workers may publish their task branches; Core alone
integrates Main. Keep the full Goal active. This does not establish a new user
decision about enum arrays or constructor layouts.

### Objective (Code)

Fresh inspection of the baseline plus frozen Epoch6: recursive node selections
already allow enum payloads, and `nodes.c` validates their tags. The separate
`pg_c_representation_list` helper only recognizes scalar/nat32 payloads for finite
copy-out/copy-in. Its generated field assignments already use the selected C
payload type. Simply admitting enum shapes without validating input array tags
would construct invalid source representations. No producer gap is established.
Existing List fixtures have both an integer and Bool payload, so they are not the
single-payload shape addressed here. No general slice-based native execution or
native Acc/QuickSort completion follows from this candidate.

Historical freeze: Epoch6's exact 16-file manifest stayed unchanged during the
isolated trial. Later Core published/pushed `cea1dc0` and released the freeze.
Fresh worker inspection verifies all 16 historical hashes against that commit.
Core reports all ten O2 gates on the joint E3 producer, prototype Main merge
`53debc8` and push complete (evidence/status `fd45c42`). These are Core joint
results, separate from worker seven sanitizers and this trial's pinned producer.

Fresh isolated fixture admits in 5,347 steps; checked linking reconstructs in
5,378. The frozen C6 backend emits Flags/Signals nodes successfully but no array
helpers. Source SHA256: `071ba6775442efe22c29826484d31ede6124a0e00ad43e959b1cceb1ba6acf87`;
image SHA256: `576a5c7c3965ccbe0bcb269ab91a3ad50745880030fe0dfde1bfe0f024243377`.
Evidence: `/tmp/a-program-c-backend-enum-list-trial/admission.log` and
`baseline-emission.log`. This is a concrete supported-node/missing-helper boundary,
not an admission or producer failure.

Fresh isolated implementation changes only the List helper shape predicate and
generated array-input validation/header documentation. The copied includes/build
override are scratch plumbing. O2 and native generated-client ASan/UBSan/leak
product gates pass: 875 cases per source/object/archive product, 24 source/C
observations, both constructor/field orders, copied-input independence, every
allocation failure position, invalid tags before any allocation/arena mutation,
capacity rollback preserving prior results, malformed/cyclic nodes, empty/300-node
conversion and explicit depth refusal. Checked/trusted equality, repeated products,
unchanged images and incompatible enum-array compile refusal pass. Multi-payload
node data has no helper; aggregate payloads/trees/callbacks/effects still refuse.
Numeric/List, List and frozen value-record O2 regression gates pass.

Trial backend SHA256:
`8d5875aa4c874279e38784a68014e897eee0c349fd8f875e4b8999352450bd1d`.
Generated C/header SHA256:
`d2d7759dbe9f91dd4f3580b35fe877bd24faacec8efa2654b0bfb9fab0a0e8de` /
`7bcb62bea275f0b0d19fdfe32650e4c6a6de4a452d38bf6294bfba19681434f5`.
Logs/patches are under the evidence root (`products-o2.log`, `products-san.log`,
`*-regression-o2.log`, `*-trial.patch`). Backend/producer are O2; sanitizers cover
generated source bodies/all clients, while driver objects/archives retain O2.
Worker producer is the same pinned immutable Core snapshot used for C6, not the
latest joint producer. No active gate failure or comparative performance claim.
Those trial results preceded adoption; the historical C6 manifest stays immutable.
Active Epoch7 now applies the minimal shape/tag-validation change, adds the
`check-c-enum-list` fixture/client/gate and extends the scalar raw harness with
enum-chain whole-value/copying/first-field correspondence. Raw emission forbids
evaluator/substitution steps and preserves source graph/store counts; its 17
enum-head observations must pass the rebuilt active generator before counting
final evidence. Scratch copies/build overrides will be removed from publication.

Initial active scalar/raw run stopped at a generated-client GCC
`maybe-uninitialized` warning: its empty-array case passed an uninitialized array
with count zero. Emission/inert checks and source evaluator construction completed;
the target client did not compile, so that run is not a passed gate. The empty
case now uses the documented NULL-array input; successful rebuild and active
O2/sanitizer reruns pass. No producer or lowering change was required. Log:
`/tmp/a-program-c-backend-epoch7/scalar-first-o2.log`.

Fresh final active verification at `cea1dc0` plus the exact C7 source manifest:
all eleven O2 C gates and eight native generated-client ASan/UBSan/leak gates
pass. The new raw enum-chain generator forbids evaluation/substitution during
emission, preserves source graph/store counts and checks 17 evaluator observations
plus ordered whole-enum copying, identity and rejected input preservation.
Scratch sources/build override are removed. Final logs/pins/artifacts/deltas:
[C7 handoff](2026-10-03-C-BACKEND-EPOCH7-HANDOFF.md). Evidence root:
`/tmp/a-program-c-backend-epoch7`. Ten code/test/build files are pinned by
`epoch7-source-files.sha256`, SHA256
`2420d6affa99f52a3913265c55e6d3737859ab9f358a7ca65e763876bc91e6e3`.
Current issue status is the owning Goal's single table; provenance is corrected
there and here. No active failure or joint-current-producer C7 claim.

### Assessment

Core operational reporting decisions, 2026-10-03, superseding the earlier
Subjective attribution: keep one concise status table in the owning Goal with
revision, verification, publication/integration, remaining criteria and next epoch.
Update at material changes/blockers and within six active hours; avoid invented
percentages or closure/promotion claims. Use the existing outbox route and do not
create a reporting framework. During C6 freeze the table stayed in a separate
Goal note; incorporate it after release and preserve old handoffs.

Agent candidate: reuse finite array/List copying for a nullary selected enum
payload. Copy selected structs directly; check every input enum tag before any
allocation or arena/output mutation. Reuse existing finite-node/tag validation
for copy-out. Preserve constructor/field order independence, unchanged failure
outputs, transaction rollback and explicit caller storage/lifetime contracts.
Reject aggregates/recursive pointers as before; do not infer nominal equivalence
from array layout or create a new producer interface.

Core operational freeze and delegated publication remain separate from Main
integration and accepted promotion. This plan is outside Epoch6's manifest;
the issue table is now incorporated into the owning Goal after release. Table
format/cadence/freeze are Core workflow; only the concrete feedback concern is
the direct human requirement. Preserve all historical handoffs.

### Plan

- [x] Admit an enum List fixture and verify the current missing-helper boundary.
- [x] Implement the minimal candidate in isolated representation/node copies.
- [x] Verify enum arrays, filtering/append, invalid tags, malformed chains,
  output preservation, all allocation failure positions and empty/300-node
  conversion in source/object/archive clients, with source/C observations.
- [x] Inspect the isolated emission changes and verify deterministic products,
  immutable images and retained
  unsupported controls; run focused O2 and ASan/UBSan/leak gates.
- [x] After Core released/published C6, adopt only the verified bounded change.
- [x] Verify rebuilt active raw/native gates, remove scratch publication plumbing
  and prepare a separate exact epoch. The full Goal remains active.
- [ ] Core publishes/reviews the exact frozen C7 and verifies its current joint
  producer before any prototype Main integration. No accepted promotion.
