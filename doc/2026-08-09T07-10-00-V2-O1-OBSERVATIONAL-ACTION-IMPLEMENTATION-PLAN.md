# V2-O1 Observational Action Implementation Plan

Date: 2026-08-09

Status: complete locally; O1.0 through O1.8 implemented and verified

Repository baseline:

- branch: `main`;
- commit: `575d6c0`;
- artifact format: v69;
- HOTT compiler-local manifest: `hott_fragment_v2.schema`;
- prerequisite V3-G1: complete; its pullback-interning correction was completed
  in O1.1 before non-empty HOTT bridge construction.

This document is the executable plan for the next Higher Observational Type
Theory stage identified by
`2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`.
It supersedes the old statement that V3-G1 is still the next stage and now
records the completed V2-O1 implementation. V2-A1 is the next stage.

## 1. Objective

Implement the first type-directed observational action over the existing shared
TermDB and typed occurrence graph:

```text
Gamma                  |-> Gamma^R
sigma : Delta -> Gamma |-> sigma^R : Delta^R -> Gamma^R
Gamma |- A type        |-> A^R
Gamma |- a : A         |-> a^R
```

The implementation must:

1. preserve Context, Substitution, Term, Operation, Claim, and Derivation as
   distinct graph sorts;
2. use one shared TermDB rather than adding value-side or computation-side
   copies of Pi, Lambda, APP, Match, or ADT constructors;
3. construct typed relation families and witnesses, not return an untyped
   Boolean comparison;
4. keep kernel conversion separate from object equality;
5. keep effectful computations, operation requests, handlers, universes, host
   primitives, HITs, and IADTs residual or unsupported in the first fragment;
6. validate identity, composition, reindexing, and endpoint naturality; and
7. leave artifact publication to V2-A1 after the compiler-local representation
   has stabilized.

### 1.1 Baseline verification

At commit `575d6c0`, all 16 `src/prototype/test_*.sh` scripts pass. This
includes the HOTT goal, Context category, shared-term HOTT substrate, P0
certificate, artifact, CBPV, conversion, and binding-identity checks.

This establishes only the pre-O1 baseline. The existing HOTT goal test confirms
that Substitution, Type, and Term actions are valid deferred requests; it does
not establish non-empty bridge construction or any ready object action.

## 2. Audit Result

V2-O1 cannot be implemented by changing existing `DEFERRED` records to
`READY`. Four prerequisites are incomplete or incorrectly specified.

### 2.1 Existing action records are placeholders, not actions

`src/prototype/hott.c:1729-1904` validates one monolithic
`prototype_hott_action` record. Only the empty Context action can be `READY`.
Substitution, Type, and Term actions are explicitly required to remain
`DEFERRED` by `hott_action_is_valid()`.

The record combines:

- immutable request identity;
- mutable work state;
- generated result;
- result certificate references.

This conflicts with the ownership discipline already established for HOTT
observation goals, candidates, work items, and residuals. Solving an action
would otherwise require mutation or a second logically duplicate action.

### 2.2 Non-empty bridge provenance is not representable

`src/prototype/hott.c:281-379` admits only the terminal bridge. The current
bridge record stores source Context, bridge Context, and two endpoint
Substitutions, but does not store:

- the parent bridge;
- the source extension classifier Claim;
- the Type-action result used as the relation field;
- the three generated Context-extension certificates; or
- the construction authority for the two endpoint projections.

Endpoint source/target types alone are insufficient. A forged bridge could use
an unrelated relation field while retaining correctly oriented projections.

### 2.3 The frozen Type-action contract omits endpoint context

The P1 text says that an `IS_TYPE` Claim is translated directly to a generated
relation-family `IS_TYPE` Claim. That is underspecified.

For:

```text
Gamma |- A type
pi0, pi1 : Gamma^R -> Gamma
```

the relation is a type only after introducing both endpoint values:

```text
A0 = A[pi0]
A1 = A[pi1]
Gamma^R . x0 : A0 . x1 : A1 |- A^R(x0, x1) type
```

