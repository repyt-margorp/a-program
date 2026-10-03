# Problem List

## 1. Publication and current-head qualification of the supplied audit

### Subjective (User)

2026-10-03, English paraphrase: the user supplied a new Markdown audit under
the book workspace's sibling `temp/`, regards its concern as important to the
current development structure, and requests an Issue and PR. This authorizes
documentation publication, not implementation changes, test deletion, relaxed
expectations, automatic adoption of thresholds, or launching a new worker.

Source: `/home/repyt/workspace/temp/2026-10-03-TEST-SUITE-GROWTH-AND-PROGRESS-AUDIT.md`.
The author-supplied body below is preserved verbatim, including its proposals.
Its attributed user statements are supplied provenance, not independently
reconstructed transcripts of the other development sessions.

### Objective (Code)

Freshly fetched inspection revision:
`37de1e8ca0d163e4d139ac70caafa19fae2142f9`, 2026-10-03 16:05 JST.

This submission performs **static inspection only**. It does not run a broad
suite, measure gate runtime, construct the complete invocation inventory,
demonstrate semantic redundancy, or freshly reproduce recorded worker failures.

Current findings:

| Concern | Inspected evidence and qualification |
| --- | --- |
| Broad suite composition | `src/Makefile:80` assembles acceptance from broad and specialized prerequisites; later rules add more. Examples recur in source admission, syntax round-trip, parsing and result checks. These are distinct candidate contracts, not established redundant tests. |
| Sorting/image lifecycle repetition | `quick_result.sh`, `generic_sorted.sh`, `retained_quicksort.sh`, `local_strong_sorted.sh`, `image_cli.sh` and `compatibility.sh` contain related save/load/pending/retained flows and differing assertions. Repeated mechanics warrant mapping before consolidation. |
| Legacy boundary | `compatibility.sh` deliberately enables `--legacy-intrinsic-dot`, uses archived fixtures and also checks current image/result semantics. `compatibility.tsv` retains `review_pending`. |
| Existing inventories | `tests/inventory.sh` generates a frozen historical capability inventory; `syntax_inventory.sh` explicitly checks syntax, not semantic acceptance. Typed-owner inventories also exist. No unified effective-invocation/property/lifecycle/cost ledger was identified in these inspected entrypoints; no tracked `src/prototype/test_audit/` exists at this revision. |
| Harness failures | The coordination plan records omitted inputs, incorrect header/worker assembly, report-byte normalization and migration-column setup errors. They are repository-reported operational failures, not compiler regressions freshly reproduced here. |
| Actual progress reporting | The latest coordination plan already has a Current Delivery Status table and issue-linked worker feedback requirements. Extend these rather than claim there is no reporting or create a second integration authority. |

Snapshot drift from the supplied document:

- Accepted semantic baseline remains `eb0aad6`. A path-limited diff from that
  baseline to this inspection revision is empty for accepted `src/` excluding
  `src/prototype/`, accepted `tests/`, and README.
- Reviewed Prototype Main checkpoint is now `7b27c4c`, not the supplied
  document's `435d965`; Job E2 is now integrated. E3/E4 remain separately
  qualifying; C6 awaits its current-producer integration. This is not accepted
  promotion or completion of the broader Job/Evidence work.
- `src/prototype/solver_inputs/joint_verification/e2-broad-summary.json` reports
  384 O2 recipes, 45 sanitizer commands and three unwaived strict reload cases:
  `1000:1000`, `1600:1600`, `1915:0`. These are **reported** results, not fresh
  passes/failures from this submission. Recipe counts are not independent
  semantic-contract counts. The report disclaims wall/RSS improvement,
  accepted promotion and full issue/Goal completion.

Inspection scale, not a progress/duplication/runtime metric:
`src/Makefile` has 541 lines; `compatibility.sh` 400;
`compatibility.tsv` 347; the five selected QuickSort/image scripts
55/123/47/92/274. These counts indicate where to inventory, not what to delete.

### Assessment

The proposal identifies a real coordination/verification-debt concern and merits
a tracked audit. It does not establish that any particular accepted test can
already be removed safely, that observed test growth is excessive by measurement,
or that a compiler soundness bug exists.

Adopt the first epoch as read-mostly inventory and recommendations. Start with
one small effective-invocation/property inventory and a QuickSort/persistence
pilot. Do not implement all proposed manifests as a new permanent harness before
showing their decision value. Core/user keep retirement and promotion authority;
a proposed `verification-audit` lane remains advisory until separately assigned.

Two additional safety qualifications apply to the supplied recommendations:

1. Matching source/fixture/command hashes alone is not generally sufficient to
   reuse a gate result. Pin the actual producer/binary, toolchain/flags,
   transitive inputs/harness/generators, relevant environment, policies/seeds and
   options. New assembled producers still require affected-layer and final joint
   qualification as appropriate.
2. A property may require multiple independent witnesses or interaction/scale
   cases. Matching property IDs is not proof of interchangeable coverage.
   The 20%/epoch-count thresholds are proposed review triggers, not automatic
   stop, test-retirement or waiver rules.

