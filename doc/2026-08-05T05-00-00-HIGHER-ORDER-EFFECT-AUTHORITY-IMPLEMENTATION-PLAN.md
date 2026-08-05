# Higher-Order Effect Authority Implementation Plan

Date: 2026-08-05

Status: implemented and verified

Branch: `main`

Baseline commit: `d72f691 add higher-order effect row atom representation`

## 1. References

This plan implements the decisions recorded in:

- `2026-08-05T03-00-00-HIGHER-ORDER-EFFECT-CAPABILITY-DECISIONS.md`
- `2026-08-05T04-00-00-HIGHER-ORDER-EFFECT-STATIC-IMPLEMENTATION-PROGRESS.md`
- `2026-08-05T01-00-00-THUNK-ENCODED-HIGHER-ORDER-OPERATIONS.md`
- `2026-08-05T02-00-00-HIGHER-ORDER-OPERATION-RESOURCE-IMPLEMENTATION-PROGRESS.md`

The first reference defines the semantic boundary. The second remains the
long-running static-effect ledger. This file owns the concrete migration from
provisional request classifiers to occurrence-local parameterized effect atoms.

## 2. Objective

For a higher-order request:

```text
op : forall E. Thunk(Comp(E, A)) -> Comp({op}, B)
```

and a concrete occurrence:

```text
OPERATION_REQUEST(op, &M, k)
M : Comp(E1, A)
```

the compiler must derive and preserve:

```text
OPERATION_REQUEST(op, &M, k)
  : Comp(EFFECT_ROW_OPERATION(op, E1) union effects(k), result(k))
```

The selected classifier of `&M` is occurrence-local proof data. A declaration
row binder is not a mutable solver cell, and a provisional OperationGraph
classifier is not proof authority.

When a fold handles `op`, it removes the outer atom. Latent row `E1` re-enters
the carrier only if the selected clause body forces or explicitly interprets
the thunk. Generic forwarding remains opaque through `THUNK`.

## 3. Non-goals

- No new runtime higher-order-request tag.
- No implicit traversal through `THUNK`.
- No global promotion of symbolic effects to DefEq.
- No linear/resource-safety claim from effect-row closure alone.
- No backward compatibility for artifact v58 or older proof schemas.
- No changes outside `src/prototype/` except documentation and prototype test
  expectations.

## 4. Required Invariants

1. `OPERATION_REQUEST_INTRO` independently proves the selected argument
   occurrence classifier.
2. The request's OperationGraph node is rebound to the classifier concluded by
   that proof before an enclosing fold is reified.
3. A fold never records a premise against a provisional request classifier.
4. `EFFECT_ROW_OPERATION(op,E)` is static row data and does not execute `E`.
5. Removing a handled atom does not recursively remove effects from `E`.
6. Clause-body effects determine whether a forced latent computation contributes
   to the output carrier.
7. Artifact readback can reconstruct every request and fold proof without
   trusting serialized classifier claims.
8. Partial policy may retain genuinely unsupported symbolic equations; strict
   policy may not accept unresolved proof authority.

## 5. Stage Plan

| Stage | Deliverable | Status |
|---|---|---|
| A | Freeze proof schema and tests | complete |
| B | Three-premise request proof | complete |
| C | Authoritative request binding in OperationGraph | complete |
| D | Occurrence-local atom generation | complete |
| E | Structural handled-atom residual and carrier solving | complete |
| F | Artifact replay and adversarial validation | complete |
| G | Full regression and documentation audit | complete |

## 6. Stage A: Proof Schema

The new request proof has three premises:

```text
op arg : Comp({op}, B)
arg    : Thunk(Comp(E, A))
k      : Thunk(Pi(B, Comp(F, C)))
---------------------------------- OPERATION_REQUEST_INTRO
request(op,arg,k) : Comp(scope_op<E> union F, C)
```

For first-order operations, the same three-premise shape is retained. The
argument premise verifies the application occurrence but does not create a
parameterized atom.

Acceptance:

- [x] producer records application, argument, and continuation premises;
- [x] validator checks APP operands and all three premise subjects;
- [x] validator derives the latent row from the argument classifier;
- [x] OperationGraph matching accepts only the new proof shape;
- [x] old two-premise artifacts are rejected by artifact v59.

## 7. Stage B and C: Authority Ordering

Computation constraints are dependency ordered:

1. solve request occurrences whose APP and continuation premises are ready;
2. bind their proven classifier to the corresponding OperationGraph occurrence;
3. solve folds only from proven child classifiers;
4. reify fold proofs after child bindings stabilize.

The implementation must not repair stale premise IDs with normalization-only
conversions. Such a conversion can prove equivalent classifiers but cannot
prove that the fold consumed the selected source occurrence.

Acceptance:

- [x] request constraints run before consuming folds in each fixed-point pass;
- [x] request operations publish the proof-selected classifier;
- [x] fold proof materialization reads the proven child classifier;
- [x] no proof edge references a classifier for which the child has no proof;
- [x] shared Core terms with different occurrence classifiers remain valid.

## 8. Stage D: Atom Generation

For declarations whose argument schema is a thunked computation, request
inference constructs:

```text
EFFECT_ROW_OPERATION(operation_identity, argument_comp_effect_row)
```

The ordinary application classifier remains the declaration's first-order
result. Only the request occurrence receives the parameterized atom. This keeps
function application and operation-tree construction distinct.

Acceptance:

- [x] two request occurrences with different latent rows produce distinct atoms;
- [x] the operation declaration remains quantified and unchanged;
- [x] closed-bit extraction reports an atom as symbolic;
- [x] normalization equality compares operation identity and latent row.

## 9. Stage E: Handler Residual

Residual subtraction is structural:

- closed labels remove matching handled labels;
- unions recurse and rebuild canonically;
- `scope_op<E>` is removed when `op` is handled;
- unmatched atoms remain unchanged;
- row variables remain residual unless solved independently.

Removing `scope_op<E>` does not inspect `E`. If a clause forces its argument,
the existing `FORCE` derivation contributes `E` through the clause-body row.
Discarding the thunk contributes nothing. Explicit inner folds contribute their
own proven output row.

Acceptance:

- [x] discard removes the atom and does not expose `E`;
- [x] force once exposes `E` through the clause body;
- [x] repeated force joins `E` idempotently for unrestricted thunks;
- [x] opaque forwarding retains the atom;
- [x] strict policy accepts all closed instances.

## 10. Stage F: Artifact Contract

The proof schema change bumps the artifact format to v59. Readback validates:

- operation identity and parameterized-atom latent row;
- request APP, argument, and continuation premise correspondence;
- argument thunk classifier and latent-row derivation;
- fold premise classifiers after request authority binding;
- row residual and inclusion evidence used by the final carrier.

Acceptance:

- [x] valid v59 artifacts round-trip;
- [x] v58 is rejected;
- [x] a forged atom operation identity is rejected;
- [x] a forged latent-row reference is rejected;
- [x] a forged request argument premise is rejected;
- [x] linked artifacts preserve occurrence-local atoms.

## 11. Permanent Test Matrix

- [x] first-order `print` request retains ordinary closed row;
- [x] unhandled higher-order request prints `EFFECT_ROW_OPERATION`;
- [x] handler discard case is strict and does not run the inner effect;
- [x] force-once case is strict and runs the inner effect once;
- [x] force-twice case is strict for unrestricted thunk and runs twice;
- [x] two occurrences retain different latent rows;
- [x] explicit inner fold remains independently typed;
- [x] opaque forwarding does not cross the thunk boundary;
- [x] all prototype shell tests pass;
- [x] examples 01-07 and 09, the numbered examples currently present through
  09, pass under strict policy.

## 12. Progress Log

### 2026-08-05: Plan created

- Converted the authority blocker from the decision record into an ordered
  implementation plan.
