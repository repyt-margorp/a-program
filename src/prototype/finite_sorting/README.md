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
- `cases.p`: shared consumers, empty/singleton/reversed/ordered inputs and
  different labels with equal keys. QuickSort and insertion need not be stable
  or return the same order of equal keys. Their expected labels differ.
- `stress-value.p`, `stress-wrong-function.p`: incomplete conversion diagnostics,
  appended after the provider and cases. They are not passing rejection tests.

Run from the repository root, using matching baseline binaries:

```sh
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test lists
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test source
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test quick
bash src/prototype/finite_sorting/check.sh BUILD/pointer-check BUILD/program_test insertion
bash src/prototype/finite_sorting/merge-check.sh BUILD/pointer-check BUILD/program_test lists
```

The omitted mode is `all`, which must pass before claiming completion. `source`
checks synthesis without assertions, rejection and ordinary/retained images;
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

Known gaps: full Quick observations remain pending at 100 million steps. Fully
retained source exceeds the CLI's million-record limit. Therefore `all` is **not
passing**. Tree/Bubble are not connected, and the same-domain negative and
concrete value-transport stress fixtures still need verification.

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
