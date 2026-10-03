# C Backend Callback Modules

Date: 2026-10-04. Separate C15 candidate after frozen C14; Goal remains active.

## Problem List

1. Verify the borrowed callback ABI with independently generated ordinary C
   modules and mixed products, rather than only inline C callback implementations.

## 1. Ordinary Module Composition

### Subjective (User)

2026-10-03, English paraphrase: prioritize usable downstream C modules from `.a`
and LinkerScript, keep implementation bounded and prototype-only, and leave source
authority unchanged. The [owning Goal](2026-10-03-C-BACKEND-GOAL.md) records the
direct human workflow allowing independent epochs and Core-only Main integration.

### Objective (Code)

C14's immutable23 is published as task `e9d74f70` and prototype Main `6c36dcb`,
per Root's report. Current E12 callback/shared/link/I/O/import controls pass;
explicit imports resolve the historical consumer setup error without changing
output/fuel or frontend policy. Its four product clients implement callbacks
directly in C.
The separate scalar ABI can supply equivalent functions through a status/output
adapter with valid scalar inputs. Both ABIs use exact Int32/Int64 widths; the
callback header shares guarded structure declarations across components.

Fresh C15 on task `e9d74f70` plus lane-local test/docs: serial O2 and client/source
ASan/UBSan/leak gates both exit 0 on qualified E8. Each has 99 expected status
rows (96 zero, three duplicate-link refusals), 32 linked and eight loaded-provider
client runs. Each client compares two consumer results with 400 Core cases each
and 20 source observations. Both serialized image hashes and all output checks
match. No emitter/producer rebuild or broader/timing workload was run.

### Assessment

Agent choice within existing scope: test a scalar provider and two independently
emitted callback consumers with explicit distinct aliases. Caller-owned immutable
offset contexts adapt generated scalar calls into the synchronous callback
signatures. The provider must return success for valid calls; this does not add
callback failure propagation, escaping closure ownership or source binding policy.
Loaded code must outlive every callback call. No producer/emitter/schema change
is needed; preserve C14 submitted bytes. This advances ordinary C use in AP5.6/
AP6.2, not boxed/nominal exchange or native Acc/QuickSort completion.

Root operational scheduling, 2026-10-03 UTC/2026-10-04 local: proposed Fold-only
20:20-20:30 UTC matched36 window is not a worker launch grant. C15 O2/SAN are
terminal at the 20:06 safe boundary with no live heavy children. Light handoff
work continues; keep heavy work drained only for the specified window/release.

### Plan

- [x] Admit the scalar provider and reuse exact admitted C14 consumer inputs.
- [x] Verify two consumers with each source/object/archive/shared provider pair,
  both header orders, same-width context adapters and source/Core comparisons.
- [x] Verify focused serial O2/client/source ASan/UBSan/leak controls, output
  preservation, source/object/archive duplicate-symbol refusal and code lifetime.
- [x] Pin all inputs, tools, commands/results and actual instrumentation limits.
- [x] Freeze a distinct reviewable epoch and report through the existing outbox;
  reconcile the single owning issue table without changing historical evidence.
- [ ] Root task publication/current-producer review and Main integration remain
  separate. Exact files/pins are in the [handoff](2026-10-04-C-BACKEND-EPOCH15-HANDOFF.md).
