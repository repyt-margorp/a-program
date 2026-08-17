# Unified Test Timing Audit Implementation Plan

Date: 2026-08-18

Status: complete; TA0-TA5 verified

Implementation boundary: `src/prototype/`

## 1. Objective

Give every prototype integration test one uniform wall-clock measurement and
make the full-suite cost inspectable without changing test semantics.

The completed runner must answer:

- which test ran;
- whether it passed, failed, or exited by signal;
- its exit status and wall-clock duration;
- total suite duration;
- which tests dominate the suite; and
- whether a test was accidentally executed more than once.

This work measures and exposes cost. It does not optimize compiler algorithms
or weaken test coverage.

## 2. Current Audit

Current facts verified on 2026-08-18:

- `src/prototype/Makefile` discovers and runs 36 `test_*.sh` scripts serially;
- every integration script uses `/bin/sh`;
- the suite prints test names but no per-test duration or final timing summary;
- `/usr/bin/time` is absent in the current environment, so it cannot be a
  project dependency;
- the last measured full suite took 2,133.264 seconds;
- standalone artifact flow took 17.851 seconds;
- standalone HOTT goal took 32.489 seconds; and
- standalone dependent Match took 1,140.779 seconds because it included IF8;
- standalone IF8 took 1,108.018 seconds; and
- `test_dependent_match_refinement.sh` invokes the complete IF8 script, after
  which the suite runner discovers and executes IF8 again.

The current 35-minute result therefore does not mean artifact validation alone
takes 35 minutes. It combines repeated source compilation, normalization,
artifact publication/readback, negative cases, and one known duplicate run of
the most expensive IF8 test.

## 3. Normative Timing Policy

### 3.1 Timing unit

The primary unit is one top-level integration script:

```text
src/prototype/tests/integration/test_*.sh
```

Every discovered script must produce exactly one final timing record, including
on failure. Individual `grep`, `awk`, and assertion commands are not separate
timing units.

Long tests may additionally expose named phases. Phase records use the same
clock and output grammar, but never replace the enclosing test record.

### 3.2 Clock

Use a monotonic wall clock with millisecond output. Do not use:

- shell `SECONDS`;
- Bash-only `TIMEFORMAT`;
- `/usr/bin/time`;
- CPU time as a substitute for elapsed time; or
- wall-clock calendar timestamps whose value can move backwards.

Implement one small prototype-owned clock helper using
`clock_gettime(CLOCK_MONOTONIC)`. Compile it once before suite timing begins.
Failure to initialize the clock is a runner failure, not permission to emit a
fabricated zero duration.

### 3.3 Canonical output

Each top-level test emits one line:

```text
A_PROGRAM_TEST_TIMING 1 suite=integration test=test_artifact_flow status=pass exit=0 wall_ms=17851
```

Named phases emit:

```text
A_PROGRAM_TEST_PHASE 1 suite=integration test=test_artifact_flow phase=proof_replay status=pass exit=0 wall_ms=1170
```

The grammar is whitespace-delimited and versioned. Names may contain only
ASCII letters, digits, underscore, dot, and hyphen.

The suite emits one final record:

```text
A_PROGRAM_TEST_SUMMARY 1 suite=integration total=36 executed=36 passed=36 failed=0 skipped=0 wall_ms=2133264
```

It then prints a human-readable table sorted by descending `wall_ms`. The
versioned records remain the authoritative machine-readable output.

Timing records and the human summary go to stderr so existing test stdout
remains unchanged. `--timing-output FILE` duplicates only versioned records to
a machine-readable file.

### 3.4 Failure behavior

- Always emit the failing test's timing record.
- Preserve its real nonzero exit status.
- Default to fail-fast to preserve current `make test-integration` behavior.
- Add an explicit `--keep-going` audit mode that runs all remaining tests and
  exits nonzero when any test failed.
- Forward interruption signals and remove runner-owned temporary files.
- Do not hide or buffer the test's normal stdout/stderr.

### 3.5 Discovery and uniqueness

Continue automatic lexical discovery of `test_*.sh`; do not create a manually
maintained list that can omit a new test.

The runner must reject:

- duplicate test labels;
- non-regular or non-readable discovered entries;
- an empty suite; and
- direct invocation of another integration `test_*.sh` from a test script.

Integration tests are atomic suite entries. Shared setup belongs in a helper,
not in another top-level test.

### 3.6 Performance interpretation

Initial timings are diagnostic, not pass/fail criteria. Machine speed and load
may vary. The first implementation must not reject a semantically correct test
merely because it exceeds a fixed duration.

Report these bands in the summary:

| Band | Duration |
| --- | ---: |
| fast | below 1 second |
| normal | 1-10 seconds |
| slow | 10-60 seconds |
| very slow | 60 seconds or above |

Hard budgets may be introduced later only with a separately reviewed baseline
and explicit machine/environment policy.

### 3.7 Parallelism

Keep the first audited runner serial. Parallel execution changes CPU pressure,
memory pressure, output ordering, and measured durations. It may be added later
as a separate execution mode, not mixed into the timing baseline.

## 4. Target Structure

Add:

```text
src/prototype/tests/run_integration_suite.sh
src/prototype/tests/support/monotonic_clock.c
src/prototype/tests/checks/test_timing_runner.sh
```

Responsibilities:

| File | Responsibility |
| --- | --- |
| `run_integration_suite.sh` | discovery, execution, records, summary, exit policy |
| `monotonic_clock.c` | monotonic milliseconds only |
| `test_timing_runner.sh` | isolated pass/fail/signal/output/self-audit fixtures |
| `test_support.sh` | optional named phase helpers for long scripts |

The clock helper is infrastructure, not accepted compiler code. It remains
under `src/prototype/tests/`.

## 5. Implementation Phases

### TA0. Freeze baseline and output contract

Status: complete

- [x] Record the current 36-script inventory.
- [x] Record current total, artifact, HOTT, dependent Match, and IF8 times.
- [x] Freeze the record grammar from section 3.3.
- [x] Decide stdout/stderr ownership and document it in runner usage.
- [x] Confirm the runner does not include its own self-test in discovery.

Exit criteria:

- the baseline and record grammar are reviewable before runner code is added.

### TA1. Implement the monotonic runner

Status: implemented; focused verification passes

- [x] Add and compile the monotonic clock helper.
- [x] Discover all integration scripts lexically.
- [x] Time each script around the exact existing `sh script` invocation.
- [x] Emit one record for pass, ordinary failure, and signal termination.
- [x] Implement fail-fast and explicit `--keep-going` modes.
- [x] Accumulate the suite duration independently of summed test durations.
- [x] Print descending timing and duration-band summaries.
- [x] Return nonzero if runner infrastructure or any test fails.

Exit criteria:

- synthetic pass/fail scripts produce exact statuses and one record each;
- a failing test cannot suppress its timing record;
- ordinary test output remains visible.

### TA2. Remove nested suite execution

Status: complete

- [x] Remove the IF8 script invocation from
  `test_dependent_match_refinement.sh`.
- [x] Preserve dependent Match's own fixtures and assertions.
- [x] Keep IF8 as one independently discovered top-level test.
- [x] Add a static audit rejecting integration scripts that invoke another
  integration `test_*.sh`.
- [x] Search the entire prototype test tree for other nested test entrypoints.

Exit criteria:

- every integration script executes exactly once per full suite;
- IF8 coverage is unchanged but no longer duplicated.

### TA3. Add phase timing to expensive tests

Status: complete

Add optional phase records to:

- `test_dependent_match_refinement.sh`;
- `test_if8_fuel_free_quicksort.sh`;
- `test_hott_goal.sh`; and
- `test_artifact_flow.sh`.

Initial phase names:

| Test | Phases |
| --- | --- |
| dependent Match | source, publication, readback, negative, runtime |
| IF8 | source equality, negative proofs, publication, readback, artifact equality, determinism |
| HOTT | compile, execute/publish, readback, forgery, aggregate/link |
| artifact flow | build/kernel checks, core identity artifact, typed occurrence/nominality, proof replay, intrinsic/expectations, link/import, final representation |

- [x] Keep one checkpoint protocol in `test_support.sh`; each checkpoint closes
  the previous phase and opens the next one.
- [x] Let the runner close an open phase with the child test's actual failure or
  signal status, so `set -e` cannot suppress the failed phase record.
- [x] Emit a failed phase before propagating the suite failure.
- [x] Do not duplicate compiler execution solely to obtain a phase timing.

Artifact flow interleaves publication, readback, linking, and forgery checks
throughout one 3,000-line scenario. Its phases therefore follow real semantic
fixture groups rather than pretending those operations form contiguous global
stages.

Exit criteria:

- every test above explains its dominant internal cost without changing what it
  verifies.

### TA4. Make the runner authoritative

Status: complete

- [x] Change prototype `test-integration` to call the runner.
- [x] Route `test-type-infer-and-check` through the same timing contract or the
  same single-test runner path.
- [x] Update root and prototype README test commands.
- [x] Add the timing runner self-test to an appropriate non-recursive gate.
- [x] Keep direct execution of an individual integration script supported.

Exit criteria:

- documented full-suite commands cannot bypass per-test timing;
- each direct script remains usable for focused debugging.

