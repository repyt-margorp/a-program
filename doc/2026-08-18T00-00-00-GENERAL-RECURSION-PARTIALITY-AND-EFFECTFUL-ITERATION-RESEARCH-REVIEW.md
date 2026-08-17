# General Recursion, Partiality, and Effectful Iteration Research Review

Date: 2026-08-18

## 1. Status and scope

This document is a research review, not an accepted implementation plan.

It records:

- the implementation boundary observed at commit
  `de2b44241dce30dd1c5549b14645c91eda02da56`;
- the main semantic accounts of general recursion relevant to A Program;
- the distinction between total recursion, partial computation, and productive
  infinite behavior;
- the interaction between recursion, CBPV, algebraic effects, handlers,
  dependent types, observational identity, and resource usage;
- design choices that must be made before implementation starts; and
- a staged recommendation and review checklist.

This review intentionally does not add surface syntax, Core tags, typing rules,
runtime behavior, proof rules, or an artifact version. Any such change requires
a separate accepted implementation plan.

The immediate design question is not merely whether A Program should have a
fixed-point operator. The question is which solutions of recursive equations
are observable, how those solutions compose with effects and handlers, and how
potentially diverging computation remains separated from accepted logical
evidence.

## 2. Executive conclusion

A Program should not model general recursion as a cycle in the global
definition namespace. In particular, a source equation such as:

```text
loop := loop;
```

does not provide an adequate semantic account. It confuses recursive execution
with recursive name resolution and gives neither a local recursive binder nor a
specified solution of the resulting equation.

The most defensible direction is:

1. retain structural Match induction and library-defined `Acc` as the total
   fragment;
2. add a separate computation-only partiality boundary for unguarded general
   recursion;
3. specify effectful procedural iteration using an iteration operator of shape
   `f : X -> T(Y + X)` to `f^dagger : X -> T Y`, with at least the complete
   Elgot laws as the semantic comparison point;
4. represent a recursive program by a finite, locally scoped recursive equation,
   not by a cyclic definition-name graph or an eagerly cyclic TermDB pointer;
5. distinguish silent divergence from productive or observably effectful
   infinite execution before defining observational identity for recursive
   computations;
6. require handlers and artifact transformations that claim to preserve
   recursion to preserve the chosen iteration operator;
7. prevent partial computation from entering definitional equality, type
   indices, or proof authority merely by being assigned a result type; and
8. add post-hoc partial-correctness and termination judgements only after their
   operational quantification and admissible proof principles are specified.

A single local recursive-equation representation may underlie several source
constructs, but it must have three different semantic dispositions:

```text
local recursive equation
        |
        +-- decreasing or Acc-certified ------> total computation
        |
        +-- observation-guarded --------------> productive process
        |
        `-- otherwise ------------------------> partial computation
