# Complete Function-Witness Surface Removal

Date: 2026-09-25; updated against the current worktree on the same date.
Status: implementation and verification complete; Main publication pending.
Baseline: Main `fc19ff5c0214f0277f02feee77ff9cf03b744524` plus the current,
uncommitted witness-removal and ordinary-result proof work.

This revision records implementation and verification against the current
worktree. Unchecked entries remain obligations, including the full-suite run
on the publication tree with unrelated edits excluded.

This continues the [removal/isolation plan](2026-09-24-FUNCTION-WITNESS-REMOVAL-AND-ISOLATION-PLAN.md).
That plan records completed experiments and remaining regression failures.
This plan covers their migration and the final all-green integration gate,
before unrelated solver/authority refactoring resumes.

## Latest Request and Scope

The implementation continuation requests completion of this plan, verification
and Main publication. No replacement surface interface is introduced.

The answer is deliberately narrower than "witnesses are unnecessary":

- Remove compiler-provided global `*f`; keep IH `*arg` and declaration Self `*`.
- Keep ordinary proof terms, graph-family types `@f`, and their constructors
  and eliminators. Users can still write and pass proofs as ordinary terms.
- No special accessor is needed for the verified general QuickSort result
  theorem. This is implementation evidence for that theorem, not a theorem
  that arbitrary functional correctness proofs can be generated automatically.
- Internal generation is optional checked term construction, not a second
  authority, an assumption of totality, or an implicit proof argument supplied
  by `::`. Its removal from normal compilation must not remove proof checking.

Use this file as the implementation/progress plan; keep the dated isolation
plan as the earlier baseline rather than creating a competing work list.

## Decision and Limits

Global function-witness access `*f` is not part of the surface language.
Do not replace it with `#witness`, a reserved member, a compatibility flag, or
another privileged accessor. Local recursive IH `*arg`, declaration Self `*`,
ordinary function application, and graph-family expression `@f` remain.
An ordinary, explicitly defined proof function is not a replacement keyword.

The decision removes a privileged way to obtain evidence, not evidence itself.
Ordinary calls do not automatically acquire a graph witness. A source proof
may establish the property directly, or construct ordinary graph evidence and
apply a graph theorem. Internal generation remains a separate optional client
of the existing typed source plan. Do not make every call build a proof packet,
or use `::` to fill in an omitted proof argument.

The verified general QuickSort theorem concludes:

```text
general_sorted A R (quickSort A (&le) xs)
```

It quantifies over the element type, comparator and input, with explicit
reflexivity, transitivity and comparator-correctness hypotheses. It uses
ordinary induction, not global `*f` or the optional packet generator.
`generic-quick-sorted-result.p:quick_correct_existing` connects to the same
nominal predicate as the existing graph theorem, not a structural substitute.

This establishes that this theorem needs no public witness accessor. It does
not establish automatic graph adequacy for arbitrary functions, nor eliminate
the need for proof terms. In particular, it does not prove the general equation
between an arbitrary generated packet's output and an ordinary function call.
Preserve that distinction throughout the migration.

## Current Implementation

All paths in tables are relative to `src/prototype/pointer/`.
These findings come from the current worktree, not only historical plans.

| Area | Existing work | Remaining work |
| --- | --- | --- |
| `synthesis.c:star_application`, `hypothesis_reference`, `graph_reference_step` | Global-star fallback, witness job, and special packet projection metadata removed; scoped IH/Self retained | Preserve rejection and shadowing behavior; do not remove the shared `*` token |
| `function_graph.c/.h` | Helper graph formation no longer demands helper witness generation | Keep one shared typed source plan and nominal family identity |
| `function_witness.c/.h`, `function_graph_internal.h` | Packet production separated; internal C callers retained | Keep it optional; no production CLI linkage or eager generation |
| `Makefile` | Normal CLI omits generator; control CLI links it; `check-quick-result` belongs to `check`; isolation now belongs to `check-acceptance` | Run the complete dependency graph after all consumer migrations |
| `tests/acceptance/generic-quick-sorted-result.p`, `tests/quick_result.sh` | General ordinary-result theorem, wrong result, motive and image controls pass | Preserve and reuse this proof when migrating consumers |
| `tests/fixtures/sorted-proof-provider.p`, `generic-quick-sorted.p` | Exact nominal predicates shared through imports | Keep provider variants and assembly scripts consistent |
| Legacy acceptance and script consumers | Small fixtures, five sorting-property consumers and legacy graph consumers migrated; semantic negative pairs added; complete compatibility target passed | Repeat on isolated publication tree |
| `tests/fixtures/generic_sorted/content-result-proof.p` | Ordinary-result permutation proof, with no ordering hypothesis; four provider/order variants have a passing recorded run | Repeat as part of the final gate after subsequent fixture/helper changes |
| `tests/program.c`, `tests/function_witness_packets.sh` | Optional source/image packet client, 75 producer cases, shifted-helper and import-preserving comparisons | Repeat full gate; output equality remains separate from proof-index checks below |

