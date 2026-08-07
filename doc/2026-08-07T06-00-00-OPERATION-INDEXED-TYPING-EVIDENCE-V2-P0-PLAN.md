# Operation-Indexed Typing Evidence: V2-P0 Implementation Plan

Date: 2026-08-07
Last re-audited: 2026-08-08

Status: V2-P0 is complete in the 2026-08-08 working tree. P0-R0 was its first
implementation slice, not a pre-P0 project. The implementation satisfies the
entry invariants in Section 3.1, and all P0.1-P0.8 checks are complete. HOTT
object-equality rules remain deferred to the narrowed P1 evidence-storage work
and V2-O1.

Parent plan:
`doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Audit state:

- committed baseline: `fb248ba` (`origin/main` is the same commit);
- artifact format in the P0 working tree: `A_PROGRAM_ARTIFACT 62`;
- the P0 working tree is intentionally uncommitted;
- provisional binder assumptions are revalidated against ContextDB;
- request/fold constraints retain exact child Operation IDs and refreshed
  Operation-selected classifiers;
- effect-row fixed points use canonical finite-set solver state and materialize
  stable row syntax only after the solver has stopped changing;
- `higher_order_operation_force_once_check.p` and
  `higher_order_operation_force_twice_check.p` pass in strict mode;
- CBPV surface, definition-block, computation-block, and artifact-flow
  regression suites pass at this checkpoint.

## 1. Purpose

V2-P0 corrects the ownership boundary between the shared computation graph and
typed A Program occurrences before Higher Observational Type Theory adds object
equality witnesses.

The defect is not that one Core Term can occur with more than one classifier.
That sharing is intentional. The defect is that the current JudgementDB erases
the typed occurrence identity and then tries to recover it from Core Term IDs,
contexts, classifiers, and proof payload fields.

The migration must preserve this direction:

```text
Surface syntax
    -> typed OperationGraph occurrence
    -> shared erased TermDB computation
