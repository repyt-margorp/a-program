# Open question: computation-level general recursion and loop syntax

Date: 2026-09-28
Fresh baseline: `main` at `e7162320712f1acdc9420b6ffae993cd97035663` (independent clone under a-book; no implementation edits).
Status: proposal/review, not implemented by this submission.

## Problem List

1. P1 — Add potentially infinite computation without weakening the value/proof boundary, induction, or synthesis-first typing.

## P1. General recursion at the computation boundary

### Subjective (User)

English paraphrase of the current 2026-09-28 request: open a discussion issue for introducing infinite-loop syntax and the supporting system. Submit the two supplied recursion/empty-type notes together with the backend audit in one documentation PR.
Earlier user statements quoted in those documents are supplied-document provenance, not independently reconstructed conversation records. No final token or operational rule is approved here.

### Objective (Code)

- README keeps `*k` induction-derived, declaration names non-recursive, and `::` post-synthesis.
- `src/classifier.h:25` separates effect rows and totality. TOTAL assumes admissible inputs and returning effect interpretations; UNSPECIFIED is not evidence of divergence.
- `src/evidence.c:3462` (`pg_prove_termination`) still requires an already-TOTAL suspended computation. This is not a post-hoc proof principle for arbitrary partial computation.
- `src/evidence.c:4121` (`pg_prove_total_pure_value`) requires TOTAL and an empty effect row.
- The inspected CLI/source contracts do not provide the proposed AGAIN construct. The notes describe a proposed extension, not a demonstrated current compiler defect.
- Fresh `make -s -j3 check-execution` passed: checked entry, handlers, repeat/split invocation, cancellation, source/image execution, exact bytes, effect order and I/O failure.
- Existing typed/scoped transport tests also passed; these do not test new recursion syntax.

### Assessment

Recommended architectural boundary: add general recurrence on the computation side, preserve the existing total IADT/Acc recursion, and keep potentially partial forcing out of dependent conversion and proof introduction.

The supplied notes leave important choices open:

1. Is self an explicit thunk or an implicit force operation? Which lexical binder introduces it? Does recurrence permit non-tail calls, or only tail iteration?
2. What environment is reused? Re-entering a closed thunk captures the same lexical values; rebinding a local variable does not automatically update the next iteration. Define loop-carried arguments/state and nested-loop targeting explicitly. The changing-n examples in the report are pseudocode, not an implemented state discipline.
3. How do normal-return paths determine result type while effects and totality are synthesized separately? AGAIN is only a placeholder.
4. Is a result-less computation classified using an ordinary empty value type, a separate no-return classifier, or another explicit mechanism? Preserve post-synthesis `::`; do not introduce hidden expected-type retagging.
5. How are effect handlers, resumptions, cancellation and observable traces treated? UNSPECIFIED alone must not determine any of these.
6. Is termination recovery elaboration into existing Acc/induction, or genuine post-hoc totalization? The latter needs a new sound evidence rule, not removal of the existing guard.
7. What representation supports operational unfolding without introducing unguarded proof-level cycles or requiring a new Core node prematurely?

Empty elimination requires a value of the empty type; runtime failure and divergence do not. A computation with empty normal-result type cannot normally return in a sound model, but may fail or exit through effects. It entails divergence only with extra assumptions excluding such outcomes. A thunk of a partial computation is a value, but not a proof that forcing it terminates.

