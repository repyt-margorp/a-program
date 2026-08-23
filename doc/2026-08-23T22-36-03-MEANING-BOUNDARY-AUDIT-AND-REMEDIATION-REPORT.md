# Meaning Boundary Audit and Remediation Report

Date: 2026-08-23

Status: Revised draft for maintainer review; documentation-only change

Baseline: `1672c6a62de6e8a7fe815886682ac4436c2527c7` (`main`, 2026-08-23)

Review source: the `Meaning.eml` review correspondence, critically re-evaluated against the baseline above

## 1. Executive summary

This report does **not** find a demonstrated inconsistency in the trusted kernel, a construction of `False`, pollution of definitional equality, or a closed artifact that has been made unsound. It finds one high-confidence residual-lifecycle gap and two narrower contract/validation issues. The ordering below is remediation order, not a security-severity score.

| Order | Finding | Present assessment | Required outcome |
| --- | --- | --- | --- |
| 1 | Residual effect-row equations cross the artifact boundary under an overbroad reachability fallback, are dropped by source-interface import, and have no repository-implemented discharge transition | Confirmed lifecycle and validation gap; existing relocation checks prevent one important escalation path | Bind each provisional export to its exact residual conditions, preserve them across import, and implement or explicitly defer a kind-specific discharge transition |
| 2 | The function-graph plan gives a higher-order raw-owner exception and also states an unconditional projection invariant | Documentation/contract contradiction; the implementation follows the explicit higher-order exception | Make the invariant mode-specific and validate the existing first-order/higher-order distinction structurally |
| 3 | Artifact identity-root validation directly rechecks less than local certificate validation | Metadata-validation asymmetry; accepted proof replay validates the underlying claims, and no current semantic consumer of roots was found | Specify whether a root is a canonicality certificate or only a transport designation, then validate exactly that contract |

The common architectural issue is an **authority transition whose exact meaning is either implicit or weaker at a later boundary**. A Program already separates canonical terms, occurrences, proof judgements, residual verification obligations, and artifacts. The remediation should preserve that separation instead of making any one database a catch-all.

The recommended implementation order is:

1. make residual export dependencies exact and give effect equations a real lifecycle;
2. reconcile the higher-order exception with the global function-graph invariants;
3. specify the authority of higher-identity root records before strengthening their reader;
4. update architecture documentation and negative tests in the same changes that alter each boundary.

## 2. Scope, evidence standard, and non-claims

### 2.1 Scope

The review covers the current source-to-artifact pipeline, with focused inspection of:

- effect-row constraint generation, residual recording, artifact export acceptance, relocation, and backend policy;
- certified function-graph construction, higher-order argument handling, artifact associations, and tests;
- higher identity-root construction, serialization, artifact validation, and negative tests;
- current architectural documentation and artifact schema text.

The repository baseline was compared with its remote `main`; both resolved to the commit listed above. The integration suite passed 41 of 41 tests during the audit. Passing tests establish regression status, not the correctness of an incompletely specified boundary.

### 2.2 Evidence classes

This report distinguishes four kinds of statement:

1. **Observed implementation fact** — directly supported by the reviewed code or schema.
2. **Repository contract** — stated by current project documentation or tests.
3. **Literature-informed design principle** — useful for evaluating a design, but not a specification of A Program.
4. **Inference or recommendation** — proposed here and subject to maintainer approval.

The cited papers do not prescribe A Program's artifact format, database layout, or exact proof rules. They are used only to identify established semantic distinctions: value versus computation, static effect constraints versus execution, proof-carrying validation, computational bodies versus logical evidence, and dimension substitution in cubical type theory.

### 2.3 Explicit non-claims

The following stronger allegations are not supported by the current evidence:

- No accepted derivation of `False` has been produced.
- No invalid equality has been shown to enter `DefEq`.
- No term has been shown to acquire two contradictory accepted classifiers.
- The generated-binding reindexing used by function graphs has not been shown to bypass the ordinary proof-reconstruction pipeline.
- A nominal association between an owner and a certified graph is not itself unsound. It becomes misleading only if it is interpreted as proof that the raw owner executes through that graph.
- The higher-inductive implementation remains intentionally incomplete. Incompleteness is not inconsistency.

## 3. Current authority architecture

The current pipeline is summarized in [`README.md`](../README.md), especially lines 351-422:

```text
source
  -> surface AST and source binders
  -> indexing / canonical TermDB nodes
  -> ContextDB + SubstitutionDB
  -> TypedOccurrenceGraph
  -> mutable compiler constraints and bounded solving
  -> JudgementDB reconstruction and validation
  -> VerificationDB residual obligations
  -> artifact serialization / relocation / reading
  -> backend or runtime consumer
```

These layers should not be treated as interchangeable.

| Layer | Intended role | Authority it may hold | Authority it must not silently acquire |
| --- | --- | --- | --- |
| Surface AST / binders | Preserve syntax and source identity | Syntactic ownership and source mapping | Accepted typing or equality |
| TermDB | Canonical erased terms | Structural term identity | Occurrence-sensitive typing by itself |
| ContextDB / SubstitutionDB | Context and reindexing data | Checked context/substitution structure | A proof merely because a substitution was constructed |
| TypedOccurrenceGraph | Occurrence-sensitive relationships | Links among terms, contexts, classifiers, constraints, and claims | Accepted judgement without reconstruction |
| Constraint state | Mutable inference and solving | Provisional equations and obligations | Durable proof authority |
| JudgementDB | Reconstructed, kernel-validated claims | Accepted typing/equality/proof authority | Runtime facts not derivable statically |
| VerificationDB | Currently stores both runtime fold obligations and static effect equations | Explicit pending state whose kind remains visible | A replacement for an accepted typing claim |
| Artifact | Stable transport representation | Only the authority explicitly encoded and revalidated | Stronger authority than the producer possessed |
| Backend/runtime | Execute or discharge supported obligations | Capabilities declared by that consumer | Static proof reconstruction by implication |

