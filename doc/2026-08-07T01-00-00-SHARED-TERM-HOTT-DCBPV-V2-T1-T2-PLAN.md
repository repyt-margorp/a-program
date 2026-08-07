# Shared-Term HOTT and Dependent-CBPV V2-T1/T2 Plan

Date: 2026-08-07

Status: implemented and validated; normative first-fragment calculus frozen

Parent plan:

- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Completed prerequisite:

- `doc/2026-08-07T00-00-00-CONTEXT-SUBSTITUTION-V2-C1-IMPLEMENTATION-PLAN.md`
- commit `77083ea` (`refactor: extract context and substitution semantics`)

Baseline:

- branch: `main`;
- commit: `0f94680de93be47a810753db0573d76de6509f82`;
- artifact format: `A_PROGRAM_ARTIFACT 62`;
- no object-equality TermDB tag is present;
- no artifact change is authorized by this plan.

## 1. Decision Corrected by This Plan

V2-T1 must not introduce a separate value-side Pi, Lambda, APP, or Match
syntax. The earlier phrases "value-side HOTT", "pure Pi on the value side",
and "value-side observational action" were ambiguous and could be read as a
request for a second graph language. That is not the target architecture.

The authoritative representation remains one shared TermDB containing one
tag and node schema for each existing core constructor:

```text
VAR
LAMBDA
APP
PI
MATCH
CONSTRUCTOR
...
```

Value/computation distinctions are typed interpretations of occurrences in a
context. They are not a reason to duplicate those graph constructors.

The existing CBPV boundary constructors remain explicit:

```text
RETURN
THUNK
FORCE
COMPUTATION_TYPE
OPERATION_REQUEST
COMPUTATION_FOLD
```

These constructors record a genuine change across the value/computation
adjunction or an effect-algebra operation. They do not form a second copy of
Lambda calculus.

Consequently, the first HOTT implementation must be a type-directed service
over shared TermDB terms:

```text
observation request =
    context
  + carrier classifier
  + left shared term
  + right shared term
  + observational structure
```

Raw TermDB identity is neither sufficient nor replaced by a second value or
computation graph.

## 2. Current Code Audit

### 2.1 Shared graph constructors are already present

`src/prototype/term.h` defines exactly one `PROTOTYPE_TERM_APP`, one
`PROTOTYPE_TERM_LAMBDA`, and one `PROTOTYPE_TERM_PI`. There are no
`VALUE_PI`/`COMPUTATION_PI`, `VALUE_APP`/`COMPUTATION_APP`, or corresponding
duplicate graph tags.

`prototype_term_semantics` in `src/prototype/term.c` classifies these shared
nodes by semantic role. Constructor spines and function elimination can share
APP while retaining different typed rules.

### 2.2 Polarity belongs to typed occurrences

`struct prototype_operation_node` in `src/prototype/ast.h` stores:

- one `core_term`;
- one source occurrence;
- its context;
- its solved classifier;
- its value/computation polarity;
- its application role.

Different typed occurrences may point to one alpha-interned core node. This is
the intended annotation layer and is not graph duplication.

### 2.3 Current Pi interpretation

`prototype_term_classifier_view` currently classifies `PI(A, C)` as a
computation-function classifier. A source function used as a value crosses the
existing boundary through `THUNK_TYPE(PI(A, C))` and `THUNK`/`FORCE`.

V2-T1/T2 may review the laws of this interpretation, but must not add a second
Pi tag. Any proposed change must be expressible as a typing or reindexing rule
over the existing shared Pi node.

### 2.4 Context and substitution are already separate services

V2-C1 moved ContextDB, SubstitutionDB, and ordinary term reindexing into
`src/prototype/context.c`. The existing substitution orientation is:

```text
sigma : Delta -> Gamma
term[sigma] is a term in Delta
```

This ordinary reindexing is the base on which HOTT observational action and
dependent-CBPV naturality must be stated. Observational action must not be
implemented by changing the meaning of `prototype_term_reindex`.

### 2.5 The solver is not ready for HOTT records

`struct operation_classifier_constraint` in `src/prototype/ast.c` currently
stores positional fields named `target`, `left`, `right`, and `aux`. It does
not identify:

- the context of a goal;
- a carrier classifier;
- observational endpoints;
- a dimension or higher-cell boundary;
- the selected type-former rule;
- a conversion profile and deterministic budget;
- the residual validation rule.