Therefore a Type action must produce an endpoint Context as well as a relation
type and exact formation authority. A bare pair of `result_term_id` and
`result_claim_id` cannot validate its scope.

This correction follows the telescope equality/type equality organization in
*Towards Higher Observational Type Theory*: equality of a dependent type is
heterogeneous over two endpoint substitutions and two endpoint values. It is
also required by the A Program ContextDB representation, where scope is an
explicit Context identity.

### 2.4 V3-G1 pullback reuse is still a direct-mapped cache

`prototype_context_fresh_reindex_extension()` at
`src/prototype/context.c:1104-1255` uses one direct-mapped cache slot keyed by
`(base_context, source_extension)`. On a cache collision it allocates fresh
binding IDs and an equivalent new Context path.

This is useful memoization but is not the exact interned comprehension action
claimed by V3-G1.6. It also accepts only a base Context, not a general
Substitution `sigma : Delta -> Gamma`, so it is not the general CwF pullback
needed by observational substitution.

Before O1 constructs relational Contexts, this must become an interned action
keyed by the complete typed input, at minimum:

```text
(source_extension_context_id, base_substitution_id)
```

Cache eviction may affect performance, but must never change the returned
Context, binding, or lifted Substitution identity.

## 3. Corrected First-Fragment Calculus

### 3.1 Context action

The terminal case is retained:

```text
empty^R = empty
pi0 = id_empty
pi1 = id_empty
```

For one source extension `Gamma.A`, first obtain the parent bridge:

```text
Gamma^R
pi0_Gamma, pi1_Gamma : Gamma^R -> Gamma
```

Then construct:

```text
C0  = Gamma^R . x0 : A[pi0_Gamma]
C01 = C0      . x1 : A[pi1_Gamma][p_C0]
C01 |- AR type
(Gamma.A)^R = C01 . r : AR
```

Finally construct endpoint substitutions:

```text
pi0_(Gamma.A) : (Gamma.A)^R -> Gamma.A
pi1_(Gamma.A) : (Gamma.A)^R -> Gamma.A
```

by extending the weakened parent projections with `x0` and `x1` respectively.

The bridge result must identify the parent bridge, endpoint Context, Type
action, relation-field Context extension, and exact endpoint Substitutions.

### 3.2 Type action

Input identity:

```text
(source_is_type_claim_id, source_bridge_id)
```

Output certificate:

```text
endpoint_context_id
left_endpoint_binding_id
right_endpoint_binding_id
relation_type_term_id
relation_is_type_claim_id
left_endpoint_context_certificate_id
right_endpoint_context_certificate_id
```

The result Claim must state:

```text
endpoint_context |- relation_type_term type
```

It must not claim that an unapplied binary relation function is itself a type.

### 3.3 Term action

For an exact accepted source Claim `Gamma |- a : A`, its source bridge, and the
exact Type action of `A`:

1. compute `a0 = a[pi0]` and `a1 = a[pi1]`;
2. construct the endpoint-instantiation Substitution
   `Gamma^R -> endpoint_context`;
3. reindex `AR` along that Substitution; and
4. construct `aR` with an exact accepted Claim:

```text
Gamma^R |- aR : AR[a0, a1]
```

The action result owns the endpoint-instantiation Substitution and its
certificates. It does not insert `a0 == a1` into DefEq.

### 3.4 Substitution action

For `sigma : Delta -> Gamma`, with source and target bridges:

```text
sigma^R : Delta^R -> Gamma^R
```

Construction is structural over the existing Substitution DAG. Validation must
establish:

```text
(id_Gamma)^R = id_(Gamma^R)
(sigma o tau)^R = sigma^R o tau^R
pi0_Gamma o sigma^R ~= sigma o pi0_Delta
pi1_Gamma o sigma^R ~= sigma o pi1_Delta
```

The first two should be construction laws where canonical constructors make
that possible. The endpoint equations are typed naturality obligations. They
must not be asserted from raw Substitution IDs when structurally different DAGs
have the same pointwise action.

