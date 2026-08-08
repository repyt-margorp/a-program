# Context, Substitution, and Judgement Graph-Consing V3-G1 Plan

Date: 2026-08-08

Status: active; G1.0 and G1.1 implemented locally, mandatory before V2-O1

Parent plan:

`doc/2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`

Baseline:

- Git commit: `942bed77` (`prototype: freeze irreducible HOTT evidence`)
- Artifact version at baseline: v68
- Current local artifact version: v69
- Prototype test scripts at baseline: 16

## 1. Objective

V3-G1 gives Context, Substitution, their action on typed syntax, and Judgement
evidence one consistent graph discipline before object-level higher
observational equality is added.

The migration has two equally important parts:

1. typed graph-consing: an immutable semantic object has one explicit key and
   one construction path;
2. semantic authority consolidation: when several layers describe one
   operation, the dependent layers point to its authoritative graph node
   instead of independently reconstructing the same relation.

The second part is required even if allocation performance is acceptable. The
current Context weakening implementation demonstrates the problem:

```text
SubstitutionDB:
  p : Gamma.A -> Gamma             PROJECTION

JudgementDB:
  Gamma |- t : T
  -------------------------------  CONTEXT_WEAKEN
  Gamma.A |- t : T
```

The proof rule and the morphism are different mathematical objects. They must
not be merged into one tag. Nevertheless, the weakening proof is justified by
reindexing along `p`, so it must retain an edge to `p` or to one canonical
action node which owns `p`.

At the baseline, `prototype_judgement_delta_record_context_weaken()` receives
only source evidence. `validate_context_weaken_proof()` does not receive
`SubstitutionDB`; it independently walks the Context parent chain. This creates
two authorities for the same weakening path.

## 2. Non-goals

V3-G1 does not:

- put Context, Substitution, Judgement, or proofs into TermDB;
- identify a typed Context by erased TermDB alpha shape;
- introduce De Bruijn indices or lexical depth as binding identity;
- add object equality, paths, observations, or HOTT witnesses;
- split APP, Lambda, Pi, or Match into Value and Computation graph tags;
- infer general extensional equality of substitutions;
- make a hash collision evidence of equality;
- add linear or graded typing;
- treat structural projection as permission to discard a linear resource;
- preserve the old representation through a compatibility fallback; or
- promote prototype code into accepted `src/` or `include/`.

## 3. Mathematical boundary

The implementation distinguishes five sorts:

```text
Context Gamma                    category object
Substitution sigma : Delta -> Gamma
                                 category morphism
Action(sigma, object)            substitution/reindex action
Judgement proposition            statement in a Context
Derivation                       certificate edge graph
```

They are related, not identical.

### 3.1 One semantic operation, multiple typed views

Every cross-layer feature is classified as:

1. **structure**: the immutable operation itself;
2. **action**: application of the structure to another graph object;
3. **certificate**: evidence that the action preserves a judgement;
4. **search state**: mutable candidates and residual constraints; or
5. **diagnostic projection**: data reproducible from the authority.

Only one item may be authoritative for a semantic operation. Other persistent
items use direct IDs to that authority.

For Context weakening:

```text
structure:    projection/composed projection Substitution ID
action:       reindex source proposition into target Context
certificate:  CONTEXT_WEAKEN Derivation
search state: solver candidate requesting the weakened evidence
```

The Derivation is not removed, because a morphism alone does not prove a term
has a type. The duplicated parent-chain proof is removed.

### 3.2 Graph identity is not definitional equality

Returning one ID for one exact typed key is an implementation identity. It does
not establish object-level equality or general judgemental equality.

Examples:

- two Context extensions with different binding IDs remain distinct;
- two substitutions with extensionally equal action may remain distinct unless
  constructor normalization gives them the same exact key;
- two convertible classifier Term IDs do not automatically give the same
  Context key;
- one proposition may have multiple Derivations; and
- resource usage is not erased by sharing one TermDB node.

## 4. Baseline code audit

### 4.1 ContextDB

`src/prototype/context.h` defines an immutable extension record:

```c
struct prototype_context {
        uint32_t parent;
        uint32_t binding_id;
        uint32_t classifier;
        uint32_t classifier_variable;
        uint32_t depth;
};
```

Strengths:

- entry zero is the empty Context;
- every non-empty Context points to its parent;
- binding identity is the opaque `binding_id` used by TermDB variables;
- repeated exact extensions are already reused.

