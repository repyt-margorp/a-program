# V2-P0 Re-entry Audit Formerly Classified as P1 Entry Work

Date: 2026-08-08

Status: audit complete; P0-R0A.1 and P0-R0A.2 implemented for the current calculus; P0-R0A.3 active

Baseline:

- branch: `main`;
- committed checkpoint: `985baf8 refactor: establish P0 occurrence evidence premise`;
- remote baseline: `origin/main` at the same commit;
- committed artifact header: provisional `A_PROGRAM_ARTIFACT 64`;
- implementation state: P0-R0A.1 and P0-R0A.2 implemented; P0-R0A.3 is the next active P0
  implementation phase.

This document re-audits the implementation after the earlier V2-P0 completion
claim. It found that P0 had not actually established its accepted certificate
boundary. The findings remain useful, but they are now implementation inputs to
P0-R0A in
`doc/2026-08-07T06-00-00-OPERATION-INDEXED-TYPING-EVIDENCE-V2-P0-PLAN.md`,
not a separate P1 implementation plan.

## 1. Scope

The audit covers:

- Operation-owned typing and shared Core terms;
- Judgement claims, derivations, and proof dependencies;
- conversion, expected-type exposure, effect weakening, context movement, and
  integer-literal admissibility;
- link-time declaration evidence;
- Universe constraint provenance;
- artifact v62 proof and enum encoding;
- capacities and constants that constrain the P0 certificate representation;
- the regression suite that is supposed to protect these boundaries.

The audit is now accompanied by the first P0-R0A implementation slices. They
add separate in-memory Claim and Derivation records, distinguish scoped local
obligations from publishable Claims, and carry exact selected evidence through
the principal source rules. This remains a transition scaffold, not P0
completion: legacy candidate relation/proof arrays, dead late-resolver code,
and candidate proof IDs still exist and remain removal targets. Higher
Observational equality syntax and witnesses remain outside P0 and begin only
after this certificate boundary is stable.

## 2. A Program Decisions Preserved by This Audit

The following project-specific decisions are sound and are not refactor
targets:

1. `TermDB` is the shared erased computation graph. Distinct typed occurrences
   may intentionally project to the same Core term.
2. `OperationGraph` is the authority for source and compiler-generated typed
   occurrences. A Core term does not own one global classifier.
3. Value/computation polarity is typed occurrence data over one shared graph.
   P0 must not introduce separate Value-side and Computation-side APP, Lambda,
   Pi, or Match tags.
4. Definitional conversion remains a kernel service. It is not an object-level
   equality witness and successful comparisons are not globally unioned into
   new kernel equations.
5. Future Higher Observational equality witnesses are ordinary object terms
   checked by ordinary typing claims. Meta-level certificate derivations are
   not those witnesses.
6. Every validated derivation reached within the configured solver budget may
   be retained. This does not imply that one claim has one derivation.
7. Structural proof operands already present in `OperationGraph`, `ContextDB`,
   or `TypeDeclarationDB` must not be copied into a second general structural
   payload.

## 3. Executive Result

V2-P0 substantially corrected ownership: source typing is now indexed by
Operation ID, structural validators follow typed child Operations, and shared
Core identities no longer directly select one classifier.

V2-P1 must not begin. The following findings are P0 completion conditions, not
P1 storage improvements. Historical IDs retain their `P1-` spelling only so
earlier discussion remains traceable:

| ID | Severity | Finding | Correct disposition |
| --- | --- | --- | --- |
| P1-R0 | Critical | claim identity, derivation identity, and proof dependency are still one hybrid representation | P0-R0A accepted-model refactor |
| P1-R1 | Critical | context movement and effect weakening do not validate an exact source authority | P0 derived-boundary correction |
| P1-R2 | Critical | integer admissibility validation does not require the selected source classifier | P0 validator correction |
| P1-R3 | High | link-time declaration support is selected by global Core tuple search | P0 link-authority correction |
| P1-R4 | High | Universe constraints consume Operation data but discard Operation provenance | P0 authority/provenance correction |
| P1-R5 | High | artifact v62 serializes implicit C enum ordinals and has no schema fingerprint | P0 publication boundary after in-memory normalization |
| P1-D1 | Medium | proof/fold capacities are coupled through unexplained literals | P0 schema cleanup |
| P1-D2 | Low | completed P0/V2 documentation no longer describes the current artifact and resolver state exactly | documentation correction |

The committed checkpoint passed its regression suite, but those tests do not
cover the adversarial cases above. The active migration must re-establish that
result after each authority boundary is replaced.

The findings have different origins:

- P1-R0 and P1-R2 are regressions or incomplete invariants in the P0
  implementation itself;
- P1-R1, P1-R3, and P1-R4 are older authority shortcuts that P0 narrowed but did
  not remove completely;
- P1-R5 and P1-D1 are publication/schema risks exposed by the larger P0 change,
  not evidence that current examples compute incorrectly.

### 3.1 Implemented P0 transition boundary

The 2026-08-08 implementation probe established the following compile order:

```text
solver-local JudgementDelta candidates
    -> legacy relation/proof commit
    -> normalization-premise completion
    -> OperationGraph-aware exact proof-edge resolution
    -> Claim interning and Derivation reconstruction
    -> least grounded closure ranking
    -> kernel validation and publication
```

The new reconstruction intentionally runs only after OperationGraph-aware edge
resolution. An earlier attempt to intern Claims while each delta was committed
failed for two distinct reasons:

1. a premise candidate may occur later in the same atomic delta;
2. a NAME or ASCRIPTION occurrence may project to the same Core Term while its
   exact source authority is another Operation, which cannot be recovered from
   a `(Context, subject, classifier)` tuple.

