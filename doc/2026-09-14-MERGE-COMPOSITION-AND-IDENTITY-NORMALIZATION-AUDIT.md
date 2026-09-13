# Merge Composition and Identity Normalization

Date: 2026-09-14
Branch: `rewrite/pointer-core-hott`
Starting revision: `eb007ed`
Issue: <https://github.com/repyt-margorp/a-program/issues/28>
Status: ordinary merge regression coverage added; graph formation unresolved.

## What Reproduces

The posted Issue #28 source rejects both with and without `repeatWithFuel`
on the starting revision and this worktree. The reported declaration-order
contamination is therefore not reproduced here. This does not establish what
happened in the reporter's older `843c57c` binary or other source variants.

The first failing source application is `*tail right` in `structuralMerge`.
Its `right` lambda surrounds the Match, rather than appearing inside each
branch. In this example the recursive motive returns `List A`, with `right`
captured in the environment. Thus `*tail` already produces the list; applying
it to `right` tries to call a list as a function. No Function Graph declaration
is requested by the posted source, which contains neither `@function` nor
`*function` at a named function definition.

Two valid forms are tested:

```text
// Captured argument; recursive result is a list.
\left : List A => \right : List A =>
  left @nil => right
       @cons head tail => insertBy A le head *tail

// Branch-local argument; recursive result is a function.
\left : List A =>
  left @nil => (\right : List A => right)
       @cons head tail => (\right : List A => insertBy A le head (*tail right))
```

`src/prototype/pointer/tests/merge_composition.sh` is part of `check`:

- [x] Check both valid forms, alone and alongside the independent fuel recursion.
- [x] Move the fuel definition before the merge definitions and recheck results.
- [x] Check nonempty merge, empty-left merge and fuel composition results.
- [x] Reject the original overapplication, with and without the fuel definition.
- [x] Check pending and completed `.a` inputs through ordinary Solve, with
  evaluation chunks of 1 and 64. No special merge or replay rule is added.

This is insertion-based merging, not a claim of linear-time merge or a complete
MergeSort implementation. The full user MergeSort source has been requested.

## Separate Function Graph Limitation

After correcting the recursive call, ordinary merge execution succeeds.
Requesting `@curriedMerge` still yields `unsupported`. The checked reproduction
is `merge-function-graph-request.p`, importing `merge-function-graph-composition.p`
from `src/prototype/pointer/tests/acceptance/`.

Tracing reaches `pg_function_graph_advance`: both branch plans are constructed,
but `pg_prove_inductive_type` does not admit the generated schema. This is a
different failure from the posted application's wrong classifier. Neither a
global metadata collision nor a termination failure has been established.

The failed premise is the universe bound, not positivity. On this fixture,
`graph_universe` chooses level 0 from the selected input/output telescope,
whereas `pg_data_schema_field_level` finds a field formation at level 1.
That field's retained evidence passes through `PG_PI_DOMAIN` and `PG_REINDEX`.
Pi component inversion deliberately retains the whole Pi's universe bound;
it does not recover a minimal component bound. The kernel therefore correctly
refuses to place this evidence in the generated level-0 declaration.

- [x] Identify the failed formation premise: field bound 1 exceeds declaration 0.
- [ ] Reconcile graph universe synthesis with the actual field formations.
  Either recover tighter bounds from retained formation proofs or synthesize
  a sufficient universe for the entire generated telescope and its packet.
  Do not equate universe levels, strip evidence, or bypass the admission check.
  Test polymorphic helper calls as well as direct recursion and higher-level
  source types; an unconditional level bump is not a general solution.
- [ ] Restore graph formation and witness generation, then test an actual
  post-hoc property separately from simply obtaining an output witness.
- [ ] Inspect the full MergeSort source when its location is available.

Keep Issue #28 open while reporting this distinction, rather than describing
all graph-composition concerns as fixed.

## Independent Identity Fix

An attempted Vec compatibility change exposed a genuine WHNF problem in
`identity.c:action_body_resume`. When an open action body did not reduce, the
evaluator stopped without checking whether the supplied boundary path reduced
to reflexivity. Re-evaluating the materialized result could then make progress.
QuickSort ascending input exposed this through a stuck transport and Match.

The fix invokes the existing `diagonal_fallback` in that unchanged-body case.
It adds no equality reflection, type-specific axiom, or new term constructor.
The existing fallback checks the supplied path and both endpoints; an arbitrary
loop proof is not declared reflexive. Scope pruning still precedes this check.

The direct regression uses `Act(\x. Act(q x))` applied to a computed reflexive
boundary. One WHNF pass must produce the iterated reflexivity result. It also
checks neutral non-reflexive input and suspended evaluator boundaries. Forcing
the old unchanged-body early return under GDB makes this test fail.

The portable pure policy changes from `evaluation/pure/v2` to `v3`, and the
action-body work operation from `identity/action_body/v1` to `v2`. Old normality
receipts must not silently inherit the new reduction behavior. The action-body
continuation retains its own unchanged `v1` name. Raw source inputs still use
the same Solve path; no new replay engine is introduced.

## Withdrawn Vec Trial and Verification

Lowering every total, empty-effect sequence as application of an extracted value
made the legacy Vec append check terminate, but regressed
`indexed-recursive-result.p`. Restricting the trial to syntactically dependent
codomains did not fix that regression and lost the Vec improvement. Neither
trial is retained. No failing test was removed to claim compatibility.

- [x] Debug `check-acceptance`, including the new merge suite: passed.
- [x] Existing source compatibility gate: 28/28; all six QuickSort outputs passed.
- [x] ASan/UBSan `check-acceptance`: passed, including the merge suite.
- [ ] Legacy Vec append remains open, as do general QuickSort post-hoc properties.

The overall rewrite goal remains active. These local tests do not establish
complete Higher Observational TT, general termination soundness or MergeSort
correctness for all inputs. No Main promotion is justified by this audit alone.