```

These dispositions must not be collapsed into one logical rule. They validate
different reasoning principles.

## 3. Terminology

This review uses the following terms deliberately.

### 3.1 General recursion

General recursion permits recursive calls without a syntactically evident
structural decrease or an accepted well-foundedness witness. It may diverge.

### 3.2 Structural and well-founded recursion

Structural recursion follows an inductive subobject supplied by an eliminator.
Well-founded recursion follows an accessibility proof, commonly represented by
`Acc`. Both are total when their typing derivation is accepted.

### 3.3 Partial computation

A partial computation of result type `A` may return an `A`, may perform effects,
or may never return. A partial computation of `False` is not a proof of
`False`.

### 3.4 Productive infinite computation

A productive infinite computation need not return, but it produces a finite
observable prefix after finitely many steps and continues to do so. A stream or
an infinite sequence of output requests is different from a silent loop when
the observation model records those requests.

### 3.5 Iteration

For a computation constructor or monad `T`, an iteration operator solves an
effectful loop body:

```text
f         : X -> T(Y + X)
f^dagger  : X -> T Y
```

`Y` is the exit result and `X` is the next loop state. This form makes exit and
continuation explicit and is more informative for procedural loops than the
bare equation `fix F = F(fix F)`.

### 3.6 Fixed point and selected solution

The equation `x = F(x)` can have no solution, one solution, or many solutions.
A fixed-point operator is not specified by the unfolding equation alone. A
model must state how one solution is selected, for example the least fixed point
of a continuous function on a pointed domain.

### 3.7 Runtime world, Context, and Universe

A runtime configuration may include a current computation, value environment,
handler stack, store, and scheduler state. A transition between such
configurations can be described as movement between runtime states or worlds.

This is not the same as:

- a static `ContextDB` object;
- a `SubstitutionDB` context morphism; or
- a type-theoretic Universe level.

Pure sequencing can advance control without changing an externally observable
world. An effect request can expose a transition. A guarded-recursion clock can
index future observations. These uses must not be conflated.

## 4. Baseline implementation audit

### 4.1 Audited revision

The audit was performed against:

```text
repository: https://github.com/repyt-margorp/a-program
commit:     de2b44241dce30dd1c5549b14645c91eda02da56
artifact:   v78
calculus:   31fc96a8ce13d4771250ac6ad2c2814f4ee4e7d1f04563a6e4165d825f86c2ef
HOTT:       f0cf064cb17e56e2f842ac0144954c1f31f3a120cee11258de5216d4bef8e781
```

The audited revision closes the dimension-action migration. It is not a
general-recursion revision.

### 4.2 Definition blocks are not an execution-time recursive namespace

The root `{{ ... }}.name` form is represented as a definition block followed by
a direct selection. The parser requires exactly one selected direct member,
and the definition-block integration test verifies that a definition entry is
not executed while compiling.

The current implicit definition-thunk policy can cause a selected computation
to be stored as a finite `THUNK(...)` Core term. This does not make the
definition block a temporal sequence of world mutations. Even when a named
entry denotes a computation, the act of naming it is static graph construction.

Relevant implementation:

- `src/prototype/src/frontend/reader.c`;
- `src/prototype/tests/integration/test_definition_block.sh`; and
- `src/prototype/tests/fixtures/typing/definition_block_check.p`.

A future recursive binder should therefore be local to a computation or
function body. It should not obtain its meaning from repeated lookup of a
global definition name.

### 4.3 The Core has no general-recursion term

At the audited commit, `enum prototype_term_tag` ends at
`PROTOTYPE_TERM_DIMENSION_ACTION = 34`. It contains:

```text
RETURN
THUNK
FORCE
OPERATION_REQUEST
COMPUTATION_FOLD
MATCH
INDUCTION_HYPOTHESIS
```

but contains no:

```text
FIX
REC
ITERATE
LOOP
WHILE
DELAY
PARTIAL
```

The corresponding typed-occurrence and artifact grammars also have no general
recursion case.

Relevant implementation:

- `src/prototype/include/a_program/core/term.h`;
- `src/prototype/spec/artifact_v78.schema`; and
- `src/prototype/include/a_program/graph/typed_occurrence_graph.h` and
  `src/prototype/include/a_program/graph/typed_occurrence_model.h`.

### 4.4 Current recursion is guarded Match induction

The existing `INDUCTION_HYPOTHESIS` term belongs to a recursive Match frame.
Artifact v78 preserves the owning Match, case, field, scope, and persistent
binding identities needed to validate and replay an IH occurrence. This is a
total eliminator mechanism, not unrestricted recursion.

The fuel-free QuickSort fixture constructs `Acc Nat LT size` as an ordinary
indexed family and makes both recursive calls through `*down` with explicit
decrease evidence. The implementation plan explicitly rejects direct
unguarded recursion and states that no Acc-specific kernel or evaluator rule
was added.

Relevant implementation:

- `src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p`;
- `src/prototype/tests/integration/test_if8_fuel_free_quicksort.sh`; and
- `doc/2026-08-14T18-07-35-IF8-EQUALITY-TRANSPORT-AND-FUEL-FREE-QUICKSORT-IMPLEMENTATION-PLAN.md`.

This current total mechanism should remain available after partial recursion is
added. A general-recursion feature must not weaken or reinterpret existing IH
authority.

### 4.5 Computation sequencing and handling use `COMPUTATION_FOLD`

The authoritative computation-tree constructors are currently:

```text
RETURN(value)
OPERATION_REQUEST(operation, argument, continuation)
COMPUTATION_FOLD(computation, return_clause, clauses)
```

The zero-clause specialization sequences a computation through its return
continuation. A fold with clauses handles matching operation requests and
forwards unmatched requests through captured frames.

There is no independent Core `BIND` term. Historical `BIND(M, K)` notation must
not be mistaken for the current representation.

Relevant design and implementation:

- `doc/2026-08-04T00-00-00-COMPUTATION-BLOCK-SEQUENCE-AND-LAMBDA-EXIT-MIGRATION.md`;
- `src/prototype/src/core/term/evaluation_and_conversion.inc`; and
- `src/prototype/src/graph/typed_occurrence/runtime.inc`.

General recursion would make the semantic computation tree potentially
non-well-founded. The current finite fold implementation does not by itself
specify how a handler folds an infinite tree or a cyclic recursive equation.

### 4.6 The runtime machine can iterate internally but has no source recursion

The occurrence runtime is an explicit machine with:

- a runtime value environment;
- return, application, Match, sequencing, handler, request, resumption, and
  verification frames;
- one-shot, multi-shot, and abortive resumptions; and
- an unbounded host-language driver loop around bounded machine storage.

The driver loop is an implementation loop. It does not make unbounded source
recursion representable. The machine currently has fixed capacities, including
frame, handler, resumption, and environment arrays. A naive recursive lowering
that grows these arrays would eventually report a capacity failure rather than
model divergence. Tail iteration therefore needs an explicit constant-space
runtime path or a justified dynamic representation.

There is no runtime step limit in `operation_runtime_machine_run`. The
normalizer and solver do have step limits, but an exhausted normalization
budget is a diagnostic or residual-verification outcome, not proof of semantic
divergence.

### 4.7 Current computation types do not record totality

The Core classifier view records:

```text
category
computation_kind
effect_row
result
```

It does not record totality, partiality, productivity, or a divergence effect.
Artifact v78 likewise has no totality field or partial-computation classifier.

Adding a general-recursion term without adding a static boundary would allow a
potentially diverging computation to have the same classifier shape as a total
one. In a dependent language, that is insufficient.

### 4.8 Current verification obligations are not termination obligations

Artifact v78 admits two verification kinds:

```text
COMPUTATION_FOLD_RESULT
EFFECT_ROW_EQUATION
```

The first validates the classifier obtained after a particular computation
returns. It does not prove that the computation returns. The second validates
an effect-row equation. Neither is a termination, productivity, trace, or
partial-correctness judgement.

The phrase "least-fixed-point replay" in the artifact schema refers to
reconstructing compiler lookup and closure indices after readback. It does not
mean that the language implements domain-theoretic least fixed points for
programs.

### 4.9 Current observational identity excludes this boundary

Artifact v78 explicitly leaves effectful requests and folds residual for object
identity roots. It does not claim complete contextual equivalence. There is no
accepted identity rule for partial computations, divergence, infinite traces,
or recursive processes.

This is the correct conservative state. General recursion must not be added to
Higher Observational Type Theory by treating one-step unfolding as sufficient
evidence of observational identity.

## 5. Domain theory and least fixed points

### 5.1 What domain theory contributes

The classical domain-theoretic account introduces an information order. A
partial result is approximated by increasingly informative finite stages. A
pointed complete partial order contains a least element `bottom`, and every
increasing omega-chain has a least upper bound.

For a continuous endomap `F`, the canonical recursive solution is:

```text
fix(F) = lub_n F^n(bottom)
```

This provides more than the equation:

```text
fix(F) = F(fix(F))
```

It selects the least solution. The selection matters whenever the equation has
multiple solutions.

### 5.2 What CBPV contributes

Levy's CBPV separates value judgements from computation judgements. His
recursion rule is, in readable notation:

```text
Gamma, x : U B |-c M : B
────────────────────────
Gamma |-c rec x.M : B
```

The recursive assumption is a value of thunk type `U B`, while the body and
result are computations of type `B`. The denotational model interprets value
types as predomains, computation types as pointed domains, and `rec` as a least
prefix point. The operational account includes both a recursive term and an
explicit divergent computation and proves adequacy for the model.

This establishes a principled CBPV location for recursion. It does not imply
that every CBPV model automatically supports recursion. The computation side
must carry suitable pointed and continuous structure.

### 5.3 A Program consequence

A Program can use a local recursive value of thunked computation type as the
CBPV boundary, but a typing rule of that shape is only the beginning. Before
acceptance, the design must state:

- which computation classifiers have a selected least solution;
- which Core operations are continuous;
- whether every effect operation and handler is continuous;
- whether infinite effect traces are retained or collapsed to bottom;
- what operational adequacy theorem is targeted; and
- how the selected solution is represented in artifacts without requiring
  infinite syntax.

## 6. Iteration theories and complete Elgot monads

### 6.1 Why bare fixed-point unfolding is too weak

Compiler transformations routinely:

- add parameters;
- inline or outline a loop body;
- flatten nested loops;
- rename or map exit values;
- fuse state;
- transport a loop through an effect interpretation; and
- link separately compiled components.

The unfolding equation alone does not validate these transformations.
Iteration theories axiomatize the equations expected of a coherent choice of
recursive solutions. Conway-style axioms include parameter, composition or
dinaturality, and double-dagger or codiagonal principles. Uniformity controls
solution preservation under simulation.

### 6.2 Effectful iteration

A complete Elgot monad equips a strong monad `T` with:

```text
(-)^dagger : (X -> T(Y + X)) -> (X -> T Y)
```

satisfying unfolding, naturality, dinaturality, codiagonal, uniformity, and
compatibility with strength.

The form separates:

- loop-carried state `X`;
- exit values `Y`;
- one iteration step;
- base effects represented by `T`; and
- the coherent solution operation.

It is therefore a better semantic contract for procedural `loop` and `while`
than a source-level self-reference alone.

### 6.3 Fixed points and iteration are related but not definitionally identical

In sufficiently structured Cartesian, closed, enriched, or Kleisli settings,
parametrized fixed-point operators, traces, and iteration operators can often
be derived from one another. The required products, coproducts, exponentials,
strength, continuity, and strictness conditions matter.

A Program must not assume without proof that one first-order `ITERATE` node
automatically implements every higher-order recursive function, or that an
unrestricted higher-order `REC` automatically satisfies the Elgot laws for
effectful loops.

### 6.4 A Program consequence

The proposed minimum semantic interface should be iteration, even if a future
surface language offers `rec`, `loop`, and `while` conveniences.

Conceptually:

```text
loop state with
    break y
    continue next_state
