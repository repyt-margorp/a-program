# V2-P1 Irreducible HOTT Evidence Implementation Plan

Date: 2026-08-08

Status: complete; implementation and exit audit passed

Baseline: commit `29a3190`, artifact v68

Parent plan:
`doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Entry repair and handoff:
`doc/2026-08-08T20-00-00-V2-P1-ENTRY-REFACTOR-AUDIT-PLAN.md`

Normative calculus:
`doc/2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`

## 1. Objective

V2-P1 must select the irreducible evidence retained by a Higher Observational
Type Theory rule application without turning compiler search state into object
equality or duplicating data already owned by TermDB, OperationGraph,
ContextDB, TypeDeclarationDB, SubstitutionDB, or JudgementDB.

The implementation must replace the current catch-all HOTT goal record with a
separation between:

1. an immutable observation-family proposition;
2. one or more candidate rule applications for that proposition;
3. mutable compiler work state and residual diagnostics;
4. exact references to existing accepted authorities; and
5. future object-level output Claims and witness Claims, which V2-O1 will
   construct through ordinary `IS_TYPE` and `HAS_TYPE` judgements.

P1 is complete when O1 can implement observational action without changing the
identity of goals, derivations, premise edges, rule numbers, or compiler-local
residual records.

## 2. Semantic Correction Required Before Payload Selection

The current goal state conflates formation of an observation family with
inhabitation of that family.

For an ordinary ADT and distinct constructors, the intended result is:

```text
Gamma |- ObsV(A, constructor_i(...), constructor_j(...)) type
```

where the generated observation family is uninhabited when `i != j`. Formation
of that family succeeds. Failure to construct a witness of it is a different
judgement.

Current code instead selects `OBS_ADT_DISTINCT` and writes
`PROTOTYPE_HOTT_GOAL_CONTRADICTION` in
`src/prototype/typing.c:1838-1844`. This makes family formation itself a
contradiction. P1 must remove that interpretation before adding accepted HOTT
evidence.

The normative boundary is:

```text
observation-family formation:
  exact carrier and endpoint Claims
  ---------------------------------
  Gamma |- ObsV(A, a0, a1) type

observation witness checking:
  Gamma |- p : ObsV(A, a0, a1)