Debt:

- interning uses a complete linear scan;
- solved and unresolved classifier identity is split across two fields;
- `depth` is cached structural data and must not become identity;
- artifact append rebuilds Context records by relocation order.

### 4.2 SubstitutionDB

`src/prototype/context.h` defines:

```text
IDENTITY
EMPTY
PROJECTION
EXTEND
COMPOSE
```

Each node carries source and target Context IDs. This is already the correct
shape of a typed morphism DAG.

Debt:

- insertion uses a complete linear scan;
- constructor normalization is not expressed as a single key API;
- action results are not globally memoized;
- clients recreate projections while validating dependent rules;
- artifact append reconstructs nodes instead of importing typed keys.

### 4.3 Reindex action

`prototype_term_reindex()` in `src/prototype/context.c`:

- returns the original Term ID for identity;
- builds a dense binding-replacement array by walking the target Context;
- resolves substitution images recursively;
- invokes the scope-aware TermDB reindex traversal; and
- relies on TermDB constructors to share unchanged paths.

This is not a whole-graph copy. The remaining problem is that every call
rebuilds the dense preparation and repeated composed lookups.

`prototype_context_fresh_reindex_extension()` additionally allocates fresh
bindings and a fresh Context telescope for a dependent Context action. Calls
remain in `context_category_check.c` and `ast.c`. Repeating the same pullback
request therefore need not return one graph result.

### 4.4 JudgementDB

The accepted database separates:

```text
Claim       proposition identity
Derivation  rule application
```

This separation is retained. Multiple Derivations may establish one Claim.

Debt:

- solver claim candidates and accepted Claims repeat proposition tuples;
- accepted local premises repeat kind, Context, subject, and classifier in
  parallel fixed arrays;
- proof-specific semantic operation IDs have no common edge representation;
- several validators recreate Context/Substitution actions rather than follow
  an edge stored by the Derivation.

### 4.5 Existing certificate precedent

The repository already contains the correct conceptual pattern in two limited
forms:

- `prototype_substitution_certificate` pairs an `EXTEND` substitution with the
  accepted Claim typing its extension term;
- HOTT context-formation certificates pair a Context extension with the
  `IS_TYPE` Claim for its classifier.

These records show that structure and proof should remain separate while being
connected by exact IDs. G1 generalizes that discipline; it does not flatten
these records into ContextDB or TermDB.

## 5. Semantic authority audit

G1.0 must complete this audit before changing storage. Every proof kind and
every persistent helper database is assigned one of:

- `independent rule`;
- `certificate of structural operation`;
- `certificate of action result`;
- `solver-only constraint`;
- `derived diagnostic`; or
- `duplicate authority to remove`.

### 5.1 Confirmed findings

| Feature | Current mechanisms | Finding | Required direction |
| --- | --- | --- | --- |
| Context weakening | `PROJECTION`/`COMPOSE`; `CONTEXT_WEAKEN`; ancestor walk | confirmed duplicate authority | Derivation references exact substitution/action; validator follows it |
| Pi codomain under a binder | projection created by typing helper; projection recreated by Lambda validator | same action reconstructed | canonical action ID is created once or deterministically interned and retained by rule data |
| Operation continuation family | extended Context and projection recreated by request validator | same action reconstructed | retain exact continuation Context/action edge generated by elaboration |
| Typed substitution extension | `EXTEND`; separate substitution certificate | legitimate structure/certificate pair | preserve and migrate to common edge conventions |
| Context formation | Context extension; separate HOTT formation certificate | legitimate structure/certificate pair | preserve and migrate to common edge conventions |

### 5.2 Mandatory audit before implementation

The following are suspected overlaps, not predetermined collapses:

| Candidate | Question to answer | Decision criterion |
| --- | --- | --- |
| `CONVERSION` versus kernel conversion | is the exact conversion request/profile represented, or does validation recompute it under ambient defaults? | persistent conversion evidence must identify a stable request/result or be deliberately replayable under a frozen profile |
| `EXPECTED_TYPE_EXPOSURE` versus `CONVERSION` | are these genuinely different actions because exposure solves universe/effect variables? | retain separate proof kinds only if action identity and admissibility differ |
| `EFFECT_WEAKEN` versus effect-row inclusion constraints | is inclusion a solver result which must be cited, or a closed deterministic check? | do not keep both as independent persistent authorities |
| binder assumption versus Context binding | does the proof derive solely from exact Context membership? | proof may remain a zero-premise rule, but binding identity comes only from `VAR(binding_id)` and Context edge |
| declaration proof versus TypeDeclarationDB | which record owns constructor/type classifier meaning? | proof cites declaration authority; metadata must not independently reconstruct a competing classifier |
| structural Lambda/APP/Match proof versus OperationGraph | which premise identity comes from structural occurrence edges? | Derivation cites typed Operations/Claims, not copied Core tuples |
| HOTT bridge projections versus ordinary Context action | are HOTT projection IDs ordinary Substitution nodes or a parallel morphism system? | HOTT must consume the common Context/Substitution action graph |
| universe cumulativity proof versus UniverseDB | does the proof cite the solved inequality it relies on? | universe level equality, order constraints, and proof evidence need one authority each |

The audit must search producers, validators, artifact writers/readers, test-only
builders, and debug/readback code. A shared name is not sufficient evidence of
duplication; the data dependency must be traced.

### 5.3 Authority inventory deliverable

Before G1.1 starts, add a checked-in table recording for every accepted proof
kind:

```text
proof kind
conclusion proposition key
accepted premise edges
structural authority edges
action authority edges
rule-local parameters
solver-only inputs which must not be serialized
validator entry point
artifact representation
```

No proof payload field may be added without a row in that table.

### 5.4 Baseline proof-kind authority inventory

The following table covers every proof kind accepted by the baseline validator.
`Independent` means that the rule is not itself a duplicate of another graph
operation. It may still cite structural Operation and premise edges.

| ID | Proof kind | Baseline authority/action | Classification and G1 decision |
| --- | --- | --- | --- |
| 1 | `TYPE_FORMATION_INTRO` | TypeDeclaration/TypeFormation authority and structural Term | independent formation rule; retain exact declaration authority |
| 2 | `CONSTRUCTOR_INTRO` | constructor Term plus TypeDeclarationDB owner classifier | certificate of declaration structure; declaration graph is authoritative |
| 3 | `CONSTRUCTOR_SPINE_FORMATION` | constructor owner view, telescope Context, Substitution action | action certificate; retain exact owner and telescope action edges |
| 4 | `BINDER_ASSUMPTION` | `VAR(binding_id)` and exact Context membership | independent zero-premise assumption rule; Context binding is sole identity authority |
| 5 | `MATCH_PATTERN_ASSUMPTION` | selected constructor owner/index/field and branch Context | certificate of declaration/branch structure; retain typed rule data |
| 6 | `LAMBDA_INTRO` | typed Lambda Operation, binder/body premises, recreated projection | independent intro rule with duplicated action reconstruction; cite canonical binder projection/action |
| 7 | `APP_ELIM` | function/argument Operations and dependent family instantiation | independent elimination rule; cite or deterministically intern the exact family action |
| 8 | `RETURN_INTRO` | RETURN Operation and value premise | independent CBPV intro rule |
| 9 | `THUNK_INTRO` | THUNK Operation and computation premise | independent CBPV intro rule |
| 10 | `FORCE_ELIM` | FORCE Operation and thunk premise | independent CBPV elimination rule |
| 11 | `OPERATION_REQUEST_INTRO` | request Operation, declaration, continuation Context, recreated projection | independent effect rule with duplicated Context action; retain continuation action edge |
| 12 | `COMPUTATION_FOLD_ELIM` | fold Operation and ordered clause/return premises | independent fold rule; premise edge order remains explicit |
| 13 | `MATCH_TYPE_FORMATION_INTRO` | type-level Match Operation and branch premises | independent formation rule; structural Match authority stays OperationGraph/TermDB |
| 14 | `MATCH_ELIM` | Match Operation, synthesized motive and branch premises | independent elimination rule; motive/application action must be explicit where persistent |
| 15 | `SOLVED_MATCH_MOTIVE` | solver-produced motive result and source Claims | certificate of solved constraint; solver residual is not accepted authority |
| 16 | `INDUCTION_HYPOTHESIS_ELIM` | exact Match, motive, case and recursive-field rule data | independent guarded elimination rule; preserve typed IH scope identity |
| 17 | `TEXT_LITERAL_INTRO` | literal Term and intrinsic Text classifier | independent intrinsic intro rule |
| 18 | `PURE_PRIMITIVE_TYPE_INTRO` | primitive declaration and classifier schema | certificate of intrinsic declaration; declaration is authoritative |
| 19 | `EFFECT_OPERATION_TYPE_INTRO` | effect-operation declaration and classifier schema | certificate of intrinsic operation declaration |
| 20 | `CONVERSION` | kernel conversion replay between premise and conclusion classifiers | action certificate; freeze the conversion request/profile before object equality work |
| 21 | `EXPECTED_TYPE_EXPOSURE` | compatibility replay plus universe/effect solver discharge | separate solver action from conversion; retain distinct rule only with distinct action authority |
| 22 | `TEXT_TYPE_INTRO` | intrinsic Text type Term | independent intrinsic formation rule |
| 23 | `INT_LITERAL_INTRO` | literal Term and selected machine integer classifier | independent intrinsic intro rule |
| 24 | `INT_LITERAL_ADMISSIBILITY` | range/admissibility check for selected machine representation | certificate of a deterministic host-width check; freeze machine model authority |
| 25 | `INT_TYPE_INTRO` | intrinsic machine Int type Term | independent intrinsic formation rule |
| 26 | `HOST_TYPE_INTRO` | opaque host type declaration | certificate of host declaration; host model is authoritative |
| 27 | `IS_TYPE_FROM_HAS_TYPE` | exact `HAS_TYPE` premise with Universe classifier | independent logical rule |
| 28 | `DECLARATION` | TypeDeclarationDB/export/intrinsic authority | certificate of declaration; no metadata reconstruction may compete with graph classifier authority |
| 29 | `UNIVERSE_CUMULATIVITY` | numerical universe relation, currently replayed without an action edge | certificate of UniverseDB constraint; cite exact solved constraint/provenance |
| 30 | `PI_FORMATION_INTRO` | Pi Term and domain/codomain formation premises | independent formation rule; dependent family action uses common Substitution graph |
| 31 | `CONTEXT_WEAKEN` | source Claim; Context parent walk; no projection ID | confirmed duplicate authority; first code migration |
| 32 | `EFFECT_WEAKEN` | source Claim plus closed effect-bit inclusion replay | provisional action certificate; connect to solved inclusion authority or document as non-persistent deterministic replay |

