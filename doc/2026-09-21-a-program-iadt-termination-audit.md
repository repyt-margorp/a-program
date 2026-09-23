# Verification addendum — 2026-09-24

Tracking issue: https://github.com/repyt-margorp/a-program/issues/33

## Classification and latest-revision verification

Trust-boundary and architectural audit, **not an established soundness defect**.

Fresh GitHub clone on 2026-09-24: `a72cda371109fdbf84d747456ed0aeb09af2391e`, `rewrite/pointer-core-hott`.

Confirmed:
- `Acc` is source-defined in the frozen sort provider.
- A mechanically renamed provider (`Acc -> AccessibleFoo`, `acc -> accessible_node`, whole identifiers only) checks with `done steps=57599`. This supports this provider's name independence; it is not a complete proof that no hidden special cases exist.
- `evidence.h` still contains `PG_INDUCTION_ELIM`, `PG_TERMINATION_FORM` and `PG_TERMINATION_INTRO`.
- `evidence.c:3308–3349` implements termination formation/introduction; `derivation.c` checks them through the ordinary proof constructors.

Important clarification: current `pg_prove_termination` checks ownership, contexts, the suspended endpoint, its thunk/computation type, and requires `PG_TOTALITY_TOTAL`. It does **not** inspect a runtime trace or prove arbitrary partial computations terminate. Totality is not purity: `pg_prove_total_pure_value` separately requires totality and an empty effect row (around line 4047).

No rule-disabling experiment, partial-recursion implementation, or exhaustive adversarial kernel suite was executed in this submission pass.

## Problem / audit question

An IADT-centered design can express accessibility and well-founded recursion through general induction. Dedicated termination evidence might still serve a distinct CBPV classification/staging role. Establish whether these rules are foundational, derivable, or convenient representations, rather than assuming that two uses of the word termination are identical.

A source-defined relation about encoded programs is not automatically connected to actual typed computations. Any proposed IADT replacement must supply and validate that bridge, including total-pure projection and restored artifacts.

## Requested investigation

1. Map formation/introduction consumers and their contracts, including artifact checking and total-pure result projection.
2. Explain which obligations are discharged by general positive IADT induction and which are enforced by the termination classifier.
3. Audit IH allocation/recursive-occurrence checking and positivity. General induction is part of the trusted foundation, not an Acc-specific exception.
4. Test renamed accessibility, unrelated well-founded relations, forged IHs, negative IADTs, and erased divergent Core claiming a total type.
5. In an isolated prototype, attempt a derivation/replacement of termination evidence; preserve all validation. “Bypass” in the historical proposal must not mean silently removing checks.
6. Classify failures as foundational, elaboration, evaluator, artifact or legacy dependency, and document any irreducible rule.
7. Treat future unrestricted recursion/minimization as partial unless independently reconstructed into a justified total computation. Pure alone does not imply termination.
8. Do not suggest that every general recursive computation can be totalized through Acc, or that convergence automatically makes equality decidable.

The desired result is a precise minimal trusted boundary, not a mandatory removal of every named termination rule. No claim of a halting decider or current unsoundness is made.

Related: #14's totality/partiality design scope. The attached audit motivates future partial computations but should not be mistaken for an implementation of them.

## Historical audit follows

The supplied audit below is preserved as a dated record. Its proposals are not implemented by this documentation PR. Statements about prior execution or publication refer to the original audit date; the scope of fresh verification is given above.

---

# Audit Note: Termination, `Acc`, and the IADT-Only Kernel Boundary

## Status

**Purpose:** Audit the current A Program design with respect to termination and recursion, under the design goal that the trusted kernel should not acquire dedicated recursion or termination primitives beyond the general machinery required for IADTs.

**Scope:** Current pointer-based implementation, with particular attention to:

- source-defined `Acc`,
- IADT induction/elimination,
- QuickSort and well-founded recursion,
- erased recursive execution,
- `PG_INDUCTION_ELIM`,
- `PG_TERMINATION_FORM`,
- `PG_TERMINATION_INTRO`,
- the boundary between trusted typing/evidence and executable Core terms.

This document is not a claim that the current design is unsound. It records the places where the trusted boundary should be made explicit and where further reduction of special-purpose rules should be investigated.

---

## 1. Executive summary

The current implementation already demonstrates the most important desired fact:

> **Well-founded recursion does not appear to require an `Acc`-specific kernel primitive.**

`Acc` is source-defined as an ordinary indexed inductive proposition, and QuickSort is expressed by induction over an `Acc` witness. Recursive calls are exposed through the induction hypotheses generated from recursive constructor fields.

Conceptually, the current path is:

```text
IADT
  ↓
general IADT induction / elimination
  ↓
source-defined Acc
  ↓
well-founded recursion
  ↓
QuickSort
```

This is structurally close to the desirable architecture.

The primary audit concern is therefore **not `Acc`**.

The main concern is that the evidence/kernel layer still contains rules whose necessity must be justified independently of general IADT elimination, in particular:

```text
PG_TERMINATION_FORM
PG_TERMINATION_INTRO
```

If the long-term design goal is an IADT-centered kernel, these rules should be audited as possible derived/library-level constructions rather than assumed permanent kernel primitives.

A second concern is the separation between:

1. unrestricted fixed-point-like computation in erased Core, and
2. the trusted typing rule that permits only structurally justified induction.

This separation can be sound and is in fact attractive, but it must be enforced by a sufficiently narrow trust boundary. Any path that allows an unchecked recursive Core term to acquire a total type would defeat the intended architecture.

---

## 2. Current architecture observed

### 2.1 `Acc` is source-defined

The current implementation uses a source-level accessibility type of the conceptual form:

```text
Acc :=
  \A : @ =>
  \R : A -> A -> @ =>
  @\subject : A => {
    acc :
      (x : A) ->
      ((y : A) -> R y x -> * y) ->
      * x;
  };
```

The important point is that the kernel does not need to know that this IADT means "accessibility".

From the type theory's point of view, it is simply an indexed inductive family.

The constructor recursively contains accessibility proofs for smaller elements:

```text
Acc R x
  contains
    ∀ y, R y x -> Acc R y
```

Therefore recursion over accessibility is ordinary induction over an IADT.

---

### 2.2 QuickSort uses `Acc` rather than unrestricted recursion

The current QuickSort construction follows the expected well-founded pattern.

Conceptually:

```text
quickSortAcc xs access :=
  induction access with
  | acc current down =>
      ...
      lowerResult := down lower lowerSmaller
      upperResult := down upper upperSmaller
      ...
```

where:

```text
lowerSmaller : measure lower < measure current
upperSmaller : measure upper < measure current
```

The recursive capability is therefore obtained from the induction hypothesis associated with the recursive field of the `Acc` constructor.

This is important:

> The recursive function itself does not need to appear unrestricted in the typing context.

Instead, only recursive uses justified by the constructor structure are made available.

This is the correct shape for compiling well-founded recursion to ordinary inductive recursion.

---

## 3. Positive architectural conclusion

The current implementation already supports the design target:

```text
general well-founded recursion
      ↓ elaboration
Acc over a well-founded relation
      ↓
ordinary IADT induction
```

Therefore a future surface construct such as:

```text
termination_by length
```

does not appear to require a new kernel primitive.

It could elaborate approximately into:

```text
measure relation
  ↓
proof of well-foundedness
  ↓
Acc witness
  ↓
IADT induction over Acc
```

The surface language may provide convenient syntax and proof automation while the trusted core remains unaware of "measure recursion" as a primitive concept.

---

# 4. Design motivation: admitting divergence without contaminating the total kernel

The central design problem is not how to justify recursion already expressible by IADT induction.

Such recursion is intentionally total:

```text
IADT recursion
    ↓
structural induction
    ↓
finite recursive descent
    ↓
total computation
```

The unresolved problem is instead:

> **How should A Program eventually admit genuinely potentially non-terminating computations, including unbounded minimization and general recursive loops, without turning unrestricted recursion into a trusted total-kernel primitive?**

This is the historical and intended context in which the existing `Terminates` predicate should be understood.

The presence of `Terminates` should therefore not be interpreted merely as an alternative implementation of the termination checker currently used for IADT recursion.

Its likely long-term purpose is to express convergence properties of computations that are *not total by construction*.

---

## 4.1 The intended split between total and partial computation

The target architecture should distinguish at least two computational regimes.

### Total definitions

These include computations whose termination follows from the type-theoretic structure itself:

```text
structural recursion over an IADT
well-founded recursion elaborated through Acc
bounded search over Nat
```

They inhabit ordinary total function types:

```text
A -> B
```

No `Terminates` premise should be required merely to use such a function.

### Potentially partial definitions

Future language extensions may deliberately admit definitions such as:

```text
loop x = loop x
```

or:

```text
μ n. P n
```

where the search has no statically known finite upper bound.

These computations should not automatically inhabit the same total function space.

Conceptually they belong to a partial/computation layer:

```text
A -> Partial B
```

or an equivalent representation to be determined later.

Thus the intended hierarchy is:

```text
                      FUNCTION DEFINITIONS
                              │
                ┌─────────────┴─────────────┐
                │                           │
          total by construction       possibly divergent
                │                           │
       IADT / Acc recursion           general fix / μ
                │                           │
              A -> B                  A -> Partial B
```

