# Typed Structure and Evidence Refactoring

Date: 2026-09-16
Status: in progress; typed conclusions verified, scoped context action in progress
Baseline: `1b95e551b6ef315e079120a52b1a878d06ff2d63`, `rewrite/pointer-core-hott`
Parent: [Pointer Core reimplementation](2026-09-07-POINTER-CORE-REIMPLEMENTATION-PLAN.md)

## 1. Purpose and Boundaries

Make typed program structure accessible without reconstructing it from the
history of its typing derivation. Keep object-language witnesses as Terms and
keep their computational meaning separate from acceptance evidence.

This replaces the existing occurrence representation; it must not add a third
permanent program graph alongside Occurrence and Evidence. It is not a rewrite
of the logical rules and does not complete Higher Identity. Core remains
Lambda/Application/Reference, interned by exact pointer structure. CBPV sorts,
nominal declarations, contexts and classifiers remain above erased Core.

`::` remains a post-check. Pure conversion does not acquire propositional
equality reflection. Context/substitution formation still requires checking.
Imported data does not become trusted merely because it describes a typed node.
One Solve path handles source and images; no independent Replay semantics.

The pre-existing diagnostic edit was verified and committed separately as
`4657cc6` (reject an unbound constructor index rather than report unsupported).
It is excluded from this refactor's LOC comparison. The debug acceptance gate
passed at that checkpoint, including 63/63 source compatibility cases.

## 2. Findings in the Current Code

Paths below are relative to `src/prototype/pointer/`; symbols are the stable
references, while line numbers refer to the baseline.

| Location | Observed responsibility | Refactoring implication |
|---|---|---|
| `typing.h`, `pg_occurrence` | Context, Core, annotation, operand occurrences; no accepted classifier | Not yet a self-contained typed structural view |
| `typing.c:90`, `pg_occurrence` | Exact tuple interning, not typing acceptance | Preserve structural sharing; do not infer validity from allocation |
| `evidence.h:9`, `pg_evidence_rule` | 53 derivation rules and seven judgement kinds | These are not 53 Core term constructors; tag count alone is not a defect |
| `evidence.c:10`, `pg_evidence` | Conclusion, rule, premises, overloaded certificate pointer | Separate conclusion access from proof reconstruction without deleting premises |
| `evidence.c:2559`, `pg_prove_reflexivity` | Builds an Act Term, then proves its Identity classifier | Witnesses already are Terms; no replacement witness encoding is needed |
| `evidence.c:2847`, `pg_prove_application` | Stores child occurrences and child derivations | Structural edges and proof premises overlap but have different identities |
| `evidence.c:2932/3094`, projection/reindex | Changes parent context/Core/type while retaining old operand occurrences | Child edges need an explicit scope/map interpretation |
| `evidence.c`, `return_value_origin`, `pi_component`, `constructor_origin`, `classifier_recovery_step` | Recovers typed structure/formation by walking derivation wrappers | Migrate structural access; preserve genuine typed inversion/checking |
| `function_graph.c:181`, `computation_origin` | Interprets projection, reindex, conversion and normalization to expose program operations | Generated graphs depend on derivation layout rather than a stable typed structural interface |
| `action.c:77`, `origin_step` | Independently reconstructs origin and context maps | Share structural context action; do not conflate it with Identity transport |
| `derivation.c`, `pg_prove_derivation` | Reconstructs acceptance from premises using kernel rules | Rule dispatch belongs here and remains necessary |
| `occurrence_io.c`, `derivation_io.c`, `source_io.c` | Transport structure, rule inputs and preparation dependencies | Preserve shared identity and ordinary acceptance across the migration |

These findings establish coupling, not a demonstrated logical inconsistency.
In particular, old operands under a reindexed parent can be provenance; they
are not automatically invalid. The missing invariant is whether an edge means
current typed structure or historical construction input. Consumers must not
guess. Likewise, a proof premise is not necessarily an executable operand.

## 3. Target Representation

### 3.1 One typed structural record

Evolve/replace `pg_occurrence`, rather than adding a parallel typed AST:

```text
typed subject
  Core reference + Context + classifier + judgement/sort
  declaration annotation, where semantically needed
  semantic construction inputs and their lexical scopes

derivation
  conclusion reference
  rule + premise derivations + rule-specific evidence
```

The record is the upper-layer typed use of a Core, not a new Core. Distinct
classifiers or contexts must not collapse merely because Core pointers match.
The classifier on a checked conclusion is stored once; Evidence accessors
delegate to it instead of maintaining another mutable answer.

Use existing semantic owners/layouts to describe construction inputs. Do not
introduce a tag for each proof rule, or split Lambda/APP into value/computation
Core variants. Lambda body scope, constructor fields, Match motives/branches,
and handler clauses need explicit contracts; Core APP children alone do not
encode those typed relationships. Establish these contracts in R0.

Pending source inputs remain unaccepted inputs in the ordinary work graph.
Only checked constructors publish accepted conclusions. A descriptive record
loaded from an image is not an acceptance capability.

Different derivations may establish one conclusion. Do not select a canonical
proof by overwriting alternatives or make semantic child identity depend on
which derivation arrived first. Proof premises can still reference different
derivations of the same child conclusion. This legitimate distinction is not
eliminated to save an array; avoid copying identical structural payloads.

### 3.2 Structural views and context action

Expose checked views of applications, binders, constructor fields, eliminators,
and classifier formation. A view obtains children and scopes from typed
structure, not by peeling an arbitrary Evidence history at every call site.

Context substitution acts on the typed subject, classifier, annotation and
child scopes together. Prefer a shared lazy `(typed subject, substitution)`
request over eagerly copying the reachable graph. Compose maps using the
existing substitution machinery, lift under binders, and cache by exact inputs.
The view must make the effective child context explicit. Repeated queries
reuse completed work; unfinished queries remain budgeted Solve requests.

Conversion/effect subsumption changes a judgement boundary, not the program's
construction or lexical binding. Keep the original annotated construction and
its checked relation to the target classifier; do not retag all children as if
they had been synthesized at that target. Normalization is different: it can
change the actual term constructor. Produce/check the normalized typed
structure using the existing typed reduction work, retaining its receipt;
never attach the source's operand list to the result as its current children.

No global search from erased Core to "some matching proof" is permitted.
Core and typed evidence for every returned view must correspond in the same
effective context, allowing explicitly checked conversion where needed.

### 3.3 What remains in Evidence

Keep logical rule identity, premise DAGs, ownership, conversion/reduction
receipts, and required nominal/allocation evidence. Context and substitution
judgements need not be encoded as executable Terms. Keep APP, Match, induction
and handler rules distinct: they validate different obligations.

Retaining a derivation is not object-language proof relevance. Two Identity
witness Terms are not identified just because their typing certificates are
organized alike. Conversely, eliminating redundant certificate traversal must
not delete the user's proof Terms or their higher Identity structure.

Do not replace the certificate pointer with dozens of unrelated structs as an
end in itself. Move semantic construction data to its existing owner/typed
record; keep residual certificate access rule-checked. Internal storage changes
must remove a concrete ambiguity or duplication, not merely relocate a switch.

## 4. Implementation and Progress

Each phase requires its evidence and deletion review before checking it off.
Temporary adapters are permitted within a phase, not as permanent fallbacks.

| State | Phase | Changes and completion evidence |
|---|---|---|
| [x] | R0 Contracts and baseline | Baseline `4657cc6` debug acceptance, LOC ledger and source/image timing/work/memory matrix are recorded below. The R35 publisher audit covers each node producer and the structural consumers; the role/scope table and deletion ledger specify their contracts and remaining migrations. This closes the audit, not R2-R5 implementation or the final measurements. |
| [x] | R1 Typed conclusions | Term conclusions have one typed-subject reference; classifier/context/sort accessors delegate to it. All rules publish through `accept_record`; Context and Substitution retain their distinct conclusions. Exact interning and alternative derivations are preserved. Verified by the R29 structural audit and acceptance tests below; this does not complete R2/R3. |
| [ ] | R2 Context action | Migrate projection/reindex to the same typed structure with explicit effective child maps. Use existing substitution work and binder lifting. Verify repeated lookup sharing, capture avoidance, dependent classifiers and chunked execution. |
| [ ] | R3 Structural and formation consumers | Move `return_value_origin`, `pi_component`, constructor/inductive recovery and classifier recovery to checked views. Keep theorem-specific inversions where required. Replace structural wrapper walks in `function_graph.c`, `action.c` and `synthesis.c`; remove replaced paths in the same phase. |
| [x] | R4 Images and pending work | R47 audits the current transport after R40/R46: APGOCC7 preserves descriptive typed structure, Context v3 retains family telescopes, derivation v14 and source v44/v45 preserve rule/allocation inputs. Old formats reject. Fresh-process tests check ordinary Solve, no imported acceptance bits, split budgets and normalized scoped inputs. Future representation changes must reopen this transport gate; this does not close R2/R3/R5. |
| [ ] | R5 Acceptance and cleanup | Run the full gates below, remove obsolete occurrence fields/adapters, document remaining intentional rule dispatch. Require net implementation LOC reduction, compare behavior and performance to R0, report per-file additions/deletions, then publish the verified increment. |

R1 and R2 form one vertical slice: first exercise an annotated Lambda/APP under
dependent substitution, conversion and normalization. This is an implementation
checkpoint, not permission to keep two representations indefinitely. Extend
the same contract through constructor/Match/IH, F/U/fold/request/handler and
Identity before claiming R3 complete. Do not add new language features meanwhile.

## 5. Required Tests

- Same erased Core, different Bool/Nat annotations: distinct checked typed uses;
  repeated identical requests share structure. Add direct API cases because
  separately parsed lambdas need not intern to the same Core pointer.
- Two derivations of one typed subject: both remain valid; structural results
  and generated graph meaning do not depend on their discovery order.
- APP under projection, reindex, conversion and effect widening: same semantic
  operands as a directly constructed counterpart, with correct effective scope.
- A dependent Lambda/constructor field: substitution changes Core and classifier
  coherently; binder shadowing/capture and nominal type confusion still reject.
- Typed beta/iota normalization: result views describe the result, not source
  operand history. Existing conversion receipts and fixed profiles remain required.
- Match/IH with captured and indexed parameters, raw-Pi results and recursive
  fields; length and QuickSort generated witnesses/properties retain results.
- Reflexivity, dependent Pi action, transport/lifting and existing higher
  examples: witness Terms survive context action and fresh-process images.
  No new higher coherence claim is inferred from passing these regressions.
- Effects: typed introspection executes no requests; handlers still intercept
  the same operations and runtime invocation counts remain unchanged.
- Zero/partial/completed image saves, retained reductions and recompute modes:
  same accepted results for chunks one and 64; incorrect operands, classifiers,
  scopes, nominal references and receipts do not publish accepted conclusions.
- Preserve `::` post-checking, `#Name` default spelling, legacy option behavior,
  import checking and current negative cases. Root-only `{{...}}.name` is not
  a missing nested-expression feature: both old and current parsers restrict it.

Reuse existing suites (`core`, `synthesis`, `iadt`, `identity`, `program`,
`occurrence_io`, `source_io`, `identity_io`, `execution`, and compatibility).
Avoid a duplicate audit runner or a test matrix that only counts status tags.

```sh
make -f src/prototype/pointer/Makefile check-acceptance
make -f src/prototype/pointer/Makefile check-acceptance BUILD=/tmp/a-program-typed-structure-debug CFLAGS='-std=c11 -Wall -Wextra -Werror -O0 -g'
make -f src/prototype/pointer/Makefile check-acceptance BUILD=/tmp/a-program-typed-structure-sanitize CFLAGS='-std=c11 -Wall -Wextra -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'
```

## 6. Completion and Reporting

Measure the same inputs/build flags before and after: examples 01-09, Vec
append, imported dependent Sigma, generated length and QuickSort property
source/images. Report elapsed time, peak RSS, Solve transitions and available
term/occurrence/proof counts. Separate first construction from repeated view
queries; the latter must not rescan the full derivation history. Explain any
material regression instead of trading correctness for a smaller counter.

R3 is not complete if new views merely hide the old recursive Evidence walks.
Give a removal/retention table for each function named in section 2, with a
reason for any remaining proof-rule dispatch. Logical rules and derivation
validation are legitimate remaining dispatches.

Implementation LOC reduction is an acceptance target, not just a reporting
preference. Removing reconstruction should remove code, not move it behind a
new interface. R0 must list the obsolete recovery paths and their replacement
cost; every migration phase records actual deletions alongside additions.

- [ ] Delete superseded structural recovery walks and temporary adapters.
- [ ] Obtain net-negative implementation/header LOC across the complete change,
  including newly added modules and image/support code. Moving code does not
  count as deletion; shrinking tests/docs does not offset implementation growth.
- [ ] If the final implementation grows, reopen the representation/ownership
  design and explain why; do not mark R5 complete without a reviewed exception.
- [ ] Preserve verification coverage and logical checks. Do not achieve the
  target through compressed formatting, removed diagnostics, disabled tests,
  or encoding unrelated kernel rules into a generic operation.

Report per-file `+added/-deleted/net`, separating implementation/headers,
tests/fixtures, build changes and documentation. A vertical slice may grow
temporarily, but it must name the remaining deletions. Do not invent a numeric
reduction estimate before R0 or hide growth in generated files/new modules.

The full parent goal remains active. This refactor neither resolves the
all-refuted Match motive policy nor completes general Higher Identity.
The user's implementation instruction supersedes the parent's frozen-Main
publication policy: after R0-R5 and verification, publish the completed work
to Main. Preserve the previous version's Git references; do not force-push over
unrelated remote work. Until then, checkpoints stay on `rewrite/pointer-core-hott`.
No implementation outside `src/prototype/` is authorized by this plan.

## 7. Implementation Log

### 2026-09-16: Conclusion ownership

- [x] Establish diagnostic baseline `4657cc6`; debug `check-acceptance` passed.
- [x] Put classifier and judgement/sort in the existing interned Occurrence.
  Context was already there. Exact keys now include all three; this does not
  introduce another program graph or computational constructor tag.
- [x] Remove the duplicate conclusion fields from Evidence. A term derivation
  points to its Occurrence; non-term context/substitution derivations retain
  their scope. Rule/premise/certificate data remains independent.
- [x] Migrate all accepted occurrence producers and boundary changes. Conversion,
  effect widening and type/value views preserve children but obtain the new
  conclusion. Interner allocation is still not acceptance.
- [x] Remove redundant classifier/sort arguments from private proof interning.
  Premise-determined rules retain their early lookup before fresh binder work.
- [x] Embed the occurrence interning entry in its allocation; remove the
  separate entry-to-occurrence allocation and pointer indirection.
- [x] Add tests for different classifiers/sorts over shared Core, unchanged
  boundary interning, and distinct derivations sharing the same conclusion.
- [x] Version descriptive occurrence transport as `APGOCC1`: sort, classifier,
  annotation and operand identity survive relocation; old `APGOCC` is rejected.
  Derivation/source images still reconstruct through the existing Solve rules.
- [ ] Complete R1/R2 scoped structural edge contract and run all final gates.

Debug `check-acceptance` passed after classifier/sort/scope migration and again
after the allocation consolidation, including 63/63 compatibility cases and
the generated QuickSort property source/image tests. These are partial
checkpoints, not completion of R3 or the net-negative LOC requirement.
ASan/UBSan `core_test` and fresh-process `occurrence_io.sh` also passed for this
checkpoint. Full optimized/sanitized acceptance remains a final R5 gate.

Single-run diagnostic measurements with the same `-O0 -g` build (not a stable
speedup claim), `pointer-check --legacy-intrinsic-dot --steps 1000000 --imports
src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p
src/prototype/pointer/tests/acceptance/legacy-quicksort-property.p`:

| Measure | Baseline | Conclusion slice |
|---|---:|---:|
| Solve transitions | 149501 | 149501 |
| Core Terms | 168628 | 168628 |
| Derivations | 588033 | 588033 |
| Contexts | 16079 | 16079 |
| Occurrences | 435774 | 442735 |
| Elapsed seconds | 0.890 | 0.885 |
| Peak RSS, KiB | 225532 | 224736 |

The extra 6961 occurrences distinguish boundary classifiers/sorts previously
stored only in Evidence. Before removing the separate index allocation, this
slice used 237716 KiB; the consolidation removed that memory regression. Counts
were inspected at `pg_program_destroy`; elapsed/RSS came from subprocess timing
and child resource usage, not extra compiler instrumentation. Generated length
property checking used 10950 Solve transitions both before and after. Repeat
the complete matrix at R5, including retained/recomputed image modes.

### Structural producer contracts (current audit)

| Producer family | Current operands | Required interpretation |
|---|---|---|
| Universe, host leaf, variable, nominal family | None | Leaf; classifier/context remain part of the typed use |
| APP, family APP, RETURN, THUNK, FORCE | APP `[function, argument]`; unary terms `[content]` | Ordered children in the enclosing scope; each term introduction retains its classifier formation |
| Request, zero-clause Fold, handler | `[payload, continuation]`; `[input, continuation]`; `[input, return clause, operation clauses...]` | Clause functions are typed inputs in the enclosing context. Their nested Lambda bodies retain lexical scopes; labels remain semantic-owner inputs |
| Lambda/family abstraction | `[body]` in an extended context | The body edge must retain its lexical extension when a map is lifted |
| Pi | `[value-domain formation or declared family variable, codomain]` | Value domain is in the parent scope; family variable and codomain are in the extended scope. A logical signature is not an ordinary value type |
| F/U formation | `[value type]` / `[computation type]` | Enclosing context. Effect row and totality grade remain explicit semantic-owner inputs, not extra proof children |
| Constructor | Field occurrences and retained classifier formation | The classifier retains the instantiated nominal family. Recover its exact formation and parameter map through typed structure; fields are not read from arbitrary proof premises |
| Match/induction | `[scrutinee, branches..., motive, nominal formation]`; one parameter map | Scrutinee/branch functions are in the enclosing context; motive has its explicit telescope and nominal formation retains its declaration context. Recursive allocation binders/clause contexts belong to the typed node, not a second Evidence payload |
| Type case | `[scrutinee, branch families...]` | Enclosing context; these are type-valued branches, not runtime handler clauses. Checking still needs the nominal declaration and branch telescopes |
| Identity formation/instance | `[type or relation witness, left, right]` | Enclosing context; the type/relation distinction is typed, not inferred from erased APP arity |
| Identity endpoints, reflexivity, transport/lift | `[relation witness]`, `[term]`, `[relation witness, value]` respectively | Enclosing context. Endpoint inversion retains its relation witness; reflexivity retains its formed Identity separately as the classifier |
| Termination and total-result | Termination type `[thunk type, suspended]`, witness `[suspended]`, total-result `[computation]` | Enclosing context. No purity or termination follows from descriptive allocation |
| Selected family Identity/action | `[family, paths..., left, right]` and two maps; action `[source term, formed Identity]` | The family/source retain the source context of the maps; paths/endpoints live in their common destination. No equality reflection |
| Type/value view | Original typed subject; no copied inputs/maps/allocation | Exact type/value round trips reuse the original subject; both derivations remain. Restriction restores the requested sort through the ordinary rules |
| Conversion, widening | Original typed subject and target formation; no copied operands | Reference the original construction even if only the classifier formation changes, not its Core. Attaching descriptive formation metadata during image read is a separate operation |
| Projection/reindex | Origin and typed context map; no direct operands | Effective inputs use shared context action, lifting under actual lexical binders. A substituted variable selects its typed image |
| Pi/F/U type-component inversion | Source, selected ordinal, optional typed argument | Explicit selection recipe, not the result's children. A scoped codomain is instantiated by the argument, or restricted when constant |
| Pure normalization and term-content inversion | Origin without stale direct operands | Result recipe, not immediate result structure. Congruent NF exposes checked direct and mapped inputs through shared input queries and receipts. Head-changing results and budgeted certification remain open |