### 5.5 G1.0 audit result

The audit establishes three classes of immediate work:

1. **confirmed disconnected authority**: Context weakening;
2. **reconstructed Context action**: Lambda introduction, application family
   instantiation, constructor spines, Pi formation, and operation continuation;
3. **solver/kernel action requiring a frozen authority decision**: conversion,
   expected-type exposure, effect weakening, solved motives, and universe
   cumulativity.

G1.1 starts only with class 1. Classes 2 and 3 are migrated through the common
semantic-action edge after the edge representation is proven by weakening.

## 6. Target graph model

### 6.1 Context key

Replace parallel classifier fields with a tagged reference:

```text
ContextClassifierRef =
  Solved(term_id)
  Unresolved(classifier_variable_id)

ContextKey =
  Empty
  Extend(parent_context_id, binding_id, ContextClassifierRef)
```

`depth` is derived cache data and excluded from the key.

### 6.2 Substitution key

Use a tagged exact key:

```text
Identity(context_id)
Empty(source_context_id)
Projection(extended_context_id)
Extend(prefix_substitution_id, target_context_id,
       term_id, term_classifier_id)
Compose(outer_substitution_id, inner_substitution_id)
```

Source and target Context IDs are validated derived fields. Constructors may
normalize categorical identities:

```text
id o sigma = sigma
sigma o id = sigma
```

Further equations require a separately justified normalization policy. G1 does
not silently make SubstitutionDB an e-graph.

### 6.3 Context action key

Introduce one typed action cache/arena rather than fresh helper-local results:

```text
TermReindex(term_id, substitution_id) -> term_id

ContextPullback(source_extension_id, substitution_id) -> {
  target_extension_id,
  lifted_substitution_id
}
```

If a pullback introduces a fresh binding object, creation occurs only while
materializing a previously unseen exact action key. Repeating the key returns
the existing result.

### 6.4 Judgement proposition key

Use one immutable proposition arena shared by candidates and accepted Claims:

```text
JudgementPropositionKey = {
  kind,
  authority_kind,
  authority_id,
  context_id,
  operation_id_or_invalid,
  subject_term_id,
  classifier_term_id
}
```

Candidate and accepted state reference a proposition ID. Neither state is part
of proposition identity.

### 6.5 Derivation edge model

