# Function Graphs, Generated Witnesses, and Reassessment of `Returns`

Date: 2026-08-22

Status: revised research audit and design recommendation; no language
implementation change

Repository: `repyt-margorp/a-program`

Audited revision: `29131adfff09e9d6c6d4bcc888acf450baf36bde`

Audited revision date: 2026-08-21 22:51:25 +09:00

Audited revision subject: `Enforce totality performance gates`

## 1. Question

A Program can already define an ordinary recursive function such as `length`
or the `Acc`-driven `quickSort`. It can also define an indexed family that
describes the same function's input/output derivations. For example:

```text
LengthEvaluatesTo : NatList -> Nat -> @
```

with constructors corresponding to the `nil` and `cons` clauses.

What is still required to prove an optional property about the already-defined
function?

The motivating questions are:

1. Is the missing bridge another Predicate?
2. Can the existing generic `#.Returns` or `#.Terminates` Predicate replace a
   function-specific family such as `LengthEvaluatesTo` or `QuickSortGraph`?
3. Must every function receive its own Predicate?
4. Is a tactic or a special function-induction principle required?
5. How should the answer respect A Program's CBPV, IADT, Context,
   TypedOccurrence, Claim, Derivation, artifact, and future partiality
   boundaries?

## 2. Executive conclusion

The mail discussion identified the correct implementation gap, but the phrase
"missing bridge Predicate" is slightly inaccurate.

`LengthEvaluatesTo` is already the Predicate, more precisely an indexed type
family. What is missing is a checked **bridge term/theorem** connecting that
family to the existing executable definition:

```text
length_graph :
	(xs : NatList) ->
	LengthEvaluatesTo xs (length xs)
```

The initial version of this research recommended retaining both the generic
`#.Returns` relation and function-specific graphs. Subsequent design discussion
showed that this is not the minimum architecture for the stated goal.

For a property of an already-defined, globally named function, the complete
proof path is:

```text
executable function
	-> generated function-graph witness
	-> ordinary elimination of the generated graph IADT
	-> optional property
```

`Returns` does not occur in that path. The proposed surface projections are:

```text
@length
	the generated proof-relevant graph family for the named definition length

*length
	the generated canonical graph-witness function for total length
```

Schematically:

```text
@length : NatList -> Nat -> @

*length :
	(xs : NatList) ->
	@length xs (length xs)
```

The executable function `length` is not coerced into an inhabitant of a type.
`@length` and `*length` denote two additional ordinary objects generated from
the accepted definition: an IADT family and a witness function.

The public `Returns` primitive is therefore not logically necessary for
post-definition properties of named functions. Its remaining independent use
would be a generic postcondition interface quantified over an arbitrary
computation, including anonymous blocks, sequencing, and handlers. If A Program
does not require that interface, or requires such computations to receive a
generated graph identity before they are used in a property, public
`#.Returns` may be removed.

This document now recommends a graph-first proof interface and a staged audit
for removing public `#.Returns`. The currently implemented Returns Terms,
Claims, Derivations, Context origins, artifact fields, and tests remain facts
about revision `29131ad`; this research does not silently describe them as
already deleted.

The recommended first implementation does **not** require a Lean-style
`quickSort.induct` or any new tactic:

1. retain a definition-time splitting/definition tree;
2. generate a normal function-specific IADT from that tree;
3. generate the canonical graph-witness function and its replayable
   Claims/Derivations;
4. let the user eliminate the generated IADT with A Program's ordinary IADT
   mechanism; and
5. treat future tactics only as optional elaborator conveniences that construct
   the same ordinary proof terms.

No function-specific rule belongs in the kernel. The family and witness are
generated ordinary objects checked by the existing kernel mechanisms.

## 3. The first correction: the bridge is not another Predicate

The following four objects must be distinguished.

```text
length
	ordinary function term

LengthEvaluatesTo
	function-specific indexed type family

oneRun
	one inhabitant of one indexed instance

length_graph
	function producing a graph inhabitant for every input
```

Their schematic classifiers are:

```text
length : NatList -> Nat

LengthEvaluatesTo : NatList -> Nat -> @

oneRun :
	LengthEvaluatesTo
		(NatList.cons Nat.zero NatList.nil)
		(Nat.succ Nat.zero)

length_graph :
	(xs : NatList) ->
	LengthEvaluatesTo xs (length xs)
```

The family determines which input/output pairs may be justified. Its
constructors determine how evidence is built. Neither fact by itself proves
that the separately defined `length` follows those rules.

That last assertion is the type inhabited by `length_graph`. Therefore:

> The missing object is not a fourth Predicate. It is a term inhabiting a Pi
> type whose codomain is the already-existing graph Predicate.

This distinction is exactly analogous to:

```text
Nat
	a type

Nat.zero
	a term inhabiting Nat
```

and:

```text
LengthEvaluatesTo xs n
	a type

run : LengthEvaluatesTo xs n
	a term inhabiting that type
```

## 4. The mail example, reproduced at the current revision

### 4.1 Ordinary function

The mail used the following ordinary A Program definition:

```a-program
Nat := @{
	zero : *;
	succ : * -> *;
};

NatList := @{
	nil : *;
	cons : Nat -> * -> *;
};

length := \xs : NatList =>
	xs
		@nil => Nat.zero
		@cons head tail => Nat.succ *tail;

length :: NatList -> Nat;
```

Here `*tail` is the existing structural-recursion/IH surface operation.

### 4.2 Manually written function graph IADT

The same input/output relation can be represented by an ordinary indexed
family:

```a-program
LengthEvaluatesTo :=
	@\input : NatList =>
	@\output : Nat =>
	{
		nilCase : * NatList.nil Nat.zero;

		consCase :
			(head : Nat) ->
			(tail : NatList) ->
			(tailLength : Nat) ->
			* tail tailLength ->
			* (NatList.cons head tail) (Nat.succ tailLength);
	};
```

Expanding the recursive `*` occurrences gives:

```text
LengthEvaluatesTo.consCase :
	(head : Nat) ->
	(tail : NatList) ->
	(tailLength : Nat) ->
	LengthEvaluatesTo tail tailLength ->
	LengthEvaluatesTo
		(NatList.cons head tail)
		(Nat.succ tailLength)
```

The corresponding inference rule is:

```inference
LengthEvaluatesTo tail tailLength
────────────────────────────────────────────────────────────────────
LengthEvaluatesTo (NatList.cons head tail) (Nat.succ tailLength)
```

