# Native Transitive Static Captures

Date: 2026-10-03
Status: C8 implemented/locally verified; frozen for delegated task publication.
Baseline: published task C7 `d275b75d246d9795d798ee7771519c0febc6c9bb`.
Related: [owning Goal](2026-10-03-C-BACKEND-GOAL.md), AP6.3/#49,
[Merge routing note](2026-10-03-C-BACKEND-MERGE-ROUTING-NOTE.md).

## Problem List

1. Preserve native captures through a chain of statically known local functions.

## 1. Transitive Static Captures

### Subjective (User)

2026-10-03, English paraphrase of the direct human scope clarification, recorded
in the owning Goal: emit `.a`/LinkerScript-derived C readily usable from ordinary
C modules, keep the backend downstream and avoid excessive scope expansion.
Task-branch publication is authorized; the later human workflow replacement
assigns coordination/publication/Main integration to Merge. Existing Goal and
frozen handoffs continue.

### Objective (Code)

Final active C8, 2026-10-03 at `d275b75` plus the exact source manifest:
all twelve O2 gates and nine generated-client ASan/UBSan/leak gates are terminal
exit 0, sequential `-j1`. The active lowerer exactly matches the private trial
SHA256 below. The existing static Oracle's 20 raw comparisons remain; its exact
`nested_three` source/client control is positive. The new gate contributes 300
raw evaluator comparisons, 600 Int32/seven Int64 cases per source/object/archive
product and six source observations. Existing static clients record 360 Int32/
five Int64 cases/product and nine source observations. These are gate counts,
not inferred independent property counts. Native Acc/QuickSort still refuses.
Final source manifest SHA256
`f114d8d31c94803d972a0419f69d23615d654b3966a3a7e10a1d72e20ad9afff`;
full files/pins/failures/limits: [C8 handoff](2026-10-03-C-BACKEND-EPOCH8-HANDOFF.md).
Task publication/current-producer joint verification/Main integration are pending.

Fresh read-only inspection at `cea1dc0` plus frozen C7: `lower/scalar.c:captures`
selects free binders of the callee, then binders of a directly used delayed body
and its recursive target. It does not follow another delayed body reached through
the first. `callee` later rebinds captured delayed bodies into the chosen native
environment. The current static fixture's `nested_three` remains an explicit
status-4/no-product refusal; its historical C5 evidence is preserved.
This inspection identifies a target implementation boundary, not a producer bug.

C7's 14-file manifest remains unchanged. Scratch files and this plan are outside
that historical manifest; fresh probe results follow.

Fresh probe: source SHA256
`ad2a33c1d95c3114f4f89bef354b1db62df35ed0477c150db640dfbcc0d0d1df` admits in
6,757 steps with the pinned producer; artifact SHA256
`1e7888b088f19d57deb2895fc1f1080f44137e08b186f8e28e81461465d9c576`.
Baseline native linking returns 4/no product after 6,770 reconstruction steps.
The isolated finite-queue change passes O2 and generated-client ASan/UBSan/leak
checks: 600 Int32 and seven Int64 cases per source/object/archive product, six
source observations, null outputs, deterministic checked/trusted output,
unchanged artifact bytes, callback/demanded-effect refusals. Logs:
`/tmp/a-program-c-backend-transitive-capture-trial/{o2,san}.log`.
The producer/backend and driver object/archive bodies are O2; generated source
bodies and all clients are instrumented in the sanitizer run. No raw/current-
producer combined verification or native Acc completion is claimed.

Pins: baseline backend SHA256
`8d5875aa4c874279e38784a68014e897eee0c349fd8f875e4b8999352450bd1d`, producer
`4fa70f33b8c6fc2a4ff81f7ded4942bd40fbff3134931de9103e223ea91f8a27`, trial
backend `399a6112ea33337efc103598bdf62d829071c2a3eb4cc817b2fa7c99b7154b64`,
scratch lowerer `dbeb9d53c30ca38ae8734a0c1c7c3bd5aa42303a935c47de331997ac6e0fcb54`.
The trial source delta is +27/-17 against unchanged active scalar source
`635dac1311167cc856fb2aec30c057c188ff988c17d853f1a4767453ae4b4641`.