The exact syntax and representation of `Partial` are intentionally left open in this audit.

The architectural distinction is what matters.

---

## 4.2 Why unrestricted recursion should not simply become another total kernel rule

A tempting implementation would be to admit arbitrary recursion and add a rule resembling:

```text
f : A ->? B
Terminates f
----------------
f : A -> B
```

or, at the level of individual computations:

```text
c : Partial B
Terminates c
----------------
extract c : B
```

Such a rule is dangerous if `Terminates` itself is a privileged or evaluator-driven kernel judgment.

It would make the soundness of the total fragment depend directly on a second, termination-specific trusted subsystem.

That would undermine the design objective that general termination arguments should ultimately reduce to ordinary IADT reasoning whenever possible.

The preferred direction is therefore stronger:

> A termination proof should, where possible, produce an actual total term whose totality is independently checkable by the existing IADT kernel machinery.

---

## 4.3 Intended meaning of a future `terminates_by`

A future syntax inspired by Lean may look superficially like:

```text
func f x :=
    ...
    f y
terminates_by
    measure x
```

However, its semantic meaning should **not** simply be:

```text
define an unrestricted fixpoint
+
attach a trusted termination certificate
```

Instead, the preferred elaboration is:

```text
surface recursive definition
        ↓
identify recursive edges
        ↓
generate decrease obligations
        ↓
construct a well-founded relation
        ↓
construct/use Acc
        ↓
rebuild the function as IADT induction
        ↓
kernel checks the resulting total term
```

Thus:

```text
terminates_by
```

is best regarded as an elaboration mechanism for *recovering a total definition* from recursive surface syntax.

The trusted endpoint remains ordinary IADT induction.

This is closely analogous to the desirable interpretation of Lean-style well-founded recursion, while preserving A Program's stricter goal of avoiding an additional general-recursion primitive in the total kernel.

---

## 4.4 `terminates_by` should not merely change the status of an existing partial term

The distinction is subtle but foundational.

Undesirable conceptual model:

```text
partial f
    ↓
prove Terminates f
    ↓
the same arbitrary fixed-point term is now declared total
```

Preferred conceptual model:

```text
recursive surface program / graph
    ↓
termination proof
    ↓
construct an equivalent Acc/IADT-recursive definition
    ↓
obtain a separately checkable total function
```

In schematic form:

```text
f_partial : A -> Partial B
             │
             │ termination argument
             ▼
        reconstruction
             │
             ▼
f_total   : A -> B
```

The total term should not owe its validity merely to a bit saying that the partial term terminates.

It should possess a total structure the kernel already knows how to validate.

---

## 4.5 Why A Program's function-graph machinery may matter here

A Program already has machinery intended to retain the internal graph/structure of defined functions for reasoning.

That may become useful at the partial-to-total boundary.

Suppose an unrestricted or partial recursive function has a graph exposing recursive transitions:

```text
@f
 │
 ├── recursive edge x -> y₁
 ├── recursive edge x -> y₂
 └── ...
```

A `terminates_by rank` elaborator could attempt to prove:

```text
rank y₁ < rank x
rank y₂ < rank x
...
```

for every recursive edge.

If successful, the graph supplies enough structural information to reconstruct a corresponding well-founded definition:

```text
function graph
      ↓
recursive edges
      ↓
decrease proofs
      ↓
Acc recursion
      ↓
total IADT definition
```

This is one reason the graph representation and termination machinery should remain related but conceptually distinct.

The graph describes *what the computation does*.

The accessibility proof explains *why its recursive descent is finite*.

---

## 4.6 Two different roles for termination proofs

The phrase "termination proof" may refer to two fundamentally different things, and the implementation should not conflate them.

### A. Definition-time totality reconstruction

Example:

```text
func f x :=
    ...
    f y
terminates_by rank x
```

Here the proof is used during elaboration to construct an `Acc`/IADT-recursive term.

Result:

```text
f : A -> B
```

The final total function does not depend on a privileged runtime `Terminates` assertion.

### B. A theorem about an already partial computation

Example:

```text
search : A -> Partial B

search_terminates :
    (x : A) -> Terminates (search x)
```

Here `Terminates` is genuinely a proposition about a computation that was allowed to exist without prior totality.

This is expected to become important for unbounded minimization and other partial computations.

These two mechanisms can share proof infrastructure, but they should remain semantically distinct.

---

## 4.7 Intended role of `Terminates`

Under the target architecture, `Terminates` has a legitimate role:

```text
Terminates c
```

means approximately:

> the potentially partial computation `c` reaches a result after finitely many computation steps.

It does **not** mean:

> unrestricted recursion is automatically acceptable in the total fragment.

