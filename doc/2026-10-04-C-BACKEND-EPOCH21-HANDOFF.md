# C Backend Epoch21 Handoff

Date: 2026-10-04 local. Parent task
`7b883a8eb123faab0e288661229196ffdebfe232`, branch
`parallel/c-backend-20261003`. Chosen commit message:
`prototype: retain known captures in native recursion`.

## Problem List

1. Lower admitted known predicate/map captures in private recursive functions
   under the existing native ABI, keeping dynamic and indexed/callable barriers.

## 1. Private Known Captures in Native Recursion

### Subjective (User)

Existing 2026-10-03 human scope, concise English paraphrase: prioritize bounded
downstream `.a`/LinkerScript products usable by other C modules; the backend has
no authority over A Program. Avoid excessive deep or scope-expanding investigation.
Implementation lanes may continue without waiting for Merge; workers may publish
task branches while only Core/Merge integrates Main. The
[owning Goal](2026-10-03-C-BACKEND-GOAL.md) keeps the original dated human record.
No new human ABI, erasure or producer-interface decision occurs here.

### Objective (Code)

`lower/scalar.c` adds40/removes8 lines: inspect a known lambda head through existing
Force/Thunk/application wrappers without evaluating arguments; count only native
dependencies as C parameters; retain/rebind known private thunks and validate dense
native parameter order at recursive calls. Unsupported recursion-valued captures
remain rejected. Seven new fixture/client/inert/harness/build/script files contain
746 lines. Parent build registers the new gate; the old integer script and gate
now test eight former static refusals positively with the same public aliases and
unchanged client. README and four lane documents record evidence and the sole
issue table. Exact16 paths:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-04-C-BACKEND-INTEGER-PREDICATE-PLAN.md
doc/2026-10-04-C-BACKEND-RECURSIVE-CAPTURE-PLAN.md
doc/2026-10-04-C-BACKEND-EPOCH21-HANDOFF.md
src/prototype/c_backend/README.md
src/prototype/c_backend/build.mk
src/prototype/c_backend/lower/scalar.c
src/prototype/c_backend/predicate_integer/check.sh
src/prototype/c_backend/predicate_integer/predicate.aplink
src/prototype/c_backend/recursive_capture/fixture.p
src/prototype/c_backend/recursive_capture/imported_fixture.p
src/prototype/c_backend/recursive_capture/recursive.aplink
src/prototype/c_backend/recursive_capture/client.c
src/prototype/c_backend/recursive_capture/inert_test.c
src/prototype/c_backend/recursive_capture/check.sh
src/prototype/c_backend/recursive_capture/build.mk
```

Owned disk-backed evidence is
`src/prototype/c_backend/.evidence/epoch21-recursive-captures-20261004`.
`epoch21-files.sha256`, `epoch21-submitted-snapshot.tar` and `epoch21-freeze.json`
pin these files. `epoch21-runs.sha256` pins retained commands/exits/logs, initial
and corrected probes, input images, generated C/headers/receipts/products, raw
emission, Core-generated oracle controls and client/helper binaries. All181
selected input files have retained copies and a mapping, including qualified128
runtime files. Relevant pins:

| Input/result | SHA-256 |
| --- | --- |
| `verification.json` | `e69531d51dc7be898d37a4454f2794076cffea1d0c5c25d6dff4b628ee7fea20` |
| `qualified-inputs.sha256` | `b3db446cefafa3e70e7f434fd17d0c6ea0ff1a35bd7a0b32e2505ce01dfc518d` |
| Rebuilt strict O2 backend | `0e9b3f855599f8fe64ccec9928575e3d5232246b527d3ec3b21741dc065d3827` |
| Rebuilt O2 inert helper | `f12b316ad7eb38af23de180ba2b8e581ea92b2609e145a133dfd3f6bb0daec41` |
| Qualified E8 pointer checker | `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44` |
| Final O2 gate record | `597b0ee09048e9cface6e96b56b14fafd2da966c6199f5fed50c0dec8f2503d8` |
| Final SAN gate record | `c502b7bcfba8c3e8b130c5e50c5f9cd524993c49429cea9b4b5d60f66c9ae896` |
| Affected O2 records | `11593259f40e551e905b6907fd9cca9560151c15046d94bbe664e341efbf51f1` |
| Updated integer O2/SAN records | `327850f2c003308fd4c5d846ae8688363f0fa482e3e38d6d87ae6344df49db02` |

Worker producer remains qualified E8
`50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`, runtime128 reported manifest
`9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
Every runtime input matches qualification and retained bytes. Backend/inert and
two affected helpers rebuilt serially on that runtime; the qualified pointer
checker is reused. No producer-driver rebuild, broad suite or comparative run.

