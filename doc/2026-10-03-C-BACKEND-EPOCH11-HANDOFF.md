# C Backend Epoch11 Handoff

Date: 2026-10-03
Status: focused publication-I/O epoch verified; exact snapshot ready for Root.
Branch: `parallel/c-backend-20261003`.
Parent: `fe497956c3d0031a0db0c98a13db5c464fcd2a8b`.
Chosen message: `prototype: classify C publication stream failures as I/O`.

## Problem List

1. Preserve the public I/O/unsupported status distinction during C publication.

## 1. Publication Stream Errors

### Subjective (User)

2026-10-03, concise English paraphrase of the direct human clarification via
inquiry desk `019ebfae`, recorded in the [Goal](2026-10-03-C-BACKEND-GOAL.md):
implementation lanes may proceed without waiting for Merge; Merge resolves
integration conflicts. Preserve submitted evidence and shared Git/Main boundaries;
coordinate exclusive benchmark/shared-resource runs. The existing human scope is
bounded downstream `.a`/LinkerScript ordinary-C usability, with explicit unsupported
contracts and no source erasure authority. Task-branch publication is authorized;
Merge owns Main. Native Acc/QuickSort remains unfinished.

### Objective (Code)

README's existing target statuses distinguish I/O failure (2) from unsupported
lowering (4). The pinned qualified C9/E8 backend wrongly returns 4 for an immediate
source/header/direct-output FILE error. Test-only `LD_PRELOAD` interposition uses
unbuffered `/dev/full`; it checks actual `ferror` before close. Buffered streams
separately prove close-only errors. The same admitted saved image is used before
and after, without changing producer/checking/erasure.

`main.c` and `link/driver.c` now retain the emission result, save `ferror` before
closing all handles, and classify stream/close failures as 2 before considering
unsupported status 4. Direct output prints a write-error diagnostic. Existing
temporary-file/directory cleanup and atomic rename paths are retained.

Focused results, all terminal:

- Backend-only `make -j1`, strict C11/Wall/Wextra/Werror/O2: exit 0. The existing
  assembled runtime matches all 128 exact Root C9/E8 source hashes before/after.
- Nineteen runner status assertions pass before the fix with expected immediate
  status 4, and nineteen pass after with expected status 2. Native/structural
  source and header errors, direct output, close-only and receipt errors are
  exercised. Checked/trusted callback refusal stays 4. Failed products/staging
  paths are absent; direct prior bytes and an existing product remain unchanged.
- Eighteen ordinary-C O2 combinations and eighteen client ASan/UBSan/leak
  combinations pass, using the unchanged C10 NumbersNat/ChoiceNat client.
  Duplicate default helper symbols still refuse in two required link controls;
  shared-arena rollback, invalid tags, depth, recovery and destructor lifetime pass.
- Five native/structural/direct C/header baseline files match before/after byte
  for byte. Twelve generated alias C/header files match retained qualified C10
  products. Forty-one qualified C10 inputs and runtime128 remain unchanged.
- Sixty-two client backend/cc/ar argv/exit records are retained: sixty exit 0 and
  two required duplicate-symbol links exit 1. The 36 client run statuses are
  independently extracted from complete shell traces. No active failure or
  relaxed outcome. The old status-4 reproduction remains evidence, not a gate
  failure under its explicit pre-fix expectation.

Qualified producer: `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`.
Runtime128 manifest: `9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
Reused image: `9a40589f1a5c2e1f7838dca83739018621fb24eee48d600688d5fe5df905397b`.
Original backend: `6fd5cc956cddf25a3a978a23e488e688a5c14f8d0226b58b14790453952aa362`.
Candidate O2 backend: `2479621141bdf635e3a230ccfe13579c505f2b3b7816eed482f9cd44374ac6c3`.
No producer rebuild, E9/E10 composition, broad suite or timing was run.

Retained evidence: `/tmp/a-program-c-backend-epoch11-io-20261003`.

| Evidence | SHA256 |
| --- | --- |
| `verification.json` | `e248639df52fe3297e345f365f2ef2054bbf3b548ab176290bb67fd2347be4d7` |
| `qualified-inputs.sha256` | `4a0a305c4764616a959d81dce17b06171d9a8b9cea7c13a1e5e806d5ab6f6f19` |
| `o2.log` | `2fdc95e8903f441cd7888931757d9cb638a71e50c3077095a9e9a76b5bfc1735` |
| `san.log` | `07f0867c75c001ddeef00260abe184dd01705a56843783b52c377f8aa58d5e4a` |

`verification.json` pins all 38 runner assertions, 36 client statuses, 62 tool
records, code/input/binary hashes and sanitizer limits. `build-command.json`,
`build-status.json`, `build.log`, runner command/status files and client traces
retain commands/exits. `epoch11-files.sha256` and `epoch11-freeze.json` pin the
exact snapshot; `run-files.sha256` pins retained generated products/binaries/logs.

### Assessment

Root operational steering at 11:59/12:03 UTC chooses this minimal first epoch,
permits serial focused correctness and releases live C10 files after isolating
their exact submitted bytes. Pinning, runner design and snapshot handoff are agent
workflow decisions, separate from the human coordination preference.

The fault helper is Linux test infrastructure (`LD_PRELOAD`, `/dev/full`, libdl),
not a dependency of emitted C. Sanitizers instrument clients and source component
inputs at O1; the backend, producer and driver-built object/archive bodies remain
O2. This is not full compiler/object/archive sanitizer coverage. Common publication
handling is corrected, but the focused positive profiles are native and structural;
no new scalar lowering capability is claimed.

No emitter, producer, schema, source erasure policy, accepted code or build recipe
changes. Existing callable/indexed Acc/QuickSort, effects, higher Identity,
recursive captures and broader nominal contracts remain explicit limits. The
already-selected finite value-record/single-tail List proposal is a separate next
candidate in the [next-boundaries note](2026-10-03-C-BACKEND-NEXT-BOUNDARIES-NOTE.md).
No shared-owner dependency or integration blocker was found.

Exact seven-file snapshot:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-NEXT-BOUNDARIES-NOTE.md
doc/2026-10-03-C-BACKEND-EPOCH11-HANDOFF.md
src/prototype/c_backend/main.c
src/prototype/c_backend/link/driver.c
src/prototype/c_backend/link/publication_io_fault.c
src/prototype/c_backend/link/publication_io_check.sh
```

### Plan

- [x] Record the human workflow change immediately in the active plans.
- [x] Reproduce the pinned I/O classification before implementation.
- [x] Correct only publication status precedence; preserve unsupported contracts.
- [x] Run serial focused O2/client sanitizers and cleanup/prior-output controls.
- [x] Check tabs, English comments/docs, shell syntax and exact submitted bytes.
- [x] Pin seven files and retained evidence for delegated task publication.
- [ ] Root publishes/reviews/integrates this exact task snapshot separately.
- [ ] Continue the active Goal with a distinct bounded candidate; no issue closure,
  accepted C promotion or native Acc/QuickSort completion is implied.