Adding such fields before T1/T2 determines their exact meaning would freeze an
accidental schema. V2-S1 was therefore blocked until this plan froze the four
record contracts in the normative calculus.

### 2.6 JudgementDB is intentionally still ordinary typing evidence

JudgementDB currently records `HAS_TYPE` and `IS_TYPE`. It has no generic
`JUDGEMENT_EQ`, and this plan does not add one. Object equality will eventually
be represented by an equality type and ordinary witness terms checked with
`HAS_TYPE`. Kernel conversion remains a meta-level premise used while checking
such terms.

## 3. Objectives

V2-T1/T2 must produce a finite, implementable calculus specification for the
first HOTT slice while preserving the shared TermDB architecture.

The work has five concrete outputs:

1. exact judgement forms for values, computations, and higher observational
   witnesses over shared terms;
2. a type-former support matrix for the first slice;
3. substitution and naturality laws for existing TermDB constructors;
4. an explicit boundary for effects, residual computations, universes, and
   unsupported type formers;
5. the typed input schema required by V2-S1, without implementing it yet.

"Finite" means a finite set of admitted syntax and type-former rules. It does
not mean that equality witnesses are truncated to a fixed homotopy level.
Witness terms must remain eligible for the same higher observational treatment
as other admitted values.

## 4. Non-Goals

This checkpoint must not:

- add a value-side Pi or any duplicate APP/LAMBDA/MATCH graph tag;
- add an equality, path, transport, interval, or coherence TermDB tag;
- add `JUDGEMENT_EQ`;
- promote solver `EQUAL` constraints into object equality proofs;
- make successful PropEq proofs extend global definitional equality;
- modify artifact v61 or reserve unvalidated v62 records;
- introduce subtyping to solve dependent-CBPV typing;
- define equality of effectful computations by erased graph comparison;
- claim universe equality, univalence, quotients, or HITs before their rules
  are explicitly selected.

## 5. Shared-Term Semantic Model to Freeze

### 5.1 One syntax, indexed judgements

The specification must use one term metavariable space. Value and computation
judgements classify shared terms rather than selecting different term
grammars:

```text
Gamma |- t : A          typed value occurrence
Gamma |- t : C          typed computation occurrence
Gamma |- A value-type
Gamma |- C computation-type
```

The final notation must make the two classifier classes unambiguous without
requiring separate graph constructors. A single core term may have distinct
typed occurrences only when each occurrence has its own context, classifier,
and derivation.

### 5.2 Existing CBPV boundaries

The specification must state formation, introduction/elimination, and
reindexing rules for:

```text
Comp(E, A)
Return(v)
ThunkType(C)
Thunk(m)
Force(v)
Pi(A, C)
Lambda(x, m)
App(f, v)
```

No rule may infer polarity from the untyped core tag alone. In particular,
`LAMBDA` and `APP` retain their meaning from their typed occurrence and
classifier.

### 5.3 Observational equality is type-directed

The normative notation may change, but it must distinguish at least:

```text
Gamma |- a approximately[A] b type
Gamma |- p : a approximately[A] b
```

For dependent types, the calculus must also state how a type over one endpoint
is compared or transported to the type over the other endpoint. This cannot
be represented as an untyped pair of TermDB IDs.

The calculus must define recursive higher closure: if `p` and `q` are witness
terms of the same equality type, their equality is not automatically Unit,
proof-irrelevant, or raw graph identity.

### 5.4 Definitional conversion remains separate

The following remain different relations:

```text
same TermDB node
kernel definitional conversion
object-level higher observational equality
```

Definitional conversion may discharge a reflexive introduction side
condition. It does not become the representation of every object equality and
does not absorb user proofs into a global union-find.

## 6. First-Fragment Support Matrix

T1 must produce a reviewed matrix with one of these states for every type
former:

```text
ADMITTED
RESIDUAL
REJECTED
DEFERRED
```

The starting proposal is:

| Existing structure | Proposed first state | Required rule |
| --- | --- | --- |
| Context variables and substitutions | ADMITTED | bridge/action and substitution laws |
| Ordinary ADT point constructors | ADMITTED | constructor telescope action |
| Ordinary Match on admitted ADTs | ADMITTED | motive/case naturality and iota stability |
| Shared `PI/LAMBDA/APP` with statically empty effects | ADMITTED | function observation through existing CBPV boundary |
| `RETURN` with empty effect row | ADMITTED | return naturality |
| `THUNK/FORCE` of admitted pure computations | candidate ADMITTED | explicit F/U-style action laws |
| Effectful `Comp(E,A)` | RESIDUAL | no erased computation equality |
| `OPERATION_REQUEST` and `COMPUTATION_FOLD` | DEFERRED | handler-sensitive observational semantics required |
| Primitive host types and operations | DEFERRED | logical model/refinement certificate required |
| Universe equality and univalence | DEFERRED | universe semantics not yet soundly fixed |
| Quotients, path constructors, and HITs | DEFERRED | declaration and computation rules absent |
| IADT index refinement | DEFERRED unless derivable from admitted rules | indexed constructor action required |

The table is a proposal, not a completion claim. In particular, admitting
Thunk requires proving that its observation does not silently compare an
effectful suspended graph.

## 7. Required Reindexing and Naturality Laws

For every accepted rule, T2 must state both its typing rule and its behavior
under `sigma : Delta -> Gamma`.

### 7.1 Existing context category

Retain and cite the existing laws tested by V2-C1:

```text
t[id] = t
t[sigma o tau] = t[sigma][tau]
p o <sigma,t> = sigma
q[<sigma,t>] = t
```

These are ordinary substitutions, not higher observations.

### 7.2 Shared Pi/Lambda/App

State laws equivalent to:

```text
Pi(A,C)[sigma]
  = Pi(A[sigma], C[lift(sigma)])

Lambda(x,m)[sigma]
  = Lambda(x', m[lift(sigma)])

App(f,v)[sigma]
  = App(f[sigma], v[sigma])
```

The implementation may alpha-intern the rebuilt graph. The law concerns typed
terms in their reindexed contexts, not preservation of numeric node IDs.

### 7.3 CBPV boundary

State laws for:

```text
Comp(E,A)[sigma]
Return(v)[sigma]
Thunk(C)[sigma]
Thunk(m)[sigma]
Force(v)[sigma]
```

Effect-row reindexing must distinguish a known empty row from an unresolved
row. Unknown must never be accepted as pure merely because its current closed
bitset is empty.

### 7.4 ADT and Match

State laws for:

- type-former parameter telescopes;
- dependent constructor field telescopes;
- constructor result classifiers;
- Match scrutinee reindexing;
- motive reindexing;
- case-context lifting;
- guarded IH references;
- iota reduction after reindexing.

The classifier of Match remains motive application. Uniform branch results are
observed only after normalizing that application; they are not a separate
typing or equality rule.

### 7.5 Observational action

The ordinary substitution action and the higher observational action must have
different APIs and laws. T1/T2 must specify how observation commutes with
ordinary reindexing without treating the two operations as identical.

## 8. Dependent-CBPV Boundary

The first implementation must not manufacture a static value from an
effectful computation result.

For pure computations that normalize within the selected deterministic
profile, a result may discharge an ordinary typed constraint. For an
effectful, exhausted, or unsupported computation, the dependent result remains
a typed residual obligation containing its context and family.

This plan does not adopt dependent Kleisli extension by subtyping. The
normative specification must state:

- which pure computation forms may be normalized during checking;
- which result-family applications can be constructed statically;
- which effectful dependencies become residual;
- which residual validation rule is stable across artifact readback;
- why no rule attempts to form `M(m)` when `m : Comp(E,A)` rather than `m:A`.

## 9. V2-S1 Input Contract

T1/T2 completes only after it defines the semantic fields that S1 must carry.
The minimum candidate record is:

```text
goal kind
source occurrence and source span
context id
carrier classifier
left and right endpoint terms
observational boundary/dimension description
type-former semantic rule id
ordinary substitution/reindexing data
normalization profile and deterministic step budget
status: pending, solved, residual, contradiction, unsupported
stable residual validation rule id
```

This is not permission to add all fields to the existing positional
`operation_classifier_constraint`. T1/T2 must first decide which fields belong
to:

- ordinary classifier unification;
- kernel conversion requests;
- HOTT object-term construction;
- artifact-persistent residual obligations.

One catch-all constraint record would repeat the current ownership problem.

## 10. Implementation Phases

### T12-0: Baseline and terminology correction

