# V2-P1 Entry Refactor Audit and Implementation Plan

Date: 2026-08-08

Status: implementation complete; exit audit passed

Planning baseline: `4025532` (`refactor: complete P0 certificate boundary`)

Implementation baseline: `564a2ee` (`refactor: complete P1 HOTT entry substrate`)

Planning artifact boundary: `A_PROGRAM_ARTIFACT 67`

Implemented artifact boundary: `A_PROGRAM_ARTIFACT 68`

## 1. Purpose

V2-P0 is complete. There is no remaining P0 implementation item at the
planning baseline. P0 established the accepted Claim/Derivation certificate
boundary, exact Operation authority, and artifact v67.

The P1 entry audit found that the existing S1 HOTT goal scaffold predates that
boundary and does not yet consume it soundly. This document therefore defines
`V2-P1-R0`, a mandatory substrate repair between P0 and P1:

```text
V2-P0 -> V2-P1-R0 -> V2-P1 -> V2-O1 -> V2-A1
```

P1-R0 is not object equality and does not implement Higher Observational Type
Theory rules. It makes the inputs to those rules typed, replayable, and
artifact-safe.

## 2. Frozen Design Boundaries

The implementation must preserve all of the following:

1. TermDB remains the shared, type-erased computation graph.
2. Value, computation, and type distinctions belong to typed occurrences and
   accepted Claims, not duplicate APP/LAMBDA/MATCH tags.
3. OperationGraph identifies source and generated typed occurrences.
4. One accepted Claim may have zero or more accepted Derivations. No preferred
   Derivation is added.
5. Definitional conversion is a meta-level premise. It is not object equality.
6. `NOT_EQUAL` from the conversion procedure does not establish object-level
   contradiction.
7. ContextDB and SubstitutionDB remain the ordinary categorical substrate.
   P1-R0 may certify their use but must not build a duplicate context graph.
8. TypeDeclarationDB constructor telescopes are the authoritative ADT schema.
   HOTT must not introduce a second constructor schema.
9. TypeView identity remains nominal. Sharing a Core representation does not
   make Bool and Two definitionally or observationally equal.
10. An ordinary value whose carrier is a TypeView, such as `b : Bool`, remains
    admissible to ADT observation. This is distinct from comparing Bool and Two
    as values of a Universe.
11. Solver-local state and compilation budgets are not stable proof evidence.
12. No artifact backward-compatibility path is retained when v68 replaces v67.

## 3. Audit Findings

### P1-R0-F1: HOTT goals are range-checked, not typed

`hott_goal_record_is_valid()` in `src/prototype/typing.c` checks identifier
ranges, enum values, substitution orientation, and backward parent edges. It
does not require accepted Claims establishing:

- that the carrier is formed in the goal Context;
- that the left endpoint has exactly that carrier;
- that the right endpoint has exactly that carrier;
- that the goal kind agrees with the carrier category.

The current `hott_goal_check` can use the Text type term itself as both a Text
carrier and a Text endpoint. Passing this test establishes record shape only.

Required end state: every admitted observation goal names three exact accepted
Claims: carrier formation, left endpoint typing, and right endpoint typing. If
an endpoint classifier is merely convertible to the selected carrier, an
accepted conversion-derived `HAS_TYPE` Claim must be constructed first. A goal
must never borrow a Claim through shared Core identity.

### P1-R0-F2: Conversion outcomes are not bound to the goal

`prototype_hott_goal_apply_conversion_result()` accepts an arbitrary result
record without proving that its endpoints, profile, budget, and graph state
belong to the goal. It also maps `NOT_EQUAL` to `CONTRADICTION`.

Required end state:

- conversion is requested by a goal-owned request record or recomputed by the
  checker;
- the result is accepted only for the exact request tuple;
- `EQUAL` discharges the conversion premise but does not construct an object
  witness;
- `NOT_EQUAL` leaves the observational goal for a type-directed rule or
  residual classification;
- contradiction requires explicit type-directed evidence, for example
  distinct constructors of the same ordinary ADT;
- conversion step counts and graph revisions remain cache/provenance data, not
  witness identity.