```

Typing and name selection belong to the Operation/type-view layer. Beta, iota,
normalization, and shared computation identity belong to TermDB.

## 2. Verified Code Facts

The following facts were rechecked against the 2026-08-07 codebase.

### 2.1 OperationGraph already is the typed occurrence graph

`struct prototype_operation_node` in `src/prototype/ast.h` stores:

- an Operation tag;
- value/computation polarity and application role;
- Context ID;
- shared Core Term ID;
- lowering-time known classifier;
- the fixed-point solver result;
- a classifier variable indexed by Operation ID;
- source provenance;
- typed APP, Lambda, Match, IH, CBPV, request, and fold edges.

The source comment explicitly permits Bool identity and Nat identity to share
one Core lambda while retaining distinct Operation IDs and classifiers. The
artifact regression test also requires that behavior.

### 2.2 The classifier solver is already Operation-indexed

`operation_classifier_solver.bindings[operation_id]` and
`prototype_operation_node.classifier` hold the selected synthetic solution for
one Operation occurrence. Constraint generation and dependency scheduling use
Operation IDs.

`operation_solver_materialize_judgements()` then loses that identity by calling
Term-indexed Judgement APIs with `operation->core_term`.

### 2.3 JudgementDB currently mixes distinct semantic domains

`prototype_judgement_relation.subject` is always a `uint32_t`, but current
producers use it for several different purposes:

1. a Core Term implementing a source Operation;
2. a Context binder VAR term;
3. a type-formation or Pi term;
4. a constructor or intrinsic declaration term;
5. a universe term used to generate level constraints;
6. a term exposed through an ascription or top-level declaration.

These are not all Operation identities. Replacing every `subject` value with an
Operation ID would therefore create a new category error.

### 2.4 One solver result is not the same as one admissible typing relation

The fixed-point solver selects one classifier for each Operation. However,
`operation_solver_materialize_judgements()` deliberately records both Int64 and
Int32 typings for an in-range integer literal. APP solving may specialize the
literal Operation from its provisional default to the required machine integer
domain.

Therefore P0 must distinguish:

```text
synthetic Operation classifier solution
derived/admissible typing claim
conversion or explicit exposure
```

P0 must not impose the false invariant that an Operation can participate in
only one typing relation. The stable invariant is that the solver has at most
one selected synthetic solution for an Operation at a time.

### 2.5 OperationGraph already contains most structural premises

APP children, Lambda bodies, Match cases, fold clauses, Context IDs, and binder
metadata are already stored on OperationGraph. Re-encoding every such edge in a
second premise arena is not justified before P0 determines which evidence is
not derivable from OperationGraph and its authoritative metadata.

### 2.6 Conversion and Universe behavior were previously overstated

A completed conversion may be replayed by the normative kernel conversion
profile. Its compilation budget need not be serialized as proof evidence.
Budget and profile become persistent data only for an unfinished residual that
must be resumed.

`Universe(u) : Universe(v)` currently contributes the constraint
`u + 1 <= v` to UniverseDB; it is not accepted without the universe solver.
The P0 problem is that UniverseDB discovers supporting classifiers through
Core-Term-indexed Judgement lookup. It is not an independent cumulativity
soundness defect.

## 3. P0 Premises

These premises are frozen before structural implementation begins.

### P0-P0: Typing evidence belongs to a typed occurrence, not a Core Term ID

For source and compiler-generated operations, a typing conclusion is attached
to the Operation/type-view occurrence that selected the classifier. The erased
`core_term` is a projection used for computation and normalization. It is not
the owner of the typing conclusion and is not a key from which that owner may
be reconstructed.

This is the refactoring premise for all of P0:

```text
typed occurrence authority = Operation ID + Context + selected type view
erased computation          = TermDB Core Term
typing evidence             = derivation over the typed occurrence authority
```

Consequently, two Operations which share one alpha-interned Lambda, APP, or
Match Core node remain distinct typing subjects. Authority-neutral Core facts
may assist normalization or generated-type synthesis, but they never become a
source Operation proof merely because their Term, Context, and classifier
tuples happen to match.

### P0-P1: TermDB remains shared and type-erased

P0 must not introduce separate Value, Computation, or classifier copies of APP,
Lambda, Match, or other shared Core syntax.

### P0-P2: OperationGraph is authoritative for source-occurrence typing

For a source or compiler-generated typed operation `o`, the primary claim is:

```text
Gamma(o) |- o => core(o) : classifier(o)
```

The Context and Core Term are obtained from `o`; they do not identify `o`.

### P0-P3: OperationGraph is also the structural proof skeleton

When a rule premise is already an Operation edge, validators must follow that
edge. They must not search globally for a relation with the same Core Term and
classifier. A second graph of duplicated structural premise tuples is not added
without a demonstrated missing case.

### P0-P4: Non-Operation facts keep their own authorities

P0 does not force all facts into a tagged Operation handle. The initial
authority mapping is:

| Fact | Authority after P0 |
| --- | --- |
| source/generated operation classifier | OperationGraph plus ContextDB |
| binder assumption | ContextDB extension and binding identity |
| constructor classifier/schema | TypeDeclarationDB graph-level classifier family |
| intrinsic declaration | intrinsic Term declaration and its declared classifier |
| type formation | typed type occurrence when one exists; otherwise TermDB plus TypeDeclarationDB |
| universe level constraint | UniverseDB constraint with its originating typed boundary |
| ascription conversion | ASCRIPTION Operation and its body Operation |
| exported-name exposure | compile label/export entry selecting a typed Operation or type view |

Whether physically separate DBs or a tagged common storage are used is decided
only after each producer is assigned to this table. Semantic domains must not be
collapsed merely to keep one `uint32_t subject` field.

### P0-P5: Synthesis, admissibility, and exposure are distinct

`operation.classifier` is the selected synthetic result. Numeric-literal
overload, effect weakening, context reindexing, conversion, and declaration
exposure are derived claims. They must not overwrite the synthetic result or be
mistaken for additional independent synthesis.

### P0-P6: Fixed-point state is provisional until commit

JudgementDelta may rebuild unpublished derivations while solving. P0 does not
declare every transient delta record immutable. Once a certificate is published
and referenced by stable ID, however, its referent must remain valid. The final
representation may satisfy this by immutable records, relation identity, or
validation directly from OperationGraph; P0 must not assume one representation
before migration.

### P0-P7: Meta conversion is not object equality

Successful beta/iota/normalization comparison remains a kernel decision.
P0 adds no object Eq, observational equality, path, transport, or coherence
term. Higher Observational equality remains deferred to V2-O1.

### P0-P8: P1 is not predetermined

Tagged rule payloads and a variable-size premise arena are no longer mandatory
successors. After P0, P1 must audit what evidence remains absent from
OperationGraph, ContextDB, TypeDeclarationDB, UniverseDB, and kernel replay.
Only that irreducible evidence may receive a new payload or arena.

### P0-P9: A typing conclusion belongs to an Operation/type-view occurrence

For an Operation-backed derivation, the conclusion identity is not the tuple
`(context_id, core_term, classifier)`. It is the typed occurrence selected by
`operation_id`, together with the claimed classifier and rule boundary. Two
Operations may project to the same Core Term and have equal Context and
classifier while still being distinct conclusions.

Consequently, structural premise resolution must receive the expected child
Operation ID. It may not recover a premise by globally searching for any proof
whose Core Term, Context, and classifier happen to match.

### P0-P10: Claim identity and derivation identity are separate

The singular solver result `operation.classifier` does not imply one proof per
claim or one claim per Operation. A typed claim is identified at the typed
occurrence layer, conceptually by:

```text
(operation_id, context_id, judgement_kind, claimed_classifier)
```

The Core Term is a checked projection of that claim, not its identity. Distinct
derivation DAG nodes may establish the same claim. P0 therefore makes no proof-
irrelevance, proof-uniqueness, or `relation -> exactly one proof` assumption.

Published proof records are immutable DAG nodes. A later derivation must not
overwrite a proof already referenced as a premise. Solver-local unpublished
delta state may be rebuilt. Once solving has validated a derivation, committing
it must preserve that derivation together with every other validated derivation
produced within the configured resource budget. Failed, superseded, or still
open solver candidates are not derivations and are not committed.

A later P1 storage audit may intern one claim separately from its many
derivations. P0 must not simulate that representation either by destructively
replacing one proof or by duplicating a Term-indexed relation and treating the
duplicates as distinct typed occurrences.

### P0-P10A: Operation edges are exact; erased Core projections are alpha-stable

Structural premise identity is the exact child Operation ID stored by the
OperationGraph. The corresponding TermDB child is only an erased projection.
Because TermDB alpha-interning may return an existing representative whose
stored child IDs differ from the newly lowered occurrence, projection
validation must use Core shape equality rather than raw child Term IDs.

Lambda binding identity is therefore retained directly by the Lambda
Operation and serialized across artifacts. It must not be recovered from the
alpha-interned Core representative. Scoped body projection is checked under
the pair of representative and occurrence binding identities.

This does not permit premise borrowing. The validator first follows the exact
Operation edge and only then checks that this Operation's erased Core is alpha
equivalent to the representative child stored in its parent Core node. Typed
view equality and conversion are not inferred from this projection check.

Operation structural validity does not imply that static synthesis has closed
its classifier. In particular, a zero-clause computation fold whose result
type depends on an effectful input remains a valid Operation with an unresolved
classifier residual. P0 validates its exact structural authorities but emits a
typing claim only after the classifier is solved; it must not misclassify the
residual as stale or malformed graph data.

### P0-P11: Authority-neutral facts are explicit exceptions

An Operation premise must resolve to that exact Operation. A relation with
`operation_id == PROTOTYPE_INVALID_ID` is eligible only for a rule whose
authority is explicitly non-Operation, such as a ContextDB binder assumption or
a declaration fact. Neutrality is not a fallback used when an Operation-owned
premise is missing.

### P0-P12: Expected-type exposure is not conversion

Kernel conversion proves equality under the normative normalization profile.
Classifier compatibility that also emits Universe or effect constraints is an
expected-type exposure boundary, not a conversion proof. P0 keeps these proof
kinds separate so that an unsolved metavariable constraint is never serialized
as completed definitional equality.

### P0-P13: Constraint generation, solving, and evidence commit are separate phases

The current fixed point both discovers classifiers and incrementally emits
JudgementDelta relations. That coupling is the source of several ownership
repairs and provisional-proof replacement paths.

P0 uses this conceptual pipeline:

```text
OperationGraph occurrence
    -> Operation-indexed constraints
    -> fixed-point classifier/effect solution
    -> rule-specific evidence candidate
    -> atomic certificate commit
