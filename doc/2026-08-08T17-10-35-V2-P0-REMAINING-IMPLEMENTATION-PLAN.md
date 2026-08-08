# V2-P0 Remaining Implementation Plan

Date: 2026-08-08

Status: implementation complete; exit audit passed for the current calculus

Planning baseline: `749c0a0` (`refactor: complete P0 certificate entry premise`)

Completion baseline: `4025532` (`refactor: complete P0 certificate boundary`)

This file records both the plan and its execution. It is the current P0 status
document for the accepted Claim/Derivation boundary. Historical v66 discussion
in the parent audits remains history; the current artifact contract is v67.

Parent documents:

- `doc/2026-08-07T06-00-00-OPERATION-INDEXED-TYPING-EVIDENCE-V2-P0-PLAN.md`
- `doc/2026-08-08T00-00-00-V2-P0-REENTRY-REFACTOR-AUDIT.md`
- `doc/2026-08-06T02-00-00-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V2.md`

## 1. Frozen Boundary

P0 preserves the following separation:

```text
solver-local frontier
    candidate Claims + candidate Derivations

published certificate
    accepted Claims + accepted Derivations forming a rank-decreasing DAG

erased computation
    shared TermDB Core graph

typed occurrence authority
    OperationGraph / ContextDB / TypeDeclarationDB / intrinsic authority
```

One accepted Claim may have several accepted Derivations. There is no preferred
or latest proof field. A Derivation depends on premise Claim IDs, not on another
Derivation ID. Shared Core identity is never sufficient evidence for borrowing
the authority of another typed occurrence.

The implementation does not introduce separate Value-side and
Computation-side APP, Lambda, Pi, or Match tags. Object-level Higher
Observational equality remains outside P0.

## 2. Execution Order

| Phase | Final state | Result |
| --- | --- | --- |
| P0-C1 | complete | Semantic certificate and adversarial artifact tests added |
| P0-L1 | complete | Exact export Claim authority and atomic artifact publication |
| P0-U1 | complete | Accepted-Claim Universe provenance with numerical-edge dedup |
| P0-W1 | complete | Artifact v67, stable explicit wire values, schema fingerprint |
| P0-X1 | complete | Full prototype suite, examples, and static checks pass |

The implementation order was C1, L1, U1, W1, then X1. Link authority had to
precede Universe provenance because Universe source Claim IDs cross the same
append/link boundary.

There is no remaining P0 implementation item at the completion baseline. The
table is retained as the executed plan and as the entry contract for the next
P1 re-audit. New HOTT certificate requirements discovered by P1 must not be
retroactively folded into P0 unless they invalidate the accepted
Claim/Derivation boundary itself.

## 3. P0-C1: Characterization

Status: complete for the current calculus

### 3.1 Added coverage

`src/prototype/test_p0_certificate_boundary.sh` now checks:

- accepted Derivation storage order is not proof preference;
- reversing Derivation IDs preserves the semantic Claim and Derivation
  multisets after readback and grounding;
- equal numerical Universe inequalities may retain two different exact source
  Claim provenance records;
- artifact readback validates both provenance records rather than collapsing
  them into one record.

`src/prototype/test_artifact_flow.sh` now checks:

- an export cannot borrow the Claim of a shared-Core sibling Operation;
- a linked declaration names the exact relocated provider Claim;
- forged source Claim IDs are rejected;
- unrelated Universe Claim provenance and unknown Universe reasons are
  rejected;
- artifact v66 and foreign v67 fingerprints are rejected;
- unknown Term tags and Claim authorities are rejected;
- failed link publication does not replace an existing output artifact;
- duplicate type definitions may share canonical Core while retaining distinct
  type views and classifier evidence;
- dependent constructor owner relocation remains replayable.

Existing suites continue to cover Context category rejection, conversion scope,
recursive IH certificate acyclicity, request/fold child authority, effect-row
forwarding, integer literal admissibility, residual verification obligations,
and shared-Core Bool/Nat identities.

### 3.2 Coverage decisions

