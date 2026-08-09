# Semantic Consolidation and Resource-Ready Reindex Plan

Date: 2026-08-09

Status: design proposal; no implementation is authorized by this document

Repository baseline:

- branch: `main`;
- commit: `31e5446` (`Implement first observational action fragment`);
- ordinary artifact format: v69;
- compiler-local HOTT manifest: `hott_fragment_v2.schema`;
- verification performed for this audit: the book `make check`, the Context
  category check, and the strict HOTT goal check pass.

Related documents:

- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`;
- `2026-08-08T22-51-04-CONTEXT-SUBSTITUTION-JUDGEMENT-GRAPH-CONSING-V3-G1-PLAN.md`;
- `2026-08-09T07-10-00-V2-O1-OBSERVATIONAL-ACTION-IMPLEMENTATION-PLAN.md`;
- `2026-08-07T02-00-00-SHARED-TERM-HOTT-DCBPV-NORMATIVE-CALCULUS.md`.

## 1. Objective

Reduce implementation size and semantic duplication without collapsing the
distinct categorical and proof-theoretic sorts already present in the
prototype. The consolidation must make typed reindexing the single structural
authority behind weakening, dependent instantiation, HOTT endpoint action, and
naturality checks.

The result must also preserve a clean path to graded, affine, or linear usage
checking. In particular, a structural projection must not itself become proof
that discarding a resource is admissible.

The intended high-level organization is:

```text
base CwF
  Context
  Substitution
  Type and Term reindexing
  comprehension and pullback
  typed reindex authority

ordinary proof layer
  Judgement proposition
  Derivation
  premise edges
  rule-specific evidence

observational action
  Gamma |-> Gamma^R
  sigma |-> sigma^R
  A |-> A^R
  a |-> a^R
  certificates referring to base-CwF actions

future resource layer
  usage environment U
  grade algebra
  admissibility certificate for a structural action
  Gamma ; U |- t : A
```

This is a consolidation plan, not a request to make one universal untyped
graph.

## 2. Current Size and Responsibility Audit

At the baseline, the largest prototype implementation files are approximately:

```text
src/prototype/ast.c       30,067 lines
src/prototype/typing.c    19,274 lines
src/prototype/term.c       9,494 lines
src/prototype/hott.c       5,454 lines
src/prototype/read_file.c  4,755 lines
```

The dedicated HOTT test fixture is itself approximately 3,721 lines. File size
alone is not evidence of a semantic defect, but it exposes three different
kinds of cost:

1. essential distinctions between syntax, occurrences, Contexts, proofs, and
   runtime values;
2. repeated infrastructure for interning, requests, results, certificates,
   validation, and residual state; and
3. repeated reconstruction of one mathematical action in several layers.

The first kind must remain. The second and third are the target of this plan.

### 2.1 Consolidation already completed

The current baseline already contains important safe consolidation:

- one shared TermDB is used for ordinary and observational syntax;
- Context and Substitution have typed hash-cons indices;
- Context classifier identity is one tagged reference rather than two
  independent identities;
- exact Term/Substitution reindex requests have a cache;
- comprehension pullback is an interned CwF action;
- Context weakening Derivations retain a semantic Substitution action edge;
- HOTT declarations and implementation are separated into `hott.h` and
  `hott.c`;
- HOTT action request identity is separated from mutable result state and
  replay certificates; and
- observational equality remains separate from kernel definitional
  conversion.

These decisions are retained.

### 2.2 Remaining concrete duplication

The baseline still exposes the following consolidation opportunities.

#### R1. Reindex has several public semantic faces

Weakening, explicit substitution reindex, dependent-family instantiation,
endpoint reindexing, and HOTT naturality all describe an exact typed
Substitution acting on syntax or a proposition. They have different proof
rules, but must not independently establish source Context, target Context,
subject, classifier, or converted result.

#### R2. Certificate ownership is split by feature history

Substitution certificates live with Judgement declarations. Context-formation
certificates currently live in `hott.h`, despite Context formation not being a
HOTT-specific concept. Bridge and action certificates use another set of
containers and validation entry points.

The sorts are legitimately distinct, but their common authority-edge pattern
is not represented by common infrastructure.

#### R3. Derivation premise storage remains physically duplicated

Both solver-local derivation candidates and accepted Derivations embed fixed
arrays sized for the maximum computation-fold premise count. Most rules use
zero, one, or two premises. The current representation repeats scoped premise
kind, Context, subject, and classifier tuples in every record.

This is inconsistent with the intended premise-edge graph discipline even
when the arrays are interpreted conceptually as edges.

#### R4. HOTT work and action outcomes repeat execution metadata

Observation work and type-directed action results are different semantic
objects. Nevertheless, both carry variants of state, residual reason,
normalization profile, budget, Term graph revision, and calculus fingerprint.
The identity records must stay separate; the deterministic execution outcome
plumbing need not be duplicated.

#### R5. HOTT APIs repeat one large dependency bundle

Most HOTT constructors and validators repeatedly receive Context,
Substitution, Context certificate, Substitution certificate, bridge, Term,
type-declaration, Operation, and Judgement databases. This makes declarations
long, repeats argument checking, and makes it easy to validate against a
mismatched set of stores.

#### R6. Type-former recursion is partly descriptor-driven and partly
imperative

The first observational action has one type-former descriptor, but child
action startup, residual propagation, witness construction, and proof replay
remain repeated across large tag-specific branches.

#### R7. Runtime environment extension still performs unrelated bulk copying

The Operation runtime environment is a fixed binding array copied on each
extension. This is not a CwF or HOTT unification problem, but it is an
independent source of implementation and execution cost and must not be
mistaken for static Context semantics.

## 3. Semantic Sorts That Must Remain Distinct

The consolidation must not merge the following pairs.

### 3.1 Context and Runtime Environment

```text
Context Gamma
  static telescope of binding identities and classifiers