The last row prevents claiming the migration is complete: the erased reducer
can finish while the typed result still needs exposure. Preserve this distinction
when adding result views; never reuse source operands under a changed Core.
Pending synthesis rule inputs are still unaccepted requests. Their rule dispatch
and `derivation.c`'s checking are not redundant stored program representations.
In particular, do not remove either just because accepted structural consumers
no longer need to walk Evidence wrappers.

### Deletion ledger

Current state, updated after typed nominal recovery and structural rebasing. Earlier
checkpoint notes below are historical, not additional completion claims.

| Existing path | State / next action |
|---|---|
| Evidence classifier/context/sort copies | Removed for term conclusions |
| Checked family telescope lifting | R41 removes `lift_frame`/`lift_index` and the separate signature walk. The existing temporary DAG checks prefix/signature maps from shared structural lifting; ordinary context formation and substitution pairing remain. Explicit requests preserve their supplied premise derivations |
| Normalized-input context action | R42 removes the private Lambda/Pi lifting path. R43 removes the temporary `input_frame` walk and the synchronous `normalized_input` spine loop. R45 exposes supported head-changing beta/iota results through that same typed-body machine, then checks them against the receipt before selecting children. Structural traversal resumes its existing map spine. General multi-phase NF, recursive IH exposure and dependent field-classifier transport remain open; individual kernel checks and receipt lookup remain synchronous |
| `return_value_origin`, `pg_prove_application_body` | Synchronous adapters to one indexed typed-body machine; separate Return/Fold traversal removed. R37 adds nonrecursive Match branch selection and shared computed-scrutinee dependencies. R44 deletes the private `typed_body_frame` continuation stack: nested APP, Fold prefix/continuation and computed returned values use shared queries. Nominal/family recovery advances application work incrementally; this exposes checked source bodies, not arbitrary NF children |
| `pi_component` | Deleted; selection source/ordinal/argument drive `selected_formation` |
| `pi_argument_frames` | Retained checked binder substitution; no Pi-premise layout dependency |
| `inductive_recovery_step` | Reads typed origins/maps, selections and family inputs. R33 removes its private Return/Thunk counters and Fold continuation stack; computed results and applications share indexed typed-body work. Exact nominal formation and synchronous kernel suboperations remain |
| `rebase_image` | Reads typed maps/origins and constructor/family inputs; rechecks introductions in the target context. Retains normalization receipts and nominal formation checks, not wrapper-history dispatch; synchronous work remains |
| `constructor_origin`, `pg_prove_elimination_body` | Whole-introduction reconstruction deleted in R31. A temporary constructor view exposes retained nominal formation, parameters and typed fields for Match/refinement; actual introduction in a changed context remains checked. Congruent NF field selection uses receipts; general current-result exposure remains open |
| `classifier_leaf` | Deleted |
| `classifier_recovery_step` | Uses the shared typed classifier query; separate map stack removed. Variable/Universe formation and final synchronous certification remain legitimate checks |
| `function_graph.c:computation_origin` | Deleted in favor of shared checked construction access |
| `typed_construction` | Deleted in R30. Conversion/widening retain their source typed use; checked construction access retrieves existing evidence. R32 removes operand/map/allocation copying from sort and content boundaries too. General result exposure remains open |
| Metadata attachment copies | R34 deletes `pg_occurrence_classified`, `pg_occurrence_with_maps` and `pg_occurrence_with_induction`. Match/Identity construction and image read intern complete tuples once. Inversion may still attach a newly obtained classifier to an existing descriptive selection; that distinct formation is not discarded |
| `action.c:origin_step` | Reads typed origins/maps; does not reinterpret derivation wrappers |
| `action.c:identity_structure` | R35 removes repeated introduction of the same Identity theorem. Looks up a compatible accepted formation of the exact typed subject; different derivations remain available. Checked transport/rebuilding in a changed context is still required |
| `derivation.c:pg_prove_derivation` | Intentionally retained: verifies rule inputs rather than interpreting program structure |

Baseline source LOC at `4657cc6`: `evidence.c` 4343, `typing.c` 129,
`occurrence_io.c` 114, `function_graph.c` 1636, `action.c` 839,
`derivation.c` 248. Compare all new/support modules as well, not just these files.
The first conclusion slice grows temporarily; no structural recovery deletion
has been credited yet. R5 remains unchecked until the complete replacement is
net-negative and the performance/behavior comparisons are recorded.

Conclusion checkpoint LOC delta (against `4657cc6`, excluding documentation):

| File, under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| `evidence.c` | 618 | 610 | +8 |
| `evidence.h` | 0 | 3 | -3 |
| `typing.c` | 21 | 12 | +9 |
| `typing.h` | 15 | 4 | +11 |
| `occurrence_io.c` | 20 | 14 | +6 |
| `occurrence_io.h` | 3 | 2 | +1 |
| **Implementation subtotal** | **677** | **645** | **+32** |
| `tests/core.c` | 51 | 17 | +34 |
| `tests/derivation_io.c` | 3 | 1 | +2 |
| `tests/occurrence_io.c` | 16 | 8 | +8 |

### 2026-09-16: Shared structural context action

- [x] Intern structural context maps by source/destination contexts and typed
  image pointers. Keep the checked substitution derivation separate: creating
  a descriptive map does not accept a substitution. Different derivations of
  the same images share the map without discarding either derivation.
- [x] Remove raw binding arrays from substitution Evidence allocations. The
  erased binding array is an immutable projection owned by the structural map.
  Dependent image checks still run before the substitution proof is accepted.
- [x] Share budgeted occurrence action by exact `(map, occurrence)` inputs,
  using the existing Core substitution work. Evidence reindex no longer owns
  another three-term substitution state machine.
- [x] Keep prefix projection structural: do not create substitution proofs or
  evaluator jobs merely to record unchanged Core/classifier data in a larger
  scope. It uses the same action-result constructor as computed substitution.
  A regression test requires exactly one new projection proof and no new
  occurrence-action jobs; explicit checked substitution yields the same subject.
- [x] Remove `action.c:projection_substitution`; Identity construction now
  calls the existing checked projection constructor rather than duplicating it.
- [x] Stop copying old children into projection/reindex conclusions. A mapped
  construction retains its origin and context map, with no direct child array.
  Substitution of a variable instead exposes the image construction, including
  its own map if needed. Boundary changes preserve this structural description.
- [x] Transport these dependencies with `APGOCC2`. Origins and typed map images
  participate in the occurrence DAG; scope and Core relocation remain shared.
  `APGOCC1` and older occurrence images reject rather than silently lose maps.
  Ordinary source/derivation Solve remains the acceptance path.
- [x] Test different proof paths sharing one map/action; pending and completed
  work reuse; dependent classifier substitution; variable-to-function images;
  fresh binder allocation; descriptive map creation without acceptance; and
  fresh-process mapped occurrence transport with shared maps and no Evidence.
- [ ] Expose effective children through lifted maps, using the actual binder
  allocated by Core substitution. A map record alone does not finish this.
- [ ] Replace structural Evidence consumers and normalized/inverted provenance
  operands. No R3 deletion is claimed merely for moving substitution work.
- [ ] Complete the full acceptance, sanitizer and performance checks for this
  slice; record actual deltas before marking R2 complete.

The function-image test distinguishes exact structural sharing from alpha
comparison: substituting a dependent function classifier can allocate a fresh
binder, so the resulting classifier need not be pointer-identical to the
image's classifier. Its alpha equality is checked separately. This is not a
reason to alpha-intern Core or select a different proof by erased Term identity.

Core and fresh-process occurrence transport passed ASan/UBSan for this slice.
The full debug acceptance gate passed before and after projection's allocation
fix, including 63/63 compatibility and source/image QuickSort property cases.
Full sanitized/optimized acceptance remains R5.

Performance review caught and rejected an expensive first projection design:
constructing a checked substitution just for the structural map raised the
QuickSort property's derivations from 588033 to 996207, action jobs to 493066,
elapsed time to 3.92 seconds and peak RSS to 356004 KiB. Removing those unused
proofs/jobs restored the derivation count to 588033. The current diagnostic run
has 462743 occurrences, 41480 maps, 108591 occurrence-action jobs, 16079 contexts,
168635 Core terms and 149471 Solve transitions. Elapsed time is 1.57 seconds,
peak RSS 270808 KiB, versus a repeated R1 run of 0.91 seconds / 224292 KiB.

This remaining regression is **not accepted as the final outcome**. Structural
maps/jobs now coexist with the old history-based consumers; R3 must remove
that reconstruction rather than treating the extra records as a finished
optimization. R5 must remeasure retained memory and query cost. The final
net-negative implementation LOC requirement is unchanged.

Context-action checkpoint delta against `f644889` (all paths below are under
`src/prototype/pointer/`; this is an incomplete migration, not a reduction claim):

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `action.c` | 7 | 24 | -17 |
| `evidence.c` | 77 | 77 | 0 |
| `evidence.h` | 2 | 0 | +2 |
| `typing.c` | 183 | 4 | +179 |
| `typing.h` | 34 | 0 | +34 |
| `occurrence_io.c` | 26 | 9 | +17 |
| `occurrence_io.h` | 2 | 2 | 0 |
| **Implementation subtotal** | **331** | **116** | **+215** |
| `tests/core.c` | 70 | 0 | +70 |
| `tests/occurrence_io.c` | 22 | 5 | +17 |

### 2026-09-16: Declared binder sorts and scoped map lifting

- [x] Store the existing VALUE/TYPE_FAMILY distinction in the immutable Context
  declaration and its exact interning key. Neither a new Core constructor nor
  a new logical judgement is introduced. Context allocation remains descriptive;
  ordinary extension rules still check formation, universe and freshness.
- [x] Remove the Evidence-chain walk used to recover a variable's binder sort.
  Build projection maps and logical signatures from Context structure rather
  than decoding extension proof tags. Proof premises are retained for checking.
- [x] Replace Pi's synthetic unclassified domain operand. A value binder stores
  its actual checked domain formation; a family binder stores its typed variable
  in the extended scope. Both retain the checked codomain. No fictitious
  Universe judgement is assigned to a logical family signature.
- [x] Lift structural maps under an explicitly supplied target binder, including
  the binder allocated by Core substitution. Transport the dependent domain,
  project prefix images and append the new variable. This creates no Evidence.
- [x] Preserve declaration kinds through context/declaration/source transport:
  `APGCTX2`, `data-declaration/v2`, derivation version 13, source versions 40/41
  (recompute/retained reductions). Previous affected versions reject explicitly.
  Indexed source restoration retains the pre-Self signature's binder pointers
  and the saved telescope's declaration kinds, then uses ordinary Solve.
- [x] Test exact interning across different binder kinds; actual substituted
  Lambda-body binders and dependent classifiers; family lifting; fresh-process
  context sharing; invalid declaration kinds, old context versions and truncation.
- [ ] Consume these maps in effective-child views and remove the major recovery
  walks. This checkpoint does not complete R2/R3 or satisfy the LOC target.

The family-lift test exposed an important allocation distinction: the existing
checked family lift freshly constructs its signature-local index telescope,
whereas structural signature substitution may retain different binder pointers.
Their signatures are alpha-equivalent, not necessarily pointer-identical. They
must not be interned together on that basis. The next checked-child migration
must align with the actual allocated target telescope; it cannot use pointer
inequality as rejection or silently choose an unrelated context.

The full debug acceptance gate passed, including 63/63 compatibility and the
QuickSort source/image property cases. ASan/UBSan core, context/declaration and
occurrence transport tests passed. A diagnostic QuickSort run remained at
149471 transitions, approximately 1.52 seconds and 270180 KiB; this is not a
stable speedup claim and does not resolve the R1-to-R2 performance regression.
Full optimized/sanitized acceptance and final performance comparison remain R5.

Checkpoint delta against `fc97aae`, excluding documentation:

| File, under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| `typing.c` | 49 | 2 | +47 |
| `typing.h` | 9 | 1 | +8 |
| `evidence.c` | 20 | 33 | -13 |
| `evidence.h` | 4 | 1 | +3 |
| `context_payload.c` | 18 | 11 | +7 |
| `context_payload.h` | 3 | 2 | +1 |
| `context_io.c` | 1 | 1 | 0 |
| `declaration_io.c` | 2 | 2 | 0 |
| `derivation_io.c` | 4 | 5 | -1 |
| `source_io.c` | 2 | 2 | 0 |
| `source_io.h` | 4 | 3 | +1 |
| `synthesis.c` | 16 | 5 | +11 |
| **Implementation subtotal** | **132** | **68** | **+64** |
| Tests (seven files, including migrated API calls) | 95 | 38 | +57 |

The accumulated implementation delta is still positive (+311 lines against
the diagnostic baseline). No net reduction or final completion is claimed.

### 2026-09-16: Effective inputs and explicit derived results

- [x] Add a shared `(typed subject, input index)` request, with resumable map
  traversal and explicit ready/unavailable/error outcomes. Reading structure
  creates no acceptance evidence. A repeated completed request does no work.
- [x] Lift a Lambda body's input through the actual target Core binder, using
  the shared domain substitution and context-map construction. Do not alpha
  intern binders or expose a body in its old context.
- [x] Connect RETURN/THUNK content inversion to effective inputs. The resulting
  subject shares the actual child structure when available; its certificate
  still records the inversion premise. No erased-Core proof search is used.
- [x] Represent an unresolved derived result by `origin` without `map`, not by
  putting its predecessor in `operands`. Normalization and the unrecovered
  Pi-domain result use this representation. It is explicit provenance, **not**
  a completed typed normalizer: input queries return unavailable for it.
- [x] Transport derived origins with sharing in `APGOCC3`; reject older
  occurrence formats. Source/derivation inputs continue through ordinary Solve.
- [x] Cover 10,000 mapped layers without recursive C traversal, zero/chunk-one
  budgets, repeated lookup, mapped/projected RETURN inputs, the actual mapped
  Lambda binder, missing inputs, and fresh-process derived-origin transport.
- [x] Add a regression for preserving a low-universe Pi domain after removing
  an unused outer binder.
- [ ] Complete typed result exposure, including Pi scopes and strengthening,
  before removing the associated Evidence recovery. This slice does not
  complete R2/R3. The existing major recovery walks have not been deleted.
- [x] Full debug acceptance gate, including 63/63 compatibility and QuickSort
  source/image properties; ASan/UBSan core, IADT and occurrence transport tests.

An attempted replacement of `pg_prove_pi_domain`'s history recovery passed the
existing full debug gate but failed the new nested-universe regression. For
`P = Pi(x : Nat, F(Universe 2))`, removing an unused binder from
`Pi(z : Nat, P)` must still expose `Nat : Universe 0`; inheriting `P`'s bound
instead yields `Universe 3`. That attempted replacement was withdrawn, rather
than weakening the test or silently widening the classifier. The old domain
recovery remains pending migration. A direct domain test checks the typed
subject and ordinary rule reconstruction, not accidental proof-pointer identity.

This establishes an additional R3 prerequisite: strengthening is not a total
context substitution with a fabricated image for the removed variable. A typed
result view must preserve the chosen domain formation through codomain
application and removal of unused binders, including dependent metadata. Do not
implement it by assigning every recovered type the parent's universe bound.

Checkpoint delta against `e8b2fb6`, excluding documentation:

| File, under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| `typing.c` | 146 | 4 | +142 |
| `typing.h` | 19 | 1 | +18 |
| `evidence.c` | 26 | 9 | +17 |
| `occurrence_io.c` | 15 | 4 | +11 |
| `occurrence_io.h` | 3 | 2 | +1 |
| **Implementation subtotal** | **209** | **20** | **+189** |
| Tests (three files) | 79 | 7 | +72 |

The accumulated implementation delta is **+500**, not a reduction. The final
net-negative implementation requirement is unchanged. Old recovery plus new
views is an incomplete migration, not the accepted final architecture.

The diagnostic QuickSort command completed in 149471 transitions, 1.47 seconds
and 270540 KiB peak RSS (single debug run, not a statistically established
improvement). Counts were 461284 occurrences, 588033 derivations, 41480 maps,
108673 occurrence-action requests, 667 input requests, 16079 contexts and
168935 Core terms. The earlier R1-to-R2 performance regression is still open;
full optimized/sanitized acceptance and comparative measurements remain R5.

### 2026-09-16: Scoped Pi inputs and codomain instantiation

- [x] Extend the shared input query to Pi codomains, using the existing Pi
  semantic owner's view. Core remains Lambda/Application/Reference. The
  standalone occurrence transport target now links that owner implementation.
- [x] Instantiate a scoped typed body by extending the identity context map
  with the argument, then use the existing occurrence-action request. The
  kernel still checks the argument's classifier; structural allocation alone
  grants no acceptance. Pi codomain inversion retains this result's structure
  and records its ordinary Pi/argument premises separately.
- [x] Handle a projection whose destination already contains the source's
  bound pointer (found in the open Acc regression). Opening the body freshens
  that binder and lifts the map accordingly. Compare the **closed** binder/body
  with the parent's binding; comparing free bodies would be incorrect. Repeated
  queries reuse the same chosen binder. This does not alpha-intern Core.
- [x] Stop storing a constant-codomain predecessor as its current child. It
  remains an explicit derived result until typed strengthening is implemented.
- [x] Test scope collision, mapped Pi binders, argument-scope rejection,
  chunked instantiation, repeated requests, reconstructed codomain derivations,
  and the retained low-universe domain after instantiating an outer Pi.
- [x] Full debug acceptance (including 63/63 compatibility and QuickSort
  source/images), plus ASan/UBSan core, IADT and occurrence transport checks.
- [ ] Complete strengthening and normalized-result exposure; remove the
  unavailable-input codomain adapter and remaining Evidence recovery. Neither
  R2 nor R3 is complete merely because direct/mapped Pi inputs now work.

Checkpoint delta against `b1fdb02` (paths under `src/prototype/pointer/`):

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `typing.c` | 43 | 7 | +36 |
| `typing.h` | 8 | 2 | +6 |
| `evidence.c` | 23 | 6 | +17 |
| **Implementation subtotal** | **74** | **15** | **+59** |
| `Makefile` | 2 | 2 | 0 |
| Tests (two files) | 49 | 0 | +49 |

