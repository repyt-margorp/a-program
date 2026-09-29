# Artifact Semantic Persistence Refactor

Date: 2026-09-28
Updated: 2026-09-29
Status: in progress; APGSRC65 persistence and first C backend are prototypes,
not promoted. Exact resumption and trust/fuel integration remain unfinished.
Baseline: `152b59506e915e18a34f6dc8041e981fb2a82888` (PR #45 documents
imported). Implementation is unchanged from `e716232`; unrelated working-tree
changes are excluded from this audit.

## Problem List

1. **AP1:** specify what `.a` preserves, without confusing semantic progress
   with evaluator history or making C the owner of A Program semantics.
2. **AP2:** implement shared, inert transport of that data and remove unused
   retention only after its necessary consumers have been accounted for.
3. **AP3:** verify zero-fuel stability, preserved meaning and a downstream-only
   consumption boundary; keep unfinished checkpoint work visible.
4. **AP4:** implement the first downstream C transpiler, now explicitly requested.

This is the active implementation plan, superseding R1-R6 in the
[growth audit](2026-09-28-RETAINED-IMAGE-GROWTH-AUDIT.md). That document retains
measurements, causes and the separate Context/syntax investigations.

## AP1. Persistence Contract

### Subjective (User)

User paraphrases, 2026-09-28:
- Incorporate the `.a`-to-C issue/PR when deciding which artifact data to retain.
- The transpiler follows `.a`; it must not change A Program to accommodate C.
  Keep its implementation in a separate directory under `src/`.
- Complete the artifact refactor first. Start C transpilation only after a
  subsequent user instruction. General recursion/loop syntax remains discussion.
- Include quantitative fuel/image tests, especially `--steps 0`.
- Follow-up, same date: equal cumulative fuel (for example 10 twice versus 20
  once) should produce equal-size images; byte-for-byte equality is preferable.
- Import all three research documents from PR #45 into `doc/`, preserving them.
- Follow-up, reaffirmed 2026-09-29: old artifacts need not remain readable. Do not
  maintain backward compatibility. This supersedes the agent's legacy-import
  proposal, not any of the same-format stability requirements.
- Follow-up, 2026-09-29 (user paraphrase): use one total fuel budget with a
  configurable revalidation sublimit. Explicitly trusting prior results may
  skip revalidation. Verification history may differ across reloads and is
  excluded from semantic comparison. Keep trust/reuse policy local to artifact
  I/O; it is not a prerequisite for a C backend.
- Latest follow-up, same date: reject file-size-derived read allowances. Use a
  fixed bound or no bound, and investigate excessive reachable typed data rather
  than treating a larger allowance as compaction. Proceed without further asks
  this morning.
- Follow-up, 2026-09-29 (user paraphrase): continue through C transpilation;
  extend this plan as necessary and push independently verified increments.
  This supersedes the earlier instruction to wait for another backend request.

### Objective (Code)

