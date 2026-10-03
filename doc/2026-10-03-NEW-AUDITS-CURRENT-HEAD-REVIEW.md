# Problem List

## 1. Revalidate the two newly supplied audits against current A Program

### Subjective (User)

2026-10-03, English paraphrase: the user removed the previous input Markdown and
provided two new audit documents. These are intended for a new PR and potentially
critical Issues. Fetch latest A Program and establish the current situation.
The sibling development checkout must not be modified. No implementation repair
is requested by this task.

2026-10-03, follow-up English paraphrase: the user explicitly requests creation
of the appropriate Issues and PR. Publish two distinct Issues and one
documentation-only PR containing both supplied documents and this re-audit.
This authorizes publication, not implementation repairs or automatic adoption
of all proposed grammar decisions.

Inputs, read in full:

- `/home/repyt/workspace/temp/2026-10-02-BEND-CHECKER-PERFORMANCE-RECOVERY.md`
- `/home/repyt/workspace/temp/2026-10-03-FUNCTION-GRAPH-NAMED-BINDER-SYNTAX-AUDIT.md`

### Objective (Code)

Remote `origin/main`, freshly fetched on 2026-10-03:
`eb0aad673dd0fb5219eb0d720d45a819cc50edba`.
The separate previous documentation PR is not part of this review branch.
Accepted source and the `solver_inputs` overlay must be assessed separately.
The input performance report pins its experiments to `9146893`, not this HEAD.
Its `src/prototype/bend_benchmark` scripts/products are absent from current main.

### Assessment

Current execution results, current source observations, historical experiments,
and proposed language contracts will be labeled separately. A syntax-design
inconsistency is not automatically a kernel-soundness defect. Historical timings
must not be represented as a fresh current-head speed comparison.

### Plan

- [x] Read both supplied documents completely and fetch origin.
- [x] Inspect accepted and latest Prototype implementation boundaries.
- [x] Build and reproduce named-binding polarity and brace-mode controls.
- [x] Check performance mechanism claims and benchmark reproducibility limits.
- [x] Record results and proposed Issue/PR scopes without implementation edits.
- [ ] Publish two Issues and one documentation-only PR under the follow-up
  authorization; verify the remote file list and cross-references.

## 2. Function Graph named-binding polarity and brace modes

### Subjective (User)

The supplied document proposes consistent `local := source` spelling, a
recommended unordered `{{...}}` selector set, an ordered `{...}` selector list,
and retention of positional patterns, full dependent telescopes and role groups.
These are proposals in the supplied document, not claims of implemented behavior.

### Objective (Code)

Both binaries were freshly built from the pinned HEAD with default strict
`-std=c11 -Wall -Wextra -Werror -O2` flags:

- Accepted implementation: `src/Makefile`.
- Latest experimental implementation: `src/prototype/solver_inputs/overlay.sh`,
  including its artifact-persistence/readback/conversion prerequisites.

No implementation or accepted fixture was edited. Controlled copies of
`tests/acceptance/function-graph-named-fields.p` were tested outside the checkout.

| Control | Accepted | solver_inputs |
| --- | --- | --- |
| Current `tailLength := recursive` | DONE, 13,353 steps | DONE, 9,396 steps |
| Replace with `recursive := tailLength`, keep branch references | REJECTED, 9,961 steps | REJECTED, 7,161 steps |
| Wrap a current selector list in double braces | Parse error: expected named selector | Same parse error |
| Reverse current `tail; tailLength := recursive;` entry order | DONE, 13,353 steps | DONE, 9,396 steps |

`program_test --equal` confirmed `main`, `aliasMain`, `graphMain`, and `valueMain`
equal `expected`, independently with scheduler chunk 1 and 64, on both builds.
The reordered selector case also returns the same `graphMain` result in both
chunk configurations. Existing duplicate-selector and wrong-proof negative
fixtures are rejected on both builds.

Source observations:

- `src/syntax.c`, elimination-clause parser: first identifier becomes
  `item.name`; post-`:=` identifier becomes the expression atom.
- `src/synthesis.c`, `case_field_scope`: LHS resolves against source metadata;
  RHS is introduced as the local alias. The latest overlay retains this policy.
- `src/syntax.c`, `mark_index_names`: named clauses also introduce the RHS into
  their scope scan. This additional consumer must migrate with parser/elaborator.
- `case_field_scope` already populates a canonical field array, then walks the
  complete telescope in canonical order. Current single-brace listing order is
  not enforced. Omitted fields are still internally bound.