The first implementation validates endpoint naturality pointwise on target
bindings using ordinary reindexing and kernel conversion under one fixed pure
profile. It records a compiler-local naturality certificate bound to the exact
Substitution action request and graph revision.

### 3.5 Minimal object syntax decision

O1 cannot produce a `result_term_id` without an object-language representation
for the generated relation and its preservation witness. The recommended O1.0
decision is the minimal Higher Observational interface:

```text
heterogeneous observation type, applied to its endpoint data
ap, witnessing that a typed term preserves the observation structure
```

Homogeneous equality and reflexivity are derived by applying these operations
to the terminal/diagonal bridge. A separate primitive `REFL` tag is not needed
in the first fragment. This follows the TYPES 2022 presentation, where
homogeneous `Id` and `refl` are special cases of telescope/type equality and
`ap`.

The precise TermDB operands must be frozen in O1.0, with these constraints:

- no Context, Substitution, Claim, Derivation, action, or compiler-work ID is
  stored in object Term identity;
- scoped endpoint and relation variables are ordinary direct binding IDs;
- the type-directed computation rule dispatches through the authoritative
  type-former descriptor;
- applying the observation type to an observation type remains meaningful, so
  witness equality is not collapsed to Unit or proof irrelevance; and
- no duplicate Pi, Lambda, APP, Match, value, or computation tag is added.

Expanding every equality eagerly into generated Match plus hidden internal
Unit/Empty declarations is not the recommended foundation. It makes generated
declaration identity part of equality, complicates recursive families, and
does not by itself provide the higher `ap` structure. Match expansion may be a
normal form or optimization of the object observation type after the primitive
contract is fixed.

## 4. Ownership and Data Layout

### 4.1 Keep existing graph sorts

No universal node arena is introduced:

| Sort | Authority |
| --- | --- |
| Context | `ContextDB` |
| ordinary substitution | `SubstitutionDB` |
| shared computational/type syntax | `TermDB` |
| typed surface/generated occurrence | `OperationGraph` where applicable |
| proposition | Judgement Claim identity |
| proof | Judgement Derivation DAG |
| observational request/search | HOTT compiler-local databases |

HOTT action IDs are not TermDB IDs and must not be embedded in ordinary TermDB
identity.

### 4.2 Separate request from result

Replace the current monolithic action record with:

```text
HottActionRequest
  id
  kind
  kind-specific immutable key

HottActionResult
  request_id
  state: READY | RESIDUAL | UNSUPPORTED
  kind-specific output certificate id
  calculus fingerprint
  deterministic budget/profile data where computation was required
```

Use a tagged union or kind-specific payload arenas. Do not retain one struct
containing every possible Context/Substitution/Type/Term field.

Requests are graph-consed by complete typed key. Results never alter request
identity. Repeating one request returns the same request ID and, after solving,
the same validated result.

### 4.3 Separate bridge and action certificates

`prototype_hott_bridge` is the result of Context action, not the action itself.
Extend its replay authority through a separate certificate record rather than
placing all proof fields into the structural bridge key.

Structural bridge identity contains the generated Context and endpoint
Substitutions. The certificate refers to:

- source Context action request;
- parent bridge;
- source Context-formation certificate;
- Type-action result;
- generated Context-formation certificates; and
- exact endpoint substitution construction.

This mirrors the existing distinction between structural Substitution and
`prototype_substitution_certificate`.

### 4.4 Move HOTT ownership out of `judgement.h`

The HOTT bridge, goal, candidate, work, residual, and action declarations
currently occupy `src/prototype/judgement.h:1031-1487`. They are not JudgementDB
records.

Create `src/prototype/hott.h` and move these declarations there. `hott.h` may
depend on the public Context, Term, TypeDeclaration, and Judgement APIs;
`judgement.h` must not depend back on HOTT.

This is an ownership refactor, not a new semantic layer or a second proof DB.

