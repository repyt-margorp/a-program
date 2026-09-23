# Verification addendum — 2026-09-24

Tracking issue: https://github.com/repyt-margorp/a-program/issues/32

## Classification and latest-revision verification

Design proposal / proof-interface audit, **not a demonstrated soundness bug or authorization to delete witness machinery**.

Fresh GitHub clone on 2026-09-24: `a72cda371109fdbf84d747456ed0aeb09af2391e`, `rewrite/pointer-core-hott`.

Confirmed in the current source:
- README exposes generated `@function` and global `*function`, separately from local IH syntax.
- `function_graph.c` keeps graph formation/declaration and packet/witness state separately (including `witness_status`).
- The recursive-call comment around lines 584–585 says the symbolic output is not asserted equal by conversion; the companion witness supplies it.
- `sort-quick-property.p` proves a graph theorem and consumes it through global `*quickSort` and the associated output evidence.
- The existing Nat QuickSort proof checks: `done steps=456336`, with the complete dependency assembly from `tests/sort_insertion.sh`.

The original document is retained as a proposal. No replacement adequacy operator, witness removal, artifact migration, or performance comparison was implemented or verified here.

## Problem / design question

There are two necessary semantic roles: expose function computation structure for induction, and connect that graph to the result of ordinary application. It is not established that the second role needs a separately exposed global `*f` surface operator.

Conversely, deleting that operator without a checked replacement can leave only `@f x y -> P y`, with no construction of evidence for the ordinary result `P (f x)`.

The question is whether global `*f` can become a derived/internal adequacy mechanism while graph reasoning remains available. Separate internal objects do not by themselves prove duplicate evaluation or redundant implementation. The current shared checked branch tree must not be described as two unrelated evaluators.

## Requested staged experiment

1. Preserve `@f` and local `*tail`, `*down`, `*leftGraph`, etc.
2. Specify the adequacy contract: result agreement, branch and recursive/helper-call agreement, dependent indices, effects and totality.
3. First remove explicit global `*f` only from the **surface use** of a real theorem while retaining the existing internal witness generator.
4. Check an ordinary-result theorem for the unchanged Nat QuickSort, including actual consumers.
5. Check wrong endpoints/comparators/evidence and source-only, partial and completed artifact rechecking.
6. Measure solver work and representation size before claiming reduced duplication.
7. Only after equivalent expressive power and checking are demonstrated, decide whether any internal packet/witness mechanism should change.

No global equality reflection, automatic unfolding of opaque recursive results, new unrestricted evaluation inside types, or removal of local IH syntax is requested. Effectful or partial computation requires a separately specified contract, not an unconditional pure adequacy theorem.

The attached audit includes rationale, alternatives and verification gates. Related context: #29/#30 and the existing graph-first proof-interface documentation. This issue is separate from generic comparator motive selection.

## Historical audit follows

The supplied audit below is preserved as a dated record. Its proposals are not implemented by this documentation PR. Statements about prior execution or publication refer to the original audit date; the scope of fresh verification is given above.

---

# Audit: Function Graphs, `@func`, and Global `*func` Witnesses in A Program

**Date:** 2026-09-21  
**Target:** current pointer-core implementation on `rewrite/pointer-core-hott`  
**Status:** design audit / proposal, not an implementation decision  
**Primary implementation area:** `src/prototype/pointer/function_graph.c`  
**Primary acceptance example:** `src/prototype/pointer/tests/acceptance/sort-quick-property.p`

---

## 1. Executive summary

The current A Program design exposes two related but conceptually different function-graph mechanisms:

- `@func`, which denotes a generated inductive graph describing the internal computational structure of a function, and
- global `*func`, which materializes a successful ordinary computation together with a witness inhabiting that graph.

The current surface documentation describes both together as “generated function graphs and witnesses”. The implementation, however, already separates the graph declaration/formation from the packet/witness construction. This separation matters.

The central conclusion of this audit is:

> **The function graph represented by `@func` has a strong independent justification.  
> The global `*func` witness operator does not yet have the same justification and is a plausible redundancy.**

More precisely, two semantic roles are required:

1. a structural representation of a function's internal computation, sufficient to expose intermediate values such as `lower`, `upper`, recursive-call results, partition evidence, and branch structure to proofs; and
2. an adequacy connection showing that the ordinary result of `func x` is related to `x` by the generated graph.

The first role is naturally provided by `@func`.

The second role does **not** logically require a second user-visible evaluator-like construct `*func`. It may instead be provided by a compiler-generated or kernel-checked adequacy theorem, by a derived eliminator, or by proof elaboration that internally constructs the required graph evidence.

Accordingly, the design question should not be stated as:

> “Can function witnesses be removed?”

but rather as:

> **“Can the current independently materialized global `*func` witness layer be replaced by a smaller adequacy interface derived from the ordinary function and its graph?”**

This distinction is important because graph inhabitants remain logically necessary somewhere. Removing global `*func` without supplying any replacement adequacy bridge would make it possible to prove facts of the form