| Invariant | State | Evidence |
| --- | --- | --- |
| Multiple Derivations do not create preference | covered | `test_p0_certificate_boundary.sh` |
| Shared-Core export authority is exact | covered | `test_artifact_flow.sh` |
| Certificate cycles are rejected | covered | `BadProofCycle` artifact mutation |
| Universe source Claim/reason is validated | covered | `test_artifact_flow.sh` |
| Equal numerical edges preserve provenance | covered | `test_p0_certificate_boundary.sh` |
| Recursive IH remains a finite accepted DAG | covered | recursive Match artifact tests |
| Residual closed export is explicit | covered | CBPV residual artifact/link tests |
| General Context reindexing | deferred | no general substitution producer exists |
| General IADT index solving | deferred | belongs after P0 |

The tests locate Claims through exported semantic fields or derive IDs from the
artifact. They do not depend on fixed records such as Claim 4 or Claim 16.

## 4. P0-L1: Exact Linked Authority

Status: complete

### 4.1 Export source identity

Every typed term export carries `source_claim_id`. Its interpretation is
phase-specific and explicit:

```text
serialized read / private link image: candidate Claim slot
final artifact publication:           grounded accepted Claim slot
```

The transition to an accepted ID occurs only after grounding and validation.
It is not performed during artifact loading. This prevents sparse candidate
slots from being confused with compacted accepted Claim IDs.

The exact source is selected with kind, authority, Context, Operation, subject,
and classifier. Context weakening in a nested Lambda can no longer make a
top-level type export ambiguous merely because Core subject/classifier match.

### 4.2 Link behavior

The removed design searched globally for a compatible `(Core, classifier)`
pair. The current link path instead relocates the provider export's exact source
Claim and attaches that Claim only when the declaration subject itself was an
`EXTERNAL_REF` before relocation.

This restriction matters. Resolving an external type inside a constructor can
rebuild the constructor term and change its Term ID. That does not make the
provider's type-formation Claim a direct premise of the constructor declaration.
The constructor declaration continues to replay through its own owner/schema
rule.

Residual exports have no grounded source Claim. They are not relocation
providers and cannot discharge external declarations. They remain publishable
only when the final verification database contains the required pending
obligation.

### 4.3 Atomic publication

Link construction already occurs in process-local stores that are not visible
as an artifact. Duplicating every store into a second transaction arena would
not improve the observable boundary. Publication is therefore:

1. append and relocate in the private process image;
2. ground and replay the accepted proof DAG;
3. recompute and validate Universe/interface state;
4. write a unique temporary output path;
5. close the complete artifact;
6. atomically rename it to the requested output path.

On failure, the temporary file is removed and an existing destination remains
unchanged.

## 5. P0-U1: Universe Provenance

Status: complete

Universe collection now consumes accepted Claims and accepted Derivations. It
does not publish constraints from solver-local history.

Each constraint records:

- lower and upper level variables plus offset;
- subject and classifier;
- a dedicated `prototype_universe_constraint_reason`;
- exact `source_claim_id`;
- source authority kind and ID;
- source subject and classifier.

The solver separates two keys:

```text
numerical edge = (lower, upper, offset)
provenance     = (reason, source Claim, authority, source relation)
```

Only the first identical numerical edge participates in relaxation. Distinct
provenance records remain serialized and validated. Readback rejects an absent
Claim, mismatched authority/relation, or unknown reason. Append and link rebase
source IDs through the candidate phase and bind them to accepted IDs only at
publication.

## 6. P0-W1: Artifact v67

Status: complete

The current header is:

```text
A_PROGRAM_ARTIFACT 67 33afc155d4f21373e378c91280b92e80ca32ddeed39ce4d8f88ec88859765781
```

The fingerprint is the SHA-256 of
`src/prototype/artifact_v67.schema`. The version and fingerprint are defined in
`src/prototype/ast.h`; writer and reader use those definitions.

The schema manifest records Term, Operation, Judgement, proof, authority,
Universe, normalization, effect, verification, policy, and related wire
domains. Serialized enums have explicit numeric values, so adding or reordering
an internal declaration cannot renumber existing wire tags accidentally.
Unknown values are rejected at readback.

`reserved_legacy_assumption_level` was removed from candidate and accepted
Derivations, equality checks, replay, and wire storage. No v66 compatibility
reader remains.

## 7. P0-X1: Exit Audit

Status: complete on 2026-08-08

