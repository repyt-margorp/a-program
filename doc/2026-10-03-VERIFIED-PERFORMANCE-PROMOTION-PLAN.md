# Verified Performance Promotion

Date: 2026-10-03. Status: historical epoch `7631e5a` complete; new approved stateless-frame deletion promotion in progress on 2026-10-04.
Related: [central schedule](2026-10-03-PARALLEL-CORE-BACKEND-PERFORMANCE-PLAN.md),
Issues #56/#51. This is a separate promotion epoch and its sole work list.

## Problem List

1. Promote the verified speed improvements and necessary dependencies into the
   accepted implementation while preserving unrelated local work.
2. Promote the newly approved redundant Reduction/frame deletion against accepted
   Job94a20003, with required dependency and save-byte behavior qualification.

## 1. Accepted Performance Change

### Subjective (User)

2026-10-03, English translation of explicit human approval relayed by inquiry
desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: the speed improvements may be
promoted from Prototype into `src/`. Source: the desk's message explicitly
labelled new human approval, quoting the user's original Japanese instruction.
Merge alone owns Main. Select verified changes and necessary dependencies,
compare accepted code, preserve unrelated dirty files/fixtures, update required
tests/docs, verify accepted regressions/parity and commit/push a separate epoch.
Do not blanket-adopt unrelated Surface/C/design work or unverified trials,
waive strict resume failures, or claim a universal speedup.

### Objective (Code)

At receipt, local Main `b33279e` includes reviewed prototype C7 and PR #54;
last pushed Main is `517f05a`. Accepted semantic baseline remains `eb0aad6`.
Unrelated accepted edits in evidence.c/h, iadt.c/h and tests/core.c/iadt.c,
the protected priority plan and two user fixtures have been preserved with
unchanged raw hashes. The existing supervision Goal is active; its original
no-promotion wording is superseded by this explicit approval for selected
performance work only.

Fresh E6 matched baseline/head measurement evidence has 30 passing samples,
three repetitions per variant, exact source/tool/binary pins and no timeouts.
AP trees400 median wall 17.235 -> 5.407 seconds and wrapped-command peak RSS
2971260 -> 1891724 KiB. QuickSort ranges overlap; tiny List includes startup.
Both measured variants share the E6/family/Surface producer; those prototype
dependencies are not automatically authorized for wholesale adoption. Accepted
baseline applicability and the minimal dependency set remain to be inspected.

2026-10-03, fresh advisory and code review at accepted Main `99efcf7`: exact
measured delta `5fed3e37` applies without fuzz to committed computation.c,
eval.c/h and iadt.c (+137/-12). It uses existing closure/environment, application,
materialization and immutable descriptor interfaces; no required Job/Surface/
family/C dependency is identified. Main's unrelated iadt.c schema-relocation
addition is 33 lines in a separate function and remains excluded. The two Core
physical-policy observers and Identity descriptor observer need explicit test
migration; independent head/direct/total-result/cleanup tests already exist in
the prototype. Accepted compilation and behavior are verified below.

2026-10-03, Root fresh accepted qualification: isolated candidate `a7a8bc3`
passes all 386 recorded O2 recipes and eight affected ASan/UBSan/leak command
groups after ten instrumented binaries build. Default direct/head/TotalResult/
cleanup tests cover all raw and fresh-process cuts and charged final readback.
Original candidate Core/Identity observers fail -6/134 and remain recorded;
clean original accepted controls both pass. Explicit test migrations retain all
assertions, preserve legacy materialized-Force coverage and independently verify
the default captured-head policy. Runtime sources are only the measured four
files (+137/-12); no additional accepted runtime dependency is required.

Accepted Main commit `7631e5a099f79bd33aa41d0dc3263c06c92fe244` contains exactly
those thirteen tested runtime/test/build blobs. Reverse application of the
approved patch to live protected files restores all nine original hashes.
Unrelated Core/IADT/evidence edits and fixtures remain outside the index. Private
prototype assembly after promotion matches all 128 qualified E8 runtime files;
its promoted head gate passes after correcting the retained initial link failure.
Four prototype helper/document files recognize exact accepted patches once and
add only private codec/synthesis link dependencies. Frozen historical evidence
is unchanged. [Promotion evidence](../src/prototype/performance_promotion/verification/accepted-20261003.json)
pins candidate/source/logs, original failures, controls and preservation.

Main push `e14856f` succeeded and the remote independently matches. This
completes the selected promotion, not the broader #56/performance Goal.

### Assessment

Agent decision within the new approval: finish publication of already verified
prototype epochs separately, then use an isolated accepted-code review checkout
to select performance changes and inspect dependencies. Preserve dirty Main
files without staging them. Resolve any promotion overlap against their actual
content; do not silently accept unverified local trials or overwrite user work.
Accepted full regression and affected-layer checks must establish the promoted
scope; prototype timing alone does not establish accepted parity.

Agent scope decision: select only the four measured runtime files, with explicit
observer migrations and independent normative tests needed for their accepted
contract. Keep original observer failures as historical evidence and preserve
legacy materialized-demand assertions alongside default-head policy checks.
Use a clean isolated accepted checkout; verify reverse application of the
approved promotion restores each protected working file's original bytes.
Joint E6 timings remain qualified joint-producer measurements; accepted-only
performance is unmeasured until a separate agreed slot.

