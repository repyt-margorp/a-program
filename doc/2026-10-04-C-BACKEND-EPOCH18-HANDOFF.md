# C Backend Epoch18 Handoff

Date: 2026-10-04 local. Parent task
`d39632b8adc277aa65a5a23669ca6b99d3663e33`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: verify native predicate module composition`.

## Problem List

1. Verify generated native predicate providers and two ordinary C consumers
   within the existing bounded ABI, without changing source authority.

## 1. Native Provider and Borrowed Predicate Module Composition

### Subjective (User)

Existing 2026-10-03 human scope, English paraphrase: make bounded downstream
`.a`/LinkerScript C modules readily usable from other C modules. Preserve source
authority, prototype scope and explicit unsupported contracts. Workers may
publish their task branches and continue independently; only Core/Merge
integrates Main. This is existing authorization, not a new nominal interchange,
callback failure or source-erasure decision. The
[owning Goal](2026-10-03-C-BACKEND-GOAL.md) retains the human record and full
unfinished scope.

### Objective (Code)

No emitter, ABI implementation, producer or schema changes. Eight new
fixture/client/harness/build files contain 415 lines; the parent prototype build
adds four registration lines. README and three lane documents record the
contract, retained failures and single owning issue table. Exact13 paths:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-NATIVE-PREDICATE-MODULES-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH18-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/predicate_modules/build.mk
src/prototype/c_backend/predicate_modules/check.sh
src/prototype/c_backend/predicate_modules/client.c
src/prototype/c_backend/predicate_modules/consumer.aplink
src/prototype/c_backend/predicate_modules/consumer.p
src/prototype/c_backend/predicate_modules/provider.aplink
src/prototype/c_backend/predicate_modules/provider.p
src/prototype/c_backend/predicate_modules/wrong_type.c
```

Owned disk-backed evidence is
`src/prototype/c_backend/.evidence/epoch18-native-modules-20261004`.
`epoch18-files.sha256`, `epoch18-submitted-snapshot.tar` and
`epoch18-freeze.json` pin these bytes; `epoch18-runs.sha256` pins raw commands,
logs, images, generated C/headers/products and client binaries, including initial
failures. All146 selected inputs have retained byte copies and a mapping.

| Input/result | SHA-256 |
| --- | --- |
| `verification.json` | `14d1370cd3c1f1a0170f4991efc05bfac6cfc9fc61110d3528887aabffb89eea` |
| `qualified-inputs.sha256` | `e0563ec722e5e0e4e2bd098287ce6608178d6866ec1aa1b311da125333dc15dd` |
| Retained C17 backend | `305c521b526f7d5e281c48fdd461869cd174df8347f98c83b5a187d3b90bacfa` |
| Qualified E8 pointer checker | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| Provider image | `4d92931e27fff554e69eac610f1b05e450274cd8157265b294fd2f91ccb4123e` |
| Exported consumer image | `e7adbdffbde0f907e56e374c5e7c5b47b3b83ea421ca9009d5a04e5916d11b77` |
| Explicit-import reference image | `c31db7ebd656a1f774d802b478f2c1cdbb51a6f4c5fcbb5c0cfee55c12327472` |

Worker producer is qualified E8
`50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`, reported runtime manifest
`9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
All128 runtime sources independently match Root's qualification and retained
copies. No backend/producer rebuild or broad/timing run occurs in C18.

Serial final `o2-release` and `san-release` each exit0 with 103 expected rows:
96zero and seven expected compiler-one. Sixteen consumer/provider product pairs
and both header orders produce 32 linked clients, plus eight loaded-provider
clients per phase. Each checks 180 small scalar calls, 60 extreme scalar calls,
242 filters and 726 selections, with resource/lifetime controls. All40 clients
match the same93 source observations: ninety scalar results and three finite
Lists. These are finite comparisons; product/order/phase repeats do not create
independent properties. Wrong nominal node pointers and six duplicate-symbol
links reject; images remain unchanged. Shared consumer arena capacity/depth
failures roll back new allocations and preserve earlier allocations and outputs;
copy buffer canaries and post-destroy value lifetime pass.

Initial omitted imports reject1; corrected ordinary imports admit/run0. Attempted
selection of imported names from the reference image rejects1, while its local
`module_choose` selects0. Products use the original exported consumer module
image. No universal import-policy or producer-bug claim follows. The initial
depth test expected refusal for the leaf provider at depth_limit1 and aborted134
(`o2-final` outer1). Generated source charges no recursive depth; the corrected
probe checks actual active-arena refusal4/output preservation and local depth1
success. Original fixtures, commands, binaries, logs and products remain retained.

SAN uses O1 ASan/UBSan/leak controls on clients and source-product bodies; emitted
object/archive/shared bodies, backend and producer stay O2. Tabs, English/ASCII
comments/docs, no typedef/ML comments, shell syntax and diff whitespace pass.
Parent and standalone gate registration have matching dry-runs; registration
changed after runtime gates without changing their tested sources or commands.

Root separately reports C17 task `d39632b8` and prototype Main `8dba3ecb`
pushed/remote exact, with current-E17 runtime128 `0a15914b` gates passing;
reported raw `e9fd805d` is
`merge-storage-20261003/c17-current-e17-gates/Root-results.json`.
This is a Root operational receipt, not fresh worker C18 qualification. C18
current-producer verification, exact task publication and Main integration remain
pending. Historical C17 tar/manifests still verify; sixteen unaffected live
C17 paths remain exact while Goal/README/build advance after release.

### Assessment

Agent bounded choice: generate a provider with reversed true/false enum order
and two independently named Nat/enum/List consumers. Explicit C tag adapters
and finite array copies exchange values; no nominal pointer casts or source type
equality assertions are introduced. Each callback uses a separate local arena
because its consumer's recursive arena is active. Leaf providers leave recursive
hypotheses unused and allocate nothing in tested calls. Valid provider success
and pure-total source interpretation remain external preconditions; this is not
a general foreign failure propagation protocol. Context, code/loaded handles and
all readable borrowed storage must outlive synchronous calls. Unloading happens
after all calls and local arenas end. No ownership escape is tested or granted.

Existing Nat32 magnitude/overflow, finite List, depth/allocation and validation
contracts remain. Native Acc/QuickSort, indexed/callable recursive fields, higher
Identity/general effects and general/boxed/dependent/escaping callbacks remain
explicitly unfinished or refused. This advances #61 ordinary-module usability;
it does not complete native sorting, the Goal or accepted-source adoption.

Core operational workflow: shared Git remains read-only; delegate exact task
publication from the immutable snapshot. Root owns current-producer audit and
Main integration. Manifest/outbox routing and the single issue table are workflow
decisions, separate from human design requirements. Publication is no blocker
for distinct owned work outside submitted bytes.

### Plan

- [x] Verify admitted source/provider products and explicit ordinary C adapters.
- [x] Complete focused O2/client-source SAN with resource, lifetime and refusals.
- [x] Prepare exact13 snapshot, evidence pins and ready outbox notification.
- [ ] Root exact task publication/current-producer audit/Main integration.
- [ ] Preserve unsupported contracts and continue bounded owned C work;
  next candidate splits existing adapters/client into separate C translation
  units to verify public headers and linked/loaded products without new ABI
  policy. Route concrete shared-owner dependencies through Merge.