The failed attempt also exposed that `INVALID` authority has two incompatible
historical readings: an explicitly authority-neutral constructor/declaration
fact and an Operation source that has not yet been propagated. P0 must never
guess between them. The final candidate type must use an explicit authority
sum, so neutral authority is a real variant and missing authority is invalid.

The transition now builds the Claim/Derivation DAG and computes closure ranks.
Artifact v64 additionally preserves each term export's exact source Operation,
so readback can validate the selected occurrence rather than infer it from a
shared Core root. Version 64 is still transitional because candidate records
and candidate proof IDs remain serialized; it is not the final native
Claim/Derivation schema.

### 3.2 Immediate P0 implementation premise

The next implementation step is not P1. It is the corrected entry of P0-R0A:

1. distinguish solver-local obligations from independently replayable Claims;
2. propagate exact source Claim authority in every structural producer;
3. discharge scoped Lambda/fold obligations into closed derived Claims or
   explicit scoped rule parameters;
4. intern all closed candidate Claims before resolving candidate Derivations;
5. commit the grounded candidate set atomically;
6. remove late tuple-based proof-edge reconstruction and its implementation;
7. remove the transitional relation/proof arrays and their one-to-one coverage
   invariant;
8. only then finalize a successor artifact after provisional v64 as the native
   selected Claim/Derivation schema.

Until steps 1-5 close, the reconstructed DAG is a validator and migration
scaffold, not the authoritative source of premise identity.

### 3.2.1 Structural producer closure found by the final code audit

The current higher-order handler fixture exposes the first concrete P0 gate.
Operation #34 is a clause Lambda whose selected classifier #74 contains an
effect-row variable not owned by that Lambda. The materializer therefore does
not publish `#34 : #74`, which is correct for a standalone Claim. The fold
solver nevertheless copies `(#34, #74)` into the premise list of fold
Operation #46. Grounded publication consequently has no exported fold Claim,
and the earlier provisional artifact image contained:

```text
judgements 0
proofs 0
```

This is not repaired by accepting the open Lambda Claim or by borrowing the
authority-neutral generalized Core helper. The fold producer must discharge
the clause's local row assumption against the fold carrier and create a closed
Operation-owned derived Claim, or encode that check as an explicit scoped fold
rule parameter. Until this is implemented, the accepted Claim image is not a
valid premise for the rest of P0.

The audit also found and corrected a direct producer wiring error:
`operation_solver_reify_core_proof()` passed `operation->argument` as a Lambda
body Operation premise. Lambda has no argument edge; the exact edge is
`operation->body`. This correction is necessary but does not solve the scoped
obligation problem above.

### 3.3 P0-R0A.1 premise discovered during implementation

The implementation audit was repeated against the post-`062b7cd` worktree.
That audit found that "propagate exact authority" is not merely an extra field
on the existing records. The current producer APIs frequently return or accept
only a classifier Term ID. Once that happens, the selected typed occurrence has
already been lost and no later resolver can reconstruct it from a shared Core
tuple without guessing.

Two concrete failures establish this boundary:

1. the APP used internally to classify an operation request received the
   selected function and argument classifiers, but
   `prototype_judgement_delta_record_app_elim()` did not receive the selected
   child Operation authorities;
2. a computation-body effect-weaken candidate referred to a shared Core
   variable in one generated Context, while its actual binder-assumption Claim
   belonged to a different generated Context. Looking up
   `(subject, classifier)` could find the latter, but silently changing the
   Context would manufacture a premise rather than validate one.

The second case also exposed a self-supporting candidate after classifier
canonicalization: `t : T` by effect weakening depended on the same `t : T`.
Grounded closure correctly rejects that cycle. It must not be made acceptable
by first/latest tuple lookup.

Therefore P0 begins with an authority-complete evidence-selection boundary:

```text
SelectedEvidence
  Claim kind
  authority kind and authority ID
  Context ID
  Core subject projection
  classifier

StructuralPremise
  direct child Operation ID
  child Context ID
  evidence authority derived from that child
  expected Core projection and classifier
```

The direct child Operation and the evidence authority are deliberately
different fields. A VAR child is still needed to recover its occurrence
Context and binding identity even though its accepted authority is a
ContextDB binding rather than the VAR Operation. A constructor child has the
same distinction with TypeDeclarationDB authority. Collapsing either child to
`PROTOTYPE_INVALID_ID` before constructing the Claim key loses required
information.

This is the first invariant implemented inside P0-R0A, not P1 work and not an
optional pre-P0 cleanup.
The required order is now:

1. introduce an authority-complete evidence selection/result type;
2. replace classifier-only lookup and recording APIs at APP, Lambda, Match,
   constructor spine, IH, RETURN/THUNK/FORCE, request, fold, conversion,
   Context weakening, and effect weakening boundaries;
3. derive every structural premise's full Claim key from the direct child
   Operation plus ContextDB/TypeDeclarationDB authority;
4. retain exact source Claim keys for non-structural derived boundaries;
5. only then intern Claims, resolve Derivations, and publish the least grounded
   closure atomically.

The following shortcuts are forbidden:

- selecting authority by a unique `(Context, subject, classifier)` scan;
- treating `INVALID` as both a real authority-neutral fact and missing data;
- replacing a child Operation by its evidence owner before preserving the
  child Context;
- accepting a no-op conversion or effect weakening as its own premise;
- repairing premise Contexts after candidate insertion.

### 3.4 Re-audited P0-R0A.1 gate on 2026-08-08

The active worktree was re-read after the first authority propagation changes.
It confirms that V2-P0 is already active and that the next phase is
P0-R0A.1. The current transitional certificate cannot be used as the accepted
premise of later P0 phases yet.

