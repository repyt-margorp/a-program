# V2-A1 Object-HOTT Artifact Implementation Plan

Date: 2026-08-09

Status: first-dimensional closed fragment implemented; higher-closure gate open

Repository baseline:

- branch: `main`;
- implementation baseline:
  `d4c3c3d57955925089cbdfdd47073965a7d178c6`;
- baseline subject: `Normalize persistent proposition references`;
- artifact format: v69;
- object-action manifest: `src/prototype/hott_fragment_v2.schema`;
- V2-O1, V3-SC1, and V3-PC1: complete at the baseline above;
- baseline verification recorded by PC1: all 16 prototype scripts, examples
  01-07 and 09, optimized `-Werror`, ASan/UBSan, deterministic v69 output,
  artifact append/link, CwF/reindex laws, HOTT actions, and scoped-premise
  forgeries passed.

This is the executable and progress-manageable plan for V2-A1, the artifact
publication stage selected by
`2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`.
It supersedes the earlier content of this file.

The audit at baseline `d4c3c3d` found that v69 still treats an artifact as a
serialized input to the solver rather than as a serialized accepted proof
graph. A1 cannot be implemented correctly by adding observation tags and root
records to that path. The authority boundary, publication projection, and
append mapping must be corrected first.

The `V2-A1` identifier is retained for continuity. It does not restore V2 data
structures. A1 consumes the consolidated V3 Proposition/Claim/Derivation graph
and must not reintroduce copied scoped propositions, cached Claim pointers,
fixed premise arrays, feature-owned certificate stores, or value/computation
TermDB duplication.

## 1. Objective

Publish and replay object-language identity evidence admitted by the finite
O1 fragment. Compiler-local relational action may construct that evidence but
is not itself the persistent identity language:

```text
Gamma |- R : Universe
Gamma |- p : R
```

The persistent object evidence is:

- shared ordinary TermDB nodes reached by generated identity families and
  their inhabitants;
- ContextDB objects and SubstitutionDB morphisms referenced by evidence;
- interned Propositions;
- accepted Claims naming those Propositions;
- all exact accepted Derivations and their ordered premise edges;
- rule-specific semantic payloads;
- Universe provenance that names exact accepted Claims; and
- a small interface root table selecting intentionally published identity
  Claims.

The artifact must not serialize the compiler process that discovered evidence:

- observation goals;
- candidates and candidate premise edges;
- work queues and frontier state;
- fuel budgets or consumed step counts;
- residual records;
- action requests, mutable results, and certificates;
- bridge records and bridge certificates; or
- derived caches such as closure rank and lookup indices.

The names `OBSERVATION_TYPE_FORMER` and `OBSERVATION_WITNESS_FORMER` do not by
themselves establish that this evidence is an identity type. Until A1.T0 is
complete, this document uses "relation" for the implemented four-argument
former and reserves "identity/equality" for the structure whose elimination
and computation laws have been justified.

## 2. Governing Theory and Project Policy

### 2.1 Publication is a projection, not a second elaboration

Artifact construction is a projection from a compiler session to a rooted
accepted object graph:

```text
compiler session
  -- select public roots and exact dependency closure -->
publication graph
  -- serialize --> artifact
```

The projection forgets allocation order, unused search state, and caches. It
does not forget proof-rule identity, Context, Substitution, premise order, or
the distinction among Proposition, Claim, and Derivation.

Categorically, this is intended to preserve the object-language structure
already accepted by the kernel. Contexts remain objects and Substitutions
remain morphisms. Publication relocation must preserve source, target,
composition, and binding identity. It must not reconstruct scope from surface
names.

### 2.2 Publication normalization is not judgemental equality

Dense relocation and deterministic ordering change serialization IDs only.
They do not:

- identify different Terms by DefEq;
- identify Bool and Two by shape;
- collapse different typed Operations sharing one erased CoreTerm;
- choose one Derivation and discard another;
- add a Proposition equality to kernel conversion; or
- erase higher witnesses by proof irrelevance or set truncation.

Two objects may receive different source IDs and deterministic publication IDs
without becoming equal in the object theory.

### 2.3 Rule identity is semantic data

`APP_ELIM`, `MATCH_ELIM`, `INDUCTION_HYPOTHESIS_ELIM`, and
`COMPUTATION_FOLD_ELIM` may be encodable into a smaller calculus, but the
current kernel validates different theorems for them. v70 must preserve the
exact proof kind and its premise contract.

Infrastructure for bounds checks, ID relocation, premise iteration, and wire
parsing may be shared. Rule validators must not be merged merely to reduce C
code.

### 2.4 No migration-cost exception

The following are not acceptable reasons to retain a transitional design:

- changing all callers is large;
- an offset is easier than a relocation map;
- existing tests only inspect shape;
- rebuilding candidates happens to recover a similar Claim;
- v69 compatibility is convenient; or
- sparse source IDs make the writer simpler.

When the final authority model requires an API break, change every prototype
caller in the same phase. There is no backward-compatibility requirement.

### 2.5 One shared CBPV Term graph

A1 must not create separate value-side and computation-side graph tags or
parallel TermDBs. CBPV polarity remains represented by ordinary Terms,
classifiers, `RETURN`, `THUNK`, `FORCE`, computation folds, operation requests,
and accepted typing evidence in one shared graph.

### 2.6 Logical relation is necessary evidence, not yet identity

The current action has the form:

```text
Gamma |- a : A
--------------------------------
Gamma^R |- a^R : A^R(a_0, a_1)
```

This is a relational/parametricity fundamental action. It is useful and is
also a legitimate starting point for Higher Observational Type Theory. The
error would be to identify the currently generated relation immediately with:

```text
Id A x y
```

merely by setting both endpoint types to `A`. That identification requires an
explicit account of type equality, transport/lifting, and the computation
rules of the supported type formers. In particular, the current generic
witness introduction requires pure conversion of its endpoints and therefore
only supplies a reflexivity-like fragment; structural preservation rules do
not by themselves prove that all and only observationally indistinguishable
programs are related.

Accordingly, A1.T0 fixes the current interpretation as:

```text
current Obs(A_0, A_1, x_0, x_1) = provisional RelAlong relation application
Id_(Delta.A)^delta2(x_0,x_1)     = target object identity, not yet represented
```

The selected relation `R` or telescope identity `delta2` is missing from the
current four-argument Term. No wire format or public surface syntax may rely on
the stronger `Eq` interpretation. Reflexivity, symmetry, and transitivity are
necessary consequences but do not close this gap; transport/lifting and
type-former computation are the acceptance criteria.

### 2.7 Adjudication of the relation-versus-observational-equality feedback

The review objection that the original `OBSERVATION_*` implementation is a
parametricity action rather than an observational equality is correct. It does
not, however, invalidate the separate `IDENTITY_TYPE_COMPUTATION` path added by
A1.T0. The three notions have distinct contracts:

Update 2026-08-10: the compiler-local implementation identifiers have been
renamed from `OBSERVATION_TYPE`/`OBSERVATION_WITNESS` to
`RELATION_TYPE`/`RELATION_WITNESS`. Numeric Term tags 32/33 and proof-kind
ordinals are unchanged. The rename is semantic documentation, not a new object
identity rule.

| mechanism | implemented meaning | may be a v70 identity root |
| --- | --- | --- |
| `RELATION_TYPE` / `TERM` action | fundamental action of a selected logical relation | no |
| `IDENTITY_TYPE_COMPUTATION` | type-directed computation of an object identity family | yes, only in the admitted closed fragment |
| contextual equivalence | indistinguishability by every admitted closing observation | not an object or search result; a metatheoretic soundness target |

The first path proves preservation of a relation. It does not prove equality
between arbitrary independently supplied programs. The second path computes a
family and then relies on ordinary kernel checking of an explicit inhabitant.
For example, the non-convertible Bool functions `lambda b. b` and
`lambda b. match b { false -> false; true -> true; }` are accepted by a focused
kernel fixture only because an ordinary pointwise Lambda/Match proof inhabits
the computed Pi identity family. The fixture eliminates the arbitrary input
identity and replays its accepted graph. The executable weaker constant-true
test independently confirms that an arbitrary-endpoint witness can be checked
and cannot be reused at a constant-false endpoint. Neither path manufactures
identity from endpoint conversion or from the same-source parametricity action.

The word *observational* in this plan therefore means that identity computes
according to the observed type former: ADT identity is an indexed ADT and
pure-function identity is pointwise. It does not mean that the compiler defines
identity by quantifying over all program contexts. A later adequacy theorem must
show that inhabitants of the computed identity families are indistinguishable
by the admitted A Program eliminators. Completeness or full abstraction is not
claimed.

This distinction also limits the status of A1. The generated ADT identity and
pure pointwise Pi rules form an executable closed homogeneous fragment. They
do not constitute complete Higher Observational Type Theory while Universe
identity, `glue`/bisimulation, transport/lifting, dependent telescope identity,
and effectful identity remain residual. Artifact version 70 may persist only
that closed fragment and its exact scope. In particular, the current finite
Universe correspondence is an internal construction aid, not a public identity
root; it must not be serialized as object equality until recursive higher
bisimulation evidence is represented and replayed by the kernel.

### 2.8 Decision after the relation-versus-equality review (2026-08-10)

The review is accepted. A1 does not define object identity by inhabitation of
the compiler-local logical relation, and it does not define identity by a
finite search for indistinguishable endpoints. The implementation contract is:

```text
RELATION_TYPE / RELATION_WITNESS
  compiler-local internal-parametricity substrate

IDENTITY_TYPE_COMPUTATION(A, x0, x1)
  computes the exact object family Id_A(x0,x1) for an admitted type former

ordinary Term p plus accepted HAS_TYPE Claim
  establishes p : Id_A(x0,x1)
```

This agrees with the published HOTT direction in the following limited sense:

1. every admitted type has a specified identity family;
2. degeneracy/reflexivity and term action must inhabit that exact family;
3. the family computes according to the observed type former; and
4. identity evidence remains proof-relevant and can itself be an endpoint of a
   higher identity.

It is not yet a complete HOTT core. In particular, identity at the Universe
must yield a correspondence equipped with transport and path lifting;
dependent telescope action must use that lifting; and the construction must be
coherently closed at higher dimensions. Symmetry and composition are required
derived operations and tests, but they do not replace these requirements.

### 2.9 Primary-rule recheck and implementation order (2026-08-11)

The selected HOTT presentation is governed by the following dependency, not by
promoting a general logical relation to equality:

