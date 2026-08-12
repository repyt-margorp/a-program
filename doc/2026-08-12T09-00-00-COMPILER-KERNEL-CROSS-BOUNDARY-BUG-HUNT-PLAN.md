# Compiler and Kernel Cross-Boundary Bug Hunt Plan

Date: 2026-08-12

Status: planned; no finding in this document is a confirmed bug until it passes
the reproduction gate in section 6.

Baseline:

```text
branch: main
commit: 41e22dd617883325fd623a280d6470c7ea82c6c7
        (Add permanent issue boundary audit coverage)
artifact: v72
HOTT fragment schema: v5
```

## 1. Purpose

GitHub Issues #3 through #10 exposed several bugs that were not isolated local
mistakes. They belonged to recurring bug families:

- an erased Core Term was used where a typed Operation occurrence was required;
- a structurally valid candidate ID was consumed as if it carried a certificate;
- principal, admissible, and expected classifiers were conflated;
- dependent Match carrier constraints and effect-row constraints were solved as
  one equality;
- typed IH ownership was recovered from a colliding erased binding;
- generation, proof replay, artifact publication, and artifact readback did not
  always enforce the same invariant;
- examples and documentation described behavior without an executable owner.

The purpose of this plan is to search the entire current prototype for other
instances of those bug families and for adjacent C implementation defects. The
result must be more than a list of suspicious lines. Every accepted finding must
have:

1. a stated invariant;
2. a minimal reproducer or a mechanical checker;
3. a demonstrated incorrect result, rejection, acceptance, mutation, crash, or
   artifact inconsistency;
4. a root-cause owner;
5. a permanent positive and negative boundary test; and
6. a check that the same invariant survives artifact publication and replay when
   the affected object is persistent.

This is a bug hunt, not a request to redesign every unfinished theory. Missing
HOTT, dependent CBPV, IADT, resource, or effect functionality is a bug only when
the implementation claims to support it, accepts an invalid program, rejects a
program inside the documented fragment, or serializes unverifiable evidence.

## 2. Authority and Related Documents

The current code and schemas are authoritative:

- `src/prototype/src/`
- `src/prototype/include/a_program/`
- `src/prototype/spec/artifact_v72.schema`
- `src/prototype/spec/hott_fragment_v5.schema`
- `src/prototype/tests/`

The following documents provide hypotheses and historical decisions, not a
substitute for current-code verification:

- `2026-08-11T20-04-50-GITHUB-ISSUES-3-10-AUDIT-AND-IMPLEMENTATION-PLAN.md`
- `2026-08-11T08-19-24-CURRENT-COMPILER-FUNCTIONAL-AND-THEORY-REVIEW.md`
- `2026-08-08T22-51-03-HIGHER-OBSERVATIONAL-TT-REFACTOR-AUDIT-V3.md`
- `2026-08-11T08-10-57-PROTOTYPE-SEMANTICS-PRESERVING-CODEBASE-ORGANIZATION-PLAN.md`

Known design findings F1 through F18 remain inputs. This plan must not report
them again as new bugs without a new executable counterexample.

## 3. Progress Legend

| Mark | Meaning |
| --- | --- |
| `[ ]` | not started |
| `[~]` | audit in progress or hypothesis not yet reproduced |
| `[x]` | completed with recorded evidence |
| `[!]` | invariant or stop condition that must be preserved |
| `[?]` | requires a language/theory decision before implementation |

Each work package has separate `inventory`, `reproduction`, `fix`, and
`regression` states. Finding suspicious code completes none of those states.

## 4. Non-Goals

- Do not collapse TermDB, OperationGraph, JudgementDB, ContextDB, or ArtifactDB
  merely to reduce line count.
- Do not turn every unsupported theoretical case into an implementation task.
- Do not use expected types to silently change synthesis policy.
- Do not make every structural substitution globally proof-carrying.
- Do not merge distinct kernel proof rules because their C plumbing is similar.
- Do not add compatibility readers or remapping layers to reduce migration work.
- Do not mutate accepted implementation outside `src/prototype/`.
- Do not change wire format until a confirmed persistent invariant requires it.
- Do not characterize an inconclusive fuel exhaustion as logical disproof.

## 5. Bug Model and Invariants

### 5.1 Identity ownership

The same erased `TermId` may occur in multiple typed positions. Therefore:

```text
Term identity       owns computational structure
Operation identity  owns typed occurrence facts
Binding identity    owns binder occurrence and scope
Claim identity      owns an accepted proposition
Derivation identity owns one proof route and its premise edges
```

[!] No classifier, IH field, effect row, source ascription, resource usage, or
proof premise may be recovered from `TermId` alone when two Operations can share
that Term.

### 5.2 Candidate versus certificate

Structural existence is not accepted evidence:

```text
candidate graph node != certified capability != accepted persistent root
```

[!] Every consumer that requires proof must take a certified reference or verify
exact Claim coverage. A raw numeric ID is not evidence.

### 5.3 Synthesis, checking, and inspection

```text
principal classifier  selected by synthesis
expected classifier   consumed by checking after synthesis
admissible view       separately derived classifier evidence
inspection projection read-only report of stored occurrence facts
```

[!] `::`, constructor domains, APP domains, Match motives, and `:type` must not
silently rewrite the principal classifier.

### 5.4 Constraint authority

Carrier/type equations, effect-row equations, universe constraints, resource
constraints, and residual obligations are different constraint classes.

[!] A solver may coordinate them, but success in one class must not erase or
manufacture evidence in another.

### 5.5 Persistent symmetry

For every persisted semantic object:

```text
producer closure
  -> wire validation
  -> relocation/link
  -> semantic replay
  -> republished closure
```

must preserve the same identity and proof invariants.

[!] Range checks are insufficient for sparse tables. Every reference must point
to a present entry of the expected section and semantic kind.

### 5.6 Unsupported versus false

An unsupported, residual, or fuel-exhausted case is not an uninhabited type and
is not a rejected theorem.

[!] Diagnostics and artifacts must preserve this distinction.

## 6. Finding Lifecycle and Evidence Gate

Every hypothesis receives a stable ID `BH-<AREA>-NNN` and one status:

```text
hypothesis -> reproduced -> confirmed -> fixed -> regression-owned -> closed
                    \-> disproved
                    \-> theory-decision-required
```

A finding is `confirmed` only if all applicable questions have answers:

- What documented or kernel invariant is violated?
- What is the smallest source program, C checker, or corrupted artifact that
  demonstrates it?
- Is the observed behavior deterministic?
- Is it a false acceptance, false rejection, wrong result, mutation, crash,
  leak, nondeterminism, or persistence mismatch?
- Does it reproduce in a clean build at the recorded commit?
- Does an adjacent negative case prove that the test is boundary-sensitive?
- Which layer owns the correction?
- Could the apparent bug instead be unsupported syntax or an undecided theory
  rule?

Confirmed findings are recorded in a future machine-readable file:

```text
src/prototype/tests/audit/bug_hunt_findings.tsv
```

Required fields:

```text
finding_id severity invariant owner reproducer positive_test negative_test artifact_test status commit
```

The audit manifest itself receives an integration test. Closed findings may not
lose their runner, fixture, marker, or artifact evidence unnoticed.

## 7. Baseline Freeze (BH0)

### BH0.1 Reproducible baseline

- [ ] Record full commit hash, compiler identity, target, and active intrinsic
  environment.
- [ ] Run `make -f src/prototype/Makefile clean reader`.
- [ ] Run every `src/prototype/tests/integration/test_*.sh` through
  `make -f src/prototype/Makefile test-integration`.
- [ ] Record test names, elapsed time, and exit status without relying only on
  aggregate success.
- [ ] Run `git diff --check` and require a clean worktree before each hunt slice.

### BH0.2 Diagnostic build profiles

- [ ] Add prototype-only build recipes for `-O0 -g3`.
- [ ] Add AddressSanitizer and UndefinedBehaviorSanitizer profiles where the host
  compiler supports them.
- [ ] Add a strict warning profile including conversion, shadowing, missing
  prototypes, and format checks after auditing intentional exceptions.
- [ ] Keep sanitizer binaries and generated outputs outside source control.
- [ ] Do not weaken warnings globally to accommodate one module.

### BH0.3 Inventory snapshot

- [ ] Snapshot source/header/schema/test LOC by file.
- [ ] Inventory every public constructor, lookup, verifier, replay, and readback
  entry point.
- [ ] Inventory all ID types and invalid sentinels.
- [ ] Inventory all caches and their key fields.
- [ ] Inventory all authoritative versus derived fields in Operation and
  artifact records.

