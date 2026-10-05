# C60 Actual Acc Runtime Source Capture Handoff

Date: 2026-10-05
Status: focused implementation/qualification complete; exact19 freeze.
Branch: parallel/c-backend-20261003; worker HEAD cc3cba3495 unchanged.
Parent: immutable exact13 C59 manifest e225a438697b32fdf91faf9a4d23f4343cba50a71aa05d4a0227e1e7be974d7f,
archive3acb8a1288309a4ab555de4d28421b258fd5b5ae9dfe134a0c1ce16017edb9d1.
C59 task6df48a3d450d7877b2cf0f6ae68a0b9388c3d45e/prototype
Main d0c9f9b9207eff5413790b49b6b190842241abf7 pushed/remote exact,
Merge05:24UTC receipt consumed. Independent Root13 six199/all126 products with
fresh nine inputs qualify; original link/I-O19 inherited. R2/R3/R4 not adopted.
Chosen message: `prototype: retain runtime source parameters through Acc sorting`.
Related: open #61, [plan](2026-10-05-C-BACKEND-ACC-RUNTIME-CAPTURE-PLAN.md),
[owning Goal](2026-10-03-C-BACKEND-GOAL.md). Original Goal/model remain active.

## Problem List

1. Let an ordinary C caller supply the actual source comparator's Bool argument
 while preserving captured data through both indexed/callable Acc recursive calls.

## 1. Runtime Source Parameter

### Subjective (User)

Inherited2026-10-04 conveyed human clarification via inquiry desk/Merge, English
paraphrase; original wording not supplied: readable executable C faithfully
follows actual admitted Acc QuickSort, including Acc/down recursion, indices,
partition and captures. A labeled faithful manual candidate is acceptable;
no alternate sorter or false generalized-native success. Preserve sealed C32,
original Goal/model and defer List conveniences.
Inherited2026-10-03 human paraphrase: downstream .a/LinkerScript products serve
ordinary C modules without target-driven producer/schema/checker/source-erasure
authority. Implementation lanes continue independently while Merge integrates.
No new ABI/source/effect/adoption/cost principle follows from this handoff.

### Objective (Code)

The actual admitted runtime_sort := \reverse : Bool =>
quickSort Nat &(choose_compare reverse) now yields executable C receiving that
source argument. Existing views verify the ordinary Bool parameter classifier,
retained outer lambda, remaining QuickSort Nat lambda/type/comparator association,
and exact parameter occurrence in the comparator environment. The comparator
must retain one source Bool field with its actual matcher/scrutinee and two
known Nat operand clauses; both clauses are read regardless of runtime mode.
No source evaluation/checking or synthesized expected classifier is added.
Graph/typing snapshot and four guarded advance APIs remain inert throughout
nine body emissions. New source true/false/opposite wrappers ordinarily admit.

Generated gruntime_context and both gruntime_compare clauses retain the source
parameter at runtime. The manual array boundary makes a local context from the
C enum argument, passes it through qs_compare and keeps it alive throughout
partition/Acc Fold/down/indices/both synchronous recursive calls. There is no
fixed constant context, foreign comparison implementation or alternate sorter.
Seven algorithm bodies/parent sections and LT creation recipe stay exact C54/C55.
All C59 source/header/driver/old products remain unchanged.

Explicit private target ABI proposal c_acc_runtime_bool_candidate_v1 declares
gs_sort_mode(enum gs_bool_mode,...). GS_BOOL_FIRST/SECOND name source constructor
positions0/1, not C truth values. In this admitted fixed fixture Bool.true is0
and Bool.false is1; actual source outputs and opposite clause controls verify
correspondence. Invalid modes fail2 before allocation, preserving buffer/written.
Ordinary C source/object/archive clients call both modes against one compiled
product; no recompilation or callback interpretation occurs between calls.
Public symbol inspection defines only gs_sort_mode; duplicate definitions fail1.