### TA5. Run the audit and report findings

Status: complete

- [x] Clean-build `a.out` and `read_file.out`.
- [x] Run the runner self-test.
- [x] Run the full suite in fail-fast mode.
- [x] Run an isolated synthetic failure in `--keep-going` mode.
- [x] Confirm 36 discovered scripts and 36 timing records.
- [x] Confirm IF8 executes once.
- [x] Record total time and the ten slowest tests.
- [x] Compare total time before and after duplicate removal.
- [x] Separate source compilation, normalization, publication, and readback
  costs using phase records.
- [x] Update this dashboard with results.

Exit criteria:

- all semantic tests pass;
- timing output is complete and machine parseable;
- the 35-minute total can be attributed to named tests and phases rather than
  speculation.

## 6. Permanent Verification Matrix

| Check | Required result |
| --- | --- |
| zero-duration synthetic test | valid nonnegative timing record |
| ordinary pass | `status=pass exit=0` |
| ordinary failure | timing emitted and original exit preserved |
| interrupted child | signal status emitted and suite fails |
| fail-fast | no later test starts |
| keep-going | later tests run and final status fails |
| duplicate label | runner rejects suite |
| nested integration call | static audit rejects source |
| output passthrough | child stdout/stderr remains visible |
| lexical discovery | every `test_*.sh` runs exactly once |
| phase balance | unmatched phase is rejected |
| summary arithmetic | counts and duration bands match records |

## 7. Non-goals

This plan does not:

- parallelize tests;
- cache compiler results across semantically independent tests;
- remove artifact round-trip or forgery tests;
- add hard performance thresholds;
- optimize normalization, solving, or artifact replay;
- turn machine-dependent timing into artifact semantics; or
- modify accepted implementation outside `src/prototype/`.

After TA5, algorithmic performance work should receive a separate plan based
on the measured phase data.

## 8. Progress Dashboard

| Phase | Status | Commit | Result |
| --- | --- | --- | --- |
| TA0 baseline/contract | complete | working tree | 36 tests; expensive baselines recorded |
| TA1 runner | complete | working tree | self-test covers selection/pass/fail/signal/keep-going |
| TA2 nested-run removal | complete | working tree | IF8 no longer runs inside dependent Match |
| TA3 expensive-test phases | complete | working tree | four expensive scripts expose named phases |
| TA4 authoritative entrypoint | complete | working tree | Make targets and READMEs use the runner |
| TA5 audit/report | complete | final implementation commit | 36/36 pass; 36 records; IF8 once |

## 9. Final Audit Results

The fail-fast full suite completed on 2026-08-18 with:

```text
A_PROGRAM_TEST_SUMMARY 1 suite=integration total=36 executed=36 passed=36 failed=0 skipped=0 wall_ms=1155925
A_PROGRAM_TEST_BANDS 1 suite=integration fast=14 normal=18 slow=3 very_slow=1
```

Exactly 36 top-level timing records and 23 phase records were emitted. IF8 has
exactly one top-level record.

The ten slowest tests were:

| Test | Wall time (ms) |
| --- | ---: |
| `test_if8_fuel_free_quicksort` | 988,723 |
| `test_hott_goal` | 54,793 |
| `test_artifact_flow` | 18,315 |
| `test_type_infer_and_check_examples` | 11,559 |
| `test_list_map_induction` | 9,773 |
| `test_explicit_index_family_surface` | 9,287 |
| `test_dependent_match_refinement` | 5,516 |
| `test_context_category` | 4,607 |
| `test_outer_ih_insertion_sort` | 4,516 |
| `test_dimension_term` | 4,302 |

IF8 phase attribution was:

| Phase | Wall time (ms) |
| --- | ---: |
| negative proofs | 318,084 |
| source equality | 205,492 |
| publication | 199,084 |
| determinism | 192,455 |
| artifact equality | 63,736 |
| readback | 9,853 |

Artifact flow took 18,315 ms. Its largest phases were build/kernel checks at
11,636 ms and final representation at 3,794 ms; proof replay took 1,099 ms.
HOTT took 54,793 ms, of which execute/publication took 49,614 ms. Dependent
Match took 5,516 ms after removal of its nested IF8 invocation.

The old suite baseline was 2,133,264 ms. The audited suite took 1,155,925 ms, a
reduction of 977,339 ms (45.8%). Machine load differs between runs, so this is
not a benchmark claim. It does confirm that the duplicate IF8 execution, not
artifact readback alone, caused most of the previous 35-minute total.

## 10. Privacy Review

This document contains repository-relative paths and local timing results only.
It contains no credentials, tokens, private URLs, personal names, email
addresses, or absolute home-directory paths.