```text
Id_Delta(delta0,delta1)
  selects Id_(Delta.A)^delta2(a0,a1)
  and every typed a supplies ap_a(delta2)

Id_A(x0,x1)
  is the empty-telescope specialization

refl_A(x)
  is the empty-telescope specialization of ap_x

Id_U(A0,A1)
  computes to a proof-relevant correspondence plus equivalence/lifting data
```

The current `RELATION_TYPE` and `RELATION_WITNESS` nodes implement only the
logical-relation action used while constructing this structure.  No artifact
identity root may contain them, and no successful relation action may be
converted into an object identity Claim.

The authoritative non-DefEq Bool identity versus exhaustive-Match fixture is
retained: it checks an ordinary pointwise Lambda/Match inhabitant of the
type-former-computed Pi identity.  It is an introduction example, not a
contextual-equivalence decision procedure.

The remaining T0 implementation order is therefore fixed as follows:

1. dependent Pi computes its result identity from the codomain family's exact
   action on `x2 : Id_A(x0,x1)`;
2. Universe correspondence projections become complete transport/lifting
   evidence, with replayable higher coherence;
3. identity computation acts on identity witnesses without proof-irrelevance
   or endpoint-only interning;
4. symmetry and composition are derived as ordinary Terms and tested against
   the exact computed families; and
5. only then may the admitted artifact identity fragment be widened.

Reflexivity, symmetry, and transitivity are necessary conformance properties,
but they are not the definition of observational identity: any equivalence
relation has them.  Type-former computation, exact telescope indexing,
dependent lifting, and higher closure are the authority criteria.

The non-DefEq matrix contains both the eta-Match proof and the
arbitrary-endpoint constant-function check. Neither is by itself an adequacy
proof connecting internal identity with contextual equivalence. The eta proof
is now published in the authoritative compilation Judgement/Operation graph
and registered as an exact artifact identity root; it no longer uses an
isolated fixture arena. Rejection of witness reuse at the constant-false
endpoint establishes only that the
implemented rules do not manufacture that witness; global uninhabitance still
depends on a canonicity/normalization argument for the admitted fragment.

Therefore artifact v70 persists an explicitly scoped, closed homogeneous
identity-computation fragment. The compiler-local relation action is excluded
from identity roots. Promotion to a complete HOTT identity ABI remains gated
on Universe correspondence, transport/lifting, dependent telescope action,
and higher coherence.

## 3. Current Codebase Audit

### 3.1 Accepted artifact readback is currently re-publication

`prototype_artifact_read_text_graph()` in `src/prototype/ast.c` reads Claims
and Derivations, then reconstructs `derivation_candidates` and
`candidate_premises`.

`prototype_judgement_validate_proofs()` in `src/prototype/typing.c` immediately
calls `prototype_judgement_ground_claims()`. Grounding calls
`judgement_rebuild_claim_derivations()`, recomputes closure rank, compacts
Claims/Derivations, and may assign different Claim IDs.

`read_artifact_from_path()` in `src/prototype/read_file.c` therefore performs:

```text
read accepted graph
  -> reconstruct candidates
  -> ground/publish again
  -> search by proposition/term shape
  -> rewrite export evidence Claim IDs
```

This is invalid for A1. An artifact root must denote the exact accepted Claim
carried by the artifact. Reader validation must replay that graph in place,
not ask the solver to publish a replacement.

### 3.2 Append currently downgrades accepted evidence to candidates

`prototype_artifact_append_graph()` in `src/prototype/ast.c` appends interned
Propositions and `derivation_candidates`, but does not append accepted Claims
and Derivations directly.

PC1 introduced:

```c
PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM
PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_PROPOSITION
```

This correctly made the transitional state explicit: source/readback evidence
can name a Claim, while appended evidence temporarily names a Proposition
until grounding creates a new Claim. It was a valid PC1 repair, but it must not
become the v70 design.

After A1.R2, export evidence and identity roots always name exact accepted
Claims. `EVIDENCE_REFERENCE_PROPOSITION`, shape-based rebinding, and
post-append re-grounding are deleted.

### 3.3 Claim, Proposition, and Derivation identities are independent

PC1 correctly separated these arenas. Nevertheless, current append logic uses
offsets for several proof references. An A1 append cannot derive a Claim ID
from a Proposition offset or assume source and target insertion positions.

It requires independent maps:

```text
proposition_relocation[source_proposition_id]
claim_relocation[source_claim_id]
derivation_relocation[source_derivation_id]
```

The maps may point at an existing exact target object after interning. Root and
Universe provenance relocation must use `claim_relocation`, never a shape
search.

### 3.4 v69 serializes derived closure ranks

Claims and Derivations currently write `closure_rank`. The reader accepts the
wire value, while `validate_proof_relation_coverage()` checks only ordering
inequalities. Grounding later recomputes ranks.

Closure rank is a cache derived from the premise DAG. It is not object evidence
and must not be wire authority. v70 omits it. Accepted-graph replay computes
the canonical minimal rank:

```text
rank(derivation) = 0                              when no Claim premises
rank(derivation) = 1 + max(rank(premise Claim))  otherwise
rank(Claim)      = min(rank(concluding derivation))
```

Ranks are computed as the least fixed point of these equations. A Claim or
Derivation that cannot enter that least fixed point is rejected. This is more
precise than rejecting every syntactic cycle: a Claim may have an independent
rank-0 derivation and another derivation that uses that already-grounded Claim.
What is forbidden is an unsupported cyclic component with no lower-rank
grounding. Overflow and Claims with no grounded concluding Derivation are also
rejected. Computed ranks may be cached in memory after validation.

### 3.5 v69 duplicates proof data on the wire

Current v69 Claims inline a full Proposition tuple. Premises inline scoped
Proposition tuples. Derivations also write a deduplicated `sources` list that
is derivable from Claim premise edges.

The fixed `artifact_rule_data_wire` emits seven fields with INVALID sentinels
for every rule even though in-memory rule data is a tagged union.

v70 must instead serialize each authority once:

```text
Proposition := semantic proposition tuple
Claim       := PropositionId
Premise     := ClaimId | ScopedPropositionId
Derivation  := ConclusionClaimId
               ProofKind
               ordered Premise[]
               proof-kind-specific payload
               optional admitted semantic action
```

The `sources` list, rank fields, expanded Claim proposition tuple, expanded
scoped tuple, and unused rule payload fields are removed.

### 3.6 Sparse source slots leak compiler history

`artifact_sparse_graph` preserves source slot counts and source IDs, fills
unreachable slots with holes, and writes both slot count and present count.
Unused candidate/action work can allocate Terms, Contexts, Substitutions, or
proof objects before publication. Even if those objects are unmarked, their
allocation can change slot counts and therefore artifact bytes.

The A1 determinism requirement cannot be met by adding observation roots to
this structure. A1 needs a dense `artifact_publication_graph` with relocation
maps for every serialized arena reachable from public roots.

At minimum the audit must cover:

- Terms, Match cases, case binders, IH frames, and computation-fold clauses;
- type declarations, parameters, constructors, readback field references, and
  type expressions;
- Contexts and Substitutions;
- Operations, operation cases, and operation-fold clauses if serialized;
- Propositions, Claims, Derivations, and accepted premise edges;
- Universe nodes, edges, levels, constraints, and Claim provenance; and
- interface exports, dependencies, and identity roots.

Leaving one reachable arena sparse can preserve allocation-history leakage.

### 3.7 Compiler-local relation tags are not object identity syntax

`src/prototype/term.h` defines relation term tags 32 and 33. They are closed
nullary formers used through APP spines.

`src/prototype/judgement.h` defines O1 proof kinds 33 through 42:

```text
RELATION_TYPE_FORMATION
RELATION_WITNESS_INTRO
SUBSTITUTION_REINDEX
RELATION_CONSTRUCTOR_WITNESS
RELATION_RETURN_WITNESS
RELATION_THUNK_WITNESS
RELATION_APP_WITNESS
RELATION_LAMBDA_WITNESS
RELATION_MATCH_WITNESS
RELATION_INDUCTION_HYPOTHESIS_WITNESS
```

These tags and proof kinds describe compiler-local relational action. v70 may
serialize them only when independently reachable as ordinary compiler
evidence; an identity root must reject them as family or witness authority.
Object identity uses ordinary generated ADT, Pi, Thunk, constructor, Lambda,
APP, and Match forms and rules. Unknown future integers remain rejected.

### 3.8 Current roots do not reach unnamed HOTT objects

`artifact_mark_roots()` starts from named term/type/constructor exports and
compile metadata. Completed O1 actions need not have surface names.

`artifact_mark_accepted_claim()` already walks Claim, all concluding
Derivations, exact Claim premises, scoped Propositions, Terms, constructor
views, Match motives, IH frames, Contexts, and Substitutions. Its semantic
closure behavior should be reused, but its result must feed dense publication
relocation rather than sparse copying.

### 3.9 Root extraction must validate exact action ownership

An identity-type-computation certificate contains
`identity_type_has_type_claim_id`. An optional object proof has its own exact
`HAS_TYPE` Claim whose classifier is that computed identity family.

Extraction must validate READY result/certificate/request ownership and exact
Type/Term request linkage before returning Claim IDs. Artifact code must not
inspect HOTT candidate/work arenas.

## 4. Target Authority and Wire Contract

### 4.1 In-memory accepted graph contract

After candidate publication:

- a Proposition is immutable and interned by its full semantic tuple;
- a Claim contains exactly one valid Proposition ID;
- a Derivation contains one conclusion Claim ID, exact proof kind, ordered
  premise edges, rule payload, and admitted semantic action;
- a premise edge contains exactly one of Claim ID or scoped Proposition ID;
- one Claim may have multiple distinct Derivations;
- duplicate exact Derivations may be interned, but different Derivations are
  retained; and
- closure rank and lookup indices are derived caches.

### 4.2 Interface root record

Add:

```c
struct prototype_artifact_identity_root {
	uint32_t source_type_claim_id;
	uint32_t identity_family_has_type_claim_id;
	uint32_t witness_has_type_claim_id;
	int computation_rule;
};
```

The witness ID may be `PROTOTYPE_INVALID_ID` for a family-only result. The
source `IS_TYPE` Claim and identity-family `HAS_TYPE` Claim are mandatory.
Exact duplicate quadruples are interned. `computation_rule` is persistent
identity-family computation provenance, not compiler action history: readback
must recheck that the ordinary family graph is exactly the family computed from
the source type by that rule. In particular, merely labelling an arbitrary Pi
type as an identity root is invalid.