- `src/syntax_io.c` validates persisted clause items. A new ordered/unordered
  AST distinction and orientation contract must be reflected in persistence;
  old inputs cannot silently acquire the opposite binding meaning.
- Public README documents braces and generated graph types but not this named
  selector spelling. There is no named-case `{{...}}` parser branch.

Historical checks independently confirm the supplied account: commit
`4fe5aee` stores `source_symbol_id` before `:=` and `local_symbol_id` after it;
its permanent fixture contains `tailLength := recursive` followed by
`@recursive`. The earlier design explicitly says `selector := localName`.

### Assessment

The polarity inconsistency is confirmed. It was intentional historical syntax,
not an accidental regression introduced by the latest owner refactoring.
Calling it a surface-language design defect is supported; calling it a proven
kernel unsoundness or incorrect proof acceptance is not supported by these tests.
The existing wrong-proof negative control still rejects.

The proposed direction change is a breaking source migration. Do not guess
direction by checking which side happens to be a known selector: both can be
valid names. Identity mappings alone do not test migration correctly.

Two decisions should be distinguished:

1. Restore consistent binding polarity. This addresses the confirmed problem.
2. Add brace modes, including strict order for single braces. This is an
   additional design choice, not necessary to repair polarity. It intentionally
   rejects some currently valid single-brace proofs and couples that optional
   form to generated telescope order. If adopted, this compatibility cost needs
   explicit documentation.

Canonical dependent-context construction is already present. A correction
should reuse it rather than add a second telescope/Context implementation.
Unordered source selection does not mean an unordered dependent Context.
Selecting a source origin remains owner/case-local metadata, not a new global
name or an ordinary record-field identity.

"No kernel change intended" is a reasonable boundary, but not an exemption
from testing elaborated evidence, scope scans, source/image round-trips, hidden
dependencies and IH/graph companion association. Existing tests here do not
establish all those migration obligations.

### Plan