Deliverable: `BH0-BASELINE.md` or an appended execution section in this file.

## 8. Static Cross-Boundary Audit (BH1)

### BH1.1 ID domain audit

Search all conversions and assignments among:

- `TermId`;
- Operation IDs and occurrence indices;
- `BindingId`;
- Context and Substitution IDs;
- Proposition, Claim, Derivation, and premise IDs;
- constructor representation, ordinal, field, and owner Match IDs;
- universe variables and constraints;
- effect-operation atoms and labels;
- artifact local, dense, sparse, relocation, and export IDs.

For each boundary:

- [ ] Verify the C type does not permit an accidental cross-domain assignment
  without an explicit conversion function.
- [ ] Verify `UINT32_MAX`, zero, and count values cannot be confused.
- [ ] Verify `< count` is paired with `present` and semantic-kind checks for
  sparse storage.
- [ ] Verify relocation never accepts a source-section ID as a destination ID.
- [ ] Verify diagnostic IDs cannot re-enter semantic APIs.

### BH1.2 Lookup ambiguity audit

- [ ] Find APIs returning `NULL`, `false`, zero, or an invalid ID for more than
  one reason.
- [ ] Separate `not found`, `unsupported`, `invalid`, `out of fuel`, and
  allocation failure where callers make different semantic decisions.
- [ ] Find “first matching proof/classifier/binding” scans.
- [ ] Test insertion-order reversal and duplicated derivations.
- [ ] Reject lookups that recover typed ownership through shared erased Core.

### BH1.3 Allocation and arithmetic audit

Prioritize artifact publication, wire readback, context/substitution growth,
proof premise arrays, Match case arrays, and Term application spines.

- [ ] Check `count * sizeof(T)` and cumulative section-size overflow.
- [ ] Check grow-before-write and failure atomicity around `realloc`.
- [ ] Check zero-length sections and one-past-the-end arithmetic.
- [ ] Check every partial-construction error path frees only owned memory.
- [ ] Check aliasing before `memcpy` and use `memmove` where overlap is legal.
- [ ] Check recursive visitors for cycle assumptions and depth exhaustion.

### BH1.4 Constness and mutation audit

- [ ] Trace every inspection, conversion, validation, replay, and readback API
  that receives a mutable database pointer.
- [ ] Snapshot counts/revisions before and after read-only operations.
- [ ] Distinguish intentional normalizer graph extension from accidental
  metadata/proof mutation.
- [ ] Verify failed checks do not leave accepted Claims, cache entries, labels,
  or artifact roots behind.

Deliverable: an inventory table. Suspicious code remains a hypothesis until BH2
or BH3 reproduces it.

## 9. Typed Occurrence and Shared-Core Hunt (BH2)

This work package generalizes Issues #4, #5, #9, and #10.

### BH2.1 Shared computational nodes

Generate pairs and triples of Operations that intentionally share Core Terms:

- identity Lambdas at `Bool`, `Nat`, Host Int, and dependent Pi types;
- identical constructor payload Terms under distinct nominal type views;
- repeated variables under different binder owners;
- identical Match branch bodies with different motives/effects;
- shared IH Core bindings owned by different cases or recursive fields;
- the same host literal under different occurrence metadata.

For each case:

- [ ] Principal classifiers remain occurrence-local.
- [ ] Expected classifiers remain post-synthesis checks.
- [ ] Ascriptions and source spans remain occurrence-local.
- [ ] Effect rows and resource usage remain occurrence-local.
- [ ] `:type` is read-only and reports the selected occurrence.
- [ ] Artifact export retains distinct typed identities while sharing allowed
  Core structure.

### BH2.2 Metamorphic identity tests

The following transformations should preserve accepted behavior unless the
surface language intentionally exposes the distinction:

- alpha-renaming binders;
- interning versus independently constructing the same Core Term;
- reversing unrelated declaration order;
- reversing Derivation insertion order;
- adding an unused well-typed definition;
- compiling directly versus importing an artifact;
- splitting one artifact provider into two providers with the same exports.

[ ] Add a table specifying exactly which IDs may differ and which semantic keys,
Claims, outputs, and diagnostics must remain equal.

## 10. Constraint Generation and Solver Hunt (BH3)

### BH3.1 Constraint completeness