Runtime environment rho
  dynamic assignment of evaluated values, resources, and resumptions
```

Their intended relationship is a future realization judgement:

```text
rho |= Gamma
```

It is not structural identity.

### 3.2 Context and Substitution

Context is an object of the syntactic category. A Substitution is a typed
morphism. A universal node arena would obscure source/target invariants and
would not reduce the proof obligations.

### 3.3 Term and Operation occurrence

TermDB owns shared syntax and alpha-consed Core shape. OperationGraph owns a
typed source or generated occurrence. Shared syntax may have several typed
occurrences and proof authorities.

### 3.4 Claim and Derivation

A Claim is proposition identity. A Derivation is one rule application proving
that proposition. One Claim may have several Derivations.

### 3.5 Structural action and certificate

A Substitution or Context extension is not evidence that the corresponding
typing action is valid. A certificate points to an authoritative structural
action and accepted premises.

### 3.6 Definitional and observational equality

Kernel conversion is a meta-level decision under an exact normalization
profile and budget. `OBSERVATION_TYPE` is object syntax, and its witness is an
ordinary typed Term with a Derivation. Successful observation must not extend
global definitional equality.

### 3.7 Request and result

An immutable action key must not acquire mutable search state. A repeated
request returns the same request identity and may refer to a separately
published deterministic result.

## 4. Canonical Typed Reindex Authority

### 4.1 Structural action

For an exact Substitution:

```text
sigma : Delta -> Gamma
```

the base CwF provides structural actions:

```text
A in Type(Gamma) |-> A[sigma] in Type(Delta)
t in Term(Gamma,A) |-> t[sigma] in Term(Delta,A[sigma])
```

The implementation should intern an immutable typed reindex action key:

```text
ReindexAction(
  object_kind,
  source_object_id,
  substitution_id,
  normalization_profile_if_required,
  deterministic_budget_if_required
)
```

Pure structural Term reindex does not require a normalization profile. A
profile belongs only to an action whose validation invokes conversion.

The structural result contains only replayable graph outputs:

```text
source_context_id
target_context_id
source_term_or_proposition_id
result_term_or_proposition_id
substitution_id
```

The action does not contain proof search state or a resource permission.

### 4.2 Weakening as a specialization

For a Context extension:

```text
p : Gamma.A -> Gamma
```

ordinary weakening is reindex along `p`:

```text
Gamma |- t : T
--------------------------------
Gamma.A |- t[p] : T[p]
```

`CONTEXT_WEAKEN` may remain a compact, readable proof kind. Its Derivation must
refer to the exact reindex authority whose Substitution is the projection or
canonical composition of projections. The validator must call the common
typed-reindex replay path, then additionally check the weakening-specific
shape.

It must not prove weakening by an independent parent walk. A parent walk is
only Context integrity validation.

### 4.3 General substitution reindex

`SUBSTITUTION_REINDEX` uses the same action and differs only in rule-specific
premises. There must be no second implementation of source/target orientation,
binding lookup, or result reconstruction.

### 4.4 Dependent instantiation

Pi application, constructor telescope instantiation, handler telescope
instantiation, and computation continuation classifiers must use the same
reindex action API. Callers may construct a typed `EXTEND` Substitution, but
must not directly substitute a binder through a separate Term traversal and
then claim that it represents the same categorical action.

### 4.5 HOTT endpoints and naturality

Given a bridge:

```text
pi0, pi1 : Gamma^R -> Gamma
```

the endpoint Terms and endpoint Types are ordinary reindex results:

```text
a0 = a[pi0]
a1 = a[pi1]
A0 = A[pi0]
A1 = A[pi1]
```

HOTT action certificates point to these exact base-CwF actions. Substitution
naturality may require conversion of two structurally different Substitution
DAGs, but each side of the square must still be constructed through canonical
Substitution and reindex APIs.

## 5. Common Authority-Edge Infrastructure

### 5.1 Common header, typed payloads

Do not create one untyped certificate node. Introduce a common immutable
header used by kind-specific arenas or a tagged payload:

```text
AuthorityEdgeHeader
  id
  kind
  structural_sort
  structural_id
  primary_claim_id_or_invalid
  key_hash
  hash_next