Earlier verification: general proof completed in 1,117,652 Solve steps with
the generator absent; pending/completed/retained image checks and false-result
rejection passed. Examples 01-07/09 and their existing result checks passed.
The preceding full `check` run failed first on a packet request in
`merge_composition.sh`; diagnosis found 28 positive fixture files behind 106
failing `--equal` invocations, plus script failures. These are historical
failure counts, not the current remaining count.

Recorded incremental verification after those fixture migrations:

- All 199 acceptance-source `program_test --equal` invocations extracted from
  the Makefile passed at both chunk sizes 1 and 64: 398 successful comparisons.
- `check-witness-isolation` passed with six semantic positive/negative pairs
  under both detached and generator-linked binaries, as well as syntax,
  IH/Self, graph-conditional Sorted and image checks.
- The separate `check-quick-result` baseline checked the universally quantified
  ordinary-result theorem and its wrong-result/image controls. The isolation
  test's graph-conditional theorem must not be mistaken for this stronger test.
- Imported/captured request fixtures have been edited. Their output checks do
  not by themselves replace the old internal packet-generation assertions.
- `generic_sorted.sh` completed its graph-theorem, shifted-helper result and
  image checks. The `program_test` run records completion including the added
  shifted-helper internal packet test. Neither closes the full coverage map.
- A later `derived_lt.sh` run completed all four combinations of frozen/derived
  LT and normal/tail-first partition order, including general Sorted and
  permutation proofs, outputs, images and invalid evidence. It predates the
  private helper renaming described below; rerun on the final tree.
- `quick_result.sh` passed after that rename, with the same 1,117,652-step
  ordinary-result theorem and its negative/image controls.
- `sort_insertion.sh` completed insertion, insertion sort, tree sort, merge
  sort and QuickSort ordinary-result proofs and their image/negative checks.
  The Nat QuickSort client reuses `quick_correct_existing`; the general proof's
  private helpers now use the `qsr_` prefix to avoid composition collisions.
- `retained_quicksort.sh` passed all four modes. Solve steps were 640,918
  (solved), 697,978 (retained), 1,506,404 (WHNF) and 1,563,523 (retained WHNF).
  Its budget increased from one to five million steps; this is not a speedup.
- `compatibility-current.log` records the complete target passing, including
  63/63 initial source cases, six internal QuickSort packet cases at both chunk
  sizes, property consumers, invalid proofs and saved/retained images. The
  wrong partition proof needs 1,118,285 steps to reject; its former one-million
  cap caused a pending result, not a soundness failure. Source/result budgets
  for these migrated proof consumers are now five million.
- The internal packet table passed all 75 rows at chunks 1 and 64. Image-based
  producer checks passed for imported, captured and indexed composition and
  all five sorting groups. These run the optional checked C producer without
  restoring a privileged source accessor.
- Isolation passed seven semantic positive/negative pairs and the complete /
  missing-case pair under both normal and generator-linked compilers.

Both the earlier worktree and isolated final-tree `check-acceptance` runs
passed. Earlier logs are under `/tmp/a-program-witness-completion/`, including
`equal-acceptance.log`, `isolation.log`, `program-suite.log`,
`generic-sorted.log`, `derived-lt.log`, `quick-result.log`,
`sort-insertion.log`, `retained-quicksort.log` and `compatibility.log`.
These are supporting local records, not permanent test artifacts or a
substitute for a full run on the final revision.

### Completed Consumer Repairs

Paths below are relative to `src/prototype/pointer/tests/` unless stated
otherwise. The final acceptance run repeats these focused passing checks.

| File or group | Old issue | Implemented replacement |
| --- | --- | --- |
| `acceptance/indexed-independent-pi-motive.p` | Global `*append` in `main` | Explicit general `append_graph` proves `@append xs ys (append xs ys)`; original `preserve` consumes it at that exact result; both chunk sizes passed |
| `acceptance/function-graph-missing-case.p` | Unrelated global `*length` hid coverage failure | Explicit nil graph instance; complete-case positive twin passes and incomplete eliminator rejects |
| `acceptance/generated-function-graph-parameter-mismatch.p` | Negative setup requested `*headOr` | Valid graph constructor for the original fallback; matching consumer passes, different fallback rejects |
| `fixtures/graph_adequacy/` and archived `src/prototype/tests/fixtures/` | Historical packet examples | Active consumers migrated; historical exclusions enumerated in the coverage worksheet |
| `compatibility.sh`, `image_cli.sh`, packet coverage worksheet | Partial verification only | Complete compatibility/image runs passed; producer and semantic coverage mapped below; isolated full run passed |

The normal compiler must remain detached from `function_witness.c` during
these repairs. The internal test client may drive its existing dependency API;
it must not reimplement witness planning. For output-projection tests, expose a
packet to WHNF and normalize its output only. Separately retain checks of its
typed graph evidence and indices: normalizing every suspended proof field is
neither necessary for output projection nor a substitute for those checks.

### General Proof Acceptance Target

Keep these two obligations separate and universally quantified:

```text
quick_correct_existing :
  (A : @) -> (R : A -> A -> @) -> (le : A -> A -> Bool) ->
  transitivity(R) -> reflexivity(R) -> comparator_correctness(R, le) ->
  (xs : List A) -> general_sorted A R (quickSort A (&le) xs)

quick_content_result :
  (A : @) -> (le : A -> A -> Bool) -> (xs : List A) ->
  permutation A xs (quickSort A (&le) xs)
```

The first signature abbreviates its explicit ordinary function hypotheses;
it proposes no new syntax or intrinsic. The actual declarations are in
`tests/acceptance/generic-quick-sorted-result.p` and
`tests/fixtures/generic_sorted/content-result-proof.p` respectively.

Neither signature takes an assumed graph witness for QuickSort, an assumed
equation connecting a generated output to its result, or an assumed result
property. A proof about a generated `output`, even a general one, is not a
substitute for these signatures. Concrete Bool lists supplement them but do
not replace quantification over `A`, `le` and `xs`.

The permutation proof uses ordinary Acc induction, partition/measurement
lemmas, and the existing `permutation` predicate. Its checked helper takes
explicitly typed measurement evidence. A direct Match on the computed proof
encountered unsupported inductive-instance discovery; the ordinary typed
helper works without a new rule. Do not describe this as a repair of that
separate synthesis limitation or introduce expected-type-driven synthesis.

### Immediate Execution Order

| Step | Concrete work | Exit condition |
| --- | --- | --- |
| 1 | Finish W0/W1 assertion mapping and pair each removed packet assertion with source evidence or an internal API test | Same graph, index and output properties checked, not only the same number of passing tests |
| 2 | W2: migrate general QuickSort Sorted consumers first, then content/permutation and other sorting consumers | Properties apply to ordinary calls of the unchanged imported functions |
| 3 | W3: finish script, provider, negative and image migrations | Rejections reach the intended semantic boundary; pending images resume on the same Solve path |
| 4 | W4: full acceptance, affected sanitizer checks, metrics and publication | Tested changes committed and pushed to Main, with unrelated work kept separate |

Unrelated solver/authority refactoring follows this gate. No new automatic
adequacy transformation is a prerequisite for removing the surface accessor.

## Invariants

- Keep erased Lambda/Application/Reference computation separate from typed
  occurrences, contexts and proofs. No compiler flag or receipt is a proof.
- `::` remains a post-synthesis assertion. Neither it nor an expected graph
  type may silently trigger witness synthesis or choose the program's motive.
- All explicit and internally generated proofs use ordinary checked terms,
  IADT elimination, substitution and existing Identity/Conversion rules.
  Do not add equality reflection, Acc proof irrelevance, or a sort-specific rule.
- Do not replace the imported sorting algorithm with a certified lookalike.
  Preserve provider identity, constructor indices, hypotheses and outputs.
- Source and images use the same Solve path. Do not restore removed syntax
  through retained records or create a separate proof-replay authority.
- Keep unrelated pending Context/IADT changes and derived-LT work intact.
  Implementation writes stay under `src/prototype/`; do not edit accepted
  parser/build inputs or mechanically rewrite historical documents.

## Migration Rules

| What an old test actually checked | Replacement | Insufficient replacement |
| --- | --- | --- |
| Output of a packet producer | Ordinary `f args`, checked against the same output oracle | Merely parsing the new source |
| Property of the actual result | Explicit ordinary-result proof, or explicit graph adequacy plus the existing graph theorem | Calling the algorithm and dropping the proof consumer |
| Graph declaration, cases and indices | Keep `@f`, constructor/member and eliminator checks with explicit source evidence | Checking runtime output alone |
| Internal packet construction and dependency planning | Direct internal API test using the existing typed source plan | A new hidden surface alias |
| Invalid graph/evidence/type assertion | Valid setup plus an isolated invalid proof/endpoint, checked at its original semantic boundary | Rejection caused only by removed `*f` |
| Removed syntax | Dedicated global/alias/import/qualified/shadowing rejection test | Treating every star occurrence as global access |

For example, `length-direct.p` already proves
`(xs:List) -> @length xs (length xs)` by ordinary induction and applies the
existing graph theorem at that result. It is a model for small migrations,
not a promise of a general automatic transformation. QuickSort takes a
different, direct property-proof route; both routes need ordinary proof terms.

The migrated `length-output-proof.p` provides a concrete source pattern:

```text
length_graph := \xs:NatList => xs @(self => @length self (length self))
	@nil => (@length).nil
	@cons head tail => (@length).cons head tail (length tail) *tail;
length_graph :: (xs:NatList)->@length xs (length xs);
```

Here `*tail` is an IH, not global `*length`. A proof consumer receives
`length_graph xs` as an ordinary argument. By contrast, merely binding
`output := length xs` inside a computation block does not introduce a proof
that this bound value equals a separate `length xs` expression. Preserve that
typed distinction; do not restore special packet-projection metadata to erase
it. Where needed, use the exact ordinary-call index or checked transport.

Do not automatically replace packet consumers with ordinary calls everywhere.
If the consumer uses its graph evidence, the replacement must supply an
explicit proof or be moved to the appropriate internal generator test while
retaining the separate source-level graph/property assertions. An unported
source theorem remains a documented gap, not a completed migration.