This epoch provides a separate direct admitted-image compose.py command with
explicit --source/--driver/--product/--cc/--ar/repeated --cflag. It retains ordinary
per-selector Solve, fixed roles, no trust-image mode, fresh nine inputs, pinned
driver/image/output receipts and atomic no-replace publication. Existing shared
LinkerScript profiles and gs_sort/gs_sort_with ABI are unchanged. A new target
script discriminator is still needed to expose this parameter ABI through
LinkerScript; coordinate that shared target interface separately with Merge.
No missing producer/readback/checking view was demonstrated.

Readable regenerated runtime source-parameter module1069 lines/49139 bytes:
SHAef0c3e093e16f941be2d9455e504a792ca5d56a7830ff18df11e40cdafe3f190.
Declarative header985 bytes:
SHAa16764fa9d9938220e4157fcc3c70e8064fb3b494b62867fa6d961c6304c2a3a.
Provenance6132 bytes:
SHA041f27848fd7fa9dbf07a12ab875c52150df284a6a5655a3bf616e438913f2e4.
Examples are generated from fresh admitted bodies by pack.py; not hand-edited.
Provenance labels the source parameter adapter and manual target ABI bridge.

Exact19 publication files:

- src/prototype/c_backend/acc_runtime_capture/emit.c
- src/prototype/c_backend/acc_runtime_capture/emit.h
- src/prototype/c_backend/acc_runtime_capture/main.c
- src/prototype/c_backend/acc_runtime_capture/build.mk
- src/prototype/c_backend/acc_runtime_capture/extra.p
- src/prototype/c_backend/acc_runtime_capture/pack.py
- src/prototype/c_backend/acc_runtime_capture/module.h
- src/prototype/c_backend/acc_runtime_capture/runtime.c
- src/prototype/c_backend/acc_runtime_capture/client.c
- src/prototype/c_backend/acc_runtime_capture/resource_client.c
- src/prototype/c_backend/acc_runtime_capture/compose.py
- src/prototype/c_backend/acc_runtime_capture/check.py
- src/prototype/c_backend/acc_runtime_capture/README.md
- src/prototype/c_backend/acc_runtime_capture/example/component.c
- src/prototype/c_backend/acc_runtime_capture/example/component.h
- src/prototype/c_backend/acc_runtime_capture/example/provenance.json
- doc/2026-10-03-C-BACKEND-GOAL.md
- doc/2026-10-05-C-BACKEND-ACC-RUNTIME-CAPTURE-PLAN.md
- doc/2026-10-05-C-BACKEND-EPOCH60-HANDOFF.md

New16-file subtree/two SOAP notes; sole existing publication delta is Goal/one
issue table. Baseline uses immutable C59 tar, not an invented worker Git commit.
All28 C32-C59 archives/member manifests remain exact. Shared Git/index unchanged.

Fresh serial j1 gcc14 C11/Wall/Wextra/Werror O2 and full affected-source-client
ASan/UBSan/leak phases use qualified native R1 readonly126441cbd69 and accepted
fc52755b/runtime120c206. Exact C59 closed helpers and C57 main backends plus
qualified producer pointer/Core O2 inputs are reused. No shared-source/producer
rebuild, broad suite or timing; previous original linker/I-O19 remains inherited.

| Pinned producer | O2 | Full affected-source/client SAN |
| --- | --- | --- |
| Native R1 readonly source126441cbd69 | Fresh target helpers,97 expected rows pass | Fresh helpers/generated products/clients,97 pass |
| Acceptedfc52755b/runtime120c206 | Fresh target helpers,97 expected rows pass | Fresh helpers/generated products/clients,97 pass |

Each phase has71zero/3 expected compiler-link1/9I-O2/pending3/13refusal4.
Actual source Bool values/opposite matcher maps, both recursive traces, ordinary
products/public symbols/duplicate definitions, invalid argument/output retention,
empty input, order/permutation, every forced allocation/depth/capacity/rollback,
selector/classifier/unused parameter/inactive branch/extra parameter/changed
successor, fuel, tool failures, body open/write/close/staging, prior product/
empty directory/symlink and real late output controls match expected outcomes.
Existing closed and ordinary main-native routes refuse the runtime contract4.

