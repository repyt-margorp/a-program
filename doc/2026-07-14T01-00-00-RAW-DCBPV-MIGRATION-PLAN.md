# Raw dCBPV- Migration Plan

Date: 2026-07-14

Status: raw dCBPV- migration implemented for the current prototype surface.

## 2026-07-15 Completion Audit

The raw dCBPV- rules listed below are implemented and covered by the current
prototype regression suite.

| Requirement | Current evidence | Status |
| --- | --- | --- |
| Distinguish `Comp(E, A)` from raw `Pi(A, B)` | `prototype_term_classifier_view`; effect-row helpers reject non-returning computations | implemented |
| Raw lambda in a computation context | raw function, currying, and raw Match-branch tests | implemented |
| Value function force boundary | higher-order function test checks `FORCE(VAR(...))` | implemented |
| Generalized `BIND` | `bind-function` and `dependent-bind` check a negative `Pi` result | implemented |
| Match motive codomain is a computation type | raw branch call, dependent motive, and guarded recursive motive tests | implemented |
| Pure type dependency | Sigma dependent-field and effectful-type-family rejection tests | implemented |
| Effect boundary | pure intrinsic, perform/handler, and raw-Pi handler rejection tests | implemented |
| Artifact TypeView consistency | CBPV/artifact flow and imported classifier regressions | implemented |

Two architectural items remain explicitly unfinished. They do not alter the
raw dCBPV- core terms emitted by the covered surface programs, but prevent
describing the compiler as a fully classifier-directed elaborator and proof
artifact model:

1. Application lowering retains `compile_ref.computation_kind` as a
   pre-solver occurrence candidate. It is no longer a direct TermDB tag test,
   but the final `Pi` versus `Thunk(Pi)` decision is not reified from solved
   operation constraints for every unannotated nested application.
2. Judgement materialization still concludes over an erased core TermDB node.
   Shared alpha-equivalent core terms need stable operation-occurrence proof
   subjects before every typed occurrence can be represented without the
   current representative-body workaround.

These are the next compiler-architecture changes. They are not hidden
compatibility encodings in the current surface tests, but they are material
limitations for a future fully bidirectional dCBPV elaborator and proof
artifact model.

## Goal

Replace the current hybrid lowering with a raw call-by-push-value core while
preserving the existing A Program surface syntax where it is unambiguous. The
target is the value-dependent fragment of dependent CBPV (dCBPV-): classifiers
and motives may depend on values, never on the execution of a computation.

This plan deliberately does not retain a compatibility path for the old
value-first nested-lambda encoding.

## Target Kind Discipline

The implementation must distinguish value types from computation types.

```text
ValueType A ::= Data | Thunk(B) | ...
ComputationType B ::= Comp(E, A) | Pi(A, B) | HandlerType(...) | ...
```

`Comp(E, A)` is only the computation type that returns a value. It is not the
universal representation of a computation type. In particular, a lambda has
the raw computation classifier `Pi(A, B)`.

```text
Gamma, x : A |- computation M : B
----------------------------------- lambda
Gamma |- LAMBDA(x, M) : Pi(A, B)

Gamma |- value V : A
--------------------- return
Gamma |- RETURN(V) : Comp({}, A)

Gamma |- computation M : B
-------------------------- thunk
Gamma |- THUNK(M) : Thunk(B)

Gamma |- value V : Thunk(B)
--------------------------- force
Gamma |- FORCE(V) : B
```

Only a value context inserts `THUNK`. Only a computation context expecting
`Comp(E, A)` inserts `RETURN` around a value. A computation context expecting
`Pi(A, B)` lowers a lambda directly.

## Application and Sequencing

Application must be classifier-directed, not core-tag-directed.

```text
f : Pi(A, B)              v : A
-------------------------------- APP
APP(f, v) : B[v]

f : Thunk(Pi(A, B))       v : A
-------------------------------- value-function APP
APP(FORCE(f), v) : B[v]
```

Constructor application remains a value application. The decision is made from
the selected occurrence classifier and constructor ownership, not by treating
every Pi as a raw function.

Sequencing consumes a value-returning computation, but its body may have any
computation type.

```text
M : Comp(E, A)       Gamma, x : A |- N : B
------------------------------------------- bind
BIND(M, x, N) : bind_result(E, B)
```