### P1-R0-F3: Admission is a syntactic blacklist

`prototype_hott_goal_classify_admission()` rejects every TypeView carrier and
scans only the APP head of endpoints for request, fold, and host primitives.
Consequences include:

- ordinary `Bool` and `List A` value observations are incorrectly rejected;
- a forbidden computation can be hidden under RETURN or THUNK;
- the checker lacks TypeDeclarationDB and accepted Claim authority needed for
  type-directed dispatch.

Required end state: admission is selected from the accepted carrier Claim and
its authoritative type former. Recursive purity/admissibility checks follow
typed structure, not endpoint head tags.

### P1-R0-F4: The bridge is only an oriented pair of substitutions

The goal stores a bridge Context and two substitutions. Validation checks only
that both substitutions have the expected source and target Context IDs. It
does not establish that the bridge is the recursively generated observational
context `Gamma^R`, nor that the substitutions are its projections `pi0` and
`pi1`.

Required end state: a bridge constructor produces replayable bridge evidence
over ContextDB/SubstitutionDB. The evidence records only irreducible
observational construction choices. Ordinary parent, extension, and
substitution structure continues to reside in the existing databases.

### P1-R0-F5: Goal states and child edges are forgeable

The current goal accepts `SOLVED` without a witness Term or accepted typing
Claim. `CONTRADICTION` has no evidence payload. `parent_goal_id` gives no child
role, constructor field ordinal, Match case ordinal, or Pi domain/codomain
position.

Required end state:

- before V2-O1, persisted or accepted `SOLVED` goals are rejected;
- after V2-O1, a solved goal requires a witness Term and its exact accepted
  `HAS_TYPE` Claim;
- contradiction names a stable observational rule and its replayable premises;
- child edges carry a stable rule-local role and index;
- action goals and endpoint-observation goals use separately validated record
  variants rather than fields that are silently ignored by convention.

### P1-R0-F6: Residual records are placeholders

Residual creation drops goal hierarchy, stores a zero 64-bit fingerprint,
copies unbound conversion step counts, and exposes a stale
`require_artifact_v62` API although the accepted artifact is v67.

Required end state: residuals remain compiler-local through P1/O1 unless a
later artifact plan explicitly admits them. Their parent/role relation is
preserved, the calculus identity uses the repository fingerprint format, and
the stale version-specific API is replaced without a compatibility alias.

### P1-R0-F7: Artifact polarity and solver annotations are trusted weakly

The v67 reader range-checks Operation polarity and computation-kind fields but
does not prove they agree with the accepted classifier. Forged artifacts with a
VALUE occurrence changed to COMPUTATION, or with invalid Operation/Context
classifier-variable IDs, are accepted.

Required end state:

- derive value/computation/type category centrally from the accepted Claim and
  solved classifier;
- validate any persisted polarity against that derivation;
- reject UNKNOWN polarity for closed accepted occurrences;
- do not use `computation_kind` or classifier-variable caches as semantic HOTT
  authority;
- remove solver-only fields from the accepted wire format when they can be
  reconstructed deterministically.

This requires artifact v68 before P1. v68 is a substrate-cleanup artifact, not
the object-HOTT artifact planned after O1.

### P1-R0-F8: Substitution identity contains proof choice but validation omits proof

`struct prototype_substitution` stores `term_proof_id`. It participates in
substitution interning and relocation, yet the validator does not validate it
as an accepted Claim. The validator also does not replay the extension's
classifier reindexing check. Forged valid-but-wrong classifiers and invalid
proof IDs survive artifact readback.

Required end state:

- structural substitution identity excludes proof or Derivation choice;
- remove `term_proof_id` rather than rename it ambiguously;
- replay the extension classifier equation from ContextDB and TermDB;
- introduce a separate substitution-extension certificate relation only where
  a trusted boundary needs typed substitution evidence;
- that relation names an exact accepted Claim, not a preferred Derivation;
- compiler-local candidate evidence is rebound to accepted Claim identity only
  after P0 grounded publication;
- persist only substitutions and certificates reachable from accepted
  interfaces or admitted HOTT bridges.