Retained ascending Core341 expectations remain direct for ascending mode;
descending reversal composes that finite C oracle rather than manufacture a new
source oracle or independent property count. Resource341 observations are finite
C properties, reused through a wrapper selecting the actual source data value,
not a foreign comparison provider. Source large Nat comparison still refuses
depth4 and preserves output/written. SAN covers fresh target helpers/embedded
qualified libraries/generated products/clients; pointer/Core remain O2 inputs.

Initial raw-selection command used unsupported --name and exited2; it did not
show a source frontend failure. Initial combined qualification stops1 because
the C client appended a newline absent from source output; its value/trace/
resource assertions passed. Corrected only the delimiter, retained initial
client/code/logs/path map, and reran into a fresh directory. No source algorithm/
producer-policy bug or changed outcome expectation is claimed.

Evidence: src/prototype/c_backend/.evidence/epoch60-acc-runtime-capture-20261005/.
Outer/per-phase commands.json pin exact argv/environment/exits/times/output
hashes. final-inputs/content copies/phase-products/compilation dependencies/
source-deltas/old-snapshots/style/verification and epoch60-files/runs/freezes
pin the terminal result. All24 product triplets retain parent sections/creation
recipe. Final4054 current/inherited-reference pins have1088 content copies and
349 compile-source/quoted-header dependencies. Tabs/English/no typedef/declarative
headers/Python/SOAP pass. All owned heavy children terminal/reaped; no timing.
C59 source dependencies remain exact frozen bytes.

### Assessment

Agent result: actual source parameter data can flow from an ordinary C caller
through the actual comparator and both indexed/callable Acc recurrences. This
removes the fixed-value and foreign-provider obstacles for this bounded source
wrapper while preserving the existing algorithms and admitted-view boundary.
The extra parameter receives a distinct labeled private target ABI; old ABI/
LinkerScript/Main-native contracts are preserved rather than overloaded.

One runtime Bool argument/one Bool capture/two known Nat clauses only. Extra/
unused/other parameter types/unsupported inactive clauses and general captures/
functions/effects/indexed-callable main-native forms remain explicit refusals.
Manual storage/roles/closures/array/Nat-LT interpretation, fixed body/recipe
qualifiers, whole checked Scope/source equivalence/general native/full61 remain
open. Entire borrowed immutable graph/input-output lifetime/nonoverlap/private
tokens/Nat32/depth256/node65536/status1-6/transactional output/finite SAN persist.
Output count/byte overflow guard, arbitrary pointer/full validation/larger
records/net costs remain unproved/unmeasured. Historical main-native4/input3/
private16/effect/higher recursive Identity controls remain separately reported.

No .a/producer/schema/checker/erasure/accepted-source change or Goal/issue closure;
R2/application/R3 remains unadopted owner work. No cost samples. Atomic publication
is directory visibility, not crash durability. Test/manifest/notice mechanics
are agent workflow. Merge exact task publication/current audit/prototype Main
integration and later target script interface review remain distinct. A posted
notice creates no new user source/ABI/adoption approval.
Root accepted94 operational proposal05:35:00-05:36:30UTC was ACKed after actual
C60 correctness/input-pin handles reached terminal0/reaped. No aborted/censored
gate or worker cost workload; light-only during an agreed interval. Proposal/
ACK itself grants no launch or new human principle, and does not alter this Goal.

### Plan

- [x] Admit/read actual runtime Bool wrapper and retained capture occurrence.
- [x] Emit/execute parameter data/both matcher clauses through actual Acc branches.
- [x] Provide distinct private C ABI/direct-image source/object/archive products.
- [x] Finish four97 focused phases and input/style/immutable-history checks.
- [x] Freeze exact19 manifest/archive/handoff/notice for Merge.
- [ ] Merge exact publication/current independent review/Main integration.
- [ ] Coordinate separate target LinkerScript discriminator before shared edits.
- [ ] General native/full61/Scope/source equivalence/adoption/cost remain open.
