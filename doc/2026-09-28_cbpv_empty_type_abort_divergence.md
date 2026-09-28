# Empty type, abort and divergence: current-review addendum

Date: 2026-09-28
Baseline: `e7162320712f1acdc9420b6ffae993cd97035663`.
Tracking: #43; backend implications tracked in #44.

## Problem List

1. P1 — Keep empty elimination, abnormal termination and divergence distinct when choosing a recursion result classifier.

## P1. Clarify the logical and operational boundary

### Subjective (User)

The user requested submission of the three supplied documents and an Open Question on loop introduction (2026-09-28, English paraphrase).
This note supports that question; it does not establish approval of a particular empty-type syntax or runtime abort primitive.

### Objective (Code)

At the pinned revision, classifier.h distinguishes totality from effects.
pg_prove_termination in evidence.c still requires TOTAL; pg_prove_total_pure_value requires TOTAL and an empty row.
The execution and typed/scoped-image checks described in the companion backend audit passed.
No empty-IADT source example, general-recursion implementation or new no-return classifier was tested in this submission.

### Assessment

The supplied note correctly distinguishes the impossible premise required by empty elimination from computational non-return.
One qualification is required: its §6 description of a computation returning the empty type as necessarily nonterminating assumes a safe setting with no alternative failure/effect exit. In a richer runtime, no normal return does not alone imply infinite execution.
Likewise the value/proof consistency statement is relative to a consistent context/signature without an axiom supplying a closed empty value.

A suspended diverging computation can be a value at a thunk type without being a value at its result type. The safety rule must constrain forcing and total-result projection, not ban all values containing suspended computation.

The canonical empty-value result, a distinct no-return classifier and ordinary polymorphic divergence are different design choices. CBPV's value/computation distinction does not select A Program's synthesis rule for them.

Levy's computation-level recursion rule was spot-checked in §5.3.1 of the [primary thesis](https://www.cs.bham.ac.uk/~pbl/papers/thesisqmwphd.pdf). Other citations remain supplied research references rather than newly audited claims.

### Plan

- [ ] Resolve the no-normal-return policy under #43, not through a separate runtime-abort shortcut.
- [ ] Specify empty elimination and effect failure independently.
- [ ] Add logical negative controls and operational handler/exit tests when implementing recurrence.
- [ ] Carry those distinctions into C lowering under #44.
Completion: explicit rules establish what can return, fail or recur, without making nontermination an inhabitant of logical falsity.


## Supplied research record and precedence

Tracking issues: #43 (recursion) and #44 (Artifact-to-C).
The current-review addendum above takes precedence over unpinned or broader historical statements below. Original text is preserved; its proposed names, pseudocode and phase ordering are not accepted implementation. Historical user attributions remain attributed to the supplied record. Original SHA-256: `8c2fe15e51c2ada57bb53a3e36a75348eb35fc3f1defd23e588b8f6313241308`.

## Original supplied document

# Empty Type, `abort`, and Divergence in a CBPV-Oriented Design

**Date:** 2026-09-28  
**Status:** Research / design note for A Program

## Question

When A Program eventually admits nontermination or general recursion, should this be represented by
the empty type `0` / its eliminator (`abort`), or should nontermination live separately in a
computation layer?

The conclusion of this note is:

> **Keep logical emptiness (`0`) and computational nontermination distinct.**
> `0` belongs to the value/proof structure; divergence and general recursion should be introduced
> only for computations. Runtime failure/exception is likewise a computational effect and should
> not be identified with empty-type elimination.

The important qualification is that this conclusion does **not** follow from polarization alone.
A polarized calculus can still be extended badly. The key additional design discipline is:
**general recursion/nontermination is admitted only in the computation judgement, not in the
value/proof judgement.**

---

## 1. Three notions that must not be conflated

There are three superficially similar operations:

1. **Empty-type elimination**
2. **Runtime abort / exception**
3. **Divergence**

They have different meanings.

### 1.1 Empty-type elimination

Let `0` be an empty value type. Its eliminator has the schematic form

```text
Γ, x : 0 ⊢ abort(x) : B
```

or, using an empty pattern match,

```text
Γ, x : 0 ⊢ match x {} : B
```

The crucial point is that this rule requires an input `x : 0`.

It therefore says:

> If an impossible value has somehow been supplied, any branch may be concluded.

It does **not** by itself provide a closed term

```text
⊢ abort : B
```

and it does not describe a process which runs and then fails.