A Derivation contains:

```text
conclusion_claim_id
proof_kind
ordered premise-edge slice
ordered semantic-action-edge slice
rule-parameter slice
closure/source dependency slice
```

A premise edge points to a proposition ID and optionally an accepted Claim ID.
A scoped premise has no accepted Claim but still shares its proposition node.

A semantic action edge is tagged, for example:

```text
SUBSTITUTION(substitution_id)
TERM_REINDEX(action_id)
CONTEXT_PULLBACK(action_id)
CONVERSION_REQUEST(request_id)
EFFECT_INCLUSION(constraint_or_result_id)
```

Only tags confirmed by the G1.0 audit are introduced. This is not a generic
untyped bag of IDs.

## 7. Context weakening migration

### 7.1 New construction contract

Replace the current source-only call with an API equivalent to:

```text
record_context_weaken(
  delta,
  source_evidence,
  projection_or_composition_substitution_id)
```

The constructor checks:

1. substitution source is the target judgement Context;
2. substitution target is the source judgement Context;
3. the substitution is a projection or a canonical composition of
   projections accepted as structural weakening;
4. subject and classifier action are the intended weakening action;
5. source evidence is authority-complete; and
6. the resulting Derivation records the exact substitution ID.

### 7.2 Multi-level weakening

The operation solver currently searches parent Contexts and emits one
`CONTEXT_WEAKEN` when it finds evidence. It must simultaneously construct the
canonical projection path:

```text
Gamma.A.B -> Gamma.A -> Gamma
```

The path is represented by `COMPOSE` nodes under the chosen composition
orientation. The implementation must not encode the number of parent steps as
proof data.

### 7.3 Validation contract

`validate_context_weaken_proof()` must receive `SubstitutionDB` and the
Derivation action edge. It validates the exact source/target pair and action.

The old parent walk remains useful in `prototype_context_db_validate()` and in
projection-constructor validation. It is not independently sufficient proof of
the accepted Derivation.

### 7.4 Operation ownership

`validate_operation_owned_relation()` currently exempts `CONTEXT_WEAKEN` from
the ordinary Context equality check. Replace this exceptional condition with a
check derived from the recorded substitution action. The conclusion Operation
and weakened Claim must be connected explicitly; Context mismatch must not be
accepted merely because the proof-kind tag says weakening.

### 7.5 Required tests

- one-level weakening records the exact projection;
- repeated one-level weakening reuses its Substitution ID;
- multi-level weakening records the correct composition;
- a valid ancestor with a forged unrelated projection is rejected;
- a projection with reversed source/target is rejected;
- changing only the proof-kind tag does not bypass Operation ownership;
- artifact readback preserves the action edge; and
- relocation re-interns the same typed path.

## 8. Context and Substitution interning migration

### 8.1 Index implementation

Add typed hash indices beside immutable arenas. The arena record remains the
authoritative full key. Lookup procedure:

1. hash the typed key;
2. inspect all candidates in the bucket;
3. compare every key field;
4. return the existing ID on exact match; or
5. append one immutable record and index its ID.

Hash iteration order must not affect serialized order. If deterministic arena
IDs depend on construction order, tests must freeze the construction order.

### 8.2 Migration order

1. introduce key constructors and equality/hash helpers;
2. route all Context construction through one intern API;
3. route all Substitution construction through one intern API;
4. remove old linear duplicate scans;
5. reject direct arena mutation outside initialization/import; and
6. add collision-forcing test hooks.

### 8.3 Artifact import

Artifact readers relocate dependencies first, rebuild the full typed key, and
intern it. Numeric source IDs are never equality evidence. No v68 fallback is
retained after the schema break chosen by G1.10.

## 9. Reindex and pullback migration

### 9.1 Lazy substitution action

Replace per-call dense replacement preparation with lookup by
`(binding_id, substitution_id)` and memoized recursive action. The TermDB
scope-aware traversal remains the single implementation for binding rewrite.

Required properties:

- identity action returns the original Term ID;
- unchanged subgraphs retain their IDs;
- Lambda and Match scopes block captured bindings correctly;
- IH scope identity is preserved;
- composition agrees with successive action; and
- a failed action does not publish a partial cache result.

### 9.2 Interned comprehension pullback

Replace `prototype_context_fresh_reindex_extension()` with an exact pullback
action API. Update all callers in `context_category_check.c` and `ast.c`.

