# PR #20 Meaning-Boundary Critical Review and Implementation Plan

Date: 2026-08-23

Status: implemented and verified

Reviewed pull request: `#20`, commit `93086864ee0e4d8e6c75bc7c6471ef7f8ab263f8`

PR review baseline: `1672c6a62de6e8a7fe815886682ac4436c2527c7`

Current implementation baseline: `245524c`

PR document:
`doc/2026-08-23T22-36-03-MEANING-BOUNDARY-AUDIT-AND-REMEDIATION-REPORT.md`

## 1. Purpose

PR #20 is a documentation-only audit of three authority boundaries:

1. residual effect-row equations and artifact exports;
2. first-order and higher-order function-graph associations; and
3. Higher Observational identity roots.

This plan rechecks those claims against the current code. It does not treat the
PR as authority merely because its analysis is detailed. Each claim is split
into:

- an observed implementation fact;
- a demonstrated behavior;
- an inference not yet demonstrated; and
- the correction required by A Program's own contracts.

The implementation must preserve these project invariants:

- TermDB remains the canonical erased computation graph.
- TypedOccurrenceGraph remains occurrence-sensitive typing/provenance data.
- JudgementDB remains the only authority for unconditional accepted Claims.
- A residual obligation is not an accepted Claim.
- Compilation and linking are bounded earlier computation phases, not weaker
  substitutes for runtime execution.
- A later phase may preserve or discharge an earlier residual only through an
  explicit checked transition.
- No repair in this plan adds a DefEq rule.
- No compatibility reader or remap facade will preserve an obsolete wire
  meaning.
- Migration size is not a reason to retain two authorities for one fact.

## 2. Review method

The audit used all of the following:

- the PR body and its complete 592-line report;
- the PR's baseline commit;
- the current `main` implementation;
- the semantic diff from `1672c6a` to `245524c` in the affected modules;
- artifact v83 schema text;
- function-graph and HOTT plans;
- focused artifact, function-graph, and HOTT tests; and
- a new temporary source-interface experiment described in section 4.3.
- two function-graph artifact mutations described in section 5.2.

The performance commit after the PR baseline changes instrumentation, indexes,
workspace reuse, and build behavior. It does not repair the three semantic
boundaries reviewed here.

## 3. Final verdict

| Finding | Verdict | Priority | Required response |
| --- | --- | --- | --- |
| A. Effect residual/export lifecycle | Substantially valid, but the PR overstates unconditional loss on source import | P0 | Replace generic reachability with exact conditional authority and define preservation/discharge |
| B. Higher-order function-graph contract | Valid contract contradiction and post-read validation gap; not a kernel inconsistency | P1 | Make invariants mode-specific and validate classifier/package structure |
| C. Identity-root readback | Valid and stronger than the PR concludes | P1 | Enforce the canonical-action contract already stated by artifact v83 |
| CwF/generated binding concern | No bypass demonstrated | No semantic change | Keep regression coverage only |
| Nominal graph ownership by itself | Not an error | No semantic change | Do not reinterpret association as extensional equality |
| Kernel inconsistency/False/DefEq pollution | Not demonstrated | Reject claim | No kernel-equality change |

PR #20 should not be merged as the final design contract without revision:

- Finding A must distinguish lost provider identity from regenerated downstream
  residuals.
- Finding C is not an open choice between transport-only and canonical
  semantics. The current v83 schema already selects canonical replay.
- The proposed implementation must be expressed in terms of one conditional
  evidence model, not as unrelated patches to export acceptance, import, and
  linking.

## 4. Finding A: conditional effect authority

### 4.1 Confirmed current facts

The following remain true at `245524c`:

1. `compile_phase_record_residual_effect_constraints()` stores each unsolved
   effect equation as a pending
   `PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION`.
2. `prototype_verification_db_coverage()` assigns no runtime capability to this
   kind and there is no effect-equation discharge function analogous to
   `prototype_verification_db_discharge_computation_fold_result()`.
3. `artifact_exports_have_accepted_claims()` accepts an export without an
   accepted Claim when any pending obligation is reachable from the export's
   occurrence.
4. That fallback does not require the obligation to be an effect equation, does
   not require exact ownership, and does not prove that all conditions have
   been enumerated.
