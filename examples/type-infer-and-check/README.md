# Type Inference and Checking Examples

This directory is an executable specification of the currently accepted type
inference and checking boundary. `manifest.tsv` is authoritative. The
integration runner rejects duplicate entries, missing files, unlisted `.p`
files, unexpected exit status, unstable diagnostic categories, missing source
locations, and failed artifact replay.

The levels describe language features, not separate compiler modes:

- `level0`: Universe and Pi formation;
- `level1`: finite and parameterized non-recursive ADTs;
- `level2`: structural recursion and induction;
- `level3`: indexed inductive families with explicit parameter/index binders.

<!-- BEGIN MANIFEST STATUS -->
| File | Description | Expected result | Diagnostic | Runtime | Artifact replay |
| --- | --- | --- | --- | --- | --- |
| `level0/00_universe.p` | Universe and type-level identity | pass | `-` | checked | yes |
| `level0/01_function.p` | Function types and type-level composition | pass | `-` | checked | yes |
| `level0/02_dependent.p` | Dependent Pi types | pass | `-` | checked | yes |
| `level1/00_bool.p` | Boolean elimination | pass | `-` | checked | yes |
| `level1/01_unit.p` | Unit elimination | pass | `-` | checked | yes |
| `level1/02_sum.p` | Parameterized sum type | pass | `-` | checked | yes |
| `level1/03_product.p` | Parameterized product type | pass | `-` | checked | yes |
| `level1/04_maybe.p` | Parameterized optional type | pass | `-` | checked | yes |
| `level2/00_nat.p` | Natural-number structural induction | pass | `-` | checked | yes |
| `level2/01_list.p` | List recursion and append | pass | `-` | checked | yes |
| `level2/02_tree.p` | Binary-tree recursion | pass | `-` | checked | yes |
| `level2/03_rose.p` | Nested recursive occurrence through List | expected failure | `unsupported-nested-recursion` | - | no |
| `level3/00_vec.p` | Length-indexed vectors | pass | `-` | checked | yes |
| `level3/01_fin.p` | Finite sets indexed by Nat | pass | `-` | checked | yes |
| `level3/02_eq.p` | Indexed propositional equality declaration | pass | `-` | checked | yes |
| `level3/03_matrix.p` | Matrices with multiple indices | pass | `-` | checked | yes |
<!-- END MANIFEST STATUS -->

Run the suite from the repository root:

```sh
make -f src/prototype/Makefile test-type-infer-and-check
```

Run one source file directly:

```sh
make -f src/prototype/Makefile reader
./read_file.out examples/type-infer-and-check/level1/00_bool.p
```

Passing rows compile from source, publish an artifact, and read the artifact
graph back through the current wire contract. Expected failures must reach the
named unsupported boundary and include a source location; they are not treated
as successful feature tests.

Algorithms that close cross-cutting compiler issues remain focused fixtures
rather than being duplicated in this 16-file progression:

- `src/prototype/tests/fixtures/typing/host_expression_evaluator_check.p`
  checks Host fields, recursive motives, accepted evidence, and artifact replay;
- `src/prototype/tests/fixtures/typing/recursive_ih_field_identity_check.p`
  checks exact Match/case/field induction identity;
- `src/prototype/tests/fixtures/typing/list_map_induction_check.p` and
  `src/prototype/tests/fixtures/effects/list_map_effect_check.p` check pure and
  effectful higher-order List mapping; and
- `src/prototype/tests/fixtures/typing/insertion_sort_check.p` checks nested
  recursive comparison, branch-sensitive insertion, and complete sorting.

They are run by the normal `test-integration` target alongside the manifest.