## 5. Type-Former Semantic Authority

O1 needs one declarative dispatch boundary. The existing
`prototype_term_semantics()` describes graph layer and reduction role; it is
not a type-former observational semantics interface.

Add a compiler/kernel descriptor selected from an exact accepted `IS_TYPE`
Claim and authoritative type data. The descriptor must provide or reject:

```text
formation replay
ordinary substitution action
observational Type action
observational Term action
transport rule identifier
definitional computation rule identifier
artifact semantic identifier for future V2-A1
```

The first implementation uses one declarative switch/table in the HOTT module,
not function pointers distributed across `typing.c`, `term.c`, `ast.c`, and
`type_declaration.c`.

Admission order:

1. terminal and ordinary non-recursive ADTs;
2. recursive ordinary ADTs using existing Match/IH structure;
3. Pi with admitted domain and pure computation codomain;
4. `Comp(empty,A)` exposed through `RETURN`;
5. pure `Thunk` through its admitted computation observation; and
6. ordinary Match after motive/case/IH naturality is implemented.

The following remain explicit residuals:

- Universe and Universe inhabitants;
- effectful or unresolved `Comp`;
- operation request and computation fold;
- host primitive types and values;
- TypeView equality between different nominal declarations;
- HIT/quotient constructors; and
- IADT index refinement.

## 6. Implementation Phases

### O1.0: Correct the normative contract

Status: complete

- [x] Update the normative calculus with endpoint Contexts for Type action.
- [x] Freeze the minimal object observation-type and `ap` term forms, operand
      scopes, typing rules, and reduction ownership.
- [x] Specify homogeneous equality and reflexivity as derived forms; do not add
      a separate first-fragment `REFL` primitive without a demonstrated need.
- [x] Prohibit compiler graph IDs from all object Term keys.
- [x] Replace `hott_fragment_v1.schema` with `hott_fragment_v2.schema`; retain no
      compatibility path in the compiler-local manifest check.
- [x] Keep the four action-kind numeric IDs stable unless the complete manifest
      intentionally changes them.
- [x] Add explicit result, certificate, and naturality vocabularies.
- [x] Regenerate `PROTOTYPE_HOTT_CALCULUS_FINGERPRINT` from exact manifest
      bytes.
- [x] Add a schema test proving that a semantic change without a fingerprint
      update fails.

### O1.1: Finish the G1 pullback prerequisite

Status: complete

- [x] Replace `prototype_context_fresh_reindex_extension()` with a general
      comprehension action keyed by source extension and base Substitution.
- [x] Store action entries in a collision-safe intern table, not a direct-mapped
      memo slot.
- [x] Make generated binding identity part of the interned result, never a
      cache-dependent side effect.
- [x] Preserve the existing reindex cache only as a non-authoritative
      optimization.
- [x] Migrate the two `ast.c` callers and category-law tests.
- [x] Add forced-cache-collision tests proving stable Context, binding, and
      lifted-Substitution IDs.

Exit gate: repeated exact pullback construction returns the same graph result
after arbitrary unrelated cache traffic.

### O1.2: Repair HOTT module ownership and action IR

Status: complete

- [x] Create `src/prototype/hott.h`.
- [x] Move HOTT-only declarations out of `judgement.h`.
- [x] Introduce immutable action request records with complete per-kind keys.
- [x] Introduce separate result and certificate records.
- [x] Graph-cons requests collision-safely.
- [x] Remove `state` and all result fields from request identity.
- [x] Preserve observation goal/candidate/work/residual separation.
- [x] Add malformed cross-kind reference and duplicate-request tests.

Exit gate: no action is solved by mutating its identity, and no HOTT compiler
record is confused with a TermDB or Judgement Claim ID.

### O1.3: Implement endpoint contexts and non-empty Context action

Status: complete for the first zero-field ordinary ADT fragment

- [x] Implement terminal Context action through the new request/result API.
- [x] Implement Type-action endpoint Context construction independent of the
      relation body.