5. source-interface import appends Terms, declarations, Contexts,
   Substitutions, dimensions, and interface records, but does not append the
   provider VerificationDB as provider obligation identity.
6. imported classifier lookup returns the relocated classifier Term without
   consulting the export's `source_evidence` or a residual dependency set.
7. link aggregation preserves pending effect obligations but does not discharge
   them.
8. the VerificationDB header still describes every record as a conditional
   runtime obligation, while effect equations are static/link/host constraints.
9. README says effect residuals remain in their owning subsystem, while v83 and
   the implementation persist them in VerificationDB.
10. `classifier_verification_obligation` describes classifier closure only.
    An effect equation may coexist with a solved classifier, so this field must
    not be overloaded as the owner of every semantic residual.

Relevant implementation areas:

- `src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc`
- `src/prototype/src/driver/read_file.c`
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`
- `src/prototype/include/a_program/graph/verification.h`
- `src/prototype/src/graph/typed_occurrence/verification.inc`
- `src/prototype/src/artifact/interface.c`
- `src/prototype/src/artifact/link.c`
- `src/prototype/spec/artifact_v83.schema`
- `README.md`

### 4.2 The actual defect

The central defect is not that a row equation is stored outside JudgementDB.
It must remain outside unconditional proof authority while unresolved.

The defect is that the implementation lacks a first-class relation:

```text
conditional export
  -> exact exported occurrence and classifier
  -> complete set of residual obligation IDs
  -> phase allowed to preserve or discharge each obligation
```

Instead, publication currently asks only whether one pending record is reachable.
Graph reachability is useful for computing a dependency closure. It is not an
evidence kind and cannot by itself authorize publication.

### 4.3 Correction to the PR's source-import claim

A temporary current-main experiment compiled:

```text
provider:
  Nat := @{ zero : *; succ : * -> *; };
  idNat : Nat -> Nat;
  main := idNat Nat.zero;

consumer:
  downstream := main;
```

The provider artifact contained one pending effect equation and no accepted
Claim for `main`. Importing it through `--import-interface` produced a consumer
artifact that also contained one pending effect equation. Compiling the
consumer under `--policy strict` failed with retained unresolved verification
state.

Therefore this stronger statement is not established:

> source import always drops the condition and makes the downstream use
> unconditional.

The downstream compiler can regenerate an equivalent-looking residual from the
imported classifier and its new occurrence. This is a material defense and must
be recorded in the final review.

It does not close the design gap:

- the original provider obligation identity is lost;
- equivalence between the old and regenerated equations is not checked;
- preservation depends on re-elaboration behavior rather than an interface
  contract;
- an obligation not recoverable solely from the classifier has no preservation
  path; and
- generic reachability still accepts an export using an unrelated pending kind.

The correct wording is:

> Source-interface import does not preserve the provider's exact conditional
> evidence. Some uses regenerate a new residual, but the compiler has no
> general checked theorem that regeneration is complete or equivalent.

### 4.4 Chosen model

Keep one kind-indexed VerificationDB, but correct its contract. It is a store of
residual verification obligations, not a runtime-only database.

Each obligation kind must have an executable descriptor:

```text
kind
payload validator
permitted preservation phases
permitted discharge phases
discharge checker
whether successful discharge can enable Claim reconstruction
```

Initial phase classes are:

```text
COMPUTATION_FOLD_RESULT  runtime discharge
EFFECT_ROW_EQUATION     compile/link/declared-host discharge
```

This keeps one residual data structure while preserving the semantic
difference between kinds. It avoids both a catch-all runtime interpretation and
a second mutable effect-solver authority.

Typed occurrences gain explicit occurrence-to-obligation dependency edges.
Term exports gain one of two evidence forms:

```text
ACCEPTED
  exact accepted HAS_TYPE Claim

CONDITIONAL
  exact occurrence
  exact classifier and Context
  canonical complete set of residual obligation references
