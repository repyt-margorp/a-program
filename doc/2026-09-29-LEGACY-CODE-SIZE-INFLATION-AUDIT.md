# Legacy Code-Size Inflation and Failure-Mode Audit

Date: 2026-09-29; fresh re-audit: 2026-10-01
Status: measured retrospective and proposed anti-regression gates, not a new compiler bug
Code baseline: main at `97825ec28506f8a055db18af9e2577cba8cb1da7`, clean accepted source
Historical baseline: `old-version/2026-09-14-main` = `63b00eba3a3cf87b8aa20434b7c8e513a71be92b`
Related: [active Solver/Evidence plan](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md), [artifact plan](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md)

## Problem List

| ID | Problem | Fresh disposition |
| --- | --- | --- |
| R1 | The supplied historical line counts mix measurements and do not reproduce at the named frozen tag | Corrected with revision-pinned counts |
| R2 | Historical duplicate-authority failures need operational review gates | Supported by historical records; gates are proposals |
| R3 | Current accepted code and unpromoted ownership work must not be conflated | Both inspected; both ordinary check suites pass |
| R4 | Persistence/frontier tests must distinguish semantic correctness, byte stability and exact progress | Existing public partition gate still fails in the current prototype |

This section supersedes the current-state and quantitative claims in the
preserved input below. The original file remains unchanged in the Book user's
temp directory. Its SHA-256 is
`17853b3a010375c9a848ff7bd760c6e0cdec375f328072190145617e4b873b8d`.
The embedded copy preserves wording; Markdown space-based hard breaks are
normalized to explicit HTML breaks.
Its user attributions are historical reports, not independently established
new approvals. The current user request, translated from Japanese on
2026-10-01, is to re-audit two supplied documents, submit appropriate Issues,
and include both documents in one documentation-only PR.

## R1. Reproducible size accounting

### Subjective (User)

The supplied report asks what caused legacy code growth. The fresh request asks
for verification against the latest implementation, not adoption of every
number in the original report.

### Objective (Code)

Tracked source blobs were read from the two revisions, counting physical lines
with Python `bytes.splitlines()`. Comments and blank lines are included.
Generated outputs, documentation, examples, shell tests, build products,
`src/handmade/`, archived trees and experimental alternatives are not part of
the accepted-code count. Patch context lines are never counted as applied code.

| Revision / group | Files | Lines |
| --- | ---: | ---: |
| Frozen legacy `src/prototype/src/**/*.{c,h,inc}` | 111 | 165,328 |
| Frozen legacy `src/prototype/include/**/*.{c,h,inc}` | 61 | 10,979 |
| Frozen legacy implementation + headers | 172 | **176,307** |
| Frozen legacy `src/prototype/tests/**/*.{c,h,inc}` | separate group | 23,751 |
| Current accepted `src/*.{c,h,inc}` | 115 | **41,848** |
| Current `tests/**/*.{c,h,inc}` | 19 | 37,460 |

This is a 76.3% decrease in these implementation/header groups, about 4.21x,
not an identical-feature theorem. The current prototype overlays are excluded
from 41,848 because they have not been promoted. Counting all files below
`src/` would incorrectly include prototype patches and alternative designs.

Recursive quoted `.inc` includes were followed once per path, relative to
their parent file. Disjoint sampled components reproduce as follows:

| Frozen-tag component | Files | Lines |
| --- | ---: | ---: |
| frontend lowering translation unit and recursive includes | 13 | 37,998 |
| kernel judgement translation unit and recursive includes | 21 | 29,755 |
| checker session + container | 2 | 9,045 |
| artifact wire_v86 + link | 2 | 6,594 |
| kernel context + type_declaration | 2 | 4,757 |
| typed_occurrence_graph and recursive includes | 5 | 3,483 |
| frontend reader | 1 | 3,144 |
| Non-overlapping sampled subtotal | 46 | **94,776** |

The original 37,420 / 29,222 / 93,076 values do **not** reproduce at that tag.
Even the judgement wrapper has 31 lines there, not 29. These are corrections
to attribution/measurement, not evidence against the duplication hypothesis.

The 221,626 and 196,416 figures do appear in the preserved
[single-path plan](2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md).
They are historical working-state measurements, not the frozen-tag totals.
The fresh audit does not establish their original exact worktree or scope.
The referenced `2026-09-28-DEVELOPMENT-EPOCH-AUDIT.md` is absent at the
current baseline and is not used as fresh evidence.

Current hotspot counts are synthesis.c 5,595; synthesis.h 906; evidence.c
5,464; evidence.h 742; typing.c 1,101. These replace the unpinned figures in
the original current-state discussion.

A compact recipe for the main groups, run at repository root:

```python
import re, subprocess

def git(*args):
    return subprocess.check_output(["git", *args])

def census(revision, predicate):
    paths = git("ls-tree", "-r", "--name-only", revision).decode().splitlines()
    selected = [p for p in paths if predicate(p)]
    lines = sum(len(git("show", revision + ":" + p).splitlines())
                for p in selected)
    print(revision, len(selected), lines)

old = "63b00eba3a3cf87b8aa20434b7c8e513a71be92b"
new = "97825ec28506f8a055db18af9e2577cba8cb1da7"
census(old, lambda p:
       p.startswith(("src/prototype/src/", "src/prototype/include/"))
       and re.search(r"\.(c|h|inc)$", p))
census(new, lambda p: re.fullmatch(r"src/[^/]+\.(c|h|inc)", p))
```

### Assessment

Retain the causal explanation, correct its quantitative foundation, and keep
capability differences separate from duplication. LOC is a diagnostic, not
soundness, completeness, performance, or an architecture-admission threshold.

### Plan

- [x] Pin both code revisions and recount the same declared language of files.
- [x] Include nested implementation includes instead of only tiny wrappers.
- [ ] Add a maintained measurement/feature ledger to architecture review.
- Completion: a review can reproduce totals and distinguish implementation,
  tests, prototypes, generated code and deleted/restored capabilities.

## R2. From historical failure modes to enforceable gates

### Subjective (User)

The supplied document emphasizes avoiding renewed semantic-state multiplication.
The agent proposes operational gates; the user has not approved every proposed
gate as a mandatory repository constitution.

### Objective (Code)

The [failed refactor record](2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md)
records traversal-dependent SEQUENCE_PROVENANCE producer discovery and an
IH provenance change hidden by a convertible classifier. It explicitly says
the stopped snapshot was not certified passing.

The single-path plan documents duplicate classifier answers, Context refinement
routes, revision bridges and a discarded unused parallel equation graph.
These support the historical failure-mode register. They do not independently
prove every current field named state/cache/revision to be redundant.

Current Core, occurrence, Context/map, Evidence and source/runtime distinctions
remain necessary. A proof premise may deliberately select one of several valid
receipts: a repeated pointer is not automatically redundant data.

### Assessment

Adopt the audit as a review proposal, not an indiscriminate ban on state,
proof inversion, independent checkers, or caches. In particular:

- Proof inversion for a mathematical rule remains legitimate; recovering
  current lexical/program structure from arbitrary proof history is different.
- Multiple judgments over one erased Core are not duplicate answers.
- Checking the same conclusion under a different owner or exact premise
  selection is not automatically removable.