Implemented transition pieces:

- candidate Claims and candidate Derivations carry explicit authority fields;
- accepted Claim and Derivation arenas are separate;
- APP recording can retain exact function and argument Operation IDs;
- structural grounding retains the direct child Operation long enough to
  recover its Context before selecting Operation, ContextBinding, or
  TypeDeclaration authority;
- Lambda binder premises are ContextBinding facts, not body-Operation facts;
- least grounded closure compacts accepted Claims and Derivations and does not
  reject one ungrounded alternative when another derivation grounds the same
  Claim;
- authority-neutral Core-helper candidates that match multiple typed
  occurrences remain unpublished instead of selecting one occurrence;
- local rule validation is limited to candidates selected by grounded
  publication; legacy one-relation/one-proof coverage and whole-candidate-graph
  acyclicity are no longer acceptance rules;
- the final generic `drop_unsupported_derivations` call was removed from the
  publication path after it was shown to delete a valid zero-clause fold and
  its continuation Lambda by legacy tuple matching.

Still missing before the P0-R0A.1 premise is established:

1. `SelectedEvidence` now exists as a first-class record. Source APP,
   constructor spine, operation request, and solved Match retain exact child
   Operation and Context identities. Lambda carries its body edge, but fold
   clause solving can still cite an unclosed Lambda classifier. IH stores its
   exact motive as a scoped rule parameter and no longer searches all Match
   classifiers. Classifier-only lookup remains in authority-neutral generated
   helper construction and derived proof-producing paths.
2. Structural premise handling is a compatibility helper over candidate
   tuples, not the final descriptor consumed directly by Claim interning.
3. Derived rules still carry tuple-like premise fields and can require late
   repair. Context/effect weakening and conversion are concrete examples: the
   caller may locate an exact source relation, but the recorder receives no
   `SelectedEvidence` and writes the current Operation as the premise owner.
4. Calls to `prototype_judgement_resolve_proof_edges()` have been removed from
   source compilation and artifact readback, but the dead implementation and
   legacy `premise_proof_ids` representation remain.
5. several rule-local validators still consume transitional candidate payload;
   validation has not yet been rewritten to consume accepted Derivations
   directly.
6. the handler-specific invocation of temporary-derivation pruning has been
   removed, but its unused implementation remains and the fold producer still
   emits an unsupported open premise.
7. provisional artifact v64 still serializes transitional candidate records
   selected through accepted Claims; it is not yet a native accepted
   Claim/Derivation schema.

Therefore the P0-R0A.1 gate is:

```text
typed Operation / Context / declaration authority
    -> SelectedEvidence
    -> complete candidate Claim keys
    -> candidate Derivations referring to Claim keys
    -> local rule validation
    -> least grounded closure
    -> atomic accepted Claim/Derivation publication
```

Only the accepted image after this gate is a premise for subsequent P0 work.
Candidate tuples, insertion order, and a late proof-edge resolver are not P0
premises.

## 4. Former P1-R0, now P0-R0A: Separate Claim Identity from Derivation Identity

### 4.1 Current representation

At the `062b7cd` checkpoint, the transitional producer image contains both a
conclusion tuple and one `proof_kind`/`proof_id` pair.
`validate_proof_relation_coverage()` requires every physical candidate relation
to own a unique physical candidate proof. Multiple derivations are therefore
represented by duplicating the same conclusion relation. The active P0 worktree
renames these records to `prototype_judgement_claim_candidate` and
`prototype_judgement_derivation_candidate`, but the physical one-to-one model
has not yet been removed. The rename is not completion of this item.

The insertion path then loses exact dependency identity:

- `proof_candidates_equal()` compares premise kind, Context, subject, and
  classifier, but does not compare `premise_proof_ids`;
- `add_complete_relation()` and `add_complete_delta_relation()` overwrite every
  copied `premise_proof_id` with `PROTOTYPE_INVALID_ID`;
- `prototype_judgement_resolve_proof_edges()` later reconstructs the edge;
- Operation-owned lookup scans backward and selects the latest matching
  relation, while authority-neutral lookup scans forward and selects the first.

Relevant implementation:

- `src/prototype/typing.c:4893`;
- `src/prototype/typing.c:4925`;
- `src/prototype/typing.c:5044`;
- `src/prototype/typing.c:10976`;
- `src/prototype/typing.c:11038`;
- `src/prototype/typing.c:11271`;
- `src/prototype/typing.c:13533`.

Line numbers above describe the audited checkpoint and may move during P0.
Function names and invariants, rather than numeric positions, are normative.

Consequently, the stored graph has three incompatible interpretations:

1. a relation is a claim;
2. a relation is one derivation of a claim;
3. a proof edge denotes an exact derivation, although the edge is rebuilt from
   a non-unique claim tuple.

Insertion order can change which derivation becomes a premise without changing
the source program or the semantic claim.

### 4.2 Theoretical decision

P0-R0A uses the following distinction:

```text
Claim
  = a proposition accepted by the compiler
  = (kind, authority, Context, subject, classifier)

Derivation
  = one rule application establishing one Claim
  = (conclusion Claim, rule, irreducible rule parameters, premise Claims)
```

This is not object-level proof irrelevance. Higher Observational witness terms
remain proof-relevant object terms in `TermDB`. The certificate layer may refer
to a premise Claim rather than arbitrarily choosing one meta-level derivation of
that Claim, provided validation establishes that every published Claim is in the
least well-founded closure of validated derivations.

That closure rule is essential. Merely checking that every premise tuple exists
would allow an unsupported cycle to certify itself.

### 4.3 Required representation