```

An obligation may be shared by several exports. The authority is the edge/set,
not a copied obligation payload in every export.

The complete set is generated from the frozen occurrence dependency graph and
the explicit occurrence-to-obligation edges. Reader validation recomputes the
closure and requires exact set equality. It must reject both missing and extra
dependencies.

### 4.5 Conditional import rule

Import must not turn conditional evidence into an accepted Claim.

The imported binding records:

```text
relocated subject
relocated classifier
export identity
condition set
```

Use of that binding propagates the condition set through occurrence-level
evidence. A downstream unconditional Claim may be published only when its
condition set is empty after checked discharge.

Do not add a `CONDITIONAL_CLAIM` to JudgementDB. That would make JudgementDB
serve two authority classes and would complicate accepted replay. Conditional
evidence remains occurrence/artifact evidence until its premises close.

If complete condition propagation cannot be implemented in the same phase,
source import must reject conditional exports. Silently importing only the
classifier is not an acceptable intermediate state.

### 4.6 Discharge rule

Effect discharge must call the canonical effect-row solver/checker on relocated
row Terms. It may not mutate an obligation to `DISCHARGED` merely because a
provider name was found.

For each equation:

```text
result_row = left_row
result_row = union(left_row, right_row)
```

the discharge phase must:

1. relocate all row Terms and effect labels;
2. instantiate imported row variables from exact provider evidence;
3. solve with the same canonical row semantics used by source compilation;
4. record a deterministic discharge result;
5. remove the obligation from the condition set only after validation; and
6. never create a typing Claim from effect discharge alone. Effect-row
   discharge closes only an effect condition. If a separate typing residual is
   later discharged, that residual's own descriptor must require candidate
   reconstruction and accepted replay.

Host discharge is not part of artifact v84. Compile/link discharge is limited
to closed rows accepted by the canonical row normal-form checker. Adding host
discharge requires a later wire change with stable host evidence and replay.

### 4.7 Artifact change

This changes wire meaning and requires artifact v84. No v83 compatibility
reader is retained.

The exact spelling may be refined during implementation, but v84 must contain:

- export evidence kind `ACCEPTED` or `CONDITIONAL`;
- a canonical residual-dependency edge table;
- explicit obligation kind/phase semantics;
- exact occurrence and classifier validation; and
- no generic reachability fallback.

The `SOLVED`/`RESIDUAL_VERIFICATION` occurrence status remains classifier
status. Schema text must stop claiming that it describes every deferred effect
condition.

## 5. Finding B: function-graph association modes

### 5.1 Confirmed contract contradiction

The implementation deliberately has two modes:

```text
first-order:
  certified_argument_index == INVALID
  published owner is generated projection of certified execution

higher-order:
  certified_argument_index != INVALID
  raw owner remains executable
  certified runner additionally receives graph interface/certified callback