The diagnostic QuickSort run used 201265 Solve transitions, 1.32 seconds and
249684 KiB peak RSS; the previous checkpoint used 149471 transitions, 1.47
seconds and 270540 KiB. These single debug runs do not establish a stable
speedup. Transition counts increased while wall time and retained memory fell;
they are not interchangeable measures of work. Counts are 424369 occurrences,
522142 derivations, 41981 maps, 104964 occurrence-action requests, 1601 input
requests, 16052 contexts and 183346 Core terms. Final comparisons remain R5.
Implementation LOC is still **+559** against the diagnostic baseline; no net
reduction or completed migration is claimed.

### 2026-09-16: Cancel retained Pi weakenings

- [x] Cancel exact structural projections to a prefix context, retaining the
  original typed construction. This does not assign a fictitious image to a
  removed binder. Nonidentity maps, changed boundaries and unrelated contexts
  are unavailable; repeated queries do not allocate more occurrences/proofs.
- [x] Use this cancellation for constant Pi codomains and consume the retained
  domain formation through the ordinary `PG_PI_DOMAIN` inversion. The chosen
  domain's universe is preserved rather than inherited from the larger Pi.
- [x] Share the blocking adapter for existing budgeted input requests. No new
  structural authority, Core tag or proof rule was introduced.
- [x] Keep separate regression cases for a projected inner Pi and an inner Pi
  constructed directly under the removed binder. Only the first is an inverse
  of recorded weakening. The latter still requires general strengthening;
  replacing its domain by the enclosing universe bound is not valid.
- [x] Full debug acceptance, including compatibility and QuickSort source/image
  cases; ASan/UBSan Core, IADT and fresh-process occurrence transport passed.
- [ ] Remove general Pi recovery after scoped strengthening and typed reduction
  are available. `pi_component` and `pi_argument_frames` remain in use, including
  nominal-family recovery; this checkpoint does not claim their removal.

Delta against `28068be` (paths under `src/prototype/pointer/`):

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `evidence.c` | 33 | 12 | +21 |
| `typing.c` | 18 | 0 | +18 |
| `typing.h` | 5 | 0 | +5 |
| **Implementation subtotal** | **56** | **12** | **+44** |
| `tests/core.c` | 12 | 0 | +12 |
| `tests/iadt.c` | 14 | 0 | +14 |

Implementation growth is now **+603** against the diagnostic baseline. The
net-negative R5 condition remains unmet. A diagnostic QuickSort run used 201138
transitions, 1.318 seconds and 243328 KiB peak RSS, against 201265 transitions,
1.32 seconds and 249684 KiB at the preceding checkpoint. These single debug
runs do not establish a stable speedup. Full final comparative gates and Main
publication remain pending.

### 2026-09-16: Checked structural inputs for function graphs

- [x] Index existing derivations by their exact typed conclusion, retaining
  every alternative in acceptance order. This is a lookup index over the same
  receipts, not a new Claim authority, a Core-to-type search or an image trust
  bit. Only the existing acceptance routine appends receipts; index linkage is
  not a mutation of logical premises. Reading an index publishes no proof.
- [x] Resolve a mapped typed subject by its structural dependency DAG and the
  ordinary variable/substitution/reindex rules. Source/destination contexts
  must already be accepted, every image is checked, and the resulting subject
  must match exactly. Invalid maps/boundaries and foreign stores do not grant
  acceptance. Repeated successful requests reuse their accepted receipts.
- [x] Obtain APP, RETURN, THUNK, FORCE and zero-clause Fold inputs in function
  graph construction from typed structure. Remove the duplicate basic-operation
  premise dispatch in `computation_view`. Match/IH and unavailable normalized
  constructions still require `computation_origin`; its deletion is pending.
- [x] Preserve a substituted variable's source/map when the image classifier
  and substituted classifier have different binder pointers. The input query
  follows the typed image, without alpha-interning either classifier. Previously
  a bare image boundary lost the acceptance recipe. Removing the old basic
  operation path exposed this as a rejected QuickSort property and an unexpected
  one-case generated graph. The regression was fixed, not accepted as a change
  to the graph API or hidden by restoring the old child-dispatch path.
- [x] Test alternative receipts, wrong boundaries/maps, store ownership,
  newly certified projections, alpha-distinct classifier provenance, repeated
  queries and fresh-process mapped-variable input transport without Evidence.
- [x] Full debug acceptance, including 63/63 compatibility and QuickSort
  source/image properties; ASan/UBSan Core, IADT, occurrence transport and
  QuickSort property compilation passed. The final added transport fixture
  also passed separately under sanitizers; it changes no implementation code.
- [ ] Migrate the remaining scoped/normalized/nominal inputs and their
  consumers. The structural receipt helper does not invent a context, infer
  arbitrary descriptive nodes or implement typed normalization. R3 is not done.

Delta against `9553615` (paths under `src/prototype/pointer/`):

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `evidence.c` | 115 | 0 | +115 |
| `evidence.h` | 12 | 0 | +12 |
| `function_graph.c` | 45 | 13 | +32 |
| `typing.c` | 17 | 1 | +16 |
| `typing.h` | 3 | 0 | +3 |
| **Implementation subtotal** | **192** | **14** | **+178** |
| Tests (two files) | 87 | 7 | +80 |

The implementation delta remains positive at **+781**. A single debug QuickSort
run used 201138 transitions, 1.489 seconds and 274532 KiB peak RSS, versus
1.318 seconds / 243328 KiB at the preceding checkpoint. Counts are 409090 typed
occurrences, 504226 derivations, 411458 conclusion-index keys, 39571 context
maps, 102871 occurrence actions, 2934 input queries, 14970 contexts and 179019
Core terms. The added index cost and remaining legacy consumers must be
included in R5's memory/time review; this is not a final performance acceptance
or a net-reduction claim. Main publication remains pending.

### 2026-09-17: Beta and returned-value structural access

- [x] Replace the proof-rule walks in `pg_prove_application_body` and
  `return_value_origin` with typed construction inputs and retained context
  maps. Lambda and family abstraction share the Core Lambda/body contract;
  APP, FORCE/THUNK, RETURN and sequencing are identified from their actual
  Core structure and matching typed edges, not from a selected receipt's rule.
  The old dispatches are deleted, not retained as fallback implementations.
- [x] Share traversal of structural origins/maps between the two consumers.
  A normalization origin is a recipe for the unreduced computation, never
  claimed to be the normalized result's children. Beta still uses ordinary
  checked substitution/pair/reindex. Returned-value recovery resumes only
  actual Return nodes; it neither executes requests nor guesses a value from
  a totality contract.
- [x] Factor certification of structural maps out of subject certification.
  The first QuickSort trial exposed descriptive projection images without
  receipts. Certify these through the existing variable/substitution rules;
  do not treat allocation of a structural map as acceptance. Source and
  destination contexts must still already have accepted evidence.
- [x] Extend the existing Core suite with direct/reindexed, projected,
  converted, normalized, thunk-inverted and nested higher-order beta cases.
  Check wrong arguments, derivation reconstruction, and 100 repeated queries
  with no additional Terms or accepted derivations. Existing IADT tests cover
  constructor recovery through normalized sequencing and captured scopes.
- [x] Full debug `check-acceptance` passed, including 63/63 compatibility and
  QuickSort properties through source/images at chunks one and 64. ASan/UBSan
  Core, IADT, Identity and QuickSort property compilation also passed.
- [ ] The two structural traversals still run synchronously and revisit
  retained maps on repeated calls. Reusing substitution results is not the
  same as a shared, budgeted typed-reduction request. This remains part of R3;
  neither this checkpoint nor passing regressions completes that phase.

Identity audit: ordinary Identity formation retains its three typed inputs,
but dependent family formation still obtains its selected left/right maps
from premises. Those maps and path witnesses are semantic boundary data, not
expendable bookkeeping. Migrate their structural representation before
deleting `action.c`'s formation recovery; do not infer the boundary from an
erased Core or silently discard it. No Identity rules changed here.

Delta against `e90fd19` (paths under `src/prototype/pointer/`):

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `evidence.c` | 119 | 79 | +40 |
| `tests/core.c` | 30 | 0 | +30 |

Cumulative implementation growth is **+821**, so R5's net-negative condition
is still unmet. Sequential single-run debug QuickSort measurements were
1.445 seconds / 274464 KiB at R7 and 1.464 seconds / 274344 KiB here, both
201138 Solve transitions. This does not establish a speedup or regression.
Derivations decreased from 504226 to 504223; the other R7 structural counts
were unchanged. Final comparative gates, remaining consumer deletions and
Main publication remain pending.

### 2026-09-17: Retained classifier formation

- [x] Retain the classifier's typed structural node on constructed terms.
  This is an edge between existing occurrences, not a new Core tag, proof
  database or acceptance bit. Interning checks its exact identity and enforces
  matching context and classifier Core. Conversion selects the target edge;
  normalization preserves it when the classifier is unchanged.
- [x] Construct the formation at Return/Thunk/Force, APP, extraction, request
  and Fold introduction using the existing checked rules. Constructor, Match,
  Lambda, host and Identity producers retain their supplied formations.
  Delete `classifier_leaf` and the rule-indexed classifier history walk.
  Recovery now follows typed origins/maps, reads the selected type edge, and
  checks the resulting context/classifier. Variables use the exact accepted
  context declaration; Universe formation is constructed lazily.
- [x] Add classifier-edge queries to the existing scoped input work. This is
  descriptive access only. Update occurrence transport to APGOCC4, retaining
  the type edge in the same DAG; reject earlier occurrence images. Fresh-process
  round trips retain sharing without creating acceptance evidence.
- [x] Debug `check-acceptance` passed, including 63/63 compatibility and all
  QuickSort source/image property comparisons at chunks one and 64. ASan/UBSan
  Core, IADT, Identity, synthesis and occurrence round trips passed. An initial
  synthesis regression revealed missing formation after converted Return/Thunk
  inversion; derived extraction now retains its checked target formation.
- [ ] Finish shared/budgeted access for classifier recovery: the kernel still
  follows origin/map frames synchronously inside formation-producing rules.
  Descriptive edge-query memoization alone does not settle this requirement.
- [ ] Remove nominal/Pi-component recovery and Identity boundary-history
  reconstruction only after their actual semantic inputs are retained. No
  claim of R3/R5 completion or Main publication is made at this checkpoint.

Changes against `5bc2f0e`, excluding documentation:

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `evidence.c` | 123 | 175 | -52 |
| `evidence.h` | 5 | 4 | +1 |
| `typing.c` | 52 | 7 | +45 |
| `typing.h` | 12 | 0 | +12 |
| `occurrence_io.c` | 26 | 11 | +15 |
| `occurrence_io.h` | 1 | 1 | 0 |
| **Implementation subtotal** | **219** | **198** | **+21** |
| Tests (three files) | 28 | 10 | +18 |

Cumulative implementation delta against `4657cc6` is **+842**, still above the
required net-negative result. Sequential single-run debug QuickSort measurements
were 1.456 seconds / 274332 KiB before and 1.505 seconds / 271788 KiB after.
Solve transitions fell from 201138 to 154851 because formation work moved to
construction; this is not a comparable reduction in total computational work
or a speedup claim. Current counts: 405313 occurrences, 496977 derivations,
406356 conclusion keys, 39769 context maps, 103154 occurrence actions, 3097
input queries, 14921 contexts and 175371 Core terms. Final time/memory gates
and the remaining physical deletions are still required.

### 2026-09-17: Constructor structural access

- [x] Replace `constructor_origin`'s receipt-rule switch with typed constructor
  inputs and the shared construction-origin/context-map path. Validate the
  constructor spine against those inputs, obtain the nominal instance from
  its retained classifier, and certify transported fields with existing rules.
  Derived Return results use the typed returned-value path. Remove the now
  unused `evidence_map` history helper; do not retain the old constructor walk.
- [x] Extend IADT tests with exact field extraction through normalized Fold
  and captured scopes, invalid field rejection, and 100 repeated requests
  without additional Terms or derivations.
- [x] Full debug `check-acceptance` passed again (63/63 compatibility and
  source/image QuickSort properties); ASan/UBSan IADT and Identity passed.
- [ ] Nominal classifier recovery still uses `inductive_recovery_step` and
  Pi-component/rebase helpers. Moving constructor operands is not completion
  of that remaining formation migration.

Against `e19f72f`: `evidence.c` +41/-43 (-2), `tests/iadt.c` +13/-0 (+13).
Cumulative implementation delta is **+840**. A single debug QuickSort run was
1.478 seconds, 272484 KiB peak RSS and 154851 Solve transitions; no performance
improvement claim follows from this single run. Main remains unpublished.

The next deletion prerequisites are explicit: Match/induction must retain its
typed motive and recursive clause scopes, not only scrutinee/branches/output;
dependent Identity must retain its selected left/right structural maps, not
only family/paths/endpoints. Their current presence in acceptance premises is
not redundant evidence to discard. General constant-codomain strengthening
also needs a scoped structural representation before removing Pi recovery.

### 2026-09-17: Selected Identity construction maps

- [x] Retain the two selected dependent Identity substitutions as structural
  map inputs on the existing typed occurrence. Their ordered pointers enter
  exact interning; they are neither acceptance receipts nor a second graph.
  Each destination is the construction context. Conversion retains these
  inputs; outer reindexing retains its ordinary origin/map recipe.
- [x] Delete `action.c`'s proof-rule wrapper walk. Identity formation, endpoint
  and face work now follow typed origins and selected maps, then certify the
  recovered inputs using the existing Identity rules. The direct introduction
  view remains a logical rule inversion, not a search through receipt history.
  Classifier conversion must not replace a family action's original input
  Identity. No new Identity introduction or equality reflection is added.
- [x] APGOCC5 transports selected source scopes and typed map images in the
  same occurrence DAG. Earlier occurrence images reject. Fresh-process tests
  cover selected-map order, scope relocation, sharing, invalid destinations,
  truncated input and zero acceptance evidence after readback.
- [x] Full debug `check-acceptance` passed (63/63 compatibility, QuickSort
  properties through source/images, chunks one and 64). Identity regressions
  cover converted families, dependent/higher boundaries and repeated queries
  with no additional occurrences or derivations. Tests no longer require
  traversing 128 redundant type/value receipts or choosing the same derivation
  when two valid derivations share one typed subject. Cancellation uses a cold
  work queue, since a warm queue can now reuse its completed boundary.
- [x] ASan/UBSan Core, Identity, synthesis and fresh-process occurrence I/O
  passed. Sequential single-run debug QuickSort measurements were 1.498 seconds
  / 271912 KiB before and 1.524 seconds / 271784 KiB after, both 154851 Solve
  transitions. These runs establish no performance improvement claim.

Against `513dbe3`, implementation/header changes are +190/-77 (**+113**):
`action.c` +64/-43, `evidence.c` +8/-6, `evidence.h` +4/-0,
`typing.c` +37/-10, `typing.h` +8/-0, `occurrence_io.c` +64/-14,
`occurrence_io.h` +5/-4. Tests are +61/-25 (**+36**).
Cumulative implementation delta is **+953**; R5's net-negative condition is
still unmet. Nominal/Pi recovery, Match/induction construction metadata,
shared budgeted typed normalization and final publication remain incomplete.

### 2026-09-17: Retained Match motive and nominal inputs

- [x] Match/induction conclusions retain `scrutinee, branches..., motive,
  nominal formation` as ordered typed inputs, the parameter substitution as
  one selected structural map, and the output formation on their existing type
  edge. The motive's context retains its index/scrutinee telescope. No Core
  constructor, proof-rule tag or parallel graph is added.
- [x] Elimination reindexing, selected branch extraction and recursive field
  expansion read these inputs through one local view. They no longer recover
  them from the elimination receipt's premise offsets. Checked substitution,
  Match, induction and beta rules still establish acceptance; their premise
  DAG and nominal schema validation are retained. Different derivations of one
  nominal typed subject are not treated as different nominal declarations.
- [x] Add tests for dependent motive scope, retained nominal identity and
  parameter maps before/after reindexing, repeated branch extraction without
  occurrence/proof growth, and the public NULL typing-store boundary.
  Full debug `check-acceptance` passed on the final implementation, including
  source/image properties, chunks one and 64, and 63/63 compatibility.
- [x] ASan/UBSan IADT, Identity and imported QuickSort property compilation
  passed. Sequential single-run debug QuickSort measurements were 1.495 seconds
  / 272280 KiB before and 1.534 seconds / 271540 KiB after, both 154851 Solve
  transitions. No speedup is claimed from these individual runs.
- [x] Move retained induction clause allocations/scopes to the structural
  owner before deleting the function-graph elimination-origin wrapper walk.
  Completed by `08fb5d4` and `5386916` below. Direct theorem inversion remains
  distinct from obtaining the construction behind a converted boundary.

Against `cceb78c`: `evidence.c` +68/-26 (**+42**), `tests/iadt.c` +19/-0.
Cumulative implementation delta is **+995**, still not the required reduction.
The Pi investigation also confirms that erased Core independence alone is not
a replacement for transporting typed construction dependencies. General
constant-codomain restriction must retain its scoped structural input before
`pi_component` and its associated rebase machinery can be deleted. This is
remaining work, not an exception to R3 or R5.

### 2026-09-17: Induction allocation belongs to typed construction

- [x] Move the existing recursion/argument/self binder tuple and ordered
  clause scopes from the induction receipt's certificate to its typed subject.
  Exact binder/context pointers, not allocation addresses or normalized Core,
  participate in interning. The subject copies the tuple into its own arena
  allocation; temporary construction storage is no longer retained by Evidence.
  Classifier boundaries preserve it. Reindexed/derived uses retain their
  origin rather than attaching stale clause scopes to the new context.
- [x] Remove the induction-specific certificate-key exception. The existing
  Evidence allocation accessor and derivation transport now read the same
  structural owner. Ordinary induction checking still verifies scope, branch
  types, freshness and conflicting explicit allocations. No new typing rule,
  Core tag, acceptance authority or parallel graph is introduced.
- [x] APGOCC6 transports the binder tuple and clause contexts through the
  existing shared Core/context relocation tables. Earlier occurrence images
  reject. Tests cover tuple sharing independent of temporary array identity,
  ordered scope differences, preserved boundary inputs, fresh-process binder
  relocation, truncated images and zero acceptance evidence after readback.
- [x] Full debug `check-acceptance` passed, including 63/63 compatibility and
  source/image QuickSort property checks at chunks one and 64. ASan/UBSan IADT,
  occurrence I/O and imported QuickSort property compilation passed.
- [x] Replace the function-graph consumer's Evidence wrapper traversal,
  including classifier-converted constructions whose first accepted receipt
  is not an introduction. Completed by `5386916` and `7f77b3c` below, using
  typed construction inputs rather than selecting the first receipt.

