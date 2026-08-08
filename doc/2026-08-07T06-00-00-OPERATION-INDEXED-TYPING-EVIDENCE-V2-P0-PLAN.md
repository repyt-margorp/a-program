# Operation-Indexed Typing Evidence: V2-P0 Implementation Plan

Date: 2026-08-07
Last re-audited: 2026-08-08

Status: V2-P0 active after the 2026-08-08 code audit. Operation-indexed
classifier solving is implemented, but the accepted certificate model still
mixes solver-local obligations, closed claims, derivation identity, and
dependency selection. The next implementation phase is the corrected P0 entry
gate in P0-R0A.2; P0-R0A.1 is complete for the current calculus. This is P0
itself, not P1 evidence work. HOTT
object-equality rules remain deferred until P0 closes.

Parent plan:
`doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

Re-entry code audit:
`doc/2026-08-08T00-00-00-V2-P0-REENTRY-REFACTOR-AUDIT.md`

Audit state:

- committed P0 checkpoint: `985baf8` (`origin/main` is the same commit);
- artifact format at the checkpoint: provisional `A_PROGRAM_ARTIFACT 64`;
- the committed tree writes and reads only provisional artifact v64;
  `term_export.operation` preserves the selected typed occurrence, but v64 is
  not the final native Claim/Derivation publication schema;
- the audit found that the previous P0 completion statement was too strong;
- P0-R0A.1 is committed and is the premise on which P0-R0A.2 starts; this is
  implementation inside P0, not an audit probe or a pre-P0 compatibility layer;
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

The following facts were rechecked against commit `985baf8` on 2026-08-08.

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

They are not preparatory work outside P0. Establishing and enforcing these
premises is the first implementation part of P0. P1 remains blocked until the
premises hold at source compilation, artifact readback, append, and link
boundaries.

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
`operation_id`, together with Context, judgement kind, and claimed classifier.
Two Operations may project to the same Core Term and have equal Context and
classifier while still being distinct conclusions.

The typing rule is not part of Claim identity. It belongs to the Derivation.
Otherwise two rules proving the same proposition would incorrectly create two
Claims and recreate the current relation/proof conflation under new names.

Consequently, structural premise resolution must receive the expected child
Operation ID. It may not recover a premise by globally searching for any proof
whose Core Term, Context, and classifier happen to match.

### P0-P10: Claim identity and derivation identity must be separate before P0 closes

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

This separation is not deferred to P1. The current representation duplicates a
physical relation for each proof and therefore still conflates the two
identities. P0 must intern one claim independently of its derivations. It must
not simulate one-to-many derivation identity by duplicating a relation, and it
must not make a premise depend on whichever duplicate happened to be inserted
first or last.

For a structural typing rule, a derivation consumes the exact child claims
selected by the conclusion OperationGraph node. For a derived boundary such as
conversion, literal admissibility, effect weakening, or context weakening, the
derivation stores the exact source claim identity. Alternative derivations of a
premise claim are not flattened into the parent derivation and do not create a
Cartesian product of parent proofs.

Published claims are accepted only by the least grounded closure:

```text
seed claims justified by declarations, Context bindings, and kernel axioms
repeat
    accept a derivation when all premise claims have lower closure rank
    accept its conclusion claim if not already accepted
until no claim changes
reject every remaining unsupported or cyclic derivation
```

Ordinary recursive source programs may produce cyclic Operation dependencies
during constraint solving. That does not authorize a cyclic certificate.
Induction-hypothesis availability is justified by the scoped eliminator rule,
not by a proof edge back to the conclusion being proved. Published derivations
therefore form a DAG even when solver dependencies were cyclic.

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

### P0-P10B: Closed Claims and residual obligations are distinct publication outcomes

An exported typed occurrence retains its source Operation ID, and its exported
classifier must be exactly the classifier selected for that Operation. Sharing
an erased Core root with another occurrence is never sufficient authority.

Publication has two explicit outcomes:

```text
closed export   = exact Operation + accepted grounded Claim
residual export = exact Operation + explicit reachable residual constraint
                  or pending runtime verification obligation
```

A residual export is not promoted to a Claim, and an unrelated residual
elsewhere in the artifact cannot justify it. Artifact v64 implements this
occurrence boundary. The later native Claim/Derivation schema must preserve the
same distinction rather than reconstruct it from Core terms.

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

### P0-P14: A derivation is constructed atomically over stable claim identities

The conclusion claim, rule-specific parameters, and irreducible source-claim
edges form one derivation node. They must be supplied together. Structural
premises are reconstructed from the exact conclusion Operation and its child
Operations; their `(Core Term, Context, classifier)` tuples are not copied as a
second authority.

There is no semantic late premise resolution. Artifact layout may relocate a
stable claim or derivation ID, but it may not select a claim by tuple search,
choose one derivation of that claim, mutate rule parameters, or repair an
earlier derivation.

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

### P0-P20: Evidence selection preserves the complete Claim authority

This is the first implementation invariant inside P0-R0A. It is not a
separate pre-P0 project and it is not deferred P1 work.

A proof-producing lookup may not return only a classifier. It returns either:

```text
SelectedEvidence
  Claim kind
  explicit authority kind and authority ID
  Context ID
  typed Operation ID when the authority is an Operation
  erased Core subject projection
  classifier
```

or an explicit `missing`/`ambiguous` result. "Latest candidate" is not an
evidence-selection rule. Two candidates that share the same Core subject and
classifier but have different Operation, ContextBinding, TypeDeclaration, or
other authority are distinct evidence.

Every candidate Derivation is then constructed atomically from:

- its complete conclusion Claim key;
- exact structural child Operations, with their Context retained before the
  final evidence authority is projected;
- exact source Claim keys for conversion, exposure, weakening, literal
  admissibility, linking, and other non-structural derived rules;
- immutable rule parameters.

Only after this information exists may Claims be interned and the least
grounded accepted closure be published. No later tuple scan may repair missing
authority or choose a premise.

### P0-P21: P0-R0A.1 is the re-entry premise for every later P0 phase

V2-P0 is already active. The next implementation phase is therefore not an
entry into P0 and not P1; it is `P0-R0A.1`. Here *premise* means a hard
implementation gate: no later P0 phase may consume the transitional candidate
tuple image as an accepted certificate.

`P0-R0A.1` is established only when every proof-producing path preserves:

```text
typed child occurrence or non-Operation authority
    -> SelectedEvidence(full Claim key)
    -> complete structural/derived premise
    -> candidate Derivation over Claim keys