In Levy's basic CBPV, value types include finite sums. Taking the indexing set to be empty gives
the empty value type. Elimination of a sum produces a **computation**, so the empty case is exactly
the CBPV analogue of empty elimination.

---

### 1.2 Runtime abort / exception

A primitive such as

```text
abort_runtime : B
```

that may be executed without first possessing a value of type `0` is categorically different.

It is an operation of the computation language: execution terminates abnormally instead of
producing the expected result. In CBPV terminology this belongs with computational effects
(errors/exceptions), not with logical empty-type elimination.

Thus:

```text
empty elimination : requires x : 0
runtime abort     : can be initiated as a computation
```

These should not share semantics merely because both are sometimes called "abort".

---

### 1.3 Divergence

Divergence is different again:

```text
Ω : B
```

means a computation of result type `B` that never reaches a result.

Paul Blain Levy's CBPV explicitly extends the **computation judgement** with both divergence and
general recursion. In Chapter 5 of his thesis the rules are, schematically,

```text
──────────────
Γ ⊢c diverge : B
```

and

```text
Γ, x : U B ⊢c M : B
────────────────────
Γ ⊢c μx.M : B
```

with `diverge` definable from recursion as essentially `μx. force x`.

The recursive variable is a **thunked computation** (`x : U B`), rather than an arbitrary value of
the result type. Operationally, unfolding recursion substitutes a thunk of the recursive
computation back for `x`.

So the intended reading is not

```text
Ω : Nat
```

as a mysterious nonterminating *Nat value*, but rather something like

```text
Ω : F Nat
```

— a computation intended to produce a `Nat`, which never does so.

---

## 2. Why this distinction matters logically

Suppose unrestricted general recursion were admitted directly into the value/proof fragment with
a rule morally equivalent to

```text
Γ, x : A ⊢ V : A
────────────────
Γ ⊢ fix x.V : A
```

Then for every `A` one could form the nonterminating self-reference

```text
fix x.x : A
```

and in particular

```text
fix x.x : 0
```

Under an ordinary Curry–Howard reading this destroys the intended meaning of `0` as an
uninhabited proposition: syntactically there is now a closed term of `0`, even though evaluation
never produces a canonical inhabitant.

This is the familiar tension between unrestricted general recursion and treating the entire
programming language simultaneously as a normalizing proof theory. Work on languages combining
dependent types with general recursion therefore has to separate or otherwise control the
logically sound fragment.

CBPV provides a particularly clean place to draw this boundary because it already distinguishes
values from computations.

The desired invariant is:

```text
closed value/proof of 0      : impossible
closed diverging computation : permitted
```

or symbolically,

```text
¬(⊢v V : 0)

but

⊢c Ω : B
```

for suitable computation types `B`.

---

## 3. This is not merely "because CBPV is polarized"

CBPV has two classes of types and judgements:

```text
Γ ⊢v V : A       value
Γ ⊢c M : B       computation
```

and later polarized formulations often describe positive/value types and negative/computation
types in closely related terms.

However, **polarization by itself does not force divergence to occur only on the computation
side**. One could design a polarized calculus and then add unrestricted recursion to both
judgements.

The relevant fact about Levy's CBPV is stronger and more specific:

1. the basic value/computation calculus is separated;
2. effects are added as extensions of the computation language;
3. in particular, `diverge` and term-level `μ` are introduced as computation constructs.

Therefore the useful design lesson for A Program is not simply

> "use polarized types",

but rather

> **use the value/computation boundary as the semantic boundary at which partiality is introduced.**

This allows polarity to organize syntax while the computation layer carries the operational
phenomena that may prevent production of a value.

---

## 4. Why `0` and divergence are semantically different

The distinction can be summarized as follows.

| Concept | Meaning | Requires an impossible premise? | Can exist closed? | Operational event? |
|---|---|---:|---:|---:|
| `0` | no value exists | — | no closed canonical value | no |
| empty elimination | eliminate `x : 0` | yes | not without a source of `0` | no |
| runtime abort/error | computation fails | no | yes | yes |
| divergence | computation never returns | no | yes | yes |

The key conceptual sentence is:

> **`0` says that there is no possible value; divergence says that a computation fails to produce
> a value.**

These statements are not equivalent.

A computation can therefore have a perfectly ordinary result type while diverging:

```text
M : F Nat
```

without adding any extra inhabitant to `Nat`.

Likewise a suspended diverging computation can itself be an ordinary value:

```text
thunk Ω : U (F Nat)
```

The thunk is a value; forcing it initiates the nonterminating computation. This makes the boundary
explicit rather than representing partiality by contaminating every value type with an implicit
bottom value.

---

## 5. Consequence for A Program

A conservative design direction is therefore:

### Value / proof layer

Keep ordinary inductive and logical types, including `0`, free of general recursion.

```text
0        -- genuinely empty
Nat      -- canonical natural-number values
List A   -- canonical finite lists, etc.
```

Empty elimination remains logical elimination:

```text
x : 0 ⊢ match x {} : ...
```

It should not mean "terminate the process".

### Computation layer

If A Program later introduces unrestricted recursion, minimization, potentially nonterminating
search, or other partial operations, introduce them as **computation-level constructs**.

Schematically:

```text
loop    : Comp A
fix     : (...) -> Comp A
μ       : ...
```

where the exact surface syntax and type constructors remain an A Program design choice.

A computation may promise an `A` as its result type yet fail to return one:

```text
search : Comp Nat
```

This does not imply that `Nat` itself contains a bottom or failure value.

### Runtime abort should be separate again

If A Program needs process termination, exception-like failure, assertion failure, or an
unrecoverable runtime trap, model that explicitly as a computation effect or operational
construct. Do not derive it merely from `0`.

This leaves three independently controllable mechanisms:

```text
0 / empty elimination     logical impossibility
failure / exception       abnormal computation
divergence / recursion    nonterminating computation
```

That separation should make later lowering to C, CUDA C, Verilog, or another execution model
considerably less ambiguous: logical impossibility need not be assigned the same lowering as
runtime failure or an infinite loop.

---

## 6. Proposed audit invariant

When adding nontermination to A Program, the implementation should be audited against the
following invariant:

> **No construct introduced solely to express general recursion or divergence should make a closed
> value/proof of `0` constructible.**

Equivalently, the extension should permit something analogous to

```text
⊢c Ω : Comp 0
```

while still rejecting

```text
⊢v Ω : 0
```

The former says "there is a computation that would return an impossible value if it terminated,
and therefore it does not terminate." The latter would turn nontermination into an inhabitant of
logical falsity.

This is the principal reason to keep the two mechanisms separate.

---

## 7. References

1. **Paul Blain Levy**, *Call-By-Push-Value*, PhD thesis, Queen Mary and Westfield College,
   University of London, 2001.  
   - §3.2: separate value and computation types/judgements.  
   - §5.3, especially §5.3.1 "Divergent and Recursive Terms": `diverge` and `μ` are added as
     computation terms; `diverge` is definable using recursion.  
   - The thesis explicitly treats divergence, errors, store, etc. as computational effects.  
   https://www.cs.bham.ac.uk/~pbl/papers/thesisqmwphd.pdf

2. **Paul Blain Levy**, *Call-By-Push-Value: A Functional/Imperative Synthesis*,
   Springer, 2004. Chapter 5: "Recursion and Infinitely Deep CBPV".  
   https://link.springer.com/book/10.1007/978-94-007-0954-6

3. **Zeeshan Lakhani, Ankush Das, Henry DeYoung, Andreia Mordido, Frank Pfenning**,
   "Polarized Subtyping", ESOP 2022.  
   The paper describes the CBPV-inspired polarized separation between observable values and
   computations and studies recursive computations using step-indexed semantics.  
   https://doi.org/10.1007/978-3-030-99336-8_16

4. **Garrin Kimmell et al.**, "Equational reasoning about programs with general recursion and
   call-by-value semantics", *Progress in Informatics* 10, 2013.  
   Discusses the tension between dependent type theory/Curry–Howard logical soundness and
   programming features such as unrestricted general recursion, motivating explicit separation or
   control of such features.  
   https://www.nii.ac.jp/pi/n10/10_19.html

---

## Short design conclusion

For A Program, the most useful lesson from CBPV is not merely the existence of two polarities.

It is this stricter boundary:

```text
VALUE / PROOF
    0 is empty
    no unrestricted general recursion
         │
         │ thunk / force or an equivalent explicit boundary
         ▼
COMPUTATION
    may diverge
    may use general recursion
    may later carry failure, state, IO, etc.
```

Therefore **empty-type elimination is a logical rule, while divergence is a computational
effect**. Keeping these independent allows A Program to add partial computation without turning
nontermination into a proof of arbitrary propositions.