- [x] Construct and certify `A[pi0]` and weakened `A[pi1]` extensions.
- [x] Implement the first relation type for a zero-field ordinary ADT.
- [x] Extend the endpoint Context by the relation field.
- [x] Construct both endpoint projection Substitutions.
- [x] Validate parent bridge, Type action, all Context certificates, and both
      projection orientations.
- [x] Reject a bridge whose Type-action certificate names an unrelated relation.

Exit gate: a one-binding Bool-like Context has one deterministic bridge with
validated left/right projections; unsupported binding types residualize rather
than fabricating a bridge.

### O1.4: Implement structural Substitution action and naturality

Status: complete for identity, composition, dependent reindexing, and bounded
naturality

- [x] Implement action for identity, empty, projection, extension, and
      composition Substitution nodes.
- [x] Reuse Context-action results for source and target bridges.
- [x] Create exact extension certificates for generated relational
      substitutions.
- [x] Validate left and right endpoint naturality pointwise.
- [x] Validate identity and composition laws in the one-binding Bool fixture.
- [x] Add a deterministic conversion profile and step budget to naturality
      checks.
- [x] Return typed residual on exhausted conversion; never accept exhaustion as
      equality.
- [x] Generalize extension action from the terminal-parent fixture to a
      dependent classifier reindexed along a non-trivial prefix action.

Exit gate: identity, one-level projection, extension, and two-step composition
all satisfy naturality; reversed and forged bridge orientations fail.

### O1.5: Implement Type action by admitted type former

Status: complete for the frozen first fragment

- [x] Add the declarative type-former semantic descriptor.
- [x] Generate ordinary ADT relation families as the frozen heterogeneous
      `OBSERVATION_TYPE` object form. Constructor introduction consumes exact
      accepted constructor-spine Derivations in authoritative telescope order;
      neither readback field metadata nor representation shape keys are
      semantic authority.
- [x] Generate same-constructor field observations in telescope order.
- [x] Keep distinct-constructor families uninhabited without marking the
      compiler goal contradictory or starting a Term action.
- [x] Support recursive ADTs through existing Match frame/IH identity. Match
      witness proofs retain the source Match Claim and close guarded IH action
      without treating frame IDs as ordinary binders.
- [x] Implement pointwise Pi relation for admitted domain and pure codomain.
- [x] Implement pure `Comp` and `Thunk` boundaries from the frozen T1/T2 rules.
- [x] Residualize Universe, effects, host primitives, HIT, and IADT cases.

Exit gate: every admitted descriptor has formation, substitution, action, and
replay tests; every non-admitted descriptor has a stable residual reason.

### O1.6: Implement Term action and object witness generation

Status: complete for the frozen first fragment

- [x] Reindex endpoints through the exact bridge projections.
- [x] Build the endpoint-instantiation Substitution into the Type-action
      endpoint Context.
- [x] Reindex the relation type into the bridge Context.
- [x] Generate diagonal witnesses without reducing higher witnesses to Unit.
- [x] Generate constructor witnesses recursively from field Term actions.
- [x] Generate Lambda/APP witnesses through the admitted pointwise Pi action.
- [x] Generate pure Return/Thunk witnesses only through the stated CBPV rules.
- [x] Generate Match witnesses for zero-binder, pattern-binder, and guarded
      recursive-IH branches. Uniform Match reduction is owned by the ordinary
      normalizer, not by a typing-only shortcut.
- [x] Register generated types and witnesses as ordinary exact Claims and
      Derivations with explicit HOTT rule authority.
- [x] Keep DefEq conversion premises on the consuming derivation only.

Exit gate: a generated witness is accepted only by ordinary `HAS_TYPE` replay
against the generated relation type. Deleting or forging one premise makes
validation fail.

### O1.7: Connect observation planning to action execution

Status: complete for the compiler-local boundary

- [x] Replace test-only manual action construction with a deterministic action
      executor used by `prototype_hott_observation_plan()`.
