# Current Integration Suite Acc and Motive Regression Audit

Date: 2026-08-28 06:56:28 JST

Status: complete reproduction and regression-boundary audit; no implementation
fix is included or authorized by this document

Baseline:

```text
8cff173b81d63e1df2c67801f43dc7ae1de5c7c2
correct issue 23 line accounting
```

Last directly verified good revision for both failures:

```text
e2ca3a5
fix prototype dependent function graph motives
```

First directly verified bad revision for both failures:

```text
952a073
complete dependent motive equation validation
```

## 1. Executive summary

The current `main` Integration Suite is not green. Running every integration
test with the official runner's `--keep-going` option gives:

```text
A_PROGRAM_TEST_SUMMARY 1 suite=integration
total=53 executed=53 passed=51 failed=2 skipped=0 wall_ms=34228
```

The failures are:

1. `test_checked_core_examples`
   - producer compilation succeeds;
   - accepted replay and artifact publication succeed in the ordinary indexed
     family suite;
   - the independent checked-Core pass rejects
     `explicit_index_family_acc_concrete_check.p` at a computation-fold
     occurrence.
2. `test_if8_fuel_free_quicksort`
   - the test fails before it reaches the QuickSort program;
   - its `source_equality` phase cannot compile `if8_order_check.p`;
   - reducing that fixture shows that the ordinary `LE` proof still compiles,
     while the `Acc`/`LT` `natAccessible` half does not reach a typing fixed
     point.

Both fixtures are unchanged over the audited regression range. Both pass at
`e2ca3a5` and fail at `952a073`. They therefore constitute regressions in the
implementation introduced by the dependent motive equation validation commit,
not failures introduced by newly added fixture text.

The two failures are related by indexed recursive elimination and motive/IH
classifier propagation, but they occur at different authority boundaries. They
must not be hidden behind one generic “QuickSort failed” description:

- failure A is a producer-versus-independent-checker disagreement;
- failure B is a producer fixed-point failure before accepted evidence exists.

Issue 23's focused tests remain green. Its generated Function Graph dependent
output motive, one-IH, two-IH, negative, artifact, and focused checked-Core
boundaries are not reproduced as failing by this audit.

## 2. Commands and test environment

The canonical nested checkout was fetched from `origin/main` before testing.
The real ripgrep binary was placed before the workspace compatibility wrapper
on `PATH`, because the wrapper does not implement the positive `--glob`
semantics required by some repository audit scripts.

The full-suite command was:

```sh
PATH=/path/to/real/ripgrep:$PATH \
sh src/prototype/tests/run_integration_suite.sh --keep-going \
    --timing-output /tmp/a-program-integration-timing.log
```

The normal Make target is fail-fast:

```sh
make -f src/prototype/Makefile test-integration
```

At this revision the fail-fast form reports only the first failure:

```text
total=53 executed=5 passed=4 failed=1 skipped=48
```

That summary is accurate for the fail-fast run but incomplete as a statement
about the whole suite. The `--keep-going` run is required to expose the second
failure and establish the 51/53 result.

## 3. Full-suite result

| Test | Status | Wall time | Failure phase |
| --- | ---: | ---: | --- |
| `test_checked_core_examples` | fail | 262 ms | `execute` |
| `test_if8_fuel_free_quicksort` | fail | 57 ms | `source_equality` |
| All other 51 integration tests | pass | — | — |

Relevant passing controls from the same keep-going run are:

| Control | Status | Wall time | Meaning |
| --- | ---: | ---: | --- |
| `test_explicit_index_family_surface` | pass | 919 ms | Producer, accepted replay, artifact, and IADT surface coverage remain operational |
| `test_issue_23_dependent_motive` | pass | 1,817 ms | Issue 23 focused one-IH, two-IH, negative, artifact, and checked-Core matrix passes |
| `test_function_graph_certified_execution` | pass | 8,635 ms | General Function Graph certified execution passes |
| `quicksort_dependency_closure` phase | pass | 7,379 ms | Generated QuickSort graph dependency closure passes |
| `quicksort_two_recursive_ih` phase | pass | 1,242 ms | Output-indexed property consumes both generated recursive IHs |

This matrix is important. It rules out the following overbroad conclusions:

- indexed families are not universally broken;
- all `Acc` producer compilation is not broken;
- handwritten QuickSort itself is not broken;
- Issue 23's generated Function Graph boundary is not generally broken;
- all checked-Core Function Graph checking is not broken.

## 4. Regression range

The two failing commands were executed at the relevant revisions.

| Revision | `if8_order_check.p` producer | `test_checked_core_examples` |
| --- | ---: | ---: |
| `e2ca3a5` | pass | pass |
| `952a073` | fail | fail |
| `8cff173` | fail | fail |