This allows A Program to eventually express both:

```text
a computation whose termination is unknown
```

and:

```text
a theorem proving that the computation terminates on some or all inputs
```

without weakening the basic total interpretation of ordinary function types.

---

## 4.8 The long-term design goal

The desired endpoint can be summarized as follows:

```text
                     A PROGRAM
                         │
          ┌──────────────┴──────────────┐
          │                             │
      TOTAL WORLD                  PARTIAL WORLD
          │                             │
  IADT structural recursion       general recursion
  Acc/well-founded recursion      unbounded μ
  bounded minimization            explicit divergence
          │                             │
          │                        Terminates c
          │                             │
          └──────── total reconstruction ┘
                         │
                         ▼
                  Acc / IADT term
                         │
                         ▼
                  trusted total kernel
```

The strongest version of the design principle is:

> **Partial computation may be expressive, but admission into the total world should be witnessed by reconstruction into the ordinary total core whenever such reconstruction is possible.**

This keeps the future language capable of expressing divergence without making divergence part of the trusted meaning of ordinary total functions.

---

## 4.9 Audit implication

The existing `PG_TERMINATION_FORM` and `PG_TERMINATION_INTRO` should therefore be audited with this historical/intended purpose in mind.

Their presence is not necessarily accidental or conceptually misguided.

They may be early infrastructure for the eventual distinction between:

```text
potentially divergent computation
```

and:

```text
a proof that this particular computation terminates
```

The concern is narrower:

1. Are the current rules already conflating partial-computation convergence with total-definition admissibility?
2. Are they stronger than the future partial-computation semantics actually requires?
3. Can `Terminates` eventually be represented as an ordinary inductive finite-convergence witness?
4. Can a Lean-like `terminates_by` elaborate to `Acc`/IADT reconstruction instead of invoking a trusted `Terminates -> total` coercion?
5. Can partial definitions and their total reconstructions be related through the existing function-graph machinery without enlarging the kernel?

Until these questions are answered, `PG_TERMINATION_*` should be treated as **provisional architecture**, not simply deleted and not automatically accepted as foundational.

---

# 5. Intended semantics of minimization and `Terminates`

The role of `Terminates` should be interpreted against an important distinction from recursive-function theory:

```text
bounded minimization over Nat
```

and:

```text
unbounded minimization (μ)
```

must not be conflated.

The current total type-theoretic fragment should only need the first kind.

## 4.1 Bounded minimization belongs to the total fragment

A bounded search has the conceptual form:

```text
μ n ≤ b. P n
```

where `b : Nat` is already available.

Operationally, this is only a finite search:

```text
test 0
test 1
...
test b
```

Whether or not a witness exists, the search itself terminates because the number of candidates is bounded by a natural number.

This can be implemented by ordinary structural recursion on `b`, or equivalently on the finite search interval.

Therefore bounded minimization does **not** require a special termination predicate.

Conceptually:

```text
bounded minimization
      ↓
structural recursion on Nat
      ↓
ordinary IADT induction
      ↓
total function
```

This should remain inside the ordinary total fragment.

A future A Program primitive or surface construct for bounded minimization should therefore elaborate to ordinary recursion over `Nat` rather than introduce a new trusted termination mechanism.

---

## 4.2 Important qualification

It would be too strong to identify the entire expressive power of A Program's total type theory with the classical class of functions obtained only from bounded minimization.

A dependent type theory with general inductive families and well-founded recursion may define total functions that are not most naturally described in classical primitive-recursive syntax.

The narrower and relevant claim is:

> **When A Program exposes a minimization/search operation inside the total type system, that operation should be naturally bounded by a `Nat` or another finite inductive structure.**

The bound provides the structural recursion required by the total theory.

Thus the current termination story remains:

```text
finite inductive structure
        ↓
structural recursion

or

well-founded relation
        ↓
Acc
        ↓
IADT induction
```

Neither route needs `Terminates` as a prerequisite for defining the function.

---

## 4.3 Unbounded minimization is the point where partiality enters

Unbounded minimization has the conceptual form:

```text
μ n. P n
```

with no finite upper bound known in advance.

Operationally:

```text
test 0
if false:
  test 1
if false:
  test 2
...
```

If no satisfying natural number exists, this process does not terminate.

This is qualitatively different from bounded minimization.

Adding unbounded minimization therefore introduces a computation that is meaningful to execute but is not globally guaranteed to terminate.

The expected architecture becomes:

```text
TOTAL FRAGMENT

bounded Nat minimization
structural recursion
well-founded recursion via Acc
        │
        ▼
ordinary total terms


PARTIAL FRAGMENT

unbounded μ
        │
        ▼
possibly divergent computation
        │
        ├── diverges
        │
        └── Terminates c
```