The root must not duplicate Context, subject, classifier, endpoints, bridge,
category, proof kind, or action request/result IDs. Those belong to the accepted
closure or transient compiler execution. One computation-rule discriminator is
necessary because generated ADT identity and pointwise Pi identity deliberately
reuse ordinary Term forms rather than adding one identity-specific Term tag per
rule.

Roots are selected only after the accepted Claim graph is frozen. Claim IDs held
by a transient HOTT action are not stable across a later candidate-publication
compaction; registering roots and then returning to solver publication is a
phase error.

### 4.3 Dense v70 graph grammar

The v70 graph has explicit dense sections and no in-range holes:

```text
propositions N
proposition ID KIND AUTHORITY_KIND AUTHORITY_ID CONTEXT OPERATION SUBJECT CLASSIFIER

claims N
claim ID PROPOSITION_ID

derivations N
derivation ID CONCLUSION_CLAIM PROOF_KIND PREMISE_COUNT
  premise claim CLAIM_ID
  | premise scoped PROPOSITION_ID
  [rule-specific payload]
  [semantic substitution SUBSTITUTION_ID]
```

No rank, `sources`, expanded proposition tuple, candidate, or unused union arm
is serialized.

Proof-kind-specific payload grammar:

- no payload for rules whose theorem is fixed by conclusion and premises;
- constructor pattern: owner view, constructor index, field index;
- constructor spine: owner view;
- IH elimination: Match, motive, case index, field index;
- semantic substitution action only for `CONTEXT_WEAKEN` and
  `SUBSTITUTION_REINDEX`.

The final schema must enumerate every existing proof kind, not only HOTT kinds,
and specify whether its payload is empty or one of the forms above.

### 4.4 Dense publication ordering

Publication assigns IDs independently of source allocation history.

Required algorithm:

1. Collect exact transitive closure from the ordered public interface roots.
2. Partition dependencies by arena and record all cross-arena edges.
3. Assign dense IDs by deterministic dependency-respecting traversal.
4. For unordered collections, order by a semantic structural key independent
   of source IDs.
5. Resolve key collisions by full structural comparison, not hash equality.
6. Handle cyclic strongly connected components explicitly; source ID is not an
   allowed tie-breaker.
7. Rewrite every reference through its arena relocation map.
8. Validate the relocated publication graph before writing bytes.

For ordered syntax, child-field/case/premise order is semantic and preserved.
For unordered stores such as the set of concluding Derivations, sort by exact
structural proof key while retaining every distinct derivation.

If an SCC has a true graph automorphism, canonical labeling must produce the
same bytes for isomorphic rooted publication graphs. Do not silently fall back
to source allocation IDs. The implementation may use a simpler deterministic
root traversal only after tests and an invariant proof establish that the
relevant arena cannot contain ambiguous SCCs.

### 4.5 Root validity

An identity root is valid only when:

1. family ID names a present accepted `HAS_TYPE` Claim whose classifier is an
   admitted Universe;
2. its subject is an ordinary computed identity family and is not a
   `RELATION_TYPE_FORMER` or `RELATION_WITNESS_FORMER` spine;
3. all Claims and Derivations in the exact closure replay;
4. optional witness ID names a present accepted `HAS_TYPE` Claim;
5. its witness is an ordinary constructor/Lambda/APP/Match/CBPV proof Term,
   never the endpoint-only relational witness former;
6. witness classifier is exactly the rooted identity-family subject;
7. both roots come from the same validated computation/proof result rather
   than being paired by a subject/classifier search; and
8. all Context/Substitution scope objects are present and valid.

No root introduces DefEq or equality reflection.

## 5. Mandatory Pre-A1 Refactors

These phases are part of A1 delivery. Schema work must not begin by preserving
the old authority mistakes.

### A1.T0: Relation-to-identity theory gate

Status: closed homogeneous identity-family computation, typed degeneracy,
type-level Pi/APP action, and the first dependent Universe-selected fiber
prototype complete; higher Universe coherence, general dependent-field action,
effectful identity, symmetry, and composition pending

The current O1 implementation must be reclassified and tested before its Terms
and proof rules become persistent artifact authority.

The later `IDENTITY_TYPE_COMPUTATION` work closes family computation and the
ordinary object introduction interface for the explicitly admitted closed
fragment.  `OBJECT_TERM` action constructs an ordinary Term inhabiting the
exact family returned by `IDENTITY_TYPE_COMPUTATION`; compiler-local `TERM`
action still produces only `RELATION_WITNESS` evidence and cannot introduce
object identity.  The non-convertible Bool-function fixture is therefore a
valid non-trivial introduction test, but neither it nor the current generic
action constitutes a contextual-equivalence decision procedure or a complete
HOTT calculus.

The governing audit is
`2026-08-09T19-46-59-A1-T0-RELATION-TO-HIGHER-OBSERVATIONAL-IDENTITY-AUDIT.md`.
It found that the current four-argument form omits the selected relation
between endpoint types, while the endpoint-only witness interns all proof
constructions with common endpoints as one Term. Consequently tags 32/33 are
relational-action scaffolding and are not accepted identity syntax.

The implementation claim remains deliberately narrower than “Higher
Observational Type Theory implemented”.  The current object fragment computes
closed homogeneous identity families for ordinary ADTs, pure Return/Thunk, and
pure nondependent Pi, and constructs typed diagonal/pointwise witnesses through
ordinary Terms and Derivations.  Symmetry and composition are required derived
operations, while transport/lifting and their higher coherence are required
before the fragment can be called a complete HOTT core.  The old
`RELATION_*` action remains a parametricity/logical-relation mechanism and is
not evidence for these object identity roots.

The code audit first found that the arbitrary-left/right observation planner
could select non-diagonal ADT and Pi candidates while execution only supported
`OBS_DIAGONAL`. A subsequent prototype now recursively executes conversion and
same-constructor ADT candidates, but still materializes the endpoint-only Term
`RELATION_WITNESS(left,right)`. This closes plumbing only; it does not close
the object-proof gap because distinct proof constructions with common endpoints
still intern to one Term. T0 therefore requires ordinary candidate-specific
proof Terms. Planning or endpoint-token materialization is not identity
publication evidence.

- [x] State whether the four-argument former is a general logical relation, a
      heterogeneous identity type, or a relation generated by an identity of
      types. Do not use these as interchangeable descriptions.
- [x] Specify homogeneous identity separately and determine whether
      `Id A x y := Rel A A x y` is derivable or requires additional data.
- [x] Specify equality between types. If it is represented by a relation, state
      the equivalence/bisimulation condition that distinguishes type identity
      from an arbitrary relation. The selected contract is a proof-relevant
      correspondence plus an `IsBisimulation`-strength payload; A1 does not
      persist such identities until that payload has an object representation.
- [x] Specify transport/lifting for dependent families and its computation on
      reflexive/diagonal evidence. If a different eliminator replaces `J`, give
      its typing and computation rules explicitly. The contract is recorded as
      `trr`/`trl` plus `liftr`/`liftl`; the current closed homogeneous fragment
      has no transport operation, so every use requiring it is residual.
- [x] Give the relation/identity computation rule for every admitted O1 type
      former, including Pi, ADT constructors, Match, Return, and Thunk. Mark
      unsupported effectful forms as residual rather than empty. A1 admits
      ordinary ADTs with nullary/nondependent fields, pure Return/Thunk, pure
      nondependent Pi, and the indexed Match needed to check their witnesses.
      Dependent fields/codomains, Universe identity, requests, folds, and
      effectful computations are residual and cannot become roots.
- [x] Define `EqC(C,M0,M1)` as
      `IdV(ThunkType(C),Thunk(M0),Thunk(M1))` before Pi identity. `PI` is a
      computation-function classifier and
      `Pi(Bool,Bool)` is not a valid executable-function surrogate. Reuse the
      shared TermDB tags; do not add value/computation copies of APP/LAMBDA/PI.
- [x] Define pure `EqC(Comp(empty,A),RETURN(v0),RETURN(v1))` by an indexed
      identity constructor for the thunked endpoints with field evidence
      `IdV(A,v0,v1)`. Neutral, exhausted, effectful, and unresolved cases are
      residual. The `THUNK_RETURN` identity-computation rule now generates and
      replays this ordinary three-field constructor, accepts its ordinary
      constructor-spine proof, rejects distinct Bool endpoints, and rejects a
      forged result index. Its result identity is not restricted to a generated
      ADT declaration: a nested `THUNK(RETURN(function-value))` uses the direct
      pointwise Pi identity family as the third field. Generation, certificate
      replay, and TypeDeclaration validation share one validator for this
      choice rather than maintaining a HOTT-local duplicate rule.
- [x] Select the finite representation: ordinary anonymous generated indexed
      TypeDeclarations for ADT identity, and ordinary Pi/Lambda graph forms for
      pure function identity. Do not introduce per-former identity Term tags.
- [x] Add a first-class index telescope to `TypeDeclarationDB`, separate from
      uniform parameters. `IdT` endpoint arguments are indices and its higher
      constructors specialize them; do not encode endpoints as parameters just
      to reuse the current constructor cache API.
- [x] Explain why witnesses remain proof-relevant and how relation witnesses
      themselves enter the next observational dimension.
- [x] Construct the strong positive non-conversion test. Show whether
      `lambda b. b` and `lambda b. match b { true -> true; false -> false; }`
      admit an explicit ordinary Lambda proof when the neutral Match is not
      DefEq to `b`. The kernel must check the proof; it must not infer identity
      merely from endpoint search. The nondependent pure Pi identity family is
      now computed directly as a nested thunked Pi proof type and its
      certificate replays without semantic-arena mutation. The focused test
      rejects endpoint conversion, eliminates the indexed Bool identity,
      returns the corresponding generated computation-identity constructor,
      and replays the complete Derivation graph read-only. Its family/witness
      Claims are published in the authoritative compilation graph and
      registered as an artifact identity root. The test exposed and removed a
      Pi-formation wiring error:
      the classifier of `THUNK(codomain-family-lambda)` is now
      `ThunkType(Pi(domain,Comp(empty,Universe)))`, not
      `ThunkType(Comp(empty,Universe))`.
- [x] Implement indexed Match elimination before the Pi litmus proof. Matching
      `xR : IdBool x0 x1` must refine each branch from the exact higher
      constructor result indices; ordinary field-binder extension alone is
      insufficient. The nullary generated-identity fragment now records one
      canonical Context substitution per branch premise, reindexes the body
      and motive through it, and rejects a forged substitution during accepted
      replay. Constructors with fields remain outside this completed fragment.
