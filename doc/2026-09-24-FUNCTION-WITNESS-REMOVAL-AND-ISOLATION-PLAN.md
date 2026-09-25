# Function-Witness Surface Removal and Isolation

Date: 2026-09-24; updated 2026-09-25
Starting commit: `fc19ff5c0214f0277f02feee77ff9cf03b744524`

Next work: the [surface-removal completion plan](2026-09-25-FUNCTION-WITNESS-SURFACE-REMOVAL-COMPLETION-PLAN.md)
specifies migration of the remaining consumers and restoration of the full
acceptance gate. The evidence below remains the completed isolation baseline,
not a claim that that migration has already happened.

## Required Outcome

Remove global function-witness access `*f` permanently from the active pointer
compiler. Preserve local recursive IH `*arg` and declaration Self `*`.
Separate the internal graph-witness generator, link a compiler without that
generator, and measure which existing capabilities actually require it.
Do not replace the removed syntax with another public witness accessor.

The existing uncommitted Context/IADT work is outside this task. Preserve it.
Do not silently change the source QuickSort algorithm, its graph relation,
the general Sorted theorem, conversion, or `::` post-synthesis checking.

## Questions and Evidence

The latest audit distinguishes the general graph theorem
`G_quickSort(xs, ys) -> Sorted(ys)` from `Sorted(quickSort(xs))`.
The latter had not been constructed generally at the audit baseline. Rejection of
one packet projection does not prove it impossible. Removing a surface
accessor neither proves nor disproves that connection.

Starting implementation coupling (now removed):

- `synthesis.c:function_graph_step` waits for helper witness generation.
- `function_graph.c:pg_function_graph_supply` demands a completed witness.
- Schema construction itself reads helper formation and result types.
- Packet construction consumes helper witnesses and recursive IHs.

Distinguish a mathematical dependency from an unnecessary implementation
dependency. A failed no-generator build proves only the latter until its
call path has been inspected.

## Work and Gates

- [x] Inspect source resolution, graph planning, generator callers and tests.
- [x] Move packet/witness construction into `function_witness.c/.h`.
  Share the existing branch plan through a private declarative header; do not
  duplicate planning, invent a new proof store, or add kernel rules.
- [x] Build normal and generator-detached binaries. The ordinary compiler
  now omits `function_witness.c` entirely; an explicit control binary links it.
  Symbol-table checks enforce actual detachment, not a stubbed success path.
- [x] Compare ordinary execution, graph formation, general Sorted formation,
  internal witness construction, and existing witness consumers.
- [x] Remove avoidable graph-schema dependence on witness production if
  confirmed by the comparison and actual consumers.
- [x] Delete the global-star resolution path, without a compatibility flag.
  Test globals, aliases, imports, local non-IH bindings, and shadowing.
- [x] Preserve IH and indexed Self, including recursive function fields.
- [x] Audit old positive/negative witness fixtures. Do not count an early
  obsolete-syntax rejection as verification of their original proof property.
  Preserve those semantic assertions through internal tests or an approved
  source migration, and report any unresolved coverage explicitly.
- [x] Recheck general Sorted and the ordinary-result connection separately.
- [x] Run focused tests followed by affected acceptance/image regressions.
  This means running and recording the results, not claiming an all-green gate.
- [x] Record per-file line changes and the dependency verdict.
- [ ] Migrate the remaining historical packet consumers and recover their
  original semantic assertions, following the completion plan linked above.
  Do not redefine their failures as passes.

## Surface Decision

The user explicitly selected isolation first, with no new source syntax.
Automatic functional-elimination elaboration is deferred, not silently
replaced with another public witness accessor.
Keeping a generator internally does not imply eagerly running it for every
ordinary function call or making it an implicit type-directed proof search.

## Required General Result Connection (2026-09-25)

The user requires the stronger theorem before declaring this work complete:
under explicit order/comparator hypotheses and arbitrary `xs : List A`,
prove `general_sorted A R (quickSort A (&le) xs)`. Checking only the graph
conditional, evaluated lists, or a theorem assuming this connection is not
sufficient. No new public syntax, global witness accessor, equality reflection,
or QuickSort-specific kernel rule is authorized.

- [x] Prove ordinary partition property preservation and pivot bounds.
- [x] Prove property preservation and Sorted through Acc recursion using the
  actual supplied accessibility argument; do not assume proof irrelevance.
- [x] Connect measurement and the actual `natAccessible` argument to QuickSort.
- [x] Postcheck the universally quantified exact ordinary-call result, with
  no extra adequacy hypothesis. The explicit proof proceeds directly, rather
  than automatically generating graph adequacy and applying `quick_correct`.
- [x] Connect the local proof predicates to the exact existing nominal
  `general_sorted` and `general_decision` used by the graph theorem.