```text
@f x y -> P y
```

while losing the ability to transport those facts to the actual ordinary computation

```text
P (f x).
```

Therefore the proposed direction is:

```text
ordinary function f
        +
generated graph @f
        +
kernel/compiler-generated adequacy bridge
        +
graph induction/elimination
```

rather than:

```text
ordinary function f
        +
generated graph @f
        +
second user-visible evaluator *f
```

The audit further recommends keeping the following distinctions strict:

- global `*func` is not the same concept as local `*field`;
- `*leftGraph`, `*rightGraph`, `*down`, and `*tail` are induction hypotheses associated with recursive fields and should not be removed merely because global `*func` may be redundant;
- removal of global `*func` must not force the removal of `@func`;
- artifact serialization must preserve semantic graph structure and whatever evidence is necessary for independent checking, but need not preserve a redundant surface syntax merely to reconstruct `*func`.

The current QuickSort proof makes the issue especially visible. `@quickSort` is genuinely useful because it exposes the internal names and proof-relevant structure required by the proof. By contrast, the final use of

```text
*quickSort ... sample @ys =>
    read_sorted ys (quick_correct sample ys @ys)
```

primarily performs an adapter role between ordinary computation and the already-proved graph theorem. That adapter is the strongest candidate for elimination.

---

## 2. Scope of this audit

This document addresses the status of generated function graphs and witnesses in the current pointer-core rewrite.

It does **not** propose eliminating function graphs.

It does **not** propose eliminating induction hypotheses such as `*tail`, `*down`, `*leftGraph`, or `*rightGraph`.

It does **not** claim that ordinary definitional equality is already sufficient to prove graph adequacy for every supported function.

It does **not** claim that the current witness implementation can simply be deleted without replacement.

Instead, the audit asks four narrower questions:

1. Why is `@func` currently useful or necessary?
2. What semantic work does global `*func` actually perform?
3. Which parts of that work are intrinsically necessary, and which parts are representational duplication?
4. What interface could preserve the proof power of the current system while reducing the “type-theoretic assembly language” character of the surface language?

---

## 3. Current repository observations

### 3.1 Surface language explicitly exposes both graph and witness syntax

The current top-level README lists:

```text
Generated function graphs and witnesses: @function and *function
for the supported fragment, distinct from schema Self and branch IH syntax.
```

This is already an important distinction: the repository documentation explicitly says that generated function `*function` is **not** the same mechanism as schema Self or branch induction-hypothesis syntax.

That distinction should be preserved in all future design discussion.

### 3.2 The implementation internally separates graph construction and witness construction

`src/prototype/pointer/function_graph.c` maintains separate state for:

```text
formation
declaration
packet
witness
motive
witness_branches
witness_status
```

The generated graph schema is constructed before the witness is completed.

The code exposes separate implementation APIs including conceptually:

```text
pg_function_graph_formation(...)
pg_function_graph_declaration(...)
pg_function_graph_witness(...)
pg_function_graph_packet(...)
```

This is evidence that the implementation itself does not require us to think of graph and witness as a single indivisible semantic primitive.

They are already separate products of the function-graph subsystem.

### 3.3 Recursive-call results in the graph are deliberately not identified by conversion

A particularly important implementation comment states, in substance:

```text
A recursive call creates an output variable.
It is not asserted equal by conversion.
The companion witness supplies the variable from the corresponding IH.
```

This is a sound and conservative design choice.

It prevents the graph builder from inventing definitional equality between an unknown recursive result and some term merely because the source contains a recursive call.

However, it also reveals the exact purpose of the witness machinery:

> the witness layer instantiates symbolic graph outputs using actual recursive computation evidence.

This is a stronger and more precise statement than “`*func` proves the function is correct”.

The witness is primarily a **realization mechanism for a separately generated relational graph**.

### 3.4 Graph schema and witness share one checked branch tree

The witness code comments also state that source, schema, and witness share one branch tree, with checked constructor refinements.

This is good evidence that the intended semantics is not “two unrelated evaluators”.

Nevertheless, source computation and witness materialization are still represented as separate derived objects, and the surface language exposes them as two distinct operations.

That duplication is exactly what should be audited.

---

## 4. Why `@func` has an independent reason to exist

Consider an ordinary function:

```text
f : (x : A) -> B x
```

From this type alone a theorem can mention:

```text
x
f x
```

but cannot name arbitrary intermediate values created during the implementation of `f`.

For a QuickSort implementation those values include, depending on the branch:

```text
pivot
tail
lowerSize
lower
upperSize
upper
lowerBound
upperBound
partitioning
left
right
leftGraph
rightGraph
appending
```

An ordinary function result does not expose these values as stable proof binders.

One could unfold the function body, but body unfolding is not equivalent to an inductive proof interface:

- local names are not necessarily stable under normalization;
- recursive calls are not ordinary structural subterms;
- intermediate computations may involve sequencing, effects, helper functions, or total-result projection;
- a proof that depends directly on arbitrary reduction shape is fragile under implementation refactoring;
- recursive-call hypotheses must be presented with an appropriate motive.

The generated function graph solves this by **reifying the internal computation into an inductive family**.

Abstractly, if:

```text
f : Π x : A. B x
```

then its graph has the shape:

```text
G_f : Π x : A. Π y : B x. @
```

where constructors correspond to supported computational cases of `f`.

The graph constructor telescope can expose exactly the intermediate values required by a proof.

This is why, in the current design, `@func` is not merely a decorative trace.

It is a **proof-facing structural interface to the implementation of a function**.

### 4.1 The QuickSort example

The current QuickSort correctness theorem has the form:

```text
quick_correct :
    (xs : List Nat) ->
    (output : List Nat) ->
    @quickSort Nat (&natLessOrEqual) xs output ->
    Sorted output
```

The proof pattern-matches on the generated QuickSort graph.

That graph gives the proof access to the measured list, accessibility evidence, recursive sort graph, partitioning evidence, and the eventual result.

At the lower `quickSortAcc` level, graph induction exposes the `lower` and `upper` partitions and recursive graph children. These are exactly the structures needed to prove:

- all lower elements are ordered relative to the pivot;
- all upper elements are ordered relative to the pivot;
- recursively sorted partitions are sorted;
- appending the left result, pivot, and right result preserves sortedness.

Without *some* graph-like structural interface, those locally defined entities are not addressable in a robust theorem statement.

Therefore this audit does **not** recommend removing function graphs.

---

## 5. What global `*func` appears to mean

The current graph relation can be modeled abstractly as:

```text
G_f x y
```

meaning that the generated graph relates input `x` to output `y`.

The global witness mechanism behaves approximately like:

```text
W_f : Π x : A. Σ y : B x. G_f x y
```

or, in a computation-aware formulation, a total computation returning a packet containing:

```text
output : B x
graph  : G_f x output
```

The current `function_graph.c` implementation literally constructs a packet type containing the output and graph proof, then builds witness branches returning such packets.

Thus `*func` is not merely an alias for `@func`.

Its job is approximately:

> run or reconstruct the function computation while simultaneously constructing an inhabitant of the generated graph that corresponds to the produced output.

At the surface level the QuickSort test uses it as:

```text
*quickSort ... sample @ys =>
    read_sorted ys (quick_correct sample ys @ys);
```

The proof `quick_correct` has already established:

```text
@quickSort xs output -> Sorted output
```

The global `*quickSort` construct then supplies:

```text
ys
@ys : @quickSort xs ys
```

so that the theorem can be applied to the actual result.

This is an adapter between:

```text
ordinary computation
```

and:

```text
relational graph theorem.
```

That adapter role is necessary.

The specific global `*func` representation of the adapter is not obviously necessary.

---

## 6. The key distinction: graph evidence is necessary; a second surface evaluator may not be

Suppose we retain:

```text
f   : Π x : A. B x
G_f : Π x : A. Π y : B x. @
```

To transport a graph theorem to the result of the ordinary function, we need some form of:

```text
adequacy_f : Π x : A. G_f x (f x)
```

or an equivalent dependent statement compatible with A Program's computation/value distinction.

Then a theorem written against the graph:

```text
graph_property :
    Π x : A.
    Π y : B x.
    G_f x y ->
    P x y
```

immediately yields:

```text
ordinary_property :
    Π x : A.
    P x (f x)
```

by:

```text
ordinary_property x =
    graph_property x (f x) (adequacy_f x)
```

No user-visible `*f` is required.

The logical requirement is therefore:

```text
G_f x (f x)
```

not:

```text
*f
```

as a special syntax category.

This is the central argument for auditing `*func` as redundant.

---

## 7. Why this is not merely syntax sugar

It would be too weak to describe the proposal as:

> “Hide `*func` with prettier syntax.”

The stronger proposal is:

> **Do not treat a separately callable witness runner as part of the user-visible semantic vocabulary unless there is a theorem or capability that cannot be obtained from `f`, `@f`, and an adequacy bridge.**

Today the implementation contains:

```text
source function
graph formation/declaration
packet type
witness function
```

The audit asks whether the last two need to survive as long-lived semantic objects, or whether they are an implementation strategy for proving/realizing adequacy.

The distinction affects:

- surface grammar;
- generated artifacts;
- serialization;
- solver state;
- proof search;
- implementation complexity;
- conceptual duplication;
- future refactoring freedom.

If `*func` is retained only because current graph evidence happens to be produced by a packet-returning witness function, then a temporary implementation technique has leaked into the language model.

That is exactly the type of abstraction leakage the pointer rewrite should try to avoid.

---

## 8. `@func` and `*func` are asymmetric

The current syntax makes them visually symmetric:

```text
@func
*func
```

but their semantic roles are not symmetric.

### `@func`

`@func` provides a new proof object: the graph relation itself.