- [x] Construct the first negative witness-reuse test. The constant-true
      function and the constantly-false function do not accept the same
      witness. The stronger discrimination test must show that the identity
      Bool function and the constantly-false function do not receive such a
      witness merely because both are well typed or preserve an unrelated
      relation. The implemented test specializes the same Pi identity
      computation to the constant-true and constant-false endpoints, verifies
      that this classifier is not convertible to the positive classifier, and
      rejects selection of the positive ordinary proof Term there. Together with
      the indexed Bool test that rejects every constructor at a distinct
      endpoint fiber, this exercises the discriminating `true/false` branch.
      This is a kernel rejection test, not a proof that the negative identity
      family is uninhabited by every possible Term. That stronger statement
      requires a canonicity/normalization argument for the admitted fragment
      and remains a metatheoretic obligation.
- [x] Separate kernel introduction from optional bounded synthesis: identity
      family computation plus checking an ordinary proof Term is normative;
      candidate search is elaborator/compiler policy and never direct
      authority.
- [x] State and scope the adequacy claim connecting internal identity evidence
      to the observations available in A Program. Do not claim completeness or
      full abstraction unless proved.
- [x] Adopt telescope-indexed identity as the governing judgement: the fiber
      identity is indexed by exact endpoint substitutions and their Context
      identity, homogeneous identity is its empty-telescope instance, and
      reflexivity is the corresponding `ap` action.
- [x] Demonstrate that the existing Context bridge is sufficient to represent
      the required telescope identity; otherwise replace it before persisting
      any identity root. The bridge now distinguishes empty, parametric, and
      object-identity semantics. An `OBJECT_IDENTITY` extension stores the
      exact computed fiber and ordinary witness Claim, so constant-family
      non-empty telescopes are executable. A merely parametric bridge remains
      inadmissible as `delta2`; general dependent extensions still require
      lifting before publication.
- [x] Produce a rule correspondence table between the retained kernel rules and
      the selected Higher Observational Type Theory calculus.
- [x] Keep Term tags 32/33 and their proof rules compiler-local as
      parametricity/relation-action infrastructure, and name them
      `RELATION_TYPE`/`RELATION_WITNESS` rather than observational identity.
      They are not v70 object identity syntax. Computed identities and proofs
      use ordinary generated ADT, Pi, constructor, Lambda, APP, and Match graph
      forms.
- [ ] Connect degeneracy to computed identity families. For every admitted
      source Claim `Gamma |- a : A`, construct an ordinary object Term
      `refl_A(a)` and an exact Claim against the family computed for `(a,a)`.
      Do not use `RELATION_WITNESS(a,a)` as that Term.
      Canonical ordinary-ADT values are now implemented: nullary Bool,
      non-recursive field spines, and recursive Nat spines construct ordinary
	  generated identity constructors recursively. The shared constructor-spine
	  Judgement API records exact field and field-identity premises. A neutral
	  value now constructs an ordinary dependent Match with motive
	  `lambda z. IdA(z,z)`, then publishes a Conversion Claim to the exact
	  generated family. Nullary Bool, field-bearing Box, and recursive Nat pass
	  through ordinary branch Contexts, constructor-spine Claims, and guarded
	  `INDUCTION_HYPOTHESIS_ELIM` evidence. The remaining admitted type formers are
	  still required before this item is complete; recursive identity remains
	  ordinary evidence, not a fallback witness token. Pure `THUNK(RETURN(v))`
	  degeneracy is now also executable: it obtains the accepted Return/Thunk
	  premises, recursively constructs the ordinary ADT witness for `v`, and
	  applies the generated three-field thunk-identity constructor. The first Pi
	  pointwise case, `THUNK(LAMBDA(x, RETURN(x)))`, constructs the exact nested
	  three-Lambda witness from `x0`, `x1`, and `xR` and checks every CBPV
	  Return/Thunk boundary. Pure Lambda bodies now dispatch through a recursive
	  typed value action: a bound variable uses the exact input-identity Claim and
	  ordinary ADT constructor bodies recursively construct generated identity
	  constructors. Match, APP, and nested Lambda bodies still require their
	  respective object actions below.
- [ ] Connect term action to computed identity families. Given an exact
      telescope identity and a typed source Term, construct an ordinary object
      Term inhabiting the exact target family. The result must be independently
      checkable after HOTT action state is discarded. The canonical identity
      Lambda is the first executable case. Its implementation established two
      mandatory invariants for the remaining cases: read the typed binder/body
      occurrences from the accepted Lambda Derivation rather than assuming the
      alpha-interned Core Lambda stores the same binder IDs; and instantiate the
      computed endpoint family by a certified Context substitution plus Claim
      reindexing, rather than rebuilding type formation from a substituted Term.
      The identity certificate retains template binders for read-only replay,
      but endpoint specialization may intern to an alpha-equivalent family with
      different physical Binding IDs. Proof construction now reads binders from
      that concrete family and never compares them to the template IDs.
      Context variables are now type-former independent: their action
      instantiates the exact certificate family and returns the identity Claim
      introduced with that binding. This covers both generated ADT identity and
      direct pointwise Pi identity. A typed Lambda in a non-empty object bridge
      now composes the outer endpoint substitutions through its `x0/x1/xR`
      Context, reindexes inherited identity Claims, and constructs an ordinary
      pointwise Lambda witness. A pure raw-CBPV body
      `APP(FORCE(f),x)` now recursively acts on `f` and `x`, applies the
      pointwise function witness to `x0/x1/xR`, and sequences intermediate
      results through ordinary zero-clause `COMPUTATION_FOLD_ELIM` DAGs.
      Generated intermediate function types are formed through ordinary
      structural typing; convertible specialized families use explicit
      `CONVERSION` Claims. A nullary ordinary Match now recursively acts on
      its accepted scrutinee and branch Claims, eliminates the indexed
      scrutinee-identity witness with an ordinary `MATCH_ELIM`, and converts
      the result to the exact endpoint identity family. Its permanent fixture
      uses a motive application convertible to `Bool`, so carrier selection,
      endpoint instantiation, and the final witness all retain explicit
      `CONVERSION` DAGs rather than relying on Term-ID equality. Field-bearing
      Match object action, arbitrary computation bodies, effects, and
      recursive nested Lambda dispatch remain to be generalized.
      Pure `THUNK(RETURN(v))` now also acts
      in a non-empty object bridge: the accepted THUNK/RETURN Derivations expose
      `v`, its ordinary object action supplies `v0/v1/vR`, and the generated
      three-field thunk-identity constructor is checked against the exact
      certificate-instantiated endpoint family. Closed degeneracy is the
      diagonal instance of this same object-term action; the old separate
      THUNK_RETURN degeneracy implementation and its generated-ADT-only
      recursion have been removed. Value action also follows an accepted
      `SUBSTITUTION_REINDEX` premise by composing its semantic substitution
      with both endpoint substitutions. Thus weakening a closed Lambda into a
      body Context no longer hides its ordinary Lambda proof or requires a
      shape-based Claim search. Object term action is now also an immutable
      `OBJECT_TERM` request/result/certificate, distinct from compiler-local
      `TERM`. Its key fixes the source Claim, `OBJECT_IDENTITY` bridge, and
      exact `IDENTITY_TYPE_COMPUTATION` request. Its certificate records both
      reindexed endpoint Claims, the instantiated ordinary identity-family
      Claim, and the ordinary witness Term/Claim. Universe-variable action and
      subsequent `R` projection exercise this path. The checkbox remains open
      for the listed Match, arbitrary computation, effect, and nested-Lambda
      cases, not because object `ap` still lacks action identity.
- [x] Before generalizing term action beyond the closed homogeneous fragment,
      reject every request whose selected `delta2` is represented only by the
      old parametric relation bridge. A non-empty telescope action is not
      implemented merely by threading endpoint substitutions; it requires the
      exact object telescope identity that selects each dependent fiber.
      Bridge certificates now carry an explicit semantic kind:
      `EMPTY_IDENTITY`, `PARAMETRIC_RELATION`, or
      `OBJECT_IDENTITY`. This prevents graph shape or a relation-action
      certificate from being interpreted as object identity. A non-empty
      homogeneous `OBJECT_IDENTITY` extension is now constructed from an exact
      `IDENTITY_TYPE_COMPUTATION` certificate and ordinary Context-binding
      witness Claim. Relation and identity bridges for the same source Context
      coexist under distinct semantic keys; forged certificate exchange is
      rejected. General dependent fiber lifting remains residual.
- [x] Keep identity introduction type-directed. Do not add a rule from bounded
      endpoint observation, successful relation search, conversion cache
      membership, or reflexive/symmetric/transitive relation metadata directly
      to an identity Claim. Such services may only synthesize an ordinary proof
      Term that independently checks against the computed family. The positive
      non-DefEq constant-true/constant-Match Bool function example is checked
      by an ordinary pointwise Lambda witness, while reuse of that witness for
      the constant-false endpoint is rejected. The stronger identity/eta-Match
      proof also eliminates an arbitrary `IdBool(x0,x1)` witness with
      branch-local index refinements and is published as an authoritative
      identity root. `T0.R8` records the normative boundary.
- [x] Test the computation laws for the admitted fragment, including
      `ap_id(p) = p`, `ap_const(p) = refl`, and
      `ap_f(refl(a)) = refl(f a)`, at least up to the kernel conversion
      relation supported by that fragment. Variable action returns the exact
      Context identity witness; constant thunk action interns to the exact
      closed degeneracy constructor spine; and the pointwise witness for the
      Bool identity function is executed through three pure CBPV applications
      at `false`, `false`, and `refl(false)`, yielding the exact thunk
      degeneracy witness for `f false`.
- [x] Implement the first degenerate identity-of-identity computation without
      proof irrelevance. An accepted ordinary proof
      `p : IdBool(false,false)` is now an endpoint of the type-directed family
      `Id_(IdBool(false,false))(p,p)`, and ordinary degeneracy constructs its
      constructor witness. The rule selects exact nullary constructors of this
      indexed fiber and never applies compiler-local `RELATION_*` evidence.
      This is only the square whose four boundary edges are degenerate; it is
      not the general two-dimensional action.
- [ ] Close identity under its own type computation for every admitted family
      before calling the whole fragment Higher. The degenerate Bool case is the
      nullary indexed base case. The object bridge for the full endpoint
      Context constructs six explicit boundary faces of a square. Acting on
      `IdBool(x0,x1)` now computes an eight-index center family from those six
      faces and the two new proof endpoints. Its ordinary constructor inhabits
      the coherent all-`false` square and is rejected after one corner is
      changed to `true`. A `Box Bool` constructor now expands its source field
      to four corner values, four edge identities, and one square witness; its
      nine-field higher constructor spine checks normally. Dependent fields and
      dimensions above this square still require the lifting described next.