Against `40e0687`, implementation/header changes are +128/-37 (**+91**):
`evidence.c` +5/-7, `evidence.h` +0/-8, `typing.c` +55/-10,
`typing.h` +13/-0, `occurrence_io.c` +51/-9, `occurrence_io.h` +4/-3.
Tests are +39/-5 (**+34**). Cumulative implementation growth is **+1086**;
the net-negative gate remains unmet. Sequential single debug QuickSort runs
were 1.515 seconds / 272088 KiB before and 1.575 seconds / 284304 KiB after,
both 154851 Solve transitions. These do not establish a timing trend, but the
12216 KiB RSS increase is a recorded cost of this representation change, not
a claimed optimization. Final R5 cleanup and comparative measurement remain
required. No Main publication has taken place.

### 2026-09-17: Function-graph computation origin from typed inputs

- [x] Delete `function_graph.c`'s `computation_origin` receipt-wrapper walk.
  The shared Evidence interface follows retained structural origins/maps and
  certifies their construction through existing variable, Lambda, APP, F/U,
  Fold, Match and induction rules. Match uses its retained motive/parameter
  inputs; induction uses the allocation now owned by its typed subject.
  No arbitrary Core-to-proof lookup or new kernel acceptance rule is used.
- [x] Re-establish the actual introduction from typed operands instead of
  assuming the first receipt is an introduction. A changed classifier boundary
  may have only a subsumption/conversion receipt. Preserve that alternative;
  reconstructed introductions can have the pre-subsumption classifier. Check
  exact source Core/context agreement before publishing the returned view.
  Normalized inputs expose their checked source recipe, not alleged WHNF
  operands. This does not complete budgeted typed normalization.
- [x] Test direct/reindexed induction origins, exact scope maps, 100 repeated
  lookups without additional occurrences/derivations, and an initially
  subsumption-only Return boundary whose first receipt remains unchanged.
  Full debug acceptance passed, including 63/63 compatibility and source/image
  QuickSort property checks. ASan/UBSan IADT, program/function-graph tests and
  imported QuickSort property compilation passed.
- [x] Migrate `pg_function_graph_source`'s separate public-entry wrapper walk,
  owner contract and Evidence-keyed dependencies. Completed by `7f77b3c`
  below. Direct signature/branch theorem inversions remain intentional; this
  does not complete R3's unrelated Pi/nominal recovery work.

Against `08fb5d4`: implementation/header +102/-41 (**+61**): `evidence.c`
+92/-5, `evidence.h` +7/-0, `function_graph.c` +3/-36. Tests: +27/-0.
Cumulative implementation growth is **+1147**. Sequential single debug
QuickSort runs were 1.577 seconds / 284352 KiB before and 1.543 seconds /
284012 KiB after, both 154851 Solve transitions; no speedup is claimed.
The R5 net-negative gate, remaining Pi/nominal recovery migration, shared
budgeted normalization and Main publication are still outstanding.

### 2026-09-17: Typed function source and generation identity

- [x] Make `pg_function_graph_source` take its typing/classifier owner and
  use the checked typed-construction origin interface. Remove its independent
  projection/quotation receipt walk. The remaining F/U rule inversion is of
  the introduction just certified from typed inputs. Do not silently discard
  a nonidentity context map when selecting the source Lambda.
- [x] Key synthesis function-graph requests by typed subject and the selected
  source layout, not by a typing receipt. Compare helper/self dependencies by
  typed subject too. Different derivations remain accepted alternatives;
  neither their pointers nor erased Core alone define the generated family.
- [x] Move Match source naming/layout association to the existing synthesis
  index keyed by typed subject. `SOURCE_ORIGIN_JOB` is a completed metadata
  entry with no accepted result, not another acceptance authority or program
  graph. Remove `source_origin` from every synthesis job. Graph requests still
  include the selected source layout; semantic sharing does not erase public
  field-order/naming requirements.
- [x] Regression tests attach a distinct conversion receipt to the same
  Lambda subject and verify repeated source recovery without occurrence/proof
  growth. Both names then expose the exact same generated relation and named
  constructor. Tests also reject dropping a genuine scope extension and
  retain dependency mismatch checks at budgets one and 64.
- [x] Full debug `check-acceptance` passed (63/63 compatibility, source/image
  QuickSort properties). ASan/UBSan program, synthesis and imported QuickSort
  property compilation passed. No receipt alternatives were removed.

Against `5386916`, implementation/header +35/-29 (**+6**):
`function_graph.c` +17/-15, `function_graph.h` +4/-3, `synthesis.c` +14/-11.
Tests: +52/-2 (**+50**). Cumulative implementation growth is **+1153**, still
not the required reduction. Sequential single debug QuickSort runs were
1.526 seconds / 284736 KiB before and 1.540 seconds / 284432 KiB after, both
154851 Solve transitions. These are checkpoints, not a demonstrated speedup.
Direct signature/branch theorem inversions remain intentional; Pi component,
nominal formation and strengthening history recovery still require migration.
R0-R5 aggregate completion and Main publication remain unclaimed.

### 2026-09-17: Preserve F/U inputs across classifier boundaries

- [x] `content_subject` retains an available component's construction when
  Core, context and judgement match but its classifier boundary changes.
  In particular, constant-codomain extraction can inherit a larger Pi
  Universe bound without losing the F/U components' semantic inputs.
  The ordinary inversion rule still establishes the new conclusion; no
  structural record grants acceptance on its own.
- [x] Term inversion retains the changed classifier's formation as well.
  An initial implementation lost it for a converted Return of a function;
  the existing synthesis regression exposed that failure. The corrected
  implementation distinguishes an unchanged component from a newly exposed
  boundary by its typed input, not by receipt discovery order. The second
  input lookup reuses the same completed structural request.
- [x] Add tests for F/U extraction through a wider Universe, retained Pi
  domain formation, ordinary derivation reconstruction and 100 repeated
  lookups without occurrence/proof growth. Extend the converted-function
  regression to require both the original body and the new classifier
  formation. Existing exact-subject checks for unchanged values remain.
- [x] Full debug `check-acceptance` passed on the corrected implementation:
  63/63 compatibility and source/image QuickSort properties at chunks one
  and 64. ASan/UBSan Core, synthesis and imported QuickSort property checks
  passed. `git diff --check` passed.

Against `7f77b3c`, implementation `evidence.c` is +5/-3 (**+2**); tests are
`tests/core.c` +26/-0 and `tests/synthesis.c` +3/-0. Cumulative implementation
growth is **+1155**. Sequential single debug QuickSort runs were 1.526 seconds
/ 284672 KiB before and 1.571 seconds / 284044 KiB after, both 154851 Solve
transitions. No performance improvement is claimed. This removes an avoidable
loss of structure at a producer; it does not remove general Pi restriction,
normalization or nominal recovery. The aggregate R0-R5 and net-negative LOC
gates remain open, and Main has not been published.

### 2026-09-17: Structural scope actions in nominal/Pi recovery

- [x] Replace temporary `evidence_frame` entries with structural scope
  actions. A frame contains either the exact total context map, or the typed
  Pi component whose independent inputs must be recovered in its smaller
  context. It does not retain a chosen projection/reindex receipt. Scope
  restriction is not encoded as a total substitution with a fabricated image
  for the removed binder. No persistent graph or wire-format tag was added.
- [x] Delete `strengthened_context`, `evidence_map_step` and `evidence_image`.
  Their replacement consumers use the retained map or restriction context,
  not repeated proof-rule tests and nested premise offsets. Pi argument
  lifting retains its checked map directly, without constructing a variable
  and its reindex receipt merely to recover that map again later.
- [x] Share structural binder-image lookup through `pg_context_map_image`
  in nominal recovery, typed beta body access and scoped child queries.
  Keep `pg_substitution_image` as genuine receipt inversion: its callers can
  still select the original alternative image proof. Structural lookup does
  not grant acceptance or change any alternative derivation.
- [x] Handle a variable image through the destination context's variable
  rule, rather than re-entering the arbitrary receipt used to certify a
  shared map. The first implementation of shared map frames exposed a loop
  when that receipt was itself a projection of the same variable. A bounded
  regression now pre-registers that alternative and verifies that unknown
  nominal formation terminates as unavailable at budgets one and 64. This
  negative result is not a proof that the type has no inhabitants.
- [x] Full debug `check-acceptance` passed, including 63/63 compatibility,
  source/image QuickSort properties and split budgets. ASan/UBSan Core,
  IADT, synthesis and imported QuickSort property checks passed.
  `git diff --check` passed; none of the deleted helper names remain.

Against `d5272ea`, implementation/header changes are `evidence.c` +79/-86,
`evidence.h` +1/-1, `typing.c` +13/-4, and `typing.h` +3/-0: **+5 net**.
`tests/core.c` adds 19 lines. Cumulative implementation growth is **+1160**;
the complete change still fails the net-negative gate. Three alternating,
sequential debug QuickSort samples used 154851 Solve transitions each:

| Version | Elapsed seconds | Peak RSS, KiB |
|---|---|---|
| Previous checkpoint | 1.553, 1.562, 1.541 | 284548, 284288, 284356 |
| Structural scope frames | 1.511, 1.501, 1.514 | 271124, 271376, 271804 |

These samples show a repeatable memory reduction for this input, consistent
with removing intermediate variable/reindex receipts. They are not a general
speedup claim. `pi_component`, `inductive_recovery_step` and `rebase_image`
still recover structure from receipt history; replacing their scope-action
payload does not complete their removal or shared budgeted normalization.
R0-R5 aggregate completion and Main publication remain outstanding.

### 2026-09-17: Source allocation scopes from typed binder inputs

- [x] Share direct Lambda/Pi binder-edge validation through
  `pg_occurrence_scoped_input`. It checks the exact parent context, binder and
  Core body; it describes structure without granting acceptance. Mapped and
  derived inputs still use the existing budgeted structural request.
- [x] Recover source Match branch and handler payload/resumption scopes from
  their typed Lambda inputs, rather than peeling conversion receipts and
  indexing Lambda/Pi premises. Keep recursive erasure clause scopes separate:
  induction allocation clauses are not necessarily the original source
  branch scopes and cannot replace those binders.
- [x] Test distinct typed identity bodies, mismatched binders/bodies/parents,
  mapped and derived records, and direct versus reindexed Pi bodies.
- [x] Full debug acceptance passed, including 63/63 compatibility and
  source/image QuickSort properties. ASan/UBSan Core, synthesis, one/two-clause
  handler image reconstruction and imported QuickSort compilation passed.
  QuickSort used 154851 Solve transitions. `git diff --check` passed.

Against `57fc128`, implementation/header changes are `evidence.c` +2/-3,
`synthesis.c` +25/-33, `typing.c` +14/-7, and `typing.h` +5/-0: **+3 net**.
Tests add 17 lines. Cumulative implementation growth is **+1163**. This does
not complete the remaining formation/normalization migration or the R5
net-negative gate; Main remains unpublished.

### 2026-09-17: Family construction uses the shared typed source

- [x] Extend the shared checked construction interface to family Lambda/APP
  and rename it `pg_prove_construction_origin`. Re-establish the appropriate
  ordinary family or computation rule from typed inputs. Keep those logical
  rules distinct; add no Core constructor or serialized evidence tag. Require
  the reconstructed source's judgement, context and Core to match the retained
  construction. Reject context/substitution evidence at this interface.
- [x] Remove `family_function_step`'s projection/reindex receipt recursion.
  It reads typed Lambda bodies/application arguments and applies the shared
  accumulated context map to the resulting function. The extended context is
  taken from the family introduction just checked, not an arbitrary receipt.
  Nominal declarations still require their ordinary declaration/instance
  checking; a missing Lambda/APP view does not admit a nominal declaration.
- [x] Share exact scoped-body validation in typed beta-body exposure too.
  Test direct and projected family constructions, partial/final family
  applications and repeated lookup without proof/occurrence growth. Test that
  partial application followed by pure normalization still supports `&F`,
  with the same resulting judgement as quotation before normalization.
- [x] Fix a related consumer failure discovered while auditing the shared
  API: graph generation for a Match branch calling a helper ending in
  `#int_add` dereferenced a missing construction view. Absence of an induction
  helper now returns control to ordinary computation handling. The regression
  generates `@f` and `*f` and verifies the runtime result `3` at chunks one and
  64; it does not assert that every host operation has a structural graph.
- [x] Full debug acceptance passed on the final implementation, including
  63/63 compatibility and source/image QuickSort properties. ASan/UBSan Core,
  IADT, synthesis, program and imported QuickSort checks passed.
  `git diff --check` passed. No wire format or acceptance policy changed.

Against `3849848`, implementation/header changes are `evidence.c` +14/-8,
`evidence.h` +2/-2, `function_graph.c` +5/-4, `synthesis.c` +16/-12:
**+11 net**. Tests are +50/-6 (**+44**). Cumulative implementation/header
changes from `4657cc6` are +2482/-1308 (**+1174**), still failing the reduction
gate. Sequential single debug QuickSort runs were 1.532 seconds / 271616 KiB
before and 1.499 seconds / 271536 KiB after, both 154851 Solve transitions.
No performance improvement is inferred from one pair of measurements.

The next substantial dependency remains typed Pi restriction/result structure:
the constant-codomain fallback retains its Pi source but not the scoped body,
and the codomain fallback does not retain its typed argument. These omissions
must be resolved before deleting `pi_component`, its scope-frame path and the
remaining nominal history recovery. They cannot be replaced with fabricated
substitution images or by treating normalized source inputs as result children.
Shared budgeted typed normalization, aggregate R0-R5 acceptance and Main
publication remain outstanding.

### 2026-09-17: Retain Pi selection and its typed argument

- [x] Replace `pi_component`'s proof-rule traversal with `selected_formation`.
  A typed subject retains the selected input ordinal and, for dependent Pi
  application, the actual typed argument. Selection is not another executable
  Core constructor or a new acceptance rule. Its operands describe the
  selection recipe, not the normalized result's children.
- [x] Preserve selection through classifier boundaries and exact interning.
  Scope restriction uses the selected Pi's own context before applying outer
  maps. Stop at a selected computed type; traversing its source again loses
  the distinction between input selection and execution.
- [x] Reuse a materialized input only when ordinary rules certify its scope.
  Structural binder lifting alone does not establish context formation.
  Keep the checked Pi inversion recipe when such a descriptive view cannot
  be certified. This fixed generated dependent motives and QuickSort recovery
  without admitting fabricated substitutions or changing source synthesis.
- [x] Consume direct F/U type inputs structurally in nominal recovery. An
  alternative first receipt (for example constant-codomain inversion at a
  widened universe) must not cause a loop back to the same selection.
- [x] Transport selections in `APGOCC7`; source versions become 42/43.
  Versions 40/41 and `APGOCC6` reject explicitly rather than silently losing
  selection arguments. Unaccepted descriptions still load without granting
  evidence. Test exact sharing, distinct ordinals/arguments, source images,
  and the generated `typed-selection-motive.p` regression.
- [x] Final debug and optimized `check-acceptance` passed, including 63/63
  compatibility and source/image QuickSort properties. Full sanitizer
  acceptance passed before the waiting-frame change below. On the final
  implementation, ASan/UBSan Core, IADT, synthesis and imported QuickSort
  property checks passed. Full sanitizer acceptance on the eventual R5
  snapshot remains required. `git diff --check` passed.
- [x] Remove recursive advancement between selected-input requests. Explicit
  waiting frames account for dependency transitions in the caller's budget.
  Test 10000 nested selections at chunks one and 64, one-step bounded request
  allocation, completion of a shared dependency by another caller, and repeat
  lookup without additional work. The Core suite passes on this change.

`inductive_recovery_step` and `rebase_image` still interpret parts of Evidence
history. The structural Pi migration does not complete that removal, shared
budgeted typed normalization, or the net-negative implementation LOC gate.
Do not mark aggregate R0-R5 complete or publish Main at this checkpoint.

Against `1bda739`, implementation/header changes are:

| File in `src/prototype/pointer/` | Added | Deleted |
|---|---:|---:|
| `action.c` | 1 | 0 |
| `evidence.c` | 85 | 106 |
| `occurrence_io.c` | 16 | 9 |
| `occurrence_io.h` | 4 | 2 |
| `source_io.c` | 2 | 2 |
| `source_io.h` | 3 | 3 |
| `typing.c` | 65 | 10 |
| `typing.h` | 9 | 0 |
| **Total** | **185** | **132** |

This increment is **+53**, not a net reduction. Cumulative implementation/
header changes from `4657cc6` are **+2614/-1387, net +1227**. Tests, the new
18-line source fixture, build wiring and documentation are excluded.

Three alternating sequential debug runs of the imported QuickSort property
used the same input and flags as the baseline measurements above:

| Version | Elapsed seconds | Peak RSS, KiB | Solve transitions |
|---|---|---|---:|
| R19 | 1.403, 1.454, 1.390 | 272144, 271696, 272352 | 154851 |
| R20 | 1.397, 1.382, 1.416 | 277752, 278100, 277332 | 132488 |
| Initial baseline, second comparison | 0.831, 0.811, 0.775 | 225248, 226036, 225976 | 149501 |
| R20, second comparison | 1.414, 1.404, 1.405 | 277920, 277848, 277768 | 132488 |

Lower Solve counts do not establish a speedup: R20 is approximately unchanged
from R19 in elapsed time, but the cumulative refactor is substantially slower
and larger than the initial baseline. These are local debug measurements, not
an optimized production benchmark. Do not waive this regression at R5.

A diagnostic `-O0 -g -pg` run identifies a concrete repeated-work path:
`pg_context_map_projection` was called 342376 times and called `pg_occurrence`
9320652 times while rebuilding image arrays before map interning. Most calls
came from `pg_prove_projection` through substitution lifting. This is not
millions of distinct accepted conclusions; repeated lookup preparation is the
problem. Sampling attributed about 47% inclusive time to this path, but the
short instrumented sample is only a profiling lead, not a precise wall-time
attribution.

- [x] Reuse projection requests by their immutable source/destination before
  reconstructing all images. Preserve the same canonical map returned through
  the general map constructor; do not add a second acceptance authority or
  special-case QuickSort. Test generic/projection construction in both orders,
  invalid prefixes, and repeated lookup without rebuilding variable inputs.
- [ ] Repeat baseline/final timing, memory and work-count comparisons after
  that change and the remaining structural recovery removal.

### 2026-09-17: Reuse structural projection requests

- [x] Index completed projection requests by immutable source/destination.
  Each entry references the existing interned `pg_context_map`; it stores no
  additional images, classifier, acceptance bit, or mutable solution. A miss
  still checks the prefix and uses the general map constructor. Generic-first
  and projection-first construction therefore return the same map.
- [x] Add Core tests for both construction orders, empty maps, value/family
  binder sorts, invalid/unrelated prefixes and 10000 repeated requests without
  term/occurrence/map/index growth. The Core suite passes.
- [x] Full debug acceptance passed, including 63/63 compatibility and
  source/image QuickSort properties. ASan/UBSan Core, IADT, synthesis and
  imported QuickSort checks passed. `git diff --check` passed. R5's final
  optimized/sanitized full gates remain open with the rest of the migration.

On the same imported QuickSort property, sequential debug samples were:

| Version | Elapsed seconds | Peak RSS, KiB | Solve transitions |
|---|---|---|---:|
| R20 | 1.409, 1.384 | 278228, 278284 | 132488 |
| Projection reuse | 1.002, 1.010, 0.976 | 278476, 278280, 278800 | 132488 |
| Initial baseline | 0.809, 0.807 | 226076, 225472 | 149501 |

Instrumented projection-to-occurrence calls fell from **9320652 to 360092**;
projection requests remained **342376**. This confirms removal of repeated
lookup preparation, not a weakening of checking or a smaller accepted result.
It does not eliminate the remaining baseline time/memory regression. Against
`5e7e5dd`, `typing.c` is +19/-1, `typing.h` +2/-0 and tests +20/-0: implementation
net **+20**, cumulative **+1247**. The code-reduction gate remains unmet.

### 2026-09-17: Nominal recovery follows typed construction

- [x] Replace `inductive_recovery_step`'s derivation-rule switch with typed
  maps, selected inputs, origin recipes and direct APP/RETURN/THUNK/FORCE/Fold
  inputs. Remove the two F/U-content history counters; type components are
  selected structurally. Keep pending index arguments in their actual scopes.
- [x] Resolve the final nominal owner using the declaration's exact original
  context, declared classifier and Core. Require its accepted `INDUCTIVE_FORM`
  certificate. This is not global lookup by erased Core and does not treat a
  descriptive nominal reference as acceptance. Boundary conversions do not
  change which declaration was chosen.
- [x] Add tests for alternative receipts of one nominal subject and for a
  second typing store sharing the Core graph: descriptive structure and foreign
  evidence do not transfer acceptance. Existing dependent-index, constant Pi,
  F/U, Fold, substitution and split-budget regressions remain enabled.
- [x] Full debug `check-acceptance` passed, including 63/63 compatibility and
  source/image QuickSort properties. ASan/UBSan Core, IADT, synthesis and
  imported QuickSort checks passed. `git diff --check` passed.

Against `fb86cdc`, `evidence.c` is **+89/-84**, `evidence.h` **+4/-4** and
`tests/iadt.c` **+20/-0**. Implementation/header net is **+5**, cumulative
**+1252** from `4657cc6`; this is a structural migration, not a LOC reduction.
A single sequential debug QuickSort diagnostic took **1.092 s**, **278900 KiB**
peak RSS and **132383** Solve transitions. It does not establish a speedup or
close the previously documented baseline memory/time regression.

Remaining prerequisites are checked strengthening (`rebase_image`), shared
budgeted typed normalization/exposure, the final producer/consumer audit and
the full R5 gates. In particular, replacing receipt dispatch does not make
the synchronous work inside each nominal recovery transition fully budgeted.
No Main publication or aggregate completion is claimed at this checkpoint.

### 2026-09-17: Checked rebasing from typed inputs

- [x] Remove `rebase_image`'s Evidence-wrapper traversal and reconstruction
  switch. Read retained context maps, returned-value origins, constructor
  fields and family-application inputs from typed structure. Compose maps
  through the ordinary substitution checker, preserving lexical dependencies.
- [x] Re-establish each restricted introduction in the target context. Keep
  exact nominal formation checks and the normalization receipt for an exact
  typed source/result recipe. Type/value boundary changes still use ordinary
  rules, and the final Core/classifier must agree with the requested image.
- [x] Reuse accepted constructor introductions of the exact typed subject.
  Route substitution composition through shared typed context action, then
  certify its result only when no matching accepted result exists. This avoids
  wrapping already accepted variable images in redundant REINDEX derivations;
  no existing alternative derivation is removed or overwritten.
- [x] Add positive/negative restriction tests: an irrelevant map image may
  mention the removed binder, but a real constructor field may not. Test pure
  normalization through a type/value boundary, repeated accepted-result reuse,
  and identity/variable substitution composition. Existing tests remain enabled.
- [x] Final debug `check-acceptance` passed, including 63/63 compatibility and
  source/image QuickSort properties. Final ASan/UBSan Core, IADT, synthesis and
  imported QuickSort checks passed. `git diff --check` passed.

Against `2f310a6`, implementation (`evidence.c`) is **+110/-113, net -3**;
tests are **+55/-0**. Cumulative implementation/header net is still **+1249**.
The new structural restriction uses synchronous checked suboperations; it does
not complete shared/budgeted typed result exposure or the net-negative gate.

Sequential debug diagnostics, with the same imported QuickSort input:

| Version | Elapsed seconds | Peak RSS, KiB | Solve transitions |
|---|---|---|---:|
| Initial baseline | 0.873 | 225096 | 149501 |
| R22 | 1.046, 1.068 | 278520, 278564 | 132383 |
| R23 before result reuse | 1.081 | 283092 | 132383 |
| R23 final | 1.061, 1.095 | 276024, 276160 | 132370 |

Result reuse eliminates the interim memory regression, but these short samples
do not establish a wall-time improvement over R22. The cumulative time/memory
regression against the initial baseline still requires the R5 review. Producer
contracts above now distinguish actual construction inputs, logical inversion
dependencies and result recipes; no phase is marked complete from tests alone.
### 2026-09-17: Classifier access uses the shared typed-input query

- [x] Remove the separate classifier map stack and its temporary arena.
  Retained classifiers use the same indexed, budgeted context-action query as
  typed input views. Final formation still requires ordinary acceptance.
- [x] Keep legitimate leaf rules: Universe formation and a variable's declared
  type from its accepted context. Descriptive nodes do not certify themselves.
- [x] Add a dependent mapped-function regression checking zero budget, one-step
  progress, exact result reuse and no extra proof/query work on repeat access.
- [x] Full debug acceptance passed, including 63/63 compatibility and the final
  QuickSort source/image result checks. ASan/UBSan Core, IADT, synthesis and
  imported QuickSort passed. No input format or logical rule changed.

Against `bb02b9d`, `evidence.c` is +21/-41, `evidence.h` +5/-6:
implementation/header net **-21**. Tests are +22/-0. Cumulative implementation
net is still **+1228**; the net-negative and final publication gates stay open.
The final certification of a typed result can still invoke synchronous kernel
checks; this change does not claim to budget all kernel work or complete typed
normalization exposure.

#### Expanded baseline diagnostics

Both binaries use debug O0 flags and the same inputs, legacy spelling option,
and one-million-step limit. Baseline is the retained `4657cc6` binary, R24 is
`bb02b9d` plus this change. Each timing runs sequentially in a fresh process.
These are single diagnostic samples, not a statistical speedup claim. Counts
were measured separately at `pg_program_destroy` using GDB, without modifying
the source. The eight root examples matching 01-09 are listed below; this
checkout has no matching root example 08.

| Input | Baseline / R24 seconds | Baseline / R24 Solve transitions | Baseline / R24 Terms, typed subjects, proofs |
|---|---|---|---|
| 01_bool | 0.0011 / 0.0026 | 520 / 470 | 92/70/97 / 92/80/100 |
| 02_nat | 0.0015 / 0.0012 | 307 / 300 | 73/61/92 / 73/70/92 |
| 03_main | 0.0014 / 0.0021 | 520 / 470 | 92/70/97 / 92/80/100 |
| 04_match | 0.0017 / 0.0022 | 1302 / 1221 | 171/185/336 / 172/255/345 |
| 05_bool_to_nat | 0.0018 / 0.0023 | 946 / 853 | 118/122/205 / 118/156/213 |
| 06_pred | 0.0016 / 0.0025 | 811 / 755 | 119/122/207 / 119/152/210 |
| 07_add | 0.0021 / 0.0023 | 1844 / 1685 | 227/246/382 / 227/305/408 |
| 09_list_induction | 0.0039 / 0.0034 | 3536 / 3177 | 290/351/614 / 286/443/618 |
| Vec-append | 0.0132 / 0.0151 | 22765 / 20744 | 6301/2737/6505 / 8257/3122/4123 |
| dependent-Sigma | 0.0025 / 0.0051 | 4048 / 3734 | 614/475/741 / 681/600/755 |
| generated-length | 0.0095 / 0.0134 | 10950 / 9580 | 1829/2249/3856 / 1911/3146/3724 |
| QuickSort-property | 0.8403 / 1.0766 | 149501 / 131321 | 168628/435774/588033 / 179803/416611/433565 |

| Zero-work image | Baseline / R24 seconds | Baseline / R24 Solve transitions |
|---|---|---|
| Vec-append | 0.0123 / 0.0133 | 23074 / 21049 |
| dependent-Sigma | 0.0028 / 0.0036 | 4357 / 4039 |
| generated-length | 0.0084 / 0.0111 | 11259 / 9885 |
| QuickSort-property | 0.8841 / 1.0683 | 149810 / 131626 |

The image cases save at zero Solve steps and resume in a fresh process; each
version writes its own format. Vec uses `explicit_index_family_append_check.p`
with `legacy-vec-append-results.p`; Sigma uses
`dependent_constructor_provider_check.p` with `import-dependent-constructor.p`;
length uses `length-output-proof.p`; QuickSort uses
`if8_fuel_free_quicksort_check.p` with `legacy-quicksort-property.p`.
Provider files are under `src/prototype/tests/fixtures/typing`, clients under
`src/prototype/pointer/tests/acceptance`.

QuickSort source peak RSS was **225904 / 275876 KiB**; image RSS was
**225804 / 276156 KiB** (baseline / R24). Small-case RSS was 10.7-11.1 MiB and
is dominated by the measurement process's inherited high-water mark, so it
cannot resolve allocator changes. Fewer proofs and Solve transitions do not
by themselves imply lower time/memory: typed records and shared work are
larger than the old receipts. This remains an R5 representation-cost concern,
not a reason to bypass logical checks. Repeated final measurements are still
required after the remaining structural work.

### 2026-09-17: Shared typed application and Return bodies

- [x] Replace the synchronous application-body walk with exact typed-input
  requests. Return extraction uses the same machine, including pending
  zero-clause Fold continuations; delete its separate traversal and stack.
  Alternative receipts of one subject share work, not a canonical proof.
- [x] Advance application requests one transition at a time from nominal
  recovery and family quotation. Explicitly requeue pending family work in
  Solve. Context-map composition and final reindexing use existing checked
  rules; their internal cost is not claimed to be one primitive step.
- [x] Add zero/chunked-budget and repeated-result tests, shared requests from
  different receipts, 10000 nested thunk/force wrappers, Return/Fold under
  substitution/projection, invalid argument sorts, and a neutral Fold prefix
  which must not be skipped. An unsupported body is not an inequality proof.
- [x] Final debug acceptance passed through the final source/image QuickSort
  results, including 63/63 compatibility. Final ASan/UBSan Core, IADT,
  synthesis and imported QuickSort passed; `git diff --check` passed.

`typing.typed_bodies` indexes computational work, not another program graph or
acceptance authority. Keys are exact typed source/argument references; a
missing argument denotes the separately validated Return-body request. Results
reference ordinary accepted evidence. Jobs/frames are graph-owned, are not
serialized, and are recreated through Solve after loading. No new logical rule,
image version, host execution, or equality reflection was introduced.

Against `d0c336f`, implementation/header changes are: `evidence.c` +192/-124,
`evidence.h` +21/-5, `synthesis.c` +3/-1, `typing.c` +2/-0 and `typing.h` +1/-0:
net **+89**, cumulative **+1317**. Tests are +58/-1. The shared resumable machine
replaces both walks, but its scheduling interface still costs code; this does
not satisfy the net-negative completion gate.

Sequential O0 QuickSort diagnostics: R24 **1.0729 s / 276160 KiB**, R25
**1.0663 s / 276396 KiB** and **1.0352 s / 276008 KiB**. R25 has exactly the
same **179803 Terms / 416611 typed subjects / 433565 proofs** as R24, plus 537
shared body requests. Solve transitions change **131321 -> 132011** because
previously synchronous traversal is now charged incrementally. These samples
show no material time/memory change, not a demonstrated speedup. Final typed
NF-child exposure, synchronous certification costs, cumulative representation
cost and the full R5 publication gates remain open. Main has not been pushed.

### 2026-09-17: Checked congruent NF inputs

- [x] Add a read-only congruence view of completed NF receipts. It requires
  unchanged heads before and after the single child-rebuilding phase. It does
  not treat beta/iota/eta's source operands as children of the result.
- [x] Expose a direct typed construction input using its own accepted evidence
  and the corresponding child reduction receipt. Lambda bodies retain their
  extended context. Semantic inputs on an APP spine keep their classifier;
  there is no lookup of an arbitrary proof by erased Core.
- [x] Connect this checked view to existing RETURN/THUNK content inversion.
  Share exact normalization-receipt lookup with structural rebasing. Keep the
  normalized parent's origin recipe without copying old operands onto it.
- [x] Test changed inputs beneath RETURN, THUNK and Lambda; preserved scope
  and classifier; ordinary derivation reconstruction; repeated result reuse;
  invalid input index/source; and rejection of a head-changing beta receipt.
- [x] Full debug acceptance passed (63/63 compatibility and final QuickSort
  source/image results). ASan/UBSan Core, IADT, synthesis and imported
  QuickSort passed. `git diff --check` passed.

This is not a general typed NF evaluator. `pg_prove_normalization_input` is a
checked synchronous view of direct source inputs and completed congruent NF
receipts. It does not yet expose every mapped input, perform iota reconstruction,
or schedule receipt-spine traversal as shared Solve work. These limitations
remain R3 work, rather than new language restrictions. The view introduces no
logical rule, Core tag, image format, or independent acceptance authority.

Against `14d52a2`: implementation/header `eval.c` +12/-0, `eval.h` +4/-0,
`evidence.c` +53/-7 and `evidence.h` +6/-0: net **+68**, cumulative **+1385**.
Tests are +43/-0. The net-negative gate remains unmet.

Sequential O0 QuickSort samples: R25 **1.0864 s / 275928 KiB**, R26
**1.0772 s / 276048 KiB**. Both used **132011** Solve transitions. R26 retained
the same **179803 Terms / 416611 typed subjects / 433565 proofs / 537 body
requests**. This does not establish a speedup or resolve the cumulative cost
regression documented above. Logs: `/tmp/a-program-typed-structure-r26-debug.log`
and `/tmp/a-program-typed-structure-r26-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Scoped congruent NF inputs (R27)

- [x] Use the shared structural input query for mapped NF inputs. Certify
  descriptive binder lifts with the existing substitution-lift rule, requiring
  the resulting map to equal the requested map exactly. Unproject declaration
  variables only when the surviving declaration has the same sort/classifier.
- [x] Align a reduction receipt with its typed premise by explicit alpha
  comparison. Whole-term and child substitution can independently freshen
  bound pointers. This does not rename free variables, intern alpha-equal Core,
  change classifiers, or promote propositional Identity into conversion.
- [x] Remove a duplicate input traversal in RETURN/THUNK content inversion.
- [x] Test projection/nonidentity substitution under Lambda/THUNK, preserved
  result scopes and classifiers, two distinct constructor field positions,
  repeated lookup reuse, wrong lifted domains, removed variables and changed
  free arguments. Fresh-process derivation Solve accepts alpha-aligned NF
  receipts with chunks one/64; it still computes and checks the saved target.
- [x] Debug acceptance passed through all final QuickSort source/image results
  (63/63 compatibility). ASan/UBSan Core, IADT, derivation IO, synthesis and
  imported QuickSort passed. No image format or new proof rule was added.

Against `2a484c0`: implementation/header `evidence.c` +58/-15,
`evidence.h` +3/-3, `typing.c` +6/-0, `typing.h` +2/-1: net **+50**,
cumulative **+1435** against `4657cc6`. Tests are +96/-13. The net-negative
gate remains unmet; this is not completion of R3 or R5.

Sequential O0 QuickSort samples: R26 **1.0593 s / 276548 KiB / 132011 steps**;
R27 **1.0722 s / 276112 KiB / 131899 steps**. R27 retained **179362 Terms /
415826 typed subjects / 435123 proofs / 537 body requests**, versus R26
179803/416611/433565/537. Fewer typed subjects but more checked derivations do
not establish a speedup or resolve the baseline memory regression. Logs are
`/tmp/a-program-typed-structure-r27-debug.log` and
`/tmp/a-program-typed-structure-r27-san-{core,iadt,derivation,synthesis,quicksort}.log`.

Remaining: receipt traversal/final certification are synchronous; nested
normalization recipes and head-changing beta/iota result exposure are not
generalized. Constructor NF input tests validate the checked input API, not
`constructor_origin` rebuilding a dependent constructor with normalized fields.
That consumer still needs coherent field-classifier conversion before replacing
its source-field reconstruction. Full final optimized/sanitized acceptance,
representation-cost reduction and Main publication remain outstanding.

### 2026-09-17: Constructor field selection (R28)

- [x] Reproduce the stale-field path: selecting a field from a normalized
  two-field constructor returned the source redex rather than the current NF
  field. The new IADT assertion failed before the implementation change.
- [x] Resolve field positions from the checked nominal declaration, verify its
  erased layout/saturation, and read the checked typed input. Do not rebuild
  the complete constructor introduction just to select an available input.
  Retain the existing computed-construction exposure for unavailable inputs;
  reject a historical field whose Core is not alpha-equal to the current field.
- [x] Test distinct field positions and a dependent package `(A : @, x : A)`
  whose type field reduces. The second field keeps its existing conversion
  evidence/classifier: normalization does not silently retag it with the first
  field's new Core. Repeated selection creates no additional proofs.
- [x] Debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image results. ASan/UBSan Core, IADT, synthesis and imported
  QuickSort passed. `git diff --check` passed; no rule or format change.

Against `fc4af49`: `evidence.c` +36/-7, `evidence.h` +4/-4: implementation net
**+29**, cumulative **+1464**. Tests are +31/-0. This replaces the unconditional
field reconstruction path, not all of `constructor_origin`; the net-negative
gate and R3/R5 remain open. In particular, maps outside normalization recipes,
general head-changing result exposure, and coherent reconstruction of an entire
dependent normalized constructor still need the shared result-view work.

Sequential O0 QuickSort: R27 **1.0945 s / 275964 KiB**, R28
**1.0698 s / 275332 KiB**, both **131899** Solve transitions. R28 retained
**179362 Terms / 415826 typed subjects / 435124 proofs / 537 body requests**,
one more proof than R27. These single samples do not establish a speedup or
resolve the cumulative representation cost. Logs:
`/tmp/a-program-typed-structure-r28-debug.log` and
`/tmp/a-program-typed-structure-r28-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Composed normalized input views (R29)

- [x] Reproduce failed constructor-field selection after projection or
  nonidentity substitution of a normalized constructor. Compose these maps
  and congruent NF receipts inside-out using checked input scopes and the
  ordinary substitution-lift/reindex/normalization rules.
- [x] Share receipt-spine selection with the public normalization-input API.
  Treat unchanged-Core NF certificates as checked normality, including cached
  certificates without rebuilding phases. No new reduction or proof rule.
- [x] Test Lambda/THUNK scopes, swapped constructor fields, 1000 mapped layers,
  chunks one/64 and repeated lookup without additional accepted records.
  Repeated traversal time is not asserted to be cached by these tests.