## Stages and Gates

### W0. Freeze the Semantic Test Inventory

- [x] Record revision, worktree diff and compiler flags. Separate inherited
  Context/IADT work from witness-removal changes when reporting and committing.
- [x] Inventory `Makefile`, `tests/acceptance/`, `tests/fixtures/`, inline C
  source strings, and all acceptance scripts. Resolve each star by scope;
  textual search alone cannot distinguish global witness, IH and Self.
- [x] Record each old assertion as: file/export, property, provider, expected
  value or failure, replacement route, positive/negative/image checks, status.
  Use a table in this document, not a new production registry/framework.
- [x] Include `check-acceptance` dependencies beyond `check`; recheck any
  historical expected rejection for accidental early-syntax failure.

Gate: every affected invocation has an owner and replacement obligation.
Do not count an invocation removed from a Makefile as a repaired test.

### W1. Migrate Ordinary Outputs and Small Source Proofs

- [x] Convert output-only consumers to ordinary calls, preserving every export
  oracle and effects/totality assumption. Split mixed-purpose fixtures only
  where needed so removed syntax cannot invalidate unrelated definitions.
- [x] Migrate `length-output-proof.p` using explicit adequacy and its existing
  property. Preserve both `Unary` and `LengthOf`, empty/nonempty consumers,
  rather than testing only the shorter example. All four result checks pass
  at chunk sizes 1 and 64; semantic negative coverage is tracked under W3.
- [x] Work through the graph fixtures in the coverage table below. Keep graph
  formation and elimination assertions even when the old output accessor goes.
- [x] Keep internal packet-production tests in `tests/program.c`/`tests/core.c`
  or a focused test file using their existing helpers. Do not embed a second
  interpreter or duplicate the graph/witness planning algorithm in tests.
  Specifically audit output-only migrations that now retain only `@f`
  formation: callable-parameter eta graphs, captured/helper calls, exposed
  Matches and indexed cases still need their old producer coverage recorded.
  Closed explicit inferred-index proofs do not establish general adequacy.

Gate: each migrated group has matching outputs, meaningful positive/negative
proof coverage, and no public substitute for `*f`.

### W2. Migrate Sort Proof Consumers Without Weakening Them

- [x] Update `tests/fixtures/generic_sorted/boolean-consumer.p` to consume
  `quick_correct_existing` at the ordinary QuickSort result. Retain empty,
  singleton, ordered, reversed and duplicate-input checks as applications of
  the general theorem, not replacements for it. These consumers pass for the
  frozen provider; the complete variant gate below remains open.
- [x] Preserve the graph-conditional theorem as its own test. The new direct
  theorem does not obsolete tests of generated graph elimination.
- [x] Migrate permutation/content-origin consumers independently. Sortedness
  does not imply preservation of elements or multiplicity; the existing
  general content theorem and its actual-result consumers must retain their
  assertions. If direct preservation/adequacy is missing, prove it explicitly
  using existing rules before marking this item complete. The direct
  `quick_content_result` proof and consumers are now present and check for the
  frozen provider and all four variant/image/negative runs; W4 repeats them
  on the isolated final tree.
- [x] Migrate insertion, insertion sort, tree, merge and Nat QuickSort consumers
  in `sort_insertion.sh`; its focused run passed. Existing graph theorems remain,
  and ordinary-result proofs preserve the original predicates and algorithms.
- [x] Complete `legacy-quicksort-*` and compatibility verification. Explicit
  general `ContentsOf A xs (quickSort A &le xs)` and measurement/partition
  bridges are present; the complete focused compatibility target passed.
- [x] Revalidate derived-LT and partition-order variants in `derived_lt.sh`.
  Update exact patches where source movement requires it; preserve zero-fuzz
  application, wrong field-order rejection and nominal-provider isolation.
  The recorded four-variant run passed; W4 still requires the final-tree run.
- [x] Repair the provider-integrity check in `sort_insertion.sh` after inspecting
  the diff: only shared predicates were appended, not algorithm changes. The
  reviewed SHA-256 is now
  `a674b893cd5d0dc1899e79238352c6167a536876393ae7f53b2a9de602b647e6`.
  Keep the fixed fingerprint and one nominal declaration of each shared
  predicate; do not derive the expected fingerprint from the input at runtime.

Gate: Sorted, content/permutation and exact execution results remain distinct
verified obligations. No source theorem assumes its desired conclusion.

### W3. Negative Tests, Images and Isolation

- [x] For each migrated proof negative, first check its valid twin, then change
  only the intended index, comparator, scope or evidence. Removed syntax must
  not be the reason it rejects. Preserve internal forged-packet negatives
  through direct checked API tests where their subject is the generator.
- [x] Update `merge_composition.sh` and `image_cli.sh` with the migrated
  consumers. Keep declaration-order, captured/indexed scope, import, incomplete
  image resume, completed image and output-comparison coverage.
- [x] Update `retained_quicksort.sh`, `generic_sorted.sh`, `derived_lt.sh`,
  `sort_insertion.sh`, and `compatibility.sh`; check ordinary and retained
  images and inert resaves. Record actual executed assertions, not just exits.