```

Kind-specific payloads retain exact additional authority:

```text
ContextFormationAuthority
  context_id
  classifier_is_type_claim_id

SubstitutionExtensionAuthority
  substitution_id
  assigned_term_has_type_claim_id

ReindexAuthority
  reindex_action_id
  source_claim_id
  result_claim_id

HottBridgeAuthority
  bridge_id
  parent_bridge_id
  type_action_certificate_id
  endpoint Context authority IDs
  endpoint Substitution authority IDs
```

Common infrastructure supplies allocation, interning where identity is
immutable, index rebuild, ID bounds checking, and dispatch. Kind-specific
validators remain independent.

### 5.2 Ownership correction

Context-formation certificates are base Context/Judgement boundary objects.
Move their declarations out of `hott.h`. A suitable destination is a small
`certificate.h`/`certificate.c` module, or the Context/Judgement boundary if a
new module would create a dependency cycle.

Substitution certificates should move through the same public authority-edge
interface. HOTT depends on this interface rather than owning parallel Context
and Substitution certificate concepts.

### 5.3 Independent replay remains mandatory

Constructors and validators may share immutable descriptors and structural
lookup APIs. They must not share one imperative procedure which both constructs
and accepts evidence. Otherwise one implementation bug can create and approve
the same forged certificate.

The accepted pattern is:

```text
constructor
  uses canonical graph constructors
  publishes an authority edge

validator
  follows stored IDs
  independently checks the declared rule
  uses the same declarative rule descriptor
```

## 6. Premise Edge Arena Migration

### 6.1 Target representation

Replace fixed premise arrays in accepted Derivations with an edge slice:

```text
Derivation
  proof_kind
  conclusion_claim_id
  rule_data
  first_premise_edge
  premise_edge_count
  key_hash
  hash_next

PremiseEdge
  owner_derivation_id
  ordinal
  role
  accepted_claim_id_or_invalid
  scoped_proposition_id_or_invalid
```

A scoped proposition is interned once:

```text
JudgementProposition(
  kind,
  authority,
  context_id,
  operation_id,
  subject,
  classifier
)
```

It is not automatically an accepted Claim.

### 6.2 Candidate representation

Solver-local derivation candidates should use a candidate edge arena with the
same immutable proposition identity but separate acceptance state:

```text
DerivationCandidate
  proof_kind
  conclusion_proposition_id
  first_candidate_edge
  candidate_edge_count
  rule_data