The action cache is invalidated only by arena destruction, not by solver
frontier rollback. Its inputs are immutable graph IDs.

## 10. Judgement migration

### 10.1 Proposition interning

Introduce `prototype_judgement_proposition` and intern it by the exact key in
Section 6.4. Convert:

- claim candidates to `{ proposition_id, candidate_state... }`;
- accepted Claims to `{ proposition_id, closure_rank... }`;
- selected evidence to carry the Claim/proposition authority without copying
  a second writable tuple where possible.

### 10.2 Premise arena

Replace accepted fixed parallel premise arrays with one ordered edge arena.
Candidate arrays may remain temporarily during solver migration, but they must
be converted through one edge-construction API and removed before G1 exits.

The edge role distinguishes structural premise order from source closure
dependencies. It must not infer role from array position in unrelated rules.

### 10.3 Multiple derivations

Graph-consing applies to proposition identity, not derivation uniqueness.
Multiple sound derivations of one proposition are retained. An exact duplicate
rule application may be interned by its complete derivation key, but a later
derivation never overwrites an earlier one.

### 10.4 Rule-data authority

Move constructor, induction, substitution, conversion, and effect-specific
parameters into typed rule-data records. Do not add another fixed union field
for each new HOTT rule to the common Derivation header.

## 11. Resource-ready boundary

Future resource checking uses the same binding objects but a separate usage
environment:

```text
Gamma                         dependent structural Context
U : BindingID -> Grade        usage demand
Gamma ; U |- operation : A
```

This separation is mandatory because weakening has two meanings:

- structurally, projection `Gamma.A -> Gamma` always exists;
- resource-theoretically, discarding `A` may be forbidden.

V3-G1 records the structural projection exactly. A later resource proof adds a
premise authorizing the action; it does not change Context or TermDB identity.

## 12. Artifact and readback policy

If the in-memory migration changes wire records, increment the artifact version
once after the target model is stable.

The new artifact must serialize:

- immutable Context keys;
- immutable Substitution keys;
- proposition records;
- Claims and Derivations by graph IDs;
- premise edges;
- semantic action edges; and
- typed rule-data records required for replay.

It must not serialize:

- hash tables;
- mutable solver candidates;
- transient WHNF/reindex caches unless a future cache section is explicitly
  non-authoritative;
- source allocation offsets as semantic identity; or
- duplicate parent-chain descriptions of recorded projections.

Malformed artifacts must be rejected for dangling action edges, wrong
source/target Contexts, forged premise Claims, wrong operation ownership, and
duplicate records which claim conflicting identity.

## 13. Implementation phases

### V3-G1.0 Semantic authority inventory

Status: complete locally

- [x] enumerate every proof producer and validator;
- [x] enumerate persistent Context/Substitution/HOTT certificate helpers;
- [x] classify every overlap using Section 5;
- [x] freeze the authority inventory before changing structs;
- [x] add allocation, lookup, hit, and probe counters with G1.2 instrumentation;
- [x] add a direct regression demonstrating the current unreferenced weakening
      projection problem.

Exit condition: no accepted proof kind has an undocumented structural/action
dependency.

### V3-G1.1 Context weakening authority edge

Status: complete locally

- [x] define canonical one- and multi-level projection construction;
- [x] add typed substitution/action rule data to Derivation candidates;
- [x] change the weakening producer API;
- [x] change solver ancestor search to return the path authority;
- [x] change the validator to consume `SubstitutionDB`;
- [x] remove proof-kind-only Operation Context exception;
- [x] add forgery and artifact tests.

Implementation result:

- `CONTEXT_WEAKEN` carries a `SUBSTITUTION` semantic action ID;
- one- and multi-step weakening use `PROJECTION`/`COMPOSE` paths;
- artifact v69 serializes and validates that edge and rejects a forged ID;
- Context classifier resolution rebuilds the projection path after Context ID
  relocation instead of retaining a stale pre-resolution path;
- general `EXTEND` substitutions are not blindly relocated, because their term
  classifier is indexed by the target Context and requires typed reindexing;
- all 16 prototype scripts pass after the migration.

Exit condition: no accepted `CONTEXT_WEAKEN` proof can exist without an exact
Substitution/action edge.

### V3-G1.2 Typed interning infrastructure

Status: complete locally

- [x] implement typed key hash/equality helpers;
- [x] add collision-safe indices;
- [x] add deterministic construction tests;
- [x] expose request, hit, and probe counters.