```

should lower to an equation of shape:

```text
step : X -> Comp(E, Y + X, partial)
iterate step initial_state : Comp(E, Y, partial)
```

The exact representation of sums and the totality classifier is not yet
accepted. This equation is a semantic target, not current syntax.

## 7. Algebraic effects, handlers, and recursive effect trees

### 7.1 Algebraic operations are constructors of effect trees

Plotkin and Power characterize algebraic operations by their naturality with
respect to computation results and their compatibility with monadic
substitution. Plotkin and Pretnar treat handlers as interpretations of such
operations.

A Program's `OPERATION_REQUEST` and `COMPUTATION_FOLD` architecture is close to
this constructor-and-fold view. General recursion changes the input to a fold
from a finite tree into a potentially cyclic or non-well-founded computation.

### 7.2 Handler preservation of iteration

Suppose `h` interprets computations from `T` into `S`. To move a handler across
a recursive computation safely, it should preserve the selected iteration:

```text
h(f^dagger) = (h f)^dagger
```

The exact right-hand side must account for the functorial mapping of exit and
continue branches and for carried parameters. The important point is that
monad-morphism preservation alone is not automatically iteration preservation.
Complete Elgot monad morphisms add this requirement explicitly.

An arbitrary handler may:

- discard a continuation;
- resume it once;
- resume it many times;
- retain it for later use;
- change the base effect; or
- turn a finite step into a nonterminating computation.

Consequently, handler typing and totality cannot be specified independently.

### 7.3 Infinite observable effects

The following computations are both non-returning:

```text
silent_loop
```

```text
print "tick"; print "tick"; print "tick"; ...
```

A bottom-only result semantics may identify them. A trace or resumption
semantics can distinguish them. A Program already has observable operation
requests, captured resumptions, and handler multiplicities, so collapsing all
non-returning executions to one bottom value would lose information that the
runtime can in principle expose.

The generalized coinductive resumption construction records a possibly
infinite tree of effect observations over a base monad. Complete Elgot
structure then supplies coherent unguarded iteration when the base supports
it. This is a closer comparison point for A Program than a partial-value monad
alone.

### 7.4 Resource-sensitive resumptions and recursion

The current runtime distinguishes multi-shot, one-shot, and abortive
resumptions, and artifact propositions carry usage grades `ZERO`, `ONE`, and
`MANY`.

A conventional recursive binder is reusable and therefore has grade `MANY`.
If its closure captures a one-shot resumption or a linear resource, repeated
recursive invocation can duplicate that resource. A future rule must either:

- reject such capture;
- require an explicit unrestricted modality;
- use a graded or linear fixed-point principle;
- prove that the recursive path consumes the resource at most once; or
- transform the loop so the resource is threaded linearly as state.

This is not a later optimization. It affects the sound typing rule for the
recursive binder.

## 8. Partiality inside constructive type theory

### 8.1 Delay or partiality monads

Capretta represents a partial element of `A` coinductively with constructors
equivalent to:

```text
now   : A -> Partial A
later : Partial A -> Partial A
```

Finite `later` prefixes represent delayed termination and an infinite sequence
of `later` constructors represents divergence. This permits general recursive
functions to be represented as total functions returning partial values.

The construction exposes an intensional question: should computations that
differ only by a finite or countably infinite number of silent delay steps be
equal? Quotienting weak bisimilarity constructively is nontrivial. A Program's
observational identity work makes this choice especially visible.

### 8.2 Domain predicates and `Acc`

The Bove-Capretta method derives an inductive domain predicate describing the
inputs on which a recursive equation terminates. The function then recurses
structurally over a proof of that predicate.

A Program's current `Acc` QuickSort is in this family. It is the correct method
when the intended function is total and a termination argument is available.
It is not an implementation of an intentionally partial interpreter, server,
or event loop.

### 8.3 Fixed-point induction and admissibility

Domain-theoretic reasoning often uses a rule schematically like:

```text
P(bottom)
for all x, P(x) -> P(F(x))
P is admissible
──────────────────────────
P(fix(F))
```

Admissibility normally requires closure under limits of increasing chains.
Not every predicate in an expressive dependent type theory is admissible.
Partiality, State and Dependent Types emphasizes that unrestricted fixed-point
induction is unsound without an admissibility discipline or a restricted
logic.

A Program therefore cannot introduce a generic proof constructor saying
"unfold once and use induction" for every dependent proposition over a partial
computation.

## 9. Dependently typed CBPV and logical isolation

### 9.1 Dependency on returned values

Dependent CBPV distinguishes a conservative system without dependent Kleisli
extension from a stronger system that allows later types to depend on returned
values. Vakar shows that the stronger principle interacts materially with
effects: execution can refine the type of a computation, uniqueness of typing
can be lost, and subtyping may be required.

This is directly relevant to A Program's `COMPUTATION_FOLD_RESULT` obligation,
which waits for a returned value before validating a dependent continuation
classifier. That obligation is conditional on return; it does not establish
termination.

### 9.2 Total and divergent fragments

F* separates pure total computation from possibly divergent computation using
`Tot` and `Dv`. A `Dv False` computation may loop forever and is not a proof of
`False`; `Tot` computations can be weakened into `Dv`, but `Dv` cannot be
silently promoted into `Tot`.

A Program needs at least an equivalent one-way isolation boundary if general
recursion is admitted. Treating divergence solely as an ordinary effect-row
atom is risky because ordinary operation effects may be handled and removed.
No ordinary handler should be able to turn an arbitrary partial computation
into accepted total evidence.

### 9.3 Post-hoc reasoning is a separate feature

F*'s simple `Dv` isolation does not by itself provide unrestricted extrinsic
proofs about divergent code. Hoare-style type theories instead support partial
correctness assertions about results that are actually returned.

A Program has discussed the schematic judgements:

```text
PartialEnsures(c, P) := for every v, Returns(c, v) implies P(v)
TotalEnsures(c, P)   := Terminates(c) and PartialEnsures(c, P)
```

These are useful only after `Returns` and `Terminates` are defined against a
specific operational or denotational semantics.

They are not current A Program IADTs, Claims, Derivations, certificates, or
verification obligations.

## 10. May, must, productivity, and fairness

For deterministic closed computations, it is common to define divergence as
the absence of a terminal evaluation. Levy notes that this definition is
acceptable only in a deterministic setting.

With nondeterminism or concurrency, at least the following differ:

```text
MayReturn(c, v)
    some execution of c returns v

