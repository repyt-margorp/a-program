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
| [ ] | R0 Contracts and baseline | Record current tests, per-file LOC, time/memory/work counters. Classify every occurrence producer and Evidence structural consumer. Specify child roles/scopes for each semantic owner, including type families and Identity. Pin conversion/normalization behavior. |
| [ ] | R1 Typed conclusions | Replace occurrence/conclusion duplication in `typing.h/.c`, `evidence.h/.c`. Migrate all accepted constructors, not only APP/Lambda. Preserve input-versus-acceptance distinction, exact interning and alternative derivations. No mutable classifier side table. |
| [ ] | R2 Context action | Migrate projection/reindex to the same typed structure with explicit effective child maps. Use existing substitution work and binder lifting. Verify repeated lookup sharing, capture avoidance, dependent classifiers and chunked execution. |
| [ ] | R3 Structural and formation consumers | Move `return_value_origin`, `pi_component`, constructor/inductive recovery and classifier recovery to checked views. Keep theorem-specific inversions where required. Replace structural wrapper walks in `function_graph.c`, `action.c` and `synthesis.c`; remove replaced paths in the same phase. |
| [ ] | R4 Images and pending work | Update occurrence/derivation/source transport and allocation dependency collection. Bump the affected format/policy explicitly. Reconstruct through ordinary Solve, not copied acceptance bits; document whether old images migrate or reject with a version diagnostic. |
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

### Structural producer contracts for the next slice

| Producer family | Current operands | Required interpretation |
|---|---|---|
| Universe, host leaf, variable, nominal family | None | Leaf; classifier/context remain part of the typed use |
| APP, family APP, RETURN, THUNK, FORCE, request/fold | Direct construction inputs | Ordered children in the enclosing scope; handler bodies retain their own binders |
| Lambda/family abstraction | Body in an extended context | The body edge must retain its lexical extension when a map is lifted |
| Pi | Checked value-domain formation, or declared family variable; checked codomain | Value domain is in the parent scope; family variable and codomain are in the extended scope. A logical signature is not an ordinary value type |
| Constructor | Field occurrences | Preserve nominal schema and parameter substitution separately from executable fields |
| Match/induction/type case | Scrutinee and branch occurrences | Preserve motive/indices, constructor branch telescope and recursive binder scopes |
| Identity, termination, family action | Typed construction inputs | Keep the selected family and dependency maps; no equality reflection |
| Type/value view, conversion, widening | Unchanged construction children | Different conclusion boundary, not resynthesized operands |
| Projection/reindex | Old children copied under a new parent | Replace with explicit scoped context action; not directly usable as current children |
| Normalization and content/Pi inversions | Historical source operands | These are provenance, not the target's immediate semantic children |

The last two rows prevent claiming that the current operand array is already
the target structural graph. Next migrate these producers and their consumers
together. Do not implement a new view by simply hiding their Evidence walks.
In particular, substitution of a variable must expose the image's typed
construction, not retain the variable's empty operand list. Under a Lambda,
child context action must account for the binder chosen by Core substitution;
changing every child to the destination context would capture lexical binders.

### Deletion ledger

Current state, updated after the typed Pi-selection migration. Earlier
checkpoint notes below are historical, not additional completion claims.

| Existing path | State / next action |
|---|---|
| Evidence classifier/context/sort copies | Removed for term conclusions |
| `return_value_origin`, `pg_prove_application_body` | Read typed construction/maps; still synchronous and not shared budgeted typed normalization |
| `pi_component` | Deleted; selection source/ordinal/argument drive `selected_formation` |
| `pi_argument_frames` | Retained checked binder substitution; no Pi-premise layout dependency |
| `inductive_recovery_step`, `rebase_image` | Remaining Evidence-history consumers; nominal formation and strengthening migration unfinished |
| `constructor_origin`, `pg_prove_elimination_body` | Read typed fields/motive/allocation; ordinary nominal acceptance still required |
| `classifier_leaf` | Deleted |
| `classifier_recovery_step` | Reads retained classifier/maps, with variable formation from its context; calls into synchronous certification remain |
| `function_graph.c:computation_origin` | Deleted in favor of shared checked construction access |
| `action.c:origin_step` | Reads typed origins/maps; does not reinterpret derivation wrappers |
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
