# C Backend Epoch9 Handoff

Date: 2026-10-03
Status: verified frozen task epoch; publication/joint review pending.
Branch: `parallel/c-backend-20261003`.
Parent: `884d5bbe192e128fc98de8f3afcd2d3bce59211d`.
Chosen message: `prototype: lower selected applied families to native C`.

## Problem List

1. Publish the exact downstream applied-family/List/C-client epoch for review.

## 1. Frozen Applied Family Epoch

### Subjective (User)

2026-10-03, concise English paraphrase of the human requirements already recorded
in the owning Goal: emit usable ordinary C modules from `.a` and LinkerScript,
keep the project bounded and downstream, and preserve explicit unsupported
contracts. Workers may publish their own task branches; Merge alone owns Main
integration. No source promotion or issue closure is authorized by this epoch.

### Objective (Code)

Merge published exact C8 as parent `884d5bb`, reviewed its bounded capture queue
and released its live freeze. All 17 historical hashes match that immutable
commit. Its current-qualified-producer twelve gates/Main review are still pending
in the latest report. C7 tested `091669f` is integrated; Main `66fa707` is
pushed/remote verified according to Merge. Publication and integration remain
distinct. C8's historical handoff/manifests are unchanged.

The new implementation adds target-only `data_of VALUE ALIAS`: ordinary checked
or explicitly trusted admission first selects a closed value; the adapter then
borrows its retained closed value-type classifier. Applied selection maps the
existing declaration's explicit reference arguments to native representations
in source parameter order. Signature lookup uses the exact selected applied
term. One instance per erased constructor/Match layout rejects ambiguity; no
layout equivalence is treated as source nominal equality. Receipts record the
selection method and retain emitter-selected contract metadata.

Seven existing implementation files change; `selection.c/.h` are new. Existing
native lowerer bodies are reused. No producer/schema/artifact/checker changes,
normalization, type-factory evaluator, source graph construction or expected-type
inference are added. The existing `pg_inductive_instance` completion helper is
not called. Headers remain declarative. Scratch remains excluded from this epoch.

Tests add four applied profiles: Nat List, enum List, reversed constructor/field
order and `Choice Nat Bool`. Ordinary C clients also combine separate Numbers
and Flags modules by explicit array conversion with the shared versioned arena
ABI; incompatible nominal node-pointer calls fail compilation. Source take/drop/
slice fixtures use genuine Match/known induction recurrences and composed calls.
Drop reconstructs its suffix. No named target algorithm or hand-coded sorter is
substituted.

Fresh final worker verification is sequential `-j1`, using the immutable producer
snapshot `/tmp/a-program-c-backend-epoch4-current/core-snapshot` (1025-file
manifest SHA256 `c4de94b2f70b581229c89ae4a842bbc16c0873dc1c2fdb2ce0bda8eff6d41e45`).
All snapshot and 34 new source/test/doc hashes remain unchanged after tests.
This is not a fresh Merge current-producer qualification.

All thirteen O2 gates are terminal exit 0:

```text
check-c-backend check-c-link check-c-scalar check-c-enum check-c-data
check-c-list check-c-numeric-list check-c-static-functions
check-c-transitive-functions check-c-value-records check-c-enum-list
check-c-sorting-boundary check-c-applied-families
```

All ten generated-client ASan/UBSan/leak gates are terminal exit 0: scalar, enum,
data, List, numeric List, static functions, transitive functions, value records,
enum List and applied families. Compiler/producer/raw tools and driver-generated
object/archive bodies remain O2; generated source bodies and ordinary clients
are instrumented. No fully instrumented compiler claim follows.

The new gate records, per source/object/archive product: 1093 Nat Lists, 127 enum
Lists, 127 reversed Lists and 34 two-parameter values with nine source observations.
Four applied profiles plus the source slice profile emit identical ordinary C/
headers under guards forbidding evaluation/substitution/WHNF/typed-query advances
and checking unchanged source graph/evidence counts. Source slice clients cover
1093 Lists with five take/drop limits and 25 slice pairs each, including
UINT32_MAX limits; nine complete payload observations match the interpreter.
Every measured allocation position, capacity/depth, null, malformed tag/cycle,
output/prior arena preservation and checked/trusted determinism passes. Mixed
module clients pass nine product pairs/both header orders and 1093 rows each;
these repeated cases are not independent property counts.

All fourteen checked/trusted refusal requests return 4/no product: type factory,
indexed family, missing argument representation, duplicate erased layout,
unselected signature, callback and type-as-value. The original `data list_type`
refusal and zero-fuel pending/no-product control remain. All older positive and
negative gates, including native Acc/QuickSort refusal, are retained.

Exact evidence under `/tmp/a-program-c-backend-epoch9`:

- `epoch9-source-files.sha256`, 34 files: SHA256
  `1ceade3b72ffe12c448cd21983e759cc3d67dad004a641abc3ab69e24b83eec6`.
- `o2.log`: SHA256
  `c79e4aec05c3a3eeea63e920b61cd006a5e25132ef6bb4f0ff6759078e97ed5e`.
