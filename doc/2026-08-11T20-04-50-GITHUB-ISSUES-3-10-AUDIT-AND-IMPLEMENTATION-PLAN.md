# GitHub Issues #3-#10: Current-Code Audit and Implementation Plan

Date: 2026-08-11

Status: implemented and verified; the containing Git commit is the publication
record for this completed plan.

## 1. Purpose and authority

This document audits every open GitHub issue in `repyt-margorp/a-program` as
of 2026-08-11 and turns each issue into an implementation and progress plan.
The issues were reported against commit `54200d1`. The audit was performed on:

```text
branch: main
base commit: a9de28e679ca929c442057f7fa145d238d95d172
artifact after implementation: v72
```

The current worktree and executable behavior are authoritative. Issue-body
hypotheses are retained only where current code and reproduction support them.

Open issues covered here:

| Issue | Title |
| ---: | --- |
| [#3](https://github.com/repyt-margorp/a-program/issues/3) | Clarify the proof boundary between SubstitutionDB and CwF certificates |
| [#4](https://github.com/repyt-margorp/a-program/issues/4) | Add regression tests for host-typed ADT literals and recursive constant motives |
| [#5](https://github.com/repyt-margorp/a-program/issues/5) | Support constant-motive induction over multiple recursive fields |
| [#6](https://github.com/repyt-margorp/a-program/issues/6) | Add regression coverage for higher-order List map under induction |
| [#7](https://github.com/repyt-margorp/a-program/issues/7) | Define and test outer-IH scope across nested matches for insertion sort |
| [#8](https://github.com/repyt-margorp/a-program/issues/8) | Refresh and continuously run the type-infer-and-check example suite |
| [#9](https://github.com/repyt-margorp/a-program/issues/9) | Add an inspection-only `:type` without reflection |
| [#10](https://github.com/repyt-margorp/a-program/issues/10) | Make `#40 :: #.Int` work without turning `::` into synthesis input |

Issue #10 has two author comments. The
[second comment](https://github.com/repyt-margorp/a-program/issues/10#issuecomment-5250378246)
explicitly supersedes both the first comment and the unresolved alternatives in
the issue body. It is normative for this plan: every unsuffixed numeric literal
synthesizes the Intrinsic environment's `#.Int`; the default environment maps
`#.Int` to `#.Int32`, while `#.Int64` remains distinct.

## 2. Progress legend

| Mark | Meaning |
| --- | --- |
| `[x]` | Verified during this audit or already implemented |
| `[ ]` | Required implementation work |
| `[~]` | Partially present; contract or coverage remains incomplete |
| `[!]` | Decision or invariant that must be preserved |

Issue progress is tracked by phase, not by a single percentage. An issue is
complete only when its implementation, focused tests, integration tests,
artifact boundary, diagnostics, and documentation gates all pass.

## 3. Audit method

- Read every open issue body and all current comments. Issues #3-#9 have no
  comments. Issue #10 has two comments; the later language-author decision is
  applied here and the earlier decision is treated as superseded.
- Re-ran each supplied minimal program with current `./read_file.out`.
- Ran all files under `examples/type-infer-and-check` against the current
  prototype reader.
- Traced failed fixed-point states using an unoptimized debug build and GDB.
- Read the current parser, AST lowering, OperationGraph classifier solver,
  Judgement rules, Context/Substitution implementation, CwF certificate code,
  artifact v71 schema/readback, REPL, test layout, and build manifests. The
  implementation then introduced artifact v72 and archived v71 without a
  fallback reader.
- Distinguished an erased TermDB identity from a typed Operation occurrence.

Temporary diagnostic fixtures were placed under `/tmp`; they are not source
changes and are not acceptance tests.

## 4. Initial reproduction matrix

| Issue | Result at base commit `a9de28e` | Audit conclusion |
| ---: | --- | --- |
| #3 | Structural and classifier validation pass without certificate coverage | The issue is current. The three guarantees are not named or enforced at one boundary. |
| #4a | `Expr.literal #42` fails in pending classifier inference | Lowering hard-codes Int64 as the literal principal; constructor formation does not contextually replace it with the `#.Int` field domain. The normative fix is an environment-selected `#.Int` principal, not constructor-driven synthesis. |
| #4b | recursive `Expr -> #.Text` fails in pending classifier inference | The immediate failure is the same erased-binder ownership bug as #5, not a Host-type prohibition. |
| #5 | left IH of a two-recursive-field constructor fails | Typed case identity is lost during IH validation; a colliding Core binding from another case is selected first. |
| #6 | monomorphic `mapNat` fails | Match motive equality compares whole computation classifiers and cannot join `{}` with the captured function's effect-row variable. |
| #7a | unparenthesized nested Match rejects `*tail` in the reader | The parser assigns following `@` clauses to the outer elimination; this is grouping ambiguity, not lost Core IH scope. |
| #7b | the same program with the nested Match parenthesized passes | Outer-frame IH lowering, solving, proof materialization, and evaluation already work for this case. |
| #7c | recursive comparator plus eager insertion still fails generically | A separate composition/fixed-point failure remains and must be re-audited after #5/#6 boundaries are fixed. |
| #8 | all 16 `.p` files fail before advertised behavior | Level 0-2 use removed combined declaration syntax; Level 3 has additional stale grammar/schema assumptions. |
| #9 | no `:type` command exists | Compile labels retain only one classifier/operation view and do not preserve separate body/ascription inspection data. |
| #10 | top-level and inline integer ascriptions fail | The implementation selects Int64 as principal, contrary to the latest language decision. Existing Int32 admissibility explains contextual success but is not the intended principal-synthesis rule. |

### 4.1 Result after implementation

| Issue | Implemented result |
| ---: | --- |
| #3 | Structural, classifier-coherent, and certified substitution guarantees are separate APIs; certified consumers require exact Claim coverage and v72 replays it. |
| #4 | Configured-Int constructor fields, recursive `#.Text`/`#.Int` motives, and the expression evaluator returning 42 pass with accepted evidence and artifact replay. |
| #5 | IH identity is the typed owner Match/case/field occurrence; left/right/both/shared-Core and binary-tree size pass. |
| #6 | Match result carriers and branch effect rows are solved separately; pure/effectful higher-order `List.map` and row forgery tests pass. |
| #7 | Parenthesized outer-IH scope, precise grouping diagnostics, comparator, branch-sensitive insertion, and three-element insertion sort pass. |
| #8 | All 16 examples are manifest-owned: 12 pass and 4 reach their advertised unsupported boundary. |
| #9 | `:type` reports raw occurrence-local body and expectation projections without mutation or reflection. |
| #10 | Unsuffixed literals synthesize the selected environment's `#.Int`; default Int32 range, post-synthesis `::`, v72 environment replay, and alternate-environment rejection pass. |

## 5. Shared architectural findings

### 5.1 Occurrence-local classifier evidence: #4a, #9, #10

The current architecture correctly permits two typed Operation occurrences to
share one erased TermDB node. A principal classifier therefore belongs to an
Operation occurrence, not to a Term ID.

Three distinct concepts must be made explicit:

```text
principal classifier
    the one classifier selected by synthesis for an Operation occurrence

admissible classifier view
    an independently derived HAS_TYPE result for that same occurrence; it is
    never a replacement for the configured principal-synthesis rule

expected classifier
    a post-synthesis assertion or a rule-owned input domain
```

For an unsuffixed numeric literal, the principal is not an unresolved overload:
it is the Intrinsic environment's `#.Int`, selected before APP, constructor,
Match, Lambda, or `::` can constrain it. In the default environment this is the
same Host type as `#.Int32`, not `#.Int64`. Constructor fields and ordinary APP
then check that principal normally. `::` remains a post-synthesis assertion and
must not alter the choice. Any future suffixed or explicitly width-polymorphic
literal syntax requires a separate rule rather than reusing expected-type
propagation implicitly.

### 5.2 Match has two coupled solvers: #4b, #5, #6, #7

The Match result currently mixes:

1. the dependent value/result motive; and
2. the CBPV computation effect row of each branch.

The result carrier can be constant while branch effects differ. Requiring
DefEq of complete `Comp(E, A)` classifiers is too strong. Motive equations and
effect-row equations need distinct records and a coordinated fixed point.

IH ownership also belongs to the typed Match occurrence and exact case field.
An erased Core VAR binding cannot recover that identity after alpha interning.

### 5.3 Structural graph versus certified use: #3

SubstitutionDB is useful as an interned structural/candidate graph. It should
not be made globally proof-carrying merely because some consumers require CwF
morphisms. Conversely, a consumer must not treat a raw ID as certified.

The intended contract should be:

```text
SubstitutionId
    structurally valid mapping with classifier-coherent EXTEND payload

CertifiedSubstitutionRef
    SubstitutionId plus exact CwF formation evidence

accepted artifact use
    every reachable proof use of EXTEND has reconstructible accepted evidence
```

This keeps tentative compiler graphs possible while making proof-bearing
boundaries explicit.

### 5.4 Examples must be executable specifications: #8

README prose is not a test manifest. The current hierarchy drifted because no
machine-readable expectation owns each file. The runner and manifest must be
authoritative; the README must describe that executable state.

## 6. Required implementation order

The issues should not be fixed in issue-number order.

| Order | Work package | Issues | Reason |
| ---: | --- | --- | --- |
| 0 | Freeze focused reproductions and diagnostic categories | all | Prevent fixes from moving failures without explaining them. |
| 1 | Typed IH field ownership | #4b, #5, part of #7 | Removes an actual erased-Core identity bug before extending motive solving. |
| 2 | Match result/effect constraint separation | #6, remaining #7 | Required for higher-order calls and recursive composition. |
| 3 | Intrinsic `#.Int` principal and post-synthesis assertion APIs | #4a, #10 | Fix the language-level literal invariant before adding inspection metadata. |
| 4 | Compile-label inspection projection and `:type` | #9 | Must consume the classifier/evidence model rather than invent another one. |
| 5 | CwF candidate/certificate boundary | #3 | Independent kernel work; may proceed in parallel but artifact semantics require a deliberate version decision. |
| 6 | Refresh executable example suite | #8 | Migrate syntax early, but mark feature expectations only after packages 1-5 settle. |
| 7 | Full book-algorithm integration | #4-#8 | Expression evaluator, tree algorithms, map, insertion sort, artifact replay. |

### 6.1 Primary code ownership map

This table names the expected review surface. It is not permission to change
every listed file; implementation should touch only the owners required by the
selected package.

| Issue | Primary implementation owners | Test/specification owners |
| ---: | --- | --- |
| #3 | `src/kernel/context.c`, `src/kernel/cwf_certificate.c`, `src/kernel/kernel_view.c`, corresponding `include/a_program/kernel/` headers, certified consumers under `src/identity/` and `src/parametricity/` | Context/CwF checks, artifact replay/forgery, next artifact schema |
| #4 | `src/frontend/lowering/context_and_type_lowering.inc`, constructor formation in `graph_construction.inc`/`constraint_solver.inc`, typed IH owners from #5 | focused Host constructor/Match fixtures and expression evaluator |
| #5 | `include/a_program/graph/operation_model.h`, IH lowering/constraints, Match/IH proof emission, accepted replay | multi-field tree fixtures, shared-Core checks, artifact forgery |
| #6 | Match constraint records in `include/a_program/graph/operation_model.h`, `src/frontend/lowering/constraint_solver.inc`, Match proof/replay rules | pure/effectful map fixtures and artifact replay |
| #7 | `src/frontend/reader.c`, Match/IH lowering from #5, Match effect solving from #6 | grouping diagnostics, insertion/comparator/sort fixtures |
| #8 | example manifest/validator, prototype integration runner, current example sources and README | all 16 manifest rows and normal CI entry point |
| #9 | `include/a_program/graph/operation_model.h`, compile metadata, lowering finalization, compiler session, `src/driver/repl.c` | read-only REPL and graph/artifact immutability checks |
| #10 | Intrinsic declarations/environment, literal lowering, literal proof emission/replay, ascription finalization, artifact header/link checks | configured-range, ascription, environment, artifact, and `:type` fixtures |

Paths in this table are relative to `src/prototype/` unless they name an
example, repository README, or documentation file explicitly.

## 7. Issue #3: SubstitutionDB and CwF certificates

### 7.1 Current code

- `src/prototype/src/kernel/context.c:601` constructs EXTEND nodes and checks
  that the supplied classifier is reference-equal to the reindexed target
  classifier.
- The constructor rejects invalid sentinels but does not directly check both
  Term IDs against `term_count`.
- `src/prototype/src/kernel/context.c:799` validates graph shape and only the
  EXTEND term bound.
- `src/prototype/src/kernel/context.c:869` checks both Term bounds and
  classifier compatibility, but it does not check a Claim or certificate.
- `src/prototype/src/kernel/cwf_certificate.c:155` creates an EXTEND certificate
  only from an exact accepted `HAS_TYPE` Claim.
- `src/prototype/src/kernel/cwf_certificate.c:173` validates certificates that
  exist; it does not require coverage of selected substitutions.
- `src/prototype/src/kernel/kernel_view.c:5` delegates to that non-coverage
  validator.
- Artifact v71 explicitly excludes compiler-local certificates while retaining
  accepted Contexts, Substitutions, Claims, Derivations, and semantic actions.

### 7.2 Decision

[!] Adopt the candidate graph model internally and proof-carrying use at
explicit kernel/artifact boundaries. Do not require a certificate for every
interned substitution, because solver and relation-action construction may
contain unselected candidates.

[!] Identity, empty, projection, and composition remain primitive structural
CwF constructors. EXTEND is the only primitive requiring a term typing Claim.
Certified composition recursively requires certified EXTEND leaves and valid
endpoints; it does not need a duplicate Claim for the composition node.

### 7.3 Implementation plan

#### CWF-0: name the guarantees

- [x] Add a normative table to `src/prototype/README.md` and kernel headers:
  structural validity, classifier coherence, and certified formation.
- [x] Document Context generativity: binding identity is not Context
  alpha-quotienting.
- [x] Rename `prototype_substitution_db_validate_typed` to
  `prototype_substitution_db_validate_classifier_coherence`.
- [x] Do not leave a compatibility alias; update all callers in one change.

#### CWF-1: strengthen construction without conflating proof

- [x] Check `term < term_count` and `term_classifier < term_count` directly in
  `prototype_substitution_extend`.
- [x] Keep classifier reindex/reference checks in the constructor.
- [x] Add focused failure reasons for malformed endpoints, target parent,
  reindex failure, classifier mismatch, and out-of-range terms.
- [x] Keep Claim lookup out of `prototype_substitution_extend`.

#### CWF-2: add a certified capability

- [x] Introduce a small kernel-facing reference containing substitution ID and
  certificate ID; do not add another semantic graph.
- [x] Add an API that verifies the exact pair and returns the certified view.
- [x] Change parametricity/object-Identity/CwF consumers that need a morphism to
  accept the certified view, not an unqualified raw ID.
- [x] Add a coverage validator over an explicit set of authoritative roots.
- [x] Do not make `prototype_kernel_view_validate` silently mean global
  coverage; either give it required roots or split structural and certified
  view validation by name.

#### CWF-3: artifact contract

- [x] For every accepted derivation semantic action that reaches an EXTEND,
  require the exact accepted `HAS_TYPE` Claim used to certify that extension.
- [x] Reconstruct certificate coverage from accepted Claims on readback, or
  serialize a compact edge if reconstruction is not unique.
- [x] Treat either change as an artifact semantic-contract change and create
  artifact v72; do not mutate the v71 manifest in place.
- [x] Continue excluding solver candidate certificates and HOTT work queues.

#### CWF-4: tests and teaching output

- [x] Raw structurally valid uncertified EXTEND is accepted by SubstitutionDB.
- [x] The same ID is rejected by a certified CwF consumer.
- [x] Exact Claim creates a valid certified reference.
- [x] Wrong Context, subject, classifier, or Claim is rejected.
- [x] Certified composition succeeds only when all EXTEND leaves are covered.
- [x] Artifact forgery deleting the required Claim or changing its classifier
  is rejected.
- [x] Add inspection output showing a Context telescope, substitution mapping,
  and the exact Claim/certificate used.
- [x] Add one glossary/status table distinguishing Proposition, Claim,
  Derivation, Certificate, Context, Substitution, reindexing, and Runtime
  Environment by structural/checked/certified/surface status.
- [x] Keep Runtime Environment output visibly separate from static Context and
  Substitution output.
- [x] Add human-facing small-value readback beside, not instead of, Core debug
  readback.
- [x] Audit touched book-facing documentation for the canonical spelling
  `A Program`; do not mix this kernel change with an unbounded repository-wide
  prose rewrite.

### 7.4 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Current-code audit | complete | Re-read constructors, validators, certificate DB, KernelView, and v71/v72 schemas |
| CWF-0 contract | complete | README guarantee table; only `validate_classifier_coherence` remains |
| CWF-1 bounds/diagnostics | complete | focused HOTT scaffold checks every EXTEND failure reason |
| CWF-2 capability | complete | exact certified reference and covered/uncovered composition checks |
| CWF-3 artifact | complete | v72 exact-evidence replay and missing/wrong Claim forgeries |
| CWF-4 book tooling | complete | structured Context/Substitution/evidence and Runtime Environment dump |

## 8. Issue #4: Host fields and recursive Host motives

This issue contains two independent defects and must not be closed by one
regression alone.

### 8.1 Constructor literal defect

Current ordinary APP solving calls
`operation_solver_specialize_integer_literal` before applying a Pi classifier
at `src/prototype/src/frontend/lowering/constraint_solver.inc:2503`.

Constructor application uses a different constraint,
`OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION`, and
`operation_solver_constructor_spine_classifier` at
`src/prototype/src/frontend/lowering/graph_construction.inc:5746`. It collects
already-selected argument classifiers and calls the constructor telescope
checker. It never specializes an integer literal against each field domain.

The deeper mismatch is earlier:
`src/prototype/src/frontend/lowering/context_and_type_lowering.inc:1747-1754`
hard-codes Int64 as the classifier of every unsuffixed integer literal even
though the Intrinsic namespace table at
`src/prototype/src/core/term/declarations.inc:160-164` maps `Int` and `Int32`
to the same Host type. Therefore `#42` remains Int64 and fails a
`#.Int`/Int32 field, while a separately typed `#.Int` variable succeeds.

The latest Issue #10 decision makes adding the APP specialization to
constructor formation the wrong final fix. It would preserve expectation-driven
literal synthesis and make the selected principal depend on where the literal
appears. Once literal lowering independently selects the environment's
`#.Int`, both APP and constructor formation should perform ordinary domain
checking without replacing the literal principal.

### 8.2 Recursive Host motive defect

The Host carrier is not the cause. Debug tracing found:

```text
literal-case binder Core binding: 4, recursive=false
wrap-case binder Core binding:    4, recursive=true
IH argument Core binding:         4
```

`operation_solver_validate_guarded_motive_occurrence` scans all Core Match
cases by `binding_id` and returns failure at the first nonrecursive collision.
The same defect occurs with constructive `Nat` in #5.

### 8.3 Implementation plan

#### HST-0: focused fixtures

- [x] Add positive direct-literal constructor-field fixtures for `#.Int` and
  its default `#.Int32` alias.
- [x] Pin the default `#.Int64` policy separately: a typed Int64 variable may
  inhabit an Int64 field, while an unsuffixed literal does not acquire Int64
  from that expected field under the ASC-1 default plan.
- [x] Add in-range and out-of-range negative fixtures.
- [x] Add recursive constant motives returning `#.Text` and `#.Int`.
- [x] Assert the failing solver reason and source Operation before changing it.

#### HST-1: consume the configured literal principal

- [x] Implement ASC-0/ASC-1 from Issue #10 first so `#42` independently
  synthesizes the environment's `#.Int`.
- [x] Remove the APP-only principal-replacement rule
  `operation_solver_specialize_integer_literal`; APP and constructor formation
  must use the same already-selected argument classifier.
- [x] Keep constructor telescope checking occurrence-local and verify each
  field domain against the exact argument Operation.
- [x] Do not add expected-domain propagation from a constructor field into
  unsuffixed literal synthesis.
- [x] Preserve configured-range rejection before an Operation receives its
  principal classifier.
- [x] If explicit-width literal syntax is added later, give it a separate AST
  and proof rule rather than reviving contextual overload replacement.

#### HST-2: recursive motive

- [x] Implement the typed IH field-identity work in Issue #5.
- [x] Remove the all-case Core binding scan from guarded motive validation.
- [x] Verify that no Host-type-specific branch remains in motive inference.

#### HST-3: end-to-end expression evaluator

- [x] Add `literal`, `add`, and `negate` expression constructors.
- [x] Check `evaluate : Expr -> #.Int` and the exact teaching case
  `50 + negate (3 + 5) = 42`.
- [x] Check accepted Match/IH/primitive Claims, not only runtime output.
- [x] Write/read/link the artifact and replay the accepted proof graph.
- [x] Add source-located diagnostics for literal-domain and motive failures.

### 8.4 Progress

| Subproblem | State | Dependency |
| --- | --- | --- |
| Direct constructor literal reproduced | complete | none |
| Configured `#.Int` principal | complete | #10 ASC-0/ASC-1 |
| Recursive Text motive reproduced and traced | complete | none |
| Typed IH field ownership | complete | #5 |
| Evaluator and artifact replay | complete | returns 42; read, aggregate, and replay pass |

## 9. Issue #5: multiple recursive fields

### 9.1 Proven current root cause

The failure is not yet evidence that the constant-motive equation is too hard.
The solver already finds a constant candidate. It then rejects the IH because
Core binding IDs are alpha-interned and are not globally unique case-field
identities.

In the current reproduction:

```text
IH Operation:       operation #6
owner Match:        operation #12
constant candidate: term #59
IH Core binding:    2
literal case field: binding 2, recursive=false
pair.left field:    binding 2, recursive=true
pair.right field:   binding 5, recursive=true
```

The validator encounters the literal field first and returns `-1`. This is a
typed-occurrence ownership error caused by recovering proof identity from the
erased graph.

### 9.2 Required representation

An IH occurrence must retain:

```text
owner Match Operation ID
IH scope/frame ID
owner case index
recursive field index
source AST binder ID
argument Operation ID
```

The Core term may remain `INDUCTION_HYPOTHESIS(frame, VAR(binding))`; no new
Term tag is required. The additional coordinates belong to OperationGraph and
proof payloads.

### 9.3 Implementation plan

#### MRF-0: operation identity

- [x] Extend the lowering-time IH-scope map with case and field ordinals.
- [x] Add dedicated IH occurrence fields or a referenced IH-edge record to the
  OperationGraph; do not overload unrelated generic fields further.
- [x] Fill the owner Match Operation after branch lowering, as is already done
  for the parent edge.
- [x] Validate AST binder, case field, Core argument, and recursive flag as one
  atomic relation.

#### MRF-1: solver and proof payload

- [x] Replace `operation_solver_validate_guarded_motive_occurrence`'s global
  Core binding scan with the exact typed IH edge.
- [x] Make `operation_solver_set_ih_motive_application` key the equation by
  owner Match and exact field occurrence.
- [x] Materialize `INDUCTION_HYPOTHESIS_ELIM` with the same case/field payload.
- [x] Verify accepted replay against the typed owner and payload rather than
  rediscovering a field by Core binding.

#### MRF-2: tests

- [x] zero-IH branch, left-only, right-only, and both-IH cases.
- [x] Direct and computation-block sequencing forms.
- [x] A negative nonrecursive-field IH.
- [x] A forged wrong-case and wrong-frame payload.
- [x] Binary-tree `size` and concrete evaluation.
- [x] Artifact write/read/link preserving owner Match, case, and field.
- [x] Shared-Core fixture proving two case fields may share a Core binding ID
  without sharing typed IH identity.

### 9.4 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Reproduction | complete | base commit failed before typed field identity |
| Root-cause trace | complete | exact colliding case bindings observed |
| MRF-0 representation | complete | typed Match/case/field identity in OperationGraph and wire v72 |
| MRF-1 solver/proof | complete | accepted replay plus wrong edge/frame/subject/proof forgeries |
| MRF-2 algorithms | complete | left/right/both/size/shared-Core tests and artifact round trip |

## 10. Issue #6: higher-order List map under induction

### 10.1 Proven current root cause

Current `mapNat` reaches these branch classifiers:

```text
nil branch:  Comp({}, List Nat)
cons branch: Comp(?e, List Nat)
```

The `?e` row comes from applying the captured `transform` parameter. The result
carrier is already the same.

`build_operation_uniform_motive` compares complete branch classifiers by
DefEq. It therefore rejects the uniform motive. The fallback builds a
case-indexed motive, but `operation_solver_materialize_match_solution` then
requires that result to be DefEq to the original constant seed and fails at
`src/prototype/src/frontend/lowering/constraint_solver.inc:3765-3770`.

This is not a generic higher-order unification failure. It is a missing Match
effect-row equation/join at a boundary that currently treats `Comp(E, A)` as
one indivisible motive.

### 10.2 Semantic rule

For a value Match whose branches return computations:

```text
branch_i : Comp(E_i, M(C_i fields))
```

the solver must establish separately:

```text
result equation: branch_result_i = M(C_i fields)
effect equation: E_match includes E_i
```

For a constant result carrier, `M = \_ => List B` can be solved independently
of whether branch effects are `{}`, a closed operation set, or a generalized
row variable.

### 10.3 Implementation plan

#### HOF-0: split Match constraints

- [x] Change motive-case constraints to retain branch computation view:
  effect row and result carrier separately.
- [x] Solve the motive from result carriers only.
- [x] Add an explicit Match branch-effect constraint producing the least row
  accepted by every branch under the current row-variable discipline.
- [x] Do not make effect-row union a global DefEq reduction.

#### HOF-1: row ownership and generalization

- [x] Distinguish an unsolved local row metavariable from a quantified latent
  effect belonging to a captured function parameter.
- [x] Do not discharge a captured latent row merely because another branch is
  pure.
- [x] Generalize the resulting map function only after branch-row constraints
  are solved.
- [x] Ensure a pure supplied transform permits pure execution, while an
  effectful transform propagates its effect through map.

#### HOF-2: proof reconstruction

- [x] Extend `SOLVED_MATCH_MOTIVE`/`MATCH_ELIM` evidence with the branch effect
  premises already represented in the OperationGraph constraint solution.
- [x] Verify the joined row in accepted replay.
- [x] Persist only solved rows or a supported residual obligation; do not
  serialize solver metavariables as though they were accepted types.

#### HOF-3: tests

- [x] Direct constructor mapper.
- [x] First-order helper call in a recursive branch.
- [x] Monomorphic pure `mapNat`.
- [x] Fully polymorphic `map`.
- [x] Effectful transform and inferred effect propagation.
- [x] Direct and computation-block forms.
- [x] Empty, singleton, and multi-element evaluation.
- [x] Artifact replay and forged branch-row rejection.

### 10.4 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Reproduction | complete | base commit failed before motive/effect separation |
| Branch classifier trace | complete | `{}` versus row variable, same carrier |
| HOF-0 motive/effect split | complete | result-carrier motive and explicit row-union constraints |
| HOF-1 generalization | complete | pure and effectful higher-order map fixtures |
| HOF-2 proof/artifact | complete | replay checks row inclusion; forged solved row is rejected |
| HOF-3 book example | complete | direct/helper/mono/poly/block and empty/single/multi cases |

## 11. Issue #7: outer IH and nested Match

### 11.1 Current lexical result

`parse_elimination_suffix` pushes outer case binders while parsing its body,
and lowering maps each AST binder to an IH frame. The lexical data is capable
of retaining an outer IH.

The failure occurs because `parse_case_body` parses an unparenthesized body
with `parse_app_term`, not a complete nested elimination. The subsequent
`@true`/`@false` tokens are consumed as additional clauses of the outer Match.
The parser then pops the outer `tail` binder before it reaches `*tail`.

Parenthesizing the nested Bool Match causes `parse_term` to own those clauses.
The current compiler then succeeds through:

- outer and inner Match Operation construction;
- outer IH owner edge;
- constant motive solution;
- `INDUCTION_HYPOTHESIS_ELIM` proof;
- nested computation-fold sequencing; and
- complete judgement materialization.

### 11.2 Syntax decision

The unparenthesized notation is genuinely ambiguous because both outer and
inner eliminations use the same postfix `@label => body` syntax. The parser
cannot know from syntax alone whether `@true` belongs to the Bool scrutinee or
the outer ADT.

[!] Keep explicit parentheses as the grouping mechanism. Do not add
type-directed parser reassociation or indentation-sensitive ownership.

The issue should be fixed by a precise diagnostic and documented grammar, not
by pretending the grammar is unambiguous.

### 11.3 Implementation plan

#### OIH-0: parser diagnostic

- [x] Add a reader-side elimination diagnostic context that retains the names
  and spans of case binders closed earlier in the currently parsed suffix.
- [x] When unresolved `*name` matches such a closed binder, report that the
  intervening `@` clauses were grouped as sibling clauses and that a nested
  elimination body requires parentheses. Do not re-associate the AST.
- [x] After constructor resolution, diagnose an impossible sibling clause
  label at its `@` span as the same grouping class; this covers ambiguous
  programs that contain no IH reference and therefore pass the reader.
- [x] Keep the generic nonlocal-IH diagnostic for names that do not match a
  closed case binder.
- [x] Document the parenthesized spelling in the surface grammar and book.

#### OIH-1: freeze the already-working scope rule

- [x] Add a positive parenthesized nested Match using an outer IH.
- [x] Add an outer IH whose result is a function and apply it inside the nested
  Match.
- [x] Add wrong-frame and nonrecursive-field negatives.
- [x] Assert owner Match Operation, IH scope, case, and field after #5.
- [x] Artifact round-trip the exact frame payload.

#### OIH-2: recursive comparator composition

- [x] Retain separate fixtures for comparator, eager insertion, and sort-shaped
  traversal.
- [x] Re-run composition after #5 exact field identity and #6 Match effect-row
  solving are complete.
- [x] If still failing, record the first unresolved/contradictory constraint as
  a source diagnostic before changing the solver.
- [x] Add branch-sensitive insertion and an evaluation trace proving the tail
  IH is not executed in the early-insert branch.
- [x] Add complete insertion sort over three elements and artifact replay.

### 11.4 Progress

| Subproblem | State | Result |
| --- | --- | --- |
| Unparenthesized failure | complete | reproduced in reader and now precisely diagnosed |
| Parenthesized outer IH | complete | implementation worktree passes |
| Grammar decision | complete | parentheses required |
| Precise diagnostic/docs | complete | reader/compiler reason code and source span |
| Typed frame regression | complete | exact owner/frame/case/field artifact payload |
| Recursive composition | complete | comparator and branch-sensitive insertion pass |
| Insertion sort | complete | three-element sort, trace, and artifact replay |

## 12. Issue #8: type-infer-and-check suite

### 12.1 Current state

There are 16 source files:

| Level | Files | Current earliest failure |
| ---: | ---: | --- |
| 0 | 3 | removed `name : Type := body` syntax |
| 1 | 5 | removed combined declaration syntax |
| 2 | 4 | removed combined declaration syntax, masking IH behavior |
| 3 | 4 | constructor result/old indexed-family grammar assumptions |

The README still invokes `./bin/program`, describes old output, references
`T_IH_CALL`, and treats manual execution as the development workflow. No
manifest or integration runner owns these expectations.

### 12.2 Implementation plan

#### EX-0: manifest before migration

- [x] Add a machine-readable manifest with path, expectation, diagnostic code,
  required runtime result, and artifact-replay flag.
- [x] Give every existing file exactly one row.
- [x] Reject duplicate paths, missing files, and unlisted `.p` files.
- [x] Use stable diagnostic categories, not full stderr snapshots.

#### EX-1: current syntax

- [x] Convert Level 0 and Level 1 to split assignment/ascription syntax.
- [x] Convert Level 2 syntax without changing the algorithm being tested.
- [x] Split files where a supported algorithm is followed by a future feature
  that would mask it.
- [x] Keep Level 3 as explicit negative fixtures until indexed families exist.
- [x] Verify each Level 3 failure reaches the advertised indexed-family
  boundary, not parser drift.

#### EX-2: integration runner

- [x] Add a runner under `src/prototype/tests/integration`.
- [x] Build/use the current prototype executable through the existing source
  manifest; do not hard-code a stale binary path.
- [x] Check exit status, diagnostic category, selected runtime output, and
  artifact replay according to the manifest.
- [x] Add the runner to the normal prototype integration command/CI.

#### EX-3: synchronize with algorithm issues

- [x] Split List map and direct List recursion so #6 cannot mask append/length.
- [x] Split binary-tree constant, one-IH, and both-IH cases for #5.
- [x] Add the expression evaluator from #4 and insertion sort from #7 after
  their focused tests pass.
- [x] Generate or mechanically verify README status tables from the manifest.

### 12.3 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Inventory/re-run | complete | all 16 files classified |
| EX-0 manifest | complete | exact coverage/duplicate/missing-file validator |
| EX-1 syntax migration | complete | Levels 0-2 pass; Level 3 has stable expected failures |
| EX-2 CI runner | complete | normal `test-integration` discovers the manifest runner |
| EX-3 algorithm synchronization | complete | focused #4-#7 fixtures run independently |
| README refresh | complete | generated status table is checked against the manifest |

## 13. Issue #9: inspection-only `:type`

### 13.1 Current code

`prototype_compile_label` currently stores:

```text
name, term, classifier, operation, canonical key
```

`compile_phase_check_expectations` deliberately obtains the body Operation's
classifier when the definition root is ASCRIPTION, but later mutates
`compiled_classifier` for exposure. `compile_phase_publish_labels` publishes
only that final single field. A REPL implementation that prints
`label->classifier` would therefore not reliably satisfy the issue contract.

The REPL has `:whnf` and `:nf` in
`src/prototype/src/driver/repl.c:809-826`; no `:type` command exists.

### 13.2 Required inspection projection

Compile metadata must preserve, for a named assignment:

```text
body_operation
raw_principal_classifier
exposed/root_operation
optional expectation classifier
ascription evidence/claim and check mode
```

This is derived inspection metadata over existing Operation/Judgement records,
not a new type authority. Artifact exports continue to use the published root
contract; REPL inspection must not alter it.

### 13.3 Implementation plan

#### TYP-0: metadata model

- [x] Replace the ambiguous single label operation/classifier meaning with
  explicitly named body and exposed fields.
- [x] Populate them at definition compilation and expectation checking.
- [x] Store the exact accepted Claim/evidence for a successful ascription.
- [x] Validate that the body Operation has one selected principal classifier.
- [x] Keep inline ascription inspection occurrence-local even when the named
  root is an outer ASCRIPTION Operation.

#### TYP-1: read-only API

- [x] Add a const query API returning a value object with body classifier and
  optional expectation status.
- [x] The API must not call synthesis, normalization, conversion, evaluation,
  interning, or proof construction.
- [x] Return an explicit unavailable/ambiguous state instead of selecting a
  Judgement by Term ID.

#### TYP-2: REPL command

- [x] Parse `:type name` beside `:whnf` and `:nf`.
- [x] Print the raw classifier with the existing debug readback.
- [x] Print expected classifier and accepted evidence kind separately.
- [x] Do not print WHNF/NF unless a future distinct command requests it.
- [x] Add no object-language syntax or reflection primitive.

#### TYP-3: tests

- [x] Raw reducible classifier remains syntactically unreduced.
- [x] Match shows `APP(motive, scrutinee)`.
- [x] Satisfied DefEq ascription reports body and expectation separately.
- [x] Integer ascription reports `#.Int` as the independently synthesized body
  principal and reports the accepted ascription evidence separately after #10.
- [x] Shared erased Core lambda reports Bool and Nat classifiers by occurrence.
- [x] Snapshot TermDB/Operation/Judgement counts, budgets, and artifact bytes
  before and after `:type`; all must remain unchanged.

### 13.4 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Metadata audit | complete | single-field ambiguity confirmed |
| TYP-0 metadata | complete | body/exposed/expectation projections and exact Claim metadata |
| TYP-1 const API | complete | unavailable/ambiguous checks and unchanged storage counters |
| TYP-2 command | complete | `:type name` prints raw body and separate expectation |
| TYP-3 non-reflection | complete | state snapshots and byte-identical artifacts |

## 14. Issue #10: integer literal ascription

### 14.1 Proven current root cause

An unsuffixed integer literal currently selects Int64 in
`src/prototype/src/frontend/lowering/context_and_type_lowering.inc:1747-1754`.
This is not merely provisional metadata: it becomes the Operation occurrence's
selected principal unless an APP-specific solver rule replaces it.

For an in-range literal, Judgement materialization later derives
`INT_LITERAL_ADMISSIBILITY` at `#.Int`/Int32 in
`src/prototype/src/frontend/lowering/constraint_solver.inc:5522-5557`.
That second Claim explains why some contextual APP uses succeed, but it does
not implement the language invariant for principal synthesis.

Top-level expectation checking obtains the body principal classifier, then
calls `prototype_judgement_solve_expected_effect_rows(expected, actual)` at
`src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc:323-331`.
`#.Int` and Int64 are not DefEq, so the phase rejects before it could inspect
alternate evidence. Inline ascription follows the same wrong initial
principal through a different lowering path.

`prototype_judgement_add_expected_type_exposure` also only derives conversion
or compatibility exposure. It is not the function that should invent literal
admissibility.

### 14.2 Decision

[!] Apply the latest language-author decision from Issue #10:

```text
synthesize(#numLiteral) = configured #.Int
default #.Int == #.Int32
default #.Int != #.Int64
```

The earlier plan to retain Int64 as principal is superseded and must not be
implemented.

[!] `::` remains post-synthesis. It may select exact accepted alternate
evidence for a body occurrence where the language independently derives such
evidence, but it may not select the literal principal, re-run literal solving,
or change the body principal classifier.

[!] Under the default environment, an unsuffixed literal outside the Int32
range is rejected at the literal source span. Its parser storage may remain an
`int64_t`; storage width is not an object-language classifier. An explicit
Int64 literal/conversion syntax is a separate language feature.

### 14.3 Implementation plan

#### ASC-0: make the Intrinsic environment explicit

- [x] Introduce one immutable Intrinsic environment descriptor owning the
  namespace bindings and the default unsuffixed-integer Host type.
- [x] Make the default descriptor map both `Int` and `Int32` to
  `PROTOTYPE_HOST_TYPE_INT32`, with `Int64` distinct.
- [x] Replace direct semantic reads of the file-static namespace table with a
  const environment query; do not resolve the string `"Int"` independently at
  every literal occurrence.
- [x] Thread the selected environment through reader name resolution, compiler
  session/lowering, kernel literal-proof validation, readback, artifact
  publication/readback, and linking without adding mutable global state.
- [x] Change `prototype_intrinsic_namespace_lookup` and source-name readback to
  take the descriptor explicitly; update all callers in one change and retain
  no compatibility alias.
- [x] Define a stable Intrinsic-environment fingerprint containing at least the
  namespace-to-Host-type mapping, primitive signatures, and default literal
  representation.

#### ASC-1: principal synthesis and proof rules

- [x] Change `compile_ref_from_term` so an unsuffixed `INT_LITERAL` receives
  the configured `#.Int` classifier, never a context-selected domain.
- [x] Reject values outside the configured representation before publishing a
  solved Operation classifier, with a source-located range diagnostic.
- [x] Remove `operation_solver_specialize_integer_literal`; APP, constructor,
  Match, Lambda, and ascription must observe rather than replace the principal.
- [x] Make Operation-authoritative `INT_LITERAL_INTRO` record exactly that
  principal and replay it against the same environment contract.
- [x] Stop using Int64-to-Int32 admissibility as the mechanism that gives an
  unsuffixed literal its ordinary type.
- [x] Either remove automatic alternate Int64 evidence for unsuffixed literals,
  or retain it only behind an explicit conversion/literal rule with separate
  tests. The default plan is removal.

#### ASC-2: one post-synthesis assertion path

- [x] Route inline and top-level `::` through one assertion/evidence API.
- [x] First check the exact body Operation's selected classifier by DefEq and
  applicable effect-row compatibility.
- [x] If the language independently provides an admissible alternate view,
  select it by exact Operation, Context, subject, and expected classifier; do
  not scan by shared Term ID.
- [x] Return the accepted Claim/evidence to ascription metadata for Issue #9.
- [x] Never feed `Expected` into literal, APP, constructor, Match, or Lambda
  synthesis.
- [x] Keep the body Operation principal unchanged; only the outer ASCRIPTION or
  exported expectation view exposes `Expected`.
- [x] Preserve distinct source spans and owning Operation IDs.
- [x] Remove duplicated fallback selection paths from finalization.

#### ASC-3: artifact and link contract

- [x] Persist the Intrinsic-environment fingerprint and selected default
  integer representation, or make them an explicit component of the artifact
  calculus identity that is checked independently at read/link time.
- [x] Reject read/link between incompatible `#.Int` configurations rather than
  silently reinterpreting literal Claims.
- [x] Treat this as an artifact semantic-contract change. Coordinate with #3
  so the next format is introduced once, rather than creating competing v72
  drafts.
- [x] Replay literal and ascription Claims against the artifact's declared
  environment contract.

#### ASC-4: tests

- [x] Top-level `#40 :: #.Int`.
- [x] Inline `(#40 :: #.Int)`.
- [x] Contextual `identity #40` remains accepted.
- [x] `:type` reports `#.Int` as the body principal after #9, with the
  ascription check shown separately.
- [x] Default Int32 minimum/maximum pass; adjacent out-of-range values fail at
  the literal span.
- [x] `#.Int` and `#.Int32` are DefEq in the default environment.
- [x] `#.Int` and `#.Int64` are not DefEq in the default environment.
- [x] Unsuffixed `#40 :: #.Int64` follows the explicit policy chosen in ASC-1;
  under the default removal plan it is a source-located mismatch.
- [x] Incompatible ascription is source-located.
- [x] Artifact replay retains the principal Operation, environment identity,
  and exact ascription Claim.
- [x] Changing compile effort/fuel does not change the literal principal.
- [x] A second test environment selecting a supported different `#.Int`
  representation either round-trips with its own fingerprint or is rejected as
  unsupported explicitly; it must never silently use the default.

### 14.4 Progress

| Phase | State | Completion evidence |
| --- | --- | --- |
| Reproduction | complete | top-level and inline failed at the base commit |
| Superseding design authority | complete | latest Issue #10 author comment incorporated |
| Current mismatch trace | complete | hard-coded Int64 principal and secondary admissibility located |
| ASC-0 environment | complete | immutable explicit descriptor, fingerprint, and alternate environment |
| ASC-1 principal | complete | configured Int principal, range checks, and replay |
| ASC-2 assertion | complete | one post-synthesis path for inline/top-level positive and negative cases |
| ASC-3 artifact | complete | v72 environment contract and cross-environment rejection |
| ASC-4 REPL/book | complete | `:type`, examples, fuel invariance, and source diagnostics |

## 15. Cross-cutting diagnostics plan

Several issues currently end at `0:0: failed to compile AST graph`. Fixing only
the successful paths would leave the book and future audits unable to identify
regressions.

- [x] Add a compile diagnostic record with phase, source AST/span, Operation,
  constraint kind, reason code, and relevant expected/actual classifiers.
- [x] Record the first stable contradiction or unsupported boundary, not an
  arbitrary final unresolved dependent.
- [x] Keep diagnostics derived from solver state; they are not Claims or
  artifact authority.
- [x] Give #4 constructor-domain mismatch, #5 IH ownership, #7 grouping, and
  #10 configured-range/principal/ascription failures distinct reason codes.
  A source-level #6 branch-effect mismatch is deliberately absent: branch
  rows form a total least-union operation, so differing branch rows are not a
  contradiction. The accepted solved row is instead checked by replay and by
  an artifact-forgery regression.
- [x] Assert diagnostic codes and spans in negative tests.

## 16. Consolidated progress dashboard

| Package | Issue(s) | Audit | Design | Implementation | Focused tests | Artifact/CI | Documentation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CWF candidate/certificate boundary | #3 | complete | decided | complete | complete | complete | complete |
| Typed IH field identity | #4b/#5/#7 | complete | decided | complete | complete | complete | complete |
| Match motive/effect separation | #6/#7 | complete | decided | complete | complete | complete | complete |
| Nested Match grouping diagnostic | #7 | complete | decided | complete | complete | n/a | complete |
| Executable example manifest | #8 | complete | decided | complete | complete | complete | complete |
| Read-only type inspection | #9 | complete | decided | complete | complete | complete | complete |
| Intrinsic environment and literal principal | #4a/#10 | complete | decided | complete | complete | complete | complete |
| Post-synthesis ascription assertion | #9/#10 | complete | decided | complete | complete | complete | complete |

### 16.1 Issue acceptance traceability

| Issue | Required outcome from the issue | Owning plan/gate |
| ---: | --- | --- |
| #3 | State raw, classifier-coherent, and certified guarantees and the mandatory coverage boundary | CWF-0, CWF-2 |
| #3 | Explain primitive structural substitutions and test uncertified EXTEND behavior | CWF-0, CWF-4 |
| #3 | Provide teachable Context/Substitution/Claim/Certificate inspection | CWF-4 |
| #4 | Direct `#.Int` constructor literal and configured range behavior | HST-0/HST-1, ASC-0/ASC-1 |
| #4 | Recursive constant Host motives, including two recursive fields | HST-2, MRF-0/MRF-1 |
| #4 | Expression evaluator returns 42 with accepted evidence and artifact replay | HST-3 |
| #5 | Ignore, left, right, and both IHs in direct/block forms | MRF-2 |
| #5 | Binary-tree evaluation and exact Match/IH artifact replay | MRF-1/MRF-2 |
| #6 | Direct, helper, monomorphic, and polymorphic map progression | HOF-3 |
| #6 | Runtime cases, Match/IH evidence, artifact replay, and precise restriction diagnostics | HOF-2/HOF-3, diagnostics gate |
| #7 | Define outer-IH lexical/frame rule across nested Match | OIH-0/OIH-1 |
| #7 | Preserve branch-sensitive recursion and compose recursive comparison/insertion | OIH-2 |
| #7 | Three-element insertion sort, trace, and exact frame artifact replay | OIH-1/OIH-2 |
| #8 | Every existing file has a current syntax and explicit executable expectation | EX-0/EX-1 |
| #8 | README command, normal CI runner, runtime/replay checks, and stable negative diagnostics | EX-2/EX-3 |
| #9 | One raw assignment-body classifier, separate ascription status, no normalization | TYP-0/TYP-2/TYP-3 |
| #9 | Shared-Core occurrences remain distinct and inspection mutates nothing | TYP-1/TYP-3 |
| #9 | No object-language reflection or motive reification | TYP-1/TYP-2, non-goals |
| #10 | Unsuffixed literal independently synthesizes configured `#.Int` | ASC-0/ASC-1 |
| #10 | Top-level/inline `::`, contextual APP, ranges, Int32/Int64 policy | ASC-2/ASC-4 |
| #10 | Principal/ascription evidence and environment identity survive artifact replay/link | ASC-3/ASC-4 |
| #10 | `::` never drives synthesis and compile effort never changes principal selection | ASC-1/ASC-2/ASC-4 |

## 17. Completion gates

The issue program is complete only when all of the following are true:

- [x] Every issue has focused fixtures under `src/prototype/tests`.
- [x] Every positive fixture compiles and checks accepted evidence.
- [x] Every negative fixture has a stable source span and reason code.
- [x] Shared-Core tests prove that no fix falls back to Term-ID classifier or
  binder ownership.
- [x] Match tests separate result motive equations from effect-row equations.
- [x] IH proof payloads retain exact typed Match/case/field identity.
- [x] Every unsuffixed numeric literal selects the configured `#.Int` principal
  independently of APP, constructor, Match, Lambda, `::`, and compile effort.
- [x] `::` never changes synthesis input or the body principal classifier.
- [x] Artifact read/link verifies the Intrinsic environment that gives
  `#.Int` its concrete representation.
- [x] `:type` is observably read-only and adds no source reflection.
- [x] Certified CwF consumers reject uncovered EXTEND substitutions.
- [x] Artifact replay and forgery tests cover all newly persistent guarantees.
- [x] All 16 existing type-infer-and-check files are owned by the executable
  manifest and reach their advertised boundary.
- [x] The book-facing README commands and status tables match the runner.
- [x] Existing prototype integration tests remain green.

## 18. Non-goals

This plan does not:

- turn JudgementDB into a TermDB equality union-find;
- make `::` bidirectional elaboration input;
- add object-language type reflection;
- identify an IH field from an erased Core binding;
- make every interned SubstitutionDB candidate globally certified;
- collapse Match result motives and CBPV effects into one DefEq equation;
- implement indexed inductive families merely to make stale Level 3 examples
  parse; or
- preserve obsolete APIs or artifact versions for compatibility.

## 19. Implemented sequence

The implementation began with MRF-0/MRF-1 from Issue #5 because the proven
occurrence-identity defect also blocked #4b and the frame tests in #7. Its
review boundary was:

1. add exact case/field identity to typed IH edges;
2. remove Core binding search from guarded IH validation;
3. update proof materialization/replay payload checks;
4. add left/right/both/nonrecursive/shared-Core tests; and
5. run artifact replay and the full prototype integration suite.

That slice passed before Match effect-row solving changed for #6. Subsequent
packages followed the dependency order in section 6, with CWF/artifact v72
work integrated once rather than through compatibility adapters.

## 20. Completion evidence index

| Area | Authoritative implementation evidence | Focused verification |
| --- | --- | --- |
| CwF boundary | `kernel/context.c`, `kernel/cwf_certificate.c`, `kernel/kernel_view.c`, artifact v72 accepted semantic-action coverage | `checks/hott/universe_scaffold.inc`, `integration/test_hott_goal.sh` |
| Host evaluator and literal policy | explicit Intrinsic environment in Core/frontend/kernel/artifact; one configured literal principal | `integration/test_host_expression_evaluator.sh`, integer sections of `test_artifact_flow.sh` |
| Typed recursive fields | Operation IH edge and proof payload carry owner Match, scope, case, and field | `integration/test_recursive_ih_identity.sh`, including shared erased-Core collision and forgeries |
| Match effects and higher-order map | result-carrier motive construction, occurrence effect constraints, solved-row replay | `integration/test_list_map_induction.sh` |
| Outer IH and algorithms | reader grouping diagnostic, exact frame relocation, branch-sensitive reduction trace | `integration/test_outer_ih_insertion_sort.sh` |
| Executable examples | `examples/type-infer-and-check/manifest.tsv` and current source syntax | `integration/test_type_infer_and_check_examples.sh` |
| Read-only inspection | compile-label projections and const `prototype_compile_metadata_inspect_type` | `checks/type_inspection_projection_check.c`, `integration/test_type_inspection.sh` |
| Artifact contract | `spec/artifact_v72.schema`, `artifact/wire_v72.c`, explicit environment fingerprint and exact proof references | `integration/test_artifact_flow.sh`, `integration/test_hott_goal.sh`, `integration/test_spec_consistency.sh` |

The canonical final verification command is:

```sh
make -f src/prototype/Makefile test-integration
```

Final worktree verification on 2026-08-12 completed with exit status 0. It
ran every `src/prototype/tests/integration/test_*.sh` script, including all
focused issue suites listed above. `git diff --check` and the corresponding
trailing-whitespace scan over newly added files also completed successfully.

Publication is complete only after that command, `git diff --check`, commit,
and push to `main` all succeed.
