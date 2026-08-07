# Shared-Term HOTT and Dependent-CBPV Normative Calculus

Date: 2026-08-07

Status: frozen V2-T1/T2 first-fragment specification

Implementation plan:

- `doc/2026-08-07T01-00-00-SHARED-TERM-HOTT-DCBPV-V2-T1-T2-PLAN.md`

Baseline:

- branch: `main`;
- source baseline: `0f94680de93be47a810753db0573d76de6509f82`;
- artifact format: `A_PROGRAM_ARTIFACT 61`;
- ContextDB/SubstitutionDB extraction: commit `77083ea`.

This document freezes the semantic input required by V2-S1. It does not add
object-equality syntax, equality TermDB tags, or artifact v62 records.

## 1. Architectural Axiom

A Program has one shared TermDB syntax. Value and computation are classifier
classes and typed-occurrence judgements over that syntax. They are not two
copies of Lambda calculus.

The following duplication is forbidden:

```text
VALUE_PI                 COMPUTATION_PI
VALUE_LAMBDA             COMPUTATION_LAMBDA
VALUE_APP                COMPUTATION_APP
VALUE_MATCH              COMPUTATION_MATCH
```

The existing F/U boundary constructors remain semantically significant:

```text
COMPUTATION_TYPE(E, A)
RETURN(a)
THUNK_TYPE(C)
THUNK(m)
FORCE(v)
```

They express movement across the CBPV adjunction. They are not alternate
representations of APP, LAMBDA, PI, or MATCH.

TermDB interning may identify alpha-equivalent erased graph nodes. Every
typing or observational claim is nevertheless indexed by an explicit context
and classifier.

## 2. Judgement Forms

### 2.1 Context and substitution

```text
Gamma ctx
Delta |- sigma : Gamma
```

The second judgement means `sigma : Delta -> Gamma`. Reindexing is
contravariant:

```text
Gamma |- t : T        Delta |- sigma : Gamma
---------------------------------------------
Delta |- t[sigma] : T[sigma]
```

### 2.2 Classifier classes over shared terms

```text
Gamma |-v A type
Gamma |-c C ctype
Gamma |-v t : A
Gamma |-c t : C
```

The `v` and `c` markers classify a judgement. Both `t` metavariables range over
the same TermDB. OperationGraph polarity records which judgement a source
occurrence is intended to satisfy.

The current JudgementDB encodes both term forms through `HAS_TYPE` and both
formation forms through `IS_TYPE`. Until V2-P1, the classifier tag and proof
kind recover the `v`/`c` distinction.

### 2.3 Kernel conversion

```text
Gamma |- T ==def U
```

This is a meta-level, profile-indexed conversion judgement. Its implementation
returns structured outcomes:

```text
EQUAL
NOT_EQUAL
RESIDUAL
BLOCKED_EFFECT
EXHAUSTED
INVALID
```

Only `EQUAL` may discharge a conversion premise. No result extends a global
equivalence relation and no result is automatically an object witness.

### 2.4 Observational families and witnesses

For an admitted value classifier:

```text
Gamma |-v A type
Gamma |-v a0 : A
Gamma |-v a1 : A
--------------------------------
Gamma |-v ObsV(A, a0, a1) type
```

A higher observational witness is an ordinary value term:

```text
Gamma |-v p : ObsV(A, a0, a1)
```

Pure computations use an auxiliary typed construction judgement:

```text
Gamma |-c m0 : C
Gamma |-c m1 : C
--------------------------------
Gamma |- ObsC(C, m0, m1) observational-goal
```

`ObsC` is not an untyped Boolean comparison. For the admitted fragment it
constructs the value-family required to observe suspended computations. For an
effectful or unsupported computation it returns a typed residual.

No generic `JUDGEMENT_EQ` is added. Future concrete `ObsV` types and their
witness terms are checked through ordinary `IS_TYPE` and `HAS_TYPE`
derivations.

## 3. Context Bridge and Higher Closure

Higher observational structure is represented recursively, not by truncating
to a numeric maximum dimension.

For every admitted context `Gamma`, define a bridge context `Gamma^R` with two
endpoint substitutions:

```text
pi0_Gamma : Gamma^R -> Gamma
pi1_Gamma : Gamma^R -> Gamma
```

The empty context is mapped to the empty bridge. Context extension is mapped
by:

```text
(Gamma.A)^R =
    Gamma^R
  . A[pi0_Gamma]
  . A[pi1_Gamma]
  . AR
```

where `AR` is the observational family for the two endpoint values of `A`.

For every substitution `Delta |- sigma : Gamma`, its action is:

```text
Delta^R |- sigma^R : Gamma^R
```

and must satisfy:

```text
(id_Gamma)^R == id_(Gamma^R)
(sigma o tau)^R == sigma^R o tau^R
pi0_Gamma o sigma^R == sigma o pi0_Delta
pi1_Gamma o sigma^R == sigma o pi1_Delta
```

For every admitted type and term:

```text
Gamma |-v A type      gives Gamma^R |-v A^R type
Gamma |-v a : A       gives Gamma^R |-v a^R : A^R(a[pi0], a[pi1])
```

The same action applies when `A` is itself an admitted observational witness
type. Therefore witnesses can have non-trivial witnesses between them. The
implementation must not replace their equality by Unit, proof irrelevance, or
raw TermDB identity.

The bridge context and endpoint substitutions, rather than a small integer
`dimension`, are the authoritative boundary description passed to future HOTT
goals. A parent-goal edge records recursive higher structure when needed.

## 4. Shared-Term Typing Rules

### 4.1 Pi, Lambda, and APP

The current Pi is a computation-function classifier:

```text
Gamma |-v A type
Gamma.A |-c C ctype
-------------------------------- PI-FORM
Gamma |-c Pi(A, C) ctype

Gamma.A |-c m : C
-------------------------------- PI-INTRO
Gamma |-c Lambda(x, m) : Pi(A, C)

Gamma |-c f : Pi(A, C)
Gamma |-v a : A
-------------------------------- PI-ELIM
Gamma |-c App(f, a) : C[<id_Gamma, a>]
```

`PI`, `LAMBDA`, and `APP` are the existing shared TermDB tags. A function
available as a value has classifier `ThunkType(Pi(A,C))`; no value Pi is
introduced.

### 4.2 Computation return

```text
Gamma |-v A type
Gamma |-v a : A
-------------------------------- RETURN-INTRO
Gamma |-c Return(a) : Comp(empty, A)
```

An unresolved effect row is not `empty`.

### 4.3 Thunk and Force

```text
Gamma |-c C ctype
Gamma |-c m : C
-------------------------------- THUNK-INTRO
Gamma |-v Thunk(m) : ThunkType(C)

Gamma |-v v : ThunkType(C)
-------------------------------- FORCE-ELIM
Gamma |-c Force(v) : C
```

The equations admitted by kernel computation remain:

```text
Force(Thunk(m)) ==def m
```

and the existing computation-fold/Return cut equation where its profile
permits it. This specification adds no host-effect reduction to DefEq.

### 4.4 ADT constructors and Match

A constructor declaration owns a parameter context, dependent field context,
and result classifier. A fully applied constructor spine is a value
introduction even though its fields use the shared APP node.

For a motive `M` over the scrutinee type:

```text
Gamma |-v s : D
Gamma.D |- K classifier-family
each constructor case is checked in its instantiated field context
------------------------------------------------------------------- MATCH
Gamma |- Match(s, cases) : K[<id_Gamma, s>]
```

The exact value/computation class of `K[<id,s>]` determines the Match
judgement. Uniform branch classifiers are not a primitive rule. They may be
observed only by normalizing the motive application.

Guarded IH references remain scoped to the owning Match frame and recursive
field. Reindexing a Match must remap the frame and every IH reference together.

## 5. Observational Rules for the First Fragment

### 5.1 Definitional diagonal

Every admitted type former supplies a diagonal witness operation:

```text
Gamma |-v a : A
-------------------------------- OBS-DIAGONAL
Gamma |-v diagonal_A(a) : ObsV(A, a, a)
```

If endpoints are only definitionally equal, conversion may be used as a
premise to check the diagonal witness at the requested endpoints:

```text
Gamma |-v diagonal_A(a) : ObsV(A, a, a)
a ==def a0
a ==def a1
-------------------------------- OBS-CONVERT
Gamma |-v diagonal_A(a) : ObsV(A, a0, a1)
```

`diagonal_A` is a rule family generated by admitted type-former semantics. This
checkpoint does not prescribe a single `REFL` TermDB tag.

### 5.2 Ordinary ADTs

For an ordinary point-constructor ADT, observation is generated from the
authoritative constructor telescope:

- equal constructor ordinals reduce to the iterated observational family of
  corresponding dependent fields;
- distinct constructor ordinals reduce to an uninhabited observational
  branch;
- dependent later fields are compared after transporting/reindexing along
  earlier field witnesses;
- recursive fields invoke the same generated observation recursively.

This is a generated Match-family rule, not raw constructor-node comparison.
Until the object representation for uninhabited and iterated observation is
added, the rule can be represented only as a typed HOTT construction goal.