```

A constraint records Operation identities for its conclusion and structural
children. Core Term IDs are projections used by normalization and erased-shape
checks; they are not enough to identify a constraint operand.

The solver may revise unpublished bindings and evidence candidates. It must not
revise a committed proof node. A failed or superseded candidate is discarded
before commit rather than published and repaired afterwards.

### P0-P14: A proof is constructed atomically

The conclusion authority, Context, kind, Core projection, classifier, premise
authorities, premise tuples, and rule-specific parameters form one proof node.
They must be supplied together.

Late resolution may translate already fixed premise authorities to persisted
proof IDs during artifact layout. It may not select a different semantic
premise, mutate rule parameters, or repair an earlier derivation by tuple
search.

This makes the existing post-insertion updates in constructor-spine formation,
match-pattern ownership, Context reindexing, and DB commit explicit P0 debt.

### P0-P15: Generic TermDB inference is not source-operation inference

Raw TermDB scans may derive authority-neutral kernel facts for generated Core
terms. They may not materialize source Operation certificates or feed a source
Operation solver result. Source rules must start from OperationGraph and retain
child Operation IDs through their constraints.

The generic computation and CBPV boundary scans are compatibility paths to
remove from source compilation, not fallback ownership mechanisms.

### P0-P16: The fixed-point frontier is not the certificate graph

The classifier frontier is solver state. A frontier entry may be revised when
new constraints refine a binder, Match motive, computation result, effect row,
or expected-type boundary. It is therefore not yet a typing certificate.

Only after the relevant Operation-indexed constraints are solved may P0 build
the corresponding claim and its derivation DAG. This preserves the intended
frontier computation while preventing provisional binder assumptions or fold
classifiers from surviving as accepted proofs after their Context or solver
binding has changed.

### P0-P17: Effect-row equations are solver state, not Term normalization

An effect row appearing in a classifier is a projection of an occurrence-level
constraint solution. In particular, a symbolic equation such as:

```text
rho_out = rho_return union rho_clause union residual(rho_input, handled)
```

must be solved in Operation-owned solver state. Repeatedly constructing
`EFFECT_ROW_UNION` TermDB nodes and treating each larger syntax tree as the next
classifier is not a solver: it can grow `rho union rho`, oscillate between
symbolic approximations, and make fixed-point convergence depend on Term shape.

TermDB continues to represent a solved or residual row expression, and kernel
conversion may compare such expressions. It does not own the mutable equation
environment. The solver must canonicalize row variables and finite operation
sets in its own state, detect an unchanged solution semantically, and
materialize a classifier Term only after that solution is stable. An unresolved
row remains an explicit residual; it is not accepted as completed typing
evidence.

### P0-P18: Only closed, independently replayable claims become certificates

Successful source checking and a closed kernel certificate are distinct
outcomes. A classifier containing an unowned effect-row variable is solver or
linker state, not a completed JudgementDB conclusion. Likewise, compatibility
which depends on a transparent imported definition cannot be serialized as a
standalone conversion proof unless that definition is an explicit replayable
premise.

Consequently:

- recursive proof materialization applies the same closure check at every
  Operation, not only at top-level iteration roots;
- ascription and export checks may validate a residual boundary without
  emitting a closed certificate;
- a late check phase never writes a second proof which bypasses the
  Operation-indexed evidence-candidate path;
- artifact dependencies and residual constraints remain explicit until the
  linker can close them.

### P0-P19: Effect equations and external effect obligations are distinct

The equation `rho = rho` is semantically unchanged solver state. It does not by
itself constrain `rho`. However, a fallback COPY record may also be the only
current record that an unresolved external operation's effect row must be
closed at link time. P0 must not conflate these meanings.

At the current v62 checkpoint, an identity COPY is solved when its Operation
subtree is internally constructed or its row variables are scoped by that
Operation. It remains residual when the subtree contains an unresolved
`EXTERNAL_REF`. A later representation should give the external obligation its
own explicit kind instead of continuing to overload COPY.

### 3.1 P0 first slice: required refactoring direction

P0 begins by establishing this construction boundary. It must not begin by
adding equality terms or by changing proof payload tags. This is the first
slice of P0 itself, not an optional pre-P0 compatibility project.

```text
Surface occurrence / generated typed occurrence
    -> Operation-indexed constraints
    -> provisional fixed-point candidates in JudgementDelta
    -> authority-specific validation
    -> complete derivation DAG commit to JudgementDB