For `B = Comp(E2, C)`, `bind_result(E, B)` is `Comp(E union E2, C)`. For a
negative computation type such as `Pi(A, B')`, it is generalized sequencing:
the binder is pushed under the negative introduction. This must be implemented
structurally, not approximated by wrapping the result in `Comp`.

## Match and Iota

Match eliminates a value. If surface syntax supplies a computation scrutinee,
lowering first sequences it into a value binder.

```text
Motive : ScrutineeType -> ComputationType
branch_i : Motive(constructor_i(fields))
------------------------------------------------
MATCH(scrutinee, branches) : APP(Motive, scrutinee)
```

The motive codomain is any computation type. Therefore a curried function is
represented without an artificial value boundary.

```text
zero => LAMBDA(m, RETURN(m))
```

Iota reduction only chooses and substitutes the branch. It neither inserts nor
removes `RETURN` or `THUNK`.

```text
MATCH(CONSTRUCTOR(C, values), ..., C(fields) => body, ...) --> body[values/fields]
```

Unsolved motive equations remain solver-local. They are materialized as a
TermDB motive only after a finite solution is justified.

## dCBPV- Boundary

Types and motives may refer to lambda, Match-pattern, and Bind-result values.
They may not refer to an unexecuted computation or make conversion execute an
operation. `PURE_TYPE_WHNF` remains effect-free.

Dependent sequencing beyond this value boundary is deferred. In particular,
an effectful computation result does not become a globally available type index
merely because it is bound at runtime.

## Required Changes

1. Add an internal computation-kind classifier API that distinguishes
   `Comp(E,A)`, `Pi(A,B)`, and handler computation types without collapsing
   them into `effect_row/result` fields.
2. Change source lowering so `AST_LAMBDA` in a computation context uses the
   raw lambda path. Keep lambda-to-thunk lowering solely in value contexts.
3. Replace `compile_ref_is_raw_function` term-tag tests with occurrence
   classifier/polarity tests. A partially applied raw function remains raw.
4. Generalize BIND typing and normalization over negative computation types.
   `Comp`-specific effect-row union remains a case of this operation.
5. Generalize Match motive solver and materialization so the codomain is a
   computation type, not necessarily `Comp(E,A)`.
6. Keep operation requests as `Comp(E,A)` requests with thunked continuations.
   Do not use `perform` to model pure intrinsics in the intended surface
   language; pure intrinsics use ordinary application and execution reduces
   that application to `RETURN(result)`. `PURE_TYPE_WHNF` never performs this
   intrinsic reduction.
7. Restrict source/Judgement materialization to selected source operations or
   source-reachable terms. TermDB may retain evaluation intermediates, but
   they must not independently acquire JudgementDB derivations.
8. Make the typed proof subject an occurrence, not only an erased core term.
   An `OperationGraph` occurrence must retain its selected classifier and its
   source child occurrences when it is materialized into a proof.  A shared
   `LAMBDA` core node can point at the representative body chosen by TermDB
   hash-consing, while another source occurrence of that lambda has a distinct
   `RETURN(VAR)` child and a distinct classifier.  The lambda-introduction
   proof must refer to the latter occurrence, not silently recover the former
   from `core_term.as.lambda.body`.

## Surface Syntax Policy

The existing syntax remains the default:

```text
lambda:   \x : A => body
apply:    f x
match:    value @case => body
bind:     let x <- computation in body
effect:   perform (operation argument)
handler:  handle computation with ...
```

`return`, `thunk`, and `force` remain accepted internal/debug syntax while the
elaborator inserts them from kind requirements. They are not required in normal
pure source programs. Future surface syntax must make latent effect rows and
explicit computation/thunk types expressible, but that is not a prerequisite
for the raw-core migration.

## Current Hybrid Paths To Remove

The current core nodes and evaluator already implement the essential CBPV
cuts: `BIND(RETURN(V), x, N)`, `FORCE(THUNK(M))`, operation requests with
thunked continuations, and Match sequencing of a computation scrutinee.  The
following lowering and typing paths are not part of the target discipline.