- [ ] Implement Universe identity as a proof-relevant correspondence equipped
      with the finite fibrancy interface required by this fragment. The first
      object representation must expose `trr`, `trl`, `liftr`, and `liftl`, or
      an exactly equivalent ordinary A Program telescope, and its constructor
      and eliminator computations must be independently checkable by the
      kernel. An arbitrary `RELATION_TYPE` value is not Universe identity.
	      This item precedes general dependent lifting: for `b : B(a)`, the bridge
	      between `B(a0)` and `B(a1)` is selected by the object action of `B` on
	      `aR`; endpoint conversion cannot manufacture it.
	      The first finite representation is now implemented as an ordinary
	      generated indexed ADT with one seven-field constructor carrying `A0`,
	      `A1`, `R`, `trr`, `trl`, `liftr`, and `liftl`. Its read-only structural
	      validator and diagonal `Bool` witness support internal type-action tests.
	      It is deliberately rejected as a v70 identity root: without the
	      higher-recursive `id` field, the four operational fields admit total
	      correspondences that collapse distinguishable Bool values. Forging rule
	      6 onto an admitted root is rejected. `Id_U(Bool,Bool)` is classified by the successor
	      Universe rather than the element Universe. The indexed Match refinement
	      now supports the full seven-field constructor telescope. Ordinary
	      `MATCH_ELIM` Terms project `R`, `trr`, `trl`, `liftr`, and `liftl`; after
	      substituting the diagonal `Bool` glue witness, all five projections
	      satisfy their constructor iota laws. No C-side constructor-spine
	      projection is used. This item remains open for recursive bisimulation
	      coherence; the finite representation is scaffolding, not an admitted
	      public Universe identity fragment.
	      The selected full representation is not an eight-field positive ADT.
	      Canonical type formers continue identity dimension-by-dimension through
	      object action, while arbitrary `glue` requires a negative/coinductive
	      `IsBisim` interface whose recursive `id` observation supplies the next
	      fiber coherence on demand. A1 keeps rule 6 out of the artifact ABI until
	      that interface and its read-only replay exist.
- [ ] Implement transport and path lifting for the admitted Universe-identity
      representation. At minimum, `trr`/`trl` must compute on the selected
      correspondence, `liftr`/`liftl` must inhabit the selected fiber identity,
      and the diagonal correspondence must satisfy the executable identity
      laws used by Context extension. Unsupported Universe levels or
      effect-bearing families remain residual rather than acquiring a generic
      witness.
      Before these projections can be implemented as ordinary object Terms,
      generalize indexed Match refinement from nullary constructors to
	      field-bearing constructors. In a `glue` branch, the refinement
      substitution must map the outer endpoints and proof to the constructor
      result indices and full constructor spine under the branch field
      telescope. This is shared dependent-Match infrastructure, not a
	      Universe-specific C projection. The current nullary helper cannot type
	      the seven-field branch and must not be bypassed by inspecting a canonical
	      constructor spine in C.
	      The field-bearing refinement and all five ordinary projections are now
	      implemented. `liftr` and `liftl` build their dependent codomains from the
	      ordinary `R` and transport projection Claims. The remaining work in this
	      item is to consume those lifts while extending a dependent Context; the
	      existence of a projection alone does not yet establish telescope action.
	      The dependent-Context experiment is now closed for general correspondence
	      witness Terms. A projection Match is first checked under the generic
	      Context `Gamma,x0:U,x1:U,p:IdU(x0,x1)`, abstracted by three ordinary
	      Lambdas, and applied to the actual `A0`, `A1`, and `p`. Certificate replay
	      follows the exact `APP_ELIM`, `LAMBDA_INTRO`, `MATCH_ELIM`, conversion, and
	      reindex Derivation DAG. It does not search by Core shape, inspect a
	      constructor spine in C, or pretend that an arbitrary scrutinee Term is a
	      Context variable for indexed refinement.
	      The finite seven-field correspondence also lacks the higher-recursive
	      `id`/bisimulation-coherence field used by the HOTT model to close
	      correspondence under identity. It must be represented as ordinary
	      proof-relevant object data before this item, generic box filling, or the
	      complete-HOTT claim can close.
- [ ] Implement the dependent telescope action and then use it for indexed
      generated identity declarations. A later field/fiber identity must be
      selected by the identity of all preceding fields and indices; it must not
      be reconstructed by comparing the two endpoint classifier Terms. The
      constant-family base law is already executable: a closed carrier weakened
      into a non-empty object bridge reuses its generated identity declaration
      under `CONSTANT_FAMILY_LIFT`, corresponding to
      `ap_const(delta2) = refl`. Two such extensions construct the complete
      six-face Context boundary. The existing `INDEXED_HIGHER_LIFT` consumes
      that boundary and computes square centers for nullary and nondependent
      field constructors only. It is a base case, not the implementation of
      dependent lifting. Keep constructors with a field classifier that uses a
      preceding field or index outside the admitted identity fragment until the
      selected fiber action and its ordinary witness Claim have been built.
      The object-telescope driver now recursively traverses Context parents and
      extends an `OBJECT_IDENTITY` bridge for every READY identity-computation
      certificate. The first dependent rule handles `A : Universe, b : A` by
      projecting `R` from the exact Universe witness and using `R(b0,b1)`.
      Variable fibers may refer to that same `A` at any later telescope depth:
      generation and certificate replay apply the immutable `OBJECT_TERM`
      action to the type expression `A : Universe` in the current object
      bridge, then project `R` from the resulting Universe-identity witness.
      Binding identity is consumed by ordinary variable action; there is no
      Universe-fiber-specific ancestor search. The permanent
      `A : Universe, b : A, c : A` test prevents reintroducing an accidental
      immediately-previous-binding restriction or a second binder-remapping
      mechanism.
	      Type-level nondependent `Pi(A,Universe)` now computes an ordinary
	      pointwise identity family without CBPV wrappers. General type application
	      recursively acts on its function and argument Claims, applies that
	      pointwise witness by ordinary `APP_ELIM`, and obtains an exact Universe
	      identity witness. The permanent `B : Pi(A,Universe), a : A, b : B(a)`
	      fixture projects `R` from that witness and extends the object identity
	      Context with fiber `R(b0,b1)`. Valid unsupported fibers still return
	      residual evidence and are never filled by `RELATION_TYPE`. Other
	      type-level expressions, genuinely dependent Pi codomains, and higher
	      Universe coherence remain open.
	      The first-dimensional ordinary-ADT generator now accepts dependent
	      constructor fields. It uses the complete object bridge of the source
	      field Context, validates left/right classifiers under the prior
	      BindingId correspondence, and consumes the exact five-argument object
	      action spine for the selected dependent fiber. The permanent dependent
	      `Box` fixture constructs and checks the resulting six-field witness.
	      This does not widen `INDEXED_HIGHER_LIFT`: dependent constructor squares
	      remain residual until dependent box filling and recursive Universe
	      coherence are implemented. A beta-redex in a type expression is handled
	      by `PURE_TYPE_WHNF` plus conversion retargeting; raw unapplied type-level
	      Lambda action now uses the same object-action path as CBPV Lambda without
	      adding a value/computation duplicate Term tag. Neutral type-level Match
	      action and genuinely dependent Pi remain open.
- [ ] Derive or construct symmetry and composition for every admitted root
      family, and check them as ordinary object Terms. These are consequences
      and regression tests, not the definition of identity. Do this after
      object action on general type expressions, dependent Pi identity,
      dependent field lifting, and Universe higher coherence. Do not duplicate
      inversion/composition recursively in C for each ADT former; dimensional
      symmetry plus box filling is the shared HOTT construction.
- [ ] Keep Universe levels, effectful correspondence, and every dependent
      transport/lifting case not covered by the finite implementation above
      residual. Until the Universe correspondence and dependent telescope gate
      are implemented, describe the persistent result as the closed
      homogeneous observational-identity computation fragment, not as complete
      Higher Observational Type Theory.

Exit gate: a reviewable theory note, executable positive and negative tests,
generic object `refl`/`ap` for every admitted root former, higher action on the
resulting identity families, and an explicit decision that tags 32/33 have no
persistent object-identity meaning exist. `artifact_v70.schema` and identity
roots may be exercised inside `src/prototype/` as an implementation fixture,
but they are not a frozen or promotable identity ABI until this gate closes.
The gate closes only the explicitly listed closed fragment; it is not a claim
that Universe identity, general dependent transport, or effectful HOTT has
been implemented.

The wire root rules are also intentionally narrower than the compiler action
rules. `CONSTANT_FAMILY_LIFT` and `INDEXED_HIGHER_LIFT` depend on a non-empty
object-identity bridge and its full boundary. Because v70 omits bridges and
action certificates, these rules cannot be reconstructed from the root tuple
and are rejected as root labels. A closed concrete identity fiber may instead
become the source carrier of a new `ORDINARY_ADT` root. This persists an exact
closed identity-of-identity computation without pretending that the general
eight-boundary action was serialized.

Primary theory anchors for this gate:

- *Towards Higher Observational Type Theory* (TYPES 2022), which begins with a
  logical-relations/telescope calculus but derives homogeneous identity and
  reflexivity as special cases:
  <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>
- Narya's HOTT account, which distinguishes parametricity from HOTT and makes
  transport/lifting and a bisimulation/equivalence condition on type relations
  explicit: <https://narya.readthedocs.io/en/latest/hott.html>
- *Internal Parametricity, without an Interval* for the relational layer:
  <https://arxiv.org/abs/2307.06448>
- *Observational Equality, Now* for the separation of substitutive,
  function-extensional propositional equality from decidable definitional
  equality: <https://doi.org/10.1145/1292597.1292608>

### A1.R0: Separate candidate publication from accepted replay

Status: complete in the working tree; full prototype suite passed on 2026-08-09

- [x] Split the current `prototype_judgement_validate_proofs()` lifecycle into
      explicit APIs with non-overlapping effects, for example:
      `prototype_judgement_publish_candidates()` and
      `prototype_judgement_validate_accepted_graph()`.
- [x] Keep candidate-to-Proposition/Claim/Derivation construction only in the
      compiler publication path.
- [x] Make accepted replay read-only with respect to semantic graph identity.
      It may rebuild lookup indices and derived ranks.