It introduces information not present in the ordinary extensional function type.

Specifically, it exposes:

- branch identity;
- constructor refinements;
- named intermediate values;
- recursive-call outputs;
- recursive graph edges;
- helper-call relations where supported;
- evidence associated with the internal computational route.

This is semantically additive.

### global `*func`

Global `*func` primarily relates two representations of computation that already exist:

```text
ordinary f x
```

and:

```text
@f x y.
```

It is therefore closer to an adequacy/correctness bridge than to a new computational structure.

This asymmetry strongly suggests that graph and witness should not automatically receive equal status in surface syntax.

---

## 9. Do not confuse global `*func` with local `*field`

This is the most important notation warning in the entire audit.

The following uses of `*` are conceptually different.

### 9.1 Recursive schema/field induction hypothesis

Examples:

```text
*tail
*down lowerSize lowerBound
```

For `Acc`, a field such as:

```text
down : (y : A) -> R y x -> * y
```

contains recursive Self occurrences.

When the enclosing object is eliminated by induction, the corresponding induction result is available through `*down`.

This is an ordinary induction principle over a recursive field.

Likewise `*tail` is the induction result for the recursive `tail`.

These uses should be analyzed as **local induction-hypothesis accessors**.

### 9.2 Function-graph induction hypothesis

Inside an induction over a function graph, recursive graph fields may produce:

```text
*leftGraph
*rightGraph
```

These are again local induction hypotheses, now over recursive graph edges.

They are structurally analogous to `*tail` and `*down`.

### 9.3 Global `*func`

By contrast:

```text
*quickSort ...
```

does not merely access an induction hypothesis already available in a local elimination branch.

It invokes the separately generated function witness machinery and returns an output-plus-graph packet.

Therefore:

> **Removing global `*func` must not imply removing local `*field`.**

Conflating these two mechanisms would damage a useful uniformity of the existing induction design.

---

## 10. Why the current design feels like a type-theoretic assembly language

A Program's low-level flavor is not primarily caused by dependent types themselves.

It arises when compiler-internal proof wiring becomes part of ordinary source code.

In the QuickSort proof pipeline the user can currently be required to see and manually connect:

```text
ordinary quickSort
@quickSort
*quickSort
@quickSortAcc
recursive graph fields
*leftGraph
*rightGraph
```

The underlying semantics may be elegant, but the surface requires the user to know which representation of “the same computation” is currently in scope.

This resembles assembly-level programming because the programmer is manually routing intermediate semantic resources that a higher-level elaborator could derive.

A useful design principle is:

> **Expose a low-level semantic object when it gives the user genuinely new expressive access.  
> Do not expose a low-level adapter merely because the implementation uses it internally.**

Under that principle:

- `@func` currently passes the test;
- global `*func` is questionable.

---

## 11. Why ordinary unfolding is not a replacement for `@func`

A tempting counterproposal is:

> “If we remove generated graph syntax entirely, simply unfold `f` during the proof.”

This is not equivalent.

Graph reification gives proofs a controlled inductive interface over function structure.

Raw unfolding instead couples proofs to:

- beta-reduction details;
- sequencing representation;
- helper inlining;
- normalization strategy;
- local administrative redexes;
- future compiler transformations.

Moreover, a recursive function's semantic recursion need not correspond to structural recursion over the syntactic output term.

QuickSort is a clear example.

The graph can preserve the meaningful proof-level vocabulary:

```text
lower
upper
partitioning
left
right
```

even when the actual evaluator or IR representation changes.

Thus `@func` can act as a **proof ABI** for an implementation.

If A Program wants post-hoc proofs about ordinary functions, this is valuable.

---

## 12. The actual problem introduced by removing global `*func`

Assume we simply delete global `*func`.

We can still write:

```text
quick_correct :
    (xs : List Nat) ->
    (output : List Nat) ->
    @quickSort ... xs output ->
    Sorted output
```

But when we evaluate:

```text
quickSort ... xs
```

we have no graph inhabitant to pass to `quick_correct`.

The theorem remains disconnected from actual execution.

Therefore deletion is sound only if one of the following replaces the current witness interface.

### Option A: generated adequacy theorem

Generate something conceptually equivalent to:

```text
quickSort.graph :
    (xs : List Nat) ->
    @quickSort ... xs (quickSort ... xs)
```

The theorem can remain internal if the elaborator inserts it automatically.

### Option B: derived function induction principle

Instead of exposing the relation inhabitant, synthesize a theorem/recursor that directly proves properties of the ordinary result:

```text
induct_quickSort :
    <branch proofs with recursive IHs> ->
    (xs : List Nat) ->
    P xs (quickSort xs)
```

The function graph remains the implementation from which this eliminator is generated, but no graph inhabitant needs to appear in ordinary user code.

### Option C: on-demand graph reconstruction

When a theorem requires:

```text
@f x (f x)
```

the kernel/elaborator reconstructs a checked graph derivation from the already-owned typing/evaluation evidence.