The first bad revision is therefore `952a073` within the tested linear history.

The commit changes motive equation validation, exact recursive-IH provenance,
dependent-classifier invalidation, branch-reindex caching, use-site effect-row
instantiation, and the shared computation-fold rule. The audit does not claim
that every one of those changes is wrong. It establishes only that the two
regressions enter at that combined commit and identifies the narrower semantic
boundaries below.

## 5. Failure A: independent checked-Core rejects a producer-accepted Acc

### 5.1 Reproducer

```sh
sh src/prototype/tests/integration/test_checked_core_examples.sh
```

The focused fixture is:

```text
src/prototype/tests/fixtures/typing/
    explicit_index_family_acc_concrete_check.p
```

Its essential declarations are:

```a-program
Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
    acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

accElim := \A : @ => \R : A -> A -> @ => \P : A -> @ =>
    \step : (x : A) -> ((y : A) -> R y x -> P y) -> P x =>
    \subject : A => \proof : Acc A R subject =>
        proof @acc x down => { stepAtX := step x; stepAtX *down; };
```

This is a general `Acc` eliminator. The recursive field `down` is lifted to the
induction hypothesis `*down`, and the result is passed to `stepAtX` through a
computation block.

### 5.2 Actual result

The test compiles its C checker successfully, then fails while independently
checking the source-produced semantic module:

```text
explicit_index_family_acc_concrete_check.p:
checker stopped status=3 reason=4 subject=19 kind=14
core=189:30 classifier=310:24
origin-core=189 source-core=219:30 source-origin=219:30
report-term=26 role=0 evidence=1 wrapped=4294967295 effort=89
```

The reported occurrence is a computation fold. Its source children are the
application that computes `stepAtX` and the continuation lambda that consumes
the lifted recursive result.

### 5.3 Boundary matrix

| Stage | Result |
| --- | ---: |
| Surface parse and producer elaboration | pass |
| Producer classifier fixed point | pass |
| Accepted P0 replay | pass |
| v86 artifact write/read in IADT surface suite | pass |
| Independent checked-Core | fail |

`test_explicit_index_family_surface` includes this same fixture, writes and
reads its artifact, and passes. Therefore the failure is not “Acc cannot be
constructed” and not “the producer cannot replay its own evidence.” It is a
disagreement between the produced semantic occurrence/classifier graph and the
independent checked-Core rule reconstruction.

The checker must not be weakened merely to agree with the producer. Either the
producer now emits a classifier/evidence combination outside the checked-Core
calculus, or checked-Core lacks a valid instantiation rule needed for the new
producer representation.

### 5.4 Confirmed semantic delta

The source dump was compared between the last good and first bad revisions.
The typed occurrence count remains 53, but the recursive IH classifier changes.

At `e2ca3a5`, occurrence 12 has an outer empty computation effect and the
recursive `P y` result is also closed with an empty effect row.

At `952a073`, the corresponding recursive `P y` computation retains an
`EFFECT_ROW_VAR(27)` inside the lifted IH classifier. The outer occurrence is
still presented as an empty-effect computation.

In schematic form:

```text
good:
  IH : Comp({}, Thunk(... Comp({}, P y, TOTAL) ...), TOTAL)

bad/checker-rejected:
  IH : Comp({}, Thunk(... Comp(?e27, P y, TOTAL) ...), TOTAL)
```

Term counts also increase from 759 to 971, showing that the new motive/effect
projection materializes a substantially different classifier graph rather
than merely renumbering the same graph.

Commit `952a073` adds use-site effect-row instantiation to
`prototype_judgement_computation_fold_result_classifier`. Nevertheless, the
independent checker still rejects this concrete occurrence. The remaining
question is therefore not whether an instantiation helper exists, but whether:

1. the generalized IH classifier is valid at this source occurrence;
2. the computation-fold continuation presents the corresponding expected row
   scheme to checked-Core;
3. checked-Core has the same binder/row scope needed to instantiate it;
4. the producer has incorrectly retained a solver-local row variable in the
   frozen semantic occurrence.

This audit confirms the representation mismatch but does not choose between
those four explanations without a checked-Core rule trace.

### 5.5 Required diagnostic improvement

The current report exposes IDs, tags, and child Terms, which is useful, but it
does not print:

- the exact computation-fold domain expected by the continuation;
- the actual input value classifier after row instantiation;
- the effect-row binder or scope that owns `?e27`;
- the conversion result before and after attempted use-site instantiation.

Those values should be available in a focused diagnostic mode before changing
the rule.

## 6. Failure B: nested Acc/LT proof does not reach a producer fixed point

### 6.1 Reproducer

```sh
./read_file.out \
    src/prototype/tests/fixtures/typing/if8_order_check.p
```

