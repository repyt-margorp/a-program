# Local versus strong sortedness: pinned submission review

Date: 2026-09-27.
Status: design proposal; targeted baseline and small-fixture verification complete.
Code baseline: main, **8cc975296b5d6f54704e1f13950637977a37754f**; fresh independent clone; no implementation edits.
Related: [Issue #39](https://github.com/repyt-margorp/a-program/issues/39).
Source: supplied 2026-09-26 audit, preserved below as a historical record.

## Problem List

| ID | Problem | Status |
| --- | --- | --- |
| P1 | Separate adjacency and all-pairs evidence without weakening existing contracts | Proposed; baseline verified |

## P1. Explicit local and strong sortedness contracts

### Subjective (User)

- User request, English paraphrase, 2026-09-27: clone the latest A Program, review the supplied sortedness audit, and submit a separate Issue and documentation PR.
- The supplied audit proposes local sortedness as the base sorting contract, with strong sortedness derived under transitivity.
- Submission is authorized; adopting a naming policy, changing library semantics, or deleting existing proofs is **not** inferred from that authorization. The design recommendations remain proposals.

### Objective (Code)

The source document was located at `workspace/temp/2026-09-26-sortedness-local-vs-strong-design-audit.md`.
The current default branch is **main**, not the older rewrite branch. The compiler has been promoted:
`src/prototype/pointer/` is now `src/`, tests are in `tests/`, and the checker is
`build/pointer/pointer-check`. Historical paths in the supplied audit must be read with that mapping.

At the pinned revision:

- `tests/fixtures/sorted-proof-provider.p:305–317` defines `general_all_from`,
  `general_sorted`, and `general_decision`. A sorted cons stores a bound to **all**
  tail elements plus a sorted tail: its meaning is pairwise, not merely adjacent.
- `tests/acceptance/generic-quick-sorted-result.p:194–231` uses `qsr_all_trans`
  for cross-partition bounds and `refl pivot` in the join helper.
- Lines 272–290 convert the local proof family to imported `general_sorted`
  and state the ordinary-result theorem with transitivity, reflexivity and
  comparator evidence. The theorem mentions `quickSort A (&le) xs` directly.
- No definitions named `general_locally_sorted` or `general_strongly_sorted`
  were found in the current .p tests or C implementation. This is a scoped
  name search, not proof that equivalent predicates cannot be expressed.

Fresh verification:

1. `make -s pointer-check` succeeded in the fresh clone.
2. `bash tests/quick_result.sh build/pointer/pointer-check` passed in full.
   The general result checked in 1,128,412 transitions; reload in 1,128,645;
   pending-image resume in 1,128,715; retained-image reload in 1,166,731.
   The wrong result index was rejected (1,274,951), as was the resumed wrong
   result (1,275,254). The runner also checked computation motives and
   normalized-index positive/negative cases. No complete repository suite or
   sanitizer run is claimed.
3. The standalone fixture included below checked with the existing provider:
   `done steps=67729`, exit 0. It checks the proposed local IADT spelling,
   a local [a,b,c] certificate for a cyclic relation, and the general theorem
   `general_decision A R x x answer -> R x x`.
   It does **not** implement a comparator for the cycle or certify QuickSort
   under that comparator.
4. [Agda v2.4 Linked.Properties](https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.Properties.html)
   was re-read: it exposes AllPairs-to-Linked without assumptions,
   Linked-to-AllPairs under transitivity, and append via boundary connectivity.
   Other proof assistants were not executed in this pass. The cited Rocq page
   could not be fetched in this pass; the remaining cross-system comparison
   is preserved as supplied research, not fresh executable verification.

### Assessment

This is a **library specification and evidence-structure proposal**, not a
current soundness failure. The existing strong QuickSort theorem is valid and
passes; it should not be replaced by a weaker meaning under the same name.

The core mathematical distinction is sound:

- strong implies local without assumptions;
- local implies strong under transitivity;
- without transitivity, a directed cycle separates them.

For empty lists the exact local edge-position count is **max(n - 1, 0)**;
the original audit's n - 1 formula presupposes n >= 1.
The strong count is n(n - 1)/2 in the unfolded evidence shape. Neither number is
a measured allocation, serialized-size, runtime or speedup bound in a shared DAG.

In a proof-relevant universe, functions in both directions establish mutual
derivability/inhabitance implications. They do **not** automatically establish
definitional equality, inverse round trips, an Equiv of evidence types, or
proof irrelevance. Losing and reconstructing nonadjacent proofs can change data.

The diagonal Decision lemma confirms that a separate refl argument is logically
redundant **under this certificate interface**. It does not allow strict less-than
on equal inputs and does not establish transitivity.

Adopt the supplied audit's separation as the subject of a staged experiment.
Keep the current strong API during review. Local QuickSort without transitivity
is a well-motivated target, but its full A Program construction is still unverified
here. No new Core rule or expected-type behavior for `::` is proposed.

### Plan

- [x] Pin current GitHub revision, inspect current declarations and join helpers.
- [x] Recheck the existing ordinary-result theorem and its negative/image controls.
- [x] Compile local IADT/cyclic-list construction and diagonal Decision extraction.
- [ ] Introduce explicitly named local and strong predicates, keeping legacy
  `general_sorted` strong. Choose aliases versus fresh nominal families deliberately.
- [ ] Prove strong-to-local and local-to-strong; require transitivity only for
  the latter. Test open inputs and wrong endpoint evidence.
- [ ] Supply a cyclic comparator and its directional certificates; certify local
  QuickSort for arbitrary inputs without trans/refl parameters.
- [ ] Add a separation negative: reject attempts to manufacture the missing
  nonadjacent relation evidence. A finite local example alone is not that test.
- [ ] Derive the strong ordinary-result theorem via local-to-strong and retain
  direct strong proof consumers as a compatibility oracle.
- [ ] Keep permutation/content correctness separate; do not restore global
  `*quickSort` syntax or silently weaken old theorem types.
- [ ] Compare transitions, RSS, evidence nodes and image sizes across local,
  derived-strong and direct-strong proofs. No performance gain is assumed.
- [ ] Review insertion/tree/merge independently and decide legacy-name migration.

Completion requires the new universal theorem and conversions, positive/negative
source and artifact checks, preserved strong consumers, and recorded measurements.
Publishing or merging this audit is **not** completion of that implementation.

## Reproduction

From the pinned repository root:

```sh
make -s pointer-check
bash tests/quick_result.sh build/pointer/pointer-check
build/pointer/pointer-check --legacy-intrinsic-dot --steps 5000000 --imports tests/fixtures/sorted-proof-provider.p local-probe.p
```

The standalone file below lives in the Book workspace during this review; it is
embedded in documentation, not added to the accepted source or test suite.


## Checked local-probe.p

```a-program
import List;
import Bool;
import general_decision;

general_locally_sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	one:(x:A)->* ((List A).cons x (List A).nil);
	cons:(x:A)->(y:A)->(ys:List A)->R x y->
		* ((List A).cons y ys)->* ((List A).cons x ((List A).cons y ys));
};

Point := @{ a:*; b:*; c:*; };
Cycle := @\x:Point => @\y:Point => {
	aa:* Point.a Point.a;
	bb:* Point.b Point.b;
	cc:* Point.c Point.c;
	ab:* Point.a Point.b;
	bc:* Point.b Point.c;
	ca:* Point.c Point.a;
};
empty := (List Point).nil;
input := (List Point).cons Point.a ((List Point).cons Point.b ((List Point).cons Point.c empty));
local_certificate := (general_locally_sorted Point Cycle).cons Point.a Point.b
	((List Point).cons Point.c empty) Cycle.ab
	((general_locally_sorted Point Cycle).cons Point.b Point.c empty Cycle.bc
		((general_locally_sorted Point Cycle).one Point.c));
local_certificate :: general_locally_sorted Point Cycle input;

decision_diagonal := \A:@ => \R:A->A->@ => \x:A => \answer:Bool =>
	\proof:general_decision A R x x answer => proof
	@yes evidence => evidence
	@no evidence => evidence;
decision_diagonal :: (A:@)->(R:A->A->@)->(x:A)->(answer:Bool)->
	general_decision A R x x answer->R x x;
```

## Supplied historical audit (2026-09-26)

The original report below is preserved. Its prototype paths and unpinned retrieval
status are superseded by the pinned observations above. Design recommendations
remain proposals rather than implemented or user-approved decisions.

---

# Local vs. Strong Sortedness in A Program

**Date:** 2026-09-26  
**Status:** Design audit and implementation proposal  
**Target:** `src/prototype/pointer/` on `main`  
**Intended repository path:** `doc/2026-09-26-sortedness-local-vs-strong-design-audit.md`

## 1. Executive conclusion

A Program should distinguish **local (adjacent) sortedness** from **strong/pairwise sortedness** as two different indexed predicates.

The current predicate named `general_sorted` is structurally a **strong/pairwise** predicate: for a non-empty list `h :: t`, it stores evidence that `h` is related by `R` to **every** element of `t`, together with recursively sorted evidence for `t`. This is not merely local sortedness.

The recommended long-term split is:

- `general_locally_sorted A R xs`: every consecutive pair in `xs` is related by `R`;
- `general_strongly_sorted A R xs`: every earlier element is related by `R` to every later element.

The two should be connected by explicit library theorems:

- `strongly_sorted_to_locally_sorted`: no assumption on `R`;
- `locally_sorted_to_strongly_sorted`: requires transitivity of `R`.

For sorting algorithms, especially QuickSort, the **fundamental correctness theorem should target local sortedness**. The stronger theorem should be a derived corollary under transitivity. This matches the separation used, under different names, in Rocq, Agda, and Lean, and it also matches the adjacency-based `sorted` check in Idris 2's base library.

This is not just a naming issue. In A Program the distinction is operationally and proof-structurally meaningful:

1. local sortedness contains only `n - 1` relation-evidence positions for a list of length `n`;
2. strong/pairwise sortedness contains `n(n - 1)/2` relation-evidence positions;
3. the present QuickSort proof uses transitivity specifically to manufacture cross-partition pairwise evidence that local correctness does not require;
4. the present proof uses reflexivity specifically because its strong join construction temporarily asks for `R pivot pivot`; a local join does not need that step;
5. the current `general_decision` already implies `R x x` at `x = y`, so a separately passed `refl` is in any case stronger API surface than necessary for the current comparator-certificate design.

The recommended migration is therefore **not** to mutate `general_sorted` silently. Introduce the explicit two-predicate split first, prove conversions, add a local QuickSort theorem, derive the strong theorem from it, migrate consumers, then retire or clearly deprecate the ambiguous legacy name.

---

## 2. Repository snapshot and provenance

This audit re-read the current GitHub `main` source on 2026-09-26 JST, in particular:

- repository root and current pointer-core status;
- `src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p`;
- `DESIGN-PHILOSOPHY.md`;
- `AGENTS.md`.

At retrieval time the repository root reported 1,191 commits and identified `src/prototype/pointer/` as the current implementation. The QuickSort Sorted acceptance file was still reported as 290 physical lines / 275 LOC.

The execution environment used for this audit could read the current GitHub `main` pages but could not clone the repository or resolve the GitHub commit API/commit-list endpoint. Therefore this document intentionally describes the **current retrieved `main` content**, not an immutable SHA. Before merging this document into the repository, record the exact local SHA with:

```sh
git rev-parse HEAD
```

and, if desired, add it to this section.

Relevant A Program sources:

- https://github.com/repyt-margorp/a-program
- https://github.com/repyt-margorp/a-program/blob/main/src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p
- https://github.com/repyt-margorp/a-program/blob/main/DESIGN-PHILOSOPHY.md
- https://github.com/repyt-margorp/a-program/blob/main/AGENTS.md

`AGENTS.md` permits documentation-only changes outside `src/prototype/` when explicitly requested, and asks repository documentation to remain in English. This report follows that rule.

---

## 3. What A Program currently proves

### 3.1 Current structural meaning of `general_sorted`

The acceptance proof defines a local copy `qsr_Sorted` and then maps it directly into the imported nominal predicate `general_sorted`.

Structurally, the current predicate has the recurrence

```text
StrongSorted R []       = unit
StrongSorted R (h :: t) = All (R h) t × StrongSorted R t
```

where `All (R h) t` carries one `R h x` witness for each `x` in `t`.

Thus for

```text
[a, b, c, d]
```

the evidence contains positions for

```text
R a b
R a c
R a d
R b c
R b d
R c d
```

not merely

```text
R a b
R b c
R c d
```

The final `qsr_sorted_connection` performs a constructor-for-constructor conversion from this shape to `general_sorted`, so the imported nominal predicate has the same strong/pairwise meaning.

### 3.2 Current QuickSort theorem

The present ordinary-result theorem has the logical shape

```text
Transitive R
-> Reflexive R
-> ComparatorDecision R le
-> (xs : List A)
-> general_sorted A R (quickSort A (&le) xs)
```

where the comparator certificate is indexed by the Boolean answer and provides one of the two directional relation witnesses:

```text
le x y = true  -> R x y
le x y = false -> R y x
```

This is a directional/totality certificate for the comparator, not the usual `Decidable (R x y)` proposition of the form `R x y + Not (R x y)`.

### 3.3 Where transitivity is actually consumed

The current proof contains a helper corresponding to

```text
R x pivot
-> All (R pivot) ys
-> All (R x) ys
```

implemented by repeatedly applying transitivity.

That helper is then used while appending the sorted lower partition to `pivot :: sortedUpper`. Its purpose is to manufacture evidence such as

```text
R lowerElement upperElement
```

for **every** lower/upper cross pair.

This is exactly what strong/pairwise sortedness needs. Local sortedness does not need it.

### 3.4 Where reflexivity is actually consumed

The current strong join builds an `All (R pivot)` proof for the whole right side

```text
pivot :: sortedUpper
```

and therefore inserts

```text
R pivot pivot
```

using the explicit `refl` argument.

Again, this is an artifact of the chosen strong join formulation. Local sortedness only needs a boundary relation from `pivot` to the first element of the upper output, if such an element exists. It never needs `R pivot pivot` merely because the pivot is placed in the list.

### 3.5 `refl` is already logically latent in `general_decision`

For the current comparator certificate, instantiate `x = y`.

Whatever Boolean `le x x` returns, its certificate branch has type `R x x`:

- `true` branch: `R x x`;
- `false` branch: also `R x x` because the arguments are identical.

Therefore

```text
general_decision A R x x (le x x)
```

is sufficient to derive `R x x`.

This means the current theorem's explicit `refl : (x:A) -> R x x` is redundant relative to the stronger `decide` interface, even before changing Sorted. It may remain convenient internally, but it need not be a logically independent public assumption.

This observation also shows that `general_decision` is designed for a non-strict, connex-style ordering relation. A strict relation such as `<` cannot inhabit the certificate at equal arguments.

---

## 4. Two different mathematical predicates

Let

```text
R : A -> A -> @
```

be arbitrary.

### 4.1 Local sortedness

For a list

```text
[x0, x1, ..., xn]
```

local sortedness means only

```text
R x0 x1
R x1 x2
...
R x(n-1) xn
```

Equivalently, every edge in the list chain is in `R`.

No transitivity assumption belongs in the definition.

### 4.2 Strong/pairwise sortedness

Strong sortedness means

```text
for every i < j, R xi xj
```

or inductively:

```text
Strong R []       = unit
Strong R (h :: t) = All (R h) t × Strong R t
```

This is the current A Program structure.

### 4.3 Logical relationship

For every relation `R`:

```text
Strong R xs -> Local R xs
```

because adjacent pairs are among all ordered pairs.

The converse is false for arbitrary `R`.

If `R` is transitive:

```text
Transitive R -> Local R xs -> Strong R xs
```

and therefore local and strong sortedness become logically equivalent under transitivity.

The distinction is still useful even when most production comparators are transitive, because it records which assumptions an algorithm actually requires and which evidence structure a theorem actually constructs.

---

## 5. A concrete non-transitive example

A three-element cyclic relation is a useful acceptance test because it separates the two notions while remaining directionally total.

Let the elements be

```text
A, B, C
```

and let the non-reflexive directional core be

```text
A R B
B R C
C R A
```

with reflexive cases added if required by the existing `general_decision` contract.

Then

```text
[A, B, C]
```

is locally sorted because

```text
A R B
B R C
```

but it is not strongly sorted because strong sortedness additionally requires

```text
A R C
```

while the cycle gives the opposite direction `C R A`.

This example is important for A Program because it demonstrates that the local theorem is genuinely more general; it is not merely a compressed representation of the same proposition unless transitivity has separately been assumed.

---

## 6. Comparison with Rocq

Reference:

- https://docs.rocq-prover.org/v8.19/stdlib/Coq.Sorting.Sorted.html
- https://docs.rocq-prover.org/v8.10/stdlib/Coq.Sorting.Mergesort.html

Rocq makes the distinction explicit in the standard library.

### 6.1 `LocallySorted`

`LocallySorted R` is defined by the three familiar cases:

- empty list;
- singleton list;
- `a :: b :: l`, requiring `R a b` plus local sortedness of `b :: l`.

So it stores exactly the adjacent relation edges.

### 6.2 `Sorted`

Rocq also supplies `Sorted`, an equivalent two-step presentation using `HdRel`. Importantly, this `HdRel` only relates the new head to the first element of the tail; it does **not** quantify over the whole tail. The library proves `Sorted <-> LocallySorted` without a transitivity assumption.

### 6.3 `StronglySorted`

`StronglySorted R (a :: l)` requires both:

- `StronglySorted R l`;
- `Forall (R a) l`.

This is structurally the same predicate as current A Program `general_sorted`.

### 6.4 Conversions

Rocq provides the exact asymmetry recommended for A Program:

```text
StronglySorted -> Sorted
```

without assumptions, and

```text
Transitive R -> Sorted -> StronglySorted
```

with transitivity.

### 6.5 Sorting-algorithm precedent

Rocq's mergesort library is particularly informative. Its basic theorem proves the ordinary local `Sorted` result. A separate corollary derives `StronglySorted` under an explicit transitivity assumption.

That is almost exactly the architecture proposed here for A Program QuickSort:

```text
quick_locally_sorted
quick_strongly_sorted = locally_to_strongly_sorted trans quick_locally_sorted
```

Rocq therefore provides direct precedent not only for the predicate split, but for **where the transitivity assumption should enter a sorting-correctness API**.

---

## 7. Comparison with Agda

References:

- https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.html
- https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.Properties.html
- https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.AllPairs.html
- https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Sorted.TotalOrder.html
- https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Sorted.TotalOrder.Properties.html
- https://agda.github.io/agda-stdlib/v2.4/Data.List.Sort.Base.html

Agda's standard library separates the two notions even more cleanly at the generic-relation level.

### 7.1 `Linked`

`Linked R xs` means that every consecutive pair is related. This is the generic local notion.

It is exactly the conceptual analogue of Rocq `LocallySorted` and Lean `List.IsChain`.

### 7.2 `AllPairs`

`AllPairs R xs` means that each head is related to every later element, recursively. It is the generic strong/pairwise notion.

This is the conceptual analogue of Rocq `StronglySorted`, Lean `List.Pairwise`, and current A Program `general_sorted`.

### 7.3 Conversions

Agda provides:

```text
AllPairs R xs -> Linked R xs
```

for arbitrary `R`, and

```text
Transitive R -> Linked R xs -> AllPairs R xs
```

for transitive `R`.

The helper used for the second direction has the same mathematical role as A Program's current `qsr_all_trans`: it propagates a relation from an adjacent edge through a linked tail. The architectural difference is that Agda uses this propagation in the **conversion theorem**, rather than forcing every sorting algorithm to construct AllPairs directly.

### 7.4 What Agda calls `Sorted`

For a `TotalOrder`, Agda's `Sorted` abstraction is based on the local/`Linked` notion. The properties module then proves both

```text
AllPairs -> Sorted
Sorted -> AllPairs
```

using order transitivity for the second direction.

This strongly supports using local evidence as the algorithm-facing default and deriving pairwise evidence when the order structure justifies it.

### 7.5 Append is a boundary property

Agda's sorted append theorem is also important. To prove that `xs ++ ys` is sorted, it combines:

- sortedness of `xs`;
- sortedness of `ys`;
- a connection between the last element of `xs` and the first element of `ys`.

It does not require all cross-product pairs as input to the local theorem.

This is exactly the right mental model for a local QuickSort join:

```text
sorted lower
last(lower) R pivot
pivot R head(upper)
sorted upper
```

A Program's partition evidence is currently even stronger than these boundary facts, so it can prove the local join without adding any new ordering assumption.

---

## 8. Comparison with Lean 4 / mathlib

References:

- https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Chain.html
- https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Pairwise.html
- https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Sort.html

Lean/mathlib uses generic names rather than making every relation-specific notion a form of `Sorted`.

### 8.1 `List.IsChain`

`List.IsChain R [a1, ..., an]` means

```text
R a1 a2
R a2 a3
...
R a(n-1) an
```

This is precisely local sortedness for an arbitrary relation.

Mathlib's sorting predicates such as `SortedLE` are related to this chain view for preorder relations.

### 8.2 `List.Pairwise`

`List.Pairwise R xs` means that every earlier position is related to every later position:

```text
forall i j, i < j -> R (xs[i]) (xs[j])
```

Its inductive characterization is again “head related to all tail elements + pairwise tail,” the same structural shape as current A Program `general_sorted`.

### 8.3 Transitivity is an explicit bridge

Modern mathlib exposes constructors/results such as `Pairwise.cons_cons_of_trans`, while the chain API uses transitivity for results that skip intermediate list elements, such as taking arbitrary sublists.

The library therefore preserves the distinction between:

- evidence for actual adjacent edges;
- the transitive closure consequence that all earlier/later elements are related.

The relevant lesson for A Program is not a particular Lean theorem name. It is that **generic relational data structures remain distinct even though they coincide for the transitive order relations used by normal sorting**.

---

## 9. Comparison with Idris 2

References:

- https://idris-lang.org/Idris2/base/source/Data.List.html
- https://idris-lang.org/Idris2/base/docs/Data.List.Quantifiers.html

Idris 2's base library does not appear to expose the same canonical proof-level pair `LocallySorted` / `StronglySorted` under obvious standard names. That makes it weaker evidence for naming policy than Rocq, Agda, or Lean.

It still gives two useful data points.

### 9.1 The ordinary `sorted` check is local

`Data.List.sorted` recursively checks adjacent values:

```text
x <= y && sorted (y :: rest)
```

So the conventional operational meaning of “sorted” in Idris 2 is local adjacency.

### 9.2 Strong evidence is naturally expressible with `All`

`Data.List.Quantifiers.All` is a proof-carrying list of pointwise evidence. Therefore the current A Program strong shape can be encoded in ordinary Idris 2 dependent data as

```text
All (R head) tail
```

plus recursive strong evidence for the tail.

The lesson is similar: dependent types make either representation straightforward, but the generic library need not conflate them.

---

## 10. Cross-system comparison

| System | Local / adjacent notion | Strong / all-pairs notion | Strong -> local | Local -> strong |
|---|---|---|---|---|
| Rocq | `LocallySorted`, `Sorted` | `StronglySorted` | unconditional | requires `Transitive R` |
| Agda | `Linked`, and order-level `Sorted` | `AllPairs` | unconditional | requires transitivity |
| Lean 4 | `List.IsChain` | `List.Pairwise` | structurally weaker | transitivity bridges skipped edges |
| Idris 2 base | `Data.List.sorted` checks adjacent pairs | no canonical proof predicate found; expressible via `All` | user-defined | user-defined with transitivity |
| A Program current | no separately exposed generic local predicate in this proof | `general_sorted` is structurally pairwise | not separated | not separated; QuickSort proves strong directly |
| A Program proposed | `general_locally_sorted` | `general_strongly_sorted` | explicit theorem | explicit theorem requiring transitivity |

The strongest consensus is therefore not that one definition is “the true Sorted.” The consensus is that **adjacency and all-pairs are different generic structures and should remain distinguishable**.

---

## 11. Why the distinction matters more in A Program

### 11.1 A Program is proof-relevant at this layer

The current relation has type

```text
R : A -> A -> @
```

and sortedness is represented as ordinary indexed data. The system does not currently get to assume that all inhabitants of such evidence types are irrelevant or free.

Therefore logical equivalence under transitivity does not imply identical engineering cost.

### 11.2 Evidence-position count

For a list of length `n`:

- local sortedness has `n - 1` relation-evidence positions;
- strong sortedness has

```text
(n - 1) + (n - 2) + ... + 1
= n(n - 1) / 2
```

relation-evidence positions.

Examples:

| `n` | Local relation positions | Strong relation positions |
|---:|---:|---:|
| 10 | 9 | 45 |
| 100 | 99 | 4,950 |
| 1,000 | 999 | 499,500 |

This table describes the logical shape, not a guaranteed measured heap ratio. Pointer interning and shared proof subgraphs may reduce physical duplication. Nevertheless, the strong type requires quadratically many relation slots in its canonical inductive structure.

### 11.3 Derived transitivity chains can enlarge evidence further

If `R x z` evidence is constructed by repeatedly composing

```text
R x y
R y z
```

through a transitivity function, the individual pairwise witness may itself contain a non-trivial derivation chain. Sharing can help, but a strong proof should not be assumed to have the same cost profile as a local proof.

### 11.4 Strong evidence remains valuable

The conclusion is not “delete strong sortedness.” Strong evidence is useful when a consumer needs arbitrary earlier/later comparisons directly, for example:

- extracting `R xi xj` for arbitrary `i < j` without re-running transitivity;
- reasoning about arbitrary sublists without reconstructing skipped links;
- downstream theorems whose natural induction hypothesis is `All (R head) tail`;
- exporting a fully pairwise certificate.

The correct architecture is therefore **local as the weakest algorithmic contract, strong as an explicit stronger certificate**.

---

## 12. Proposed A Program predicates

The following is a design sketch in current `.p` style. It is intentionally a proposal, not claimed here as already type-checked source.

### 12.1 Local predicate

A direct `Linked`-style IADT keeps exactly one relation witness per edge:

```text
general_locally_sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	one:(x:A)->* ((List A).cons x (List A).nil);
	cons:(x:A)->(y:A)->(ys:List A)->
		R x y->
		* ((List A).cons y ys)->
		* ((List A).cons x ((List A).cons y ys));
};
```

The intended recursive evidence is:

```text
Local R []
Local R [x]
R x y -> Local R (y :: ys) -> Local R (x :: y :: ys)
```

This closely matches Rocq `LocallySorted`, Agda `Linked`, and Lean `IsChain`.

An alternative Rocq-like representation would introduce a one-step `head_rel` predicate and use a two-constructor outer family. The direct three-case `Linked` form is preferable for A Program initially because:

- its evidence meaning is visually explicit;
- no helper family is required merely to encode optional head presence;
- relation evidence count is transparent;
- it mirrors existing induction behavior directly.

### 12.2 Strong predicate

The existing structure should be preserved under an explicit strong name:

```text
general_strongly_sorted := \A:@ => \R:A->A->@ => @\xs:List A => {
	nil:* (List A).nil;
	cons:(h:A)->(t:List A)->
		general_all_from A R h t->
		* t->
		* ((List A).cons h t);
};
```

This is essentially the current `general_sorted` definition under a semantically accurate name.

### 12.3 Why not encode one as a flag on the other?

Do not introduce a mode index such as

```text
SortedMode.local
SortedMode.strong
```

unless later use cases demonstrate a concrete benefit.

Separate nominal predicates are clearer because:

- their constructors carry different data;
- the strong predicate has asymptotically more evidence slots;
- the conversion requires a real theorem and, in one direction, a real assumption;
- separate names make theorem assumptions visible at call sites.

This follows A Program's existing preference for explicit mechanisms and avoids turning a library-level distinction into a core or elaborator concern.

---

## 13. Required conversion theorems

### 13.1 Strong to local

Proposed type:

```text
strongly_sorted_to_locally_sorted ::
	(A:@)->(R:A->A->@)->(xs:List A)->
	general_strongly_sorted A R xs->
	general_locally_sorted A R xs;
```

No transitivity or reflexivity is needed.

At each strong `cons h t`, either:

- `t` is empty, yielding the singleton local constructor; or
- `t = y :: ys`, and the head of `general_all_from A R h t` gives `R h y`; recursively convert the strong tail.

### 13.2 Local to strong

Proposed type:

```text
locally_sorted_to_strongly_sorted ::
	(A:@)->(R:A->A->@)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(xs:List A)->
	general_locally_sorted A R xs->
	general_strongly_sorted A R xs;
```

The key helper is the local analogue of the current `qsr_all_trans` idea:

```text
R x y
-> All (R y) ys
-> All (R x) ys
```

using transitivity.

The important architectural move is that this transitive closure now belongs in a **generic conversion theorem**, not in every sorting algorithm's direct proof.

---

## 14. Refactoring QuickSort around the weaker theorem

### 14.1 What partition already proves

The current `qsr_PartOrdered` records:

```text
for every x in lower: R x pivot
for every y in upper: R pivot y
```

The existing `qsr_quick_all` theorem then shows that an arbitrary `All P` property is preserved by recursively sorting a partition.

Therefore after recursive calls, A Program already has enough evidence to know:

```text
All (\x => R x pivot) sortedLower
All (\y => R pivot y) sortedUpper
```

without transitivity.

### 14.2 Local join lemma

The direct local join can have the conceptual type

```text
local_join :
	Local R sortedLower ->
	All (\x => R x pivot) sortedLower ->
	Local R sortedUpper ->
	All (\y => R pivot y) sortedUpper ->
	Local R (sortedLower ++ pivot :: sortedUpper)
```

No `trans` is needed.

No `refl` is needed.

A simple proof can recurse over `sortedLower`:

- if lower is empty, construct local sortedness of `pivot :: sortedUpper` from the upper `All` proof plus local upper evidence;
- if lower has one element, use its `R element pivot` witness at the boundary;
- if lower has at least two elements, preserve its first local edge and recurse on the remaining lower suffix.

This avoids introducing optional last/head machinery as a prerequisite. A later library abstraction analogous to Agda's boundary `Connected` can be added only if repeated use justifies it.

### 14.3 Fundamental QuickSort theorem

The proposed base theorem should be approximately:

```text
quick_locally_sorted ::
	(A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->
	(xs:List A)->
	general_locally_sorted A R (quickSort A (&le) xs);
```

The crucial point is the absence of:

```text
trans
refl
```

from the algorithmic sortedness theorem.

The comparator certificate plus the partition invariant is sufficient for adjacency correctness.

### 14.4 Derived strong QuickSort theorem

Then derive:

```text
quick_strongly_sorted ::
	(A:@)->(R:A->A->@)->(le:A->A->Bool)->
	(trans:(x:A)->(y:A)->R x y->(z:A)->R y z->R x z)->
	(decide:(x:A)->(y:A)->general_decision A R x y (le x y))->
	(xs:List A)->
	general_strongly_sorted A R (quickSort A (&le) xs);
```

by composition:

```text
quick_strongly_sorted
= locally_sorted_to_strongly_sorted trans (quick_locally_sorted decide xs)
```

This theorem should not require a separate `refl` parameter if the conversion is formulated normally. If some internal helper still needs reflexivity, derive it from the existing comparator certificate rather than exposing it as a separate public assumption.

### 14.5 What happens to the current direct strong proof

Once equivalence has been regression-tested, these roles change:

- current `qsr_all_trans`: moves conceptually into local-to-strong conversion;
- current `qsr_append_sorted`: no longer needed for the base QuickSort theorem;
- current `qsr_join_sorted`: replaced by a local join without `trans`/`refl`;
- current strong theorem: retained as a derived theorem or as a regression oracle during migration.

Do not delete the direct strong implementation immediately. Keeping both implementations temporarily is useful for checking that the new derived theorem inhabits the same strong result type on existing transitive-order fixtures.

---

## 15. Naming policy

There are three plausible policies.

### Policy A: silently redefine `general_sorted` as local

**Reject.**

Although the new meaning would align with Rocq/Agda/Lean sorting terminology, existing A Program proofs currently import `general_sorted` with strong semantics. Reusing the same name for a weaker predicate creates a semantic breaking change that can be difficult to detect from downstream source text.

### Policy B: keep `general_sorted` permanently strong and add `general_locally_sorted`

This is compatibility-friendly, but leaves the shortest and most natural name attached to the stronger-than-necessary notion. Future sorting proofs are then likely to keep targeting the strong predicate by habit.

### Policy C: explicit names for both, deprecate the ambiguous name

**Recommended.**

Long-term canonical names:

```text
general_locally_sorted
general_strongly_sorted
```

Transition:

1. introduce both explicit predicates;
2. define/retain `general_sorted` with its **current strong semantics** as a temporary compatibility name;
3. mark in documentation that `general_sorted` is legacy/ambiguous;
4. migrate all current sorting consumers to one of the explicit names;
5. eventually remove `general_sorted` rather than silently changing what it means.

If the project later wants a single conventional `Sorted` spelling, it can introduce that name as a separate deliberate compatibility break. The current refactor does not need to settle that policy.

### Optional structural names

A more theorem-prover-neutral library could instead expose

```text
general_linked
general_pairwise
```

and define sorting-specific terminology on top. This mirrors Agda/Lean especially well. It is attractive if the same relations will be reused for properties such as uniqueness or graph paths.

For the current A Program codebase, however, `general_locally_sorted` / `general_strongly_sorted` communicate the migration intent more directly and avoid adding an extra abstraction layer merely for naming.

---

## 16. Proposed acceptance tests

The split should not be accepted based only on transitive `Nat` examples, because under transitivity local and strong are equivalent and a broken distinction can go unnoticed.

### 16.1 Non-transitive separation fixture

Add a small three-element cyclic relation.

Required positive case:

```text
general_locally_sorted CycleR [A, B, C]
```

Required negative case:

```text
general_strongly_sorted CycleR [A, B, C]
```

must be rejected when `A R C` has no constructor.

A reflexive extension of the cycle can satisfy the existing comparator-certificate shape while remaining non-transitive.

Suggested fixture names:

```text
tests/acceptance/sorted-local-nontransitive.p
tests/acceptance/sorted-strong-nontransitive-reject.p
```

### 16.2 Conversion: strong to local

Construct a strong proof for a short list and verify conversion without any `trans` argument.

Suggested fixture:

```text
tests/acceptance/sorted-strong-to-local.p
```

### 16.3 Conversion: local to strong

Use a genuinely transitive relation and verify that local evidence converts to strong evidence.

Suggested fixture:

```text
tests/acceptance/sorted-local-to-strong-transitive.p
```

### 16.4 QuickSort local theorem without transitivity

Use a directionally total but non-transitive relation/comparator and prove only local sortedness of the ordinary QuickSort result.

This is the decisive test that the new theorem actually has weaker assumptions than the old one.

Suggested fixture:

```text
tests/acceptance/generic-quick-local-sorted-result.p
```

### 16.5 QuickSort strong theorem from transitivity

For the normal transitive relation fixtures, prove the strong result by composing the local theorem with the conversion theorem.

Suggested fixture:

```text
tests/acceptance/generic-quick-strongly-sorted-result.p
```

### 16.6 Preserve ordinary-result theorem shape

Both public theorems should mention

```text
quickSort A (&le) xs
```

directly, with no global `*quickSort` witness syntax. The recent removal of global function-witness access should not be reversed by this change.

### 16.7 Keep permutation orthogonal

Sortedness and permutation should remain separate theorems. Neither local nor strong sortedness establishes that QuickSort retained the input elements.

### 16.8 Measure proof-graph effects

For representative list/proof sizes, record at least:

- solver steps;
- peak RSS;
- serialized Program image size if applicable;
- evidence/graph node counts if an existing diagnostic can expose them.

Compare:

```text
local theorem
strong theorem derived from local
current direct strong theorem
```

The expected logical evidence-position reduction is clear, but actual pointer-core performance should be measured rather than inferred from asymptotics alone.

---

## 17. Migration plan

### Phase 1 — Introduce explicit predicates

Add:

```text
general_locally_sorted
general_strongly_sorted
```

Keep current `general_sorted` unchanged for compatibility.

### Phase 2 — Add conversion library

Add and test:

```text
strongly_sorted_to_locally_sorted
locally_sorted_to_strongly_sorted
```

The second theorem alone takes transitivity.

### Phase 3 — Prove QuickSort locally

Refactor the QuickSort acceptance proof so the recursion theorem targets `general_locally_sorted` and takes only the comparator certificate as its relation-level assumption.

Keep partition `All` evidence initially; there is no need to optimize partition certificates in the same change.

### Phase 4 — Derive strong QuickSort

Build the strong theorem by applying the generic local-to-strong conversion under transitivity.

Remove explicit `refl` from the public theorem if no independent use remains.

### Phase 5 — Migrate the other sort proofs

The README currently reports universal Sorted proofs for insertion, tree, merge, and QuickSort. Audit each one under the same distinction:

- prove local correctness with the weakest assumptions the algorithm needs;
- derive strong correctness under transitivity;
- keep permutation/content correctness independent.

Do not assume every existing proof uses transitivity for the same reason; inspect each proof before changing its API.

### Phase 6 — Retire ambiguous naming

After all consumers use explicit local/strong names, deprecate and eventually remove legacy `general_sorted`.

Do not silently repoint the same name from strong to local semantics.

---

## 18. Expected effect on QuickSort proof architecture

Current architecture:

```text
partition directional All evidence
        |
        v
recursive strong proofs
        |
        +-- transitivity expands lower -> pivot -> upper
        |
        +-- reflexivity supplies pivot R pivot
        v
strong/pairwise sorted output
```

Recommended architecture:

```text
partition directional All evidence
        |
        v
recursive local proofs
        |
        +-- only boundary evidence is consumed
        v
locally sorted output
        |
        +-- optional generic transitive closure
        v
strong/pairwise sorted output
```

This separates two independent facts:

1. **the algorithm arranges neighboring output elements consistently with its comparator certificate**;
2. **a transitive relation lets neighboring facts imply all earlier/later facts**.

The second statement is mathematics about the relation, not QuickSort-specific control flow. It belongs in a reusable conversion theorem.

---

## 19. Why this fits A Program's design philosophy

`DESIGN-PHILOSOPHY.md` emphasizes a minimal but expressive dependent IR, explicit evidence, predictable checking, and “mechanisms, not policies.”

The proposed split follows that philosophy:

- no new Core term is required;
- no new surface syntax is required;
- no elaborator special case is required;
- both notions are ordinary IADTs;
- transitivity remains an explicit function argument where it is actually used;
- stronger evidence is derived by a library theorem rather than silently imposed on every sorting algorithm;
- the ordinary algorithm result remains the theorem subject.

In particular, using strong sortedness as the only predicate effectively bakes a transitive-closure policy into the standard sorting certificate. Separating local from strong keeps the primitive algorithm contract weaker and moves closure into reusable library logic.

---

## 20. Risks and cautions

### 20.1 Do not confuse logical equivalence with definitional equality

Under transitivity the two predicates are propositionally/interpreted-equivalent, but they are different indexed data structures. They should not be expected to reduce definitionally to each other.

### 20.2 Do not change `general_sorted` semantics in place

A silent weakening can make old theorem names appear to prove the same thing while actually returning less evidence.

### 20.3 Do not delete strong evidence merely for size reasons

Some consumers may intentionally rely on direct arbitrary-pair extraction. Strong sortedness is a useful certificate; it should become explicit, not disappear.

### 20.4 Do not over-generalize the QuickSort refactor into comparator redesign

The observation that `general_decision` already implies reflexivity is relevant, but redesigning the comparator certificate is a separate issue. The sortedness split can be implemented without simultaneously replacing `general_decision`.

### 20.5 Validate proposed `.p` syntax against current IADT rules

The direct `general_locally_sorted` sketch above should be compiled as a small acceptance fixture before being treated as canonical source. If the recursive schema occurrence for `y :: ys` requires a more explicit constructor-index form, adjust the encoding while preserving the same evidence semantics.

---

## 21. Concrete decision recommended by this audit

Adopt the following design target:

```text
// Weakest generic sorting certificate.
general_locally_sorted A R xs

// Explicit all-earlier/all-later certificate.
general_strongly_sorted A R xs
```

with:

```text
general_strongly_sorted A R xs
-> general_locally_sorted A R xs
```

and:

```text
Transitive R
-> general_locally_sorted A R xs
-> general_strongly_sorted A R xs
```

Then make QuickSort expose two theorems:

```text
ComparatorDecision R le
-> general_locally_sorted A R (quickSort A (&le) xs)
```

and

```text
Transitive R
-> ComparatorDecision R le
-> general_strongly_sorted A R (quickSort A (&le) xs)
```

The second theorem should be derived from the first via the generic conversion theorem.

Keep the current `general_sorted` semantics only as a temporary compatibility surface during migration, and do not silently change its meaning.

This division gives A Program the same conceptual separation found across mature proof ecosystems while retaining A Program's own proof-relevant, explicit-IADT design.

---

## 22. References

### A Program

1. Repository root and current pointer implementation status  
   https://github.com/repyt-margorp/a-program
2. Current generic QuickSort Sorted acceptance proof  
   https://github.com/repyt-margorp/a-program/blob/main/src/prototype/pointer/tests/acceptance/generic-quick-sorted-result.p
3. Design philosophy  
   https://github.com/repyt-margorp/a-program/blob/main/DESIGN-PHILOSOPHY.md
4. Agent/documentation rules  
   https://github.com/repyt-margorp/a-program/blob/main/AGENTS.md

### Rocq

5. `Coq.Sorting.Sorted` / local and strong sortedness  
   https://docs.rocq-prover.org/v8.19/stdlib/Coq.Sorting.Sorted.html
6. Mergesort correctness, including strong corollary under transitivity  
   https://docs.rocq-prover.org/v8.10/stdlib/Coq.Sorting.Mergesort.html

### Agda

7. `Linked`  
   https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.html
8. `Linked.Properties`, including `AllPairs => Linked` and transitive `Linked => AllPairs`  
   https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Linked.Properties.html
9. `AllPairs`  
   https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.AllPairs.html
10. Total-order Sorted properties  
    https://agda.github.io/agda-stdlib/v2.4/Data.List.Relation.Unary.Sorted.TotalOrder.Properties.html
11. Sorting algorithm record  
    https://agda.github.io/agda-stdlib/master/Data.List.Sort.Base.html

### Lean 4 / mathlib

12. `List.IsChain`  
    https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Chain.html
13. `List.Pairwise`  
    https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Pairwise.html
14. List sorting predicates  
    https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/List/Sort.html

### Idris 2

15. `Data.List`, including adjacency-based `sorted`  
    https://idris-lang.org/Idris2/base/source/Data.List.html
16. `Data.List.Quantifiers.All`  
    https://idris-lang.org/Idris2/base/docs/Data.List.Quantifiers.html

---

## 23. Follow-up implementation checklist

- [ ] Record exact repository SHA at implementation time.
- [ ] Add explicit local predicate.
- [ ] Rename/copy current strong structure under an explicit strong name.
- [ ] Add strong-to-local conversion without assumptions.
- [ ] Add local-to-strong conversion requiring transitivity.
- [ ] Add non-transitive cyclic relation acceptance fixtures.
- [ ] Refactor QuickSort recursion proof to target local sortedness.
- [ ] Verify the local QuickSort theorem has no `trans` argument.
- [ ] Verify the local QuickSort theorem has no independent `refl` argument.
- [ ] Derive the strong QuickSort theorem from local + transitivity.
- [ ] Compare the derived strong theorem against the current direct strong proof.
- [ ] Keep permutation/content theorem separate.
- [ ] Audit insertion/tree/merge sortedness under the same split.
- [ ] Measure solver steps, RSS, and evidence/image size before deleting old proof paths.
- [ ] Deprecate ambiguous `general_sorted` only after consumers migrate.