P0-R0A must replace the current physical one-relation/one-proof pairing with:

```text
JudgementClaim
  claim kind
  Context identity
  explicit authority identity
  Core subject projection
  classifier
  first/count or another adjacency index for derivations

JudgementDerivation
  conclusion claim ID
  rule kind
  irreducible rule parameters
  explicit source claim IDs only for derived boundaries
```

Structural premise Claims for APP, Lambda, Match, IH, RETURN, THUNK, FORCE,
request, and computation fold are reconstructed deterministically from the
conclusion Operation and its typed child Operations. P0 must not duplicate those
Operation edges in a tagged payload.

The validator then:

1. validates each authority and structural graph independently;
2. validates each Claim against its authority;
3. validates each Derivation's local rule and derived source links;
4. computes closure ranks seeded by declaration/context/intrinsic axioms;
5. publishes only Derivations whose premise Claims have lower ranks;
6. rejects cyclic and unsupported Derivations. Recursive source typing uses the
   scoped IH/eliminator rule rather than a certificate back-edge.

This model preserves all accepted derivations without inventing a semantic
one-proof-per-claim rule.

## 5. P1-R1: Context Movement Is Not Yet a Valid Reindex Rule

### 5.1 Current behavior

Generation in `operation_solver_require_evidence_in_context()` walks parent
Contexts and records a `CONTEXT_REINDEX` derivation when it finds the same
Operation claim in an ancestor.

Validation only checks:

- one premise exists;
- source and target Context IDs differ;
- subject and classifier IDs are unchanged.

It does not check that the source is an ancestor, that a Context morphism exists,
or that substitution preserves the subject and classifier.

Relevant implementation:

- `src/prototype/ast.c:26413`;
- `src/prototype/typing.c:10572`;
- `src/prototype/typing.c:13334`.

Furthermore, `judgement_expected_premise_operation()` has no
`CONTEXT_REINDEX` case. Its premise is therefore resolved by the generic Core
tuple path and is not required to belong to the conclusion Operation.

### 5.2 Theoretical correction

The current generated rule is not general CwF reindexing. Because it keeps the
same subject and classifier while moving from an ancestor Context into an
extension, it is Context weakening.

P0 must choose one of two explicit rules:

```text
CONTEXT_WEAKEN
  source Context is an ancestor of target Context
  the same graph terms remain valid under extension

CONTEXT_REINDEX
  stores a substitution/morphism sigma : target -> source
  conclusion is subject[sigma] : classifier[sigma]
```

The current caller needs the first rule. Rename it to `CONTEXT_WEAKEN`, retain
the source Claim, and validate the Context parent path. A future general reindex
rule must carry a `substitution_id`; it must not reuse this shortcut.

## 6. P1-R1: Effect Weakening Loses Its Source Operation

`validate_effect_weaken_proof()` correctly checks that the source and target are
returning computations, have convertible result types, and that the source
effect bits are included in the target bits.

However, the proof resolver has no `EFFECT_WEAKEN` ownership case. It may attach
the premise to any matching Core/Context/classifier claim. The rule therefore
proves row inclusion but does not prove that this exact typed occurrence had the
source computation classifier.

Relevant implementation:

- `src/prototype/typing.c:10597`;
- `src/prototype/typing.c:12278`;
- `src/prototype/typing.c:11289`;
- `src/prototype/typing.c:13733`.

P0 must store an exact source Claim ID for effect weakening. The local validator
must verify both row inclusion and source authority. This is irreducible
derived-boundary evidence and belongs in P1.

## 7. P1-R2: Integer Admissibility Can Be Forged at Artifact Readback

Generation is stricter than validation:

- the generator receives `selected_classifier` and records it as the premise;
- the admissible classifier is Int32;
- the value must fit Int32.

The validator only requires the premise classifier to be some primitive integer
type. `INT_LITERAL_INTRO` is also exempt from equality with the Operation's
selected classifier.

Relevant implementation:

- `src/prototype/typing.c:10785`;
- `src/prototype/typing.c:10816`;
- `src/prototype/typing.c:12581`;
- `src/prototype/typing.c:12600`;
- `src/prototype/typing.c:13728`.

A forged artifact can therefore attempt to make an Int32 intro claim under an
Operation selected as Int64, then use that claim as the admissibility source.
The present tests verify the emitted normal form, but do not mutate the source
classifier and prove rejection.

P0 must enforce:

1. `INT_LITERAL_INTRO` is exactly the Operation-selected classifier;
2. `INT_LITERAL_ADMISSIBILITY` points to that selected source Claim;
3. source and target are distinct;
4. the only current derived target is Int32;
5. the literal satisfies the Int32 range predicate.

If future integer representations generalize this relation, admissibility must
be driven by an explicit host-type representation declaration, not by weakening
these checks.

## 8. P1-R3: Link-Time Declaration Support Uses Global Core Search

`prototype_judgement_finalize_linked_declaration_premises()` mutates a new,
unpublished linked image. Mutation itself is acceptable at that construction
stage.

The problem is how support is selected. It scans all relations for:

- the same Core subject;
- a non-declaration proof;
- a compatible classifier.

It does not select support through the imported/exported declaration identity or
an explicit typed Operation/type-view boundary. If no support exists, it expands
raw Lambda, APP, or Match Core and scans again.

Relevant implementation:

- `src/prototype/typing.c:11408`;
- `src/prototype/read_file.c:3516`.

This reintroduces the P0 error at the linker boundary: two names or type views
sharing erased Core can lend declaration evidence to each other.

P0 must make a linked declaration Claim depend on one explicit provider/export
Claim after relocation. Link construction may create a new Derivation, but its
source must be the resolved declaration/export identity. It must not recover
authority by scanning the entire JudgementDB for a compatible Core tuple.

