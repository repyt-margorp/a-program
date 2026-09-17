# Issue 29: Graph Exports and Indexed Transport

Date: 2026-09-18. References: [issue #29](https://github.com/repyt-margorp/a-program/issues/29)
and [documentation PR #30](https://github.com/repyt-margorp/a-program/pull/30).
Baseline: `3a3bf550b3e882ab650395fd612e0fa2526b45bc`, with the in-progress
typed-data authority refactor. Preserve those unrelated changes.

## Scope and Invariants

Fix the two independently reproduced limitations, not claim universal Sorted
proofs for four sorting algorithms. PR #30 contains investigation and fixtures,
not a proposed implementation. Its failed comparator proof has no established
compiler diagnosis and is not automatically a regression requirement.

- Keep graph constructor identities distinct; names are owner-local aliases.
- Keep indexed refinement as explicit checked Identity transport. Do not equate
  indices by mutation, bypass kernel checks, or use `::` to generate a proof.
- Source synthesis and restored artifacts use the same checking path.
- No new Core tags, acceptance authority, or special LE rule.

## Progress

- [x] Read issue #29 and PR #30, including their fixed-revision caveats.
- [x] Finish the existing optimized acceptance run before editing source (pass).
- [x] Reproduce both failures against current code.
- [x] Replace graph export-name collision rejection with unambiguous local names.
- [x] Diagnose why checked transport cannot synthesize the LE predecessor result.
- [x] Fix the underlying generic transport limitation without weakening checks.
- [x] Add positive and negative permanent tests, graph elimination/witness tests,
      and serialized/resumed checks.
- [x] Run targeted debug and ASan/UBSan program tests and optimized full acceptance.
- [x] Finish ASan/UBSan image-CLI regression run.
- [x] Record verified scope and remaining limitations; do not close #29 on mere
      graph extraction or finite sorted examples.

## Initial Findings

`function_graph_exports` rejects any repeated constructor alias, although the
generated declaration already has distinct constructor objects. An existing
acceptance fixture expects this `UNSUPPORTED` result, so a green suite alone
does not cover the requested behavior.

`match_dependent_motive_step` declines a general type-case fallback when index
Identity arguments are present. Removing that guard is not a fix: first inspect
the preceding candidate synthesis and checked index transport. LE predecessor
needs both endpoint equalities, not just one independently transported index.

## Implemented Changes

`function_graph_exports` now gives every generated constructor a canonical,
owner-local `case0`, `case1`, ... name. Distinct constructors stay distinct.
Unambiguous source names remain aliases; repeated aliases are unavailable rather
than overwriting a leaf. Canonical names take precedence over a source constructor
literally named `case1`, which is covered by a separate fixture. This is a naming
policy introduced by this patch, not a requirement prescribed by PR #30. The
ordinals identify the generated layout, not stable IDs across edits to a function.

`index_transport_step` previously checked each transported candidate immediately
against the final result type. It can now continue through another checked
transport when the classifier loses a free-binder dependency unavailable at the
destination. A strictly decreasing count bounds the recursive search; all steps
still construct ordinary Identity transport evidence. Both directions and the
remaining candidates are tried. No LE-specific rule, guessed equality, new job
kind, or expected-type-driven synthesis was added.

This search is deliberately incomplete: a valid proof requiring a non-decreasing
intermediate dependency count may still need explicit transport. It also revisits
the scoped path candidates at intermediate types; do not claim an optimal search
or constant-time behavior. The final dependent-motive guard remains in place.

## Verification and Limits

The unmodified minimal probes reproduced `unsupported` at steps 754, 842 and
3352. After the fix they reached `done` at 779, 867 and 6788 respectively; the
larger permanent fixtures include proof consumers and have different counts.

Permanent coverage under `src/prototype/pointer/tests/acceptance/`:

- `graph-duplicate-leaf`, `graph-helper-leaf`: three distinct leaves, graph
  elimination, witness packet consumption and all branch outputs.
- `function-graph-branch-name-collision`: four nested Bool combinations plus
  the base case; the previous expected-unsupported test now proves a Size property.
- `graph-canonical-leaf-name`: a source alias cannot hijack a canonical name.
- `graph-comparison-leaves`: graph induction establishes an independent recursive
  `Compared` specification and checks four witness outputs. This is not yet the
  theorem connecting the comparator to conventional LE.
- `le-predecessor`: open-index inversion and a closed result equality.
- Four negative fixtures: ambiguous alias, wrong graph leaf, reversed LE endpoints,
  and an invalid predecessor proof must be rejected.

`tests/image_cli.sh` tests positive fixtures at save budgets 0/100/100000,
an inert resave, and ordinary resumed Solve/result comparison. Negative cases
are saved at 0/100 and must remain rejected after resave. `program_test` result
checks use both chunks 1 and 64.

Optimized `check-acceptance` passed with `-Wall -Wextra -Werror -O2`;
log: `/tmp/a-program-issue29-acceptance-o2.log`. Focused debug results passed.
ASan/UBSan program baseline, predecessor, nested graph property, comparator and
four negative tests passed, as did source `match-origins` and
`constructor-inputs`. The complete `tests/image_cli.sh` runner also passed
under ASan/UBSan (log: `/tmp/a-program-issue29-asan-images.log`). This is not a
claim that every parent-plan acceptance target has been run under sanitizers.

Exploratory ordinary LE transitivity with a recursive IH and nested indexed
Match still reached `unsupported` (10001 steps). A comparator proof returning
an LE-indexed Decision family also remains unsupported in the attempted spelling.
The transitivity spelling, using the predecessor fixture's Nat and LE, was:

```text
le_trans := \x:Nat => \y:Nat => \proof:LE x y => proof
    @zero n => (\z:Nat => \upper:LE n z => LE.zero z)
    @succ a b prior => (\z:Nat => \upper:LE (Nat.succ b) z => upper
        @succ c d rest => LE.succ a d (*prior d rest));
```

These attempts need further motive/IH analysis; they are neither accepted proofs
nor evidence that every encoding is impossible. No universal Sorted proof for
the four algorithms was completed. Issue #29 remains open; PR #30 has not been
merged or presented as an implementation patch.

The next refactoring findings, including the distinction between `@f` and `*f`,
are in the [Solver audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md).
Execution priority is now governed by the
[IADT / issue 29 / PR 30 plan](2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md):
finish those coordinated workstreams before the broad Solver cleanup.

## 2026-09-18 Follow-up: Transitivity

After the initial IADT declaration/pattern changes, the full optimized image-CLI
runner still passes the graph/predecessor cases. #29 and PR #30 remain open
(queried again with `gh`); no issue closure or PR merge is claimed.

A second probe recursing on the upper LE proof remained unsupported at 4653
steps. A debug trace first reached `graph_reference_step` for `*prior` while
exploring an inner Match before the outer IH was available. This identifies a
failed provisional route, not proof that IH binding is always lost.

The retained `tests/le-transitivity-probe.p` instead factors the nested Match
into a helper taking an ordinary higher-order IH. It currently rejects at
4260 steps. The first rejected source application is `ih d rest` in the helper:
its INDEX_TRANSPORT_JOB exhausts the available paths without a checked argument.
The next investigation is `index_transport_candidate`: choosing the context
prefix before the refined variable can exclude a later independent field
needed in `LE c d`. Inspect checked pattern inversion and context transport
before changing that prefix. This is a concrete hypothesis, not a proven fix;
do not remove scope checks or silently identify c and b.

Neither probe is counted as an accepted universal theorem. The remaining
transitivity, comparator and per-sort obligations are unchanged.

## 2026-09-18 Follow-up: Retaining Independent Later Fields

The previous scope hypothesis was reproduced on published Surface revision
`d5a02de`: the retained transitivity probe rejected at 4363 steps. The first
failed field transport could not form its family for `LE c d`. Choosing only
the prefix before `c` discarded independent `d` as well as dependent fields.

The candidate builder now constructs a checked map retaining later declarations
whose domains can be rebased into the retained scope. It omits the varied binder
and unavailable dependent declarations. It then appends one fresh varied binder.
Pattern inversion still checks substitution back to the original classifier;
ordinary family transport checks both endpoint substitutions and the path.
The family for this example is `t |-> LE t d`, with `d` held constant. This is
context exchange/weakening through existing rules, not equality reflection,
raw Core replacement, or a new refinement store. Rebase queries use the existing
typed-subject/context cache. Unsupported dependent exchanges remain unsupported.

Focused results with the change:

- `indexed-later-scope.p`: a nonrecursive reconstruction through a typed helper;
  rejected at 2996 steps before, accepted at 4854 after.
- `le-transitivity.p`: the original conventional two-constructor proof, extended
  with base and two-level consumers; accepted at 11447 steps. The previous
  investigation fixture moved into acceptance coverage.
- `comparator-order.p`: graph induction proves `Decision x y answer`, whose true
  constructor contains `LE x y` and false constructor contains `LE (succ y) x`.
  This is an order theorem, not the earlier equation-only `Compared` relation.
  Base, less-than, greater-than and duplicate/equal executions pass result checks
  with Solve chunks 1 and 64. No direct proof of the historical alternative
  comparator spelling is claimed.
- Reversed scope-transport arguments and reversed transitivity endpoints reject.

Final acceptance/image/sanitizer gates pass; implementation `1590b2f` and the
documentation-only PR #30 merge `5d9fa03` have been published to Main.
The four universal sort properties are still separate unfinished work.

### Validation and Cost Comparison

Full optimized `check-acceptance` passed after finalizing the negative fixtures
(`/tmp/a-program-g2-final-acceptance.log`). A first negative comparator attempt
omitted the successor lifting entirely and stopped at unsupported motive
synthesis, not explicit rejection. It was replaced by a well-formed theorem
with a reversed-endpoint ascription, which rejects. No existing rejection test
was relaxed. Sanitized synthesis, Match-origin and constructor-input/root tests
also pass. The all-sanitized image-CLI run passed (exit 0):
`/tmp/a-program-g2-asan-images.log`. It covers positive saves at 0/100/100000
steps, negative saves at 0/100, inert resaves and result Solve chunks 1/64.

Comparison against published Surface `d5a02de`, using identical source inputs
and options; both checkers use `-O2` for the time samples. The allocation counts
were read at `report` in separate `-O0 -g` builds from each exact source tree.
Arena usage is the sum of used aligned units (32 bytes here), not total RSS or
temporary/index heap allocation. Times are single noisy local samples, not a
claimed speedup or statistically established regression.

| Input | Solve steps before/after | Arena bytes before/after | Seconds before/after |
| --- | ---: | ---: | ---: |
| `le-predecessor.p` | 7801 / 8453 | 3955904 / 4240192 | 0.009 / 0.011 |
| imported `legacy-certified-length-results.p` | 5683 / 5683 | 2279520 / 2279520 | 0.005 / 0.008 |
| imported `legacy-quicksort-witness.p` | 94926 / 95227 | 33426432 / 33596064 | 0.093 / 0.101 |

The predecessor increase is 8.4% in steps and 7.2% in arena use. QuickSort adds
0.32% in steps and 0.51% in arena use; length is structurally unchanged. Retaining
more independent declarations makes some unsuccessful candidates larger; this
is a bounded search cost, not another acceptance authority. Keep this baseline
for later shared-synthesis cleanup rather than claiming the change is free.
