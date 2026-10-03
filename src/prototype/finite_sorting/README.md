# Common Sorting Proof Prototype

Baseline: `7160cbe`, 2026-09-28. Progress and design provenance belong to
[F3/F4](../../../doc/2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md).
This directory is experimental, not part of the accepted `check-acceptance` gate.

Current candidate: [explicit image bound](../image_limit/README.md), based on
`cf1081e`, passes all five backend gates. Quick/Insertion `all` also passes
ASan/UBSan. Use the composed candidate's binaries and the explicit bound below;
the default remains unchanged and retained typing results are not trusted.

```sh
SORTING_IMAGE_LIMIT=3000000 bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/finite_sort_image_compare all
```

- `common.p`: an ordinary sorting function with Local and permutation theorems;
  derived Strong, Fin occurrence correspondence and same-length Vec rebuilding.
- `quick.p`: connection to the existing ordinary QuickSort, not a proof-result
  surrogate or a new algorithm.
- `insertion.p`: general Local/permutation proofs of the existing insertion
  functions. Only directional comparator evidence is required for Local.
- `merge.p`: generic `merge_sort_fuel_by`, `merge_sort_by` and their
  Local/permutation proofs. `merge_backend A R le decide` connects arbitrary
  payload types to the same List/Vec/Fin contract as Quick/Insertion. Nat indexes
  lengths and fuel only; the internal fuel bound follows from `measure A xs`.
  Local requires directional comparison evidence, not transitivity; Strong
  uses the common wrapper with explicit transitivity. The repeated-insertion
  merge and alternating split are preserved, not replaced by two-front merging.
  Accepted `mergeSort`/`mergeSortFuel` remain unchanged Nat-specific fixtures.
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
bash src/prototype/finite_sorting/merge-generic-check.sh BUILD/pointer-check BUILD/program_test
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
All backend gates share status/image checks through `check-functions.sh`;
callers retain their budgets, optional reader bounds and fixture selection.
The complete Bubble gate passes on the closed-readback candidate under O2
(204 s) and ASan/UBSan (553 s, leak checking and halt-on-error enabled).
The shared Tree runner also passes again (175 s).

## Generic Merge and Interface Recheck, 2026-10-01

At baseline `2a0ba449` plus the source-library patch, the Merge prototype now
accepts an arbitrary payload type. Fuel and length still use Nat:

```text
merge_sort_by :: (A:@)->(A->A->Bool)->List A->List A;
merge_backend :: (A:@)->(R:A->A->@)->(le:A->A->Bool)->
    ((x:A)->(y:A)->general_decision A R x y (le x y))->sorting_backend A R;
```

Call `merge_backend Bool &bool_order &bool_le &bool_decide`, then use the existing
`sorting_run`, `sorting_content`, `sorting_positions`, `sorting_vector` and
`sorting_vector_value`. No new Fin/permutation specification is introduced.
The prototype proof/fit helpers also take `A` as their first argument. Old Nat
clients migrate by adding `Nat`; the accepted `mergeSort` and `mergeSortFuel`
definitions/signatures remain untouched for existing consumers.

| Backend | Payload type | Local proof requirement |
| --- | --- | --- |
| Quick | Arbitrary A | Directional comparator evidence |
| Insertion | Arbitrary A | Directional comparator evidence |
| Tree | Arbitrary A | Directional comparator evidence |
| Merge | Arbitrary A in this prototype | Directional comparator evidence |
| Bubble | Arbitrary A | Directional evidence and explicit transitivity |

All Strong wrappers use explicit transitivity. Accepted `insertionSort` is a
Nat/default-comparator convenience specialization of `insertionSortBy`; its
generic backend already exists. There is no corresponding fix needed there.

Fresh strict-O2 checks on the assembled latest solver-input candidate pass:

```sh
bash src/prototype/finite_sorting/merge-check.sh BUILD/pointer-check BUILD/program_test views
bash src/prototype/finite_sorting/merge-generic-check.sh BUILD/pointer-check BUILD/program_test
bash src/prototype/finite_sorting/generic-interfaces-check.sh BUILD/pointer-check
```

The first gate additionally compares all five existing Nat samples with the
unchanged accepted algorithm. The second checks open-input actual-result
theorems, Bool cases, distinct labelled equal-key payloads, List/Vec output,
Fin origins/value transport/inverses, six rejection controls, image reload and
100/0-step checkpoint resave/resume. Comparisons run at chunks 1 and 64. The
third checks all five generic backend classifiers. All three include synthesis
with assertions removed. The two new gates are also registered in the
prototype `check-sorting-backends` aggregate; that full aggregate was not rerun.

This remains a repeated-insertion MergeSort, not a new two-front merge or a
stability/runtime-complexity proof. Strict-O2 prototype verification is not
accepted compiler/library adoption. No full acceptance/sanitizer/C-lowering
claim accompanies this source-only change. Related issue: #41, partial library
scope only; F5 integration and semantic persistence remain separate.

## Finite Function API, 2026-10-03

The prototype `finite-functions.p` fills #41/F3-F4 common API coverage for
`Fin n -> A`. It uses `vec_tabulate`, the existing `sorting_vector` ordinary
backend result and `vec_lookup`; no sorting implementation or position algebra
is duplicated. Append this module to `provider.sh` along with a selected backend.

```text
sorting_function A R backend n values i
sorting_function_positions A R backend n values
sorting_function_restore A R backend n values original_position
```

`sorting_function_value` transports an explicit predicate from a sorted entry
to its original entry selected by `position_forward`; `_value_back` transports
in the reverse direction. `_restore` reads the sorted entry at
`position_backward original_position`. `_recover` and `_recover_back` transport
predicates between that restored entry and the original entry at the supplied
position. Both inverse maps refer to labelled positions even if keys or full
values coincide; these laws do not assert stability or object Identity.
`sorting_value_back` and `sorting_vector_value_back` expose the corresponding
existing reverse observation through the common List/Vec API.

`sorting_function_contents` exposes the same sorted Vec's List traversal used
by `sorting_function` lookup. `_content` and `_local` certify that traversal;
`_strong` takes explicit transitivity. There is no automatic Strong conversion
or function extensionality assertion. The ordinary-result contract and backend
requirements, including Bubble's explicit transitivity, are preserved.

The focused gate uses the existing checker and typed image comparator, with
sequential correctness work and the existing 40M fuel/180-second command bounds:

```sh
bash src/prototype/finite_sorting/finite-functions-check.sh BUILD/pointer-check BUILD/program_test
bash src/prototype/finite_sorting/finite-functions-duplicates-check.sh BUILD/pointer-check BUILD/program_test
```

It checks open A/n/backend/input laws and independent synthesis, all five actual
labelled outputs, forward origins, backward destinations and original-order
recovery. Different labels share keys; Insertion/Tree/Bubble exercise a
non-self-inverse position cycle so substituting forward for backward is rejected.
Empty/singleton observations, wrong domain/bound/labels, Local-as-Strong and
lost-duplicate controls, ordinary load, 100-step pending/inert byte-resave/resume,
resumed actual-result comparison and persisted semantic rejection are included.
The supplemental duplicate gate observes identical full payloads across all
five backends, including their separate forward origins and backward destinations,
checks both inverse-law consumers, rejects collapsed occurrence positions and a
dropped identical duplicate, and repeats fresh-process ordinary/resumed result
checks for this extended fixture.
Qualification status and exact byte/binary hashes belong to the owning SOAP
checkpoint and `verification/finite-functions-e6.json` after the complete gate.
This remains prototype library coverage; accepted-build F5 integration, compiler
acceptance, sanitizers, native lowering and runtime-cost claims are separate.

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