## 9. P1-R4: Universe Constraints Discard Typed-Occurrence Provenance

Universe collection now receives `OperationGraph`, and Match collection uses the
exact Match Operation to find branch classifiers. This is an improvement.

The resulting `prototype_universe_constraint`, however, stores only:

- lower and upper level variables;
- offset;
- Core subject;
- classifier;
- `reason_kind`.

It does not store Operation ID, Context ID, declaration authority, or Claim ID.
Two typed occurrences sharing Core and classifier are therefore indistinguishable
after collection.

There is also an enum-domain problem: `reason_kind` is documented as
`prototype_universe_constraint_reason`, but most callers store a Judgement proof
kind in it. The only declared Universe-specific reason currently starts at
`1001`. Artifact output serializes this mixed integer domain directly.

Relevant implementation:

- `src/prototype/universe.h:23`;
- `src/prototype/universe.h:48`;
- `src/prototype/universe.c:190`;
- `src/prototype/universe.c:407`;
- `src/prototype/ast.c:5473`.

This does not necessarily change the numerical solution when duplicated
inequalities are identical. It does break P0's stronger requirement that every
constraint retain its explicit source boundary, and it prevents reliable
residual diagnostics and future HOTT-indexed obligation replay.

P0 must:

1. give Universe constraint reasons their own explicit enum values;
2. store a source Claim ID or an explicit authority reference;
3. include that source in deduplication when provenance is semantically retained;
4. validate the source after artifact relocation;
5. distinguish numerical inequality identity from provenance identity.

## 10. P1-R5: Artifact v62 Is Not a Stable P1 Wire Contract

The artifact writer and reader hard-code `62`. Proof kinds, Judgement kinds,
Term tags, and Operation tags are serialized as integer values obtained from C
enums whose later members have implicit ordinals.

The P0 commit inserted `INT_LITERAL_ADMISSIBILITY` in the middle of the proof
enum while also changing v61 to v62, so that particular change does not create a
v62-to-v62 mismatch. P0 must not repeat the pattern while v62 artifacts already
exist.

The tests compute proof-kind values from the current header. That protects the
test mutators from becoming stale, but cannot detect an accidental wire-number
change. The header also contains no calculus/schema fingerprint despite the V2
design documents requiring one.

Relevant implementation:

- `src/prototype/judgement.h:21`;
- `src/prototype/term.h:36`;
- `src/prototype/ast.h:352`;
- `src/prototype/ast.c:6597`;
- `src/prototype/ast.c:6923`;
- `src/prototype/test_artifact_flow.sh:125`.

Required P0 publication policy:

1. define one `PROTOTYPE_ARTIFACT_FORMAT_VERSION` constant;
2. introduce wire codecs independent of C enum ordinals, preferably symbolic
   names in this text format or explicitly assigned wire values;
3. add a calculus/schema fingerprint covering proof rules, normalization
   profiles, effect operation declarations, and HOTT residual schema;
4. remove `reserved_legacy_assumption_level` in the coordinated new schema;
5. treat v64 as the current occurrence-preserving transition schema and use a
   later version for native accepted Claim/Derivation storage;
6. retain no backward-compatibility reconstruction path unless separately
   requested.

## 11. P1-D1: Replace Coupled Magic Capacities with One Contract

The current computation-fold limit is encoded independently as:

- `clause_count > 31` in generation, solving, and validation;
- arrays of 31 clause values;
- arrays of 64 computation premises;
- `PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES 65`;
- `premise_operations[64]` and `premise_classifiers[64]`.

For a fold with `n` clauses, the current rule needs `2 + 2n` structural
premises. With `n = 31`, that is 64. The value 65 has no documented relation to
that rule, and changing one literal can make generation, storage, or validation
disagree.

Relevant implementation:

- `src/prototype/judgement.h:60`;
- `src/prototype/judgement.h:123`;
- `src/prototype/typing.c:6786`;
- `src/prototype/typing.c:7277`;
- `src/prototype/typing.c:12726`;
- `src/prototype/ast.c:12811`.

P0 should define and derive:

```text
PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
PROTOTYPE_COMPUTATION_FOLD_STRUCTURAL_PREMISE_COUNT(n)
PROTOTYPE_JUDGEMENT_MAX_STRUCTURAL_PREMISES
```

This is ordinary capacity debt, not a reason to add a variable-size premise
arena. The same rule applies to the repeated 16-entry binder/type-argument
limits: public structure capacities need named constants, while local recursion
budgets must be named separately from semantic arity limits.

## 12. P1-D2: Validation APIs and Documents Need Narrow Corrections

`prototype_operation_graph_validate()` validates storage shape, reference ranges,
APP role consistency, fold occurrence alignment, and case limits.
`prototype_judgement_validate_operation_typing()` validates typed Core projection
and tag-specific typing structure. These are defensible separate responsibilities,
but their names make both appear to be complete Operation validators.

P0 should rename or document the first as a storage/shape validator and keep the
second as the typed semantic validator. Consolidation is required only where the
same invariant is genuinely checked twice; no new all-purpose helper should be
introduced.

The completed P0 document also contains stale statements:

- its baseline still names an older commit and an intentionally uncommitted
  working tree;
- it says late proof-edge resolution remains only for non-structural evidence,
  while current insertion clears all premise proof IDs and the resolver handles
  structural rules too;
- the V2 audit says the physical v62 migration is deferred even though main now
  emits v62;
- V2-O1 is still described as blocked by completed P0.

These are documentation errors, not separate kernel defects, but they must be
corrected during P0-R0A so that progress checkboxes do not substitute for code
state.

