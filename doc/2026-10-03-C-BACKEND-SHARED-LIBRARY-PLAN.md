# Ordinary C Shared-Library Product

Date: 2026-10-03
Status: bounded C13 implementation verified; exact handoff ready for Root.
Baseline: exact C12 task `97f8d7c28f3438fb1b36d687e0e7efc175d8c6f3`.
Owning Goal and sole issue table: [C Backend Goal](2026-10-03-C-BACKEND-GOAL.md).

## Problem List

1. Add a shared-library product using the existing chosen C ABI and explicit
   selected-public-symbol visibility.

## 1. C Shared Modules

### Subjective (User)

2026-10-03, English paraphrase of the recorded human priority: `.a`/LinkerScript
should produce modules readily usable from ordinary C code, with bounded
downstream work and explicit unsupported contracts. The backend owns no source
semantics. The direct workflow clarification via desk `019ebfae` lets lanes
continue without waiting for Merge, which resolves integration conflicts.
Preserve submitted snapshots/shared Git/Main boundaries and coordinate exclusive
benchmark resources. Native Acc/QuickSort remains unfinished; invent no erasure
policy. No shared-product spelling or visibility convention is attributed to
the user.

### Objective (Code)

Fresh inspection at the baseline plus C12: `link/plan` accepts source/object/
archive/executable products only; `driver.c` uses four corresponding receipt
names. AP5.6 explicitly leaves shared-library visibility/products open. Existing
headers already expose isolated, scalar and native C contracts; native data/array
helpers are ordinary C functions. Public aliases are validated identifiers.
No new producer field or foreign callable interpretation is needed for a product
using those existing interfaces. Callable Pi/owned-handle policy remains separate.

Fresh C13 at the exact C12 parent plus owned edits: strict backend-only O2 build,
shared O2/client ASan/UBSan/leak gates, affected existing LinkerScript gate and all
nineteen I/O controls pass. Each shared phase has 58 expected/actual status rows
(45 success, eleven I/O/profile status 2, two checked/trusted callback status 4).
Four linked and four dlopen clients run, plus eight repeated/trusted dlopen calls.
Exact dynamic definitions are 3 scalar, 4 isolated, 17 record-List and 8 applied
Nat-List public functions; runtime/private definitions are hidden. Whole record
fields, Nat32 maximum, arithmetic wrapping, copy capacity, active tags, cycles,
arena rollback/depth/lifetime and isolated output-error recovery pass.

Producer remains qualified E8 `50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`;
all runtime128 hashes match Root's qualified snapshot. This is not worker E9+E10
qualification. Root separately reports C12's fourteen current E9+E10 O2/three
affected sanitizer gates and nineteen I/O controls pass, with task `97f8d7c`
pushed and prototype Main integration complete. Historical C12 snapshot stays exact.

Retained C13 evidence: `/tmp/a-program-c-backend-epoch13-shared-20261003`;
`verification.json` SHA256
`757e27e3fa051d7ef82b6d3317bea6e4cf7ff8ac3d7eab4386e7796b016ad5a7`.
Exact source/test/docs, hashes, failed historical harness runs and limitations:
[C13 handoff](2026-10-03-C-BACKEND-EPOCH13-HANDOFF.md).

### Assessment

2026-10-03, Root operational receipt of the disposition made within the
human-authorized issue audit: open #61 supersedes #44/#49, with source-semantic/native-target
residuals and the same C owner. Historical milestones remain; no new erasure or
representation authority follows. This bounded product work routes to #61.

Agent choice: add explicit `product shared`, with no executable entry and no
new native-layout-script combination. Compile component/required runtime with
PIC and link `library.so` with an anonymous ELF version script. Whitelist selected
`ap_export_ALIAS` symbols plus native helper names for selected aliases; mark
other definitions local. Unmatched helper names create no exported definitions.
Derive names from the existing ABI/plan; do not reconstruct representations.

The [GNU ld VERSION documentation](https://sourceware.org/binutils/docs/ld/VERSION.html)
defines anonymous symbol-visibility maps; the
[GCC link options](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html) describe
shared/PIC compilation. This product requires a host toolchain supporting those
ELF options; tool failure remains I/O status 2 with staged cleanup. It adds no
platform-neutral dynamic loader or serialized target configuration to `.a`.

Test actual ordinary linked and dlopen clients for native scalar/container and
structural exports; verify exact defined dynamic symbols, hidden runtime/private
symbols, lifetime, repeated calls, failure/resource boundaries and input-image
immutability. Library handles must outlive calls and destructor use. Keep existing
entry/layout restrictions, refusal and C11 atomic/error controls explicit.

Two initial harness errors are corrected without producer/emitter changes:
checked structural admission can change the selected Core graph, so trusted
behavior is checked by running its library rather than assuming C byte equality;
Nat32 is the existing `uint32_t` API, not an invented struct. The failed logs
remain. Repeated checked C/header/maps are identical, native trusted C is also
identical, and trusted structural headers/maps/behavior match. O2 and sanitizer
phases produce byte-identical C/headers/objects/libraries/maps on this host.

Sanitizers instrument ordinary clients only; shared-library bodies, backend and
producer are O2. Allocation-capacity status/rollback is tested; this gate does not
interpose every shared-body malloc position. No install/SONAME/version policy,
portable loader, general callable/boxed ABI or native sorter is claimed.

### Plan

- [x] Identify the AP5.6 product/visibility obligation and current code gap.
- [x] Implement only explicit shared product, PIC/link/visibility and receipt.
- [x] Verify O2 and affected client sanitizers, linked/dlopen calls and symbols.
- [x] Check failure cleanup/prior products, unsupported profiles and entry rules.
- [x] Preserve C12's immutable snapshot; update the sole issue table at handoff.
- [x] Pin source/test/docs evidence in a separate C13 snapshot and handoff.
- [ ] Root task publication/current-producer review/Main integration are separate.
- [ ] Continue the active bounded #61 Goal; wider contracts remain unfinished.