The governing invariant proposed by this report is:

> Data may cross an authority boundary only through an explicit, checked transition that either preserves the exact condition or lets the receiving consumer replay or independently validate the property being claimed.

This invariant is already consistent with the repository's separation of TermDB, TypedOccurrenceGraph, JudgementDB, and VerificationDB. The findings below concern places where the transition is missing, ambiguous, or weaker on readback.

## 4. Literature-informed semantic baseline

### 4.1 Values, computations, and effects

Levy's call-by-push-value separates value types from computation types and makes sequencing of computations explicit [1]. Vákár extends this perspective to dependent types and distinguishes systems in which types may depend on values returned by computations, including the additional conditions required for subject reduction [2]. Ahman, Ghani, and Plotkin give a categorical account of dependent types with computational effects [3].

For this audit, the narrow consequence is that a static equation used to infer an effect row is not automatically a runtime predicate. Moving it into a runtime-oriented residual database requires a defined consumer and a sound discharge rule; the papers do not supply one automatically.

Leijen's row-polymorphic effect system presents effect rows as part of static type inference and generalization [4]. This supports treating an unresolved row equation as compiler/linker state. It does not imply that all row equations must be solved within one compilation unit: separately compiled objects may retain constraints, provided the artifact lifecycle identifies them as relocatable constraints and a later phase actually solves them.

### 4.2 Computational definitions and logical evidence

Bove and Capretta model general recursive functions by separating a computational component from an accessibility predicate that supplies logical justification [5]. That separation is relevant to A Program's function graphs: executable code and evidence may be represented separately. It does **not** prove that two separately stored callables are extensionally equal, nor that evidence for one callable automatically certifies another.

The project's public contract must therefore state consistently whether a function graph certifies:

- the exact callable executed by the owner;
- a separately packaged certified variant; or
- only the graph relation, with no claim about the raw owner's operational path.

### 4.3 Consumer-side validation and semantic preservation

Proof-carrying code makes the consumer validate evidence before trusting code [6]. Verified compilation establishes preservation by a proved relationship between source and target semantics [7]. Both are useful analogies for A Program's reader-side checks and preference for replayable evidence. They do not establish any A Program-specific artifact property; the exact export/evidence relationship still has to be defined and checked by this repository.

### 4.4 Cubical dimensions and canonical actions

Cubical type theory gives dimension names, substitutions, and composition operations computational significance [8]. It therefore motivates treating a dimension-extension operator as semantic data rather than an incidental serialization detail. The exact `source_dimension + 1`, boundary shape, and operator encoding used by A Program remain project-specific invariants and must be defined and checked by A Program itself.

## 5. Finding A — residual effect-row lifecycle and export evidence are underspecified

### 5.1 What an unresolved equation can mean

An unresolved effect-row equation can legitimately exist in a separately compiled artifact. It is not, by itself:

- an accepted `HAS_TYPE` judgement;
- a computation-fold runtime check;
- a backend capability;
- proof that an exported declaration is unconditionally usable; or
- a constraint that has already been solved by linking.

Such an artifact can still be meaningful if its interface is explicitly conditional: the export is usable only when a named set of static obligations is discharged. A Program does not currently name artifact stages such as “relocatable” and “closed”; those terms below describe a proposed distinction, not an existing field or command-line mode.

### 5.2 Observed implementation path

The reviewed implementation performs this sequence:

1. [`effect_propagation_and_residuals.inc`](../src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc), lines 1092-1160, records every non-solved effect constraint as a pending `PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION`.
2. [`verification.h`](../src/prototype/include/a_program/graph/verification.h), lines 37-42, describes records as conditional runtime obligations that are never closed typing derivations, while also defining the effect-equation kind.
3. [`verification.inc`](../src/prototype/src/graph/typed_occurrence/verification.inc), lines 167-176, gives that kind no required runtime capability. A repository-wide reference search found only computation-fold discharge, implemented at lines 326-342 and called from the runtime.
4. [`read_file.c`](../src/prototype/src/driver/read_file.c), lines 608-684, accepts an export without an exact accepted `HAS_TYPE` claim when **any** pending obligation is reachable from the export occurrence. The fallback does not check obligation kind, exact occurrence equality, classifier, or an enumerated dependency set.
5. The driver compile policy defaults to `HYBRID` in the same file at line 3097. This is a compiler-wide default, not an existing artifact-stage flag.
6. [`artifact_v83.schema`](../src/prototype/spec/artifact_v83.schema), lines 277-281, documents that an effect equation may be discharged by linking or a declared host boundary and is never an accepted typing claim by itself.
7. Link aggregation relocates and copies effect obligations in [`read_file.c`](../src/prototype/src/driver/read_file.c), lines 1042-1065, but does not solve them. The integration test at [`test_artifact_flow.sh`](../src/prototype/tests/integration/test_artifact_flow.sh), lines 3116-3123, deliberately requires a pending equation to survive the reviewed link operation.
8. Source-interface loading relocates the provider's artifact graph through `prototype_artifact_append_graph()` in [`read_file.c`](../src/prototype/src/driver/read_file.c), lines 2337-2367, but that operation has no source or target compile-metadata/VerificationDB argument. The same path deliberately omits the provider's accepted judgement graph, as documented at lines 2370-2373. Later, source lowering obtains the exported classifier and calls it authoritative in [`context_and_type_lowering.inc`](../src/prototype/src/frontend/lowering/context_and_type_lowering.inc), lines 1347-1385, without examining `source_evidence`. A provider's residual condition is consequently neither copied into the source compilation's verification state nor attached to the imported classifier.
9. The schema says at [`artifact_v83.schema`](../src/prototype/spec/artifact_v83.schema), lines 198-207, that a solved occurrence has no verification obligation and every deferred semantic requirement is referenced from a `RESIDUAL_VERIFICATION` occurrence. Effect recording requests no returned obligation ID and does not set `classifier_verification_obligation`; [`evidence_and_freeze.inc`](../src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc), lines 3962-4012, assigns that field only for computation-fold classifier residuals and clears it for solved classifiers. The effect/export relationship is therefore recovered later by reachability rather than the exact reference described by the schema. Reader checks in [`wire_v83.c`](../src/prototype/src/artifact/wire_v83.c), lines 2690-2702, require a classifier for `SOLVED` and an obligation for `RESIDUAL_VERIFICATION`, but do not enforce the schema's converse rule that a solved occurrence has no obligation.

No effect-equation discharge implementation was found in the reviewed repository. The schema therefore documents a possible later transition for which this baseline contains neither a linker implementation nor a declared-host evidence format.

### 5.3 Existing defenses that limit the claim

The current code does **not** simply convert a residual into an accepted claim:

- strict compilation rejects retained verification state in [`finalization_and_entrypoints.inc`](../src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc), lines 2279-2283;
- serialized residual exports retain invalid source evidence rather than a fabricated Claim ID, as selected in [`interface.c`](../src/prototype/src/artifact/interface.c), lines 832-844;
- [`relocation.c`](../src/prototype/src/artifact/relocation.c), lines 184-190, refuses to use a residual provider export to discharge an external declaration; and
- [`test_artifact_flow.sh`](../src/prototype/tests/integration/test_artifact_flow.sh), lines 2766-2778, requires hybrid preservation and strict rejection of the unresolved case.

These checks are material. They are why this report does not claim a demonstrated unsound closed artifact or kernel derivation. The confirmed issue is narrower: the conditional export/effect relationship is validated by generic graph reachability, source-interface import drops the provider verification state while treating its classifier as authoritative, and no implemented transition discharges the condition.

The artifact-flow suite separately covers ordinary source-interface import and hybrid residual preservation/linking, but no focused test was found that imports a hybrid residual provider through the source-interface path. The import-loss conclusion above is based on the data passed by that code path, not on a completed exploit fixture.

### 5.4 Documentation drift

[`README.md`](../README.md), lines 399-422, says that VerificationDB currently stores dependent computation-fold runtime checks and that effect constraints remain in their owning subsystem. The code and version 83 schema contradict that storage description by persisting effect equations in VerificationDB. The schema's own lines 198-207 also describe an exact residual-occurrence reference that the effect path does not create.

The README's separate statement that residual state is not published as a completed `HAS_TYPE` judgement remains literally true: the code does not synthesize such a claim. The problem is instead that `artifact_exports_have_accepted_claims()` accepts an export with no such claim under a weaker generic-reachability condition.

### 5.5 Why generic reachability is insufficient

Reachability answers “is there a path in the occurrence graph?” It does not answer:

- whether the obligation is the reason this export lacks source evidence;
- whether the obligation constrains the exported classifier;
- whether discharging it would permit reconstruction of the missing claim;
- whether the next phase implements a verifier for that obligation kind; or
- whether all conditions for the export have been enumerated.

The current occurrence model already has an exact `classifier_verification_obligation` field for residual classifier verification, but effect residuals keep a solved classifier and are not represented through that field. A repair should reuse the same design principle—an exact, checked dependency—without pretending that an effect equation is a runtime classifier check.

### 5.6 Required invariants and design alternatives

The correctness requirements do not force one physical database layout. Either of the following can be coherent:

- **Static-store design:** keep unresolved effect equations in a relocatable constraint section outside VerificationDB, matching the current README's ownership description.
- **General-residual design:** keep them in VerificationDB, but change the database contract from “runtime obligations” to a kind-indexed residual store whose consumer phase and discharge operation are explicit.

In either design:

1. a provisional export identifies its exact occurrence, classifier, context, and complete residual-obligation set;
2. an obligation kind is accepted only at a phase that preserves or can discharge it;
3. link/host discharge validates relocated row terms with a specified deterministic checker or the canonical effect solver;
4. successful discharge returns through ordinary judgement reconstruction if an accepted claim is to be created;
5. a consumer-ready artifact cannot retain an effect equation unless its host contract explicitly owns and validates that equation; and
6. generic reachable pending state never substitutes for export-specific evidence.

An explicit artifact stage is one reasonable encoding:

```c
enum prototype_artifact_stage {
    PROTOTYPE_ARTIFACT_RELOCATABLE,
    PROTOTYPE_ARTIFACT_CONSUMER_READY
};
```

This enum is a proposal, not a finding. Equivalent semantics could be expressed by a link command contract or by separate artifact kinds.

### 5.7 Proposed implementation phases

#### ER0 — specify the conditional interface

- Decide whether effect equations remain in VerificationDB or move to a static constraint section.
- Define the exact export-to-obligation relationship.
- Define the point at which an artifact becomes consumer-ready.
- If a host may discharge rows, specify the host evidence format and validation algorithm; “declared host boundary” alone is incomplete.

#### ER1 — replace generic reachability

- Replace the fallback in `artifact_exports_have_accepted_claims()` with export-specific residual evidence.
- Require an exact accepted `HAS_TYPE` claim whenever no such conditional evidence is present.
- Reject wrong-kind, wrong-occurrence, duplicated, orphaned, incomplete, or classifier-incompatible references.
- Ensure interface import preserves the conditionality of the classifier instead of copying only the classifier term.

#### ER2 — implement a discharge transition

- Relocate every term, row variable, label, and declaration reference in an effect equation.
- Define how imported and local row constraints are merged.
- Invoke a specified effect-row checker/solver rather than changing the obligation state by convention.
- Reconstruct accepted typing evidence through the ordinary judgement path when discharge closes the export.
- Otherwise preserve the exact condition or fail the consumer-ready transition.

#### ER3 — align policy and documentation

- Decide whether `HYBRID` output is always conditional/relocatable or may also target a host contract.
- Update the README, VerificationDB header comment, schema, diagnostics, and tests together.
- Bump the artifact version if the wire meaning or export evidence record changes.

### 5.8 Acceptance tests

At minimum, add coverage for:

- exact effect obligation preserved in a conditional artifact;
- unrelated reachable computation-fold obligation cannot authorize an export;
- wrong occurrence, classifier, context, or incomplete obligation set is rejected;
- interface import retains the provider condition rather than treating the classifier as unconditional;
- strict downstream compilation cannot erase a hybrid provider's condition merely by importing its interface;
- successful link/host evidence discharges the row and reconstructs the intended claim;
- missing or ambiguous provider information prevents the consumer-ready transition;
- residual provider export cannot discharge an external declaration;
- read-write-read and relocation preserve the exact condition without changing its authority class.

### 5.9 Rejected shortcuts

- Do not mark an effect equation accepted merely because its referenced terms are present.
- Do not give every backend a dummy effect capability.
- Do not reinterpret a pending effect row as a completed typing claim.
- Do not retain the generic reachability fallback under a narrower name.
- Do not require a new artifact-stage enum if an existing command contract can express and enforce the same lifecycle; the semantic distinction, not the spelling, is required.

## 6. Finding B — the higher-order function-graph contract is internally inconsistent

### 6.1 The implementation follows the explicit higher-order exception

The function-graph plan contains an explicit corrected higher-order contract. [`2026-08-22T18-06-00-FUNCTION-GRAPH-CERTIFIED-EXECUTION-AND-RETURNS-REMOVAL-PLAN.md`](2026-08-22T18-06-00-FUNCTION-GRAPH-CERTIFIED-EXECUTION-AND-RETURNS-REMOVAL-PLAN.md), section 3.7, says that the generated graph/certified interface receives proof-side callback parameters while executable QuickSort keeps its raw comparator.

The implementation matches that text:

- [`function_graph.c`](../src/prototype/src/frontend/function_graph.c), lines 7921-7928, generates a binary proof-side callback package for the higher-order case;
- lines 7996-8007 deliberately retain the accepted raw owner as executable authority and generate a projection only for first-order definitions; and
- lines 8015-8042 record the owner, graph, result, interface, adapter, runner, executable assignment, and certified argument index in compile metadata.

The missing relationship is specifically between the raw owner's operational callback and the extra proof-side interface/certified callback accepted by the generated runner; the proof-side package itself is already implemented.

### 6.2 The plan also states a conflicting universal invariant

The same plan says in section 4.4 that executable `f` is the evidence-erasing projection of certified execution. Lines 999-1000 make this unconditional whenever a graph interface is published, and lines 1022-1026 name separately authoritative executable `f` and certified `*f` as a stop condition.

Those statements are true of the first-order branch but not of the implemented higher-order branch. This is first a documentation/semantic-contract contradiction, not evidence that the implementation violates its explicit section 3.7 design.

### 6.3 The artifact already has an implicit mode discriminator

The artifact association already has an implicit semantic mode. [`artifact_v83.schema`](../src/prototype/spec/artifact_v83.schema), lines 184-196, defines `CERTIFIED_ARGUMENT_INDEX == 4294967295` as first-order and any other value as identifying the raw higher-order argument followed by its graph interface and certified callback in generated families and the runner. The field in [`interface.h`](../src/prototype/include/a_program/artifact/interface.h), lines 101-109, therefore acts as a first-order/higher-order discriminator.

What the reader validates is narrower. [`wire_v83.c`](../src/prototype/src/artifact/wire_v83.c), lines 87-153 and 480-528, checks referenced export bounds, naming conventions, optional adapter bounds, and uniqueness of the owner association. It does not rederive from accepted classifiers that:

- `CERTIFIED_ARGUMENT_INDEX` is within the raw owner's argument spine;
- the runner has the raw argument followed by the expected interface and certified callback at that position;
- graph/result/interface/adapter/runner classifiers form the package described by the schema; or
- an adapter is present exactly when the corresponding callback shape requires one.

This is an artifact structural-validation gap. It is not an extensional-equivalence gap unless the project chooses to claim that the raw owner and certified runner execute identically.

### 6.4 What is and is not established

Generated AST objects pass ordinary indexing, constraint solving, judgement reconstruction, and accepted replay. The audit therefore does not identify generated binding construction as an authority bypass.

Under the explicit section 3.7 contract:

- the raw owner is accepted ordinary executable code;
- the runner is separately accepted certified code with additional proof-side inputs; and
- the artifact association is nominal ownership and package metadata.

This contract does not establish, and need not claim, that the raw owner is a projection of the runner or that arbitrary raw callbacks carry a graph interface. A defect arises only if documentation or tooling presents `@f`/`*f` as certification of the raw owner's higher-order execution path.

### 6.5 Test coverage and the correct test oracle

[`test_function_graph_certified_execution.sh`](../src/prototype/tests/integration/test_function_graph_certified_execution.sh), lines 51-89, exercises certified QuickSort graph generation. The explicit executable-projection check at lines 91-95 uses the first-order fixture.

The lack of a higher-order projection-equality test is not a coverage gap under section 3.7: the higher-order owner is intentionally not projected. The missing tests are artifact-coherence tests for the higher-order association and tests that user-facing output distinguishes “certified variant available” from “raw owner executes by certified projection.”

### 6.6 Required correction and optional redesign

The minimal correction is to retain the implemented contract and make every invariant mode-specific:

| Mode | Existing encoding | Required meaning |
| --- | --- | --- |
| First-order projection | `CERTIFIED_ARGUMENT_INDEX == INVALID` | Published owner is the projection of the certified runner |
| Higher-order certified variant | `CERTIFIED_ARGUMENT_INDEX != INVALID` | Raw owner remains executable; runner is certified only when supplied the matching proof-side callback interface |

No new enum is required if the sentinel encoding remains documented and validated consistently. An enum may improve readability, but adding one is not itself a correctness repair.

A stronger design in which the raw higher-order owner consumes a certified callback package and becomes a projection is possible. It would change the source/ABI contract and should be treated as a separate language feature, not as the mandatory repair for the present documentation conflict. An extensional-equivalence proof between the current raw and certified variants would be an even larger, separately specified proof obligation.

### 6.7 Proposed implementation phases

#### FG0 — correct the plan

- Qualify section 4.4, invariant 7, and the stop condition by first-order versus higher-order mode.
- State precisely what `@f` and `*f` mean for a higher-order owner.
- State that nominal association does not certify the raw owner's execution path.

#### FG1 — strengthen existing association validation

- Validate `CERTIFIED_ARGUMENT_INDEX` against the accepted owner and runner classifiers.
- Validate the expected extra graph-interface/certified-callback positions in the runner.
- Validate graph/result/interface/adapter ownership and classifier coherence, not names alone.
- Change the wire format only if those checks require additional non-derivable data; otherwise no version bump is necessary.

#### FG2 — add mode-specific tests

- Assert first-order structural projection.
- Assert higher-order runner package shape without asserting raw-owner projection.
- Reject out-of-range certified argument indices and mismatched owner, graph, result, interface, adapter, or runner exports.
- Cover imported associations, anonymous callback rejection/residual behavior, relocation, and linking.
- Verify that diagnostics and inspection tools distinguish raw-owner execution from certified-variant availability.

### 6.8 Rejected shortcuts

- Do not equate nominal association with extensional equality.
- Do not apply the first-order projection test oracle to the deliberate higher-order exception.
- Do not add a redundant mode field without first deciding whether the existing sentinel is insufficient.
- Do not redesign the higher-order ABI merely to repair contradictory prose.

## 7. Finding C — the identity-root record has a weaker contract than the local certificate

### 7.1 Local certificate validation

The local generic-action validator in [`action_certificate_validation.inc`](../src/prototype/src/identity/action_certificate_validation.inc) performs a substantial canonicality check. The helper at lines 283-305 recognizes the canonical dimension extension. The validation at lines 695-803 checks, among other conditions:

- source dimension is nonzero and does not overflow on extension;
- higher family/backing and source relationships match;
- target dimension is source dimension plus one;
- higher boundary count is `source_boundary_count * 3 + 2`;
- the higher operator is the canonical extension; and
- endpoint contexts, bindings, dimension applications, and boundary terms have the expected shape.

The producer path in [`artifact_root_extraction.inc`](../src/prototype/src/identity/artifact_root_extraction.inc), lines 36-64, calls full action-database validation before registering a root. Artifacts produced normally by this baseline therefore start from the stronger local certificate checks.

### 7.2 What artifact readback does recheck

For a generic dimension-action root, [`interface.c`](../src/prototype/src/artifact/interface.c), lines 349-396, opens the source and higher family instances, requires the higher action's source to match the source family, and requires a one-dimension increase. It does not directly require a nonzero source dimension or the canonical extension operator, and it discards several decoded identities after those checks.