### 7.1 Static results

- [x] No old global linked-declaration Core search remains.
- [x] Universe publication consumes accepted Claims/Derivations.
- [x] Export and Universe records preserve exact source authority.
- [x] Accepted Derivations are rank-decreasing and replayed as a DAG.
- [x] No legacy assumption-level field remains in implementation code.
- [x] Artifact semantic enums have explicit stable wire values.
- [x] The artifact carries and validates a calculus/schema fingerprint.
- [x] Candidate state remains limited to solving, relocation construction, and
  reconstruction of the accepted graph; it is not the published certificate.
- [x] Object-level equality tags were not introduced prematurely.

### 7.2 Verification performed

- [x] `make -j4`
- [x] `make reader`
- [x] all 16 `src/prototype/test_*.sh` scripts
- [x] examples 01 through 07
- [x] example 09
- [x] `git diff --check`
- [x] artifact write/read/aggregate/link and forged artifact cases
- [x] schema fingerprint equals the checked-in v67 manifest hash

### 7.3 Regressions found during exit audit

The exit audit found and corrected four real boundary mistakes:

1. linked provider support was also attached to reconstructed constructors;
2. export Claim IDs were converted between candidate and accepted spaces too
   early;
3. residual exports were incorrectly required to supply declaration authority;
4. export Claim selection ignored the source Operation Context.

It also corrected two stale tests: the v67 Derivation field position and the
explicit-value enum spelling.

## 8. File Map

| Area | Files |
| --- | --- |
| Claim/link authority | `src/prototype/ast.c`, `ast.h`, `read_file.c`, `typing.c` |
| Universe provenance | `src/prototype/universe.c`, `universe.h`, `repl.c` |
| Stable schema | `src/prototype/artifact_v67.schema`, `ast.h`, serialized enum headers |
| Adversarial tests | `test_artifact_flow.sh`, `test_p0_certificate_boundary.sh` |
| Shared-Core fixture | `shared_core_universe_provenance_check.p` |

All implementation remains under `src/prototype/`. No accepted source under
`src/` or `include/` was changed.

## 9. Deferred Work After P0

P0 completion does not claim implementation of:

- object-level Higher Observational equality witnesses;
- PropEq, transport, rewrite, or equality reflection;
- general higher-order unification;
- IADT index refinement;
- general Context reindexing with an explicit substitution;
- artifact backward compatibility.

The next HOTT phase may rely on the v67 accepted certificate boundary. It must
not merge object equality witnesses with meta-level accepted Derivations.

## 10. P0 Handoff

P0 hands the following stable facts to P1:

1. a typed source occurrence is identified by Operation authority and Context,
   not by shared Core identity;
2. one accepted Claim may have multiple accepted Derivations;
3. accepted Derivations form a rank-decreasing DAG over premise Claim IDs;
4. structural source premises are replayed from exact OperationGraph edges;
5. link and Universe provenance name exact accepted Claims;
6. solver-local candidates and residual obligations are not accepted Claims;
7. artifact v67 is the first stable wire boundary for this certificate model.

P1 must first re-audit which HOTT evidence cannot be reconstructed from these
authorities. It must not start by adding a generic tagged payload, a duplicate
premise graph, or object equality syntax.

That re-audit is complete. It found no reopened P0 item, but it did find a
mandatory post-P0 entry refactor before P1 can select observational evidence.
The findings, implementation order, artifact v68 transition, and progress
checklist are recorded in
`doc/2026-08-08T20-00-00-V2-P1-ENTRY-REFACTOR-AUDIT-PLAN.md`.

## 11. Remaining-Item Reconciliation

Status: complete; no open P0 implementation item

The parent P0 plan still contains unchecked boxes from several earlier design
iterations. Those boxes are not all current work. Treating them as one backlog
would incorrectly reopen P0 and would mix three different things:

1. work completed by the accepted Claim/Derivation implementation;
2. proposals replaced by the final P0 representation;
3. capabilities deliberately deferred beyond the current calculus.

This section is the authoritative disposition of those apparent remaining
items. The code baseline for this disposition is `4025532`. Uncommitted v68 and
P1-entry work after that commit is not evidence that P0 was incomplete.

### 11.1 Completed by the P0 boundary