The audit found existing compiler-generated substitutions for which no exact
accepted Claim is retained. Therefore this migration must first determine
certificate reachability. It must not make every transient substitution
artifact-visible merely to satisfy a new field.

### P1-R0-F9: Context shape is not context formation

Context validation checks parent, depth, binding, and identifier shape. It does
not establish that every binding classifier is a type in its parent Context.

Required end state: a Context admitted to a HOTT goal has recursively replayable
formation authority. Closed HOTT goals reject unresolved classifier-variable
only Context entries; unresolved construction becomes a residual obligation.

### P1-R0-F10: P1 lacks exact accepted-Claim queries

P0 stores the required Claim/Derivation model, but P1 has no narrow public API
for exact Claim lookup by kind, authority, Context, Operation, subject, and
classifier, or for enumerating all Derivations concluding one Claim.

Required end state: add authority-specific read-only queries. They must not
select a preferred proof and must not fall back to TermDB subject identity.

### P1-R0-F11: TypeDeclarationDB is usable, but needs a semantic query boundary

Constructor telescopes already use:

```text
parameter_context -> field_context, result_classifier
```

and the curried classifier is derived. This is the correct source for
dependent constructor observation. P1-R0 must not redesign this storage.

Required end state: add a narrow validated query returning the nominal
TypeView declaration and constructor telescope needed by observational rule
selection. Readback metadata and Core shape keys remain non-authoritative.

## 4. Target Data Flow

The P1 entry path must become:

```text
accepted source/interface root
  -> exact accepted carrier-formation Claim
  -> exact accepted left/right HAS_TYPE Claims
  -> validated Context formation
  -> certified endpoint substitutions
  -> constructed observational bridge
  -> rule-directed HOTT goal
  -> bound kernel-conversion premise
  -> pending child goals or compiler-local residual
```

No step may recover authority by scanning for another Claim with the same Core
Term ID.

## 5. Implementation Phases

### P1-R0.0: Characterization and negative fixtures

Status: complete

Files:

- `src/prototype/hott_goal_check.c`
- `src/prototype/test_hott_goal.sh`
- `src/prototype/test_artifact_flow.sh`

Tasks:

- [x] Replace the current ill-typed Text endpoint fixture with typed endpoint
  fixtures and retain the old case as a rejection test.
- [x] Add a shared-Core test proving a sibling occurrence Claim cannot type an
  endpoint.
- [x] Add forged polarity, classifier-variable, substitution classifier, and
  substitution evidence artifact fixtures.
- [x] Record current failures before changing the implementation.

Exit condition: every weakness in Section 3 has an executable failing fixture
or a documented static invariant where serialization is not yet present.

### P1-R0.1: Exact Claim and judgment-category service

Status: complete

Files:

- `src/prototype/judgement.h`
- `src/prototype/typing.c`

Tasks:

- [x] Add exact accepted-Claim lookup keyed by full Claim identity.
- [x] Add enumeration of all accepted Derivations concluding one Claim.
- [x] Add central judgment-category derivation from accepted Claim and
  classifier semantics.
- [x] Prohibit Core-key fallback in all new APIs.
- [x] Add tests with one Claim and multiple Derivations.

Exit condition: a caller can prove exact endpoint typing and category without
selecting a Derivation or scanning by shared Term identity.

### P1-R0.2: Operation and Context wire cleanup, artifact v68

Status: complete

Files:

- `src/prototype/ast.h`
- `src/prototype/ast.c`
- `src/prototype/context.h`
- `src/prototype/context.c`
- `src/prototype/test_artifact_flow.sh`

Tasks:

- [x] Classify every persisted Operation and Context field as structural,
  accepted semantic authority, reconstructible cache, diagnostic, or residual.
- [x] Remove reconstructible solver fields from the accepted artifact.
- [x] Validate retained polarity from exact accepted classifier authority.
- [x] Reject UNKNOWN polarity on closed accepted operations.
- [x] Reject unresolved Context entries at the HOTT admission boundary.
- [x] Emit artifact v68 and reject v67 without fallback.
- [x] Update schema fingerprint and sparse reachability.