### 5.3 Pure computation results

For `Comp(empty,A)`:

```text
m0 ==def Return(a0)
m1 ==def Return(a1)
-------------------------------- OBS-COMP-RETURN
ObsC(Comp(empty,A), m0, m1) = ObsV(A, a0, a1)
```

Both reductions must complete under the selected pure conversion profile and
deterministic budget. Neutral, exhausted, effect-blocked, or unsupported
computations produce a typed residual; they do not produce inequality and do
not fall back to graph identity.

### 5.4 Computation functions

For a pure computation-function classifier `Pi(A,C)`, function observation is
pointwise over related inputs:

```text
ObsC(Pi(A,C), f0, f1) =
  for a0 : A[pi0], a1 : A[pi1], ar : A^R(a0,a1),
  ObsC(C^R(ar), App(f0,a0), App(f1,a1))
```

This is a rule schema over the shared Pi/Lambda/App nodes. It does not create a
value Pi. A function value is observed through its existing thunk classifier:

```text
ObsV(ThunkType(Pi(A,C)), Thunk(f0), Thunk(f1))
  = ObsC(Pi(A,C), f0, f1)
```

Only statically pure codomain computations are admitted in the first fragment.

### 5.5 Thunked pure computations

For any admitted pure `C`:

```text
ObsV(ThunkType(C), Thunk(m0), Thunk(m1)) = ObsC(C, m0, m1)
```

If `C` has an effectful or unresolved row, or contains an operation request or
handler whose observational semantics is not selected, the result is
`RESIDUAL` or `UNSUPPORTED`.

### 5.6 Match

Observation commutes with Match only when all of the following are available:

- the scrutinee observation;
- the generated ADT constructor observation;
- the motive action;
- every case-body action in its reindexed case context;
- guarded recursive observation for every IH use.

Iota reduction may expose one case and simplify the goal. A neutral Match does
not become equal merely because its case arrays are alpha-equivalent.

## 6. Reindexing and Naturality Laws

All equalities in this section are typed definitional equations or tested
structural laws. They do not introduce object-equality witnesses by themselves.

### 6.1 Context laws

```text
t[id] = t
t[sigma o tau] = t[sigma][tau]
p o <sigma,t> = sigma
q[<sigma,t>] = t
```

### 6.2 Shared Pi/Lambda/App

```text
Pi(A,C)[sigma]
  = Pi(A[sigma], C[lift(sigma)])

Lambda(x,m)[sigma]
  = Lambda(x', m[lift(sigma)])

App(f,a)[sigma]
  = App(f[sigma], a[sigma])
```

The fresh binder `x'` may differ numerically. Alpha-canonical TermDB identity
or kernel conversion validates the resulting graph.

### 6.3 F/U boundary

```text
Comp(E,A)[sigma] = Comp(E[sigma], A[sigma])
Return(a)[sigma] = Return(a[sigma])
ThunkType(C)[sigma] = ThunkType(C[sigma])
Thunk(m)[sigma] = Thunk(m[sigma])
Force(v)[sigma] = Force(v[sigma])
```

### 6.4 Match and constructor telescopes

```text
Constructor(owner, ordinal)[sigma]
  = Constructor(owner[sigma], ordinal)

Match(s,cases)[sigma]
  = Match(s[sigma], cases[lift_case(sigma)])
```

Case lifting must preserve constructor owner, ordinal, recursive-field bits,
and scoped IH ownership while reindexing the owner and branch bodies.

### 6.5 Observational action

```text
(A[sigma])^R = A^R[sigma^R]
(a[sigma])^R = a^R[sigma^R]
ObsV(A,a,b)[sigma]
  = ObsV(A[sigma], a[sigma], b[sigma])
```

Ordinary `prototype_term_reindex` implements the left side's syntactic
substitution. A future observational-action service implements the `^R`
operation. They must remain separate APIs.

## 7. Purity and Residual Boundary

Effect-row classification has three semantic outcomes:

```text
PURE          the row is closed and its bitset is empty
EFFECTFUL     the row is closed and its bitset is non-empty
UNRESOLVED    the row contains a row variable, forall, or unresolved operation
```

`classifier_view.effects == 0` is not evidence of purity because the current
view uses zero as a conservative cached bitset when the row is unresolved.
Purity must be decided through the row structure.

The first HOTT fragment accepts `PURE`, rejects or residualizes `EFFECTFUL`
according to the requested rule, and always residualizes `UNRESOLVED`.

For dependent computation sequencing:

- a pure computation that normalizes to `Return(v)` may instantiate a result
  family with `v`;