- [x] Reproduce current polarity and lack of double-brace support on both builds.
- [x] Verify current list reordering and existing negative controls.
- [x] Created [Issue #57](https://github.com/repyt-margorp/a-program/issues/57):
  consistent Function Graph binding polarity, explicit
  brace-mode decision, migration and persistence/role/dependency regression gates.
- [ ] Obtain a language-design decision before implementing a breaking grammar.
- [x] Prepare the supplied audit for the explicitly requested documentation PR,
  preserving the body with a dated current-head qualification.

## 3. Checker overhead, historical Bend measurements and reproducibility

### Subjective (User)

The supplied performance document recovers older AP/Bend measurements and new
experiments at AP `9146893f07f0fe3f4c8309c370420c1702dfb96d`. The current task
is to establish which concerns still apply to latest A Program, not to silently
promote its experimental implementation or relax proof validation.

### Objective (Code)

Current accepted source and the latest solver_inputs overlay both retain:

| Mechanism | Current source observation |
| --- | --- |
| Eager demanded-head materialization | `eval.c:resume_frame` calls `pg_materialize_step` before delivering a `const pg_term *` answer to the continuation. |
| Suspended argument prefix copying | The same path copies links through `pg_eval_frame_copy_argument` until the demanded target is replaced. |
| Administrative branch application | `iadt.c:apply_fields` builds `lambda k. k fields...` and applies that lambda to the branch closure. |
| Small-object arena minimum | `graph.c:pg_alloc` uses a minimum `512 * sizeof(max_align_t)` payload; it is not automatically small for temporary readback owners. |
| Per-index initial allocation | `graph.c:pg_index_init` initializes 64 buckets. |
| Per-transition reduction scheduling | `synthesis_conversion.c` advances WHNF with budget 1, polls the certificate, and re-enqueues pending work. |
| Repeated normalization input projection | `normalization_step` still obtains `pg_evidence_subject(checking_term)->core` each time. The accessor is currently a small switch/direct pointer read; no repeated term copying is established by that line. |

Thus the principal intermediate readback and branch-application mechanisms have
not been removed merely by the newer Job/Evidence owner refactoring. However,
the surrounding scheduling/preparation implementation has changed substantially;
its current costs require profiling, not transplantation of old percentages.

The report's `src/prototype/bend_benchmark/remaining_*` scripts, test adapter,
raw JSON, patches and workload sources are absent from fetched `origin/main`
and from that path at the pinned historical revision. They were not supplied
with the two Markdown files and were not found in the other available
`../temp/a-program-latest` clone. This establishes a missing reproducibility
bundle in the reviewed inputs, not that the reported local experiments never
occurred.

No fresh Bend installation, AP/Bend matched-workload timing, 400-pair replay,
allocation profile or experimental patch validation was performed in this
review. The fresh builds and named-binding executions above are not a checker
benchmark and do not establish a speed ratio.

### Assessment

The current source supports a concrete performance investigation Issue. It does
not yet support citing 3.908x time or 21.2x RSS as current-head measurements.
Those figures belong to the report's pinned revision plus its experimental
overlay, under different native proof encodings. Even "800 equalities" denotes
concrete computational checks, not 800 universal tree theorems.

Before implementing recovered changes, obtain the reproducibility bundle and
separate these obligations:

- Semantic acceptance/rejection: preservation of capture, neutral/effectful
  fallbacks, graph/equality rules, and rejection controls.
- Reduction policy and Effort: head readiness cannot issue completed evidence
  before required final readback/validation; changed administrative steps must
  have an explicit charging contract.
- Partition/checkpoint behavior: one-shot and resumed execution agree under
  the selected policy, including persistence. A faster answer on one file is
  insufficient. Existing public-reload failures must not be hidden by adapted
  fixtures or claimed as fixed by this review.
- Measurement: identical source hashes, compiler flags, workload correspondence,
  fresh processes, repeated interleaved timings, RSS and step/counter records.

The supplied report honestly records failures in original Core fixtures and
success only after a test adapter. That is valuable evidence, but not an unchanged
full-suite pass or permission to weaken a production test. A changed continuation
API or old fuel expectation requires independent new tests of the normative
behavior, alongside a documented migration decision.

Caching `pg_evidence_subject(...)->core` is currently an accessor/polling
optimization candidate, not an observed copying bug. Allocation minima should
be tuned by live owner/lifetime measurements rather than globally reduced on
faith. Neither source observation establishes a safe whole-program speedup.

### Plan

- [x] Confirm principal mechanisms still occur in accepted and latest Prototype.
- [x] Distinguish historical ratios from unmeasured current-head performance.
- [x] Identify missing experimental scripts/workloads/raw products.
- [x] Created [Issue #56](https://github.com/repyt-margorp/a-program/issues/56):
  reproduce and profile checker readback/closure/branch
  overhead on current HEAD, then prototype narrow optimizations with semantic,
  Effort and persistence gates. Relate to #51/#52 without claiming they already
  specify this complete performance investigation.
- [ ] Obtain the evidence bundle before treating recovered patches as verified.
- [x] Prepare both supplied documents for one documentation-only PR, with dated
  current-head qualification. No implementation change is included.

## Reproduction and scope

Local build, fixture copies and detailed result logs:
`/tmp/a-program-new-audits-20261003.hY5Rbc/`.
The runner is `named-binding-controls.sh`; its combined log is
`results/controls.log`. Temporary files are not durable upstream evidence.

Build the accepted binary from the pinned checkout using:

```sh
make -f src/Makefile BUILD=/tmp/ap-audit-accepted \
  /tmp/ap-audit-accepted/pointer-check /tmp/ap-audit-accepted/program_test
bash src/prototype/solver_inputs/overlay.sh /tmp/ap-audit-candidate
make -f /tmp/ap-audit-candidate/src/Makefile BUILD=/tmp/ap-audit-candidate/build \
  /tmp/ap-audit-candidate/build/pointer-check \
  /tmp/ap-audit-candidate/build/program_test
```

The overlay requires a previously nonexistent destination. Run `pointer-check
--steps 10000000` on the named-fields fixture, then on separate copies making
the three exact substitutions described in the controls table. Verify the
result and existing rejection controls using:

```sh
program_test --equal tests/acceptance/function-graph-named-fields.p graphMain expected
program_test --reject tests/acceptance/function-graph-named-duplicate.p
program_test --reject tests/acceptance/function-graph-named-wrong-proof.p
```

Input SHA-256:

- Performance: `20fc934285e40d093300f25101df400fbd785311d0f1eba1dd5b2782997dc930`.
- Named binders: `8e8b1b0e7b9d6062e99401ff44e444bc6b68ea3eec54a771f421cd531f3579f2`.

Both supplied originals are unchanged. The sibling `../a-program` development
checkout was not accessed or modified. No book chapter/example/PDF was changed;
no full acceptance, Book `make check`, PDF build or sanitizer pass is claimed.
The previous Distributed design-intent PR #55 remains separate and untouched.