MayTerminate(c)
    some execution terminates

MustTerminate(c)
    every permitted execution terminates

PartialEnsures(c, P)
    every returned result satisfies P, while divergence is allowed

Productive(c)
    every finite observation demand can be met in finite execution
```

If scheduling exists, `MustTerminate` can quantify over all schedules, all fair
schedules, or one selected deterministic scheduler. These are different
properties.

The earlier schematic formula:

```text
Terminates(c) and for every v, Returns(c, v) implies P(v)
```

is adequate only after this quantification is fixed. For a nondeterministic
language, an existential `Terminates` combined with universal returned-result
safety is not total correctness.

## 11. Guarded recursion, coinduction, and worlds

### 11.1 Guarded recursion

Guarded type theories place recursive references under a `later` modality.
Conceptually:

```text
fix : (later A -> A) -> A
```

The guard ensures that recursion advances one logical time step before the
recursive value is demanded. Clock quantification can recover coinductive
types and control elimination of the guard.

This is appropriate for productive streams, reactive systems, and step-indexed
reasoning. It is not a semantics for arbitrary silent divergence: a silent
self-call with no guard is intentionally rejected.

### 11.2 Coinduction

Coinduction describes potentially infinite observations through a greatest
fixed point or final coalgebra. General partial recursion in a domain commonly
uses a least fixed point. These are related in resumption models but are not
synonyms.

The equation:

```text
state -> base_effect(result + observation * next_state)
```

has a clear transition-system or world-step reading. Each observation exposes
one layer and a successor state. A pure computation-block bind does not, by
itself, create such an observation.

### 11.3 A Program consequence

If A Program eventually supports both servers or streams and arbitrary partial
functions, it should retain separate classifiers or certificates for:

- total computation;
- partial computation; and
- productive process.

One local recursive-equation representation may be shared, but the proof rule
and observational equality must differ.

## 12. Candidate representations for A Program

No candidate in this section is accepted.

### 12.1 Candidate A: raw computation recursion

Add a Core rule corresponding closely to Levy:

```text
Gamma, self : Thunk(B) |-c body : B
────────────────────────────────
Gamma |-c rec self.body : B
```

Advantages:

- directly supports higher-order recursive computation;
- respects the CBPV value/computation boundary; and
- uses a local binder rather than a definition-name cycle.

Unresolved obligations:

- selected fixed-point solution;
- Elgot or Conway laws;
- handler compatibility;
- totality classification;
- resource grade of `self`;
- constant-space tail execution;
- proof principles; and
- artifact replay.

This rule alone is not sufficient.

### 12.2 Candidate B: structured effectful iteration

Add a first-order semantic constructor corresponding to:

```text
ITERATE(step, initial_state)