Exit condition: mutating a category or solver-local identifier cannot change or
forge accepted meaning during artifact readback.

### P1-R0.3: Structural substitution and certificate split

Status: complete

Files:

- `src/prototype/context.h`
- `src/prototype/context.c`
- `src/prototype/ast.c`
- `src/prototype/judgement.h`
- `src/prototype/typing.c`

Tasks:

- [x] Remove `term_proof_id` from structural substitution identity.
- [x] Remove proof-offset relocation for substitutions.
- [x] Strengthen structural validation to replay extension classifier
  reindexing using TypeDeclarationDB.
- [x] Define a substitution-extension certificate keyed by substitution ID and
  exact accepted Claim ID.
- [x] Determine reachability policy for interface and HOTT bridge
  substitutions.
- [x] Rebind compiler-local candidate evidence only after grounded Claim
  publication.
- [x] Serialize and validate only accepted reachable certificates in v68.

Exit condition: equal substitutions are independent of derivation choice, and
every substitution consumed as typed HOTT evidence has an exact accepted Claim.

### P1-R0.4: Typed HOTT goal grounding

Status: complete

Files:

- `src/prototype/judgement.h`
- `src/prototype/typing.c`
- `src/prototype/hott_goal_check.c`

Tasks:

- [x] Split endpoint-observation and action records into a tagged union or
  separate record types with total validators.
- [x] Add carrier-formation, left endpoint, and right endpoint Claim IDs.
- [x] Validate exact Context, subject/Operation, and classifier agreement.
- [x] Require a conversion-derived exact Claim before changing the selected
  endpoint carrier.
- [x] Reject solved goals until O1 supplies witness Terms and Claims.
- [x] Require explicit evidence for contradiction.

Exit condition: no HOTT goal can be admitted from Term IDs and identifier ranges
alone.

### P1-R0.5: Bound conversion premise

Status: complete

Files:

- `src/prototype/judgement.h`
- `src/prototype/typing.c`
- `src/prototype/term.c`

Tasks:

- [x] Define the exact conversion-request tuple owned by a goal.
- [x] Execute conversion internally or verify an externally supplied result
  against the complete request.
- [x] Constrain normalization profiles by observational rule.
- [x] Keep `EQUAL` as a discharged premise only.
- [x] Change `NOT_EQUAL` from contradiction to pending rule dispatch or
  residual.
- [x] Validate status, reason, and step-count coherence.

Exit condition: conversion evidence cannot be replayed against unrelated goal
endpoints and cannot manufacture an object contradiction.

### P1-R0.6: Observational bridge and child-edge boundary

Status: complete

Files:

- `src/prototype/context.h`
- `src/prototype/context.c`
- `src/prototype/judgement.h`
- `src/prototype/typing.c`

Tasks:

- [x] Implement deterministic bridge construction for the first admitted
  Context fragment.
- [x] Certify `Gamma^R`, `pi0`, and `pi1` using ContextDB, SubstitutionDB, and
  exact Claims.
- [x] Add stable child roles and ordinals for Pi, ADT constructor fields, Match
  cases, and recursive/IH obligations.
- [x] Preserve goal hierarchy in residual conversion.
- [x] Avoid duplicating ordinary context/substitution edges in the HOTT arena.

Exit condition: arbitrary oriented substitutions cannot masquerade as an
observational bridge, and child obligations are replayable without positional
guessing.

### P1-R0.7: Type-directed admission and residual cleanup

Status: complete

Files:

- `src/prototype/type_declaration.h`
- `src/prototype/type_declaration.c`
- `src/prototype/judgement.h`
- `src/prototype/typing.c`

Tasks:

- [x] Add a validated nominal TypeView/constructor telescope query.
- [x] Admit ordinary ADT value carriers while deferring universe-level
  TypeView equality.
- [x] Recursively enforce the pure first fragment through Pi, Comp, RETURN,
  THUNK, FORCE, and admitted ADT structure.
- [x] Prevent THUNK/RETURN wrappers from hiding requests, folds, or host
  primitives.