| Parent-plan item | Disposition | Completion evidence |
| --- | --- | --- |
| Separate Claim identity from proof identity | complete | Candidate and accepted Claim arenas are distinct from candidate and accepted Derivation arenas. A Derivation names its conclusion Claim. |
| Preserve more than one Derivation for one Claim | complete | Publication enumerates all concluding Derivations; the storage-order reversal regression preserves the same semantic sets. |
| Remove preferred/latest proof ownership | complete | No preferred proof field participates in Claim identity or accepted publication. |
| Remove semantic late proof-edge resolution | complete | `prototype_judgement_resolve_proof_edges()` and the compatibility publication path are absent. |
| Require complete authority at producer boundaries | complete for the current calculus | Operation, Context, declaration, type-formation, intrinsic, Universe, and Core-helper authorities are explicit. `INVALID` is not neutral authority. |
| Make derived rules retain their source Claim | complete | Conversion, expected exposure, weakening, integer admissibility, export authority, and Universe provenance retain exact source authority. |
| Reject self-supporting and cyclic certificate closure | complete | Accepted closure is rank decreasing; forged cycles are rejected. Recursive Match uses scoped IH evidence rather than a conclusion back-edge. |
| Preserve shared Core under distinct typed occurrences | complete | Bool/Nat identity and shared-Core export-forgery regressions require exact Operation authority. |
| Preserve exact Claim/Derivation identity across artifacts | complete | Artifact v67 serializes accepted Claims and Derivations and validates their relocated closure. |
| Make publication atomic | complete | Link output is written privately and renamed only after certificate and interface validation. |
| Validate Universe provenance | complete | Numerical edges and their source Claim/reason records are separate and independently validated. |
| Stabilize the wire contract | complete | Artifact v67 has explicit wire values and a checked schema fingerprint. |

### 11.2 Replaced rather than left unfinished

| Earlier proposal | Final P0 decision |
| --- | --- |
| Remove candidate Claim/Derivation storage entirely | Candidate storage remains solver-local frontier state. Only accepted arenas are publishable. Removing the frontier would erase the generation/solve/commit phase boundary. |
| Put all structural child tuples into Derivations | Structural source ownership remains in OperationGraph. Accepted Derivations retain Claim dependencies and irreducible rule parameters only. |
| Infer a unique proof from a relation tuple | Rejected. Claim identity does not imply proof uniqueness; all validated Derivations may coexist. |
| Recover authority from `(Core, Context, classifier)` | Rejected. Exact typed-occurrence authority is selected before insertion and is never recovered from shared Core identity. |
| Introduce separate Value-side and Computation-side Core tags | Rejected. TermDB remains shared and type-erased; polarity belongs to typed occurrence/classifier evidence. |
| Add a universal tagged proof payload before P1 | Not required by P0. A later phase may add only the payload demanded by an irreducible object-equality rule. |

### 11.3 Explicitly deferred capabilities

The following are not P0 defects and must not be implemented under a P0 label:

| Capability | Owning phase | Reason for deferral |
| --- | --- | --- |
| General Context reindexing over arbitrary substitutions | P1-R0/C1 follow-up | P0 validates the substitutions produced by the current compiler; it does not define a general substitution calculus. |
| General IADT index refinement and higher-order unification | after P1 | The current source calculus does not yet provide the complete constraint language or solver contract. |
| Object-level observational equality witnesses | O1 | P0 establishes meta-level typing certificates only. |
| PropEq, transport, rewrite, or equality reflection | O1/A1 or later | These require an object-theory rule and must not mutate meta-level DefEq implicitly. |
| Backward artifact compatibility | none unless separately approved | The project explicitly does not retain compatibility readers during prototype schema changes. |

## 12. P0 Reopening Criteria

P0 is reopened only if an audit demonstrates that one of its exported
certificate invariants is false. A missing HOTT feature, a new object rule, or
a desired artifact field is not sufficient.

Reopen P0 when at least one reproducible case shows that:

- an accepted Claim can be published without a validating Derivation;
- an accepted Derivation can depend on a Claim of equal or greater closure
  rank without an explicitly scoped eliminator rule;