- [x] Ensure accepted replay never calls
      `judgement_rebuild_claim_derivations()` and never reads candidate arrays.
- [x] Change every compiler call site to publish candidates explicitly before
      accepted replay.
- [x] Change every artifact read/link call site to replay the accepted graph
      directly.
- [x] Remove reader reconstruction of `derivation_candidates` and
      `candidate_premises`.
- [x] Remove `artifact_exports_have_accepted_claims(..., rebind=1)` behavior
      and any shape search that changes an imported Claim reference.
- [x] Remove `prototype_universe_rebind_provenance()` from accepted artifact
      readback. Universe provenance must validate the exact relocated Claim ID;
      it must not search for a replacement after replay.
- [x] Compute canonical minimal closure ranks from accepted premise edges;
      reject unsupported cyclic components, uncovered Claims, and overflow.
- [x] Test that accepted Claim/Derivation IDs and edge targets are byte-for-byte
      unchanged by replay.
- [x] Test that malformed accepted graphs cannot become valid by republishing a
      subset of their data.

Exit gate: the compiler can publish candidates, the reader can replay accepted
evidence, and neither path impersonates the other.

### A1.R1: Replace sparse copying with a publication graph

Status: complete for the admitted v70 object graph

- [x] Replace `artifact_sparse_graph` with
      `artifact_publication_graph` or an equivalently explicit data structure.
- [x] Store a dense destination arena and source-to-publication relocation map
      for every serialized arena listed in section 3.6.
- [x] Build the closure from ordinary exports plus explicit identity roots.
- [x] Preserve Context-object and Substitution-morphism dependencies exactly.
- [x] Preserve Binding IDs structurally; do not rebuild binders by names or De
      Bruijn indices.
      On 2026-08-10 the whole-arena `binder_offset` import was removed.
      Publication now collects only Binding identities referenced by the
      rooted closure and assigns one dense relocation shared by Term,
      Match-case binder, Context, TypeExpr, parameter declaration, Operation,
      computation-fold clause, and residual-obligation records. Allocating an
      unused Binding before all rooted construction no longer changes v70
      bytes. This proves removal of Binding-slot holes; it does not yet prove
      canonical ordering under arbitrary permutations of independently
      allocated reachable objects.
- [x] Define deterministic ordering for every arena and document whether it is
      ordered syntax, an interned set, or an SCC-bearing graph.
      Public exports, identity roots, Type parameter/constructor telescopes,
      Match cases, computation-fold clauses, and proof-premise edges are
      ordered syntax: their order is part of the public graph. Term, Type,
      TypeExpr, Context, Substitution, Proposition, Claim, and Derivation
      arenas are physical interned graphs. Publication traverses ordered roots
      and semantic child positions, emits parents/dependencies in the required
      structural order, and sorts accepted Derivations after relocation.
      Match/IH frame back-edges form the only SCC-bearing Term component and
      are traversed through the fixed Match-case/binder order. Universe data is
      recollected from the dense public graph rather than copied in source-slot
      order. Finally, the first dense publication is republished through the
      same pipeline; this removes the remaining first-encounter order inherited
      from the compiler arenas without adding per-arena remap policies.
      Tests now preserve bytes across unused successful work, preallocated
      Binding IDs, reversed independent Type allocation, and a complete
      reversal of the reachable Derivation arena.
- [x] Add full structural collision resolution for all hash/fingerprint keys.
      The 2026-08-10 audit confirms that Term canonical keys and Type code-shape
      fingerprints are prefilters only. Every matching-key link/intern path
      follows with `prototype_term_view_shape_equal_for_link()` or
      `prototype_type_declaration_representations_equal()` before sharing an
      object. Context, Substitution, and Judgement intern tables likewise check
      complete keys after bucket selection. `core_view_representation_check`,
      `term_identity_frame_check`, and `context_category_check` inject
      collisions and retain distinct objects. A larger digest would improve
      distribution but would not replace this structural collision rule.
- [x] Remove source slot counts and hole emission from the v70 publication
      model.
- [x] Verify unused successful HOTT work that allocated Terms, Contexts,
      Substitutions, Propositions, or proof objects cannot change output bytes.
      The 2026-08-10 perturbation executes an additional successful Bool
      identity computation after allocating an unrelated Binding, Context,
      projection, reindexed Claim, and legacy relation Term. The second rooted
      publication remains byte-identical to the first.
- [x] Keep publication normalization separate from TermDB interning and DefEq.
- [ ] Measure per-file LOC before and after; do not judge correctness by LOC,
      but record whether sparse adapters were actually deleted.

Exit gate: two compiler sessions with the same rooted accepted object graph
produce the same dense publication graph regardless of unused allocation
history.

### A1.R2: Append accepted graphs directly

Status: accepted-graph append complete; final determinism audit remains in R1

- [x] Change `prototype_artifact_append_graph()` to consume and produce
      accepted Proposition/Claim/Derivation graphs.
- [x] Allocate independent Proposition, Claim, Derivation, and premise
      relocation maps.
- [x] Intern exact Propositions in the target and record each source mapping.
- [x] Intern/map Claims through relocated Proposition IDs.
- [x] Relocate each exact Derivation conclusion, ordered premise edge,
      rule-specific payload, and semantic action.
- [x] Retain all distinct Derivations; only exact duplicate Derivations may
      intern.
- [x] Relocate Universe source Claim provenance through `claim_relocation`.
- [x] Relocate ordinary exports through `claim_relocation`; identity roots
      are relocated as exact Claim pairs; witness roots remain gated on generic
      object identity introduction.
- [x] Delete `PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_PROPOSITION` after all
      callers use exact accepted Claims.
- [x] Delete post-append grounding and shape-based evidence canonicalization.
- [x] Do not use Claim, Proposition, or Derivation offsets as a substitute for
      maps.
- [x] Replay the appended accepted graph without changing its identities.
- [x] Run accepted replay and the complete artifact flow after the Claim-only
      evidence change.

Exit gate: append is a structure-preserving map of accepted object graphs, not
a conversion back into solver input.

## 6. A1 Implementation Phases

### A1.0: Freeze the corrected v70 manifest

Status: complete in the working tree

- [x] Create `src/prototype/artifact_v70.schema` from the corrected authority
      contract, not by mechanically extending v69.
- [x] Enumerate all dense sections and reference directions.
- [x] Exclude relational-action term tags 32/33 and proof kinds 33-42 from
      identity roots; preserve ordinary graph forms reached by identity Claims.
- [x] Specify direct Proposition, Claim, Derivation, and premise-edge records.
- [x] Specify proof-kind-specific payloads.
- [x] Omit rank, `sources`, candidates, holes, and expanded tuples.
- [x] Specify the dense identity-root table.
- [x] State explicitly that action/work/bridge/certificate state is absent.
- [x] Increment `PROTOTYPE_ARTIFACT_FORMAT_VERSION` from 69 to 70.
- [x] Recompute `PROTOTYPE_CALCULUS_FINGERPRINT` from exact schema bytes.
- [x] Reject v69 before interface parsing.
- [x] Delete, rather than retain, temporary v69 adapters.

Exit gate: the schema describes one accepted dense object graph and no
transitional authority state.

### A1.1: Add identity-root ownership and extraction

Status: complete for all 16 currently admitted family and witness roots

- [x] Add `prototype_artifact_identity_root` arena/count/capacity to the
      interface.
- [x] Extend interface initialization and all fixture allocators.
- [x] Add exact source/family/witness/rule interning.
- [x] Add HOTT-side extraction from validated READY identity-type computation
      results. Do not accept the legacy relation `TERM` action as a witness.
- [x] Verify result/certificate/request ownership and identity computation
      linkage.
- [x] Verify the exact computed identity `HAS_TYPE` Claim and optional ordinary
      witness `HAS_TYPE` Claim.
- [x] Permit identity-family-only roots with INVALID witness.
- [x] Persist and replay-check the source type Claim and identity computation
      rule; reject an arbitrary ordinary graph merely labelled as identity.
- [x] Require root selection after final accepted-graph publication.
- [x] Keep the serialized root independent of HOTT action/work state.

Exit gate: unrelated action results cannot be combined and repeated exact
registration produces one root.

### A1.2: Integrate root closure into publication

Status: complete for the currently admitted ordinary object witnesses

- [x] Mark source type, identity family, and optional witness through the generic
      accepted-Claim closure walker.
- [x] Preserve every distinct accepted Derivation concluding a rooted Claim.
- [x] Include exact Claim premises and scoped Proposition dependencies.
- [x] Include Match cases, binders, IH frames, Contexts, Substitutions, and
      rule payload Terms.
- [x] Include imported endpoints reachable only through object-identity
      evidence. A closed imported Bool endpoint, absent from ordinary exports,
      is retained through its generated `IdBool(n,n)` family and ordinary
      neutral-Match witness. Its qualified dependency is discovered from the
      dense rooted publication graph rather than from compiler-local relation
      action state.
- [x] Exclude unrelated successful actions and search state.
- [x] Convert marks to dense relocation through A1.R1, not
      sparse source slots.

Exit gate: publication is exactly ordinary export closure union explicit
observation-root closure.

### A1.3: Implement v70 term and accepted-proof wire

Status: complete for the admitted ordinary witness matrix

- [x] Write/read generated indexed identity declarations and ordinary
      constructor/Pi/Lambda/APP/Match witness graphs through existing sections.
- [x] Reject compiler-local relation-action former tags in identity roots.
- [x] Audit every term offset/relocation/canonicalization switch reached by the
      current family-only root fixture.
- [x] Write/read dense Propositions independently of Claims.
- [x] Write/read Claims as direct Proposition references.
- [x] Write/read Derivations with direct ordered premise references.
- [x] Implement proof-kind-specific payload parser/writer for the admitted wire
      proof kinds.
- [x] Admit the ordinary proof kinds reached by an identity-root Claim; do not
      grant artifact authority to `RELATION_*` compiler-action derivations.
- [x] Do not reconstruct candidates after read.
- [x] Rebuild only indices and closure-rank cache, then run accepted replay.
- [x] Reject unknown tags, proof kinds, payload fields, and semantic actions.

Exit gate: each O1 object form and proof rule independently round-trips as
accepted evidence.

### A1.4: Write/read and validate roots

Status: ADT, Thunk/Return, and pointwise Pi family/witness roots write, read,
validate, and aggregate with ordinary accepted type-formation closure