[`action.c`](../src/prototype/src/dimension/action.c), lines 78-136, shows that `prototype_dimension_action_family_instance_info()` accepts an instance only when its application count equals the boundary count for its target dimension. [`face.c`](../src/prototype/src/dimension/face.c), lines 8-37, defines that count as `3^dimension - 1`, so adjacent dimensions imply `higher_count = 3 * source_count + 2`. Since the artifact validator calls the helper for both instances and compares adjacent dimensions, the arity relationship is already enforced indirectly and is not part of the remaining gap.

Artifact reading also replays accepted JudgementDB proofs. [`dimension_action.inc`](../src/prototype/src/kernel/rules/introduction/dimension_action.inc), lines 1-104, validates operator source/target dimensions and the classifier of a dimension-action type-formation proof. Thus arbitrary malformed operators or family applications are not accepted merely because the root-level predicate is shallow.

### 7.3 Remaining direct asymmetry

The root-level reader still does not directly establish all properties of the local certificate:

- source dimension is greater than zero;
- the selected operator is the project-specific canonical extension rather than another well-typed operator with the same dimensions;
- local endpoint/context-certificate identities correspond to the designated root; and
- the root's generic computation-rule tag denotes the same theorem as the local action certificate, rather than only a broad source/family relationship.

The serialized record in [`interface.h`](../src/prototype/include/a_program/artifact/interface.h), lines 94-99, contains three Claim IDs and a computation-rule tag. It does not carry the local action certificate fields needed to replay every construction-time check.

### 7.4 Current impact is metadata integrity

A repository-wide reference audit found identity roots being produced, serialized, validated, relocated, linked, and printed, but found no frontend lowering, kernel rule, or runtime execution path that consumes the root designation as new proof authority. The claims named by a root are still independently accepted JudgementDB claims.

No accepted artifact was constructed in this audit in which a noncanonical but well-typed action was successfully relabelled as a generic identity root. Consequently:

- the direct predicate asymmetry is confirmed;
- acceptance of a noncanonical root is not confirmed; and
- current impact is best described as artifact metadata integrity and future-proofing.

This is lower priority than the effect lifecycle. It should become higher priority before any consumer uses `computation_rule` to select proof, normalization, or execution behavior.

### 7.5 Contract decision

The project should first decide what an identity-root record claims:

| Contract | Meaning | Required validation |
| --- | --- | --- |
| Transport designation | Names accepted source/family/witness claims and a broad rule family | Current broad checks may be sufficient, but schema and names must not imply replay of the local certificate |
| Canonical-action certificate | Certifies the exact generic higher action constructed locally | Reader must replay the canonical operator, dimension, context, endpoint, and rule-specific conditions |

If the second contract is intended, canonicality should have one shared executable predicate. If some facts cannot be reconstructed from the accepted claims, the artifact needs a compact replay certificate. If the first contract is intended, adding unused certificate data would be unnecessary; the documentation should instead narrow the record's authority.

### 7.6 Proposed implementation phases

#### HI0 — specify the root claim

- State whether the record is a transport designation or canonical-action certificate.
- List facts already implied by accepted proof replay and `family_instance_info()`.
- List only the additional root-specific facts that must be rechecked.

#### HI1 — share canonical validation if required

- Move the canonical extension predicate into a shared identity/dimension module.
- Reuse it from local certificate validation and artifact readback.
- Serialize only non-derivable certificate facts.

#### HI2 — add discriminating tests

Existing forgery coverage in [`test_hott_goal.sh`](../src/prototype/tests/integration/test_hott_goal.sh), lines 129-163, changes source Claim IDs and computation-rule tags. It does not exercise a separately accepted noncanonical operator with the same dimensions.

- Construct a valid accepted action using a noncanonical operator with the same source/target dimensions and attempt to designate it as generic.
- Attempt a dimension-zero source designation.
- Reject wrong source/family/witness Claim combinations.
- Exercise write/read, relocation, and linked-artifact paths.
- Retain malformed-term tests as defense in depth, but do not confuse them with root-canonicality tests.

### 7.7 Rejected shortcuts

- Do not infer missing arity validation solely from unused local variables when the called helper already validates arity.
- Do not duplicate the local predicate in the artifact reader.
- Do not serialize a full certificate before determining which facts accepted replay already implies.
- Do not cite cubical type theory as if it specified A Program's canonical operator or boundary encoding.

## 8. Reassessment of the remaining `Meaning.eml` topics

| Topic | Reassessment | Action |
| --- | --- | --- |
| CwF substitution / generated binding | No demonstrated bypass: generated syntax is later indexed and validated through the ordinary pipeline | No correctness fix proposed; retain focused regression tests |
| Nominal function-graph association | Nominal ownership is legitimate metadata | Clarify that it is not execution equivalence; address under Finding B |
| Effect residuals | Confirmed lifecycle/discharge gap with meaningful existing defenses | Replace generic reachability and specify conditional import/link semantics first |
| Higher identity artifacts | Confirmed direct-predicate asymmetry; helper and accepted-proof replay cover more than the first report credited | Specify root authority, then add only the missing checks |
| Kernel consistency | No contradictory judgement or `False` construction established | Do not frame this remediation as a kernel-unsoundness incident |

## 9. Cross-cutting implementation requirements

### 9.1 Preserve one proof authority

JudgementDB remains the authority for accepted claims. Neither VerificationDB, artifact reachability, nominal graph associations, nor identity-root tags should become an alternate path to accepted typing or equality.

### 9.2 Do not change definitional equality to repair packaging

