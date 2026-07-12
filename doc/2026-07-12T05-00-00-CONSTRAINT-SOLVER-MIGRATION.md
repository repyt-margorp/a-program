# Constraint Solver Migration

Date: 2026-07-12

Status: in progress in `src/prototype/`

## Implemented Boundary

Classifier synthesis now starts from an operation-scoped solver arena rather
than from global fixed-point insertion into `JudgementDelta`.

Each `prototype_operation_node` has a solver-local classifier variable. A
lowering-time fact is stored separately as `known_classifier`; the public
`classifier` field is written only from the solver binding. The solver emits
constraints for:

- `HasType` for source atoms and declarations,
- equality for source-name occurrences,
- expected checks for ascriptions,
- Pi formation/application for lambda and application operations,
- one motive equation per match branch, and
- an induction-hypothesis requirement for `*rest`.

Each motive equation also records its constructor, source case-binder range,
and match frame in the solver arena. The range is source scope, not merely a
core TermDB binder id: tagless core `VAR` nodes may be shared by unrelated
source scopes.

An ascription emits an explicit `Convertible` constraint. It checks the
resolved source-operation classifier by kernel normalization equality, but
does not bind that source operation or seed a motive. The post-synthesis
ascription phase only materializes the corresponding conversion proof.

The solver first propagates classifier equations to a fixed point. A solved
match is recorded as either a ready branch equation set or a guarded recursive
equation; neither state creates a TermDB motive. At the fixed-point boundary,
the compiler materializes the solved motive and its `APP(motive, scrutinee)`
classifier, then starts another propagation round for dependent IH, lambda,
and application constraints. Only after no further solved motives remain are
match-pattern assumptions, solved match motives, lambda/application
derivations, and IH derivations materialized in `JudgementDB`.

`JudgementDB` is not used as the solver's global candidate store.

Lambda and application materialization takes the solved source-operation
premise classifiers explicitly. It does not rediscover them by searching for
every judgement whose subject is the same shared core term. Thus the two
source operations for the shared core `\x => x` can materialize distinct
`Bool -> Bool` and `Nat -> Nat` derivations without treating either type as a
property of the core node itself. Declared external references, intrinsics,
text literals, and integer literals are likewise source operations; their
intro/declaration facts are materialized only after the operation classifier
is known. A small integer retains both valid `Int` and `Int64` literal
derivations, while the source operation's default classifier remains `Int64`.

Lambda binder and declaration facts are copied into a solver-owned input-fact
table for each solve invocation. The `HasType` atom constraint reads only that
table; it no longer searches committed or pending JudgementDB relations.
After the operation classifiers are resolved, the same pending facts are
materialized as binder-assumption and declaration proofs. Thus JudgementDB is
an output of synthesis, not its mutable input.

A declared type former also has a graph-level formation classifier. For
example, `List` is classified by `Pi(Universe(u), \A => Universe(v))`, while
`List A` is classified by the result of applying that family. The unapplied
former is recorded with a `TYPE_FORMATION_INTRO` proof directly from its
declaration; a saturated type instance is recorded with a universe classifier.
This keeps `List A` as an ordinary application derivation and avoids a generic
declaration-proof fallback for local type formers.

The worklist has a bounded number of passes and rejects non-convergence; it
does not silently publish a partially progressed fixed point.

Before committing bindings, the solver requires every source `APP`, `LAMBDA`,
`MATCH`, and `*rest` operation to have a classifier solution. This prevents an
invalid application such as `Bool.true Bool.false` from surviving as an
untyped core graph node merely because no surrounding expectation demanded a
type.

All match results use a materialized `APP(motive, scrutinee)` classifier. A
uniform result is represented by a constant lambda motive, so its common type
is obtained by beta reduction rather than by a special kernel conversion rule.
The compiler first checks source-operation binder use before consulting a core
classifier's free binders. This prevents an unrelated shared core `VAR` from
making a branch appear dependent. A source use together with a matching free
binder is conservatively treated as a dependent branch.

Before checking free binders, the classifier is reduced to kernel WHNF. This
matters for nested matches: a constant inner motive may be written as
`APP(\_ => Int, scrutinee)`, whose raw argument mentions outer case binders
even though its beta-reduced classifier is simply `Int`.

Artifact slicing also retains branch-body judgement relations required by a
`SOLVED_MATCH_MOTIVE` validator. Those are semantic dependencies of the
variable-arity motive proof, even though they are not stored as a fixed proof
premise array.

When representation interning merges two type declarations, rebinding
`TYPE_FORMER` anchors now remaps all graph edges to the representative core
node. This preserves the intended invariant: `Bool` and `Two` may retain
different `TYPE_VIEW` identities while their erased type-former core is one
node.