```

The code re-audit fixes the following directions as P0 premises:

1. An Operation-backed claim is owned by `operation_id`. Its Context, Core
   subject, and classifier are checked projections and cannot recover the
   occurrence by tuple lookup.
2. Structural premises follow exact child Operation edges. Context,
   declaration, type-formation, and Universe facts use explicitly named
   non-Operation authorities. A missing Operation premise cannot fall back to
   a neutral Core relation.
3. `JudgementDelta` is unpublished candidate storage. Candidates may be
   rebuilt or discarded during fixed-point solving. `JudgementDB` is the
   publication boundary and receives only complete records whose conclusion
   authority, premise authorities, and rule payload are already fixed.
4. One selected synthetic classifier per Operation is distinct from the set of
   admissible or derived typings. Multiple valid derivations of one typed claim
   are permitted and cannot destructively replace one another.
5. Effect-row equations use canonical finite sets plus unresolved atoms as
   solver state. TermDB row syntax is only input/output representation.
   Higher-order operation atoms stay opaque: residual handling may remove an
   atom through its declared operation label, but cannot expose latent effects
   from the thunk carried by that atom.
6. Generic TermDB inference may remain only as an explicitly Core-level helper.
   It creates authority-neutral candidates and cannot publish evidence for a
   source Operation until that evidence is reified through the exact Operation
   and Context.
7. Artifact relocation and linking may construct a new unpublished certificate
   image. They may not mutate semantic fields in an already published
   in-memory certificate.

These are not advisory design preferences. They are the **P0 entry gate** for
every implementation item below:

- an Operation-backed producer must receive the owning `operation_id` directly;
- an Operation-backed premise must receive the exact child `operation_id`
  directly from OperationGraph;
- no Operation owner or child may be reconstructed by searching JudgementDB for
  a matching `(Core Term, Context, classifier)` tuple;
- Core Term equality may validate only the erased projection after the exact
  Operation edge has been selected;
- Context binding, declaration, type-formation, and Universe facts must enter
  through authority-specific APIs and must not be accepted as an anonymous
  fallback for a missing Operation proof;
- the fixed-point frontier may emit only unpublished candidates; a published
  derivation is an immutable DAG node and a second valid derivation is appended,
  not substituted for the first;
- `operation.classifier` is the one selected synthesis result, while literal
  admissibility, conversion, effect weakening, context reindexing, and export
  exposure are separately justified derived boundaries.

Any P0 implementation which requires violating one of these items must stop and
revise this plan rather than add another compatibility lookup or mutable repair
path.

Current first-slice state after re-reading the implementation:

| Boundary | State | Remaining work in P0 |
| --- | --- | --- |
| Operation ownership and exact premise edges | first-slice boundary complete | retain exact child Operation IDs; finish rule-specific certificate validation in P0.3 |
| Candidate/commit separation | first-slice boundary complete | keep generic erased-Core inference authority-neutral and permit publication only through exact Operation reification |
| Atomic proof construction | complete for normal and delta insertion | retain declaration completion only as construction of a new unpublished linked image |
| Canonical effect-row fixed point | complete for the current row language | add direct stability assertions and retain free external atoms as explicit residuals |
| Operation structural validation | implemented for all current Operation tags | consolidate the older `prototype_operation_graph_validate()` checks with `prototype_judgement_validate_operation_typing()` during P0.3/P0.7 |
| Non-Operation authority separation | P0.5 work | split binder, declaration, type-formation, and Universe queries from generic `subject` lookup |
| Derived typing boundaries | P0.4 work | finish ASCRIPTION/exposure, literal specialization, weakening, and reindex provenance |

Therefore the current project phase is **V2-P0**. The working tree has already
implemented the P0-R0 construction boundary, and its implementation must remain
subject to the entry gate above. The next ordered work is P0.1
characterization, followed by P0.2-P0.7. Non-Operation authority cleanup and
derived-boundary cleanup are P0 work; they are not a separate phase before P0.

## 4. Target Boundary

P0 separates three concepts currently stored as similar Term tuples.

### 4.1 Operation typing certificate

An operation certificate identifies:

```text
operation_id
selected classifier
rule result/status
optional evidence not derivable from authoritative graphs
```

Its Context, Core Term, polarity, rule shape, and structural children are read
from OperationGraph and checked for agreement. A physical certificate table may
be unnecessary if the validated Operation node itself carries all required
data; P0.2 makes that decision before changing artifact layout.

### 4.2 Kernel or declaration fact

A type-formation, constructor, intrinsic, or universe fact is indexed by its
own authority. It is not disguised as an Operation certificate merely because
its syntax is represented by a TermDB node.

### 4.3 Derived boundary claim

Ascription, export exposure, effect weakening, context reindexing, and literal
specialization state both the source claim and the explicit boundary that
derives the target claim. They do not add classifiers globally to a shared Core
Term.

## 5. Confirmed Migration Surface

| Code area | Current role | P0 direction |
| --- | --- | --- |
| `src/prototype/ast.h:prototype_operation_node` | typed occurrence and solver result | retain as operation authority |
| `src/prototype/ast.c:operation_solver_materialize_judgements` | converts Operation solutions into Term-indexed relations | materialize/validate by Operation ID |
| `src/prototype/ast.c:operation_solver_reify_core_proof` | reconstructs structural proof through Core relations | follow typed Operation edges first |
| `src/prototype/ast.c:compile_phase_check_ascriptions` | knows the ASCRIPTION/body Operation but adds Core conversion | attach the derived claim to the typed boundary |
| `src/prototype/ast.c:compile_phase_check_expectations` | sometimes uses Operation classifier, sometimes scans Core relations | make label/export selection explicit |
| `src/prototype/typing.c:prototype_judgement_delta_infer_core_helper_facts` | named erased-Core helper closure | retain authority-neutral output; never publish it as Operation evidence without exact reification |
| `src/prototype/typing.c:add_complete_relation` | immutable complete DB insertion | retain all distinct validated derivations; do not infer proof uniqueness from conclusion tuples |
| `src/prototype/typing.c:add_complete_delta_relation` | complete unpublished candidate insertion | preserve rebuild/discard semantics before commit |
| `src/prototype/typing.c:prototype_judgement_add_conversion` | Core-level derived classifier chain | replace with explicit typed-boundary conversion API |
| `src/prototype/typing.c:prototype_judgement_validate_operation_typing` | validates current Operation tags against exact graph/context/declaration authorities | retain as the structural certificate entry point and consolidate duplicate graph validation |
| `src/prototype/universe.c:lookup_authority_classifier` | obtains an authority-neutral classifier for a Core Term | replace source-boundary callers with explicit selected Operation/type-view classifiers |
| artifact judgement read/write/relocation in `src/prototype/ast.c` | assumes every subject is a Term ID | change only after the P0 ownership model is implemented |

## 5.1 P0 Entry Re-audit: Findings and Disposition

The 2026-08-07 implementation re-audit found that Operation ownership fields
had been added to Judgement relations and proofs, but semantic selection still
used Core tuples. The P0-R0 working tree has fixed the construction-boundary
items below. The remaining rows are the ordered P0 work, rather than reasons to
pretend that P0 has not started.

| Re-audit finding | Current disposition | Remaining P0 action |
| --- | --- | --- |
| Core-tuple premise lookup could borrow another occurrence's proof | old Operation reindex/proven-classifier helpers are removed; structural proof resolution now receives the expected child Operation ID | keep neutral fallback available only for explicitly non-Operation rules |
| One physical relation points to one proof | insertion now preserves distinct complete derivations instead of replacing a prior derivation | P0.8/P1 decides whether claim interning plus one-to-many derivation edges improves storage; no semantic uniqueness assumption is permitted |
| Structural premise arrays could lose the selected occurrence | exact child Operation IDs are retained and forged cross-occurrence premises are rejected | treat persisted proof IDs only as replay edges; never use them to rediscover a structural Operation edge |
| Ascription compatibility was conflated with conversion | `EXPECTED_TYPE_EXPOSURE` is distinct from kernel `CONVERSION` | complete Universe/effect constraint provenance in P0.4 |
| Fold/request dependencies lacked exact typed operands | constraints retain child Operation IDs and canonical effect-row solver state | retain tag-specific dependency validation and add stability characterization |
| Rule payloads were mutated after insertion | normal and delta insertion receive complete candidates; committed derivations are not destructively replaced | keep link completion confined to a new unpublished image and re-audit residual late proof-ID relocation |
| Generic all-Term inference had ambiguous authority | it is now named `prototype_judgement_delta_infer_core_helper_facts`, forces invalid Operation authority, and verifies every emitted fact remains neutral | preserve it only as motive/type-synthesis support; exact Operation reification is mandatory before publication as source evidence |
| Alpha-interned Lambda/Match representatives do not preserve occurrence binder/child IDs | OperationGraph now stores direct Lambda `binding_id`; structural validation follows exact Operation edges and uses alpha-stable Core projection checks | consolidate duplicate raw-ID checks in `prototype_operation_graph_validate()` during P0.3/P0.7 |
| Non-Operation facts still share generic Term-subject storage | declaration-specific record APIs exist, but binder and Universe consumers still use generic relation lookup | finish the authority split in P0.5 |
| Universe collection calls `lookup_authority_classifier(core_term)` | it can still select an authority-neutral classifier by erased Core identity | pass explicit selected Operation/type-view or declaration/type-formation inputs in P0.5 |

The immediate refactoring order is therefore:

```text
Operation-indexed constraints and operands
    -> solver/evidence-candidate separation
    -> atomic immutable committed derivations
    -> exact Operation premise lookup
    -> tag-specific structural materialization
    -> derived boundary separation
    -> non-Operation authority cleanup
    -> artifact transition and P1 re-audit
