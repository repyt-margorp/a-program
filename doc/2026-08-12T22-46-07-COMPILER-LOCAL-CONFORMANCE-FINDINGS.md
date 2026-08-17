# Compiler Local Conformance Findings

Date: 2026-08-12 22:46:07 JST

## 1. Purpose

This document is the authoritative ledger for findings produced by the local
compiler conformance audit defined in:

- `doc/2026-08-12T09-00-00-COMPILER-KERNEL-LOCAL-CONFORMANCE-AUDIT-PLAN.md`

The audit records confirmed defects, disproved hypotheses, and unresolved
design questions separately. Program A is evidence collection only. No
implementation correction is made merely because a finding is recorded here.

## 2. Fixed Baseline

```text
implementation commit: 7d3be10bf0a7cd09abfc9b11e58431dd22f07e39
compiler: GCC 14.2.0
target: x86_64-linux-gnu
kernel: Linux 6.12.48+deb13-amd64
integration scripts: 27
```

Local evidence is stored outside the repository under:

```text
/tmp/a-program-conformance/7d3be10/
```

The evidence directory is not a durable artifact. Commands, locations, and
minimal reproducers required to reconstruct important results are therefore
recorded below.

## 3. Audit Progress

- [x] BH0: baseline, diagnostic profiles, and inventories
- [ ] BH1-BH4: C contracts, identities, ownership, and caches (suspended during BH1)
- [ ] BH5-BH8: kernel rules, effects, artifacts, and surface boundaries
- [ ] BH9-BH10: reduction profiles and residual-proof boundaries
- [ ] Program A consolidation
- [ ] Program B remediation planning

### 3.1 Suspension Record

The survey was suspended on 2026-08-12 during BH1 at the user's direction.
Conversation output concerning the audit was repeatedly interrupted or not
displayed, making the interactive workflow unreliable. This is recorded as an
interaction/output constraint, not as evidence that the local compiler audit
is technically impossible or that the confirmed findings cannot be remediated.

No cybersecurity investigation was being performed. BH1 was limited to local
compiler and artifact conformance: ID domains, lookup identity, integer and
slice bounds, allocation contracts, and unintended state changes. The work did
not access a remote target, privileged service, or third-party system.

At suspension:

- BH0 was complete;
- BH1 had produced the confirmed findings listed below;
- BH1's remaining equivalent-boundary classification was incomplete;
- BH2 through BH10 had not been surveyed; and
- no Program B implementation correction had begun.

If the survey is resumed, continue from the remaining v72 serialized-slice
checks and classify each equivalent-looking predicate by trust boundary before
moving to BH1 lookup and constness checks.

## 4. Finding Summary

| ID | Status | Severity | Area | Short description |
| --- | --- | --- | --- | --- |
| BH-CMEM-002 | Confirmed | Critical | Artifact publication | Empty slices reach copy interfaces with null pointers |
| BH-CMEM-003 | Confirmed | Medium | Import driver | Successful source-import search retains a directory stream |
| BH-CMEM-004 | Confirmed | Critical | Resource usage | Empty usage vectors reach `memmove` with null pointers |
| BH-ART-001 | Confirmed | Critical | Artifact readback | Wrapped type-parameter slice passes validation and is indexed |
| BH-ID-001 | Confirmed debt | Low | Term normalization API | Three callers narrow unsigned Term IDs through `int` |
| BH-TEST-001 | Confirmed | Low | Test infrastructure | Artifact test depends on unsupported positive `rg --glob` behavior |
| BH-ANALYZER-001 | Disproved | None | Accepted replay | Reported null dereference is excluded by validated call ordering |
| BH-IH-API-001 | Under review | Undetermined | IH resolution API | Legacy subject-level API lacks typed Operation identity |

Severity describes the violated contract, not the expected size of a patch.
The two critical findings currently preserve semantic output in ordinary builds
but violate the C library contract under the diagnostic profile.

## 5. Confirmed Findings

### 5.1 BH-CMEM-002: Empty publication slices reach non-null copy contracts

```text
status: confirmed and reproduced
severity: critical under the C runtime contract criterion
remediation: deferred to Program B
```

