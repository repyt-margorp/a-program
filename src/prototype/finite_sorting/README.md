# Common Sorting Proof Prototype

Baseline: `7160cbe`, 2026-09-28. Progress and design provenance belong to
[F3/F4](../../../doc/2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md).
This directory is experimental, not part of the accepted `check-acceptance` gate.

- `common.p`: an ordinary sorting function with Local and permutation theorems;
  derived Strong, Fin occurrence correspondence and same-length Vec rebuilding.
- `quick.p`: connection to the existing ordinary QuickSort, not a proof-result
  surrogate or a new algorithm.
- `insertion.p`: general Local/permutation proofs of the existing insertion
  functions. Only directional comparator evidence is required for Local.
- `merge.p`: Local/permutation proofs for the unchanged, Nat-specific legacy
  MergeSort. The relation is arbitrary; Local requires directional comparison
  evidence, not transitivity. Its internal fuel bound follows from `measure`.
  This does not generalize the algorithm's element type or replace its merge.
- `tree.p`: generic Local/permutation proofs for the unchanged `treeSort`.
  A single `tree_all` predicate handles both bound directions; flattening reuses
  the existing List proofs. Local does not require transitivity. Strong uses
  the common wrapper and its explicit transitivity argument.
- `bubble.p`: right-to-left adjacent-pass BubbleSort using `SizedList` length
  decrease, with content preservation for arbitrary comparators. No fuel cutoff.
- `bubble-order.p`: Strong and a common backend with **explicit transitivity**.
  The algorithm itself accepts arbitrary comparators.
- `bubble-directional-counterexample.p`: internally refutes Local for one valid
  directional comparator and the actual Bubble result. Directional evidence
  alone is insufficient; transitivity is sufficient, not claimed necessary.
- `cases.p`: shared consumers, empty/singleton/reversed/ordered inputs and
  different labels with equal keys. QuickSort and insertion need not be stable
  or return the same order of equal keys. Their expected labels differ.
- `stress-value.p`: concrete-value conversion diagnostic, appended after cases.
- `stress-wrong-function.p`: same-domain negative, now included in `check.sh`;
  passing rejection requires the conversion-head candidate below.

Run from the repository root, using binaries from the same build. Results below
distinguish the accepted baseline from the isolated closed-readback candidate:

```sh
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test lists
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test source
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test quick
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test insertion
bash src/prototype/finite_sorting/merge-check.sh BUILD/pointer-check BUILD/program_test lists
bash src/prototype/finite_sorting/value-check.sh BUILD/pointer-check
bash src/prototype/finite_sorting/tree-check.sh BUILD/pointer-check BUILD/finite_sort_image_compare
bash src/prototype/finite_sorting/bubble-check.sh BUILD/pointer-check BUILD/finite_sort_image_compare
```

For `check.sh`, the omitted mode is `all`, which must pass before claiming
completion. `source` checks synthesis without assertions, rejection and
ordinary/retained images;
`lists` observes ordinary algorithm outputs; `quick`/`insertion` also evaluate
Vec rebuilding and labelled Fin origins. Commands have a 180-second deadline
and a default 40-million-step budget (`SORTING_CHECK_STEPS` can override it).
Each failed gate returns nonzero; a timeout or pending comparison is not an
accepted equality or a rejection proof.

Insertion's full Vec/Fin report passes at 32,348,238 total transitions. Legacy
MergeSort's open proofs, assertion-free synthesis, ordinary List outputs,
negative cases and ordinary-image resume pass. `merge-check.sh ... views` adds
its Vec/Fin observations. Vec remains pending at 40 million transitions; the
following Fin-origin gate has not been reached.

Accepted baseline gaps: full Quick observations remain pending at 100 million
steps. Fully retained source exceeds the CLI's million-record limit. Therefore
`all` is **not passing**. The same-domain negative and baseline concrete
value-transport stress fixtures still need work. Do not pursue Bubble's
unconditional Local theorem: the checked counterexample disproves it.

The [closed-readback prototype](../readback_support/README.md) now completes
Quick and legacy Merge Vec/Fin observations and concrete value transport. This
is not yet an accepted compiler change. The same-domain wrong-function check
is still pending at 80M with that patch alone. The separate
[conversion-head candidate](../conversion_head/README.md) rejects it at 5,816,830
steps and verifies ordinary/retained partial-image resume. The default CLI
retained-image limit still fails; `all` is not yet passing.
The combined candidate passes full compiler `check-acceptance`, Quick/Insertion
observations, Tree/Bubble full gates and legacy Merge's `views` gate. Its fresh
`all` run passes all earlier checks and stops specifically at retained CLI load.
The active plan distinguishes these candidate results from the baseline above.
`value-check.sh` verifies positive specialization, synthesis without `::`, and
rejection of wrong labels. The candidate passes all four checks. The retained
API gate accepts an optional `all` mode to observe both Quick and Insertion.

The Tree source proofs also pass on the accepted baseline (13,294,981 steps;
4,965,391 without assertions). The closed-readback candidate passes the full
`tree-check.sh` gate: generic and assertion-free synthesis, labelled List/Vec/Fin
observations, wrong comparator/content/value rejection, and ordinary/retained
images with zero/100-step resume. Retained checks use the explicit bounded API,
not a relaxed CLI default. The existing cyclic relation fixture additionally
checks Local without transitivity; treating that proof as Strong is rejected.
This is not a formal uninhabitance proof for every possible Strong term.
`tree-cases.p` shares the other backends' cases rather than duplicating them.
The full Tree gate passes in O2 (170 s) and ASan/UBSan (464 s, leak checking and
halt-on-error enabled). No accepted compiler change is part of this addition.

Bubble's gate additionally checks a single pass that does **not** sort the full
input, an incorrect transitivity argument and a mismatched `SizedList` length.
The cyclic-relation example exercises arbitrary-comparator content and one
closed Local certificate; it is not a proof of general nontransitive Local.
The directional counterexample proves `Local actual_result -> empty_type` for
`[a,c,b]`, with all nine comparator decisions checked. Here both comparisons
between `b,c` answer false, which the directional contract permits. Independent
synthesis, actual result comparison and wrong-edge rejection are also checked.
Tree and Bubble source `check-functions.sh` to share status/image checks.
The complete Bubble gate passes on the closed-readback candidate under O2
(204 s) and ASan/UBSan (553 s, leak checking and halt-on-error enabled).
The shared Tree runner also passes again (175 s).

## Bounded Image and Cost Diagnostics

```sh
make -f src/prototype/finite_sorting/probe.mk BUILD=/tmp/sort-probe
make -f src/prototype/finite_sorting/probe.mk BUILD=/tmp/sort-probe /tmp/sort-probe/finite_sort_image_compare
bash src/prototype/finite_sorting/retained-check.sh BUILD/pointer-check /tmp/sort-probe/finite_sort_image_compare
/tmp/sort-probe/finite_sort_probe INPUT.a insertion_report insertion_report_expected 20000000 20000000 3000000
```

The image comparator reuses `tests/program.c`'s typed comparison unchanged, with
an explicit caller-selected reader limit. Three million records suffice for the
current retained fixture; `SORTING_IMAGE_LIMIT` configures the test limit. This
does not change the CLI default, trust serialized proofs, or skip resynthesis.
The probe separates source synthesis, Core NF and typed normalization, sampling
the existing evaluator's readback phases. It is a cost diagnostic, **not** an
alternative typed acceptance gate. `provider.sh` only assembles source fixtures.