```

Commit maps candidate premise propositions to exact accepted Claims where
required. It must not copy seven parallel arrays.

### 6.3 Rule data

Replace the growing common Derivation struct fields with a tagged rule-data
arena or compact union. Match and induction parameters remain exact, but a
text-literal proof does not reserve their storage.

HOTT-specific source bridge and action request identities belong to
HOTT-specific rule data, not to every ordinary Derivation.

### 6.4 Artifact policy

Premise representation is part of the accepted proof wire contract. Perform
one explicit artifact schema migration after the in-memory edge representation
is stable. Do not retain v69 fixed-array compatibility inside the new graph.

A separate compatibility conversion tool may read an old artifact and emit a
new artifact, but the kernel reader must validate only one current schema.

## 7. Shared Kernel and HOTT Views

### 7.1 Read-only validation view

Bundle the mutually consistent read-only stores passed to validators:

```c
struct prototype_kernel_view {
	const struct prototype_context_db* contexts;
	const struct prototype_substitution_db* substitutions;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_operation_graph* operations;
	const struct prototype_judgement_db* judgement;
	const struct prototype_authority_db* authorities;
};
```

Term and TypeDeclaration pointers are mutable only where normalization caches
or graph-consing require mutation. APIs should use narrower const views when
possible.

### 7.2 Mutable construction view

Use a separate builder view for action execution:

```text
HottActionBuilder
  kernel stores
  mutable Context/Substitution stores
  mutable authority store
  bridge store
  action store
  deterministic normalization policy
```

The read-only validator must not accept a builder and must not mutate action
identity.

### 7.3 No hidden global dependency

The view is a checked capability bundle, not a global singleton. Initialization
validates that all referenced graph capacities and fingerprints belong to one
compile session. Artifact import constructs a fresh view after relocation and
index rebuild.

## 8. Shared Deterministic Outcome Plumbing

Observation work and observational action requests remain distinct identities.
Extract only their common execution outcome:

```text
DeterministicOutcome
  state: READY | RESIDUAL | UNSUPPORTED
  residual_reason
  normalization_profile
  step_limit
  steps_used
  term_graph_revision
  calculus_fingerprint
```

Rules:

- a Goal never becomes an Action request;
- an Action request never becomes mutable Work identity;
- a conversion budget is recorded only when conversion was actually used;
- exhaustion is residual, never successful equality;
- publishing an outcome does not publish an object witness; and
- Artifact publication remains governed by the HOTT artifact phase.

Common helper code may validate states, fingerprints, profiles, and revisions.
Candidate selection and action-certificate validation remain kind-specific.

## 9. Descriptor-Directed Type and Term Action

### 9.1 One type-former semantic descriptor

Extend the existing type-former descriptor rather than adding tag switches in
each feature. The descriptor may declare:

```text
type former kind
formation replay rule
ordinary substitution behavior
observational Type action rule
observational Term action rule
child roles and ordering
purity requirement
resource policy hook
artifact semantic identifier
```

The descriptor does not contain mutable action output.

### 9.2 Generic recursion, specific formation

The HOTT executor may share:

- child Type-action request interning;
- child Term-action request interning;
- result-state propagation;
- exact Claim lookup;
- witness-premise edge creation;
- endpoint reindex startup; and
- deterministic outcome publication.

Type-former handlers retain:

- the exact relation-family constructor;
- the exact witness constructor;
- rule-specific telescope formation;
- Match/IH scope validation;
- effect and purity rejection; and
- rule-specific Derivation payload.

This avoids both a single unreviewable generic callback machine and repeated
imperative scaffolding.

## 10. Resource-Ready Boundary

### 10.1 Structural Context remains unchanged

Do not place usage grades in Context identity:

```text
Gamma
  dependent telescope and binding identity

U : BindingID -> Grade
  usage demanded by one derivation

Gamma ; U |- t : A
  future resource-sensitive Judgement
```

The same Context and Term may participate in linear, affine, graded, and
unrestricted derivations.

### 10.2 Grade algebra is a policy

The first resource implementation should define a checked grade interface, not
hard-code one interpretation into TermDB:

```text
zero
one
add
multiply or substitution scaling
order/admissibility relation
unrestricted grade when provided
```

Example policies include:

```text
ordinary: 0, 1, omega with unrestricted weakening and contraction
affine:   0 or 1; weakening allowed, contraction rejected
linear:   exactly 1 for linear assumptions
graded:   policy-specific semiring or ordered semiring
```

### 10.3 Structural versus admissible Substitution

Retain the current raw typed Substitution DAG:

```text
sigma : Delta -> Gamma
```

Add a separate future resource certificate:

```text
ResourceSubstitutionAuthority
  substitution_id
  source_usage
  target_usage
  per-assignment usage summaries
  grade_policy_id