- a computation term `m : Comp(E,A)` is never supplied directly to a family
  requiring `A`;
- an effectful or incomplete result creates a typed residual obligation;
- no subtyping rule converts an unresolved dependency into a static one;
- host dispatch is never run by the conversion kernel.

## 8. Frozen First-Fragment Matrix

| Structure | State | Reason |
| --- | --- | --- |
| Context variables and ordinary substitutions | ADMITTED | implemented CwF base |
| Ordinary value types already accepted by the kernel | ADMITTED | carrier-directed observation |
| Ordinary ADT point constructors | ADMITTED | generated telescope observation goal |
| Match over admitted ordinary ADTs | ADMITTED | motive/case/IH action required |
| Shared Pi/Lambda/App with pure codomain computations | ADMITTED | pointwise computation observation |
| `Comp(empty,A)` reducing to Return | ADMITTED | result observation |
| `Return` | ADMITTED | F introduction and naturality |
| `ThunkType`/Thunk/Force over admitted pure computations | ADMITTED | U/F observation laws above |
| Neutral pure computation not resolved within budget | RESIDUAL | no result value available |
| Closed non-empty effect row | RESIDUAL | handler-sensitive semantics absent |
| Unresolved effect row | RESIDUAL | purity is not established |
| Operation request and computation fold equality | DEFERRED | algebra/handler observation not frozen |
| Host primitive equality beyond DefEq literals | DEFERRED | logical refinement model absent |
| TypeView nominal equality as object equality | REJECTED | view identity is not a witness |
| Universe equality and univalence | DEFERRED | universe HOTT semantics absent |
| Quotients, path constructors, and HIT declarations | DEFERRED | declaration schema absent |
| IADT index-refining observation | DEFERRED | indexed constructor action not frozen |

## 9. Current Rule Ownership Inventory

| Structure | Graph construction | Typing/proof authority | Reindex/reduction | First-fragment status |
| --- | --- | --- | --- | --- |
| VAR/context extension | `term.c`, `context.c` | binder-assumption proofs in `typing.c` | `context.c`, bound-var substitution in `term.c` | ADMITTED |
| PI | `prototype_term_pi_family` | Pi formation and Lambda/App rules in `typing.c` | shared substitution and beta normalization in `term.c` | ADMITTED when pure |
| LAMBDA | `prototype_term_lambda` | `LAMBDA_INTRO` | shared substitution in `term.c` | ADMITTED when classifier is admitted |
| APP | `prototype_term_app` | `APP_ELIM`; occurrence application role in `ast.c` | shared substitution and beta normalization | ADMITTED when classifier is admitted |
| Constructor spine | constructor plus shared APP | constructor telescope and intro/spine proofs | telescope reindex in `context.c`; iota in `term.c` | ADMITTED |
| MATCH | `prototype_term_match_with_frame` | Match formation/elimination and solved motive proofs | scoped frame/IH substitution and iota in `term.c` | ADMITTED with generated action |
| IH | `prototype_term_induction_hypothesis` | guarded IH elimination | remapped with owning Match frame | ADMITTED only inside admitted Match |
| Effect rows | effect-row constructors in `term.c` | effect constraints in `typing.c` and `ast.c` | union/forall normalization and substitution | only closed empty row admitted |
| COMPUTATION_TYPE | `prototype_term_computation_type` | classifier views and CBPV rules | shared substitution | admitted only for empty row result fragment |
| RETURN | `prototype_term_return` | `RETURN_INTRO` | shared substitution and computation cut | ADMITTED |
| THUNK_TYPE/THUNK | corresponding `term.c` constructors | `THUNK_INTRO` | shared substitution | admitted only over admitted pure C |
| FORCE | `prototype_term_force` | `FORCE_ELIM` | shared substitution and force/thunk cut | admitted only over admitted pure C |
| Operation request | `prototype_term_operation_request` | request proof and effect constraints | substitution/runtime handling | DEFERRED |
| Computation fold | `prototype_term_computation_fold` | fold proof and residual obligations | substitution/cut/runtime handling | DEFERRED |
| TypeView | `prototype_term_type_view` | source occurrence/classifier selection | source-aware rebuild | object equality REJECTED |
| Host primitives | primitive constructors and declarations | intrinsic typing rules | selected host execution profiles | DEFERRED |

The inventory exposes one remaining ownership debt for V2-S1: classifier
constraints and motive equations are private positional records in `ast.c`.
They must not be reused as HOTT object goals.

## 10. V2-S1 Typed Goal Contract

V2-S1 must split four semantic records rather than extend one catch-all tuple.

### 10.1 Classifier unification goal