- `san.log`: SHA256
  `a1653830a065157b44dd5427162b9888817fa0480d89d807458e86fc4ca286f0`.
- `style.log`: SHA256
  `b5d2d8b91b1de9b1d015e011c00119f69d110052dc23ff272272468f4e4eb056`.
- `build/a-to-c`: SHA256
  `d351f9f1fa2a47d8d0cc8368f3df770fabeb49c2e9bd3a1ffac2af7a15489602`.
- `build/pointer-check`: SHA256
  `4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27`.
- `build/c_applied_inert_test`: SHA256
  `4cda569b630977bc334984548ec82df32d195ea8e80c273bd86b16ee52ef14da`.

Fresh warning-free build log is `/tmp/a-program-c-backend-epoch9-build.log`.
`source-after.log` and `producer-after.log` pin final immutability checks.
Detailed per-file deltas and the full final 38-file manifest are generated after
this documentation; their paths/hashes are in the ready outbox notice.

No active gate failure. Historical private setup failures remain linked in the
[original plan](2026-10-03-C-BACKEND-APPLIED-LIST-PLAN.md) and
[continuation](2026-10-03-C-BACKEND-APPLIED-LIST-NEXT-NOTE.md). The initial slice
fixture incorrectly used two Nat.succ binders for its single recursive field;
it rejected in 1615 steps. Its archived bytes are retained; correcting the
fixture admits in 2981 steps without producer changes. A broad style marker
false positive and generated patch context failure were corrected without
relaxing tests or source outcomes. Final tabs/English comments, no typedef/ML
comments, shell syntax and diff whitespace checks pass.

Exact frozen files, 38 total:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-APPLIED-LIST-PLAN.md
doc/2026-10-03-C-BACKEND-APPLIED-LIST-NEXT-NOTE.md
doc/2026-10-03-C-BACKEND-EPOCH9-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/main.c
src/prototype/c_backend/selection.c
src/prototype/c_backend/selection.h
src/prototype/c_backend/link/plan.c
src/prototype/c_backend/link/plan.h
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/lower/representation.c
src/prototype/c_backend/lower/representation.h
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/applied/baseline.aplink
src/prototype/c_backend/applied/check.sh
src/prototype/c_backend/applied/choice.aplink
src/prototype/c_backend/applied/choice_client.c
src/prototype/c_backend/applied/differential.p
src/prototype/c_backend/applied/fixture.p
src/prototype/c_backend/applied/flags.aplink
src/prototype/c_backend/applied/flags_client.c
src/prototype/c_backend/applied/gate.sh
src/prototype/c_backend/applied/inert_check.sh
src/prototype/c_backend/applied/inert_test.c
src/prototype/c_backend/applied/numbers.aplink
src/prototype/c_backend/applied/numbers_client.c
src/prototype/c_backend/applied/reverse.aplink
src/prototype/c_backend/applied/reverse_client.c
src/prototype/c_backend/applied/modules/check.sh
src/prototype/c_backend/applied/modules/client.c
src/prototype/c_backend/applied/modules/refuse_nominal_client.c
src/prototype/c_backend/applied/slice/check.sh
src/prototype/c_backend/applied/slice/client.c
src/prototype/c_backend/applied/slice/differential.p
src/prototype/c_backend/applied/slice/fixture.p
src/prototype/c_backend/applied/slice/source.aplink
```

### Assessment

Agent choice within authorized prototype scope: use only existing retained typed
views for this bounded target selector. Merge's release/freeze/publication routing
are operational instructions, not new user design principles. Shared Git metadata
remains read-only; delegated publication implements existing branch authorization.
No sandbox bypass, worker Main merge/push, issue closure or source promotion.

Limits remain explicit: closed unindexed reference arguments; prior selected
representations; one instance per erased layout; supported direct fields and one
Self tail. Nonreference arguments, indexed/dependent/function fields, recursive
aggregate/tree layouts, callbacks/effects and unsupported recursive captures
remain refused. Nat32 is target-local uint32 magnitude with status 5 on successor
overflow; depth defaults/maxes at 256 with status 4. Allocation status 3 and
transactional output/storage rules persist. Native indexed SizedList/callable Acc/
QuickSort is not implemented; structural FFTT is a separate gate. No producer
defect or shared-interface dependency has been demonstrated.

Integration needs: publish exact C9 on its C8 task parent, then Merge qualifies
all thirteen gates against its current producer and separately reviews Main
integration. C8 current-producer/Main review is still pending; do not treat C9's
worker evidence or notification as integration approval. Keep the full Goal open.

### Plan

- [x] Adopt and verify the bounded implementation and retained refusal controls.
- [x] Finish thirteen O2 and ten generated-client sanitizer gates.
- [x] Pin source/binary/log evidence and tabs/English comments before handoff.
- [x] Freeze the exact 38 files; report chosen message/branch/manifest to Merge.
- [ ] Delegated task-branch commit/push; immutable publication review is separate.
- [ ] Merge current-producer gates, code audit and any Main integration.