- [x] Full debug acceptance passed through the final source/image QuickSort
  comparisons (63/63 compatibility). ASan/UBSan Core, IADT, synthesis and
  imported QuickSort passed. No image version change.
- [x] Audit R1 independently of the unfinished structural view work:
  `accept_record` is the only Evidence allocator; its term conclusion is one
  typed-subject pointer. `pg_evidence_judgement`, `pg_evidence_context` and
  `pg_evidence_classifier` read that record. The occurrence interner keys
  context/Core/classifier/sort/structure and distinguishes pending INPUT.
  Tests retain same-Core/different-classifier uses, alternative derivations,
  owner isolation and ordinary image acceptance. No mutable classifier answer
  is duplicated in Evidence. R1 is complete; R2-R5 are not.

Against `d8085bc`, `evidence.c` is +70/-24: implementation net **+46**,
cumulative **+1510** against `4657cc6`. Tests are +50/-0. The representation
and net-negative gates remain unmet. The temporary input frame stack does not
add a permanent typed graph, but its synchronous traversal/certification still
needs to join shared budgeted result exposure. General head-changing beta/iota
and dependent constructor reconstruction are not completed by this change.

Sequential O0 QuickSort: R28 **1.0920 s / 276012 KiB**, R29
**1.0521 s / 276140 KiB**, both **131899** Solve transitions. R29 retained
**179362 Terms / 415830 typed subjects / 435124 proofs / 537 body requests**.
These single samples do not establish a speedup or resolve the baseline memory
regression. Logs: `/tmp/a-program-typed-structure-r29-debug.log` and
`/tmp/a-program-typed-structure-r29-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Retain classifier-boundary origins (R30)

- [x] Conversion and effect widening reference their original typed use and
  the target formation, rather than copying the program inputs. Exact reuse
  of the same formation returns the existing subject. A different formation
  is retained even when its Core is identical to the previous classifier.
- [x] Keep `pg_occurrence_classified` as descriptive formation attachment for
  image loading and new inversion records. `pg_occurrence_reclassified`
  describes the checked boundary change instead. Neither function accepts
  evidence. Combining these operations would add spurious origins on readback
  or lose the original introduction on conversion.
- [x] Delete `typed_construction`, including its duplicated Lambda, Match,
  APP, F/U and Fold introduction reconstruction. Construction lookup now uses
  the exact original typed subject's accepted evidence. This is not a search
  for an arbitrary proof sharing erased Core, nor a replacement of alternative
  derivations by one canonical proof.
- [x] Let scoped and general input queries follow unchanged-Core boundaries.
  During context restriction, skip only boundaries preserving Core,
  classifier and sort; other conversions still require checking.
- [x] Add direct tests for converted Lambda scope, widening origin, repeated
  construction lookup without new accepted records, and fresh-process inert
  images whose classifier Core is unchanged but formation identity differs.
  Existing tests exposed two missed consumers: saved Match branch scopes and
  restriction of converted parameter images in direct List recursion. Both
  are fixed without relaxing their assertions. The branch-name collision
  fixture continues to be unsupported, rather than silently losing branches.
- [x] Full debug acceptance passed (63/63 compatibility and all final
  QuickSort source/image results). ASan/UBSan Core, IADT, synthesis, occurrence
  IO, saved Match scope and imported QuickSort passed. `git diff --check`
  passed. An R29 retained typed Match image was also resaved by R30 and passed
  both retained checking and recomputation; no wire grammar/version changed.

Against `83ef616`: `evidence.c` +10/-66, `typing.c` +16/-1, `typing.h` +5/-1:
implementation/header net **-37**, cumulative **+1473** against `4657cc6`.
Tests: `core.c` +30/-3, `occurrence_io.c` +12/-3, net **+36**. This deletes
one real reconstruction path, but does not meet the whole-change reduction
gate. Constructor reconstruction, sort/content boundary copies, generalized
typed reduction exposure and synchronous certification remain open.

Sequential O0 QuickSort: R29 **1.0638 s / 275996 KiB**, R30
**1.0753 s / 275944 KiB**, both **131899** Solve transitions. R30 retained
**179313 Terms / 415798 typed subjects / 435044 proofs / 537 body requests**,
with 3497 structural input requests. These single samples show no established
speedup and do not resolve the baseline memory regression. Logs:
`/tmp/a-program-typed-structure-r30-debug.log` and
`/tmp/a-program-typed-structure-r30-san-{core,iadt,synthesis,occurrence,match,quicksort}.log`.

### 2026-09-17: Constructor inspection without re-introduction (R31)

- [x] Replace `constructor_origin` with a temporary structural view of nominal
  formation, parameter substitution and typed fields. Check constructor layout
  and saturation; compose retained maps through ordinary checked substitution.
  No new permanent graph, rule, Core tag or image format is introduced.
- [x] Match unfolding and refinement factoring read those fields directly,
  rather than rebuilding `PG_CONSTRUCTOR_INTRO` and inspecting its premises.
  Context restriction still introduces a constructor when its context really
  changes. Current NF field selection retains the existing receipt/Core checks.
- [x] Count constructor introductions around computed/mapped/scoped field and
  Match queries. The new test fails against R30 at computed field selection;
  it passes with R31. Existing dependent-field and nominal rejection tests pass.
- [x] Full debug acceptance passed (63/63 compatibility, final source/image
  QuickSort comparisons). ASan/UBSan Core, IADT, synthesis and imported
  QuickSort passed. `git diff --check` passed.

Against `7122351`: `evidence.c` **+89/-71**, implementation net **+18**;
`tests/iadt.c` **+22/-0**. Cumulative implementation/header net is **+1491**
against `4657cc6`: the net-negative gate remains unmet. The view exposes source
construction through maps, not arbitrary current NF structure. Shared budgeted
result exposure, sort-boundary copies and R2-R5 completion remain open.

Sequential O0 QuickSort: R30 **1.0699 s / 276372 KiB**, R31
**1.0552 s / 276008 KiB**, both **131899** Solve transitions. R31 retains
**179313 Terms / 415723 typed subjects / 434924 proofs / 537 body requests**,
with 3491 structural input requests. Single samples establish no speedup;
the baseline memory regression remains unresolved. Logs:
`/tmp/a-program-typed-structure-r31-debug.log`,
`/tmp/a-program-typed-structure-r31-before-test.log` (expected old-code failure),
`/tmp/a-program-typed-structure-r31-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Sort and content boundary sharing (R32)

- [x] `pg_occurrence_boundary` references the original typed subject instead of
  copying its operands, selected maps and induction allocation. Exact no-ops
  and type/value round trips reuse existing subjects without erasing evidence.
  A changed retained formation does not qualify for round-trip cancellation.
- [x] Context restriction traverses unchanged-Core/classifier sort boundaries
  and restores the requested judgement using checked type/value rules. Existing
  rebasing tests caught this required consumer change; their checks remain.
- [x] Update structural tests to assert origin sharing instead of copied
  arrays, including converted Return content and fresh-process induction/map
  transport. Full debug acceptance passed (63/63 compatibility and final
  QuickSort comparisons). ASan/UBSan Core, IADT, synthesis, occurrence IO and
  imported QuickSort passed. R31 retained typed Match images also pass R32
  resave/check/recompute. No wire grammar/version or logical rule changed.

Against `182f48d`: implementation/header `evidence.c` **+1/-2**, `typing.c`
**+8/-4**, `typing.h` **+2/-1**, net **+4**; tests **+11/-7**, net **+4**.
Cumulative implementation/header net **+1495** against `4657cc6`; R5 and the
net-negative gate remain open. Descriptive construction/formation attachment
still builds allocation tuples; this is not a claim that all allocation
overhead or synchronous result recovery has been removed.

Sequential O0 QuickSort: R31 **1.0877 s / 276108 KiB / 131899 transitions**;
R32 **1.0773 s / 276036 KiB / 131961 transitions**. R32 retains
**179313 Terms / 415652 typed subjects / 434853 proofs / 537 body requests**,
with 3491 input requests. The 62 extra transitions and small single-sample time
difference do not establish a performance improvement; baseline memory remains
regressed. Logs: `/tmp/a-program-typed-structure-r32-debug.log` and
`/tmp/a-program-typed-structure-r32-san-{core,iadt,synthesis,occurrence,quicksort}.log`.

### 2026-09-17: One body machine for nominal recovery (R33)

- [x] Remove `inductive_fold` and nominal recovery's Return/Thunk counters.
  Nominal recovery advances shared Return/application body requests instead;
  declaration ownership, parameter maps and dependent index checks remain.
- [x] Share Force/Thunk traversal for applications and returned computations.
  Mapped suspension variables and computed suspension contents resume through
  the existing body continuation frames. A new nested-thunk test exposed a
  missing case after the initial consolidation; it now passes without restoring
  a second continuation machine. No effects are executed by these queries.
- [x] Test dependent indexed-family recovery through Force/Fold and assert it
  completes the same cached body request used by term recovery. Chunked and
  repeated queries retain results. Existing nominal and context rejection
  checks remain. Full debug acceptance passed after the nested-thunk fix,
  including 63/63 compatibility and final QuickSort source/image comparisons.
  ASan/UBSan Core, IADT, synthesis and imported QuickSort passed.

Against `739ffb9`: `evidence.c` **+31/-70**, `evidence.h` **+1/-3**,
implementation/header net **-41**; tests **+41/-2**, net **+39**. Cumulative
implementation/header net is **+1454** against `4657cc6`. The whole-change
reduction gate, general normalized result exposure and R2-R5 remain open.

Sequential O0 QuickSort: R32 **1.0726 s / 275420 KiB / 131961 transitions**;
R33 **1.0601 s / 276088 KiB / 132079 transitions**. R33 retains
**179313 Terms / 415696 typed subjects / 434897 proofs / 529 body requests**,
with 3491 input requests. These single samples establish no speedup or memory
improvement. Logs: `/tmp/a-program-typed-structure-r33-debug.log`,
`/tmp/a-program-typed-structure-r33-nested-before.log` (reproduced missing case),
`/tmp/a-program-typed-structure-r33-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Intern complete structural tuples once (R34)

- [x] Use a stack header of the existing `pg_occurrence` structure to describe
  a complete node. `pg_occurrence_intern` copies operand/map arrays and lexical
  allocation into one immutable interned record. There is no extra descriptor
  graph, Core tag, acceptance bit, or alternative proof representation.
- [x] Remove the three metadata-attachment APIs. Match/induction and dependent
  Identity no longer intern partial records before adding maps or allocation.
  Sort/conversion boundaries retain their distinct origin-preserving APIs;
  descriptive construction must not replace accepted typing derivations.
- [x] Read complete image tuples in one operation. Descriptive formation,
  selected arguments, ordered maps and induction scopes retain exact identity.
  Missing formation metadata stays missing; the reader no longer fills it
  from a same-classifier origin. A round-trip regression failed before this
  correction. No wire fields or version changed, and readback accepts no proof.
- [x] Add checks that complete construction increments the occurrence index
  once, repeated interning adds nothing, recursive elimination has no stripped
  intermediate nodes, and image read creates exactly its serialized node count.
  Keep invalid classifier/scope/map checks and all prior negative fixtures.
- [x] Full debug acceptance passed after the readback correction, including
  63/63 compatibility and final QuickSort source/image comparisons. ASan/UBSan
  Core, IADT, synthesis, occurrence transport and imported QuickSort passed.
  An R33 retained typed Match image can be resaved, checked and recomputed by
  R34; R33 also checks R34's retained typed Match image. This does not waive
  the final fixed-snapshot full optimized/debug/sanitizer gates.

Against `b9dc294`:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| `typing.c` | 36 | 56 | -20 |
| `typing.h` | 7 | 9 | -2 |
| `evidence.c` | 12 | 6 | +6 |
| `occurrence_io.c` | 19 | 20 | -1 |
| **Implementation/header** | **74** | **91** | **-17** |
| `tests/iadt.c` | 8 | 1 | +7 |
| `tests/occurrence_io.c` | 63 | 15 | +48 |

Cumulative implementation/header delta remains **+1437** against `4657cc6`.
Sequential, alternating O0 QuickSort samples (seconds / peak KiB):

| Sample | R33 | R34 |
|---|---:|---:|
| 1 | 1.0689 / 275688 | 1.0484 / 275944 |
| 2 | 1.0549 / 276204 | 1.0754 / 275840 |
| 3 | 1.0808 / 275688 | 1.0718 / 276016 |

Both use **132079 Solve transitions**. R34 retains **179313 Terms / 415569
typed subjects / 434897 proofs / 529 body requests / 3491 input requests**:
127 fewer typed nodes, with other counts unchanged. These samples show no
meaningful speed or peak-memory improvement. Logs:
`/tmp/a-program-typed-structure-r34-final-debug.log`,
`/tmp/a-program-typed-structure-r34-readback-before.log` (reproduced failure),
`/tmp/a-program-typed-structure-r34-san-{core,iadt,synthesis,occurrence,quicksort}.log`.

This allocation consolidation does not close general normalized-result access.
In particular, the common typed-body machine does not yet expose Match results;
recursive branch inspection requires checked classifier/induction services and
must not instantiate a second classifier owner merely to reuse a function.
R2-R5 and the whole-change net-negative gate remain open.

### 2026-09-17: Reuse accepted Identity formations (R35)

- [x] Remove Identity introduction reconstruction from `action.c`. Boundary
  inspection selects an accepted Identity formation by the exact typed subject;
  it does not look up an arbitrary same-Core term or replace other derivations.
  The existing boundary-view rule check distinguishes the applicable theorem.
- [x] Construct an Identity using a non-first derivation of its family, then
  inspect it without adding proof or subject records. The test fails on R34,
  which reconstructs the theorem with the first child receipt instead. A second
  valid derivation of the same Identity remains independently retrievable.
- [x] Recheck all occurrence-construction calls outside tests: only `typing.c`,
  `evidence.c` and inert `occurrence_io.c` publish these records. Each accepted
  producer is classified in the role/scope table above, including family
  abstractions, family-domain Pi, selected Identity, endpoint inversion and
  F/U. Structural query helpers create descriptive maps/results, not acceptance.
  The original named consumers are covered by the deletion ledger. General
  head-changing/scoped normalized results remain explicitly unfinished.
- [x] Revalidate the R0 baseline evidence: the recorded `4657cc6` debug log
  ends with the full QuickSort source/image comparisons; the earlier LOC ledger
  and expanded source/zero-work-image matrix retain that baseline. R0 is an
  audit gate and is now closed. R5 still requires final repeated measurements,
  all build modes and net-negative implementation/header LOC.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image comparisons. ASan/UBSan Identity, Core, synthesis,
  Identity transport and imported QuickSort passed. These checkpoint checks do
  not replace R5's final fixed-snapshot gates.

Regression log: `/tmp/a-program-typed-structure-r35-identity-before.log`.
No new logical rule, typed record, image version or query index was introduced.

Against `ec1cf7c`, `action.c` is +11/-28 (**-17** implementation lines),
and `tests/identity.c` is +18/-1 (**+17** test lines). Cumulative
implementation/header delta remains **+1420** against `4657cc6`; the whole-change
reduction target is still unmet. A sequential O0 QuickSort diagnostic run took
1.0674 seconds / 276120 KiB / 132079 Solve transitions. This is not a speedup
claim. Acceptance log: `/tmp/a-program-typed-structure-r35-debug.log`;
sanitizer QuickSort log: `/tmp/a-program-typed-structure-r35-san-quicksort.log`.

### 2026-09-17: Fold results in callee position (R36)

- [x] Reproduce a typed-body query failure for a checked zero-clause Fold
  whose continuation returns a raw Pi computation. The ordinary Fold rule and
  evaluator already support this term; the query had an extra condition that
  recognized Fold only when no application argument was pending.
- [x] Remove that condition. The existing continuation frame already preserves
  the pending application argument, context maps and Force demand while it
  obtains the prefix's returned value and enters the continuation. No new
  computation constructor, typing rule or wrapper graph is needed.
- [x] Extend the shared-body regressions with direct, normalized, Force/Thunk
  and projected Fold callees; verify accepted result derivations, bounded
  advancement and repeated lookup reuse. A neutral prefix remains unavailable
  even if the continuation ignores its argument; callee lookup must not skip it.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image comparisons.
- [x] ASan/UBSan Core, IADT, synthesis and imported QuickSort passed.

Before-fix regression: `/tmp/a-program-typed-structure-r36-fold-before.log`.
Acceptance log: `/tmp/a-program-typed-structure-r36-debug.log`.
Against `8de90db`, `evidence.c` is +1/-1 (net zero implementation lines), and
`tests/core.c` is +14/-2 (+12 test lines). The cumulative implementation/header
delta is still +1420 against `4657cc6`. This removes one unsupported body-query
case, not the outstanding general Match/IH result-query and R2-R5 obligations.

Sequential O0 QuickSort: 1.0728 seconds / 275272 KiB / 132053 Solve transitions
(R35: 1.0674 seconds / 276120 KiB / 132079 transitions). R36 retains 178465
Terms / 415460 typed subjects / 434767 proofs / 506 body requests / 3442 input
requests. The work/allocation counts decreased; these single timings do not
establish a wall-time improvement. Sanitizer logs:
`/tmp/a-program-typed-structure-r36-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Shared nonrecursive Match body work (R37)

- [x] Share constructor/branch selection between elimination inspection and
  typed-body work. The selected branch and fields retain their checked nominal
  formation and effective scope. Ordinary typed APP connects them; no Match
  introduction, second classifier owner or new logical rule is synthesized
  just to inspect the result. Recursive IH construction remains separate.
- [x] Carry outer context maps through scrutinee and branch selection. Return
  extraction and callee application now enter a nonrecursive Match through the
  same body machine instead of reporting this construction as unsupported.
- [x] Treat typed RETURN extraction and TOTAL pure-result projection as demands
  on their original computation. A constructor query can suspend on shared
  body work instead of recursively running another C query. Explicit waiting
  frames charge dependency transitions to the advancing root; completed work
  can be supplied by another caller. This does not run operation requests or
  equate TOTAL with an empty effect row.
- [x] Test dependent indexed Match, renamed/reindexed contexts, raw-Pi branch
  results, neutral scrutinee rejection and 2048 nested computed scrutinees.
  Verify chunked advancement, independently advanced shared dependencies,
  result scope/classifier, ordinary derivation reconstruction and stable
  completed request reuse. The direct Match query regression fails when built
  with `fe200bb`'s `evidence.c` and passes with this implementation.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image comparisons. ASan/UBSan Core, IADT (including nested
  Match dependencies), synthesis and imported QuickSort passed.

Before-fix log: `/tmp/a-program-typed-structure-r37-match-before.log`.
This checkpoint does not expose every recursive/normalized result. Individual
kernel certification and constructor inspection still have synchronous costs;
the dependency-transition budget does not bound all C instructions. General
IH result queries, pending-image work retention and the R2-R5 gates stay open.
Current delta against `fe200bb`: `evidence.c` +100/-20, `evidence.h` +3/-1,
`tests/iadt.c` +59/-0. Implementation/header net is **+82**, cumulative **+1502**
against `4657cc6`; this increases rather than satisfies the deletion debt.