Compiler-local only:

```text
goal_id
source_operation
source_ast
context_id
classifier_variable
equation_kind
left_classifier
right_classifier
dependency_ids
state
```

This replaces the HOTT-irrelevant uses of positional `target/left/right/aux`.
It never denotes object equality.

### 10.2 Kernel conversion goal

Compiler-local unless retained as a proof premise:

```text
goal_id
context_id
carrier_classifier_or_invalid
left_term
right_term
normalization_profile
step_limit
result_status
result_reason
```

The carrier is mandatory when conversion is used under a type-directed rule.

### 10.3 HOTT construction goal

Compiler-local until V2-O1 defines object records:

```text
goal_id
goal_kind: VALUE_OBSERVATION | COMPUTATION_OBSERVATION |
           TYPE_ACTION | TERM_ACTION
context_id
carrier_classifier
left_endpoint
right_endpoint
bridge_context_id
left_endpoint_substitution
right_endpoint_substitution
parent_goal_id_or_invalid
type_former_rule_id
normalization_profile
step_limit
state: PENDING | SOLVED | RESIDUAL | CONTRADICTION | UNSUPPORTED
```

There is no numeric maximum dimension. `bridge_context_id` and
`parent_goal_id` describe recursive higher boundaries.

### 10.4 Persistent residual obligation

Artifact v62 candidate, not v61:

```text
obligation_id
validation_rule_id
context_id
carrier_classifier
left_endpoint
right_endpoint
bridge_context_id
endpoint substitutions
normalization profile
step limit used during compilation
residual reason
source provenance
calculus fingerprint
```

Artifact validation replays `validation_rule_id` under the serialized calculus
fingerprint. A faster machine may discharge more goals before serialization,
but every residual artifact states exactly what was not discharged and under
which deterministic step limit.

## 11. Rule Identity and Proof Mapping

Stable rule IDs must be allocated only after V2-P1 begins. Their semantic names
are frozen here:

```text
OBS_DIAGONAL
OBS_CONVERT
OBS_ADT_CONSTRUCTOR
OBS_ADT_DISTINCT
OBS_MATCH_ACTION
OBS_COMP_RETURN
OBS_PI_POINTWISE
OBS_THUNK_PURE
OBS_REINDEX
```

Each successful object construction ends in ordinary `IS_TYPE` or `HAS_TYPE`
evidence with a tagged rule-specific payload. Conversion results, solver
equations, and canonical TermDB keys may be premises but are never the witness
payload themselves.

## 12. Law-Test Contract

Before T1/T2 completion, executable tests must establish:

1. identity and composition reindexing for the existing context category;
2. reindexing of shared Pi/Lambda/App without alternate tags;
3. reindexing of Comp/Return/Thunk/Force;
4. Match scrutinee/body reindexing and binder shadowing;
5. dependent context/telescope classifier reindexing;
6. one shared core node can have distinct typed occurrences and classifiers;
7. effect rows distinguish PURE, EFFECTFUL, and UNRESOLVED;
8. no equality, path, or duplicate value/computation graph tag is added;
9. artifact v61 remains byte-identical for a representative source;
10. all prototype tests and examples 01-07 and 09 continue to pass.

These tests characterize the substrate. They do not claim that V2-O1 object
equality has already been implemented.

## 13. Deferred Theory Obligations

The following are intentionally outside V2-T1/T2 and block corresponding
future admissions:

- universe observation and univalence;
- quotient and higher-constructor declaration schemas;
- observational semantics of algebraic handlers and resumptions;
- higher-order operation scope action;
- IADT index-refining observational action;
- host primitive logical models and refinement certificates;
- the concrete surface syntax for equality types and witness terms.

Deferral is explicit. None of these cases may silently use DefEq, raw graph
comparison, TypeView identity, or an empty cached effect bitset as a substitute.

## 14. References and Project Responsibility

1. Altenkirch, Chamoun, Kaposi, and Shulman, *Internal Parametricity Without an
   Interval*, POPL 2024, <https://arxiv.org/abs/2307.06448>.
2. Altenkirch, Chamoun, Kaposi, and Shulman, *Higher Observational Type Theory*,
   TYPES 2022, <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>.
3. Vakar, *A Framework for Dependent Types and Effects*, 2015,
   <https://arxiv.org/abs/1512.08009>.

The bridge/action organization is constrained by HOTT's type-former-directed
internal parametricity. The computation boundary is constrained by dependent
CBPV. The shared TermDB, typed occurrence graph, deterministic residual policy,
and no-subtyping boundary are A Program-specific normative decisions fixed by
this document.
