# V2-P0 Remaining Implementation Plan

Date: 2026-08-08

Status: implementation complete; exit audit passed for the current calculus

Code baseline: `749c0a0` (`refactor: complete P0 certificate entry premise`)

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