Hashes select candidate buckets only. Context, Substitution, Claim, and
Derivation identity is decided by complete field comparison. The permanent
collision test inserts more keys than buckets and retrieves distinct colliding
nodes by exact identity.

### V3-G1.3 Context migration

Status: complete locally

- [x] replace parallel classifier fields with a tagged reference;
- [x] route extension through one key API;
- [x] keep exact binding identity;
- [x] move ancestor/path checks to Context integrity APIs;
- [x] update artifact relocation and tests.

The tagged reference has `TERM`, `VARIABLE`, and `PROVISIONAL(term, variable)`
states. Fixed-point iterations retain `PROVISIONAL`; only the final
materialization pass changes a resolved entry to `TERM`. This prevents both
premature classifier capture and permanently split equivalent Contexts.

### V3-G1.4 Substitution migration

Status: complete locally

- [x] route all five constructors through collision-safe key APIs;
- [x] normalize identity composition only under documented laws;
- [x] remove linear duplicate scans;
- [x] validate source/target types on imported nodes;
- [x] add category and collision law tests.

### V3-G1.5 Reindex action migration

Status: complete locally

- [x] add memoized exact `(term, substitution)` action results;
- [x] preserve one scope-aware TermDB traversal implementation;
- [x] keep the cache non-authoritative and clear it after graph relocation;
- [x] retain deep, sparse, Lambda, Match, and IH law coverage.

### V3-G1.6 Pullback action migration

Status: complete locally

- [x] define `(base_context, source_extension)` as the pullback request key;
- [x] cache action results without making the cache identity authority;
- [x] prevent repeated exact requests from allocating fresh bindings;
- [x] prove repeated exact requests reuse Context and Substitution IDs;
- [x] retain different binding-object requests as distinct.

### V3-G1.7 Judgement proposition and edge migration

Status: complete locally

- [x] graph-cons accepted Claim propositions;
- [x] graph-cons accepted Derivations independently of closure rank;
- [x] retain ordinary premise Claim IDs as direct DAG edges;
- [x] retain scoped premises as rule-local parameters without fabricating Claims;
- [x] introduce typed semantic-action edges for non-derivable actions;
- [x] preserve multiple distinct Derivations of one Claim;
- [x] remove the duplicated `source_claim_ids` accepted dependency list.

The planned separate premise-edge arena was rejected after implementation
audit. A Claim ID stored in rule-premise order already is the graph edge, just
as a TermDB child ID is an edge. A second arena would add an identity layer
without removing information. Scoped parameters remain inline because they are
not accepted propositions. Closure, HOTT traversal, and artifact reachability
now follow premise Claim IDs directly.

### V3-G1.8 Remaining authority consolidation

Status: audit complete; independent kernel work recorded

- [x] retain conversion and expected exposure as different checks: DefEq
      conversion versus compatibility/solver exposure;
- [x] classify closed-row `EFFECT_WEAKEN` as deterministic replay rather than
      a second persistent effect-constraint authority;
- [x] keep Lambda and continuation projections as canonical derived actions
      from premise Contexts, not duplicated stored action IDs;
- [x] keep HOTT Context/Substitution certificates on the common graph;
- [ ] connect universe cumulativity to an exact UniverseDB inequality witness;
- [x] record the remaining declaration and universe work as kernel work rather
      than concealing it in graph-consing.

`validate_universe_cumulativity_proof()` currently checks only that both terms
are universe variables. It does not receive UniverseDB and therefore cannot
certify an inequality. This is a pre-existing kernel defect and the next
universe phase must change the validator interface and artifact authority; it
is not repaired by inventing another generic semantic-action tag here.

### V3-G1.9 Resource boundary audit

Status: complete locally

- [x] verify no usage count enters TermDB or Context keys;
- [x] verify binding IDs suffice as future grade-map keys;
- [x] document structural weakening versus resource admissibility;
- [x] reserve no speculative resource proof tag.

### V3-G1.10 Artifact migration

Status: complete locally

- [x] freeze artifact v69;
- [x] update writer, reader, append, and linker paths;
- [x] reject v68 without retaining a compatibility reader;
- [x] add forged weakening-action fixtures;
- [x] rebuild non-wire hash indices after readback and sparse slicing;
- [x] verify deterministic artifact output.

### V3-G1.11 Exit audit and O1 handoff

Status: complete locally