- [x] Convert exact diagonal ready candidates into Type/Term action requests.
- [x] Preserve multiple valid candidates for one immutable observation goal.
- [x] Keep budgets, source locations, consumed steps, graph revision, profile,
      fingerprint, and residual diagnostics in work/result state.
- [x] Publish a parent result certificate only after all premises validate.
      Successful child Claims may remain because they are independently valid
      DAG evidence. If result publication fails after adding the parent
      certificate, the just-added terminal certificate is removed atomically;
      failure never promotes an invalid or incomplete parent result.
- [x] Add a compiler API entry point without adding surface equality syntax.

Exit gate: the existing HOTT goal fixture produces validated compiler-local
families/witnesses for admitted cases and stable residuals for excluded cases.

### O1.8: Exit audit and V2-A1 handoff

Status: complete locally

- [x] Run `make` and `make reader`.
- [x] Build all touched C files with the repository warning policy and
      `-Werror`.
- [x] Run all 16 `src/prototype/test_*.sh` scripts.
- [x] Run examples 01-07 and 09.
- [x] Run the HOTT fixture under optimized `-Werror` and ASan/UBSan builds.
- [x] Exercise result-capacity failure and verify parent-certificate rollback.
- [x] Run `git diff --check`.
- [x] Confirm artifact v69 remains unchanged and deterministic.
- [x] Freeze the minimal object witness/certificate set that V2-A1 must
      serialize.
- [x] Update the V3 status table and remove stale “O1 next” handoff text.

## 7. Required Characterization Tests

### 7.1 Context and pullback

- empty bridge is unique;
- one-binding bridge has exactly three relational extensions;
- two-binding dependent bridge uses the parent bridge and reindexed classifier;
- cache collision does not allocate a second equivalent pullback;
- unrelated relation field is rejected;
- missing exact Context-formation Claim is rejected; and
- left/right projection reversal is rejected.

### 7.2 Substitution laws

- identity action;
- empty substitution action;
- one-level and multi-level projection action;
- extension action with dependent classifier;
- composition action;
- both endpoint naturality squares;
- action after ordinary reindex agrees with ordinary reindex after action; and
- exhausted naturality comparison becomes residual.

### 7.3 Type and term action

- Bool-like diagonal and distinct constructors;
- zero-field and multi-field constructors;
- recursive Nat/List constructor action;
- dependent constructor telescope fixture reserved for Sigma;
- Pi pointwise action;
- APP action with dependent codomain;
- pure Return and Thunk;
- Match motive/case naturality;
- shared Core term under distinct typed Operations remains distinct at the
  Claim/action boundary; and
- witness-of-witness input is not collapsed by graph identity.

### 7.4 Exclusion tests

- Universe action returns `UNIVERSE` residual;
- effectful and unresolved rows remain distinct residuals;
- operation requests and computation folds are unsupported;
- host primitives do not use C implementation equality;
- different TypeViews do not become observationally equal by shape; and
- no successful object proof is added to global DefEq.

## 8. Files Expected to Change

| File | Planned role |
| --- | --- |
| `src/prototype/hott.h` | new declarative HOTT compiler/kernel API |
| `src/prototype/hott.c` | request/result graph, bridge/action construction, validators |
| `src/prototype/term.h/.c` | minimal object observation/ap terms and type-directed reduction selected in O1.0 |
| `src/prototype/judgement.h` | remove HOTT-owned declarations; retain Claim/Derivation API |
| `src/prototype/typing.c` | exact generated Claim/Derivation constructors and replay hooks only |
| `src/prototype/context.h` | general interned comprehension action API |
| `src/prototype/context.c` | collision-safe pullback action and ordinary reindex support |
| `src/prototype/type_declaration.h/.c` | authoritative ADT semantic queries where missing |
| `src/prototype/hott_fragment_v2.schema` | corrected action/result/naturality contract |
| `src/prototype/hott_goal_check.c` | action and adversarial fixtures |
| `src/prototype/context_category_check.c` | pullback collision/law fixtures |
| `src/prototype/test_hott_goal.sh` | compile new HOTT module boundary |
| `src/prototype/test_context_category.sh` | execute new pullback laws |