- [x] Emit dense roots in interface order.
- [x] Parse with exact count/capacity and duplicate checks.
- [x] Validate references only after accepted graph construction.
- [x] Validate exact source/family/witness kinds and proof closure for generated
      ADT identity roots.
- [x] Reject forged source Claim and unsupported/forged computation rule.
- [x] Add structural replay validation for pointwise Pi identity roots before
      admitting them from untrusted artifacts.
- [x] Add structural replay validation for generated pure Thunk/Return identity
      declarations, including exact field telescope and endpoint indices.
- [x] Replace fixture-only Thunk/Pi `IS_TYPE` Claims with ordinary accepted
      type-formation DAGs before publishing those roots.  Structural validation
      alone is not artifact authority.
- [x] Validate witness-to-family linkage as an exact ordinary `HAS_TYPE` Claim:
      same Context and classifier equal to the rooted family Term. Do not invent
      a special premise edge from every typing derivation to the family-formation
      Claim; ordinary constructor/Lambda/APP rules do not have that premise.
- [x] Validate the family classifier after dense relocation/readback rather than
      guessing it from source IDs.
- [x] Verify identity-family-only roots do not fabricate witnesses.

Exit gate: forged IDs, wrong kinds, wrong linkage, missing evidence, duplicate
roots, and malformed dense references are rejected.

### A1.5: Append/link identity roots

Status: complete for generated ADT, Thunk/Return, pointwise Pi, dependency-only,
and aggregate cases in the admitted fragment

- [x] Add root capacity preflight.
- [x] Relocate all three root Claim IDs through exact `claim_relocation` and
      preserve the computation rule.
- [x] Preserve INVALID witness IDs.
- [x] Preserve dependencies reachable only through evidence. Dependency
      collection reruns over the dense rooted publication graph, and the
      publication arena reserves room for dependencies that were absent before
      root closure. The imported neutral-Bool root retains
      `fixture_7.fixture_8`; unrelated accepted work still cannot leak imports.
- [x] Replay the complete linked accepted graph.
- [x] Verify no canonical Term key, TypeView shape, or Proposition offset is
      used to guess a root Claim.

Exit gate: multiple artifacts preserve every exact root and derivation after
append/link.

### A1.6: End-to-end closed observational-identity matrix

Status: admitted first-dimensional kernel/wire matrix complete; general higher
closure remains residual and does not widen the v70 root calculus

- [x] Generated Bool identity has no constructor at a distinct-constructor
      fiber; do not persist the relation-only `RELATION_*` family.
- [x] Bool diagonal identity family and ordinary constructor witness.
- [x] Zero-field and multi-field generated identity constructors and ordinary
      witnesses.
- [x] Recursive Nat constructor witness.
- [x] Pi pointwise family. The family computes read-only and its generic and
      concrete Lambda witness roots survive write, read, and aggregation.
- [ ] Lambda and dependent APP witness. Nondependent pure raw-CBPV
      `APP(FORCE(f),x)` is executable and tested through ordinary
      `FORCE_ELIM`, `APP_ELIM`, zero-clause `COMPUTATION_FOLD_ELIM`, and
      `CONVERSION` DAGs. Dependent codomain lifting remains.
- [x] Return and Thunk witness. Typed ordinary constructor witnesses, forged
      endpoint rejection, write/read, and append/aggregation pass.
- [x] Neutral Match witness with pattern binders. Closed imported Bool and the
      earlier non-empty fixtures use an ordinary dependent Match witness.
- [x] Recursive Match/IH witness. Recursive Nat degeneracy persists ordinary
      Match and guarded `INDUCTION_HYPOTHESIS_ELIM` evidence.
- [x] Nullary identity-of-identity preserves the ordinary Bool-identity proof
      Term as an endpoint, computes the exact indexed fiber, constructs ordinary
      degeneracy, and publishes both roots without `RELATION_*`.
- [ ] General identity-of-identity remains residual for indexed generated
      constructors with fields. It must not collapse proof Terms by proof
      irrelevance or accept them through compiler-local relation action.
- [x] Shared Core under distinct typed Operations remains distinct at
      Claim/root boundary. The permanent artifact-flow fixture gives
      `identityBool` and `identityNat` the same alpha-interned Core Lambda while
      retaining different classifiers, Operation authorities, and accepted
      Claims. Readback rejects replacing an export's Operation/Claim with its
      shared-Core sibling, and rejects replacing an APP premise by the Claim
      owned by the other typed Operation. Identity roots likewise contain exact
      Claim IDs rather than Core keys, so this invariant carries through the
      root closure instead of being reconstructed from Term identity.
- [x] Imported endpoint/classifier through a root-only path. The endpoint is
      not an ordinary term export; write/read and aggregate retain its exact
      namespace-aware dependency through root closure.

Exit gate: every admitted O1 form survives write, read, append, link, and full
accepted replay.

### A1.7: Adversarial and determinism audit

Status: complete for the admitted v70 fragment

- [x] Missing or out-of-range Proposition, Claim, Derivation, Term, Context,
      Substitution, and Universe references.
- [x] Wrong source/family Claim kind in generated ADT root.
- [x] Unsupported computation rule presented as an identity root.
- [x] Relation-action Claim pair presented as an identity root.
- [x] Wrong witness classifier or unrelated computed identity Claim.
- [x] Missing, reordered, or wrong-kind premise.
- [x] Match/IH/computation-fold derivation replaced by APP merely because it is
      computationally encodable.
      A closed `lambda x. identity x` degeneracy root retains ordinary
      FORCE/APP/zero-clause computation-fold evidence. A neutral Nat
      degeneracy root retains ordinary dependent Match and guarded IH
      evidence. The adversarial reader test relabels each of proof kinds 12,
      14, and 16 independently as APP and requires rejection.
- [x] Forged scoped premise Context, subject, or classifier.
- [x] Invalid proof-kind-specific payload or semantic action.
- [x] Direct cyclic Claim dependency and uncovered rooted Claim.
- [x] Indirect cyclic Claim component or rank overflow. The artifact test now
      finds an existing two-Derivation dependency structurally and inserts the
      reverse edge without relying on fixed Claim or Derivation numbers;
      accepted replay rejects the resulting ungrounded component. Rank
      computation separately rejects `UINT32_MAX - 1 + 1`; constructing that
      many accepted Claims is not a practical dynamic fixture, so the overflow
      branch is retained as a reviewed defensive bound rather than simulated
      by trusting a serialized rank (v70 does not serialize ranks).
- [x] Compiler-local observation former/proof kind presented as persistent
      identity authority, and unknown persistent proof kind.
- [x] v69 rejection with no fallback parser.
- [x] Byte-identical output under different candidate/work/action allocation
      histories yielding the same rooted accepted graph.
- [x] Byte-identical output when unused successful HOTT actions allocate extra
      Terms, Contexts, Substitutions, Propositions, or proofs.
      The perturbation includes an unrelated successful
      `IDENTITY_TYPE_COMPUTATION`, its generated graph, and a reindexed
      accepted Claim/Derivation; none is reachable from the public roots.
- [x] Confirm no READY state, residual, fuel, graph revision, certificate, or
      HOTT action fingerprint occurs in artifact bytes.

Exit gate: bytes are a function of the public rooted accepted graph and module
interface, not compiler history.

### A1.8: Exit audit and handoff

Status: verification complete in the working tree; commit/LOC record pending

- [x] Run `make` and `make reader`. Passed again on 2026-08-11.
- [x] Compile all touched prototype C with `-Wall -Wextra -Werror`.
      The REPL, reader, and focused HOTT binaries pass on 2026-08-10.
- [x] Run all prototype test scripts. All 16 scripts pass again on 2026-08-11.
- [x] Run examples 01-07 and 09. All pass again on 2026-08-11; the permanent
      training `double` and `list_nat_match` programs also pass.
- [x] Run focused artifact tests under ASan/UBSan. The HOTT goal binary,
      including identity-root construction and artifact emission, passes with
      AddressSanitizer and UndefinedBehaviorSanitizer again on 2026-08-11.
- [x] Run `git diff --check`. Passed again on 2026-08-11.
- [x] Record deterministic v70 hashes for repeated and perturbed-history runs.
      Both `/tmp/a-program-hott-identity-root.apo` and
      `/tmp/a-program-hott-identity-root-perturbed.apo` produced
      `2d64defe38600f40e73bcb6bfa5b92e925f771783b8ea14de2641daba0496514`
      on 2026-08-11 and compare byte-for-byte equal. The same hash survives
      Binding preallocation, reversed independent Type allocation, and complete
      reversal of the reachable Derivation arena.
- [ ] Record final commit and verification evidence here.
- [ ] Record per-file before/after LOC and deleted adapters/helpers.
- [ ] Update V3 progress and select the next theory stage from remaining
      dependencies.

Exit gate: v70 is the only accepted format, all rooted object evidence replays
after link, and no compiler-local HOTT execution state persists.

## 7. Required File Changes

| File | Required change |
| --- | --- |
| `src/prototype/judgement.h` | declare separate candidate publication and accepted replay APIs; no new proof kinds |
| `src/prototype/typing.c` | split grounding/publication from replay; derive ranks; replay accepted graph without identity mutation |
| `src/prototype/ast.h` | v70 version, root arena, dense publication relocation structures/API, Claim-only export evidence |
| `src/prototype/ast.c` | replace sparse publication, direct accepted wire, direct accepted append, exact relocation maps, root closure/validation |
| `src/prototype/read_file.c` | remove candidate reconstruction/re-grounding and shape-based Claim rebinding |
| `src/prototype/hott.h` | declare exact publication-root extraction result/API |
| `src/prototype/hott.c` | validate READY result/certificate/request ownership and extract exact Claims |
| `src/prototype/term.h` | no new tags; verify frozen observation values 32/33 |
| `src/prototype/term.c` | no semantic changes expected; fix only exhaustive relocation/replay defects exposed by tests |
| `src/prototype/artifact_v70.schema` | authoritative dense accepted-object wire calculus |
| `src/prototype/test_artifact_flow.sh` | v70 roundtrip, append/link, forgery, no-republication, and determinism matrix |
| `src/prototype/test_hott_goal.sh` | shared tag/proof/fingerprint and root-extraction checks |
| `src/prototype/hott_goal_check.c` | reuse existing O1 fixtures where practical; do not add feature-owned persistence |
| V3 audit/progress Markdown | record A1 completion and handoff only after exit gate |

No accepted implementation under `src/` outside `src/prototype/`, `include/`,
parser/lexer inputs, or accepted build rules is in scope without explicit
approval.

## 8. Audit Traceability