- A genuinely independent checker may be valuable under an explicit threat
  model; code reduction alone is not a reason to erase a trust boundary.
- Revisions and notifications are acceptable derived machinery when
  reconstructing/discarding them preserves semantics.

### Plan

- [ ] Require each new persistent/mutable structure to name its exact fact,
  authoritative owner, stable key, invalidation and deletion relationship.
- [ ] Extend existing chunk tests with deterministic supported ready-order
  variations and comparison of scoped typed conclusions/dependency topology.
- [ ] Inventory proof-history consumers and classify logical inversion versus
  structural reconstruction.
- [ ] Add artifact-field and backend-field ownership ledgers, with review of
  undocumented new categories rather than a blacklist of names.
- Completion: gates detect duplicated authority without merging distinct
  judgments, exact proof selections or necessary semantic theories.

## R3. Accepted implementation versus the latest ownership prototype

### Subjective (User)

The fresh audit should recognize improvements already implemented, rather than
asking development to repeat them.

### Objective (Code)

Accepted `src/synthesis_conversion.c` still wraps checked inputs through
`pg_synthesis_evidence` for Job-based normalization. The current alternative
under `src/prototype/solver_inputs/` removes that adapter factory and passes
checked receipts or actual pending producers directly.

The same unpromoted prototype consolidates classifier-query ownership, nominal
IADT recovery and family-construction outputs. It borrows actual result owners
and read-only structural queries, rather than copying progress into completed
wrappers. Its pending prefix has no
separate cursor/status/result. The active SE1-SE5 plan still marks the overall
frontier/ownership work unfinished.

Fresh verification on this baseline:

| Command / configuration | Result |
| --- | --- |
| `make -j2 check`, accepted checkout | pass |
| `solver_inputs/overlay.sh NEW_DIR`, clean accepted source + committed overlays | generation succeeds |
| `make -f NEW_DIR/src/Makefile BUILD=NEW_DIR/build -j2 check` | pass |
| artifact `check-artifact-semantic` with the overlay's adapted tests | pass |
| C prototype `check-c-backend check-c-scalar` on the same overlay | pass |

Default `check` is not the full `check-acceptance` target. No fresh full
acceptance or sanitized run is claimed. The candidate uses only committed
patches from this revision; no production source was edited.

### Assessment

Do not open a new replacement-solver implementation track. Reference the
existing SE1-SE5 plan and measure progress there. A passing prototype milestone
does not establish production promotion, a single dispatcher, exact frontier
resumption, or complete backend verification.

### Plan

- [x] Check both accepted and unpromoted implementations separately.
- [ ] Attach the retrospective review/measurement gates to the existing work
  list and preserve exact selection and nominal/scoped identity controls.
- Completion: future review states which owner was actually removed and
  distinguishes a verified prototype from accepted production behavior.

## R4. The public persistence/frontier boundary is still open

### Subjective (User)

The supplied retrospective warns against making disposable history semantic.
The agent additionally distinguishes that warning from the legitimate product
requirement to retain unfinished work under a declared checkpoint contract.

### Objective (Code)

On the current combined ownership/artifact candidate:

```sh
IMAGE_AUDIT_STRICT_BYTES=1 bash src/prototype/image_audit/partition_fuel.sh \
    NEW_DIR/build/pointer-check examples/09_list_induction.p \
    NEW_REPORT_DIR ordinary 100:100 1600:1600
```

The command exits 1. Selected rows:

| Partition / path | Used steps | Status | Image bytes |
| --- | ---: | --- | ---: |
| 100+100 / single, memory and save-without-reload | 200 | pending | 26,390 |
| 100+100 / reload | 200 | pending | 26,212 |
| 1600+1600 / single, memory and save-without-reload | 2,626 | done | 50,820 |
| 1600+1600 / reload | 3,200 | pending | 31,696 |

The earlier c9e0ce81 candidate used 2,617 steps for that continuous run.
Accepted source/test blobs are unchanged between it and 97825ec2; the
prototype was regenerated and all listed checks repeated after the update.

The in-memory paths agree. The first reload row is byte/size drift without a
terminal-status difference; the second is also a progress/status difference.
The prototype README already reports this gate as unfinished. This is fresh
confirmation of a known checkpoint limitation, not discovery of acceptance of
an invalid proof or a changed completed computation result.

### Assessment

Recomputation images are allowed to repeat work under their declared contract;
exact continuation claims need separate owner/frontier gates. Byte equality
alone neither establishes semantic equivalence nor authenticates accepted
evidence. Pure history minimization does not substitute for checkpoint correctness.

### Plan

- [x] Reproduce the public partition limitation without changing the language.
- [ ] Keep its repair in existing SE1-SE5/AP3 work; do not create a rival codec,
  replay engine or new Issue pretending the known failure is a new bug.
- [ ] Make any broader anti-regression gate report byte, status, fuel,
  structural semantics and acceptance separately.
- Completion: the specified persistence profile satisfies its actual contract;
  no checkpoint/reuse claim is inferred from normal round-trip acceptance.

## Submission scope