1. `compile_ast_computation_ref` has a value fallback that always emits
   `RETURN(value)`.  It does not know whether the required computation type is
   `Comp(E, A)` or `Pi(A, B)`.  Consequently a nested lambda in a Match branch
   becomes `RETURN(THUNK(LAMBDA(...)))` instead of a raw `LAMBDA(...)`.
2. `compile_ref_is_raw_function` identifies a raw function by the TermDB tag
   `LAMBDA`.  The application lowering also has special fast paths when the
   source function is a top-level `NAME`.  Both are representation and source
   position tests, not classifier-directed CBPV elaboration.  They fail for
   partial raw applications and make alpha-equivalent source arrangements
   lower differently.
3. `compile_ast_ref` probes a value lowering first and only then a computation
   lowering.  That is useful as a temporary parser-facing ambiguity resolver,
   but cannot be the final kind checker: a term must be elaborated against an
   expected value or computation kind.
4. `prototype_term_classifier_view` collapses every computation classifier
   into `{effect_row, result}`.  It represents `Pi(A, B)` by putting the whole
   Pi in `result` and an invalid effect row.  `solve_bind_constraint` and the
   handler solver then assume that every computation has a concrete effect row
   and can be rebuilt as `Comp(E, A)`.  This is the direct cause of the nested
   function and generalized-sequencing failure.
5. Match result/motive solving is currently tuned to value-returning
   computations.  A Match may syntactically contain raw branch lambdas, but
   the motive must be allowed to return any computation type before that is
   sound.
6. Constructor and intrinsic application are selected by AST/head-shape
   helpers (`ast_application_head_is_constructor`,
   `ast_application_head_is_host_operation`) and constructor application is
   recognized again from the TermDB head.  Constructor/value application is a
   real CBPV distinction, but the final decision must come from the resolved
   occurrence classifier and constructor ownership.  The host-operation path
   must not make an ordinary pure intrinsic application into an operation
   request.
7. Ascription and explicitly typed definitions now select value versus
   computation lowering from `prototype_judgement_classifier_view`, after
   normalizing the annotation. This is the first expected-kind boundary, but
   unannotated nested occurrences still use generic fallback lowering.
8. `dependent-bind` deliberately asserts `RETURN(THUNK(LAMBDA(...)))` for a
   lambda that is returned as the *value result* of a computation. That
   encoding is valid CBPV. The test must be split from raw-function
   regressions so it cannot be mistaken for evidence that a lambda in a
   negative-computation context should be wrapped.
9. Judgement materialization still uses a core TermDB node as the proof
   conclusion subject.  This is not sufficient for shared erased terms.  It
   fails at an artifact boundary: after importing `idNat : Nat -> Nat`, a
   source `\\x : #.Nat => x` can reuse the same core `LAMBDA`, even though its
   source `RETURN(VAR)` occurrence and classifier are different.  The current
   lambda proof then expects the imported core body's judgement but is given
   the local body's classifier.  This is a proof-graph inconsistency, not a
   valid reason to reject the program.  The operation/annotation layer needs
   stable occurrence identities before artifact compatibility can be claimed.

The following are valid source elaborations and should remain, but must become
expected-kind-directed rather than fallback rules:

- A value in a context requiring `Comp({}, A)` elaborates to `RETURN(value)`.
- A raw computation in a context requiring `Thunk(B)` elaborates to
  `THUNK(computation)`.
- A computation scrutinee used by Match is sequenced to a value before Match.
- A value-held function is forced when its classifier is `Thunk(Pi(A, B))`.

Single-operation handlers and implicit latent effect-row binders are current
scope limits, not alternative CBPV semantics.  They remain supported during
this migration, but handler generalization is separate from making the core
raw.

## 2026-07-15 Audit: Import and Surface Elaboration

The following audit separates valid elaboration from temporary source-shape
shortcuts.  The shortcuts must be removed as expected-kind-directed lowering
is introduced.

1. `compile_ast_ref` and `compile_ast_value_then` still try value lowering
   before computation lowering.  This is an ambiguity resolver for the
   current surface grammar, not a dCBPV typing rule.
2. `compile_ref.computation_kind` and several AST/head helpers infer whether
   an occurrence is a raw function.  A final implementation must derive this
   from the selected occurrence classifier.  In particular, a type former
   can have a Pi *formation classifier* while its TypeView remains a value.
