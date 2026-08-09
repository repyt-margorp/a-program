# V3-PC1 Persistent Proposition Reference Normalization Plan

Date: 2026-08-09

Status: complete; V2-A1 may start after this gate

Repository baseline:

- branch: `main`;
- implementation commit: `7cc6dc9` (`Consolidate semantic proof
  infrastructure`);
- artifact format: v69;
- V3-SC1: complete; and
- V2-A1: blocked on this normalization gate.

Related documents:

- `2026-08-09T17-05-44-POST-SC1-PHYSICAL-COMPACTION-AUDIT-AND-PLAN.md`;
- `2026-08-09T13-35-50-V3-SC1-SEMANTIC-CONSOLIDATION-IMPLEMENTATION-PLAN.md`;
- `2026-08-09T13-11-17-V2-A1-OBJECT-HOTT-ARTIFACT-IMPLEMENTATION-PLAN.md`; and
- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`.

## 1. Objective

Normalize Proposition references in the persistent accepted proof graph before
artifact v70 publishes that graph as a permanent wire contract.

The current accepted representation still contains two migration leftovers:

```text
Claim
  proposition_id
  cached Proposition pointer

AcceptedPremiseEdge
  accepted Claim id or INVALID
  copied scoped Proposition tuple
```

The v70 plan intends to serialize Proposition records and ID edges directly.
If A1 starts from the current layout, it must translate copied scoped tuples to
wire Proposition IDs and repair Claim pointers after readback and relocation.
That would create another adapter immediately after SC1.

PC1 removes these persistent in-memory duplicates first. It does not attempt a
general solver-memory or runtime compaction.

The baseline C11 layout at `7cc6dc9` is 40 bytes per Claim and 56 bytes per
accepted premise edge. The target fields require one Proposition ID per Claim
and two discriminated 32-bit IDs per accepted edge, with final padding to be
measured rather than assumed.

## 2. Semantic Boundary

### 2.1 Proposition existence is not Claim acceptance

Interning a Proposition establishes only immutable proposition identity:

```text
Proposition(Gamma, authority, subject, classifier)
```

It does not establish that the Proposition is accepted. Acceptance remains a
separate Claim record, and proof remains one or more Derivations concluding
that Claim.

A scoped premise may therefore refer to an interned Proposition without
creating a Claim.

### 2.2 An accepted premise edge is a discriminated reference

Use two IDs with an exact-one-valid invariant:

```c
struct prototype_judgement_premise_edge {
	uint32_t claim_id;
	uint32_t scoped_proposition_id;
};
```

Interpretation:

```text
claim_id valid, scoped_proposition_id INVALID
  accepted DAG premise; its Proposition is obtained through the Claim

claim_id INVALID, scoped_proposition_id valid
  rule-local scoped Proposition; no Claim is implied
```

Both valid and both INVALID are malformed. Storing a Proposition ID alongside
a valid Claim ID would duplicate authority because the Claim already fixes its
Proposition.

### 2.3 Claim stores one identity

The accepted Claim becomes:

```text
Claim
  proposition_id
  closure rank
  hash/index cache