Invariant:

> An empty semantic slice is represented by `count == 0` and may have a null
> data pointer. Physical copy operations must not evaluate that pointer through
> a C library interface requiring non-null arguments.

Observed sites:

| Site | Reports | Empty object |
| --- | ---: | --- |
| `src/prototype/src/artifact/publication/closure_marking_and_slices.inc:837` | 114 | canonical substitution order |
| `src/prototype/src/artifact/publication/dense_publication.inc:1054` | 32 | representation table |
| `src/prototype/src/artifact/publication/dense_publication.inc:1870` | 31 | compact type declarations |
| `src/prototype/src/artifact/publication/dense_publication.inc:1875` | 31 | compact constructor declarations |

Evidence:

```text
/tmp/a-program-conformance/7d3be10/BH0/runtime-check-integration.log
```

Root cause:

`artifact_alloc_bytes` deliberately maps a zero-length allocation to a null
pointer. Four publication paths nevertheless call `memcpy` without checking the
corresponding count. A zero byte count does not make a null argument valid under
the instrumented library contract.

Architecture direction:

- preserve `count == 0, data == NULL` as the empty semantic slice;
- do not allocate dummy semantic objects;
- do not add four unrelated local exceptions;
- introduce or reuse one authoritative count-aware slice-copy operation at
  physical publication boundaries; and
- audit every publication copy site against the same convention.

Permanent test requirements:

- publish a graph with zero substitutions;
- publish one with zero representations;
- republish one with zero type and constructor declarations;
- compare ordinary and diagnostic artifacts; and
- require no runtime contract report at zero-length copy boundaries.

Artifact impact:

No wire-format change is expected. The defect is in physical construction of
an already-defined empty slice.

### 5.2 BH-CMEM-003: Successful source-import search retains directory stream

```text
status: confirmed and reproduced
severity: medium
remediation: deferred to Program B
```

Invariant:

> Every successful or unsuccessful directory search closes its directory
> stream before returning to the compiler driver.

Owner:

- `src/prototype/src/driver/read_file.c:2354` opens the stream in
  `add_source_import_from_search_dirs`;
- failure and exhausted-search paths close it; and
- the successful candidate path returns at line 2418 without calling
  `closedir(directory)`.

Observed behavior:

`test_artifact_flow.sh` produced three child processes retaining one, two, and
one allocations of 32,816 bytes. Making the first report fatal traced it to a
successful compile using `--import-search-dir`.

Evidence:

```text
/tmp/a-program-conformance/7d3be10/BH0/localization-1786541175/results.tsv
/tmp/a-program-conformance/7d3be10/BH0/leak-first-1786541344/trace.log
```

Architecture direction:

Keep directory ownership local to the search function and converge every return
edge through one release discipline. This is separate from slice-copy findings
even though the same integration script exposes both.

Permanent test requirements:

- resolve one and two imports through `--import-search-dir`;
- exercise candidate-not-found and invalid-candidate paths;
- compare ordinary and diagnostic semantic output; and
- verify exactly one release on every path after a successful `opendir`.

Artifact impact:

None. This is driver resource ownership.

### 5.3 BH-CMEM-004: Empty resource-usage vectors violate copy contract

```text
status: confirmed by a minimal public-API reproducer
severity: critical under the C runtime contract criterion
remediation: deferred to Program B
```

Invariant:

> A zero-entry usage vector is a valid quantitative-resource value. Copying it
> must preserve the empty vector without requiring backing storage.

Owner:

`prototype_usage_vector_init` permits `entries == NULL, capacity == 0`.
`prototype_usage_vector_copy` rejects a null target only when the source count
is nonzero, but then calls `memmove` unconditionally at:

```text
src/prototype/src/kernel/resource_usage.c:88
```

Evidence:

```text
/tmp/a-program-conformance/7d3be10/BH1-empty-usage-copy.c
/tmp/a-program-conformance/7d3be10/BH1-empty-usage-copy/run-kernel.log
```

The minimal checker copies between two empty initialized vectors. It reports
both null pointer arguments while preserving semantic success.