- [x] Replace coarse local-rule IDs with the stable P1 rule vocabulary:
  `OBS_DIAGONAL`, `OBS_CONVERT`, `OBS_ADT_CONSTRUCTOR`,
  `OBS_ADT_DISTINCT`, `OBS_MATCH_ACTION`, `OBS_COMP_RETURN`,
  `OBS_PI_POINTWISE`, `OBS_THUNK_PURE`, and `OBS_REINDEX`.
- [x] Replace the stale version-specific residual gate.
- [x] Keep residuals compiler-local through P1/O1.

Exit condition: admission follows the carrier's semantic type former and the
normative first-fragment matrix, not a list of endpoint head tags.

### P1-R0.8: Exit audit

Status: complete

Tasks:

- [x] Run every `src/prototype/test_*.sh` script.
- [x] Run examples 01-07 and 09.
- [x] Run `git diff --check`.
- [x] Confirm artifact v68 rejects v67 and all forged fixtures.
- [x] Re-audit P1 payload needs after all reconstructible evidence is removed.
- [x] Update this document and the V2 parent with commit and test evidence.

Exit condition: V2-P1 is unblocked only after every acceptance criterion below
passes.

## 6. Required Test Matrix

| Test | Expected result |
| --- | --- |
| Endpoint Term lacks exact accepted `HAS_TYPE` Claim | reject |
| Sibling occurrence shares Core but owns the Claim | reject |
| Goal kind disagrees with accepted carrier category | reject |
| Ordinary `Bool` value carrier | admit to ADT rule dispatch |
| Bool and Two compared as Universe-level TypeViews | unsupported/residual |
| Request hidden under THUNK or RETURN | unsupported/residual |
| Conversion result names unrelated endpoints | reject |
| Definitional conversion returns `NOT_EQUAL` | not contradiction |
| Goal is marked SOLVED without witness Claim | reject |
| Arbitrary bridge Context and oriented substitutions | reject |
| Substitution classifier is valid ID but semantically wrong | reject |
| Substitution evidence ID is invalid/unaccepted | reject |
| Operation polarity is forged in artifact | reject |
| Context/Operation solver variable is forged | reject or absent from wire |
| One Claim has multiple Derivations | preserve all; select none |
| v67 artifact is read by v68 reader | reject without fallback |

## 7. Acceptance Criteria

P1-R0 is complete only when:

1. all admitted HOTT endpoint goals are grounded in exact accepted Claims;
2. goal category follows accepted classifier authority;
3. conversion results are bound to their request and `NOT_EQUAL` is not object
   contradiction;
4. ordinary ADT values are admitted through TypeDeclarationDB;
5. bridge substitutions are certified, not merely oriented;
6. substitution identity is independent of proof choice;
7. artifact v68 contains no trusted unresolved solver cache;
8. solved and contradiction states require explicit evidence;
9. all negative fixtures and the full prototype regression suite pass;
10. a final P1 audit identifies the genuinely irreducible HOTT payload.

## 8. Explicit Non-Goals

P1-R0 does not add:

- surface `==` syntax;
- equality witness Terms;
- transport or rewrite;
- equality reflection;
- Cubical interval syntax;
- quotient/HIT constructors;
- effectful computation equality;
- higher-order operation semantics;
- general IADT index solving;
- a duplicate Value/Computation TermDB;
- a preferred proof per Claim.

## 9. Progress Log