Actual result under `strict`, `hybrid`, and `exploratory` policies:

```text
typing fixed point: classifier inference failed status=-1
compile pending failed: infer pending types
if8_order_check.p:0:0: failed to compile AST graph
```

Changing the policy does not admit or residualize the program. The failure is
an internal solver failure with no source-local terminal diagnostic.

### 6.2 Why the test name is misleading

`test_if8_fuel_free_quicksort.sh` begins its `source_equality` phase with:

```sh
./read_file.out "$order"
```

Only after that command succeeds does it compile and compare the fuel-free
QuickSort fixture. The current test therefore fails before evaluating
QuickSort.

The Integration Suite label must not be summarized as “handwritten QuickSort
does not compile.” The failing control is the separate order/accessibility
proof.

### 6.3 Reduction

`if8_order_check.p` was mechanically separated into two temporary programs.

The first contains:

- `Nat`;
- indexed `LE`;
- structurally recursive `leRefl`.

It passes.

The second contains:

- `Nat`;
- indexed `LT`;
- indexed `Acc`;
- recursive `natAccessible`;
- the concrete `main` application.

It fails with the same fixed-point error.

Therefore the failure is not general indexed-family formation and is not the
ordinary unary `Nat` induction used by `leRefl`. It is localized to the nested
`Acc`/`LT` elimination and recursive-IH classifier interaction.

The essential recursive branch is:

```a-program
@succ k =>
    (Acc Nat LT).acc (Nat.succ k)
        &(\y : Nat => \edge : LT y (Nat.succ k) =>
            edge
                @step n => *k
                @weakenRight m n prior => *prior)
```

There are two distinct recursive eliminations and IH owners in this branch:

1. `*k`, owned by the outer Match over `n`;
2. `*prior`, owned by the inner Match over the `LT` evidence `edge`.

The `952a073` work explicitly changes recursive equation provenance to be
indexed by exact IH occurrence and changes invalidation/recomputation of
downstream classifiers. The reduced reproducer shows that nested independent
IH owners still form a non-converging or invalid solver state in this shape.

This is a stronger localization than “motive inference failed,” but the
current generic `status=-1` output is insufficient to identify which motive
equation, projection, or invalidation edge causes the terminal failure.

### 6.4 Required diagnostic improvement

When the fixed point terminates with `status=-1`, the compiler should report at
least:

- Match occurrence and source span;
- motive solution state;
- exact IH occurrence and owner Match;
- recursive-equation owner and revision;
- projected motive ID;
- classifier before and after invalidation;
- constraint that returned INVALID rather than PENDING;
- whether the failure was correspondence, guardedness, reindexing, effect-row
  instantiation, or conversion.

The current `0:0` diagnostic prevents a source-level user from distinguishing
an invalid proof from a compiler fixed-point defect.

## 7. Handwritten QuickSort remains operational

The actual handwritten QuickSort fixture is:

```text
src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
```

It defines in A Program source:

- `LT` and `Acc`;
- `SizedList` and `Measured`;
- size-bounded `Partition` evidence;
- `partition`, `measure`, and `append`;
- `quickSortAcc` using two `down` calls with strict size witnesses;
- the public `quickSort` wrapper;
- `quickSortTerminates`.

The fixture was compiled directly at `8cff173`, independently of the failing
group script. The following all pass:

```text
main           == expected
emptyMain      == emptyExpected
singletonMain  == singletonExpected
ascendingMain  == ascendingExpected
descendingMain == descendingExpected
duplicateMain  == duplicateExpected
```

Artifact write/read also succeeds and publishes both `quickSort` and
`quickSortTerminates`.

The Function Graph certified execution suite independently compiles this
QuickSort source, generates its Function Graph, verifies dependency closure,
and passes. The Issue 23 suite also constructs an output-indexed structural
property that consumes both lower and upper recursive IHs.

Consequently, failure B blocks one integration test group and one reusable
`natAccessible` proof style, but it is not evidence that A Program cannot
currently express or execute handwritten functional QuickSort.

## 8. Relationship to Issue 23

Issue 23 corrected a real false rejection:

```text
generated Function Graph
+ output-dependent motive
+ recursive generated IH
```

The focused correction remains verified at `8cff173`:

```text
checked_core                 pass
generated_dependent_motive   pass
indexed_controls             pass
quicksort_two_recursive_ih   pass
recursive_rejections         pass
authority_boundary           pass
```

The present regressions were introduced while strengthening the same motive
equation machinery. Fixing them must preserve the Issue 23 acceptance and
rejection matrix. Reverting all of `952a073`, restoring the guardedness-as-DefEq
shortcut, or returning to one last-writer recursive IH slot would reopen Issue
23 and is not an acceptable correction.