- [ ] For every Operation kind, enumerate generated carrier, effect, universe,
  resource, and residual constraints.
- [ ] Verify every generated constraint is either solved, explicitly residual,
  or rejected with a stable diagnostic.
- [ ] Verify no pending constraint disappears when a neighboring constraint is
  solved.
- [ ] Verify no proof Claim is published before all rule-required constraints
  have accepted evidence.

### BH3.2 Match and induction matrix

Cross these dimensions rather than testing one showcase program:

```text
motive: constant / dependent / Host-result / computation-result
branches: pure / effectful / mixed rows
recursion: none / one field / multiple fields / nested outer IH
core: unique / deliberately shared
result: closed / universe variable / residual
```

- [ ] Every supported cell receives a positive fixture.
- [ ] Every forbidden cell receives a stable negative fixture.
- [ ] Unsupported cells remain residual or diagnostic, never accepted by
  fallback.
- [ ] Branch permutation does not change the solved least effect row.
- [ ] Field permutation changes only declared field identity, not IH ownership
  by accidental ordinal collision.

### BH3.3 Principal/checking boundary

- [ ] Repeat literal tests for top-level, APP argument, constructor field, Match
  branch, Lambda body, nested computation block, and artifact import.
- [ ] Test default and alternate intrinsic environments.
- [ ] Ensure an invalid ascription cannot feed synthesis backward.
- [ ] Ensure admissible secondary Claims do not replace the principal.
- [ ] Ensure inspection reports both principal and explicit expectation where
  present without synthesizing new evidence.

### BH3.4 Fixed-point stability

- [ ] Run solver worklists under reversed and randomized insertion order.
- [ ] Detect duplicate work that changes accepted Claim or proof multiplicity.
- [ ] Verify monotonicity: adding valid premises cannot invalidate an already
  accepted derivation unless the rule explicitly carries a linear resource.
- [ ] Verify fuel exhaustion returns a residual result and cannot be cached as
  rejection.

## 11. Context, Substitution, and Proof DAG Hunt (BH4)

### BH4.1 Context and binding identity

- [ ] Audit extension, weakening, projection, composition, and reindexing for
  exact source/target Contexts.
- [ ] Construct alpha-equivalent contexts with distinct Binding IDs.
- [ ] Construct shared Terms under distinct Contexts and reject cross-scope
  evidence reuse.
- [ ] Verify direct binding references survive Context interning and artifact
  relocation.
- [ ] Verify no De Bruijn-like positional assumption has reappeared in helper
  scans.

### BH4.2 Candidate/certificate consumers

Enumerate every SubstitutionDB consumer and classify it:

```text
structural-only
classifier-coherent
certified CwF use
persistent accepted use
```

- [ ] Structural consumers accept valid candidates without fabricated proof.
- [ ] Certified consumers reject absent, mismatched, or forged Claim coverage.
- [ ] Artifact roots include all reachable certificate premises.
- [ ] Readback validates exact propositions, not merely Claim ID range.

### BH4.3 Derivation DAG integrity

- [ ] Multiple Derivations may conclude one Claim without overwrite.
- [ ] Premise order and multiplicity are preserved where the rule requires them.
- [ ] Cyclic premise graphs are rejected.
- [ ] A Derivation cannot cite a Claim from an incompatible Context.
- [ ] Removing one alternative Derivation does not remove the Claim while
  another accepted route remains.
- [ ] Replay does not infer proof identity from reconstruction order.

### BH4.4 Resource-sensitive readiness

- [ ] Weakening and projection evidence identify the same Context morphism where
  semantically connected, without collapsing their distinct proof roles.
- [ ] Usage zero/one/many survives substitution and branch joins.
- [ ] Shared graph nodes do not accidentally count as one source occurrence.
- [ ] One-shot resumptions are not validated solely by Core occurrence count.

## 12. Evaluation, Conversion, and Cache Hunt (BH5)

### BH5.1 Reduction-profile matrix

Build explicit profiles from supported reduction dimensions:

- beta;
- constructor iota/Match;
- recursive IH unfolding;
- transparent external references;
- intrinsic pure evaluation;
- CBPV return/force/fold rules;
- WHNF versus NF.

- [ ] Cache keys include every semantic profile dimension.
- [ ] Cache keys include environment/transparency identity where required.
- [ ] Graph revision invalidates only entries whose assumptions changed.
- [ ] A weaker profile result is never reused as a stronger profile result.
- [ ] Effectful or unresolved requests never enter pure conversion.