```

`function_graph_generate_one()` implements this distinction explicitly.
Artifact v83 also documents the sentinel meaning.

The function-graph plan nevertheless states an unconditional invariant that
every executable owner is the projection of certified execution and lists
separate executable/certified bodies as a stop condition. Those statements are
false for the accepted higher-order mode.

This is a documentation/contract inconsistency. It is not evidence that the
generated higher-order runner proves the raw callback's execution.

### 5.2 Confirmed validation gap

`wire_v83.c` validates:

- referenced export indices;
- generated naming conventions;
- optional adapter bounds; and
- uniqueness by owner.

It does not validate from the loaded accepted graph that:

- a non-invalid certified argument index exists in the owner Pi spine;
- the runner contains the expected interface/certified callback domains at the
  corresponding position;
- graph, result, interface, adapter, and runner classifiers form one coherent
  package; or
- adapter presence agrees with the callback package shape.

Name validation is useful diagnostics, but names are not semantic evidence.

This gap was reproduced against `245524c`. A real QuickSort function-graph
artifact contained one higher-order association with
`certified_argument_index == 1`. Two independently modified artifacts were
then read:

1. replacing that index with the first-order `INVALID` sentinel; and
2. replacing it with the out-of-range value `999`.

The current `--read-graph` path accepted both artifacts. This confirms that the
missing post-read classifier/package check is observable artifact behavior, not
only a documentation concern.

### 5.3 Chosen correction

Retain the existing sentinel encoding and higher-order ABI. Do not add a mode
enum to the artifact merely to duplicate the sentinel, and do not redesign the
raw owner as a certified projection in this correction.

Add a post-read semantic validator in the artifact interface layer. It runs
after Terms, declarations, Contexts, accepted Claims, and Derivations have been
loaded and replayed. The token parser in `wire_v83.c` should continue to perform
only local syntax/bounds checks.

The semantic validator must use accepted export classifiers and the ordinary
callable-classifier/Pi-spine helpers. It must not reconstruct meaning from
generated names.

Required mode-specific checks:

#### First-order mode

- owner, graph, result, interface, and runner exports have accepted evidence;
- owner and runner result carriers agree under pure conversion;
- the owner Term is the generated projection shape required by the contract;
- no certified argument position is claimed; and
- adapter presence follows the generated first-order package rule.

#### Higher-order mode

- certified argument index is within the raw owner's value-domain spine;
- the indexed raw argument is callable with the supported callback shape;
- runner preserves the owner prefix/suffix and introduces the graph interface
  and certified callback at the specified position;
- interface and adapter classifiers match that callback domain;
- no check claims that the raw owner is extensionally equal to the runner; and
- tools report `certified variant`, not `certified raw execution`.

No artifact version bump is needed if these facts are derivable from existing
v83 data. If implementation proves otherwise, add only the minimal missing
semantic field and move it into the same v84 transition required by Finding A.

## 6. Finding C: identity-root canonicality

### 6.1 The contract is already decided

PR #20 presents two possible root meanings:

- transport designation; or
- canonical-action certificate.

For the current repository this is no longer an open choice. Artifact v83 says:

> The computation rule must be replayed from the source Claim, the canonical
> dimension operator embedded in the acted Term, the acted-family Claim, and
> the witness Claim; the root label is not authority by itself.

The HOTT fragment also defines repeated action through canonical extensions
`e_0`, `e_1`, and so on. Producer registration calls full action-database
validation before adding the root.

Therefore the intended root is a compact canonical-action certificate. The
reader is required to re-establish the root-specific canonical facts from the
ordinary accepted graph.

### 6.2 Confirmed missing checks

For `GENERIC_DIMENSION_ACTION`, current artifact validation checks family
instances, shared source family, adjacent dimensions, and Context presence.
It does not check:

- `source_dimension > 0` for repeated action;
- integer-overflow safety before `source_dimension + 1`; or
- that the higher operator is the canonical identity-axis extension.

Family-instance parsing already checks boundary application arity. PR #20 is
correct not to request a duplicate arity check.

Accepted proof replay checks ordinary formation and typing. It does not imply
the project-specific choice of canonical extension among all well-typed
operators with the same source and target dimensions.

### 6.3 Chosen correction

Move the canonical extension predicate out of
`identity/action_certificate_validation.inc` into the dimension operator
module as one shared read-only predicate, for example:

```text
prototype_dimension_operator_is_canonical_extension(
    operators,
    operator_id,
    source_dimension
)
```

Use this same predicate from:

- local HOTT action certificate validation;
- identity-root registration; and
- artifact identity-root readback validation.

The reader then checks exactly the root contract derivable from serialized
data:

- source and higher family instances are valid;
- repeated source dimension is nonzero and increment is safe;
- higher source is the source family;
- target dimension is exactly one greater;
- operator is the canonical extension;
- accepted source/family/witness Claims have the required Context and
  classifier relation; and
- the computation rule matches the family shape.

Do not serialize compiler-local HOTT action IDs, bridge IDs, search state, or
the complete local certificate. They are not required when the root property
is derivable from Terms, dimensions, Contexts, and accepted Claims.

No wire bump is required for this validator alone.

## 7. Implementation phases

### MB0: lock the audited baseline and tests

- [x] Add a focused `test_meaning_boundaries.sh` integration entry or place the
  fixtures in the existing owning suites with one manifest entry.
- [x] Add the provider/consumer conditional-import fixture from section 4.3.
- [x] Assert hybrid preservation and strict rejection.
- [x] Add a fixture proving that provider obligation identity is not currently
  preserved, so the test distinguishes regeneration from exact preservation.
- [x] Record current artifact version, current PR baseline, and current main SHA
  in test diagnostics.
- [x] Run the full suite before semantic edits.

Gate: tests distinguish observed behavior from the stronger unproven PR claim.

### MB1: exact residual dependency model

- [x] Add immutable occurrence-to-obligation edges to compile metadata.
- [x] Make residual effect generation return the exact obligation ID.
- [x] Attach the ID to the source occurrence through the edge table.
- [x] Validate edge kind, occurrence, core Term, Context, and referenced rows.
- [x] Canonically sort and deduplicate dependency sets.
- [x] Compute export condition closure from frozen occurrence edges.
- [x] Delete the `any reachable pending obligation` acceptance loop.
- [x] Require either one exact accepted Claim or one complete conditional
  certificate.
- [x] Reject orphaned, extra, missing, duplicate-kind, and wrong-occurrence edges.

Gate: an unrelated reachable obligation can no longer authorize an export.

### MB2: conditional evidence propagation

- [x] Extend imported binding metadata with an exact relocated condition set.
- [x] Keep conditional evidence outside accepted JudgementDB Claims.
- [x] Do not approximate propagation by regenerating a local equation. The v84
  initial fragment preserves the provider set and rejects its first use.
- [x] Permit accepted Claim publication only for an empty condition set.
- [x] Preserve provider obligation identity through source-interface import.
- [x] Until all propagation paths are implemented, reject conditional imports
  at the first unsupported use.

Gate: no downstream accepted Claim depends on a conditional import unless every
condition has been discharged.

### MB3: effect discharge lifecycle

- [x] Define a kind descriptor for effect-row equations.
- [x] Reuse the canonical effect-row normal-form checker.
- [x] Implement compile/link discharge over relocated row Terms.
- [x] Decide explicitly whether host discharge is in the first supported
  fragment; omit it unless a replayable host evidence format is implemented.
- [x] Record that effect discharge cannot enable Claim reconstruction; typing
  residuals retain their own accepted-replay requirement.
- [x] Reject consumer-ready output with remaining unsupported conditions.
- [x] Keep pending residuals valid only in explicitly conditional output.

Gate: a condition becomes accepted evidence only through ordinary proof
reconstruction after solver validation.

### MB4: artifact v84

- [x] Add accepted/conditional export evidence syntax.
- [x] Add canonical residual dependency edges.
- [x] State that occurrence classifier status and effect residual status are
  independent axes.
- [x] Update dense publication rooting and relocation.
- [x] Update readback validation and linked-artifact aggregation.
- [x] Delete v83 reader/writer compatibility paths rather than remapping
  ambiguous evidence.
- [x] Update README and the archive index.

Gate: write/read/write is deterministic and every exported condition survives
relocation by identity and meaning.

### MB5: function-graph semantic validation

- [x] Correct sections 4, 7, and 9 of the function-graph plan to be mode-specific.
- [x] Add a shared post-read association validator.
- [x] Validate first-order projection structure.
- [x] Validate higher-order owner argument index and runner package structure.
- [x] Validate interface/adapter classifier coherence.
- [x] Invoke the validator for direct read, source import, relocation, and link.
- [x] Update diagnostics to name `first-order projection` or
  `higher-order certified variant`.

Gate: mutations that keep generated names but break classifier/package
structure are rejected.

### MB6: identity-root canonical replay

- [x] Move canonical extension recognition into the dimension module.
- [x] Replace the local private recognizer with the shared predicate.
- [x] Apply the predicate during artifact root validation.
- [x] Enforce nonzero repeated source dimension and overflow safety.
- [x] Keep family arity validation in `family_instance_info()` only.
- [x] Add a valid, accepted, noncanonical same-dimension operator fixture.
- [x] Reject designation of that fixture as a generic identity root.
- [x] Add a dimension-zero generic-root mutation.
- [x] Exercise direct read, relocation, and aggregate/link paths.

Gate: producer and reader accept exactly the same root-specific canonical
dimension condition.

### MB7: documentation and completion audit

- [x] Revise the PR #20 report before merge or supersede it with this verified
  wording.
- [x] Align README, VerificationDB comments, artifact schema, HOTT schema, and
  function-graph plan.
- [x] Confirm that no new DefEq/equality-reflection path was introduced.
- [x] Confirm JudgementDB remains the sole unconditional Claim authority.
- [x] Report per-file additions, deletions, and net lines.
- [x] Report focused and full-suite times using the repository timing harness.
- [x] Run `git diff --check`.

Gate: documentation describes the same authority transitions enforced by code.

## 8. Required negative tests

### Effect/export boundary

- [x] accepted export with exact Claim succeeds;
- [x] conditional export with exact complete dependencies succeeds in hybrid;
- [x] conditional export fails in strict/consumer-ready mode;
- [x] unrelated reachable computation-fold obligation does not authorize an
  effect-conditional export;
- [x] wrong occurrence, classifier, payload reference, or obligation ID fails;
- [x] missing a required dependency fails and duplicate `(occurrence, kind)`
  obligations fail before they can masquerade as independent conditions;
- [x] exact-set validation rejects extra dependencies; duplicate-condition
  mutation is retained as the wire-level regression fixture;
- [x] source import preserves exact provider dependency identity;
- [x] provider condition cannot disappear through aliasing;
- [x] successful link discharge removes only the solved dependency;
- [x] failed/ambiguous discharge creates no accepted Claim.

### Function-graph boundary

- [x] first-order projection association succeeds;
- [x] higher-order certified-variant association succeeds;
- [x] out-of-range certified argument index fails;
- [x] first-order sentinel on a higher-order package fails;
- [x] higher-order index on a first-order package fails;
- [x] swapped graph/result/interface/runner exports fail even with valid names;
- [x] wrong callback domain or adapter classifier fails;
- [x] imported and linked associations receive the same validation.

### Identity-root boundary

- [x] canonical repeated extension succeeds;
- [x] well-typed but noncanonical same-dimension operator fails as a root;
- [x] dimension-zero source tagged as repeated generic action fails;
- [x] wrong source/family/witness Claim combination fails;
- [x] malformed family arity remains rejected by the existing family helper;
- [x] relocation and aggregation preserve canonical acceptance.

## 9. Performance constraints

The preceding performance work reduced the full suite substantially. These
repairs must not reintroduce global repeated scans.

- Residual dependency closure is computed once per frozen graph and stored as
  canonical adjacency/spans.
- Export validation reads those spans and does not run one graph traversal per
  export-obligation pair.
- Function-graph classifier spines are decomposed once per association and may
  use a validation-local cache.
- Canonical dimension operator recognition is linear only in operator image
  count and is shared, not duplicated.
- Readback validators remain read-only and allocate bounded scratch storage once
  per artifact validation.
- New timing phases must report effect-certificate validation, function-graph
  association validation, and identity-root validation separately when nonzero.

Performance gates:

- [x] ordinary IF8 median regression below 10 percent;
- [x] Function Graph QuickSort median regression below 10 percent;
- [x] full warm integration-suite regression below 10 percent;
- [x] no Context/Substitution index rebuild appears in steady state;
- [x] deterministic artifact bytes remain unchanged across repeated builds of
  the same v84 input.

Any larger regression requires a profile and an algorithmic correction. It is
not accepted merely because the new checks are semantically important.

## 10. Rejected approaches

- Do not merge PR #20 unchanged as the final contract.
- Do not describe the source-interface case as demonstrated unconditional
  acceptance; the current minimal case regenerates a residual and strict mode
  rejects it.
- Do not retain generic reachability as conditional evidence.
- Do not copy mutable solver cells into artifacts.
- Do not make VerificationDB an alternate accepted-typing authority.
- Do not add conditional Claims to accepted JudgementDB.
- Do not use `classifier_verification_obligation` for effect rows; it belongs to
  classifier closure.
- Do not solve an effect equation by changing its state without running the row
  checker.
- Do not add a redundant function-graph mode field while the sentinel remains
  sufficient.
- Do not claim raw higher-order execution is certified by nominal association.
- Do not redesign the higher-order callable ABI merely to repair prose.
- Do not weaken identity roots to transport-only metadata; that contradicts the
  current v83 and HOTT contracts.
- Do not duplicate canonical dimension predicates in producer and reader.
- Do not serialize compiler-local HOTT action/search state.
- Do not change DefEq, conversion, or equality reflection for any issue here.

## 11. Completion record

This section is filled during implementation.

### Baseline

- Current implementation commit: `245524c`
- PR audit commit: `9308686`
- Artifact version before work: `83`
- Full integration result before work: `41/41` at `245524c`
- Full integration timing before work: `33.550 s` cold unified object cache,
  `16.047 s` warm unified object cache
- Focused performance baselines: IF8 median `183 ms`; Function Graph QuickSort
  median `1.719 s`

### Progress

- [x] PR content retrieved and reviewed.
- [x] PR baseline compared with current `main`.
- [x] Current semantic paths re-read.
- [x] Conditional source-import minimal experiment executed.
- [x] Claims classified as confirmed, narrowed, or rejected.
- [x] Implementation plan written.
- [x] MB0 complete.
- [x] MB1 complete.
- [x] MB2 complete with first-use rejection as the explicit v84 propagation boundary.
- [x] MB3 complete for compile/link ground rows; host discharge is intentionally absent.
- [x] MB4 complete.
- [x] MB5 complete.
- [x] MB6 complete.
- [x] MB7 complete.

### Final verification

- Focused effect tests: passed (`test_meaning_boundaries`, effect-row unit checks)
- Function-graph tests: passed
- HOTT tests: passed
- Artifact-flow tests: passed
- Full integration suite: `42/42`, final warm `17.569 s`
- Performance comparison: IF8 `183 ms -> 181 ms` (`-1.1%`); Function Graph
  QuickSort `1.719 s -> 1.709 s` (`-0.6%`); full warm suite
  `16.047 s -> 17.569 s` (`+9.5%`)
- Artifact determinism: passed for conditional evidence, function graphs, and IF8
- Steady-state Context/Substitution index rebuilds: `0` / `0`
- Per-file line delta: recorded below; total `+3824/-278`, net `+3546`
- Final commit: this implementation changeset; the resulting hash is reported
  by Git rather than embedded recursively in the commit itself

### Final line accounting

The counts use Git's rename-aware staged `--numstat`; the v83 wire source and
header are therefore counted as v84 renames rather than deletion plus addition.

| File | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `README.md` | 4 | 3 | +1 |
| function-graph implementation plan | 30 | 13 | +17 |
| this PR #20 plan and completion record | 857 | 0 | +857 |
| `src/prototype/README.md` | 5 | 5 | 0 |
| `src/prototype/build/sources.mk` | 1 | 1 | 0 |
| `src/prototype/calculus.h` | 2 | 2 | 0 |
| `artifact/interface.h` | 34 | 4 | +30 |
| `artifact/wire_v83.h -> wire_v84.h` | 2 | 2 | 0 |
| `core/term.h` | 9 | 0 | +9 |
| `dimension/operator.h` | 8 | 0 | +8 |
| `graph/compile_metadata.h` | 3 | 1 | +2 |
| `graph/verification.h` | 56 | 2 | +54 |
| `spec/archive/README.md` | 1 | 1 | 0 |
| `spec/artifact_v84.schema` | 366 | 0 | +366 |
| `artifact/interface.c` | 1038 | 4 | +1034 |
| `artifact/link.c` | 43 | 5 | +38 |
| `publication/closure_marking_and_slices.inc` | 0 | 29 | -29 |
| `publication/dense_publication.inc` | 44 | 11 | +33 |
| `publication/wire_primitives.inc` | 1 | 1 | 0 |
| `publication/writer.inc` | 71 | 8 | +63 |
| `artifact/relocation.c` | 7 | 4 | +3 |
| `artifact/wire_v83.c -> wire_v84.c` | 75 | 15 | +60 |
| `core/term/canonicalization.inc` | 28 | 0 | +28 |
| `dimension/action.c` | 3 | 1 | +2 |
| `dimension/operator.c` | 25 | 0 | +25 |
| `driver/program_storage.c` | 7 | 2 | +5 |
| `driver/read_file.c` | 135 | 89 | +46 |
| `constraint/branch_refinement_and_motives.inc` | 2 | 1 | +1 |
| `constraint/effect_propagation_and_residuals.inc` | 32 | 7 | +25 |
| `constraint/evidence_and_freeze.inc` | 2 | 2 | 0 |
| `lowering/context_and_type_lowering.inc` | 39 | 0 | +39 |
| `lowering/finalization_and_entrypoints.inc` | 11 | 0 | +11 |
| `graph/compile_metadata.c` | 6 | 2 | +4 |
| `typed_occurrence/runtime.inc` | 6 | 2 | +4 |
| `typed_occurrence/verification.inc` | 392 | 14 | +378 |
| `checks/hott/adt_identity.inc` | 20 | 0 | +20 |
| `checks/hott/test_support.inc` | 3 | 1 | +2 |
| `checks/spec_enum_check.c` | 1 | 1 | 0 |
| `checks/whnf_profile_cache_check.c` | 101 | 2 | +99 |
| `integration/test_artifact_flow.sh` | 24 | 34 | -10 |
| `integration/test_computation_block_sequence.sh` | 2 | 2 | 0 |
| `integration/test_definition_block.sh` | 1 | 1 | 0 |
| `integration/test_dependent_match_refinement.sh` | 2 | 2 | 0 |
| `integration/test_function_graph_certified_execution.sh` | 61 | 0 | +61 |
| `integration/test_hott_goal.sh` | 37 | 0 | +37 |
| `integration/test_meaning_boundaries.sh` | 222 | 0 | +222 |
| `integration/test_p0_certificate_boundary.sh` | 2 | 2 | 0 |
| `integration/test_spec_consistency.sh` | 3 | 2 | +1 |
| **Total** | **3824** | **278** | **+3546** |

## 12. Implementation order

The required order is:

```text
MB0
  -> MB1
  -> MB2
  -> MB3
  -> MB4
  -> MB5
  -> MB6
  -> MB7