This is the intended future role in which a predicate such as `Terminates` becomes important.

---

## 4.4 `Terminates` should describe convergence of a partial computation

Under this interpretation:

```text
Terminates c
```

should not mean:

> the kernel grants permission to define `c`.

Instead it should mean:

> the already-defined potentially partial computation `c` reaches a value after finitely many computation steps.

That distinction is central.

For total recursion:

```text
termination proof
    → permits elaboration into total recursion
```

For unbounded minimization:

```text
partial computation exists first
    → a later theorem may prove Terminates
```

So `Acc` and `Terminates` occupy different logical roles.

### `Acc`

```text
Acc R x
```

is a structural/well-founded witness used to justify total recursive descent.

### `Terminates`

```text
Terminates c
```

is a convergence proposition about a computation that may otherwise diverge.

They should not be identified.

---

## 4.5 Relation to future `termination_by`

This distinction also clarifies a possible future meaning of `termination_by`.

For a total recursive definition:

```text
def f x := ...
termination_by m x
```

the annotation may elaborate to:

```text
measure
  ↓
well-founded relation
  ↓
Acc
  ↓
IADT induction
```

and no separate `Terminates` proposition is required.

By contrast, for an already-defined unbounded search:

```text
search := μ n. P n
```

one might prove:

```text
Terminates search
```

using some theorem that establishes a finite witness, bound, ranking argument, or other convergence certificate.

Thus:

```text
termination_by
```

should be viewed as a possible **proof method**, not as the definition of `Terminates` itself.

A proof of termination may come from many mathematical arguments that are not syntactically visible as a simple decreasing measure.

---

## 4.6 Example: proving an unbounded search terminates

Suppose:

```text
search P := μ n. P n
```

is partial.

If one later proves:

```text
∃ k : Nat, P k
```

then the unbounded search is known to terminate.

The proof need not say that the search was structurally recursive at definition time.

Instead it establishes after the fact that there is a finite successful index.

Conceptually:

```text
witness k with P k
      ↓
search examines only 0 ... k before success
      ↓
finite convergence witness
      ↓
Terminates (search P)
```

This is one reason `Terminates` should be more general than any particular `termination_by` annotation.

---

## 4.7 Preferred kernel treatment of `Terminates`

Even if `Terminates` is semantically necessary once unbounded minimization exists, it does not follow that it should be a primitive kernel predicate.

A terminating execution has a finite derivation.

Finite derivations are naturally representable by inductive data.

For example, if partial computation has a one-step relation:

```text
Step : Computation A -> Computation A -> @
```

and a final-state predicate:

```text
Returns : Computation A -> A -> @
```

then finite convergence can conceptually be represented by an IADT:

```text
Converges c v := {
  done :
    Returns c v ->
    * c v;

  step :
    (c' : Computation A) ->
    Step c c' ->
    * c' v ->
    * c v;
};
```

and:

```text
Terminates c :=
  exists v, Converges c v
```

The exact encoding depends on A Program's eventual representation of partial computations.

The architectural objective is:

```text
unbounded μ
      ↓
partial computation object

Terminates
      ↓
source-defined finite convergence witness

kernel
      ↓
checks only ordinary IADT evidence
```

rather than hard-coding "termination" as an opaque trusted judgment.

---

## 4.8 Why `PG_TERMINATION_*` remains an audit concern

The presence of:

```text
PG_TERMINATION_FORM
PG_TERMINATION_INTRO
```

should therefore not be criticized merely because a termination predicate exists.

A proposition of this kind is likely to be genuinely useful for future unbounded minimization.

The audit question is narrower:

> Does the semantic notion of finite convergence require these dedicated trusted rules, or can the same proposition be encoded using ordinary IADT machinery?

The desired long-term outcome, if possible, is:

```text
Terminates
  = ordinary logical/IADT construction

proof of Terminates
  = finite certificate

kernel
  = validates certificate structurally
```

while preserving `#Terminates` or similar syntax purely as elaboration sugar.

---

## 4.9 The kernel should check a certificate, not solve the halting problem

For unbounded minimization, determining whether an arbitrary computation terminates is undecidable.

Therefore `PG_TERMINATION_INTRO` must never amount conceptually to:

```text
run computation;
if it finishes:
    accept Terminates
```

as a foundational rule.

An elaborator or external proof procedure may execute computations opportunistically, but the final trusted result should ideally be a finite proof object that the kernel can check independently.

The intended pattern is:

```text
proof search / evaluator / automation
              ↓
       finite certificate
              ↓
       small trusted checker
```

not:

```text
trusted kernel
     ↓
attempt arbitrary execution
     ↓
hope it terminates
```