- a typed occurrence can borrow authority solely through shared Core identity;
- a linked export or Universe constraint names the wrong accepted Claim;
- candidate solver state leaks into the artifact as accepted evidence;
- artifact readback accepts an invalid accepted certificate under the declared
  schema and fingerprint.

Do not reopen P0 merely because:

- P1 needs a new exact-Claim query API;
- substitutions need a new HOTT-specific certificate relation;
- HOTT goals need observation/action payloads;
- artifact v68 removes provisional compiler fields;
- a new equality or effect rule needs additional evidence.

Those are consumers or extensions of the P0 boundary and belong to their own
phase.

## 13. Conditional Implementation Plan

This plan is dormant while Section 12 has no failing case. If P0 is reopened,
execute it in this order. Do not patch the first failing consumer directly.

### P0-R1: Reproduce and classify

- [x] No currently known P0 invariant failure at `4025532`.
- [ ] Add a minimal adversarial fixture that fails on the completion baseline.
- [ ] Classify the failure as Claim identity, Derivation validation, closure,
  link authority, Universe provenance, or artifact readback.
- [ ] Prove that the failure concerns accepted certificate semantics rather
  than a post-P0 consumer API.

Exit condition: one deterministic test fails for exactly one stated P0
invariant. If this condition cannot be met, close the audit without changing
P0.

### P0-R2: Repair the in-memory authority boundary

- [ ] Fix Claim construction without changing shared TermDB identity.
- [ ] Fix Derivation construction using exact premise Claim IDs.
- [ ] Preserve all alternative valid Derivations; do not select first/latest.
- [ ] Keep solver-local candidates unpublished until grounding succeeds.
- [ ] Validate the repaired rule against its owning OperationGraph, ContextDB,
  TypeDeclarationDB, intrinsic, or Universe authority.

Exit condition: the new regression passes in memory, existing shared-Core
regressions still pass, and no semantic tuple lookup is introduced.

### P0-R3: Repair closure and consumers

- [ ] Recompute the least rank-decreasing accepted closure.
- [ ] Reject unsupported SCCs while preserving scoped IH/eliminator evidence.
- [ ] Revalidate exports, link relocation, and Universe collection against the
  repaired accepted Claim IDs.
- [ ] Verify that residual obligations remain distinct from accepted Claims.

Exit condition: every published root reaches only accepted Claims and valid
Derivations, and all source provenance points to the exact authority.

### P0-R4: Migrate the artifact atomically

- [ ] Change the artifact version only if the accepted wire meaning changes.
- [ ] Write or update the schema manifest before changing writer and reader.
- [ ] Reject the previous version without a compatibility fallback.
- [ ] Update mark, write, read, append, relocate, aggregate, and link paths as
  one migration.
- [ ] Add forged sparse-slot, authority, cycle, and provenance mutations for
  every changed field.

Exit condition: the schema hash matches the checked-in manifest, all artifact
paths agree on the same accepted model, and failed publication leaves the
destination unchanged.

### P0-R5: Exit audit

- [ ] Run `make -j4`.
- [ ] Run `make reader`.
- [ ] Run every `src/prototype/test_*.sh` script.
- [ ] Run examples 01 through 07 and 09.
- [ ] Run `git diff --check`.
- [ ] Search for restored preferred-proof fields, late tuple authority lookup,
  and accepted/candidate ID confusion.
- [ ] Record the completion commit and artifact version in this document.

Exit condition: all checks pass and the new completion statement names a
specific commit. Until then, P1 remains blocked.

## 14. Current Progress Sheet

| Work item | State | Next action |
| --- | --- | --- |
| P0 accepted certificate model | complete | preserve |
| P0 exact link authority | complete | preserve |
| P0 Universe provenance | complete | preserve |
| P0 artifact v67 | complete at `4025532` | historical boundary; do not edit in place |
| P0 regression and exit audit | complete | rerun when substrate changes |
| P0 reopened defect | none | no P0 implementation action |
| P1 entry substrate refactor | separate active plan | follow `2026-08-08T20-00-00-V2-P1-ENTRY-REFACTOR-AUDIT-PLAN.md` |

The next implementation phase is therefore P1-R0, not an uncompleted P0
subphase. Any future report that says "P0 remains" must name the failing P0
invariant and the reproducing test from Section 12.