- [x] run warning-free build of interpreter and reader;
- [x] run all 16 prototype tests;
- [x] run examples 01-07 and 09;
- [x] run artifact round-trip, append, and link tests;
- [x] run allocation/collision/deep-context tests;
- [x] run `git diff --check`;
- [x] re-audit direct struct writes and unreferenced semantic reconstruction;
- [x] update V3 and this ledger with artifact evidence.

## 14. Test matrix

| Area | Positive cases | Negative cases |
| --- | --- | --- |
| Context identity | exact extension reuse; distinct parent/binder/classifier distinction | hash collision; forged depth |
| Substitution identity | identity, projection, extend, compose reuse | wrong source/target; dangling child |
| Weakening | one-level and composed projection | ancestor without recorded path; unrelated path; reversed path |
| Reindex | identity, composition, dependent classifier, Match/IH scope | capture, stale binding, partial failed cache |
| Pullback | repeated exact action reuse | distinct source binding accidentally merged |
| Proposition | candidate/Claim share proposition; multiple derivations | operation authority or Context mismatch |
| Premises | accepted and scoped edges; ordered fold premises | dangling Claim; copied tuple disagreement |
| Conversion/effects | exact frozen action or documented replay | ambient profile change; forged row inclusion |
| Artifact | deterministic round trip and append | sparse hole, dangling edge, duplicate conflicting key |

## 15. Performance evidence

G1 is not justified only by speed, but regressions must be measured. Record for
representative examples:

- Context node count and intern probes;
- Substitution node count and intern probes;
- reindex request/cache-hit/materialization counts;
- pullback request/cache-hit/materialization counts;
- proposition and premise-edge counts;
- accepted Claim and Derivation counts; and
- artifact Context/Substitution/Judgement section sizes.

The expected semantic result is stable identity and fewer reconstructions. No
fixed numeric Term or Claim ID is a test oracle.

## 16. Stop conditions

Stop implementation and revise this plan if it requires:

- equating Contexts by convertible rather than exact classifier references;
- merging distinct binding objects;
- adding a second binding database;
- treating a hash as proof;
- storing mutable solver variables in accepted immutable identity without an
  explicit solved/unresolved transition;
- validating a proof by silently recreating an existing structural operation;
- making every proof rule carry all database IDs regardless of relevance;
- turning SubstitutionDB into an unrestricted equality-saturation engine;
- making artifact acceptance depend on allocation order; or
- retaining old and new authority paths simultaneously for compatibility.

## 17. Progress ledger

| Phase | State | Commit | Artifact | Tests | Notes |
| --- | --- | --- | --- | --- | --- |
| Baseline | complete | `942bed77` | v68 | 16 scripts | V2-P1 frozen |
| G1.0 | complete locally | uncommitted | v68 | 16 scripts | 32-rule authority inventory; counters move with G1.2 instrumentation |
| G1.1 | complete locally | uncommitted | v69 | 16 scripts | exact projection action, relocation, artifact forgery rejection |
| G1.2 | complete locally | uncommitted | v69 | 16 scripts | collision-safe indices and counters |
| G1.3 | complete locally | uncommitted | v69 | 16 scripts | tagged provisional/concrete Context identity |
| G1.4 | complete locally | uncommitted | v69 | 16 scripts | five Substitution constructors graph-consed |
| G1.5 | complete locally | uncommitted | v69 | 16 scripts | non-authoritative reindex cache |
| G1.6 | complete locally | uncommitted | v69 | 16 scripts | exact pullback request reuse |
| G1.7 | complete locally | uncommitted | v69 | 16 scripts | Claim/Derivation graph-consing; premise IDs are DAG edges |
| G1.8 | audit complete | uncommitted | v69 | 16 scripts | derived checks documented; UniverseDB witness remains separate kernel work |
| G1.9 | complete locally | uncommitted | v69 | 16 scripts | resource grades remain outside graph identity |
| G1.10 | complete locally | uncommitted | v69 | 16 scripts | strict schema break and forged-action rejection |
| G1.11 | complete locally | uncommitted | v69 | 16 scripts | strict builds and handoff audit pass |

## 18. Immediate next action

Commit and publish V3-G1, then begin the next explicitly scoped kernel phase.
The highest-priority independent defect discovered by the authority audit is
Universe cumulativity evidence: accepted cumulativity must cite or replay an
exact solved UniverseDB inequality instead of accepting any two universe
variables.