This avoids a persistent witness function but may be expensive unless evidence is cached or represented carefully.

### Option D: retained internal witness, no surface `*func`

The implementation may temporarily continue generating the existing witness function internally, while removing global `*func` from the language surface.

This is the lowest-risk first implementation step.

It would establish that `*func` is not needed as a user-facing abstraction before attempting to simplify internal witness construction.

---

## 13. Recommended staged interpretation

The audit recommends separating three questions that are currently easy to collapse.

### Stage 1 — surface question

Can ordinary user programs and proofs stop using global `*func`?

Likely yes, provided elaboration can insert the existing witness automatically.

This is the easiest claim to validate empirically.

### Stage 2 — semantic API question

Does the language model need a first-class global witness function as a public semantic object?

Probably not, if an adequacy theorem/interface is sufficient.

This requires replacing every public use of witness packets with a graph adequacy interface.

### Stage 3 — implementation question

Can `packet` and `witness` construction in `function_graph.c` itself be removed or substantially reduced?

This is not yet established.

Current graph construction deliberately leaves recursive outputs symbolic and relies on companion witness logic to realize them.

A replacement must account for:

- recursive calls;
- helper calls;
- dependent outputs;
- captured arguments;
- specialization;
- total/pure computations;
- effects in whatever fragment is supported;
- constructor refinement;
- graph induction hypotheses.

Therefore the final implementation simplification is a research/refactoring task, not a mechanical deletion.

---

## 14. A proposed semantic decomposition

The system should conceptually distinguish four objects.

### 14.1 Ordinary function

```text
f : Π x : A. B x
```

This is the computational interface.

### 14.2 Function graph

```text
@f : Π x : A. Π y : B x. @
```

This is the proof-facing intensional interface.

### 14.3 Graph eliminator / induction principle

```text
rec_@f
```

or an equivalent implicit mechanism.

This exposes graph branches and recursive induction hypotheses.

### 14.4 Adequacy bridge

```text
adequacy_f :
    Π x : A. @f x (f x)
```

with the exact formulation adapted to A Program's CBPV/total-result semantics.

Only the first two necessarily deserve stable source-level names.

The latter two can be generated and possibly invoked implicitly.

The current global `*f` appears to combine pieces of 3 and 4 with result materialization in a way that is convenient for implementation but not necessarily ideal as a language abstraction.

---

## 15. The QuickSort transformation

### 15.1 Current pattern

The current acceptance proof ends in the style:

```text
quick_correct :=
    \xs =>
    \output =>
    \g : @quickSort ... xs output =>
        <graph proof>;

main :=
    *quickSort ... sample @ys =>
        read_sorted ys
            (quick_correct sample ys @ys);
```

This explicitly asks the user to bridge:

```text
quickSort
```

to:

```text
@quickSort.
```

### 15.2 Desired user-level pattern

A higher-level interface should allow:

```text
quick_correct :
    (xs : List Nat) ->
    Sorted (quickSort Nat (&natLessOrEqual) xs)
```

while still allowing the proof implementation to use graph structure.

Conceptually:

```text
quick_correct xs :=
    graph_induction quickSort xs {
        ...
        lower
        upper
        IH_lower
        IH_upper
        ...
    };
```

The exact syntax is open.

The important point is that:

```text
output
g : @quickSort ... xs output
```

need not be part of the theorem's public statement unless the theorem is intentionally about graph executions rather than the function result.

### 15.3 Internal elaboration

The above could elaborate to:

```text
quick_correct_graph :
    (xs : List Nat) ->
    (output : List Nat) ->
    @quickSort ... xs output ->
    Sorted output

quick_correct xs :=
    quick_correct_graph
        xs
        (quickSort ... xs)
        (adequacy_quickSort xs)
```

This preserves the current proof method exactly while deleting the manual global `*quickSort` plumbing from user code.

---

## 16. Why the result projection precedent matters

The current repository already contains an instructive design precedent in the total-pure result projection work.

That design introduces an explicit Core semantic reference for a typed operation but deliberately does **not** expose it as a general-purpose surface function.

The lesson is not that function witnesses are identical to total-result projection.

They are not.

The relevant architectural principle is:

> **A semantic operation may be explicit and canonical below the surface without becoming permanent user syntax.**

Therefore an argument of the form:

> “The kernel needs this operation, therefore the language must expose `*func`”

would be inconsistent with an architectural distinction the pointer rewrite already uses elsewhere.

---

## 17. Adequacy must be explicit in the design even if hidden in syntax

Eliminating global `*func` is safe only if adequacy becomes clearer, not less clear.

The key invariant is approximately:

```text
for each supported f and valid x,
the ordinary evaluation of f x and the graph relation @f
describe the same output.
```

This requires at least the following obligations.

### 17.1 Result agreement

If ordinary evaluation produces `y`, the generated graph evidence must relate `x` to that same `y`.

No independent witness evaluator may silently compute a different result.