```

A structural projection always exists in the base CwF. It is not necessarily
an admissible morphism for a linear Judgement.

### 10.4 Weakening policy

For:

```text
p : Gamma.A -> Gamma
```

ordinary or affine weakening may accept a zero demand for the new binding. A
linear policy rejects the action unless the binding is under an unrestricted
modality or the rule provides an explicit discard capability.

Therefore:

```text
projection existence != resource discard permission
```

The common typed-reindex action remains valid structurally. The future
resource-sensitive Derivation additionally refers to a resource admissibility
certificate.

### 10.5 Contraction policy

A diagonal-like Substitution may assign two target bindings from one source
binding. Usage composition adds both demands. Affine and linear policies reject
the resulting demand when duplication is forbidden; an unrestricted policy may
accept it.

### 10.6 Usage action under substitution

Each assigned Term has a usage vector over source bindings. A Substitution can
therefore be interpreted as a sparse usage matrix. Reindexing transforms a
target usage vector by composition with that matrix.

```text
U_Delta = usage(sigma) * U_Gamma
```

The structural Term result may still be cached by `(term, substitution)`.
Resource admissibility must not use that cache entry as evidence; its key also
contains the source usage, target usage, and grade policy.

### 10.7 HOTT resource action

For a relational extension:

```text
Gamma^R.x0:A0.x1:A1.r:AR(x0,x1)
```

future HOTT action must compute or validate usage of both endpoints and the
relation witness. Usage is attached to generated Claims and Derivations, not
to `OBSERVATION_TYPE` or `OBSERVATION_WITNESS` Term identity.

The HOTT action request identity need not change until resource-sensitive
action is implemented. A future resource action request must include an exact
usage-policy input rather than silently reusing an unrestricted result.

### 10.8 Runtime boundary

Static usage evidence and runtime consumption state remain separate:

```text
Gamma ; U |- operation : A
rho |= Gamma ; U
```

Runtime linear resources require move/consume discipline, and resumptions
require multiplicity enforcement. Neither concern is represented by copying
grades into the static Context node.

## 11. Runtime Environment Refactor Boundary

This phase is independent of semantic consolidation but should follow the same
identity discipline.

Replace fixed-array whole-environment copying with one of:

1. a parent-linked persistent environment frame;
2. a stack with explicit save/restore indices; or
3. immutable frames for captured resumptions plus a stack for ordinary calls.

Instantiate a Term with one combined environment substitution rather than one
complete Term traversal per binding. Runtime lookup may be indexed separately
from static Context lookup.

Resource-sensitive execution later adds consumption state to runtime bindings
or frames. It does not mutate ContextDB and does not turn Runtime Environment
identity into SubstitutionDB identity.

## 12. File-Level Target Layout

One possible physical organization is:

```text
context.h / context.c
  Context and raw Substitution graph
  comprehension pullback
  structural reindex action

certificate.h / certificate.c
  common authority-edge infrastructure
  Context formation authority
  Substitution extension authority
  reindex authority

judgement.h / judgement.c
  proposition and Claim graph
  Derivation and premise-edge graph
  rule validation dispatch

hott.h / hott.c
  HOTT identity and public action APIs

hott_plan.c
  observation goals, candidates, work, residuals

hott_action.c
  Context/Substitution/Type/Term action execution

hott_validate.c
  independent HOTT certificate replay

resource.h / resource.c, future
  grade policy
  usage vectors
  resource admissibility certificates

operation_runtime.c, future extraction from ast.c
  runtime frames, environment, requests, resumptions
