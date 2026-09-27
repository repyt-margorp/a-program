# Finite-Position Sorting

Date: 2026-09-27
Baseline: `ebe0648` (schema owner `fb15003`, documentation PR #42 merged).
Tracking: [#41](https://github.com/repyt-margorp/a-program/issues/41).
This is the implementation work list for the sorting priority in the
[authority plan](2026-09-25-POST-SURFACE-ISSUE-AND-AUTHORITY-SOAP-PLAN.md).
The [imported audit](2026-09-27-GENERIC-CONTAINER-SORTING-AUDIT.md) is research,
not evidence that its proposed interfaces already work. Broad P4/P5 remain open.

## Problem List

| ID | Problem | State |
| --- | --- | --- |
| F0 | Import and critically assess #41 / PR #42 | Complete |
| F1 | Checked finite positions and bijections | Complete |
| F2 | Lawful List and indexed-container views | In progress; application-conversion gap reproduced |
| F3 | One ordinary-result sorting specification | Pending |
| F4 | Quick/Merge/Insertion/Bubble and additional backends | Pending |
| F5 | Permanent rejection, image and regression gates | Pending |

## F0. Scope and Invariants

### Subjective (User)

English paraphrase of 2026-09-27 conversation: prioritize the submitted Fin
sorting research, implement it in the surface language, and prove QuickSort,
MergeSort, InsertionSort and BubbleSort with a common method; check additional
straightforward algorithms. Do not change algorithms merely to obtain Local or
Strong sortedness. Those are separate post-hoc properties of the order relation.
The latest explicit invariant is that `a :: A` is only an assertion: synthesize
`a` independently, then check its classifier against `A`. Never use `A` to
invent an argument, motive or proof for `a`.

### Objective (Code)

Existing List proofs cover ordinary QuickSort results, Local/Strong sortedness
and an inductive permutation with nil/keep/swap/composition. The latter preserves
multiplicity, but is not itself an explicit bijection of labelled positions.
`src/program.c` does not expose the internal Identity library as generic source
Eq/refl/transport names. `src/synthesis.c:source_expect_step` and
`src/synthesis_conversion.c:expect_step` consume independently synthesized
inputs; Match discovery skips the assertion's right side.

PR #42 changes documentation only. Its merge automatically closed #41 despite
its explicit non-completion statement; #41 was reopened to track implementation.
The existing merge fixture uses repeated insertion, not linear two-front merge.

### Assessment

Agent decisions within the requested implementation scope:

- Use ordinary source IADTs, Pi, Match and explicit proofs. No Fin/Sort oracle,
  new keyword, Core tag, expected-type inference or privileged acceptance path.
- Fix permutation direction: **new position -> old position**. For input
  `values : Fin n -> A`, output at `i` is `values (forward i)`.
- Preserve labels even when their values coincide. A bijection has forward and
  backward functions and both pointwise inverse laws. Membership is insufficient.
- A source `same_position` relation over Fin can express these laws with
  ordinary indexed elimination. It is not a replacement for Higher Identity,
  equality reflection, proof irrelevance or a new universal Eq primitive.
- Fin's zero constructor needs an explicit bound: no field determines it.
  Successor can recover its bound from the recursive Fin field. A post-check
  must not fill the missing bound. Explicit Match motives belong to the term.
- Keep semantic views distinct from native random-access/cost capabilities.
  Do not assume arbitrary IADTs, heterogeneous positions, infinite containers,
  stability or asymptotic complexity are covered.

### Plan

- [x] Merge #42; preserve research provenance and reopen unfinished #41.
- [x] Check current contracts and the four primary references below.
- [ ] Reassess this plan only from concrete source/checker evidence; record any
  necessary compiler fix separately, without weakening a theorem or post-check.

## F1. Finite Positions and Bijections

### Subjective (User)

Finite sorting must be expressible and checkable in A Program itself.

### Objective (Code)

At `ebe0648` plus this source-library patch, `finite_positions.p` checks open
`n` proofs of identity/inverse/composition, fixed-head lifting and a self-inverse
first-two-position swap. General roundtrip theorems mention actual projections
of composed/lifted permutations; closed three-position cycles are also checked.
Neither kernel nor synthesis implementation changes. Focused checks pass in
optimized (4.186 s), Debug (5.142 s), and ASan/UBSan (21.783 s) builds, with leak
checks and halt-on-error enabled. These include ordinary/retained images and
negative controls; they are test wall times, not speed comparisons. Full
`check-acceptance` passes in **1,515.435 seconds** wall (1,408.055 user,
106.560 system), including 63/63 compatibility, generic/ordinary-result sorting,
Local/Strong, all LT variants, Fin and optional witness packets. The earlier
user-interruption run was terminated and is not counted as a pass. Sanitizer
build/check work overlapped this gate; its wall time is not a speedup claim.

### Assessment

Define the small position algebra first, so later sorting proofs can reuse it
without reconstructing bounds or introducing a second proof authority.

### Plan

- [x] Add `tests/fixtures/finite_positions.p`: Fin, observation, weakening,
  same-position elimination, bijection identity/inverse/composition and action.
- [x] Check these laws for open `n`, not just small closed enumerations.
- [x] Add fixed-head lifting, a nontrivial swap/cycle and a duplicate-value/label example.
- [x] Reject wrong bounds, missing bounds and non-bijections. A motive-free
  relation eliminator synthesizes, but adding the desired symmetry post-check
  rejects it; the assertion cannot replace the inferred motive. The explicitly
  annotated symmetry theorem passes.

Source-trial decision: a function projection already returns a suspended
function value. Adding `&` to that projection wrapped the returning computation
again and failed its argument contract. Use its ordinary result when passing it
to `position_keep_map`; no coercion rule or compiler relaxation was added.

## F2.1. Application Conversion Before View Proofs

### Subjective (User)

Preserve independent synthesis and the post-check-only meaning of `::`. Reuse
the existing typed structure and checking mechanisms rather than accumulating
special cases or extra authorities to make examples pass.

### Objective (Code)

The standalone diagnostic is
`src/prototype/family_domain_conversion.p`, excluded from the accepted build.
At `ebe0648`, an ordinary application accepts the synthesized result of
`shift (Nat.succ k) b`, whose classifier is `Box (pred (Nat.succ k))`, at domain
`Box k`. The control without the final definition passes in 1,928 steps.
Applying the indexed `Family k` to the same result is rejected in 2,424 steps.
This also fails without any `::` (2,305 steps); the assertion is not its cause.

`src/synthesis.c:prepare_application` sends logical-family arguments directly
to `PG_TYPE_FAMILY_APP`. `src/evidence.c:family_application` correctly requires
alpha-equal domain/classifier inputs. Ordinary application first supplies checked
conversion through `pg_synthesis_expect`. The family path omits that step.
A debugger confirms both family/application premises are accepted before the
family rule rejects them. Do not weaken that kernel check into unchecked equality.

Separate temporary Vec trials check lookup, tabulation, tail, and a general
head-entry theorem about the ordinary lookup result. Full lookup coverage is
not checked. A wrapper experiment also reaches an implicit-constructor-index
rejection; it is not established that one conversion fix completes F2.

### Assessment

This is an application-elaboration completeness gap, not permission to infer an
argument from an expected type. The actual argument and family's domain are
already known. Their definitional equality must have an explicit checked
conversion, just as for ordinary application.

The domain must come from the selected typed family declaration, including its
scope/substitutions. A raw signature is not a substitute for that formation.
`pg_synthesis_application_domain` currently expects computation-Pi formation;
blindly calling it on a logical family is not a completed solution. Share the
typed declaration/domain inspection, without adding a value-side Pi tag or
reconstructing a second typing authority. Do not require user wrappers or eagerly
normalize every entire family telescope as a workaround.

### Plan

- [x] Preserve a minimal source diagnostic; distinguish it from an accepted test.
- [ ] Expose the next domain from the existing typed family declaration and
  scope action; retain the distinction between value and family parameters.
- [ ] Reuse checked conversion after independent argument synthesis, before
  ordinary `PG_TYPE_FAMILY_APP` admission. Keep `::` outside this path.
- [ ] Verify both directions of reducible classifiers, partial/dependent family
  applications, family-valued parameters, and wrong bound/nominal/effect rejection.
- [ ] Recheck the Vec coverage proof; diagnose any remaining index-recovery or
  motive failures separately instead of claiming this fix solves them all.
- [ ] Move the repaired diagnostic into permanent source/image/budget tests;
  run full gates and paired performance checks before publishing compiler changes.

## F2-F4. Views and Common Sorting Proofs

### Subjective (User)

Reuse one proof specification across the requested sorting algorithms; do not
silently replace their ordinary results with separately generated proof results.

### Objective (Code)

Existing proof providers are under `tests/fixtures/` and source theorems under
`tests/acceptance/`. `generic_sorted/content-result-proof.p` connects the
permutation theorem to ordinary QuickSort. No current theorem connects it to a
Fin permutation or proves a same-shape reconstruction interface.

### Assessment

The difficult obligation is the bridge, not creating a new record name. List
permutation must yield a length-preserving occurrence correspondence; enumeration
and lookup then establish a Fin bijection and its value/action equation.
List and an indexed homogeneous vector must share this theorem, with explicit
dependent transports where needed. Positivity alone is not a traversal law.
Keep Local plus permutation as a base contract; add transitivity to derive
Strong. Comparator directional correctness is still required, but antisymmetry
and stability are not universal requirements.

### Plan

- [ ] F2: implement finite enumeration/lookup and prove coverage, uniqueness,
  observation/reconstruction and shape/index preservation for List and Vec.
- [ ] F2: connect existing inductive List permutation to finite-position
  bijections and prove the action equation on actual values, including duplicates.
- [ ] F3: package an ordinary function with its general Local and permutation
  proofs; derive Strong separately. Prove action identity/composition pointwise.
- [ ] F3: check both sort-to-List and same-shape wrappers against their laws;
  document selected traversal rather than claim arbitrary-container support.
- [ ] F4: instantiate QuickSort, InsertionSort, BubbleSort and the existing
  merge fixture. Add a separately named conventional two-front MergeSort if
  required; never silently replace the old algorithm or claim its complexity.
- [ ] F4: cover a straightforward additional backend (selection or existing
  tree sort), recording the actual relation assumptions per algorithm.
- [ ] F4: verify open-input ordinary-result theorems, then closed examples for
  empty/singleton/reversed/already-sorted/duplicate-labelled data.

## F5. Verification and Publication

### Subjective (User)

Use progress-tracked Markdown and publish tested milestones to Main.

### Objective (Code)

`src/Makefile:check-acceptance` is the permanent gate. Existing source-image
tests cover ordinary/retained images and budgeted resume. No new image format
is needed for source library definitions.

### Assessment

Successful examples alone cannot establish view laws or general sorting.
Small finite checks are boundary tests, not substitutes for open proofs.

### Plan

- [x] Add a focused permanent target to `check-acceptance`; reuse program/image
  comparison tools instead of adding another checker or proof runner.
- [ ] Reject omitted/duplicated positions, bad inverses/refill, wrong shape/length,
  wrong result certificates, and Local-to-Strong without the required assumptions.
- [x] F1: compare assertion-free and asserted programs; missing source information
  must stay missing. Explicit motives and constructor bounds must pass.
- [x] F1: check imports, ordinary/retained images and resume from 0/100 steps.
- [x] F1: run the full existing gate; for later compiler-change epochs repeat it,
  and for source-only
  milestones also run affected generic sorting and Local/Strong checks.
- [x] F1: record per-file additions/deletions, measured proof/evaluation work and
  nonclaims separately. Do not equate checker steps with algorithm complexity.
- [x] F1: report evidence and remaining scope on #41; close only when
  its complete implementation criteria are satisfied, not at this milestone.

### Milestone Accounting

Relative to `ebe0648`; inherited root-worktree experiments excluded. All entries
below are additions; there are no deletions. This adds a source proof library and
tests, not a compiler-code reduction. Accepted C implementation/header changes: 0.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `tests/fixtures/finite_positions.p` | 132 | 0 | +132 |
| `tests/acceptance/finite-positions.p` | 71 | 0 | +71 |
| `tests/finite_positions.sh` | 82 | 0 | +82 |
| `src/Makefile` | 5 | 0 | +5 |
| `src/prototype/family_domain_conversion.p` | 10 | 0 | +10 |
| `README.md` | 4 | 0 | +4 |
| Authority plan | 2 | 0 | +2 |
| Imported sorting audit (successor link only) | 2 | 0 | +2 |
| This plan | 292 | 0 | +292 |

Local logs: `/tmp/a-program-finite-positions-{focused,debug,asan}.log` and
`/tmp/a-program-finite-positions-acceptance-resumed.log`. Temporary logs and
unsuccessful Vec trials are not published; the minimal compiler diagnostic is.
Source-only changes need no wire-format migration. F2-F5 and broad P4/P5 remain
open; #41 must not close on this milestone.

## Primary References

Inspected 2026-09-27; no external prover executed. The rest of PR #42's broad
historical bibliography has not been exhaustively revalidated here.

- [Mathlib Tuple.Sort](https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Fin/Tuple/Sort.html):
  finite-position permutation and monotonicity; its LinearOrder assumption is
  stronger than our Local contract and is not adopted wholesale.
- [Agda finite permutations](https://agda.github.io/agda-stdlib/v2.3/Data.Fin.Permutation.html):
  finite equivalences and inverse laws support the pointwise bijection interface.
- [Rocq Mergesort](https://rocq-prover.org/doc/V9.1.0/stdlib/Stdlib.Sorting.Mergesort.html):
  local sortedness and stronger transitive-order conclusions are separate.
- [F*/Pulse sorting abstraction](https://fstar-lang.org/tutorial/book/agentic/agentic_sorting_algorithms.html):
  shared proof-carrying sorting contracts across algorithms; cost is additional.