This distinction will become particularly important once unbounded `μ` is admitted.

---

# 6. Primary audit concern: dedicated termination evidence rules

The current evidence layer contains dedicated rules:

```text
PG_TERMINATION_FORM
PG_TERMINATION_INTRO
```

and surface forms such as:

```text
#Terminates ...
#terminates ...
```

are synthesized into those rules.

This deserves a separate audit.

## 4.1 Why this is a concern

If `Acc`-based termination can already be represented by ordinary IADT induction, then there are two conceptually distinct termination mechanisms:

```text
A. IADT / Acc route

IADT
 ↓
Acc
 ↓
well-founded recursion


B. Dedicated termination route

PG_TERMINATION_FORM
PG_TERMINATION_INTRO
 ↓
#Terminates
```

The existence of two mechanisms is not automatically wrong, because they may express different propositions.

However, if `#Terminates` represents something that could itself be encoded using ordinary types/IADTs, then retaining special kernel evidence rules would enlarge the trusted base unnecessarily.

The architectural question is therefore:

> Is `Terminates` genuinely a primitive logical notion, or merely a convenient derived proposition about computations?

This should be answered explicitly.

---

## 4.2 Required investigation

The following questions should be answered before treating `PG_TERMINATION_*` as permanent:

1. Can `Terminates t` be represented as a source-defined IADT or proposition?

2. Can introduction of `Terminates t` be derived from:
   - evaluation,
   - a logical relation,
   - an accessibility argument,
   - a totality witness,
   - or another ordinary proposition?

3. Does `PG_TERMINATION_INTRO` prove a fact that is impossible to express internally?

4. Does the rule inspect runtime behavior or evaluator state in a way ordinary IADT rules cannot?

5. Is `#Terminates` needed for soundness, or only for convenience and staging?

6. Can the same user-facing syntax remain while elaborating into an ordinary proposition?

The desired outcome, if technically possible, is:

```text
surface #Terminates
      ↓ elaboration
ordinary source-level proposition
      ↓
ordinary evidence / IADT machinery
```

rather than:

```text
surface #Terminates
      ↓
special trusted rule
```

---

# 7. `PG_INDUCTION_ELIM` is different from an `Acc` primitive

The presence of:

```text
PG_INDUCTION_ELIM
```

should not be conflated with dedicated support for accessibility or well-founded recursion.

If A Program treats IADTs as a fundamental language feature, then an elimination/induction rule for IADTs is expected to be part of the trusted type theory.

The desirable distinction is:

```text
acceptable kernel-level generality:
    IADT formation
    constructor introduction
    IADT elimination
    IADT induction

undesirable special cases unless proven necessary:
    Acc-specific elimination
    QuickSort-specific recursion
    measure-recursion primitive
    general-recursion primitive
    dedicated termination primitive
```

So far, `Acc` appears to use only the general mechanism.

That is a strong point of the present design.

---

# 8. Audit concern: the exact strength of `PG_INDUCTION_ELIM`

Although a general IADT induction rule is expected, it is still one of the most security-critical rules in the system.

The rule should be audited to guarantee that it cannot accidentally become an unrestricted recursion mechanism.

## 6.1 Required invariant

For each recursive constructor field, the branch should receive only the induction hypothesis justified by that field.

Conceptually:

```text
constructor:
  C : ... -> T smaller -> T current

branch receives:
  recursiveValue : T smaller
  recursiveIH    : P smaller recursiveValue
```

It should **not** receive an unrestricted function such as:

```text
∀ x, P x
```

unless such a function has already been independently justified.

For `Acc`, this distinction becomes:

```text
safe:
  down :
    ∀ y, R y x -> Result y

dangerous:
  recurse :
    ∀ y, Result y
```

The former is obtained through the recursive constructor structure.

The latter would amount to unchecked general recursion.

---

## 6.2 Audit questions

The implementation should be checked for:

- recursive fields hidden under dependent function types,
- recursive occurrences under nested IADTs,
- mutual recursion,
- negative occurrences,
- higher-order recursive fields,
- recursive occurrences under equality transport,
- recursive occurrences hidden by aliases or normalization,
- universe-polymorphic recursive fields,
- recursive occurrence detection after substitution.

The correctness of `Acc` is only one test case.

The induction machinery must be valid for arbitrary legal IADTs.

---

# 9. Audit concern: positivity

If IADTs are intended to carry the full burden of recursion, then **strict positivity becomes part of the termination story**.

A malformed inductive definition with a negative self-occurrence could encode nontermination or logical inconsistency.

For example, a shape analogous to:

```text
Bad := {
  bad : (Bad -> False) -> Bad
}
```