## 13. Test Audit

### 13.1 Verified on the baseline

The following passed on commit `701654a`:

- all 15 `src/prototype/test_*.sh` scripts;
- examples 01-07;
- example 09.

Existing tests do protect:

- distinct Operation identity over shared Core identity;
- rejection of an APP premise changed to another Operation's proof;
- normal integer literal intro/admissibility emission;
- existence of multiple proof kinds for one claim tuple;
- proof acyclicity;
- v61 rejection by the v62 reader;
- artifact append/link/readback for the existing schema.

### 13.2 Missing P0 re-entry tests

Before implementation, add characterization tests that currently expose or
would expose:

1. two derivations of the same Claim used under two parent derivations, with
   insertion order changed;
2. a forged `CONTEXT_REINDEX` premise from a non-ancestor Context;
3. a forged Context movement premise owned by another shared-Core Operation;
4. a forged `EFFECT_WEAKEN` source from another shared-Core Operation;
5. a forged Int32 intro under an Int64-selected literal Operation;
6. an admissibility premise whose classifier is not the selected classifier;
7. two linked exports sharing Core but requiring distinct declaration authority;
8. two Universe constraints sharing Core syntax but originating from distinct
   Operations, with provenance preserved through artifact append/link;
9. accidental changes to every serialized rule/tag wire value;
10. v62 rejection and successor-schema acceptance after the P0 schema migration;
11. unsupported cyclic Claims despite every premise tuple being present;
12. a recursive source rule accepted through a finite scoped-IH derivation DAG,
    while a forged cyclic certificate is rejected.
13. a request-internal APP whose Core function is shared by two typed
    occurrences, proving that the selected child authority is retained;
14. a shared Core VAR present in two Contexts, proving that structural premise
    construction uses the direct child Operation's Context and cannot borrow a
    binder assumption from the other Context;
15. a conversion/effect weakening that canonicalizes to `t : T -> t : T`,
    proving that it is rejected as a self-supporting derivation unless another
    grounded derivation establishes the Claim.
16. a fold clause Lambda with a parent-owned effect-row variable, proving that
    the local obligation is discharged into replayable fold evidence and that
    a closed artifact export cannot be emitted with zero Claims/Derivations,
    while an explicitly residual export remains visibly residual.

## 14. Audit-to-P0 Mapping

The normative checklist now lives in the P0 plan. The findings map to this
order:

```text
P0-R0A.0 characterization tests
  -> P0-R0A.1 local-obligation/closed-Claim boundary
  -> P0-R0A.2 authority-complete evidence API
  -> P0-R0A.3 Claim/Derivation accepted model
  -> P0-R0A.4 deterministic structural premise-to-Claim reconstruction
  -> P0-R0A.5 derived source Claim migration
  -> P0-R0A.6 grounded closure and cycle validation
  -> P0-R0A.7 linked declaration authority migration
  -> P0-R0A.8 Universe provenance migration
  -> P0-R0A.9 named capacity and wire-enum cleanup
  -> P0-R0A.10 successor artifact migration
  -> P0-R0A.11 full regression and documentation reconciliation
```

### P0-R0A.0 Characterization

- [ ] Add all adversarial tests in Section 13.2.
- [ ] Mark which tests fail on the baseline for the expected reason.
- [ ] Preserve byte-stability checks only within one fixed schema version.

### P0-R0A.1 Local-obligation and closed-Claim boundary

- [x] Represent or otherwise explicitly distinguish a solver-local obligation
  from a candidate Claim.
- [x] Stop computation-fold constraints from copying an unowned clause
  classifier into an ordinary Claim premise.
- [x] Materialize a specialized Operation-owned clause Claim or store a
  replayable scoped fold parameter.
- [x] Diagnose or leave unpublished an unresolved exact structural producer
  instead of treating it as a closed Claim.
- [x] Require every exported typed occurrence to have either an accepted
  grounded Claim or an explicit residual/verification obligation reachable
  from that exact source Operation.
- [x] Add the higher-order handler artifact regression described in item 16.

Current state: complete for the current calculus. A residual export is not an
accepted Claim: artifact v64 preserves `term_export.operation`, requires its
classifier to equal the selected Operation classifier, and permits the export
without a Claim only when an unsolved residual constraint or pending runtime
verification obligation belongs to that Operation subtree. This distinction is
a premise for R0A.2 and later atomic
publication.

### P0-R0A.2 Authority-complete evidence API

This section is the immediate P0 entry point after commit `985baf8`. Its
premise is P0-P0, not a storage preference: a typed conclusion belongs to its
Operation/type-view occurrence, while its Core Term is only the shared erased
projection. R0A.2 must establish authority at producer construction time
before R0A.3 is allowed to make accepted Claim IDs authoritative.

- [x] Introduce `prototype_judgement_selected_evidence` with complete Claim
  authority, Context, Operation projection, subject, and classifier.
- [x] Migrate generic APP candidate collection to preserve function and
  argument evidence identity.
- [x] Replace all remaining proof-producing classifier-only lookups with
  `SelectedEvidence`.
- [x] Make all candidate constructors require an explicit authority variant;
  missing authority must not encode a neutral fact.
- [x] Preserve direct child Operation and child Context independently from the
  final ContextDB/TypeDeclarationDB/Operation authority.
- [x] Remove unique/first/latest tuple authority recovery from proof producers.
- [x] Split generated Match/motive inference from source Match occurrence
  recording before removing the classifier-only helper. Do not let a generated
  helper inherit the requesting source Operation through `current_operation_id`.