- Fixed the intended proof schema at three premises.
- Fixed the artifact migration target at v59.
- Deferred implementation status changes until each acceptance command passes.

### 2026-08-05: Request authority and structural residual completed

- `OPERATION_REQUEST_INTRO` now records APP, selected argument occurrence, and
  continuation premises.
- Higher-order request rows are derived from the selected thunk argument as
  `EFFECT_ROW_OPERATION(operation_id, latent_row)`.
- OperationGraph lowering facts remain fixed-point seeds, but a validated
  request/fold proof now replaces that occurrence's seed and solved classifier.
- APP result inference was separated from request-tree classification; a
  higher-order request no longer reuses its parameterized row as the APP result.
- Fold residual subtraction now removes an outer handled atom without entering
  its latent row. Clause-body `FORCE` proofs are the only route by which that
  latent row joins the output carrier.
- The proof validator independently rebuilds the same structural carrier and
  rejects a serialized fold conclusion that does not match it.

### 2026-08-05: Authority boundary and regression completion

- Effect-row residual subtraction moved to `term.c` as the shared structural
  operation used by both typing validation and the occurrence-level effect
  solver. The two components no longer carry separate higher-order residual
  rules.
- Effect constraints are rebuilt from the current fixed-point classifier state
  on each generation pass. Constraints from an earlier symbolic approximation
  are not retained as false residual obligations.
- Structural self-equality closes only ground effect rows. A row containing an
  unresolved variable, such as an external declaration's unknown effect, stays
  residual and remains rejected by strict policy.
- Every operation request reconstructs its internal `APP(operation, argument)`
  proof in the request occurrence's context. It no longer borrows a proof for
  an alpha-shared Core APP from a sibling lambda occurrence.
- Generic inference can temporarily create a derivation in a context later
  superseded by OperationGraph elaboration. Before commit, conclusions with no
  complete premise tuple in their declared context are removed transitively;
  supported occurrence-specific derivations remain.
- Resolved source-context classifier provenance is compile-time data. Artifact
  graph append retains the concrete classifier and discards that provenance;
  only genuinely unresolved classifier variables are relocated.
- Added `higher_order_operation_distinct_latent_check.p` and strict assertions
  for unhandled, handled, force-once, and force-twice higher-order requests.
- Added artifact mutation tests for operation identity, latent-row references,
  and request argument premise integrity.
- Artifact flow, CBPV boundary/surface, computation block, constructor
  polarity, context category, dependent Pi, resumption multiplicity, and shared
  Core occurrence test scripts all pass.
- Examples `01_bool.p` through `07_add.p` and `09_list_induction.p` pass under
  strict policy. There is no numbered example 08 in the repository.

## 13. Deviations

- The initial implementation attempt stopped seeding request/fold classifiers.
  That made the OperationGraph fixed point unable to reach the Judgement pass.
  The final design retains lowering-time seeds and gives validated rule-specific
  proofs explicit replacement authority.
- A handler clause is permitted even when its operation does not occur in the
  input row. Residual subtraction therefore removes matching occurrences but
  does not require every declared clause label to be present.
- `prototype_context.classifier_variable` currently serves as unresolved
  classifier identity and, during compilation only, resolved source-binder
  provenance. Artifact linking deliberately drops the latter. A future naming
  cleanup may split those roles, but no artifact or kernel decision depends on
  resolved provenance after compilation.

## 14. Remaining Semantic Boundary

This migration supports higher-order operation arguments encoded as thunked
computations and gives their latent rows occurrence-local static authority. It
does not by itself define a general higher-order handler calculus:

- a first-order fold does not recursively enter an unforced thunk;
- unrestricted thunks may be forced zero, one, or many times, so the effect row
  records possibility rather than multiplicity;
- linear or affine resource usage requires a separate usage discipline;
- forwarding an unknown scoped higher-order operation still needs an explicit
  policy for whether the handler is installed inside the forwarded thunk.

These are deliberate boundaries, not unresolved proof obligations in the
implemented first-order fold plus thunk encoding.