```

The second judgement is an ordinary future `HAS_TYPE` Claim. Definitional
conversion remains a meta-level premise and never becomes this witness by
itself.

## 3. Current Code Audit

### P1-F1: One mutable record owns five different concepts

`struct prototype_hott_goal` in `src/prototype/judgement.h:1117` currently
contains:

- immutable proposition inputs;
- mutable `state` and `residual_reason`;
- one selected `rule`;
- parent/child adjacency;
- future `witness_term` and `witness_claim_id`;
- a conversion request, result, and graph revision; and
- diagnostic `source_ast` provenance.

This repeats the pre-P0 error that was removed from ordinary typing: a
proposition must not contain the selected derivation or mutable solver state.

### P1-F2: One goal cannot retain multiple valid derivations

`prototype_hott_goal_classify_admission()` overwrites `goal->rule` in
`src/prototype/typing.c:1781-1887`. A diagonal endpoint at an admitted ADT or Pi
can have both a diagonal derivation and a structure-directed derivation. The
current ordering chooses one by mutation and loses the other.

P1 must follow the accepted Claim/Derivation split established by P0: one
observation-family goal may have multiple rule candidates and, after O1, one
output Claim may have multiple accepted derivations.

### P1-F3: Parent identity is stored on the child

`parent_goal_id`, `parent_role`, and `parent_index` in
`src/prototype/judgement.h:1124-1126` make a subgoal belong to one parent. The
same subgoal may be shared by several rule applications. Parent-to-child edges
belong to a derivation candidate, not to immutable goal identity.

The current validator also requires parents to have lower numerical IDs in
`src/prototype/typing.c:1405-1410`. That insertion-order restriction is not a
semantic well-foundedness proof.

### P1-F4: Observation identity duplicates exact Claim data

`prototype_hott_observation_goal` stores Context, carrier, endpoints, Claims,
and endpoint Operations in `src/prototype/judgement.h:1098-1109`. Exact accepted
Claims already determine:

- Context identity;
- TermDB subject;
- classifier;
- Operation identity when the Claim is Operation-owned; and
- value/computation category through
  `prototype_judgement_claim_category()`.

P1 must retain exact Claim IDs and reject inconsistent combinations. It must
not copy fields that can be replayed from those Claims.

### P1-F5: Every observation owns an unrelated generic conversion request

The validator requires every observation goal to carry one endpoint conversion
request in `src/prototype/typing.c:1556-1566`. This is not the premise schema of
every observational rule:

- `OBS-CONVERT` needs explicit conversion premises;
- `OBS-COMP-RETURN` needs normalization of each computation to `Return`;
- `OBS-PI-POINTWISE` needs related-input and codomain child observations;
- `OBS-THUNK-PURE` needs an underlying computation observation; and
- ADT rules need constructor-telescope children.

Conversion requests and results therefore belong to the derivation which uses
them. They are not part of every goal.

### P1-F6: Action contracts are incomplete

The current action variant stores only Context, subject, and subject Claim in
`src/prototype/judgement.h:1111-1115`. It has no generated output and only
distinguishes type and term action. The normative calculus requires four
actions:

1. Context action: `Gamma -> GammaR` with projections `pi0` and `pi1`;
2. substitution action: `sigma -> sigmaR` between source and target bridges;
3. type action: an accepted `IS_TYPE` Claim to a generated relation-family
   `IS_TYPE` Claim; and
4. term action: an accepted `HAS_TYPE` Claim plus its type action to a generated
   relation-witness `HAS_TYPE` Claim.

P1 freezes these contracts. O1 constructs their object terms and accepted
Claims.

### P1-F7: Context formation uses an unstable existential scan

`hott_context_is_formed()` scans every accepted Claim and accepts the first
matching `IS_TYPE` Claim in `src/prototype/typing.c:1196-1238`. A non-empty
Context extension needs an exact accepted formation authority for its binding
classifier. An existential scan is insufficient for deterministic replay and
artifact publication.

P1 must add a compiler-local Context-formation certificate:

```text
(context_extension_id, exact_classifier_is_type_claim_id)
```

The certificate is separate from structural Context identity. It follows the
same ownership rule as `prototype_substitution_certificate` at
`src/prototype/judgement.h:324-357`.

### P1-F8: Child roles do not describe the frozen rules

The roles at `src/prototype/judgement.h:1047-1054` cover Pi, ADT fields, Match
cases, and IH only. They cannot distinguish:

- the underlying result observation of `Comp(empty,A)`;
- the underlying computation observation of `ThunkType(C)`;
- Pi related-domain input from Pi codomain observation;
- conversion anchoring;
- Context/substitution/type/term action dependencies; or
- reindex/naturality dependencies.

P1 must freeze explicit numerical role IDs for every first-fragment rule. A
role and ordinal describe an edge; they do not become a TermDB tag.

### P1-F9: Admission classification is not the frozen first fragment

The current classifier dispatch in `src/prototype/typing.c:1818-1888` has four
specific defects:

1. Pi is accepted by outer tag without recursively establishing a pure
   computation codomain.
2. Any non-distinct TypeView endpoint pair is labelled
   `OBS_ADT_CONSTRUCTOR`, including neutral endpoints which require generated
   Match-family action rather than same-constructor field comparison.
3. Diagonal is selected only after TypeView, Pi, Comp, and Thunk dispatch, so it
   cannot coexist as another derivation.
4. Unsupported primitive observations can remain indefinitely under the
   fallback `OBS_CONVERT` rule.

P1 must make admission generate zero or more typed rule candidates and a
separate residual reason. It must not mutate the proposition.

### P1-F10: The calculus fingerprint does not identify HOTT semantics

`PROTOTYPE_CALCULUS_FINGERPRINT` in `src/prototype/calculus.h:7` currently uses
the artifact v68 manifest digest. `artifact_v68.schema` lists no HOTT rule,
role, admission, or residual vocabulary. Changing HOTT semantics therefore
does not necessarily change the fingerprint copied into residual records.

P1 must add a checked-in first-fragment HOTT manifest and a distinct HOTT
calculus fingerprint. Artifact wire identity and HOTT calculus identity are
different authorities. A1 may later serialize the latter.

### P1-F11: Residual records duplicate goals and contain a dead parent field

`prototype_hott_residual_obligation` at `src/prototype/judgement.h:1205-1224`
copies Context, carrier, endpoints, bridge, parent information, rule, profile,
and source AST. `parent_obligation_id` is always initialized to INVALID in
`src/prototype/typing.c:2047`.

P1 must reduce the residual to compiler policy and diagnostics over an immutable
goal/work item. It must not become a second observation proposition.

### P1-F12: HOTT remains an isolated test scaffold

Outside `judgement.h` and `typing.c`, current HOTT construction is used only by
`src/prototype/hott_goal_check.c`. The AST/compiler pipeline does not generate
object observation terms, and artifact v68 serializes no HOTT goal, bridge,
action, residual, or witness record.

This is an intentional boundary for P1. The phase must not claim user-visible
equality support.

## 4. Selected Ownership Model

### 4.1 Immutable observation-family goal

The compiler-local proposition identity will contain only:

```text
goal_id
observation_category       VALUE or COMPUTATION
carrier_is_type_claim_id
left_has_type_claim_id
right_has_type_claim_id
bridge_id
```

Validation derives and checks Context, carrier, endpoints, Operations, and
categories from the exact Claims. The goal is interned by this identity.

No rule, state, source AST, parent edge, witness, conversion request, or
normalization result is part of this record.

### 4.2 Rule-application candidate

One goal may own zero or more compiler-local candidates:

```text
candidate_id
conclusion_goal_id
rule_id
claim_premise_slice
child_goal_edge_slice
conversion_premise_slice
context_certificate_premise_slice
substitution_certificate_premise_slice
```

There will be no catch-all tagged payload array. Each premise class has a typed
edge arena and validator. This prevents unrelated integers from acquiring
rule-dependent meanings.

### 4.3 Child goal edge

```text
candidate_id
child_goal_id
role_id
ordinal
```

Edges are owned by candidates. Child goals may be shared. Validation checks the
role schema for the owning rule and detects cycles independently of numerical
insertion order.

### 4.4 Compiler work state

Mutable search state is separate:

```text
work_item_id
goal_id
state                 PENDING, READY, RESIDUAL, UNSUPPORTED
selected_candidate_or_invalid
residual_reason
source_ast
normalization_profile
step_limit
steps_used
term_graph_revision
hott_calculus_fingerprint
```

`READY` means that a checked rule candidate is available for O1 construction.
It does not mean that an object witness exists. `CONTRADICTION` is removed from
family-formation work state.

### 4.5 Future accepted certificate boundary

After O1 constructs object terms, accepted HOTT formation evidence contains
only:

```text
stable HOTT rule id
exact conclusion IS_TYPE Claim id
ordered exact premise Claim ids
exact bridge/action certificate references required by the rule
```

An equality witness is independently represented by an ordinary exact
`HAS_TYPE` Claim whose classifier is the generated observation type.

Candidate IDs, goal IDs, work states, source AST IDs, budgets, graph revisions,
and residual reasons are never accepted object evidence.

## 5. Rule-by-Rule Payload Audit

| Rule | Reconstructible authority | Irreducible candidate choice | O1 output |
| --- | --- | --- | --- |
| `OBS_DIAGONAL` | carrier/endpoint Claims and bridge | rule ID; no generic conversion premise | observation-family `IS_TYPE` Claim and diagonal constructor derivation when a witness is requested |
| `OBS_CONVERT` | endpoint Claims and kernel conversion service | deterministic anchor orientation plus exact successful conversion premise references | derivation reducing to an already generated observation family |
| `OBS_ADT_CONSTRUCTOR` | TypeView declaration and constructor telescope | selected constructor ordinal; ordered field child edges | iterated dependent field observation family |
| `OBS_ADT_DISTINCT` | TypeView declaration and distinct ordinals | two constructor ordinals; no contradiction state | uninhabited observation family |
| `OBS_MATCH_ACTION` | Match cases, motive, frame/IH metadata | scrutinee, motive, case, and IH child roles/ordinals | generated Match-family action |
| `OBS_COMP_RETURN` | `Comp(empty,A)` carrier and pure normalizer | two successful Return-exposure premises plus one result child observation | `ObsV(A,a0,a1)` family |
| `OBS_PI_POINTWISE` | Pi domain/codomain family and bridge | related-input and codomain child edge schema | pointwise computation observation family |
| `OBS_THUNK_PURE` | Thunk classifier and pure computation carrier | one underlying computation observation child | corresponding `ObsC` family |
| `OBS_REINDEX` | Context/SubstitutionDB and exact certificates | source action, substitution action, and naturality child references | reindexed observation-family derivation |

The constructor ordinal is retained only when it is not uniquely recoverable
from the exact endpoint head and rule conclusion. The implementation audit must
prefer replay over storage and delete any redundant ordinal after validators can
derive it.

## 6. Frozen Child Roles

P1 must assign explicit numerical values in the HOTT fragment manifest. At
minimum, the first fragment needs roles equivalent to:

```text
ADT_FIELD
ADT_DEPENDENT_REINDEX
MATCH_SCRUTINEE
MATCH_MOTIVE_ACTION
MATCH_CASE_ACTION
MATCH_RECURSIVE_IH
COMP_LEFT_RETURN_EXPOSURE
COMP_RIGHT_RETURN_EXPOSURE
COMP_RESULT_OBSERVATION
PI_DOMAIN_ACTION
PI_RELATED_INPUT
PI_CODOMAIN_OBSERVATION
THUNK_COMPUTATION_OBSERVATION
CONVERT_ANCHOR_OBSERVATION
CONTEXT_ACTION
SUBSTITUTION_ACTION
TYPE_ACTION
TERM_ACTION
REINDEX_NATURALITY
```

The final names may be shortened during implementation, but their semantic
distinctions must not be collapsed merely to reduce enum size.

## 7. Action Contracts Frozen by P1

### 7.1 Context action

Input:

- source Context ID;
- exact Context-formation certificates for every non-empty extension.

Output contract for O1:

- relational Context ID;
- left projection Substitution ID and certificate;
- right projection Substitution ID and certificate.

### 7.2 Substitution action

Input:

- source Substitution ID;
- source and target bridge IDs;
- exact extension certificates where applicable.

Output contract for O1:

- relational Substitution ID;
- exact certificate establishing each generated extension.

### 7.3 Type action

Input:

- exact source `IS_TYPE` Claim ID;
- bridge ID.

Output contract for O1:

- generated relation-family Term ID;
- exact generated `IS_TYPE` Claim ID.

### 7.4 Term action

Input:

- exact source `HAS_TYPE` Claim ID;
- exact type-action output Claim ID;
- bridge ID.

Output contract for O1:

- generated relation-witness Term ID;
- exact generated `HAS_TYPE` Claim ID.

P1 implements validators and compiler-local request/result boundaries. O1
implements the recursive graph action and creates outputs.

## 8. Implementation Phases

### P1.0: Characterization fixtures

Status: complete

- [x] Preserve the P1-R0 typed Claim and bridge adversarial tests.
- [x] Add a fixture showing one diagonal ADT goal has at least two candidate
      rules without overwriting either.
- [x] Add a distinct-constructor fixture which forms an uninhabited observation
      family and is not classified as compiler contradiction.
- [x] Add a neutral ADT endpoint fixture which is not accepted as
      same-constructor field observation.
- [x] Add a Pi fixture with an effectful codomain and require residualization.
- [x] Add an unsupported primitive observation fixture which cannot remain an
      unqualified `OBS_CONVERT` candidate.
- [x] Add a shared-subgoal fixture with two parent derivation candidates.

Primary file: `src/prototype/hott_goal_check.c`.

### P1.1: Split goal identity, candidate, work state, and residual

Status: complete

- [x] Replace the catch-all `prototype_hott_goal` with immutable
      observation-family identity.
- [x] Intern exact goal identities and validate Claim-derived fields.
- [x] Add candidate, child-edge, typed-premise, and work-item arenas.
- [x] Move parent roles from goals to candidate-owned edges.
- [x] Move conversion requests/results from goals to conversion premise edges.
- [x] Move `source_ast`, budget, revision, and residual reason to work items.
- [x] Remove `witness_term` and `witness_claim_id` from compiler goal storage.
- [x] Remove family-level `CONTRADICTION` state.
- [x] Remove dead `parent_obligation_id` and duplicate residual fields.

Primary files: `src/prototype/judgement.h`, `src/prototype/typing.c`, and
`src/prototype/hott_goal_check.c`.

### P1.2: Add exact Context-formation authority

Status: complete

- [x] Add `prototype_context_formation_certificate` and its DB.
- [x] Validate each certificate against one non-empty Context extension and one
      exact accepted classifier `IS_TYPE` Claim in the parent Context.
- [x] Replace `hott_context_is_formed()` full-DB scans with exact certificate
      lookup.
- [x] Keep terminal Context formation structural and certificate-free.
- [x] Feed Context and substitution certificates into bridge/action contracts.
- [x] Add forged, missing, duplicate, and wrong-parent certificate tests.

This phase does not place proof IDs in structural Context identity.

### P1.3: Freeze HOTT manifest and numerical vocabulary

Status: complete

- [x] Add a checked-in HOTT first-fragment semantic manifest.
- [x] Give every rule, role, action kind, work state, and residual reason an
      explicit numerical value.
- [x] Generate or verify a dedicated 64-hex HOTT calculus fingerprint.
- [x] Stop using the artifact v68 digest as HOTT calculus identity.
- [x] Add a schema-hash test which fails after semantic vocabulary changes
      without a manifest update.

No artifact version bump occurs in this phase.

### P1.4: Generate rule candidates without mutating goals

Status: complete

- [x] Replace `prototype_hott_goal_classify_admission()` with candidate
      generation over immutable goals.
- [x] Emit all applicable candidates in deterministic rule order.
- [x] Validate each rule's allowed child roles and premise counts.
- [x] Detect candidate dependency cycles structurally, not by ID ordering.
- [x] Keep successful conversion, normalization, and structural premises on the
      candidate that consumes them.
- [x] Separate no-candidate residual classification from malformed-input error.

### P1.5: Repair the first-fragment admission matrix

Status: complete

- [x] Recursively validate Pi domains and pure computation codomains.
- [x] Distinguish same-constructor, distinct-constructor, and neutral ADT heads.
- [x] Generate diagonal independently of structure-directed candidates.
- [x] Validate `Comp(empty,A)` through two Return exposures and one result child.
- [x] Validate Thunk through one admitted underlying computation child.
- [x] Residualize effectful and unresolved rows using the existing purity
      trichotomy.
- [x] Reject or defer host primitives, TypeView nominal equality, Universe,
      operation request, computation fold, HIT, and IADT cases exactly as the
      normative matrix specifies.

### P1.6: Freeze action request/result validators

Status: complete

- [x] Replace the two-field action variant with the four action contracts in
      Section 7.
- [x] Implement terminal Context action as the first valid result.
- [x] Validate non-empty Context action inputs but return deferred until O1 can
      construct the relation field.
- [x] Validate source/target orientation for substitution action.
- [x] Require exact source Claims for type and term action.
- [x] Ensure action outputs cannot be fabricated without accepted Claims.

### P1.7: Select the future accepted payload

Status: complete

- [x] Define the accepted HOTT derivation contract without serializing it in
      artifact v68.
- [x] Prove by validator replay that copied Context, Term, Operation, classifier,
      and source AST fields are unnecessary.
- [x] Retain stable rule ID, exact conclusion/premise Claims, ordered rule edges,
      and only irreducible bridge/certificate references.
- [x] Document which compiler candidate fields disappear when O1 commits an
      accepted derivation.
- [x] Reject any proposal to store a universal untyped payload array.

### P1.8: Exit audit

Status: complete

- [x] Run `make` and `make reader`.
- [x] Compile the prototype with the repository warning policy including
      `-Werror`.
- [x] Run every `src/prototype/test_*.sh` script.
- [x] Run examples 01-07 and 09.
- [x] Run HOTT shared-subgoal, multiple-derivation, formation/witness,
      conversion-provenance, Context-certificate, purity, and residual tests.
- [x] Verify artifact v68 output remains byte-stable where publication roots are
      unchanged.
- [x] Verify no HOTT compiler-local record is emitted into v68 artifacts.
- [x] Run `git diff --check`.

## 9. Implementation Result

P1 is implemented in `src/prototype/hott.c`. The old mutable HOTT scaffold was
removed from `src/prototype/typing.c`; no compatibility alias or duplicate
implementation remains. This is a module-ownership split only. Value and
computation observations still refer to exact typed occurrences over the one
shared TermDB and do not introduce polarity-specific APP, Lambda, Pi, or Match
nodes.

The checked implementation has these boundaries:

- an interned observation goal contains only category, three exact Claim IDs,
  and one bridge ID;
- candidates own typed premise slices and parent-to-child edges;
- work items alone own source location, budget, graph revision, selected search
  candidate, residual state, and the dedicated HOTT fingerprint;
- successful conversion is candidate-local evidence, while failed conversion
  creates neither inequality nor contradiction;
- distinct ADT constructors form an admitted uninhabited family candidate;
- neutral ADT heads select Match action instead of constructor equality;
- pure `Comp` and `Thunk` rules create exact child goals because accepted
  `RETURN_INTRO` and `THUNK_INTRO` derivations expose their typed occurrences;
  and
- Pi, constructor-telescope, Match, reindex, and non-terminal Context action
  retain frozen typed role schemas, but their recursive object terms and output
  Claims are constructed only by O1.

The future accepted derivation payload selected by P1 is a stable rule ID,
exact conclusion and premise Claim IDs, ordered typed child/action edges, and
only the bridge or certificate references required by that rule. Candidate ID,
goal ID, work state, source AST, normalization budget, graph revision,
residual reason, and selected-candidate preference are compiler state and must
not be serialized as accepted evidence. Artifact v68 therefore remains
unchanged.

## 10. Artifact Boundary

P1 is compiler-local and does not bump artifact v68.

Artifact publication remains unchanged because:

- no object observation term is constructed yet;
- non-empty relational Context bridges are not publishable yet;
- HOTT candidates and work items are solver state;
- residual obligations remain explicitly forbidden by
  `prototype_hott_residual_db_require_artifact_empty()`; and
- accepted HOTT derivations do not exist until O1 creates exact output Claims.

V2-A1 will select a later artifact schema only after O1 demonstrates which
object Terms, Claims, Derivations, Context bridges, and certificates are
reachable from publication roots.

## 11. Non-Goals

P1 does not add:

- surface `==` syntax;
- a `REFL` TermDB tag;
- object equality witnesses;
- Universe equality or univalence;
- quotient, HIT, or IADT observation rules;
- effect-handler observation;
- host primitive extensional equality;
- a value-side Pi/Lambda/App/Match graph;
- DefEq union-find entries from object proofs;
- HOTT artifact records; or
- compatibility aliases for the replaced compiler-local HOTT structures.

No backward compatibility is retained for the old HOTT goal scaffold.

## 12. Acceptance Criteria

P1 is complete only when all of the following hold:

1. observation-family identity is immutable and interned;
2. family formation and witness inhabitation are separate;
3. one goal may retain multiple derivation candidates;
4. child adjacency belongs to derivations, not child goals;
5. exact Claims are the authority for carrier and endpoints;
6. Context formation and substitution extension use exact certificates;
7. conversion and normalization premises are rule-local;
8. the first-fragment admission matrix matches the normative calculus;
9. HOTT semantic vocabulary has its own checked fingerprint;
10. residuals contain policy/diagnostic state without duplicating propositions;
11. no new TermDB polarity-specific graph tags are introduced;
12. artifact v68 remains free of compiler-local HOTT state; and
13. the full regression and adversarial test matrix passes.

## 13. Implementation Order and Stop Conditions

The required order is:

```text
P1.0 characterization
  -> P1.1 ownership split
  -> P1.2 exact Context authority
  -> P1.3 stable manifest
  -> P1.4 candidate generation
  -> P1.5 admission repair
  -> P1.6 action contracts
  -> P1.7 accepted-payload audit
  -> P1.8 exit audit
  -> V2-O1