- [Issue #44](https://github.com/repyt-margorp/a-program/issues/44) and
  [PR #45](https://github.com/repyt-margorp/a-program/pull/45) propose a semantic
  view, selected-export readiness and preserving materialized progress. They do
  not implement those facilities or authorize accepting unverified inputs.
- `src/source_io.h:6` and `source_io.c:590` describe APGSRC62 reconstruction;
  `source_io.c:924` reads inputs and `:990` schedules ordinary synthesis.
  This is not a general export of settled typed roots.
- `src/main.c:54` and `reduction_io.c:12` collect whole evaluator stores and old
  archives. The CLI does not consume that archive as a reusable result cache.
- `src/typing.h:30` already defines descriptive occurrences: Core, classifier,
  typed formation, context, scoped operands, maps and induction allocation.
  They are not acceptance receipts. `occurrence_io.h` transports them inertly.
- `occurrence_io.c:152,275` uses name/resolve context I/O only. Full descriptor
  variants already exist in `context_io.h`; nominal declaration transport exists
  in `declaration_io.c`. There is no need for a second C-specific type graph.
- `derivation_io.h` already transports unaccepted rule inputs and effect
  definitions with shared premises. Reduction claims are separate. Neither a
  restored classifier nor a stored status proves validity.
- `program.h` makes named exports whole-module-checked selections. Per-export
  backend readiness must not silently bypass that current language contract.

### Assessment

**Revision to the earlier cleanup proposal:** deleting history and always writing
only APGSRC62 would fix one growth source, but would not satisfy the semantic
progress requirement of #44. Do not declare that limited change the completed
artifact refactor. Conversely, future C output does not justify retaining every
WHNF/NF request. Reachability from meaningful roots, not elapsed work, determines
what must survive.

Agent-proposed data contract, to be refined against concrete owners in AP1.2:

| Data | Retention rule |
| --- | --- |
| Selected exports and already materialized Core/Oracle structure | Retain roots and their dependency closure; preserve aliases and root order |
| Typed occurrences, contexts/maps, classifier formation and nominal declarations | Retain reachable construction, not another flattened copy of the same answers |
| Object-language proof/witness Terms | Ordinary semantic roots/dependencies; never erase merely because they are proofs |
| Inputs needed to validate a retained judgement or transformation | Share the existing rule/premise structure where required; first audit whether occurrence structure already suffices |
| Unfinished elaboration/verification/effect obligations | Retain their proposition/inputs, context and dependency links; no copied scheduler status as authority |
| Source/annotation recipes for unmaterialized parts | Keep enough to continue through the existing Solve path; do not drop pending definitions just because no typed root exists yet |
| Reduction results/certificates | Keep only if a specified reachable consumer needs them; wholesale evaluator snapshots are not semantic exports |
| Hash buckets, redundant indexes and old generations | Reconstruct/discard; not canonical content |
| Unfinished computation frontier / continuation | Retain or deterministically reconstruct the necessary current state if exact fuel-partition resumption requires it; do not confuse it with all past reductions |
| C names, layout, ABI, calling conventions, target profiles and generated code | Downstream only; absent from canonical `.a` |

This table describes result-retaining output. The user's earlier permission to
emit explicitly recomputable images is implemented by the opt-in inputs profile
below; it omits materialized results rather than claiming their progress survives.

Use existing graph identity and shared relocation. Wire ordinals identify records
inside an image, not permanent machine pointers or a new Claim ID hierarchy.
Do not merge nominal families or scoped binders by alpha/WHNF equality.

By default loading restores descriptive content, not trusted acceptance. Inspecting it must
not run synthesis or normalization. Any validation still required is explicit,
budgeted through existing owners, and distinguishable from loading. Preserving
the witness/input is not the same as remembering that a previous process accepted
it. Do not add an independent replay engine or promise free proof checking.

Adopt #44's one-way dependency and semantic export intent. Defer its profile
lattice, general lowered IR, range analysis, CUDA/RTL and assumption manifests.
The exact API names and extra layers in the supplied document are proposals,
not mandatory architecture. A read-only view may borrow existing nodes; it need
not copy them into a second permanent database.

#### Owner Inventory (2026-09-28)

Inspected against `a696276` (`src/` unchanged from `e716232`). This inventories
the existing image payload and identifies missing progress, not a completed
checkpoint implementation. Prototype tests also run separately against the
working tree; unrelated Evidence/IADT changes are not part of this refactor.

| Current owner/data | Consumer and decision |
| --- | --- |
| `source_io.c`: definition policy, ordered selections, source scopes, names/imports, syntax DAG | Source reconstruction and name lookup; required for unresolved source. Selection aliases must remain aliases. |
| Producer DAG: source/rule/definition/operation/normalization inputs, module entry indices | Exact source-to-producer reconnect; retain identity/operands, not another lookup by erased Core. Current producer records have **no materialized typed result** slot. |
| Source binding addresses, declaration/member/match contexts and lexical references | Preserve generative identities when elaboration continues. These are allocation inputs, not proof that a type or branch is valid. |
| `occurrence_io.c`: judgement, context, Core, classifier, formation, annotation, origin/map, selection, operands, construction maps, induction allocation | Typed inspection and ordinary kernel rules. Required reachable semantic structure; currently omitted from the Program writer. Descriptor transport is now prototyped. |
| `context_payload.c` / `declaration_io.c`: telescopes, indices, constructor/result images, nominal descriptors | Shared typed/Core dependency closure; required. Temporary packing arrays and wire ordinals are disposable, not another semantic authority. |
| `derivation_io.c`: rule parameters and premise DAG; comparison endpoints instead of certificate pointers | Existing source rule reconstruction and ordinary `pg_synthesis_derive`; keep necessary validation inputs. Do not equate an occurrence's existence with accepted evidence. Audit which are reconstructible from the retained occurrence before deleting any. |
| `effect_inference.c`: parameter/seed pairs and dependency triples | Input equations for handler/classifier inference. Existing exporter requires complete contributions, then reader leaves them unsealed. Partial contribution discovery and converged results need an explicit progress contract. |
| `program.retained_reductions`; all-store snapshot plus previous archive | CLI writes/reads but does not consume this as a cache. Standalone `pg_reduction_check_*` tests recompute it. Remove this Program retention; keep live evaluation and explicitly needed evidence APIs. |
| `synthesis_work.c`: ready order, wait edges, private owner state, result; `synthesis.steps` | One charged step dispatches one ready worker. Source import recreates work, not these continuations. Saving only producer recipes cannot preserve this progress. |
| Typing queries/actions, substitution and conversion; WHNF/NF machines | Called by private synthesis workers. Some have existing inert codecs, others do not. A codec roundtrip alone does not establish provenance or reconnect them to the owner. Still to inventory for AP1.3. |
| Hash buckets, lookup caches, transport scratch arrays, old archive generations | Rebuild or discard. Neither pointer bucket iteration order nor elapsed history is a semantic root order. |

**Fuel constraint:** the current unit is a scheduler dispatch, not wall time or
one Core beta step. Preserving exact progress requires its unfinished owner
continuation (including nested work), not merely a saved total counter. Import
revalidation must remain explicit and budgeted; how it composes with that
continuation is still an AP1.3 task. No restored `DONE` bit will authorize evidence.

**Resolved fuel/trust direction (2026-09-29):** use total budget B and validation
sublimit R: actual validation v <= min(B,R), and v + useful progress <= B.
User-authorized trust/reuse may avoid v; it must be explicitly selected. Reaching
R in strict mode leaves validation pending, not silently trusted. No separate
replay engine or uncharged verification. Trust must retain provenance and module
obligations; it cannot be implemented as importing an arbitrary DONE flag.

Agent assessment: a hash in the same file establishes neither proof validity nor
prior trusted validation. Hash reuse needs a trusted prior record and must cover
the semantic dependency closure and checker/Oracle assumptions. Imported
assumptions must not be relabeled as fresh kernel proofs. Signing/blockchain is
not required for this work. History is accounting, not semantic graph identity;
inert load/save must append nothing. At equal useful progress, semantic content
must agree; total-fuel comparisons additionally report validation spent. The
existing loss of unfinished work is still a defect, not an accounting difference.

#### Pending Owners (2026-09-29)

Further code inspection against `e716232`, excluding unrelated local edits:

| Owner | Current continuation that recipe-only import loses |
| --- | --- |
| `synthesis_work.c` | Ready order and waiter dependency/preparation flags. `inputs[]` is heterogeneous; serializing its pointer array is not a codec. `result` points to checked evidence, not an importable acceptance bit. |
| `synthesis.c` | Source stage and child producers; definition indexing/activation cursors; block scope/frame/tail; application specialization; Match branch/motive scans, generalization and indexed-boundary cursors. Existing allocation export preserves identity, not this progress. |
| `synthesis_binding.c`, `synthesis_schema.c` | Pending domain/normalization, telescope tail/scope/allocation, constructor registration/check cursors and member producers. Name hash tables can be rebuilt from these semantic entries. |
| `synthesis_function.c`, `synthesis_cbpv.c` | Provisional structures, pending substitution/comparison, result-context work, and Return/Thunk/sequence stages. Raw structure is not a checked result. |
| `synthesis_context.c`, `typing.c` | Substitution's built prefix and next image; occurrence action's next Core/classifier/annotation component and nested substitution; input-map/scope stacks and context-lift cursor/fresh binder. Completed typed graphs do not encode these cursors. |
| `synthesis_derivation.c`, `synthesis_conversion.c` | Next premise; endpoint-comparison stage; normalization owner; current checking term/type. Existing rule inputs retain obligations but restart these workers. |
| `synthesis_handler.c`, `synthesis_operation.c`, `synthesis_effect.c` | Clause scan/collection and open-contribution count; allocated label/binders; pending rule, equation solve/substitution and row-child dependencies. Definition serialization does not preserve the solve queue. |
| `synthesis_iadt.c`, `synthesis_identity.c` | Field/index/endpoint cursors, checked map prefixes, normalization/comparison, typed queries, and formation/face workers. Persisting only the final family object cannot resume them. |
| `typed_query.c` and rule owners | Exact role/input key, dependency stack and private partial construction. Roles are static C descriptors, not portable object identities; checked results need their normal kernel justification. |
| `computation_io.h`, `eval_io.h`, `identity_io.h` | Existing raw state codecs cover parts of nested work. They explicitly do not certify provenance or reconnect a saved state to a source producer; do not mistake codec existence for checked resumption. |

Assessment: copying all arena state would retain dead memoization and machine
pointers; importing only counters would discard work. The required closure starts
at selected results and their unfinished producers, follows live continuations
and necessary checked-premise inputs, and excludes unrelated completed workers.
Fresh acceptance must still be obtained through the existing kernel rules; an
explicit trust import must remain distinguishable from that acceptance. A proposed
continuation codec must specify its exact input link and validation before it can
replace recipe-only reconstruction. Do not add another general replay engine or
mark AP1.2/AP1.3 complete from this ownership table alone.

The focused prototype now exports a checked root's existing derivation inputs
and typed subject into one shared payload, destroys the source Program, then
checks those inputs in a bare graph/typing/Solve owner. No source scope or
intrinsic installation is reconstructed. Universe, Lambda, constructor and
recursive Match/IH cases return the exact imported typed subject in 9, 69, 304
and 930 charged steps respectively (O2 and ASan/UBSan pass). An unrelated saved
typed root remains unaccepted. This demonstrates reuse of existing kernel rules,
not a new replay engine, free validation or integrated source resumption.
With the current dispatch-based fuel, even checking retained inputs consumes
positive fuel. The resolved accounting contract remains separate from whether
those inputs and unfinished owner state can be transported.

### Plan

- [x] **AP1.1:** fetch #43/#44/#45 and import the three PR Markdown files unchanged.
- [ ] **AP1.2:** inventory each persisted field by owner, reader/consumer and
  semantic root. Classify as required, recomputable, or dead. Include typed
  queries, effect equations and reduction consumers; do not infer usefulness
  from a serializer roundtrip test alone.
  Image-payload inventory is above; private pending-owner continuation coverage
  and the occurrence-versus-rule validation-input analysis are still open.
- [ ] **AP1.3:** specify one export/root contract using existing occurrences,
  declarations and residual inputs. Demonstrate parse-only, partly materialized,
  completed and rejected examples. Specify how a saved result reconnects to its
  exact source producer without rediscovering it by unbounded search.
  Define the fuel unit and required current continuation so that splitting a
  budget does not lose charged progress. Inventory queues/cursors individually;
  their implementation layout is not a wire contract, but blanket deletion of
  every continuation is not justified by the history-growth finding.
  Completed term evidence alone is not completed source-module evidence:
  `synthesis_derivation.c:258` exports the term's proof, whereas definition
  registration/activation in `synthesis.c:2479` also checks siblings and `::`.
  Any replacement of a source producer must preserve those obligations and its
  namespace/polarity outputs, not forward only a valid selected term's proof.
- [x] **AP1.4:** current-format-only policy, explicitly requested by the user.
  Prototype writes/reads APGSRC65/APGRET4, with APGOCC8/APGSCP2 typed payloads.
  Earlier formats reject and must be rebuilt from source. Remove compatibility
  branches, reserved legacy archive fields, migration APIs/tests and old-version
  build dependencies. Existing seed tests check rejection of every older header.
  Focused typed-payload tests also reject earlier and unknown occurrence/scoped
  headers without publishing roots or allocating Terms (O2 and ASan/UBSan pass).
  The semantic payload test rejects APGRET versions 0-3 and 5 with unchanged
  outputs and no new Terms/proofs; version 4 succeeds (O2 and ASan/UBSan pass).
  Promotion and complete regression coverage remain separate requirements.

## AP2. Shared Transport and Removal

### Subjective (User)

Retain the separation of computational Core and typed structure, with a single
owner for each concept. Avoid duplicated reconstruction machinery, unnecessary
boundaries and broad retention just to support eventual resumption.

### Objective (Code)

`graph_io.h` already provides an enclosing image with a shared Term/object table.
`context_io.h`, `context_payload.h`, `occurrence_io.h`, `derivation_io.h` and
`declaration_io.h` provide overlapping pieces, not a complete semantic Program
image. `program->retained_reductions` currently owns the raw history on read.
The growth audit also reproduces temporary Ref-wrapper duplication; this is a
physical encoding issue, not evidence that nominal objects should be merged.

### Assessment

Extend the existing transport owners instead of building a backend-specific
serializer. A source adapter may inspect Program/Synthesis to select available
roots. The resulting semantic view must not expose parser nodes or scheduler
handles as its public ABI. Source continuation inputs can remain opaque to that
view; the existing elaborator remains their consumer.

Do not persist both a full source-derived answer graph and a disconnected export
copy. Both references must reach the same materialized nodes. Memo tables remain
in-memory optimizations unless an explicit consumer proves a need for transport.
If a necessary derivation demands a reduction endpoint, keep its dependencies;
this is not permission to save all evaluator jobs.

### Plan

- [ ] **AP2.1:** extend scoped/unscoped occurrence I/O to the existing full
  descriptor codec path. Keep name/resolve APIs as thin adapters only if still
  used. Test nominal declarations and shared binders without source reconstruction.
  Prototype implementation and targeted O2 tests pass: descriptor APIs delegate
  to the same occurrence/context codec; used name/resolve calls are adapters.
  Promotion and final acceptance coverage remain open.
- [ ] **AP2.2:** share root collection and relocation across occurrences,
  contexts, declarations, validation inputs and residual source obligations.
  Reuse `pg_graph_image_*` / `pg_graph_dependencies_init`; fix Ref transport
  duplication without copying or normalizing the borrowed semantic graph.
  Prototype fixes temporary Ref aliases by their identical object pointer in
  the shared dependency collector. Nominal objects and Lambda/Application nodes
  remain distinct by pointer. The prototype now connects available typed roots
  to their existing producer ordinals in that same table, without erased-Core lookup.
  Clean-baseline source, derivation, occurrence and scoped-structure tests pass;
  the added test also passes ASan/UBSan. Completed Example 09: retained bytes
  32,625 (prior baseline) -> 32,463, no duplicate Ref records; ordinary 26,374
  unchanged. Fuel-partition resumption still fails; this is not the history fix.
- [ ] **AP2.3:** implement an inert semantic-root view plus persistence of the
  available construction and residual inputs. Inspecting an absent result returns
  unavailable, not an implicit Solve request. Keep existing module acceptance rules.
  Prototype view borrows `pg_occurrence` directly, without another semantic DB.
  Imported construction is an unaccepted producer input, not a restored result
  receipt. Ordinary Solve still checks its source independently. Validation-input
  reuse and suspended-owner continuation remain open; this is not a checkpoint.
- [ ] **AP2.4:** remove all-store snapshot/previous-generation accumulation and
  dead Program ownership after AP1's consumer inventory. Review `main.c`,
  `program.h`, `source_io.*`, `retained_io.*`, `reduction_io.c`, `eval_internal.h`
  and `computation_io.h`. Preserve live evaluator memoization and required
  evidence APIs. Retire `--retain-reductions` explicitly rather than as a no-op.
  Prototype removes the snapshot API and Program archive field; CLI/REPL now
  have one writer. The retired option errors without creating an image.
  Explicit reduction records/checking and live memoization remain. Source,
  normalization, identity I/O, CLI images and focused ASan/UBSan pass; promotion
  remains open. Full acceptance now passes on the combined candidate below.
- [ ] **AP2.5:** use the current format only; update CLI/REPL writers, docs and
  tests. Older inputs reject, rather than taking a migration path. Same-version
  zero-fuel saving must preserve bytes. No automatic reader-limit escalation.

Implementation location before acceptance: `src/prototype/artifact_persistence/`.
Approved changes can later move to their existing artifact/semantic owners.
The future C implementation is planned under `src/transpile/c/`, separate from
those owners; no transpiler directory, emitter or target IR is needed in this phase.

#### Prototype Decisions And Checks (2026-09-28)

Compared with `f2f6f69`, new patches modify only existing source, synthesis-work
and retained-I/O owners. The reader reconnects results by producer ordinal;
selected roots and their explicit producer dependencies determine retained
results. Lexical-origin discovery supplies allocation inputs, not more exports.
The typed DAG's own closure supplies its Terms, Contexts, maps and annotations.

Rejected trials: traversing every typed/source backreference retained incidental
workers and grew Example 09 by about 12.7 KB per completed generation. Seeding
only materialized Core still retained every constructor-use Context receipt;
an extra member index did not fix that. Both broad traversal and that index were
removed. Existing constructor field addresses already identify the exact binders.
Use those addresses for default fields; retain explicit roots/non-default
allocations separately. Match allocations still need their existing source
origins. No Context, nominal declaration or binder is merged by alpha equality.
Also rejected: treating `pg_evidence_scope(term_proof)` as its term environment;
that API selects Context declarations only, not arbitrary term contexts.

Artifact-only prototype on top of `5eaf3b7`, freshly applied to clean `e716232`:
`check-artifact-history`, `check-artifact-transport` and seed tests pass. They include
source/normalization, explicit reduction checking, CLI/REPL images and fuel curves.
Focused semantic/source tests also pass ASan/UBSan. A test adds 32 unrelated live
evaluation jobs and requires unchanged saved bytes: caches are not image roots.
Read-only inspection still cannot call synthesis/evaluation, and forged roots or
invalid module siblings are not accepted. Exact saved-versus-rechecked Core is
captured before Solve so the test cannot compare a fresh result with itself.

Example 09: 25,234 bytes at step 0; 25,670 at step 100; 50,472 when complete.
Three inert generations at each fuel and three completed generations are byte
identical. The old archive's +444 bytes/generation path no longer exists.
This retains more typed structure than the former source-only image; it is not
a claim that total image size always shrinks. Reports and reproducible gates are
documented in the prototype README. The user's no-compatibility decision removes
the earlier successful migration experiment from the implementation/test suite.

Full acceptance exposed another encoding redundancy: optional occurrence
annotations/classifiers were written as Core placeholders even when absent.
General Sorted retained 263,420 occurrences but only 2,673 annotations; its
Context section exceeded the unchanged one-million-unit bound. APGOCC8/APGSCP2
omit absent reference slots. In the isolated reproducer this saves 2,085,976
bytes (31,467,832 -> 29,381,856) and restores ordinary-result comparison under
the original bound. No proof, field or classifier was erased to fit the limit.
The small transport test checks omitted slots directly and inert roundtrips.
Full acceptance also found a stale seed version assertion (updated) and a
remaining read failure for assertion-free finite permutation views. Full
acceptance/F3/F4 and promotion remain open; do not claim the large case fixed
from the small transport tests or silently increase the reader limit.
Fresh current-format reproduction: compilation completes in 1,475,909 steps;
the 20,875,920-byte image fails step-0 import. The shared Core reader encounters
214,216 objects + 814,001 Terms + 23,963 root references = 1,052,180 units, above
its 1,000,000-unit bound, before reading typed payloads. This is not backward
compatibility. Investigate retained dependency growth under AP1.2/AP2.3 rather
than treating a larger limit as the persistence fix.

**2026-09-29 combined candidate:** `candidate.sh` composes the artifact patches
with the existing readback-sharing and Fold-comparison prototypes, without
copying their implementations into a second owner. Full `check-acceptance`
passes, including the previously failing assertion-free Fin case. This run
precedes the effect-order fix below. Readback sharing alone reduces that Fin
case to 927,681 compile steps and 20,091,101 bytes; its zero-step import passes
the unchanged bound. Sharing typed substitution indiscriminately is not an
equivalent optimization: destination-context binders must remain distinct.

Before the byte-bound prototype below, the separate F3/F4 sorting gate fails.
The full QuickSort case builds
in 5,811,051 steps and writes 32,372,583 bytes; 284,806 objects, 1,062,813 Terms
and 46,350 root references exceed the same reader bound. Its selected Core closure
has only 30,716 Terms; typed headers retain a closure of 1,061,082 Terms and
178,600 typed nodes. MergeSort also fails image loading after successful source
construction. A larger diagnostic API bound allows measurement and exact inert
resave, but is not the CLI policy or a passing regression workaround.

`check-artifact-metrics` now counts Core/typed roots, unique reachable Terms and
objects, typed nodes, rule-premise edges and section bytes. It compares each
zero-step generation with its source image and forbids synthesis, reduction,
substitution and effect solving during inspection. Focused ASan/UBSan passes.
Current-continuation/residual-owner coverage is still needed to complete AP3.2.

**Reproduced and fixed in prototype:** effect definitions were enumerated in
pointer-hash bucket order, changing saved bytes after relocation without Solve.
`effect_inference` now links its existing equation/edge nodes in registration
order; it does not copy constraints or alter solver adjacency/queue ordering.
The new test fails before the fix and passes afterward: 12 inert generations,
duplicate edges/constant aliases, and unchanged definitions across partial/full
effect solving. Focused semantic ASan/UBSan, Core and derivation-I/O tests pass.
The whole acceptance run must be repeated on the eventual final candidate.

A further deterministic test reverses external operation-label addresses while
keeping their names fixed. Both closed effect rows and Fold clause descriptors
previously changed bytes at zero steps. The prototype now retains the first
effect-set enumeration and source clause order, with private pointer-sorted
membership/dispatch indexes. Lookup identity, set union/difference and clause
positions are unchanged; no pointer ordering is exported. The index is derived,
not a second mutable authority or an additional wire table. This costs one
in-memory pointer/index per label; semantic payload size does not increase.
O2/ASan/UBSan focused transport, semantic, metrics and Core tests pass. Full
`check-acceptance` passes with all three ordering fixes on the clean `e716232`
baseline (exit 0). The separate large F3/F4 and fuel-partition failures remain.
The existing source-handler boundary test now compares inert resave bytes, not
only reconstructed behavior. All 4,483 snapshots across four cases pass (including
nested calls, an invalid resumption, and a forwarded unhandled operation).

`check-artifact-partitions` still fails across reload: 100+100 produces 25,474
versus 25,830 bytes; 1600+1600 is pending at 3,200 steps versus done at 3,129
(31,002 versus 50,472 bytes). In-memory partitions and 10+10 versus 20 pass.
Next: AP1.2/AP1.3 must connect owner-local unfinished work to ordinary Solve,
accounting for import validation under the resolved AP1 fuel/trust contract.
Do not call uncharged recomputation preserved progress.

#### Rejected Read Policy and Retention Audit (2026-09-29)

The user rejected the agent's byte-derived allowance. That prototype and its
historical passing results (`/tmp/a-program-byte-bounded-*.log`) do not establish
the policy for adoption. In particular, the earlier description of every
reachable typed dependency as "required" was too strong.

Agent implementation decision within the requested fixed-or-unbounded choice:
keep a fixed default of 1,000,000 decoder units, expose `--image-limit N`, and
use the same explicit bound for files and pipes. No size-based retry or inferred
allowance. Unbounded decoding is not implemented in this increment; SIZE_MAX
cannot just be passed to codecs whose allocation checks assume a finite bound.
The limit is per-payload records/references/name units, not bytes, memory or fuel.
The large sorting tests explicitly select 10,000,000; this is **not compaction**.

Prototype `artifact/file.c` owns file/pipe adaptation and atomic publication;
the codecs still own graph transport. Pipe staging exists because the shared
relocation table needs seeks, not to measure an allowance. Later trust/history
policy belongs here; Kernel and a future backend do not acquire file policies.

Fresh, inert measurements on the 32,372,583-byte QuickSort image:

| Diagnostic closure | Typed nodes | Unique header Term dependency closure |
| --- | ---: | ---: |
| Materialized producer Core roots only | not applicable | 30,716 |
| Operands/maps, omitting type and origin edges | 9,314 | 341,950 |
| Add type edges, omit origin | 25,834 | 344,220 |
| Add origin edges, omit type | 59,481 | 905,258 |
| Full retained typed closure | 178,600 | 1,061,082 |

The last row has 28,784 mapped constructions, 14,616 derived constructions and
198,805 map-image edges. Headers include Core/classifier/annotation; the closure
also follows nominal descriptor dependencies. Context/allocation roots add more
records to the complete image. These diagnostic omissions are not valid codecs
or size-saving claims. Measurements forbid Solve and verify unchanged resave.
Reproduce with `artifact_metrics IMAGE 10000000 --retention`.

Assessment: `source_io.c` retains each reachable producer's materialized result,
not just explicitly selected exports. `occurrence_io.c:operand` then retains
origin/type/map dependencies. Therefore absence of evaluator history does not
mean absence of construction-history retention. `typing.c` input selection and
context action still consume these origins, so blind deletion would lose typed
construction. Separate public result structure, remaining work's exact inputs,
and optional reconstruction/checking aids by **consumer**, not by reachability
alone. Avoid adding a second parallel type graph or serializing all workers.

- [x] Withdraw byte-derived allowance; add explicit fixed limits and localized
  file I/O in the prototype. Tests: exact threshold/larger-bound agreement,
  file/pipe zero-step identity, large-file explicit override, and atomic failed
  replacement. Existing current-format rejection remains unchanged.
- [x] Add reproducible edge-attribution metrics; do not mutate the graph to
  inspect it or call an ablation a valid reduced artifact.
- [ ] AP1.2/AP2 continuation: classify retained producer roots by export, pending
  consumer and verification provenance. Record which can be omitted in a
  recompute profile and which a progress-preserving profile actually consumes.
- [ ] Replace any confirmed redundant origin/type retention with the existing
  typed owner's shared construction or an explicitly selected recompute policy.
  Gate on open scopes, distinct nominal families, ordinary Sorted results,
  module siblings/`::`, and step-0 identity; never waive obligations to shrink.
- [ ] Implement the single-fuel validation sublimit and explicit trust/import
  provenance from AP1. Record history separately; no false checkpoint claim.

#### Explicit Recompute Profile (2026-09-29)

Agent implementation within the user's earlier requirement to allow either
recomputation or retained intermediate results, controlled by an option:

- `--save FILE.a` retains available typed results as before.
- `--save-inputs FILE.a` preserves producer/source/rule inputs and obligations but
  omits materialized producer results. It restarts Solve on import, deliberately.
  This is neither a checkpoint nor a backend-ready typed export.

Both use one writer/reader and APGSRC65; no second graph, solver or compatibility
format. Only result-root selection differs. Nominal allocation dependencies
needed by retained inputs still use the existing closure collector. Omitted
results do not waive module siblings, annotations or later proof checking.
The choice is per save; `:save` keeps its existing result-retaining behavior.

Measured conversion at zero steps: QuickSort 32,372,583 -> 3,070,587 bytes (90.5%
smaller). Materialized typed nodes go from 178,600 to 0; shared typed/rule Core
table becomes 21 Terms. The remaining file includes source syntax and producer
inputs, not just this shared table. The default fixed allowance reads this image.
This measures an explicit space/recomputation tradeoff, **not** a 90.5% reduction
while preserving all typed progress. The result-retaining profile is unchanged.

- [x] Prototype the input profile without mutating source state. Small tests
  cover parse-only/partial/completed recursive Match/IH, host arithmetic, alias
  roots, and a valid selected name beside an invalid sibling. Zero-step resave
  is identical; re-solving preserves the eventual done/rejected status.
- [x] Complete large Quick/Insertion recomputation and negative-proof gates,
  plus current fixed-limit regression/sanitizer checks. Fresh candidate
  `/tmp/a-program-input-profile` on clean `e716232` plus prototype: history/CLI,
  examples 01-09/results, focused O2/ASan/UBSan and file-policy tests pass.
  Five-backend sorting passes: Quick/Insertion 143s, Merge 90s, Tree 127s,
  Bubble 175s, plus concrete value transport. Input-only general QuickSort
  and invalid-sibling/shape rechecking pass; other large backends exercise the
  result-retaining profile. Reports: `/tmp/a-program-input-profile-{history,
  asan,sorting,file-policy}.log`. Full acceptance has not been rerun for this
  increment; no production promotion or main push is claimed.
- [ ] Continue AP1/AP2 reduction of redundant **retained-result** dependencies;
  inputs-only output does not discharge that work or checkpoint preservation.

#### Restore Existing Inputs Only (2026-09-29)

Inspection found that `pg_sources_read` called the source constructor, which
installed a second intrinsic namespace and scheduled its typing work before
restoring the image's own namespace. The prototype now shares owner allocation
through `pg_program_allocate_empty`; source construction adds its standard
namespace, while image loading uses only the saved lexical inputs. The imported
Program convenience scope is empty; source/REPL extensions use the restored
producer's lexical environment, not another prelude. No kernel rule is skipped.

The new zero-root image test fails before the fix: loading an empty image leaves
unrelated runnable work. It passes afterward, with no runnable work or consumed
steps. Restored `#Int`/`#int_add` and subsequent source extension, semantic tests,
history/CLI tests and focused ASan/UBSan pass. Full `check-acceptance` passes on
this candidate (exit 0). Example 09 from the same zero-fuel image completes in 2,942 rather
than 3,129 steps; completed image bytes remain 50,472. Inert and completed-cycle
gates remain exact. The strict partition gate still fails: 100+100 gives 25,830
versus 26,070 bytes; 1600+1600 is pending versus done at 2,942. This removes
duplicate initialization, not the missing continuation or validation fuel cost.
Reports: `/tmp/a-program-restore-inputs-{history,asan,partitions,acceptance}.log`.

The five-backend sorting gate passes on this candidate: Quick/Insertion 135s,
MergeSort 98s, TreeSort 139s, BubbleSort 192s, plus concrete value transport.
This does not discharge the separate partition/resumption gate.

The split-budget probe now measures a terminal boundary rather than assuming
one, and tests both `terminal+0` and `0+terminal`. It reports byte/size agreement,
local status and consumed fuel separately. At `2942+0`, the image and cumulative
fuel match exactly but local status changes from `done` to `pending`: zero-step
import intentionally does not admit evidence. This is distinct from the earlier
`100+100` lost-progress failure. The strict gate still reports failure; no
acceptance condition was silently relaxed. This predates the AP1 user decision;
the revised gate must report validation/trust provenance separately from progress.
Report: `/tmp/a-program-restore-inputs/terminal-partition-fields/partitions.tsv`.

## AP3. Verification and Handoff

### Subjective (User)

Measure growth versus fuel, with zero-step cases mandatory. Finish this refactor
before taking up C emission. The user will continue investigating #43 separately.
The user additionally requests equal-size outputs for equivalent fuel partitions,
and preferably identical bytes, not merely absence of growth.

### Objective (Code)

The existing prototype `image_audit/fuel_curve.sh` records bytes, deltas, hashes,
reported fuel and exit status. Example 09 passes ordinary cycles but detects
retained growth of 444 bytes per completed generation. The large ordinary
prototype matrix is stable at 3,065,779 bytes; retained mode hits the default
reader bound. These are prior targeted results, not tests of the planned format.

Fresh split-budget probe on the same clean `e716232` compiler (implementation
unchanged at `df250f3`), Example 09, starting every path from the same zero-fuel
image: 10+10 versus 20 is byte-identical. At 100+100 versus 200, ordinary save/load
produces **25,266 versus 25,598 bytes** despite both reporting 200 used steps and
pending status. Splitting in memory, with or without an intermediate save but no
reload, matches the single run. Retained mode also fails across reload and has
some same-size byte differences. `partition_fuel.sh` records these as failures,
not accepted deviations. Logs: `/tmp/a-program-partition-*`.
At 1600+1600 versus 3200, the ordinary images are even byte-identical (26,374
bytes), but the single run is done at 3,129 steps while the reloaded split remains
pending at 3,200. Thus byte equality alone cannot certify preserved Solve progress.

Rechecked against the `f2f6f69` clean-baseline transport overlay on 2026-09-28:
both failures persist. The prototype `check-artifact-partitions` target now runs
the strict byte gate as well as size/status/consumed-fuel checks. It deliberately
fails rather than accepting these cases as expected failures; its reports remain
available for comparison. This is not yet a production-registered or passing gate.

### Assessment

Size stability alone can hide loss of useful progress. Require both inert
stability and preservation of already materialized semantic roots. Reading a
view without Solve is testable now without implementing a C emitter. An unfinished
export may remain unavailable; a malformed/rejected input must not become accepted.

The desired composition law is `advance(advance(S, a), b) = advance(S, a+b)`;
the serialized-resume variant inserts save/load between the two advances.
Fix compiler version, input image, selected roots, policy and requests when
testing it. Exclude format migration and host execution from this pure gate.
Baseline APGSRC62/63 and prototype APGSRC65 reconstruction do not preserve the full running frontier;
the equality of supplied or reported fuel therefore does not establish equality
of progress. A recompute fallback must be reported as such, not counted as passing
this resume gate. Count any required revalidation explicitly; do not hide its cost
or silently trust imported claims merely to make the equation pass. An explicitly
selected trust mode is covered by AP1, not an automatic test workaround.
In particular, local acceptance after importing an image is not a serialized fact:
distinguish revalidation work from a lost computation frontier when investigating
these diagnostics. Do not fix the counter/status mismatch by restoring a trusted
`done` flag. The intended structural gate concerns saved progress, not that flag.

Minimum gate: same materialized structure/residual frontier and byte count.
Stronger gate: identical bytes under deterministic serialization, without alpha
interning or normalization during save. Record both outcomes separately. Fixed
padding, retained dead records or omitting useful results cannot repair a failure.
CLI status/step/byte equality alone cannot prove the structural part: the planned
semantic-root instrumentation remains necessary, even when this prototype passes.

### Plan

- [x] **AP3.0:** retain the existing quantitative prototype and baseline results.
- [x] **AP3.0b:** add/run the split-budget CLI probe, with separate size and exact
  byte verdicts and fresh source/host/invalid-assertion controls. No production fix.
- [x] **AP3.1:** extend it for the chosen format: source fuel 0/1/100/completion;
  three same-format zero-step rewrites of each state; partial resume and three
  completed generations, each followed by zero-step rewrites. Require exact byte
  identity at zero, no host output/acceptance, and no prior-history accumulation.
  Prototype history/metrics gate passes on the byte-bound candidate. Reopening
  then finishing is still recomputation, not preserved partial progress; the
  stricter continuation requirement remains open under AP3.1b.
- [ ] **AP3.1b:** compare 20 with 10+10, 1+19, 0+20 and 20+0, plus 0 versus 0+0;
  include larger splits at actual construction/evaluation suspension boundaries
  and after completion. Compare single run, in-memory split, split with an inert
  save, and split across a fresh-process reload. Record supplied/used cumulative
  fuel, validation spent, trust provenance, status, semantic roots/residuals,
  counts, bytes and digests. Under the updated AP1 contract, require size and
  structural agreement at equal useful progress; exclude verification history
  from that comparison, but test history accounting separately. Keep total-fuel
  comparisons visible, including their actual revalidation cost. Run semantic
  byte equality as a separately visible stricter
  gate. Neither smaller files nor equal-sized but changed files count as full
  equivalence. Compare actual consumed fuel as well as budgets when work finishes
  early; do not require consuming the unused budget after completion.
- [ ] **AP3.2:** count semantic roots, unique reachable Terms/objects, typed nodes,
  obligation edges and section bytes in addition to total bytes. Attribute
  positive-fuel growth to newly retained dependencies. Do not impose universal
  bytes-per-step proportionality or call every fresh binder a duplicate.
  Prototype metrics cover the current semantic payload and rule edges; nested
  pending-owner continuation/residual counts remain open.
- [ ] **AP3.3:** verify save/load preserves existing materialized results and
  pending obligations without advancement. Instrument that inspecting the view
  makes no synthesis/evaluator calls, not merely that a CLI counter reports zero.
  Rechecking retained material must use existing kernel rules under explicit fuel.
- [ ] **AP3.4:** test same erased Core with different annotations, distinct nominal
  families, open scopes/maps, selected-root aliases, IADT/Identity witnesses,
  effect equations, invalid `::`, and normalization requests. Keep required
  negative/trust tests when removing duplicate ordinary/retained matrices.
- [x] **AP3.5:** add a read-only semantic-view consumer test with no C codegen.
  It must not include syntax/synthesis headers or change the image on inspection.
  Test a materialized export beside an unfinished sibling without waiving any
  whole-module proof needed to execute the selected root.
  Prototype O2/ASan/UBSan passes: a consumer reads a suspended function's Pi,
  formation, binder, scoped body and result classifier without forcing it.
  An invalid unfinished sibling still rejects the module/checked selection;
  inspection leaves zero steps, no new evidence and identical saved bytes.
  The consumer test is not a backend-readiness or acceptance API.
- [ ] **AP3.6:** run source/occurrence/scoped/derivation I/O, execution, F3/F4 and
  full acceptance tests plus focused ASan/UBSan on the candidate. Promote only
  authorized pieces; register the small fuel test permanently and the large one
  with sorting. Report per-file additions/deletions for implementation, tests
  and docs separately. Record remaining unavailable cases, not false completion.
  The earlier result-retaining candidate passed these regression suites and
  focused sanitizers against clean `e716232`. The newer fixed-limit/input-profile
  increment has its separate results above; do not inherit an unrun full-suite
  verdict from the previous candidate. Promotion, permanent registration and the
  separate failing partition gate remain open; this is not completion of AP1-AP3.

Handoff: the agreed semantic content survives `.a` roundtrips, reading/inspection
is inert, unused history no longer accumulates, and existing language behavior
remains unchanged. The user has now requested C transpilation (AP4). #44 remains
open until its implementation and verification criteria are met.
#43 remains open and receives no syntax or termination-rule changes in this work.

## AP4. First C Transpiler

### Subjective (User)

2026-09-29, paraphrase: implement at least `.a` to C, keeping artifact work in
scope, and push at appropriate verified milestones. The earlier one-way backend
dependency and separation of computation from typing remain requirements.

### Objective (Code)

At `5eaf3b7` plus the AP1-AP3 prototype, loading is inert and materialized typed
roots are inspectable. Whole-module acceptance still requires ordinary Solve;
an imported occurrence is not accepted evidence. No C emitter exists. Core has
Lambda/Application/Reference; Oracle owners already expose inert host, CBPV and
constructor/matcher views. These views need not be copied into an artifact IR.

AP4 implementation after `966df23`: `c_backend/emit.c` consumes the existing
occurrence/Core/Oracle views. Its command adapter alone imports images and
requests whole-module checking. Generated executables link only the target
runtime and libc. O2 and ASan/UBSan differential gates pass for captures, partial
applications, repeated suspensions, deep handlers, recursive ADTs, generic List,
host overflow/formatting, NUL, pending/rejected inputs and atomic output.
The raw Oracle gate covers two distinct operation clauses and all ten host
arithmetic/formatting functions; emission is checked for zero evaluator steps
and unchanged graph/proof counts. Repeated imports emit identical C and do not
change input-image digests.

Focused O2 gates also pass against clean `e716232` plus the committed persistence,
readback and conversion prototypes, without the unrelated Evidence/IADT edits.
The separate full acceptance run uses the current working-tree candidate; its
result must be recorded separately when finished.

The existing Acc QuickSort source checks in 67,229 steps and image selection in
67,377 steps. Interpreter execution prints `FFTT` in 18,495 runtime steps. C
emission currently refuses its reachable `identity-field` Oracle. This is an
explicit unsupported boundary, **not** a successful C QuickSort execution.

### Assessment

Agent implementation decision: build a separate prototype backend consuming a
closed typed root, not parser nodes or scheduler state. A thin command driver
loads `.a`, obtains a checked named export through existing Solve, and passes the
borrowed occurrence downstream. Explicitly report reconstruction fuel: this
first driver does not pretend to preserve the unfinished solver frontier or to
implement trusted import. Zero fuel must not emit an unchecked executable.

Emit C functions/closures from the structural DAG, with a backend-local runtime
for the supported Oracles. No embedded source parser, Solver, kernel or `.a`
reader in the generated executable; no compile-time execution of host effects.
Use exact fixed-width unsigned arithmetic for host wrapping operations, exact
Text bytes and structural ADTs, not a heuristic Nat-to-machine-int conversion.
Recognize nominal objects by pointer identity, never source spelling. Unsupported
reachable Oracles fail before publishing output. Do not erase proof Terms on
the assumption that every proof is computationally irrelevant.

Rejected shortcut: treating Identity transport as an unconditional no-op, or
normalizing the entire QuickSort at translation time to hide missing runtime
support. `identity.c:field_answer` has a diagonal transport rule, but the general
U/Pi transport also transforms inputs/results and lifting. That needs its own
faithful target realization; the mere presence of checked typing does not erase
those computations. Preserve this boundary test until AP4.6 is implemented.

### Plan

- [x] **AP4.1:** add `src/prototype/c_backend/` with emitter, runtime, command
  adapter and focused tests; keep accepted build untouched during trial.
- [x] **AP4.2:** cover Lambda/Application, Return/Thunk/Force, Fold/request,
  host integers/Text/print; retain effect order and delayed execution.
- [x] **AP4.3:** add structural constructors and Match, including recursive
  Lambda encoding, without changing IADT identity or typechecking rules.
  This is the structural runtime fragment, not all Identity-bearing IADT uses.
- [x] **AP4.4:** compile emitted C independently and compare execution to the
  interpreter; test overflow, embedded NUL, higher-order captures, repeated
  thunks, multiple handler clauses, invalid siblings, pending input, unsupported
  Oracles, output failure and unchanged input artifacts. Run sanitizers.
- [ ] **AP4.5:** document the exact supported subset, fuel/trust limitations,
  generated runtime ownership and per-file line changes. Push verified chunks;
  do not close #44 or mark AP1-AP3 complete from a limited backend milestone.
- [ ] **AP4.6:** implement the needed Identity transport/action/lifting target
  equations using exact Oracle contracts, then replace QuickSort's explicit
  rejection gate with actual generated-C differential execution. Include
  non-diagonal and dependent U/Pi cases; no unconditional proof erasure.

Initial backend line delta (new files; no accepted implementation changed):

| File/group | Added | Removed |
| --- | ---: | ---: |
| `c_backend/emit.c` | 230 | 0 |
| `c_backend/main.c` | 102 | 0 |
| `c_backend/runtime.c` | 273 | 0 |
| `c_backend/emit.h` | 10 | 0 |
| `c_backend/runtime.h` | 42 | 0 |
| `c_backend/oracle_test.c` | 149 | 0 |
| `check.sh` / `oracle_check.sh` / `sorting_check.sh` | 85 / 11 / 26 | 0 |
| `build.mk` | 20 | 0 |
| Eight `.p` fixtures | 74 | 0 |
| Backend README | 87 | 0 |

Implementation: +657. Tests/fixtures/build: +365. Backend documentation: +87.
This is a new backend feature, not a claim of code reduction or completed AP1-AP3.

## Research Records

The following PR #45 files are imported verbatim. Their supplied research remains
available; this plan neither rewrites it nor claims to have revalidated its whole
bibliography. The decisions above govern this narrower implementation phase.

- [Artifact/backend audit](2026-09-28-artifact-backend-lowering-audit.md)
- [General recursion audit](2026-09-27-GENERAL-RECURSION-CBPV-AUDIT.md)
- [Empty type, abort and divergence](2026-09-28_cbpv_empty_type_abort_divergence.md)