```

Remove the cached `const Proposition*`. Resolve through a checked accessor that
receives the exact JudgementDB and Claim ID. Array relocation, compaction,
append, and readback then have no pointer-repair obligation.

### 2.4 Kernel rules remain distinct

PC1 changes reference representation only. `APP_ELIM`, `MATCH_ELIM`,
`INDUCTION_HYPOTHESIS_ELIM`, and `COMPUTATION_FOLD_ELIM` remain separate proof
kinds with separate replay validators. Lambda/App encodability is not a reason
to merge these rules.

## 3. Ownership Boundaries from the Physical-Compaction Audit

The post-SC1 audit also considered tagged references spanning delta and
accepted arenas. PC1 deliberately does not make such tags part of persistent
identity, because delta records and accepted records have different owners and
lifetimes, not because converting them would require a larger patch.

Compiler-local records remain unchanged in this stage:

- `prototype_judgement_candidate_premise` may still contain a Proposition
  value;
- `prototype_judgement_derivation_candidate` may still cache conclusion
  fields;
- `prototype_judgement_selected_evidence` remains an authority-complete value
  transported across solver lookups; and
- `prototype_judgement_delta` remains a rewindable local arena.

Those records are not accepted proof records and are not serialized by A1.
Introducing a permanent
`DELTA/ACCEPTED` arena tag into the accepted graph would leak solver lifecycle
into persistent identity. A later solver compaction may use owner-typed local
reference types, but commit must erase that distinction by interning into the
accepted Proposition arena.

Also outside PC1:

- variable-arity computation-constraint operands;
- runtime environment frames;
- proof metadata descriptors;
- passive Term child traversal;
- session storage ownership; and
- file splitting.

## 4. Current Codebase Impact

### 4.1 Accepted Claim access

Direct `claim->proposition` use is widespread in:

- `src/prototype/typing.c`;
- `src/prototype/hott.c`;
- `src/prototype/ast.c`;
- `src/prototype/cwf_certificate.c`;
- `src/prototype/universe.c`; and
- focused test helpers.

Add checked accessors before deleting the field:

```c
const struct prototype_judgement_proposition*
prototype_judgement_claim_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
);
```

Callers which already hold a Claim pointer must also retain or recover its
Claim ID. Do not derive the ID by pointer subtraction outside JudgementDB.

### 4.2 Accepted premise replay

The following paths currently read or write copied `scoped_proposition`
fields and must resolve IDs instead:

- derivation hashing and exact equality;
- candidate publication into accepted Derivations;
- `operation_solver_reify_core_proof()`;
- accepted proof validation;
- sparse artifact reachability;
- v69 write expansion and read interning;
- append/link relocation; and
- HOTT and CwF forged-proof fixtures.

### 4.3 Proposition rebuild

`prototype_judgement_db_rebuild_index()` currently remaps Proposition IDs and
then repairs every Claim pointer. PC1 changes this to remap:

- Claim `proposition_id`;
- accepted scoped `scoped_proposition_id`; and
- candidate conclusion IDs already owned by the accepted JudgementDB.

No raw pointer survives rebuilding.

### 4.4 Artifact slices currently identify Proposition slots with Claim slots

The v69 graph-slice builder currently sets its Proposition count from the Claim
slot count and copies one Proposition tuple for each present Claim. Readback
also reconstructs `claim.proposition_id` from the Claim slot. This works only
because scoped premises are stored as tuples outside the Proposition arena.

After scoped premises become Proposition IDs:

- Proposition reachability is independent of Claim reachability;
- a scoped Proposition may be present without any Claim;
- several Claims and scoped premises may refer to one Proposition; and
- Proposition relocation offsets are not Claim relocation offsets.

PC1 must therefore build an explicit Proposition slice/remap and a separate
Claim slice/remap. The temporary v69 writer expands IDs back to tuples only at
the wire boundary; the reader interns Claim and scoped tuples and may produce
different Proposition and Claim counts.

## 5. Implementation Phases

### PC1.0 Baseline and invariant characterization

Status: complete

- [x] Record struct sizes and counts for Claim and accepted premise edges.
- [x] Add accepted-edge tests for accepted, scoped, both-valid, and
      both-invalid states.
- [x] Add a test proving that interning a scoped Proposition does not create a
      Claim.
- [x] Record all direct Claim Proposition pointer consumers.
- [x] Freeze v69 output before representation changes.

Exit gate: current behavior is characterized independently of the new layout.

### PC1.1 Introduce checked Proposition resolution

Status: complete

- [x] Add checked Proposition-by-ID and Claim-to-Proposition accessors.
- [x] Migrate kernel/HOTT/CwF consumers to accessors while the cached pointer
      still exists.
- [x] Add out-of-range rejection tests and make the owning JudgementDB explicit
      at every Claim-ID resolution boundary. A raw `uint32_t` from another DB
      cannot be distinguished from an in-range local ID with the same bits;
      cross-DB safety is therefore an API ownership invariant, not a dynamic
      property of the ID.
- [x] Ensure no accessor treats Proposition existence as Claim acceptance.

Exit gate: all semantic consumers can operate without reading the cached Claim
pointer directly.

### PC1.2 Normalize accepted premise edges

Status: complete

- [x] Replace copied scoped tuples with `scoped_proposition_id`.
- [x] Intern scoped Propositions in the accepted JudgementDB without creating
      Claims.
- [x] Enforce exactly one of Claim ID and scoped Proposition ID.
- [x] Update derivation hashing, equality, commit, replay, and validation.
- [x] Preserve ordered premise identity and multiple Derivations per Claim.

Exit gate: every accepted premise is one checked ID edge and no accepted edge
contains a Proposition tuple.

### PC1.3 Remove cached Claim pointers

Status: complete

- [x] Remove `prototype_judgement_claim.proposition`.
- [x] Update Claim interning and identity checks to use Proposition IDs.
- [x] Update rebuild/compaction to remap IDs only.
- [x] Delete pointer repair after readback, append, and index rebuild.
- [x] Verify no raw process pointer participates in accepted identity.

Exit gate: Claim has one Proposition identity field and no repairable alias.

### PC1.4 Keep v69 through one strict adapter

Status: complete

- [x] Expand accepted/scoped Proposition IDs to v69 tuples only in the writer.
- [x] Intern v69 scoped tuples and construct discriminated edges in the reader.
- [x] Split Proposition reachability/remapping from Claim reachability/remapping.
- [x] Relocate scoped Proposition IDs with the Proposition remap, not the Claim
      offset, during append/link.
- [x] Remove every assumption that Proposition slot count or ID equals Claim
      slot count or ID.
- [x] Preserve deterministic v69 bytes where semantics are unchanged.
- [x] Keep this as the only compatibility adapter; A1 deletes it with v70.

Exit gate: v69 round-trips the normalized graph without an alternative
in-memory tuple path.

### PC1.5 Exit audit and A1 handoff

Status: complete

- [x] Run all prototype scripts and examples 01-07/09.
- [x] Run optimized `-Werror`, ASan, and UBSan builds.
- [x] Run CwF/reindex laws and HOTT action/forgery tests.
- [x] Run deterministic artifact write/read/append/link tests.
- [x] Confirm no accepted scoped Proposition tuple remains.
- [x] Confirm no Claim caches a Proposition pointer.
- [x] Record final struct sizes, code changes, and completion commit.
- [x] Set the A1 implementation baseline to the PC1 completion commit.

Exit gate: the persistent in-memory graph already has the reference shape that
v70 will serialize directly.

## 6. Required File Changes

| File | Required change |
| --- | --- |
| `src/prototype/judgement.h` | compact accepted edge, pointer-free Claim, checked accessor declarations |
| `src/prototype/typing.c` | ID interning/resolution, derivation identity, replay, rebuild, and validation |
| `src/prototype/ast.c` | sparse marking, v69 adapter, readback, append/link relocation |
| `src/prototype/hott.c` | migrate accepted Claim consumers to checked accessors |
| `src/prototype/cwf_certificate.c` | resolve certificate Claims through the JudgementDB |
| `src/prototype/universe.c` | migrate exact Claim provenance reads if present |
| `src/prototype/hott_goal_check.c` | compact-edge success and forgery fixtures |
| `src/prototype/test_artifact_flow.sh` | v69 adapter determinism and malformed edge coverage |
| `doc/...V3.md` | completion status and A1 handoff |
| `doc/...V2-A1...md` | consume the PC1 completion commit as baseline |

No accepted implementation outside `src/prototype/` is in scope.

## 7. Validation Matrix

```text
accepted Claim premise
scoped Proposition premise
zero-premise Derivation
multiple ordered premises
same Proposition with no Claim
same Proposition later accepted as a Claim
multiple Derivations for one Claim
delta rollback without accepted leakage
Proposition hash collision
JudgementDB rebuild and compaction
distinct Proposition and Claim counts/IDs
sparse artifact hole
v69 scoped tuple forgery
append/link Proposition relocation
Match pattern scoped premise
recursive IH scoped premise
observation Lambda/Match/IH witness replay
```

## 8. Progress Sheet

| Phase | Status | Dependency | Completion evidence |
| --- | --- | --- | --- |
| PC1.0 Characterization | complete | SC1 | edge-state and pointer-consumer inventory |
| PC1.1 Checked resolution | complete | PC1.0 | DB-scoped ID/range tests and migrated consumers |
| PC1.2 Accepted edge IDs | complete | PC1.1 | no scoped tuples; exact-one-valid tests |
| PC1.3 Pointer-free Claims | complete | PC1.2 | no pointer repair; rebuild tests |
| PC1.4 v69 adapter | complete | PC1.3 | deterministic round-trip/link/forgery tests |
| PC1.5 Exit/A1 handoff | complete | PC1.4 | full suite, sanitizers, measurements, commit |

At most one phase is `in progress`.

## 9. Dependency Order

```text
V3-SC1
  -> V3-PC1
  -> V2-A1.0
  -> V2-A1.1 ... V2-A1.9