- [x] Add permanent general positive and wrong-result/motive negatives,
  including pending/retained image verification.

Prefer explicit ordinary source proofs. If an existing rule implementation
blocks them, isolate and repair that general implementation before proceeding;
do not silently weaken the target statement or change the source algorithm.

## Verified Isolation Results

The general theorem is conditional on `g : @quickSort ... xs output`.
Forming and checking it does not construct that argument. The internal
producer is needed for its former packet-based callers, not for the theorem
declaration or ordinary QuickSort execution.

| Controlled comparison | With generator | Without generator |
| --- | --- | --- |
| Before removing helper-schema/witness coupling: general Sorted | done, 603123 steps | unsupported, 431528 steps |
| After removing that coupling: general Sorted | done, 602578 steps | done, 602578 steps |
| Ordinary QuickSort before surface removal | done, 57816 steps | done, 57816 steps |

The first no-generator experiment used a test-only unsupported stub. The final
build does not contain that stub or the generator. `check-witness-isolation`
compares final linked/detached binaries, including invalid global stars,
retained and pending image roundtrips, general Sorted, a wrong-index negative,
and ordinary QuickSort output equality at step chunks 1 and 64.

Examples 01-09: all eight available files check; the six existing execution
oracles also pass at step chunks 1 and 10000. There is no example 08 file.
Internal C tests still exercise actual witness construction and output
agreement. This is not a new surface proof interface.

The broader regression inventory and per-file changes follow below. A legacy
`*f` rejection is not a semantic test pass.

## Ordinary-Result Proof (2026-09-25)

`tests/acceptance/generic-quick-sorted-result.p` proves:

```text
quick_correct :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
  (trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
  (refl:(x:A)->R x x)->
  (decide:(x:A)->(y:A)->Decision A R x y (le x y))->
  (xs:List A)->Sorted A R (quickSort A (&le) xs);
```

`Sorted` says each head is related to every element of its tail and that the
tail is sorted. `Decision` supplies `R x y` in the true case and `R y x` in
the false case. Nothing assumes the input sorted, graph adequacy, equality
of distinct Acc witnesses, or a concrete element type/input/comparator.
The imported source algorithm and its `natAccessible` argument are unchanged.
`join_values` only abbreviates its branch in proof types; the final postcheck
names the imported `quickSort`, not a separately certified replacement sort.

The final export is checked against the existing predicate, not merely a
new family with the same intended meaning:

```text
quick_correct_existing :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
  (trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
  (refl:(x:A)->R x x)->
  (decide:(x:A)->(y:A)->general_decision A R x y (le x y))->
  (xs:List A)->general_sorted A R (quickSort A (&le) xs);
```

`general_all_from`, `general_sorted`, and `general_decision` moved unchanged
from `generic-quick-sorted.p` into its existing provider. Both the graph and
ordinary-result theorems now import those same nominal definitions.
`all_connection`, `sorted_connection`, and `decision_connection` explicitly
map the local proof predicates to/from them by ordinary elimination. No
definitional equality between separately declared families is assumed.

Two implementation gaps blocked this source proof:

1. Explicit Match motives used parameter-domain elaboration, turning Pi into
   `U Pi` and then `F (U Pi)`. Preserve computation-type motives, including Pi;
   only value-type motives receive the implicit F layer. `::` still supplies
   no synthesis information.
2. The existing total/pure result projection `q` only handled exactly saturated
   Match/Fold. Preserve trailing applications before projecting the selected
   result: `q(Fold(M,K) args) = q(K(q(M)) args)`, and distribute those arguments
   into Match branches before q. These are pure result-projection equations,
   not a rule that executes effects or identifies different Acc proofs.
   The policy is versioned as `evaluation/pure/v7`; v6 retained receipts are
   rejected. Description-only inputs can be solved again.

Conversion now tries application congruence for reference-headed applications
with alpha-equal callees before unfolding. It still compares argument terms;
it does not identify applications merely because the callee matches. A failed
probe falls back to normalization. Visible Lambda redexes retain normal-order
beta so an ignored argument is not forced by this optimization. No pointer
interning, equality reflection, or QuickSort-specific rule was added.

The new reduction exposed a regression in normalized-index transport:
`indexed-normalized-transport.p` compared a reduced endpoint against an
unreduced typed index. The existing normalized factoring stage now selects
the actual typed index through the shared Conversion query. Ordinary
substitution pairing and Identity transport still validate the candidate;
there is no raw rewrite of its classifier or new accepted-evidence store.
Normalizing the entire classifier was considered and rejected: a reduced
Core alone does not replace the original typed operands used by inversion,
and normalizing unrelated recursive branches is unnecessary.