None of the findings requires a new `DefEq` rule. Effect linking, function-graph association coherence, and higher-action designation should be handled at their own boundaries. Expanding equality would obscure the relevant contracts and enlarge the trusted surface.

### 9.3 Version wire changes explicitly

Adding artifact stage, export-specific residual evidence, or a higher-action replay certificate changes serialized meaning. A new function-graph mode field would also change the wire format, but is not required if the existing `CERTIFIED_ARGUMENT_INDEX` sentinel is sufficient. Introduce a new schema version only for an actual wire or wire-semantic change.

### 9.4 Make diagnostics identify the failed transition

Diagnostics should state:

- the authority being requested;
- the exact record or occurrence involved;
- the missing or incompatible evidence;
- the phase capable of resolving it.

Examples include “consumer-ready export lacks accepted or conditional evidence,” “effect row remains unresolved after link,” “higher-order association certifies the runner variant, not raw-owner projection,” and “identity-root contract requires a canonical extension operator.”

### 9.5 Keep mutable solver state out of stable proof data

Relocatable effect constraints need stable IDs and serialization, but they remain inputs to re-solving. Solver worklists, convergence counters, or provisional union-find representatives should not become durable proof evidence.

### 9.6 Avoid compatibility facades that preserve the ambiguity

The existing version 83 sentinel already distinguishes first-order from higher-order associations and should be preserved if its meaning remains unchanged. By contrast, mapping version 83 effect residuals to a new export-specific evidence record would claim a relationship the old format did not encode. Reject, conservatively classify, or require explicit conversion with revalidation.

## 10. Proposed change map

| Area | Likely files | Purpose |
| --- | --- | --- |
| Artifact lifecycle | `spec/artifact_v*.schema`, artifact interface headers and read/write code | Express conditional versus consumer-ready output if an explicit stage is chosen |
| Effect residual generation | `frontend/lowering/constraint/effect_propagation_and_residuals.inc` | Preserve exact static row conditions under the chosen residual-store design |
| Export/import gate | `driver/read_file.c`, `context_and_type_lowering.inc` | Bind conditional classifiers to exact obligations across import |
| Effect relocation/linking | `artifact/relocation.c`, linker/constraint modules | Preserve or discharge effect equations through a specified transition |
| VerificationDB | `graph/verification.h`, `graph/typed_occurrence/verification.inc` | Align the database description with its runtime and static obligation kinds |
| Function graphs | function-graph plan, `artifact/wire_v83.c`, association records | Make existing sentinel modes explicit and validate classifier/package coherence |
| Higher identities | `identity/action_certificate_validation.inc`, `dimension/action.c`, `artifact/interface.c` | Define root authority and share only required canonical checks |
| Documentation | `README.md`, function-graph plan, artifact schema comments | Remove contradictions and state authority transitions |
| Integration tests | artifact-flow, function-graph, HOTT suites | Add negative, relocation, link, and readback coverage |

Exact file allocation should follow module ownership discovered during implementation. The table is a review map, not permission to duplicate logic across every listed file.

## 11. Delivery sequence and gates

### Phase 1 — conditional effect authority

Deliver Finding A as an isolated change series. The gate is an export-specific residual relationship, preservation of that condition and verification state across source-interface import, and either a demonstrated discharge transition or an explicitly conditional output that cannot be mistaken for consumer-ready authority.

### Phase 2 — function-graph contract

First correct the function-graph plan to describe the existing first-order and higher-order modes. The gate is that the sentinel modes have precise reader-validated meanings and higher-order tests do not inherit the first-order projection claim.

### Phase 3 — higher-identity root contract

Decide whether the record is a transport designation or canonicality certificate. If it is the latter, centralize the missing predicate and add discriminating accepted-graph fixtures. Exact construction/readback equivalence is unnecessary if the record intentionally claims less and is named accordingly.

### Phase 4 — architecture documentation audit

Reconcile `README.md`, the artifact schema, and focused design documents with the landed behavior. Older design-philosophy prose should be marked historical or updated where it conflicts with the current surface-AST and occurrence-graph architecture.

Each phase should be independently reviewable. Do not combine these boundary repairs with unrelated language features.

## 12. Verification matrix

| Requirement | Positive case | Negative case | Boundary exercised |
| --- | --- | --- | --- |
| Effect row local solve | row is solved and accepted claim reconstructed | contradictory row labels fail | compiler -> JudgementDB |
| Effect row transition | link/host evidence closes a named row condition | missing/ambiguous evidence remains conditional or fails | conditional artifact -> consumer boundary |
| Conditional import | classifier and exact condition are imported together | classifier-only import fails | artifact interface -> source compiler |
| Export authority | exact accepted claim or exact conditional evidence matches | unrelated pending obligation fails | artifact export gate |
| First-order graph | owner structurally projects through runner | mismatched runner fails | graph association reader |
| Higher-order certified variant | sentinel reports runner package without raw-owner projection | tool treating raw owner as projected fails | schema/tooling contract |
| Higher-order package coherence | runner accepts matching proof-side package | out-of-range index or callback/interface mismatch fails | artifact reader/import |
| Higher identity designation | declared root contract validates | accepted noncanonical action fails only if canonical contract is chosen | accepted graph -> root designation |
| Higher identity serialization | write/read and relocation preserve the declared root meaning | wrong claim combination fails | artifact reader/linker |