- [x] Keep `function_witness_isolation.sh`: no generator symbols in the normal
  CLI, identical public acceptance with/without optional linkage, global-star
  rejection including images, and valid shadowed IH/Self cases.
- [x] Add isolation coverage to the normal acceptance dependency graph.
  Reuse Make targets; avoid rerunning the entire C program suite merely because
  two prerequisites share it. Do not delete overlapping tests until their
  distinct assertions and replacement coverage are recorded.
- [x] Preserve `evaluation/pure/v7` admission checks for retained reductions.
  Rejected older receipts are not silently upgraded to recover old syntax.

Gate: migration remains correct through the same budgeted Solve and image path.

### W4. Full Verification and Publication

- [x] Run focused groups after each migration, then a fresh optimized
  `check-acceptance` with zero unexpected failures. Do not use `make -i` as
  the success gate, remove difficult targets, or change failures to expected
  `unsupported` merely to obtain a green run.
- [x] Run `check-witness-isolation`, `check-quick-result`, example source and
  result gates, and affected sanitizer tests. Track which targets are already
  covered by the full gate to avoid unnecessary repeated long runs.
- [x] Record wall time, Solve steps and output for the same general theorem
  and representative sort/image tests before and after migration. Keep proof
  construction cost separate from ordinary sorting execution. The current
  frozen-provider source comparisons take 10,311,386 steps for
  `duplicates_length` and 10,696,429 for `duplicates_content`; these include
  source solving and proof consumption, not just QuickSort execution. The
  comparison cap was raised from 10,000,000 to 50,000,000, while compilation
  checks in that variant script retain their separate cap. The composed Nat
  QuickSort test now also needs more than its former one-million-step compile
  budget (5,309,924 steps recorded), and some proof-result comparisons exceed
  sixteen million. Record these costs instead of claiming a speedup from a
  larger budget; retain the same input/flags for before/after measurements.
- [x] Report added/deleted/net lines per file; separate implementation, tests,
  build and documentation, excluding inherited edits. Prefer deleted packet
  shortcuts/shared proofs, but never remove assertions to meet a line target.
- [x] Update the README and active plan status with supported syntax, internal
  API usage and any unresolved limitations. Keep dated audit history intact.
- [ ] After these gates, commit the reviewed changes and push Main under the
  established publication policy. List the exact tested revision and remaining
  unrelated work. Do not publish a claim that the entire suite passed earlier.

## Coverage Worksheet

The existing Makefile/script export names and expected values remain the
output oracle. `function_witness_packets.sh` is the per-function/input producer
inventory (75 rows, each at chunks 1 and 64); the rows below map its groups to
separate graph/property assertions. None of its output checks is presented as
a proof of general graph adequacy.

| Group / paths | Assertions to preserve | Status |
| --- | --- | --- |
| `inferred-index-{graph,copy}.p`, `function-graph-{curried,partial-source,indexed,indexed-canonical,family-parameters,function-field}.p` | Recoverable indices, dependent results, currying, parameter and recursive-function fields | Existing outputs and explicit graph constructors pass; corresponding packet-table rows pass; dependent graph motive assertions retained |
| `function-graph-{exposed-match,known-match,branch-tree,branch-name-collision,refined-case,captured-match}.p` | Typed branch selection, capture and graph cases, not just ordinary results | Existing outputs/graph consumers plus packet-table branches pass; missing-case negative has passing complete-case twin |
| `graph-{duplicate-leaf,helper-leaf,canonical-leaf-name,comparison-leaves}.p` | Canonical naming, ambiguity rejection, helper identity | All original exported comparisons and packet inputs retained; existing wrong-constructor/ambiguity negatives remain in `check` |
| `function-graph-{helper-call,callable-parameter,call-sites,named-fields}.p` | Call-site order, separate recursive results, callable inputs and property consumers | Packet table covers ordinary/helper/eta/repeated calls; source proof consumers retained; callable wrong-index twin and C order/field checks pass |
| `generated-function-graph*.p`, `dependent-graph-motive.p`, `comparator-order.p`, `order-reflexivity.p`, `length-output-proof*.p` | Graph witnesses, correct motives, comparator/order and length properties | Same source predicates and outputs; general `length_graph`; seven isolated valid/invalid pairs; wrong-motive tests retained |
| `merge-function-graph-*-request.p`, `merge-function-graph-request.p`, `indexed-captured-graph-*.p` | Imported/captured source identity, graph and wrong-index image tests | `merge_composition.sh` passes source/image output checks and optional packet-image checks for curried/structural/repeated/captured/indexed functions; original wrong-index controls retained |
| `fixtures/generic_sorted/*` | General Sorted and content proofs at ordinary results; unchanged provider variants and all output oracles | All four variants passed in the isolated final run; `quick_correct` graph theorem remains distinct from `quick_correct_existing` and `quick_content_result` |
| `legacy-quicksort-*.p`, `sort-*-property*.p` | Existing Nat predicates, graph theorems and proof consumers | Same predicates/algorithms; direct result proofs added; all five sort groups and legacy retained/property checks passed; optional packet-image checks pass |
| `tests/compatibility.sh` and migrated old `src/prototype/tests/fixtures/` inputs | Supported original behavior and meaningful negative boundaries | Complete target passed, not just its initial 63 cases; wrong recursive/partition evidence reaches typed rejection |
| `tests/program.c:function_graphs` and suspended-helper tests | Optional internal generator, helper dependencies, cancellation and invalid construction | Exact graph classifier, wrong output field, ordered mirror proof term, repeated call slots, incompatible/self/pending helper rejection, totality grades and cancellation tested at both chunks |

