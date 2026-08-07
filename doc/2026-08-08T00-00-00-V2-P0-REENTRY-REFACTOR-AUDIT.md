# V2-P0 Re-entry Audit Formerly Classified as P1 Entry Work

Date: 2026-08-08

Status: audit complete; P0-R0A transition scaffold implemented; P0 remains active

Baseline:

- branch: `main`;
- commit: `701654a refactor: index typing evidence by operation`;
- remote baseline: `origin/main` at the same commit;
- artifact header: `A_PROGRAM_ARTIFACT 62`;
- implementation worktree: active P0-R0A transition over this baseline.

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

The audit is now accompanied by the first P0-R0A implementation slice. It adds
separate in-memory Claim and Derivation records and reconstructs them at the
publication boundary after the current Operation-aware proof-edge resolver has
run. This is a transition scaffold, not P0 completion: the legacy
relation/proof arrays and semantic late resolver still exist and remain the
next removal target. Higher Observational equality syntax and witnesses remain
outside P0 and begin only after this certificate boundary is stable.

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

The current code passes its regression suite, but those tests do not cover the
adversarial cases above.

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

The transition now builds the Claim/Derivation DAG from resolved exact proof
edges and computes closure ranks. All 15 prototype regression scripts pass.
This gives P0 a checkable migration boundary without claiming that the old
resolver is sound enough to retain.

### 3.2 Immediate P0 premise

The next implementation step is not P1. It is the remainder of P0-R0A:

1. propagate exact source Claim authority in solver candidates;
2. intern all candidate Claims before resolving candidate Derivations;
3. commit the grounded candidate set atomically;
4. remove late tuple-based `prototype_judgement_resolve_proof_edges()`;
5. remove the transitional relation/proof arrays and their one-to-one coverage
   invariant;
6. only then migrate artifact v62 to the selected Claim/Derivation schema.

Until step 4 closes, the reconstructed DAG is a validator and migration
scaffold, not the authoritative source of premise identity.

## 4. P1-R0: Separate Claim Identity from Derivation Identity

### 4.1 Current representation

`prototype_judgement_relation` contains both a conclusion tuple and one
`proof_kind`/`proof_id` pair. `validate_proof_relation_coverage()` requires every
physical relation to own a unique physical proof. Multiple derivations are
therefore represented by duplicating the same conclusion relation.

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

Required P1 policy:

1. define one `PROTOTYPE_ARTIFACT_FORMAT_VERSION` constant;
2. introduce wire codecs independent of C enum ordinals, preferably symbolic
   names in this text format or explicitly assigned wire values;
3. add a calculus/schema fingerprint covering proof rules, normalization
   profiles, effect operation declarations, and HOTT residual schema;
4. remove `reserved_legacy_assumption_level` in the coordinated new schema;
5. because v62 has already been published by `main`, write the P1 schema as
   v63 and reject v62 rather than silently changing v62;
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

## 14. Audit-to-P0 Mapping

The normative checklist now lives in the P0 plan. The findings map to this
order:

```text
P0-R0A.0 characterization tests
  -> P0-R0A.1 Claim/Derivation in-memory split
  -> P0-R0A.2 deterministic structural premise-to-Claim reconstruction
  -> P0-R0A.3 derived source Claim migration
  -> P0-R0A.4 grounded closure and cycle validation
  -> P0-R0A.5 linked declaration authority migration
  -> P0-R0A.6 Universe provenance migration
  -> P0-R0A.7 named capacity and wire-enum cleanup
  -> P0-R0A.8 successor artifact migration
  -> P0-R0A.9 full regression and documentation reconciliation
```

### P0-R0A.0 Characterization

- [ ] Add all adversarial tests in Section 13.2.
- [ ] Mark which tests fail on the baseline for the expected reason.
- [ ] Preserve byte-stability checks only within one fixed schema version.

### P0-R0A.1 Claim and Derivation

- [ ] Intern one Claim per full authority-aware claim key.
- [ ] Store one or more grounded Derivations per accepted Claim; keep
  zero-derivation candidates unpublished.