### 17.2 Branch agreement

The graph constructor used must correspond to the same source/evaluation branch taken by the ordinary computation.

### 17.3 Recursive-call agreement

For every recursive call, the graph child must correspond to the ordinary recursive result.

The current implementation handles this through companion witness/IH machinery.

Any replacement must provide the same guarantee.

### 17.4 Helper-call agreement

If generated graphs expose helper-function calls, their graph evidence must agree with the helper's ordinary semantics.

### 17.5 Dependent typing agreement

If `B x` or an intermediate result type depends on earlier values, graph evidence must preserve the same indices and substitutions as ordinary execution.

### 17.6 Effects and totality boundary

The adequacy theorem must be scoped to exactly the function fragment for which generated function graphs are sound.

It must not silently treat effectful, partial, or unsupported computations as total pure functions.

### 17.7 Identity / higher structure

No proposal here should be taken to establish general Higher Identity coherence.

Graph adequacy must integrate with the existing typed evidence and conversion policy without turning graph evidence into unrestricted definitional equality.

---

## 18. Do not solve adequacy by broadening definitional equality

The current implementation correctly avoids asserting unknown recursive-call outputs equal by conversion.

That discipline should be preserved.

Removing `*func` must not lead to a shortcut such as:

```text
recursive graph output == ordinary recursive call result
```

being inserted into global DefEq merely because both “should compute the same”.

Instead the correspondence should remain proof-relevant or rule-governed.

Possible representations include:

```text
@f x (f x)
```

or an internal adequacy evidence object.

This preserves the separation between:

- computation;
- conversion;
- propositional/graph evidence.

That separation is one of the pointer-core rewrite's strongest architectural choices.

---

## 19. Artifact implications

The artifact format should not be designed around preservation of surface `*func` syntax.

The artifact should preserve what is semantically authoritative.

Depending on the eventual implementation, that may include:

```text
ordinary function graph/root
generated function graph schema
typing derivations/evidence
graph adequacy evidence or enough information to reconstruct/check it
retained reductions where explicitly requested
source metadata for presentation/reassembly
```

It does not follow that the artifact must contain:

```text
a distinct surface-level *func entity
```

if that entity is merely a derived adapter.

This matters for the broader A Program goal that artifact representation should not constrain future surface syntax.

A function graph is potentially a durable semantic object.

A particular witness convenience syntax is not necessarily durable.

---

## 20. Refactoring and proof-stability implications

Retaining `@func` while reducing dependence on global `*func` gives a useful separation.

### Function implementation refactoring

A graph should intentionally change when proof-relevant internal structure changes.

That is acceptable: graph-dependent proofs are intentionally intensional.

### Ordinary clients

Clients that only need:

```text
f x
```

should not know that a graph witness exists.

### Ordinary correctness theorems

A theorem such as:

```text
Sorted (quickSort xs)
```

should ideally survive changes to how the graph witness is materialized.

Its proof may still require graph branch updates if QuickSort's proof-facing structure changes, but its theorem statement should not mention the current witness packaging strategy.

This reduces unnecessary coupling between:

```text
the statement being proved
```

and:

```text
the current internal proof transport mechanism.
```

---

## 21. Potential objections

### Objection 1: “If `*func` is removed, how do we obtain the result binder `ys`?”

Use the ordinary function result:

```text
quickSort xs
```

If a local name is desirable:

```text
ys := quickSort xs;
```

The result does not need to be reintroduced through a witness-specific syntax.

### Objection 2: “How do we get the graph proof corresponding to `ys`?”

Through generated adequacy:

```text
adequacy_quickSort xs :
    @quickSort xs (quickSort xs)
```

or by elaborator insertion.

### Objection 3: “Isn't adequacy just `*func` under another name?”

It may initially be implemented using the same machinery.

But the abstraction is different.

`*func` is currently a result-producing surface construct.

Adequacy is a theorem/interface connecting two already-defined semantic objects.

That distinction allows the implementation to change later without preserving the witness runner as a language concept.

### Objection 4: “Could `@func` also disappear?”

Possibly from ordinary surface syntax in a future higher-level language.

But this audit does not recommend that now.

For proofs that intentionally depend on internal names and branch structure, the graph is providing real expressive access.

### Objection 5: “Could everything be proved directly by induction on inputs?”

Not for every intended function and property without redoing substantial implementation reasoning.

Function graphs exist precisely to turn intensional computation structure into an inductive proof object.

For post-hoc proofs of existing functions, this remains valuable.

---

## 22. Stronger possibility: generated ordinary-result induction

A more ambitious design can eliminate even explicit graph evidence from most theorem source while retaining `@func` internally.

Given:

```text
@f
```

the compiler can derive a function-specific induction principle whose motive is stated directly over:

```text
x
f x.
```

For QuickSort, branch proof contexts would expose:

```text
pivot
lower
upper
partitioning
left
right

IH_left  : P lower left
IH_right : P upper right
```