3. Constructor and pure-intrinsic spines have dedicated lowering paths.
   Constructors are values and pure intrinsics are ordinary applications, so
   both distinctions are semantically valid.  Their selection must move from
   AST/head tags to the resolved classifier and declaration kind.
4. Ascription and typed definitions now choose their polarity from the
   normalized classifier view, including an alias that reduces to `Pi`. This
   is intentionally narrower than a complete expected-kind API: nested,
   unannotated occurrences still have no caller-supplied expected kind.
5. Type-level lowering still has a special opening for pure
   `THUNK(LAMBDA(...))` families.  This is permitted only as an instance of
   the common pure normalizer, never as a second type-only reduction rule.
6. An unannotated top-level source `LAMBDA` is directly lowered as a raw
   computation. This is the raw CBPV meaning of lambda, not an annotation
   workaround. Typed definitions now obtain their expected classifier first;
   the remaining work is to give unannotated nested occurrences the same
   expected-kind API.
7. `compile_ast_function_ref` dispatches on source tags (`LAMBDA`, `NAME`,
   `MATCH`, `BIND`, and `INDUCTION_HYPOTHESIS`) to decide whether an
   application head is raw. This is a transitional heuristic. In the final
   system, the selected occurrence classifier decides whether an application
   head is `Pi(A, B)` or `Thunk(Pi(A, B))`.
8. `compile_ast_value_then` implicitly inserts `BIND` after any source form
   which cannot lower as a value. Sequencing a computation to obtain a value
   is valid surface sugar, but the current value-first probe fixes evaluation
   order and elaboration before the expected kind is known. It must become an
   explicit expected-value rule, not a generic fallback.
9. `INDUCTION_HYPOTHESIS` is provisionally marked as a function computation
   before its motive equation has been solved. This is necessary for the
   current guarded-IH prototype, but not a dCBPV rule. The constraint solver
   must eventually establish the computation kind from the solved motive.
10. `bind_cbpv_operation_classifiers` skips a nonrepresentative shared lambda
   body while materializing proofs. This protects current examples from an
   erased-core occurrence mismatch, but is a proof-identity workaround rather
   than a lowering rule. Stable operation-occurrence proof subjects are still
   required.

### Imported TypeView Invariant

An imported artifact is appended before source lowering.  Its exported term
classifiers and constructor classifier families therefore refer to relocated
TermDB TypeViews.  A constructor selected through an imported namespace must
use the same relocated TypeView instance as its owner.  It must not use an
`EXTERNAL_REF` owner while its classifier family returns a local TypeView:
that split makes a direct application such as `idNat Nat.zero` fail both the
Pi domain check and constructor-introduction proof validation.

`EXTERNAL_REF` remains the name-layer reference emitted for imported terms and
artifact dependencies.  It is not a substitute for the TypeView used in the
core classifier graph during this compilation unit.

## Verification

The implementation is complete only when all of the following hold:

1. `\n : Nat => n @zero => \m : Nat => m ...` lowers with raw nested branch
   lambdas, not `RETURN(THUNK(LAMBDA(...)))`.
2. A Match whose branches are raw lambdas can itself be directly applied:
   `((choose Nat.zero) Nat.zero)` lowers to nested `APP` over the raw Match,
   without an intervening returned thunk.
3. `((add two) three)` performs direct computation-function application; it
   does not bind a returned thunk solely to call the next argument.
4. A higher-order parameter still receives a thunked function and is forced
   exactly once at its call site.
5. A Match with a computation scrutinee sequences that scrutinee before iota.
6. `01` through `07`, `09`, `training/double.p`, artifact round trips, and new
   raw-function/effect-row regressions pass.
   `training/dependent_match.p` and
   `training/recursive_dependent_match.p` are also permanent Match-motive
   regressions: they require a non-uniform motive and a guarded recursive
   motive respectively.
7. `PURE_TYPE_WHNF` does not dispatch host effects.
8. Importing a transparent artifact that exports `idNat : Nat -> Nat` does
   not prevent a local `idInt : #.Int => #.Int` or `idNat2 : NatAlias ->
   NatAlias` from using the same erased identity core.  Each typed occurrence
   must retain a valid, separate proof graph.