Architecture direction:

This is the same physical empty-slice convention missing in BH-CMEM-002, but it
remains a separate finding because its owner is kernel resource evidence. The
eventual implementation should share the count-aware copy policy without
introducing dummy resource entries.

Permanent test requirements:

- copy `empty -> empty` with null storage;
- cover allocated-empty combinations;
- copy one and multiple sorted entries; and
- retain the overlap behavior currently promised by `memmove`.

Artifact impact:

None expected. Empty quantitative-resource evidence remains empty.

### 5.4 BH-ART-001: Wrapped type-parameter slice passes artifact validation

```text
status: confirmed by mutation of a compiler-produced v72 artifact
severity: critical
process result: segmentation fault (status 139)
remediation: deferred to Program B
```

Invariant:

> A serialized slice `(first, count)` is valid only when `first <= total` and
> `count <= total - first`. Validation must reject the slice before computing
> or indexing `first + count` in the serialized integer domain.

Owner:

`prototype_artifact_read_text_interface` reads `first_parameter` and
`parameter_count` as `uint32_t` values and validates them at:

```text
src/prototype/src/artifact/wire_v72.c:332
```

The current predicate is:

```c
export->first_parameter + export->parameter_count >
	interface->type_parameter_count
```

When `first_parameter == UINT32_MAX` and `parameter_count == 1`, unsigned
addition wraps to zero. An artifact with zero type parameters therefore passes
this predicate. The following loop indexes
`type_parameters[first_parameter + j]`, producing an out-of-range access.

Minimal reproduction:

1. Compile the repository's `examples/01_bool.p` to an artifact.
2. Change the first `type` export's `first_parameter` and `parameter_count`
   fields from `0 0` to `4294967295 1`.
3. Read the mutated artifact with `--read-graph`.

Observed artifact line:

```text
type Bool 0 0 0 4294967295 1 0 2 6002940286336241402 2 0 2 0 0 0 0 namespace 01_bool
```

Observed result:

```text
status=139
Segmentation fault
```

Evidence:

```text
/tmp/a-program-conformance/7d3be10/BH1-type-export-overflow.32GyxI/base.apo
/tmp/a-program-conformance/7d3be10/BH1-type-export-overflow.32GyxI/mutated.apo
/tmp/a-program-conformance/7d3be10/BH1-type-export-overflow.32GyxI/read.out
/tmp/a-program-conformance/7d3be10/BH1-type-export-overflow.32GyxI/read.err
```

A Program interpretation:

An artifact stores results of prior compilation and proof construction, but it
is not live in-memory authority merely because another compiler invocation
produced it. Readback is the boundary that must reconstruct and validate those
results before they re-enter the Term, Operation, Context, and proof graphs.
Consequently this is not an optional defensive check and cannot be justified by
the fact that the ordinary writer does not emit wrapped slices.

Architecture direction:

- define one authoritative overflow-free slice predicate over `(first, count,
  total)`;
- use subtraction after proving `first <= total`, or use checked arithmetic;
- apply the same predicate to every serialized and persistent slice domain;
- preserve sparse-storage `present` checks as a second, distinct condition;
- do not repair only this line while leaving equivalent predicates elsewhere;
  and
- do not clamp or normalize malformed artifact values, because that would turn
  invalid prior evidence into a different accepted graph.

The source audit has already found similar `first + count` predicates in type
exports, Operation cases, Match binders, type declarations, candidate premise
slices, and publication closure. Those locations remain under review until
their construction and validation boundaries are classified. This finding
confirms the bug family but does not claim every syntactically similar site is
externally reachable.

Permanent test requirements:

- mutate each v72 serialized slice to `(UINT32_MAX, 1)`;
- test `first == total, count == 0` as valid;
- test `first > total, count == 0` as invalid;
- test `first == total, count == 1` as invalid;
- test valid nonempty slices at both boundaries;
- require deterministic artifact rejection without graph mutation; and
- run the matrix in ordinary and diagnostic builds.

Artifact impact:

The v72 wire schema need not change merely to reject malformed slices. If a
shared slice representation or explicit validation status is serialized later,
that is a separate versioning decision.

