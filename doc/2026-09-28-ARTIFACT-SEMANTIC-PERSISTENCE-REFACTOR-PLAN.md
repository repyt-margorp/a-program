# Artifact Semantic Persistence Refactor

Date: 2026-09-28
Status: in progress; shared transport and materialized-result prototype, not promoted.
Baseline: `152b59506e915e18a34f6dc8041e981fb2a82888` (PR #45 documents
imported). Implementation is unchanged from `e716232`; unrelated working-tree
changes are excluded from this audit.

## Problem List

1. **AP1:** specify what `.a` preserves, without confusing semantic progress
   with evaluator history or making C the owner of A Program semantics.
2. **AP2:** implement shared, inert transport of that data and remove unused
   retention only after its necessary consumers have been accounted for.
3. **AP3:** verify zero-fuel stability, preserved meaning and a downstream-only
   consumption boundary before starting any transpiler.

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

Use existing graph identity and shared relocation. Wire ordinals identify records
inside an image, not permanent machine pointers or a new Claim ID hierarchy.
Do not merge nominal families or scoped binders by alpha/WHNF equality.

Loading restores descriptive content, not trusted acceptance. Inspecting it must
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
- [ ] **AP1.4:** choose the version/migration policy before deleting the old
  writer. A new semantic payload needs explicit versioning. APGSRC62/63 may be
  bounded legacy imports with unavailable semantic roots reported explicitly;
  do not claim that old source-only images can expose missing results at zero fuel.
  Prototype writes APGSRC64/APGRET3 and explicitly reads legacy 62/63. A fresh
  62 -> 64 zero-fuel migration followed by a byte-identical 64 resave passes.
  Final migration/legacy-archive removal and full acceptance are still pending.

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
  to the same occurrence/context codec; legacy name/resolve calls are adapters.
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
- [ ] **AP2.5:** implement the chosen legacy migration, then update CLI/REPL
  writers, documentation and tests. A migration may change bytes once; normal
  same-version zero-fuel saving may not. No automatic reader-limit escalation.

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

Rejected trial: feeding every materialized object's source backreferences into
origin discovery retained incidental workers and grew Example 09 by 12,677 bytes
per completed generation, mostly derivation inputs (18,032 -> 30,224 bytes), not
duplicate Core nodes. That traversal and its extra collection API were removed.
Also rejected: treating `pg_evidence_scope(term_proof)` as the term's environment.
That API selects Context declarations only. Raw Contexts/typed formation edges
are preserved; no arbitrary Context receipt or Universe bound is inferred.

Fresh clean-baseline O2 source/derivation/transport suites and focused ASan/UBSan
pass. Read-only consumption uses `typing.h`, not parser/scheduler headers.
Forged stored construction does not bypass `::`, nor an invalid sibling module
obligation. Example 09, host and invalid-assertion fuel curves pass ordinary
zero-fuel rewrites and three completed generations. Example 09 is now 49,830
bytes (previous source-only 26,374); all those completed/inert saves are identical.
This is additional reachable typed construction, not a claimed size reduction.
Fresh-process split-fuel checks still fail: 100+100 yields 25,482 vs 25,838 bytes,
and 1600+1600 remains pending with 29,546 vs the completed 49,830. Existing private
continuations and import-validation work must be addressed next, not hidden by
restoring a trusted completion bit. The optional old reduction archive still
grows by 444 bytes/generation in this prototype; AP2.4 must remove that ownership.
Full acceptance/F3/F4 and promotion are open.

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
Current APGSRC62/63 reconstruction does not preserve the full running frontier;
the equality of supplied or reported fuel therefore does not establish equality
of progress. A recompute fallback must be reported as such, not counted as passing
this resume gate. Count any required revalidation explicitly; do not hide its cost
or trust imported claims merely to make the equation pass.
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
- [ ] **AP3.1:** extend it for the chosen format: source fuel 0/1/100/completion;
  three same-format zero-step rewrites of each state; partial resume and three
  completed generations, each followed by zero-step rewrites. Require exact byte
  identity at zero, no host output/acceptance, and no prior-history accumulation.
- [ ] **AP3.1b:** compare 20 with 10+10, 1+19, 0+20 and 20+0, plus 0 versus 0+0;
  include larger splits at actual construction/evaluation suspension boundaries
  and after completion. Compare single run, in-memory split, split with an inert
  save, and split across a fresh-process reload. Record supplied/used cumulative
  fuel, status, semantic roots/residuals, counts, bytes and digests. Require size
  and structural agreement; run byte equality as a separately visible stricter
  gate. Neither smaller files nor equal-sized but changed files count as full
  equivalence. Compare actual consumed fuel as well as budgets when work finishes
  early; do not require consuming the unused budget after completion.
- [ ] **AP3.2:** count semantic roots, unique reachable Terms/objects, typed nodes,
  obligation edges and section bytes in addition to total bytes. Attribute
  positive-fuel growth to newly retained dependencies. Do not impose universal
  bytes-per-step proportionality or call every fresh binder a duplicate.
- [ ] **AP3.3:** verify save/load preserves existing materialized results and
  pending obligations without advancement. Instrument that inspecting the view
  makes no synthesis/evaluator calls, not merely that a CLI counter reports zero.
  Rechecking retained material must use existing kernel rules under explicit fuel.
- [ ] **AP3.4:** test same erased Core with different annotations, distinct nominal
  families, open scopes/maps, selected-root aliases, IADT/Identity witnesses,
  effect equations, invalid `::`, and normalization requests. Keep required
  negative/trust tests when removing duplicate ordinary/retained matrices.
- [ ] **AP3.5:** add a read-only semantic-view consumer test with no C codegen.
  It must not include syntax/synthesis headers or change the image on inspection.
  Test a materialized export beside an unfinished sibling without waiving any
  whole-module proof needed to execute the selected root.
- [ ] **AP3.6:** run source/occurrence/scoped/derivation I/O, execution, F3/F4 and
  full acceptance tests plus focused ASan/UBSan on the candidate. Promote only
  authorized pieces; register the small fuel test permanently and the large one
  with sorting. Report per-file additions/deletions for implementation, tests
  and docs separately. Record remaining unavailable cases, not false completion.

Handoff: the agreed semantic content survives `.a` roundtrips, reading/inspection
is inert, unused history no longer accumulates, and existing language behavior
remains unchanged. Stop here for the user's C-transpiler instruction. #44 remains
open; documentation import or this prerequisite alone does not implement it.
#43 remains open and receives no syntax or termination-rule changes in this work.

## Research Records

The following PR #45 files are imported verbatim. Their supplied research remains
available; this plan neither rewrites it nor claims to have revalidated its whole
bibliography. The decisions above govern this narrower implementation phase.

- [Artifact/backend audit](2026-09-28-artifact-backend-lowering-audit.md)
- [General recursion audit](2026-09-27-GENERAL-RECURSION-CBPV-AUDIT.md)
- [Empty type, abort and divergence](2026-09-28_cbpv_empty_type_abort_divergence.md)