with the necessary equality/adequacy facts tying `left` and `right` to recursive function results.

This would let the programmer reason at a level similar to:

```text
induction f
```

rather than:

```text
g : @f x y
g @case...
```

However, this should be considered a later elaboration layer.

The present graph is still useful as the canonical derivation source of such an induction principle.

---

## 23. Recommended design direction

### Recommendation A — preserve `@func` graph semantics

Do not remove function graphs merely to simplify syntax.

They currently provide a unique proof interface to function-internal structure.

### Recommendation B — demote global `*func` from language primitive to derived/internal mechanism

First remove the need for users to write it.

Keep the existing witness implementation internally if necessary.

### Recommendation C — introduce an explicit adequacy contract

Document the intended invariant connecting:

```text
f x
```

and:

```text
@f x y.
```

Do not leave that relationship implicit in the existence of witness packets.

### Recommendation D — keep local `*field` induction syntax independent

Do not change `*tail`, `*down`, `*leftGraph`, or related branch-IH syntax merely as part of the global witness cleanup.

### Recommendation E — make ordinary theorem statements ordinary

Prefer:

```text
(xs : List Nat) -> Sorted (quickSort xs)
```

over:

```text
(xs : List Nat) ->
(output : List Nat) ->
@quickSort xs output ->
Sorted output
```

when the intended theorem is a property of the ordinary function.

The graph-shaped theorem may remain as an internal helper or intentionally exported lower-level theorem.

### Recommendation F — retain an escape hatch for graph-level properties

Properties of execution structure itself may genuinely quantify over:

```text
@f x y
```

Examples include:

- branch-sensitive claims;
- call-tree properties;
- cost models;
- trace/event properties;
- proof of a particular internal invariant.

The design should not force every graph theorem into an extensional theorem about `f x`.

---

## 24. Concrete implementation experiment

A low-risk experiment can test the thesis without rewriting `function_graph.c`.

### Phase 1

Keep all existing graph/witness generation unchanged.

Add an internal elaboration path equivalent to:

```text
adequacy_f x
```

using the current generated witness.

### Phase 2

Rewrite one acceptance proof, preferably QuickSort Sorted, so that the public theorem becomes:

```text
quick_correct :
    (xs : List Nat) ->
    Sorted (quickSort Nat (&natLessOrEqual) xs)
```

and does not contain global `*quickSort`.

Internally it may call a graph theorem plus hidden adequacy.

### Phase 3

Add a negative test ensuring that arbitrary graph evidence is not synthesized for:

- unsupported function shapes;
- partial computations;
- effectful computations outside the supported contract;
- a mismatched output.

### Phase 4

Measure solver transitions against the existing proof.

This is important because an abstraction improvement that silently duplicates a multi-million-transition graph witness would be ergonomically better but computationally worse.

### Phase 5

Only after the surface experiment succeeds, audit whether:

```text
packet
witness
witness_branches
witness_status
```

can be replaced internally by a smaller adequacy derivation.

---

## 25. Verification gates for any `*func` removal

A proposal should not be accepted merely because examples still parse.

At minimum:

- [ ] Existing `@function` graph theorem statements still synthesize.
- [ ] Graph branch fields remain accessible with the same dependent types.
- [ ] Recursive graph induction still supplies local `*leftGraph` / `*rightGraph` IHs.
- [ ] `Acc` and ordinary recursive-field induction remain unchanged.
- [ ] QuickSort `Sorted` can be stated and proved directly about `quickSort xs`.
- [ ] QuickSort content-preservation proof still passes.
- [ ] Helper-function graph dependencies remain sound.
- [ ] Dependent indexed function graphs remain sound.
- [ ] Unsupported graph fragments remain rejected/pending rather than guessed.
- [ ] Effectful functions do not acquire an invalid pure adequacy theorem.
- [ ] Source and saved program images agree after reload.
- [ ] Retained reductions cannot forge graph adequacy under an older policy.
- [ ] Debug and ASan/UBSan acceptance pass.
- [ ] Solver-transition cost is measured before and after.
- [ ] No new rule broadens global definitional equality merely to connect graph outputs to function outputs.

---

## 26. A note on naming

The current notation contributes to conceptual confusion because:

```text
*tail
*down
*leftGraph
*quickSort
```

all visually look like instances of the same operation.

They are not.

A future surface design should consider reserving `*` for the genuinely uniform concept:

> induction result associated with a recursive field available in the current elimination context.

Under that interpretation:

```text
*tail
*down ...
*leftGraph
```

fit naturally.

Global:

```text
*quickSort
```

does not.

This is another argument for removing or renaming the global witness form even if its current internal implementation remains.

Such a cleanup would make the language conceptually smaller:

```text
*
```

would have one meaning rather than two nearby but distinct meanings.

---

## 27. Broader architectural interpretation

The current A Program surface is close to a typed proof-oriented IR.

That is not inherently a defect.

The danger appears when implementation support objects become permanent syntax and therefore constrain:

- source-language evolution;
- artifact format;
- proof ergonomics;
- optimization;
- refactoring.

Function graphs are plausibly fundamental semantic IR because they preserve intentional proof structure.

Global function witness runners are more plausibly compiler support infrastructure.

The design should therefore distinguish:

```text
semantic structure worth preserving
```

from:

```text
machinery currently used to establish that structure's relationship to execution.
```

This distinction is especially important for A Program because the project explicitly aims to keep Core extremely small and to place typing, evidence, contexts, substitutions, and derivations above it.

The same minimality principle should be applied one level higher:

> do not elevate a derived evidence transport mechanism into permanent surface semantics unless it provides independent expressive power.

---

## 28. Open questions requiring implementation-level investigation

The audit does not claim the following questions are resolved.

### 28.1 Can adequacy be derived without constructing the current packet?

The current witness machinery constructs a packet containing both output and graph evidence.

It must be determined whether direct evidence:

```text
@f x (f x)
```

can be produced more cheaply and simply.

### 28.2 Can recursive result binding use ordinary result projection safely?

The graph planner currently introduces symbolic output variables rather than equating them by conversion.

A replacement must preserve this discipline.

The existing total-pure result projection mechanism may help in some cases, but it must not be misused as universal recursive-result equality.

### 28.3 What is the contract for helper calls?

`function_graph.c` can depend on helper graph witnesses.

If global witnesses are removed internally, helper adequacy needs an explicit replacement.

### 28.4 How should non-total/effectful functions be represented?

The current generated function graph feature is a supported fragment, not a theorem about arbitrary A Program computations.

Adequacy must remain fragment-indexed or otherwise explicitly constrained.

### 28.5 Should graph constructor names be part of the stable proof API?

If `@func` is retained as a proof ABI, the project should decide whether generated case names and field names are stable source-facing identifiers or implementation-derived names.

This is separate from the `*func` question but becomes more important if graphs are intentionally retained.

---

## 29. Provisional conclusion

The present evidence supports the following architecture:

```text
                    USER / SURFACE

              f x
               |
               | theorem about ordinary result
               v
             P (f x)

              ^             optional explicit graph reasoning
              |                         |
       hidden adequacy                   v
              |                       @f x y
              |                         |
              +-------------------------+
                         graph induction


                 SEMANTIC / EVIDENCE LAYER

             ordinary function f
                    |
                    +------ generated graph G_f
                    |
                    +------ checked adequacy connection

```

The architecture to avoid making permanent is:

```text
ordinary f
   |
   +------ @f graph
   |
   +------ *f second surface execution/witness interface
```

unless a future example demonstrates independent expressive power of global `*f` that cannot be represented as graph adequacy or derived elimination.

The strongest current interpretation is therefore:

> **`@func` is a plausible essential proof-facing representation because it names internal computational structure that ordinary function application does not expose.**
>
> **Global `*func` is plausibly an implementation-derived realization/adequacy adapter and should be treated as removable until shown otherwise.**
>
> **The graph witness relation itself cannot disappear: it must be replaced by an explicit, kernel-checked adequacy path from ordinary function computation to the retained graph.**
>
> **Local `*field` induction hypotheses are a separate mechanism and should remain unaffected.**

This conclusion should be tested first by removing global `*func` from a real theorem's *surface use* while retaining the current internal witness generator. QuickSort Sorted is the appropriate first target because it already exercises:

- `Acc`-based well-founded recursion;
- generated function graphs;
- recursive graph IHs;
- partition proof data;
- an ordinary executable result;
- post-hoc correctness.

If that experiment succeeds without loss of expressiveness and without unacceptable solver duplication, the next audit should determine whether the packet/witness implementation itself can be collapsed into a smaller adequacy mechanism.

---

## 30. Repository evidence reviewed

This audit was written against the current `rewrite/pointer-core-hott` repository state available on 2026-09-21, especially:

- `README.md`
  - surface language declaration of `@function` and `*function`;
  - distinction from schema Self and branch IH syntax;
  - current pointer-core status;
  - current QuickSort proof status and solver-cost note.

- `src/prototype/pointer/function_graph.c`
  - separate graph `formation` / `declaration`;
  - separate `packet` / `witness`;
  - recursive-call output variables;
  - explicit comment that recursive outputs are not equated by conversion;
  - companion witness use of recursive IHs;
  - shared source/schema/witness branch tree;
  - witness packet construction.

- `src/prototype/pointer/tests/acceptance/sort-quick-property.p`
  - graph theorem over `@quickSort`;
  - graph-based `quick_sorted` proof;
  - final global `*quickSort` adapter use;
  - direct ordinary QuickSort value alongside packet-derived values.

- `doc/2026-09-14-TOTAL-PURE-RESULT-PROJECTION.md`
  - precedent for a semantically explicit Core operation that is deliberately not exposed as a general-purpose surface function;
  - continued separation between typed evidence and unrestricted definitional equality.