must not be admitted as an ordinary positive inductive declaration.

Therefore the kernel boundary cannot merely say:

> "All recursion comes from IADTs."

It must say:

> "All recursion comes from kernel-validated strictly positive IADTs and their valid eliminators."

The positivity checker is therefore part of the trusted termination mechanism.

This deserves explicit documentation.

---

# 10. Audit concern: executable fixed-point encoding in erased Core

The current erased implementation uses Lambda/Application-level self-application to implement recursive execution.

Conceptually:

```text
function = λ recursion. λ argument. body
unfold   = λ self. function (self self)
...
```

This is essentially a fixed-point encoding at the executable term level.

The design can still be sound because:

```text
Core execution capability
    ≠
typing permission
```

In other words:

> The runtime may be able to represent divergence even if the type system refuses to certify arbitrary divergent programs as total inhabitants.

This is a reasonable architecture.

However, the separation must be airtight.

---

## 8.1 Required invariant

No path should exist from an arbitrary erased recursive term to a total typed term without passing through validated IADT induction evidence.

The following implication must never hold merely because a Core term can be constructed:

```text
Core term exists
    ⇒
typed total term exists
```

Instead:

```text
typed total term
    ⇒
trusted evidence
    ⇒
valid IADT/induction structure
    ⇒
Core term
```

The direction matters.

---

## 8.2 Required adversarial tests

The test suite should intentionally attempt to:

- construct a self-application loop directly,
- smuggle it through a reference,
- hide it behind a lambda,
- hide it behind an IADT field,
- hide it under a dependent pair,
- pass it through an equality cast,
- package it behind an existential,
- re-import it through an artifact,
- exploit graph/witness machinery,
- exploit erased fields,
- exploit proof irrelevance.

Each such program should fail to obtain a total type unless an actual structural/well-founded argument exists.

---

# 11. Audit concern: runtime recursion must not become typing recursion

Because recursive execution is represented in the erased Core, it is especially important that evaluator behavior not leak into definitional equality in an unsound way.

Potential danger:

```text
type checker
   ↓ asks evaluator to normalize
evaluator
   ↓ unfolds unrestricted recursive term
general recursion enters conversion checking
```

If unrestricted runtime recursion participates in conversion without appropriate normalization guarantees, type checking may:

- diverge,
- cease to be decidable,
- or accidentally trust runtime behavior as proof.

The audit should therefore distinguish:

```text
trusted normalization used by typing
```

from:

```text
general execution after typing
```

If both use the same evaluator implementation, their allowed inputs or reduction policies must still be clearly separated.

---

# 12. Audit concern: source-defined `Acc` must not depend on hidden `Acc` semantics

It is not sufficient that the source code contains an IADT named `Acc`.

The audit should ensure that no implementation layer recognizes:

```text
"Acc"
"acc"
accessibility-shaped constructor signatures
```

and silently gives them privileged treatment.

The intended property is alpha/renaming invariance:

```text
Acc
```

should be replaceable by:

```text
Foo
```

with exactly the same behavior if the structure is identical.

A useful acceptance test is therefore:

> Rename the accessibility IADT and all of its constructors to unrelated names and confirm that all well-founded recursion tests still pass.

This verifies that the behavior arises from general IADT rules rather than name-based special casing.

---

# 13. Audit concern: relation and measure are library concepts

The kernel should ideally not understand:

```text
<
length
measure
well-founded order
```

as special termination concepts.

They should appear only through ordinary terms and proofs.

The target architecture is:

```text
surface:
  termination_by xs.length

elaboration:
  construct relation
  prove decrease obligations
  obtain well-foundedness
  obtain Acc witness
  generate IADT induction

kernel:
  sees only ordinary typed terms + IADT induction
```

This separation should be preserved even if later automation becomes sophisticated.

---

# 14. Audit concern: well-foundedness should remain proof-relevant only where necessary

There is a design choice around whether accessibility witnesses influence computation.

For a function such as QuickSort, different proofs of the same accessibility fact should ideally not change the observable result.

The audit should check whether:

```text
quickSortAcc xs accProof1
```

and:

```text
quickSortAcc xs accProof2
```

are definitionally equal, propositionally equal, or merely observationally equivalent.

If proof witnesses can affect runtime results unexpectedly, then proof erasure and proof irrelevance become relevant to the computational semantics.

This does not automatically indicate a bug, but the intended semantics should be explicit.

---

# 15. Audit concern: graph-based reasoning and termination must remain orthogonal

A Program also uses function graphs / witnesses for reasoning about function behavior.

The termination mechanism and the graph mechanism should not accidentally become mutually dependent.

The desirable decomposition is:

```text
termination:
  justifies that recursion is legal

graph:
  exposes the computational relation for later proofs
```

A graph witness should not be required merely to make recursion terminate.

Conversely, an `Acc` witness should not substitute for the graph needed to reason about the internal input/output relation of a function.

For QuickSort, the conceptual layers should remain:

```text
Acc
  → justifies recursive descent

function graph
  → records recursive computational structure

Sorted proof
  → reasons using graph + recursive IHs
```

This separation should be maintained because it allows future changes in proof representation without changing the trusted termination story.

---

# 16. Proposed target trust boundary

A minimal long-term trusted boundary could aim for something structurally like:

```text
Core type theory
├── universes
├── dependent functions
├── variables / references
├── equality machinery if primitive
└── IADT machinery
    ├── formation
    ├── constructors
    ├── positivity validation
    ├── dependent elimination
    └── induction
```

Then the following should ideally be derived outside that boundary:

```text
Nat recursion
List recursion
Acc
well-foundedness
measure-based recursion
QuickSort recursion
termination_by
function graphs
Sorted
#Terminates, if internally representable
```

Whether equality and other current primitives belong in the minimal kernel is a separate audit question and is outside the narrow termination scope of this document.

---

# 17. Recommended concrete tests

## 15.1 `Acc` genericity test

Define accessibility twice under unrelated names:

```text
Acc
AccessibleFoo
```

with isomorphic definitions.

Verify that both support the same well-founded recursive definitions.

---

## 15.2 No-name-special-casing test

Rename:

```text
Acc
acc
```

to arbitrary identifiers.

All tests should still pass.

---

## 15.3 Direct divergence rejection

Construct the erased equivalent of:

```text
loop x = loop x
```

and verify that it cannot receive a total ordinary type through any public typing route.

---

## 15.4 Negative recursive IADT rejection

Attempt a non-positive declaration and confirm rejection before an eliminator is generated.

---

## 15.5 Fake induction-hypothesis rejection

Attempt to manufacture:

```text
∀ y, Result y
```

where only a recursive-field-specific IH should be available.

---

## 15.6 Measure independence

Implement the same recursive function using:

- list length,
- a custom natural-number measure,
- lexicographic pairs,
- an arbitrary user-defined well-founded relation.

No kernel changes should be needed.

---

## 15.7 Termination-rule elimination experiment

Prototype a version of the relevant examples with:

```text
PG_TERMINATION_FORM
PG_TERMINATION_INTRO
```

disabled or bypassed.

Determine precisely which features fail.

For each failure, classify it as:

```text
A. true foundational requirement
B. elaboration convenience
C. evaluator contract
D. artifact/metadata requirement
E. legacy implementation dependency
```

This experiment would provide the strongest evidence about whether dedicated termination evidence belongs in the trusted base.

---

# 18. Design recommendation

For recursive function definitions, the preferred long-term strategy is:

```text
surface recursive syntax
        ↓
elaborator analyzes recursion
        ↓
structural recursion?
   yes ─────────→ direct IADT induction
   no
        ↓
user/synthesized well-founded relation
        ↓
source-defined Acc
        ↓
IADT induction
        ↓
ordinary typed Core
```

The kernel should not need separate concepts named:

```text
structural recursion
well-founded recursion
measure recursion
QuickSort recursion
```

if all of them can be reduced to general IADT induction.

The phrase "IADT-only kernel" should nevertheless be understood carefully:

> It does not mean that the kernel merely stores IADT declarations.

It means that the trusted kernel contains the **general rules necessary to validate IADTs and their eliminators**, including positivity and the constraints that make induction structurally sound.

Those general rules are the real foundation on which `Acc`-based termination rests.

---

# 19. Current audit conclusion

The current QuickSort/`Acc` architecture is compatible with the intended minimal-kernel direction.

The evidence presently supports the following conclusion:

```text
Acc itself does not need to become a primitive.
Well-founded recursion does not need to become a primitive.
Measure-based termination does not need to become a primitive.
```

The two highest-priority unresolved issues are:

1. **Determine whether `PG_TERMINATION_FORM` and `PG_TERMINATION_INTRO` can be removed from the trusted layer or derived from ordinary propositions/IADTs.**

2. **Audit `PG_INDUCTION_ELIM` and recursive-occurrence detection as part of the actual trusted termination foundation.**

The conceptual trust chain should ultimately be demonstrable as:

```text
strictly positive IADT
        ↓
sound dependent induction
        ↓
source-defined Acc
        ↓
well-founded recursion
        ↓
user-level recursive programs
```

If that chain can be made complete without additional termination primitives, then A Program can support expressive general recursion while keeping the kernel centered on a single general inductive mechanism rather than accumulating special-purpose recursion rules.