All eight exact frozen C20 known/captured wrappers now emit0. Standalone and
explicit-source-reexport families produce source/object/archive modules with
identical public headers. O2/client-source SAN each pass114 expected rows:
74zero, two no-fuel3 and38 refusal4. Native clients, raw-generated clients and
loaded-image execution match sixteen source observations. Raw emission calls
none of the four guarded evaluator/substitution/WHNF/query routines and changes
no source graph/proof counts. Separate existing Core evaluation then supplies200
finite Nat comparisons per family, compiled/run against native products.

Each main client retains C20's341 Lists/width and six all/none calls/List. It adds
85 capture/map Lists, twelve flag/shadow/unused-effect calls/List, five Int32 and
five Int64 shifts/List, twenty-five two-offset Int32 maps/List, and85 genuine Nat
filters plus425 Nat selections. These counts exclude extra boundary calls and
repeat across products/families/phases, not independent properties. Signed
extrema and modulo arithmetic, payload/field order, input preservation, tag/null/
cycle validation, prior arena allocations, rollback, depth and copy-out pass.
Int64 parameters produce the Int64 paths; the sixteen closed source observations
are Int32/Nat, with Int64 extrema separately identified native controls.

The updated integer gate passes61 expected rows each O2/SAN:26zero, one no-fuel3
and34 refusal4. Its unchanged client now tests the eight former static refusals
through the existing public aliases. Seventeen dynamic/effect/callable cases remain
checked/trusted4 without product/output. C21 additionally retains demanded-effect
and dynamic-map refusals in both families. Checked/trusted C/header determinism,
matching-tool repeat receipts, unchanged images and no staging residue pass.

Affected O2 transitive static-capture, native numeric/List and borrowed native-
predicate gates pass. Their original logs report existing comparisons/cases;
these are regression results, not new independent counts. Predicate-native full
outputs are retained. The existing transitive/numeric scripts delete their own
temporary products; their command/terminal logs and input/helper pins remain,
without claiming retained hashes for deleted generated products.

Initial evidence remains: the first capture fix supported2/8 wrappers while six
still refused; known wrapper/application recognition supports all eight. The
initial transitive fixture passed an effectful lambda to a pure map and rejected1;
an ordinary effectful outer export admits but remains target refusal4. Selecting
imported local names as published members failed checked1/trusted3 with BOTH old
and trial backends; own exports worked. Explicit ordinary source re-exports of
the imported types/functions resolve the setup and pass both product families.
No producer defect or new source-export policy is inferred.

SAN instruments O1 clients plus source/raw/oracle product bodies with ASan/UBSan/
leak controls and no PIE. Emitted object/archive, backend/inert/producer remain O2.
Tabs, English/ASCII comments/docs, no typedef/ML comments, shell syntax, diff
whitespace and parent/standalone registration pass. Moving registration after
the114-row gates changes no runtime code; the legacy prefix used there ignores
exports and remains identical after test reconciliation. Only the changed61-row
integer gate was rerun. Fresh read-only Git matches all10 C20 task blobs and
remote `7b883a8e` to the original manifest; its immutable archive remains exact.
No C20 Main/current-producer outcome is inferred from that publication fact.

### Assessment

Agent bounded implementation uses admitted existing typed views, source parameter
order and target-only capture storage. Known suspended bodies are private compiler
objects, never new public closures or ownership. Source use still determines when
they are forced. Explicit source re-exports provide the selected module members;
there is no private checker/IR, `.a` field, producer, public ABI or erasure change.

Existing target limits remain: borrowed inputs must be readable/valid and outlive
shared results; input/output/arena storage does not overlap; callers own the arena.
Recursive depth defaults/maxes at256. Nat magnitudes are uint32; deep comparison
can refuse4 rather than promising source-unbounded execution. Source signed
arithmetic uses fixed-width modulo bits, not a newly supplied signed comparator.
Dynamic signed enum-result predicates, general recursion-valued captures,
indexed/callable Acc, native QuickSort, higher Identity/effects/boxed ownership and
full #61 remain open or refused. No accepted adoption, issue closure, cost claim
or Goal completion follows.

Core operational workflow: delegate exact task publication from the immutable
snapshot because shared Git remains read-only. Root owns current-producer review
and Main integration. The C20 live tests advanced only after independent exact
task/remote verification; historical evidence remains unchanged and every removed
static-refusal expectation has positive source/export/client coverage. This is
test evolution for implemented behavior, not deletion or relaxed acceptance.

### Plan

- [x] Implement private known captures and dense recursive call ordering.
- [x] Verify products/source/readback/raw inert/separate Core/O2/client-source
  SAN and affected regressions; retain setup/intermediate failures.
- [x] Prepare exact16 manifest/archive and ready notice for delegated publication.
- [ ] Root exact task publication/current-producer audit/Main integration.
- [ ] Inspect the next admitted concrete Acc/native representation barrier within
  bounded scope; route actual shared-owner needs and keep full Goal unfinished.