```

Splitting a file is not by itself a line-count reduction. The split is complete
only when dependency direction is one-way and duplicated helpers are deleted.

## 13. Migration Plan

### S0. Freeze measurements and invariants

- Record production and test line counts separately.
- Record Context, Substitution, reindex, Claim, Derivation, HOTT action, and
  certificate allocation/probe counters for representative programs.
- Add a small derivation for `lambda x. lambda y. x` which proves the exact
  projection/reindex authority used by weakening.
- Add a negative resource-ready fixture asserting that structural projection
  alone is not a discard certificate.
- Run all prototype scripts, not only the book and HOTT tests.

Exit condition: repeatable baseline results and no semantic change.

### S1. Introduce checked store views

- Add read-only kernel view and mutable construction view.
- Migrate HOTT APIs without changing records or action identity.
- Delete repeated store-bundle NULL/orientation checks only after view
  construction validates them.
- Retain narrow direct APIs for low-level unit tests.

Exit condition: identical artifacts, calculus fingerprint, and test results.

### S2. Extract common authority-edge infrastructure

- Move Context-formation certificate ownership out of `hott.h`.
- Migrate Substitution certificate storage to the common infrastructure.
- Keep bridge/action certificate payloads typed.
- Add forged cross-kind and cross-store ID tests.
- Do not change proof rules in this phase.

Exit condition: no feature owns a second writable copy of Context or
Substitution typing authority.

### S3. Canonicalize typed reindex authority

- Add one immutable reindex action/result API.
- Route Weakening and `SUBSTITUTION_REINDEX` construction through it.
- Route Pi, constructor, handler, and continuation instantiation through it.
- Route HOTT endpoint creation through it.
- Keep conversion-based naturality as a separate certificate over canonical
  square sides.
- Delete direct binder-substitution paths which claim the same action.

Exit condition: every accepted reindexing Derivation names one exact action;
no validator independently reconstructs an unrecorded action.

### S4. Migrate Derivation premises to edge arenas

- Introduce immutable proposition identity for scoped and accepted premises.
- Migrate candidate premise arrays.
- Migrate accepted Derivation premise arrays.
- Move rule-specific parameters to typed rule-data payloads.
- Update interning keys and collision tests.
- Update proof replay and closure-rank calculation.

Exit condition: no fixed maximum-premise arrays remain in candidate or
accepted Derivation identity.

### S5. Perform one artifact schema migration

- Freeze the in-memory authority and premise-edge layouts.
- Define the next artifact schema.
- Serialize exact premise edges and semantic action authority IDs.
- Rebuild graph-cons indices after readback.
- Reject old wire records in the kernel reader.
- Update link, relocation, forgery, and deterministic round-trip tests.

Exit condition: one strict schema, deterministic artifact bytes, and no
compatibility fields in accepted graph nodes.

### S6. Consolidate deterministic HOTT outcome plumbing

- Extract common result-state and deterministic budget validation.
- Keep Goal, Candidate, Work, Action request, Action result, and Certificate
  identities separate.
- Remove repeated residual publication code.
- Preserve the compiler-local HOTT artifact boundary until its publication
  phase explicitly changes it.

Exit condition: no action identity is mutable and no Goal is reused as an
Action request.

### S7. Descriptor-direct HOTT recursion

- Extend the authoritative type-former descriptor.
- Share child-action startup and result propagation.
- Preserve independent rule-specific formation and replay.
- Add coverage checks requiring every admitted type former to declare its
  ordinary reindex, HOTT Type action, HOTT Term action, and resource hook
  status.

Exit condition: adding a type former cannot silently omit one semantic action.

### S8. Reduce tests through checked fixture builders

- Add builders for Context extensions, accepted Claims, Operations, bridges,
  action requests, and expected outcomes.
- Keep raw-record forgery tests separate and explicit.
- Replace repeated successful setup, not malformed input construction.

Exit condition: test intent is shorter and malformed-record coverage is not
weakened.

### S9. Extract runtime environment implementation

- Move Operation runtime environment and machine code out of `ast.c`.
- Replace whole-environment copying.
- Add combined instantiation and captured-resumption tests.
- Freeze hooks for future consumption/multiplicity state.

Exit condition: no change to accepted static Context or proof semantics.

### S10. Add resource typing only as a separate project

- Select the grade algebra and logical policy.
- Define usage proposition and certificate identity.
- Add usage composition for typed Substitution.
- Make Weakening and Contraction require policy-specific evidence.
- Extend HOTT action with explicit resource inputs.
- Add runtime consumption semantics only after static preservation laws pass.

Exit condition: unrestricted programs retain their current meaning, while
linear/affine rejection is derived from explicit usage evidence.

## 14. Validation Matrix

Every consolidation phase must retain or add the following checks.

### 14.1 Base CwF laws

```text
id action
composition action
projection orientation
extension typing
comprehension pullback
reindex identity
reindex composition
unchanged shared Term reuse
hash-collision distinction
```

### 14.2 Weakening and substitution

```text
one-level weakening names exact projection
multi-level weakening names canonical composition
forged unrelated projection is rejected
source/target reversal is rejected
reindexed classifier mismatch is rejected
same subject under unrelated Context is rejected
```

### 14.3 Premise graph

```text
zero-, one-, two-, and maximum-premise rules
shared premise proposition
multiple Derivations for one Claim
scoped premise does not become accepted Claim
edge reordering changes ordered rule identity
cycle and forward-edge rejection
hash collision does not merge Derivations
```

### 14.4 HOTT action

```text
empty and non-empty Context bridge
identity and composition action
left and right endpoint naturality
Type action endpoint Context
Term action endpoint instantiation
ADT, Pi, pure Comp, Thunk, Match, and IH cases
host primitive and Universe remain explicitly unsupported where specified
effectful computation remains residual
conversion exhaustion remains residual
no successful action extends DefEq
```

### 14.5 Resource readiness before resource semantics

```text
usage is absent from Context and Term identity
projection construction does not create discard permission
reindex cache does not create resource evidence
authority payload has a reserved typed extension path, not a generic integer
Runtime Environment remains a distinct store
```

### 14.6 Future resource laws

```text
identity usage action
Substitution usage composition
linear rejection of unused binding
affine acceptance of unused binding
affine rejection of duplication
unrestricted acceptance of weakening and contraction
HOTT endpoint and witness usage preservation
resumption multiplicity preservation
```

## 15. Deletion and Code-Size Policy

Each migration phase must include a deletion manifest. A phase is not complete
when it only adds a new abstraction beside the old path.

Required deletion targets include:

- independent parent-chain weakening authority;
- direct substitution traversals which claim canonical reindex semantics;
- feature-owned Context/Substitution certificate allocation boilerplate;
- fixed Derivation premise arrays after edge migration;
- repeated HOTT result-state/profile/fingerprint validation;
- repeated successful HOTT test fixture setup; and
- fixed-array Runtime Environment copying after runtime extraction.

Do not set a line-count target before the migration diff exists. Some phases,
especially premise and artifact migration, may temporarily add code. The exit
criterion is net removal of old semantic paths and lower authoritative surface
area, not an arbitrary percentage.

Track separately:

```text
production implementation lines
public declarations
validator lines
artifact reader/writer lines
test fixture lines
raw forgery test lines
```

Independent validator code is not counted as accidental duplication merely
because it mirrors a constructor. It becomes accidental duplication only when
both paths independently reconstruct an authority which should have been
stored as an exact edge.

## 16. Stop Conditions

Stop and revise the consolidation if it requires any of the following:

- merging Context, Substitution, Term, Claim, Derivation, or Runtime value IDs;
- treating a hash match as semantic equality;
- making observation evidence part of kernel DefEq;
- storing Context, action, Claim, or resource-policy IDs in Term identity;
- deriving Weakening solely from the existence of a projection;
- making every structural Substitution resource-admissible;
- putting usage grades into Context identity;
- keying resource evidence only by `(term, substitution)`;
- accepting a certificate through the same unchecked procedure that created
  it;
- retaining old and new proof/Artifact layouts through parallel kernel paths;
- using insertion order as a proof of acyclicity or authority; or
- reducing code size by deleting forged-input or independent replay tests.

## 17. Expected Result

After S0-S9, before adding resource typing, the implementation should have:

1. one base-CwF reindex authority used by ordinary typing and HOTT;
2. typed certificate edges with common storage discipline and independent
   validators;
3. compact premise DAGs rather than maximum-sized arrays;
4. smaller HOTT APIs and shared deterministic outcome plumbing;
5. descriptor-directed action recursion with explicit unsupported cases;
6. a runtime environment implementation physically and semantically separate
   from static Context; and
7. an explicit extension point for usage evidence which does not weaken the
   current unrestricted calculus.

The future resource layer can then refine admissibility without rewriting the
base graphs:

```text
structural fact
  sigma : Delta -> Gamma

ordinary typed fact
  sigma preserves a Claim by reindexing

resource-sensitive fact
  sigma transforms U_Gamma to an admissible U_Delta under policy R
```

This preserves the current responsibility separation while reducing repeated
semantic authority and leaving linear, affine, graded, and unrestricted
interpretations open.