```

Adding the record alone is insufficient. APP, Lambda, Match, constructor
spine, IH, CBPV boundaries, request, computation fold, conversion, weakening,
admissibility, linking, and Universe provenance must consume complete evidence
or be explicitly classified as non-proof-producing. A classifier-only lookup
at any proof-producing boundary keeps this gate open.

Only after this gate closes may P0 replace the publication image, remove the
late resolver, migrate the artifact schema, and continue with later P0 work.
This ordering prevents a new Claim/Derivation store from preserving guessed
tuple edges in a cleaner data structure.

### P0-P22: Solver-local obligations are not accepted Claims

The 2026-08-08 producer audit found a missing boundary not captured precisely
enough by P0-P13 or P0-P18. A constraint may legitimately classify a child
under a row variable, motive variable, or other assumption owned by its parent
rule. Such a classifier is useful solver state, but it is not an independently
replayable Claim.

The concrete counterexample is the higher-order operation handler fixture:

```text
clause Lambda Operation #34
  selected solver classifier = classifier #74
  classifier #74 contains an effect-row variable not owned by Operation #34

parent computation-fold Operation #46
  copies (#34, #74) as a required premise
```

The materializer correctly refuses to publish `#34 : #74` as a standalone
closed Claim. The fold producer nevertheless requires exactly that Claim, so
the grounded closure contained no Claim for the exported fold and the earlier
provisional artifact image wrote `judgements 0` and `proofs 0`. The legacy candidate graph
can still print a fold derivation, which demonstrates that successful solver
output and an accepted certificate are currently different facts.

P0 therefore separates:

```text
LocalObligation
  owner rule/constraint
  scoped variables and assumptions
  exact child Operation
  provisional classifier

ClosedDerivedClaim
  exact Operation or non-Operation authority
  closed/scope-owned classifier
  derivation discharging the LocalObligation under the parent rule

AcceptedClaim
  a ClosedDerivedClaim in the least grounded publication closure
```

For computation fold, checking a clause against the fold's expected carrier
must either:

1. produce an Operation-owned closed derived Claim for the clause Lambda at
   the specialized expected classifier; or
2. retain the clause check as an irreducible scoped parameter of the fold
   Derivation, with a validator that replays it under the fold constraint.

It must not copy an unowned residual classifier into an ordinary premise and
must not publish an authority-neutral helper as though it belonged to the
source clause Operation. The implementation must select one of the two forms
after comparing their artifact replay and recursion behavior; no tuple lookup
or late proof repair is allowed in either form.

This is the first P0 implementation gate together with P0-P20/P0-P21. Derived
conversion and weakening migration follows only after structural producers no
longer consume solver-local obligations as Claims.

#### P0-P22 implementation order and code targets

The next code change follows this order. Later steps must not be used to hide a
failure in an earlier step.

1. **Name the local state at the computation-constraint boundary.**
   Extend `prototype_judgement_computation_constraint` in
   `src/prototype/judgement.h` so each operand is either a closed selected
   evidence key or an explicitly scoped solver operand. Do not represent both
   with `premise_operations[]` plus `premise_classifiers[]` alone.
2. **Stop blind classifier copying.**
   Change `operation_solver_refresh_computation_constraint_operands()` in
   `src/prototype/ast.c`. It may refresh solver operands from OperationGraph,
   but it may mark an ordinary Claim premise ready only after closure/ownership
   succeeds. A classifier containing an unowned effect-row variable remains a
   local operand.
3. **Discharge fold clause obligations.**
   Change `solve_clause_computation_fold_constraint()` in
   `src/prototype/typing.c`. The already-computed
   `expected_outer_classifier` is the fold-specialized goal. Validate the
   exact clause Operation against that goal and emit either a closed
   Operation-owned derived candidate or an explicit fold-scoped parameter.
   Do not select the authority-neutral Lambda generated by
   `infer_lambda_classifier_for_app_argument()` as a source occurrence.
4. **Make Lambda materialization total over its declared result kind.**
   Keep the corrected `operation->body` structural edge in
   `operation_solver_reify_core_proof()`. Replace the current silent positive
   return for a required exact producer with a typed result distinguishing
   `closed`, `local residual`, and `invalid`. Parent fold construction may
   consume `local residual` only through step 3, never as a Claim ID.
5. **Publish only closed candidates.**
   Update candidate construction in `src/prototype/typing.c` so
   `add_delta_relation_with_explicit_premises()` is never called with a local
   operand disguised as a premise tuple. Grounding then remains a publication
   check, not the first place the category error is discovered.
6. **Guard exported roots.**
   Before artifact slicing in `artifact_mark_roots()` in
   `src/prototype/ast.c`, reject an exported typed Operation for which no
   accepted authority-matching Claim exists, unless that exact Operation owns
   an explicit residual constraint or verification obligation. A residual
   export remains residual and is not treated as a Claim.
7. **Add adversarial regression.**
   Extend `test_cbpv_surface.sh` to require non-zero accepted proof closure for
   `higher_order_operation_handler_check.p`, then mutate the serialized
   operation identity and require readback rejection. The test must mutate all
   matching sparse Term slots, as a sparse fixture may contain
   structurally duplicated row atoms.
8. **Only after this gate, continue authority migration.**
   Migrate generated Core helpers, conversion, Context/effect weakening,
   integer admissibility, link authority, and Universe provenance. Then remove
   dead `prototype_judgement_resolve_proof_edges()`,
   `prototype_judgement_delta_drop_temporary_derivations()`,
   `premise_proof_ids`, and transitional candidate proof IDs.

Required invariant after step 7:

```text
every exported typed Operation
  -> at least one accepted authority-matching Claim
  -> at least one grounded Derivation
  -> every source Claim edge is closed or explicitly scoped by its rule
```

This order deliberately puts fold/Lambda closure before conversion and
weakening cleanup. The latter are real P0 debt, but they cannot establish a
publication boundary while a structural parent cites a premise that the
kernel correctly refuses to publish.

At the current v62 checkpoint, an identity COPY is solved when its Operation
subtree is internally constructed or its row variables are scoped by that
Operation. It remains residual when the subtree contains an unresolved
`EXTERNAL_REF`. A later representation should give the external obligation its
own explicit kind instead of continuing to overload COPY.

### 3.1 P0 first slice: required refactoring direction

P0 has already begun. Its first slice establishes this construction boundary;
it does not add equality terms or begin by changing proof payload tags. This is
P0 itself, not an optional pre-P0 compatibility project.

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
   publication boundary and receives interned Claims plus immutable
   Derivations only after the fixed point is stable.
4. One selected synthetic classifier per Operation is distinct from the set of
   admissible or derived typings. One Claim may have multiple immutable
   Derivations. A premise refers to the Claim, not to an insertion-order-selected
   Derivation.
5. Effect-row equations use canonical finite sets plus unresolved atoms as
   solver state. TermDB row syntax is only input/output representation.
   Higher-order operation atoms stay opaque: residual handling may remove an
   atom through its declared operation label, but cannot expose latent effects
   from the thunk carried by that atom.
6. Generic TermDB inference may remain only as an explicitly Core-level helper.
   It creates authority-neutral candidates and cannot publish evidence for a
   source Operation until that evidence is reified through the exact Operation
   and Context.
7. A published Claim must be in the least grounded closure of locally valid
   Derivations. Existence of a cyclic tuple graph is not evidence.
8. Artifact relocation and linking may construct a new unpublished certificate
   image. They may not mutate semantic fields in an already published
   in-memory certificate.
9. A solver-local classifier containing assumptions owned by a parent rule is
   not a Claim premise. The parent producer must discharge it into a closed
   derived Claim or retain an explicitly scoped, replayable rule parameter.
10. A proof-producing producer that cannot close one of its exact structural
    operands is a diagnosed P0 closure failure. The materializer must not
    silently skip it while a parent candidate continues to cite it.

These are not advisory design preferences. They are the **first internal
invariants of P0-R0A** for every implementation item below:

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
- the fixed-point frontier may emit only unpublished candidates; publication
  interns the conclusion Claim and appends an immutable Derivation;
- no accepted premise edge is reconstructed from a non-unique tuple or from
  the first/latest physical proof record;
- claim closure, not proof-record acyclicity alone, determines whether recursive
  evidence is grounded;
- `operation.classifier` is the one selected synthesis result, while literal
  admissibility, conversion, effect weakening, context reindexing, and export
  exposure are separately justified derived boundaries.

Any P0 implementation which requires violating one of these items must stop and
revise this plan rather than add another compatibility lookup or mutable repair
path.

Current first-slice state after re-reading the implementation:

| Boundary | State | Remaining work in P0 |
| --- | --- | --- |
| Operation ownership and exact premise edges | partial | Operation IDs exist for several structural producers, but Lambda materialization and fold clause solving still confuse a local residual classifier with a closed premise |
| Candidate/commit separation | partial | Delta is provisional and grounded compaction exists, but candidate relation/proof storage remains the producer and artifact image |
| Claim/Derivation separation | transition scaffold implemented; not authoritative | accepted Claim/Derivation arenas and grounded closure exist, but are reconstructed from the legacy one-relation/one-proof candidates |
| Atomic derivation construction | missing at producer boundary | discharge scoped solver obligations, preserve complete selected evidence and exact source Claim keys, then remove semantic late resolution |
| Canonical effect-row fixed point | complete for the current row language | add direct stability assertions and retain free external atoms as explicit residuals |
| Operation structural validation | implemented for all current Operation tags | consolidate the older `prototype_operation_graph_validate()` checks with `prototype_judgement_validate_operation_typing()` during P0.3/P0.7 |
| Non-Operation authority separation | P0.5 work | split binder, declaration, type-formation, and Universe queries from generic `subject` lookup |
| Derived typing boundaries | P0.4 work | finish ASCRIPTION/exposure, literal specialization, weakening, and reindex provenance |

Therefore the current project phase is **V2-P0**. Earlier work implemented the
Operation-indexing part of P0-R0, but did not complete its certificate boundary.
The next ordered work is P0-R0A below, followed by a revalidation of P0.1-P0.7.
These are P0 tasks; P1 must not begin while the accepted certificate still
depends on duplicated relations or late tuple reconstruction.

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
| `src/prototype/typing.c:add_complete_relation` | appends a duplicated relation/proof pair and clears premise proof IDs | replace with Claim interning plus immutable Derivation insertion |
| `src/prototype/typing.c:add_complete_delta_relation` | stores an unpublished relation/proof candidate but still uses the hybrid shape | preserve rebuild/discard semantics while changing the candidate to explicit Claim keys and exact source Claims |
| `src/prototype/typing.c:prototype_judgement_add_conversion` | Core-level derived classifier chain | replace with explicit typed-boundary conversion API |
| `src/prototype/typing.c:prototype_judgement_validate_operation_typing` | validates current Operation tags against exact graph/context/declaration authorities | retain as the structural certificate entry point and consolidate duplicate graph validation |
| `src/prototype/universe.c:lookup_authority_classifier` | obtains an authority-neutral classifier for a Core Term | replace source-boundary callers with explicit selected Operation/type-view classifiers |
| artifact judgement read/write/relocation in `src/prototype/ast.c` | serializes the hybrid relation/proof pairing, duplicated conclusions, and resolved proof IDs | change only after the P0 in-memory Claim model is stable |

## 5.1 P0 Entry Re-audit: Findings and Disposition

The 2026-08-08 re-audit confirmed that Operation ownership fields were added,
but the accepted certificate still performs semantic tuple selection. P0 has
started, but its construction boundary is not complete.

| Re-audit finding | Current disposition | Remaining P0 action |
| --- | --- | --- |
| Core-tuple premise lookup could borrow another occurrence's proof | structural rules can compute an expected child Operation, but accepted premise IDs are still found by first/latest tuple search | remove semantic late resolution; structural dependencies come only from exact OperationGraph edges |
| One physical relation points to one proof | insertion preserves alternatives by duplicating the relation | intern one Claim and attach zero-or-more immutable Derivations in P0-R0A |
| Structural premise arrays could lose the selected occurrence | computation constraints retain exact child Operations, but proof insertion clears premise IDs before later reconstruction | retain exact Claim authority through candidate validation and commit; never reconstruct it from tuples |
| Ascription compatibility was conflated with conversion | `EXPECTED_TYPE_EXPOSURE` is distinct from kernel `CONVERSION` | complete Universe/effect constraint provenance in P0.4 |
| Fold/request dependencies lacked exact typed operands | constraints retain child Operation IDs and canonical effect-row solver state | retain tag-specific dependency validation and add stability characterization |
| Rule payloads were mutated after insertion | most rule parameters are now supplied at insertion, but premise identity is still repaired after insertion | make Derivations atomic and remove the repair pass |
| Generic all-Term inference had ambiguous authority | it is now named `prototype_judgement_delta_infer_core_helper_facts`, forces invalid Operation authority, and verifies every emitted fact remains neutral | preserve it only as motive/type-synthesis support; exact Operation reification is mandatory before publication as source evidence |
| Alpha-interned Lambda/Match representatives do not preserve occurrence binder/child IDs | OperationGraph now stores direct Lambda `binding_id`; structural validation follows exact Operation edges and uses alpha-stable Core projection checks | consolidate duplicate raw-ID checks in `prototype_operation_graph_validate()` during P0.3/P0.7 |
| Non-Operation facts still share generic Term-subject storage | declaration-specific record APIs exist, but binder and Universe consumers still use generic relation lookup | finish the authority split in P0.5 |
| Universe collection calls `lookup_authority_classifier(core_term)` | it can still select an authority-neutral classifier by erased Core identity | pass explicit selected Operation/type-view or declaration/type-formation inputs in P0.5 |

The immediate refactoring order is therefore:

```text
Operation-indexed constraints and operands
    -> Claim/Derivation accepted-model separation
    -> solver/evidence-candidate separation
    -> grounded atomic publication
    -> exact structural Operation and derived Claim dependencies
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
- [ ] Replace the physical relation/proof pairing with separate Claim and
  Derivation identities. Appending a derivation must not duplicate its Claim.
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
- [ ] Introduce one evidence-candidate builder whose input contains a complete
  conclusion Claim key, exact derived source Claim keys, and rule payload.
- [x] Keep evidence candidates outside the committed relation/proof arrays
  until their Operation classifier, Context bindings, and rule premises are
  stable for the current fixed point.
- [ ] At commit, intern the Claim once and preserve every distinct validated
  Derivation; do not commit failed, superseded, open, or ungrounded candidates.
- [x] Replace constructor-spine and match-pattern post-insertion payload writes
  with the complete candidate builder.
- [ ] Make delta commit construct complete immutable Derivation nodes and
  publish only the least grounded claim closure.
- [ ] Remove semantic late proof-ID resolution. Relocation may translate stable
  IDs only; it may not choose a premise by tuple.
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

### P0-R0A Normalize Claim and Derivation ownership

This is the immediate implementation slice. It precedes every remaining P0
check and is not a P1 storage optimization.

#### P0-R0A first invariant: authority-complete evidence flow

P0-R0A cannot proceed to authoritative Claim/Derivation storage by only
replacing relation/proof arrays. Every producer
must first stop separating a classifier from the authority that selected it.
The re-audit against the `062b7cd` checkpoint found two concrete failures:

- the request-internal APP helper knew its source child Operations in
  `ast.c`, but the APP recording API accepted only Core subjects and
  classifiers;
- a computation-body effect weakening was generated in a different Context
  from the actual shared-Core binder assumption, and tuple lookup could only
  hide that mismatch by inventing an edge.

The P0 premise is therefore:

```text
classifier selection
    -> SelectedEvidence(full Claim key)
    -> complete candidate Derivation
    -> Claim interning
    -> least grounded atomic publication
```

For structural rules, use a descriptor that retains the direct child
Operation, the child's Context, and the final evidence authority separately.
Do not map VAR or constructor children to `INVALID` before their Context and
binding/declaration authority have been captured.

- [x] Add one authority-complete evidence selection/result record.
- [ ] Migrate classifier lookup APIs used by proof-producing code to return
  that record, not a classifier Term ID alone.
- [x] Preserve the structural-premise descriptor fields: direct child
  Operation, child Context, authority kind/ID, subject projection, and
  classifier. These fields currently live in the candidate Derivation rather
  than a second wrapper type; adding another tag-bearing structure is not a P0
  requirement.
- [x] Migrate APP first, including request-internal helper APPs, because this
  is the smallest reproduced authority-loss path.
- [x] Migrate source Lambda, Match, constructor spine, request, computation
  fold, and source CBPV structural premises. IH deliberately remains a
  premise-free scoped eliminator rule and stores the exact Match motive as an
  immutable rule parameter; adding the parent Match Claim as a premise would
  create a Match -> branch -> IH -> Match cycle.
- [ ] Migrate authority-neutral generated RETURN/THUNK/Lambda/APP/Match helper
  paths. They still call `add_delta_relation_with_premises()` with only Core
  tuples and therefore cannot become accepted evidence by guessing an
  Operation authority.
- [ ] Migrate conversion, expected-type exposure, Context weakening, effect
  weakening, integer admissibility, declaration linking, and Universe
  constraints as explicit derived-source boundaries.
- [ ] Reject no-op derived candidates that become self-premises after
  canonicalization.
- [ ] Delete all unique/first/latest Core-tuple authority recovery.
- [ ] Add adversarial tests for shared Core under distinct Contexts and
  authorities before replacing the publication image.

Re-audited implementation state:

- [x] APP recorders retain selected function and argument Operation IDs.
- [x] direct structural child Context is retained before VAR/constructor
  authority is projected to ContextDB/TypeDeclarationDB.
- [x] Lambda binder premises are classified as ContextBinding authority.
- [x] final generic tuple-pruning was removed from ordinary publication after
  it deleted a valid zero-clause computation fold.
- [x] authority-ambiguous Core-helper candidates remain unpublished rather
  than borrowing one typed occurrence.
- [x] legacy one-to-one candidate coverage and whole-candidate-graph
  acyclicity no longer override the least grounded accepted closure.
- [x] add the first-class `prototype_judgement_selected_evidence` record and
  use it for generic APP candidate collection and selected CBPV child evidence.
- [x] keep a structural child Operation ID distinct from selected evidence
  authority; a VAR or constructor child is not relabelled as Operation-owned.
- [ ] replace every remaining proof-producing classifier-only lookup with
  `SelectedEvidence`. Source structural producers and the conversion,
  expected-exposure, Context-weakening, effect-weakening, and integer-
  admissibility boundaries are migrated. Generated Core-helper and generated
  Match/type-formation paths still create tuple-only premises.
- [x] make IH validation independent of a global classifier scan. The IH
  candidate stores `induction_motive` and validates `M recursive_argument`
  locally while remaining premise-free.
- [ ] finish generated Lambda/RETURN/THUNK/APP/Match helper migration. These
  facts must use explicit authority-neutral Core-helper Claims, or remain local
  solver obligations. They may not inherit `current_operation_id` and may not
  be promoted by matching a source Core tuple.
- [ ] make candidate Derivations refer to complete candidate Claim keys rather
  than repaired proof IDs.
- [ ] validate and publish only the accepted Claim/Derivation image.

This checklist is the implementation premise of P0-R0A.1 and P0-R0A.2. P1 and
object-level Higher Observational equality remain blocked until it is complete.

The code-level migration inventory is:

| Boundary | Current authority loss | Required P0 form |
| --- | --- | --- |
| `lookup_classifier()` / `lookup_delta_proven_classifier()` | returns only classifier | return one complete selected Claim candidate or explicit ambiguity |
| `collect_subject_classifiers()` and APP candidate collection | deduplicates classifier values | retain evidence identity for each function/argument candidate pair |
| `prototype_judgement_delta_record_app_elim()` | accepts Core subject and classifier only | accept complete function and argument evidence; request helper uses the same API |
| source Lambda/Match/constructor/CBPV recorders | migrated to direct child Operations and Contexts | preserve this path while Claim IDs replace tuple payloads |
| scoped IH rule | formerly recovered motive by global Match classifier search | keep zero premises; store and validate the exact motive as a rule parameter |
| generated Core helper recorders | RETURN/THUNK/Lambda/APP/Match premises contain Core tuples only | either provide exact generated authority or keep the helper unpublished; never borrow a source Operation |
| conversion, exposure, Context/effect weakening, integer admissibility | recorder APIs now consume `SelectedEvidence`, but storage is still a transitional tuple | preserve that exact source key when candidate premises become Claim IDs |
| linked declaration completion | scans global Core support | consume relocated export/declaration authority |
| Universe constraint insertion | keeps inequality but loses selected occurrence | keep authority/provenance beside the constraint reason |

Implementation order is strict:

1. add characterization tests for request APP and shared-Core/different-Context
   binder premises;
2. introduce the evidence and structural-premise records without changing
   accepted publication;
3. migrate APP end to end and restore all source tests;
4. preserve the completed source Match, constructor spine, scoped IH,
   Lambda/CBPV, request, computation-fold, and local-obligation boundaries;
5. migrate generated Core helpers and generated Match/type-formation paths,
   then preserve the already-selected source evidence as Claim IDs at every
   derived boundary; migrate link authority and Universe provenance afterward;
6. replace candidate tuple edges with Claim IDs and atomic grounded
   publication;
7. remove the old resolver and one-to-one coverage invariant;
8. migrate the artifact schema only after the in-memory model is final.

#### P0-R0A.0 Transition checkpoint implemented on 2026-08-08

- [x] Add separate in-memory `prototype_judgement_claim` and
  `prototype_judgement_derivation` arenas without changing TermDB identity.
- [x] Keep Claim identity independent of rule/Derivation identity.
- [x] Reconstruct source Claim edges from the exact resolved proof IDs at the
  existing publication boundary, then compute a least grounded closure rank.
- [x] Reject a reconstructed certificate containing an unsupported cycle.
- [x] At committed checkpoint `985baf8`, preserve the existing source, CBPV,
  shared-Core, artifact, and HOTT substrate behavior; all 15
  `src/prototype/test_*.sh` scripts and examples 01-07/09 pass with the
  occurrence-preserving v64 boundary. Native accepted Claim/Derivation
  serialization remains a P0 exit condition.
- [x] Rename the generated ancestor-context rule from `CONTEXT_REINDEX` to the
  semantically accurate `CONTEXT_WEAKEN` and validate ancestry.
- [x] Replace unexplained fold/premise capacity literals with named constants.
- [ ] Remove the transitional relation/proof arrays. They remain the input to
  Claim reconstruction in this checkpoint.
- [ ] Remove `prototype_judgement_resolve_proof_edges()`. Its source/artifact
  call sites are gone, but the dead implementation and legacy proof-ID fields
  remain.

This checkpoint deliberately does not intern Claims during each solver delta
commit. At that point premise candidates may still appear later in the same
delta, NAME/ASCRIPTION source authority has not been uniformly propagated, and
historical `INVALID` authority can mean either a real neutral fact or missing
information. Building accepted edges there would reintroduce tuple inference.

The current transition order is:

```text
candidate generation
    -> legacy atomic candidate commit
    -> OperationGraph-aware legacy edge resolution
    -> Claim/Derivation reconstruction
    -> grounded closure validation
```

The required P0 end state is:

```text
authority-complete candidate generation
    -> intern all candidate Claims
    -> resolve candidate Derivations by exact Claim IDs
    -> atomic grounded publication
```

The first order is a migration scaffold. The second order is the P0 completion
condition and must replace it without a compatibility resolver.

#### P0-R0A.1 Local-obligation and closed-Claim boundary

This premise phase is complete for the current calculus. Its invariants remain
mandatory preconditions of every following P0 change.

- [x] Distinguish scoped solver operands from Claim premises.
- [x] Discharge fold clause Lambda residuals under the fold carrier.
- [x] Reject exported roots without an accepted authority-matching Claim or an
  explicit reachable residual obligation.
- [x] Pass the higher-order handler forged-artifact regression.

#### P0-R0A.2 Authority-complete producer boundary

This phase is complete for the current calculus. It is not P1 and not optional
cleanup. Its
order is mandatory:

```text
SelectedEvidence API
    -> structural and derived producers retain complete Claim keys
    -> candidate Derivations refer to Claim keys
    -> accepted Claims and Derivations become the sole publication image
```

The existing accepted arenas are a useful scaffold, but they do not satisfy
this phase while they are rebuilt from `premise_proof_ids` after a resolver.

Entry premises, checked against `985baf8`:

1. R0A.1 remains true: a closed exported occurrence has an accepted grounded
   Claim, while an unfinished occurrence has an explicit reachable residual or
   verification obligation. The two outcomes are never encoded as one Claim.
2. The conclusion authority is supplied before candidate insertion. A helper
   constructor may not infer it from `proof_kind`, `current_operation_id`, or a
   matching Core tuple after insertion.
3. Every premise-bearing producer receives a complete selected Claim key. A
   classifier alone is solver information, not evidence.
4. Source structural rules follow OperationGraph child edges. Generated motive
   and type helpers use `CORE_HELPER` authority and exact helper premises; they
   do not borrow the enclosing source Operation.
5. Context-binding, declaration, intrinsic, type-formation, and Universe facts
   retain their own authority domains. `INVALID` is an error or absence marker,
   not a spelling of authority-neutral evidence.
6. No accepted-model rewrite begins until the legacy producer image passes the
   full suite with these authority invariants. This prevents Claim interning
   from freezing incorrectly inferred authority.

Concrete R0A.2 producer inventory at this checkpoint:

| Producer family | Current state | R0A.2 action |
| --- | --- | --- |
| source APP/Lambda/Match/constructor/CBPV/request/fold/IH | exact occurrence evidence implemented | preserve and add authority-forgery regressions |
| conversion/expected exposure/Context weakening/effect weakening/int admissibility | consumes `SelectedEvidence` | retain the exact source as a Claim ID in R0A.3 |
| generated Pi helper RETURN/Lambda/THUNK | partially explicit `CORE_HELPER` | migrate the remaining classifier-only fallback path |
| generated Match motive Lambda/RETURN/THUNK/APP | tuple-only premises remain in `prototype_judgement_delta_build_match_motive()` | select exact generated evidence and state `CORE_HELPER` authority at construction |
| generated Match type-formation and Match elimination | classifier-only branch selection remains in `prototype_judgement_delta_expand_match_motive_with_premises()` and `prototype_judgement_delta_expand_match*()` | split source-occurrence recording from authority-neutral helper inference; reject ambiguity |
| generic inferred Lambda-at-Universe fallback | tuple-only premises remain in `prototype_judgement_delta_ensure_type_at_universe()` | consume the exact binder/body evidence already generated in their real Contexts |
| no-premise introductions | stored authority is currently filled by fallback classification | pass the declaration/intrinsic/type-formation/Universe/Core-helper authority explicitly |

The migration must not be implemented as a blind replacement of every old
helper call. In particular, Match APIs currently serve both source occurrence
recording and authority-neutral generated inference. Those paths must be split
at their caller boundary first; otherwise a generated Match can accidentally
inherit the source Operation that requested motive synthesis.

- [x] Add a transitional interned Claim record whose
  key is `(kind, authority, context_id, subject_projection, classifier)`.
- [x] Add explicit authority kind/ID fields to candidate and accepted Claims.
- [x] Make every premise-bearing proof producer consume `SelectedEvidence` or
  an explicitly authority-neutral helper Claim; do not select a candidate by
  reverse/forward insertion order.
- [x] Remove the classifier-only `add_delta_relation_with_premises()` producer
  path after migrating its generated Lambda, RETURN, THUNK, APP, Match motive,
  Match elimination, and type-formation callers.
- [x] Require generated helper conclusions to state `CORE_HELPER` authority
  explicitly. `INVALID` authority means invalid/missing data, never neutral.
- [x] Keep generated helpers unpublished when their premises are ambiguous or
  solver-local; never recover a source Operation from `(Core, Context, type)`.
- [x] Represent authority completely at every producer. Operation-backed claims
  carry an exact `operation_id`; Context, declaration, type-formation,
  intrinsic, and Universe claims use their own authority kind and ID rather
  than `INVALID` as an ambiguous fallback.
- [ ] Replace `proof_kind/proof_id` ownership on a candidate relation with a
  one-to-many Claim-to-Derivation adjacency. An accepted Claim has at least one
  grounded Derivation; zero-derivation entries remain unpublished candidates
  only.
- [x] Add a transitional accepted Derivation record with conclusion and source
  Claim IDs and least-closure rank.
- [x] Store each authoritative Derivation as `(conclusion_claim_id, rule_kind,
  rule_parameters, derived_source_claim_ids)`.
- [ ] Do not duplicate APP/Lambda/Match/IH/CBPV structural child tuples in the
  Derivation. Reconstruct them from the conclusion OperationGraph node.
- [ ] Retain the erased Core subject only as a validated projection for
  normalization, diagnostics, and artifact reachability.

The completed implementation also preserves two identities for structural
premises instead of conflating them: `premise_operations[]` remains the direct
OperationGraph child, while `premise_evidence[]` carries the selected Claim
authority. This distinction is required for NAME/type-formation/intrinsic
children whose evidence authority is not their direct child Operation. CBPV
boundary reification now starts from the child Operation classifier and records
an explicit conversion when the selected parent classifier uses a convertible
universe representative; it no longer inverts the parent classifier and then
guesses child evidence by a Core tuple.

#### P0-R0A.3 Candidate generation and publication

- [x] Keep solver candidates separate from accepted Claims and Derivations.
- [x] Generate source APP, Lambda, constructor-spine, request, fold, and solved
  Match candidates from exact child Operation IDs and child Contexts.
- [ ] Generate every remaining structural candidate from exact Operation IDs, never from a
  JudgementDB tuple lookup.
- [x] Make conversion/exposure, integer admissibility, effect weakening, and
  context weakening consume exact source `SelectedEvidence` before commit.
- [x] Replace their transitional premise tuple with the interned source Claim
  ID when accepted Claim publication becomes authoritative.
- [x] Intern Claims deterministically and append all distinct valid
  Derivations reached within the configured resource budget.
- [x] Deduplicate Derivations by semantic rule payload and source Claim IDs;
  do not compare or discard edges by proof insertion order.
- [x] Publish atomically after classifier, effect-row, Context, and authority
  validation has stabilized.

The accepted image now retains premise order separately from its deduplicated
closure-source set. Ordinary premises are accepted Claim IDs. Fold-clause
local classifiers and the generated request APP are scoped rule parameters,
not globally publishable Claims. Final rule validation iterates accepted
Derivations and reconstructs a temporary validator view from the conclusion
Claim and ordered premise Claim IDs; it no longer chooses validation work from
candidate `accepted` flags. This exposed and fixed two producer defects: sparse
artifact holes were incorrectly publishable, and request/fold local premises
could retain `UNKNOWN` evidence.

The remaining R0A.3 blocker is representation removal, not validation logic:
candidate Claims still physically own one `proof_kind/proof_id`, and accepted
Derivations still carry `source_candidate_proof_id` solely because artifact
v64 reachability and serialization return to the legacy candidate graph.

#### P0-R0A.4 Grounded closure

- [ ] Classify declaration, intrinsic, Context-binding, and other primitive
  introductions as explicit closure seeds.
- [x] Validate every local Derivation rule against its authoritative graph or
  database.
- [x] Compute the least fixed point of reconstructed Claims justified by seeds or by a
  Derivation whose premise Claims have lower closure rank.
- [x] Accept only reconstructed Derivations respecting that rank, so the proof graph
  is a DAG.
- [x] Reject unsupported strongly connected components, including a forged
  pair of Claims that justify only each other.
- [ ] Validate recursive source programs through the scoped IH/eliminator rule,
  never through a back-edge to the conclusion Claim.

#### P0-R0A.5 Consumer migration order

- [ ] Migrate construction and validation in `src/prototype/typing.c` first.
- [ ] Migrate Operation solver materialization and expectation checks in
  `src/prototype/ast.c` second.
- [ ] Migrate Universe collection to consume exact Claim/Operation provenance.
- [ ] Migrate artifact mark/write/read/append/link only after the in-memory
  model and closure algorithm are stable.
- [ ] Remove `prototype_judgement_resolve_proof_edges()`; no compatibility
  resolver or old relation/proof representation remains.

The immediate next code change remains the first item, now scoped from the
implemented evidence/APP slice as:

1. close the fold/Lambda local-obligation boundary described by P0-P22;
2. finish migrating authority-neutral generated helper construction and
   derived rules from classifier-only selection to exact Claim keys; IH motive
   validation is already local and must remain premise-free;
3. make every migrated candidate Derivation retain the selected Claim key,
   rather than projecting back to a tuple before insertion;
4. propagate exact source identity through NAME, ASCRIPTION, conversion,
   expectation exposure, weakening, literal admissibility, and link completion.
   In particular, `prototype_judgement_delta_record_context_weaken()`,
   `prototype_judgement_delta_record_effect_weaken()`, and
   `prototype_judgement_delta_add_conversion()` must consume the selected
   source Claim, not manufacture a premise from `current_operation_id`;
5. intern every Claim in an atomic candidate batch before attaching any
   Derivation;
6. rewrite validation to traverse accepted Claim IDs rather than candidate
   `premise_proof_ids`;
7. delete the dead tuple-based proof-edge resolver, temporary-pruning helper,
   and legacy tuple/proof-ID fields;
8. replace provisional v64 candidate serialization with native accepted
   Claim/Derivation serialization only after this in-memory boundary is final;
   continue rejecting v62 without a compatibility reconstruction path.

This sequence is the premise for the remaining P0 phases. Artifact migration,
Universe provenance cleanup, and HOTT witness work must not bypass it.

#### P0-R0A.6 Characterization and adversarial tests

- [ ] One Core identity Lambda used as Bool and Nat remains two Operation
  Claims and cannot lend evidence across occurrences.
- [ ] One Claim accepts two Derivations in either insertion order, and a parent
  premise remains a reference to that Claim.
- [ ] A forged same-Term/same-Context/same-classifier different-Operation
  premise is rejected.
- [ ] A forged context movement from a non-ancestor Context is rejected.
- [ ] A forged effect weakening or integer admissibility source Claim is
  rejected.
- [ ] An unsupported derivation cycle is rejected; recursive source typing is
  accepted through a finite scoped-IH derivation DAG.
- [ ] Artifact round-trip and append preserve Claim/Derivation identity exactly
  after the later schema migration.
- [ ] A fold clause carrying a parent-owned effect-row residual is discharged
  into replayable evidence; its exported artifact has non-zero accepted Claim
  and Derivation closure and rejects a forged operation identity.

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
- [ ] Re-run a multiple-derivation test on the normalized Claim model, showing
  that two validated derivations of
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
- [ ] Make committed derivation insertion append-only or otherwise immutable;
  do not infer semantic proof uniqueness from a storage tuple.
- [ ] Represent physically the logical one-to-many relation from a typed Claim
  to its accepted Derivations. This is P0 correctness, not deferred P1 layout.
- [ ] Require complete rule payload and exact derived source Claim authority at
  insertion; expose
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
- [ ] Remove late proof-edge resolution. Persisted non-structural evidence must
  carry its exact source Claim before publication.
- [ ] Remove relation/proof conclusion duplication and the physical
  one-relation/one-proof coverage invariant.
- [x] Remove every Operation-owned call to
  `operation_solver_reindex_existing_proof()` and
  `operation_solver_lookup_proven_classifier()`.

### P0.7 Validation and artifact transition

- [x] Validate each Operation's Context, Core projection, selected classifier,
  polarity, and typed child references.
- [x] Validate derived boundaries against their source Operation/fact.
- [x] Validate non-Operation facts against their explicit authority.
- [ ] Replace physical proof-node acyclicity with grounded Claim closure over
  locally valid Derivations.
- [ ] Choose the next artifact representation only after the in-memory Claim
  model is stable; artifact v62 is not the final P0 certificate schema.
- [ ] Update mark, write, read, relocation, append, and link logic once; retain
  no fallback that guesses whether an integer is a Term ID or Operation ID.

### P0.8 Re-audit P1

- [x] Enumerate evidence fields still not derivable from OperationGraph,
  ContextDB, TypeDeclarationDB, UniverseDB, or kernel replay.
- [x] Decide whether any tagged rule payload remains necessary.
- [x] Decide whether any variable-size premise arena remains necessary.
- [x] Treat fixed premise capacity as general implementation capacity debt, not
  automatically as a HOTT blocker.
- [ ] Re-run this audit after P0-R0A. The 2026-08-08 audit showed that the
  proposed claim/derivation work belongs to P0 itself.

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
- Claim/Derivation storage and exact derived-boundary sources are reclassified
  as P0-R0A because they are required to make Operation ownership true.
- P1 remains intentionally unspecified until P0-R0A is complete. The former
  plan to move every structural rule into a tagged payload and variable-size
  premise arena remains cancelled.

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
16. one physical accepted Claim is interned independently of its one-or-more
    grounded Derivations;
17. accepted dependencies refer to exact Claims or are reconstructed from exact
    OperationGraph edges, never selected by first/latest tuple search;
18. the least grounded closure rejects unsupported cyclic evidence;
19. `prototype_judgement_resolve_proof_edges()` and the one-relation/one-proof
    coverage invariant no longer exist.

## 9. Progress Record

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-07 | P0.0 code re-audit | complete | Verified Operation-indexed solver, Term-indexed materialization, mixed non-Operation facts, and integer literal overload counterexample. |
| 2026-08-07 | P0 premise correction | complete | Removed mandatory one-certificate, conversion-budget serialization, tagged payload, and premise-arena assumptions. |
| 2026-08-07 | P0 internal-premise code re-audit | complete | Found remaining Core-keyed proof borrowing, relation/derivation identity conflation, expected-type/conversion conflation, and a fold return-edge dependency error. Added them as mandatory P0 premises. |
| 2026-08-08 | P0 reopened after certificate audit | active | Each transitional `prototype_judgement_claim_candidate` still owns one candidate proof; premise proof IDs are cleared at insertion and recovered by first/latest tuple search. P0-P10, P0-P14, and P0-P20 are not satisfied. |
| 2026-08-08 | P0-R0A transition scaffold | complete | Separate accepted Claim/Derivation arenas and least grounded compaction exist, but are reconstructed from transitional candidate relation/proof records. |
| 2026-08-08 | P0-R0A.1 local-obligation boundary | complete for current calculus | Scoped fold/Lambda obligations are not published as standalone Claims; exact fold rule parameters are replayed locally. Export publication distinguishes grounded Claims from exact occurrence-owned residual constraints. |
| 2026-08-08 | P0-R0A.2 authority-complete producer migration | complete for current rules | Generated Pi/Match/motive helpers state `CORE_HELPER` authority and consume exact evidence; no-premise introductions state their authority domain; computation constraints retain direct child Operations separately from selected evidence authority. All 15 prototype scripts pass after NAME/type-formation/intrinsic authority and CBPV universe-conversion reification were corrected. |
| 2026-08-08 | P0-R0A.2 final entry audit at `985baf8` | complete | Reclassified the remaining work precisely: derived recorder APIs already consume `SelectedEvidence`; generated motive/type helpers and no-premise authority fallback are the immediate implementation targets. P0-R0A.2, not P1, is next. |
| 2026-08-08 | P0-R0A.1 evidence API and APP slice | partial; implemented | Generic APP candidate collection retains function/argument evidence; Lambda and CBPV APIs retain direct child Operations separately from evidence authority. Normalization-equal and proof-kind selection now return evidence instead of classifier-only results. |
| 2026-08-08 | P0-R0A.1 structural producer slice | implemented for current fold rules | Fold clause Lambdas and the request-internal APP are scoped rule parameters replayed by their enclosing validators, not independent Claims. Return-body effect weakening retains the exact body Operation, occurrence Context, and Core projection rather than reconstructing them from an alpha-interned Lambda representative. The CBPV surface suite, including higher-order handlers and forged artifacts, passes. |
| 2026-08-08 | P0-R0A.1 fixed-point publication boundary | implemented | Operation-owned source candidates are publishable only when their classifier equals the final Operation classifier; explicit conversion, weakening, exposure, and admissibility rules remain allowed derived classifiers. Superseded solver-frontier candidates are retained as history but cannot become Claims. |
| 2026-08-08 | P0-R0A.1 fold effect authority | implemented | Operation-owned folds compute clause rows from their exact clause Operation classifiers. Global candidate scanning remains only for authority-neutral Core inference. Local clause/resumption rows require inclusion in the carrier; the validator separately checks that their exact union equals the final fold row. |
| 2026-08-08 | P0 authority validator probes | partial; uncommitted | Context movement was narrowed to ancestor weakening, context/effect premise Operation ownership was tightened, and fold capacities received named constants. A premature Operation-only integer check broke authority-neutral Core helper facts and was removed; exact integer source Claims must wait for P0-R0A. |
| 2026-08-08 | P0.1 characterization tests | partial; revalidation required | Existing shared-Core and solver tests pass, but insertion-order-independent Claim derivation and unsupported-cycle tests do not yet exist. |
| 2026-08-08 | P0.2 authority-specific APIs | partial; revalidation required | Operation APIs retain exact occurrence IDs, but accepted non-Operation Claims still encode authority through generic relation fields and `INVALID`; P0-R0A must make the distinction explicit. |
| 2026-08-07 | P0-R0 Operation-indexing boundary | partial | Exact Operation operands and provisional solver state are implemented. Accepted Claim/Derivation identity and grounded publication remain P0-R0A blockers. |
| 2026-08-07 | P0-R0 provisional binder cleanup | complete in working tree | Pending binder assumptions now use the current ContextDB classifier and stale assumptions are removed before commit; `higher_order_operation_force_once_check.p` passes. |
| 2026-08-07 | P0-R0 exact computation operands | complete in working tree | Request/fold constraints retain child Operation IDs, refresh classifiers from the Operation solver, and expose an explicit solved classifier. Proof history is no longer their solver input. |
| 2026-08-07 | P0-R0 effect-row solver | complete for current row language | Canonical finite bits plus unresolved atoms are compared outside TermDB union syntax and materialized only after stabilization. Residual handling removes higher-order atoms by declared labels without exposing latent thunk effects. Force-once, force-twice, and multi-clause handler strict checks pass; unresolved external rows remain artifact residuals. |
| 2026-08-07 | P0-R0 effect residual closure | complete for current row rules | Recursive reification rejects unowned open rows. This is not the grounded Claim closure required by P0-R0A. |
| 2026-08-07 | P0-R0 regression restoration | complete | CBPV surface, definition-block, computation-block sequence, and artifact-flow suites pass. |
| 2026-08-08 | P0.3 structural rules | source slice implemented; generated helpers open | Source Operation structural validation follows exact graph edges, and IH stores its motive locally. Generated Core helpers and accepted proof premises still use transitional tuples. |
| 2026-08-08 | P0.4 derived boundaries | next code slice | ASCRIPTION/exposure retains a source Operation, but conversion, context/effect weakening, literal admissibility, link completion, and Universe provenance do not yet consume exact source Claims. |
| 2026-08-08 | P0.5 non-Operation facts | partial; revalidation required | Binder input is binding-indexed and named APIs exist, but link and Universe provenance are not yet represented as exact Claim authorities. |
| 2026-08-08 | P0.6 obsolete reconstruction | reopened | Source/artifact calls to `prototype_judgement_resolve_proof_edges()` are removed; its dead implementation, temporary pruning code, and legacy proof-ID fields must be deleted after producer closure. |
| 2026-08-08 | P0.7 validation/artifact | active | Grounded Claim closure exists as a transition validator. Provisional v64 preserves `term_export.operation` and rejects closed exports without accepted evidence, but still serializes transitional candidates rather than native accepted Claims/Derivations. |
| 2026-08-08 | P0.8 P1 re-audit | superseded | The audit correctly found Claim/Derivation debt but classified it too late. It is now P0-R0A. |
| 2026-08-08 | P0-R0A premise re-audit | complete | Rechecked the active `062b7cd` worktree: structural authority propagation is partial; calls to the late resolver and handler tuple pruning are gone, but classifier-only helper selection, candidate proof IDs, and provisional v64 candidate serialization remain. |
| 2026-08-08 | P0-R0A.2 complete; P0-R0A.3 next | in progress | Authority-complete producer construction is established for the current calculus. The next change makes accepted Claims/Derivations the sole atomic publication image and removes candidate proof ownership from Claim identity. |
| 2026-08-08 | P0 entry premise finalized from active code | complete | P0 is already active. Typed conclusions belong to Operation/type-view occurrences; TermDB remains shared erased computation; closed Claims and residual obligations are distinct. R0A.2 now continues generated-helper/derived-source Claim preservation before Claim-ID publication and dead resolver deletion. |

## 10. Mandatory Ordering

```text
V2-C2 -> V2-B1 -> V2-S1 -> V2-P0 -> P1 re-audit -> V2-O1 -> V2-A1
```

V2-P0 is active. The next implementation work is P0-R0A: normalize accepted
Claim/Derivation identity, exact derived-boundary sources, and grounded closure
without duplicating OperationGraph structural edges. P1 is blocked and will be
re-audited after P0 closes. A general tagged structural payload and variable-size
premise arena are not prerequisites. HOTT equality terms still begin only after
this certificate boundary is fixed.
