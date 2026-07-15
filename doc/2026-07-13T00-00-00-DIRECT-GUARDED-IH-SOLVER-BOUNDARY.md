# Direct Guarded IH Solver Boundary

Date: 2026-07-13

Status: implemented restricted fragment in `src/prototype/`; general motive
constraint solving is future work.

## Purpose

The prototype constructs the core computation graph before classifier synthesis.
An induction-hypothesis occurrence (`*rest`) is therefore not given a provisional
TermDB classifier while lowering.  During classifier synthesis it denotes the
solver-local expression:

```text
M(rest)
```

where `M` is the motive of the enclosing source Match operation.  `M(rest)` is
not an evaluator environment, a TermDB node, or a JudgementDB relation before
the motive is solved.

## Implemented Fragment

The current solver recognizes one guarded recursive equation directly:

```text
M(C(..., rest, ...)) = M(rest)
```

The unresolved branch must be exactly the `*rest` IH of the same Match frame,
and `rest` must be a recursive constructor field of that frame.  These checks
prevent an arbitrary cyclic classifier equation from being accepted as a
recursive motive.

For example, the following non-uniform motive is accepted:

```text
M(zero0) = Bool
M(zero1) = Nat
M(succ k) = M(k)
```

The source branch is:

```text
fold := \n : BadNat =>
	n @zero0 => Bool.true
	  @zero1 => Nat.zero
	  @succ k => *k;
```

After propagation reaches a fixed point, the compiler materializes a finite
motive graph and records the classifier of the original Match as:

```text
APP(motive, scrutinee)
```

Constant motives are a separate solved case.  If every branch classifier is
normalization-equal and does not capture a case binder, the compiler builds a
constant lambda motive.  The apparent common classifier is then obtained by
ordinary beta reduction of `APP(\_ => T, scrutinee)`, not by a uniform-Match
kernel reduction.

## Current Restriction

The implementation is not a general recursive-motive solver.  In particular,
it cannot solve an equation where the recursive motive application is nested in
another type expression:

```text
M(zero)   = Nat
M(succ k) = List(M(k))
```

Nor can it generally solve a branch combining multiple recursive hypotheses:

```text
M(leaf) = Nat
M(node left right) = Pair(M(left), M(right))
```

The direct guarded recognition intentionally requires an unresolved branch to
be an IH node by itself.  It is sufficient for the present `Nat`, `List`,
`append`, and `recursive_dependent_match` examples, but it is not sufficient
for dependent constructor results, indexed inductive families, or recursive
type-level transformations.

## Why This Is Not Solved by Building a TermDB Match Early

A source Match can always be lowered to a TermDB graph.  The missing object is
not the value-level Match representation.  It is an unsolved type expression
for its motive.  Materializing a guessed `M` into TermDB before solving the
equations would make an unresolved meta-variable look like a concrete language
term and risks cyclic JudgementDB derivations.

The solver must therefore keep unresolved motive applications outside TermDB
until it has a justified finite solution.

## Future Solver Direction

Introduce a solver-local type-expression language with at least:

```text
TermRef(term)
Meta(variable)
App(function, argument)
Pi(domain, codomain)
Match(scrutinee, cases)
MotiveApp(motive_variable, argument)
```

Match typing should emit equations of the form:

```text
M(Constructor(fields)) = branch_classifier_expression
```

The solver should then:

1. propagate ordinary classifier constraints separately from motive equations;
2. perform a structural occurs/guardedness check for `M(rest)` under approved
   type constructors;
3. reject unguarded recursive equations and unresolved metavariables with a
   source diagnostic; and
4. materialize the final lambda/Match motive in TermDB only after solving.

This is deliberately narrower than unrestricted higher-order unification.
The initial target is structural recursive motive equations over the language's
existing type-former graph, not arbitrary higher-order proof search.

## Relevant Implementation

- `src/prototype/ast.c`: `operation_solver_has_guarded_recursive_equation`,
  `build_operation_guarded_recursive_motive`, and
  `operation_solver_materialize_induction_hypothesis`.
- `src/prototype/ast.c`: solver-local `operation_solver_motive_application`
  and `operation_motive_equation` records.
- `training/recursive_dependent_match.p`: regression fixture for the current
  direct guarded fragment.
