# Generic finite-container sorting: current verification and design audit

Date: 2026-09-27
Status: design proposal; existing List prerequisites verified, proposed container interfaces not implemented or checked.
Execution successor: [Finite-Position Sorting](2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md).
The checklist below is the original audit proposal, not a second active work list.
Code baseline: main at `2a67ac3e6387ca30506a8ba81400819eb1831b00` (clean implementation checkout).
Tracking issue: https://github.com/repyt-margorp/a-program/issues/41
Related: #39 and #40 (local/strong sortedness foundation); this proposal does not reopen the resolved sortedness distinction.

## Problem List

1. **P1 — Define the semantic and operational boundaries of sorting finite containers**, without conflating finiteness, ordered traversal, lawful reconstruction, and native random-access capability.

## P1. Finite sorting beyond the current List backend

### Subjective (User)

English paraphrase of the user's 2026-09-27 request: submit the supplied generic-container sorting audit as an Issue and documentation PR; organizing how finite objects are sorted is important.

The user has not selected a particular dictionary, public name, permutation convention, implementation technique, or new language primitive. The interface names below are proposals from the supplied audit, not user-approved requirements.

### Objective (Code)

Fresh checks at the pinned revision:

- `tests/fixtures/sorted-proof-provider.p` provides the List/SizedList-based sorting foundation.
- `tests/fixtures/local-strong-sorted.p` defines local sortedness separately and aliases the existing stronger relation as `general_strongly_sorted`.
- `tests/acceptance/generic-quick-local-sorted-result.p` proves the ordinary QuickSort result locally sorted using directional comparator evidence. The strong-result proof additionally uses transitivity.
- `tests/fixtures/generic_sorted/content-proof.p` defines an inductive permutation relation with nil, keep, swap, and composition. The result theorem in `content-result-proof.p` relates the input to the actual QuickSort output, not merely a membership set. Duplicate multiplicities matter.
- The implementation is generic in element type/comparison but these theorems use a particular List representation. SizedList is used in the termination machinery; it is not already a generic container abstraction.
- A scoped search of test `.p` and compiler `.c` sources found none of the proposed names `FiniteContents`, `FiniteLinearView`, `sortToList`, `sortWithin`, `sortPermutation`, `IndexedSortKernel`. This does not prove equivalent encodings are impossible.
- README documents the current merge implementation as repeated insertion. A conventional two-front MergeSort complexity claim is not justified by that implementation.

Verification:

| Check | Result |
| --- | --- |
| `make -s pointer-check` | Pass |
| `bash tests/quick_result.sh build/pointer/pointer-check` | Pass, including negative, image reload and resume controls |
| Generic local ordinary-result theorem | Pass; 1,054,265 checker steps |
| Derived strong ordinary-result theorem | Pass; 1,087,230 checker steps |
| Generic QuickSort permutation-result theorem | Pass; 2,504,960 checker steps |

The last three checks used `--legacy-intrinsic-dot --steps 10000000 --imports PROVIDER SOURCE`.
Providers were assembled from existing repository fixtures, with import directive lines removed from appended modules:
base = sorted-proof-provider + local-strong-sorted + quick-sort-proof-common.
The local check used this base; the strong check appended generic-quick-local-sorted-result;
the content check appended generic-quick-sorted-result to the base, and checked the concatenation of
acceptance/generic-quick-sorted.p, fixtures/generic_sorted/content-proof.p and content-result-proof.p.
No theorem body was modified.

These are **prerequisite tests**, not proofs that the proposed view wrappers compile.
The full acceptance suite, sanitizer suite, new container implementations, and performance comparisons were not run.
Checker transition counts are not sorting runtime complexity measurements.

### Assessment

This is a design/library-extension issue, not a newly reproduced compiler bug.

Separate three questions:

1. **Sort into a List:** choose an observation `contents : X -> List A`, then apply a certified List backend.
2. **Sort within the original shape:** additionally provide lawful same-length reconstruction and prove shape/index preservation.
3. **Sort positions:** choose finite ordered positions, produce a bijection of positions, and act on the original representation.

A finite object does not by itself choose a traversal, ranking, or reconstruction operation.
Even a type-correct `contents` function can omit intended contents; adequacy must be specified against the intended model.
Strict positivity of an IADT does not establish any of these sorting laws.

A reasonable experiment is a proof-carrying backend with `sort`, a specified local or strong sortedness theorem, and a permutation theorem; stability and cost remain separate properties.
Do not erase the newly implemented local/strong distinction or demand a total-order package for every theorem.

For a linear view, require:
- contents after reconstruction equal the requested replacement sequence;
- shape/index preservation;
- reconstruction of original contents agrees observationally with the original;
- the selected traversal accounts for each intended position exactly once.

Start with homogeneous List and SizedList/Vec-like representations. Arbitrary dependent positions may hold different types, so not every same-length replacement is legal. Do not assume proof irrelevance or independence from length-equality witnesses without establishing it.

A value-list permutation theorem and a position bijection are related but not interchangeable:
duplicates and labels can distinguish positions with equal values. Fix the direction of the position map explicitly (for example, new position to old position).

Independent reference spot-check: Mathlib's `Tuple.sort` returns an `Equiv.Perm (Fin n)` for a tuple `Fin n -> A` under a linear order; `Tuple.monotone_sort` states monotonicity after reindexing. This is a concrete permutation-first precedent, not evidence that its assumptions or implementation transfer unchanged to A Program. [Mathlib Tuple.Sort documentation](https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Fin/Tuple/Sort.html).
This source was inspected; Lean was not executed.

Native indexed comparison/swap capabilities are a separate operational interface. Do not force linked lists into a random-access model, or infer complexity/stability from semantic view laws.

Adopted here: preserve the audit and expose these alternatives for review.
Deferred: public API selection, compiler changes, automatic derivation for arbitrary IADTs, coinductive/infinite sorting, and performance claims.

### Plan

- [x] Recheck current List-based sortedness and permutation prerequisites at a pinned revision.
- [x] Separate current observations from historical claims and untested proposals.
- [ ] Prototype sort-to-List with a specified finite contents observation and a certified backend.
- [ ] Prototype same-shape transport for List and one genuinely indexed homogeneous representation; prove the reconstruction laws.
- [ ] Compare this with finite-position permutation plus a lawful representation action before selecting the public interface.
- [ ] Add positive and negative law tests: omitted/duplicated positions, incorrect refill, wrong length, shape change, and duplicate elements.
- [ ] Specify traversal choices for trees and key/payload behavior for maps before claiming either supported.
- [ ] Measure representation-specific runtime and proof/checking costs separately if performance is in scope.

Completion for a later implementation: two distinct representations reuse the backend with checked sortedness and occurrence-preserving permutation, with all required view/action laws explicit and no unjustified arbitrary-container claim.
This documentation PR itself does not complete that implementation plan.

## Provenance and precedence

The supplied audit below is retained as a historical research record. Its SHA-256 before inclusion was
`3256054f6ccd70908ed8364174be84e7871750f368eac00a772bb2e0799ff807`.
Its older snapshot references and broad cross-language survey are not all independently revalidated by this submission.
In particular, the fresh local/strong observations above supersede any older description that treats that split as pending.
Sketch interfaces and pseudocode in the supplied report are not newly accepted syntax or tested implementations.
The addendum above is the active work list; historical recommendations below are review material, not additional approved commitments.

## Supplied audit (preserved)

# Generic Container Sorting, QuickSort, and MergeSort Audit

**Date:** 2026-09-27  
**Repository:** `repyt-margorp/a-program`  
**Audited branch:** `main`  
**Repository snapshot:** current `main` source/API state inspected on 2026-09-27; the original audit snapshot recorded `5c064c2` on 2026-09-26.  
**Revision:** expanded cross-language/C/C++ comparison and proof-assistant/refinement correction on 2026-09-27.  
**Status:** Design audit. Recommend a finite-linear-view/specification layer, with an optional capability-specific native-sequence layer. Coinduction/codata is intentionally **deferred to a separate discussion** rather than expanded in this revision.

---

## 1. Executive conclusion

The current A Program QuickSort is already substantially generic, but along the **element/order axis**, not along the **container-representation axis**. The current generic correctness development quantifies over an element type `A`, a relation `R`, a Boolean comparator, and proofs connecting that comparator to `R`; however, the data being sorted remains a `List A`, temporarily measured as a `SizedList A n` for well-founded recursion and partition reasoning.

That is not an unusual limitation if one looks only at the **surface sorting APIs** of Lean, Agda, Rocq, or Idris 2: their standard QuickSort/MergeSort functions overwhelmingly remain on representations such as `List` or `Array`. However, a deeper proof-oriented survey changes the interpretation substantially. Existing theorem-proving ecosystems already contain several of the generalizations relevant here, but they often place them in **different layers**: finite-index permutations, abstract sequence interfaces, ghost semantic contents, separation-logic specifications, module/refinement interfaces, and verified representation refinement. Lean Mathlib's `Tuple.sort`, for example, directly maps `f : Fin n -> α` to a permutation of `Fin n`; Agda separately develops finite-set permutations and permutation actions on functional vectors; Isabelle/Sepref refines a list interface (including indexed swap) to arrays; and F*/Pulse factors multiple imperative array-sorting algorithms through one polymorphic proof-carrying `array_sort` specification.

Accordingly, the earlier shorthand conclusion that “proof assistants do not really do generic container sorting” was **too broad**. A more accurate conclusion is: they rarely package all of these layers as one universal `sort : C A -> C A`, but theorem-proving libraries already contain much of the semantic and refinement machinery from which such a design can be assembled.

The central conclusion of this audit is therefore:

> A Program should not try to make QuickSort or MergeSort operate natively on an arbitrary strongly-positive IADT. It should first separate a **generic sorting specification** from a **sequence sorting backend**, and transport a verified sequence sort through an explicit, law-bearing finite view of another data structure.

Two distinct generic operations should be recognized:

1. **sort-to-sequence**: enumerate a finite structure and return a sorted `List A`;
2. **shape-preserving sort-through-view**: enumerate a finite structure in a declared linear order, sort those values, and refill the same shape.

The second operation requires much more than “being a container”. It requires a finite collection of positions, a chosen linear order on those positions, and a lawful reconstruction operation. In categorical language, an ordinary container gives a shape and a set/type of positions; it does **not** by itself choose a linear order of those positions.

C's `qsort` adds an important qualification to this conclusion. It is indeed highly generic, but on a **different axis**: it erases the element type while fixing the representation to a finite array/table of `nel` equal-width objects starting at `base`. Its contract is therefore not “sort any container”; it is closer to “sort any finite, linearly indexed, fixed-stride mutable sequence whose elements can be compared through a callback”. In A Program, the element-type part of this generality is already represented more safely by `A`; the useful C-inspired extension would be an explicit capability such as a **finite indexed/swappable sequence**, not `void *`-style byte erasure and not an arbitrary-IADT `Container` constraint.

This produces a three-layer architecture rather than only two layers: (1) a generic semantic sorting specification, (2) transport through finite content/linear views, and optionally (3) native capability-specific algorithms over an interface such as `SwappableSequence` or `RandomAccessSequence` when several representations genuinely need the same operational QuickSort. C++'s `std::sort` reinforces this distinction by abstracting over a random-access iterator range rather than over all containers, while `std::list` has its own `list::sort`.

The absence of a coinductive `Stream` in A Program is **not** the blocker for finite generic sorting. Coinduction/productivity and strict positivity are separate issues. Moreover, sorting an arbitrary infinite stream is generally not productive or even mathematically possible as a permutation into a globally nondecreasing stream. Coinductive streams should therefore not be introduced merely to make finite sorting “more generic”.

A low-risk architecture is available using facilities already conceptually present in the current system:

- keep QuickSort internally on `List` / `SizedList`;
- package its existing sortedness and permutation theorems as a proof-carrying sequence-sort witness;
- define a small explicit dictionary such as `FiniteContents A X` for finite enumeration;
- refine that to `FiniteLinearView A X` when same-shape reconstruction is meaningful;
- derive generic `sortToList` and `sortWithin` wrappers;
- instantiate `List` first, then a genuinely different second representation such as `SizedList`/`Vec` before generalizing further.

This gives A Program a principled generic theory of sorting without pretending that every IADT is naturally sortable, and without adding typeclasses, higher-kinded machinery, coinductive primitives, or new kernel rules.

---

## 2. Scope and terminology

This document audits four different notions that are easy to conflate:

| Axis | Question | Current A Program status |
|---|---|---|
| Element polymorphism | Can one implementation sort values of arbitrary `A`? | Yes, in the current generic QuickSort development. |
| Order polymorphism | Can the relation/comparator be abstract? | Yes, subject to supplied relation/comparator laws. |
| Sequence representation polymorphism | Can the same algorithm run over `List`, vector, array-like sequence, etc.? | Not currently established. |
| Element representation erasure | Can one implementation operate without statically knowing the byte-level element type? | C `qsort` does this with `void *` + element width; A Program normally does not need this because `A` gives typed polymorphism. |
| Position/access polymorphism | Can one algorithm operate through abstract indexing/swapping/iteration capabilities? | Not currently established; this is the most useful C/C++-inspired axis to investigate. |
| Arbitrary-container polymorphism | Can every finite/positive IADT be “sorted” without further structure? | This is not a well-posed goal in general. |

“Container” is also overloaded. In this audit:

- **categorical container** means a shape/position presentation roughly of the form `Σ s : S. P s -> A`;
- **collection** means a structure from which some finite multiset of `A` values can be enumerated;
- **linear container/view** means a collection whose selected positions have an explicit linear traversal order;
- **sequence** means a representation whose ordering of positions is intrinsic enough for QuickSort/MergeSort semantics to be direct.

The recommendation below deliberately avoids using the bare word `Container` as the proposed A Program abstraction name. It is too broad and would suggest guarantees that ordinary containers do not provide.

---

## 3. Audited A Program baseline

### 3.1 Current core and source-language context

The current README describes the promoted pointer core in `src/`, with a deliberately small current Core and richer source-level machinery above it. The source language includes Pi types, generative ADTs, indexed families, recursive schema, recursive/function fields, and the infrastructure needed by the current verified examples.

This matters because the proposed abstraction in this audit should remain a **source-level proof/data abstraction**. There is no evidence that generic sorting requires a new evaluator primitive, Core node, conversion rule, equality principle, or positivity exception.

The historical `DESIGN-PHILOSOPHY.md` remains useful background for the `*`/positivity/IADT direction, but current architectural claims in this audit follow the current README and source/tests where they differ from older design documents.

### 3.2 What the current generic QuickSort actually generalizes

The current acceptance development `tests/acceptance/generic-quick-sorted-result.p` is important because it already draws the correct boundary more clearly than a superficial reading of “generic QuickSort” might suggest.

The proof development:

- quantifies over `A`;
- quantifies over a relation `R : A -> A -> @`;
- takes a Boolean comparison function;
- takes laws connecting the Boolean decision to the logical relation;
- proves orderedness using a generic `Sorted`-style predicate over `List A`;
- starts from `xs : List A`;
- measures that list into `SizedList A n`;
- performs partition and well-founded recursive reasoning over the sized representation;
- returns/proves properties of the ordinary list result.

So the current theorem is best described as:

> **generic in elements and ordering; specialized to a list-shaped sequence representation.**

That specialization is not itself a defect.

### 3.3 Current merge status must not be overstated

The current README explicitly warns that the reported merge operation uses repeated insertion rather than a conventional linear two-front merge. Older merge audit material likewise distinguishes those fixtures from a complete verified MergeSort development.

Therefore this document treats “MergeSort” in two ways:

- as a comparison target against Lean/Agda/Rocq/Idris;
- as a future A Program sequence-sort backend that could inhabit the same generic specification once a conventional verified implementation exists.

This audit does **not** claim that current A Program already has a conventional linear-time verified MergeSort equivalent to the standard-library implementations discussed below.

---

## 4. The first design correction: a generic sort specification is not a generic sorting algorithm

There are two independent questions:

### 4.1 Generic specification

One can define, for a carrier `X`, what it means for a function to be a correct sort:

- its observable contents are a permutation of the input contents;
- its observable contents are sorted under `R`;
- if a same-shape operation is intended, its shape is preserved;
- optionally, it is stable under a declared notion of input order.

This specification can be highly generic.

### 4.2 Generic implementation

QuickSort and MergeSort are not merely proofs of that specification. Their operational structure assumes sequence-like capabilities.

QuickSort needs, in some form:

- a finite recursive measure;
- a pivot/decomposition operation;
- partition into two recursive subproblems;
- evidence that the recursive subproblems are smaller;
- concatenation/reassembly in a linear order.

MergeSort needs, in some form:

- a finite measure;
- a split into smaller subproblems;
- a linear notion of fronts/ordered iteration for merge;
- reassembly into the sequence order.

An interface strong enough to provide all of this for an “arbitrary container” has, in substance, already required that container to behave like a finite sequence. Calling it `Container` would obscure rather than increase generality.

The audit therefore recommends making the **specification generic first**, while leaving algorithms on the representation that naturally supports them.

---

## 5. Why arbitrary containers are not intrinsically sortable

A categorical container can be presented schematically as:

```text
C A  ~=  Σ (s : Shape). Position s -> A
```

The shape determines which positions exist. This is enough to talk about mapping values while preserving shape. It is not enough to talk about a sequence ordering.

### 5.1 A tree demonstrates the ambiguity

For a binary tree, at least the following traversals can be chosen:

- preorder;
- inorder;
- postorder;
- breadth-first order.

If we “sort the tree”, which traversal is required to become nondecreasing?

There is no representation-independent answer. If “sorted tree” instead means binary-search-tree ordering, that is a different invariant and generally changes the role of the tree structure itself.

### 5.2 A set demonstrates that same-container sorting may be meaningless

For an unordered finite set, sorting the set and returning another set has no observable effect if the set abstraction intentionally forgets order.

A meaningful operation is instead:

```text
finite set -> sorted List
```

Lean's mathlib follows exactly this style for `Finset.sort`: a finite unordered collection is converted to an ordered list rather than pretending that the finite set itself has become “sorted”. `Multiset.sort` similarly returns a `List`.