Current state: complete for the current calculus. The evidence API and source
APP/Lambda/constructor/request/Match/CBPV/fold producer migration are implemented.
`SOLVED_MATCH_MOTIVE` waits for all
branch classifiers and records exact branch premises instead of validating
through a later global tuple scan. IH carries `induction_motive` as an
immutable scoped-eliminator parameter and intentionally has no parent-Match
premise. Conversion, expected exposure, Context/effect weakening, and integer
admissibility consume `SelectedEvidence`. Generated Lambda/RETURN/THUNK/APP/
Match/type-formation paths explicitly use `CORE_HELPER` or their own authority
domain. Structural computation constraints retain direct child Operations and
the independently selected evidence authority. Preserving those exact sources
as accepted Claim IDs is the next R0A.3 representation change.

### P0-R0A.3 Claim and Derivation

- [x] Intern one Claim per full authority-aware claim key.
- [x] Store one or more grounded Derivations per accepted Claim; keep
  zero-derivation candidates unpublished.
- [x] Remove `proof_kind` and `proof_id` from accepted Claim identity.
- [ ] Remove `proof_kind` and `proof_id` ownership from the legacy candidate
  Claim representation.
- [ ] Stop clearing and later guessing exact proof IDs.
- [ ] Keep object HOTT witnesses entirely separate from this meta certificate.

Current state: accepted Claim interning, ordered Claim-ID premise retention,
least grounded closure, and final rule validation are authoritative in memory.
The validator walks every accepted Derivation and does not select it from the
legacy candidate `accepted` bit. Scoped fold/request rule parameters remain
local payload rather than fabricated Claims. The remaining dependency on proof
IDs is confined to candidate construction/resolution, substitution metadata,
Universe consumers, and v64 artifact reachability/serialization. In
particular, `source_candidate_proof_id` must disappear together with the native
Claim/Derivation artifact migration; removing it earlier would merely replace
it with a reverse tuple lookup.

### P0-R0A.4 Structural dependencies

- [x] Introduce `SelectedEvidence` (or an equivalently named record) carrying
  the full candidate Claim key; classifier-only lookup is not an authority
  boundary.
- [x] Preserve structural-premise fields carrying both direct child Operation
  identity and derived evidence authority without adding a redundant tagged
  wrapper type.
- [x] Preserve a VAR/constructor child's Context before mapping authority to
  ContextDB/TypeDeclarationDB.
- [x] Derive source APP/Lambda/Match/CBPV premise keys from OperationGraph.
- [x] Keep IH premise-free and retain its exact motive as a local rule
  parameter, avoiding a recursive parent-Match proof edge.
- [ ] Give remaining generated APP/Match helpers explicit authority
  or keep them unpublished; do not resolve their Core tuples to source
  Operations.
- [x] Resolve to a unique Claim ID, never to the latest Derivation.
- [x] Keep binder and constructor assumptions under ContextDB and
  TypeDeclarationDB authority.
- [x] Remove structural use of generic Core tuple proof lookup from accepted
  closure and final proof validation.

Current state: APP and Lambda binder handling have been corrected; constructor
spines retain argument Operations; request/fold retain their direct children,
but fold does not yet discharge an open clause classifier;
solved Match derivations retain case-local branch Operations and Contexts; and
IH validation is local to its stored motive. Authority-neutral generated
helpers, dead temporary-pruning removal, and final accepted-model validation must still
consume exact Claim identities end to end.

### P0-R0A.5 Derived boundaries

- [x] Rename current Context rule to `CONTEXT_WEAKEN` and validate ancestry.
- [ ] Reserve true `CONTEXT_REINDEX` for an explicit substitution ID.
- [x] Store source Claim IDs for effect weakening.
- [x] Store and validate the selected source Claim for integer admissibility.
- [x] Store source Claims for conversion/exposure only where kernel replay and
  OperationGraph cannot recover them unambiguously.

The recorder API migration is complete: these rules accept one complete source
`SelectedEvidence`. The remaining work is representational: when accepted
Claims become authoritative, each derived Derivation must reference that exact
source Claim ID instead of serializing the transitional authority tuple and
leaving `premise_proof_ids` unresolved.

### P0-R0A.6 Grounded closure

- [x] Validate local rule shape after graph closure against the immutable
  accepted Derivation image. Moving this before closure is unnecessary because
  unresolved candidates are intentionally not accepted objects.
- [x] Seed authoritative axiom Claims explicitly through zero-premise
  Derivations in their authority domain.
- [x] Compute least supported Claim closure and assign closure ranks.
- [x] Publish only rank-decreasing Derivations; reject cycles and orphan
  Derivations.
- [x] Do not require one Derivation per Claim.

### P0-R0A.7 Link authority

- [ ] Replace global Core support search with relocated export/declaration Claim
  identity.
- [ ] Build the linked image privately, then validate and publish atomically.
- [ ] Add shared-Core/different-export forged-link tests.

### P0-R0A.8 Universe provenance

- [ ] Add explicit Universe reason values.
- [ ] Add source authority/Claim identity.
- [ ] Separate inequality deduplication from provenance deduplication.
- [ ] Relocate and validate provenance in artifacts.

### P0-R0A.9 Constants and wire values

- [ ] Define the artifact version once.
- [ ] Define fold clause and structural premise capacities once.
- [ ] Replace serialized implicit enum ordinals with stable wire codecs.
- [ ] Add compile-time or fixture-based wire-schema tests.

### P0-R0A.10 Successor artifact

- [ ] Finalize the in-memory model before changing the writer.
- [ ] Add the calculus/schema fingerprint.
- [ ] Serialize Claims separately from Derivations.
- [ ] Remove the reserved legacy assumption field.
- [ ] Reject v62 with no reconstruction fallback.
- [ ] Validate read, relocation, append, link, and forged artifacts.