The packet driver's image entry uses the ordinary image reader and Solve, then
the same checked producer API as its source entry. Imports are resolved in the
prepared lexical environment, not incorrectly assumed to be public exports.
There is no second source-to-witness implementation in the test harness.

Historical exclusions, not passing current tests:

- `fixtures/graph_adequacy/{length-packet,quick-packet,quick-named}.p` are earlier
  audit inputs; no current Make/script gate invokes their removed accessor.
- Archived `typing/function_graph_import_consumer.p`,
  `function_graph_import_named_consumer.p` and
  `function_graph_quicksort_benchmark_suffix.p` still serve old prototype
  integration/performance scripts. Their compatibility manifest status remains
  `review_pending`; they are not part of the current pointer acceptance claim.
- Active compatibility cases under the old fixture directory were migrated
  individually; no blanket rewrite of archived parser/runtime inputs occurred.

## Publication Verification Record

Publication source snapshot: Git tree
`88510fcbabd826dbaa912e2281f8d934f9ef96fd`, exported to
`/tmp/a-program-witness-publication.yeuHjl/tree`. It excludes inherited
Context/IADT relocation edits, their tests and two derived-LT working files.
The implementation/tests in that tree are the publication candidates;
documentation is updated afterwards with the results.

- Optimized flags: `-std=c11 -Wall -Wextra -Werror -O2`.
- Isolated `check-acceptance`: exit zero, including all four provider/partition
  variants, both compiler linkages, 75 producer rows and all existing gates.
  The clean build plus complete matrix took approximately 27 minutes.