### 5.3 Maps and indexed structures add semantic constraints

For a map, is the ordering about keys, values, or entries? Reordering values while holding keys fixed may change semantics rather than merely alter representation order.

For an indexed IADT, the index may encode shape, size, invariants, or semantic information. A generic same-carrier sort must state precisely which indices and which shape evidence are preserved.

### 5.4 Conclusion

“Sortable container” is not a structural property implied by positivity. It is an additional **semantic interface**.

---

## 6. Cross-language comparison: what is actually generalized?

The most useful comparison is not simply “does language X have a generic `sort`?” The important question is **which axis of genericity is abstracted**:

1. element type;
2. ordering relation/comparator;
3. concrete sequence representation;
4. position-access capability;
5. mutation/state model;
6. input abstraction (array, list, range, iterator, enumerable, sequence, container);
7. output abstraction (same carrier, canonical list/array, sorted view, permutation/index);
8. allocation and normalization strategy;
9. correctness specification and proof strength;
10. stability/complexity guarantees and the cost model assumed by the abstraction.

On these axes, Lean, Agda, Rocq, Idris 2, C, and C++ make materially different choices. Those differences are more informative for A Program than the surface name `sort` or `qsort`.

### 6.1 Lean: several sequence representations, shared semantic ideas, not one universal container sorter

Lean is a particularly useful comparison because it combines a dependent type theory, proof-oriented APIs, and performance-oriented array operations.

#### 6.1.1 `List.mergeSort` is deliberately List-specific

Lean's current language reference exposes:

```text
List.mergeSort
  (xs : List α)
  (le : α -> α -> Bool := ...)
  : List α
```

It is documented as a **stable merge sort**. The definition visible to reasoning is intentionally simplified for verification, while the runtime replaces it with an efficient implementation that has been proved equivalent.

Two observations matter for A Program:

- the element type `α` and comparator are generic;
- the structural carrier is still `List α`.

Lean therefore does not require “container polymorphism” in order to regard its list merge sort as fully reusable and verified.

#### 6.1.2 `Array.qsort` is a distinct, array-native algorithm

Lean separately exposes:

```text
Array.qsort
  (as : Array α)
  (lt : α -> α -> Bool := ...)
  (lo : Nat := 0)
  (hi : Nat := as.size - 1)
  : Array α
```

The documentation describes this as an in-place quicksort over an array subrange. Internally, the implementation uses indexed positions and swaps around a pivot. This is not implemented by first pretending that every container is a List; it is a representation-specific algorithm whose operational assumptions match arrays.

This distinction is important: **Lean shares element/order polymorphism across representations, but not necessarily the recursive operational implementation**.

The `Array.qsort` implementation is also evidence that a proof-oriented language can maintain a pure/logical array model while compiling uniquely referenced arrays to efficient mutation. A Program therefore need not model C-style unsafe pointers merely to express a C-like “indexed sequence” abstraction.

#### 6.1.3 Lean added a separate `Array.mergeSort` rather than generalizing `List.mergeSort` over all containers

Lean 4.30.0 (2026-05-26) added an `Array.mergeSort`. The release notes explicitly contrast it with `List.mergeSort` and `Array.qsort`: the array merge sort is stable and has `O(n log n)` worst-case cost, and it has its own allocation/performance tradeoffs.

The associated array sort lemmas include permutation/sortedness/stability-oriented results. Again, the design is **representation-specific implementation plus shared semantic properties**, not one `SortableContainer` abstraction driving every algorithm.

#### 6.1.4 `Finset.sort` and `Multiset.sort` expose the semantic boundary especially clearly

Mathlib's:

```text
Finset.sort ... : List α
Multiset.sort ... : List α
```

construct sorted lists from unordered structures. For `Finset.sort`, the documentation explicitly says that it constructs a sorted list from the unordered set and uses merge sort; a theorem relates the result by permutation to the finset's list contents.

This is an excellent precedent for A Program's proposed `sortToList`:

```text
unordered finite structure
        -> chosen finite enumeration
        -> sorted List
```

Trying to return another `Finset` would erase the observable effect of sorting. Lean therefore distinguishes **ordering a representation** from **obtaining an ordered view of contents**.

#### 6.1.5 Lean's lesson for A Program

Lean suggests three separate layers:

- a semantic notion of sortedness/permutation;
- representation-specific sequence algorithms (`List`, `Array`);
- conversion/view APIs for unordered structures.

This is close to the architecture recommended here. The notable extra lesson from `Array.qsort` is that A Program may later add a native capability interface for indexed/swappable finite sequences, but that should be distinct from the broad `FiniteContents`/`FiniteLinearView` semantics.

### 6.2 Agda: categorical Containers exist, yet the standard `SortingAlgorithm` is explicitly over Lists

Agda is the strongest direct counterexample to the idea that a sufficiently general theory of containers should force sort itself to be defined over the categorical `Container` abstraction.

#### 6.2.1 Agda has an explicit shape/position `Container`

`Data.Container.Core` defines approximately:

```text
record Container ... where
  field
    Shape    : Set
    Position : Shape -> Set

⟦ S ▷ P ⟧ X = Σ[ s ∈ S ] (P s -> X)
```

This is the canonical shape-and-positions presentation. The library also has indexed containers and container morphisms.

So Agda is not lacking the abstraction that the current A Program discussion calls “general container”. It has it explicitly and mathematically.

#### 6.2.2 Nevertheless, Agda's sorting specification is `List A -> List A`

Current `Data.List.Sort.Base` defines:

```text
record SortingAlgorithm where
  field
    sort   : List A -> List A
    sort-↭ : ∀ xs -> sort xs ↭ xs
    sort-↗ : ∀ xs -> Sorted (sort xs)
```

The standard-library comment is unusually informative: the use of propositional permutation is deliberate so that a sorting algorithm can change only the **order** of elements and cannot alter the elements themselves.

`Data.List.Sort` then chooses MergeSort as an instance of this `SortingAlgorithm` and publicly re-exports `sort`, the permutation theorem, and the sortedness theorem.

This is almost exactly the proof-carrying backend abstraction recommended for A Program.

#### 6.2.3 Why Agda's Container does not automatically supply `sort : ⟦ C ⟧ A -> ⟦ C ⟧ A`

A `Container` provides positions, but not necessarily:

- a finite number of positions;
- a canonical total order on positions;
- an efficient split or partition operation;
- an efficient same-shape refill discipline suited to sorting.

A binary tree container illustrates this immediately: its positions can be enumerated preorder, inorder, postorder, or breadth-first. The categorical container structure itself does not select one of those traversal orders.

Therefore Agda's standard library makes the same separation argued for here: generic container theory and list sorting theory coexist without being collapsed into one interface.

#### 6.2.4 Agda's lesson for A Program

Agda gives direct support for two choices:

1. keep `SortingAlgorithm` as a law-bearing sequence sorter;
2. treat a finite linear traversal/refill as **extra structure on a container**, not as something positivity or `Container` automatically provides.

A Program can adopt this lesson even without copying Agda's universe/library organization.

### 6.3 Rocq 9.1: order abstraction is modular, representation remains List, and proof strength is stratified

Rocq's `Stdlib.Sorting.Mergesort` is useful for a different reason: it shows that the **logical assumptions on the ordering relation can be separated from the representation and from the strength of the theorem obtained**.

#### 6.3.1 The implementation is a modular List MergeSort

The library describes itself as a modular mergesort with `O(n log n)` complexity in the length of the list. Its main module is parameterized by a boolean order module:

```text
Module Sort (Import X : Orders.TotalLeBool').
```

Operationally, `sort` remains a function over lists. The implementation uses a stack of pending merges rather than defining a generic sorter over arbitrary inductive structures.

#### 6.3.2 Rocq deliberately requires only totality at the weakest layer

The library documentation says that the minimal assumption is totality of the boolean order. Transitivity is **not** mandatory for the basic module.

Under this weaker interface it can prove local/ordinary sortedness and permutation. It exposes results including:

```text
LocallySorted_sort
Sorted_sort
Permuted_sort
```

and then obtains:

```text
StronglySorted_sort
```

when a transitivity hypothesis is supplied.

This is relevant to A Program's recent distinction between local sortedness and stronger all-earlier/all-later notions. It suggests that the generic sorting backend should not bake the strongest possible order law into every layer unless the desired theorem actually needs it.

#### 6.3.3 Rocq's lesson for A Program

Rocq suggests separating:

- the computational comparator interface;
- the weakest laws necessary for execution/correctness;
- stronger relational laws needed for stronger sortedness theorems.

This is orthogonal to container generality, but it makes the proposed `SortingAlgorithm A R` interface more precise: there may be a small base witness plus refinements for transitivity, totality, stability, or strong sortedness rather than one monolithic record with maximal assumptions.

### 6.4 Idris 2: generic Foldable/Traversable infrastructure exists, but `sortBy` remains a List merge sort

Idris 2 is useful because it is closer to a dependently typed programming language than a theorem prover library, and because it already exposes general iteration interfaces.

#### 6.4.1 `Foldable` can turn a generic structure into a List

Current `Prelude.Interfaces` defines `Foldable t` with `foldr`, `foldl`, and:

```text
toList : t elem -> List elem
```

with a default implementation by `foldr (::) []`.

This is essentially a library-level precedent for the weak A Program proposal:

```text
FiniteContents A X
  contents : X -> List A
```

The difference is that A Program's proposed witness should make finiteness, chosen content semantics, and proof obligations explicit rather than inheriting Idris's general typeclass conventions blindly.

#### 6.4.2 `Traversable` preserves the outer structure but still does not make sort generic

Idris defines:

```text
interface (Functor t, Foldable t) => Traversable t where
  traverse : Applicative f => (a -> f b) -> t a -> f (t b)
```

Thus Idris can generically visit elements while rebuilding the same functorial shape.

Yet the standard List sort is still:

```text
sortBy : (a -> a -> Ordering) -> List a -> List a
sort   : Ord a => List a -> List a
```

This is strong evidence that `Traversable` is **not itself an operational QuickSort/MergeSort interface**. Traversability gives an ordered visit/rebuild discipline, but not necessarily an efficient native split, random access, swap, or merge operation.

#### 6.4.3 Idris's actual `sortBy` implementation is a stable List merge sort

The current source splits a list into two parts, recursively sorts both, and combines them with `mergeBy`. The implementation uses `assert_smaller` for recursive calls because the termination checker does not infer structural decrease from the locally computed halves.

The comments explicitly coordinate the split order and the tie-breaking behavior of `mergeBy` to retain stability.

That is a useful comparison with A Program's `SizedList`/measure approach: both systems face the fact that the algorithmic recursion is not syntactically the obvious tail recursion of the original List. Idris chooses a termination assertion here; A Program is building explicit measured/well-founded proof infrastructure.

#### 6.4.4 Idris's lesson for A Program

Idris suggests:

- a generic `contents`/`toList` layer is natural;
- a shape-preserving traversal is a stronger abstraction than Foldable;
- neither one automatically gives a good native QuickSort/MergeSort implementation;
- termination evidence for divide-and-conquer recursion should remain an algorithm-level concern.

### 6.5 C `qsort`: very generic in element representation, deliberately narrow in storage shape

The user's recollection that C's `qsort` is “very general” is correct, but it is essential to identify **what is general and what is fixed**.

POSIX, aligned with the ISO C interface, gives essentially:

```c
void qsort(
    void *base,
    size_t nel,
    size_t width,
    int (*compar)(const void *, const void *)
);
```

The contract says that `base` points to an array/table of `nel` objects, each object has `width` bytes, and `compar` determines their ordering.

#### 6.5.1 What C `qsort` abstracts

It abstracts at least these dimensions:

- **element static type**: the implementation sees `void *`, not `T *`;
- **element byte width**: supplied dynamically by `width`;
- **comparison/order**: supplied by a callback over pointers to two elements;
- **number of elements**: supplied dynamically by `nel`;
- **actual concrete record layout**: opaque to the sorting routine except for byte width and copying/swapping.

That is a major amount of genericity for C.

#### 6.5.2 What C `qsort` does *not* abstract

However, the representation is far from arbitrary:

- it is an **array/table** beginning at one base address;
- there are exactly `nel` elements;
- each element has one fixed width;
- positions are linearly indexed from `0` to `nel-1`;
- address calculation is effectively `base + i * width`;
- the implementation may reorder the array elements in place;
- there is no linked-list/tree/container navigation callback in the API.

So C `qsort` is not “sort any container”. It is more accurately:

> **sort any finite fixed-stride array of opaque records using a user-supplied comparator.**

That is a powerful but intentionally representation-constrained interface.

#### 6.5.3 `qsort` is not even a standards-level promise of the QuickSort algorithm

This is an important naming subtlety. The C/POSIX contract is a generic **sorting operation**. Despite the name `qsort`, the standards do not require the implementation to use QuickSort and do not impose a stability or complexity guarantee. Equivalent elements need not retain their input order.

For A Program, this argues strongly for keeping the public proof-bearing abstraction named something like `SortingAlgorithm` or `sort`, while treating QuickSort as one implementation/witness. A generic semantic API should not be overcommitted to a particular recursion scheme merely because its first backend happens to be QuickSort.

#### 6.5.4 Typed polymorphism already gives A Program the safer half of C's generality

C uses `void *` because C lacks parametric polymorphism in the relevant interface. A Program already has an element type parameter `A`.

Therefore A Program should **not imitate**:

```text
void * + byte width + casts
```

at source level.

The typed analogue is simply:

```text
A : @
```

plus an explicit representation capability.

The interesting missing abstraction is not “unknown element bytes”; it is **unknown finite linear storage implementation**.

### 6.6 A C-inspired native abstraction for A Program: generalize over capabilities, not over `Container`

If A Program eventually wants one QuickSort implementation to execute directly on multiple sequence representations without flattening to `List`, the C lesson points toward a finite indexed/swappable capability.

A schematic pure/state-passing interface could look like:

```text
SwappableSequence A X := {
  size : X -> Nat,
  get  : (x : X) -> Fin (size x) -> A,
  swap : (x : X) ->
         Fin (size x) ->
         Fin (size x) ->
         X,

  size_swap : ...,
  get_swap_left  : ...,
  get_swap_right : ...,
  get_swap_other : ...
}
```

The exact dependent signature would need adaptation to A Program's current equality/index ergonomics. A cleaner first prototype may index the representation by its fixed size:

```text
SwappableSequence A n X := {
  get  : X -> Fin n -> A,
  swap : X -> Fin n -> Fin n -> X,
  ...laws...
}
```

This says exactly what an array-style QuickSort needs: finitely many linearly addressable positions and a lawful exchange operation.

A more semantic, proof-first variant is:

```text
FinitePermutationView A X := {
  size    : X -> Nat,
  at      : (x : X) -> Fin (size x) -> A,
  permute : (x : X) -> Permutation (Fin (size x)) -> X,
  ...laws...
}
```

This is less directly executable as QuickSort but can serve as the specification of what swap sequences are allowed to do. The operational `swap` interface can then refine it.

#### 6.6.1 Why this is compatible with strong-positive IADT design

Nothing here requires the recursively defined interface itself to occur negatively. `A`, `X`, and `Fin n` are parameters/ordinary argument types to function fields. The proposed capability can be an ordinary source-level dictionary/IADT carrying operations and laws.

Thus the C-inspired abstraction does not require introducing a negative recursive datatype or coinductive primitive.

#### 6.6.2 Do not force `List` through random access merely for uniformity

A linked List *can* implement indexing and swapping extensionally, but doing so may cost `O(n)` per indexed operation. Running an array-style QuickSort through such an instance can destroy the intended performance.

This leads to a crucial distinction:

> **semantic genericity should be broader than operational genericity.**

`FiniteContents` / `FiniteLinearView` can include Lists, vectors, arrays, and explicitly traversed trees at the correctness level. A native `SwappableSequence` QuickSort should be used only for representations where indexed access/swap has the intended cost model.

The capability should therefore not be treated as a universal superclass of every sortable finite structure.

### 6.7 C++ makes the capability boundary explicit: `std::sort` uses random-access ranges, `std::list` has its own sort

C++ is useful as a bridge between C's byte-erased `qsort` and a typed A Program design.

The classic `std::sort` API is parameterized by `RandomIt` and optionally a comparator:

```text
sort(first : RandomAccessIterator,
     last  : RandomAccessIterator,
     comp)
```

The important abstraction is **not the container class**. It is the iterator/range capability, specifically random access.

A `std::list` does not provide random-access iterators, so generic `std::sort` cannot be applied to it. Instead `std::list` exposes its own `list::sort`, which can exploit linked-list structure and is stable.

This is perhaps the clearest mainstream precedent for the architecture A Program should consider:

- generic semantic notion of sorting;
- generic native algorithm over the **minimum capability it actually needs**;
- specialized algorithms for representations whose efficient primitives differ.

In other words:

> **Generalize over operations/cost structure, not over the noun “container”.**

### 6.8 QuickSort and MergeSort should not necessarily share one native representation interface

The C/C++ comparison also exposes a mistake that a too-eager A Program abstraction could make.

An array-style QuickSort naturally wants:

- finite size;
- random/indexed access;
- swap/exchange;
- subrange boundaries.

A linked-list MergeSort naturally wants:

- sequential decomposition;
- cheap head/tail or runs;
- cheap splicing/merge;
- no random access at all.

Therefore the right common abstraction is usually the **correctness specification** (`sorted + permutation`, optionally stability), not the entire operational algebra.

A Program should be willing to have:

```text
SortingAlgorithm A R          -- common semantic package
ListQuickSort                 -- current list/SizedList strategy
RandomAccessQuickSort         -- optional future capability-generic strategy
ListMergeSort                 -- future list-native merge sort
ArrayLikeMergeSort            -- optional future array-native strategy
```

without forcing all of them through one maximal `SortableContainer` record.

### 6.9 Ada 2022: perhaps the closest standard-language precedent for an index/swap sorting capability

Ada deserves substantially more weight in this audit than a casual language survey would normally give it. Its standard library contains **two different levels of sort generalization**, and the newer one is strikingly close to the `SwappableSequence` direction proposed for A Program.

#### 6.9.1 Generic array sorting

Ada defines `Ada.Containers.Generic_Array_Sort` approximately as:

```text
generic
   type Index_Type is (<>);
   type Element_Type is private;
   type Array_Type is array (Index_Type range <>) of Element_Type;
   with function "<" (Left, Right : Element_Type) return Boolean is <>;
procedure Ada.Containers.Generic_Array_Sort
   (Container : in out Array_Type);
```

This already generalizes over:

- index type;
- element type;
- concrete array type;
- ordering function.

The representation remains an Ada array, but it need not be one predefined array class. This is analogous to a statically typed and safer version of the representation family that C `qsort` targets.

Ada 2022 reference: https://www.adaic.org/resources/add_content/standards/22rm/html/RM-A-18-26.html

#### 6.9.2 `Generic_Sort`: `Before(index,index)` + `Swap(index,index)`

More importantly, Ada 2022 also standardizes:

```text
generic
   type Index_Type is (<>);
   with function Before (Left, Right : Index_Type) return Boolean;
   with procedure Swap (Left, Right : in Index_Type);
procedure Ada.Containers.Generic_Sort
   (First, Last : Index_Type'Base);
```

The language reference explicitly says that this reorders an **indexable structure** over `First .. Last`. `Before` compares the *elements identified by two indices*, while `Swap` exchanges the values identified by two indices.

This is a major precedent. The sorting algorithm does not need:

- an element type parameter;
- a pointer to storage;
- an array object;
- a `get` function returning an element at all.

The algorithm only needs the observable operations required by an in-place comparison sort:

```text
ordered finite index interval
+ compare positions
+ exchange positions
```

The element representation is completely hidden behind the two operations.

That is more abstract than C `qsort` in one sense: C erases the element type but still assumes contiguous fixed-width storage; Ada's `Generic_Sort` can target any representation for which the user can implement indexed comparison and swapping.

#### 6.9.3 Why this matters for A Program

A Program should record this as a serious design ancestor for a future native sorting capability. A minimal operational interface may not need to expose `get : Fin n -> A` at all if the algorithm only compares and swaps positions.

A proof-oriented variant could separate:

```text
IndexedSwapKernel n X := {
  before : X -> Fin n -> Fin n -> Bool,
  swap   : X -> Fin n -> Fin n -> X,
  ... laws ...
}
```

from a richer semantic view that relates indexed positions to values:

```text
IndexedContents A n X := {
  at : X -> Fin n -> A,
  ...
}
```

This split could reduce proof obligations inside the algorithm while retaining a value-level specification outside it.

The strongest lesson is historical: **capability-based sort is not merely a C++ template trick. It is present directly in a language standard as an index/compare/swap algebra.**

---

### 6.10 Go: one language contains both operational capability sorting and collect-to-slice sorting

Go is unusually useful because the standard library has passed through two visibly different genericity styles.

#### 6.10.1 Historical `sort.Interface`: `Len`, `Less`, `Swap`

The classic `sort` package defines:

```text
type Interface interface {
    Len() int
    Less(i, j int) bool
    Swap(i, j int)
}
```

The algorithm therefore knows nothing about the element type. It knows only:

- finite length;
- how to compare two indexed positions;
- how to swap two indexed positions.

This is almost the same operational algebra as Ada `Generic_Sort` and is even closer to the first A Program sketch of `SwappableSequence`.

Current documentation still records this interface and requires `Less` to describe a strict weak ordering.

Reference: https://pkg.go.dev/sort

#### 6.10.2 Modern generics: `slices.Sort`

With Go generics, the preferred APIs can be more direct:

```text
func Sort[S ~[]E, E cmp.Ordered](x S)
func SortFunc[S ~[]E, E any](x S, cmp func(a, b E) int)
```

Here Go regains the element type statically but **narrows the carrier to slice-like values**. This is a useful reminder that more expressive static typing does not force a more abstract container API. Sometimes a concrete sequence representation is the right operational boundary.

#### 6.10.3 `slices.Sorted`: iterator input, canonical slice output

Go 1.23 added another layer:

```text
func Sorted[E cmp.Ordered](seq iter.Seq[E]) []E
func SortedFunc[E any](seq iter.Seq[E], cmp func(E,E) int) []E
```

The implementation contract is conceptually:

```text
iterator -> collect into []E -> sort -> []E
```

This is exactly the broad semantic pattern proposed as `FiniteContents -> sortToList`, except Go chooses a slice as the canonical output.

Reference: https://pkg.go.dev/slices

#### 6.10.4 Go's lesson

Go demonstrates that these are **not competing designs**:

- `Len/Less/Swap` is operational genericity;
- `[]E` is representation-specific generic programming;
- `iter.Seq[E] -> []E` is semantic collect-and-sort.

A mature A Program library can likewise expose all three at different levels without pretending they are one abstraction.

---

### 6.11 Zig: typed slices at the high level, index/swap contexts at the low level

Current Zig standard-library sorting is another unusually direct precedent.

The ordinary API sorts typed slices:

```text
sort(T, items : []T, context, lessThanFn)
sortUnstable(T, items : []T, context, lessThanFn)
```

but the implementation lowers these to context-based algorithms. The context variants operate on an index interval and expect the context to supply methods equivalent to:

```text
lessThan(a_index : usize, b_index : usize) -> bool
swap(a_index : usize, b_index : usize) -> void
```

For example the current `std.sort` source documents `insertionContext`, `heapContext`, and PDQ-sort context variants in this style; `std.mem.sortUnstableContext` exposes the lower-level interface publicly.

Source: https://github.com/ziglang/zig/blob/master/lib/std/sort.zig

Source: https://github.com/ziglang/zig/blob/master/lib/std/mem.zig

#### 6.11.1 Why Zig is stronger evidence than a toy abstraction

Zig uses this mechanism for real nontrivial representations. `MultiArrayList`, a structure-of-arrays representation where different fields live in separate arrays, builds a sort context whose `swap` exchanges the corresponding entries across **all fields**, then feeds that context to the generic sort kernel.

Source: https://github.com/ziglang/zig/blob/master/lib/std/multi_array_list.zig

That is a concrete demonstration of why `compare-position + swap-position` can be more expressive than `[]T`: the logical “record” being sorted need not be physically stored as one contiguous array of records.

#### 6.11.2 A Program lesson

If A Program later wants native sorting over multiple physical layouts, an index-operation kernel can be more general than a value-returning `get` abstraction and can avoid exposing representation details to the sorting algorithm.

The proof layer can then establish that each `swap` corresponds to a transposition of the semantic contents.

---

### 6.12 D: range concepts make the required capabilities explicit

D's Phobos library is one of the clearest mainstream examples of **algorithm requirements stated as composable capabilities**.

Its general sorting algorithm operates on random-access ranges and related operations use constraints such as:

```text
isRandomAccessRange!Range
hasLength!Range
hasSlicing!Range
hasSwappableElements!Range
hasAssignableElements!Range
```

The `sort` documentation describes sorting a random-access range and returns a `SortedRange` wrapper. Partition and selection algorithms expose related but not identical capability requirements.

Reference: https://dlang.org/phobos/std_algorithm_sorting.html

#### 6.12.1 `makeIndex`: sort the index, not the data

D also provides `makeIndex`, which constructs a sorted array of pointers or integer indices into another range. The documentation explicitly motivates this as useful when:

- the original collection is immutable;
- the original collection lacks random access;
- multiple independent orderings/indexes are desired;
- moving large objects would be expensive.

This is highly relevant to A Program. “Sorting” need not mean physically rebuilding the original carrier. A proof-carrying result can instead be a **permutation/index witnessing a sorted view**.

That suggests an additional semantic output form:

```text
sortPermutation : X -> Permutation (Fin n)
```

with a theorem that applying the permutation to the contents yields a sorted sequence.

This can be especially attractive for indexed IADTs whose physical rebuilding is awkward or semantically undesirable.

---

### 6.13 Rust: strong element/comparator genericity deliberately stops at mutable slices

Rust's standard sort methods live on slices:

```text
[T]::sort
[T]::sort_by
[T]::sort_by_key
[T]::sort_unstable
[T]::sort_unstable_by
```

The stable and unstable variants have explicit complexity and allocation contracts. `sort_unstable_by` accepts an arbitrary comparison closure, but the carrier remains `&mut [T]`.

Reference: https://doc.rust-lang.org/std/primitive.slice.html

This is a significant negative result for over-generalization: Rust has an exceptionally rich iterator ecosystem, traits, associated types, and generic programming, yet the core in-place sort is **not** defined for arbitrary `Iterator`, `IntoIterator`, or collection traits.

The reason is operationally obvious: iterators do not promise writable random positions or a representation that can be rearranged in place.

Rust therefore supports the same design principle as C++/Swift/D:

> put native reordering on a sequence abstraction whose operational guarantees match the algorithm; use collection/iterator conversion outside that kernel.

For A Program, this argues against making `Foldable`-like enumeration sufficient for a native QuickSort implementation.

---

### 6.14 Swift: `MutableCollection` + `RandomAccessCollection` as a type-level capability conjunction

Swift provides an especially clean protocol-level formulation. The mutating `sort()` is a method of mutable collections but is available when:

```text
Self : RandomAccessCollection
Element : Comparable
```

and `sort(by:)` similarly requires the mutable random-access collection capability.

Reference: https://developer.apple.com/documentation/swift/mutablecollection/sort%28%29

`RandomAccessCollection` promises O(1) index movement and distance computation, while `MutableCollection` supplies writable positions and operations such as `swapAt`.

Reference: https://docs.swift.org/latest/documentation/swift/randomaccesscollection/

This is almost a high-level typed statement of the cost-sensitive interface A Program would want for an efficient indexed sort:

```text
finite linear indices
+ efficient random access
+ mutation/replacement
+ comparator
```

The important part is that **the cost model is part of the protocol choice**. Swift does not merely ask whether positions can theoretically be reached; it distinguishes efficient random access from weaker bidirectional/forward traversal.

That supports making an eventual A Program native sort capability stronger than mere extensional `get : Fin n -> A` if the library wants algorithmic guarantees.

---

### 6.15 Java: broad `List.sort` semantics implemented by array normalization and refill

Java is a particularly strong precedent for separating a broad semantic carrier from the efficient backend representation.

#### 6.15.1 Arrays remain representation-specific

`Arrays.sort` has separate overloads for primitive arrays and object arrays. Current JDK documentation exposes different algorithmic notes: primitive arrays may use Dual-Pivot Quicksort-style implementations, while object arrays are required to be stable and use an adaptive merge-style implementation.

Reference: https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/util/Arrays.html

#### 6.15.2 `List.sort` is broader than random-access lists

`List.sort(Comparator)` applies to any **modifiable List**, including linked lists. The Java SE 26 documentation states that the default implementation:

1. obtains an array containing all list elements;
2. sorts that array;
3. iterates through the list and writes the sorted elements back.

It explicitly says this avoids the `n^2 log(n)` behavior that would result from repeatedly random-accessing a linked list.

Reference: https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/util/List.html

#### 6.15.3 This is almost exactly `FiniteLinearView`

Abstractly Java is doing:

```text
List carrier
   -> linear array view/copy
   -> efficient backend sort
   -> refill same list positions
```

That is extremely close to the proposed A Program architecture:

```text
X
 -> toList/toSequence
 -> verified backend sorter
 -> refill
 -> X
```

Java therefore supplies a mainstream, performance-motivated example where **semantic genericity is broader than the operational sort kernel**, with an explicit normalization/refill boundary between them.

For A Program this is one of the strongest arguments that `FiniteLinearView` is not merely a theorem-prover convenience.

---

### 6.16 Kotlin: `Iterable -> List` and `MutableList -> unit` are deliberately separate APIs

Kotlin makes the two semantic forms explicit in function names and types:

```text
Iterable<T>.sortedWith(comparator) : List<T>
MutableList<T>.sortWith(comparator) : Unit
```

Reference: https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.collections/-mutable-list/

The first operation accepts a broad enumerable input but **normalizes the output to `List`**. The second preserves/mutates the existing linear carrier but therefore requires a mutable list.

This is a concise language-level demonstration that:

```text
broad input abstraction + canonical output
```

and

```text
narrower mutable linear abstraction + same-carrier reorder
```

should be separate operations.

That separation maps almost directly to A Program's proposed `FiniteContents` versus `FiniteLinearView`/native capability split.

---

### 6.17 Scala: stable sequence sorting coexists with array-specific QuickSort

Scala's collection layer exposes `sorted`, `sortWith`, and `sortBy` on sequences using an `Ordering[T]`. The current `Seq` documentation notes that the sort is stable and that it forces lazy collections/views rather than remaining lazy.

Reference: https://www.scala-lang.org/api/3.x/scala/collection/immutable/Seq.html

Separately, `scala.util.Sorting.quickSort` is fundamentally an array-oriented utility. `Ordering[T]` is a first-class strategy object/typeclass-like value that can be constructed by projection or composition.

References:

- https://www.scala-lang.org/api/current/scala/util/Sorting%24.html
- https://www.scala-lang.org/api/3.x/scala/math/Ordering.html

Scala therefore separates at least three concerns:

- the ordering dictionary;
- a high-level sequence sort;
- an array-native QuickSort path.

For A Program the interesting part is not Scala's exact collection-builder machinery, but the refusal to identify “an ordering” with “a specific sorting algorithm or representation”. The order witness should be reusable independently of the carrier and algorithm.

---

### 6.18 .NET and F#: array/list native sorts versus sequence-level ordered views

The .NET ecosystem exposes both representation-specific mutation and broad sequence ordering.

#### 6.18.1 `Array.Sort` and `List<T>.Sort`

`Array.Sort` and `List<T>.Sort` mutate their carriers and use comparer interfaces/delegates. Current .NET documentation describes `List<T>.Sort` as using `Array.Sort`, with introspective sorting that combines insertion sort, heapsort, and quicksort and is not stable.

References:

- https://learn.microsoft.com/dotnet/api/system.array.sort
- https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1.sort

#### 6.18.2 LINQ `OrderBy`: broad sequence input, ordered sequence result

LINQ instead accepts:

```text
IEnumerable<TSource>
```

and returns:

```text
IOrderedEnumerable<TSource>
```

Reference: https://learn.microsoft.com/en-us/dotnet/api/system.linq.enumerable.orderby

This is not same-carrier mutation at all. It is an ordered view/pipeline abstraction over sequence contents.

#### 6.18.3 F# exposes representation-specific families explicitly

F# keeps distinct modules:

- `List.sort : 'T list -> 'T list`, stable;
- `Array.sort : 'T array -> 'T array`, returns a new array and is documented as not stable;
- `Array.sortInPlace`, mutating;
- `Seq.sort : seq<'T> -> seq<'T>`, stable but consumes the whole input sequence when enumerated and is inappropriate for infinite sequences.

References:

- https://fsharp.github.io/fsharp-core-docs/reference/fsharp-collections-listmodule.html
- https://fsharp.github.io/fsharp-core-docs/reference/fsharp-collections-arraymodule.html
- https://fsharp.github.io/fsharp-core-docs/reference/fsharp-collections-seqmodule.html

The .NET/F# family therefore reinforces the distinction between:

- physical reordering of a concrete mutable representation;
- rebuilding a representation-specific immutable value;
- ordering an abstract enumerable sequence.

---

### 6.19 Python and Ruby: perhaps the clearest dynamic-language form of `FiniteContents -> canonical sequence`

Dynamic languages are useful here because they remove static type-system limitations from the explanation. If arbitrary-container sort were intrinsically natural, dynamically typed libraries could expose it very easily. Usually they do not.

#### 6.19.1 Python

Python's built-in:

```text
sorted(iterable, key=None, reverse=False) -> list
```

accepts **any iterable** but always returns a new list.

Reference: https://docs.python.org/3/library/functions.html#sorted

By contrast, `list.sort` mutates only lists. Thus Python cleanly separates:

```text
arbitrary enumeration -> canonical ordered list
```

from

```text
native same-carrier list reorder
```

This is almost exactly the proposed distinction between `FiniteContents` and a stronger linear mutable/refill abstraction.

#### 6.19.2 Ruby

Ruby's `Enumerable#sort` similarly enumerates an arbitrary `Enumerable` and returns an `Array`; the current implementation literally converts the enumerable to an array and sorts it. `Array#sort!` is the in-place carrier-specific operation.

References:

- https://ruby-doc.org/3.4.1/Enumerable.html
- https://ruby-doc.org/3.4.1/Array.html

Again, broad content abstraction does **not** imply preservation of the original container type.

---

### 6.20 Common Lisp: a genuine broad same-sequence sort, because `sequence` is already a linear abstraction

Common Lisp is an important counterweight to the repeated “convert to list/array” pattern.

The Common Lisp HyperSpec defines:

```text
sort        sequence predicate &key key -> sorted-sequence
stable-sort sequence predicate &key key -> sorted-sequence
```

and says these operations destructively sort a **proper sequence**. If the input is a vector, the result is a vector with the same actual array element type; if the input is a list, the result is a list.

Reference: https://www.lispworks.com/documentation/HyperSpec/Body/f_sort_.htm

This is a genuinely broad same-family sorting abstraction.

But the important qualification is the meaning of Common Lisp `sequence`: it is not “arbitrary container”. It is a language-defined abstraction over **linearly ordered sequences**, principally lists and vectors, with positions that already have an iteration order.

So Common Lisp does not refute the audit's central argument. It sharpens it:

> a generic same-carrier sort is natural when the abstraction itself guarantees a finite/linear positional interpretation suitable for reordering.

This suggests that A Program may eventually want an explicit source-level notion named something like `FiniteSequence`, distinct from a broad categorical `Container`.

`FiniteSequence` could be the semantic superclass of List/Vector-like structures, while trees, sets, and maps would require chosen views or different operations.

---

### 6.21 Clojure and Elixir: collections/enumerables are sorted into canonical sequential results

Clojure's:

```text
(sort coll)
(sort comp coll)
```

returns a **sorted sequence of the items in `coll`** and guarantees stability. The carrier itself is not generally preserved.

Reference: https://clojure.github.io/clojure/branch-master/clojure.core-api.html

Elixir is even more explicit:

```text
Enum.sort(enumerable) :: list()
Enum.sort(enumerable, sorter) :: list()
```