### BH5.2 Differential and metamorphic evaluation

- [ ] Compare cached and cache-disabled evaluation.
- [ ] Compare WHNF followed by NF with direct NF.
- [ ] Compare direct source evaluation with artifact-linked evaluation.
- [ ] Compare transparent and opaque exports.
- [ ] Compare shared and unshared graph construction.
- [ ] Verify normalization order does not alter confluent pure results.

### BH5.3 Conversion result discipline

- [ ] Audit every caller for structured `equal`, `not equal`, `residual`, and
  `error` handling.
- [ ] Ensure unsupported rules do not fall through to unequal.
- [ ] Ensure residual results are not stored in union-find-like equivalence
  state.
- [ ] Ensure observational/object Identity witnesses are never globally promoted
  to definitional equality.

## 13. CBPV and Effect Hunt (BH6)

### BH6.1 Value/computation boundaries

- [ ] Enumerate APP, computation sequencing/fold, Return, Thunk, Force,
  operation request, handler clause, and computation-block lowering.
- [ ] Verify APP consumes the documented Pi/value boundary.
- [ ] Verify computation fold consumes a computation and continuation with the
  correct dependent classifier.
- [ ] Verify implicit surface coercions are unique and recorded, not guessed by
  whichever solver runs first.
- [ ] Verify pure computations are not silently treated as values where explicit
  Thunk is semantically required.

### BH6.2 Effects and handlers

- [ ] Operation identity is stable across aliases, imports, and artifact link.
- [ ] Effect union is associative, commutative, idempotent, and independent of
  insertion order at the semantic level.
- [ ] Handler subtraction removes exactly handled atoms.
- [ ] Forwarded operations preserve request argument, continuation, latent
  computation, and resumption multiplicity.
- [ ] Return and all selected operation clauses target one compatible carrier.
- [ ] Zero-, one-, and multiple-clause computation folds are either typed and
  replayable or rejected consistently at all layers.

### BH6.3 Resumption and higher-order operations

- [ ] Cross one-shot/multi-shot/abortive modes with branch alternatives, Thunk
  capture, nested handlers, and shared subgraphs.
- [ ] Force-once and force-twice tests distinguish latent computations.
- [ ] Unsupported higher-order forwarding is explicit; it must not masquerade
  as first-order success.

## 14. Artifact and Linker Adversarial Hunt (BH7)

### BH7.1 Producer/consumer invariant table

For every v72 section, record:

- producer and closure marker;
- local identity domain;
- dense/sparse representation;
- required presence and kind checks;
- relocation owner;
- semantic validator;
- replay consumer;
- republish behavior.

### BH7.2 Systematic corruption tests

Starting from valid minimal artifacts, mutate one field at a time:

- [ ] reference equals count;
- [ ] reference points into a sparse hole;
- [ ] reference points to the wrong section/kind;
- [ ] duplicated export key;
- [ ] missing reachable premise;
- [ ] forged Claim/Proposition pairing;
- [ ] cyclic Derivation premise;
- [ ] wrong Context or Substitution source/target;
- [ ] wrong constructor owner/case/field;
- [ ] intrinsic environment fingerprint mismatch;
- [ ] effect atom or universe variable collision across providers;
- [ ] overflowed count/offset/size and truncated section;
- [ ] valid checksum/shape with invalid semantic payload, where applicable.

Every malformed artifact must be rejected before semantic objects become
accepted state, with no partial publication.

### BH7.3 Round-trip and composition tests

- [ ] source -> artifact -> read -> republish preserves semantic keys and roots;
- [ ] provider split/merge preserves imports and renumbers local domains safely;
- [ ] link order does not change accepted semantics;
- [ ] transparent/opaque boundaries survive every round trip;
- [ ] shared Core plus distinct typed exports remains distinct after relocation;
- [ ] alternate intrinsic environments cannot be linked accidentally.

## 15. Identity, Parametricity, and HOTT Fragment Hunt (BH8)

This package audits implemented claims only. It does not declare full Higher
Observational Type Theory complete.

### BH8.1 Relation versus object Identity

- [ ] Every public/internal API names whether it constructs a parametric relation
  action or an object Identity witness.
- [ ] A relation-preservation certificate cannot be consumed as object Identity
  merely because their endpoint Terms coincide.