### Plan

- [x] Record explicit promotion approval and its limits before investigation.
- [x] Compare verified performance changes with committed accepted code and
  identify the minimal justified dependency set and local-edit overlaps.
- [x] Implement the selected promotion in an isolated review checkout; update
  meaningful accepted regression tests/docs only as needed.
- [x] Run affected checks and accepted full regression/parity; preserve actual
  failures and producer-specific strict-resume observations.
- [x] Review the exact diff, preserve all unrelated dirty bytes, publish the
  separate accepted epoch and report issue-linked scope and verification.
- Completion: the approved performance scope is accepted and verified, with
  necessary dependencies explicit and unrelated local work preserved.


## 2. Stateless Frame Deletion Promotion

### Subjective (User)

2026-10-04 12:20 UTC, English translation of explicit human approval relayed by
inquiry desk thread `019ebfae-06be-7b71-974a-b97505daed4a`: the performance work
appears to remove unnecessary Reduction and improve speed; move those verified
improvements into Accepted. The original Japanese is retained in the
[approval receipt](../src/prototype/coordination/inbox/performance-accepted-promotion-approval-20261004.json).
This supersedes the former no-promotion boundary for selected verified
performance deletions and necessary dependencies. Reported speed improvement is
the user's observation; current matched RSS/time remains evidence to verify.

### Objective (Code)

Approval received at Main c205d507 after accepted Job94a20003. Main's protected
nine files and empty index are preserved. Inquiry desk's two documentation
edits are separately backed up and copied into the isolated promotion checkout.
Candidate c2063542 changes eval.c/h, eval_internal.h and eval_io.c only; all 116
other accepted runtime files/two Makefiles are exact 94a. Latest source55/cost6
freeze is a review/request, not runtime READY. Prior Root raw review records
candidate own inert0/328, parent36/328 and paired20/164 byte failures unwaived.
Six completion fuels and current consumer/public evidence are newly supplied
worker reports awaiting this Root review. No matched cost sample is available.

2026-10-04 12:58 UTC, fresh Root exact c206 runtime120 qualification: accepted
check/head/host/execution0; affected head/frame/evalIO/IdentityIO SAN0; SAN Core,
IADT, Synthesis, full SourceIO, normalization and image CLI0. Four new independent
normative tests preserve old assertions and cover reentry/inline-state lifetimes,
stateless cleanup/fallback/failure and invalid readback ordinal/count/wire bounds.
Fresh six O2 completion fuels equal accepted parent, including local-sorted956507.
Public12 paired images/24 inert/24 resume byte/verdict/fuel match; all52 partition
images/full40 TSV equal parent with original strict3/exit1 retained. Fresh raw1152
semantic/fuel/zero-step/SAN commands0, candidate328 inert0; paired20/164 remain
failed. No full check-acceptance or current matched time/RSS claim. See the
[Root promotion review](../src/prototype/performance_promotion/verification/accepted-20261004/review.json).

### Assessment

Root implementation decision within explicit approval: select the four-file
frame-reuse/order candidate because the isolated two-file trial70ad5682 retains
raw15 paired/29 inert byte failures. Root independently recomputed all100 existing
failed artifact observations: identical DAG bodies/counts/steps/flags, differing
entry/configuration/footer permutations. This bounded explanation does not waive
any byte failure or establish every historical cause.

2026-10-04 13:03 UTC correction: WITHDRAW the earlier public-materialized957
counterexample rationale. Fresh two-file O2 build and three explicit argv controls
show same-profile materialized resave is exact; switching to compact reproduces
the earlier31696->27014 artifact pair. The earlier original argv was not retained,
so its exact invocation is unknown. Keep the failed-helper setup and original
artifacts/claim with the [correction](../src/prototype/performance_promotion/verification/accepted-20261004/two-file-profile-correction/correction.json).
Use the valid raw-byte dependency evidence, not the withdrawn public rationale.

Use exact frozen c2063542 eval.c/h, eval_internal.h and eval_io.c: recycle only
stateless returned head frames, preserve opaque/materialized/fallback lifetimes,
and enumerate reusable readback entries in creation/received-wire order. No
new wire record or required cache deletion. Private entry metadata adds8*n bytes
to loader arrays/writer scratch; actual cost must include it. Root will freshly
qualify all four files and independent lifetime/malformed-ordinal controls.
Original parent36/paired20 raw byte failures and strict3 remain unwaived; full
raw-byte READY/full51/56 and measured RSS/time are separate criteria. No unrelated
optimization/checkpoint/Surface/C trial or Main dirty implementation is selected.

### Plan

- [x] Record explicit human approval before further implementation investigation
  and deliver it to the original Performance owner's existing inbox.
- [x] Verify source55/cost6/all5868 private records/current correctness evidence;
  exclude the isolated two-file trial on confirmed raw-byte failures and
  select the demonstrated four-file deletion/order dependency.
- [x] Resolve or isolate failing pieces within the authorized scope and prepare
  the minimal accepted diff with necessary tests.
- [x] Run accepted regressions, fresh affected sanitizer/fuel/resume/profile
  checks and verify protected local edits are unchanged.
- [ ] Commit/push intentional accepted promotion; report exact scope, verification
  and limitations to the inquiry desk. Actual RSS/time remains a distinct
  matched exclusive experiment, with retained censored rows and no universal gain.