- [ ] Remove `proof_kind` and `proof_id` from Claim identity.
- [ ] Stop clearing and later guessing exact proof IDs.
- [ ] Keep object HOTT witnesses entirely separate from this meta certificate.

### P0-R0A.2 Structural dependencies

- [ ] Derive APP/Lambda/Match/IH/CBPV premise Claim keys from OperationGraph.
- [ ] Resolve to a unique Claim ID, never to the latest Derivation.
- [ ] Keep binder and constructor assumptions under ContextDB and
  TypeDeclarationDB authority.
- [ ] Remove structural use of generic Core tuple proof lookup.

### P0-R0A.3 Derived boundaries

- [ ] Rename current Context rule to `CONTEXT_WEAKEN` and validate ancestry.
- [ ] Reserve true `CONTEXT_REINDEX` for an explicit substitution ID.
- [ ] Store source Claim IDs for effect weakening.
- [ ] Store and validate the selected source Claim for integer admissibility.
- [ ] Store source Claims for conversion/exposure only where kernel replay and
  OperationGraph cannot recover them unambiguously.

### P0-R0A.4 Grounded closure

- [ ] Validate local rule shape before graph closure.
- [ ] Seed authoritative axiom Claims explicitly.
- [ ] Compute least supported Claim closure and assign closure ranks.
- [ ] Publish only rank-decreasing Derivations; reject cycles and orphan
  Derivations.
- [ ] Do not require one Derivation per Claim.

### P0-R0A.5 Link authority

- [ ] Replace global Core support search with relocated export/declaration Claim
  identity.
- [ ] Build the linked image privately, then validate and publish atomically.
- [ ] Add shared-Core/different-export forged-link tests.

### P0-R0A.6 Universe provenance

- [ ] Add explicit Universe reason values.
- [ ] Add source authority/Claim identity.
- [ ] Separate inequality deduplication from provenance deduplication.
- [ ] Relocate and validate provenance in artifacts.

### P0-R0A.7 Constants and wire values

- [ ] Define the artifact version once.
- [ ] Define fold clause and structural premise capacities once.
- [ ] Replace serialized implicit enum ordinals with stable wire codecs.
- [ ] Add compile-time or fixture-based wire-schema tests.

### P0-R0A.8 Successor artifact

- [ ] Finalize the in-memory model before changing the writer.
- [ ] Add the calculus/schema fingerprint.
- [ ] Serialize Claims separately from Derivations.
- [ ] Remove the reserved legacy assumption field.
- [ ] Reject v62 with no reconstruction fallback.
- [ ] Validate read, relocation, append, link, and forged artifacts.

### P0-R0A.9 Exit verification

- [ ] Warning-free prototype build.
- [ ] All 15 prototype test scripts pass.
- [ ] Examples 01-07/09 pass.
- [ ] All Section 13.2 adversarial tests pass.
- [ ] `git diff --check` passes.
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
9. the new schema is versioned as v63 and carries its calculus fingerprint;
10. existing and adversarial tests pass without compatibility fallbacks.

Only after these criteria are met should V2-O1 add object-level Higher
Observational equality types and witnesses.

## 16. Progress Record

| Date | Phase | Status | Evidence / decision |
| --- | --- | --- | --- |
| 2026-08-08 | P0 re-entry code audit | complete | Re-read Operation, Judgement, derived-boundary, link, Universe, artifact, enum, capacity, and test paths at `701654a`. |
| 2026-08-08 | Baseline regression | complete | All 15 prototype scripts and examples 01-07/09 pass. |
| 2026-08-08 | Certificate model decision | complete | Split Claim from Derivation; structural dependencies resolve to Claims, not latest proofs; object HOTT witnesses remain separate. |
| 2026-08-08 | Former P1-R0 through P1-R5 | reclassified | Mandatory P0-R0A work before P1 can be scoped. |
| 2026-08-08 | P0-R0A implementation | not started | The small uncommitted authority-validator edits are probes, not the Claim/Derivation migration. |