```

MB1 through MB4 form one authority migration and must not be declared complete
individually in a released artifact format. MB5 and MB6 can be implemented and
reviewed independently after MB0, but the final schema/documentation update is
performed once in MB7.

The highest-risk change is conditional evidence propagation, not the row solver
itself. The implementation should stop if it can publish a downstream accepted
Claim while any imported condition remains pending. The correct temporary
behavior at that boundary is rejection, not classifier-only continuation.

## 13. Planned file ownership

| File/module | Planned responsibility |
| --- | --- |
| `src/prototype/include/a_program/graph/verification.h` | kind-indexed residual contract and immutable dependency-edge declarations |
| `src/prototype/src/graph/typed_occurrence/verification.inc` | obligation payload validation, kind descriptors, and discharge dispatch |
| `src/prototype/include/a_program/graph/typed_occurrence_model.h` | occurrence-facing dependency span/reference only; classifier status remains separate |
| `src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc` | create exact effect obligation and return its ID |
| `src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc` | freeze and validate occurrence condition sets; prohibit conditional Claim publication |
| `src/prototype/src/frontend/lowering/context_and_type_lowering.inc` | import accepted or conditional export evidence without dropping provenance |
| `src/prototype/include/a_program/artifact/interface.h` | v84 export authority and residual dependency records |
| `src/prototype/src/artifact/interface.c` | semantic export, function-graph, and identity-root validators |
| `src/prototype/src/artifact/publication/*` | dense rooting and deterministic writing of exact conditions |
| `src/prototype/src/artifact/link.c` | relocation, condition-set union, and checked effect discharge transition |
| `src/prototype/src/artifact/wire_v84.c` and header | local wire parsing/bounds checks only |
| `src/prototype/src/driver/read_file.c` | invoke semantic validators and delete generic reachability authorization |
| `src/prototype/src/dimension/action.c` and public header | one canonical dimension-extension predicate |
| `src/prototype/src/identity/action_certificate_validation.inc` | consume the shared dimension predicate |
| `src/prototype/spec/artifact_v84.schema` | accepted versus conditional export semantics and exact dependencies |
| `src/prototype/spec/hott_fragment_v6.schema` | clarify shared canonical-root replay predicate if wording changes are needed |
| function-graph implementation plan | mode-specific projection/certified-variant invariants |
| integration suites | all positive, forgery, import, relocation, link, and timing gates |

Do not put semantic classifier/Pi validation into the token scanner. Wire
parsing establishes syntax and bounds; artifact interface validation establishes
meaning after the accepted graph is available.