```

## 6. Implementation Phases

### P0.0 Characterize the current boundary

- [x] Verify shared Core/distinct Operation identity behavior.
- [x] Verify that the classifier solver is Operation-indexed.
- [x] Identify the loss of Operation identity during judgement materialization.
- [x] Identify non-Operation judgement producers.
- [x] Identify integer-literal multiple admissible typings as a counterexample
  to one-relation-per-Operation.
- [x] Reclassify conversion, Universe, artifact, and HOTT residual findings.
- [x] Add a checked-in producer/consumer ownership table beside the code or in
  this document as implementation discovers previously indirect call sites.

### P0-R0 Establish the certificate construction boundary

This is the entry refactor for P0. It is not a separate pre-P0 project.

- [x] Add Operation ownership to relations, proofs, and computation constraints.
- [x] Preserve a computation constraint's owner Operation across solver passes.
- [x] Stop replacing an existing proof merely because its conclusion tuple
  matches a newly discovered derivation.
- [x] Pass Lambda premise Contexts and Context-reindex source Context at
  insertion instead of repairing them afterwards.
- [x] Emit binder assumptions and computation-body effect weakening as explicit
  authority-neutral facts rather than assigning them to the parent fold.
- [x] Revalidate pending binder assumptions against the final ContextDB binding
  before materialization, and remove stale assumptions during provisional
  derivation cleanup.
- [x] Extend request/fold computation constraints with the exact child
  Operation IDs needed by their typing rules.
- [x] Move symbolic effect-row equations out of classifier-Term reconstruction
  and into canonical Operation-owned solver state.
- [x] Represent each row solution as a finite operation set plus unresolved row
  variables/residuals; equality of solver states must not depend on union-tree
  association, duplication, or Term ID.
- [x] Materialize computation classifiers from stable row solutions once per
  fixed-point result. Do not widen a classifier by repeatedly appending row
  syntax.
- [x] Introduce one evidence-candidate builder whose input contains complete
  conclusion authority, premise authorities, and rule payload.
- [x] Keep evidence candidates outside the committed relation/proof arrays
  until their Operation classifier, Context bindings, and rule premises are
  stable for the current fixed point.
- [x] At commit, preserve every validated derivation of a typed claim; do not
  commit failed, superseded, or open candidates.
- [x] Replace constructor-spine and match-pattern post-insertion payload writes
  with the complete candidate builder.
- [x] Make delta commit construct complete immutable DB proof nodes; remove
  `copy_db_relation_rule_parameters()`.
- [x] Restrict late proof-ID resolution to relocation/linking of already fixed
  premise identities; it must not perform semantic premise selection.
- [x] Stop the legacy APP premise refresh from rewriting Operation-owned APP
  premises by globally scanning shared Core relations.
- [x] Enforce closure during recursive Operation proof reification so a parent
  ASCRIPTION or APP cannot publish a residual external effect row.
- [x] Remove late ascription proof insertion from the validation phase; exact
  ASCRIPTION evidence is emitted through JudgementDelta.
- [x] Avoid publishing expected-type evidence whose conversion depends on an
  imported definition not represented as a proof premise.
- [x] Prove and name the remaining generic TermDB scan as
  `prototype_judgement_delta_infer_core_helper_facts`; force its emitted
  relations and proofs to remain authority-neutral and require exact Operation
  reification before source-operation publication.
- [x] Restore all pre-P0 CBPV and artifact tests before P0.1 proceeds.

P0-R0 is complete only when preserving multiple derivations does not change
which classifier the fixed point selects. The former
`higher_order_operation_force_once_check.p` regression demonstrated that a
binder assumption emitted for a provisional Context classifier survived after
the fixed point refined that binding. The working tree now revalidates the
candidate against ContextDB before materialization and cleanup; it does not
loosen assumption validation or restore destructive proof replacement.

The former `higher_order_operation_force_twice_check.p` regression is restored.
The current solver keeps finite effect bits and unresolved row/operation atoms
in Operation-owned metadata, compares that state semantically, and materializes
one canonical TermDB row only after the fixed point is stable. Residual handling
uses a higher-order atom's declared operation label without unfolding the
latent thunk effects. External free-row obligations remain residual.

### P0.1 Add characterization tests before changing storage

- [x] Preserve the existing identityBool/identityNat shared-Core test.
- [x] Assert distinct Operation IDs and distinct selected classifiers.
- [x] Add an operation-level lookup test proving that neither occurrence can
  borrow the other's classifier.
- [x] Preserve in-range integer literal Int64/Int32 admissibility and assert one
  selected solver classifier.
- [x] Preserve context-driven integer literal specialization in APP.
- [x] Add an ascription with normalization-equal but non-identical classifier
  terms and verify that the body Operation is unchanged.
- [x] Add repeated fixed-point execution and verify stable selected Operation
  classifiers, without requiring transient delta IDs to remain stable.
- [x] Add Universe constraint regression coverage before changing its inputs.
- [x] Add a forged shared-Core premise test: replacing a child premise with a
  proof from another Operation must be rejected even when Core Term, Context,
  and classifier match.
- [x] Add a multiple-derivation test showing that two validated derivations of
  one Operation-level claim are both preserved and neither overwrites nor
  invalidates the other.
- [x] Add a provisional-binder regression showing that a superseded classifier
  candidate is absent from committed evidence while the final binder
  assumption validates against ContextDB.
- [x] Preserve `higher_order_operation_force_once_check.p` as the provisional-
  binder regression.
- [x] Preserve `higher_order_operation_force_twice_check.p` as a row-solver
  convergence regression; forcing twice must not grow the inferred effect row.

### P0.2 Introduce authority-specific APIs

- [x] Add an Operation classifier query that accepts only Operation ID and
  returns the selected synthetic classifier.
- [x] Add an Operation typing validation entry point that receives
  OperationGraph, ContextDB, TermDB, TypeDeclarationDB, and the Operation ID.
- [x] Add separate APIs for binder assumptions, declaration facts, type
  formation, and universe constraints rather than one untyped `subject` API.
- [x] Decide from call sites whether operation certificates require a separate
  persisted relation table or can be validated directly from OperationGraph.
- [x] Do not introduce a general subject tag unless common persistence provides
  a concrete benefit that separate APIs cannot provide.
- [x] Replace `operation_solver_reindex_existing_proof()` with exact Operation
  premise resolution plus explicitly named authority-neutral resolution.
- [x] Replace Core-keyed proven-classifier lookup on fold/request paths with an
  Operation-indexed query.
- [x] Make committed derivation insertion append-only or otherwise immutable;
  do not infer semantic proof uniqueness from a storage tuple.
- [x] Represent the logical one-to-many relation from a typed claim to its
  accepted derivations, without requiring the final P1 physical layout yet.
- [x] Require complete rule payload and premise authority at insertion; expose
  no setter that mutates a published proof.

### P0.3 Migrate structural operation rules

- [x] Migrate VAR, Lambda, APP, constructor, Match, and IH structural validation to the
  owning Operation and its typed children.
- [x] Migrate RETURN, THUNK, FORCE, operation request, and computation fold in
  the same way.
- [x] Read binder assumptions from ContextDB/binding identity for Operation
  structural validation.
- [x] Read constructor case and schema information from OperationGraph and
  TypeDeclarationDB.
- [x] Remove global Core-tuple searches from operation premise selection.
- [x] Keep TermDB projection only for checking the erased node shape and for
  normalization.
- [x] Validate fixed-point dependency payloads against tag-specific
  OperationGraph edges, including computation-fold return clauses.

### P0.4 Migrate derived typing boundaries

- [x] Make ASCRIPTION the ordinary expected-type conversion boundary.
- [x] Link the boundary to its body Operation and selected actual classifier.
- [x] Replay successful conversion with the normative kernel profile; do not
  serialize compilation budget for a completed result.
- [x] Represent unfinished conversion only through the existing typed residual
  design, including profile and budget state.
- [x] Model top-level name/export expectations as explicit typed exposure, not
  a new classifier attached to a shared Core Term.
- [x] Classify integer specialization, effect weakening, and context reindex as
  derived claims and preserve their source boundary.
- [x] Remove the hard-coded conversion predecessor walk once boundary identity
  makes it unnecessary.
- [x] Keep `EXPECTED_TYPE_EXPOSURE` distinct from `CONVERSION`, and feed the
  former's Universe/effect constraints to the appropriate solver.

### P0.5 Rehome non-Operation facts

- [x] Remove binder assumption ownership from generic Core relations where
  ContextDB already supplies the fact.
- [x] Validate constructor and intrinsic declarations from their declaration
  authorities.
- [x] Keep generated type-term formation explicit without inventing a fake
  source Operation.
- [x] Change UniverseDB collection to consume explicit selected classifiers and
  declaration/type-formation inputs rather than `lookup_classifier(core_term)`.
- [x] Preserve the existing `u + 1 <= v` constraint semantics and solver.
- [x] Reject any API that accepts an unclassified `uint32_t subject` across
  these domains.

### P0.6 Remove obsolete Term-indexed reconstruction

- [x] Remove the former
  `find_compatible_classifier_for_expectation` Operation-typing path.
- [x] Remove the former `find_unique_synthetic_classifier_for_label`
  reconstruction path; labels use their typed Operation/type-view selection.
- [x] Remove tuple-based premise selection where OperationGraph supplies the
  edge.
- [x] Re-evaluate late proof-edge resolution: retain it only for genuinely
  persisted non-structural evidence that cannot use an authoritative graph
  edge.
- [x] Re-evaluate relation/proof duplication and destructive replacement after
  operation facts no longer share Core keys.
- [x] Remove every Operation-owned call to
  `operation_solver_reindex_existing_proof()` and
  `operation_solver_lookup_proven_classifier()`.

### P0.7 Validation and artifact transition

- [x] Validate each Operation's Context, Core projection, selected classifier,
  polarity, and typed child references.
- [x] Validate derived boundaries against their source Operation/fact.
- [x] Validate non-Operation facts against their explicit authority.
- [x] Preserve proof acyclicity only for the residual evidence graph that still
  exists after structural edges move to OperationGraph.
- [x] Choose the final v62 representation only after the in-memory model is
  stable.
- [x] Update mark, write, read, relocation, append, and link logic once; retain
  no fallback that guesses whether an integer is a Term ID or Operation ID.

### P0.8 Re-audit P1

- [x] Enumerate evidence fields still not derivable from OperationGraph,
  ContextDB, TypeDeclarationDB, UniverseDB, or kernel replay.
- [x] Decide whether any tagged rule payload remains necessary.
- [x] Decide whether any variable-size premise arena remains necessary.
- [x] Treat fixed premise capacity as general implementation capacity debt, not
  automatically as a HOTT blocker.
- [x] Update or cancel V2-P1 accordingly before beginning it.

P0.8 result:

- APP, Lambda, Match, IH, RETURN, THUNK, FORCE, request, and fold structural
  edges are derivable from OperationGraph and must not receive duplicate tagged
  payloads in P1.
- binder assumptions are derivable from ContextDB binding identity;
  constructor/intrinsic facts are replayed from their declaration authorities;
  Universe inequalities are retained in UniverseDB with their explicit source
  boundary.
- conversion/exposure still requires the actual source classifier and boundary
  identity; effect weakening and context reindex require their source claim;
  multiple accepted derivations of one claim require stable derivation identity.
  These are the irreducible evidence candidates for P1.
- the current fixed premise arrays remain an implementation-capacity limit.
  They are not a reason to introduce a general tagged payload or arena before a
  HOTT rule demonstrates an unbounded evidence requirement.
- P1 is therefore narrowed to claim/derivation storage and derived-boundary
  evidence. The former plan to move every structural rule into a tagged payload
  and variable-size premise arena is cancelled.

## 7. Explicit Non-Goals

V2-P0 does not:

- add Higher Observational equality terms;
- turn successful kernel conversion into an object witness;
- make every Core Term own one global classifier;
- require one typing relation per Operation;
- retain failed, superseded, or unfinished solver candidates;
- duplicate Core syntax by value/computation polarity;
- introduce De Bruijn indices or name-based proof identity;
- redesign Universe levels or cumulativity;
- serialize transient fixed-point or solver scheduling state;
- preserve the old artifact representation for backward compatibility.

## 8. Acceptance Criteria

V2-P0 is complete only when:

1. source/generated operation typing is selected by Operation identity;
2. Core Term identity is used only for shared computation, normalization, and
   explicitly Core-level facts;
3. Bool and Nat identities share Core computation without sharing typing
   occurrence identity;
4. the solver has one selected classifier per solved Operation while legitimate
   derived typings such as integer literal specialization remain representable;
5. APP, Lambda, Match, IH, and CBPV validators follow typed Operation edges;
6. conversion and exported expectations attach to explicit typed boundaries;
7. Universe collection no longer selects an arbitrary classifier by Core Term;
8. no unclassified `uint32_t subject` API crosses semantic domains;
9. an Operation premise cannot borrow a proof from another Operation sharing
   its Core Term;
10. accepted proof DAG nodes are not destructively replaced, and multiple
    derivations of one Operation-level claim remain representable;
11. provisional fixed-point candidates are not visible as accepted evidence;
12. effect-row fixed points compare canonical solver states rather than growing
    TermDB union syntax;
13. P1 has been re-audited rather than assumed;
14. warning-free build, all prototype scripts, examples 01-07/09, artifact
    tests at the restored checkpoint, and forged-occurrence tests pass;
15. `git diff --check` passes.

## 9. Progress Record

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-07 | P0.0 code re-audit | complete | Verified Operation-indexed solver, Term-indexed materialization, mixed non-Operation facts, and integer literal overload counterexample. |
| 2026-08-07 | P0 premise correction | complete | Removed mandatory one-certificate, conversion-budget serialization, tagged payload, and premise-arena assumptions. |
| 2026-08-07 | P0 entry code re-audit | complete | Found remaining Core-keyed proof borrowing, destructive proof replacement, expected-type/conversion conflation, and a fold return-edge dependency error. Added them as mandatory P0 premises. |
| 2026-08-08 | P0.1 characterization tests | complete | Shared-Core ownership, forged APP premise, literal admissibility/specialization, repeated selected-classifier stability, multiple derivations, provisional binder cleanup, row convergence, Universe regressions, non-interned normalization-equal Pi conversion, and ASCRIPTION body-Operation preservation pass. |
| 2026-08-08 | P0.2 authority-specific APIs | complete | Operation classifier/validation APIs are exact; Context binding, declaration, type-formation, intrinsic, and Universe entry points are named by authority. The remaining Core lookup API is explicitly authority-neutral and has no Operation solver caller. No general subject tag or separate structural certificate table is introduced. |
| 2026-08-07 | P0-R0 certificate boundary | complete in working tree | Constraint owner preservation, exact operands, complete atomic relation/proof candidates, immutable delta commit, constructor/match/IH payload construction, unpublished candidate staging, canonical row-solver state, explicitly neutral Core helper facts, exact structural Operation validation, and regression restoration are implemented. Non-Operation authority separation is P0.5, not an R0 blocker. |
| 2026-08-07 | P0-R0 provisional binder cleanup | complete in working tree | Pending binder assumptions now use the current ContextDB classifier and stale assumptions are removed before commit; `higher_order_operation_force_once_check.p` passes. |
| 2026-08-07 | P0-R0 exact computation operands | complete in working tree | Request/fold constraints retain child Operation IDs, refresh classifiers from the Operation solver, and expose an explicit solved classifier. Proof history is no longer their solver input. |
| 2026-08-07 | P0-R0 effect-row solver | complete for current row language | Canonical finite bits plus unresolved atoms are compared outside TermDB union syntax and materialized only after stabilization. Residual handling removes higher-order atoms by declared labels without exposing latent thunk effects. Force-once, force-twice, and multi-clause handler strict checks pass; unresolved external rows remain artifact residuals. |
| 2026-08-07 | P0-R0 evidence closure | complete in working tree | Recursive reification rejects unowned open rows; late ascription insertion was removed; imported-definition-dependent exposure is not emitted as a standalone kernel proof. |
| 2026-08-07 | P0-R0 regression restoration | complete | CBPV surface, definition-block, computation-block sequence, and artifact-flow suites pass. |
| 2026-08-08 | P0.3 structural rules | complete | `prototype_judgement_validate_operation_typing()` validates every current Operation tag through exact OperationGraph edges, direct Lambda binding identity, ContextDB, TypeDeclarationDB, and alpha-stable erased-Core projection. Classifier-goal payloads are checked against tag-specific edges, including fold return operations. |
| 2026-08-08 | P0.4 derived boundaries | complete | ASCRIPTION/exposure follows its body Operation, completed conversion replays the normative profile, residual goals retain profile/budget, the predecessor walk is removed, and integer admissibility now has an explicit source-claim premise. |
| 2026-08-08 | P0.5 non-Operation facts | complete | Binder input is binding-indexed, declarations and type formations use named APIs, and UniverseDB consumes OperationGraph/proof/declaration authorities without `lookup_authority_classifier(core_term)`. |
| 2026-08-08 | P0.6 obsolete reconstruction | complete | Structural premise lookup is exact by Operation; late proof-ID resolution remains only to connect fixed non-structural persisted claims in a new unpublished image. No committed proof is destructively replaced. |
| 2026-08-08 | P0.7 validation/artifact | complete | Per-Operation and per-proof validation, proof acyclicity, v62 Operation IDs, relocation, append, link, and forged artifact tests pass without an ID-domain fallback. |
| 2026-08-08 | P0.8 P1 re-audit | complete | P1 is narrowed to derived-boundary and claim/derivation evidence. Mandatory tagged structural payloads and a premise arena are cancelled; fixed premise capacity remains ordinary capacity debt. |

## 10. Mandatory Ordering

```text
V2-C2 -> V2-B1 -> V2-S1 -> V2-P0 -> P1 re-audit -> V2-O1 -> V2-A1
```

V2-P0 and its P1 storage re-audit are complete. The next implementation work is
the narrowed P1 task: represent irreducible derived-boundary evidence and the
claim-to-many-derivations relation without duplicating OperationGraph structural
edges. A general tagged subject, tagged structural payload, and variable-size
premise arena are not prerequisites. HOTT equality terms still begin only in
V2-O1 after that storage boundary is fixed.