It accepts anything implementing the Enumerable protocol and returns a list. The documentation states that the implementation uses merge sort.

Reference: https://hexdocs.pm/elixir/Enum.html#sort/1

Both ecosystems choose the same architecture as Python/Ruby/modern Go iterator sorting:

```text
general enumeration boundary
        -> canonical linear result
        -> sorting semantics
```

For A Program this is strong cross-paradigm evidence for making `sortToList` (or a more neutral `sortToSequence`) a first-class generic operation rather than treating it as an embarrassing fallback.

---

### 6.22 Haskell: `Foldable.toList` is generic; `Data.List.sort` remains list-specific

Haskell's standard `Data.List.sort` and `sortBy` operate on lists. `sortBy` takes an explicit comparison function assumed to define a total ordering.

Reference: https://downloads.haskell.org/ghc/latest/docs/libraries/base/Data-List.html

By contrast, `Foldable` provides:

```text
toList :: Foldable t => t a -> [a]
```

which enumerates a foldable structure into a list.

Reference: https://downloads.haskell.org/ghc/latest/docs/libraries/base/Data-Foldable.html

The library therefore already contains the ingredients for generic “sort the contents” in user code:

```text
sort . toList
```

but it does **not** elevate this into a promise that every `Foldable t` can be sorted back into the same `t`.

That distinction is theoretically meaningful. `Foldable` supplies an ordered fold/enumeration; it does not generally supply a lawful inverse/refill preserving shape.

This is very close to the difference between proposed:

```text
FiniteContents
```

and

```text
FiniteLinearView
```

in A Program.

---

### 6.23 OCaml and Standard ML: representation-specific sorting has remained a durable ML-family design

OCaml exposes both `List.sort` and `Array.sort`. The comparator interface is similar, but the operational behavior is different: List sorting produces a list, Array sorting mutates an array in place.

Reference: https://ocaml.org/docs/higher-order-functions

SML/NJ is even more historically explicit. Its current utility library still contains:

```text
ListMergeSort.sort : ('a * 'a -> bool) -> 'a list -> 'a list
ArrayQSort.sort    : ('a * 'a -> order) -> 'a array -> unit
```

References:

- https://smlnj.org/doc/smlnj-lib/Util/str-ListMergeSort.html
- https://smlnj.org/doc/smlnj-lib/Util/str-ArrayQSort.html

The names themselves encode the representation/algorithm pairing: merge sort for immutable linked lists, QuickSort for mutable arrays.

This long-lived design is useful evidence against assuming that one universal operational sorter is inherently more elegant. In ML-family libraries, parametric polymorphism already makes element types general; representation-specific algorithm choice remains worthwhile.

---

### 6.24 Julia: rich ordering parameters, multidimensional axes, and permutation outputs

Julia's sorting API illustrates two further axes of generalization that A Program should record explicitly.

#### 6.24.1 Ordering configuration is richer than a binary comparator

`sort!` accepts:

```text
alg
lt
by
rev
order
```

and `Base.Order.Ordering` packages an ordering strategy separately from the algorithm.

Reference: https://docs.julialang.org/en/v1/base/sort/

This suggests that A Program should eventually distinguish:

- the semantic relation `R`;
- a decidable comparator implementing `R`;
- key projection;
- reversal/lexicographic composition;
- the chosen sorting algorithm.

They need not be collapsed into one comparator record.

#### 6.24.2 Multidimensional arrays require choosing an axis/linearization

Julia can sort arrays along a chosen `dims`, and `sortslices` sorts rows/columns/higher-dimensional slices. The requested dimension is part of the operation because a multidimensional shape does not have one unique “sort order”.

This directly parallels the tree traversal issue in A Program: **a structure can have contents without having one canonical linear position order**.

#### 6.24.3 `sortperm`: return a permutation instead of rebuilding contents

Julia's:

```text
sortperm(A) -> permutation indices
```

returns indices `I` such that `A[I]` is sorted.

This is closely related to D's `makeIndex` and suggests a proof-friendly A Program API:

```text
sortPermutation : (x : X) -> Permutation (Fin n)
```

A permutation result can be semantically cleaner than physically rebuilding complicated indexed structures, and the permutation theorem is already central to sorting correctness.

---

### 6.25 JavaScript, Lua, Erlang, and Scheme sorting libraries: dynamic typing still does not imply arbitrary-container sorting

A short survey of additional ecosystems reinforces the same boundary.

#### JavaScript

`Array.prototype.sort()` mutates arrays. `TypedArray.prototype.sort()` separately sorts typed arrays; MDN explicitly notes that the typed-array method is **not generic** and applies only to typed-array instances.

Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/sort

The language therefore has separate native sequence families rather than one arbitrary-object sort.

#### Lua

`table.sort(list, comp)` sorts positions `1 .. #list` of a table in place. A Lua table is extremely general as a data structure, but the sorting API intentionally interprets only its **array/list part** as sortable.

Reference: https://www.lua.org/manual/5.4/manual.html#pdf-table.sort

This is a very clean dynamic-language example of adding a *linear-view precondition* to an otherwise general container representation.

#### Erlang

Erlang's standard `lists:sort` and merge functions are explicitly list operations. The language has tuples, maps, and other structures, but sorting is not lifted over all terms as containers.

Reference: https://www.erlang.org/doc/apps/stdlib/lists.html

#### Scheme / SRFI 132

SRFI 132 specifies a full sorting toolkit with **separate list and vector operations**, rather than one arbitrary Scheme-object sorter.

Reference: https://srfi.schemers.org/srfi-132/srfi-132.html

The repeated conclusion is robust across static and dynamic languages: **genericity normally stops at a meaningful linear sequence boundary, or else the operation produces a canonical linear result.**

---

### 6.26 PHP, R, MATLAB, and Nim: preserving associations, returning permutations, and making the linear axis explicit

These ecosystems add several dimensions that are easy to miss if the comparison is limited to `List` versus `Array`.

#### 6.26.1 PHP: “what must be preserved?” is part of the sort operation

PHP's `array` is not merely a C-style vector. It is an ordered map-like structure with keys and values, and the standard library therefore exposes multiple sorting operations with deliberately different preservation semantics.

The PHP manual distinguishes, among others:

- `sort`: sort by values **and discard/reassign existing keys**;
- `asort`: sort by values **while preserving key-value association**;
- `ksort`: sort by keys while preserving their associated values;
- user-comparator variants such as `usort`/`uasort` with corresponding association choices.

Since PHP 8.0, equal elements retain their relative order in these sorting functions.

This is a useful warning for A Program. A theorem of the form

```text
Permutation (values x) (values (sort x))
```

may be insufficient when the carrier contains additional observational structure. For an associative carrier, correctness may instead need to state that sorting acts on **records/pairs** or preserves a key-value relation:

```text
Permutation (entries x) (entries y)
```

not merely that the multiset of projected values is unchanged.

So a future generic sorting interface should make its **preservation object** explicit. Depending on the abstraction, this may be:

- elements;
- key-value entries;
- labelled positions;
- an index permutation;
- or a shape plus a position-labelling function.

This matters directly to dependent/IADT representations, where indices can carry semantics that must not be silently erased by a “sort”.

#### 6.26.2 R: `sort` and `order` separate reordered values from the ordering permutation

R's base API separates two common products of sorting:

```text
sort(x)   -- sorted values
order(...) -- a permutation/order of positions
```

and lower-level `sort.int` can also request an index result. This is another mature example of treating the permutation as useful computational data rather than only as a proof that the value result is a rearrangement.

For A Program, the R/Julia/D family suggests that

```text
sortPermutation : X -> Permutation (Fin n)
```

could be a first-class generic interface, with actual carrier reconstruction factored separately.

#### 6.26.3 MATLAB: multidimensional arrays force the linearization axis to be explicit

MATLAB R2026b's `sort` demonstrates a different issue. For a vector, there is an obvious one-dimensional order. For matrices and multidimensional arrays, however, sorting requires an **axis**:

```text
B = sort(A, dim)
[B, I] = sort(A, dim)
```

`B` has the same size and type as `A`, while `I` records the index rearrangement along the selected dimension. The default chooses the first non-singleton dimension, but the API exposes `dim` because there is no unique global linear order intrinsic to an n-dimensional array.

This is directly relevant to the earlier tree discussion. The issue is broader than trees: even a highly regular finite positive structure can have **several legitimate linear position systems**. Thus `FiniteLinearView` should be understood as evidence of a chosen linearization, not as something automatically derivable from “finite container”.

A more explicit future name such as

```text
Linearization A X
```

or a parameterized family

```text
FiniteLinearView Axis A X
```

may eventually be preferable if A Program gains matrix/tensor-like indexed structures.

MATLAB also returns the sort index alongside the sorted value, reinforcing the usefulness of first-class permutation results.

#### 6.26.4 Nim: `openArray` gives a deliberately bounded representation abstraction

Nim's standard `algorithm` module defines stable merge sort over

```text
var openArray[T]
```

and a non-mutating `sorted(openArray[T]) -> seq[T]`. `openArray` abstracts over array/sequence-like contiguous indexable inputs without pretending to be a universal collection interface.

This is a useful middle point between C arrays and C++ iterator concepts:

- broader than one concrete `seq` type;
- narrower than arbitrary iterable/container;
- strong enough to make the implementation and complexity meaningful.

The same pattern supports the recommendation that A Program should prefer **small capability/representation classes with clear laws** over one maximal `SortableContainer` concept.

---

### 6.27 The historical design lineages that emerge

The expanded survey is easier to understand as several recurring lineages rather than as twenty unrelated APIs.

#### Lineage I — erased contiguous records

Canonical example:

```text
C qsort(base, count, width, comparator)
```

Generalized:

- element representation/type is erased;
- comparator is injected.

Fixed:

- contiguous array;
- fixed element width;
- mutable in-place reordering.

#### Lineage II — index/compare/swap operational kernels

Canonical examples:

```text
Ada 2022 Generic_Sort
Go sort.Interface
Zig *Context sort kernels
```

Generalized:

- physical representation;
- sometimes even element type;
- logical record layout.

Required:

- finite index range/length;
- comparison of indexed positions;
- exchange of indexed positions.

This is the strongest historical family for a future A Program `SwappableSequence`/`IndexedSortKernel`.

#### Lineage III — random-access/range capability algorithms

Canonical examples:

```text
C++ std::sort
Swift MutableCollection + RandomAccessCollection
D random-access ranges
```

Generalized:

- representation through iterator/range protocols;
- element type;
- ordering.

Required:

- explicit traversal/access complexity and mutation capabilities.

This lineage says that **cost-relevant capabilities should appear in the interface**, not merely extensional reachability of positions.

#### Lineage IV — representation-specific algorithm families

Canonical examples:

```text
Rust slice sort
OCaml List.sort / Array.sort
SML/NJ ListMergeSort / ArrayQSort
Lean List.mergeSort / Array.qsort
Java Arrays.sort
```

Generalized:

- element type/order.

Fixed:

- sequence representation chosen to match the algorithm.

This remains an entirely respectable design even in languages with sophisticated generic programming.

#### Lineage V — enumerate/collect then return a canonical ordered sequence

Canonical examples:

```text
Python sorted(iterable) -> list
Ruby Enumerable#sort -> Array
Elixir Enum.sort -> list
Go slices.Sorted(iter.Seq) -> []E
Kotlin Iterable.sortedWith -> List
Clojure sort -> sorted sequence
```

Generalized:

- broad source container/enumeration.

Fixed:

- canonical ordered result representation.

This is the strongest precedent for A Program `FiniteContents -> sortToList`.

#### Lineage VI — broad linear-sequence abstraction with same-family result

Canonical example:

```text
Common Lisp sort(sequence)
```

and, in a more statically constrained form, Java `List.sort` or Swift's mutable random-access collections.

The key is that the abstract carrier is already a **linear sequence**, not an arbitrary container. This lineage motivates a possible A Program `FiniteSequence` concept above concrete List/Vector types but below general IADTs.

#### Lineage VII — sort an index/permutation instead of the carrier

Canonical examples:

```text
D makeIndex
Julia sortperm
```

This is particularly attractive for proof-oriented systems because the mathematical essence of sorting is already a permutation. It may avoid unnecessary rebuilding and offers a natural bridge from semantic correctness to multiple physical representations.

---

### 6.28 Extended comparative matrix

| System/API | Broad input abstraction | Native operational requirement | Output form | Same carrier preserved? | Notable lesson for A Program |
|---|---|---|---|---:|---|
| C `qsort` | fixed-width record array | contiguous storage + count + width + comparator | same array storage reordered | Yes | erase element type, but representation remains strong |
| Ada `Generic_Array_Sort` | arbitrary Ada array type | array indexing + `<` | same array | Yes | statically generic array family |
| Ada `Generic_Sort` | arbitrary indexable structure | index interval + `Before` + `Swap` | hidden carrier reordered | Yes | near-direct ancestor of index/swap capability |
| C++ `std::sort` | random-access iterator range | random access + permutation/swapping | same range reordered | Yes | generalize over capabilities, not container class |
| Go `sort.Interface` | arbitrary user representation | `Len` + `Less(i,j)` + `Swap(i,j)` | hidden carrier reordered | Yes | index/swap operational interface |
| Go `slices.Sort` | slice family | mutable slice | same slice reordered | Yes | typed representation-specific genericity |
| Go `slices.Sorted` | `iter.Seq` | collect then slice sort | new slice | No | semantic enumeration-to-sequence layer |
| Zig context sorts | arbitrary context | index interval + `lessThan` + `swap` | hidden carrier reordered | Yes | works even for structure-of-arrays layouts |
| D `sort` | random-access range | random access/length/slicing/swappability | `SortedRange` over reordered range | Yes | capability predicates explicit |
| D `makeIndex` | forward/random-access source depending overload | writable index range | sorted pointers/indices | Original untouched | sorting can mean producing an index |
| Rust `[T]::sort*` | mutable slice | writable contiguous slice semantics | same slice reordered | Yes | rich traits do not imply arbitrary iterator sort |
| Swift `sort` | mutable random-access collection | mutation + efficient random access | same collection reordered | Yes | cost model encoded by protocol refinement |
| Java `Arrays.sort` | array | array operations | same array reordered | Yes | representation-specific algorithm family |
| Java `List.sort` | any modifiable `List` | default implementation normalizes to array and refills | same List abstractly | Yes | semantic carrier broader than backend representation |
| Kotlin `Iterable.sortedWith` | any iterable | enumeration + allocation | new List | No | broad input/canonical result |
| Kotlin `MutableList.sortWith` | mutable list | writable list | same list | Yes | separate API for same-carrier reorder |
| Scala `Seq.sorted` | sequence | forces finite sequence contents | sequence result | Sequence-level | ordering strategy independent from algorithm |
| Scala `Sorting.quickSort` | Array | array mutation | same array | Yes | separate native QuickSort path |
| .NET LINQ `OrderBy` | `IEnumerable<T>` | enumeration + buffering/order pipeline | `IOrderedEnumerable<T>` | No | ordered view rather than carrier mutation |
| .NET `List<T>.Sort` | List | mutable list/array backend | same list | Yes | native mutation path separate from LINQ |
| F# `Seq.sort` | `seq<'T>` | consume whole sequence | ordered sequence | No physical carrier promise | semantic sequence ordering |
| Python `sorted` | any iterable | enumerate + build list | new list | No | canonical sorted representation |
| Ruby `Enumerable#sort` | any Enumerable | `to_a` + array sort | Array | No | implementation literally normalizes to array |
| Common Lisp `sort` | proper sequence | mutable sequence positions | list remains list, vector remains vector | Yes | broad same-family sort works because `sequence` is linear |
| Clojure `sort` | collection | enumeration/buffering | sorted sequence | No general carrier preservation | content semantics over shape |
| Elixir `Enum.sort` | Enumerable | enumeration + merge sort | list | No | generic enumeration explicitly returns list |
| Haskell `Data.List.sort` | List | functional list operations | List | Yes as type | generic `Foldable.toList` remains a separate abstraction |
| OCaml `List.sort` | List | list-native functional sort | List | Yes as type | representation-specific |
| OCaml `Array.sort` | Array | mutable array | same Array | Yes | separate representation-specific path |
| SML/NJ `ListMergeSort` | List | list-native | List | Yes as type | algorithm chosen for representation |
| SML/NJ `ArrayQSort` | Array | mutable array | same Array | Yes | algorithm chosen for representation |
| Julia `sort!` | Vector/array axes | mutable indexed array | same array/axis reordered | Yes | ordering and algorithm independently parameterized |
| Julia `sortperm` | indexable collection/array | compare values, produce indices | permutation/index | Original untouched | permutation is a first-class sorting result |
| R `sort` / `order` | vector/data columns | value ordering or index ordering | sorted values or position order | Depends API | permutation/order is an independent product |
| MATLAB `sort` | vector/matrix/n-D array + chosen `dim` | indexed axis traversal | same-size sorted array + optional index array | Yes in shape/type | linearization axis is part of the specification |
| PHP `sort` / `asort` | ordered-map-like array | mutable array entries | same array, but key association policy differs | Depends operation | preservation law itself is an API choice |
| Nim `sort` / `sorted` | `openArray[T]` | mutable indexable array-like input | in-place input or new `seq[T]` | Depends API | bounded representation abstraction can be enough |
| JavaScript `Array.sort` | Array | mutable array | same Array | Yes | dynamic typing still keeps sequence boundary |
| JavaScript `TypedArray.sort` | TypedArray | typed contiguous sequence | same TypedArray | Yes | explicitly non-generic outside typed arrays |
| Lua `table.sort` | table's list part `1..#t` | integer linear positions | same table list-part reordered | Yes | general table becomes sortable only through linear view |
| Erlang `lists:sort` | List | functional list sort | List | Yes as type | non-list containers are not implicitly sortable |
| Scheme SRFI 132 | lists/vectors via separate procedures | representation-specific | list/vector | Depends API | explicit family rather than arbitrary-container sorter |
| Lean | List/Array/Finset etc. via distinct APIs | representation-specific + proofs | list/array/list-view | Depends API | shared semantic theorem, separate physical algorithms |
| Agda | List sorting despite general Container theory | List operations | List | Yes as type | categorical container ≠ sortable linear sequence |
| Rocq | List | list operations | List | Yes as type | order-law strength stratified from representation |
| Idris 2 | List sort; Foldable can `toList` | list merge sort | List | Yes for sort | enumeration abstraction separate from native sort |