The central invariants are retained: distinguish implementation from verification
volume, current behavior from legacy compatibility, semantic failures from
harness failures, and Prototype integration from accepted adoption. Preserve
unique rejection/regression witnesses and the unwaived core failures.

### Plan

- [x] Read the complete supplied document and fetch current origin/main.
- [x] Inspect Make, compatibility, sorting/image scripts and coordination records.
- [x] Qualify snapshot drift and existing reporting without altering supplied text.
- [x] Create [Issue #59](https://github.com/repyt-margorp/a-program/issues/59)
  for TA1–TA8, with no test-deletion or worker-launch authorization.
- [x] Prepare this single documentation file for a separate PR.
- [ ] Complete the substantive inventory, overlap review, pinned cost measurement,
  legacy classification and progress-ledger work under the Issue.
- [ ] Obtain explicit approval before any permanent suite change.

Related: [#51](https://github.com/repyt-margorp/a-program/issues/51),
[#56](https://github.com/repyt-margorp/a-program/issues/56), and
[#52](https://github.com/repyt-margorp/a-program/issues/52).
This documentation submission does not close them or Issue #59.

Input body SHA-256:
`f70d0dd54a572588696f776fc4bd5e95028d89e425e2c5370b973125fe2215a4`.
Only the publication cover is added; the original body and its relative links
are retained. The three related document links exist at the inspection revision.
No accepted source/test/build changes, Book checks/PDF changes or sibling
development-checkout changes are part of this submission.

---

# Test-Suite Growth, Verification Debt, and Implementation Progress Audit

Date: 2026-10-03  
Status: proposed active audit; documentation only; no test deletion or accepted-source change is authorized by this document.  
Related: [Parallel Core, Backend, Performance and Surface Work](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md), [Solver and Evidence Duplication Audit](2026-09-30-SOLVER-EVIDENCE-DUPLICATION-AUDIT-AND-PLAN.md), [Artifact Semantic Persistence Refactor Plan](2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md).

This document audits whether the recent increase in implementation velocity is accompanied by healthy verification, or whether the repository is beginning to accumulate a second, increasingly expensive architecture in its test harnesses, epoch freezes, manifests, compatibility layers and repeated broad gates.

The central rule is:

> A larger test suite is not progress by itself. A smaller test suite is not progress by itself either. Verification is healthy when each retained test protects a distinct current contract at a justified layer, and implementation progress is reported independently from the amount of verification machinery surrounding it.

No test should be removed merely because the same fixture appears elsewhere. The audit unit is the **semantic verification obligation**, not the filename, shell command, fixture, or test count.

## Problem List

1. **TA1 — Progress accounting is conflating implementation, prototype integration, tests, documentation and acceptance.** Recent work can look extremely fast because many verified prototype epochs and large test/doc diffs land quickly, while accepted `src/` promotion and the three exact-resume failures remain separate.
2. **TA2 — The accepted test suite has hidden overlap and no explicit semantic inventory.** `check-acceptance` composes many targets, while the monolithic `check` target and specialized shell suites exercise overlapping fixtures and lifecycle paths without one machine-readable map of which distinct contract each invocation protects.
3. **TA3 — Historical compatibility coverage is still mixed into current verification.** Archived legacy fixtures, current syntax inventory, compatibility semantics and current acceptance are intentionally different concepts, but their boundaries are not obvious enough to prevent old tests from becoming permanent obligations by inertia.
4. **TA4 — QuickSort, persistence and source/image verification form a high-cost overlap cluster.** Multiple scripts repeatedly check complete, pending, retained, reloaded and negative variants over related sorting/proof fixtures. Much of this may be orthogonal and necessary, but the current suite does not expose which runs are unique obligations and which are repeated demonstrations of the same property.
5. **TA5 — The verification harness is now producing its own operational failure modes and artifact volume.** Missing private-copy inputs, manifest/report normalization, assembly mistakes and migration-report schema mismatches have already interrupted verification without representing compiler regressions.
6. **TA6 — A dedicated `verification-audit` worker is needed.** Implementation workers should not be the sole judges of whether their expanding gates are still unique, current, and cost-effective.
7. **TA7 — Tests need an explicit lifecycle: current contract, regression witness, compatibility-only, experimental/epoch-only, superseded or removable.** There is no single policy that requires every permanent test to state why it still belongs in the default suite.
8. **TA8 — Progress reports need a stable ledger that makes verification debt visible.** Commit count, test count and green gates must not obscure whether semantic state is actually being simplified, whether prototype work is promoted, or whether known core failures are moving.

---

## TA1 — Separate implementation progress from verification volume

### Subjective (User)

2026-10-03, English paraphrase of the current user concern: recent A Program development appears substantially faster, but the user is concerned that the apparent speed may partly be an expanding test/harness system rather than implementation progress. The user wants actual progress to remain visible and wants a dedicated worker to audit whether old tests are being dragged forward or whether multiple tests are duplicating the same verification obligation.

This is not a request to weaken verification. It is a request to prevent verification machinery from becoming an unmeasured second architecture whose growth is mistaken for compiler progress.

### Objective (Code)

The repository currently still defines the accepted implementation as the pointer core under `src/`; prototype work remains under `src/prototype/` and requires explicit promotion. The current top-level README also continues to state that the C backend is a separate prototype and that exact saved-Solve resumption remains unfinished.

The current coordination plan records:

- accepted semantic baseline `eb0aad673dd0fb5219eb0d720d45a819cc50edba`;
- reviewed prototype Main checkpoint `435d965`;
- Surface prototype delivered and stopped;
- C backend epochs through Epoch5 integrated into prototype Main;
- Job/Evidence Epoch1 integrated, with later E2/E3/E4 still separate;
- performance correction integrated, while the full performance Goal remains active;
- the same three strict public split-fuel/exact-resume failures remain visible and unwaived.

Recent diffs demonstrate why raw line or commit counts are misleading. C backend Epoch5 reports lowering changes of only `+7/-4`, build `+8/-0`, tests `+488/-4`, and documentation `+492/-8`. This can be completely legitimate for an ABI boundary, but it means that roughly one thousand changed verification/documentation lines cannot be presented as one thousand lines of compiler implementation progress.

Job/Evidence work gives a more useful kind of progress signal: E1 removed copied references without changing dispatch count; the combined census attributes 4,629 fewer dispatches to the performance head path and 3,753 fewer copied result references to Job E1. E4 removes three duplicated registration fields. These are structural deltas and ownership simplifications, even where wall/RSS speedup has not yet been measured.

### Assessment

The project is currently in a **high-throughput prototype exploration phase**, not a uniformly high-throughput accepted-core phase.

That is not a defect. The problem is reporting. A report that says “five epochs landed” or “384 recipes pass” can make progress look larger than it is if it does not separately state:

1. what semantic/implementation structure changed;
2. whether the change is only prototype Main or accepted `src/`;
3. whether code was deleted or merely wrapped;
4. whether a previously open core failure closed;
5. whether verification cost increased;
6. whether tests/docs increased because a new property was added or because an existing property was re-expressed.

The correct progress unit is therefore **contract and architecture movement**, not test count.

### Plan

- [ ] Add a compact progress ledger to the active coordination plan. Every material epoch should report the following fields separately:
  - accepted source delta;
  - prototype implementation delta;
  - test delta;
  - documentation/report delta;
  - structures/fields/jobs/owners removed or added;
  - known failures closed, introduced, or unchanged;
  - current promotion state: worker-only / task branch / prototype Main / accepted `src/`;
  - measured runtime/memory effect if actually measured, otherwise `unmeasured`.
- [ ] Do not use commit count, passing-test count or total changed LOC as a primary progress metric.
- [ ] Add a one-line **verification debt delta** to each epoch: number of new permanent obligations, new epoch-only gates, removed/superseded obligations, and newly discovered harness failures.
- [ ] Treat promotion to accepted `src/` as its own milestone rather than silently equating prototype Main integration with accepted implementation.
- Completion: a reader can determine in one table whether A Program itself became simpler/more capable, whether only its verification grew, and which unresolved semantic failures moved.

---

## TA2 — Audit the accepted suite by semantic obligation, not by file count

### Subjective (User)

2026-10-03, English paraphrase: audit whether old tests are being carried indefinitely or multiple tests are covering the same thing. A dedicated worker should inspect this continuously as implementation accelerates.

### Objective (Code)

`src/Makefile` currently exposes a broad `check-acceptance` target composed from the ordinary `check` target plus examples, result checks, compatibility, sorting/proof suites, image-origin and handler/origin checks, and several newer specialized suites.

The accepted `check` target itself is large. It runs internal C tests and shell tests and then contains a long inline sequence of `program_test` checks over many acceptance fixtures. The same Makefile also runs separate specialized shell suites over many of the same conceptual areas.

A concrete overlap candidate already exists around the simple examples:

- `check-examples` runs `examples/01-09` through `pointer-check` for source admission;
- the ordinary `check` target runs `syntax_io_test` over `examples/01-09`;
- the ordinary `check` target runs `parse_files` over `examples/01-09`;
- `check-example-results` validates selected results for examples 03-09.

These are **not automatically redundant**. They may protect four different layers: parsing, syntax serialization, admission, and result semantics. The problem is that this distinction is implicit in Makefile structure rather than recorded as explicit contract IDs.

The same pattern occurs at larger scale. A fixture can be checked directly, saved as an image, reloaded, compared with `program_test`, rerun with retained reductions, rerun at fuel cuts, and then included again in a compatibility or specialized property script. Without an inventory, repeated use can be either valuable orthogonal coverage or unnoticed duplication.

### Assessment

The suite is mature enough that filename-level deduplication would be dangerous. The audit must classify tests by the **observable promise** they protect.

The following classes should be distinguished:

1. **Exact duplicate** — same producer, binary, options, input, assertion and expected outcome. One instance is normally removable.
2. **Semantic duplicate** — different harness spelling but no additional observable boundary; both fail and pass for the same reason. Candidate for consolidation.
3. **Orthogonal reuse** — same fixture, but a different contract is checked, for example parse vs admission vs persistence vs result equality. Keep, but name the separate obligations.
4. **Regression witness** — deliberately narrow reproduction of a past bug. Keep if the bug remains plausible and the witness is cheaper or clearer than a broader suite.
5. **Historical compatibility** — protects an intentionally supported legacy behavior. It should not silently become a current language contract.
6. **Epoch-only verification** — proves one prototype handoff or migration and need not become permanent accepted-suite coverage after its unique property is represented elsewhere.
7. **Harness/meta verification** — validates publication, manifests, transport or audit tooling rather than language semantics. Keep outside semantic progress counts.

The current suite does not provide a central mapping from invocation to one of these classes.

### Plan

- [ ] Create a generated inventory, initially under a prototype audit directory, with one row per effective test invocation and at least:

  `test_id, entrypoint, target, binary, fixture, options, expected, layer, property_id, lifecycle, historical_origin, owner, estimated_cost, duplicate_group, promotion_blocking`

- [ ] Assign stable semantic property IDs. Example families:
  - `PARSE.CURRENT`;
  - `SYNTAX.ROUNDTRIP`;
  - `SOURCE.ADMISSION`;
  - `RESULT.EQUALITY`;
  - `IMAGE.INERT_RESAVE`;
  - `IMAGE.PENDING_RESUME`;
  - `IMAGE.REJECTED_RESUME`;
  - `EFFECT.ORDERING`;
  - `SORT.ORDINARY_RESULT`;
  - `SORT.LOCAL_SORTED`;
  - `SORT.STRONG_SORTED`;
  - `LEGACY.SOURCE_COMPAT`.
- [ ] Normalize commands and identify byte-for-byte/exact invocation duplicates automatically.
- [ ] Review semantic duplicates manually. Do not delete merely because fixture/options resemble another test.
- [ ] For each permanent test, require either a unique `property_id` or a documented regression-witness reason.
- [ ] Produce `test-overlap.tsv` and a short Markdown summary containing keep/consolidate/archive/remove recommendations, but make no deletion in the first audit epoch.
- Completion: every default acceptance invocation has a named current reason to exist.

---

## TA3 — Separate historical compatibility from the current language contract

### Subjective (User)

2026-10-03, English paraphrase: old tests should not be dragged forward indefinitely merely because they once existed. The current system should preserve meaningful regressions and compatibility deliberately, not inherit them accidentally.

### Objective (Code)

There are currently at least three distinct compatibility-related mechanisms:

1. `tests/compatibility.sh` is a semantic compatibility gate. It explicitly enables `--legacy-intrinsic-dot`, reads archived legacy fixtures, checks selected result pairs, and contains additional current image/result checks.
2. `tests/compatibility.tsv` is a broader inventory. It includes old AST/CLI/internal-check/program entries from `archive/legacy/`, with many entries currently marked `review_pending`.
3. `tests/syntax_inventory.sh` reads only `program` entries from `compatibility.tsv`, parses them with legacy spelling enabled, and explicitly reports syntax expectations rather than semantic acceptance.

The current README correctly distinguishes the archived old implementation from the promoted pointer core. Nevertheless, current test machinery still deliberately reaches into `archive/legacy/` for compatibility fixtures and current QuickSort/provider cases.

`compatibility.sh` also grew beyond a compact “legacy compatibility” check: after its initial case table it contains current source/image persistence, host values, effects, function-graph and proof/result checks. This makes the boundary between “legacy compatibility” and “current semantic regression suite” increasingly unclear.

### Assessment

Using archived fixtures is not itself wrong. The risk is **semantic inheritance by location**: a test that began as a bridge during pointer-core promotion may remain in the default gate even after a clearer current fixture protects the same contract.

A test should remain compatibility-only when the reason is genuinely “we intentionally still support this old source or behavior.” A test should migrate to a current fixture when it protects an active current-language rule independent of historical syntax. A test should be retired when it only demonstrates a superseded implementation detail.

The `review_pending` inventory is useful evidence that compatibility review is incomplete; it should not be interpreted as a permanent requirement to recreate every old capability.

### Plan

- [ ] Split the compatibility inventory conceptually into:
  - **supported legacy source compatibility**;
  - **historical inventory / review pending**;
  - **current semantic regressions that happen to use old fixtures**.
- [ ] For every `archive/legacy/` fixture executed by the default acceptance suite, record one of:
  - `legacy-contract` — old syntax/behavior intentionally supported;
  - `current-contract-needs-migration` — property is current; replace with a current fixture before retiring the old one;
  - `unique-regression-witness` — old fixture is the smallest useful witness;
  - `historical-only` — remove from default acceptance and retain in inventory/archive.
- [ ] Do not require `review_pending` entries to become passing current tests by default. Review is a classification task, not a restoration mandate.
- [ ] Move current semantic checks out of `compatibility.sh` when their only connection to compatibility is historical accident, after proving equivalent coverage elsewhere.
- [ ] Keep `--legacy-intrinsic-dot` coverage intentionally small and clearly separate from default-spelling/current-source tests.
- Completion: default acceptance does not depend on an archived fixture unless a current documented contract requires it.

---

## TA4 — Audit the QuickSort/persistence overlap cluster before it expands further

### Subjective (User)

2026-10-03, English paraphrase: test growth should not run ahead of the implementation. Expensive repeated verification, especially around known large workloads, should be audited for overlap instead of being copied into every new worker epoch.

### Objective (Code)

QuickSort and related sortedness/persistence properties currently appear across multiple permanent suites:

- `compatibility.sh` checks the archived fuel-free QuickSort fixture, several concrete outputs, packet construction, saved images, legacy QuickSort witness/property clients and retained-result images;
- `generic_sorted.sh` checks generic comparator/sortedness boundaries, complete and pending images, retained images and negative comparator cases;
- `quick_result.sh` checks the ordinary-result theorem, pending/resume, retained/reload and negative final post-check behavior;
- `retained_quicksort.sh` checks solved/retained/WHNF combinations and inert resave behavior;
- `local_strong_sorted.sh` checks local-vs-strong sortedness, cyclic comparator separation, complete/pending/retained images and legacy consumers;
- `image_cli.sh` uses the archived QuickSort fixture in retention/origin checks and separately applies persistence cuts to a large set of acceptance fixtures.

Again, these are not all duplicates. The same large fixture is being used to protect different properties. The problem is that the property boundaries are implicit, and each new script tends to reproduce its own save/load/pending/retained loops.

The current performance/Job verification also runs large current-producer acceptance and checkpoint matrices around the same kinds of artifacts. The coordination plan already instructs workers to avoid needless identical suite reruns when qualifying E2/E3/E4 in order.

### Assessment

This cluster is the highest-value first target for the verification audit because it combines:

- high runtime;
- large proof workloads;
- multiple persistence modes;
- old and current fixtures;
- several distinct correctness theorems;
- repeated shell implementations of the same image lifecycle pattern.

The likely simplification is **not** “one QuickSort test.” It is to separate reusable lifecycle mechanics from semantic obligations.

For example, the property “a pending image resumes to the same final verdict” should have one canonical lifecycle helper. Sorting theorem suites should supply the source and expected semantic assertions rather than each reimplementing the save/resave/reload protocol. Conversely, local-vs-strong sortedness and ordinary-result correctness are distinct mathematical obligations and must remain separate even if they share a provider.

### Plan

- [ ] Make QuickSort the first full `property_id` mapping exercise.
- [ ] Build an overlap matrix with rows for scripts and columns for:
  - source admission;
  - concrete output;
  - ordinary-result theorem;
  - local sortedness;
  - strong sortedness;
  - negative theorem/post-check;
  - pending image;
  - retained image;
  - inert resave;
  - reload final verdict;
  - byte identity;
  - source/image NF equality;
  - legacy spelling/provider compatibility.
- [ ] Identify repeated lifecycle code that can eventually share one helper without merging semantic assertions.
- [ ] Measure wall time and invocation count for each QuickSort-related obligation on one pinned producer.
- [ ] Retain at least one small fixture for each persistence mechanic when a giant QuickSort proof is not necessary to exercise that mechanic.
- [ ] Keep QuickSort where its size or proof structure is itself required to expose a bug; document that reason.
- [ ] Before adding another permanent QuickSort gate, require the worker to state which matrix cell is new.
- Completion: the project can explain why every expensive QuickSort invocation exists and can estimate what would be lost if it were removed.

---

## TA5 — Treat harness failures as verification debt, not compiler failures

### Subjective (User)

2026-10-03, English paraphrase: a separate audit worker is needed because test/harness infrastructure can itself become a source of complexity and false progress. Verification should make the implementation safer, not become another unstable subsystem.

### Objective (Code)

The current coordination record contains multiple failures caused by verification/assembly infrastructure rather than the compiler behavior under test:

- a broad acceptance run failed because a worker's private copy omitted four training inputs; restoring the inputs made the unchanged syntax gate pass;
- a Job/Evidence publication report normalized TSV newlines and thereby broke exact report hashes; publication was correctly held while the runtime freeze remained unchanged;
- the first E2 qualification setup stopped at a migration TSV column-name error before any gate ran;
- one combined performance build accidentally replaced the newer prototype `eval.h` with the accepted header; correcting assembly produced the expected passing result;
- a C epoch archive lacked Git metadata and an initial assembly omitted Surface; corrected assembly then passed the intended gates.

These are valuable findings because they demonstrate that the verification system now has enough moving pieces to fail independently.

The current epoch system also retains exact source manifests, report hashes, local logs, canonical patches and frozen snapshots. This gives strong provenance but increases maintenance volume and the possibility that report-generation or assembly mechanics become the dominant source of failure.

### Assessment

The project has crossed the point where “run more gates” is sufficient process guidance.

Verification artifacts should be divided into:

1. **semantic evidence** — demonstrates compiler/language/backend behavior;
2. **reproducibility evidence** — pins the producer, inputs and relevant outputs;
3. **transport/publication evidence** — proves the worker handed off the intended bytes;
4. **harness self-tests** — proves the audit/coordination machinery itself works.

These layers should not all be counted as compiler correctness tests. A failed report hash due to newline normalization is important operational evidence, but it must not appear as a semantic regression. Conversely, hiding such failures would make the process untrustworthy.

### Plan

- [ ] Add `failure_class` to epoch reports: `semantic`, `compiler-crash`, `sanitizer`, `harness`, `assembly`, `transport`, `report-generation`, `environment`, `expected-known`.
- [ ] Progress summaries must report semantic failures separately from harness failures.
- [ ] Stop committing or copying large per-epoch artifacts solely because an earlier epoch did so. Retain the minimum canonical evidence needed to reproduce the claim.
- [ ] Prefer Git object IDs plus a concise manifest over copying canonical patches already pinned by Git, matching the E3 lesson already recorded in the coordination plan.
- [ ] Reuse one manifest/report schema. A worker must not invent a new TSV/JSON shape for each epoch without a new requirement.
- [ ] Add a small self-test for the audit manifest generator itself, but do not place its pass count in semantic acceptance metrics.
- [ ] Track **harness-caused interruption count** per development period. An increasing count is a signal to simplify the verification infrastructure.
- Completion: a reader can tell immediately whether a red gate means an A Program regression or a broken verification setup.

---

## TA6 — Add a dedicated `verification-audit` worker

### Subjective (User)

2026-10-03, English paraphrase: there should be a worker whose job is to audit whether old tests are being carried forward and whether tests overlap as implementation accelerates. Actual implementation progress must remain visible.

### Objective (Code)

The current worker model separates Core coordination, C backend, performance, Surface and Job/Evidence. Core already reviews diffs/tests and performs integration, but Core is also responsible for coordination and architecture review. Implementation workers naturally optimize for proving their own epoch safe; they are not structurally independent auditors of global suite growth.

The active coordination plan already demonstrates the benefit of independent roles: performance can attribute dispatch reduction separately from Job/Evidence storage reduction; Core can reject E5 rather than allowing its author to reinterpret the failing namespace case. A verification-audit lane extends this separation to test debt.

### Assessment

A dedicated worker is justified, but it should **not** become another implementation owner or a sixth source of permanent gates.

Its first role is read-mostly analysis. It should produce evidence and deletion/consolidation proposals. It should not rewrite accepted tests autonomously because test deletion changes the project's safety boundary.

Recommended worker name: `verification-audit`.

Recommended write scope:

- `doc/` for the active audit plan and reports;
- a new prototype-only audit tool directory such as `src/prototype/test_audit/` if scripts are needed to inventory Make/shell invocations;
- no direct edits to accepted `tests/`, `src/Makefile`, or accepted `src/` during the first audit epoch.

### Plan

#### Worker Goal

> Inventory current and historical verification obligations, detect exact and semantic overlap, measure test cost, distinguish semantic failures from harness failures, and recommend consolidation without weakening current contracts. Keep implementation progress and verification growth separately measurable.

#### Required first-epoch outputs

- [ ] `doc/2026-10-03-TEST-SUITE-GROWTH-AND-PROGRESS-AUDIT.md` — this active SOAP audit.
- [ ] `src/prototype/test_audit/test_inventory.tsv` — one row per effective default-suite invocation.
- [ ] `src/prototype/test_audit/property_inventory.tsv` — one row per semantic obligation and the tests protecting it.
- [ ] `src/prototype/test_audit/overlap_report.tsv` — exact/semantic duplicate candidates and orthogonal reuse groups.
- [ ] `src/prototype/test_audit/gate_inventory.tsv` — Make targets / shell entrypoints / per-epoch broad gates and their inclusion relationships.
- [ ] `src/prototype/test_audit/cost.tsv` — repeated wall time plus invocation count on a pinned producer, without concurrent performance workers.
- [ ] `src/prototype/test_audit/legacy_review.tsv` — every default-suite dependency on `archive/legacy/` with its retention reason.

#### Worker operating rules

- [ ] Read current `src/Makefile`, test scripts, compatibility inventory, active epoch plans and accepted README before classifying anything.
- [ ] Never equate same fixture with duplicate test.
- [ ] Never remove a negative test unless the protected rejection is represented by another explicit property witness.
- [ ] Never convert an inherited failure into expected success/failure merely to simplify the suite.
- [ ] Do not run broad suites repeatedly while constructing the inventory. Static analysis comes first; runtime cost measurement is a separate pinned pass.
- [ ] Reuse already recorded current-producer results where hashes and commands prove equivalence; rerun only when the audit needs a measurement not present in existing evidence.
- [ ] Notify Core when a candidate deletion affects a promotion-blocking property or when two workers are adding equivalent permanent tests.
- [ ] Do not become an approval authority. Core and the user decide permanent test deletion/promotion.

#### Continuous duty after the first audit

For each new worker handoff, inspect:

1. Which new semantic property is covered?
2. Is the test permanent, regression-only, compatibility-only or epoch-only?
3. Does an existing test already protect the property?
4. If an expensive fixture is used, is its size/structure necessary?
5. Did the epoch increase harness complexity?
6. Did implementation complexity decrease enough to justify the new verification cost?

The worker should return a short delta rather than rerun the entire global audit for every epoch.

- Completion: new tests cannot accumulate silently; every permanent addition has a recorded unique obligation or regression reason.

---

## TA7 — Introduce an explicit test lifecycle and retirement policy

### Subjective (User)

2026-10-03, English paraphrase: the concern is not merely test count but old tests being dragged forward, duplicated tests accumulating, and development becoming dominated by verification scaffolding.

### Objective (Code)

The current repository has several natural lifecycle classes already, but they are expressed by convention rather than policy:

- current accepted `tests/`;
- archived legacy fixtures;
- compatibility inventory marked `review_pending`;
- prototype epoch verification directories and frozen manifests;
- worker-private setup failures and retained reports;
- permanent regression fixtures created after a discovered bug, such as the namespace test retained after rejecting Job/Evidence E5.

The E5 example is instructive. The implementation was rejected, but the 17-line test-only control is valuable and is being retained because it exposes a real nominal-identity hazard. This is healthy regression-test growth. By contrast, a publication TSV normalization failure should remain historical process evidence, not necessarily become another permanent compiler test.

### Assessment

Every test should have a lifecycle reason. The following states are sufficient:

- `current-contract` — permanent default-suite protection for current behavior;
- `regression-witness` — permanent narrow protection for a historical bug class;
- `compatibility-contract` — intentionally supported legacy surface/behavior;
- `audit-only` — measurement or structural audit, not default correctness;
- `epoch-only` — handoff/migration qualification that can retire after promotion;
- `superseded` — coverage exists in a clearer/cheaper current test;
- `historical` — retained as evidence but not run by default.

Tests should not stay `epoch-only` indefinitely. A prototype promotion should either migrate its unique properties into the accepted suite or explicitly retire the epoch harness.

### Plan

- [ ] Add lifecycle classification to the audit inventory before changing the suite.
- [ ] New permanent tests must declare a property ID and lifecycle.
- [ ] Every prototype promotion must answer:
  1. which epoch-only gates become permanent;
  2. which are already covered by accepted tests;
  3. which can be archived with the prototype handoff.
- [ ] Review `archive/legacy/` dependencies separately from current regression witnesses.
- [ ] Prefer one small regression witness over permanently running an entire historical integration scenario when both fail on the same invariant.
- [ ] Preserve large integration cases when their scale, ownership graph, persistence cut or interaction is necessary to expose the bug.
- [ ] Add a quarterly or milestone-based audit trigger: accepted promotion, major artifact-format change, or >20% default-suite runtime growth since the last audit.
- Completion: every default test has an owner, property and lifecycle; no test survives only because deleting it feels risky.

---

## TA8 — Establish a verification-aware progress ledger and stop conditions

### Subjective (User)

2026-10-03, English paraphrase: actual implementation progress is very important. The audit should make it obvious whether A Program is moving forward or merely producing more tests and reports.

### Objective (Code)

Recent work contains both healthy and risky signals.

Healthy signals:

- E5 Job/Evidence deletion was rejected by a real semantic counterexample rather than merged for apparent progress.
- The same three public exact-resume failures remain unwaived instead of being reclassified away.
- Performance and Job/Evidence effects are separately attributed: fewer dispatches vs fewer copied references.
- Structural deletions and owner-localization are occurring in prototype code rather than only new tests being added.

Risk signals:

- accepted-source promotion is not keeping pace with the number of prototype epochs;
- broad acceptance/sanitizer/checkpoint matrices are repeatedly frozen around slightly different producers;
- harness/assembly/report failures already consume review cycles;
- some epochs add hundreds of test/doc lines around very small implementation diffs;
- the exact-resume failures remain unchanged despite substantial surrounding activity.

### Assessment

The project should define an explicit point at which continued verification expansion becomes a blocker rather than additional confidence.

A practical rule is:

> If two consecutive development epochs add permanent verification obligations without either removing implementation state/complexity, closing a known semantic failure, adding a genuinely new supported feature, or promoting previously verified work, Core should pause new broad-gate creation and request a verification-audit review.

A second rule should address repeated producer qualification:

> If the source/fixture/command hashes for a broad gate are unchanged, do not rerun it merely because another report was generated. Reuse the prior result when provenance is sufficient, and rerun only the delta/affected gates plus one final combined qualification at the integration boundary.

This does not weaken acceptance; it moves repeated work to the point where it changes the decision.

### Plan

#### Progress ledger

For every supervision checkpoint, record:

| Metric | Meaning |
| --- | --- |
| Accepted capability delta | New or removed behavior in accepted `src/` |
| Prototype capability delta | Verified but not promoted behavior |
| Structural complexity delta | Jobs, fields, duplicate owners, copied refs, dispatches, or code paths removed/added |
| Permanent test obligations delta | New minus retired accepted-suite obligations |
| Epoch-only verification delta | Temporary handoff gates/reports added |
| Default-suite wall time | Measured on one pinned producer, exclusive slot |
| Harness failures | Failures in assembly/report/transport rather than compiler semantics |
| Semantic failures | New/closed/unchanged known compiler/language failures |
| Promotion backlog | Verified epochs not yet accepted/rejected |

#### Immediate stop conditions

Core should request verification-audit review before another broad permanent test expansion when any of the following occurs:

- [ ] default acceptance runtime grows by >20% without a new supported semantic contract;
- [ ] the same semantic property is implemented independently in three or more shell loops;
- [ ] two consecutive worker epochs are blocked by harness/report/assembly failures rather than implementation failures;
- [ ] a prototype lane accumulates three or more verified epochs awaiting integration while adding further permanent gates;
- [ ] a large legacy fixture remains in default acceptance but nobody can state its unique current property;
- [ ] permanent test/doc additions exceed implementation changes by an order of magnitude for several consecutive epochs and no explicit ABI/persistence reason explains the ratio;
- [ ] known P0 semantic failures remain unchanged while new peripheral verification layers continue to grow.

#### Priority for the current repository

The first audit should prioritize:

1. the Make target/invocation graph;
2. QuickSort/sortedness/persistence overlap;
3. all default-suite dependencies on `archive/legacy/`;
4. image lifecycle loops repeated across scripts;
5. current-producer epoch gate repetition;
6. the three exact-resume failures as a fixed reference point for whether surrounding work is converging.

- Completion: Core can answer “is A Program progressing?” without citing test count, and can answer “why is this test still here?” without reading the entire repository history.

---

# Initial Audit Findings — Do Not Treat as Deletion Authorization

The following are immediate candidates for the `verification-audit` worker. They are observations, not conclusions that tests should be removed.

| Cluster | Evidence | Preliminary classification | Required audit |
| --- | --- | --- | --- |
| Examples 01-09 | `check-examples`, `syntax_io_test`, `parse_files`, selected `check-example-results` | likely orthogonal reuse mixed with some redundant admission work | map parse / syntax-I/O / admission / result properties separately |
| QuickSort ordinary result | `compatibility.sh`, `quick_result.sh`, `generic_sorted.sh`, `retained_quicksort.sh`, `local_strong_sorted.sh` | distinct theorems plus repeated lifecycle mechanics | build property matrix and factor lifecycle repetition only after coverage proof |
| Pending/save/load loops | `generic_sorted.sh`, `local_strong_sorted.sh`, `image_cli.sh`, `compatibility.sh`, `quick_result.sh` | strong semantic value, but shell mechanics are repeated | identify one canonical lifecycle helper and preserve distinct semantic assertions |
| Legacy fixture use | `compatibility.sh`, `retained_quicksort.sh`, `image_cli.sh` and current provider imports | mixed compatibility/current regression | classify every archived dependency by retention reason |
| Syntax inventory vs semantic compatibility | `syntax_inventory.sh` reads `compatibility.tsv`; `compatibility.sh` has a separate hard-coded semantic set | not duplicate, but naming/ownership ambiguity | document separate contracts and ensure inventory does not become restoration mandate |
| Current-producer broad gates | 380/384 acceptance recipes, 45 sanitizer commands, 70 TotalResult cuts, 128-source manifests, 1033-record verification hash set | valuable integration evidence with rerun risk | normalize one gate inventory; rerun only changed layers plus final integration gate |
| Epoch report/manifests | exact hashes, frozen reports, publication deltas, external patches | reproducibility evidence, not semantic tests | minimize copied evidence and distinguish report failures from runtime failures |

# Proposed Worker Handoff

Suggested new lane:

```text
core
├── job-evidence
├── performance
├── c-backend
├── surface              [completed/stopped]
└── verification-audit   [read-mostly, suite debt/progress audit]
```

The worker must not become another integration authority. Its output is advisory evidence for Core and the user.

Recommended first instruction:

> Audit the current A Program verification system as a graph of semantic obligations rather than files. Inventory `src/Makefile`, `tests/*.sh`, acceptance fixtures, legacy dependencies and active prototype epoch gates. Identify exact duplicates, semantic duplicate candidates, orthogonal reuse, historical compatibility, regression witnesses and epoch-only verification. Measure the cost of default gates on one pinned producer. Do not delete or weaken tests in the first epoch. Produce a reviewed consolidation proposal and a progress ledger that separates implementation, tests, docs, prototype integration and accepted promotion. Pay special attention to QuickSort/persistence loops, archived legacy fixtures, and harness-caused failures. Keep the three exact-resume failures visible and unwaived.

# Completion Criteria for This Audit

This audit is complete only when all of the following are true:

- [ ] every default acceptance invocation is represented in the test inventory;
- [ ] every invocation maps to at least one semantic/meta property and lifecycle;
- [ ] all default dependencies on `archive/legacy/` have an explicit retention reason;
- [ ] exact duplicate invocations are identified;
- [ ] semantic duplicate candidates have a written keep/consolidate decision;
- [ ] the QuickSort/persistence overlap matrix is complete;
- [ ] default-suite runtime is measured on a pinned producer;
- [ ] harness failures are separately counted from semantic failures;
- [ ] epoch-only verification has a retirement/promotion path;
- [ ] the active coordination plan reports implementation progress independently from verification volume;
- [ ] no test is removed without preserving every current property it uniquely protects;
- [ ] no inherited failure is waived or reclassified merely to simplify the suite.

Until those criteria are met, test growth should remain conservative: new regression witnesses are allowed when they expose a real bug or protect a genuinely new contract, but new broad wrapper suites should justify the property they add and why existing gates cannot express it.
