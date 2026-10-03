# C Backend Next Bounded C-Client Boundaries

Date: 2026-10-03
Status: publication-I/O correction and focused controls terminal; separate C11.
Baseline: task `fe497956c3d0031a0db0c98a13db5c464fcd2a8b`; source audit at C9
`60c100c9d06b314eaeb024462e8ce0e3f2da5bab`; historical C10 six-file freeze
`01c927fcbc221f6b0b6c7b74c300983c7bd34fe1ba1aa832569a9df4e78b4828` unchanged.
Owning work list/table remains in the [Goal](2026-10-03-C-BACKEND-GOAL.md).

## Problem List

1. Assess a bounded native single-tail List of selected value records and its
   ordinary C array boundary.
2. Check the public distinction between output I/O failure and unsupported lowering.

## 1. Value-Record List Boundary

### Subjective (User)

2026-10-03, concise English paraphrase of the direct human workflow change via
inquiry desk `019ebfae`: each implementation lane may continue without waiting
for Merge; Merge resolves integration conflicts. This supersedes blanket review
holds. Continue a distinct prototype epoch within the existing Goal/scope while
preserving submitted C10 bytes and shared Git/Main. Native Acc/QuickSort remains
unfinished; invent no erasure policy. Coordinate truly exclusive benchmark or
shared-resource runs with Merge, and report consumed instructions, concrete work
and real blockers rather than only queued receipt. The owning Goal receives this
note after its six-file publication freeze is released. Root released live files
at 12:03 UTC after copying/verifying the immutable submitted snapshot; the human
workflow clarification is now also recorded in the owning Goal.

2026-10-03, concise English paraphrase of the recorded human priority: make
ordinary C modules readily usable through `.a` and LinkerScript, with bounded
downstream work and explicit unsupported contracts. The backend does not own
source semantics. No new container design or scope expansion is attributed to
the user; the candidate choice below is the agent's assessment.

### Objective (Code)

Fresh source inspection at the stated task revision:
`fixtures/nested_records.p` already declares
`Recursive := @{nil : *; cons : Envelope -> * -> *;};`, with previously selected
finite Packet/Envelope records. `nested_record_check.sh` retains its native
status-4 selection control. Root's historical current-E8 thirteen-gate evidence
includes the value-record gate at exit 0; this turn does not rerun admission or
infer native support from that historical result.

`lower/representation.c:151` explicitly excludes non-natural value-data payloads
from recursive nodes. `pg_c_representation_list` independently excludes those
payloads. `lower/nodes.c` currently validates only a scalar/enum/Nat32 node payload;
merely removing the guards would miss nested active-field validation.
`lower/scalar.c:835` already emits recursive finite value-record validators in
prior selection order, but currently emits them after node helpers.

Inspected implementation SHA256 pins: `representation.c`
`a74fc1f14e1a72e294b15d6e273aeb905b82ebc3c2481bba008cdbd825d7753e`;
`nodes.c` `7adb637bb2751e808d31efa571eb664e169fc6018cb00ed7432c47068a384243`;
`scalar.c` `fe0ea3fe8dad968101bcd450d4cb4a402c8e54cb5e97d7945e4ce4c821b441b9`.
No source test, raw probe, emitter edit or build has run for this candidate.

### Assessment

Agent separate next candidate, after the focused I/O epoch:
support only an already selected, complete, nonrecursive value-record payload
plus the existing single direct Self tail. Reuse the existing value validator
for node and array inputs before allocation, and retain two-pass finite copy-out
so failures preserve the buffer/written count. Emit validators before any helper
that calls them. C struct assignment copies the existing value representation;
no new source nominal equality, producer field or private checker is required.

The former `Recursive` control must remain visible as an explicit newly positive
case if implemented. Add separate retained refusals for recursive aggregate
payloads, multiple tails, callable/indexed fields and missing/later selections;
do not delete negative coverage. Native Acc/QuickSort remains unsupported.
This is a target representation proposal, not a demonstrated producer gap.

### Plan

- [x] Identify the existing source shape and exact target guards/validator order.
- [ ] After release, prove ordinary admission and the current target refusal with
  pinned inputs before adopting any candidate change.
