# Native Selected Value Records

Date: 2026-10-03
Status: bounded Epoch6 verified; exact publication handoff ready for freeze.
Baseline: `parallel/c-backend-20261003`, published Epoch5 `312c2da`.
Related: [Goal](2026-10-03-C-BACKEND-GOAL.md), AP6.4/#49.

## Problem List

1. Determine whether explicitly selected closed value records can contain other
   selected value records using the existing native struct/call machinery.

## 1. Nested Value Fields

### Subjective (User)

2026-10-03, English paraphrase of direct user scope recorded in the owning Goal:
produce C readily usable from other C modules through `.a` and LinkerScript,
keep work bounded and downstream, and avoid excessive deep investigation. Do not
extend `.a` for target convenience or create a private checker. Task-branch
publication is allowed; only Core integrates Main. This record does not imply a
new user request for nested-record ABI rules.

### Objective (Code)

Fresh inspection at the baseline plus frozen Epoch5: `representation.c` rejects
a selected data field whose target representation itself has non-natural
constructors. Type entries are installed in selection order; declarations are
emitted in that order. `scalar.c:value` currently projects every nonrecursive
tagged field to its tag, and public validation checks direct enum/tag fields only.
Simply deleting the representation refusal would produce wrong aggregate C.

The existing data fixture's `Nested` refusal omits its Packet field selection;
it does not test an explicitly selected Packet/Nested pair. No producer gap is
established. Linker alias validation uses prefixed symbols and distinct namespaces;
the inspection found no naming defect to fix.

Initial probe failed source admission (`rejected steps=1889`) before native
lowering ran. Its nested Bool Match lacked the explicit parentheses used by the
existing Packet fixture; correct the probe before diagnosing a backend limitation.
Evidence: `/tmp/a-program-c-backend-nested-trial/evidence/admission.log`.

Corrected probe admits in 2,764 steps; checked linking reconstructs in 2,780 and
both checked/trusted requests refuse with status 4/no product at the representation
contract. Source SHA256:
`d12dbb0fc82c983243c44ede03731d94e2d66b6eb4f89327460b4ec0828503f9`;
image SHA256: `7567487f112978b1f2514d7882b2ce52f56c18330f94a5719aeeadb96d0dfd14`.
Logs: `admission-corrected.log`, `current-checked.log`, `current-trusted.log` under
the same evidence directory. A generated-script setup omission was corrected
before the actual native probes; its log is retained separately. Neither failure
establishes a producer bug.

Fresh final verification at `312c2da` plus the exact Epoch6 edits passes all ten
O2 C gates and seven native generated-client ASan/UBSan/leak gates. The new record
gate checks 105 cases per source/object/archive product, 16 source observations,
whole-value extraction/capture/return, output aliasing, active nested tags and
unchanged outputs on failure. Missing/later children, recursive aggregate payloads,
function/dependent fields and a value record containing a selected recursive
pointer refuse with status 4/no product. The simple Chain selection alone succeeds.
Existing callback/effect/indexed/tree/capture-chain and native Acc refusals remain.

The new raw correspondence probe initially used a nonexistent clause member;
its build failed, and the first scalar run used the earlier fixture binary. The
member correction and successfully rebuilt scalar/raw rerun pass; only the final
logs count as final evidence. Initial source/setup/build logs remain historical.
No producer or lowering change was needed for those harness errors.

Final fixture including the recursive-pointer control admits in 2,950 steps;
checked linking reconstructs in 2,968. It uses existing Int64 fields supplied by C,
not unsupported surface Int64 literals. Final source SHA256:
`38f1e5f24c8b8a5443acc7c3f9407a6afca9d9e6ef3467416eacb2ac8c548789`.
Evidence: `/tmp/a-program-c-backend-nested-trial/final-*.log`; exact tool/image/C
pins and source deltas are in the [Epoch6 handoff](2026-10-03-C-BACKEND-EPOCH6-HANDOFF.md).
Backend/producer/raw tools are O2; sanitizer scope is generated source and clients,
while driver-built object/archive bodies remain O2. The pinned worker producer
is not latest Main combined verification; Core must run its current producer.

### Assessment

Agent candidate: reuse selected by-value structs and native constructor/Match
calls for closed nonrecursive aggregate fields. Require each child selection
before its containing selection, as ordinary C complete-type declarations do;
reject a missing/later child or a recursive aggregate field before publication.
This is an explicit bounded target contract, not new source inference, dependency
normalization, a general type graph or shared nominal authority. Preserve whole
aggregate field values and validate all active nested fields using shared private
validators. Keep source/target input storage and by-value ownership unchanged.

Agent work decision (historical): first admit a small fixture and pin the current
rejection; reserve scratch work if Epoch5 remains frozen. Keep unsupported
recursive/indexed/dependent/function fields, effects,
callbacks, three-closure captures and native Acc/QuickSort explicit. Abandon this
candidate if it needs upstream interfaces or a broader representation rewrite.

Later Core operational release: exact Epoch5 is published as `312c2da`; nine
combined gates and prototype Main merge `435d965` pass. Active next-epoch edits
may now proceed; a scratch copy is unnecessary. Preserve old handoffs, bounded
scope and owner routing. This is coordination, not a new user design principle.

Agent implementation decision: retain whole aggregate field expressions instead
of projecting their tags, and generate one private C validator per selected value
record, checking only active fields. These are C ABI domain checks; source admission
remains the existing producer's job. Copy the nested-field flag from the successful
emitter into the receipt; do not reconstruct representations for that metadata.
Child-before-parent selection keeps declarations and validator calls complete
without a general dependency pass. No shared interface or producer change is needed.

### Plan

- [x] Admit the fixture and pin current checked/trusted representation failures.
- [x] Implement only selected closed value fields after Core released Epoch5.
- [x] Verify active-field validation, whole-value extraction/capture/return,
  input/output aliasing, source/C observations and source/object/archive clients.
- [x] Retain missing/later/recursive/function/dependent selection refusals.
- [x] After Epoch5 release, adopt only a verified bounded change and prepare a
  separate exact epoch. This does not complete AP6/#44/#49 or the full Goal.
- [ ] Core publishes/reviews the frozen manifest and verifies its current
  combined producer before any Main integration. Publication is not promotion.