- Isolated core, synthesis and program suites passed with
  `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, leak detection
  and UBSan halt-on-error enabled.
- The same sanitized program driver passed all 75 packet-table rows at both
  chunks and the complete import/capture/indexed image-composition script.
- `git diff --check` passed.

Same-input measurements against baseline `fc19ff5c` (one local sample each,
not a statistically established performance improvement):

| Input / operation | Baseline steps / wall seconds | Candidate steps / wall seconds |
| --- | --- | --- |
| Unchanged `if8_fuel_free_quicksort_check.p`, `--nf main` | 124,792 / 0.043 | 125,173 / 0.043 |
| Same input, NF plus retained save | 124,792 / 0.041 | 125,173 / 0.042 |
| Reload each compiler's retained image | 131,401 / 0.040 | 131,782 / 0.037 |
| Same new general-result theorem and shared provider | rejected at 332,258 / 0.338 | done at 1,117,652 / 0.805 |

The ordinary result graph is unchanged. The additional 381 transitions are
about 0.31% of the baseline NF test. The last row is new proof capability, not
a speedup comparison: the old compiler rejects that theorem. Measurements
use `--legacy-intrinsic-dot`, identical inputs and step caps; builds are
excluded. The larger composed proof tests and increased caps recorded in W4
remain genuine proof-checking costs, not ordinary sorting runtime.

## Change Ledger

Compared with `fc19ff5c`; inherited edits excluded. Moved generator code is
counted on both sides of the move, not reported as pure deletion. The new
ordinary-result proof terms and regression checks account for most growth.
Documentation counts include this ledger.

| Group | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| implementation | 673 | 611 | +62 |
| tests | 1778 | 359 | +1419 |
| build | 28 | 9 | +19 |
| documentation | 1322 | 24 | +1298 |

<details>
<summary>Per-file line changes</summary>

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `README.md` | 12 | 10 | +2 |
| `doc/2026-09-24-FUNCTION-WITNESS-REMOVAL-AND-ISOLATION-PLAN.md` | 297 | 0 | +297 |
| `doc/2026-09-24-GENERIC-QUICKSORT-FIRST-IMPLEMENTATION-PLAN.md` | 8 | 0 | +8 |
| `doc/2026-09-24-GLOBAL-FUNCTION-WITNESS-SYNTAX-AUDIT.md` | 399 | 14 | +385 |
| `doc/2026-09-25-FUNCTION-WITNESS-SURFACE-REMOVAL-COMPLETION-PLAN.md` | 606 | 0 | +606 |
| `src/prototype/pointer/Makefile` | 28 | 9 | +19 |
| `src/prototype/pointer/computation.c` | 27 | 7 | +20 |
| `src/prototype/pointer/function_graph.c` | 61 | 505 | -444 |
| `src/prototype/pointer/function_graph.h` | 4 | 11 | -7 |
| `src/prototype/pointer/function_graph_internal.h` | 136 | 0 | +136 |
| `src/prototype/pointer/function_witness.c` | 341 | 0 | +341 |
| `src/prototype/pointer/function_witness.h` | 15 | 0 | +15 |
| `src/prototype/pointer/graph.c` | 27 | 0 | +27 |
| `src/prototype/pointer/graph_internal.h` | 1 | 0 | +1 |
| `src/prototype/pointer/synthesis.c` | 61 | 88 | -27 |
| `src/prototype/pointer/tests/acceptance/comparator-order.p` | 10 | 4 | +6 |
| `src/prototype/pointer/tests/acceptance/computation-motive.p` | 14 | 0 | +14 |
| `src/prototype/pointer/tests/acceptance/dependent-graph-motive.p` | 5 | 4 | +1 |
| `src/prototype/pointer/tests/acceptance/function-graph-branch-name-collision.p` | 11 | 1 | +10 |
| `src/prototype/pointer/tests/acceptance/function-graph-branch-tree.p` | 29 | 11 | +18 |
| `src/prototype/pointer/tests/acceptance/function-graph-call-sites.p` | 21 | 5 | +16 |
| `src/prototype/pointer/tests/acceptance/function-graph-callable-parameter-wrong.p` | 2 | 1 | +1 |
| `src/prototype/pointer/tests/acceptance/function-graph-callable-parameter.p` | 13 | 7 | +6 |
| `src/prototype/pointer/tests/acceptance/function-graph-captured-match.p` | 8 | 2 | +6 |
| `src/prototype/pointer/tests/acceptance/function-graph-curried.p` | 17 | 9 | +8 |
| `src/prototype/pointer/tests/acceptance/function-graph-exposed-match.p` | 12 | 5 | +7 |
| `src/prototype/pointer/tests/acceptance/function-graph-family-parameters.p` | 9 | 2 | +7 |
| `src/prototype/pointer/tests/acceptance/function-graph-function-field.p` | 13 | 6 | +7 |
| `src/prototype/pointer/tests/acceptance/function-graph-helper-call.p` | 25 | 10 | +15 |
| `src/prototype/pointer/tests/acceptance/function-graph-indexed-canonical.p` | 6 | 3 | +3 |
| `src/prototype/pointer/tests/acceptance/function-graph-indexed.p` | 18 | 10 | +8 |
| `src/prototype/pointer/tests/acceptance/function-graph-known-match.p` | 10 | 4 | +6 |
| `src/prototype/pointer/tests/acceptance/function-graph-missing-case.p` | 1 | 1 | +0 |
| `src/prototype/pointer/tests/acceptance/function-graph-named-fields.p` | 8 | 4 | +4 |
| `src/prototype/pointer/tests/acceptance/function-graph-partial-source.p` | 6 | 2 | +4 |
| `src/prototype/pointer/tests/acceptance/function-graph-refined-case.p` | 5 | 1 | +4 |
| `src/prototype/pointer/tests/acceptance/generated-function-graph-direct-forgery.p` | 2 | 4 | -2 |
| `src/prototype/pointer/tests/acceptance/generated-function-graph-direct.p` | 3 | 4 | -1 |
| `src/prototype/pointer/tests/acceptance/generated-function-graph-parameter-mismatch.p` | 3 | 4 | -1 |
| `src/prototype/pointer/tests/acceptance/generated-function-graph.p` | 13 | 17 | -4 |
| `src/prototype/pointer/tests/acceptance/generated-function-witness-mismatch.p` | 3 | 4 | -1 |
| `src/prototype/pointer/tests/acceptance/generated-packet-imitation.p` | 4 | 1 | +3 |
| `src/prototype/pointer/tests/acceptance/generated-packet-shadow.p` | 9 | 3 | +6 |
| `src/prototype/pointer/tests/acceptance/generated-packet-wrong-output.p` | 8 | 2 | +6 |
| `src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p` | 290 | 0 | +290 |
| `src/prototype/pointer/tests/acceptance/generic-quick-sorted.p` | 3 | 12 | -9 |
| `src/prototype/pointer/tests/acceptance/graph-canonical-leaf-name.p` | 2 | 2 | +0 |
| `src/prototype/pointer/tests/acceptance/graph-comparison-leaves.p` | 10 | 4 | +6 |
| `src/prototype/pointer/tests/acceptance/graph-duplicate-leaf.p` | 3 | 3 | +0 |
| `src/prototype/pointer/tests/acceptance/graph-helper-leaf.p` | 3 | 3 | +0 |
| `src/prototype/pointer/tests/acceptance/indexed-captured-graph-request.p` | 9 | 10 | -1 |
| `src/prototype/pointer/tests/acceptance/indexed-independent-pi-motive.p` | 6 | 1 | +5 |
| `src/prototype/pointer/tests/acceptance/inferred-index-copy.p` | 6 | 1 | +5 |
| `src/prototype/pointer/tests/acceptance/inferred-index-graph.p` | 5 | 2 | +3 |
| `src/prototype/pointer/tests/acceptance/legacy-measure-property.p` | 10 | 4 | +6 |
| `src/prototype/pointer/tests/acceptance/legacy-partition-property.p` | 25 | 2 | +23 |
| `src/prototype/pointer/tests/acceptance/legacy-quicksort-graph.p` | 5 | 4 | +1 |
| `src/prototype/pointer/tests/acceptance/legacy-quicksort-property.p` | 79 | 2 | +77 |
| `src/prototype/pointer/tests/acceptance/legacy-quicksort-witness.p` | 2 | 2 | +0 |
| `src/prototype/pointer/tests/acceptance/length-output-proof.p` | 9 | 5 | +4 |
| `src/prototype/pointer/tests/acceptance/lt-derived-helper-graph-shifted.p` | 12 | 4 | +8 |
| `src/prototype/pointer/tests/acceptance/merge-function-graph-captured-request.p` | 9 | 15 | -6 |
| `src/prototype/pointer/tests/acceptance/merge-function-graph-request.p` | 1 | 2 | -1 |
| `src/prototype/pointer/tests/acceptance/order-reflexivity.p` | 8 | 3 | +5 |
| `src/prototype/pointer/tests/acceptance/sort-insertion-property.p` | 26 | 5 | +21 |
| `src/prototype/pointer/tests/acceptance/sort-insertion-sort-property.p` | 11 | 6 | +5 |
| `src/prototype/pointer/tests/acceptance/sort-merge-fuel-wrong.p` | 2 | 2 | +0 |
| `src/prototype/pointer/tests/acceptance/sort-merge-property.p` | 55 | 7 | +48 |
| `src/prototype/pointer/tests/acceptance/sort-quick-property.p` | 26 | 7 | +19 |
| `src/prototype/pointer/tests/acceptance/sort-tree-property.p` | 74 | 7 | +67 |
| `src/prototype/pointer/tests/compatibility.sh` | 17 | 8 | +9 |
| `src/prototype/pointer/tests/core.c` | 19 | 0 | +19 |
| `src/prototype/pointer/tests/derived_lt.sh` | 14 | 5 | +9 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/boolean-consumer.p` | 5 | 4 | +1 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/boolean-content-consumer.p` | 4 | 4 | +0 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/boolean-wrong-comparator.p` | 1 | 1 | +0 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/boolean-wrong-content-result.p` | 3 | 0 | +3 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/boolean-wrong-output.p` | 1 | 2 | -1 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/content-proof.p` | 6 | 6 | +0 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/content-result-proof.p` | 101 | 0 | +101 |
| `src/prototype/pointer/tests/fixtures/generic_sorted/derived-lt.patch` | 2 | 2 | +0 |
| `src/prototype/pointer/tests/fixtures/graph_adequacy/quick-direct.p` | 43 | 0 | +43 |
| `src/prototype/pointer/tests/fixtures/sorted-proof-provider.p` | 14 | 0 | +14 |
| `src/prototype/pointer/tests/function_witness_isolation.sh` | 96 | 0 | +96 |
| `src/prototype/pointer/tests/function_witness_packets.sh` | 102 | 0 | +102 |
| `src/prototype/pointer/tests/identity_io.c` | 2 | 1 | +1 |
| `src/prototype/pointer/tests/merge_composition.sh` | 9 | 0 | +9 |
| `src/prototype/pointer/tests/program.c` | 233 | 28 | +205 |
| `src/prototype/pointer/tests/quick_result.sh` | 52 | 0 | +52 |
| `src/prototype/pointer/tests/retained_quicksort.sh` | 5 | 3 | +2 |
| `src/prototype/pointer/tests/sort_insertion.sh` | 24 | 7 | +17 |
| `src/prototype/pointer/tests/synthesis.c` | 3 | 3 | +0 |
| `src/prototype/tests/fixtures/negative/function_graph_coarse_forgery.p` | 3 | 1 | +2 |
| `src/prototype/tests/fixtures/typing/function_graph_block_binding_check.p` | 5 | 1 | +4 |
| `src/prototype/tests/fixtures/typing/function_graph_dependent_output_ih_check.p` | 5 | 5 | +0 |
| `src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p` | 6 | 4 | +2 |
| `src/prototype/tests/fixtures/typing/function_graph_generated_length_check.p` | 5 | 5 | +0 |
| `src/prototype/tests/fixtures/typing/function_graph_named_case_check.p` | 7 | 3 | +4 |
| `src/prototype/tests/fixtures/typing/function_graph_nominal_index_constant_motive_check.p` | 6 | 11 | -5 |
| `src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p` | 6 | 9 | -3 |

</details>

## Completion and Non-Goals

Completion means no active positive surface fixture depends on global `*f`,
its semantic assertions have recorded replacements, the normal compiler
remains generator-detached, and the complete acceptance gate passes. Negative
syntax tests and explicitly historical documents may still contain `*f`.

Do not add automatic general adequacy, a new proof-search scheduler, a new
artifact authority, public witness syntax, or a solver rewrite to finish this
migration. Keep the internal producer while its C clients are exercised;
deleting it is a separate decision, not a consequence of removing surface
access. If a source proof requires a genuinely missing general rule, first
reduce the failure and update this plan with that rule and its negative tests.
Do not disguise the gap with a new privileged accessor or a weaker theorem.