step : X -> Comp(E, Y + X, partial)
```

Advantages:

- directly models procedural loops;
- exposes loop-carried state;
- distinguishes exit from continuation;
- has a mature equational theory through complete Elgot monads; and
- admits a constant-space abstract machine implementation.

Unresolved obligations:

- representation of `Y + X` in the current Core;
- derivation of higher-order recursion;
- dependent loop-state families;
- interaction with multi-shot handlers; and
- whether guarded and total variants share the same Core tag.

This is the recommended minimum semantic contract.

### 12.3 Candidate C: coinductive resumption

Represent recursive effectful computation through a final coalgebra or
resumption layer over the existing effect system.

Advantages:

- records infinite observable behavior;
- separates one observable step from the rest of the process;
- is a natural basis for guarded recursion and productivity; and
- composes with complete Elgot structure under studied conditions.

Costs:

- substantially larger type and artifact design;
- requires an explicit observational quotient or bisimilarity policy;
- complicates handler and HOTT integration; and
- is unnecessary if the first milestone only needs silent partial functions
  and finite-effect prefixes.

This should be treated as a semantic extension point, not silently assumed by
a first implementation.

### 12.4 Candidate D: encode recursion as an algebraic operation

A recursive call could be surfaced as an operation request and interpreted by
a handler. This is attractive because A Program already has operations and
resumptions.

It is not recommended as the foundational encoding without further proof:

- unguarded recursive solution selection is additional structure beyond a
  free algebraic signature;
- a general recursive call is locally scoped and binds an equation;
- ordinary handlers may discard or duplicate continuations;
- removing an operation row must not certify termination; and
- recursion must remain meaningful when no user handler is present.

Higher-order or scoped-effect research may supply an encoding, but the encoding
must still prove the iteration laws and totality boundary.

## 13. Recommended semantic split

### 13.1 Computation classifier

Add a totality or recursion mode independent of the ordinary effect row. A
schematic classifier is:

```text
Comp(effect_row, result, totality)

totality ::= total | partial | productive
```

The exact representation is undecided. The required ordering is at least:

```text
total <= partial
```

A total computation may be used where partial computation is permitted. The
reverse direction requires accepted evidence and must not be an ordinary
subeffect elimination.

### 13.2 Total fragment

The current structural Match/IH and `Acc` mechanisms remain total. Their
evaluation can continue to participate in pure normalization and definitional
conversion under the existing normalization profiles.

### 13.3 Partial fragment

Unguarded recursion produces partial computation. It may execute at runtime but
must not be unfolded by conversion in a way that makes type checking diverge.
Its result cannot be substituted into a type until an actual return is observed
at an explicitly permitted dependent-computation boundary.

### 13.4 Productive fragment

Guarded recursion or coinductive resumption may later produce a productive
classifier. Productivity does not imply termination and must not be promoted to
total result production.

### 13.5 Divergence is not an ordinary removable operation

It may be convenient to display `partial` in an effect-like diagnostic row, but
the authority must remain separate. A user handler that removes all explicit
operation requests cannot thereby prove that the handled computation
terminates.

## 14. Proposed lowering boundary

Surface syntax is explicitly postponed. Whatever spelling is chosen, the
lowering should preserve these invariants:

1. the recursive binder has an AST and persistent local binding identity;
2. its scope is the recursive body only;
3. the binding does not enter the root definition namespace;
4. the Core graph remains finitely serializable;
5. the recursive edge is explicit rather than inferred from a shared TermDB
   identity;
6. typed occurrences preserve source provenance and Context;
7. a recursive occurrence has a declared resource grade;
8. the classifier records partiality before runtime;
9. the artifact records the semantic recursive edge and does not reconstruct it
   through name lookup; and
10. linking preserves recursive binding identity without creating accidental
    cross-artifact cycles.

One possible conceptual lowering is:

```text
surface while/loop
    -> structured ITERATE equation
    -> local recursive Core binder or runtime loop frame
```

A separately named recursive function would name a finite thunk containing
that local equation:

```text
definition name
    -> THUNK(local recursive computation)