| Date | Item | Status | Evidence |
| --- | --- | --- | --- |
| 2026-08-08 | P0 completion baseline | complete | `4025532`, artifact v67 |
| 2026-08-08 | P1 entry code audit | complete | Findings P1-R0-F1 through F11 |
| 2026-08-08 | P1-R0 implementation plan | complete | This document |
| 2026-08-08 | Pre-change regression baseline | complete | All 16 `src/prototype/test_*.sh` scripts pass; `test_hott_goal.sh` still characterizes the pre-R0 semantics and is not R0 exit evidence |
| 2026-08-08 | P1-R0.0 characterization | complete | Typed Text endpoints, shared-Core sibling rejection, forged artifact fields, and old ill-typed endpoint rejection are executable fixtures. |
| 2026-08-08 | P1-R0.1 Claim/category service | complete | Exact Claim lookup, complete concluding-Derivation enumeration, centralized category selection, and the multiple-Derivation fixture pass. |
| 2026-08-08 | P1-R0.2 artifact v68 cleanup | complete | Context NF6, Operation NF26, and Substitution NF9 remove solver/proof caches; retained polarity is replayed; v67 is rejected. |
| 2026-08-08 | P1-R0.3 substitution certificate split | complete | Structural proof identity is removed, extension classifiers are replayed, exact-Claim certificates are separate, and link relocation covers substitution terms/classifiers. |
| 2026-08-08 | P1-R0.4 typed goal grounding | complete | Observation/action variants have total validators; observation goals require exact carrier and endpoint Claims; solved goals are rejected. |
| 2026-08-08 | P1-R0.5 conversion premise binding | complete | Goal-owned request tuple, internal execution, full result coherence, and non-contradictory `NOT_EQUAL` behavior are tested. |
| 2026-08-08 | P1-R0.6 bridge/child-edge boundary | complete for the first fragment | The terminal Context has the unique identity bridge; forged oriented bridges are rejected; nonempty relational extension is deferred to O1. |
| 2026-08-08 | P1-R0.7 type-directed admission | complete | Nominal TypeView queries, ordinary ADT dispatch, Universe deferral, recursive first-fragment scan, stable rules, and compiler-local residuals are implemented. |
| 2026-08-08 | P1-R0.8 exit audit | complete | `make`, `make reader`, `-Werror`, all 16 scripts, examples 01-07/09, schema hash, v67 rejection, and `git diff --check` pass. |
| 2026-08-08 | P1-R0 implementation baseline | complete | `564a2ee`, artifact v68, calculus fingerprint `f5f32952ee941bcb393d3f9f09f789d921edbc1b349d116f6cb34bd1c3203ceb` |

## 10. Implemented Boundary Decisions

### 10.1 Reachable substitution certificates

Artifact v68 does not contain a substitution-certificate section. This is not
an omitted reachable payload:

1. accepted interfaces currently root structural Contexts, Substitutions,
   Operations, Claims, and Derivations, but do not root HOTT bridges;
2. HOTT goals, bridges, and residuals remain compiler-local through P1/O1;
3. the only bridge admitted by P1-R0 is the terminal Context identity bridge,
   whose two projections are the same canonical identity substitution and have
   no extension typing Claim;
4. therefore the set of accepted extension certificates reachable from a v68
   publication root is empty.

Structural substitutions are still serialized. Their extension classifier
equations are replayed after Context and Term readback, and link relocation now
rewrites both substitution terms and substitution classifiers. A later
artifact version may add certificates only when O1 introduces a publishable
nonempty relational bridge or another accepted root.

### 10.2 First relational Context fragment

P1-R0 constructs `Gamma^R`, `pi0`, and `pi1` only for the terminal Context. In
that fragment all three are definitionally the existing empty Context and its
unique identity substitution. An arbitrary Context with merely well-oriented
substitutions is rejected.

Nonempty `Gamma^R` is intentionally not guessed. Its extension requires the
object relation field selected by O1, including that field's classifier and
accepted formation Claim. The bridge constructor returns deferred for a
nonempty source Context rather than emitting a provisional certificate.

### 10.3 Irreducible P1 payload after cleanup

The re-audit after removing reconstructible fields leaves these compiler-local
inputs for the next P1 rule design:

- exact accepted carrier and endpoint Claim IDs;
- the stable observational rule and child role/ordinal;
- the constructed bridge ID, once the relevant Context fragment is admitted;
- the exact kernel-conversion request and coherent result provenance;
- residual hierarchy and the 64-hex calculus fingerprint.

These records are not equality witnesses. P1 must decide which rule-local
choices remain irreducible after O1 defines observational actions. It must not
serialize the whole goal scaffold merely because it now validates correctly.

## 11. Immediate Next Step

V2-P1-R0 is complete. The next phase is the V2-P1 irreducible-payload audit,
followed by V2-O1 object-rule implementation. Do not add surface equality,
witness Terms, nonempty relational bridges, or artifact HOTT records before
that audit identifies their exact rule-local authority.