## Constructor Schema Boundary

`classifier_family` is the authoritative constructor schema. Shape keys,
constructor import/export checking, constructor application, pattern-field
typing, and recursive-field recognition all consume its graph-level `PI`
spine. The `readback` field and `readback_field_types` table remain only for
source diagnostics and artifact readback; they do not decide a constructor's
meaning.

The artifact-flow suite exercises this boundary with
`Sigma := \A => \B => @{ mk : (a : A) -> B a -> *; }`: an importing artifact
applies `mk`, a match derives the second field classifier `B a`, and an
alpha-renamed `Sigma2` shares the same core representation. Indexed result
families are still intentionally outside the current surface type literal
syntax; this is an IADT feature, not a field-schema fallback.

## Recursive Constant Motives

For a recursive match, a solved non-recursive branch may provide a constant
motive candidate. The candidate is retained only in the solver arena. An IH
is retained there as the scoped expression `M(argument-operation)`, rather
than as a provisional classifier id. In particular, this does not create a
provisional `APP(motive, scrutinee)` term or a provisional JudgementDB
relation.

The induction-hypothesis equation can use that solver-local candidate while
typing a recursive branch. Once every branch has a solved classifier, the
fixed-point boundary creates the actual motive lambda and the
`APP(motive, scrutinee)` and `APP(motive, rest)` terms. This is sufficient for
the current `Nat` and `List` recursive examples, including `append`.

Candidate closedness is checked only against the source branch that produced
the candidate. Checking every case against raw core binder ids would reject a
closed candidate when another case happens to share that tagless core binder
slot.

## Direct Guarded Recursive Motives

The solver now recognizes the direct guarded equation

    M(C(..., rest, ...)) = M(rest)

when the unresolved branch is exactly the `*rest` IH of its own match. It
materializes the final motive as a finite match graph only after recognizing
that equation. The training program `training/recursive_dependent_match.p`
exercises a non-uniform motive with two distinct base result types and this
recursive branch.

The solver records each symbolic `M(rest)` with the owning match frame. Its
occurs check accepts only an occurrence whose argument is an actual recursive
field of that same frame; an unguarded self-reference cannot become a cyclic
solver substitution. This is deliberately a structural occurs check, not a
general higher-order unifier.

This is not general higher-order unification. For example, an equation such
as `M(C(rest)) = F(M(rest))` still needs solver type-expression nodes and
unification before it can be accepted.

## Operation Binder Identity

Core `VAR` nodes can be shared by tagless canonicalization. Therefore, a core
term id is not enough to decide which source variable receives a match-pattern
classifier. `prototype_operation_node` now retains the source AST binder id
for variable occurrences. Match resolution updates only operations referring
to the corresponding source binder.

This distinction prevents a pattern-field assumption from overwriting an
unrelated operation occurrence that happens to share a core `VAR` node.

## Remaining Work

The following items are intentionally not claimed as complete:

1. The current arena stores operation classifier metavariables, scoped
   `M(argument)` expressions, motive equations, constant motive candidates,
   and a structural occurs check. It does not yet have general higher-order
   type-expression metavariables or unification.
2. A recursive dependent motive whose `M(rest)` is transformed by another type
   expression still needs solver-level type-expression nodes. It must not be
   solved by prematurely materializing a TermDB motive.
3. `SOLVED_MATCH_MOTIVE` validation checks that every materialized motive case
   agrees with an available branch classifier without inserting cyclic proof
   edges. It still needs operation-identity evidence in the proof object;
   current validation can only query core-term branch relations.
4. DefEq now distinguishes `Universe(u)` and `Universe(v)` by level-variable
   identity. Application accepts distinct universe variables only as a
   cumulativity obligation. The materialized `APP_ELIM` proof is collected as
   `v <= u` by `UniverseDB` after linking; it never merges the two universe
   nodes as DefEq. Pi `max(u, v)` formation is still incomplete.
5. App constraints currently propagate forward from a resolved function and a
   resolved argument. Reverse Pi propagation requires an explicit separation
   between value application and type-level application before it is safe to
   add: the current operation graph contains both forms.
6. `SOLVED_MATCH_MOTIVE` currently records its branch-classifier evidence as
   an artifact-slicing dependency rather than explicit variable-arity proof
   premises. The proof representation should eventually encode that evidence
   directly.

## Verification

The migration is currently exercised by:

- the shared-core typed identity artifact test,
- generic `List Nat` construction and match,
- recursive `append` and list-length examples,
- `training/double.p`, and
- `training/dependent_match.p`,
- `training/recursive_dependent_match.p`, and
- `src/prototype/test_artifact_flow.sh`.
