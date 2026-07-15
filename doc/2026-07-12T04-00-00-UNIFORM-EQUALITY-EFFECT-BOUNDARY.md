# Uniform Match, Equality, and Effect Boundaries

Date: 2026-07-12

Status: uniform-match kernel removal implemented; equality and effect sections
remain design work.

This note records the next four theory-driven criticisms of the prototype.
They are intentionally separate from the Core/TypeView boundary. The immediate
goal is to prevent the current evaluator, future equality types, and future
effects from being merged into one undifferentiated notion of computation.

## 5. Uniform Match Is Not a Kernel Beta or Iota Rule

### Removed implementation

Until this migration, `term.h` defined
`PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH` and `PURE_TYPE_WHNF` enabled it.
`term.c` then normalized every branch of a match with a neutral scrutinee and
returned a common body when all normalized branches compared equal.

Thus the prototype can perform an equation of the following form:

```text
MATCH(b, true -> T, false -> T)  ->  T
```

without learning whether `b` is `true` or `false`.

The rule and its host-effect guard have been removed. Kernel conversion now
contains beta, constructor iota, and induction reductions only.

### Why this differs from beta and iota

Beta and iota are local transitions with a fixed redex shape:

```text
APP(LAMBDA(x, body), argument)              -> body[x := argument]
MATCH(CONSTRUCTOR(owner, ordinal), cases)   -> selected case body
```

Uniform match instead needs global inspection of all branches and recursive
conversion checks between their bodies. It therefore depends on the very
normalization equality relation that kernel conversion is trying to decide.
It is a synthesis/optimization decision, not a primitive transition of the
core calculus.

It also cannot be valid for general effects. Inspecting, normalizing, or
discarding unselected branches may duplicate, suppress, or reorder observable
operations.

### Correct role of constant results

The important case is classifier synthesis, not arbitrary value rewriting. If
the constraint solver establishes:

```text
C_true == T
C_false == T
```

then it may synthesize the constant motive:

```text
M := \_ : Bool => T
match_classifier := APP(M, b)
```

`APP(M, b)` reduces to `T` by ordinary beta. This does not assert the separate
term-level equation:

```text
MATCH(b, true -> value, false -> value) == value
```

The latter may be a pure optimizer rewrite after a separate purity proof, but
must not be kernel DefEq.

### Implemented change

1. `PROTOTYPE_TERM_REDUCE_UNIFORM_MATCH` and the normalizer-side
   `uniform_match_body(...)` path were removed.
2. The classifier solver replaces uniform classifier handling with a motive
   constraint rule: when all branch classifiers solve
   to one classifier, build a constant lambda motive explicitly.
3. If a future optimizer retains case-of-constant rewriting, place it in an
   optimization pass with a purity/effect analysis and never use it as
   Judgemental/Definitional equality.

## 6. Propositional Equality Must Not Extend Global DefEq

### Current implementation

There is no surface equality type, `==` parser production, `REFL`, `J`,
transport, or rewrite term in the prototype. `JudgementDB` has
`PROTOTYPE_JUDGEMENT_PROOF_CONVERSION`, but this is a meta-level typing proof
whose validity is checked by the fixed normalizer. It is not a first-class
equality witness in `TermDB`.

This is currently a good boundary. There is no path from a user-written proof
to an additional reduction rule or a global equivalence-class merge.

### Required invariant

When a first-class equality is introduced, this implication must remain false:

```text
p : Eq(A, lhs, rhs)
-------------------
lhs DefEq rhs
```

Adding every user proof to a union-find or global rewrite table would make
conversion depend on imported proofs, proof-search order, artifact contents,
and optimizer configuration. It would be equality reflection, not ordinary
intensional equality.

The safe future operation is explicit, directed rewrite/transport:

```text
p : Eq(A, lhs, rhs)
term : P(lhs)
-----------------
transport(P, p, term) : P(rhs)
```

The rewrite must be requested at a concrete occurrence by source syntax or a
compiler optimization certificate. The kernel checks `p`; it does not add
`lhs -> rhs` to global DefEq.

### Required preparation

1. Keep `prototype_term_normalization_equal...` as a fixed meta-level
   conversion procedure.
2. Keep `PROTOTYPE_JUDGEMENT_PROOF_CONVERSION` distinct from any future
   object-language equality term.
3. Do not repurpose core-shape equality or TypeView identity changes as
   equality witnesses.
4. Design explicit rewrite/transport together with an equality type; do not
   introduce an unscoped optimizer rewrite registry first.

## 7. Cubical or Higher Observational Equality Is a Kernel Choice

### Current implementation

The prototype has no representation for path terms, dimension variables, face
constraints, composition, coercion, transport, higher constructors, or
observation levels. Its equality mechanism is only fixed normalization equality
used by the typing kernel.