---

### 6.29 Synthesis: five distinct meanings of “generic sort”

The broader survey supports a slightly finer taxonomy than the earlier three-way split.

#### Meaning A — element/order generic

```text
sort : (A, comparator/laws) -> List A -> List A
```

This is the minimum form shared by almost every typed language. Current A Program already does this substantially.

#### Meaning B — content/enumeration generic with canonical result

```text
X
 -> enumerate contents
 -> canonical List/sequence
 -> sort
```

Examples include Python `sorted`, Ruby `Enumerable#sort`, Elixir `Enum.sort`, Kotlin `Iterable.sortedWith`, Go `slices.Sorted`, Haskell `sort . toList`, Lean `Finset.sort` in spirit.

This maps to `FiniteContents`.

#### Meaning C — finite linear-sequence generic with shape/carrier preservation

```text
X
 -> ordered finite positions + refill/mutation
 -> same X (or same sequence family)
```

Examples include Common Lisp `sort(sequence)`, Java `List.sort`, and Swift's mutable random-access collections under stronger operational conditions.

This maps to `FiniteLinearView` or a possible future `FiniteSequence` abstraction.

#### Meaning D — native operational capability generic

```text
index range
+ compare positions
+ swap positions
 -> same native algorithm directly over hidden X
```

Examples include Ada `Generic_Sort`, Go `sort.Interface`, Zig context sorting, and—at a richer iterator level—C++/D/Swift random-access abstractions.

This maps to an optional `IndexedSortKernel` / `SwappableSequence` layer.

#### Meaning E — permutation/index generic

There is also a useful orthogonal output form:

```text
X -> Permutation (Fin n)
```

rather than `X -> X`. D `makeIndex` and Julia `sortperm` show that this is a practical API, not merely a proof artifact.

For A Program this may be especially elegant because sorting correctness already contains a permutation theorem. A generic sorter could expose the permutation as a first-class computational result and derive representation-specific reorderings from it.

The design error to avoid is collapsing A through E into one enormous `SortableContainer` interface.

### 6.30 Comparative matrix (original proof-oriented core)

| System/API | Element type generic? | Comparator/order generic? | Representation generalized how? | Same native algorithm over arbitrary containers? | Correctness/proof shape | Stability/complexity notes |
|---|---:|---:|---|---:|---|---|
| Lean `List.mergeSort` | Yes | Yes | Fixed to `List` | No | library lemmas/specification around list result | stable; reasoning-friendly definition with proved-equivalent runtime implementation |
| Lean `Array.qsort` | Yes | Yes | Fixed to `Array` / subrange | No | array-specific lemmas/internal vector reasoning | in-place-style array QuickSort |
| Lean `Array.mergeSort` | Yes | Yes | Fixed to `Array` | No | permutation/sortedness/stability lemmas | stable, `O(n log n)` worst-case per release notes |
| Lean `Finset.sort` / `Multiset.sort` | Yes | Yes | unordered structure converted to `List` | No | output sorted list + permutation/membership facts | demonstrates sort-to-sequence semantics |
| Agda `SortingAlgorithm` | Yes | via `TotalOrder` module | Explicitly `List A -> List A` | No | record bundles permutation + sortedness | standard `Data.List.Sort` selects MergeSort |
| Agda `Container` | Yes | N/A | shape + positions | N/A | generic container semantics | does not itself choose finite linear position order |
| Rocq `Stdlib.Sorting.Mergesort` | Yes | module parameter | Fixed to `list` | No | permutation + local/strong sortedness layers | `O(n log n)` in list length; stronger theorem with transitivity |
| Idris 2 `sortBy` | Yes | Yes | Fixed to `List` | No | programming-library function; termination uses `assert_smaller` | stable List merge sort by source comments |
| Idris 2 `Foldable` | Yes | N/A | generic enumeration via `toList` | No | interface operation | natural precedent for sort-to-list |
| Idris 2 `Traversable` | Yes | N/A | generic ordered traversal + shape rebuild | No | `traverse` | still not native QuickSort/MergeSort capability |
| C `qsort` | Byte-level type-erased | Yes | **array of fixed-width records** | No | no dependent proof package | algorithm, stability, complexity not mandated by standard |
| C++ `std::sort` | Yes | Yes | **random-access iterator range** | No | requirements/concepts rather than proofs | `O(N log N)` comparisons; not stable |
| C++ `std::list::sort` | Yes | Yes | linked list native | No | container member semantics | stable; exists because `std::sort` requires random access |
| A Program current QuickSort | Yes | Yes, with laws | `List`, internally `SizedList` | No | generic sortedness + permutation/content proof | current stability/complexity not claimed |
| A Program proposed semantic layer | Yes | Yes | `FiniteContents` / `FiniteLinearView` | transport, not native algorithm | proof-carrying | stability/cost optional refinements |
| A Program possible native layer | Yes | Yes | `SwappableSequence` / capability-specific | Yes, for representations satisfying capability | laws for get/swap/permutation + backend proof | cost model must be stated separately |

### 6.31 Earlier three-way synthesis (historical note; superseded by §6.29)

This three-way split was useful in the first revision, but the broader survey shows that it conflates two important cases: canonical-result collection versus same-shape linear sorting, and it omits permutation/index output. It is retained only to document the evolution of the audit. **For current design decisions, use the five-way taxonomy in §6.29.**

#### Historical Meaning A — element/order generic

```text
sort : (A, comparator/laws) -> List A -> List A
```

Current A Program already does this substantially.

#### Historical Meaning B — semantic container/view generic

```text
X
 -> contents/refill witness
 -> sequence sorter
 -> sorted view or shape-preserving result
```

This is the recommended next correctness abstraction.

#### Historical Meaning C — operational representation generic

```text
X
 -> finite indexed/swappable/splittable capability
 -> same native recursive algorithm directly over X
```

C `qsort` and C++ `std::sort` are precedents for this style, but only under **strong representation capabilities**. This should be an optional later layer, not the definition of “container” and not a prerequisite for the semantic generic theory.

The design error to avoid is collapsing A, B, and C into one enormous interface.

### 6.32 Correction after a proof-assistant-specific second survey: the abstraction already exists, but is distributed across layers

The first revisions of this audit made a statement that was directionally useful but too coarse: Lean, Agda, Rocq, and Idris 2 expose their ordinary sorting algorithms mainly on `List`/`Array`, therefore proof assistants appeared not to pursue the broader abstraction being considered here.

That statement mixed up two very different questions:

1. **What carrier does the public sorting function consume?**
2. **Where does the theorem prover place representation independence?**

Once the second question is investigated directly, the picture changes substantially. The theorem-proving ecosystem contains several close precedents for the proposed A Program design. They are simply not normally packaged as one STL-style “generic sorting algorithm over every container”.

This correction is important enough that the later design recommendations should be read in light of it.

#### 6.32.1 Why the earlier survey underestimated proof-oriented genericity

A conventional programming-language search naturally looks for signatures such as:

```text
sort : Container A -> Container A
```

or:

```text
sort : Range R => R -> R
```

That is often the right place to find genericity in C++, Rust, Swift, D, Ada, Go, or a collection library.

A theorem prover has several additional places in which the same genericity can live:

```text
(A) the mathematical result:
    permutation of finite positions

(B) the semantic model:
    concrete carrier X <-> ghost List/Seq contents

(C) the correctness interface:
    output is sorted + permutation of input

(D) the abstract operation algebra:
    length/index/swap/get/set

(E) the refinement theorem:
    implementation R refines abstract carrier/model S

(F) the executable algorithm itself:
    List sort, Array sort, heap-manipulating QuickSort, ...
```

In ordinary library documentation, only (F) may be called “sorting”. In proof engineering, however, (A) through (E) can contain most of the reusable abstraction.

The earlier audit inspected (F) much more heavily than (A)-(E). That made a real body of prior art look absent.

The corrected research question is therefore not:

> “Does Lean/Agda/Rocq expose `sort` over arbitrary containers?”

but:

> “Does the ecosystem already separate finite positions, permutation, semantic contents, abstract sequence operations, and concrete representation refinement in a way from which a generic verified sort can be built?”

The answer to the latter is **yes, in several independent forms**.

---

#### 6.32.2 Lean Mathlib: `Tuple.sort` is almost exactly the permutation-first abstraction

The strongest correction comes from Lean Mathlib.

`Mathlib.Data.Fin.Tuple.Sort` treats an `n`-tuple as the dependent-function representation:

```text
f : Fin n -> α
```

and defines:

```text
Tuple.sort (f : Fin n -> α) : Equiv.Perm (Fin n)
```

The result of sorting is therefore **not primarily another list or array**. It is a permutation of the finite position type. The sorted tuple is obtained by composition:

```text
f ∘ Tuple.sort f
```

and Mathlib proves:

```text
Tuple.monotone_sort : Monotone (f ∘ Tuple.sort f)
```

It goes further. `Tuple.eq_sort_iff` characterizes the sorting permutation using both monotonicity and original indices as a tie-breaker; operationally, the construction sorts `(f i, i)` lexicographically. Thus the API contains a stability/canonical-tie-breaking idea at the **position permutation** level.

Source:

- https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Fin/Tuple/Sort.html

This is highly relevant to A Program. The previously proposed form:

```text
sortPermutation :
  (Fin n -> A) -> Permutation (Fin n)
```

is not merely inspired by Julia/D/MATLAB-style index sorting. There is a direct theorem-prover precedent, with the permutation itself as the mathematical result.

Lean then uses different interfaces elsewhere:

- `List.mergeSort` sorts a list;
- `Array.qsort` / `Array.mergeSort` sort array-like representations;
- `Finset.sort` and `Multiset.sort` return a sorted `List` because their input structures do not themselves carry a meaningful sorted positional arrangement;
- `Tuple.sort` returns the **ordering permutation** of finite positions.

This is not inconsistency. It is evidence that Mathlib deliberately uses different semantic outputs for different carrier structures.

A Program should take this precedent seriously. A finite-position permutation can be a more fundamental semantic object than a same-carrier sorting function.

---

#### 6.32.3 Agda: position permutations and permutation actions already exist independently of List sorting

Agda's standard sorting record remains deliberately list-shaped:

```text
SortingAlgorithm.sort   : List A -> List A
SortingAlgorithm.sort-↭ : sort xs ↭ xs
SortingAlgorithm.sort-↗ : Sorted (sort xs)
```

It would therefore be wrong to claim that Agda's *sorting API* is generic over categorical containers.

But that is only one part of the library.

Agda also defines finite permutations directly:

```text
Permutation m n = Fin m ↔ Fin n
Permutation′ n  = Permutation n n
```

in `Data.Fin.Permutation`.

More revealingly, the functional-vector permutation relation is defined by existentially exhibiting a finite-position permutation:

```text
xs ↭ ys =
  Σ[ ρ ∈ Permutation _ _ ]
    (∀ i -> xs (ρ i) ≡ ys i)
```

Thus for a functional vector, “these two values are permutations” is literally “there is a bijection of their finite positions under which their values agree”.

Sources:

- https://agda.github.io/agda-stdlib/v2.3/Data.Fin.Permutation.html
- https://agda.github.io/agda-stdlib/v2.4/Data.Vec.Functional.Relation.Binary.Permutation.html
- https://agda.github.io/agda-stdlib/master/Data.List.Sort.Base.html

This matters because Agda also has an explicit container theory of shape and positions. The conceptual pieces are therefore all present:

```text
shape
positions
finite-position bijection
position action on indexed/function-like data
List SortingAlgorithm with Sorted + Permutation
```

What is absent is primarily a **library-level unification** saying that every suitable finite-linear container automatically inherits the same sorting construction.

Again, the conclusion is not “Agda cannot express it”. The conclusion is “Agda keeps the abstractions separate”.

That separation may itself be a design lesson for A Program.

---

#### 6.32.4 Isabelle/HOL and Sepref: genericity is often a refinement theorem, not a sort signature

Isabelle provides perhaps the clearest explanation for why searching only public sort signatures is misleading.

The AFP/Sepref Imperative Refinement Framework contains an abstract list interface with operations including indexed `swap`. The abstract swap has the expected semantic theorems:

```text
swap_nth
swap_set
swap_multiset
swap_length
map_swap
```

and is registered as an abstract list operation.

Then `IICF_Array` is explicitly titled:

> **Plain Arrays Implementing List Interface**

and states that fixed-length lists are directly implemented with arrays. In other words, the proof development can reason at a list/sequence interface and later refine that interface to concrete mutable arrays.

Sources:

- https://isa-afp.org/browser_info/current/AFP/Sepref_IICF/Refine_Imperative_HOL.IICF_List.html
- https://isa-afp.org/browser_info/current/AFP/Sepref_IICF/Refine_Imperative_HOL.IICF_Array.html
- https://www.isa-afp.org/browser_info/current/AFP/Refine_Imperative_HOL/document.pdf

Isabelle separately contains a verified imperative QuickSort on arrays. That development proves multiset preservation and sortedness and ultimately relates the resulting concrete array contents to the functional list `sort` reference result.

Source:

- https://isabelle.in.tum.de/website-Isabelle2022/dist/library/HOL/HOL-Imperative_HOL/Imperative_Quicksort.html

This style suggests a different architecture from C++ templates:

```text
abstract semantic sequence algorithm/specification
                |
                | refinement
                v
      mutable array representation
```

rather than:

```text
one executable generic sort function
     instantiated at many containers
```

The two architectures can have similar reuse, but the reuse is located in different places.

For a theorem prover, the refinement formulation is often more attractive because:

- the abstract specification can use simple mathematical sequences/lists;
- sortedness and permutation are easy to state on that model;
- concrete memory ownership and update behavior remain representation-specific;
- extraction/refinement can preserve efficient array operations;
- one does not have to pollute the semantic interface with every operational detail of every concrete representation.

This is a major reason that theorem-prover libraries may look “less generic” than C++ at the API level while actually having strong representation independence in their proof architecture.

---

#### 6.32.5 F*/Pulse: a common proof-carrying sorting class across several algorithms

The current F* Proof-Oriented Programming material makes the proof/specification layer explicit.

It introduces a class approximately of the form:

```text
class array_sort = {
  sort:
    fn (#a : Type)
       (arr : array a)
       (len : size)
       (ord : total_order a)
       (#s : erased (Seq a))
    requires arr |-> s
    ensures exists s'.
      arr |-> s' **
      sorted ord s' /\ permutation s s'
}
```

and motivates it precisely as a common rubric shared by insertion sort, merge sort, heapsort, and quicksort.

A later version additionally abstracts a complexity bound and an instrumented comparator, using ghost monotonic state to make comparison-count bounds part of the same interface.

Source:

- https://fstar-lang.org/tutorial/book/agentic/agentic_sorting_algorithms.html

This is representation-specific (`array`) but **algorithm-generic, element-generic, order-generic, and proof-generic**. More importantly for A Program, the physical array is related to an erased mathematical `Seq a` used for specification.

That is another instance of the recurring proof-oriented pattern:

```text
physical carrier
    |
    | ghost/semantic relation
    v
mathematical sequence
    |
    +-- Sorted
    +-- Permutation
    +-- optional cost bound
```

A Program's proposed `FiniteContents`/`FiniteLinearView` should therefore be understood not merely as a programming-language collection interface, but as a possible **semantic model boundary** analogous to these ghost sequence views.

---

#### 6.32.6 Rocq: modularity is strong over the order/specification axis, but the carrier stays `list`

Rocq's `Stdlib.Sorting.Mergesort` is another useful contrast.

It is a functor:

```text
Module Sort (Import X : Orders.TotalLeBool').
```

and assumes only a total Boolean relation at the base level. The library explicitly notes that transitivity is not required to prove local sortedness; with transitivity it upgrades the result to `StronglySorted`.

It proves, among other results:

```text
LocallySorted_sort
Permuted_sort
StronglySorted_sort   -- with Transitive leb
```

Source:

- https://rocq-prover.org/doc/V9.1.0/stdlib/Stdlib.Sorting.Mergesort.html

So Rocq chooses to make the **order theory and theorem strength** modular while keeping the executable carrier as `list`.

This is not evidence against a more generic carrier abstraction. It shows one of the strongest recurring tendencies in proof libraries: first maximize reuse in the *logical specification*, while keeping the data representation concrete enough that termination and executable recursion stay simple.

That is also why the Rocq example remains particularly relevant to the A Program distinction between local and strong sortedness.

---

#### 6.32.7 Why3 and Dafny: abstract contents and ghost mathematical collections are normal proof boundaries

Why3's module-interface mechanism explicitly supports abstract data types with semantic contents. Its documentation demonstrates an abstract set interface:

```text
type t = abstract { contents : fset int }
```

implemented by an ordered list, with cloning/interface checks connecting implementation to abstraction.

Its array library separately defines representation-independent logical predicates over generic array element types, including `sorted`, `exchange`, and `permut`.

Sources:

- https://why3.org/doc/syntaxref.html
- https://why3.org/stdlib/array.html
- https://why3.org/stdlib/seq.html

Dafny similarly uses mathematical sequences and multisets as specification values around mutable arrays. A conventional sorting postcondition can state that an array snapshot is sorted and that its multiset equals the pre-state multiset.

The lesson is not that Why3/Dafny already expose the exact A Program interface proposed here. The lesson is that **concrete representation -> ghost mathematical contents -> generic logical property** is a standard verification architecture.

A Program can profitably separate those layers instead of forcing representation genericity into the executable function type alone.

---

#### 6.32.8 The key distinction: theorem provers often generalize the *proof object* rather than the *carrier API*

The broad programming-language survey showed several operational abstractions:

```text
C        : base + count + width + comparator
Ada/Go   : length + before/less + swap
C++/D    : random-access range + comparator
```

The proof-assistant survey reveals another family:

```text
Lean     : finite-position permutation
Agda     : finite-position bijection + action/relation
Isabelle : abstract list interface + representation refinement
F*/Pulse : ghost Seq + common proof-carrying array-sort class
Rocq     : abstract order + graded sortedness theorems
Why3     : abstract semantic contents + module refinement
Dafny    : array snapshots + multiset/sequence specifications
```

These are not weaker versions of ordinary generic programming. They are **different factorisations of the same problem**.

A conventional generic algorithm asks:

> What operations must every runtime carrier implement so that this code can run?

A proof-oriented development can instead ask:

> What mathematical object captures the behavior of all these carriers, and how do I prove that each concrete implementation refines that object?

The second formulation is often preferable once mutation, aliasing, dependent indices, or extraction efficiency matter.

---

#### 6.32.9 Why there still is no ubiquitous `sort : forall C. Container C => C A -> C A`

The correction above does **not** imply that a universal categorical-container sort has been hiding in every prover. Its absence still has principled reasons.

First, a categorical container gives shapes and positions, but not necessarily a canonical **linear order** of those positions. Sorting needs such an order if the result is to be described as monotone.

Second, some structures are unordered. For a finite set, the meaningful operation is usually “produce an ordered enumeration”, not “rearrange the set”. Lean's `Finset.sort` reflects exactly this distinction.

Third, concrete operational capabilities differ radically. Array QuickSort, linked-list MergeSort, tree rebuilding, and purely functional vector sorting do not have one honest common cost model. An overly general executable interface can preserve extensional correctness while destroying the algorithmic property that motivated the implementation.

Fourth, proof systems have unusually strong incentives to choose a simple **reference model**. Lists, sequences, finite functions, and multisets have mature induction and extensionality theories. Proving a concrete structure refines that model can be substantially easier than re-proving every theorem over a maximal operational interface.

Fifth, dependent representations make a universal mutation interface expensive. A `swap` may need to preserve indices, shape invariants, ownership, or dependent labels. Keeping the semantic permutation separate from the physical realization makes these obligations local to the representation.

Thus the historical pattern is understandable:

```text
semantic reuse     -> very broad
representation reuse -> via refinement or separate instances
native algorithm reuse -> only under strong capability agreement
```

That is a more accurate statement than “theorem provers do not generalize sorting”.

---

#### 6.32.10 Revised proposal: make finite-position permutation the semantic center

The Lean/Agda evidence suggests strengthening the earlier architecture.

Instead of treating `sortPermutation` merely as an optional convenience, A Program should investigate making it the **semantic center** of finite linear sorting.

Conceptually:

```text
FiniteLinearPresentation A X
  size      : X -> Nat
  observe   : (x : X) -> Fin (size x) -> A

SortPermutation A R n
  sortPerm  : (Fin n -> A) -> Perm (Fin n)
  sorted    : Monotone_R (f ∘ sortPerm f)
  stable    : ...                 -- optional/explicit
```

Then representation-specific action is separated:

```text
PermutationAction X
  act       : X -> Perm (Fin n) -> X
  identity  : ...
  compose   : ...
  observe_act : ...
```

or, for immutable shape-preserving values:

```text
refill : X -> (Fin n -> A) -> X
```

An operational in-place kernel remains a separate refinement:

```text
IndexedSortKernel X n
  before : X -> Fin n -> Fin n -> Bool
  swap   : X -> Fin n -> Fin n -> X
```

with a theorem that its physical swaps realize the semantic permutation produced or permitted by the sorting specification.

This yields a proof-oriented stack:

```text
                finite values / relation R
                         |
                         v
              sort permutation on Fin n
                         |
             +-----------+-----------+
             |                       |
             v                       v
      semantic refill/action    proof/reference model
             |                       |
             v                       v
       List / Vec / tree       Sorted + Permutation
             |
             | refinement
             v
      mutable/native kernels
      (array / packed storage / ...)
```

This architecture combines ideas that existing provers already validate separately:

- Lean validates sorting-as-finite-permutation;
- Agda validates finite-position permutation relations/actions;
- Isabelle validates representation refinement from an abstract list-like interface to arrays;
- F*/Pulse validates a shared proof-carrying sorting specification over a ghost sequence model;
- Rocq validates separating order assumptions from theorem strength.

A Program's opportunity is therefore not to invent generic verified sorting from nothing. It is to **synthesize several proof-oriented abstraction patterns that existing systems currently expose in separate modules and layers**.

---

#### 6.32.11 Revised comparative matrix: where is the genericity located?

| Ecosystem | Surface sort carrier | More abstract proof object/model | Representation abstraction | Key lesson for A Program |
|---|---|---|---|---|
| Lean Mathlib | `List`, `Array`, etc. | `Tuple.sort : (Fin n -> α) -> Perm (Fin n)` | separate APIs by carrier | permutation-first sorting already has direct precedent |
| Agda stdlib | `List A` in `SortingAlgorithm` | `Fin` bijections; vector permutation as positional permutation | categorical containers elsewhere | position permutation and sorting can remain orthogonal modules |
| Rocq stdlib | `list` | `Permutation`, `LocallySorted`, `StronglySorted` | module functor over order | maximize logical/order genericity before carrier genericity |
| Isabelle/HOL + Sepref | functional lists and concrete arrays both occur | list/multiset reference semantics | explicit refinement; arrays implement List interface | representation independence can live in refinement, not function polymorphism |
| F*/Pulse | mutable array | erased mathematical `Seq`; `sorted + permutation` class | separation-logic ownership/specification | one proof-carrying algorithm class can cover many implementations while fixing physical carrier |
| Why3 | arrays/sequences in algorithms | abstract `contents`, generic `sorted/permut` predicates | module interfaces and cloning | semantic contents are an explicit abstraction boundary |
| Dafny | usually arrays for imperative sort | immutable sequence snapshot + multiset | contracts/ghost state | verification can be generic at the semantic-value layer |
| A Program opportunity | currently `List`/`SizedList` | proposed `Fin n` permutation + finite-linear observation | later `refill` / action / refinement | combine permutation-first semantics with explicit representation laws |

The key column is not “surface sort carrier”. It is “where is the genericity located?”.

---

#### 6.32.12 Corrected historical conclusion

The earlier statement should therefore be replaced by the following:

> Existing theorem provers usually do **not** expose one universal executable sorting function over all container representations. However, they **do** contain substantial generic sorting machinery at the level of finite-position permutations, generic sortedness/permutation specifications, ghost sequence models, abstract sequence interfaces, and representation refinement. The apparent absence came from looking primarily at surface List/Array sorting APIs instead of the proof architecture around them.

For A Program this strengthens, rather than weakens, the case for a layered design.

It also changes the priority of experiments. A minimal next prototype should no longer be only “generalize `List` through `FiniteContents`”. It should compare two semantic centers:

```text
Model 1:
  X -> finite linear view -> List backend -> refill

Model 2:
  X -> Fin n observation -> sorting permutation -> permutation action/refill
```

The second model now has enough direct theorem-prover precedent that it deserves equal or greater priority.


## 7. Positivity, coinduction, and Stream: deferred in this revision

> **Revision note (2026-09-27):** coinduction/codata/Stream is intentionally not expanded further here. The paragraphs below preserve the previous audit's scoping argument only. A separate design audit should revisit productive codata, guarded/corecursive definitions, and their relationship to A Program's positivity discipline. None of the C/C++-inspired sorting recommendations in this revision depend on settling that later question.

The concern that A Program has mainly strong-positive IADTs while a stream is in some sense “negative” points at a real type-theoretic distinction, but the terms should be kept separate.

### 7.1 Strict positivity

Strict positivity is primarily a restriction on where a recursively defined type may occur in constructor arguments. It is used to preserve well-founded inductive semantics and consistency properties.

Container theory is in fact closely connected to strictly positive types. The classic work of Abbott, Altenkirch, and Ghani develops containers precisely as a semantic account of broad classes of strictly positive type constructors.

So “A Program is strongly positive” does **not** imply “A Program cannot support a useful finite container abstraction”. The opposite is closer to the conceptual relationship: strict positivity is fertile ground for container-like semantics.

### 7.2 Coinduction/productivity

An infinite stream requires a coinductive/final-coalgebra interpretation or an equivalent productive operational discipline. That is not provided merely by allowing a recursively mentioned field.

For example, a conventional coinductive stream constructor can still mention the recursive stream type positively. What changes is how infinite objects and recursive definitions are accepted and observed: productivity/guardedness replaces ordinary finite termination as the central criterion.

Thus:

> “coinductive” is not simply synonymous with “negative occurrence”.

A Program may eventually need a separate story for codata, observations, guarded corecursion, or productive infinite objects. That is a substantial language-design topic, but it is orthogonal to this finite sorting audit.

### 7.3 Why arbitrary infinite stream sort is not the desired abstraction

For an arbitrary infinite input stream, emitting the first element of a globally sorted permutation may require knowing that no smaller element occurs arbitrarily far in the future.

In general that information is unavailable after any finite amount of observation.

Worse, some streams have no least element. An integer stream such as

```text
0, -1, -2, -3, ...
```

cannot be permuted into a globally nondecreasing stream with a first element, because its set of values has no minimum.

By contrast, **merging two already-sorted infinite streams** can often be productive: compare their current heads and emit one. This is a coinductive merge operation, not a general infinite sorting operation.

Therefore adding `Stream` would not solve “generic sorting”; it would introduce a separate domain in which the usual total finite sort specification itself needs revision.

---

## 8. The correct abstraction boundary for A Program

This audit recommends a two-stage finite abstraction, followed by a proof-carrying sorting backend.

The names below are schematic. They are intentionally not proposed as kernel primitives or final surface syntax.

### 8.1 `FiniteContents A X`: broad finite enumeration

At the weakest useful level, a value of some carrier `X` can expose a finite sequence of `A` values:

```text
contents : X -> List A
```

The important point is semantic: `contents` defines **which values are considered the sortable contents** and, where relevant, an enumeration order.

This is enough to define:

```text
sortToList : SortingAlgorithm A R -> FiniteContents A X -> X -> List A
```

schematically as:

```text
sortToList alg view x = alg.sort (view.contents x)
```

This operation is meaningful for many things that should not support same-carrier sorting:

- finite sets;
- multisets/bags;
- trees with an explicitly chosen traversal;
- syntax trees when a particular class of leaves/fields is intentionally collected;
- fixed indexed containers.

It also mirrors Lean's `Finset.sort` / `Multiset.sort` pattern.

### 8.2 `FiniteLinearView A X`: finite enumeration plus lawful refill

For shape-preserving sorting, the interface must additionally support reconstruction from a list of the same number of values.

Schematic requirements are:

```text
contents : X -> List A
refill   : (x : X) -> (ys : List A) -> SameLength ys (contents x) -> X
```

with laws equivalent to:

1. `contents (refill x ys p)` is `ys`;
2. `refill` preserves the intended shape/index of `x`;
3. refilling with the original contents is observationally the original structure;
4. each declared position is visited/refilled exactly once.

The exact equality language should follow what A Program can prove conveniently today. These laws need not initially be definitional equalities. Propositional content and shape relations are acceptable if they avoid forcing new equality machinery.

The same interface can be presented more mathematically by requiring, for each shape, a finite linearly ordered position set or an equivalence:

```text
Position(shape x)  ~=  Fin(size x)
```

That expresses the essential fact: a chosen rank turns the positions into a sequence.

### 8.3 Why the carrier-level form is preferable initially

A tempting declaration would quantify over a higher-kinded constructor `C : @ -> @`. That may eventually be elegant, but it is not required to validate the design.

A lower-risk first prototype quantifies over concrete types:

```text
A : @
X : @
```

and passes an explicit `FiniteLinearView A X` witness.

This already handles:

- `X = List A`;
- `X = SizedList A n` for a fixed `n`;
- `X = Vec A n`;
- `X = Tree A` under an explicitly chosen traversal.

It also avoids making higher-kinded abstraction quality a prerequisite for deciding whether the generic sorting idea itself is sound.

### 8.4 Optional third layer: `FiniteIndexed` / `SwappableSequence` for native algorithms

The C/C++ comparison adds a useful layer that is deliberately **narrower** than `FiniteLinearView`. If A Program wants a single native QuickSort implementation to execute against several array/vector-like representations, define an operational capability instead of broadening `FiniteLinearView` until it becomes an array API.

Candidate names include:

```text
FiniteIndexed
RandomAccessSequence
SwappableSequence
FinitePermutationView
```

A good separation is:

```text
FiniteLinearView        -- semantic transport / correctness
FinitePermutationView   -- semantic position permutation action
SwappableSequence       -- executable indexed exchange capability
```

`SwappableSequence` should refine or be connected by proof to the semantic permutation view. That allows QuickSort's many local swaps to be verified once as permutations of finite positions.

Do not make this the mandatory parent interface for `List`. A List instance with linear-time indexing would satisfy the extensional equations while violating the performance assumptions that motivate indexed QuickSort. Cost capabilities must therefore be documented or separately witnessed rather than inferred from extensional type signatures alone.

---

## 9. A proof-carrying sorting backend

The second recommended abstraction is not “QuickSort over containers”. It is a small record/IADT/dictionary describing a verified sequence sorting implementation.

Agda's `SortingAlgorithm` is a useful precedent.

Schematic shape:

```text
SortingAlgorithm A R := {
  sort        : List A -> List A,
  sorted      : (xs : List A) -> Sorted R (sort xs),
  permutation : (xs : List A) -> Permutation xs (sort xs)
}
```

Optional refinements can later add:

```text
stable      : ...
length      : ...
complexity  : ...
```

but the current QuickSort theorem should not be silently strengthened with properties it does not presently prove.

### 9.1 Current QuickSort can become one backend witness

The current A Program development already has the hard pieces needed for such a witness:

- generic sortedness at ordinary result;
- generic permutation/content correctness at ordinary result;
- length-related properties in the current verification suite;
- well-founded termination via measured/sized internal data.

Packaging those proofs does not require rewriting QuickSort itself.

### 9.2 Future MergeSort becomes another backend witness

A future conventional verified MergeSort would implement the same `SortingAlgorithm A R` interface.

The generic container transport layer would then be oblivious to whether the backend is:

- QuickSort;
- MergeSort;
- insertion sort;
- another verified list sorter.

This is a cleaner factoring than parameterizing every algorithm directly over a huge “generic container” operation set.

---

## 10. Generic derived operations

With the two abstractions above, A Program can have genuinely generic sorting operations while preserving a simple algorithmic core.

### 10.1 Sort any finite enumerable structure to a list

Schematic definition:

```text
sortToList alg view x = alg.sort (view.contents x)
```

Generic theorem:

```text
Sorted R (sortToList alg view x)
```

and a content theorem relating the result to `view.contents x` by permutation.

This is the widest useful operation.

### 10.2 Sort within a chosen linear shape

Schematic definition:

```text
sortWithin alg view x =
  let ys = alg.sort (view.contents x)
  in view.refill x ys (length/permutation-derived proof)
```

Then derive:

- `contents_sorted`;
- `contents_permutation`;
- `shape_preserved`;
- optionally `stable` if the backend and view expose the required ordered-position semantics.

The current QuickSort length/permutation theorem should make the refill-length side condition provable without changing the algorithm.

### 10.3 List is an instance, but need not cease being the backend

For lists:

```text
contents xs = xs
refill xs ys _ = ys
```

up to whatever witness packaging is chosen.

Then the generic `sortWithin` specialized to the List view should propositionally, and ideally computationally, agree with the current list sort.

This satisfies the desirable architectural statement that “List sorting is an instance of the generic theory” without requiring the QuickSort recursion itself to execute against every possible container representation.

---

## 11. Relation to `SizedList` and `Measured`

The current QuickSort proof already has a natural internal distinction:

```text
List A
  -> measure
SizedList A n
  -> partition / Acc recursion
List A result
```

This is a useful design asset.

However, this audit does **not** recommend immediately exposing `SizedList` as the public generic container interface. Doing so would mix two concerns:

- public observable finite contents;
- internal evidence used to prove structural decrease.

A cleaner first layer is `contents : X -> List A`. The existing QuickSort implementation can continue to create its own `SizedList` internally.

A later optimized or proof-oriented interface may expose a measured view directly, for example:

```text
measureContents : X -> Measured A
```

if that demonstrably reduces proof duplication. It should be introduced only after the List-level view design has been validated.

---

## 12. What a truly representation-generic QuickSort would require

There are actually **two different native QuickSort generalizations** worth distinguishing.

### 12.1 Partition-oriented functional QuickSort

The style closest to current A Program's List development needs an abstraction supporting:

- finite size/measure;
- pivot extraction or selection;
- partition under the comparator;
- content conservation;
- lower/pivot/upper ordering facts;
- strictly smaller recursive measures;
- concatenation or equivalent linear reassembly.

An interface exposing all of these is already recognizably a **finite sequence/decomposition algebra**, not a categorical container.

This may be a good fit for List-like persistent representations.

### 12.2 Index/compare/swap QuickSort: the Ada/Go/Zig lineage

The broad survey reveals a stronger precedent than C alone. Ada 2022 `Containers.Generic_Sort`, Go's classic `sort.Interface`, and Zig's context sorting kernels independently converge on nearly the same minimal operational abstraction:

- a finite index interval or `Len`;
- `Before(i,j)` / `Less(i,j)` comparing **positions**;
- `Swap(i,j)` exchanging **positions**.

Notably, an explicit generic `get : X -> Fin n -> A` is *not required by the sorting kernel itself*. The comparator can close over or otherwise observe the hidden representation. Zig demonstrates why this matters: one `swap(i,j)` can simultaneously exchange corresponding fields in a structure-of-arrays representation.

A Program should therefore distinguish two possible interfaces. A low-level kernel may be almost exactly:

```text
IndexedSortKernel n X =
  before : X -> Fin n -> Fin n -> Bool
  swap   : X -> Fin n -> Fin n -> X
  ... laws ...
```

whereas a proof-facing view may additionally expose:

```text
SwappableSequence A n X =
  at     : X -> Fin n -> A
  swap   : X -> Fin n -> Fin n -> X
  ... lookup/permutation laws ...
```

The first maximizes representation hiding; the second makes correctness proofs easier because element observations are explicit. They need not be the same source-level object if that causes proof or performance awkwardness.

