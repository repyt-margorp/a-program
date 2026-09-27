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
```

The omitted mode is `all`, which must pass before claiming completion. `source`
checks synthesis without assertions, rejection and ordinary/retained images;
`lists` observes ordinary algorithm outputs; `quick`/`insertion` also evaluate
Vec rebuilding and labelled Fin origins. Commands have a 180-second deadline
and a default 20-million-step budget (`SORTING_CHECK_STEPS` can override it).
Each failed gate returns nonzero; a timeout or pending comparison is not an
accepted equality or a rejection proof.

Known gaps: full Quick observations remain pending at 100 million steps, full
Insertion observations at 20 million. Fully retained source with all post-checks
exceeds the CLI's million-record reader limit. Therefore `all` is **not passing**.
General source theorems and ordinary image loading have passed, but this is not
yet an end-to-end verified finite sorting API. Merge/Tree/Bubble are not connected.