Primary-source spot-check: Levy's thesis §5.3.1 adds divergence/recursion as computation constructs with a thunked self binder. It does not choose A Program's synthesis policy or empty-result convention. [Levy thesis](https://www.cs.bham.ac.uk/~pbl/papers/thesisqmwphd.pdf).
The supplied wider bibliography was not exhaustively reverified.

### Plan

- [ ] Agree on operational rules and lexical/state behavior before choosing syntax.
- [ ] Specify result/effect/totality synthesis and branch joining.
- [ ] Specify conversion and logical consistency invariants.
- [ ] Design any termination-recovery rule separately.
- [ ] Prototype and test pure recurrence, printing recurrence, conditional return, changing loop state, nested recurrence and handler interaction.
- [ ] Add negative tests: no closed empty proof from divergence, no UNSPECIFIED-to-TOTAL cast, no contextual rewrite of synthesized type through `::`.
- [ ] Test bounded execution/cancellation without calling a budget exhaustion proof of divergence.

Completion for this Open Question: reviewed rules, explicit decisions and a scoped implementation plan. Documentation acceptance alone does not implement recursion.

## Supplied research record and precedence

Tracking issues: #43 (recursion) and #44 (Artifact-to-C).
The current-review addendum above takes precedence over unpinned or broader historical statements below. Original text is preserved; its proposed names, pseudocode and phase ordering are not accepted implementation. Historical user attributions remain attributed to the supplied record. Original SHA-256: `27715af0597937eb78053c0c83b6ae309484c5943a9776a872bb2109a107028d`.

## Original supplied document

# General Recursion at the CBPV Boundary: Design Audit

Date: 2026-09-27  
Status: design audit / unresolved  
Code baseline: `main` as inspected on 2026-09-27. The repository reports the promoted pointer core in `src/`; the exact HEAD SHA was not available through the read-only retrieval used for this audit and must be pinned before implementation.  
Related: `README.md`, `src/classifier.h`, `src/evidence.c`, `src/evidence.h`, existing IADT induction and `Acc` support.

Revision note: expanded after the 2026-09-27 discussion of result-less recurrence. The leading direction now treats effect inference, totality, and normal-return result synthesis as separable questions and investigates an empty result type for computations with no normal-return path.

## Problem List

| ID | Problem | Issue / PR | Status |
| --- | --- | --- | --- |
| P1 | Preserve the meaning of `*k` while adding general recursion | — | design constraint |
| P2 | Decide where non-terminating / potentially non-terminating computation belongs | — | strong design conclusion: CBPV computation layer |
| P3 | Determine a surface form for computation-level self recurrence (`AGAIN`) | — | unresolved |
| P4 | Define what information `AGAIN` contributes to classifier synthesis | — | direction revised |
| P5 | Define the relation among `AGAIN`, `termination_by`, `Terminates`, and total result projection | — | unresolved |
| P6 | Prevent general recursion from contaminating dependent definitional equality | — | required invariant |
| P7 | Decide how to classify computations with no normal-return path (`0`, `NoReturn`, or another computation-level result notion) | — | unresolved subordinate design issue |

---

## P1. Preserve the meaning of `*k` while adding general recursion

### Subjective (User)

* User position, 2026-09-27, paraphrased: A Program's current recursion model is intentionally not ordinary named self-recursion. Surface recursion is constrained through IADT elimination, where `*k` denotes the induction result for a recursive field.
* User position, 2026-09-27, paraphrased: General recursion should not automatically imply adopting the conventional syntax `f := ... f ...`.
* User position, 2026-09-27, paraphrased: The existing finite recursion mechanism should remain conceptually distinct from any mechanism introduced for infinite or potentially infinite computation.

### Objective (Code)

* `README.md` states that `*k` is the induction result for recursive field `k`, not an unrestricted recursive call.
* `README.md` also states that a declaration's own name is not an implicit recursive alias.
* Current indexed induction includes source-defined `Acc`, so non-structural but well-founded recursion can already be expressed without unrestricted self-recursion.
* Core remains deliberately small: Lambda, Application, and Reference. Typing and CBPV value/computation distinctions live above erased Core.

### Assessment

A Program currently has a strong and useful distinction:

```text
IADT recursive field k
        |
        v
       *k
        |
        v
induction result already justified by the eliminator
        |
        v
finite / well-founded recursion
```

This should not be weakened by redefining `*k` as a general recursive-call syntax.

The phrase "finite recursion" should also be used carefully. IADT induction plus `Acc` is broader than bounded minimization. Bounded search is one program that can be expressed in the total fragment; the fragment also contains ordinary structural recursion and arbitrary well-founded recursion.

A future general-recursion mechanism should therefore be a **second recursion principle**, not an extension of the meaning of `*k`.

### Plan

* [ ] Preserve `*k` exclusively as induction-derived evidence/result.
* [ ] Do not make a declaration name implicitly recursive.
* [ ] Specify general recursion independently at the CBPV computation level.
* [ ] Add negative tests ensuring any future general-recursion syntax cannot be confused with `*k`.
* Completion: existing IADT/`Acc` programs retain their present meaning and totality behavior without relying on a new general-recursion rule.

---

## P2. Put general recursion on the computation side of CBPV

### Subjective (User)

* User position, 2026-09-27, paraphrased: The dependent type / IADT side already provides the intended finite recursion mechanism; a separate mechanism may be required on the CBPV side for unbounded or infinite computation.
* User position, 2026-09-27, paraphrased: The relevant primitive need not be a named recursive function. What is needed is some way for a computation to invoke the computation itself again.

### Objective (Code)

`src/classifier.h` currently separates totality from the operation row:

```c
enum pg_totality {
    PG_TOTALITY_UNSPECIFIED,
    PG_TOTALITY_TOTAL
};
```

The comments state:

* `TOTAL` requires finite computation for admissible inputs and returning interpretations of effects.
* `UNSPECIFIED` is **not** proof of divergence.
* The totality contract is independent of the operation row.
* Purity does not discharge totality.
* `pg_thunk_type(...)` already exists for suspended computations.

This gives a natural target for a general recursive computation:

```text
Comp(UNSPECIFIED, E, A)
```

rather than adding a `Divergence` algebraic effect to `E`.

### Assessment

The useful CBPV separation is:

```text
Dependent value / proof side
    structural or well-founded recursion
    strong normalization required for logical use

CBPV computation side
    potentially unbounded evaluation
    general recursion / fixed points may live here
```

The general-recursion primitive should be understood as a **computation fixed point**, not an algebraic effect and not a new proof-level recursion rule.

At the semantic level, the standard CBPV shape is approximately:

```text
self : U C
----------------
body : C

fix/self(body) : C
```

where `self` is a thunk of the enclosing computation and forcing it resumes/restarts that computation.

For A Program, the important additional grading is that a computation using unrestricted self recurrence should not synthesize `TOTAL` merely by construction. Its default result contract should be `UNSPECIFIED`.

### Plan

* [ ] Formalize a computation-level fixed-point rule independently of the surface syntax.
* [ ] Require the unrestricted rule to produce an `UNSPECIFIED` computation contract.
* [ ] Keep effect rows orthogonal to totality.
* [ ] Decide whether the recursive capability is represented explicitly as a thunk (`U C`) internally even if the surface hides `force`.
* Completion: the calculus can express a potentially diverging computation without adding general recursion to proof/value normalization.

---

## P3. Surface placeholder: `AGAIN`

### Subjective (User)

* User position, 2026-09-27, paraphrased: Use the placeholder spelling `AGAIN` for discussion; no commitment to the final token or operator spelling is intended.
* User position, 2026-09-27, paraphrased: A plausible surface design is an operator available only inside a computation block `{ ... }`, referring to the enclosing computation rather than to a definition name.

### Objective (Code)

Current surface syntax already has sequential computation blocks:

```text
{ x := M; N; }
```

and quotation:

```text
&M
```

The current language does not provide an implicit recursive alias for the enclosing declaration.

### Assessment

The most A-Program-specific surface idea discussed so far is:

```text
{
    ...
    AGAIN
    ...
}
```

with an intended meaning roughly equivalent to:

```text
"resume/re-enter this enclosing computation"
```

This can hide the conventional fixed-point binder:

```text
fix (self => body)
```

and preserve the existing decision not to make definition names recursively visible.

Two semantic interpretations must be distinguished:

1. **`AGAIN` is the self thunk**  
   Internally, `AGAIN : U C`; explicit forcing is required.

2. **`AGAIN` is the recursive action**  
   The surface token directly means "force the self thunk"; internally the compiler introduces the `U C` self capability.

The second form is probably easier to read, but the first is closer to the raw CBPV rule. No choice is approved yet.

Nested computation blocks also require a lexical rule. The natural candidate is:

> `AGAIN` refers to the nearest enclosing computation block that introduces a recursive capability.

However, automatically making every `{ ... }` recursively self-referential may interact badly with inference and should not be assumed before P4 is solved.

### Plan

* [ ] Treat `AGAIN` only as a discussion placeholder, not accepted syntax.
* [ ] Compare explicit-self-thunk and implicit-force surface interpretations.
* [ ] Define lexical scoping for nested computation blocks.
* [ ] Decide whether recursion is enabled for every computation block or only for an explicitly marked recursive computation.
* Completion: one surface proposal has an unambiguous elaboration into the fixed-point rule and does not introduce named self-recursion.

---

## P4. `AGAIN` should probably contribute no result-type information

### Subjective (User)

* User concern, 2026-09-27: "`AGAIN` alone inside a computation may not have a type."
* Follow-up user position, 2026-09-27, paraphrased: this may be the correct behavior. The more useful question is not how to force an arbitrary result type onto `AGAIN`, but what programs containing `AGAIN` should synthesize naturally.
* Follow-up user position, 2026-09-27, paraphrased: assigning an arbitrary expected type such as `Nat` to a non-returning computation feels syntactically and semantically wrong for A Program's term interpretation; a dedicated bottom/empty result type appears more plausible.

### Objective (Code)

`README.md` states that `::` is a **post-synthesis check**:

> it never supplies an expected type to guide synthesis.

The same README documents a closely related current limitation: a standalone indexed Match whose every branch is refuted may remain pending when no application result constraint exists, and a trailing `::` does not supply that constraint.

`src/classifier.h` defines the computation classifier as three conceptually separable pieces:

```text
Comp(totality, effect-row, value-type)
```

where totality is explicitly independent of the operation row.

### Assessment

The previous version of this audit described `{ AGAIN; }` primarily as a circular inference failure:

```text
C = C
```

That observation remains technically useful, but it is no longer the preferred semantic interpretation.

A better decomposition is:

```text
AGAIN

result value type : contributes no new information
effect row        : contributes no new operation by itself
totality          : prevents a construction-only claim of TOTAL
```

In other words, `AGAIN` is not expected to *synthesize* the normal-return value type of the enclosing computation.

This makes the following examples distinct.

#### Base-returning recurrence

```text
{
    if done then
        return n
    else
        AGAIN
}
```

The normal-returning branch contributes `Nat`. `AGAIN` contributes no competing value type. The candidate classifier is therefore:

```text
Comp(UNSPECIFIED, E, Nat)
```

where `E` is inferred from the body.

#### Effectful recurrence with no normal return

```text
{
    #print "tick";
    AGAIN
}
```

The body contributes:

```text
totality : UNSPECIFIED
effects  : {Print}
result   : no normal-return witness/type information
```

The remaining design question is whether the absence of all normal-return paths should synthesize a canonical empty result type. P7 argues that this is a promising direction.

### Why "check `AGAIN` at any expected type" is not the preferred A Program design

In ordinary type systems, divergence is often bottom-polymorphic: a diverging expression can inhabit any expected result type because it never produces a value. Standard CBPV also gives `diverge` at an arbitrary computation type.

That does **not** imply that A Program should make the surface term:

```text
{ AGAIN; }
```

silently become a `Nat` computation merely because a context happens to ask for `Nat`.

A Program currently emphasizes synthesis from the term itself, and its `::` annotation intentionally does not feed an expected type into synthesis. Reinterpreting an otherwise result-less computation as any expected result type would therefore be a substantial change in surface typing philosophy.

The current leading direction is instead:

* infer effects independently;
* infer/grade totality independently;
* infer the normal-return value type from actual normal-return paths;
* if there are no normal-return paths, investigate a canonical empty result type (`0`) rather than arbitrary contextual retagging.

### Plan

* [ ] Give `AGAIN` no result-type synthesis rule of its own.
* [ ] Specify the totality constraint introduced by `AGAIN` (`UNSPECIFIED` unless a separate total-recursion elaboration proves otherwise).
* [ ] Specify how branch/block synthesis collects the types of actual normal returns independently from `AGAIN`.
* [ ] Verify whether the current source synthesizer can aggregate result information from base branches without introducing a general expected-type mode.
* [ ] Treat arbitrary expected-type polymorphism for `AGAIN` as a separate rejected-or-deferred design, not the default.
* Completion: examples with base returns synthesize their result type from those returns, while result-less recursive computations follow the explicit P7 policy rather than an accidental inference cycle.

---

## P5. `termination_by`, `Terminates`, and totalization are different mechanisms

### Subjective (User)

* User question, 2026-09-27, paraphrased: If a computation can express an infinite loop through `AGAIN`, can a Lean-like `termination_by` mechanism later turn it into a value/total computation?
* User design direction from the discussion: termination reasoning should be able to recover totality where justified, but it is not yet decided whether this is elaboration-time or post-hoc.

### Objective (Code)

Current code already has:

```text
Comp(TOTAL, E, A)
Comp(UNSPECIFIED, E, A)
```

and directed weakening from `TOTAL` to `UNSPECIFIED`.

`src/evidence.h` explicitly says totality weakening never strengthens an unknown contract.

`src/classifier.h` also defines:

```text
Terminates(suspended)
```

through termination-type and termination-witness constructors.

However, the current proof constructor in `src/evidence.c` only introduces a termination witness if the suspended computation is **already `TOTAL`**:

```c
if (totality != PG_TOTALITY_TOTAL) return NULL;
```

Therefore the current `Terminates` machinery is **not yet** a proof principle for taking an arbitrary `UNSPECIFIED` general-recursive computation and proving that it terminates.

The current code also has total/pure result projection: a computation can be projected to a value only when it is `TOTAL` and its effect row is empty.

### Assessment

Two different mechanisms should not be conflated.

#### A. Lean-like `termination_by`

A recursive-looking source program is accepted as total only after proving that every recurrence decreases under a well-founded relation.

For A Program this could elaborate:

```text
AGAIN + termination_by measure
```

into existing total machinery such as `Acc` / IADT induction.

Conceptually:

```text
recursive-looking source
        |
termination_by / decrease proof
        |
        v
well-founded recursion
        |
        v
Comp(TOTAL, E, A)
```

In this route, the accepted term need never become a genuine `UNSPECIFIED` fixed point.

#### B. Post-hoc totalization

A genuine general-recursive computation is first accepted as:

```text
M : Comp(UNSPECIFIED, E, A)
```

Then a separate proof establishes:

```text
p : Terminates(&M)
```

and an explicit totalization principle would derive something like:

```text
totalize M p : Comp(TOTAL, E, A)
```

This principle **does not exist in the current implementation**.

It also should not be implemented as an unchecked classifier cast. It requires a sound semantic/evidence rule explaining how the termination witness turns a partial computation into a total one.

Finally, only if `E` is empty may the existing total/pure projection yield an ordinary value `A`.

Thus:

```text
Comp(UNSPECIFIED, E, A)
          |
      termination proof
          v
Comp(TOTAL, E, A)
          |
       E = {}
          v
          A
```

The effect row is not erased by a termination proof.

### Plan

* [ ] Keep `termination_by` conceptually separate from post-hoc `Terminates`.
* [ ] If `termination_by` is added, first investigate elaboration into existing `Acc`/IADT total recursion.
* [ ] Design a separate proof rule before allowing `Terminates` to totalize `UNSPECIFIED` computations.
* [ ] Do not remove the current `TOTAL` guard from `pg_prove_termination` without a replacement soundness argument.
* [ ] Specify whether `totalize` is a computation constructor, an evidence-level coercion, or another explicit boundary.
* [ ] Verify that non-empty effects survive totalization.
* Completion: the language distinguishes "proved total during recursive elaboration" from "general computation proved terminating afterward", and neither route permits an unproved `UNSPECIFIED -> TOTAL` strengthening.

---

## P6. General recursion must not enter dependent definitional equality

### Subjective (User)

* User design direction, 2026-09-27, paraphrased: finite recursion belongs naturally to the dependent/IADT side, while potentially infinite computation appears to require an additional CBPV-level mechanism.

### Objective (Code)

The implementation keeps typed occurrences and classifiers above the erased Core graph. The current dependent fragment relies on bounded checking/normalization and rejects unfinished computation as proof.

### Assessment

If arbitrary general-recursive computation can participate in definitional equality used to type-check dependent types, type checking itself may diverge.

The intended firewall should therefore be approximately:

```text
dependent types / proofs
    depend on values and accepted total computation results

general-recursive computation
    remains behind the CBPV computation boundary
    and is not definitionally unfolded as proof evidence
```

A thunk of an `UNSPECIFIED` computation may be a value in the CBPV sense, but this alone must not imply that forcing or normalizing the suspended computation is allowed during dependent conversion.

This is the same reason that:

```text
Comp(UNSPECIFIED, {}, False)
```

must not become a proof of `False`.

### Plan

* [ ] State the conversion invariant for `UNSPECIFIED` computations before implementing general recursion.
* [ ] Add negative tests showing that a looping computation cannot establish an arbitrary proposition.
* [ ] Define exactly what quotation/thunk equality is available without forcing the underlying computation.
* [ ] Keep proof normalization bounded to the accepted total fragment.
* Completion: general recursion can run operationally without making dependent conversion or proof checking non-terminating.

---

## P2A. Strong conclusion: unrestricted recurrence belongs on the CBPV computation side

### Subjective (User)

* User position, 2026-09-28, paraphrased: regardless of the unresolved `0`/`abort` question, infinite loops and unrestricted recurrence should live in the computation layer.
* User position, 2026-09-28, paraphrased: the theoretical appearance of `abort` from an ordinary empty value type is understood; it does not change the larger architectural conclusion.

### Objective (Theory and Current Architecture)

The current A Program architecture already distinguishes:

```text
dependent / IADT / proof-oriented structure
```

from:

```text
CBPV computation
```

and independently tracks computation totality.

Standard CBPV also places divergence/general recursion on the computation side. A recursive computation can be modeled through a computation-level fixed point with a suspended self reference; divergence is then an operational/semantic property of computation rather than a constructor of ordinary values.

### Assessment

This audit now treats the following as the strongest current design conclusion:

```text
Dependent / IADT side
    *k
    structural induction
    Acc / well-founded recursion
    proof-relevant total computation
        |
        v
      TOTAL

-------------------------------- CBPV boundary

Computation side
    unrestricted recurrence
    fixed-point computation
    AGAIN-like self recurrence
    unbounded search
    event loops
    server loops
        |
        v
   UNSPECIFIED by default
```

The unresolved question is no longer **where** potentially non-terminating computation belongs.

The unresolved questions are lower-level:

1. what exactly `AGAIN` re-enters operationally;
2. how state/closure/environment is captured across recurrence;
3. how normal-return information is synthesized;
4. how a computation with no normal-return path is classified;
5. how later termination evidence may recover `TOTAL`.

This distinction is important because it prevents the `0`/`abort` discussion from accidentally driving the architecture. The architecture should be chosen first:

> unrestricted recurrence is computation-level.

Then the result-channel design is chosen inside that architecture.

### Plan

* [x] Treat general recursion / potentially infinite recurrence as a CBPV computation-layer feature.
* [x] Preserve IADT/`*k` recursion as the total, induction-derived mechanism.
* [ ] Specify the operational semantics of the computation-level self-recurrence primitive.
* [ ] Specify its interaction with environment/state capture.
* [ ] Only after that, finalize the result-channel design (`0`, `NoReturn`, or another representation).
* Completion: the architecture no longer depends on resolving the empty-result-type question.

---

## P7A. Clarification: `abort` is a consequence of ordinary empty-type elimination, not a requirement of general recursion

### Subjective (User)

* User position, 2026-09-28, paraphrased: the theoretical reason `abort` appears when using an ordinary empty type is now understood.
* The user has not committed to using an ordinary empty value type as the final result representation.

### Objective (Theory)

For an ordinary empty value type:

```text
0 : Type
```

with no constructors, dependent type theory normally provides empty elimination:

```text
abort : 0 -> A
```

for arbitrary `A`.

This is a theorem/eliminator of the empty value type. It is not the source of divergence and is not what gives a computation the ability to recur indefinitely.

Conversely, CBPV-style divergence/general recursion can exist without introducing an empty result type at all.

### Assessment

The audit should keep three notions separate:

```text
1. General recursion / divergence
   - computation-level operational phenomenon

2. Empty value type 0
   - ordinary value type with no constructors
   - therefore supports empty elimination / abort

3. No normal return
   - control-flow/result-channel property of a computation
   - may or may not eventually be represented using 0
```

Therefore:

```text
"general recursion belongs in the computation layer"
```

is a substantially stronger and more stable conclusion than:

```text
"non-returning computations should have result type 0"
```

The latter remains an implementation/type-design choice.

If A Program uses ordinary `0` for the result channel, then empty elimination is not an accidental extra feature: it follows from treating `0` as an ordinary empty value type.

If that elimination behavior is undesirable for the computation-result classifier, then the alternative should not be described as an "empty type without abort" unless the theory explicitly defines such a nonstandard object. A dedicated computation-level `NoReturn`/result-classifier state would be conceptually different from an ordinary empty type.

### Plan

* [ ] Avoid calling a computation-specific no-return classifier `0` unless it is intentionally the ordinary empty value type.
* [ ] If ordinary `0` is chosen, accept and document its eliminator rather than pretending it is absent.
* [ ] If empty elimination is not desired at this layer, design a distinct computation-result notion instead.
* [ ] Keep all of these choices subordinate to the already-selected CBPV computation-layer placement of general recursion.
* Completion: documentation never conflates divergence, empty values, and lack of normal return.

---

## P7. Candidate: use the empty value type as the normal-return type of non-returning computations

### Subjective (User)

* User position, 2026-09-27, paraphrased: if a computation has no normal-returning path, assigning a dedicated bottom/empty result type is more reasonable than pretending that the computation returns whatever type the surrounding context expects.
* User example motivating the issue: an infinite printing loop should be representable even though it produces no ordinary result value.

### Objective (External theory)

There are two different notions that are easy to conflate:

1. **semantic bottom** `⊥`, representing divergence/nontermination in a domain model;
2. **empty value type** `0` / `Empty` / `Never`, having no values.

They are not the same thing.

In Paul Blain Levy's CBPV treatment of recursion, the language is extended with a divergent computation at an arbitrary computation type and with a recursive computation binder. The recursive variable is a thunk of the computation. In the Scott-style semantics, a diverging computation denotes the least element `⊥` of the computation domain. See Levy, *Call-By-Push-Value*, Chapter 5, especially §5.3, "Divergence and Recursion".

Thus standard CBPV does **not** derive an empty return type merely from divergence; rather, divergence is available at each computation type.

Capretta's `Partial(A)` / delay-style treatment makes a different but compatible distinction: a partial computation is either eventually a returned `A` or can keep taking computation steps indefinitely. Again, the possibility of infinite computation is represented at the computation layer, not by identifying divergence with an empty value type.

### Assessment

For A Program, nevertheless, an empty value type is a plausible **surface synthesis result** when static control-flow analysis finds no normal-return path.

The intended meaning would be:

```text
Comp(UNSPECIFIED, E, 0)
```

means:

> this computation has effect row `E`; totality is not guaranteed; and if control returns normally through the computation-result channel, the returned value would have to inhabit the empty type.

This is deliberately weaker than saying "the computation diverges forever".

For example:

```text
{
    #print "tick";
    AGAIN
}
```

could synthesize:

```text
Comp(UNSPECIFIED, {Print}, 0)
```

because there is no syntactic normal-return path.

But the same result type could also be appropriate for a computation that terminates through an aborting effect or exception handler path rather than by normal return. Therefore:

```text
result type = 0
```

means **no normal return value**, not **proof of divergence**.

That distinction matches the existing meaning of `UNSPECIFIED`, which is explicitly *not* proof of divergence.

### Important distinction: `0` is not domain-theoretic `⊥`

The notation "bottom" is dangerously overloaded.

This audit proposes using:

```text
0
```

for an **empty value type** in the result position.

Levy's domain semantics uses:

```text
⊥
```

for the least semantic element denoting divergence in the **computation domain**.

They must not be identified.

A useful picture is:

```text
value/result layer                 computation semantics

0  = no returned value             ⊥ = nontermination / least approximation

Comp(UNSPECIFIED, E, 0)
         |
         +-- may diverge (⊥)
         +-- may terminate abnormally through effects
         `-- cannot normally return an ordinary value
```

### Candidate synthesis rule

For a computation block, collect normal-return result types from actual return/exit paths.

Conceptually:

```text
normal returns:
    return a1 : A
    return a2 : A
    ...

recursive recurrence:
    AGAIN     : contributes no result value type

effects:
    union operations independently

totality:
    AGAIN prevents construction-only TOTAL
```

Then:

```text
if one or more normal-return paths exist:
    result type = the type/join required by those normal-return paths

if no normal-return path exists:
    result type = 0            [candidate rule]
```

This gives:

```text
{ AGAIN }
    => Comp(UNSPECIFIED, {}, 0)

{ #print "x"; AGAIN }
    => Comp(UNSPECIFIED, {Print}, 0)

{ if p then return 42 else AGAIN }
    => Comp(UNSPECIFIED, {}, Nat)
```

The exact branch-joining rule remains to be designed; this section only proposes what happens when the set of normal-returning branches is empty.

### Why this may fit A Program better than bottom polymorphism

A bottom-polymorphic typing rule would say, in effect:

```text
diverging computation checks at any expected result type A
```

That is conventional and semantically defensible, but it makes the contextual expected type participate in assigning the term's apparent return type.

The empty-result proposal instead preserves a more syntax-directed reading:

```text
the term has no normal result
        |
        v
result type = 0
```

Any later use of that computation in a context expecting another result type should go through an explicit or principled empty elimination / unreachable continuation rule, rather than silently changing the synthesized classifier.

This is particularly attractive because current A Program deliberately keeps `::` post-synthesis.

### Open questions

1. Does A Program already have a canonical source-level empty IADT, or should `0` be a library definition rather than a primitive?
2. Is the empty type a value type at the correct universe level for all desired result positions?
3. What is the exact CBPV sequencing rule after a computation of result type `0`?
4. Should there be an explicit empty eliminator, or can existing IADT elimination express it?
5. How should handlers whose operations abort or resume interact with the "normal-return path" analysis?
6. If an effect handler later turns an aborting computation into a normally returning one, does handler typing transform `Comp(E,0)` into an ordinary result type in the expected way?
7. Should a syntactic infinite recurrence with `AGAIN` be recognized as `0` only when no normal return is reachable syntactically, or should this be a general control-flow property proved by evidence?

### Plan

* [ ] Verify whether an empty IADT is currently definable and accepted by the pointer implementation.
* [ ] Prototype a control-flow/result synthesis judgment that distinguishes normal return from recurrence and effect exits.
* [ ] Test the three canonical cases: pure loop, printing loop, conditional base-case recurrence.
* [ ] Specify empty elimination at the computation boundary without adding arbitrary expected-type coercion to synthesis.
* [ ] Keep semantic `⊥` and value type `0` distinct in all documentation and implementation names.
* Completion: a program with no normal-return path has a principled synthesized result classifier, and the rule does not confuse divergence with the empty value type.

---

## Candidate Minimal Model

This section is **an agent proposal for analysis only**, not an approved language design.

The smallest coherent model currently visible is:

```text
1. Existing total recursion

IADT elimination
    recursive field k
        |
        v
       *k
        |
        v
structural / Acc-backed recursion
        |
        v
Comp(TOTAL, E, A)


2. New general computation recurrence

computation body
        |
        +-- normal return paths contribute A
        |
        +-- effect operations contribute to E
        |
        `-- AGAIN contributes:
              no normal-return value type
              no algebraic effect by itself
              loss of construction-only TOTAL
        |
        v
Comp(UNSPECIFIED, E, A)

If there is no normal-return path at all:

Comp(UNSPECIFIED, E, 0)      [candidate]


3. Optional recovery of totality

(a) termination_by
    elaborate the recursive-looking source to Acc / existing total recursion

or

(b) post-hoc proof
    M : Comp(UNSPECIFIED, E, A)
    p : Terminates(&M)
             |
             v
    totalize M p : Comp(TOTAL, E, A)

If E = {}, existing total/pure projection may then produce A.
```

The central open problem has shifted.

The original question was:

> How can the recursive self occurrence obtain its own computation classifier?

The current leading answer is:

> `AGAIN` should not be responsible for synthesizing the normal-return result type at all.

The remaining problem is instead:

> How does block/branch synthesis derive the result type from actual normal-return paths, and what canonical result is used when there are none?

P7 proposes the empty value type `0` for the latter case.

---

## Result-type examples that the eventual design should classify

These are pseudocode, not current A Program syntax.

### E1. Pure unconditional recurrence

```text
{
    AGAIN;
}
```

Leading candidate:

```text
Comp(UNSPECIFIED, {}, 0)
```

Rationale: no normal return, no operation effect, no totality guarantee.

### E2. Conditional recurrence with a base result

```text
{
    if done then
        return n
    else
        AGAIN;
}
```

Leading candidate:

```text
Comp(UNSPECIFIED, {}, Nat)
```

Rationale: the base return determines the normal result type; `AGAIN` contributes no result type.

### E3. Effectful server / print loop

```text
{
    #print "tick";
    AGAIN;
}
```

Leading candidate:

```text
Comp(UNSPECIFIED, {Print}, 0)
```

This is the motivating example for independent inference of:

```text
totality
effect row
normal-return type
```

### E4. Well-founded recurrence written with `AGAIN`

```text
{
    if n == 0 then
        return result
    else {
        n := pred n;
        AGAIN;
    }
}
termination_by n
```

Candidate behavior: elaborate to existing `Acc`/IADT total recursion and produce `TOTAL`, rather than retaining an operational general fixed point.

### E5. Unbounded minimization

```text
{
    if P(n) then
        return n
    else {
        n := succ n;
        AGAIN;
    }
}
```

Expected default:

```text
Comp(UNSPECIFIED, {}, Nat)
```

Given a separate existence/termination proof, a future post-hoc totalization rule may recover `TOTAL`.

### E6. Aborting computation with no normal result

Conceptually:

```text
{
    raise error;
}
```

Depending on the final effect semantics, a computation may have result type `0` while terminating abnormally through an effect. This example is important because it proves that:

```text
result type 0
```

must not be defined as synonymous with:

```text
diverges forever
```

---

## Non-Goals of This Document

This audit does **not** approve any of the following:

* the final spelling `AGAIN`;
* implicit recursion in every `{ ... }` block;
* a new Core `Fix` node;
* named self-recursion;
* changing the meaning of `*k`;
* adding divergence to the algebraic effect row;
* removing the existing `TOTAL` requirement from `pg_prove_termination`;
* an `UNSPECIFIED -> TOTAL` cast;
* changing `::` from post-synthesis checking into bidirectional expected-type propagation.
* identifying the empty value type `0` with semantic divergence `⊥`.
* implicit coercion of every non-returning computation to every expected result type.

Each of those requires a separate design decision.

---

## Primary Literature and External References

The papers below motivate the distinctions used in this audit. They do **not** by themselves determine A Program's final surface syntax or classifier rules.

### Paul Blain Levy — CBPV

1. **Paul Blain Levy. "Call-by-Push-Value: A Subsuming Paradigm." TLCA 1999, LNCS 1581, pp. 228–243.**  
   DOI: https://doi.org/10.1007/3-540-48959-2_17

   Primary introduction of CBPV. The key architectural point for this audit is the separation between values and computations.

2. **Paul Blain Levy. "Call-by-Push-Value: Decomposing Call-by-Value and Call-by-Name." Higher-Order and Symbolic Computation 19(4), 2006, pp. 377–414.**  
   DOI: https://doi.org/10.1007/s10990-006-0480-6  
   Author preprint: https://www.cs.bham.ac.uk/~pbl/papers/hosc05.pdf

   Expanded journal treatment of CBPV and its computational effects.

3. **Paul Blain Levy. *Call-By-Push-Value* (PhD thesis / later monograph material), Chapter 5 "Recursion and Infinitely Deep CBPV", especially §5.3 "Divergence and Recursion".**  
   PDF: https://www.cs.bham.ac.uk/~pbl/papers/thesisqmwphd.pdf

   This is the most directly relevant primary source for the current design discussion. Levy adds:

   * a divergent computation at arbitrary computation type;
   * a computation-level recursion binder whose recursive variable has thunk type `U B`;
   * an operational unfolding rule substituting the thunk of the recursive computation;
   * Scott/domain semantics in which divergence denotes the least element `⊥`.

   This supports the claim that general recursion belongs naturally on the computation side of CBPV. It **does not** imply that the result value type should be the empty type. The `0` proposal in P7 is A Program-specific surface/classifier design.

### Venanzio Capretta — general recursion inside total type theory

4. **Venanzio Capretta. "General Recursion via Coinductive Types." Logical Methods in Computer Science 1(2:1), 2005.**  
   DOI: https://doi.org/10.2168/LMCS-1(2:1)2005  
   Open article: https://lmcs.episciences.org/2265

   Capretta associates each type `A` with a type of partial computations (`Partial(A)` / delay-style partial elements), generated conceptually by return and computation-step constructors. This demonstrates a different way to keep general recursion separate from ordinary total values while remaining inside intensional type theory.

   It is particularly relevant to the possible future split:

   ```text
   partial/general computation first
             |
      convergence evidence
             v
        recovered value/totality
   ```

### Altenkirch, Danielsson, Kraus — partiality monad refinement

5. **Thorsten Altenkirch, Nils Anders Danielsson, Nicolai Kraus. "Partiality, Revisited: The Partiality Monad as a Quotient Inductive-Inductive Type." FoSSaCS 2017, LNCS 10203, pp. 534–549.**  
   DOI: https://doi.org/10.1007/978-3-662-54458-7_31  
   Preprint: https://www.cse.chalmers.se/~nad/publications/altenkirch-danielsson-kraus-partiality.pdf

   This revisits Capretta-style partiality and highlights that the equality theory of partial computations matters. It is useful background if A Program eventually gives `UNSPECIFIED` computations a richer internal mathematical representation rather than treating them only operationally.

### Lean as a comparison point, not a direct model

6. **Leonardo de Moura, Sebastian Ullrich. "The Lean 4 Theorem Prover and Programming Language." CADE 28, 2021, pp. 625–635.**  
   DOI: https://doi.org/10.1007/978-3-030-79876-5_37  
   PDF: https://lean-lang.org/papers/lean4.pdf

7. **Lean Reference Manual, "Recursive Definitions".**  
   https://lean-lang.org/doc/reference/latest/Definitions/Recursive-Definitions/

   The reference manual is not a primary research paper, but it is the authoritative implementation reference for `termination_by`, structural recursion, well-founded recursion, and the separation of `partial`/`unsafe` definitions from kernel reduction.

   For this audit the important comparison is:

   ```text
   Lean termination_by:
       recursive-looking source
           -> well-founded/structural total definition

   proposed A Program post-hoc totalization:
       genuine UNSPECIFIED computation
           + separate Terminates proof
           -> TOTAL computation
   ```

   These should remain separate design concepts.

### Current A Program code references

* `README.md`: current surface recursion contract, `*k`, post-synthesis `::`, CBPV boundary, and current limitations.
* `src/classifier.h`: `PG_TOTALITY_UNSPECIFIED`, `PG_TOTALITY_TOTAL`, computation classifiers, thunk types, termination types.
* `src/evidence.c`: current `pg_prove_termination` only accepts an already-`TOTAL` suspended computation; total/pure projection additionally requires an empty effect row.

---

## Progress

| Date | Problem | Material result or decision | Evidence / next step |
| --- | --- | --- | --- |
| 2026-09-27 | P1 | `*k` should remain induction-derived; named self-recursion is not assumed. | Current `README.md`; discussion on 2026-09-27. |
| 2026-09-27 | P2 | General recursion is most naturally investigated at the CBPV computation layer with `UNSPECIFIED` totality. | `src/classifier.h` totality/thunk structure; CBPV analysis. |
| 2026-09-27 | P3 | `AGAIN` is a temporary discussion name for an enclosing-computation recurrence operation. | User explicitly requested placeholder status. |
| 2026-09-27 | P4 | Revised: `AGAIN` should probably contribute no normal-result type information; arbitrary expected-type retagging is not the preferred surface semantics. | Current `::` is post-synthesis only; result/type/effect information can be considered separately. |
| 2026-09-27 | P5 | Lean-like `termination_by` and post-hoc `Terminates` totalization must be treated as distinct designs. | Current termination witness accepts only already-`TOTAL` thunks. |
| 2026-09-27 | P6 | General recursion must remain outside dependent definitional normalization. | Required for proof/type-checking termination; needs formal rule and negative tests. |
| 2026-09-27 | P7 | Leading candidate: if a recursive/effectful computation has no normal-return path, synthesize empty result type `0` rather than an arbitrary contextual result type. | Must keep value type `0` distinct from semantic divergence `⊥`; verify empty IADT support and handler interaction. |


## Latest Design Conclusions (2026-09-28)

1. **General recursion / potentially infinite looping belongs on the CBPV computation side.**  
   This is now treated as the stable architectural direction of the audit.

2. **`*k` remains an induction-derived result, not a general recursive call.**  
   General recursion should not be introduced by changing the meaning of `*k` or by making declaration names implicitly recursive.

3. **The `0`/`abort` question is subordinate.**  
   Using an ordinary empty value type `0` implies ordinary empty elimination (`abort`). This is a property of the value theory, not the mechanism that enables nontermination.

4. **"No normal return" is not automatically the same thing as divergence.**  
   A computation may have no normal value because it diverges, aborts through an effect, or otherwise exits the normal result channel.

5. **The next major design task is operational semantics for `AGAIN`.**  
   In particular, the system must decide what enclosing computation is re-entered and what environment/state is observed on the next iteration.