### 4.3 Concrete graph inhabitants work

At revision `29131ad`, the following concrete inhabitants compile:

```a-program
emptyRun := LengthEvaluatesTo.nilCase;
emptyRun :: LengthEvaluatesTo NatList.nil Nat.zero;

oneRun := LengthEvaluatesTo.consCase
	Nat.zero NatList.nil Nat.zero emptyRun;
oneRun :: LengthEvaluatesTo
	(NatList.cons Nat.zero NatList.nil)
	(Nat.succ Nat.zero);
```

The current reader reports `LengthEvaluatesTo` with two constructors and accepts
all expectations. This confirms that the IADT itself and concrete membership
evidence are not the missing features.

### 4.4 The general bridge still fails

The natural bridge term is:

```a-program
lengthCorrect := \xs : NatList =>
	xs
		@nil => LengthEvaluatesTo.nilCase
		@cons head tail =>
			LengthEvaluatesTo.consCase
				head tail (length tail) *tail;

lengthCorrect ::
	(xs : NatList) -> LengthEvaluatesTo xs (length xs);
```

At revision `29131ad`, this fails with:

```text
P0 IH replay failed derivation=60 claim=60 occurrence=41 ...
P0 accepted proof graph validation failed
failed to compile AST graph
```

This reproduces the mail's result at a newer revision than its audited
`b13f047` baseline.

The failure is not evidence that the intended theorem is false. It shows that
the current IH replay mechanism cannot connect:

- the recursive call made by the previously defined `length`;
- the recursive proof call being made by `lengthCorrect`;
- the refined output index `length tail`; and
- the constructor premise `LengthEvaluatesTo tail (length tail)`.

That connection is precisely what definition-time graph/bridge generation must
preserve.

## 5. What the literature actually provides

### 5.1 Generic evaluation relations are language-wide

Big-step operational semantics relates a program directly to its final result.
Leroy and Grall summarize the standard distinction as follows: big-step
semantics relates programs to final evaluation results, while small-step
semantics forms sequences of one-step reductions. They also emphasize proofs by
induction over big-step evaluation derivations.

This supports a generic relation of the shape:

```text
Evaluates program value
```

or, at A Program's CBPV boundary:

```text
Returns computation value
```