- [ ] Non-DefEq but extensionally equal closed Bool functions retain an explicit
  object Identity witness.
- [ ] Distinguishable Bool functions do not acquire one through diagonal or
  fallback rules.

### BH8.2 Higher evidence

- [ ] Witness-of-witness examples preserve distinct proof Terms and Derivations.
- [ ] Derivation insertion order does not collapse higher evidence.
- [ ] Unsupported Universe/dependent/higher cases remain residual.
- [ ] Artifact roots include exact relation/Identity distinction and all premise
  closure.

### BH8.3 Rule correspondence

- [ ] Map every implemented formation/introduction/action rule to
  `hott_fragment_v5.schema`.
- [ ] Find code paths with no normative rule and schema rules with no executable
  owner.
- [ ] Treat a mismatch as a theory decision unless one side clearly claims
  authority and the other violates it.

## 16. Frontend, Surface, and Driver Hunt (BH9)

### BH9.1 Parser and grouping

- [ ] Generate nested Lambda, Match, computation block, ascription, import, and
  handler combinations around every precedence boundary.
- [ ] Pair accepted parenthesized forms with rejected ambiguous forms.
- [ ] Ensure a syntax diagnostic is not later reported as a typing failure.
- [ ] Ensure source aliases resolve to semantic identity without string-based
  fallback.

### BH9.2 Lowering conservation

- [ ] Every AST node produces exactly the intended Operation occurrence and Core
  sharing behavior.
- [ ] Source spans, names, explicit expectations, and execution demand attach to
  the Operation, not globally to the shared Term.
- [ ] Failed lowering is failure-atomic.
- [ ] Definition blocks and top-level definitions obey their distinct binding
  semantics.

### BH9.3 Driver consistency

- [ ] REPL and file compiler use the same compiler-session initialization,
  intrinsic environment, reduction defaults, and diagnostics.
- [ ] `:type`, `:whnf`, and `:nf` remain inspection/evaluation commands and do
  not mutate accepted proof state unexpectedly.
- [ ] All documented examples are manifest-owned executable specifications.

## 17. Automated Input Generation (BH10)

### BH10.1 Parser fuzzing

- [ ] Build a bounded grammar generator from current surface productions.
- [ ] Seed it with all positive and stable-negative fixtures.
- [ ] Run under ASan/UBSan with deterministic seeds and strict timeouts.
- [ ] Minimize crashes and assertion failures before filing findings.

### BH10.2 Artifact fuzzing

- [ ] Add structure-aware v72 mutation rather than relying only on random bytes.
- [ ] Preserve enough framing to reach each section validator.
- [ ] Assert clean rejection, no leak, no partial accepted state, and bounded
  resource use.
- [ ] Keep minimized corrupt artifacts as generated fixtures or compact mutator
  recipes, not opaque large binaries.

### BH10.3 Small-model semantic enumeration

Enumerate bounded terms over Bool, Nat depth 0-3, simple Pi, Return/Thunk/Force,
and one/two effect atoms.

- [ ] Compare alpha variants.
- [ ] Compare cached/uncached reduction.
- [ ] Compare direct/artifact execution.
- [ ] Check effect-row laws.
- [ ] Check that accepted Claims replay.
- [ ] Check deterministic diagnostics for rejected terms.

The generator is an audit tool, not an oracle. Semantic mismatches require manual
reduction to a stated invariant.

## 18. Execution Order

| Order | Package | Reason |
| ---: | --- | --- |
| 0 | BH0 baseline and diagnostic profiles | Makes later failures reproducible. |
| 1 | BH1 static boundary inventory | Produces targeted hypotheses without changing semantics. |
| 2 | BH2 typed occurrence/shared Core | Highest recurrence from Issues #4/#5/#9/#10. |
| 3 | BH3 constraint/solver | Highest false-accept/reject risk after identity ownership. |
| 4 | BH4 Context/proof DAG | Audits certificate and persistent premise authority. |
| 5 | BH5 evaluation/cache | Requires stable typed and proof identities as oracle inputs. |
| 6 | BH6 CBPV/effects | Builds on corrected constraints and conversion results. |
| 7 | BH7 artifact/link | Adversarially checks all prior persistent invariants. |
| 8 | BH8 HOTT fragment | Audits exact implemented fragment after proof persistence. |
| 9 | BH9 frontend/driver | Cross-checks surface ownership and user diagnostics. |
| 10 | BH10 generated testing | Broadens search after deterministic oracles exist. |