- [ ] Verify actual source recursive Match plus ordinary C source/object/archive
  clients over record arrays, active nested tags, cycles, allocation rollback,
  depth/capacity/output preservation and source differential observations.
- [ ] Preserve the former control explicitly and all remaining refusal contracts.
- [ ] Hand off only a verified bounded epoch; route concrete shared needs to Root.

## 2. Publication I/O Classification

### Subjective (User)

The 2026-10-03 human workflow change recorded in Problem 1 also governs this
independent epoch: continue implementation without waiting for Merge while
preserving C10's exact submitted files and the existing downstream authority.

2026-10-03, English paraphrase of the same recorded human C-usability priority:
usable source/object/library products should expose their concrete limitations
and failures. Specific exit-code precedence is an existing target contract;
the fault-injection choice below is an agent proposal, not a direct user quote.

### Objective (Code)

At the same task revision, README documents exit 2 for input/I/O failure and
exit 4 for unsupported lowering. `emit.c` and `lower/scalar.c` return failure
when `ferror` is set. `link/driver.c:198` assigns status 4 to every emission
failure before checking close failures; `main.c:36` similarly assigns 4 before
closing a failed direct emission stream. Thus a stream-error branch reaches the
unsupported status path. This is a pinned source-path observation, not a freshly
reproduced runtime failure or evidence that publication cleanup is broken.

Inspected SHA256 pins: `main.c`
`a8b9d0fa6a15ae2aa3a12cda957a4644a28350d0f94179541cf9163a613fa81d`;
`link/driver.c` `19224076ea4be7bfa617d9637aadadafad618162a6bfaab70084e05ab4190a88`.
Fresh reproduction after the release uses the exact qualified C9/E8 backend
SHA256 `6fd5cc956cddf25a3a978a23e488e688a5c14f8d0226b58b14790453952aa362`
and retained C10 image `9a40589f1a5c2e1f7838dca83739018621fb24eee48d600688d5fe5df905397b`.
Unbuffered `/dev/full` interposition proves status 4 for native source/header,
structural source/header and direct output. Buffered close-only and receipt
failures already return 2. Checked/trusted callback refusal stays 4. Failed
publications leave no product/staging path and preserve prior direct output.
The focused runner passes before the fix with expected immediate status 4 and
after it with expected status 2; no failure cleanup regression is observed.

### Assessment

Root operational release, 2026-10-03 11:59 UTC: C10 evidence review is done;
only its exact six paths remain frozen while Root publishes/integrates. First
reproduce this pinned I/O classification path; if proved, make the minimal
stream-error/status correction and run serial focused O2/affected-client
sanitizer controls for unsupported status, atomic cleanup and prior output.
No producer/schema/erasure change, broad suite or timing. The value-record List
proposal stays separate; ordinary admission/refusal setup may be inspected.
Root's 12:03 UTC update releases live files after isolating the exact submitted
six-file snapshot. Focused correctness at j1 may overlap other lanes; publication
is not a blocker. This is operational steering, separate from the human workflow.

Consumed agent action: prepare a stream-fault shim and exercise the retained
qualified backend before touching implementation. Distinguish stream errors
before closing FILE handles only after reproduction, while preserving atomic
cleanup and prior output. Keep native-tool/receipt errors and genuine unsupported
lowering at their existing target statuses. This is a separate next epoch.

### Plan

- [x] Identify the concrete status branches and separate them from cleanup claims.
- [x] After scheduled release, reproduce source/header/direct-output failures
  without a producer/checker change; retain unsupported status 4 and no product.
- [x] Fix only the proved target I/O distinction: save `ferror` before closing
  handles and prefer stream/close I/O status 2 over emitter status 4.
- [x] Finish affected ordinary-client O2/sanitizer controls: eighteen combinations
  per phase pass; two default-symbol links refuse as required. Generated native
  C/header bytes are unchanged, and all 41 qualified inputs/runtime128 remain
  unchanged. Exact pins and limits are in the
  [C11 handoff](2026-10-03-C-BACKEND-EPOCH11-HANDOFF.md).

Superseding workflow: C10's submitted snapshot remains immutable; live owned
prototype work proceeds without waiting for publication. Consume the bounded
I/O release above; coordinate exclusive measurement/shared resources separately.
This note adds no second issue table/new Goal and changes no frozen C10 path.