- [x] Record the current commit, artifact version, and clean worktree.
- [x] Replace ambiguous "value-side Pi/HOTT" wording in active plans.
- [x] Add a permanent rule forbidding duplicated value/computation graph tags.
- [x] Record that explicit F/U boundary nodes are not graph duplication.

Exit: active plans describe one shared TermDB and typed occurrence categories.

### T12-1: Current-rule inventory

- [x] Map every admitted TermDB tag to its current formation, introduction,
  elimination, normalization, and reindexing code.
- [x] Map every rule to its JudgementDB proof kind.
- [x] Identify rules currently inferred only from `classifier_view` or
  positional solver fields.
- [x] Produce the accepted/residual/rejected/deferred matrix.

Exit: no first-fragment type former lacks an identified current owner.

### T12-2: Normative shared-term HOTT judgements

- [x] Fix context, type, term, equality-type, and witness judgement notation.
- [x] Fix heterogeneous dependent-type observation over endpoint
  substitutions.
- [x] Specify reflexive introduction using DefEq only as a checked premise.
- [x] Specify recursive higher closure without fixed-level truncation.
- [x] Specify ADT point-constructor and Match rules for the first fragment.
- [x] Explicitly defer universe equality, univalence, quotients, and HITs.

Exit: every accepted judgement has premises and a conclusion, not prose alone.

### T12-3: Normative dependent-CBPV laws

- [x] Specify shared Pi/Lambda/App typing and reindexing.
- [x] Specify Comp/Return/Thunk/Force typing and reindexing.
- [x] Specify the pure-effect criterion without conflating empty and unknown
  effect rows.
- [x] Specify residual behavior for effectful dependent computations.
- [x] Specify observational action through admitted pure F/U boundaries.
- [x] Confirm that no second Pi, Lambda, APP, or Match syntax is required.

Exit: all admitted CBPV rules commute with ordinary substitution on paper.

### T12-4: Characterization and law tests without equality tags

- [x] Add a context-category companion test with Pi/Lambda/App reindexing laws.
- [x] Add Comp/Return/Thunk/Force reindexing tests.
- [x] Add Match motive and dependent constructor telescope reindexing tests.
- [x] Test that one shared core with different classifiers does not establish
  object equality.
- [x] Test that unknown effects are not classified as pure.
- [x] Run all prototype scripts and examples 01-07 and 09.
- [x] Confirm byte-identical artifact v61 output for a representative input.

Exit: current implementation is characterized against the frozen laws without
adding HOTT object terms.

### T12-5: Freeze the S1 handoff

- [x] Define separate typed records for unification, conversion, HOTT
  construction, and persistent residual obligations where required.
- [x] Give every record a stable rule identity and validation owner.
- [x] Define which records remain compiler-local and which require artifact
  v62.
- [x] Map each future proof payload to the normative rule that justifies it.
- [x] Review the schema before changing `ast.c`, `judgement.h`, or artifact
  serialization.

Exit: V2-S1 can be implemented without guessing any HOTT semantic field.

### T12-6: Review gate

- [x] No duplicated graph constructor has been proposed.
- [x] DefEq, solver equality, and object equality remain separate.
- [x] All accepted type formers have substitution and naturality laws.
- [x] Unsupported effectful cases have explicit residual or rejection rules.
- [x] The parent V2 matrix is updated to mark T1/T2 complete only after all
  normative outputs and law tests pass.

## 11. Anticipated Code Changes After the Freeze

No code change is authorized merely by writing this plan. Once T1/T2 passes,
the likely V2-S1 implementation surface is:

| Area | Expected change |
| --- | --- |
| `src/prototype/ast.c` | replace positional HOTT-relevant constraints with typed records or delegate them to a solver module |
| `src/prototype/judgement.h/.c` | add rule-specific proof payloads only for frozen rules |
| `src/prototype/context.h/.c` | expose any missing structural lift/reindex operation; do not add observational semantics here |
| `src/prototype/typing.c` | check typed HOTT construction requests and residual boundaries |
| `src/prototype/term.h/.c` | eventually add only the object constructors required by the frozen calculus; do not duplicate APP/LAMBDA/PI |
| artifact reader/writer | migrate once to v62 after proof and HOTT records are final |