```

Stop and revise this plan if any phase requires:

- using TermDB node identity as typed occurrence identity;
- storing a preferred proof in Context or Substitution structural identity;
- treating failed conversion as object inequality;
- treating a solver residual as an object witness;
- introducing separate value/computation graph constructors; or
- serializing HOTT work state before O1 supplies accepted object outputs.

## 14. Progress Ledger

| Date | Item | Status | Evidence |
| --- | --- | --- | --- |
| 2026-08-08 | Current-code audit | complete | Findings P1-F1 through P1-F12 |
| 2026-08-08 | P1 ownership and payload selection | complete as design | Sections 4 through 7 |
| 2026-08-08 | Detailed implementation plan | complete | Sections 8 through 12 |
| 2026-08-08 | P1.0 characterization | complete | adversarial matrix in `hott_goal_check.c` |
| 2026-08-08 | P1.1 ownership split | complete | immutable goals, typed candidates, work items, minimal residuals |
| 2026-08-08 | P1.2 Context formation certificates | complete | exact accepted `IS_TYPE` authority and forged-certificate rejection |
| 2026-08-08 | P1.3 HOTT manifest | complete | `hott_fragment_v1.schema` and checked independent SHA-256 fingerprint |
| 2026-08-08 | P1.4 candidate generation | complete | deterministic alternatives, typed premises, shared children, cycle rejection |
| 2026-08-08 | P1.5 admission repair | complete | diagonal, ADT head trichotomy, Pi purity, Comp/Thunk, unsupported residual tests |
| 2026-08-08 | P1.6 action contracts | complete | Context, substitution, type, and term action validators |
| 2026-08-08 | P1.7 accepted-payload audit | complete | Section 9 freezes replay-minimal future payload |
| 2026-08-08 | P1.8 exit audit | complete | strict builds, analyzer, all 16 scripts, examples 01-07/09, artifact v68, diff check |

## 15. Immediate Next Step

Begin V2-O1 by constructing the terminal and first non-terminal object-level
observation-family terms and exact output Claims through the frozen action
contracts. O1 must consume P1 candidates without adding rule choice, work
state, or residual diagnostics to accepted proof identity.