`make ... check-quick-result` is part of the normal `check` prerequisites:

| Check | Result |
| --- | --- |
| General source theorem including existing-predicate connection, generator absent | done, 1117652 steps |
| Description-only completed image | done |
| Pending image (100 steps), then resume | done |
| Retained reductions, then reload | done |
| Retype the theorem as Sorted of the unsorted input | rejected |
| Same incorrect postcheck after pending-image resume | rejected |
| Pi-valued explicit motive, two trailing arguments | done |
| Wrong branch result under that motive | rejected |
| Existing normalized-index transport, source and image | done |
| Wrong normalized index | rejected |

This establishes this general sorting theorem, not universal automatic
function-graph adequacy or arbitrary higher Identity coherence.

## Regression Boundary

The ordinary `make check` is not green: it stops in `merge_composition.sh`
when `merge-function-graph-request.p` requests `*curriedMerge`. Continuing
the recipe for inventory is diagnostic only, never a substitute for that gate.
The internal core, synthesis, program, IADT and Identity tests pass, including
internal witness production. Examples 01-07/09 check and their six existing
runtime result oracles pass. General graph-conditional Sorted, the new ordinary
result theorem, and generator-linked/detached controls pass independently.

Historical positive fixtures still containing global witness access include:

```text
comparator-order                 dependent-graph-motive
function-graph-branch-name-collision  function-graph-branch-tree
function-graph-call-sites        function-graph-callable-parameter
function-graph-captured-match    function-graph-curried
function-graph-exposed-match     function-graph-family-parameters
function-graph-function-field    function-graph-helper-call
function-graph-indexed-canonical function-graph-indexed
function-graph-known-match       function-graph-named-fields
function-graph-partial-source    function-graph-refined-case
generated-function-graph-direct generated-function-graph
graph-canonical-leaf-name        graph-comparison-leaves
graph-duplicate-leaf             graph-helper-leaf
inferred-index-copy              inferred-index-graph
length-output-proof             order-reflexivity
```

These names refer to `tests/acceptance/*.p`; several contain many output checks.
Whole-module validity means even an ordinary `main` cannot be checked if a
different definition in the same file contains removed syntax. Their `--equal`
failure therefore does not by itself demonstrate a changed runtime result.
The merge/image scripts also contain packet consumers. Negative packet
fixtures (wrong output, shadowing, imitation and forged graph evidence) can
now reject before reaching their old proof obligation. Do not claim those
old obligations covered by rejection alone. The new isolation negatives test
syntax removal intentionally; the new ordinary-result negatives test an
actual incorrect type assertion after synthesis.

Dependency verdict: optional packet production is not required to execute
QuickSort, form its graph type, check the graph-conditional theorem, or prove
Sorted of its ordinary result. It remains used by explicit internal C callers.
No general automatic functional-elimination feature is claimed or added.

## Code Size Record

Relative to the starting commit, counting this task only. New files count as
additions. Existing Context/IADT edits are excluded; in `tests/core.c`, exclude
the 26 pre-existing context-renaming test lines. All paths below are relative
to `src/prototype/pointer/`. Documentation is excluded from code totals.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| computation.c | 27 | 7 | +20 |
| function_graph.c | 61 | 505 | -444 |
| function_graph.h | 4 | 11 | -7 |
| function_graph_internal.h | 136 | 0 | +136 |
| function_witness.c | 341 | 0 | +341 |
| function_witness.h | 15 | 0 | +15 |
| graph.c | 27 | 0 | +27 |
| graph_internal.h | 1 | 0 | +1 |
| synthesis.c | 61 | 88 | -27 |
| **Implementation subtotal** | **673** | **611** | **+62** |
| Makefile | 24 | 8 | +16 |
| tests/core.c, this task | 19 | 0 | +19 |
| tests/identity_io.c | 2 | 1 | +1 |
| tests/program.c | 22 | 4 | +18 |
| tests/synthesis.c | 3 | 3 | 0 |
| tests/function_witness_isolation.sh | 85 | 0 | +85 |
| tests/quick_result.sh | 52 | 0 | +52 |
| tests/acceptance/computation-motive.p | 14 | 0 | +14 |
| tests/acceptance/generic-quick-sorted-result.p | 290 | 0 | +290 |
| tests/acceptance/generic-quick-sorted.p | 3 | 12 | -9 |
| tests/fixtures/sorted-proof-provider.p | 14 | 0 | +14 |
| tests/fixtures/graph_adequacy/quick-direct.p | 43 | 0 | +43 |
| **Test subtotal** | **547** | **20** | **+527** |
| **Code, build and tests** | **1244** | **639** | **+605** |

Most test growth is an explicit general source proof, not new kernel machinery.
The witness split is chiefly relocation, not deletion of the internal feature.