Sequential O0 QuickSort: 1.1036 seconds / 275424 KiB / 132053 Solve transitions.
Two additional alternating time samples were R36 1.0727/1.0769 seconds and
R37 1.0485/1.0637 seconds. No significant speedup/regression is established.
Counts remain 178465 Terms / 415460 typed subjects / 434767 proofs / 506 body
requests / 3442 input requests. Logs:
`/tmp/a-program-typed-structure-r37-debug.log`,
`/tmp/a-program-typed-structure-r37-san-{core,iadt,synthesis,quicksort}.log`.

### 2026-09-17: Shared motive scope queries (R38)

- [x] Extend the shared construction-input request to lift a Match/induction
  motive's dependent telescope, not just a single Lambda/Pi binder. Domain
  substitution uses existing resumable work; colliding binders are freshened.
  Lambda/Pi's exact lexical checks remain unchanged. A motive must extend the
  enclosing context; an unrelated descriptive scope is not accepted.
- [x] Retain nominal formations and selected Identity source families in their
  declaration contexts. Their explicit parameter/boundary maps, not ambient
  weakening of these inputs, describe their use at an elimination site.
- [x] Replace `elimination_instance`'s private motive-lifting call with the
  shared input query. Ordinary context-map and term rules still certify the
  result; this does not grant acceptance to descriptive records or change the
  elimination theorem. Constructor/refinement scope formation still uses
  `lift_scope`; its remaining uses are not redundant queries.
- [x] Fix exact weakening cancellation exposed by this migration. After
  reaching the requested context, `pg_occurrence_unproject` unnecessarily
  applied an identity map. For a variable with a converted classifier, that
  added another structural origin and broke exact lifted-map reconstruction.
  Return the original typed subject at that boundary instead of relaxing map
  equality. The focused Core regression fails against `84ec50e`'s `typing.c`.
- [x] Test dependent multi-index motives, chunk sizes 1/64, repeated query
  sharing, binder collisions, retained declarations/families, converted-variable
  weakening and unrelated-scope rejection. Repeated equivalent eliminations
  may now reuse an already completed body query; zero budget changes no steps.
- [x] Full debug acceptance, including 63/63 compatibility and final QuickSort
  source/image properties, passed. ASan/UBSan Core, IADT, Identity, synthesis
  and imported QuickSort passed.

The first full run exposed the weakening regression in QuickSort's property
client; it was fixed, not excluded from acceptance. Logs:
`/tmp/a-program-typed-structure-r38-unproject-before.log`,
`/tmp/a-program-typed-structure-r38-debug.log`,
`/tmp/a-program-typed-structure-r38-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `84ec50e`: `typing.c` +51/-17, `typing.h` +3/-1, `evidence.c` +8/-4;
implementation/header **+40**, cumulative **+1542** against `4657cc6`.
Tests: `core.c` +17/-0, `iadt.c` +43/-2, `identity.c` +11/-0.
R2-R5 and the net-negative gate remain open. Scope enumeration and ordinary
certification still have synchronous costs; this is not a full instruction
budget. No image format or logical rule changed.

Sequential O0 QuickSort: 1.0728 seconds / 275684 KiB / 132053 Solve transitions.
Counts: 181118 Terms / 415469 typed subjects / 434780 proofs / 506 body requests /
3458 input requests. Compared with R37, the migration allocates 2653 more Terms
and 16 more input requests because it first reindexes the whole eliminator to
open its motive. This is a remaining avoidable construction cost, not a speedup;
future shared scope-action work should request the input under its map directly
without fabricating an unverified mapped parent. Keep the declaration-scope and
exact weakening regressions when removing that intermediate construction.

### 2026-09-17: Request mapped inputs without rebuilding parents (R39)

- [x] Add an optional outer context map to the existing shared input request.
  Exact `(subject, input index, map)` requests share work. Mismatched source
  contexts reject; the query publishes no acceptance evidence. Scoped inputs
  retain capture avoidance, dependent telescope action and declaration scope.
- [x] Remove the whole-eliminator reindex previously needed to obtain its
  motive. `elimination_instance` requests that input under the substitution
  directly and checks it with ordinary structural/context rules. There is no
  fabricated parent, second work graph, new logical rule or wire change.
- [x] Test mapped Lambda and dependent indexed motives, zero/chunked budgets,
  completed request reuse, exact motive identity, source-map rejection and
  absence of evidence publication during structural queries. An opened Lambda
  binder can differ from a separately substituted closed Lambda's binder;
  compare the closed terms modulo alpha, not by interning them together.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image properties. ASan/UBSan Core, IADT, Identity, synthesis
  and imported QuickSort passed.

Against `6119f8a`: `evidence.c` +4/-3, `typing.c` +25/-10, `typing.h` +4/-0;
implementation/header **+20**, cumulative **+1562** against `4657cc6`.
Tests: `core.c` +9/-0, `iadt.c` +15/-0. R2-R5 and net-negative acceptance
remain open; this checkpoint removes unnecessary construction, not the
remaining structural recovery adapters.

Sequential O0 QuickSort: 1.0880 seconds / 275760 KiB / 132053 Solve transitions.
Counts: 178449 Terms / 415460 typed subjects / 434771 proofs / 506 body requests /
3458 input requests. Compared with R38, Terms decrease by 2669, typed subjects
and proofs by nine each. Time and memory remain similar; no speedup is claimed.
Logs: `/tmp/a-program-typed-structure-r39-debug.log`,
`/tmp/a-program-typed-structure-r39-san-{core,iadt,identity,synthesis,quicksort}.log`.

Remaining scope prerequisite: checked family lifting allocates its signature's
index binders independently of structural signature substitution. Consequently
alpha-equivalent signatures can still produce different exact destination
contexts/maps. Do not relax exact map checking or treat a family signature as
a value type to bypass this. A shared signature/telescope allocation contract
must account for ambient binder collisions and nested family parameters before
removing that fallback.

### 2026-09-17: Retain family telescopes and share scoped allocation (R40)

- [x] Resolve R39's family-scope prerequisite. A family declaration now retains
  its index telescope in the existing Context DAG. Exact context interning
  includes that edge. It is descriptive data, not a formation certificate;
  raw descriptions still require ordinary checking.
- [x] Share logical signature construction between structural scope action,
  family context formation and IADT formation. Remove Evidence's private
  signature-building routine and synthesis's readback loop that reconstructed
  the same index declarations from Pi syntax and a separate declaration scope.
- [x] Use one memoized lifting request for value and family declarations.
  Nested signatures preserve their dependent telescope and freshen collisions
  once in the structural producer. Checked lifting uses those exact target
  binders and verifies the resulting map, rather than allocating another
  alpha-equivalent telescope. No alpha interning or relaxed map equality.
- [x] Advance nested lifting and substitution through explicit waiting frames.
  Construction-input queries use this same work instead of their private
  domain-substitution path. Zero budget does no work; completed requests share
  results. Telescope enumeration, signature closing and kernel certification
  still have synchronous costs: the transition budget is not an instruction
  or wall-time bound.
- [x] Preserve the additional Context edge in the existing relocation DAG.
  Formats become `APGCTX3`, `data-declaration/v3`, derivation version 14 and
  source versions 44/45 (recompute/retained). Old formats reject; rebuild from
  source rather than treating old payloads as the new schema. Source 42/43
  and Context 2 rejection are covered explicitly. No independent Replay path.
- [x] Cover exact checked/descriptive map identity, nested family parameters,
  ambient binder collisions, mapped Pi bodies, chunked work, repeated lookup
  without new Terms/proofs, malformed signatures, Context cycles and shared
  family telescope roundtrips with no imported acceptance bits.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort image properties. The final additional boundary tests passed in
  debug and ASan/UBSan. Sanitizer Core, IADT, Identity, synthesis, graph/context
  transport, source-image/retention, seed and imported QuickSort passed; the
  affected synthesis/source tests were rebuilt after removing the readback loop.

Logs: `/tmp/a-program-typed-structure-r40-debug.log`,
`/tmp/a-program-typed-structure-r40-{core,graph,seed}.log`, and
`/tmp/a-program-typed-structure-r40-san-{core,iadt,identity,synthesis,graph,source,seed,quicksort}.log`.

Against `55eda9b`, implementation/header net is **+145**, cumulative **+1707**
against `4657cc6`. Per-file nonzero nets: `typing.c` +170/-22 (+148),
`typing.h` +16/-0 (+16), `evidence.c` +21/-30 (-9), `synthesis.c` +3/-19 (-16),
`context_payload.c` +21/-16 (+5), `context_payload.h` +2/-1 (+1).
Version-only files have net zero: `context_io.c` +1/-1,
`declaration_io.c` +2/-2, `derivation_io.c` +1/-1, `source_io.c` +2/-2,
`source_io.h` +3/-3. Tests: `core.c` +45/-2, `graph_acceptance.c` +23/-5,
`seed.c` +2/-2. This does not satisfy the net-negative gate.

Sequential O0 QuickSort: 1.0946 seconds / 278380 KiB / 132053 Solve transitions;
178449 Terms / 416924 typed subjects / 434771 proofs / 14619 shared lift
requests / 3458 input requests. Compared with R39, time is similar, Terms and
proofs are unchanged, typed subjects increase by 1464 and peak RSS by 2620 KiB.
The new shared work has a storage cost; no speedup is claimed. R2-R5 remain
open, including normalized/strengthened result access, obsolete recovery
removal, final performance/memory review and net code reduction. Main is not
published by this checkpoint.

### 2026-09-17: Check shared family lifting dependencies (R41)

- [x] Remove Evidence's separate family telescope walk and its `lift_frame`
  and `lift_index` records. Use the existing temporary `pg_dag` over prefix
  and signature maps, then ordinary checked context formation and pairing.
  No additional persistent program graph, logical rule or image format.
- [x] Share destination formation between descriptive map checking and
  explicit checked lifting. Require the exact structurally allocated Context;
  alpha-equivalent allocation does not substitute for that identity.
- [x] Preserve explicit prefix/source derivations. Do not first construct an
  alternative root substitution just to obtain its destination context.
  For an already checked destination, generic image checking does not require
  reconstructing an unproved strengthened prefix. Completed maps are reused.
- [x] Cover 64 nested family signatures with ambient binder collisions,
  repeated lookup without new proofs, checked-prefix reuse, ordinary maps
  without an accepted prefix, and two distinct valid prefix derivations.
  A fresh explicit lift must publish the requested derivation first, not an
  unrequested alternative assembled by structural lookup.
- [x] ASan/UBSan Core, IADT, Identity, synthesis and imported QuickSort passed.
- [x] Final full debug acceptance after the premise-preservation correction:
  63/63 compatibility cases and final QuickSort source/image property checks
  passed; the complete command exited successfully.

The intermediate implementation passed functional tests but increased
QuickSort proofs from 434771 to 460048. Rule counts localized the increase
mostly to Projection (+20942) and Substitution (+4344): structural checking
produced an alternative before explicit lifting used the caller's premises.
This was redundant certificate construction, not a demonstrated invalid
judgement. Reusing checked prefixes alone did not fix the fresh-context case.
The final correction shares destination formation and pairs the supplied
premises directly. No alternative proof is overwritten or discarded.

Final narrow O0 QuickSort: 1.1130 seconds / 278684 KiB / 132053 transitions;
178454 Terms / 416930 typed subjects / 434774 proofs / 14619 lift requests.
Compared with R40, Terms +5, typed subjects +6, proofs +3, lift requests and
Solve transitions unchanged. No speedup is claimed. Logs:
`/tmp/a-program-typed-structure-r41-debug.log`,
`/tmp/a-program-typed-structure-r41-{core,iadt}.log`, and
`/tmp/a-program-typed-structure-r41-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `b50b1e0`, `evidence.c` +107/-101, `typing.c` +5/-0 and `typing.h`
+2/-0: implementation/header **+13**, cumulative **+1720** against `4657cc6`.
`tests/core.c` adds 61 lines. Removing a private walk has not yet reduced the
whole implementation: the shared checked dependency path also needs code.
R2-R5 remain open. In particular, `structural_input` still has the synchronous
normalized/mapped-result adapter, and `rebase_image`/selected formations retain
checked reconstruction work. Do not delete these before their replacements
handle dependent result classifiers and retained image evidence. Main remains
unpublished until the original completion and net-negative gates are met.

### 2026-09-17: Share context action for normalized inputs (R42)

- [x] Remove the manual Lambda/Pi binder selection, scope lifting and reindex
  path from `structural_input`. Route an already exposed normalized child
  through the existing indexed `pg_occurrence_input` work. No additional
  graph, acceptance store, Core tag or image format is introduced.
- [x] Include the supplied child in the exact request key. Direct inspection
  and action on a normalized input must not share an answer merely because
  their parent/index agree. Scope lifting, projection and substitution remain
  one implementation. The caller still establishes that the supplied child
  belongs to the source input; descriptive allocation cannot establish this.
- [x] Check zero/chunked budgets, repeated request identity, unchanged work
  counters after completion, no evidence publication by structural queries,
  and exact Core/classifier/Context agreement for mapped normalized Lambda
  bodies and thunk contents. Invalid entry arguments reject.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image results. ASan/UBSan Core, IADT, Identity, synthesis
  and imported QuickSort passed.

Sequential O0 QuickSort: 1.1323 seconds / 278612 KiB / 132053 transitions.
Counts remain 178454 Terms / 416930 typed subjects / 434774 proofs / 3458
input requests, unchanged from R41. This example does not exercise the new
normalized-input request; the direct Core and constructor-field tests do.
No speedup is claimed. Logs: `/tmp/a-program-typed-structure-r42-debug.log`,
`/tmp/a-program-typed-structure-r42-{core,iadt,quicksort}.log`, and
`/tmp/a-program-typed-structure-r42-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `a67afbf`, implementation/header: `evidence.c` +3/-12, `typing.c`
+20/-9 and `typing.h` +5/-0; net **+7**, cumulative **+1727** against
`4657cc6`. `tests/core.c` adds 19 lines. This is a path consolidation, not
completion of R2-R5 or satisfaction of the net-negative gate. The receipt
selection and temporary normalization frames in `structural_input` remain;
head-changing and dependent normalized results must not be presented as
fully exposed typed structure. Main remains unpublished.

### 2026-09-17: Budget and memoize checked input exposure (R43)

- [x] Replace the temporary normalization/input frame stack with indexed
  checked queries. Rename the existing `typed_bodies` work index/API to
  `typed_queries`; body and input requests share allocation, exact-key lookup,
  dependency waiting and budget accounting. The input ordinal distinguishes
  a query, not a Core node or a logical proof rule. No additional work index,
  persistent program graph, image format or acceptance authority is added.
- [x] Accept only checked subjects as seeds for checked input queries. Keys
  use the typed subject and query arguments, not the selected derivation.
  Alternative derivations remain available and share a structural answer.
  Raw descriptive input work still publishes no acceptance evidence.
- [x] Remove `direct_structural_input`, `normalized_input` and `input_frame`.
  `structural_input` is now a blocking adapter to the shared query. Traversal
  of a congruence receipt's APP spine advances one phase per transition;
  alpha comparison, exact receipt lookup and individual kernel checks retain
  their own synchronous costs. This is not a wall-time/instruction bound.
- [x] Resume a blocked structural query with its exposed normalized child
  and retained immutable map spine. Replace R42's immediate-parent reindex
  entry point rather than keep two scope-action APIs. The original query is
  not mutated. A provisional per-origin dependency scheme was rejected:
  restarting raw traversal at every prefix can scan a deep map chain
  quadratically. The final implementation scans and resumes that chain once.
- [x] Cover zero/split budgets, completed reuse without added work or evidence,
  equivalent typed subjects with distinct derivations, invalid request inputs,
  exact normalized child scope/type, and 1000 composed maps. The deep checked
  input completes in 2019 transitions with budget one; its repeated lookup
  adds none. A head-changing beta result remains unavailable instead of
  returning the original APP's callee as the resulting RETURN's child.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image properties. ASan/UBSan Core, IADT, Identity, synthesis
  and imported QuickSort passed. The final additional head-change boundary
  test was rebuilt and passed in both debug and ASan/UBSan Core tests.

QuickSort O0: 1.0436 seconds / 279048 KiB / 132053 transitions. Counts remain
178454 Terms / 416930 typed subjects / 434774 proofs. The shared query index
now contains 2587 body/input requests; raw structural input requests decrease
from 3458 to 3384. The added checked-query memoization has a storage cost;
timing differences from single samples do not establish a speedup. Logs:
`/tmp/a-program-typed-structure-r43-debug.log`,
`/tmp/a-program-typed-structure-r43-{core,iadt,quicksort}.log`, and
`/tmp/a-program-typed-structure-r43-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `2ff31ad`, implementation/header: `evidence.c` +114/-116,
`evidence.h` +15/-9, `synthesis.c` +3/-3, `typing.c` +17/-8 and `typing.h`
+8/-6: net **+15**, cumulative **+1742** against `4657cc6`. Tests:
`core.c` +64/-36, `iadt.c` +37/-30; many changed lines rename the shared work
API rather than add tests. Net-negative acceptance remains unmet.

R2-R5 remain open: head-changing beta/iota result exposure, dependent
normalized classifier transport, selected/strengthened result reconstruction,
final representation/memory review, and net implementation reduction. The
checked-query cache is not completion of those semantics. Main is not pushed.

### 2026-09-17: Share nested typed body computation (R44)

- [x] Delete `typed_body_frame` and `typed_body_resume`. Nested application,
  Fold prefix/continuation and computed returned-value traversal now depend
  on existing indexed body queries, using the same budgeted dependency
  scheduler as checked input exposure. The caller retains its context maps,
  force depth and argument; the child query does not capture those ambient
  values. No Core constructor, proof rule, image format or acceptance owner
  changes.
- [x] Add regression checks that a parent request actually completes the
  shared callee/prefix request, and that a projected caller reuses it without
  additional child transitions. Existing tests retain split budgets, deeply
  nested force/thunk, nominal scopes, converted classifiers and pure-only
  introspection. Source bodies remain checked by ordinary substitution and
  introduction/inversion rules; a cached request is not an acceptance axiom.
- [x] Full debug acceptance passed, including 63/63 source compatibility and
  final QuickSort source/image properties. ASan/UBSan Core, IADT, Identity,
  synthesis and imported QuickSort all passed. Logs:
  `/tmp/a-program-typed-structure-r44-debug.log` and
  `/tmp/a-program-typed-structure-r44-san-{core,iadt,identity,synthesis,quicksort}.log`.

Sequential O0 QuickSort: 1.1277 seconds / 279048 KiB / **131964 transitions**
(R43: 132053). Terms/typed subjects/proofs remain 178454/416930/434774.
Shared typed requests increase from 2587 to 2744; raw input requests remain
3384. This is a measured work-sharing change, not evidence of a wall-time or
memory improvement from a single run. Memoization replaces private evaluation
frames with reusable work; the final storage review remains required.