A QuickSort implementation can then be written once against the operational kernel, while its correctness theorem is transported through the proof-facing laws. A compiler is free to lower unique array-like values to mutation; the source-level theory need not expose unsafe pointers.

### 12.3 A semantic refinement can make the swap laws manageable

Rather than prove low-level lookup equations independently everywhere, it may be cleaner to define a semantic action of finite permutations:

```text
FinitePermutationView A n X =
  at      : X -> Fin n -> A
  permute : X -> Permutation (Fin n) -> X
  law     : at (permute x p) i = at x (p⁻¹ i)
```

and show that `swap i j` is the action of the transposition `(i j)`.

Then the content-permutation proof of QuickSort can be factored through permutation composition. Whether A Program's current equality/permutation library makes this ergonomic should be tested experimentally before committing to the exact record shape.

### 12.4 Cost is part of the operational abstraction even if it is not yet a theorem

The signature `get : X -> Fin n -> A` alone says nothing about cost.

- array/vector: typically constant-time indexing;
- linked List: typically linear-time indexing;
- tree with rank metadata: perhaps logarithmic;
- plain tree under traversal indexing: potentially linear or worse.

Thus a generic indexed QuickSort over all extensional instances can be *correct* yet asymptotically inappropriate.

A Program should therefore avoid claiming “C-like generic QuickSort” until at least two representations have suitable operational costs. Initially this can be a documented precondition; later it could become a cost/complexity witness if the language develops such a theory.

### 12.5 Recommendation

Keep the current List/SizedList QuickSort unchanged as the first verified backend.

Prototype `SwappableSequence` only after a second array/vector-like representation exists for which sharing the same indexed QuickSort implementation would remove real duplication. Do not introduce it merely so that List can be called an instance.

This is the key reconciliation with C: **C's generality is real because all inputs already satisfy one strong storage model.** A Program should generalize to an equally explicit capability boundary, not to arbitrary IADTs.

## 13. What a truly representation-generic MergeSort would require

MergeSort highlights why QuickSort's C-like capability must not become a universal “sortable sequence” interface.

### 13.1 List-native MergeSort wants decomposition and sequential merge

A persistent/list implementation naturally needs:

- finite size or termination evidence;
- split into two smaller list-like values;
- proofs of decrease;
- head/front comparison;
- efficient linear merge/reconstruction.

Random indexing and swapping are unnecessary.

Rocq and Idris 2 both demonstrate this List-native shape. Idris's `sortBy` recursively splits and `mergeBy`s Lists; Rocq's stdlib maintains a stack of pending List merges.

### 13.2 Array-native MergeSort wants a different capability set

An efficient array merge sort may instead want:

- indexed ranges/slices;
- temporary buffer storage;
- bulk copy/move operations;
- range merge;
- explicit allocation/cost discipline.

Lean therefore has a distinct `Array.mergeSort` implementation rather than mechanically reusing `List.mergeSort` through a universal container abstraction.

### 13.3 A common semantic interface, not necessarily a common operational interface

QuickSort and MergeSort should normally share:

```text
sortedness
permutation/content preservation
optional stability
optional complexity specification
```

They need not share:

```text
swap
random access
split representation
merge buffer
partition primitive
```

Therefore the recommended architecture is:

```text
                 SortingAlgorithm A R
                 /        |        \
                /         |         \
      List QuickSort   List MergeSort   Array-like sort
          |                 |                |
   List/SizedList ops   split/merge ops   index/swap/buffer ops
```

The upper layer is reusable proof semantics; the lower layer is capability-specific execution.

### 13.4 Consequence for the current A Program merge work

The current repeated-insertion `merge` fixture should not be stretched into a “generic MergeSort interface”. First implement/verify a conventional List MergeSort with a true linear two-front merge. Once that backend inhabits `SortingAlgorithm`, compare its proof and runtime structure with the existing QuickSort backend.

Only after that comparison should a native representation-generic merge interface be considered.

## 14. Foldable, Traversable, and container-theory lessons

External theory helps sharpen the abstraction boundary.

### 14.1 Containers and strict positivity

Abbott, Altenkirch, and Ghani's container work shows the close connection between shape/position descriptions and strictly positive type constructors.

For A Program, the useful inference is:

> strong positivity is compatible with, and conceptually supportive of, finite container/view abstractions.

The inference that should **not** be made is:

> every strongly-positive IADT automatically has a canonical sortable element sequence.

It does not.

### 14.2 Traversability and finitary containers

Work by Jaskelioff and O'Connor gives a precise relationship between traversable functors and finitary containers. Gibbons and Oliveira likewise explain traversal/iterator structure as a way to visit elements while preserving shape.

This supports `FiniteLinearView` as a sensible semantic target: finite positions plus a lawful visit/reconstruction discipline.

But a traversal still chooses an order. That choice is part of the interface and must not be inferred from positivity alone.

---

## 15. Stability semantics

Generic sorting through a traversal makes stability more subtle, not less.

A stable sort preserves the relative order of elements equivalent under the comparison relation. For a tree or other non-list container, “relative order” must mean relative order in the **declared traversal**.

Therefore:

- current QuickSort should remain specified only by the properties currently proved;
- a future stable MergeSort backend may expose a stability law;
- `FiniteLinearView` must define the input linear order against which stability is interpreted;
- unordered `FiniteContents` structures should not receive a meaningful stability claim unless their enumeration order is intentionally semantically observable.

This is another reason to keep sortedness/permutation as the base interface and stability as an optional refinement.

---

## 16. Complexity semantics

The current README correctly avoids claiming generic QuickSort complexity results.

If generic transport is added, complexity should be factored explicitly:

```text
cost(sortThrough x)
  = cost(contents x)
  + cost(sequenceSort ...)
  + cost(refill ...)
```

For a conventional finite linear view, enumeration and refill should normally be linear. In that case they do not change the asymptotic order of an `O(n log n)` MergeSort, though allocation and constants still matter.

For QuickSort, the backend retains its own average/worst-case behavior.

A pathological view whose `contents` or `refill` is superlinear must not inherit an unjustified complexity theorem merely because the underlying list sorter is efficient.

Complexity should therefore remain outside the first correctness abstraction.

---

## 17. Indexed containers and shape preservation

A Program's IADT emphasis makes indexed structures especially relevant.

Suppose `Vec A n` is sorted. The output should remain `Vec A n`. In a generic view this means the refill operation must preserve the index `n`, or more generally produce evidence connecting the input and output indices.

This is a good reason for the **second prototype after List** to be an indexed finite representation such as `SizedList` or `Vec`, not merely another unindexed list wrapper.

If the abstraction handles that cleanly using existing source-level IADT/function-field machinery, it provides strong evidence that the design aligns with A Program's actual strengths.

If doing so requires changes to Core, conversion, positivity, or dependent elimination, that should be treated as a warning that the abstraction is being pushed too early.

---

## 18. Proposed implementation sequence

### Phase 0 — preserve the current QuickSort

Do not rewrite the existing well-founded QuickSort recursion.

Use the current generic result theorems as the baseline.

### Phase 1 — introduce a verified sequence-sort witness

Prototype a source-level one-constructor IADT/dictionary corresponding to:

```text
SortingAlgorithm A R
```

containing at least:

- list sort function;
- sortedness proof;
- permutation proof.

Construct a witness from current QuickSort.

### Phase 2 — define broad finite-content transport

Prototype:

```text
FiniteContents A X
```

with `contents : X -> List A`.

Define/prove `sortToList`.

### Phase 3 — define same-shape linear transport

Prototype:

```text
FiniteLinearView A X
```

with a refill operation and laws.

Define/prove `sortWithin`.

### Phase 4 — instantiate List

The List view should be trivial and should recover current semantics.

The goal is not only that both sort correctly, but that the generic wrapper does not introduce an unexpected semantic distinction from the current operation.

### Phase 5 — instantiate an indexed second representation

Preferred candidates:

- `SizedList A n`;
- `Vec A n` if the current library form is suitable.

This phase validates index/shape preservation.

### Phase 6 — optional tree traversal experiment

Define an explicit `inorder` (or another clearly named traversal) view for a finite binary tree.

Do **not** call the result simply “tree sort” without naming the traversal semantics.

This phase validates that the abstraction is genuinely broader than sequence aliases.

### Phase 7 — Ada/Go/Zig-style index/swap kernel experiment, only if a second native representation exists

If an array/vector-like representation exists, prototype the smallest operational witness justified by the concrete implementation. Begin with the historical common denominator:

```text
IndexedSortKernel n X
  before : X -> Fin n -> Fin n -> Bool
  swap   : X -> Fin n -> Fin n -> X
```

and add an explicit `at/get` field only if the proof architecture requires it. Then implement one indexed QuickSort against that kernel. The experiment should compare at least two appropriate random-access-like representations, ideally including one nontrivial layout (for example an array-of-structs versus a structure-of-arrays analogue). Do **not** use List as the only second instance merely because it can emulate indexing.

Verify that swaps compose to a permutation and that the resulting backend can be packaged as the same `SortingAlgorithm` semantics.

In parallel, prototype a computational `sortPermutation` result if current permutation infrastructure makes it cheap; D `makeIndex`, Julia `sortperm`, R `order`, and MATLAB's sort index all show that this is a practically useful abstraction rather than proof-only metadata.

### Phase 8 — consider other native algorithm interfaces separately

Only if concrete code duplication or performance evidence demands it should A Program consider additional interfaces such as:

- `PartitionableSequence` for persistent/functional QuickSort;
- `SplittableMergeSequence` for List-like MergeSort;
- range/buffer capabilities for array-like MergeSort.

These should be derived from concrete use cases, not invented solely for abstraction purity. There is no requirement that QuickSort and MergeSort share one native representation algebra.

---

## 19. Acceptance criteria

The generic sorting work should be accepted only if all of the following remain true:

- [ ] no new Core node is required;
- [ ] no evaluator primitive is required;
- [ ] no change to conversion/equality is required merely for generic sorting;
- [ ] no relaxation of positivity is required;
- [ ] no coinductive `Stream` feature is introduced merely for this task;
- [ ] current QuickSort behavior and existing acceptance fixtures remain unchanged;
- [ ] current generic QuickSort supplies the first `SortingAlgorithm` witness;
- [ ] List supplies the first `FiniteLinearView` instance;
- [ ] at least one genuinely different/indexed carrier supplies a second instance;
- [ ] sortedness is proved through the generic wrapper;
- [ ] permutation/content preservation is proved through the generic wrapper;
- [ ] shape/index preservation is proved for `sortWithin`;
- [ ] stability is not claimed unless separately witnessed;
- [ ] complexity is not claimed unless view and backend costs are separately accounted for;
- [ ] no `void *`/byte-erasure analogue is introduced merely to imitate C; typed `A` remains the source-level element abstraction;
- [ ] any C-like native QuickSort interface states finite indexed/swap capabilities explicitly;
- [ ] List is not forced through a random-access abstraction if doing so destroys the intended cost model;
- [ ] semantic `SortingAlgorithm` remains independent of whether the implementation is literally QuickSort.

---

## 20. Rejection criteria / warning signs

The design should be reconsidered if any of the following occurs:

- the proposed “container” interface contains so many list operations that it is effectively a renamed List;
- a generic interface requires every IADT to expose a canonical traversal even where no such traversal is semantically privileged;
- unordered structures are forced to return the same structure from “sort”, making the operation observationally meaningless;
- Stream/coinduction is added merely to make the abstraction appear more general;
- the QuickSort proof is rewritten before a second representation demonstrates the need;
- generic sorting requires kernel changes despite being expressible as ordinary source-level functions/proofs;
- dependent shape preservation is silently replaced by unchecked reconstruction;
- stability or complexity properties leak into the base API without proofs;
- C `qsort` is cited as evidence for “arbitrary container” sorting rather than for fixed-array/type-erased genericity;
- an operational capability is named `Container` even though it really requires random access, swapping, splitting, or another much stronger sequence property;
- one maximal interface is created solely to make both QuickSort and MergeSort share all operational primitives.

---

## 21. Recommended nomenclature

Avoid these names for the first abstraction:

```text
Container
SortableContainer
GenericContainer
```

They are too broad.

Prefer names that state the added semantics, for example:

```text
FiniteContents
FiniteLinearView
FiniteTraversal
Linearizable
RefillableTraversal
SortingAlgorithm
```

The exact final name should follow A Program's existing naming conventions after a source prototype exists.

Of these, this audit uses:

- `SortingAlgorithm A R` for the proof-carrying list/sequence backend;
- `FiniteContents A X` for sort-to-list;
- `FiniteLinearView A X` for same-shape sorting.

For an optional C-like native layer, prefer capability-revealing names such as:

- `FiniteIndexed`;
- `RandomAccessSequence`;
- `SwappableSequence`;
- `FinitePermutationView`.

Do not call that interface merely `Container`: the whole point of the C/C++ comparison is that the native algorithm relies on a much stronger positional model than arbitrary containment.

---

## 22. Concrete theorem architecture

The following is intentionally pseudocode rather than promised-valid current A Program syntax.

### 22.1 Backend specification

```text
SortingAlgorithm A R =
  sort        : List A -> List A
  sort_sorted : (xs : List A) -> Sorted R (sort xs)
  sort_perm   : (xs : List A) -> Permutation xs (sort xs)
```

### 22.2 Broad finite contents

```text
FiniteContents A X =
  contents : X -> List A
```

Derived:

```text
sortToList alg view x = alg.sort (view.contents x)
```

Theorems:

```text
sortToList_sorted
sortToList_permutation_of_contents
```

### 22.3 Same-shape linear view

```text
FiniteLinearView A X =
  contents : X -> List A
  refill   : (x : X) ->
             (ys : List A) ->
             SameLength ys (contents x) -> X

  contents_refill : ...
  shape_refill    : ...
  refill_contents : ...
```

Derived:

```text
sortWithin alg view x =
  refill x
         (alg.sort (contents x))
         (length consequence of alg.sort_perm)
```

Theorems:

```text
sortWithin_sorted
sortWithin_permutation
sortWithin_shape
```

This is the recommended initial abstraction boundary.

---

## 23. How this relates to the current `qsr_*` proof family

The existing `qsr_*` proof development should mostly remain below the new backend boundary.

Conceptually:

```text
qsr partition / SizedList / Acc machinery
              |
              v
       current quickSort
              |
              +--> qsr generic Sorted theorem
              +--> generic permutation theorem
              +--> length theorem
              |
              v
    QuickSort SortingAlgorithm witness
              |
      +-------+----------------+
      |                        |
      v                        v
 sortToList             sortWithin
 FiniteContents         FiniteLinearView
```

This preserves the substantial existing proof investment and changes only how the theorem package is exposed to higher-level generic code.

---

## 24. A note on canonical sorted results

For a total antisymmetric order, sortedness plus permutation often characterizes the sequence result strongly enough for a useful canonicalization theorem.

For a preorder or comparator admitting distinct equivalent values, there may be multiple sorted permutations. Stability then determines how equivalent elements are ordered relative to the chosen input traversal.

Therefore a generic “container sort” should not be sold as producing a canonical representation unless its order and equivalence assumptions are strong enough to prove that claim.

This also matters for trees: even if the flattened values have a canonical sorted list, refilling those values into a fixed tree shape produces a traversal-sorted tree, not necessarily a canonical tree representation.

---

## 25. Why List should remain central even in the generic theory

It may seem aesthetically disappointing to define a generic container layer by converting to List. In the present system that is a feature, not a flaw.

List currently serves three valuable roles:

1. it is the representation on which sortedness/permutation specifications already exist;
2. it is a canonical finite linear observation boundary;
3. it isolates representation-independent correctness from termination-specific `SizedList` internals.

Once the generic proof interface is stable, A Program can replace the backend sequence representation if concrete performance or expressiveness evidence justifies it.

Starting by removing List from the architecture would discard an already verified boundary before a replacement abstraction has demonstrated value.

---

## 26. Final assessment

### Finding A — no current evidence of a fundamental expressiveness defect; the earlier cross-prover comparison was too surface-level

The fact that current QuickSort is specialized to `List A` does not show that A Program is uniquely unable to express generic sorting. Lean, Agda, Rocq, and other proof ecosystems often retain list/array-specific *surface sorting functions*, but a deeper inspection shows significant genericity in their proof layers: finite-index permutations, abstract sequence interfaces, ghost semantic sequences, common sorting specifications, and representation refinement. The correct comparison is therefore not merely function signature versus function signature.

### Finding B — the missing abstraction is semantic, not coinductive

What A Program currently lacks is a reusable **law-bearing finite linear view / sorting-specification layer**, not a Stream type.

### Finding C — strong-positive IADTs are sufficient territory for the first abstraction

A finite view/refill dictionary can in principle be ordinary source-level data/functions with proofs. Strict positivity is not the obstacle.

### Finding D — arbitrary-IADT sorting should be rejected as a goal

Not every IADT represents a homogeneous finite collection, not every collection has an intrinsic position order, and not every container has a meaningful same-shape sort.

### Finding E — generic specification should precede generic native algorithms

The correct next experiment is to package current QuickSort as a verified `SortingAlgorithm` **and compare two semantic presentations**: (1) transport through `FiniteContents` / `FiniteLinearView`, and (2) a `Fin n` observation whose primary result is a sorting permutation. The proof-assistant follow-up makes the second option substantially more important than the first revision suggested.

Only after at least two native sequence representations require shared algorithm code should A Program consider representation-generic QuickSort/MergeSort operation algebras.

### Finding F — MergeSort should be treated separately from the current insertion-based merge fixture

A conventional verified MergeSort can later inhabit the same backend interface. The generic-container design should not depend on claiming that such a backend is already complete today.

### Finding G — C `qsort` supports capability-specific generalization, not arbitrary-container generalization

C `qsort` is highly generic over element representation and comparator, but its storage contract is a finite array of equal-width objects. Its useful lesson for A Program is to generalize a native algorithm over **explicit finite linear access capabilities**, not to erase the distinction between arrays, lists, trees, and arbitrary IADTs.

### Finding H — the best common layer for QuickSort and MergeSort is semantic

