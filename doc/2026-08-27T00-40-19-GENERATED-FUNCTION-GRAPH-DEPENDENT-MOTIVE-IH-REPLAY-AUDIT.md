# Generated Function Graph Dependent-Motive and IH Replay Audit

## Status

This document records a compiler audit, not an implementation plan and not an
implementation patch.

- Repository: `repyt-margorp/a-program`
- Primary audited commit: `fd546e13784c0d7c59f984beeacdac199ddab321`
- Primary commit subject: `Implement PR22 checked-core effort compilation`
- Earlier comparison commit: `4fe5aee`
- Related Bug: [#23](https://github.com/repyt-margorp/a-program/issues/23)
- Earlier, broader result-boundary Bug: [#13](https://github.com/repyt-margorp/a-program/issues/13)
- Audit date: 2026-08-27

The current compiler can generate a Function Graph for a recursive function,
can expose the graph through `@function`, and can expose the execution witness
through `*function`. It can also construct induction-hypothesis fields while a
generated graph is eliminated.

One important combination nevertheless fails:

1. the proof result depends on the generated graph output;
2. a recursive constructor branch consumes the generated IH;
3. the accepted P0 derivation is replayed.

The immediate example is the theorem that the output of `length` has its own
unary shape. The same compiler boundary is needed before an already-defined
QuickSort can receive a post-hoc `Sorted output` theorem.

The failure is not a limitation of indexed families in general. An equivalent
hand-written indexed execution graph and dependent eliminator pass. It is a
representation and validation Bug in the generated Function Graph path.

## Executive conclusion

The lowering solver constructs the wrong motive.

The intended motive is:

```text
M input output graph = Comp({}, Unary output, TOTAL)
```

The stored motive observed during the audit is equivalent to:

```text
M input output graph = Comp({}, Unary Nat.zero, TOTAL)
```

The three binders exist, but none occurs in the body. The motive has been made
syntactically higher arity without becoming semantically dependent.

This happens because the provisional-motive seed uses exact physical term
replacement. In a generated Function Graph, the constructor output index and
the occurrence of that index in the branch classifier have distinct TypeView
representations. They are semantically corresponding terms but do not have the
same term id. Replacement therefore changes nothing.

The recursive cons branch initially exposes the bad motive:

```text
expected from stored M: Unary Nat.zero
actual generated IH:     Unary tailLength
```

A guarded-IH fallback then accepts that mismatch because the IH has valid
ownership. Ownership establishes that the IH came from the correct guarded
recursive occurrence. It does not establish equality between these two
classifiers.

Accepted P0 replay later performs the missing classifier check and rejects the
artifact. Replay is behaving correctly. The correction belongs in motive
construction and compiler-side branch validation, not in weaker replay.

## Why this audit matters

Function Graphs were introduced so that a property can be proved after a
function is defined, without reverse-compiling an opaque function and without
duplicating the executable algorithm as a proof-specialized implementation.

For `length`, the desired boundary is:

```text
input  : NatList
output : Nat
graph  : @length input output
---------------------------------
property : Unary output
```

For QuickSort, the corresponding boundary is:

```text
input  : NatList
output : NatList
graph  : @quickSort input output
---------------------------------
property : Sorted output
```

The mathematical property differs, but the compiler obligation is the same:
the eliminator motive must remain indexed by the output recorded in every graph
constructor, including recursive outputs.

If only constant motives work, Function Graphs can fold an execution tree but
cannot yet express the main post-hoc result properties for which the graph
interface was designed.

## Boundaries of the finding

This audit does **not** conclude any of the following:

- general IADT elimination is broken;
- all generated Function Graph elimination is broken;
- recursive IH materialization is always broken;
- multiple recursive fields are unsupported;
- the accepted replay checker should trust the compiler;
- PR22 effort compilation caused this Bug;
- a new `Returns` proposition is required.

The controls later in this document rule out those broader conclusions.

## Current surface reproducer

The exact Book example used in the audit is:

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

Unary := @\value : Nat => {
	zero : * Nat.zero;
	succ : (predecessor : Nat) -> * predecessor ->
		* (Nat.succ predecessor);
};

lengthOutputUnary := \input : NatList => \output : Nat =>
	\graph : @length input output =>
		graph
			@nil => Unary.zero
			@cons head tail tailLength tailGraph =>
				Unary.succ tailLength *tailGraph;
```

This example is intentionally small.

- `length` is defined before its property.
- `@length input output` is the generated Function Graph type family.
- `graph` is evidence that the existing `length` execution relates `input` to
  `output`.
- Matching `graph` exposes graph-constructor fields.
- In the cons case, `tailLength` is the recursive output.
- `tailGraph` is the graph evidence for the recursive call.
- `*tailGraph` selects the induction hypothesis generated for that recursive
  graph field.
- Its expected type is `Unary tailLength`.
- The result of `Unary.succ tailLength *tailGraph` is
  `Unary (Nat.succ tailLength)`, which is exactly the output index of the cons
  graph constructor.

No tactic is required. The proof is ordinary elimination of an indexed family,
with one generated IH selector.

The surrounding Book file also contains a nonrecursive output-shape control and
an execution-witness match. Those controls are useful, but they are not needed
to understand the minimal failing intersection.

## Actual failure

Running the Book verification reaches accepted replay and reports:

```text
P0 IH replay failed derivation=166 claim=166 occurrence=103
subject=388 classifier=692 premises=0
P0 accepted proof graph validation failed
```

The later source-level message is too generic and is associated with a `0:0`
span. It does not identify:

- `lengthOutputUnary`;
- the generated cons graph constructor;
- `tailGraph` or its IH selector;
- the expected `Unary Nat.zero` classifier;
- the actual `Unary tailLength` classifier;
- the provisional motive that lost output dependency.

This diagnostic weakness is secondary to the semantic Bug, but it makes the
failure unnecessarily difficult to locate.

## Verification matrix

The following matrix was run against the current implementation.

| Case | Graph origin | Motive depends on output | Uses recursive IH | Result |
|---|---|---:|---:|---:|
| Official named `length` fixture | generated | no | yes | pass |
| Output-shape probe | generated | yes | no | pass |
| Manual `LengthGraph` and `Unary` proof | hand-written IADT | yes | yes | pass |
| `lengthOutputUnary` | generated | yes | yes | fail |
| Official QuickSort dependency closure | generated | no | two IH fields | pass |

The matrix is more informative than the single failure.

### Generated graph plus constant motive passes

The official named-case fixture exercises a recursive generated graph and IH
production, but its result classifier is constant with respect to the graph
output. It therefore does not require successful output-index abstraction.

### Generated graph plus output dependency without IH passes

A property that records the outer output shape can match the generated graph
and return an output-indexed value without consuming the recursive IH. This
shows that generated output indices are not uniformly unusable.

### Hand-written indexed graph plus dependent IH passes

An ordinary indexed family with nil and cons execution constructors can express
the same input/output relation. Eliminating it with motive `Unary output` and
using the recursive IH succeeds.

This control is decisive: the type theory, parser surface, and general IADT
eliminator have enough expressivity for the desired theorem.

### Generated graph plus dependent IH fails

This is the unique failing combination. The generated graph representation is
therefore part of the cause.

### QuickSort with two IH fields passes only a constant-motive test

The official integration suite checks dependency closure over both recursive
QuickSort calls. That is valuable coverage for IH ownership and graph closure.
Its fold result is nevertheless a constant `List Nat` classifier. It does not
test `P output`, such as `Sorted output`.

## Mathematical account

Suppose `LengthGraph` has the conceptual constructors:

```text
graphNil :
  LengthGraph NatList.nil Nat.zero

graphCons :
  (head       : Nat) ->
  (tail       : NatList) ->
  (tailLength : Nat) ->
  LengthGraph tail tailLength ->
  LengthGraph
    (NatList.cons head tail)
    (Nat.succ tailLength)
```

The property motive is:

```text
M :
  (input : NatList) ->
  (output : Nat) ->
  LengthGraph input output ->
  Type

M input output graph := Unary output
```

The eliminator supplies the recursive hypothesis:

```text
tailIH : M tail tailLength tailGraph
```

By definition of `M`, that is:

```text
tailIH : Unary tailLength
```

The cons proof is then ordinary constructor application:

```text
Unary.succ tailLength tailIH
  : Unary (Nat.succ tailLength)
```

There is no need for a special QuickSort theorem mechanism, proof reflection,
or a `Returns` relation to establish this small example. A correctly generated
IADT plus its ordinary eliminator is sufficient.

## A Program representation account

The mathematical presentation uses one apparent index `output`. The compiler
has several physical representations involved in constructing and validating
that index:

1. the generated graph declaration contains constructor indices;
2. source and core TypeViews preserve the generated declaration boundary;
3. the branch classifier contains the result family applied to a projected
   representation of the output;
4. the solver creates fresh motive binders;
5. the solver tries to replace constructor-index terms with those binders;
6. accepted replay later reinstantiates the stored motive independently.

These objects may denote the same mathematical index without sharing the same
term id. Any algorithm that abstracts indices by physical id must either prove
that the representations have already been canonicalized or explicitly cross
the TypeView boundary.

The current seed path assumes more physical identity than generated Function
Graphs provide.

## Compiler construction path

### 1. The function is lowered

The `length` match and its recursive call are represented in the ordinary term
and operation structures.

### 2. Function Graph metadata is generated

The compiler owns the generated graph type family, its constructors, recursive
graph fields, and selector metadata under the function owner. No additional
user global name is required.

### 3. The post-hoc proof matches graph evidence

Lowering encounters the match on:

```text
graph : @length input output
```

The match needs an indexed motive because its result classifier varies with the
graph output.

### 4. A saturated branch provisionally seeds the motive

`operation_solver_seed_indexed_motive_from_branch` in
`src/prototype/src/frontend/lowering/graph_construction.inc` reconstructs a
saturated constructor occurrence and its classifier.

It then:

1. allocates one fresh binder for every IADT index;
2. calls `prototype_term_graph_replace_exact` to replace each constructor index
   in the provisional branch classifier;
3. allocates a binder for the matched value;
4. performs another exact replacement for the constructor term;
5. wraps the resulting classifier in lambdas;
6. reapplies the candidate motive to the same constructor occurrence;
7. accepts it when conversion reproduces the same branch classifier.

The algorithm is reasonable only if replacement actually abstracts every
dependency that the candidate is supposed to represent.

### 5. The generated output index is not physically found

Temporary instrumentation showed the generated nil graph branch using two
representations:

```text
generated constructor index:
  CONSTRUCTOR(TYPE_VIEW(Nat, ...))

classifier occurrence:
  projected TypeView core/source containing direct Nat.zero constructor
```

For the generated graph output index, exact replacement reported no change.

For the equivalent hand-written `LengthGraph`, the output `Nat.zero` term was
physically found in the classifier and was replaced with the fresh output
variable.

This difference explains the control matrix without appealing to a general
failure of dependent elimination.

### 6. Same-branch reinstantiation cannot detect the lost dependency

After failed replacement, the candidate is constant in `output`:

```text
M input output graph = Unary Nat.zero
```

The seed routine reapplies it only to the nil branch, whose output is itself
`Nat.zero`:

```text
M NatList.nil Nat.zero graphNil = Unary Nat.zero
```

That check succeeds. It proves that the candidate explains the seed branch. It
does not prove that the candidate explains the other constructors.

This is a familiar underdetermination problem. From one observation at
`output = Nat.zero`, both of these functions fit:

```text
M1 input output graph = Unary output
M2 input output graph = Unary Nat.zero
```

The recursive constructor is what distinguishes them.

### 7. The cons branch produces the right IH relation

The generated IH edge itself is indexed correctly:

```text
Unary tailLength
```

This is evidence that recursive output metadata and IH relation construction
are not wholly broken.

### 8. Guarded fallback accepts the wrong comparison

Branch validation in
`src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc`
first attempts ordinary conversion.

For the bad candidate it compares:

```text
M tail tailLength tailGraph = Unary Nat.zero
```

with:

```text
actual IH relation = Unary tailLength
```

Conversion fails, as it should.

The fallback then finds a unique IH owned by the match operation and validates
that it is a guarded recursive occurrence. It changes the conversion status to
equal.

The implication is invalid:

```text
valid guarded ownership
  does not imply
candidate motive instance == typed IH classifier
```

Guardedness answers whether recursive evidence may be used. Motive equality
answers what proposition that evidence proves. These are separate judgements.

### 9. Accepted replay rejects the malformed proof

`src/prototype/src/kernel/typing/accepted_replay.inc` validates the stored P0
derivation. Its IH-elimination check reconstructs the expected classifier from
the stored motive and compares it with the IH relation.

It correctly observes the mismatch and stops before the artifact can be
treated as accepted.

## Three authority layers

The failure is easier to understand when the compiler stages are kept
separate.

### Producer

Frontend lowering and its solver propose:

- a motive;
- branch classifiers;
- IH ownership edges;
- a proof derivation.

The producer currently makes both identified mistakes: a vacuous index
abstraction and a guardedness/equality conflation.

### Accepted replay authority

Accepted replay checks the claimed derivation against stored semantic data. It
does not reconstruct the compiler heuristic. It rejects the malformed IH
elimination.

This is the desirable trust boundary.

### From-scratch checked-core authority

PR22 adds the independent checked-core path for admitted artifacts. The current
failure happens earlier, so this last authority is not reached.

The Bug therefore does not demonstrate a checked-core failure. It demonstrates
that the compiler producer is unable to produce a replayable proof for a valid
program.

## Why this is not a PR22 regression

The exact failure was reproduced at commit `4fe5aee`, before the PR22
checked-core effort-compilation implementation, as well as at current commit
`fd546e1`.

PR22 makes the final artifact authority more explicit, but it did not create
the malformed motive. The earlier Function Graph lowering path already had the
same behavior.

## Relation to Issue 13

Issue #13 correctly identified that an existing computation needed a sound
surface and semantic bridge to optional dependent Claims. At that time,
`Returns` or dependent sequencing were discussed schematically.

The current architecture has since selected Function Graphs as the principal
bridge:

```text
@f      generated execution relation
*f      witness that an execution of f returned
graph   eliminable evidence connecting inputs and output
```

Consequently, the current Bug is narrower than #13. The relation exists, its
evidence exists, and simple consumers work. What fails is the construction of a
dependent motive over a recursive generated graph.

Adding a second `Returns` relation would not repair this defect. It would add a
parallel representation of a boundary the Function Graph already models.

## Relation to QuickSort

### What already works

The official integration test can:

- generate the QuickSort Function Graph;
- retain both recursive graph dependencies;
- expose both recursive IH fields;
- validate a constant-motive dependency closure.

This is real and useful progress. The graph is not merely unary, and recursive
ownership is represented.

### What remains blocked

A sortedness proof needs a motive shaped like:

```text
M input output graph = Sorted output
```

The lower recursive graph must provide:

```text
lowerIH : Sorted lowerOutput
```

and the upper recursive graph must provide:

```text
upperIH : Sorted upperOutput
```

The current Bug would seed a property specialized to the first constructor
output and then fail or misvalidate later recursive branches. Passing a
constant `List Nat` fold does not establish this capability.

After this compiler Bug is fixed, a complete QuickSort proof still needs
ordinary library mathematics:

- a proposition defining ordered lists;
- a permutation or membership-preservation proposition if desired;
- comparator reflection or proof-producing comparison;
- a partition theorem retaining lower/pivot/upper ordering facts;
- a theorem combining two sorted partitions around the pivot.

Those are theorem inputs, not further evidence that Function Graph lowering is
broken. The present issue concerns only the ability to carry `P output` through
the generated recursive execution structure.

## Correction requirements

This audit deliberately does not prescribe one physical implementation. It
does prescribe semantic constraints that any correction must satisfy.

### Requirement 1: abstraction must cross generated TypeViews correctly

The solver must recognize the constructor output index in the branch
classifier even when generated source/core TypeViews give it a different
physical representation.

Possible implementation families include:

- canonicalizing both sides before abstraction;
- a TypeView-aware structural abstraction operation;
- reconstructing the result-family application directly from the generated
  declaration and branch fields;
- retaining an explicit mapping from generated constructor indices to branch
  occurrences during graph generation.

Whatever approach is selected must use an existing semantic authority or
certified mapping. It must not become a broad heuristic that equates unrelated
terms merely because normalization happens to resemble them.

### Requirement 2: dependency success must be observable

The seed routine currently allocates binders even when replacement changes no
occurrence. It should distinguish:

- a binder deliberately unused because the motive is genuinely constant;
- a binder expected to abstract an index but missed due to representation;
- a binder whose dependency can only be established after inspecting more
  branches.

A changed flag alone may not be the final interface, but silent vacuity is not
sufficient.

### Requirement 3: constant motives must remain legal

Not every eliminator depends on every index. A blanket rule requiring all
indices to occur would reject valid programs and the official constant-motive
controls.

The solver may need:

- validation against all available constructor equations;
- a distinction between provisional and final motive solutions;
- a generated declaration mapping that determines dependency structurally;
- or another principled way to resolve the underdetermination.

### Requirement 4: guardedness and classifier equality remain separate

The IH owner check should continue to reject forged, foreign, or unguarded
recursive evidence. It must not overwrite a failed classifier conversion.

If recursive motive equations genuinely require a special conversion rule,
that rule must construct or cite the equation being used and be replayable. A
typed ownership edge alone is not that equation.

### Requirement 5: accepted replay must not be weakened

The accepted replay failure is the strongest evidence in this audit that the
authority boundary is functioning. The correction must make the compiler
produce a derivation that the existing-strength replay can verify.

### Requirement 6: no special cases for the example

The correction must apply to arbitrary result families `P output` and arbitrary
generated Function Graph outputs. Special cases for `Nat.zero`, `Unary`,
`length`, `Sorted`, or QuickSort would conceal the representation error.

### Requirement 7: preserve function-owned metadata

The solution should not create new global constructor or selector names.
Generated graph internals remain local selector metadata owned by the function,
consistent with the current Function Graph surface design.

## Required permanent regression tests

The implementation should retain the full control matrix, not only the newly
passing reproducer.

### Positive test A: generated length, dependent motive, recursive IH

The exact `lengthOutputUnary` proof in this document must compile.

It must pass:

- frontend proof production;
- accepted P0 replay;
- serialization/deserialization;
- from-scratch checked-core validation.

### Positive test B: generated graph, dependent motive, no IH

This protects the already-working nonrecursive output-dependency path.

### Positive test C: generated graph, constant motive, IH

This protects legal constant motives and prevents an overcorrection that
requires every index binder to occur.

### Positive test D: manual IADT, dependent motive, IH

This preserves the general indexed-elimination baseline.

### Positive test E: QuickSort, two output-dependent IHs

Before requiring the entire sortedness library, a smaller structural property
indexed by the produced list can test both recursive outputs. It must differ
from the current constant `List Nat` dependency-closure test.

### Negative test A: genuinely incompatible recursive property

Valid IH ownership must not discharge a real classifier mismatch.

### Negative test B: foreign IH ownership

An IH from another operation or recursive edge must be rejected.

### Negative test C: unrelated TypeView structure

Semantic abstraction must not conflate unrelated constructors that have
similar wrapper shapes.

### Negative test D: vacuous provisional abstraction

A candidate that only reproduces the seed branch but contradicts another
constructor must not become a READY motive.

## Diagnostics requirements

A future failure in this area should report at least:

- source function name;
- generated graph owner;
- graph constructor or local selector;
- motive index position;
- whether index abstraction found an occurrence;
- expected branch or IH classifier;
- actual branch or IH classifier;
- whether guarded ownership passed independently;
- the source span of the graph match or IH selector.

Internal numeric ids are useful supplementary data but not an adequate primary
diagnostic.

## Non-fixes

The following changes would reduce evidence or duplicate concepts without
solving the cause:

- weakening accepted P0 replay;
- skipping IH classifier validation when ownership is valid;
- forcing every generated graph fold to use a constant motive;
- hard-coding `length`, `Unary`, or `Nat.zero`;
- duplicating the executable algorithm inside its correctness proof;
- generating global names for every graph constructor or recursive field;
- adding `QuickSort.induct` as a tactic-like primitive before ordinary graph
  elimination is correct;
- reintroducing a parallel, user-forgeable `Returns` proposition;
- treating `::` as motive synthesis input rather than an expectation check.

## Reproduction and audit hygiene

The audit used temporary probe sources and temporary diagnostic printouts to
compare term representations and stored motives. Those probes and printouts
were removed after collecting the evidence.

The primary checkout was restored to a clean tracked state before this document
was added. The earlier-commit comparison used a separate temporary worktree,
which was also removed.

The document therefore records observations rather than depending on a patched
diagnostic compiler.

## Final current-baseline verification

After restoring the unmodified compiler source, the official focused
integration suite was run from the repository root:

```sh
sh src/prototype/tests/integration/test_function_graph_certified_execution.sh
```

All of its phases passed, including:

```text
generated_graph
two_recursive_calls
dependent_argument_spine
block_bound_recursive_result
named_case_roles
quicksort_dependency_closure
artifact_association
imported_association
cbpv_negative
```

This establishes that the existing official Function Graph controls remain
green on `fd546e1`.

The exact Book reproducer was then run directly:

```sh
./a.out \
  ../code/14_function_graph_surface_boundary/posthoc_length_property.p
```

It failed as expected with:

```text
P0 IH replay failed derivation=166 claim=166 occurrence=103
subject=388 classifier=692 premises=0
P0 accepted proof graph validation failed
../code/14_function_graph_surface_boundary/posthoc_length_property.p:0:0:
failed to generate accepted function graph
```

The combined observation is the audit result: the official constant-motive and
structural controls pass, while the output-dependent recursive property fails
at accepted replay.

## Final assessment

A Program already has the right high-level decomposition for post-hoc
properties:

```text
ordinary function
    -> generated Function Graph
    -> ordinary IADT elimination
    -> generated recursive IH evidence
    -> separately defined property proof
```

The current failure does not call for replacing that model. It identifies two
local but semantically important implementation errors:

1. generated output indices are abstracted by physical exact replacement even
   though TypeView representation prevents physical identity;
2. guarded IH ownership is allowed to stand in for classifier equality.

Fixing those errors, while retaining independent replay authority, should make
the small `lengthOutputUnary` theorem possible and establish the compiler
foundation needed for later `Sorted (quickSort input)` proofs.