```

## 10. Stop Conditions

Stop and revise PC1 if implementation requires:

- making Proposition interning create a Claim;
- storing both a valid Claim ID and scoped Proposition ID in one edge;
- retaining a copied Proposition tuple as a fallback;
- retaining a Claim Proposition pointer after migration;
- exposing delta arena tags in accepted graph identity;
- deriving Claim IDs by pointer arithmetic in feature code;
- merging rule-specific eliminators or validators;
- serializing solver candidates or selected-evidence snapshots; or
- retaining old and new accepted edge layouts simultaneously.

## 11. Deferred Post-A1 Compaction

The following audit findings remain valid work but do not block v70:

1. solver constraint operand arenas and transactional builders;
2. runtime persistent/stack environments;
3. narrowly declarative proof metadata;
4. passive Term child-role views;
5. owning session storage; and
6. responsibility-based file splitting after data boundaries stabilize.

They require separate plans and measurements. None may be smuggled into PC1
merely to reduce line count.

## 12. Completion Report

PC1 completed the persistent reference normalization without retaining a
compatibility layout in memory:

```text
Claim
  proposition_id
  closure_rank
  hash/index cache

AcceptedPremiseEdge
  claim_id
  scoped_proposition_id
```

Measured C11 layouts on the completion build:

| Record | SC1 baseline | PC1 result |
| --- | ---: | ---: |
| `prototype_judgement_claim` | 40 bytes | 24 bytes |
| `prototype_judgement_premise_edge` | 56 bytes | 8 bytes |
| `prototype_judgement_proposition` | 48 bytes | 48 bytes |

The implementation deliberately rejected a temporary convenience accessor
which accepted `Claim*`. Semantic entry points resolve a Claim ID with the
owning JudgementDB; code which already holds an accepted Claim reads its
`proposition_id` and uses the checked Proposition accessor. No cached
Proposition pointer remains in Claim.

Artifact v69 remains a strict boundary adapter. Its legacy premise tuple is
expanded only while writing and interned immediately while reading. Sparse
artifact construction copies reachable Claim and scoped Proposition records
independently, then `prototype_judgement_db_rebuild_index()` applies its
Proposition remap to both Claim `proposition_id` and scoped premise IDs. Claim
offsets are not used for Proposition relocation.

Term-export authorization no longer stores an untagged integer which means a
Claim before append and a Proposition after append. It uses an explicit
`prototype_artifact_evidence_reference { kind, id }`. Source/readback
interfaces carry `CLAIM`; an appended graph which has copied candidates but has
not republished accepted Claims carries `PROPOSITION`. The v69 writer converts
a Proposition reference back to an actually published Claim or rejects the
write. It never serializes a Proposition ID under the legacy `claim` label.

Validation completed:

- all 16 `src/prototype/test_*.sh` scripts;
- examples 01-07 and 09 under optimized, ASan, and UBSan builds;
- optimized C11 `-Wall -Wextra -Werror` build;
- exact-one accepted-edge fixtures for accepted, scoped, both-valid, and
  both-invalid states;
- scoped Proposition interning without Claim publication;
- deterministic artifact write/read/append/link coverage in
  `test_artifact_flow.sh`; and
- HOTT/CwF/reindex and forged-proof fixtures reached by the prototype suite.

The final migration audit also found accepted-premise fixture initializers
which set only `claim_id`. C zero-initialized `scoped_proposition_id` to `0`,
creating a both-valid edge. Every such fixture now initializes the scoped ID to
`PROTOTYPE_INVALID_ID`; the exact-one invariant is therefore exercised by the
same HOTT paths as production replay rather than bypassed to reduce migration
work.

ASan memory-safety execution passed with leak detection disabled. LeakSanitizer
separately reports an existing 3072-byte shutdown leak from
`prototype_type_declaration_db_init()` storage owned by the REPL process. PC1
did not hide or broaden scope to repair that unrelated ownership issue; it
remains a post-PC1 cleanup item.