Fresh Git inspection: all 14 historical C7 manifest records match immutable task
`d275b75`; Merge reports push/remote verification and releases the live freeze.
Historical C7 evidence remains unchanged.

Fresh post-release private gates: raw emission passes all four no-source-work
wrappers and unchanged source graph/evidence checks. O2/generated-client
ASan/UBSan/leak execution agrees with 300 evaluator comparisons for three/four/
eight closures. `oracle.log` SHA256
`fc43b972e5f9664fffee28ff5d9392f6625f175b7e7f469ed65cdf8e31fc32c1`.
The separate static regression gate is O2 terminal exit 0: exact old
`nested_three` becomes a positive source/object/archive client case, alongside
all former positives and retained dynamic callback/demanded effect refusals.
It records 360 Int32/five Int64 cases per product, nine source observations and
the existing Nat32/native-module resource/composition controls. Log SHA256
`8ab5877c6b74c490c51373aa549a27b20b5fde383366357651e973c23e818bf2`.
Static regression also passes generated-client ASan/UBSan/leak checks. Other
eight other private O2 native/link/sorting gates also pass. The initial raw
generator build omitted its typed-query declaration header; the header is added,
warning-free build passes and the earlier build log remains. No lowering change
was needed for that harness fix.

### Assessment

Agent C8 adoption decision, 2026-10-03 after private verification: apply only the
finite capture dependency diff to the active prototype. Add a portable lane-local
source/product/raw gate, retain the original static Oracle controls and convert
the exact old three-closure fixture to positive coverage. The applied-List
selector remains separate scratch work and is excluded from C8. Final twelve
O2/nine generated-client sanitizer gates will verify the reviewable epoch before
freeze and delegated task publication. No producer/schema/shared-owner edit.

This capture candidate is an agent choice within the bounded downstream scope,
not a new user-requested transformation or callback ABI.

Merge operational release, 2026-10-03: heavy correctness/build hold is explicitly
released; resume remaining private gates sequentially at `-j1`. Merge retains
immutable C7 current-producer gates. This trial remains separate from the applied
List scratch and is not adopted or task-published.

Agent candidate: compute the finite transitive dependency set of the existing
static lexical bindings, retaining source support membership and native recursion
captures. Preserve stable lexical ordering and binder identity; reuse existing
callee/capture rebinding. Use a work queue rather than a source evaluation pass
or a hard-coded nesting limit. Keep all state in the target invocation's arena.
Abort or route the need through Merge if a shared producer/checking change is
required. Do not introduce dynamic callbacks, a closure ABI, source substitution,
new artifact fields or an upstream checker.

An isolated copy of `scalar.c` under `scratch/transitive_captures` permits a
reviewable trial while every frozen C7 implementation/document stays fixed.
Only adopt a successful bounded diff after C7 release, with positive coverage
for the exact former refusal and retained callback/effect/recursive limits.
This improves AP6.3 local calls; it does not establish Acc/QuickSort lowering.

Merge scheduling decision after C7 task publication: performance's bounded
exclusive wall/RSS slot now holds further C builds/runtime gates until explicit
release. Current build and both client commands have exited 0; no live heavy
owned child was found. Only light inspection continues. This is a verification
hold, not a paused/completed Goal or a waiver of the remaining gates.

### Plan

- [x] Pin the source, baseline binary, producer and fresh baseline refusals.
- [x] Implement the finite dependency walk in the isolated lowerer copy.
- [x] Verify three/four/eight-function chains, shadowed captures, repeated demand
  and unused effectful bodies with ordinary C source/object/archive clients.
- [x] Compare source observations, test signed extrema and Int64 parameters,
  null outputs, deterministic checked/trusted emission and immutable input bytes.
- [x] Retain explicit callback/demanded-effect/native Acc refusal controls and
  verify inert raw emission and relevant existing native gates in the private trial.
- [x] Apply only the capture diff and portable tests to the active prototype;
  keep applied-List work separate and retain original static raw controls.
- [x] Verify final twelve O2/nine generated-client sanitizer gates and style.
- [x] Record sanitizer scope, failures and source/test deltas; send a bounded
  result to Merge. Keep this trial separate from C7 publication.
- [ ] Merge's delegated task publication and separate joint/Main review.
- Completion: a small target-local diff preserves captures for the tested known
  static chains without changing admission/authority or hiding unsupported cases.
