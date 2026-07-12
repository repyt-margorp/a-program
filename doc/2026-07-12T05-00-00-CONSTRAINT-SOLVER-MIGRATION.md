# Constraint Solver Migration

Date: 2026-07-12

Status: in progress in `src/prototype/`

## Implemented Boundary

Classifier synthesis now starts from an operation-scoped solver arena rather
than from global fixed-point insertion into `JudgementDelta`.

Each `prototype_operation_node` has a solver-local classifier variable. The
solver emits constraints for:

- `HasType` for source atoms and declarations,
- equality for source-name occurrences,
- expected checks for ascriptions,
- Pi formation/application for lambda and application operations,
- one motive equation per match branch, and
- an induction-hypothesis requirement for `*rest`.

Each motive equation also records its constructor, source case-binder range,
and match frame in the solver arena. This is the scoped context used when a
constant motive candidate is admitted; a candidate may not capture any case
binder.

An ascription constraint is recorded as a conversion obligation but does not
bind an operation classifier or seed a motive. The post-synthesis ascription
phase checks it against the resolved classifier.

The solver commits resolved classifiers to `OperationGraph` only after its
worklist reaches a fixed point. `JudgementDB` derivations are then
materialized from the resolved operation classifiers. It is not used as the
solver's global candidate store.

All match results use a materialized `APP(motive, scrutinee)` classifier. A
uniform result is represented by a constant lambda motive, so its common type
is obtained by beta reduction rather than by a special kernel conversion rule.

## Recursive Constant Motives

For a recursive match, a solved non-recursive branch may provide a constant
motive candidate. The candidate is retained only in the solver arena. In
particular, it does not create a provisional `APP(motive, scrutinee)` term or
a provisional JudgementDB relation.

The induction-hypothesis equation can use that solver-local candidate while
typing a recursive branch. Once every branch has a solved classifier, the
compiler creates the actual motive lambda and the `APP(motive, scrutinee)` and
`APP(motive, rest)` terms. This is sufficient for the current `Nat` and
`List` recursive examples, including `append`.

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

1. The current arena stores resolved classifier term ids and a constant motive
   candidate. It does not yet have general scoped type/motive metavariables or
   an occurs check.
2. A recursive dependent motive whose `M(rest)` cannot be represented by a
   constant seed still needs solver-level type-expression nodes. It must not be
   solved by prematurely materializing a TermDB motive.
3. `SOLVED_MATCH_MOTIVE` validation checks that every materialized motive case
   agrees with an available branch classifier without inserting cyclic proof
   edges. It still needs operation-identity evidence in the proof object;
   current validation can only query core-term branch relations.
4. Universe level constraints remain in `UniverseDB`; they have not yet been
   moved into the solver arena.
5. Constructor schema is still represented by both graph-level
   `classifier_family` and readback metadata. The graph-only constructor
   schema migration, including dependent field telescopes, is a later phase.

## Verification

The migration is currently exercised by:

- the shared-core typed identity artifact test,
- generic `List Nat` construction and match,
- recursive `append` and list-length examples,
- `training/double.p`, and
- `src/prototype/test_artifact_flow.sh`.