| Current implementation | Current problem | Required phase | Required deletion/replacement evidence |
| --- | --- | --- | --- |
| `prototype_judgement_validate_proofs()` calls `prototype_judgement_ground_claims()` | replay mutates accepted identity through publication | A1.R0 | separate publish/replay callers; replay test preserves IDs |
| `prototype_artifact_read_text_graph()` rebuilds candidate arrays | accepted wire is downgraded to solver input | A1.R0, A1.3 | no candidate writes in reader |
| `artifact_exports_have_accepted_claims(..., 1)` | exact evidence is rebound by shape after grounding | A1.R0 | no rebind flag/path; exact Claim validation only |
| `prototype_universe_rebind_provenance()` after read | Universe Claim provenance is searched again | A1.R0, A1.R2 | exact Claim relocation and validation |
| `artifact_sparse_graph` preserves source slot counts | artifact bytes expose allocation history | A1.R1 | dense publication arenas and relocation maps |
| sparse builder copies all computation-fold clauses | unreachable computation data can leak | A1.R1 | closure-marked dense fold-clause arena |
| v69 writes Claim/Derivation `closure_rank` | derived cache becomes wire authority | A1.R0, A1.0, A1.3 | rank absent from schema/writer; least-fixed-point recomputation |
| v69 Claim embeds Proposition tuple | Proposition authority is duplicated | A1.0, A1.3 | independent Proposition section and direct Claim reference |
| v69 premise embeds scoped tuple | scoped Proposition authority is duplicated | A1.0, A1.3 | exact-one Claim/scoped-Proposition ID edge |
| v69 Derivation writes `sources` | premise information is serialized twice | A1.0, A1.3 | `sources` absent from schema/parser/writer |
| `artifact_rule_data_wire` emits all union arms | irrelevant INVALID payload becomes wire state | A1.0, A1.3 | proof-kind-specific grammar |
| `prototype_artifact_append_graph()` appends candidates | append is re-elaboration, not accepted graph mapping | A1.R2 | direct Proposition/Claim/Derivation append |
| append export evidence changes Claim to Proposition | transitional dual authority survives append | A1.R2 | Claim-only evidence enum/API |
| append uses arena offsets where identities may intern | source/target identity independence is lost | A1.R2 | separate relocation map tests for every arena |
| artifact roots exclude unnamed identity results | valid object evidence is unreachable | A1.1, A1.2 | exact identity root and closure tests |
| v69 read switches stop before the corrected object fragment | generated identity declarations and ordinary proofs cannot round-trip | A1.0, A1.3 | generated ADT/Pi identity matrix; compiler-local tags 32/33 and proof kinds 33-42 excluded from identity-root closure |

## 9. Required API Lifecycle

The completed compiler lifecycle must be explicit:

```text
AST lowering / fixed-point / solver
  -> candidate Propositions and Derivations
  -> prototype_judgement_publish_candidates
  -> accepted Proposition/Claim/Derivation graph
  -> prototype_judgement_validate_accepted_graph
  -> select exports and identity roots
  -> build dense artifact_publication_graph
  -> validate relocated accepted graph
  -> write v70
```

No candidate publication or Claim-ID compaction may occur between root
selection and dense publication. If more solver work is required, discard and
rebuild the root table after the next accepted-graph freeze.

The completed reader lifecycle must be different:

```text
read v70 dense graph
  -> bounds and exact-reference validation
  -> rebuild indices and derived closure ranks
  -> prototype_judgement_validate_accepted_graph
  -> validate exports, Universe provenance, and identity roots by exact IDs
```

The reader must not call candidate publication.

The completed append lifecycle is:

```text
source accepted graph + target accepted graph
  -> independent arena relocation/interning maps
  -> direct accepted graph append
  -> exact export/root/provenance relocation
  -> accepted replay
```

Append must not create candidates.

## 10. Characterization Matrix

| Object | Persistent Term root | Persistent Claim root | Persistent compiler state |
| --- | --- | --- | --- |
| computed identity family | ordinary generated ADT or Pi/Thunk graph | exact `IS_TYPE` Claim | none |
| family-only identity | ordinary identity-family Term | exact `IS_TYPE` Claim | none |
| witness | ordinary constructor/Lambda/APP/Match Term | exact `HAS_TYPE` plus selected identity Claim | none |
| Lambda/APP witness | shared Terms and scoped evidence | exact accepted derivation closure | none |
| Match/IH witness | Match/case/frame Terms | exact rule-specific closure | none |
| Return/Thunk witness | ordinary CBPV Terms | exact rule-specific closure | none |
| residual/unsupported action | none | none | none |

Sharing one CoreTerm does not merge typed Operations or Claims. Computational
encodability does not merge proof rules.

## 11. Stop Conditions and Rejected Shortcuts

Stop implementation and revise this plan if any phase requires:

- a Claim ID recovered by searching subject/classifier shape;
- a Claim ID calculated from a Proposition ID or common offset;
- converting accepted Derivations into candidates during read or append;
- assigning object meaning to serialized closure rank;
- serializing HOTT action/certificate/work state;
- retaining v69 parsing or a compatibility remap;
- using raw source allocation ID as a deterministic-order tie-breaker;
- dropping a distinct accepted Derivation to simplify canonical output;
- splitting the shared Term graph into value/computation copies; or
- changing DefEq because an observational proof was published.

These are architecture violations, not deferred optimizations.

## 12. Progress Sheet

| Phase | Status | Blocked by | Completion evidence |
| --- | --- | --- | --- |
| A1.T0 relation-to-identity theory gate | in progress: family computation, non-trivial inhabitance, pure raw-CBPV APP action, nullary ordinary Match action, finite Universe correspondence/diagonal, field-bearing indexed Match, ordinary `R`/`trr`/`trl`/`liftr`/`liftl` projections, proof-preserving Pi weakening, type-level Pi/APP/Lambda action, Universe-selected `B(a)` fibers, first-dimensional dependent constructor fields, arbitrary-endpoint family formation/witness checking, the non-DefEq constant-function matrix, and the authoritative identity-versus-eta-Match root complete; neutral type-level Match action, genuinely dependent Pi, dependent higher constructor squares, field-bearing source Match action, effectful identity, higher Universe coherence, symmetry/composition, and general surface `refl`/`ap` pending | close dependent higher lifting, symmetry/composition, and higher Universe coherence before widening the admitted fragment | ordinary object `refl`/`ap`, Context-level transport/lifting laws, residual boundary, all prototype scripts pass |
| A1.R0 authority lifecycle split | complete | none | reader replay preserves exact IDs; no candidate reconstruction; all prototype scripts passed |
| A1.R1 dense publication graph | complete for admitted v70 graph | none | independent relocation maps; allocation perturbations and derivation reversal preserve bytes |
| A1.R2 direct accepted append | complete | none | exact Proposition/Claim/Derivation append, Universe provenance relocation, Claim-only evidence, accepted replay, and artifact flow pass |
| A1.0 v70 manifest | complete | none | corrected schema/fingerprint; v69 rejected |
| A1.1 root ownership/extraction | complete for admitted fragment | T0 before widening | exact request/certificate/Claim linkage for 16 roots |
| A1.2 root closure | complete for admitted fragment | T0 before widening | dense minimal public closure including root-only dependencies |
| A1.3 term/proof wire | complete for admitted fragment | T0 before widening | generated ADT/Pi families and ordinary proof DAG replay directly; compiler-local relation tags/proofs never gain identity authority |
| A1.4 root wire/validation | complete for admitted fragment | recursive Universe bisimulation before adding Universe roots | forged roots rejected; no compiler-local or finite Universe scaffold gains identity authority |
| A1.5 append/link roots | complete for admitted fragment | T0 before widening | relocated exact Claims replay and aggregate |
| A1.6 object matrix | complete for admitted fragment | dependent/higher/effect rules remain residual | all admitted forms round-trip/link; unsupported forms cannot become roots |
| A1.7 adversarial/determinism | complete for admitted fragment | none | forged graph rejection and history-independent bytes |
| A1.8 exit audit | in progress: verification complete, commit/LOC pending | final record | full suite, sanitizers, LOC/deletion report, final commit |

## 13. Recommended Execution Order

```text
V3-PC1 at d4c3c3d
  -> A1.T0
  -> A1.R0
  -> A1.R1
  -> A1.R2
  -> A1.0
  -> A1.1
  -> A1.2 and A1.3
  -> A1.4
  -> A1.5
  -> A1.6
  -> A1.7
  -> A1.8
```

A1.T0 decides what the v70 schema is allowed to mean. Existing uncommitted R0
work may be retained as authority-boundary cleanup, but no observation wire
contract may be frozen while T0 remains open.

A1.R1 and A1.R2 must not be deferred until after the v70 schema is frozen;
their relocation and authority contracts determine that schema. A1.2 and
A1.3 may proceed in parallel only after dense IDs and root ownership are
fixed.

## 14. Per-Phase Measurement Record

Fill this table at each completed phase. A reduction in LOC is desirable only
when it records deletion of obsolete authority paths.

| Phase | Commit | `ast.c` +/- | `typing.c` +/- | `read_file.c` +/- | other prototype +/- | deleted paths | tests/hash |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| baseline | `d4c3c3d` | 0 | 0 | 0 | 0 | none | PC1 evidence |
| combined A1 working tree | pending final commit | +3749 | +2428 | +47 | `hott.c` +12882; `hott_goal_check.c` +7888; `type_declaration.c` +1442; artifact test +100 | sparse publication; Proposition evidence references; accepted-graph rebinding; `OBSERVATION_*` object interpretation | all 16 scripts; examples 01-07/09; sanitizer; hash `2d64defe...` |

The growth is intentional and is not presented as compaction. Most of it is
ordinary object identity generation/replay and adversarial fixtures. A later
extraction may reduce fixture size, but must not merge distinct kernel rules or
reintroduce shape-based authority recovery merely to reduce LOC.

## 15. Handoff After A1

A1 deliberately stops at persistent object evidence. A1.T0 may require enough
transport/lifting and Universe-relation structure to determine what that
evidence means, but A1 still does not add surface `==`, general rewrite,
equality reflection, univalence, effect contextual equivalence, or
machine-word HIT computation.

After A1.8, re-audit the actual accepted graph before selecting among:

- object-facing equality syntax and named proof exports;
- transport, symmetry, composition, and coherence;
- Universe observational actions;
- effect-aware observational semantics; or
- machine-word quotient/HIT semantics.

The next stage is chosen from remaining kernel dependencies, not from the most
visible surface syntax.
