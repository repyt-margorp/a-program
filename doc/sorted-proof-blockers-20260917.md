# Post-hoc universal Sorted proofs: confirmed blockers and bounded results

Date: 2026-09-17.

Tracking issue: [#29](https://github.com/repyt-margorp/a-program/issues/29).
This document reports limitations; merging it does not fix or close the issue.

## Summary and scope

The intended result is a proof for **every input**, attached after defining a sort.
For a pure total sort, the mathematical target is:

```text
sort_sorted : (xs : List Nat) -> Sorted (sort xs)
```

A graph-based presentation is also suitable:

```text
sort_sorted_graph :
  (xs : List Nat) -> (ys : List Nat) ->
  @sort xs ys -> Sorted ys
```

These are schematic signatures, not claims that either declaration has already
been accepted. A witness connects the second presentation to the actual function.
This request does not prescribe new tactics, Returns, or function-specific
induction syntax. Ordinary IADT elimination over the graph is an intended route.

**No universal per-sort proof was completed in this investigation.** We started
with insertion-sort prerequisites and comparator correctness, encountered the
limitations below, and reduced them to small probes. We did not produce four
complete proofs that the checker then rejected. Nor have we established that
every possible encoding is impossible.

Two independently actionable limitations were confirmed:

1. Graph export rejects distinct leaves with the same source constructor name.
2. A basic indexed LE inversion reaches an unsupported dependent-motive path.

A third direct comparator-proof attempt was rejected, but its cause has not been
established. It is not presented as a confirmed compiler defect.

## Revision and provenance

- Tested implementation: **3233bfa80a66efe5db4b20e104108e640583b1a2**.
- Commit title: "Preserve retained typing inputs and expose remaining source allocation gaps".
- Test checkout: an isolated Book-side worktree; no sibling development files were changed.
- GitHub default branch at report submission: `rewrite/pointer-core-hott`,
  **1b95e551b6ef315e079120a52b1a878d06ff2d63**.
- The tested revision came from the local committed rewrite branch, not GitHub HEAD.
  The source line references below refer to **3233bfa**, not the PR base.
- This documentation PR is based on GitHub's default branch and does **not**
  publish the intervening implementation commits.
- Sort provider: Book-side `src/prototype/pointer/library/sort.p`, an external
  experimental source snapshot, not a claim about tracked upstream library coverage.
- Provider SHA-256:
  `b859e517843b40a9448e26e28ab9d26f926e4485d6e1f62a6ef763d95aa3639c`.
  The complete snapshot is included below.

Reproduction of the precise results requires the tested revision. If it is not
available remotely yet, a maintainer can use the local committed rewrite branch.
Do not interpret a failure or a pass on the older PR base as the same measurement.

## What did pass

| Algorithm | Empty | Singleton [2] | Input [2,1,2] | Universal proof |
|---|---|---|---|---|
| Insertion Sort | Pass | Pass | Pass | Not completed |
| Tree Sort | Pass | Pass | Pass | Not completed |
| Merge Sort | Pass | Pass | Pass | Not completed |
| QuickSort | Pass | Pass | Pass | Not completed |

The attached sample constructs a proof for [1,2,2] and checks it against
`Sorted (sort input)` for each actual computation, not merely against a separately
written expected list. The empty and singleton cases are checked similarly:
12 computation-indexed checks in total.

The conventional two-constructor LE in this sample means nondecreasing natural
order. AllFrom requires the head to be below every tail element; Sorted requires
that bound and a recursively sorted tail. Duplicates are allowed. No machine-checked
equivalence with a numeric-index specification was established here.

Observed results with a 1,000,000-step budget:

- Twelve closed checks: `done steps=388354`, exit 0.
- Reload and recheck saved artifact: `done steps=388604`, exit 0.
- Evaluation equality of all four outputs with [1,2,2]: passes.
- Reusing the certificate as `Sorted input` for the unsorted input:
  `rejected steps=388997`, exit 1.

Ascription checks existing evidence; it is not used as a proof generator.
Passing finite examples does not prove the universal theorem. Sorted alone also
does not establish permutation, stability, or complexity. The experimental merge
uses repeated insertion, not a standard linear-time two-front merge.

## Blocker A: Graph leaf-name collisions

Save the first fixture below as `graph-duplicate-leaf.p`. Its ordinary function
is accepted. Requesting `@f` produces `unsupported steps=754`, exit 4.
Replacing `main := @f;` with `main := f;` gives `done steps=726`.

This occurs in the real comparator too:

```a-program
import natLessOrEqual;
main := @natLessOrEqual;
```

With the attached provider, this gives `unsupported steps=59783`.
The comparator first matches the left Nat and then the right Nat in the successor
branch. Different graph leaves therefore originate from a constructor named zero.

### Confirmed implementation location

In `src/prototype/pointer/synthesis.c` at the tested revision:

- `function_graph_exports`, line 2990:
  `if (prior->producer && same_name(prior->name, name->name)) return -1;`
- A debugger stopped with `index = 1`, both names equal to `zero`.
- The return is mapped to UNSUPPORTED by `function_graph_step`, lines 3172–3173.

This is an export-name collision, not evidence that the comparison function is
ill-typed or that its mathematical correctness is unprovable.

Factoring the inner match into the named helper in the second fixture was also
tested: body accepted (`done steps=813`), graph unsupported
(`unsupported steps=842`). That factoring is **not** a verified workaround.

### Requested behavior and regression coverage

- Distinct graph leaves must retain distinct identities even when source names
  repeat in nested matches.
- Provide an unambiguous, owner-local way to refer to the leaves. Do not overwrite
  an existing export, silently drop a branch, or allocate unrelated global names.
- Verify graph formation, elimination, witness use, and artifact reload.
- Prove a simple property by eliminating the resulting graph; extraction alone
  is insufficient regression coverage.
- Include nested equal-name cases, named-helper factoring, and this comparator.
- Verify that selecting a wrong leaf/evidence cannot produce an accepted proof.

No particular surface syntax is prescribed by this report.

## Blocker B: LE predecessor inversion

The third fixture defines conventional LE and attempts:

```text
LE (succ m) (succ n) -> LE m n
```

The successor constructor carries precisely the smaller proof. Its two endpoint
indices must be refined against the scrutinized type. The zero branch is impossible.

Observed result: `unsupported steps=3352`, exit 4.

### Confirmed implementation location

At `synthesis.c:5644`, in `match_dependent_motive_step`, the debugger observed
non-null `state->path_context` and execution of:

```c
if (state->path_context) goto unsupported;
```

It exits through line 5668, called from `match_step` at line 6302.
Constant-motive candidates also fail earlier; the final confirmed unsupported
path is the dependent-motive fallback.

This establishes a limitation of this concrete proof spelling. It does not prove
that all indexed elimination is unsupported. Previously checked examples such as
Sorted-tail elimination do pass, and another encoding may offer a workaround.

### Requested behavior and regression coverage

- Accept the inversion lemma with an appropriately checked motive/refinement,
  or document and test a supported equivalent spelling.
- Include open m and n, not only closed numerals.
- Follow up with LE transitivity, a prerequisite in the attempted proof plan.
- Reject mismatched endpoints and invalid predecessor proofs.
- Recheck both successful and invalid cases after artifact serialization.
- Do not bypass refinement checks merely to turn UNSUPPORTED into acceptance.

## Other attempted routes and limits of diagnosis

The final appendix preserves an unfinished direct comparator-correctness attempt.

1. A recursive proof of transitivity for conventional LE led to dependent-match
   failures, reduced to the inversion probe above.
2. A separate experimental LE with a transitivity constructor allowed general
   le_refl and all_trans prerequisites to pass (`done steps=68847`).
   This is a different presentation from the two-constructor LE in the closed
   tests. Their equivalence was not proved inside A Program.
3. A Decision family indexed by the comparison Bool was used to try proving
   `Decision x y (natLessOrEqual x y)` directly, without `@natLessOrEqual`.
   The attempt is rejected (`rejected steps=73507`); removing the final ascription
   does not make the body pass. Its rejection has not been isolated to a confirmed
   implementation defect and must not be reported as one.

Fixing A and B is not a guarantee that all remaining proofs immediately pass.
The work still requires insertion-bound preservation, tree bounds and traversal
lemmas, merge preservation, and pivot/partition/append lemmas for QuickSort.
Comparator correctness must connect a Boolean decision to the intended order;
an arbitrary Boolean function alone provides no such law.

## Relation to existing reports

This report complements #13's broader post-hoc-property discussion but gives
specific reproductions at a fixed rewrite revision. It is not a reinstatement
of the original #28 merge/recursive-hypothesis diagnosis, which was subsequently
corrected. Do not conflate an incorrectly applied IH with these independently
reproduced limitations.

## Reproduction commands

Extract the fenced fixtures below into files with the indicated names.
Run from a checkout of the tested revision:

```sh
make -C src/prototype/pointer pointer-check
src/prototype/pointer/.build/pointer-check --steps 1000000 graph-duplicate-leaf.p
src/prototype/pointer/.build/pointer-check --steps 1000000 graph-helper-leaf.p
src/prototype/pointer/.build/pointer-check --steps 1000000 le-pred-probe.p
src/prototype/pointer/.build/pointer-check --legacy-intrinsic-dot --steps 1000000 --imports sort-provider.p --save sorted-samples.a sort-samples-sorted.p
src/prototype/pointer/.build/pointer-check --steps 1000000 --load sorted-samples.a
src/prototype/pointer/.build/pointer-check --legacy-intrinsic-dot --steps 1000000 --imports sort-provider.p insertion-sorted-attempt.p
```

Expected failure of a probe reproduces a limitation; it is **not** a successful
universal proof. The legacy flag is for the provider's existing intrinsic spelling.

For debugging only, rebuild pointer-check with
`CFLAGS='-std=c11 -Wall -Wextra -Werror -O0 -g'` and `-B`. Break at
`synthesis.c:2990` for graph export and `synthesis.c:5644` for inversion.
Inspect the conditions above rather than treating every speculative candidate
failure as the final cause.

The Book-wide `make check` was also attempted and stopped at the pre-existing
intrinsic-arithmetic exact-conversion failure. That uses a different checkout
and is not evidence about these isolated sort tests.

## Fixtures and complete external snapshots

Only this Markdown document is proposed for the repository. The code blocks
are reproducible evidence, not changes to the accepted implementation or test suite.

### graph-duplicate-leaf.p

```a-program
// Two different leaves originate from the constructor named zero.
Nat := @{ zero : *; succ : * -> *; };
f := \x : Nat => x
	@zero => Nat.zero
	@succ n => (n @zero => Nat.zero @succ k => Nat.succ k);
main := @f;
```

### graph-helper-leaf.p

```a-program
// Equivalent computation, factored into a named helper.
Nat := @{ zero : *; succ : * -> *; };
helper := \n : Nat => n @zero => Nat.zero @succ k => Nat.succ k;
f := \x : Nat => x @zero => Nat.zero @succ n => helper n;
main := @f;
```

### le-pred-probe.p

```a-program
// Standard inversion lemma, deliberately retained as an unsupported probe.
Nat := @{ zero : *; succ : * -> *; };
LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
le_pred := \m : Nat => \n : Nat => \proof : LE (Nat.succ m) (Nat.succ n) => proof
	@succ a b prior => prior;
le_pred :: (m : Nat) -> (n : Nat) -> LE (Nat.succ m) (Nat.succ n) -> LE m n;
```

### sort-provider.p

```a-program
// Foundational list-sorting algorithms for the pointer-core prototype.
//
// Every algorithm takes the same explicit Boolean comparison. `quickSort`
// uses Acc evidence to justify recursion on a strictly smaller measured list.

Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

natLessOrEqual := \left : Nat =>
	left @zero => (\right : Nat => Bool.true)
		@succ leftPredecessor =>
			(\right : Nat =>
				right @zero => Bool.false
					@succ rightPredecessor =>
						*leftPredecessor rightPredecessor);

natLessOrEqual :: Nat -> Nat -> Bool;

insertBy := \A : @ => \le : A -> A -> Bool => \value : A => \xs : List A =>
	xs @nil => (List A).cons value (List A).nil
		@cons head tail =>
			(le value head
				@true => (List A).cons value xs
				@false => {
					insertedTail := *tail;
					(List A).cons head insertedTail;
				});

insertBy :: (A : @) -> (A -> A -> Bool) -> A -> List A -> List A;

insertionSortBy := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	xs @nil => (List A).nil
		@cons head tail => {
			sortedTail := *tail;
			insertBy A le head sortedTail;
		};

insertionSortBy :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

insertNat := insertBy Nat natLessOrEqual;
insertNat :: Nat -> List Nat -> List Nat;

insertionSort := insertionSortBy Nat natLessOrEqual;
insertionSort :: List Nat -> List Nat;

LT :=
	@\left : Nat =>
	@\right : Nat =>
	{
		step : (n : Nat) -> * n (Nat.succ n);
		weakenRight : (m : Nat) -> (n : Nat) -> * m n ->
			* m (Nat.succ n);
		lift : (m : Nat) -> (n : Nat) -> * m n ->
			* (Nat.succ m) (Nat.succ n);
	};

Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

accessibleSucc := \n : Nat => \proof : Acc Nat LT n =>
	proof @acc current down =>
		(Acc Nat LT).acc (Nat.succ current)
			&(\y : Nat => \edge : LT y (Nat.succ current) =>
				edge @step k => proof
					@weakenRight m k prior => down m prior
					@lift m k prior => *down m prior);

accessibleSucc :: (n : Nat) -> Acc Nat LT n -> Acc Nat LT (Nat.succ n);

natAccessible := \n : Nat =>
	n @zero =>
		(Acc Nat LT).acc Nat.zero
				&(\y : Nat => \edge : LT y Nat.zero =>
					edge @step k => Nat.zero
						@weakenRight m k prior => Nat.zero
						@lift m k prior => Nat.zero)
		@succ k => accessibleSucc k *k;

natAccessible :: (n : Nat) -> Acc Nat LT n;

SizedList := \A : @ => @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> A -> * n -> * (Nat.succ n);
};

Measured := \A : @ => @{
	measured : (n : Nat) -> SizedList A n -> *;
};

Partition := \A : @ => \bound : Nat => @{
	parts :
		(lowerSize : Nat) ->
		(lower : SizedList A lowerSize) ->
		(upperSize : Nat) ->
		(upper : SizedList A upperSize) ->
		LT lowerSize (Nat.succ bound) ->
		LT upperSize (Nat.succ bound) -> *;
};

partitionLower := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					(Nat.succ lowerSize)
					((SizedList A).cons lowerSize head lower)
					upperSize upper
					(LT.lift lowerSize (Nat.succ size) lowerBound)
					(LT.weakenRight upperSize (Nat.succ size) upperBound);

partitionLower :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionUpper := \A : @ => \head : A => \size : Nat =>
	\partitioned : Partition A size =>
		partitioned
			@parts lowerSize lower upperSize upper lowerBound upperBound =>
				(Partition A (Nat.succ size)).parts
					lowerSize lower
					(Nat.succ upperSize)
					((SizedList A).cons upperSize head upper)
					(LT.weakenRight lowerSize (Nat.succ size) lowerBound)
					(LT.lift upperSize (Nat.succ size) upperBound);

partitionUpper :: (A : @) -> A -> (size : Nat) -> Partition A size ->
	Partition A (Nat.succ size);

partitionByDecision := \A : @ => \head : A => \size : Nat =>
	\decision : Bool => \partitioned : Partition A size =>
		decision
			@true => partitionLower A head size partitioned
			@false => partitionUpper A head size partitioned;

partitionByDecision :: (A : @) -> A -> (size : Nat) -> Bool ->
	Partition A size -> Partition A (Nat.succ size);

partition := \A : @ => \le : A -> A -> Bool =>
	\pivot : A => \size : Nat => \xs : SizedList A size =>
		xs @nil =>
			(Partition A Nat.zero).parts Nat.zero (SizedList A).nil
					Nat.zero (SizedList A).nil
					(LT.step Nat.zero) (LT.step Nat.zero)
		@cons tailSize head tail => {
			decision := le head pivot;
			partitionByDecision A head tailSize decision *tail;
		};

partition :: (A : @) -> (A -> A -> Bool) -> (pivot : A) ->
	(size : Nat) -> SizedList A size -> Partition A size;

measure := \A : @ => \xs : List A =>
	xs @nil => (Measured A).measured Nat.zero (SizedList A).nil
		@cons head tail =>
			(*tail @measured n values =>
				(Measured A).measured (Nat.succ n)
					((SizedList A).cons n head values));

measure :: (A : @) -> List A -> Measured A;

append := \A : @ => \left : List A =>
	left @nil => (\right : List A => right)
		@cons head tail =>
			(\right : List A => (List A).cons head (*tail right));

append :: (A : @) -> List A -> List A -> List A;

// TreeSort demonstrates a second structural recursion scheme. The tree is
// deliberately unbalanced: the supplied comparison controls its shape.
Tree := \A : @ => @{
	empty : *;
	node : A -> * -> * -> *;
};

treeInsert := \A : @ => \le : A -> A -> Bool => \value : A =>
	\tree : Tree A =>
		tree @empty => (Tree A).node value (Tree A).empty (Tree A).empty
			@node root lower upper =>
				(le value root
					@true => {
						insertedLower := *lower;
						(Tree A).node root insertedLower upper;
					}
					@false => {
						insertedUpper := *upper;
						(Tree A).node root lower insertedUpper;
					});

treeInsert :: (A : @) -> (A -> A -> Bool) -> A -> Tree A -> Tree A;

treeBuild := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	xs @nil => (Tree A).empty
		@cons head tail => {
			builtTail := *tail;
			treeInsert A le head builtTail;
		};

treeBuild :: (A : @) -> (A -> A -> Bool) -> List A -> Tree A;

treeToList := \A : @ => \tree : Tree A =>
	tree @empty => (List A).nil
		@node value lower upper => {
			lowerValues := *lower;
			upperValues := *upper;
			append A lowerValues ((List A).cons value upperValues);
		};

treeToList :: (A : @) -> Tree A -> List A;

treeSort := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	treeToList A (treeBuild A le xs);

treeSort :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

// Alternating split gives each recursive MergeSort call approximately half of
// its input. The result is intentionally unindexed; the recursion below is
// justified by an explicit decreasing fuel value.
Halves := \A : @ => @{
	halves : List A -> List A -> *;
};

splitAlternating := \A : @ => \xs : List A =>
	xs @nil => (Halves A).halves (List A).nil (List A).nil
		@cons head tail =>
			(*tail @halves left right =>
				(Halves A).halves ((List A).cons head right) left);

splitAlternating :: (A : @) -> List A -> Halves A;

// `right` is captured by the Match. Therefore `*tail` already has type
// `List A`; it must not be applied to `right` again.
mergeBy := \A : @ => \le : A -> A -> Bool => \left : List A =>
	\right : List A =>
		left @nil => right
			@cons head tail => {
				mergedTail := *tail;
				insertBy A le head mergedTail;
			};

mergeBy :: (A : @) -> (A -> A -> Bool) -> List A -> List A -> List A;

mergeSortFuel := \le : Nat -> Nat -> Bool => \fuel : Nat =>
	fuel @zero => (\xs : List Nat => xs)
		@succ remaining =>
			(\xs : List Nat =>
				xs @nil => (List Nat).nil
					@cons head tail => {
						splitResult := splitAlternating Nat xs;
						splitResult @halves left right => {
							leftSorted := *remaining left;
							rightSorted := *remaining right;
							mergeBy Nat le leftSorted rightSorted;
						};
					});

mergeSortFuel :: (Nat -> Nat -> Bool) -> Nat -> List Nat -> List Nat;

mergeSort := \le : Nat -> Nat -> Bool => \xs : List Nat =>
	measure Nat xs @measured size ignored => mergeSortFuel le size xs;

mergeSort :: (Nat -> Nat -> Bool) -> List Nat -> List Nat;

quickSortAcc := \A : @ => \le : A -> A -> Bool =>
	\size : Nat => \access : Acc Nat LT size =>
		access @acc current down =>
			(\input : SizedList A current =>
					input @nil => (List A).nil
						@cons tailSize pivot tail => {
							partitioned := partition A le pivot tailSize tail;
							partitioned
								@parts lowerSize lower upperSize upper lowerBound upperBound =>
									{
										lowerResult := *down lowerSize lowerBound lower;
										upperResult := *down upperSize upperBound upper;
										append A lowerResult
											((List A).cons pivot upperResult);
									};
				});

quickSortAcc :: (A : @) -> (A -> A -> Bool) ->
	(size : Nat) -> Acc Nat LT size -> SizedList A size -> List A;

quickSort := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	measure A xs @measured size values =>
		quickSortAcc A le size (natAccessible size) values;

quickSort :: (A : @) -> (A -> A -> Bool) -> List A -> List A;

quickSortTerminates := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	#.terminates (&(quickSort A &le xs));
```

### sort-samples-sorted.p

```a-program
// Closed examples only: these checks do NOT prove the universal theorems.
import Nat;
import List;
import insertionSort;
import treeSort;
import mergeSort;
import quickSort;
import natLessOrEqual;
LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
};
AllFrom := \head : Nat => @\xs : List Nat => {
	nil : * (List Nat).nil;
	cons : (next : Nat) -> (tail : List Nat) -> LE head next -> * tail ->
		* ((List Nat).cons next tail);
};
Sorted := @\xs : List Nat => {
	nil : * (List Nat).nil;
	cons : (head : Nat) -> (tail : List Nat) -> AllFrom head tail -> * tail ->
		* ((List Nat).cons head tail);
};
one := Nat.succ Nat.zero;
two := Nat.succ one;
empty := (List Nat).nil;
last := (List Nat).cons two empty;
tail := (List Nat).cons two last;
input := (List Nat).cons two ((List Nat).cons one last);
expected := (List Nat).cons one tail;
one_two := LE.succ Nat.zero one (LE.zero one);
two_two := LE.succ one one (LE.succ Nat.zero Nat.zero (LE.zero Nat.zero));
last_sorted := Sorted.cons two empty (AllFrom two).nil Sorted.nil;
tail_sorted := Sorted.cons two last ((AllFrom two).cons two empty two_two (AllFrom two).nil) last_sorted;
certificate := Sorted.cons one tail
	((AllFrom one).cons two last one_two ((AllFrom one).cons two empty one_two (AllFrom one).nil))
	tail_sorted;
certificate :: Sorted expected;
certificate :: Sorted (insertionSort input);
certificate :: Sorted (treeSort Nat &natLessOrEqual input);
certificate :: Sorted (mergeSort &natLessOrEqual input);
certificate :: Sorted (quickSort Nat &natLessOrEqual input);
empty_certificate := Sorted.nil;
empty_certificate :: Sorted (insertionSort empty);
empty_certificate :: Sorted (treeSort Nat &natLessOrEqual empty);
empty_certificate :: Sorted (mergeSort &natLessOrEqual empty);
empty_certificate :: Sorted (quickSort Nat &natLessOrEqual empty);
last_sorted :: Sorted (insertionSort last);
last_sorted :: Sorted (treeSort Nat &natLessOrEqual last);
last_sorted :: Sorted (mergeSort &natLessOrEqual last);
last_sorted :: Sorted (quickSort Nat &natLessOrEqual last);
insertion_output := insertionSort input;
tree_output := treeSort Nat &natLessOrEqual input;
merge_output := mergeSort &natLessOrEqual input;
quick_output := quickSort Nat &natLessOrEqual input;
main := certificate;
```

### insertion-sorted-attempt.p

```a-program
// UNFINISHED proof attempt: compare_correct is rejected on revision 3233bfa.
// Prerequisites before Decision pass; NOT yet a theorem about insertBy.
// LE includes an explicit transitivity rule, avoiding the unsupported inversion
// encountered with the two-constructor presentation in le-pred-probe.p.
import Nat;
import Bool;
import List;
import natLessOrEqual;
import insertBy;
import insertionSortBy;

LE := @\left : Nat => @\right : Nat => {
	zero : (n : Nat) -> * Nat.zero n;
	succ : (m : Nat) -> (n : Nat) -> * m n -> * (Nat.succ m) (Nat.succ n);
	trans : (x : Nat) -> (y : Nat) -> (z : Nat) -> * x y -> * y z -> * x z;
};
AllFrom := \head : Nat => @\xs : List Nat => {
	nil : * (List Nat).nil;
	cons : (next : Nat) -> (tail : List Nat) -> LE head next -> * tail ->
		* ((List Nat).cons next tail);
};
Sorted := @\xs : List Nat => {
	nil : * (List Nat).nil;
	cons : (head : Nat) -> (tail : List Nat) -> AllFrom head tail -> * tail ->
		* ((List Nat).cons head tail);
};

le_refl := \n : Nat => n
	@zero => LE.zero Nat.zero
	@succ k => LE.succ k k *k;
le_refl :: (n : Nat) -> LE n n;

le_trans := \x : Nat => \y : Nat => \xy : LE x y => \z : Nat => \yz : LE y z =>
	LE.trans x y z xy yz;
le_trans :: (x : Nat) -> (y : Nat) -> LE x y -> (z : Nat) -> LE y z -> LE x z;

all_trans := \y : Nat => \xs : List Nat => \bound : AllFrom y xs => bound
	@nil => (\x : Nat => \xy : LE x y => (AllFrom x).nil)
	@cons head tail yh yt => (\x : Nat => \xy : LE x y =>
		(AllFrom x).cons head tail (le_trans x y xy head yh) (*yt x xy));
all_trans :: (y : Nat) -> (xs : List Nat) -> AllFrom y xs ->
	(x : Nat) -> LE x y -> AllFrom x xs;

Decision := \x : Nat => \y : Nat => @\answer : Bool => {
	yes : LE x y -> * Bool.true;
	no : LE y x -> * Bool.false;
};
compare_correct := \x : Nat => x
	@zero => (\y : Nat => (Decision Nat.zero y).yes (LE.zero y))
	@succ m => (\y : Nat => y
		@zero => (Decision (Nat.succ m) Nat.zero).no (LE.zero (Nat.succ m))
		@succ n => (*m n
			@yes prior => (Decision (Nat.succ m) (Nat.succ n)).yes (LE.succ m n prior)
			@no prior => (Decision (Nat.succ m) (Nat.succ n)).no (LE.succ n m prior)));
compare_correct :: (x : Nat) -> (y : Nat) -> Decision x y (natLessOrEqual x y);

main := le_refl Nat.zero;
expected := LE.zero Nat.zero;
```