C++'s separation between random-access `std::sort` and linked-list `list::sort`, together with Lean's separate List/Array sorting implementations, argues against one maximal operational “sortable container” interface. Share `sorted + permutation (+ optional stability/cost)` at the proof boundary; share lower-level operations only where concrete representations truly have the same efficient capabilities.

### Finding I — a finite linear-sequence layer is a real cross-language abstraction, not an ad-hoc compromise

Common Lisp `sequence`, C++/D/Swift range capabilities, Nim `openArray`, and Java `List` all show useful abstractions strictly between “concrete List/Array” and “arbitrary container”. A Program should be willing to introduce an explicit `FiniteSequence`/`FiniteLinearView` layer rather than forcing every finite IADT through one universal container notion.

### Finding J — `Before/Less(index,index) + Swap(index,index)` has unusually strong independent precedent

Ada 2022, Go, and Zig independently expose nearly the same operational sorting kernel. This is stronger evidence than C `qsort` alone for an optional A Program `IndexedSortKernel`. It also suggests that `get` need not belong to the minimal execution interface even if it remains useful in the proof interface.

### Finding K — normalize/collect/sort/reconstruct is mainstream, not a second-class workaround

Java `List.sort` normalizes through an array and refills the list; Python, Ruby, Elixir, Kotlin, Go's iterator sorting, Julia's iterator-like dictionary sorting, and Lean's unordered-to-List APIs all use canonical sequence materialization in some form. A Program should not reject `FiniteContents -> sequence backend -> refill/result` merely because the native representation is temporarily lost inside the implementation.

### Finding L — permutation/index output deserves first-class consideration, with direct theorem-prover precedent

D `makeIndex`, Julia `sortperm`, R `order`, and MATLAB's optional sort-index output expose the ordering permutation as useful data. More importantly for this audit, Lean Mathlib directly defines `Tuple.sort : (Fin n -> α) -> Equiv.Perm (Fin n)`. Agda's finite permutation and functional-vector permutation APIs independently validate the same finite-position viewpoint. In A Program this form should therefore be considered a candidate semantic center, not merely an optional convenience.

### Finding M — proof assistants frequently put representation genericity in refinement rather than in the executable sort type

Isabelle/Sepref's arrays-implement-list-interface development is the clearest example: abstract list operations, including indexed swap, are related by refinement to mutable arrays. F*/Pulse, Why3, and Dafny likewise use ghost mathematical sequences/contents to state generic correctness while concrete storage remains representation-specific. This explains why a search limited to `sort` signatures underestimates existing genericity.

### Finding N — A Program should distinguish semantic permutation from physical permutation action

A theorem such as “there exists/compute `σ : Perm (Fin n)` such that `f ∘ σ` is sorted” is more representation-independent than “return another `X`”. A separate `PermutationAction`/`refill` law can then explain how a particular `List`, vector, fixed tree shape, packed array, or future native storage realizes `σ`. This separation localizes dependent-index and shape-preservation obligations.

### Finding O — the opportunity is synthesis, not invention ex nihilo

The proposed architecture combines patterns that are already independently validated: Lean's permutation-first tuple sorting, Agda's finite-position permutations, Rocq's proof-strength/order modularity, Isabelle's data refinement, and F*/Pulse's common proof-carrying sorting class. The novel contribution for A Program would be to place these ideas into one deliberately small IADT-oriented hierarchy, not to claim that generic verified sorting has no precedent.

---

## 27. Recommended decision

**Proceed with a source-level generic sorting specification experiment, but do not generalize QuickSort/MergeSort directly to arbitrary IADTs. Treat finite-position permutation as a first-class candidate semantic center.**

The proof-assistant-specific survey now supports two closely related prototype directions: transport through a finite linear view, and permutation-first sorting over `Fin n`. They should be compared before committing the public abstraction. The recommended direction is therefore:

```text
                       generic sorting semantics
                                |
               +----------------+----------------+
               |                                 |
               v                                 v
     finite-content/linear view          finite-position presentation
       -> sequence backend               (Fin n -> A)
       -> refill/result                       |
                                             v
                                      sortPermutation : Perm (Fin n)
                                             |
                                      act/refill on carrier
```

These routes can share the same `Sorted + Permutation` theorem package. The second route is no longer merely an optional optimization: Lean Mathlib demonstrates that a finite-position permutation can itself be the primary mathematical sort result.

The broader survey therefore suggests the following layered picture rather than a single interface:

```text
FiniteContents                         -- can expose finite elements
      |
FiniteLinearView / FiniteSequence      -- chooses ordered positions; may refill
      |
      +--> SortingAlgorithm            -- Sorted + Permutation specification
      |
      +--> FinitePositionPresentation  -- observe as Fin n -> A
                 |
                 v
          sortPermutation              -- first-class semantic ordering permutation
                 |
          PermutationAction/refill     -- representation-specific realization

IndexedSortKernel                      -- separate native execution capability
  (finite index range + Before/Less + Swap)
```

The `IndexedSortKernel` track is supported directly by Ada 2022, Go, and Zig, and indirectly by C++/D/Swift random-access abstractions. It should be judged as **native representation polymorphism**, not as the definition of generic finite sorting itself.

A useful design principle from PHP/MATLAB/Julia is also to state explicitly **what structure the sort preserves**: element multiset, labelled entries/key association, shape, axis, or an index permutation. Do not hide this choice behind a generic name `sort`.

Coinduction/codata/Stream is deliberately left for a separate design discussion and should not be coupled to this change.

---

## 28. Sources reviewed

All external library/API claims in the expanded cross-language section were rechecked on 2026-09-27.

### A Program

- Repository: https://github.com/repyt-margorp/a-program
- `README.md`, current `main` as inspected 2026-09-27.
- `tests/acceptance/generic-quick-sorted-result.p`, current `main` as inspected 2026-09-27.
- `tests/fixtures/generic_sorted/content-result-proof.p`, current `main` as inspected 2026-09-27.
- `doc/2026-09-14-MERGE-AND-GENERIC-QUICKSORT-AUDIT.md` (historical audit; used only with its date/scope understood).
- `DESIGN-PHILOSOPHY.md` (historical design background; current README/source controls current architecture claims).
- The original revision of this audit recorded repository head `5c064c2` on 2026-09-26. This expanded revision describes current `main` source/API behavior rather than making the old SHA a claim about the final repository head at every later moment.

### Lean

- Lean Language Reference, Linked Lists / `List.mergeSort`: https://lean-lang.org/doc/reference/latest/Basic-Types/Linked-Lists/
- Lean Language Reference, Arrays / `Array.qsort`: https://lean-lang.org/doc/reference/latest/Basic-Types/Arrays/
- Lean API, `Init.Data.Array.QSort.Basic`: https://lean-lang.org/doc/api/Init/Data/Array/QSort/Basic.html
- Lean API, `Init.Data.Array.Sort.Lemmas`: https://lean-lang.org/doc/api/Init/Data/Array/Sort/Lemmas.html
- Lean 4.30.0 release notes (`Array.mergeSort`, 2026-05-26): https://lean-lang.org/doc/reference/latest/releases/v4.30.0/
- mathlib `Finset.sort`: https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Finset/Sort.html
- mathlib `Multiset.sort`: https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Multiset/Sort.html

### Agda

- Agda stdlib `Data.Container.Core`: https://agda.github.io/agda-stdlib/master/Data.Container.Core.html
- Agda stdlib `Data.Container.Indexed.Core`: https://agda.github.io/agda-stdlib/master/Data.Container.Indexed.Core.html
- Agda stdlib `Data.List.Sort.Base`: https://agda.github.io/agda-stdlib/master/Data.List.Sort.Base.html
- Agda stdlib `Data.List.Sort`: https://agda.github.io/agda-stdlib/master/Data.List.Sort.html

### Rocq

- Rocq 9.1 stdlib `Stdlib.Sorting.Mergesort`: https://rocq-prover.org/doc/V9.1.0/stdlib/Stdlib.Sorting.Mergesort.html

The present revision intentionally does not expand the coinduction/codata comparison; that topic is deferred.

### Idris 2

- Idris 2 `Data.List` source (`mergeBy`, `sortBy`, `sort`): https://raw.githubusercontent.com/idris-lang/Idris2/main/libs/base/Data/List.idr
- Idris 2 `Prelude.Interfaces` source (`Foldable.toList`, `Traversable`): https://raw.githubusercontent.com/idris-lang/Idris2/main/libs/prelude/Prelude/Interfaces.idr
- Idris 2 repository: https://github.com/idris-lang/Idris2

### C / POSIX

- POSIX / Open Group `qsort`: https://pubs.opengroup.org/onlinepubs/009695399/functions/qsort.html
- POSIX Programmer's Manual mirror, `qsort(3p)`: https://man7.org/linux/man-pages/man3/qsort.3p.html
- cppreference C `qsort`: https://en.cppreference.com/w/c/algorithm/qsort
- cppreference C++ `std::qsort`: https://en.cppreference.com/w/cpp/algorithm/qsort

The important contract facts used here are: array/table storage, element count, fixed element width, comparator callback, in-place reordering; the standard does not require the internal algorithm to be QuickSort and does not guarantee stability or complexity.

### C++ capability comparison

- `std::sort`: https://en.cppreference.com/w/cpp/algorithm/sort
- `std::list::sort`: https://en.cppreference.com/w/cpp/container/list/sort

These are used only as a design comparison: `std::sort` requires random-access iterators, whereas linked lists have their own native sort.

### Broad programming-language/library survey

- Ada 2022 Reference Manual, A.18.26 Array Sorting / `Ada.Containers.Generic_Sort`: https://www.adaic.org/resources/add_content/standards/22rm/html/RM-A-18-26.html
- Go `sort` (`Interface`, `Sort`, `Stable`): https://pkg.go.dev/sort
- Go `slices` (`Sort`, `SortFunc`, `Sorted`, `SortedFunc`): https://pkg.go.dev/slices
- Zig `std.sort` context kernels: https://github.com/ziglang/zig/blob/master/lib/std/sort.zig
- Zig `std.mem.sortUnstableContext`: https://github.com/ziglang/zig/blob/master/lib/std/mem.zig
- Zig `MultiArrayList` context sorting example: https://github.com/ziglang/zig/blob/master/lib/std/multi_array_list.zig
- D `std.algorithm.sorting` (`sort`, capability constraints, `makeIndex`): https://dlang.org/phobos/std_algorithm_sorting.html
- Rust slice sorting: https://doc.rust-lang.org/std/primitive.slice.html
- Swift `MutableCollection.sort`: https://developer.apple.com/documentation/swift/mutablecollection/sort%28%29
- Swift `RandomAccessCollection`: https://developer.apple.com/documentation/swift/randomaccesscollection
- Java SE 26 `List.sort`: https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/util/List.html
- Java SE 26 `Arrays.sort`: https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/util/Arrays.html
- Kotlin `MutableList` sorting APIs: https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.collections/-mutable-list/
- Scala 3 `Seq` sorting: https://www.scala-lang.org/api/3.x/scala/collection/immutable/Seq.html
- Scala `scala.util.Sorting`: https://www.scala-lang.org/api/current/scala/util/Sorting%24.html
- .NET `List<T>.Sort`: https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1.sort
- .NET LINQ `Enumerable.OrderBy`: https://learn.microsoft.com/dotnet/api/system.linq.enumerable.orderby
- F# `List`, `Array`, and `Seq` modules: https://fsharp.github.io/fsharp-core-docs/
- Python `sorted`: https://docs.python.org/3/library/functions.html#sorted
- Ruby `Enumerable#sort`: https://ruby-doc.org/3.4.1/Enumerable.html
- Ruby `Array` sorting: https://ruby-doc.org/3.4.1/Array.html
- Common Lisp HyperSpec `sort` / `stable-sort`: https://www.lispworks.com/documentation/HyperSpec/Body/f_sort_.htm
- Clojure core `sort`: https://clojure.github.io/clojure/branch-master/clojure.core-api.html
- Elixir `Enum.sort`: https://hexdocs.pm/elixir/Enum.html#sort/1
- Haskell `Data.List` and `Data.Foldable`: https://hackage.haskell.org/package/base/docs/Data-List.html and https://hackage.haskell.org/package/base/docs/Data-Foldable.html
- OCaml list/array documentation: https://ocaml.org/docs/lists and https://ocaml.org/docs/higher-order-functions
- SML/NJ `ListMergeSort` and `ArrayQSort`: https://smlnj.org/doc/smlnj-lib/Util/str-ListMergeSort.html and https://smlnj.org/doc/smlnj-lib/Util/str-ArrayQSort.html
- Julia sorting and `sortperm`: https://docs.julialang.org/en/v1/base/sort/
- MATLAB R2026b `sort`: https://www.mathworks.com/help/matlab/ref/double.sort.html
- R base `sort` / `sort.int`: https://search.r-project.org/R/refmans/base/html/sort.html
- PHP sorting-array semantics (`sort`, `asort`, key association): https://www.php.net/manual/en/array.sorting.php
- Nim `std/algorithm` (`sort(openArray)`, `sorted -> seq`): https://nim-lang.org/docs/algorithm.html
- JavaScript typed-array sorting: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/sort
- Lua 5.4 `table.sort`: https://lua.org/manual/5.4/manual.html
- Erlang `lists`: https://www.erlang.org/doc/apps/stdlib/lists.html
- Scheme SRFI 132 sorting library: https://srfi.schemers.org/srfi-132/srfi-132.html

The purpose of this survey is not to count APIs. It is to identify recurring abstraction boundaries that have survived across language families: contiguous records; linear sequences; iterator/range capabilities; index/compare/swap kernels; canonical collect-and-sort results; and permutation/index outputs.

### Proof-assistant/refinement follow-up (2026-09-27 correction)

- Lean Mathlib, `Mathlib.Data.Fin.Tuple.Sort`: https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Fin/Tuple/Sort.html
  - `Tuple.sort : (Fin n -> α) -> Equiv.Perm (Fin n)` is the most direct precedent found for permutation-first sorting.
- Agda stdlib, `Data.Fin.Permutation`: https://agda.github.io/agda-stdlib/v2.3/Data.Fin.Permutation.html
- Agda stdlib, functional-vector permutation relation: https://agda.github.io/agda-stdlib/v2.4/Data.Vec.Functional.Relation.Binary.Permutation.html
- Isabelle AFP/Sepref, abstract List interface and indexed swap: https://isa-afp.org/browser_info/current/AFP/Sepref_IICF/Refine_Imperative_HOL.IICF_List.html
- Isabelle AFP/Sepref, **Plain Arrays Implementing List Interface**: https://isa-afp.org/browser_info/current/AFP/Sepref_IICF/Refine_Imperative_HOL.IICF_Array.html
- Isabelle Imperative HOL verified QuickSort: https://isabelle.in.tum.de/website-Isabelle2022/dist/library/HOL/HOL-Imperative_HOL/Imperative_Quicksort.html
- F*/Pulse Proof-Oriented Programming, **On Abstraction & Rubrics**: https://fstar-lang.org/tutorial/book/agentic/agentic_sorting_algorithms.html
  - factors insertion sort, merge sort, heapsort, and quicksort through a polymorphic proof-carrying `array_sort` class; later enriches the class with complexity instrumentation.
- Rocq 9.1 `Stdlib.Sorting.Mergesort`: https://rocq-prover.org/doc/V9.1.0/stdlib/Stdlib.Sorting.Mergesort.html
- Why3 module interfaces: https://why3.org/doc/syntaxref.html
- Why3 generic array sorted/permutation predicates: https://why3.org/stdlib/array.html
- Why3 mathematical sequence sortedness: https://why3.org/stdlib/seq.html
- Dafny mathematical collections / multisets: https://dafny.org/latest/OnlineTutorial/ValueTypes

These sources are used to correct the earlier inference from surface sorting APIs. They show that proof-oriented genericity is frequently located in finite permutations, semantic models, specifications, and refinement relations rather than in one executable container-polymorphic `sort` function.

### Theory

- Michael Abbott, Thorsten Altenkirch, Neil Ghani, **“Containers: Constructing Strictly Positive Types”**, *Theoretical Computer Science* 342(1), 2005. DOI: 10.1016/j.tcs.2005.06.002.
- Mauro Jaskelioff, Russell O'Connor, **“A Representation Theorem for Second-Order Functionals”**, *Journal of Functional Programming* 25, e13, 2015. DOI: 10.1017/S0956796815000088. The paper explicitly derives that traversable functors are finitary containers.
- Jeremy Gibbons, Bruno C. d. S. Oliveira, **“The Essence of the Iterator Pattern”**, *Journal of Functional Programming* 19(3-4), 2009. DOI: 10.1017/S0956796809007291.

## 29. Non-claims

This audit does not claim:

- that every strongly-positive IADT is a categorical container in the exact formal sense without further encoding work;
- that A Program currently supports all higher-kinded abstractions one might want for a polished container library;
- that current QuickSort is stable;
- that current QuickSort has a proved complexity bound;
- that the current insertion-based merge fixture is a conventional verified MergeSort;
- that arbitrary infinite streams can be sorted productively;
- that a future Stream/codata feature would be unnecessary for other A Program goals;
- that `FiniteContents` / `FiniteLinearView` are final names or final syntax;
- that C `qsort` sorts linked lists, trees, or arbitrary containers;
- that the C standard requires the `qsort` implementation to use QuickSort;
- that a `get`/`swap` interface alone proves constant-time random access;
- that QuickSort and MergeSort should share one native operational interface;
- that every `Iterable`, `Enumerable`, `Foldable`, or traversable value can be reconstructed in the same carrier after sorting;
- that `Before/Less + Swap` by itself proves constant-time access or any particular complexity bound;
- that Common Lisp `sequence` is an example of sorting arbitrary containers — it is evidence for a broad **linear sequence** abstraction;
- that a value-level permutation theorem automatically gives the right preservation law for keyed, labelled, indexed, or dependent structures;
- that one native sorting backend should be forced on QuickSort, MergeSort, list, array, tree, matrix, and associative carriers merely for API uniformity.
- that existing theorem provers already provide one universal executable `sort` over every finitary/container representation; the corrected claim is that they contain several of the semantic/refinement components separately.

The claim is narrower: **the generic finite-sorting problem can be factored cleanly at source level without making coinduction or arbitrary-IADT sorting prerequisites.**