An observational implementation should receive typed occurrences or explicit
`context + classifier + term` inputs. It must not call
`prototype_term_classifier_view` and infer the whole equality semantics from a
core tag.

## 12. Risks and Required Challenges

### 12.1 Pi is currently computation-classified

The phrase "pure Pi" must not hide this fact. The first fragment must state
whether it observes:

- a computation function directly under `Pi(A,C)`;
- its suspended value under `ThunkType(Pi(A,C))`;
- or both, connected by explicit laws.

It may not solve the ambiguity by inventing a value Pi.

### 12.2 Thunk can hide effectful computations

Treating every Thunk as an ordinary value equality would allow an unhandled
effectful graph to be compared without handler semantics. The first fragment
must admit only a justified pure subset or return residual.

### 12.3 Match equality is not raw case-array equality

Match alpha-interning provides core identity. HOTT equality of Match terms is
still carrier-directed and must respect motive, constructor telescope, IH
scope, and reindexing. One must not be substituted for the other.

### 12.4 Existing solver `EQUAL` is not PropEq

`OPERATION_CONSTRAINT_EQUAL` is a classifier-solving equation. Renaming or
typing it may be necessary in S1, but it must not become an object equality
witness merely because both use the word equality.

### 12.5 Type-former semantics remain distributed

Formation and elimination rules currently span TermDB, Typing/JudgementDB,
TypeDeclarationDB, and AST solver code. T12-1 must identify one semantic owner
per admitted rule before an observational authority is implemented.

## 13. Completion Criteria

V2-T1/T2 is complete only when:

- [x] one normative calculus document exists;
- [x] it uses one shared term syntax;
- [x] no value-side Pi or duplicated computation graph is introduced;
- [x] the first-fragment support matrix is reviewed;
- [x] all accepted rules have substitution and naturality laws;
- [x] higher witnesses are not truncated by implementation convenience;
- [x] the dependent-CBPV residual boundary is explicit;
- [x] law-level characterization tests pass;
- [x] artifact v61 remains unchanged;
- [x] the exact V2-S1 typed schema is approved.

V2-S1 is now unblocked. V2-P1, V2-O1, and artifact v62 remain ordered after
the S1 typed-constraint implementation.

## 14. Theory References and A Program-Specific Responsibility

The following references constrain the design but do not define A Program's
calculus automatically:

1. Altenkirch, Chamoun, Kaposi, and Shulman, *Internal Parametricity Without an
   Interval*, POPL 2024, <https://arxiv.org/abs/2307.06448>.
2. Altenkirch, Chamoun, Kaposi, and Shulman, *Higher Observational Type Theory*,
   TYPES 2022, <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>.
3. Vakar, *A Framework for Dependent Types and Effects*, 2015,
   <https://arxiv.org/abs/1512.08009>.

The HOTT work motivates type-former-directed higher observation and coherent
action on substitutions. The dependent-CBPV work identifies constraints on
dependent sequencing and effects. Neither reference supplies the exact
shared-TermDB, residual-verification, no-subtyping calculus required here.
Those rules must be stated and tested as A Program's own normative theory.

## 15. Implementation and Verification Evidence

Normative output:

- `doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`;
- one shared TermDB syntax;
- bridge-context higher closure without a fixed numeric truncation;
- frozen first-fragment matrix;
- four distinct V2-S1 record contracts.

Implementation output:

- `prototype_term_effect_row_purity` classifies rows as `PURE`, `EFFECTFUL`,
  `UNRESOLVED`, or `INVALID`;
- `src/prototype/shared_term_reindex_check.c` checks shared Pi/Lambda/App,
  Comp/Return/Thunk/Force, Match motive, binder shadowing, scoped IH frame
  remapping, dependent telescope reindexing, and substitution laws;
- `src/prototype/test_shared_term_hott_substrate.sh` prevents duplicate
  value/computation core tags and premature equality tags.

Validation on 2026-08-07:

- clean `make` and `make reader` builds completed without warnings;
- all fourteen `src/prototype/test_*.sh` scripts passed;
- examples 01-07 and 09 compiled successfully;
- the baseline and changed compiler both emitted `A_PROGRAM_ARTIFACT 62` for
  `examples/07_add.p`;
- both artifact files had SHA-256
  `c5ea541a817ec9a2f87aa7f013afda65a5c8ea710b0293cdb39f13c50e908a31`;
- `git diff --check` passed.