BH2 through BH6 may produce artifact tests immediately; BH7 is the systematic
closure, not permission to defer obvious persistence regressions.

## 19. Progress Dashboard

| Package | Inventory | Reproduction | Fixes | Permanent regression | Status |
| --- | --- | --- | --- | --- | --- |
| BH0 baseline | [ ] | [ ] | N/A | [ ] | not started |
| BH1 C/ID/static boundaries | [ ] | [ ] | [ ] | [ ] | not started |
| BH2 typed occurrence/shared Core | [ ] | [ ] | [ ] | [ ] | not started |
| BH3 constraints/solver | [ ] | [ ] | [ ] | [ ] | not started |
| BH4 Context/proof DAG | [ ] | [ ] | [ ] | [ ] | not started |
| BH5 evaluation/conversion/cache | [ ] | [ ] | [ ] | [ ] | not started |
| BH6 CBPV/effects | [ ] | [ ] | [ ] | [ ] | not started |
| BH7 artifact/link | [ ] | [ ] | [ ] | [ ] | not started |
| BH8 Identity/parametricity/HOTT | [ ] | [ ] | [ ] | [ ] | not started |
| BH9 frontend/driver | [ ] | [ ] | [ ] | [ ] | not started |
| BH10 generated testing | [ ] | [ ] | [ ] | [ ] | not started |

## 20. Per-Finding Implementation Discipline

For each confirmed bug:

1. Commit the minimal failing test or retain it visibly failing in the same
   reviewable change when the repository policy requires green commits.
2. Put the fix in the authoritative owner; do not add a remap/fallback at the
   later consumer merely to pass the fixture.
3. Update all proof materialization, verifier, artifact, and replay paths that
   carry the changed invariant.
4. Add a positive boundary test, an adjacent negative test, and an artifact test
   when persistent.
5. Add the finding to `bug_hunt_findings.tsv` with a stable marker in the
   authoritative runner.
6. Run the focused test, sanitizer profile where applicable, full integration,
   `git diff --check`, and a clean-worktree check.
7. Record whether the fix changes artifact schema. If yes, create one deliberate
   version transition without compatibility fallback unless explicitly approved.

No issue is closed because a showcase example passes. The root invariant and
its nearest counterexample must both be permanently owned.

## 21. Reporting Format

Each audit slice reports findings first, ordered by severity:

```text
BH-AREA-NNN: short title
severity:
status:
invariant:
reproducer:
observed:
expected:
root owner:
cross-layer impact:
fix direction:
permanent tests:
artifact impact:
commit:
```

Severity is based on consequence:

- `critical`: invalid proof/artifact accepted, memory corruption, or unsound
  conversion;
- `high`: wrong typed identity, false acceptance/rejection of supported code,
  proof loss, or persistent semantic mismatch;
- `medium`: deterministic wrong evaluation/inspection, cache contamination,
  misleading unsupported handling, or recoverable malformed-input crash;
- `low`: diagnostic/source ownership defects that do not change accepted
  semantics.

Disproved hypotheses are retained briefly with the decisive test so the same
non-bug is not repeatedly rediscovered.

## 22. Completion Gates

The comprehensive hunt is complete only when:

- [ ] every package BH0-BH10 has a completed inventory and recorded result;
- [ ] every confirmed bug has stable machine-readable ownership;
- [ ] every persistent invariant has producer, wire, linker, replay, and
  republish coverage;
- [ ] all sanitizer and strict-warning runs have no unexplained failures;
- [ ] generated-test seeds and limits are reproducible;
- [ ] full integration passes from a clean build;
- [ ] no audit tool mutates accepted state merely by inspecting it;
- [ ] unsupported theory remains explicitly distinguished from falsehood;
- [ ] a final report separates confirmed bugs, fixed bugs, remaining design
  decisions, unsupported features, and disproved hypotheses;
- [ ] file-by-file and total added/deleted LOC are reported for implementation,
  tests, generated audit tools, and documentation separately.

The expected result is not “no suspicious code.” It is that the supported
compiler fragment has explicit cross-layer invariants, adversarial tests for its
known failure families, and a durable process for turning future reports into
minimal permanent boundary tests.