### 5.5 BH-ID-001: Obsolete signed narrowing at normalization boundaries

```text
status: confirmed static design debt
severity: low at current fixed capacities
runtime counterexample: none at the current TermDB capacity
```

The public `prototype_term_normalize_complete_with_profile` API accepts a
`uint32_t term_id`. Three callers cast a valid Term ID through `int`:

- `src/prototype/src/frontend/lowering/graph_construction.inc:789`;
- `src/prototype/src/kernel/typing/classifier_solver.inc:342`;
- `src/prototype/src/kernel/typing/classifier_solver.inc:419`.

The current reader/repl capacity of 262,144 Terms cannot reach signed overflow,
so this is not presently a false acceptance or rejection. It is an obsolete ID
domain boundary left after the normalization API moved to unsigned identity.

Architecture direction:

Remove the narrowing consistently at the API boundary. Do not preserve it with
local range checks or warning-suppression casts.

Artifact impact:

None.

### 5.6 BH-TEST-001: Artifact test depends on a non-portable `rg` invocation

```text
status: confirmed in the isolated local environment
severity: low
classification: test infrastructure, not compiler semantics
```

`src/prototype/test_artifact_flow.sh` uses positive `--glob` filtering when
checking source files. The reduced `~/.local/bin/rg` wrapper in the isolated
worktree does not implement that option and can make the script match its own
text instead of the intended C sources. With the actual ripgrep executable, all
27 integration scripts pass.

Invariant:

> A compiler integration test must either declare the required tool semantics
> or avoid relying on behavior not provided by the repository's active tool
> environment.

Architecture direction:

Make the test's dependency explicit or express the source scan using a
repository-controlled file enumeration. Do not classify the wrapper-induced
failure as compiler evidence.

Permanent test requirements:

- run the test with the declared ripgrep implementation; and
- verify that the scan cannot include the test script itself.

Artifact impact:

None.

## 6. Disproved Hypothesis

### 6.1 BH-ANALYZER-001: Accepted replay null dereference

```text
status: disproved on the validated call path
origin: 15 GCC `-fanalyzer` reports
```

The analyzer considered a Claim whose Proposition is absent or has `UNKNOWN`
kind in `src/prototype/src/kernel/typing/accepted_replay.inc`.

That state may exist transiently during closure-rank recomputation, but
`prototype_judgement_validate_accepted_graph` calls
`prototype_judgement_db_rebuild_index` before either replay helper. Rebuilding
retrieves every non-invalid Derivation conclusion through
`prototype_judgement_claim_get` and rejects an absent or invalid Proposition.
Validation returns before replay can dereference it.

Conclusion:

Do not add repetitive null checks as a correctness repair. Passing the already
retrieved Proposition into the helper could improve API clarity, but that is a
separate refactor rather than a confirmed defect.

## 7. Under Review

### 7.1 BH-IH-API-001: Legacy subject-level IH resolution API

```text
status: under review; not a confirmed compiler defect
severity: undetermined
```

The active Operation classifier solver identifies induction hypotheses using
the owner Match Operation, recursive argument Operation, IH scope, case, and
field. This is consistent with the rule that one erased Core Term may have
multiple typed Operation occurrences.

An older public structure,
`prototype_induction_hypothesis_resolution_request`, carries only the subject,
IH scope, and argument and resolves through subject-level classifier lookup.
Current searches have found its declaration and definition but no production
caller.

Questions still to settle:

- is this API intentionally retained for an authority-neutral operation;
- can any accepted path call it indirectly;
- should it be removed as obsolete rather than upgraded; and
- does artifact or external API compatibility require recording its removal?

No implementation or severity decision should be made until those questions
are answered.

## 8. Program B Entry Rule

A confirmed finding enters remediation only after it has:

- an invariant stated independently of the current implementation;
- a minimal or integration reproducer;
- one identified ownership boundary;
- an architecture-level correction direction;
- permanent regression-test requirements; and
- an artifact compatibility assessment.

The findings above remain evidence records until Program A completes and the
user approves Program B implementation work.