`src/`, `include/`, parser/lexer inputs, and accepted build files are outside
this prototype implementation scope.

## 9. Explicit Non-Goals

V2-O1 does not:

- add surface `==`, `refl`, transport, or rewrite syntax; the compiler-local
  object terms selected in O1.0 precede surface notation;
- serialize HOTT action/work records in artifact v69;
- implement equality reflection;
- add successful PropEq proofs to DefEq;
- implement univalence or Universe observational equality;
- define effect-handler contextual equivalence;
- implement machine-word quotient/HIT semantics;
- add separate value/computation graph constructors; or
- replace ContextDB/SubstitutionDB with TermDB nodes.

Transport, symmetry, object-facing equality syntax, universe integration, and
artifact publication follow only after the first action laws pass.

## 10. Independent Kernel Debt

`validate_universe_cumulativity_proof()` at
`src/prototype/typing.c:14515-14527` currently validates only that both terms
are Universe variables. It does not check a UniverseDB inequality witness.

This does not block the explicitly Universe-excluding O1 fragment. It does
block Universe Type action, universe-level HOTT, and any future claim that O1
works uniformly over all `IS_TYPE` Claims. The O1 descriptor must therefore
reject Universe action explicitly until this independent kernel task is fixed.

## 11. Progress Summary

| Phase | Status | Exit evidence |
| --- | --- | --- |
| O1.0 Correct contract/manifest | complete | endpoint Context contract and fragment v2 fingerprint |
| O1.1 Finish pullback interning | complete | collision-stable comprehension action |
| O1.2 Repair action IR ownership | complete | immutable request/result separation |
| O1.3 Non-empty Context action | complete | validated one-binding bridge |
| O1.4 Substitution action | complete | dependent observation classifier and bounded pointwise naturality |
| O1.5 Type action | complete | descriptor, ordinary ADT/observation, Pi, pure Comp/Thunk, stable exclusions |
| O1.6 Term action | complete | diagonal, bridge-variable, constructor, Lambda/APP, Match/IH, Return/Thunk replay |
| O1.7 Planner integration | complete | repeatable diagonal and family-only distinct-constructor execution paths |
| O1.8 Exit audit | complete | strict builds, all 16 scripts, examples 01-07/09, deterministic v69 |

The O1 checkpoint is complete. Lambda action uses the three-field binder bridge;
Match action covers neutral constant branches, pattern binders, and a recursive
Nat fixture with guarded IH evidence. APP consumes exact function and argument
witness Claims; a bridge relation variable is accepted as a witness without
requiring `OBSERVATION_WITNESS` syntax. The observation-of-observation fixture
proves that relation fields remain typed variables rather than being collapsed
to Unit or proof irrelevance.

V2-A1 must serialize only the frozen object terms and accepted Judgement proof
kinds required for replay. Compiler-local requests, work items, residuals,
bridges, and action results remain rebuildable planning state and are not v69
artifact roots.

## 12. Theory Sources and Project-Specific Decisions

The bridge, telescope equality, and action equations are constrained by:

1. Altenkirch, Chamoun, Kaposi, and Shulman, *Internal Parametricity, without
   an Interval*, POPL 2024, <https://arxiv.org/abs/2307.06448>.
2. Altenkirch, Kaposi, and Shulman, *Towards Higher Observational Type Theory*,
   TYPES 2022,
   <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>.
3. Vakar, *A Framework for Dependent Types and Effects*,
   <https://arxiv.org/abs/1512.08009>.

The following are A Program-specific decisions and require local law tests:

- one shared TermDB for value and computation syntax;
- explicit Context/Substitution graph sorts;
- exact accepted Claims as typing authority;
- compiler-local action search before artifact publication;
- pointwise naturality validation using the existing pure conversion kernel;
- deterministic residual budgets; and
- exclusion of effects and universes from the first implemented action.