Against `7ce9f02`: `evidence.c` **+30/-48, net -18**; `tests/core.c` +14/-0.
Cumulative implementation/header delta against `4657cc6` is **+1724**.
The net-negative gate is still unmet. R2-R5 remain open, particularly typed
head-changing beta/iota result exposure and dependent normalized inputs.
This checkpoint does not complete the goal or authorize Main publication.

### 2026-09-17: Expose checked beta/iota result inputs (R45)

- [x] Add a head-exposure query to the existing typed-body machine/index,
  rather than a second evaluator. It follows the same application, Fold,
  nonrecursive Match, force/thunk and context-action work, stopping at a
  checked Lambda, RETURN or THUNK construction. The query kinds distinguish
  work requests, not Core constructors or logical rules.
- [x] Before exposing children of a head-changing result, require the
  independently checked construction's Core and context to match the pure
  receipt's actual head. Exact binder identity is required here; alpha-related
  heads with different bound pointers are not silently given each other's
  scoped inputs. Apply child NF receipts only after that correspondence.
- [x] Share the single-phase NF shape check with `pg_reduction_congruence`;
  its original unchanged-head contract remains intact. The new head-congruence
  view permits an initial contraction but still requires an unchanged final
  head recheck. Descending an APP spine cannot bypass a changed inner head.
- [x] Support a completed WHNF receipt as well as NF. The same typed subject
  can already have an earlier WHNF derivation, so requiring an NF phase at
  every lookup incorrectly hides an otherwise exposed result. A receipt
  without usable rebuilding phases is accepted for input exposure only when
  the checked construction matches its complete target.
- [x] Replace the old beta-result-unavailable regression with an actual
  returned-value check: APP input zero is not RETURN input zero. Check
  Fold/application-to-Lambda scopes and nonrecursive Match-to-RETURN under
  renaming/reindexing. Retain ordinary derivation reconstruction, invalid
  ordinal/source/receipt cases and shared budgeted lookup checks.
- [x] Update two tests that assumed the previous recovery path: a computed
  suspension need not retain an unresolved origin when its actual typed
  input is now available, and nominal recovery need not execute a Return-body
  request after checked input exposure has already supplied its result.
  Tests still check Core, classifier, context, exact nominal formation and
  reuse of the shared checked-input work; no semantic assertion is dropped.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  source/image QuickSort properties. ASan/UBSan Core, IADT, Identity,
  synthesis and imported QuickSort passed. The final extra mapped-beta tests
  also passed in rebuilt debug and ASan/UBSan Core binaries; implementation
  files were unchanged after the full gate.

Idle O0 QuickSort: 1.1286 seconds / 279276 KiB / **131357 transitions**
(R44: 131964). Counts: 180411 Terms (+1957), 414783 typed subjects (-2147),
435281 proofs (+507), 2956 typed queries (+212), 3276 raw inputs (-108).
More result constructions are checked, while fewer derived-subject wrappers
are needed. This is not a memory or timing improvement claim; the final
storage and baseline comparison is still required. Logs:
`/tmp/a-program-typed-structure-r45-debug.log` and
`/tmp/a-program-typed-structure-r45-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `e8773c9`, implementation/header: `eval.c` +8/-2, `eval.h` +4/-0,
`evidence.c` +67/-23 and `evidence.h` +7/-4: net **+57**, cumulative
**+1781** against `4657cc6`. This remains above the required net-negative
finish line. Tests: `core.c` +44/-7, `iadt.c` +10/-3. R2-R5 remain open.
In particular this is not complete recursive
IH exposure, arbitrary multi-phase normalization, dependent normalized-field
classifier transport or general fresh-binder alignment. Main remains unpublished.

### 2026-09-17: Align normalized scoped inputs (R46)

- [x] Share Lambda/Pi input-binder description between raw structural inputs
  and checked normalized inputs. After matching the pure receipt's head up to
  alpha-equivalence, transport the checked child to the receipt's bound pointer
  through the existing context-lift and occurrence-action requests. Free
  bindings are unchanged. No new proof rule or acceptance shortcut is added.
- [x] Follow Pi's codomain through the right-hand Lambda in its erased APP
  representation. Following only the left APP spine incorrectly hid this
  semantic input. The same binder description determines both traversal and
  scope alignment; this is not an arbitrary descendant search.
- [x] Test independently renamed Pi/Lambda normalization sources, nested
  dependent classifiers, and force/thunk-to-Lambda exposure. Check exact child
  Core, binder, parent context and transported classifier, then reconstruct
  each derivation through ordinary rules.
- [x] After fresh-process derivation loading and ordinary Solve, request
  normalized Lambda and RETURN inputs, including renamed receipt sources and
  Fold results. Chunks one and 64 produce the same checked children; repeated
  lookup adds no transitions. Reading alone still accepts no proofs.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image properties. ASan/UBSan Core, IADT, Identity, synthesis,
  derivation write/read/read-bulk and imported QuickSort passed. Derivation
  readers both used 1026 Solve transitions. Logs are under
  `/tmp/a-program-typed-structure-r46-{debug,san-*}.log`.

Idle O0 QuickSort: 1.1466 seconds / 279792 KiB / 131357 transitions, versus
R45's 1.1286 seconds / 279276 KiB / 131357 transitions. These single samples
do not establish a speed or memory improvement.

Against `a7cd3c0`, implementation/header: `evidence.c` +51/-7, `typing.c`
+4/-3 and `typing.h` +4/-0: net **+49**, cumulative **+1830** against
`4657cc6`. Tests: `core.c` +36/-0, `derivation_io.c` +25/-0. The net-negative
gate remains unmet; R2-R5 remain open and Main is not published.

The deletion review also distinguishes strengthening from total substitution:
`selected_formation`, `pi_argument_frames` and `rebase_image` can recover a
constant Pi component in a context without its unused binder. That binder may
have an empty type. Replacing this operation with ordinary substitution would
require inventing an inhabitant. Core free-variable independence alone also
does not discharge retained nominal/context dependencies. Their current
structural reconstruction still needs consolidation/budget review, but it
cannot simply be deleted in favor of a total map.

### 2026-09-17: Current image-boundary audit (R47)

R4 is complete for the current representation; R2, R3 and R5 remain open.
This closes the existing transport migration, not general typed normalization
or the entire refactor. Reopen R4 if the remaining representation work changes
its saved contracts.

| Boundary | Implementation and checked contract |
|---|---|
| Descriptive typed structure | `occurrence_io.c:operand` collects construction/origin/selection inputs, retained formation and typed map images. `pg_occurrences_write/read` preserve judgement, Core, classifier, annotation, maps and induction allocation in APGOCC7. Reading interns full tuples but creates no accepted derivations. |
| Lexical scope | `context_payload.c:parent` traverses both parent and family index telescope. The common pack/unpack path serves Context, declaration and induction payloads; Context v3 preserves pointer sharing after relocation. |
| Derivation requests | `pg_derivation_input_header` exports rule inputs and receipt endpoints/kind, not a copied acceptance flag. `pg_derivation_input_terms` includes induction binders and clause scopes. Derivation v14 loads inert requests; ordinary synthesis/Solve obtains checked receipts and calls the existing kernel rules. |
| Pending source and allocations | `source_io.c:collect_inputs`, `collect_allocation`, `retain_objects` and `retain_dependencies` collect lexical scopes, declaration members, handler binders, prepared producers and retained-reduction dependencies. Context payload packing supplies their shared scope representation. Export inspects work without executing Solve. |
| Retention policy | Source v44/v45 distinguish recompute and retained data. Typed query caches and waiting stacks are not serialized acceptance authorities. Resumed source/rule work goes through the same Solve path. Source loading may establish the ordinary empty context; it does not trust imported term conclusions. |
| Compatibility policy | APGOCC0-6, Context v2 and source v42/v43 reject rather than reinterpret obsolete layouts. Rebuild from source; no implicit migration or separate Replay semantics is added. |

- [x] Extend the existing fresh-process derivation fixture with an independently
  renamed Pi receipt. After ordinary Solve, expose its codomain with the exact
  normalized binder, parent context and body. Share the same checks already
  used for Lambda/RETURN; no new audit runner or implementation code.
- [x] Debug derivation I/O suite passes, including inert inputs, pending effect
  equations, operation/handler execution, nominal constructor/Match/IH/type
  cases, malformed requests and chunk-one/chunk-64 agreement. The expanded
  normalization fixture uses 1185 Solve transitions in both readers.
- [x] The same expanded derivation suite passes under ASan/UBSan, including
  both readers at 1185 Solve transitions.

The full R46 debug gate already covered occurrence, context, source,
retained/recompute, Identity and final QuickSort image suites. The additional
R47 changes are test-only. Against `373582b`, `tests/derivation_io.c` is
+24/-11; implementation/header delta is zero, cumulative **+1830**.
No claim is made that persistence reduces that outstanding implementation
growth. Main remains unpublished.

### 2026-09-17: Share constructor exposure (R48)

- [x] Remove `constructor_structure`'s private origin/map/returned-value walk
  and its second pass that reindexes every field. The existing typed-head
  query now also stops at checked constructor values; context action and
  computed results use the same machine/dependency graph as Lambda/RETURN.
  Constructor fields are checked typed-input queries on that exposed value.
- [x] Preserve the distinction between exposing a construction for Match and
  selecting a current normalized field. `pg_prove_constructor_field` still
  requires the selected field to correspond to the current Core when a
  normalization receipt changes field terms. A constructor label alone is
  not evidence for its nominal classifier or its field typings.
- [x] Add a head-changing total-result/RETURN-to-constructor NF case. Its two
  normalized fields must match the existing typed variables, their contexts
  and classifiers, not the source computation operand. Verify reconstruction
  with the actual reduction certificate and stable repeated query/proof work.
- [x] Core, IADT, synthesis and imported QuickSort pass in debug. ASan/UBSan
  Core, IADT, Identity, synthesis and imported QuickSort pass.
- [x] Full debug acceptance passed, including 63/63 compatibility and final
  QuickSort source/image properties. Idle O0 QuickSort: 1.1343 seconds /
  279732 KiB / 131357 Solve transitions. This single sample is comparable to
  R46, not evidence of a speedup. Logs:
  `/tmp/a-program-typed-structure-r48-debug.log` and
  `/tmp/a-program-typed-structure-r48-san-{core,iadt,identity,synthesis,quicksort}.log`.

Against `cebdb2a`, `evidence.c` is **+26/-46, net -20**;
`tests/iadt.c` is +25/-0. Cumulative implementation/header remains **+1810**
against `4657cc6`, so the overall net-negative gate remains unmet. No new Core
tag, proof rule, format or accepted-conclusion authority is introduced.

### 2026-09-17: Remove construction-origin staging (R49)

- [x] Remove `construction_origin` and its temporary arena/list from
  `pg_prove_construction_origin`. Descending structural origin edges now
  composes the checked inner-to-outer context action directly. Selection and
  judgement boundaries still stop traversal; this API exposes provenance,
  not the current children of a changed normalized term.
- [x] Test three nested actions with a noncommuting swap/duplication followed
  by projection. Check the exact destination, binder image, dependent
  classifier and resulting Core; repeated recovery creates no extra typed
  subjects or proofs. The accumulation changes association, not application
  order, and does not overwrite explicit alternative derivations.
- [x] `function_graph.c:structural_computation_view` uses the shared checked
  input query, eliminating its independent raw-input-then-certification path.
  Actual child Core/context checks remain. Match/IH theorem reconstruction
  remains distinct from ordinary APP/unary input inspection.
- [x] Full debug acceptance passes, including 63/63 compatibility and final
  QuickSort source/image checks. ASan/UBSan Core, IADT, Identity, synthesis and
  imported QuickSort pass. Logs use `/tmp/a-program-typed-structure-r49-*`.
  Idle O0 QuickSort: 1.1026 seconds / 279828 KiB / 131357 Solve transitions;
  one sample, not a speedup claim. Counts: 180411 Core terms, 414838 typed
  subjects, 435317 proofs (unchanged from R48), 3197 typed queries (+131),
  3371 raw input queries (unchanged). The added checked-query requests replace
  the function-graph caller's private certification path, not new conclusions.

Against `f8432f3`, implementation/header: `evidence.c` +14/-31,
`function_graph.c` +4/-5: net **-18**. Tests: `core.c` +25/-0. Cumulative
implementation/header is **+1792** against `4657cc6`; the required overall
reduction is still outstanding. R2, R3 and R5 remain open; R4's representation
contract is unchanged. Main is not published by this checkpoint.

### 2026-09-17: Compose typed-body environments directly (R50)

- [x] Remove `construction_map` and the typed-body query's private map stack.
  Each retained action composes into one checked environment, inner-to-outer.
  Lambda pairing, RETURN exposure, substituted variables and Match selection
  consume that same environment. This removes staging, not context checking;
  restriction frames used for strengthening are a different operation.
- [x] Extend the noncommuting-map regression to checked RETURN and Lambda
  body queries, with chunk-one/chunk-64 advancement, exact Core/classifier/
  destination checks and no repeated completed work.
- [x] Correct a missing NULL-result check introduced in R49's function-graph
  adapter. A normalized handler reaches RETURN even when the general typed
  input query cannot expose its child. The added test reproduced a NULL read
  under ASan before the fix. After the fix, the existing checked RETURN
  inversion builds the relation and witness; executing the witness returns
  the original payload. Do not classify this entire program as unsupported
  merely because one structural query is unavailable.
- [x] Debug Core/IADT/Identity/synthesis and imported QuickSort pass. Final
  ASan/UBSan Core (including the regression), IADT, Identity, synthesis and
  imported QuickSort pass.
- [x] Full final debug acceptance passes, including 63/63 compatibility and
  the final QuickSort source/image checks. Log:
  `/tmp/a-program-typed-structure-r50-debug-final.log`.

Implementation/header against `40bd4a4`: `evidence.c` +19/-44,
`function_graph.c` +1/-0, net **-24**. `tests/core.c` +58/-0. Cumulative
implementation/header **+1768** against `4657cc6`; net-negative remains open.
QuickSort: 131247 Solve transitions (-110), 178434 Core terms (-1977),
414593 typed subjects (-245), 433515 proofs (-1802), 3197 typed queries and
3371 raw input queries (both unchanged). The final idle O0 sample after
acceptance is 1.0977 seconds / 277400 KiB. This is a single sample, not a
speedup claim. No Core tag, image format or logical rule changed.
R2/R3/R5 remain open; this is not the requested Main publication.

### 2026-09-17: Share substitution extension checking (R51)

- [x] Replace `substitution_pair`'s separate validation, map construction and
  acceptance path with the existing `substitution_build`. A lifted prefix
  projects its checked images into the destination; the common builder checks
  only the new suffix's dependent classifiers. Full construction, extension,
  pairing and lifting retain the same rule and supplied premise identities.
- [x] Check exact evidence reuse between dependent pairing and extension,
  reject an incorrect suffix length, and compare a lifted map with direct
  construction from the same checked images. Existing family-signature,
  wrong-sort/context/image and alternative-premise tests remain enabled.
- [x] Debug Core, IADT, Identity, synthesis and imported QuickSort pass.
  ASan/UBSan passes those same suites, including the final added assertions.
- [x] Full debug acceptance passes, including 63/63 compatibility and the
  final QuickSort source/image checks (`r51-debug.log`).

Against `975a8e6`: `evidence.c` +7/-41, net **-34**; `tests/core.c` +6/-0.
Cumulative implementation/header is **+1734** against `4657cc6`. QuickSort
retains R50's 131247 Solve transitions and all five recorded graph/query
counts. The idle O0 sample is 1.1104 seconds / 277372 KiB; one sample, not a
performance improvement claim. Logs: `/tmp/a-program-typed-structure-r51-*`.
The net-negative and remaining structural migration gates are not waived.

### 2026-09-17: Expose checked unary results through the shared query (R52)

- [x] Share RETURN/THUNK inversion between explicit kernel access and checked
  input queries. A completed input query can use the existing inversion rule
  on an accepted unary result even when the source reduction's construction
  cannot be exposed. It does not re-execute the source handler or make a
  descriptive Core node acceptable. Existing successful scoped/congruent
  queries retain their typed child and classifier formation.
- [x] Remove the inversion-to-query dependency from the shared rule checker;
  it consumes an optional already checked child. This avoids recursively
  querying the same normalized result. F/U formation selection remains
  distinct and no longer has unused term-judgement branches.
- [x] Strengthen R50's normalized-handler test: the common input query now
  returns the checked result instead of being unavailable. Check projection,
  normalized thunk contents, exact context/Core/classifier, rejection of a
  nonexistent second input, ordinary derivation reconstruction, completed
  query reuse and the generated graph witness's executed result.
- [x] Full debug acceptance passes, including 63/63 compatibility and final
  QuickSort source/image checks. ASan/UBSan Core, IADT, Identity, synthesis
  and imported QuickSort pass. Logs: `/tmp/a-program-typed-structure-r52-*`.

Against `cb5769e`: `evidence.c` +42/-20 (**+22**); `tests/core.c` +32/-4.
Cumulative implementation/header is **+1756** against `4657cc6`, so the
required overall reduction is still unmet. O0 QuickSort: 1.1203 seconds /
276832 KiB / 131247 Solve transitions; all five graph/query counts remain
R51's values. One timing sample does not establish a performance change.
No Core tag, logical rule or image format changes. This makes the existing
unary inversion available consistently; it does not solve arbitrary IH or
dependent result reconstruction. R2/R3/R5 and Main publication remain open.

### 2026-09-17: Inspect current checked heads before construction origins (R53)

- [x] Head queries first use an exposed checked Lambda, constructor, RETURN
  or THUNK. Reading a normalized head no longer requires exposing the source
  program which computed it. Constructor inputs still require their own
  checked structural queries; recognizing a head does not certify fields.
- [x] RETURN body queries retain direct typed inputs or depend on the shared
  checked input query. Outer context actions still apply to the resulting
  value. Remove the later duplicate head/RETURN cases instead of keeping two
  interpretations of the same constructor.
- [x] Extend the normalized-handler regression to the body query. Check the
  exact typed returned subject and completed shared dependency reuse. Existing
  mapped/scoped/NF constructor, Lambda and noncommuting-map tests pass.
- [x] Full debug acceptance passes, including 63/63 compatibility and final
  QuickSort source/image checks. ASan/UBSan Core, IADT, Identity, synthesis and
  imported QuickSort pass. Logs: `/tmp/a-program-typed-structure-r53-*`.

Against `7a97a45`: `evidence.c` +37/-26 (**+11**), `tests/core.c` +7/-0.
Cumulative implementation/header is **+1767** against `4657cc6`. O0 QuickSort:
1.1157 seconds / 276976 KiB / 131255 transitions; one timing sample, not a
speedup claim. Counts: 178428 Core terms (-6), 414609 typed subjects (+16),
433509 proofs (-6), 3211 checked queries (+14), 3385 raw queries (+14).
This removes unnecessary origin traversal for already exposed results, not
general recursive-IH construction or dependent classifier transport. No new
Core tag, logical rule or image format. R2/R3/R5, overall code reduction and
Main publication remain open.
