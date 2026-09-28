# Artifact Semantic Persistence Refactor

Date: 2026-09-28
Status: planning; no production implementation in this update.
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
| Work queues, cursors, hash buckets, every intermediate reduction and old generations | Not required canonical content; reconstruct indexes/work as needed |
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

### Plan

- [x] **AP1.1:** fetch #43/#44/#45 and import the three PR Markdown files unchanged.
- [ ] **AP1.2:** inventory each persisted field by owner, reader/consumer and
  semantic root. Classify as required, recomputable, or dead. Include typed
  queries, effect equations and reduction consumers; do not infer usefulness
  from a serializer roundtrip test alone.
- [ ] **AP1.3:** specify one export/root contract using existing occurrences,
  declarations and residual inputs. Demonstrate parse-only, partly materialized,
  completed and rejected examples. Specify how a saved result reconnects to its
  exact source producer without rediscovering it by unbounded search.
- [ ] **AP1.4:** choose the version/migration policy before deleting the old
  writer. A new semantic payload needs explicit versioning. APGSRC62/63 may be
  bounded legacy imports with unavailable semantic roots reported explicitly;
  do not claim that old source-only images can expose missing results at zero fuel.

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
- [ ] **AP2.2:** share root collection and relocation across occurrences,
  contexts, declarations, validation inputs and residual source obligations.
  Reuse `pg_graph_image_*` / `pg_graph_dependencies_init`; fix Ref transport
  duplication without copying or normalizing the borrowed semantic graph.
- [ ] **AP2.3:** implement an inert semantic-root view plus persistence of the
  available construction and residual inputs. Inspecting an absent result returns
  unavailable, not an implicit Solve request. Keep existing module acceptance rules.
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

## AP3. Verification and Handoff

### Subjective (User)

Measure growth versus fuel, with zero-step cases mandatory. Finish this refactor
before taking up C emission. The user will continue investigating #43 separately.

### Objective (Code)

The existing prototype `image_audit/fuel_curve.sh` records bytes, deltas, hashes,
reported fuel and exit status. Example 09 passes ordinary cycles but detects
retained growth of 444 bytes per completed generation. The large ordinary
prototype matrix is stable at 3,065,779 bytes; retained mode hits the default
reader bound. These are prior targeted results, not tests of the planned format.

### Assessment

Size stability alone can hide loss of useful progress. Require both inert
stability and preservation of already materialized semantic roots. Reading a
view without Solve is testable now without implementing a C emitter. An unfinished
export may remain unavailable; a malformed/rejected input must not become accepted.

### Plan

- [x] **AP3.0:** retain the existing quantitative prototype and baseline results.
- [ ] **AP3.1:** extend it for the chosen format: source fuel 0/1/100/completion;
  three same-format zero-step rewrites of each state; partial resume and three
  completed generations, each followed by zero-step rewrites. Require exact byte
  identity at zero, no host output/acceptance, and no prior-history accumulation.
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