```

The definition name itself is not recursively resolved.

## 15. Runtime requirements

A runtime implementation should satisfy all of the following before claiming
general recursion support.

### 15.1 Proper tail iteration

A tail `continue` must not grow the ordinary application or fold stack. An
infinite tail loop should run until externally interrupted, not become
`PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY` merely because it iterated.

### 15.2 Non-tail recursion

Non-tail recursive calls may consume dynamic stack or heap. Resource
exhaustion is a host failure and must be distinguished from semantic
divergence.

### 15.3 Cancellation and execution budget

An embedding API may need cancellation, timeout, or a step budget. Exhausting
such a budget means "execution was interrupted," not "the program is proven to
diverge."

### 15.4 Infinite effects

The runtime trace model must specify whether an infinite stream of handled or
unhandled requests can be observed incrementally. It must not wait for a final
return before exposing every useful observation.

### 15.5 Resumption multiplicity

Recursive closures that capture resumptions must respect one-shot and abortive
runtime rules. Static usage authority and runtime consumed-state checks must
agree.

## 16. Normalization and conversion requirements

### 16.1 Do not normalize arbitrary partial recursion during type conversion

One-step unfolding may be available to the runtime or an explicit program
reasoner. Definitional equality must not repeatedly unfold a partial recursive
term until the normalizer's step limit is exhausted.

### 16.2 Budget exhaustion is not inequality

The current distinction among complete, residual, blocked-effect, exhausted,
and invalid normalization outcomes should be preserved. Encountering recursion
may produce a dedicated residual reason, but not a false `NOT_EQUAL` result.

### 16.3 Totalization must not retroactively extend DefEq without a policy

If a user later proves that a particular partial computation terminates, A
Program may expose a total wrapper or a theorem relating returned results. It
should not automatically add arbitrary recursive unfoldings to global
definitional equality. Doing so can threaten decidable conversion and artifact
stability.

## 17. Proof and specification requirements

### 17.1 Required operational vocabulary

Before adding proof constructors, define at least:

```text
MayReturn(computation, value)
MustTerminate(computation)
ProducesPrefix(computation, observation_prefix)
Productive(computation)
```

For the deterministic initial fragment, `MayReturn` may coincide with the
single machine execution reaching `RETURN`. The names should still avoid
silently fixing future nondeterministic quantification.

### 17.2 Partial and total correctness

Then define schematic derived properties:

```text
PartialEnsures(c, P)
    := for every v, MayReturn(c, v) implies P(v)

TotalEnsures(c, P)
    := MustTerminate(c) and PartialEnsures(c, P)