Therefore Cubical Type Theory and Higher Observational Type Theory are not
features that can be added later by inserting `Eq`, `refl`, and a few extra
normalization rules. Either direction changes what equality proofs are, which
terms compute, and what the kernel must track.

### Design consequence

Before exposing an equality type, choose the intended equality kernel.

```text
Intensional identity type:
  Eq / refl / J are object-language constructs.
  Kernel conversion remains beta/iota/delta plus fixed conversion rules.

Cubical equality:
  Paths are dimension-indexed terms.
  The core calculus needs dimension context, endpoint substitution, and
  composition/coercion machinery as computational primitives.

Higher observational equality:
  Equality must preserve its level and observational structure. It cannot be
  implemented by reducing all equality witnesses to one first-order boolean or
  one global equivalence class.
```

The first implementation milestone may still be an intensional equality
fragment, but it must be designed as a possible substrate or explicit boundary
for the intended higher theory. In particular, the implementation must not
erase proof identity merely because two proofs establish the same endpoints.

### Required preparation

1. Write a small equality-kernel design before adding syntax.
2. Specify whether equality witnesses are proof-irrelevant, proof-relevant, or
   later dimension-indexed; do not let the runtime representation decide this
   accidentally.
3. Keep judgemental conversion, propositional equality, and optimization
   certificates as three distinct mechanisms.
4. Do not claim a later Cubical/HOTT extension is conservative until its term
   representation and computation rules have been specified.

## 8. Effect Performance Must Not Absorb Beta and Iota

### Current implementation

The prototype has:

```text
INTRINSIC
EFFECT_LABEL(effect bit set)
EFFECT_TYPE(label, result)
```

Host intrinsic signatures classify `#.int_add` as pure and `#.print` as having
the `Terminal` effect. `perform_host_step(...)` is called from the ordinary APP
evaluation path. Once an APP spine has an intrinsic head and fully suitable
arguments, it evaluates those arguments and either computes a pure host result
or directly writes to `FILE*` for `#.print`.

This is not algebraic effects yet. It is a capability-gated host-oracle
evaluator with an effect annotation. There is no first-class operation request,
handler, captured continuation, resumption, or source-level `perform` term.

### Why beta and iota stay separate

Beta and iota are pure graph transformations. They may be scheduled according
to a chosen evaluation strategy, but their validity does not depend on a host
capability or an output stream.

An algebraic operation is different:

```text
perform Terminal.print text
```

constructs an operation request. A handler or runtime boundary interprets that
request and resumes the captured continuation. The operation is not itself a
beta/iota reduction rule.

Putting beta and iota inside one generic `perform` concept would hide this
distinction. It would make pure confluence facts appear to be properties of a
stateful execution machine, and make an effect handler look responsible for
ordinary substitution and constructor elimination.

### Target evaluator split

```text
Pure normalizer / kernel conversion:
  beta, iota, permitted transparent delta, and explicitly approved pure
  intrinsic reductions only.
  Never performs observable effects.

Runtime evaluator:
  selects a deterministic evaluation strategy.
  uses pure beta/iota steps to reach an operation request or a value.
  delegates operation requests to a handler/runtime capability boundary.

Effect handler/runtime boundary:
  interprets a requested operation and supplies a result or resumed
  continuation according to the operation's declared effect interface.
```

`perform` may be a useful name for the runtime transition that handles an
operation request. It must not become the umbrella name for every reduction in
the lambda calculus.

### Required refactoring direction

1. Keep `evaluate_steps(...)` as pure normalization machinery, with no direct
   `FILE*`, terminal capability, or host-effect execution path.
2. Move direct host execution out of `perform_host_step(...)` into a runtime
   interpreter boundary.
3. Represent an effectful intrinsic application as an explicit pending
   operation/result form before executing it. Do not make APP evaluation write
   to the host directly.
4. Add handlers and resumptions only when the source language has an algebraic
   operation syntax and effect interface. Until then, call the existing feature
   capability-gated host intrinsics, not algebraic effects.
5. Keep pure target-independent intrinsic reductions such as fixed-width
   integer arithmetic in the normalizer only after their bit-width, overflow,
   and target-independence contract is fixed.

## Discussion Order

The next discussion should proceed in this order.

1. Replace uniform match reduction with motive constraints and constant motive
   synthesis.
2. Specify the equality kernel boundary before designing `==` syntax.
3. Decide whether the first equality fragment is intentionally intensional or
   whether the core must immediately reserve representation for higher paths.
4. Separate the pure evaluator from the runtime effect interpreter before
   adding source-level IO or handlers.

The ConstraintDB/Motive solver discussed previously is the immediate dependency
for item 5. Equality and algebraic effects should not be implemented by adding
new exceptions to the current normalizer.