## 9. Required correction boundaries

The implementation may choose different mechanisms, but it must satisfy all of
the following.

### 9.1 Producer convergence

- Nested Matches with distinct IH owners must not alias recursive-equation
  ownership.
- Projecting an owner motive to one IH may invalidate its dependents, but must
  not indefinitely invalidate an unrelated enclosing or nested motive.
- INVALID and PENDING must remain distinguishable; a temporarily unavailable
  projection must not become an internal fatal error.
- A valid `natAccessible` proof must compile under strict policy without using
  `::` as a motive authority.

### 9.2 Checked-Core agreement

- The producer must not freeze solver-local effect-row variables without a
  semantic binder and instantiation path.
- If generalized IH effect rows are part of the accepted calculus, checked-Core
  must reconstruct their use-site instantiation from semantic input rather than
  trusting producer evidence.
- If the generalized classifier is not part of the calculus, the producer must
  close or instantiate it before freezing the occurrence.
- Accepted replay must not be weakened to hide a checked-Core disagreement.

### 9.3 Existing Issue 23 behavior

- Generated output-dependent one-IH and two-IH Function Graph proofs continue
  to pass.
- Incompatible recursive properties continue to fail with
  `motive-equation-mismatch` before accepted replay.
- Foreign and unguarded IHs remain rejected.
- Multiple recursive IH equations remain per-occurrence rather than
  last-writer state.

## 10. Acceptance tests

### Failure A

- [ ] `test_checked_core_examples.sh` passes all existing fixtures.
- [ ] `explicit_index_family_acc_concrete_check.p` passes independent
      checked-Core without an admitted fallback.
- [ ] A negative fixture with an incompatible computation-fold input effect row
      is rejected by checked-Core.
- [ ] A mutation that removes the owner of a generalized row variable is
      rejected.
- [ ] Producer/P0 and checked-Core independently compute compatible fold input
      classifiers.

### Failure B

- [ ] `if8_order_check.p` compiles under strict policy.
- [ ] Its `LE` and reduced `Acc` halves both have permanent focused coverage.
- [ ] Nested outer `*k` and inner `*prior` retain distinct IH owners.
- [ ] Swapping either owner or argument is rejected.
- [ ] A solver failure reports a source span and motive-equation diagnostic,
      not only `0:0` and `status=-1`.

### Whole-system regression matrix

- [ ] `run_integration_suite.sh --keep-going` reports 53/53 pass.
- [ ] `test_issue_23_dependent_motive.sh` remains green.
- [ ] `test_function_graph_certified_execution.sh` remains green.
- [ ] `test_explicit_index_family_surface.sh` remains green.
- [ ] Direct handwritten QuickSort source and artifact normalization checks
      remain green.

## 11. Non-fixes

The following actions would hide one symptom while violating an established
boundary:

- removing `if8_order_check.p` from the QuickSort integration group;
- marking the two tests expected-failure;
- weakening checked-Core because producer replay accepts the artifact;
- weakening accepted replay;
- treating guarded IH ownership as ordinary classifier equality;
- forcing every Match motive to be constant;
- reverting per-occurrence recursive equation provenance to a single slot;
- special-casing `Acc`, `Nat`, `LT`, or QuickSort;
- increasing solver effort without proving that the fixed point is monotone;
- calling the handwritten QuickSort implementation broken when its direct
  fixture and Function Graph controls pass.

## 12. Recommended implementation order

1. Add a focused solver diagnostic for the reduced `Acc`/`LT`
   `natAccessible` fixture.
2. Trace recursive-equation owner, projected motive, classifier invalidation,
   and revision changes until the first INVALID result at `952a073`.
3. Add a focused checked-Core trace for the Acc computation fold, including
   expected and actual effect rows before and after instantiation.
4. Decide whether the producer's generalized IH row is valid frozen semantics
   or leaked solver state.
5. Correct the producer convergence and checker agreement paths separately.
6. Run the complete 53-test keep-going suite and all Issue 23 controls before
   merging.

## 13. Conclusion

Current `main` has two concrete Integration Suite regressions, both first
observed at `952a073` and both involving indexed recursive elimination after
the dependent motive equation strengthening.

They are not one undifferentiated QuickSort failure:

- one is an independent-checker disagreement over a generalized recursive IH
  classifier in an Acc computation fold;
- the other is a producer fixed-point failure for nested, separately owned
  recursive IHs in an Acc/LT accessibility proof.

At the same time, handwritten fuel-free QuickSort, generated QuickSort Function
Graphs, and Issue 23's output-dependent two-IH proof boundary continue to work.
The correction must recover 53/53 integration success without sacrificing
those newly established dependent-motive guarantees.