```

For effectful code, `P` may need access to traces, final stores, handled
operation histories, or resource outcomes. A result-only postcondition is not a
complete effect specification.

### 17.3 Fixed-point induction

Do not admit unrestricted fixed-point induction. Select one of:

- an explicit admissibility type class or proposition;
- a restricted syntactic predicate fragment;
- step-indexed or guarded reasoning;
- a Hoare-style partial-correctness logic without general fixed-point
  induction; or
- an external certificate checker whose admitted theorem states the exact
  semantic assumptions.

### 17.4 Bridge to total computation

A future totalization operation should require an accepted termination proof
for the exact typed occurrence and environment assumptions. Its result should
be a new total computation or wrapper with explicit derivation, not a mutation
of the original partial occurrence.

## 18. Higher Observational Type Theory boundary

General recursion makes the observation relation a first-order design choice.
Possible equivalences include:

- equality of returned values only;
- may-equivalence;
- must-equivalence;
- weak bisimilarity that ignores finite silent delay;
- strong bisimilarity that counts every step;
- finite-trace equivalence;
- infinite-trace equivalence; and
- contextual equivalence under a chosen handler set.

These relations disagree on silent divergence, finite delay, infinite output,
and nondeterministic branches.

Until one is selected and its congruence rules are proved, partial recursive
computations should remain residual in object identity construction. In
particular:

```text
rec self.body
```

must not be declared equal to its one-step unfolding merely because the
operational machine can perform that step. An unfolding theorem may be valid
without being definitional equality or complete observational identity.

## 19. Artifact and audit requirements

Adding recursion would require a new artifact version because the semantic Core
and typed-occurrence vocabularies change.

An accepted artifact design must specify:

- recursive Core tag and payload;
- persistent recursive binder identity;
- typed occurrence kind and child roles;
- classifier totality mode;
- resource usage of recursive references;
- relocation and linking of recursive scopes;
- allowed recursive cycles in dependency analysis;
- proof and verification kinds, if any;
- runtime capability bits for partial recursion and iteration;
- rejection of malformed cross-scope recursive references;
- normalization residual behavior; and
- exact replay checks that do not solve recursive equations by heuristic graph
  search.

Artifact closure currently assumes a finite dense accepted object graph. That
can remain true if recursive syntax is represented by a finite binder and
reference. The semantic execution may be infinite without serializing an
infinite tree.

## 20. Comparative requirement matrix

Legend:

```text
yes       central part of the approach
partial   supported only under additional structure or restrictions
no        deliberately outside the approach
separate  handled by an orthogonal logic or classifier
```

| Requirement | Least domain fix | Levy CBPV rec | Complete Elgot | Delay/Partiality | Acc/domain predicate | Guarded recursion | F* Tot/Dv | A Program need |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Arbitrary divergence | yes | yes | yes | yes | no | no | yes | yes |
| Value/computation polarity | no | yes | partial | partial | no | partial | yes | yes |
| Effectful loop laws | partial | partial | yes | partial | no | partial | partial | yes |
| Infinite observations | model-dependent | model-dependent | via resumptions | delay only | no | yes | no | decision required |
| Totality proof | separate | separate | separate | separate | yes | productivity only | Tot | yes |
| Partial correctness | admissible predicates | separate | separate | separate | total only | guarded logic | restricted for Dv | yes |
| Dependent result types | difficult | not original CBPV | not intrinsic | yes, intensional | yes | yes | yes | yes |
| Handler preservation | model-specific | model-specific | explicit morphism law | not intrinsic | no | model-specific | effect-specific | yes |
| Resource-sensitive recursion | specialized | no | strength only | no | structural usage | specialized | no | yes |
| Artifact replay authority | no | no | no | no | proof term | no | compiler-specific | yes |

No reviewed framework supplies A Program's complete combination. The design is
therefore integration research, not selection of a single off-the-shelf rule.

## 21. Recommended staged program

### Stage GR0: observational and semantic decision

Before code changes:

- choose bottom-only, delay-sensitive, or resumption-sensitive semantics;
- decide whether infinite effect traces are observable;
- define deterministic `MayReturn` and `MustTerminate`;
- state the intended iteration laws;
- state the handler preservation requirement; and
- state the totality ordering.

Completion evidence is a normative calculus document, not a prototype test.

### Stage GR1: classifier and local equation prototype

In `src/prototype/` only:

- prototype a partiality classifier;
- prototype a finite local recursive binder and reference;
- forbid it in type-level normalization and proof authority;
- assign resource grade `MANY` unless a stricter rule is designed; and
- add negative scope and forged-artifact tests.

Do not add HOTT roots at this stage.

### Stage GR2: structured iteration runtime

- implement `X -> T(Y + X)` iteration or an equivalent typed Core shape;
- guarantee proper tail iteration;
- distinguish cancellation, capacity failure, and divergence;
- test silent loops and finite-return loops; and
- test effects before, within, and after loop steps.

### Stage GR3: handlers and resumptions

- characterize which handlers preserve iteration;
- test zero-, one-, and multi-resume behavior;
- prevent one-shot resource duplication;
- decide how unhandled infinite requests are observed; and
- add capability and artifact checks.

### Stage GR4: post-hoc properties

- add exact `MayReturn` and partial-correctness evidence;
- add `MustTerminate` only with explicit execution quantification;
- specify admissibility or avoid general fixed-point induction;
- prototype totalization as a new derived computation; and
- keep sortedness, permutation, or other functional properties independent of
  the termination certificate.

### Stage GR5: guarded and observational integration

- add productivity only if demanded by streams or reactive programs;
- choose weak or strong delay observation;
- define partial-computation congruence under handlers;
- add HOTT rules only for the proven fragment; and
- retain residual outcomes outside that fragment.

## 22. Required tests before acceptance

### 22.1 Static negative tests

- a partial computation cannot inhabit `False` as a proof;
- a partial result cannot be used as a type index without an allowed return
  boundary;
- a recursive reference cannot escape its binder;
- a recursive binder cannot be forged by a colliding Binding ID;
- an ordinary handler cannot erase partiality;
- one-shot resumptions cannot be captured by unrestricted recursion; and
- normalization exhaustion is not reported as definitional inequality.

### 22.2 Runtime tests

- a finite countdown returns;
- a silent tail loop does not grow the runtime frame stack;
- a non-tail recursive program behaves according to the host-resource policy;
- cancellation interrupts a loop without claiming divergence;
- an infinite print loop exposes finite prefixes if the chosen semantics
  observes them;
- a loop with a handled operation preserves the chosen iteration law; and
- one-shot and multi-shot handler cases remain distinct.

### 22.3 Artifact tests

- recursive binders round-trip with stable scope identity;
- relocation preserves local recursive references;
- malformed cross-artifact recursive edges reject;
- artifact linking does not infer recursion from name equality;
- a partiality capability is required for execution;
- total-only backends reject partial recursive entries; and
- no partial runtime outcome manufactures a Claim or Derivation.

### 22.4 Metatheoretic tests or proofs

- preservation and progress for the partial computation fragment;
- consistency or logical isolation of the total fragment;
- operational adequacy for the selected fixed-point semantics;
- Elgot/Conway laws for iteration;
- handler preservation for every admitted handler class;
- subject reduction at dependent computation-fold boundaries; and
- soundness of any termination-to-totality bridge.

## 23. Decisions that must remain explicit

The implementation plan must answer each of these questions rather than inherit
an accidental answer from the first runtime prototype.

1. Is silent divergence one bottom element or an explicit infinite delay?
2. Are finite numbers of silent steps observationally irrelevant?
3. Are infinite output traces preserved?
4. Is nondeterminism planned, and if so, are properties may, must, or both?
5. What scheduler and fairness assumptions are observable?
6. Is partiality a classifier axis, a modality, an effect, or a combination?
7. Which handlers preserve or change partiality?
8. What is the usage grade of a recursive reference?
9. Can a recursive closure capture a one-shot resumption?
10. Which form of iteration is artifact authority?
11. What is the relationship between raw higher-order `rec` and structured
    `iterate`?
12. Can a termination proof create a total wrapper, and what equality relates
    it to the original computation?
13. Which predicates support fixed-point induction?
14. Which observation relation supports partial-computation identity?
15. Does a backend have to support unbounded execution, cancellation, or both?

## 24. Literature coverage and its limits

### 24.1 Levy: CBPV with recursion

Levy supplies the polarity-correct recursive computation rule, operational
semantics, pointed-domain interpretation, and adequacy result. The treatment is
the primary justification for placing an unrestricted recursive assumption at
`U B` and producing a computation `B`.

It does not settle A Program's dependent classifier obligations, explicit
handler fold representation, resource grades, HOTT identity, or artifact
authority.

### 24.2 Bloom-Esik and complete Elgot monads

Iteration theories identify equations valid of coherent parametrized
fixed-point operators. Complete Elgot monads adapt this discipline to
effectful iteration, and generalized resumption work explains how base effects
combine with potentially non-well-founded processes.

This is the strongest reviewed basis for the laws of procedural effectful
loops. It does not directly provide a dependent type checker or proof artifact
format.

### 24.3 Capretta and Bove-Capretta

The delay-monad account internalizes partial computation as coinductive data.
The domain-predicate account converts a recursive equation into total recursion
on evidence of definedness.

Together they clarify why partial execution and proof of termination are
different constructions. They leave observational quotienting and integration
with A Program's handlers to be designed.

### 24.4 Vakar's dCBPV

Dependently typed CBPV explains why dependent sequencing is not an ordinary
simply typed monadic bind. Strong dependent elimination may require dependent
Kleisli extension and can affect uniqueness of typing as effects execute.

This exposes the exact area around A Program's dependent computation folds but
does not add a ready-made general-recursion proof logic.

### 24.5 Guarded dependent type theory

Later modalities and clock quantification give a disciplined account of
productive recursion and coinductive types. They support modular productivity
proofs but deliberately do not validate arbitrary unguarded silent loops.

### 24.6 F* and partial Hoare type theory

F* demonstrates practical isolation of total and divergent computation. iHTT
demonstrates reasoning about partial and stateful computations while retaining
a total dependent logic, and makes the admissibility issue for fixed-point
induction explicit.

A Program's desired post-hoc Claim and certificate architecture may need ideas
from both. Neither system's boundary should be quoted as if it already matches
A Program's accepted graph and replay model.

## 25. References

1. Paul Blain Levy. *Call-By-Push-Value*. PhD thesis, Queen Mary,
   University of London, 2001. Chapter 5, "Recursion and Infinitely Deep
   CBPV." <https://pblevy.github.io/papers/thesisqmwphd.pdf>
2. Paul Blain Levy. *Call-by-push-value: Decomposing call-by-value and
   call-by-name*. Higher-Order and Symbolic Computation 19, 2006.
   <https://pblevy.github.io/papers/hosc05.pdf>
3. Stephen L. Bloom and Zoltan Esik. *Iteration Theories: The Equational
   Logic of Iterative Processes*. Springer, 1993.
   <https://doi.org/10.1007/978-3-642-78034-9>
4. Jiri Adamek, Stefan Milius, and Jiri Velebil. *Equational Properties of
   Iterative Monads*. Information and Computation 208, 2010.
   <https://www8.cs.fau.de/ext/milius/publications/files/amv_em_revised.pdf>
5. Sergey Goncharov, Stefan Milius, and Christoph Rauch. *Unguarded
   Recursion on Coinductive Resumptions*. ENTCS 319, 2015.
   <https://arxiv.org/abs/1405.0854>
6. Sergey Goncharov, Stefan Milius, and Christoph Rauch. *Complete Elgot
   Monads and Coalgebraic Resumptions*. ENTCS 325, 2016.
   <https://arxiv.org/abs/1603.02148>
7. Venanzio Capretta. *General Recursion via Coinductive Types*. Logical
   Methods in Computer Science 1(2), 2005.
   <https://arxiv.org/abs/cs/0505037>
8. Ana Bove and Venanzio Capretta. *Modelling General Recursion in Type
   Theory*. Mathematical Structures in Computer Science 15, 2005.
   <https://doi.org/10.1017/S0960129505004822>
9. Matthijs Vakar. *A Framework for Dependent Types and Effects*.
   <https://arxiv.org/abs/1512.08009>
10. Kasper Svendsen, Lars Birkedal, and Aleksandar Nanevski. *Partiality,
    State and Dependent Types*. TLCA 2011.
    <https://kasv.dk/articles/ihtt-ext.pdf>
11. Ales Bizjak, Hans Bugge Grathwohl, Ranald Clouston, Rasmus E.
    Mogelberg, and Lars Birkedal. *Guarded Dependent Type Theory with
    Coinductive Types*. <https://arxiv.org/abs/1601.01586>
12. Masahito Hasegawa. *On Traced Monoidal Closed Categories*.
    <https://www.kurims.kyoto-u.ac.jp/~hassei/papers/tmcc.pdf>
13. Gordon Plotkin and John Power. *Algebraic Operations and Generic
    Effects*. Applied Categorical Structures 11, 2003.
    <https://homepages.inf.ed.ac.uk/gdp/publications/alg_ops_gen_effects.pdf>
14. Gordon Plotkin and Matija Pretnar. *Handling Algebraic Effects*.
    Logical Methods in Computer Science 9(4), 2013.
    <https://arxiv.org/abs/1312.1399>
15. F* documentation. *The Effect of Total Computations* and *Divergence,
    or Non-Termination*.
    <https://fstar-lang.org/tutorial/book/part4/part4_computation_types_and_tot.html>
    <https://fstar-lang.org/tutorial/book/part4/part4_div.html>

## 26. Final review position

The current implementation correctly has no accidental general recursion. Its
Match/IH and `Acc` mechanisms provide accepted total recursion, its computation
fold represents finite sequencing and effect handling, and its artifact and
HOTT boundaries remain conservative around effectful computation.

The next step must not be "add `FIX` to the Term enum." The next step is to
choose and document:

- a coherent effectful iteration theory;
- an observation model for silent and effectful infinity;
- a static totality boundary;
- handler and resource preservation laws;
- a dependent partial-correctness interface; and
- artifact authority for local recursive equations.

Only after those decisions are reviewed should a surface spelling and Core
representation be accepted.