Required regression commands should include the repository's complete integration target and the focused artifact-flow, function-graph, and HOTT tests. New tests should run through the public driver paths used by real artifact producers and consumers; unit-only validation is insufficient for these findings.

## 13. Completion audit checklist

Before declaring the remediation complete, verify all of the following:

- [ ] Every conditional export names its exact effect obligations.
- [ ] An effect equation has a specified preserving or discharging consumer transition.
- [ ] Interface import preserves conditionality rather than copying a bare authoritative classifier.
- [ ] Export typing authority is exact and occurrence-specific.
- [ ] VerificationDB documentation names the consumer of every obligation kind.
- [ ] The default driver mode cannot present conditional output as consumer-ready output.
- [ ] The schema and README describe the same residual-state ownership as the code.
- [ ] Every function-graph association's existing sentinel mode has a precise meaning.
- [ ] Higher-order raw callbacks are never reported as certified solely by nominal association.
- [ ] The chosen higher-order contract has mismatch and relocation tests.
- [ ] Identity-root authority is specified as transport-only or canonical.
- [ ] If canonical, accepted but noncanonical action fixtures are rejected as roots.
- [ ] No remediation introduces a new definitional-equality rule or a second accepted-judgement authority.
- [ ] Wire-semantic changes receive an explicit artifact-version bump.
- [ ] The full integration suite and all new negative tests pass.

## 14. Open maintainer decisions

Three decisions must be made explicitly; implementation should not infer them from current behavior:

1. **Artifact lifecycle:** Is host-provided effect-row discharge part of the language contract, or is linking the only permitted discharge phase? If hosts participate, the validated host evidence format must be specified.
2. **Higher-order graph evolution:** Is the explicit section 3.7 raw-owner/certified-variant contract final, or should a future language change make the raw owner consume a certified callback package? The present correctness repair does not require the latter.
3. **Higher identity export:** Is an identity root only a transport designation, or does it certify the exact local canonical action? The latter may require additional replay data.

These are semantic choices, not code-style questions. Once chosen, the implementation and documentation can be made mechanically consistent.

## 15. References

1. Paul Blain Levy. “Call-by-Push-Value: A Subsuming Paradigm.” *Typed Lambda Calculi and Applications (TLCA 1999)*, Lecture Notes in Computer Science 1581, 1999, pp. 228-243. DOI: [10.1007/3-540-48959-2_17](https://doi.org/10.1007/3-540-48959-2_17).
2. Matthijs Vákár. “An Effectful Treatment of Dependent Types.” arXiv:1603.04298, 2016. [https://arxiv.org/abs/1603.04298](https://arxiv.org/abs/1603.04298).
3. Danel Ahman, Neil Ghani, and Gordon D. Plotkin. “Dependent Types and Fibred Computational Effects.” *Foundations of Software Science and Computation Structures (FoSSaCS 2016)*, Lecture Notes in Computer Science 9634, 2016, pp. 36-54. DOI: [10.1007/978-3-662-49630-5_3](https://doi.org/10.1007/978-3-662-49630-5_3).
4. Daan Leijen. “Koka: Programming with Row Polymorphic Effect Types.” *Electronic Proceedings in Theoretical Computer Science* 153, 2014, pp. 100-126. DOI: [10.4204/EPTCS.153.8](https://doi.org/10.4204/EPTCS.153.8); [arXiv:1406.2061](https://arxiv.org/abs/1406.2061).
5. Ana Bove and Venanzio Capretta. “Modelling General Recursion in Type Theory.” *Mathematical Structures in Computer Science* 15(4), 2005, pp. 671-708. DOI: [10.1017/S0960129505004822](https://doi.org/10.1017/S0960129505004822).
6. George C. Necula. “Proof-Carrying Code.” *Proceedings of the 24th ACM SIGPLAN-SIGACT Symposium on Principles of Programming Languages (POPL 1997)*, 1997, pp. 106-119. DOI: [10.1145/263699.263712](https://doi.org/10.1145/263699.263712).
7. Xavier Leroy. “Formal Verification of a Realistic Compiler.” *Communications of the ACM* 52(7), 2009, pp. 107-115. DOI: [10.1145/1538788.1538814](https://doi.org/10.1145/1538788.1538814).
8. Cyril Cohen, Thierry Coquand, Simon Huber, and Anders Mörtberg. “Cubical Type Theory: A Constructive Interpretation of the Univalence Axiom.” *21st International Conference on Types for Proofs and Programs (TYPES 2015)*, LIPIcs 69, 2018, pp. 5:1-5:34. DOI: [10.4230/LIPIcs.TYPES.2015.5](https://doi.org/10.4230/LIPIcs.TYPES.2015.5).

## 16. Review conclusion

The highest-confidence problem is not a new type-theoretic inconsistency. It is that effect-conditional export acceptance uses generic reachability, source-interface import drops the provider's verification state while consuming its classifier as authoritative, and the reviewed repository contains no effect-equation discharge transition. Existing strict-policy and relocation checks materially limit this problem and prevent this report from claiming a demonstrated unsound closed artifact.

The function-graph issue is narrower than first reported: the implementation follows the explicit higher-order exception, while the same plan retains unconditional first-order-style invariants. The identity-root issue is also narrower: helper validation and accepted proof replay already imply correct family arity and general well-typedness, while canonical-operator and certificate-specific checks remain absent at the root level. Correcting those exact contracts is actionable; describing either as a kernel exploit would exceed the evidence.