9. `handle` rejects a raw `Pi` computation as its input. Handlers consume
   value-returning `Comp(E, A)` computations only; they do not provide an
   implicit effect row for a negative function computation.

## 2026-07-14 Implementation Progress

The following parts are now implemented in `src/prototype/`.

1. `prototype_term_classifier_view` distinguishes value-returning
   computations from raw `Pi` computation functions.
2. Lambda lowering in a computation context builds `LAMBDA` directly;
   lambda lowering in a value context still builds `THUNK(LAMBDA(...))`.
3. BIND typing is generalized structurally over a negative `Pi` result, and
   evaluation pushes BIND under a lambda rather than inventing a returned
   thunk.
4. Match records a function-computation candidate only when all branches
   lower as function computations. Its classifier remains solver-owned.
5. The old TermDB-head test in `compile_ref_is_raw_function` and the
   name-specific application fast paths were removed. Lowering instead carries
   a temporary `compile_ref` computation kind through a source occurrence.
   This is not a published type fact and is checked later by the Pi/App
   constraints.

The candidate field is a transition to expected-kind-directed elaboration,
not the final classifier-directed API. It is necessary because graph lowering
precedes fixed-point classifier solving. The final form must request a value
or computation kind at each lowering boundary rather than infer it from the
source form.

The following currently pass after this change:

- examples `01` through `07` and `09`
- `training/double.p`
- raw lambda, currying, higher-order function, generalized BIND, dependent
  Sigma constructor, and effect-row regressions in
  `src/prototype/test_cbpv_surface.sh`
- `src/prototype/test_cbpv_boundary.sh`

The 2026-07-15 expected-kind entry point now compiles a typed definition or
ascription against the normalized annotation classifier before lowering its
body. A `Pi` selects computation lowering, while a value classifier selects
value lowering. This removes the prior arrow-AST and top-level annotation
ordering shortcuts, while preserving bare source lambda as a raw computation.
It does not yet replace `compile_ast_ref` / `compile_ast_value_then` for
unannotated nested surface occurrences.

The effect-row boundary is now explicit in `typing.c`: effect-row union,
`perform`, `handle`, and their proof validators require
`ComputationKind::RETURNING`, namely `Comp(E, A)`. A raw `Pi(A, B)` is a
computation but has no effect row and cannot enter those rules. Generalized
`BIND` remains the sole structural rule which may return a negative `Pi`
codomain.

### dCBPV- Type-Family Boundary

The following dependent constructor is a required regression:

```text
Sigma := \A : @ => \B : A -> @ => @{ mk : (a : A) -> B a -> *; };
ConstNat := \x : Nat => Nat;
main := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
```

`ConstNat` lowers as a raw computation lambda whose body is `RETURN(Nat)`.
Type-family application now has an explicit Lowering boundary: in a type
formation context only, a `THUNK(LAMBDA(...))` is opened for
`PURE_TYPE_WHNF`, and a resulting `RETURN(Type)` is read as that type value.
This is not runtime `FORCE`; the normalizer remains effect-free and an
operation request cannot become a classifier. The regression verifies that
the second `mk` field has domain `Nat` after applying `B` to its first field.
The companion negative regression verifies that a type family whose body
performs `#.print` is rejected.

This supports pure dependent constructor families such as Sigma. IADT index
refinement, general recursive motives, and dCBPV+ dependency on computations
remain deferred.

### Pure Intrinsic Execution Boundary

An intrinsic with an empty effect row, such as `#.int64_add`, is an ordinary
application with classifier `Comp({}, Int64)`. Runtime evaluation reduces a
fully applied intrinsic spine to `RETURN(result)`. This keeps pure arithmetic
out of the operation-request/handler path. `perform` still constructs an
`OPERATION_REQUEST` and is required for operations which carry an effect row.
The pure-intrinsic evaluator flag is deliberately absent from all WHNF and
`PURE_TYPE_WHNF` profiles, so conversion cannot invoke a host intrinsic.

## Deferred Work

General recursive motive equations, multiple recursive calls, Sigma/IADT,
explicit effect-row syntax, multi-operation handlers, and higher dCBPV+
dependent Kleisli extensions remain separate follow-up work.