Tracking: [Issue #51](https://github.com/repyt-margorp/a-program/issues/51).

The corresponding Issue should track reproducible measurement and
anti-regression ownership gates, referencing the active implementation plan.
It must not reopen closed legacy Issues #17/#18 wholesale or treat all fifteen
historical failure modes as currently reproduced bugs.

The preserved input remains useful background. Its raw-file counts, current
status labels and missing-record references are superseded by R1-R4 above.

<details>
<summary>Preserved 2026-09-29 input — historical analysis and proposals, not fresh verification</summary>

# Legacy Code-Size Inflation and Failure-Mode Audit

Date: 2026-09-29<br>
Status: **deep retrospective audit / anti-regression design record**<br>
Scope: legacy A Program through the failed single-path refactor, compared with the current pointer implementation<br>
Related: `doc/2026-09-28-DEVELOPMENT-EPOCH-AUDIT.md`, `doc/2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md`, `doc/2026-09-16-TYPED-STRUCTURE-AND-EVIDENCE-REFACTOR-PLAN.md`, `doc/2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md`

## Problem List

| ID | Problem | Status |
| --- | --- | --- |
| P1 | Explain the legacy ~200k-line scale with measured evidence rather than intuition | **Verified** |
| P2 | Identify exactly which representations and state machines multiplied the implementation | **Verified** |
| P3 | Reconstruct concrete failures caused by duplicated authority, invalidation and ordering | **Verified** |
| P4 | Distinguish necessary type-theoretic complexity from accidental implementation complexity | **Verified** |
| P5 | Identify which legacy capabilities were genuinely removed rather than refactored | **Verified** |
| P6 | Audit the current pointer implementation for recurrence risk | **Audited; ongoing risk remains** |
| P7 | Define enforceable architecture gates so the same failure mode is not reintroduced | **Proposed as required policy** |

---

# Executive conclusion

The previous audit's phrase **semantic state multiplication** was correct, but too coarse to prevent recurrence.

The deeper failure was this:

> **One mathematical/semantic fact repeatedly acquired several physical identities, several mutable answer cells, several lifecycle states, and several routes by which consumers could rediscover it.**

The legacy compiler then needed a second body of code whose job was not to implement the language, but to keep those representations synchronized.

A simplified example is a typed occurrence `Γ ⊢ M : A`. In the legacy architecture, information participating in that one fact could be distributed among:

```text
source AST / binder identity
TermDB term ID
ContextDB context ID
SubstitutionDB morphism ID
TypedOccurrenceGraph occurrence ID
classifier constraint ID
classifier metavariable / solver answer
context-projection answer
provisional motive answer
JudgementDB proposition/candidate/accepted derivation
VerificationDB residual
publication / artifact record
relocated artifact ID
checked-container reconstruction
incremental snapshot / fingerprint
```

The theoretical distinctions among terms, contexts, substitutions, judgements and proofs are real and should remain. The defect was **giving too many derived views their own independently mutable authority and lifecycle**.

This distinction explains why the pointer rewrite can retain dependent typing, contexts, substitutions, evidence, indexed families, effects, Identity-related structure and incremental Solve while being dramatically smaller: it preserves semantic distinctions but rejects most *duplicate answer ownership*.

The strongest quantitative evidence is not the user's rough 200k estimate. A preserved 2026-08-31 architecture audit measured:

```text
Production C/header/include code            221,626 lines
Tests                                        48,062 lines
Old frontend lowering route                  41,622 lines
New typing_*.c route                          24,193 lines
New typing_*.h route                           5,188 lines
Boundary/state/topology/handoff modules      35,644 lines / 82 files
```

At an intermediate deletion checkpoint, production code was still 196,416 lines. The same audit recorded 1,590 production control conditions containing four or more logical operators, 358 containing eight or more, and 71 containing sixteen or more. It explicitly interpreted these as symptoms of branches simultaneously consulting facts owned by different authorities and lifecycles, not as mere formatting problems.

Source:

- `doc/2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md`, especially “Measured Failure” and “Control-Flow Rules”<br>
  https://github.com/repyt-margorp/a-program/blob/main/doc/2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md

The central anti-regression rule of this audit is therefore:

> **A new representation is acceptable only when it owns a genuinely new fact. A second representation of an existing fact must be immutable/derived/disposable, must name its authority explicitly, and must not acquire an independent completion or invalidation lifecycle.**

---

# P1. Quantitative reconstruction: where the lines actually were

### Subjective (User)

The legacy tree looked implausibly large relative to the current implementation. A superficial explanation such as “C is verbose”, “there were more tests”, or “there were many comments” would not explain a 6–7x difference.

### Objective (Code)

## 1.1 The archived project itself recorded the ~200k scale

The 2026-08-31 single-path architecture audit measured **221,626 production C/header/include lines** before the main deletion campaign and **196,416** at a deletion checkpoint. This is direct project evidence and supersedes the previous audit's reliance on the user's approximate headline figure.

The same record measured the old frontend lowering route at **41,622 lines**. That number includes a broader route than the one translation unit reconstructed below, so it should not be confused with the 37,420-line recursive include total.

## 1.2 `frontend/lowering.c` hid a 37,420-line implementation tree

The frozen legacy `frontend/lowering.c` is only seven physical lines. It says lowering remains one translation unit in order to preserve solver state and graph-ID allocation order, then includes five `.inc` modules.

Those modules recursively include seven further constraint-solver files.

Reconstructed source-line total:

| Lowering component | Lines |
| --- | ---: |
| `context_and_type_lowering.inc` | 5,916 |
| `function_graph_generation.inc` | 305 |
| `graph_construction.inc` | 10,679 |
| `finalization_and_entrypoints.inc` | 2,796 |
| constraint: model generation/index | 1,177 |
| constraint: effects/residuals | 1,710 |
| constraint: classifier/computation propagation | 2,215 |
| constraint: branch refinement/motives | 1,934 |
| constraint: motive solver | 4,815 |
| constraint: evidence/freeze | 4,356 |
| constraint: context/fixed point | 1,517 |
| **Recursive lowering implementation** | **37,420** |

This is an important audit correction. The first version of this report undercounted lowering at roughly 23k because it stopped at the first include layer.

Sources:

- frozen `frontend/lowering.c` and `frontend/lowering/**` under `old-version/2026-09-14-main`<br>
  https://github.com/repyt-margorp/a-program/tree/old-version/2026-09-14-main/src/prototype/src/frontend/lowering

## 1.3 `kernel/judgement.c` similarly hid roughly 29,222 lines

The legacy `kernel/judgement.c` is a 29-line wrapper. It includes judgement DB, conversion, classifier solver, candidate publication, rule families, candidate replay and accepted replay. Two of those include further nested rule files.

Direct recursive line-count reconstruction gives approximately:

```text
Judgement DB                         603
Conversion                          3,772
Classifier solver                   2,451
Candidate publication               2,281
Formation / elimination / CBPV      5,216+
Match rule family                   2,888
Identity/totality introduction      2,093
Dimension action                      641
Formation recording                   481
Candidate replay                    1,074
Accepted replay                     7,722
------------------------------------------
recursive Judgement implementation ~29,222 lines
```

The especially revealing entries are not just typing rules. `candidate_replay.inc` and `accepted_replay.inc` alone are about 8.8k lines. The architecture was paying separately for generating/provisionally storing evidence, publishing it, then replaying accepted evidence.

Sources:

- frozen `kernel/judgement.c` and included files<br>
  https://github.com/repyt-margorp/a-program/tree/old-version/2026-09-14-main/src/prototype/src/kernel

## 1.4 A conservative non-overlapping sampled subtotal already exceeds 93k

Using direct raw-source counts from disjoint legacy paths:

| Area | Verified physical lines |
| --- | ---: |
| recursive frontend lowering | 37,420 |
| recursive Judgement kernel | 29,222 |
| independent checker (`session.c` + `container.c`) | 8,887 |
| artifact wire + linker | 6,485 |
| ContextDB + type declarations | 4,623 |
| TypedOccurrenceGraph storage/validation/verification/runtime | 3,394 |
| frontend reader | 3,045 |
| **Sampled subtotal** | **93,076** |

This subtotal deliberately excludes many substantial areas: TermDB implementation, substitutions, dimensions/HOTT, other artifact/publication code, normalization/cache machinery, producer/capsule code, incremental fingerprinting, support/storage, parser/AST, headers, and tests.

Therefore the code-size explanation does not depend on a few anomalously large files. Large implementation mass existed independently in **lowering**, **kernel evidence**, **independent checking**, **persistence/linking**, and **graph validation/runtime**.

### Assessment

A local refactor of helper functions could not plausibly remove ~170k lines. The successful architectural direction had to eliminate whole representation/lifecycle boundaries.

### Plan

- [x] Replace the approximate legacy-size claim with the preserved 221,626-line measurement.
- [x] Recursively expand `.inc` implementation trees instead of counting wrapper `.c` files.
- [x] Record a non-overlapping 93,076-line sampled subtotal and keep it separate from the full-tree historical measurement.
- [ ] Re-run an identical local `cloc`/`wc` recipe on the frozen tag and current tree when a repository checkout is available; record exact SHAs.

Completion criterion: the report explains the scale using reproducible source groups without treating the user-provided ~200k/~30k figures as exact measurements.

---

# P2. The precise multiplication mechanisms

### Subjective (User)

User concern, 2026-09-29, paraphrased: a coarse statement such as “there were too many stores” is insufficient; the failure mechanisms must be identified finely enough that the project can recognize them before repeating the same path.

### Objective (Code)

The following failure modes are independently evidenced by legacy source or the preserved architecture audits.

## FM-1. One answer existed in multiple mutable locations

The single-path audit records that, during cleanup, a base classifier constraint solution was chosen as the sole mutable classifier answer and the old occurrence classifier, stale-classifier, evidence and state fields were deleted as mirrors. It also deleted a full-scan solution digest/revision bridge.

That means those mirrors were not hypothetical design smells: they were real fields that had to be removed.

A particularly strong legacy example was Context refinement. The audit found **three mutable mechanisms representing one refinement**:

1. candidate-to-materialized Context relocation plus dirty-binding revisions;
2. Context projection solutions with their own readiness;
3. final mutation/rebasing of occurrence, constraint, proposition and proof Context roots.

A local attempt to synchronize the copies merely moved the failure into proof publication and was rolled back.

### Failure mechanism

```text
Fact F changes
 -> copy F1 changes
 -> revision increments
 -> dependent cache C invalidated
 -> copy F2 may still contain old answer
 -> publication P may already have copied F2
 -> consumer sees combination impossible in the abstract semantics
```

The bug is not “forgot to invalidate a cache”. The bug is that several mutable cells claim to answer the same question.

### Prevention

For every mutable semantic answer, require a documented tuple:

```text
Fact:
Owner:
Stable identity/key:
Single mutable answer cell:
Allowed immutable references:
Derived caches:
Invalidation source:
```

If two rows name the same `Fact`, architecture review fails until one is proven to be a disposable cache rather than another authority.

---

## FM-2. Parallel stores created a synchronization program beside the language implementation

The legacy `compile_context` simultaneously connected TermDB, type declarations, JudgementDB, a judgement delta, computation results, compile metadata, constraints, classifier solver, effect solver, revision counters, dirty bindings, reification caches, proof-materialization work, binder/context data and many pending work arrays.

The constraint DB then needed an explicit validator checking:

- contiguous domain ranges;
- counts and first-index offsets;
- valid state enums;
- source occurrence IDs;
- Context IDs;
- parent/origin edges;
- domain-specific relationships;
- correspondence to typed occurrences and other stores.

That validator is evidence of **cross-store consistency becoming its own subsystem**.

### Failure mechanism

The more independently stored indexes and arrays there are, the more code is required for:

```text
creation
cross-linking
range validation
transaction rollback
freezing/sealing
revision tracking
serialization
relocation
reconstruction
negative testing
```

Parallel arrays additionally require manual proofs that `arrayA[i]`, `arrayB[i]` and range partition `i ∈ [first_X, first_Y)` still refer to the intended same semantic object.

### Prevention

Prefer one row/object/graph node keyed by stable semantic identity over parallel arrays split by lifecycle. If SoA layout is needed for performance, treat it as an implementation detail behind one constructor and one invariant checker, not several public authorities.

---

## FM-3. Construction order leaked into semantic topology

This is the failure that stopped the in-place refactor on 2026-09-07.

The archived failure record states that List induction failed while constructing an equation because a `SEQUENCE_PROVENANCE` operand was found by searching for an IH equation whose producer might only be constructed later. Therefore equation topology depended on traversal order.

Immediately before that, another repair found that motive/provenance could change meaningfully while the classifier remained convertible to the old classifier; consumers therefore were not woken if wake-up compared only classifier answers.

### Failure mechanism

Two categories were conflated:

```text
immutable semantic dependency topology
mutable discovery/progress/provenance
```

If graph edge existence is discovered by searching mutable current state, schedule order becomes semantics.

### Prevention

1. Immutable dependency edges must be interned from their actual semantic inputs.
2. A pending producer gets a stable identity before it has an answer.
3. Consumers subscribe to that identity rather than search globally for a currently available answer.
4. Scheduler permutation must not change final graph topology, accepted conclusions or exported identity.

**Required regression test:** run the same semantic construction under multiple deterministic queue orders/chunk sizes and compare canonical structure and accepted conclusions. This is stronger than merely checking the same final printed value.

---

## FM-4. “Answer changed” was incorrectly reduced to “classifier changed”

The failed-refactor record explicitly notes a case where provenance/motive changed while the classifier remained convertible. Consumers did not wake.

This reveals an under-specified answer type. A solver result was semantically more than its classifier, but the invalidation key observed only the classifier.

### Prevention

Define the complete observable answer per consumer. If a consumer needs:

```text
classifier + evidence + substitution/map + provenance/source identity
```

then either:

- the dependency points to an immutable object containing those exact inputs, or
- each component is a separate explicit dependency.

Do not hide needed dependency dimensions inside “metadata” and then hope classifier revision captures them.

---

## FM-5. Proof history became a structural API

The pointer-era 2026-09-16 audit found that many consumers recovered operands, scopes, maps and formation structure by walking Evidence/derivation history. `function_graph.c`, `action.c` and several evidence helpers reconstructed program structure through projection/reindex/conversion wrappers.

This was recognized as wrong ownership even after the large legacy rewrite had already happened.

The corrected split is:

```text
typed subject / classifier / semantic operands  -> typed occurrence
why that conclusion is accepted                 -> Evidence / derivation
```

### Why this inflates code

If consumers cannot ask the structural owner directly, every consumer writes its own inverse interpreter over proof rules:

```text
while proof is conversion/reindex/projection/...:
    peel wrapper
    update context/map
    recover previous operand
```

Different consumers then disagree on which wrappers are transparent and which historical operands count as current structure.

### Prevention

**No proof-to-structure inversion rule:** a production consumer may not discover current program operands, lexical binders, or Context maps by walking an arbitrary derivation if the fact has a structural owner.

Proof inversion remains legitimate when the *mathematical operation itself* is inversion of a typing judgement. The distinction must be documented at the call site.

---

## FM-6. Evidence had both candidate and accepted lifecycles, followed by replay

The Judgement subsystem contains separate candidate publication, candidate replay and accepted replay machinery. The 2026-08-31 target architecture explicitly called for candidate and accepted forms to share one immutable premise representation and for accepted replay not to rebuild a second candidate-shaped premise array.

### Failure mechanism

The architecture paid for the same derivation in several phases:

```text
candidate proposition / candidate premises
 -> publication selection
 -> accepted derivation / accepted premises
 -> replay validation
 -> artifact representation
```

Each transition introduced maps, IDs, validation, serialization and error states.

### Prevention

A derivation should have stable immutable premise identity. Acceptance may change *status/authority* or select roots, but it should not require rebuilding the semantic payload in a second shape.

---

## FM-7. Independent checked containers reimplemented semantic checking

The legacy `checker/session.c` is about 7.4k lines. It owns independent structural readers for terms, contexts and substitutions and implements checks such as type-family spine equality, binder-aware equality, local type checking, indexed motive checking, etc.

Separately, `checker/container.c` implements another wire format and reconstructs a large elaborated semantic module containing terms, contexts, substitutions, universes, schemas and other records.

This was a real security/trust feature, not meaningless duplication. However, it explains substantial LOC because A Program effectively contained another semantic consumer/checker of the full internal model.

### Prevention

Before adding an independent checker or trusted container, require an explicit threat model:

```text
What producer is untrusted?
What smaller language/schema is independently checked?
Which existing kernel function cannot be reused and why?
How is semantic drift detected?
What percentage of the main semantics must be duplicated?
```

If the independent format serializes nearly every internal compiler structure, expect near-linear code duplication and long-term drift cost.

---

## FM-8. Persistence promoted derived progress into compatibility surface

Legacy artifacts serialized extensive compiler semantic state and had separate accepted-proof and checked-semantic-container roles. Linkers relocated many dense ID spaces.

The later single-path plan concluded that ready queues, normalization caches, historical cursors and indexes should remain optional derived data and not become Authority. The current pointer README similarly states that ordinary Solve is used on load and stored completion claims do not confer authority.

### Failure mechanism

Every persisted derived field becomes:

```text
schema field
versioning obligation
reader logic
writer logic
validation logic
relocation logic
backward-compatibility decision
corruption/forgery test
invalidation contract
```

Persisting a cache therefore has much larger architectural cost than keeping the same cache in memory.

### Prevention

Default persistence policy:

1. persist immutable semantic/source identity needed to reconstruct the program;
2. persist accepted evidence only when there is a measured need;
3. recompute disposable solver/scheduler/cache state through ordinary Solve;
4. checkpoint mutable progress only after the canonical state graph is proven unique and replay-valid.

---

## FM-9. Dense IDs made module linking pay the database cost again

When terms, binders, Contexts, substitutions, occurrences and proofs have separate dense local IDs, linking requires a relocation map for each identity family plus cross-record validation.

The old linker and wire implementation therefore repeated a large fraction of the internal representation complexity at module boundaries.

### Prevention

Persistent/linkable identities should be limited to identities with actual cross-module semantics. Derived local work IDs should not escape the owning computation.

A backend/linker should ideally consume a **thin symbol/export graph plus semantic references**, not a serialized snapshot of every compiler-local index space.

---

## FM-10. Feature generalization fanned out across every layer

The development-epoch audit records several cases where one layer had already generalized while another retained an older cardinality or semantic assumption. Plural effect handlers are a clear example: AST, typed graph, solver, runtime, proof construction, traversal, relocation and artifacts all had to agree.

### Failure mechanism

With `N` first-class representations of one feature, changing the feature from singular to plural is roughly an `N`-site semantic migration. If compatibility paths keep both forms alive, the migration temporarily becomes `2N` plus adapters.

### Prevention: feature fan-out budget

Every new semantic feature or cardinality change must list the owner modules expected to change **before implementation**. If a concept requires coordinated semantic state changes across parser, Core, typed graph, private solver, proof DB, VerificationDB, runtime and artifact schema, stop and ask whether some of those layers are mirrors rather than owners.

A high touch-count is an architecture signal, not proof that the feature is inherently complicated.

---

## FM-11. Private solvers accumulated for Match, effects, motives, Context projection and other facets

The old architecture progressively acquired classifier metas, motive solutions, branch refinement, effect-row metas, computation constraints, usage/totality/universe state and Context projection state.

The 2026-08-31 target plan explicitly prohibited adding classifier-, effect-, Match-, carrier-, usage- or universe-specific storage modules to the replacement equation graph. The desired distinction was different *judgement/rule kinds* sharing one dependency/scheduling lifecycle, not different semantic theories collapsed into one rule.

### Prevention

Distinct logic rules may and should remain distinct. What should be shared is generic lifecycle infrastructure:

```text
stable key
immutable operands
answer state
explicit dependencies
ready queue
SCC/fixed-point mechanism
residual/reject outcome
```

A new private solver must justify why the common work/equation mechanism cannot represent its lifecycle.

---

## FM-12. Context identity was mutated by answers that should have been dependencies

The legacy cleanup found Context classifier alternatives, candidate/materialized Context relocation, dirty-binding lists, Context revision rescans and mutable root rebasing.

The target architecture instead proposed that a Context extension refer to the classifier **equation identity**, so the Context itself remains stable as that equation moves from pending to solved.

This is a general pattern:

> If an object's identity changes when a dependency's answer changes, the dependency has probably been copied into identity instead of referenced.

### Prevention

Use stable references to pending facts in structural keys. Do not materialize a second structural object merely because a referenced answer becomes more precise.

---

## FM-13. Revision counters compensated for missing dependency edges

The legacy compiler had solution revisions, context-dependency revisions, dirty-binding sets and fallback “all dirty” behavior. These mechanisms are understandable in mutable systems, but their proliferation indicates that semantic dependencies were not fully represented as explicit edges.

### Prevention

A revision counter is acceptable for a derived cache. It is suspicious when changing the revision is required to make semantic correctness hold.

**Review question:** if the revision counter were removed and all caches recomputed eagerly, would semantics remain correct? If no, the counter is functioning as hidden semantic dependency state and must be replaced by an explicit dependency.

---

## FM-14. New architecture was repeatedly added beside old architecture before deletion

The single-path audit records a first R1 attempt that added a new `kernel/typing/equation_graph` next to the production typing constraint state. It had no production caller and represented the same equations a second time. The project deleted those 846 implementation/test lines and changed course to simplifying the production database in place.

This is an unusually clear micro-example of how the 200k problem happened.

### Prevention

A migration may temporarily dual-run for equivalence testing, but it requires:

```text
old owner:
new owner:
dual-run start:
comparison gate:
mandatory deletion point:
maximum temporary LOC:
```

No new subsystem is “implemented” merely because its model is cleaner if production still uses the old owner.

---

## FM-15. Compatibility could preserve a bug as an architectural requirement

The pointer rewrite re-audited at least one legacy indexed-induction positive fixture and found that the legacy checker had accepted an inconsistent projected constructor index. The expected result was corrected instead of preserving compatibility.

### Prevention

Compatibility tests must be classified as:

- syntax compatibility;
- intentionally preserved semantics;
- historical behavior requiring semantic revalidation.

“Legacy accepted it” is evidence of behavior, not proof of correctness.

### Assessment

The 15 failure modes are not independent style issues. They are different manifestations of the same ownership defect: a derived view acquires identity, mutability, lifecycle, persistence or publication semantics until it behaves as another authority. The most dangerous fixes are therefore synchronization fixes. They make a duplicated architecture more internally consistent without reducing the number of facts that can disagree.

### Plan

- [x] Record each failure mode with a concrete legacy mechanism and a prevention rule.
- [x] Include construction-order dependence, Context duplication, proof-history inversion, candidate/accepted replay, persistence and migration overlap rather than limiting the audit to DB count.
- [ ] Use this failure-mode register in future architecture reviews; any new `state/solution/revision/snapshot/replay` owner should be checked against FM-1 through FM-14 before implementation.

Completion criterion: a future reviewer can map a proposed design directly to a named historical failure mode and identify the required alternative.

---

# P3. Root-cause graph

### Subjective (User)

The audit should explain not only which mistakes existed, but how they reinforced one another until local fixes produced more state and more code.

### Objective (Code)

The failures above are not independent. They form a causal chain.

```text
Semantic distinction is introduced
        |
        v
A dedicated stored representation is added
        |
        +--> separate ID space
        +--> separate state enum / phase flags
        +--> separate mutable answer
        +--> separate validation
        |
        v
Other layers copy/project the answer
        |
        +--> revision counters
        +--> dirty sets
        +--> wake-up bridges
        +--> handoff structs
        +--> candidate/accepted forms
        |
        v
Persistence serializes the representations
        |
        +--> relocation maps
        +--> replay/checking
        +--> versioning
        |
        v
Feature changes require N-way synchronized migrations
        |
        v
Partial migrations retain old + new authorities
        |
        v
Control flow begins testing combinations of states
        |
        v
Order-dependence / stale publication / wrong Context bugs
        |
        v
More flags, caches, retries and validators are added
        |
        +---------------------------+
                                    |
                                    v
                         semantic state multiplication
```

The loop is self-reinforcing. Once several authorities exist, each bug encourages another synchronization mechanism. The important intervention point is **before adding the second authority**, not after the invalidation bugs appear.

### Assessment

The causal loop explains why line count accelerated: every additional authority generates adapters, validators, invalidation and persistence obligations, and those mechanisms then create new observable state that itself needs lifecycle management. The appropriate control point is representation admission, not later cleanup.

### Plan

- [x] Preserve the causal model in this audit.
- [ ] For future regressions, record the earliest point in this graph where the duplicated authority entered rather than only documenting the final stale-cache symptom.

Completion criterion: bug retrospectives identify the first unnecessary authority/lifecycle, not merely the last incorrect synchronization branch.

---

# P4. What was necessary complexity and what was accidental?

### Subjective (User)

The smaller implementation must not be justified by deleting distinctions that are required for dependent typing, exact binder identity, proof checking or runtime semantics.

### Objective (Code)

Historical and current designs both retain separate Core, typed structure, Context/substitution, Evidence, source and runtime concepts. What changed most is whether derived answers are copied into independently mutable stores.

### Assessment

The correct lesson is not “make everything one graph” or “never store Contexts/Evidence”. Several old distinctions are mathematically or operationally necessary.

| Concern | Necessary? | Correct owner direction |
| --- | --- | --- |
| erased computation identity | yes | Core graph |
| typed use of shared Core | yes | typed occurrence / typed subject |
| lexical and binder identity | yes | Context/binder/source allocation |
| substitutions / context action | yes | Context map / morphism machinery |
| accepted typing reason | yes | Evidence / kernel derivation |
| pending computational work | yes | synthesis/query scheduler |
| runtime host resource identity | yes | runtime, not Core |
| source spelling and visibility | yes | syntax/source environment |
| artifact symbol/export identity | yes | persistent image/link layer |
| second mutable classifier answer | **no** | reference the owner |
| proof history as current operand structure | **no** | typed structural owner |
| candidate Context and materialized Context as competing identity | **no** | stable Context + dependencies |
| private lifecycle engine per judgement family | usually **no** | common scheduling/dependency mechanism |
| persisted ready queues / traversal order | **no by default** | recompute |
| independent copy of premise DAG for accepted replay | **no** | same immutable derivation structure |
| arbitrary legacy positive behavior | **no** | semantic re-audit |

The rule is therefore not minimization of data structures. It is **uniqueness of authority and lifecycle for each fact**.

### Plan

- [x] Keep necessary semantic distinctions explicit.
- [x] Reject “one giant graph/record” as the lesson of the audit.
- [ ] Require every simplification proposal to state which semantic distinctions remain and which duplicated lifecycle is actually being removed.

Completion criterion: code reduction never relies on conflating computation identity, typed identity, source identity, proof acceptance or runtime identity.

---

# P5. Genuine legacy capabilities that account for real extra LOC

### Subjective (User)

The comparison should not mislabel real artifact, linking and independent-checking functionality as accidental bloat.

### Objective (Code)

The current system should not claim a pure 221k -> 30k refactor with identical infrastructure.

The old implementation genuinely contained capabilities or stronger infrastructure contracts that are absent, deliberately simplified, or not yet restored in the same form:

- accepted proof/publication artifact formats;
- a separate checked semantic container;
- dense linker relocation across multiple semantic ID spaces;
- extensive independent replay/checking paths;
- richer incremental fingerprints/snapshots;
- publication/sealing state;
- more elaborate artifact compatibility/version machinery.

Those capabilities explain part of the size difference. They do **not** explain the duplication failures, because the legacy project's own cleanup records show multiple copied classifier answers, multiple Context-refinement mechanisms, unused parallel equation graphs, candidate/accepted premise duplication, revision bridges and mixed-authority branches.

The audit should therefore use two categories:

```text
A. feature/infrastructure scope difference
B. accidental architectural multiplication
```

Do not count A as “waste”; do not use A to excuse B.

### Assessment

Some of the size reduction is a product-scope change, but the project's own 2026-08-31 cleanup proves that scope difference is not a sufficient explanation: duplicate classifier fields, Context-refinement mechanisms, revision bridges, parallel equation graphs and candidate/accepted copies were being deleted even before the independent pointer rewrite.

### Plan

- [x] Maintain separate ledgers for removed capability and removed duplication.
- [ ] When a legacy capability is reintroduced, design it against the current owner model rather than porting its old storage topology.

Completion criterion: future size comparisons can say whether a delta is feature scope, representation duplication, or both.

---

# P6. Current pointer implementation: recurrence-risk audit

### Subjective (User)

The purpose of the retrospective is to protect the current compiler, especially while new backends, artifacts and linking capabilities are being added.

### Objective (Code)

The current architecture contains several explicit defenses against the old failure modes.

## 6.1 Strong defenses already visible in current code

Current `synthesis.c` states or implements the following contracts:

- evidence inputs are immutable checked references, while a pending producer remains its own obligation;
- registration is a derived dependency rather than a second registration state/intern key;
- source allocation/address discovery does not have to wait for proof acceptance;
- accepted typed data is the structural authority, while pending recipes exist only before that structure is available;
- application rule selection explicitly warns that queue order must not decide whether an unaccepted family is computational.

The current 2026-09-17 authority contract also states “one owner for each fact, not one record for all facts”, assigning separate ownership to Core, Context/binders, typed occurrences, Context maps, Evidence, source inputs, synthesis progress and persistent relocation.

These rules directly target the predecessor's failures rather than merely producing smaller code.

## 6.2 The current system still has hotspots

Current raw source counts include roughly:

```text
src/synthesis.c    5,404 lines
src/synthesis.h      902 lines
src/evidence.c     5,223 lines
src/evidence.h       736 lines
src/typing.c       1,041 lines
```

Large files are not inherently defects. However, `synthesis.h` exposes a broad set of APIs for source allocations, environments, retained inputs, reconstruction and persistent/source identity. This is exactly the area where the old system repeatedly created new metadata authorities.

Therefore **source identity + pending synthesis + persistence** is the highest recurrence-risk zone.

## 6.3 Evidence remains another high-risk zone

The 2026-09-16 refactor explicitly identified that shrinking Evidence alone is insufficient if consumers continue to reconstruct program structure by walking proof history. Any new helper that peels conversion/reindex/projection derivations to discover present structure should be reviewed against the “proof-to-structure inversion” rule.

## 6.4 Reintroducing richer artifacts is the largest future risk

C, CUDA C, Verilog lowering and richer linkable artifact design are now natural next steps. That is also the exact point where the old architecture could return:

```text
Program
 -> backend-lowering snapshot DB
 -> backend constraint DB
 -> proof/publication DB
 -> link snapshot
 -> checked backend container
```

The correct default is for backends to consume a **view/projection of the existing accepted Program**. Backend-specific structures should own genuinely backend-specific facts (layout, ABI, scheduling, resource placement), not copies of typing/proof/source answers.

### Assessment

The pointer architecture has explicit defenses that directly encode lessons from the failed predecessor. That is encouraging, but current `synthesis` and `evidence` remain large, semantically central owner modules, and source/image reconstruction is precisely where duplicate identity was nearly reintroduced during the pointer epoch. The risk is therefore lower, not zero.

### Plan

- [ ] Treat source/persistence changes and backend artifacts as mandatory authority reviews.
- [ ] Track recursive Evidence walks and synthesis state growth during post-promotion work.
- [ ] Add scheduler-order and backend-no-copy tests before the next large persistence/backend expansion.

Completion criterion: new backends/persistence features add only owner-specific facts and do not create alternate typing, Context, source or proof authorities.

---

# P7. Required anti-regression architecture gates

### Subjective (User)

The historical lesson must be operational: the project needs review gates that reveal the old pattern while a proposed change is still small.

### Objective (Code)

The following should be treated as a design-review checklist, not optional style advice.

## Gate A — New persistent structure admission

Before adding any DB/table/index/metadata registry that survives more than one local operation, answer all of:

```text
1. What exact mathematical/semantic fact does it own?
2. Which existing structure currently owns or can derive that fact?
3. Why is recomputation/reference insufficient?
4. Is the new data authoritative or a cache?
5. If cache: can deleting it change accepted semantics? It must not.
6. What is its stable key?
7. What mutates after construction?
8. What event invalidates it?
9. Does it have its own pending/solved/rejected lifecycle?
10. If yes, why is that lifecycle not an existing synthesis/equation job?
11. Must it be serialized?
12. If serialized, what compatibility obligation is being accepted?
13. Which old representation is deleted in the same vertical slice?
```

Any unanswered item blocks implementation.

## Gate B — One-fact/one-answer test

For every mutable semantic answer, grep/audit all fields that can contain it.

Pass condition:

```text
one authoritative answer cell
+ zero or more immutable references
+ disposable derived caches
```

Fail condition:

```text
answer
stale_answer
published_answer
projection_answer
replayed_answer
cached_answer that correctness depends on
```

Naming a copy “cache”, “projection” or “metadata” does not make it derived.

## Gate C — Queue-order invariance

For every incremental solver/synthesis feature:

- run with at least two chunk sizes;
- run with deterministic reversed/permuted ready ordering where possible;
- compare canonical structural identity and accepted conclusions;
- ensure only work count/timing may differ.

A different immutable dependency graph under a different schedule is a blocker.

## Gate D — Authority DAG review

Draw arrows among owners. The dependency of authority should be acyclic at the representation level even when the *object-language equations* contain recursive SCCs.

Good:

```text
source input -> typed structural request -> accepted Evidence
                    |
                    -> derived backend view
```

Bad:

```text
Evidence -> reconstruct structure -> mutate Context -> invalidate Evidence
```

Object-language recursion is not permission for implementation-authority cycles.

## Gate E — No proof-history structural lookup

Every call that recursively walks derivations must be classified:

```text
logical inversion/checking       -> allowed
current structure recovery       -> migrate to typed structural owner
source/binder identity recovery  -> forbidden
scheduler/progress recovery      -> forbidden
```

## Gate F — Feature fan-out review

Before adding a language feature, list every production subsystem requiring semantic changes.

If the list crosses more than a few owner boundaries, explicitly determine whether the feature is genuinely cross-cutting or whether internal representations are mirrored.

Do not use a fixed numeric threshold as a correctness theorem, but use a rising touch-count as a stop signal.

## Gate G — Migration deletion budget

Every replacement project must specify a deletion target. A clean parallel implementation with no production cutover is not progress toward simplification.

Recommended policy derived from the failed single-path experience:

- temporary duplicate implementation is permitted only behind equivalence tests;
- no third authority may be added while two coexist;
- a vertical slice is not complete until the superseded representation is deleted;
- report production `+/-/net` separately from tests/docs.

## Gate H — Persistence minimality

For every serialized field classify it as:

```text
semantic identity
accepted evidence
reconstructible derived state
runtime-only state
```

Default rules:

- semantic identity: serialize;
- accepted evidence: serialize only with explicit product need;
- reconstructible state: recompute by default;
- runtime-only state: never place in a Program image.

## Gate I — Independent checker threat-model gate

No second checker/container path without documenting:

- trust boundary;
- deliberately smaller checked schema;
- shared vs duplicated rules;
- differential tests against primary semantics;
- maintenance cost estimate.

## Gate J — Revision-counter test

For every semantic revision counter ask:

> If every derived cache were discarded and recomputed after each mutation, would correctness still hold?

If no, the revision is carrying hidden semantic dependency information and must become an explicit edge/reference.

## Gate K — Context stability test

Solving a classifier/effect/proof obligation should not require rewriting every Context/occurrence/proof root that references it. Pending semantic dependencies should have stable identity.

## Gate L — Compatibility is not proof

Every imported legacy positive fixture must be capable of being reclassified as an old bug after semantic analysis. Compatibility gates report divergence; they do not dictate acceptance.

### Assessment

These gates deliberately focus on authority, identity and lifecycle rather than naming conventions. A field named `cache` can still be an authority if correctness depends on its stored answer; a large explicit rule evaluator can be legitimate if it owns a genuinely distinct logical rule.

### Plan

- [ ] Adopt Gates A-L as the review checklist for new persistent state, synthesis jobs, artifacts and backend IRs.
- [ ] Require an authority/deletion note in substantial implementation plans.
- [ ] Add the proposed scheduler-permutation, Evidence-consumer and persistence-classification audits.
- [ ] Re-run this retrospective after artifact/backend infrastructure materially grows.

Completion criterion: a future design cannot add another long-lived semantic representation without explicitly naming its unique fact, authority, lifecycle, persistence contract and deletion relationship to existing structures.

---

# Concrete “stop immediately” signals

The following patterns should trigger an architecture audit before more code is added:

1. A proposed fix adds another `*_state`, `*_solution`, `*_snapshot`, `*_metadata`, `*_revision`, `*_dirty`, `*_published`, or `*_replay` field for a fact that already has an owner.
2. A correctness fix consists mainly of “wake this consumer too”.
3. A newly solved answer requires scanning unrelated objects to discover dependents.
4. An immutable graph edge can appear or disappear depending on queue order.
5. A Context or typed structural object is replaced because a referenced answer became more precise.
6. An API needs both a source occurrence ID and a different ID that is supposed to denote the same typed occurrence.
7. A serializer starts storing ready queues, traversal cursors, revision counters or cache contents.
8. A backend needs a copy of classifier/evidence/context data rather than a reference/projection.
9. A proof is traversed to discover the current operand of a program node.
10. A migration adds a new route while the old route remains the production authority without a deletion deadline.
11. A test compares dozens of fields/flags to assert one conceptual phase.
12. A wrapper `.c` looks tiny but preserves one translation unit specifically because global state/allocation order matters; inspect all recursive includes and hidden state coupling.

These are not automatic bugs. They are the patterns that preceded the legacy blow-up and therefore require explicit justification.

---

# Proposed architecture constitution for A Program

The retrospective evidence supports a compact set of durable rules.

## C1. Semantic distinction does not imply storage distinction

Contexts, substitutions, evidence and effects are different concepts. They do not each require an independent mutable database with a private lifecycle.

## C2. One fact has one owner

“One authority” does not mean one giant record. It means each fact has one canonical owner and all other layers refer to or derive from it.

## C3. Structure precedes acceptance, but acceptance does not define structure

Pending structural objects may exist before proof acceptance. Evidence certifies them; it does not become their identity.

## C4. Pending is an ordinary semantic state, not a reason to fabricate provisional structure

Use stable pending references/jobs. Do not create provisional Term/Core/Context objects merely to have something concrete to store.

## C5. Scheduling is operational, not semantic

Queue order, chunk size, traversal order, cache hit order and discovery order may affect cost, never semantic topology or identity.

## C6. Persistence stores meaning, not compiler history

A Program image should reconstruct the same semantic requests and identities. It does not need to preserve the incidental route used by the previous process to discover them.

## C7. Backends own backend facts only

C/CUDA/Verilog backends may own ABI/layout/scheduling/resource facts. They must not become alternative authorities for source typing, Context identity or proof acceptance.

## C8. Migration means deletion

A replacement representation is not complete until the old authority and its synchronization bridges are gone.

---

# Recommended immediate tests to add before the next architecture growth

1. **Scheduler permutation acceptance test**<br>
   Run representative dependent Match/IH/effect cases under multiple deterministic work orders. Compare structural identities and accepted conclusions.

2. **Authority inventory generated report**<br>
   Produce a build-time/static-audit table of mutable structs/fields whose names include `state`, `solution`, `revision`, `dirty`, `pending`, `accepted`, `published`, `cache`, `snapshot`. Human-review ownership; do not mechanically reject them.

3. **Persistence field classification test**<br>
   Every image section should be documented as semantic identity, accepted evidence, or reconstructible state. Reject undocumented new persistent categories in review.

4. **Backend no-copy audit**<br>
   As C/CUDA/Verilog lowering grows, record whether each backend IR field is backend-owned or copied from Program authority. Copied semantic fields need justification.

5. **Evidence structural-consumer audit**<br>
   Track all production functions that recursively inspect Evidence. Classify each as logical inversion versus structure recovery.

6. **LOC/fan-out ledger per feature**<br>
   For each substantial feature, record production files touched and net LOC. A sudden increase in cross-owner touch count is an early warning long before the tree reaches 200k again.

7. **No-hidden-include LOC audit**<br>
   Count `.c`, `.h`, and `.inc`, and recursively attribute included implementation files to their translation-unit owner. The old 7-line `lowering.c` / 37k implementation tree must not happen invisibly in future size reviews.

---

# Historical timeline of the failure

```text
Early architecture
  one Core asked to carry too much semantic identity
        |
        v
July CwF / typed-occurrence separation
  correct semantic distinctions introduced
        |
        v
Implementation represents distinctions as many physical stores
        |
        v
August features add private state and cross-layer adapters
        |
        v
221,626 production lines measured on 2026-08-31
        |
        v
single-path cleanup tries to unify state in place
        |
        +-- removes duplicate classifier fields / revision bridges
        +-- discovers three Context-refinement mechanisms
        +-- rejects unused second equation graph
        +-- identifies thousands of mixed-state conditions
        |
        v
163-file refactor (+44,089/-28,255) still encounters
construction-order-dependent equation topology
        |
        v
2026-09-07 in-place refactor stopped
        |
        v
independent pointer rewrite
        |
        +-- tiny exact-pointer Core
        +-- explicit typed structure above Core
        +-- ordinary pending synthesis jobs
        +-- Evidence separated from structural ownership
        +-- source identity separated from proof identity
        +-- one Solve path for images
        |
        v
2026-09-16/17 rewrite itself catches early recurrence:
proposed new refinement metadata/job is rejected before becoming another authority
```

The last step is especially important. The pointer rewrite did **not** magically eliminate the tendency to create duplicate authority. It survived because the project detected and reversed a new `refinement` metadata / `REFINED_SCOPE_JOB` proposal, explicitly insisting that existing Context maps, typed subjects and source allocations be audited first.

That is the behavior this document should institutionalize.

---

# Final assessment

The legacy 200k-line system was not primarily a story of bad coding or too many logical features. It was a story of **correct distinctions implemented with too many independently mutable representations**.

The most dangerous misconception would be:

> “The rewrite is small because the new data structures are cleverer.”

The stronger conclusion is:

> **The rewrite is small because fewer things are allowed to become authorities.**

The legacy implementation often represented:

```text
structure
answer
provisional answer
published answer
replayed answer
projection of answer
cache of answer
revision saying answer changed
```

as separately managed state. Once that happens, the program must implement and test the synchronization semantics among those states. That synchronization semantics is where tens of thousands of lines accumulated and where order-dependent correctness failures appeared.

The design objective going forward should therefore not be “keep the repository under N lines”. It should be:

```text
For every semantic fact:
  exactly one owner,
  stable identity before completion where needed,
  explicit dependency edges,
  no schedule-dependent topology,
  no proof-history structural reconstruction,
  no persistence of disposable compiler history,
  and deletion of superseded authorities during migration.
```

If these invariants are maintained, code size should remain a consequence of the semantic surface. If they are violated, LOC growth is an early symptom, not the root problem.

---

# Evidence index

Primary project records used for this revision:

1. Failed single-path refactor record<br>
   https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md

2. Single-path architecture / measured failure audit<br>
   https://github.com/repyt-margorp/a-program/blob/main/doc/2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md

3. Typed Structure and Evidence refactor<br>
   https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-16-TYPED-STRUCTURE-AND-EVIDENCE-REFACTOR-PLAN.md

4. Typed Data Authority reaudit<br>
   https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md

5. Legacy architecture README<br>
   https://github.com/repyt-margorp/a-program/blob/main/doc/2026-09-14-LEGACY-TOP-LEVEL-README.md

6. Frozen legacy source<br>
   https://github.com/repyt-margorp/a-program/tree/old-version/2026-09-14-main/src/prototype

7. Current pointer source<br>
   https://github.com/repyt-margorp/a-program/tree/main/src

## Measurement note

The direct source subtotals in this document were reconstructed from raw files, following nested `.inc` includes instead of counting only wrapper `.c` files. They are intentionally **subtotals**, not a replacement for the project's own complete 221,626-line measurement. The historical project measurement is the authoritative full-tree figure for 2026-08-31; the reconstructed 93,076-line subtotal demonstrates where a large fraction of the mass physically lived and how it was partitioned.

## Audit limitation

The current implementation's “~30k” figure remains a working headline rather than a full repository-local `cloc` result produced in this environment. Current raw-file counts were used only for identified hotspots. A final local checkout audit should count production `.c/.h/.inc` consistently for both revisions and keep tests/docs/generated outputs separate.

</details>