### P0-R0A.11 Exit verification

- [x] Warning-free prototype build.
- [x] All 15 prototype test scripts pass.
- [x] Examples 01-07/09 pass.
- [ ] All Section 13.2 adversarial tests pass.
- [x] `git diff --check` passes.
- [ ] P0 and V2 status/dependency text matches the implemented state.

## 15. P0-R0A Exit Criteria

P0-R0A is complete only when:

1. one semantic Claim can have multiple immutable Derivations without
   duplicating Claim identity;
2. no validator or linker selects a premise by first/latest global Core tuple;
3. structural premises are reconstructed from authoritative graphs as Claim
   dependencies, not copied payloads;
4. every derived boundary retains and validates its exact source Claim;
5. Context weakening is distinguished from general substitution reindexing;
6. published Claims are grounded in a least well-founded derivation closure;
7. Universe residuals retain typed-occurrence or declaration provenance;
8. artifact wire values are independent of C enum insertion order;
9. the final native schema succeeds provisional v64 and carries its calculus
   fingerprint;
10. existing and adversarial tests pass without compatibility fallbacks.

Only after these criteria are met should V2-O1 add object-level Higher
Observational equality types and witnesses.

## 16. Progress Record

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-08 | P0 re-entry code audit | complete | Re-read Operation, Judgement, derived-boundary, link, Universe, artifact, enum, capacity, and test paths at `701654a`. |
| 2026-08-08 | Committed baseline regression | complete | At the historical `701654a` baseline, all 15 prototype scripts and examples 01-07/09 passed. |
| 2026-08-08 | Active worktree regression | complete for existing suite | Warning-free build, all 15 `src/prototype/test_*.sh` scripts, examples 01-07/09, and `git diff --check` pass with artifact v64. Section 13.2 still lists new adversarial coverage required before P0 exit. |
| 2026-08-08 | Certificate model decision | complete | Split Claim from Derivation; structural dependencies resolve to Claims, not latest proofs; object HOTT witnesses remain separate. |
| 2026-08-08 | Former P1-R0 through P1-R5 | reclassified | Mandatory P0-R0A work before P1 can be scoped. |
| 2026-08-08 | P0-R0A transition checkpoint | complete | `062b7cd` adds separate reconstructed Claim/Derivation arenas and grounded closure while retaining the legacy producer image. |
| 2026-08-08 | P0-R0A.1 premise re-audit | complete | Classifier-only producer APIs lose child authority and Context; authority-complete evidence selection and structural premise descriptors are mandatory P0-R0A.1/R0A.2 work. |
| 2026-08-08 | P0-R0A authoritative migration | in progress | R0A.1 and R0A.2 are implemented for the current calculus. Atomic accepted publication, artifact migration, and later provenance phases remain incomplete. |
| 2026-08-08 | P0 premise code re-audit | complete | Confirmed remaining classifier-only lookups, candidate-proof-ID validation, dead resolver/pruning implementations, and provisional v64 candidate coupling. |
| 2026-08-08 | Structural authority repair | partial | APP child Operations are retained; Lambda binder premises use ContextBinding authority; direct child Context is preserved before owner projection; legacy final tuple pruning was removed. |
| 2026-08-08 | SelectedEvidence/APP implementation | partial | Added an authority-complete selected-evidence record, evidence-preserving APP candidate collection, and partial Lambda/CBPV child propagation. Remaining classifier-only proof producers keep P0-R0A.1 open. |
| 2026-08-08 | Candidate/publication separation | partial | Ungrounded or authority-ambiguous Core-helper derivations remain unpublished; local validation runs only for grounded publication selections; legacy candidate coverage and candidate-graph acyclicity no longer override accepted Claim closure. |
| 2026-08-08 | Exact structural producer migration | partial | Lambda, constructor spine, request, computation fold, and solved Match now preserve child Operation/Context identity. Match publication waits for the fixed-point frontier to classify every branch; dependent recursive Match remains accepted without a global branch tuple search. |
| 2026-08-08 | Scoped IH migration | complete | IH stores the exact Match motive as an immutable rule parameter and validates `M recursive_argument` locally. It remains premise-free to avoid a Match/branch/IH proof cycle. |
| 2026-08-08 | Final P0 premise audit | complete | The higher-order handler exposed both a parent-owned clause row obligation and alpha-interned Lambda/body occurrence divergence. The implemented boundary retains exact body Operation/Context/Core projection and treats clause Lambdas as scoped fold parameters. |
| 2026-08-08 | P0-R0A.1 implementation | complete for current rules | Superseded fixed-point classifiers are excluded from Claim publication; exact clause Operation classifiers determine fold rows; local rows are included in the final carrier and exact union is replayed by validation. `test_cbpv_surface.sh` passes, including higher-order force-once/force-twice and forged-artifact cases. |
| 2026-08-08 | P0 export occurrence boundary | implemented; provisional schema v64 | `term_export` now preserves its source Operation. Writer/readback require the export classifier to equal that Operation classifier and require either an accepted grounded Claim or a reachable explicit residual/verification obligation. Shared erased Core identity is not export evidence. |
| 2026-08-08 | P0-R0A.2 authority-complete producer boundary | complete for current rules | Generated helper and source structural producers carry explicit authority-complete evidence; direct structural Operation edges remain distinct from evidence owner identity; all 15 prototype scripts pass. |
| 2026-08-08 | Next implementation phase | in progress | P0-R0A.3 makes accepted Claims/Derivations the sole atomic publication image. P1 remains blocked until the remaining R0A phases and P0 exit checks are complete. |