This Predicate belongs to the language semantics, not to one particular
function. See [Leroy and Grall, *Coinductive Big-Step Operational Semantics*](https://cgi.cse.unsw.edu.au/~rvg/SpecialIssueSOS/21/leroy-grall-coindsem.pdf).

### 5.2 Rocq `Function` generates one graph relation per function

Rocq's legacy `Function` facility explicitly generates:

- the function;
- an inductive relation `R_f` corresponding to its graph;
- `f_complete` and `f_correct` information linking the function and graph;
- its equation; and
- induction principles reflecting the pattern-matching structure.

This is unusually direct evidence for the bridge discussed here. The official
manual describes the graph and the two linking results explicitly. See
[Rocq 9.2, Functional induction](https://rocq-prover.org/doc/V9.2.0/refman/using/libraries/funind.html).

The Equations Reloaded paper states the logical result in the form:

```text
f_graph x y <-> f x = y
```

for the Function package's complete graph. See
[Sozeau and Mangin, *Equations Reloaded*, Section 6.1](https://sozeau.gitlabpages.inria.fr/www/research/publications/Equations_Reloaded-ICFP19.pdf).

Thus established practice does use a function-specific Predicate, but it is
generated as an ordinary inductive relation. It is not a new primitive logical
constant or kernel rule for each function.

### 5.3 Equations uses one definition representation to derive several views

Equations compiles dependent pattern matching and recursion into core terms and
derives clause equations, a function graph, and an elimination principle. Its
paper describes an intermediate splitting tree and derives the one-step
unfolding, equations, graph, and elimination principle from that common source.

See:

- [official Equations overview](https://rocq-prover.org/docs/equations-docs);
- [Equations Reloaded, compilation pipeline and proof principles](https://sozeau.gitlabpages.inria.fr/www/research/publications/Equations_Reloaded-ICFP19.pdf); and
- [official Equations basics tutorial](https://rocq-prover.github.io/platform-docs/equations/tutorial_basics.html).

The important lesson for A Program is not the `funelim` syntax. It is the
single-source generation rule:

```text
definition clauses
	-> splitting tree
		-> executable core term
		-> equation theorems
		-> graph IADT
		-> bridge/elimination theorems
```

### 5.4 Lean proves that a graph IADT is not the only implementation

Current Lean 4 generates functional induction principles by inspecting the
accepted recursive definition structure. Its implementation mirrors branches
and collects one induction hypothesis for every recursive call. For
well-founded recursion it works from `WellFounded.fix`; for structural
recursion it recognizes the generated recursion structure.

See:

- [Lean 4 `FunInd.lean` specification and implementation overview](https://github.com/leanprover/lean4/blob/master/src/Lean/Meta/Tactic/FunInd.lean); and
- [Lean tactic reference for `fun_induction`](https://lean-lang.org/doc/reference/latest/Tactic-Proofs/Tactic-Reference/#tactic-fun_induction).

Therefore the following statement is valid only at the conceptual level:

> A functional induction principle has the same branch/IH shape that ordinary
> induction over a suitable function graph would have.

It is not valid to claim that current Lean necessarily implements functional
induction by first creating a public graph IADT. Lean demonstrates that the
principle can instead be derived directly from the recursion representation.

A Program may nevertheless choose the explicit graph-IADT route because it
matches its current indexed-family model and the user's desired inspectability.

### 5.5 Isabelle also generates function-shaped proof interfaces

Isabelle's function package produces defining simplification equations and a
custom induction rule following the recursive structure of each definition.
Its official tutorial shows `sep.simps`, `sep.induct`, and proofs using
`induct ... rule: sep.induct`.

See [Isabelle/HOL, *Theory Functions*](https://www.isabelle.in.tum.de/library/Doc/Functions/Functions.html).

Again, the essential mechanism is retained definition structure plus generated
checked theorems. Tactic syntax is secondary.

### 5.6 Bove-Capretta's per-function Predicate has a different purpose

Bove and Capretta generate a special-purpose inductive domain/accessibility
Predicate for a general recursive algorithm. It characterizes inputs on which
the algorithm terminates; the function then recurses structurally over evidence
of that Predicate.

See [Bove and Capretta, *Modelling General Recursion in Type Theory*](https://www.cambridge.org/core/journals/mathematical-structures-in-computer-science/article/abs/modelling-general-recursion-in-type-theory/B02BBE1A0A38C44189D53B01D659ECE5).

This must not be confused with a result graph:

```text
Domain_f x
	there is enough evidence to justify recursive evaluation from x

Graph_f x y
	evaluation of f at x follows a particular derivation and returns y
```

For a total function both may hold for every input, but they retain different
information. A Program's current `Acc` evidence addresses the first concern.
It does not provide the output-indexed evidence needed for `Sorted y`.

### 5.7 Predicate transformers specify results without exposing recursion

Dijkstra monads and weakest-precondition semantics give a generic way to state
properties of effectful computations. A computation transforms a postcondition
on its outcomes into the precondition needed to establish it. This underlies
F*, Hoare Type Theory, and related systems.

See [Ahman et al., *Dijkstra Monads for Free*](https://fstar-lang.org/papers/dm4free/).

In simplified A Program notation:

```text
PartialEnsures M P :=
	(v : A) -> Returns M v -> P v

TotalEnsures M P :=
	Terminates M and PartialEnsures M P
```

These combinators answer **what property is specified**. They do not, by
themselves, provide the function-specific branch/IH structure needed to prove
the property. A verifier may discharge the resulting obligations by unfolding,
equations, graph induction, SMT, or another proof procedure.

### 5.8 A universal execution tree is possible but should not be conflated

For effects and possible divergence, a proof-relevant universal object such as:

```text
Run M outcome
```

can serve as a common semantic core. `Returns M v` may then be a view of runs
ending in `Returned v`, while termination, traces, and effect safety are other
views.

Interaction Trees are a closely related established approach to representing
recursive and impure computations. Their authors contrast executable trees
with relational operational semantics and discuss effects, recursion,
divergence, traces, and equational reasoning. See
[Xia et al., *Interaction Trees: Representing Recursive and Impure Programs in Coq*](https://www.pure.ed.ac.uk/ws/portalfiles/portal/286180994/Interaction_Trees_XIA_DOA14102019_VOR_CC_BY.pdf).

This is relevant to A Program's future partial/effectful layer, but it does not
imply that today's `Returns` witness already contains an eliminable source-level
QuickSort tree.

## 6. The initially considered two-view architecture

The first research pass separated **semantic generality** from **proof shape**
by retaining both Returns and a function graph. This remains a coherent option,
but Section 7 explains why it is not mandatory and why the revised
recommendation prefers the graph-first interface.

### 6.1 Generic endpoint semantics

```text
#.Returns (&M) v
```

is language-wide. It says that computation `M` reaches a normal return of `v`.
It is the currently implemented generic result bridge at the CBPV boundary.

### 6.2 Function-specific derivation structure

```text
LengthEvaluatesTo xs n
QuickSortEvaluatesTo xs ys
```

are definition-specific. Their constructors correspond to the source
definition's clauses and recursive calls. Their ordinary IADT elimination
therefore exposes exactly the hypotheses wanted by later proofs.

### 6.3 Why specialization matters

Suppose `Returns` is defined by generic Core evaluation rules. Induction over a
fully exposed `Returns` derivation could produce cases for:

- thunk and force;
- beta reduction;
- return;
- computation sequencing;
- every Match step;
- operation handling; and
- administrative Core forms.

This is semantically general but a poor public proof interface for a theorem
about QuickSort. A generated `QuickSortEvaluatesTo` compresses those steps into
constructors corresponding to meaningful algorithm clauses and recursive calls.

Function-specific graphs can therefore be understood as **specialized,
source-level views** of generic execution semantics.

They need not be independent axioms. Under the revised graph-first design they
may replace public `Returns` for named definitions, while any retained internal
evaluation judgement remains implementation authority rather than a second
user-facing proof interface.

## 7. Reassessment: a graph-first interface can remove public `Returns`

### 7.1 What the implemented `Returns` already solves

`Returns M v` solves the result-binding problem:

```text
M : computation producing A
v : A
r : Returns M v
```

A dependent property may now mention the particular returned value `v`:

```text
consumeResult :
	(v : A) -> Returns M v -> P v -> P v
```

The current A Program implementation supports this kind of explicit consumer.
This records the implemented baseline; it does not show that `Returns` is the
only possible or smallest interface.

### 7.2 What the current `Returns` does not solve

The current `#.Returns` is an intrinsic object relation whose witnesses are
validated by named kernel rules. It is not declared as an ordinary user IADT
with public constructors for every evaluation rule. There is no general
eliminator that turns a `Returns (&(quickSort xs)) ys` witness into:

- the selected QuickSort branch;
- the partition result;
- the two recursive calls; and
- graph evidence for their returned values.

Consequently, the statement:

```text
(xs : List A) ->
(ys : List A) ->
#.Returns (&(quickSort A le xs)) ys ->
Sorted ys
```

is a possible generic postcondition boundary, but current `Returns` alone does
not provide the induction skeleton needed to implement the theorem.

### 7.3 Defining the graph as an abbreviation is extensionally enough but
proof-theoretically weak

One could define:

```text
QuickSortEvaluatesTo xs ys :=
	#.Returns (&(quickSort xs)) ys
```

This gives a function-specific name, but no new constructors. Eliminating it is
still just eliminating `Returns`. It therefore does not recover QuickSort's
source recursion shape.

The useful generated graph is not merely an alias. It is an inductive
presentation specialized from the accepted definition tree.

### 7.4 When `Returns` does not appear

For a fixed total named definition, the minimum interface is:

```text
@f : A -> B -> @

*f :
	(x : A) ->
	@f x (f x)

propertyFromGraph :
	(x : A) -> (y : B) ->
	@f x y ->
	P x y
```

The final property is ordinary composition:

```text
propertyOfFunction :
	(x : A) -> P x (f x)

propertyOfFunction x :=
	propertyFromGraph x (f x) (*f x)
```

There is no logical hole for `Returns` to fill. `*f` is the function-to-graph
bridge, and the graph inhabitant is already the proof-relevant evidence of the
returned output.

### 7.5 What `@f` and `*f` mean

The surface notation denotes generated definition-associated objects:

```text
f
	the executable named Term

@f
	the generated graph type family

*f
	the generated canonical graph-witness Term
```

Internally these may be stored under stable generated names such as
`f.Graph` and `f.graphWitness`. The prefix operators are projections from a
global definition identity; they are not general reflection over arbitrary
Terms.

The following restrictions are required:

- `@f` and global `*f` apply only to elaborated global named definitions;
- a local higher-order variable or anonymous Lambda has no graph unless an
  explicit graph interface is passed with it;
- an imported opaque definition exposes these projections only when its
  artifact exports the generated graph and witness;
- aliases use definition/export identity, not accidental TermDB hash-consing
  identity; and
- exactly one canonical execution graph is selected for the prefix notation.

The existing local form `*tail` keeps its current meaning: use the induction
hypothesis belonging to a recursive Match-case binder. Resolution is:

```text
*name where name is an induction-enabled local Match binder
	current ordinary IH

*name where name is a global named TOTAL definition
	generated canonical function-graph witness

otherwise
	compile error
```

This is an intentional overload, not an assertion that the two Terms have the
same classifier.

### 7.6 Parsing feasibility and current conflicts

At the audited revision, term-level `*name` is accepted only for an
induction-enabled local Match binder. Type-level `*` denotes the recursive Self
type. Prefix `@` in a type expression denotes a fresh Universe; `@\index` and
`@{...}` introduce indexed-family structure, while infix Match clauses use
`@constructor` in a different parser state.

The proposed syntax is feasible, but it is not already implemented. It needs
explicit lookahead and new AST forms:

```text
@ followed by a global definition name
	FUNCTION_GRAPH_REFERENCE

* followed by an induction-enabled local binder
	existing INDUCTION_HYPOTHESIS

* followed by a global definition name
	FUNCTION_GRAPH_WITNESS_REFERENCE
```

The interpretation must not depend on whitespace. A bare `@` remains the
Universe form. Parser state continues to distinguish a Match clause label from
a prefix graph reference.

### 7.7 Total and partial definitions

For a `TOTAL` definition, the compiler may generate the total witness:

```text
*f : (x : A) -> @f x (f x)
```

For a `MAY_DIVERGE` definition this total type is invalid: a diverging input has
no returned output and no finite graph inhabitant. The first implementation
should still generate the family:

```text
@f : A -> B -> @
```

where `@f x y` means that a finite execution of `f x` returns `y`, but it should
reject the total prefix witness `*f` unless accepted totality authority exists.

Partial correctness needs no `Returns`:

```text
fPartialCorrect :
	(x : A) -> (y : B) ->
	@f x y ->
	P x y
```

For deterministic normal-return functions, termination at an input may be
defined from graph inhabitation:

```text
TerminatesF x := Sigma (y : B). @f x y
```

Effectful terminal outcomes other than normal return require a later graph
outcome index. They do not by themselves justify retaining the current public
`Returns` name.

### 7.8 The actual remaining case for public `Returns`

The one independent capability supplied by a generic `Returns M v` is
quantification over an arbitrary computation `M` without first assigning it a
definition-specific graph interface. This supports generic formulations such
as:

```text
PartialEnsures M P :=
	(v : A) -> Returns M v -> P v
```

and generic sequence/handler composition laws.

If A Program requires these as public object-language abstractions, retaining a
generic result relation may be justified. If the language instead requires a
computation to be named and graph-published before a post-hoc property is
attached, the same uses can be expressed through generated graphs and public
`Returns` becomes redundant.

Therefore the deletion question is a language-interface decision, not a
logical impossibility:

```text
named-definition properties only
	function graphs are sufficient

polymorphism over arbitrary computations
	a generic result relation is convenient but not more expressive than an
	equally generic execution graph
```

## 8. Termination after removing public `Returns`

Current A Program admits:

```text
#.Terminates (&M)
```

from accepted `Returns` evidence or from a `TOTAL` computation classifier. This
is the implemented v82 contract, not an argument that `Returns` must remain.

This intentionally forgets the return value. In particular:

```text
Terminates (&M)
```

does not supply:

```text
v : A
Returns (&M) v
```

and even such a pair would not automatically supply a function-specific branch
derivation.

Under the graph-first design, the replacement distinction is:

```text
Terminates
	there is accepted termination evidence

FunctionGraph
	there is a source-function-specific derivation to that value
```

For a normal-return-only deterministic function, one graph inhabitant already
identifies the returned value, and a dependent pair of the value with such an
inhabitant proves termination. `TERMINATES_FROM_RETURNS` may therefore be
removed when all of its supported uses have migrated to graph evidence.

`Terminates` may still remain as a generic erased proposition derived directly
from `TOTAL` classifier authority, especially for clients that do not need an
output. It is not a substitute for `@f x y`, and it does not require public
`Returns` to exist.

## 9. The bridge laws A Program should generate

### 9.1 Pure total function bridge

For a pure value-level function, the minimum generated theorem is:

```text
f_graph :
	(x : A) -> FGraph x (f x)
```

For `length`:

```text
length_graph :
	(xs : NatList) ->
	LengthEvaluatesTo xs (length xs)
```

This is enough to transfer any graph theorem to the actual function.

```text
length_graph_property :
	(xs : NatList) -> (n : Nat) ->
	LengthEvaluatesTo xs n ->
	P xs n

length_property :
	(xs : NatList) -> P xs (length xs)

length_property xs :=
	length_graph_property xs (length xs) (length_graph xs)
```

No special functional induction principle is present. The body of
`length_graph_property` uses ordinary IADT elimination on its graph-evidence
argument.

### 9.2 Proposed surface contract

The existing schematic names can be surfaced without adding a namespace full
of generated theorem names:

```text
@f
	FGraph

*f
	f_graph
```

For `length`:

```text
@length : NatList -> Nat -> @

*length :
	(xs : NatList) ->
	@length xs (length xs)
```

For `quickSort`:

```text
@quickSort :
	(A : @) ->
	(A -> A -> Bool) ->
	List A ->
	List A ->
	@

*quickSort :
	(A : @) ->
	(le : A -> A -> Bool) ->
	(xs : List A) ->
	@quickSort A le xs (quickSort A &le xs)
```

These classifiers are schematic until the exact CBPV surface lowering is
specified. In particular, the compiler must not pretend that a possibly
diverging computation has already produced a value-level output merely to form
`*f`.

### 9.3 Why no Graph/Returns soundness bridge is required

If `@f` is generated from the same accepted definition tree as executable `f`,
and `*f` is emitted as an ordinary replayable proof Term, the minimum property
proof already has a checked connection to the actual function:

```text
*f x : @f x (f x)
```

The separate laws:

```text
@f x y -> Returns (&(f x)) y
Returns (&(f x)) y -> @f x y
```

are required only if public `Returns` is deliberately retained as a second
interface. They are not required by function-graph-based post-hoc proofs and
must not be placed in the minimum implementation milestone.

The generator must still be untrusted: it emits the graph family, constructors,
and `*f`, while existing kernel Claims and Derivations check the emitted Terms.
That is the function/graph connection. No `Returns` round trip is needed merely
to trust it.

### 9.4 Determinism is separate

For a deterministic function:

```text
f_graph_deterministic :
	(x : A) -> (left : B) -> (right : B) ->
	FGraph x left ->
	FGraph x right ->
	left == right
```

This proves uniqueness of output. It must not be confused with equality of the
two graph proof terms.

### 9.5 HOTT still distinguishes endpoints from derivations

Even without public `Returns`, there may be more than one inhabitant of
`@f x y`. Endpoint determinism establishes equality of returned outputs; it
does not silently establish equality of graph derivations. A Program must not
assume UIP or proof irrelevance when generating, eliminating, or serializing
the graph IADT.

This caution is consistent with Equations Reloaded's explicit effort to keep
dependent matching compatible with univalence and to generate axiom-free proof
terms where possible.

## 10. QuickSort under this design

### 10.1 Current executable definition

At the audited revision, QuickSort is an ordinary executable function built
from:

- `List`, `SizedList`, and `Measured`;
- a `Partition` IADT carrying lower/upper sizes and decrease evidence;
- ordinary `Acc Nat LT size`;
- `quickSortAcc`, structurally recursive over the `Acc` witness; and
- a public `quickSort` wrapper that measures the input and supplies
  `natAccessible size`.

The source is
`src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p`.

The current theorem:

```a-program
quickSortTerminates := \A : @ => \le : A -> A -> Bool => \xs : List A =>
	#.terminates (&(quickSort A &le xs));
```

is accepted. It proves totality, not sortedness.

### 10.2 Generated graph families

If generation is performed for every named recursive definition, the compiler
should distinguish at least:

```text
QuickSortAccEvaluatesTo
	the recursive worker graph, retaining the two recursive calls

QuickSortEvaluatesTo
	the public wrapper graph, connecting measure/accessibility setup to the
	worker result
```

Trying to generate only a shallow graph for the public wrapper would hide the
recursive structure required by sortedness. Either the public graph must refer
to the worker graph, or generation must provide a controlled inlining policy.

### 10.3 Conceptual recursive constructor

The worker's recursive constructor should retain at least:

```text
quickSortCons :
	(pivot : A) ->
	(tail : SizedList A tailSize) ->
	(lower : SizedList A lowerSize) ->
	(upper : SizedList A upperSize) ->
	PartitionEvaluatesTo pivot tail lower upper ->
	QuickSortAccEvaluatesTo lower sortedLower ->
	QuickSortAccEvaluatesTo upper sortedUpper ->
	QuickSortAccEvaluatesTo
		(cons pivot tail)
		(append sortedLower (cons pivot sortedUpper))
```

Exact binders must be generated from the accepted definition tree, not copied
from this schematic presentation.

### 10.4 Property proof with ordinary IADT elimination

The user writes a theorem over graph evidence:

```text
quickSort_graph_sorted :
	(xs : List A) -> (ys : List A) ->
	QuickSortEvaluatesTo xs ys ->
	Sorted ys
```

Its body eliminates the last graph argument as an ordinary IADT inhabitant.
Recursive graph fields produce the ordinary IADT induction hypotheses.

Then the actual post-hoc theorem is obtained through the generated bridge:

```text
quickSort_sorted :
	(xs : List A) ->
	Sorted (quickSort A le xs)

quickSort_sorted xs :=
	quickSort_graph_sorted
		xs
		(quickSort A le xs)
		(*quickSort A le xs)
```

No computation-result relation, `quickSort.induct`, or `fun_induction`
construct is needed. The only generated proof function used by this theorem is
the canonical `*quickSort` witness.

### 10.5 Graph evidence is not the entire sortedness proof

The graph supplies the correct recursion skeleton and recursive outputs. The
mathematical proof still requires:

- a relation connecting the Boolean comparator to an order Predicate;
- partition preservation of lower/pivot/upper inequalities;
- transitivity and other order laws;
- append/concatenation sortedness lemmas; and
- a separate permutation proof if permutation is desired.

The compiler must not fabricate these properties from the QuickSort type or
graph shape.

## 11. Conceptual unification without retaining public `Returns`

### 11.1 A possible future common semantic core

For future effects and partiality, the internal semantic hierarchy may be:

```text
Run M outcome
	proof-relevant finite execution derivation

Terminates M
	accepted total structure or existence of an accepted terminal Run

@f x y
	a definition-specific finite Run of f x ending in Returned y

PropertyOfF P
	(x : A) -> (y : B) -> @f x y -> P x y

TotalPropertyOfF P
	accepted totality together with PropertyOfF P
```

This is a useful conceptual integration. It does not require storing all of
these as primitive Term tags, and it does not require a public `Returns` view.
If a later feature genuinely needs generic quantification over arbitrary
computations, it may expose an appropriate projection from `Run` then.

### 11.2 What should remain separate

| Concept | Why it remains separate |
|---|---|
| effect row | upper bound on possible operations, not a run or termination proof |
| `TOTAL`/`MAY_DIVERGE` | computation classifier mode, not an output witness |
| compiler-local `UNKNOWN` | solver knowledge state, not an object proposition |
| `Acc` | input-side well-foundedness evidence, not an output derivation |
| function graph | source-function branch derivation, not universal operational semantics |
| `Sorted`/`Permutation` | optional mathematical properties, not execution semantics |
| tactic | proof-term construction procedure, not evidence or a trusted rule |

### 11.3 Partiality requires a later extension

Inductive finite graphs naturally represent terminating derivations. They do
not represent divergence. General recursion must keep A Program's current
`MAY_DIVERGE` boundary and later add an appropriate coinductive or guarded
execution layer.

An inhabitant of `@f x y` is a may-return statement for a particular normal
result. Nondeterminism will require an explicit decision about may- versus
must-properties. `Terminates` cannot be extended to those cases by name alone.

## 12. Tactic boundary

### 12.1 No tactic is required for the first implementation

The user's preferred initial interface is sufficient:

```text
generated graph IADT
+ generated `*f` witness Term
+ ordinary IADT elimination
```

This is theoretically complete for the intended graph-based proofs. The first
implementation should not add:

```text
quickSort.induct
fun_induction quickSort
```

### 12.2 What tactics do in established systems

Rocq's official manual states that `functional induction` is a wrapper around
an ordinary induction application using a generated scheme. Equations'
`funelim` applies its generated functional elimination principle and performs
simplification. Lean's `fun_induction` is likewise a convenience wrapper around
the generated functional induction theorem.

These tools improve proof ergonomics. They do not provide the foundational
bridge by themselves.

### 12.3 Safe future tactic rule

If A Program later adds proof automation, it should:

1. locate an explicit graph evidence term;
2. construct the ordinary IADT Match/elimination term;
3. optionally apply generated equation Claims;
4. emit ordinary Claims and Derivations; and
5. pass accepted replay without the tactic implementation.

The tactic must never:

- assert a property based on a function's simple type;
- synthesize false graph evidence;
- inspect an opaque artifact and guess missing clauses;
- mutate global DefEq;
- turn `::` into synthesis guidance; or
- become a new trusted kernel proof rule.

Lean's equation compiler illustrates the desired trust boundary: it is outside
the trusted kernel, while its generated terms are independently kernel checked.
See [Theorem Proving in Lean 4, Induction and Recursion](https://lean-lang.org/theorem_proving_in_lean4/Induction-and-Recursion/).

## 13. A Program implementation audit

### 13.1 Implemented

At `29131ad`:

- ordinary recursive functions and the `Acc`-driven fuel-free QuickSort compile;
- ordinary indexed families can represent a manually written function graph;
- concrete graph inhabitants compile;
- `#.Returns (&M) v` and `#.Terminates (&M)` are object Terms;
- their accepted evidence uses Claims and Derivations;
- computation-result Context extensions retain the source computation;
- structural/IH/Acc computations carry explicit totality;
- `TOTAL` and `MAY_DIVERGE` are object computation classifier modes;
- `UNKNOWN` remains compiler-local;
- result and totality evidence survive artifact v82 publication and replay; and
- TypedOccurrence records retain Match cases and exact IH ownership data during
  source compilation.

### 13.2 Not implemented

At the same revision:

- no definition-time `LengthEvaluatesTo` or `QuickSortEvaluatesTo` generation;
- no general `length_graph` or `quickSort_graph` bridge;
- no generated graph-to-`Returns` or `Returns`-to-graph maps; the revised
  minimum design does not require them;
- no open QuickSort sortedness theorem;
- no public elimination of intrinsic `Returns` into source-function branches;
- no persisted source splitting/definition tree designed for this purpose; and
- no graph tactic, which is intentionally not required for the first stage.

The repository's current plan explicitly leaves general function-graph
postconditions under Issue #13. See
[Issue #13](https://github.com/repyt-margorp/a-program/issues/13) and
`doc/2026-08-19T23-28-49-OPEN-ISSUES-11-13-14-16-17-18-INTEGRATED-AUTHORITY-IMPLEMENTATION-PLAN.md`.

### 13.3 Current `Returns` representation is not a public evaluation IADT

The active implementation contains intrinsic Term tags for Returns and
Terminates type/witness formers and named proof kinds for:

- Returns type formation;
- finite evaluation evidence;
- computation-result sequence binding;
- Terminates type formation;
- Terminates from Returns; and
- Terminates from a `TOTAL` classifier.

This is sound infrastructure for result endpoints and totality. It should not be
described as an already available, user-eliminable execution trace.

### 13.4 Artifact reconstruction boundary

Artifact v82 restores each typed occurrence with:

```text
source_ast = INVALID_ID
```

and clears additional AST binder identities. Therefore a future importer must
not attempt to reconstruct the exact source-level function graph from only the
serialized Core Match shape.

The graph and bridge must be generated while source definition information is
available, then serialized as ordinary exported proof objects, or the artifact
must carry a stable definition-tree recipe sufficient to regenerate and replay
them.

## 14. Recommended compiler architecture

### 14.1 One authoritative definition tree

Introduce a frontend/elaboration object representing an accepted named
definition before CBPV and Core lowering erase source distinctions:

```text
DefinitionTree
	parameters and result classifier
	source-level splits and branch order
	branch Contexts
	refinement Substitutions
	intermediate computations/bindings
	recursive call occurrences
	IH/decrease authority
	result expression per leaf
	impossible branches
```

Both today's Match syntax and any future equation-clause syntax should elaborate
to this object.

### 14.2 Derived products

```text
surface Match/equation syntax
		|
		v
	DefinitionTree
		|-- executable MATCH/IH Core
		|-- branch equation Claims
		|-- function graph IADT
		|-- canonical `*f` graph-witness Term
		`-- artifact proof-interface roots
```

All outputs must be derived from the same accepted tree. Generating the
executable function from one representation and manually reconstructing its
graph from another would recreate the mismatch the feature is intended to
eliminate.

### 14.3 Kernel boundary

The generator is not trusted to assert correctness. It must emit:

- ordinary type-family declarations and constructors;
- ordinary Terms;
- Context-correct substitutions;
- Claims and ordered-premise Derivations; and
- references to existing Core evaluation and recursive-call authority, without
  requiring a public Returns type.

Accepted replay independently checks those objects. There is no
`QUICKSORT_GRAPH_AXIOM` and no algorithm-specific proof kind.

### 14.4 HOTT boundary

Graph generation must reuse A Program's Context/Substitution branch refinement.
It must not copy Rocq techniques that require UIP/K into A Program's Higher
Observational setting.

In particular:

- retain explicit equality/refinement evidence where needed;
- distinguish endpoint determinism from equality of derivation inhabitants;
- do not quotient graph traces silently;
- do not make graph evidence affect global DefEq; and
- distinguish endpoint equality from equality of graph derivations.

### 14.5 Resource-sensitive future

Generated graph proofs may duplicate references to values and recursive
evidence. A future linear/affine system must assign their usage explicitly.
Generation should reference shared TermDB roots and replayable Context
occurrences rather than copying runtime values. Erasable proof evidence and
runtime values must remain distinguishable.

## 15. Implementation stages

### FG0: Freeze terminology and contracts

- Use `@f` as the surface projection of the canonical function-specific graph
  family for global definition `f`.
- Use global `*f` as the surface projection of its canonical graph-witness
  function when `f` has accepted `TOTAL` authority.
- Preserve local `*field` as the existing Match induction-hypothesis form.
- Call its inhabitants execution derivations or graph witnesses.
- Do not generate `f.induct`, a special eliminator, or a tactic.
- Keep `Terminates`, `Acc`, function graphs, and optional properties distinct.
- Specify whether each generated object is public, opaque, erasable, and
  exported.

### FG1: Retain a checked definition tree

- Record clauses, branch Contexts, refinement substitutions, recursive calls,
  helper calls, and results before source information is erased.
- Make generation deterministic.
- Add source span and generated-name provenance.
- Do not attempt artifact-only reverse engineering.

### FG2: Generate `@length` and `*length`

- Generate `@length` and its two constructors as ordinary IADT data.
- Generate `*length` as an ordinary recursive proof Term with schematic type
  `(xs : NatList) -> @length xs (length xs)`.
- Verify source compilation, artifact publication, readback, and accepted
  replay.
- Make the mail reproducer a permanent positive test.

This is the smallest complete test because the handwritten family and concrete
witnesses already pass while the general bridge currently fails.

### FG3: Prove the graph-only result path before deleting anything

- Migrate the closed `choose` result-property example to `@choose` and
  `*choose` without `#.Returns`.
- Migrate the open `length` theorem to `@length` and `*length`.
- Verify that dependent result indices, Context extensions, Claims,
  Derivations, artifact publication, and replay need no public Returns premise.
- Add negative tests for mismatched functions, values, branches, Contexts, and
  forged graph evidence.
- Measure proof and artifact size against the existing Returns fixtures.

### FG4: Generate helper-aware QuickSort graphs

- Generate separate worker and wrapper graph families, or document a checked
  inlining policy.
- Retain partition results and the two actual recursive calls.
- Hide irrelevant raw `Acc` implementation details from the public wrapper
  graph only through checked bridge terms, not by deleting authority.
- Prove open graph construction before attempting sortedness.

### FG5: Prove optional properties

- Implement `PartitionEvaluatesTo` or equivalent partition specification.
- Prove `quickSort_graph_sorted` by ordinary graph IADT elimination.
- Obtain `quickSort_sorted` through the generated function bridge.
- Prove permutation separately.
- Do not alter or replace the executable QuickSort term.

### FG6: Postpone tactic syntax

- Do not add a special function-induction principle in the initial feature.
- Do not add proof reflection or function-body introspection to the surface
  language.
- Consider tactic sugar only after explicit graph proofs survive artifact
  replay and their repetitive term patterns are measured.

### FG7: Generalize execution semantics later

- Introduce a proof-relevant `Run M outcome` only with a specified effect and
  partiality semantics.
- Derive termination views, traces, and definition-specific graphs without
  assuming that a public `Returns` projection is required.
- Add coinductive divergence separately from finite inductive runs.
- Specify may/must behavior before admitting nondeterminism.

### FG8: Remove public `Returns` after graph parity

Do not delete the implemented relation before graph-based replacements pass.
Once FG2 through FG5 provide parity:

- remove the surface forms `#.Returns` and `#.returns`;
- remove their dedicated AST type-expression and witness forms;
- remove public Returns type/witness Term tags and proof kinds;
- remove `TERMINATES_FROM_RETURNS`, replacing its valid uses with direct
  `TOTAL` authority or existence of graph evidence;
- migrate computation-result Context origins and dependent sequencing fixtures
  to graph-evidence origins;
- migrate `choose` and result-evidence tests to generated graphs;
- publish a new artifact schema without Returns roots and reject obsolete
  schemas according to the repository's no-compatibility-facade policy; and
- retain only the minimum internal operational judgement needed to validate
  generated graph constructors and witnesses.

This is a conditional removal plan. Failure to express anonymous computation
properties, sequencing, or handler composition without a generic result
interface is evidence to retain or redesign that capability, not permission to
forge a graph witness.

## 16. Verification matrix

### 16.1 Positive tests

- handwritten `LengthEvaluatesTo`, `emptyRun`, and `oneRun` continue to pass;
- generated `@length` has the same accepted constructor classifiers;
- generated `*length` passes for an open `xs`;
- a property proved by ordinary elimination over `LengthEvaluatesTo` transfers
  to `length xs` through `*length`;
- the closed `choose` property compiles through `@choose` and `*choose` with no
  Returns premise;
- QuickSort graph exposes two recursive graph premises in the partition branch;
- open QuickSort sortedness compiles without a second executable QuickSort;
- all generated objects survive v82-or-successor artifact publication,
  relocation, linking, and accepted replay; and
- generation is byte deterministic.

### 16.2 Negative tests

- a graph constructor with the wrong output index is rejected;
- `*f` referring to a graph owned by a different definition is rejected;
- a recursive graph premise not corresponding to an accepted recursive call is
  rejected;
- a wrong branch refinement Substitution is rejected;
- an unrelated `Acc`/decrease witness is rejected;
- global `*f` is rejected for a `MAY_DIVERGE` definition without accepted
  totality authority;
- `@g` and `*g` are rejected for a local higher-order variable without an
  explicitly supplied graph interface;
- a graph witness cannot be forged for an arbitrary endpoint;
- an opaque imported function without exported graph data is not reverse
  engineered;
- normalization exhaustion creates no graph witness or negative theorem;
- empty effects do not imply termination; and
- no tactic result is accepted without ordinary replayable evidence.

### 16.3 Performance and size tests

- measure graph family and `*f` Term/Claim/Derivation growth per function;
- measure QuickSort compile time independently from totality proof cost;
- ensure generation reuses existing TermDB roots;
- measure artifact growth with graph export enabled and disabled; and
- ensure no Context/Substitution index rebuild regression;
- compare the graph-only artifact against the current Returns-based fixtures;
  and
- require removal of Returns tags and rules to reduce or justify any net
  artifact and replay cost.

## 17. Decisions and non-decisions

### Decided by this research

1. The missing `lengthCorrect`-like object is a theorem/term, not a new
   Predicate category.
2. For a named total function, `@f` plus `*f` is sufficient for post-definition
   properties; `Returns` is absent from the proof path.
3. Public `Returns` is a convenience for abstraction over arbitrary
   computations, not a logical prerequisite for function properties.
4. A function-specific graph is justified as a generated proof-shape view, not
   as a kernel primitive.
5. `@f` denotes the canonical generated graph IADT; global `*f` denotes its
   canonical witness function for accepted total definitions.
6. The checked `*f` Term itself is the minimum function-to-graph bridge; no
   Graph/Returns round trip is required when Returns is removed.
7. Ordinary IADT elimination is enough for the first A Program interface.
8. Tactics are optional and should be postponed.
9. Generation must occur from retained definition structure during source
   compilation.
10. HOTT forbids silently identifying evidence types through proof irrelevance.
11. Removing the implemented Returns surface and artifact vocabulary is
    justified only after graph-only parity tests pass.

### Still requiring language design

1. whether `@f`/`*f` is accepted as the final spelling after parser prototypes;
2. whether graph generation is default or explicitly requested;
3. export/opacity policy for graph families and witness terms;
4. exact equation/Identity surface notation;
5. helper-call inlining versus separate helper graphs;
6. erasure and resource-usage policy for graph evidence;
7. whether anonymous computation properties require naming, local generated
   graph identities, or a generic computation interface;
8. the treatment of `*f` for `MAY_DIVERGE` definitions;
9. the future universal `Run` outcome vocabulary; and
10. may/must/coinductive semantics after nondeterminism and general recursion.

None of these undecided surface choices prevents implementing the internal
definition tree and the `length` proof-of-concept first.

## 18. Final recommendation

A Program should prototype a graph-first interface and plan removal of public
`Returns`, rather than assuming that both relations are permanently required.

Use:

```text
@f
	canonical generated proof-relevant graph IADT for global definition f

*f
	canonical generated witness function when f has accepted TOTAL authority

ordinary IADT elimination
	for implementing later properties

Terminates / TOTAL / MAY_DIVERGE
	for the independent totality boundary

optional tactics
	not in the initial design
```

The first implementation target should be the mail's `length` example. It
already isolates the exact boundary: the function, graph IADT, and concrete
graph inhabitants work, while the open function-to-graph witness fails. Expose
that generated family as `@length` and the generated witness as `*length`.

Next migrate the closed `choose` result property and the current `Acc`
QuickSort property path to `@f`/`*f`. Only after source, artifact, replay,
negative, and performance parity is demonstrated should the implementation
remove `#.Returns`, `#.returns`, their object Terms and proof rules,
`TERMINATES_FROM_RETURNS`, and their artifact roots.

If this migration reveals a required public abstraction over arbitrary unnamed
computations, A Program should specify that capability directly. It may retain
a generic result relation or require a generated graph identity, but it should
not keep `Returns` solely because it was implemented before the function-graph
surface was designed.

## 19. Primary sources

Accessed 2026-08-22.

- [Rocq Equations official overview](https://rocq-prover.org/docs/equations-docs)
- [Sozeau and Mangin, *Equations Reloaded*](https://sozeau.gitlabpages.inria.fr/www/research/publications/Equations_Reloaded-ICFP19.pdf)
- [Rocq 9.2 Reference Manual, Functional induction](https://rocq-prover.org/doc/V9.2.0/refman/using/libraries/funind.html)
- [Equations official basic tutorial](https://rocq-prover.github.io/platform-docs/equations/tutorial_basics.html)
- [Lean 4 implementation, `Lean.Meta.Tactic.FunInd`](https://github.com/leanprover/lean4/blob/master/src/Lean/Meta/Tactic/FunInd.lean)
- [Lean 4 Tactic Reference, `fun_induction`](https://lean-lang.org/doc/reference/latest/Tactic-Proofs/Tactic-Reference/#tactic-fun_induction)
- [Theorem Proving in Lean 4, Induction and Recursion](https://lean-lang.org/theorem_proving_in_lean4/Induction-and-Recursion/)
- [Isabelle/HOL, *Theory Functions*](https://www.isabelle.in.tum.de/library/Doc/Functions/Functions.html)
- [Bove and Capretta, *Modelling General Recursion in Type Theory*](https://www.cambridge.org/core/journals/mathematical-structures-in-computer-science/article/abs/modelling-general-recursion-in-type-theory/B02BBE1A0A38C44189D53B01D659ECE5)
- [Leroy and Grall, *Coinductive Big-Step Operational Semantics*](https://cgi.cse.unsw.edu.au/~rvg/SpecialIssueSOS/21/leroy-grall-coindsem.pdf)
- [Ahman et al., *Dijkstra Monads for Free*](https://fstar-lang.org/papers/dm4free/)
- [Xia et al., *Interaction Trees: Representing Recursive and Impure Programs in Coq*](https://www.pure.ed.ac.uk/ws/portalfiles/portal/286180994/Interaction_Trees_XIA_DOA14102019_VOR_CC_BY.pdf)

## 20. Local evidence

- `src/prototype/README.md`
- `src/prototype/include/a_program/core/term.h`
- `src/prototype/include/a_program/kernel/context.h`
- `src/prototype/include/a_program/kernel/judgement/types.h`
- `src/prototype/src/frontend/reader.c`
- `src/prototype/src/kernel/rules/introduction/result_evidence.inc`
- `src/prototype/src/artifact/wire_v82.c`
- `src/prototype/tests/fixtures/typing/result_evidence_dependent_check.p`
- `src/prototype/tests/fixtures/typing/result_evidence_sequence_binding_check.p`
- `src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p`
- `doc/2026-08-19T23-28-49-OPEN-ISSUES-11-13-14-16-17-18-INTEGRATED-AUTHORITY-IMPLEMENTATION-PLAN.md`
- [A Program Issue #13](https://github.com/repyt-margorp/a-program/issues/13)
- [audited revision `29131ad`](https://github.com/repyt-margorp/a-program/commit/29131adfff09e9d6c6d4bcc888acf450baf36bde)

## 21. Commands executed

```sh
git fetch origin main
git merge --ff-only origin/main
make -f src/prototype/Makefile reader
./read_file.out /tmp/a_program_length_graph_concrete_check.p
./read_file.out /tmp/a_program_length_graph_bridge_check.p
sh src/prototype/tests/run_integration_suite.sh --test-name test_result_evidence
sh src/prototype/tests/run_integration_suite.sh --test-name test_if8_fuel_free_quicksort
```

The concrete graph check passed. The open bridge check reproduced `P0 IH
replay failed` and was intentionally recorded as a current negative result, not
as a successful feature test. Both focused integration tests passed at the
audited revision, including source publication, artifact readback, artifact
equality, and deterministic regeneration for the current fuel-free QuickSort
fixture.
