# Finite-Position Sorting

Date: 2026-09-27
Baseline: `ebe0648` (schema owner `fb15003`, documentation PR #42 merged).
Tracking: [#41](https://github.com/repyt-margorp/a-program/issues/41).
This is the implementation work list for the sorting priority in the
[authority plan](2026-09-25-POST-SURFACE-ISSUE-AND-AUTHORITY-SOAP-PLAN.md).
The [imported audit](2026-09-27-GENERIC-CONTAINER-SORTING-AUDIT.md) is research,
not evidence that its proposed interfaces already work. Broad P4/P5 remain open.

## Problem List

| ID | Problem | State |
| --- | --- | --- |
| F0 | Import and critically assess #41 / PR #42 | Complete |
| F1 | Checked finite positions and bijections | Complete |
| F2 | Lawful List and indexed-container views | Complete for homogeneous List/Vec and the finite-position bridge |
| F3 | One ordinary-result sorting specification | Prototype source/observations/rejection/resume verified; promotion pending |
| F4 | Quick/Merge/Insertion/Bubble and additional backends | Five prototype gates passed; Bubble requires explicit transitivity, legacy Merge is Nat-specific insertion-based merge |
| F5 | Permanent rejection, image and regression gates | Prototype runners consolidated and verified; accepted-build integration pending |

## F0. Scope and Invariants

### Subjective (User)

English paraphrase of 2026-09-27 conversation: prioritize the submitted Fin
sorting research, implement it in the surface language, and prove QuickSort,
MergeSort, InsertionSort and BubbleSort with a common method; check additional
straightforward algorithms. Do not change algorithms merely to obtain Local or
Strong sortedness. Those are separate post-hoc properties of the order relation.
The latest explicit invariant is that `a :: A` is only an assertion: synthesize
`a` independently, then check its classifier against `A`. Never use `A` to
invent an argument, motive or proof for `a`.

### Objective (Code)

Existing List proofs cover ordinary QuickSort results, Local/Strong sortedness
and an inductive permutation with nil/keep/swap/composition. The latter preserves
multiplicity, but is not itself an explicit bijection of labelled positions.
`src/program.c` does not expose the internal Identity library as generic source
Eq/refl/transport names. `src/synthesis.c:source_expect_step` and
`src/synthesis_conversion.c:expect_step` consume independently synthesized
inputs; Match discovery skips the assertion's right side.

PR #42 changes documentation only. Its merge automatically closed #41 despite
its explicit non-completion statement; #41 was reopened to track implementation.
The existing merge fixture uses repeated insertion, not linear two-front merge.

### Assessment

Agent decisions within the requested implementation scope:

- Use ordinary source IADTs, Pi, Match and explicit proofs. No Fin/Sort oracle,
  new keyword, Core tag, expected-type inference or privileged acceptance path.
- Fix permutation direction: **new position -> old position**. For input
  `values : Fin n -> A`, output at `i` is `values (forward i)`.
- Preserve labels even when their values coincide. A bijection has forward and
  backward functions and both pointwise inverse laws. Membership is insufficient.
- A source `same_position` relation over Fin can express these laws with
  ordinary indexed elimination. It is not a replacement for Higher Identity,
  equality reflection, proof irrelevance or a new universal Eq primitive.
- Fin's zero constructor needs an explicit bound: no field determines it.
  Successor can recover its bound from the recursive Fin field. A post-check
  must not fill the missing bound. Explicit Match motives belong to the term.
- Keep semantic views distinct from native random-access/cost capabilities.
  Do not assume arbitrary IADTs, heterogeneous positions, infinite containers,
  stability or asymptotic complexity are covered.

### Plan

- [x] Merge #42; preserve research provenance and reopen unfinished #41.
- [x] Check current contracts and the four primary references below.
- [ ] Reassess this plan only from concrete source/checker evidence; record any
  necessary compiler fix separately, without weakening a theorem or post-check.

## F1. Finite Positions and Bijections

### Subjective (User)

Finite sorting must be expressible and checkable in A Program itself.

### Objective (Code)

At `ebe0648` plus this source-library patch, `finite_positions.p` checks open
`n` proofs of identity/inverse/composition, fixed-head lifting and a self-inverse
first-two-position swap. General roundtrip theorems mention actual projections
of composed/lifted permutations; closed three-position cycles are also checked.
Neither kernel nor synthesis implementation changes. Focused checks pass in
optimized (4.186 s), Debug (5.142 s), and ASan/UBSan (21.783 s) builds, with leak
checks and halt-on-error enabled. These include ordinary/retained images and
negative controls; they are test wall times, not speed comparisons. Full
`check-acceptance` passes in **1,515.435 seconds** wall (1,408.055 user,
106.560 system), including 63/63 compatibility, generic/ordinary-result sorting,
Local/Strong, all LT variants, Fin and optional witness packets. The earlier
user-interruption run was terminated and is not counted as a pass. Sanitizer
build/check work overlapped this gate; its wall time is not a speedup claim.

### Assessment

Define the small position algebra first, so later sorting proofs can reuse it
without reconstructing bounds or introducing a second proof authority.

### Plan

- [x] Add `tests/fixtures/finite_positions.p`: Fin, observation, weakening,
  same-position elimination, bijection identity/inverse/composition and action.
- [x] Check these laws for open `n`, not just small closed enumerations.
- [x] Add fixed-head lifting, a nontrivial swap/cycle and a duplicate-value/label example.
- [x] Reject wrong bounds, missing bounds and non-bijections. A motive-free
  relation eliminator synthesizes, but adding the desired symmetry post-check
  rejects it; the assertion cannot replace the inferred motive. The explicitly
  annotated symmetry theorem passes.

Source-trial decision: a function projection already returns a suspended
function value. Adding `&` to that projection wrapped the returning computation
again and failed its argument contract. Use its ordinary result when passing it
to `position_keep_map`; no coercion rule or compiler relaxation was added.

## F2.1. Application Conversion Before View Proofs

### Subjective (User)

Preserve independent synthesis and the post-check-only meaning of `::`. Reuse
the existing typed structure and checking mechanisms rather than accumulating
special cases or extra authorities to make examples pass.

### Objective (Code)

The initial standalone diagnostic was
`src/prototype/family_domain_conversion.p`. Its expanded permanent successor is
`tests/acceptance/family-domain-conversion.p` (verified publication candidate).
At `ebe0648`, an ordinary application accepts the synthesized result of
`shift (Nat.succ k) b`, whose classifier is `Box (pred (Nat.succ k))`, at domain
`Box k`. The control without the final definition passes in 1,928 steps.
Applying the indexed `Family k` to the same result is rejected in 2,424 steps.
This also fails without any `::` (2,305 steps); the assertion is not its cause.

`src/synthesis.c:prepare_application` sends logical-family arguments directly
to `PG_TYPE_FAMILY_APP`. `src/evidence.c:family_application` correctly requires
alpha-equal domain/classifier inputs. Ordinary application first supplies checked
conversion through `pg_synthesis_expect`. The family path omits that step.
A debugger confirms both family/application premises are accepted before the
family rule rejects them. Do not weaken that kernel check into unchecked equality.

The repaired application now supports the open-input theorem in
`tests/fixtures/finite_vectors.p` (initially in the acceptance file): every valid position of an arbitrary
Vec is related to the result of ordinary `vec_lookup`. Additional open-input
proofs establish tabulation's entries, uniqueness through arbitrary supplied
value predicates, and both directions of pointwise reconstruction. Independent
synthesis and image checks are permanent tests, not just closed examples.

### Assessment

This is an application-elaboration completeness gap, not permission to infer an
argument from an expected type. The actual argument and family's domain are
already known. Their definitional equality must have an explicit checked
conversion, just as for ordinary application.

The domain must come from the selected typed family declaration, including its
scope/substitutions. A raw signature is not a substitute for that formation.
`pg_synthesis_application_domain` currently expects computation-Pi formation;
blindly calling it on a logical family is not a completed solution. Share the
typed declaration/domain inspection, without adding a value-side Pi tag or
reconstructing a second typing authority. Do not require user wrappers or eagerly
normalize every entire family telescope as a workaround.

### Plan

- [x] Preserve a minimal source diagnostic; distinguish it from an accepted test.
- [x] Expose the next domain from the existing typed family declaration and
  scope action; retain the distinction between value and family parameters.
- [x] Reuse checked conversion after independent argument synthesis, before
  ordinary `PG_TYPE_FAMILY_APP` admission. Keep `::` outside this path.
- [x] Verify both directions of reducible classifiers, partial/dependent family
  applications, family-valued parameters, and wrong bound/nominal/effect rejection.
- [x] Recheck the Vec coverage proof; diagnose any remaining index-recovery or
  motive failures separately instead of claiming this fix solves them all.
- [x] Move the repaired diagnostic into permanent source/image/budget tests;
  include the general Vec lookup theorem and wrong position/bound controls.
- [x] Run the complete acceptance gate before publishing this compiler epoch.

Verified patch, 2026-09-27, baseline `bc82fc9`: the family parameter query borrows
checked abstraction/variable/schema scopes, substitutes already supplied
arguments and uses the existing checked scope lift. It constructs no alternative
Pi former or acceptance rule. The caller independently synthesizes its argument
before ordinary conversion. Family-valued arguments keep their existing logical
signature contract; this patch does not add general family-signature conversion.
The expanded source, ordinary images and budgeted resume pass focused checks.

Those checks also found an older retained-image defect: the standalone
`family-index-domain.p` saves successfully on the baseline (1,179 steps) but
reload rejects (1,394). Reconstructed index declarations are alpha-equal to the
saved ones, not pointer-identical. The working repair applies existing checked
Context alpha correspondence to Self/indices and to both endpoints of constructor
result maps. It neither mutates saved declarations nor accepts their unchecked
types. The old retained image also loads with the repaired checker (1,482 steps).
Focused checks, core and IADT tests pass in strict O2, Debug and ASan/UBSan
builds, with leak checking and halt-on-error. The final complete
`make -s -f src/Makefile BUILD=/tmp/a-program-family-conversion-build check-acceptance`
passes: **1,434.749 s** wall, **1,325.008 s** user, **109.238 s** system.
It includes 63/63 source compatibility, ordinary-result sorting, Local/Strong,
derived-LT variants, Fin/Vec laws, images and optional witness packets. These
results apply to the clean publication candidate, not inherited root-worktree
experiments. Log: `/tmp/a-program-family-conversion-acceptance-final.log`.

A further pre-publication check found that looking up a family variable's
construction by occurrence discarded its selected Universe bound. The request
now consumes the supplied formation and transports its selected scope through
projection/reindex; unit tests distinguish both bounds, directly and after both
maps. This uses the existing scope/substitution authority, not a second classifier
store. The first full-gate run was deliberately stopped for this correction and
is not counted as a pass; the final code passes the fresh complete run above.

Correction to the earlier Vec diagnosis: the final patch rejects the original
trial earlier than the previously observed constructor failure. The trial's
`tail_property` mixed a value-type result with a raw computation-Pi type. Making
both branches return value types, and explicitly annotating their dependent
Match motive, checks the theorem without another compiler change. The unannotated
helper remains unsupported; `::` must not invent its motive. A standalone
constructor-lifting control passes. The permanent theorem bundle checks in
236,706 steps with the full Fin provider; removing all its post-checks still
passes (180,264).
Ordinary/retained images and 100-step resume pass, and the wrong bound and wrong
position reject. No general constructor-index recovery defect is established by
this trial, and no temporary `at_type` workaround remains.

The vector laws transport an explicitly supplied `P : A -> @` between values
at the same position and between original/reconstructed lookup results. This
does not add object Identity syntax, equality reflection, proof irrelevance or
an automatic expected-type proof. It specifies the view's pointwise observation;
a whole-container Higher Identity witness and the List/permutation bridge are
not claimed. `vec_tabulate : (n:Nat) -> (Fin n -> A) -> Vec A n` preserves the
index by its independently checked result type.

Paired O2 measurements, six alternating samples after warmup, same inputs and
flags: length 8.981 -> 8.634 ms, family parameters 7.108 -> 7.221 ms, Fin
35.507 -> 36.177 ms, general Sorted 1.107 -> 1.100 s, ordinary-result theorem
0.706 -> 0.698 s (medians; overlapping sample ranges). The general Sorted work
count is 1,247,803 -> 1,247,827. The initial unconditional-domain-query trial
increased that case's median by about 12%; it was not adopted. Already alpha-equal
classifiers need no conversion formation; ordinary admission still verifies the
operands. Logical-family arguments retain their own existing signature check.
These are checker measurements, not sorting runtime improvements.
Evidence: `/tmp/a-program-family-conversion-bench-final.jsonl` and the
`family-conversion-{focused-vector,debug-focused,asan-focused}` logs. This repair
adds missing application behavior; it is not a P4/P5 code-reduction milestone.

### F2.1 Change Accounting

Against `bc82fc9`, excluding inherited root-worktree experiments. This is a
correctness/library checkpoint, not a net compiler-size reduction. Accepted
implementation C/headers add 249 and remove 3 lines (net +246); tests add 328.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/evidence_function.c` | 197 | 0 | +197 |
| `src/evidence.h` | 5 | 0 | +5 |
| `src/synthesis.c` | 9 | 0 | +9 |
| `src/synthesis_function.c` | 23 | 0 | +23 |
| `src/synthesis_schema.c` | 13 | 3 | +10 |
| `src/synthesis_source.h` | 2 | 0 | +2 |
| `tests/core.c` | 31 | 0 | +31 |
| `tests/iadt.c` | 41 | 0 | +41 |
| `tests/acceptance/family-domain-conversion.p` | 28 | 0 | +28 |
| `tests/acceptance/family-index-domain.p` | 7 | 0 | +7 |
| `tests/acceptance/finite-vector-lookup.p` | 127 | 0 | +127 |
| `tests/family_domain_conversion.sh` | 94 | 0 | +94 |
| `src/Makefile` | 5 | 0 | +5 |
| Superseded prototype diagnostic | 0 | 10 | -10 |

All non-document changes total **+582/-13, net +569**. The family query reuses
checked declarations and scope actions; no new term former, proof rule,
serialized job kind or alternative acceptance engine is introduced.
This plan adds 117 and removes 15 lines (net +102); the complete checkpoint
therefore totals **+699/-28, net +671**.

## F2-F4. Views and Common Sorting Proofs

### Subjective (User)

Reuse one proof specification across the requested sorting algorithms; do not
silently replace their ordinary results with separately generated proof results.

### Objective (Code)

Existing proof providers are under `tests/fixtures/` and source theorems under
`tests/acceptance/`. `generic_sorted/content-result-proof.p` connects the
permutation theorem to ordinary QuickSort. The F2 checkpoints below now add the
List/Vec reconstruction laws and the general permutation-to-Fin bridge. Connecting
these into the common sorting interface remains F3/F4, not a completed claim.

### Assessment

The difficult obligation is the bridge, not creating a new record name. List
permutation must yield a length-preserving occurrence correspondence; enumeration
and lookup then establish a Fin bijection and its value/action equation.
List and an indexed homogeneous vector must share this theorem, with explicit
dependent transports where needed. Positivity alone is not a traversal law.
Keep Local plus permutation as a base contract; add transitivity to derive
Strong. Comparator directional correctness is still required, but antisymmetry
and stability are not universal requirements.

### Plan

- [x] F2: Vec contents, indexed lookup/tabulation, general ordinary-lookup
  coverage, predicate-based uniqueness and bidirectional pointwise reconstruction;
  independently checked shape/index preservation, rejected bound/position and images.
- [x] F2: connect these Vec laws to a lawful List view and its same-length
  reconstruction. Pointwise Vec laws alone do not establish the List bridge.
- [x] F2: connect existing inductive List permutation to finite-position
  bijections and prove the action equation on actual values, including duplicates.
- [x] F3 prototype: package an ordinary function with its general Local and permutation
  proofs; derive Strong separately. Prove action identity/composition pointwise.
- [x] F3 prototype: check both sort-to-List and same-shape wrappers against their laws;
  document selected traversal rather than claim arbitrary-container support.
- [x] F4 prototype: instantiate QuickSort, InsertionSort, BubbleSort and the existing
  merge fixture. No conventional two-front MergeSort or its complexity is claimed;
  the existing algorithm is unchanged.
- [x] F4 prototype: connect the existing TreeSort, recording relation assumptions;
  generic source proofs and List/Vec/Fin observations pass. Promotion is separate.
- [x] F4 prototype: verify open-input ordinary-result theorems, then closed examples for
  empty/singleton/reversed/already-sorted/duplicate-labelled data.

### F3/F4 Implementation Checkpoint, 2026-09-28

- **Subjective (User):** continue F3/F4 implementation and verification; `::`
  remains a post-synthesis assertion, not an inference input.
- **Objective (Code):** baseline `7160cbe` already proves general Local and
  permutation properties of ordinary QuickSort. The older insertion/tree/merge
  acceptance proofs are primarily Nat-specific. The existing merge uses repeated
  insertion, not a conventional two-front merge.
- **Assessment:** use a source record containing the ordinary function and its
  Local/permutation theorems. Derive Strong with transitivity and reuse F2 for
  position correspondence and List/Vec wrappers. Do not alter an algorithm to
  fit the interface. New work starts under `src/prototype/finite_sorting/`.
- **Plan:** verify the common record and laws first, then connect QuickSort and
  generalize the other backends individually. Check synthesis without `::`,
  incorrect result/comparator/proof rejection, ordinary outputs and saved/resumed
  images. Keep unverified backends unchecked; no new Core or proof-rule tags.

Verified prototype work, relative to `7160cbe` (not a promotion into accepted
tests or a claim that F3/F4 are complete):

- [x] `common.p`: ordinary function plus Local/permutation proof fields;
  Strong via transitivity; Fin action identity/composition; List/Vec wrappers,
  reconstruction and actual-value correspondence against the original Vec.
- [x] `quick.p`: connect the existing ordinary QuickSort theorems.
- [x] `insertion.p`: prove Local and permutation for the unchanged
  `insertBy`/`insertionSortBy`. Local needs directional comparator evidence,
  not transitivity. `chain_head` is a source IADT, not a privileged rule.
- [x] Independently synthesize after removing all `::` assertions.
- [x] Ordinary Quick/Insertion List results: empty, singleton, reversed,
  ordered and distinct labels sharing a key; image comparison chunks 1 and 64.
- [x] Reject wrong function domain, dropped contents, incompatible comparator,
  wrong Vec shape and wrong action value; ordinary save/load and partial resume.
- [x] Complete normalization of the integrated Insertion Vec/Fin report.
- [x] Complete the corresponding Quick report and specialized value-transport
  tests on the isolated closed-readback candidate (not yet the accepted compiler).
- [x] Verify retained Insertion observations/resume through the existing reader
  API with an explicit three-million-record limit; the CLI limit is unchanged.
- [x] Reject the same-domain wrong-function example on the conversion-head
  candidate, including saved and ordinary/retained partial-image resume.
- [x] Resolve the prototype CLI size policy and complete the retained gates:
  explicit `--image-limit 3000000`; unchanged default and proof admission.
- [x] Connect the unchanged Nat-specific legacy MergeSort, with arbitrary
  relation/comparator, Local/permutation proofs and ordinary List observations.
- [x] Complete legacy Merge Vec/Fin observations on the closed-readback candidate.
- [x] Connect TreeSort with generic ordinary-result Local/permutation proofs;
  labelled observations, cyclic relation and bounded saved/resumed images pass.
- [x] Implement right-to-left adjacent-pass BubbleSort, prove content preservation
  for arbitrary comparators, and connect a backend with explicit transitivity.
- [x] Audit Bubble's nontransitive Local claim: the source counterexample proves
  directional decisions alone insufficient. Keep its transitive backend
  explicit; a weakest sufficient comparator law is not established.
- [x] Verify the five prototype backend gates and state each backend's scope.
  Promotion is separate; no claim of conventional two-front merge or
  arbitrary-element legacy MergeSort.
- [ ] Obtain approval before promoting the verified evaluator prototype into `src/`.

**Initial failures (7160cbe; superseded by the candidate checkpoints below):**
general/closed source checks finish in about
13 million solver transitions, and removing assertions takes about 4.7 million.
This does not establish a sorting complexity bound. The initial test consumer
unnecessarily evaluated and discarded three proof terms before returning a
List; it now checks these functions separately. A direct `sorting_run` applies
the stored function inside its clause, avoiding a returned-function adapter.
Neither adjustment solves the remaining normalization cost. The Quick report
still exhausts 100 million transitions, including with chunks 1 and 64; the
Insertion report exhausts 20 million. These are **uncompleted checks**, not
counterexamples or passes. Concrete predicate specialization and a same-domain
wrong-function test are preserved as `stress-value.p` and
`stress-wrong-function.p`; the latter is not yet a verified rejection.

The fully retained image of the initial checked prototype contains 439,615
objects, 1,621,025 terms and 20,981 graph roots. The reader rejects this graph at
its configured 1,000,000-record limit (`graph_io.c`), before graph reconstruction.
This establishes a size-limit failure, not corruption. Normal RECOMPUTE images
load and resume. Do not silently raise every limit or omit proof obligations to
declare the retained mode working.

**Next verification/publication work (current):**

Candidate verification is complete: five-backend aggregate, full compiler
regression, and focused ASan/UBSan gates pass. Remaining work:

1. Obtain explicit promotion approval. Move the verified compiler changes and
   source library/tests into their accepted owners without retaining duplicate
   implementations. Keep diagnostics separate; do not change the wire format,
   interning rules or proof admission. Keep inherited user experiments untouched.
2. Add the five-backend/reader-bound tests to the accepted `check-acceptance`,
   verify the promoted paths and update #41 with the exact scope before closing
   it. Do not claim a prototype-only result is the default compiler behavior.
3. For F4, retain Bubble's explicit transitivity requirement and its checked
   counterexample to the weaker contract. Do not pursue the disproved theorem
   or change the algorithm to make it fit. A weaker sufficient comparator law
   is a separate question, not a completed result.

The commands, scope and known incomplete gates are in
[`src/prototype/finite_sorting/README.md`](../src/prototype/finite_sorting/README.md).
No accepted compiler, algorithm, syntax, build rule or Core authority is changed.

Final checkpoint observations: source plus post-checks completes in **13,043,801**
steps; without assertions **4,724,416**. Aggregated ordinary List observations
pass for Quick in **15,170,785** and insertion in **13,253,617** transitions for
both comparison chunks (combined `lists` gate: **25 seconds**, O2). The `source`
gate passes its five rejection checks and ordinary complete/partial-image
checks, then fails the retained-image load limit. `all` remains unpassed. The
compiler's full acceptance suite and sanitizer suite were not rerun: there are
no accepted implementation changes in this prototype checkpoint.

### F3/F4 Verification Follow-Up, 2026-09-28

**Subjective (User):** advance F3/F4 verification; keep `::` strictly a post-check
and preserve ordinary algorithm results and one implementation authority.

**Objective (Code), `996f523` plus this prototype:**

- Insertion's complete Vec/Fin report passes in **32,348,238** transitions at
  chunks 1 and 64, including source checking (O2 gate: **23 seconds**). Its Core
  NF alone takes 19,298,786 transitions; after that cached result, typed NF takes
  one solver transition. The previous 20-million total budget was insufficient.
- For `quick_first`, a five-million-Core-step probe samples 77,684 states in the
  demand/readback phase and 441 elsewhere, creating 1,672,938 further terms.
  Separate 60-million-step Quick position and Vec probes also remain pending;
  they are not demonstrated semantic counterexamples or solver deadlocks.
- Complete retained Insertion results pass at 32,483,133 transitions with a
  caller-selected **3,000,000-record** bound, using the existing typed comparator.
  The retained 100-step image, byte-identical zero-step rewrite and resumed
  Insertion result also pass (combined O2 gate: **46 seconds**). This establishes
  the bounded reader API, **not** a fix to the CLI's one-million-record default.
- `merge.p` proves Local and permutation of the exact legacy `mergeSort` call.
  Local requires directional comparison evidence; Strong adds transitivity via
  the common wrapper. Internal `merge_fits` is an ordinary source IADT bounding
  the list length by fuel, derived from the existing `measure_content_result`.
  Content preservation also holds at zero fuel, where sortedness need not hold.
- Merge source/post-checks pass in **13,723,977** steps, assertion-free synthesis
  in **5,240,327**, and the five ordinary List observations in **14,293,931**.
  Positive labels 1 and 2 share a Boolean key, so the duplicate example checks
  distinct occurrences without imposing stability. Wrong bounds, comparator
  evidence and content reject; ordinary images and 0/100-step resume pass.
  O2, Debug and ASan/UBSan gates pass in **70/128/219 seconds**, with leak checking
  and halt-on-error enabled for sanitizers. Merge's integrated Vec observation
  is still pending at 40 million steps; the following Fin-origin gate was not
  reached. The full compiler acceptance suite is not claimed rerun for these
  prototype-only source/test additions.

**Assessment (agent):** retain the checked open theorems; do not declare the
remaining closed observations complete. The first Merge Local proof trial
reassociated nested neutral Matches and did not finish at 40 million steps.
The adopted helper takes the original whole split result and explicitly proves
its right-list fuel bound. It matches the ordinary function's computation,
without commuting-conversion axioms, altered algorithms or inference from `::`.
`eval.c` eagerly materializes demanded closures; repeated substitution and binder
freshening are candidates for investigation, not an established safe repair.
`probe.c` measures the existing evaluator and is not an acceptance checker.
`image_compare.c` reuses `tests/program.c` with only an explicit reader bound;
it neither trusts serialized proofs nor introduces an alternative checker.

**Plan:** retain the unchecked work above as the sole active list. The prototype
default budget is now 40 million, justified by the completed Insertion check;
do not keep raising it to conceal the unresolved Quick/Merge readback cost.
Tree/Bubble and the full `all` gate remain incomplete. No accepted compiler,
Core/surface rule, image format or default build changes in this epoch.
Logs: `/tmp/a-program-f3-{insertion-shared-provider,shared-provider-lists,
retained-api}.log`, `/tmp/a-program-f4-merge-{check,debug,asan,vector}.log`.

Change accounting against `996f523`, excluding documentation and inherited
root-worktree edits. Accepted compiler/build changes: **0**. All entries are
under `src/prototype/finite_sorting/`:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `merge.p` | 158 | 0 | +158 |
| `merge-cases.p` | 51 | 0 | +51 |
| `merge-check.sh` | 54 | 0 | +54 |
| `check.sh` | 2 | 9 | -7 |
| `provider.sh` | 10 | 0 | +10 |
| `retained-check.sh` | 28 | 0 | +28 |
| `image_compare.c` | 29 | 0 | +29 |
| `probe.c` | 108 | 0 | +108 |
| `probe.mk` | 11 | 0 | +11 |

Non-document total: **+451/-9, net +442**. Of this, 158 lines are new source
proofs; the rest are examples, verification, diagnostic and local build code.
This is added F3/F4 coverage, not a compiler-size reduction claim.

### Closed Readback Verification, 2026-09-28

**Subjective (User):** continue F3/F4 implementation verification. Keep Core
pointer interning structural, `::` post-synthesis-only, and experimental changes
under `src/prototype/` until promotion is approved.

**Objective (Code):** candidate `aaba32b` plus
[`readback_support`](../src/prototype/readback_support/README.md). The overlay
build leaves accepted source untouched. Immutable free-binder sets are derived
only on an exact Term intern miss. Materialization can reuse closed syntax;
normalization and evidence admission still use the existing executors.

| Check | Accepted baseline | Materialization-only prototype |
| --- | ---: | ---: |
| F3 source with assertions | 13,043,801 | 5,811,051 |
| F3 without assertions | 4,724,416 | 3,001,220 |
| Quick full Vec/Fin report | pending at 100M | 6,608,719 |
| Insertion full Vec/Fin report | 32,348,238 | 5,990,934 |
| Merge Vec observation | pending at 40M | 6,562,890 |
| Merge Fin origins | not reached | 6,595,767 |
| Concrete value transport | pending at 100M | 6,258,358 |
| Same-domain wrong function | pending at 20M | pending at 40M |

Counts include synthesis. Completed report comparisons pass with chunks 1/64.
Merge's complete `views` gate passes, including negative checks, assertion-free
synthesis, ordinary images and zero/100-step resume. F3 `all` reaches and passes
ordinary image checks, then fails the unchanged retained-image CLI limit. A
pending negative is not rejection. These are prototype results, not Main fixes.

**Assessment:** reject the first all-readback shortcut: closed syntax reuse
breaks the existing Context reindex contract when an inner binder collides with
the destination Context. Keeping the input closure unchanged is also necessary
for exact substitution-request lookup. The revised trial applies reuse only to
evaluation materialization; substitution preserves binder freshening. Both use
one executor, with an owner-supplied contract rather than a new saved flag or
proof rule. Core and evaluator I/O tests pass; a mixed closed/open DAG passes
inert save/resume at all seven candidate step boundaries (baseline: fifty).
Support-set storage and construction overhead remain a tradeoff to measure.

The follow-up construction benchmark rejected flat support arrays: a
6,000-binder spine uses 235,520 KiB RSS versus 3,004 KiB at baseline. The revised
pointer-key trie shares off-path nodes and uses 5,640 KiB on that same input.
It adds no Term equality or semantic authority. Acceptance/sanitizer results
from the earlier array build do not certify this representation change; rerun
the final candidate. Detailed measurements and tests are in its README.

**Plan:** the F3/F4 checklist above remains authoritative and incomplete.
Complete the general regression/sanitizer gates, explicitly bounded retained
Quick/Insertion checks, and overhead comparison before proposing promotion.
The same-domain negative and Tree/Bubble remain open. Detailed rejected trials,
commands and prototype-only checks are in the linked README. The first broad
gate stopped because the generated overlay lacked `training/` and `print.p`;
those fixture links were repaired, not the parser or its expected results.

Focused final-trie checks pass in O2 and ASan/UBSan (leak checking and
halt-on-error): Core, evaluator I/O, support sets, every-step readback resume,
Quick/Merge views and concrete value transport. Wrong Quick/Insertion labels
reject in 6,146,761/5,922,676 steps; removing assertions from the positive value
test completes in 3,448,529. Bounded retained reports and partial-image resume
pass for both Quick and Insertion. Cross-build checks also pass: a baseline
retained image read by the candidate yields the Quick report; a candidate
retained image read by baseline yields the ordinary Quick List report.

A separate three-run O2 source-only comparison has median wall times
**5.122 s baseline / 4.592 s candidate**, with identical input and budget on this
machine. Other regression work was running, so this is not an isolated benchmark
or a general speed claim; the step-count decrease is much larger than the time
decrease. The Example 09 count stays 2,824 in both builds.

The final trie candidate's **complete `check-acceptance` exits 0**, including
source compatibility **63/63**. The separate `check-support` and focused
ASan/UBSan gates also pass. The broad run takes approximately **24m46s**, including
build and concurrent verification work; this is not an isolated speed benchmark.
No accepted compiler changes or promotion are included. Logs are
`/tmp/a-program-support-acceptance-trie.log` and the `quick`, `merge`, `value-gate`,
`retained`, `cross-old-new`, `cross-new-old` logs with the same prefix.

Change accounting against `aaba32b`, excluding Markdown and inherited root
worktree edits. Paths below are relative to `src/prototype/`:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `finite_sorting/retained-check.sh` | 13 | 5 | +8 |
| `finite_sorting/stress-value.p` | 1 | 0 | +1 |
| `finite_sorting/value-check.sh` | 33 | 0 | +33 |
| `readback_support/.gitattributes` | 2 | 0 | +2 |
| `readback_support/build.mk` | 21 | 0 | +21 |
| `readback_support/construction_bench.c` | 42 | 0 | +42 |
| `readback_support/eval.c.patch` | 54 | 0 | +54 |
| `readback_support/graph.c.patch` | 14 | 0 | +14 |
| `readback_support/graph.h.patch` | 19 | 0 | +19 |
| `readback_support/overlay.sh` | 24 | 0 | +24 |
| `readback_support/resume_test.c` | 71 | 0 | +71 |
| `readback_support/support.c` | 115 | 0 | +115 |
| `readback_support/support.h` | 9 | 0 | +9 |
| `readback_support/support_test.c` | 116 | 0 | +116 |

Repository non-document delta: **+534/-5, net +529**, primarily verification and
isolated-build material. Accepted compiler delta: **0**. If promoted as written,
the actual compiler patch plus support module is **+144/-10, net +134**;
the 87 stored patch-file lines are not 87 new compiler lines. This epoch makes
previously pending observations tractable; it is not a code-reduction claim.

### Generic TreeSort Verification, 2026-09-28

**Subjective (User):** continue F3/F4 verification. Preserve the existing sorting
algorithms and separate Local from Strong; `::` remains only a post-check.

**Objective (Code):** baseline `11efe1d` plus
`src/prototype/finite_sorting/tree{,-cases,-cycle}.p` and `tree-check.sh`.
The existing generic `treeInsert`, `treeBuild`, `treeToList`, and `treeSort` are
unchanged. Source proofs pass on the accepted compiler in **13,294,981** steps,
or **4,965,391** after removing assertions. On the O2 closed-readback candidate:

| Check | Steps |
| --- | ---: |
| Source with labelled cases/value law | 6,460,917 |
| Same source without assertions | 3,479,842 |
| List/Vec/Fin report, chunks 1/64 | 6,774,594 |
| Retained report, explicit 3M reader bound | 6,926,359 |
| 100-step image resumed, ordinary/retained | 6,774,926 |
| Cyclic-relation Local proof readback | 6,150,878 |

The complete prototype gate passes in **170 seconds O2 / 464 seconds ASan/UBSan**,
with leak checking and halt-on-error enabled. It includes wrong comparator,
content and value rejection and byte-identical zero-step image rewriting.
The cyclic relation has `a <= b <= c <= a`; TreeSort of `[a,b,c]` yields
`[b,c,a]`. Its Local certificate computes that result; using it as Strong is
rejected. This test checks absence of an automatic Local-to-Strong conversion,
not a general uninhabitance theorem. The first sanitizer attempt lacked the
bounded comparator executable; the complete rerun passes after building it.

**Assessment:** `tree_all A P` unifies lower/upper bound preservation. Reuse
existing List concatenation and permutation lemmas for traversal, rather than
copy the older Nat-specific graph proofs or add privileged rules. Local needs
directional comparison evidence only; Strong requires transitivity through the
common wrapper. Only an initially missing pair of parentheses around nested
surface Matches needed correction; no compiler/type-inference repair was used.

**Plan:** Tree completes the additional-backend prototype item, not all of F4.
Bubble, the CLI retained-image policy, same-domain negative and promotion
approval remain open.
No accepted compiler changes; a broad compiler regression rerun is not claimed
for this source-only addition. Logs: `/tmp/a-program-tree-{check,sanitize,
baseline-source,baseline-independent}.log`.

Per-file non-Markdown delta against `11efe1d`: `tree.p` **+110/-0**,
`tree-cases.p` **+21/-0**, `tree-cycle.p` **+9/-0**, `tree-check.sh` **+60/-0**;
total **+200/-0**. These are source proofs and verification, not compiler growth.

### BubbleSort Checkpoint, 2026-09-28

**Subjective (User):** implement the named algorithms in the surface language,
retain one common sorting contract, and distinguish the assumptions for Local
and Strong. No expected-type inference from `::`.

**Objective (Code):** baseline `87bb490` plus `finite_sorting/bubble*.p` and its
verification scripts. The algorithm performs a right-to-left adjacent pass,
fixes its head and recursively sorts the suffix. `SizedList` carries the strictly
decreasing length; there is no insufficient-fuel fallback. `bubble.p` proves
general permutation independently of comparator laws. `bubble-order.p` proves
Strong using directional decisions and transitivity, then supplies Local to
`bubble_transitive_backend`. The same ordinary function is used in both cases.
The accepted compiler checks these open proofs in **13,292,264** steps, or
**4,925,589** without assertions. Candidate source with labelled cases passes
in **6,485,127**, without assertions in **3,457,271**; its List/Vec/Fin report
passes in **6,844,141** (chunks 1/64). The complete gate, including the
counterexample below, passes in **204 s** under O2 and **553 s** under ASan/UBSan
with leak checking and halt-on-error. The shared Tree runner passes in **175 s**.
Ordinary/retained images, byte-identical zero-step rewriting and 100-step resume
pass through the explicitly bounded reader API. Logs:
`/tmp/a-program-bubble-complete{,-sanitize}.log` and
`/tmp/a-program-tree-shared-runner.log`.

`bubble-directional-counterexample.p` adds a stronger boundary result. Its
relation has reflexive edges and `ab, bc, cb, ca`, but no `ac`. Its comparator is:

| `le` | a | b | c |
| --- | --- | --- | --- |
| a | true | true | false |
| b | false | true | false |
| c | true | false | true |

All nine directional decisions are proved. On `[a,c,b]`, the first pass produces
`[a,b,c]`, then sorting `[b,c]` produces `[c,b]`: the full result is `[a,c,b]`.
This is not Local. The source defines an empty ADT and actually constructs:

```text
bubble_counter_refute :
  general_locally_sorted bubble_point bubble_relation bubble_counter_result
  -> bubble_void
```

An indexed eliminator sends every legitimate relation edge to its allowed
type; the first Local edge at `a,c` would inhabit the empty type. This is an
internal refutation, not just a failed witness search or a rejected assertion.
Accepted baseline checks it in **13,159,863** steps, or **4,803,716** without
assertions; candidate checks it in **5,901,060**, or **3,070,986** without
assertions. Saved-image comparisons
confirm the actual result in **5,902,960** steps for chunks 1/64.

**Assessment:** directional `no` supplies `R y x`, not `not (R x y)`. Hence the
two false answers for `(b,c)` and `(c,b)` satisfy the existing contract. This
disproves the proposed unconditional Local theorem for this unchanged Bubble
algorithm. Explicit transitivity is sufficient; necessity is **not** proved.
For the separate cycle `a <= b <= c <= a`, `[a,b,c]` remains a valid closed Local
example, so failure of the minimum invariant alone would not justify this
conclusion. A bounded tournament search was inadequate: it excluded the
bidirectional case responsible for the counterexample. The common backend
continues to require the actual Local proof and is not weakened.
Reuse `predecessor`, permutation composition and the F2 views. A generic
predicate-transport lemma over permutation avoids another pass-preservation
implementation. A first helper motive incorrectly applied a nonempty-only
function across every length; the corrected helper handles zero explicitly and
places dependent continuation arguments after Match. This is source proof
construction, not a checker relaxation. Tree/Bubble now share their exact
CLI-status/image comparison helpers; no new verifier or kernel rule is added.

Primary references checked on 2026-09-28:

- [Isabelle/HOL Bubblesort](https://isabelle.in.tum.de/library/HOL/HOL-ex/Bubblesort.html):
  supports the chosen pass schedule and a size-decreasing proof strategy, but
  works under `linorder`. It does not justify our weaker Local theorem.
- [Bar-Noy and Naor, SIAM 1990](https://epubs.siam.org/doi/10.1137/0403002):
  the inspected abstract concerns translating comparison sorts to tournament
  Hamilton paths. It does not establish correctness of our unchanged program;
  no theorem from it is admitted into A Program. Full proof correspondence is
  not claimed from an abstract.

**Plan:**

- [x] Expanded O2/ASan/UBSan gates: internal refutation, assertion-free synthesis,
  ordinary/retained observations, resume and wrong-edge rejection.
- [x] Shared Tree runner regression after extracting identical test helpers.
- [ ] F3's wrong-function comparison, CLI reader policy, promotion and F5
  integration remain open. No implicit witness, sorting oracle or alternative
  source-Solve path is introduced by this milestone.

F3 recheck: attaching QuickSort's proofs to same-domain identity remains
**pending at 80,000,000 steps** on the candidate, not accepted or rejected.
This does not pass the negative gate. Next work should isolate that conversion
request and its readback cost, rather than treating further budget increases
as a fix. Log: `/tmp/a-program-f3-wrong-function-80m.log`.

Non-Markdown accounting for this checkpoint (against `87bb490`):

| Prototype file | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `bubble.p` | 85 | 0 | +85 |
| `bubble-order.p` | 99 | 0 | +99 |
| `bubble-cases.p` | 25 | 0 | +25 |
| `bubble-cycle.p` | 7 | 0 | +7 |
| `bubble-directional-counterexample.p` | 59 | 0 | +59 |
| `bubble-check.sh` | 58 | 0 | +58 |
| `check-functions.sh` | 17 | 0 | +17 |
| `tree-check.sh` | 1 | 16 | -15 |
| `stress-wrong-function.p` (comment only) | 2 | 2 | 0 |
| **Total** | **353** | **18** | **+335** |

Accepted compiler C/header changes: **0**. This is source library/proof coverage,
not a compiler-size reduction. Markdown is excluded from that total.

### F3 Conversion Head Checkpoint, 2026-09-28

**Subjective (User):** continue F3/F4 verification without changing the algorithms,
Core interning or the assertion-only role of `::`.

**Objective (Code):** baseline `ac7d906` plus the readback candidate. The
same-domain negative has one remaining comparison, not repeated synthesis.
At 12M transitions its strong-NF stack reaches 158,580 entries; at 80M it remains
pending. The WHNF is a Fold over a neutral Match with a neutral-Match lambda
continuation. The full NF fallback unfolds recursive branch code indefinitely
instead of recognizing a stable head. `src/prototype/conversion_head/` provides
the diagnostic, patch and regression gates. No accepted C/header is edited.

**Assessment:** classify under-applied Fold as rigid, and complete Fold only
when both its source and its lambda continuation body are neutral under the
existing predicate. This rules out Return/Request dispatch and right-unit eta;
existing congruence handles the children. Unknown shapes keep the old fallback.
Treating every Fold as rigid is rejected: a reducible continuation can still
become the right unit. Tests retain both positive eta cases and divergence.
This changes comparison strategy, not DefEq equations, solver authority or wire
format. The original raw-Core reproducer fails its rejection assertion; the
candidate and the complete existing Core test pass.

**Plan:**

- [x] Source rejection: **5,816,830** steps, including the original same-domain
  function. `check.sh` now includes that negative; it is not replaced by a
  wrong-domain example.
- [x] Saved rejection and ordinary/retained partial resume: **5,817,131** steps,
  with byte-identical zero-step rewrite. O2/ASan gates: **18/53 s**.
- [x] Existing Core plus new focused boundary tests pass O2 and ASan/UBSan.
  The latter check both equal and unequal branch bodies through congruence,
  without normalizing unrelated recursive branches.
- [x] Quick/Insertion retained List/Vec/Fin reports pass (**43 s**) at the
  explicit API bound; concrete-value transport and wrong-label rejection pass.
- [x] Bubble's full prototype gate passes on this candidate (**212 s**, O2),
  including the internally proved directional counterexample and image resume.
- [x] Legacy Merge's full `views` gate passes (**93 s**, O2), including ordinary
  results, wrong-proof rejection, Vec contents and labelled Fin origins.
- [x] Tree's full prototype gate passes (**195 s**, O2), including generic source
  proofs, cyclic relation controls, ordinary/retained observations and resume.
- [x] Run Quick/Insertion `all` on this candidate: observations, independent
  synthesis, all negatives and ordinary-image resume pass; the command exits
  **1** at retained-image CLI load. This is a failed whole gate, not a pass.
- [x] Full `check-acceptance` exits **0**: **63/63** source compatibility, all
  four LT/partition variants, ordinary-result theorems, Local/Strong, Fin/Vec,
  images, optional witnesses and the readback/conversion prototype tests pass.
  Sanitizer coverage is the focused Core/source suite, not the full gate.
- [ ] Resolve the CLI reader policy, obtain promotion approval and finish F5.

The next CLI change should expose the existing caller-selected reader bound as
`--image-limit N`, keeping the default and validating a positive `size_t`-sized
argument. This is a reader resource parameter, not a proof assertion or a bound
on total memory. Test insufficient/default/explicit bounds, ordinary/retained
resume and invalid option rejection. Do not silently discard retained records,
trust them, or globally increase the default to make this fixture pass.

Logs: `/tmp/a-program-conversion-head-{source,sanitize-source,core,sanitize-core,
retained,value,all,bubble,merge,tree,acceptance}.log`. Reproduction commands and the exact conservative
fragment are in the [prototype README](../src/prototype/conversion_head/README.md).

Non-Markdown accounting against `ac7d906`, excluding inherited work:

| Prototype file | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `conversion_head/build.mk` | 16 | 0 | +16 |
| `conversion_head/conversion.c.patch` | 35 | 0 | +35 |
| `conversion_head/overlay.sh` | 8 | 0 | +8 |
| `conversion_head/probe.c` | 90 | 0 | +90 |
| `conversion_head/source-check.sh` | 23 | 0 | +23 |
| `conversion_head/test.c` | 95 | 0 | +95 |
| `finite_sorting/check.sh` | 2 | 0 | +2 |
| `finite_sorting/stress-wrong-function.p` (comments) | 2 | 2 | 0 |
| **Total** | **271** | **2** | **+269** |

The patch file includes context; its generated C change is **+19/-1, net +18**.
Accepted compiler C/header changes remain **0**. This is additional regression
and diagnostic coverage, not a compiler-size reduction. Markdown is separate.

### F3 Explicit Reader Bound, 2026-09-28

**Subjective (User):** continue F3/F4, keep both ordinary and retained `.a`
workflows, and do not weaken synthesis or `::` to pass the examples.

**Objective (Code):** baseline `cf1081e`; the CLI fixes an existing reader API
parameter at one million. The full sorting image is larger. Its ordinary
source-Solve and typed API load already pass. `src/prototype/image_limit/`
composes the two verified compiler trials and exposes `--image-limit N`.
Only the CLI changes; no reader/writer, Core, proof rule or image field changes.

**Assessment (agent):** make the bound explicit per invocation, preserving the
default and reader validation. Reject zero, invalid/duplicate arguments and use
without `--load`. Report the configured bound on input failure without claiming
it is the failure's cause. The bound is not total memory or proof fuel; restored
inputs still go through source Solve. Silently raising the default, discarding
retained data and adding a trusted-load path are rejected alternatives.

**Plan:**

- [x] O2/ASan CLI boundaries, default/explicit agreement, stdin/root/NF/REPL,
  ordinary/retained 0/1/completed-image resume and invalid-post-check rejection.
- [x] Quick/Insertion `all` passes with an explicit **3,000,000** bound (**151 s**,
  O2), including full retained load and the formerly pending same-domain case.
- [x] Full-sized rejected retained image: default bound fails; explicit bound
  loads, but still rejects at **5,948,767** steps. Zero-step rewrite preserves
  bytes; subsequent rejection remains unchanged. O2 and ASan/UBSan pass.
- [x] Quick/Insertion `all` passes ASan/UBSan (**482 s**) with leak checking and
  halt-on-error; the full compiler suite is not claimed sanitized.
- [x] Aggregate five-backend gate exits 0. The full-sized rejected-image gate
  also passes separately in O2/ASan; it was added as an aggregate prerequisite
  after that aggregate invocation started.
- [x] Full compiler `check-acceptance` exits 0: 63/63 source compatibility,
  all four LT/partition variants, Local/Strong, Fin/Vec, images, optional witness
  isolation and the readback/conversion/CLI prototype tests pass.
- [ ] Obtain approval, promote the verified compiler/library/tests and add the
  F5 accepted-build targets. Approval was requested; it is not assumed.

Reproduction commands and the detailed gate list are in the
[prototype README](../src/prototype/image_limit/README.md). Logs use
`/tmp/a-program-image-limit-{build,sorting,large,sanitize-cli,sanitize-large,
sanitize-sorting,backends,acceptance}.log`. The old default still fails on the
large fixture by policy; the explicit bound is required, like an adequate step
budget. This prototype milestone alone does not complete the overall goal.

Non-Markdown accounting against `cf1081e`, excluding inherited work:

| Prototype file | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `image_limit/main.c.patch` | 62 | 0 | +62 |
| `image_limit/overlay.sh` | 8 | 0 | +8 |
| `image_limit/build.mk` | 21 | 0 | +21 |
| `image_limit/check.sh` | 45 | 0 | +45 |
| `image_limit/large-check.sh` | 18 | 0 | +18 |
| `image_limit/sample.p` | 2 | 0 | +2 |
| `image_limit/invalid.p` | 2 | 0 | +2 |
| `finite_sorting/check.sh` | 7 | 5 | +2 |
| `finite_sorting/check-functions.sh` | 1 | 0 | +1 |
| **Total** | **166** | **5** | **+161** |

Patch context is included above; the generated CLI C change is **+17/-5, net
+12**. Accepted C/header changes: **0**. Markdown is counted separately.

### List/Vec View Checkpoint, 2026-09-27

Agent implementation decision, baseline `ff85452`: move the existing Vec proofs
unchanged into `tests/fixtures/finite_vectors.p`, importing the sorting provider's
actual `List`. The acceptance file now only exercises exported definitions.
`finite_list_views.p` adds ordinary source IADTs and proof functions:

- `list_vector_view` relates the exact spine and elements of a Vec and List.
  Both `list_vector` and `vec_contents` have general coverage proofs.
- `list_size` witnesses exactly `n` elements in a replacement List.
  `vector_refill` constructs `Vec A n` from that evidence; its contents preserve
  every explicitly supplied List predicate in both directions, for any accepted
  size witness. The compiler does not invent the witness from a post-check.
- List reconstruction preserves List predicates. Rebuilding a Vec from its own
  contents and its canonical size proof preserves each position's value in both
  directions, through the earlier `vec_at` proof functions.

These are checked source view laws, not object Higher Identity, proof irrelevance,
or an automatic interface for arbitrary containers. No C/header, Core, rule,
syntax or image-format changes. The generic finite-position permutation bridge
and common sorting wrappers are still pending. The initial nested-Match
`permutation_size` trial rejected: its motive mixed a generalized List with a
fixed captured tail. The later `size_step` trial generalizes both consistently
and checks. This was a source-proof repair, not grounds for using `::` to supply
a motive. The subsequent compiler findings are distinguished below.

Fresh verification on the clean candidate: the new focused suite passes in O2,
Debug and ASan/UBSan with leak checking and halt-on-error. Source/imports, all
post-checks removed, ordinary/retained images, budgets 0/100, and invalid-image
resume are covered. Source checks in 508,407 steps; assertion-free in 363,623;
ordinary/retained loads in 508,592/534,974. Wrong length, omission, duplication,
reordering and wrong shape are rejected. Closed examples cover empty, singleton,
two different values and duplicates; they supplement the open-input proofs.
The affected general Sorted, ordinary QuickSort-result and Local/Strong targets
also pass. The complete gate was run for `ff85452`; it is not claimed rerun for
this source-only checkpoint. Logs: `/tmp/a-program-list-view-` with
`{focused,debug,asan,sorting}.log`; the relocated family/Vec suite has corresponding
`family` logs. Step counts measure checking, not sorting runtime complexity.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/Makefile` | 5 | 0 | +5 |
| `tests/acceptance/finite-list-views.p` | 40 | 0 | +40 |
| `tests/acceptance/finite-vector-lookup.p` | 3 | 115 | -112 |
| `tests/family_domain_conversion.sh` | 5 | 3 | +2 |
| `tests/finite_list_views.sh` | 63 | 0 | +63 |
| `tests/fixtures/finite_list_views.p` | 144 | 0 | +144 |
| `tests/fixtures/finite_vectors.p` | 117 | 0 | +117 |

Implementation C/headers: **0**. Source proof library/tests: **+372/-118,
net +254**; build **+5/-0**. This is library/proof coverage, not compiler growth.
This plan: **+56/-3, net +53**. Complete checkpoint: **+433/-121,
net +312**, counting the Vec proof relocation rather than duplicating it.

### Generalized Motives and Shared Comparison, 2026-09-27

Objective (Code), baseline `dd941dd`, published as `b590dbc`:

- A minimal nested Vec Match with an explicit motive reports `unsupported`
  after 2,384 steps on Main. `match_explicit_motive_step` unconditionally excludes
  generalized captured declarations. The candidate checks it in 3,127 steps.
- A 67-node shared Lambda DAG compared under an outer alpha correspondence
  remains pending after 100,000 steps (16,030 comparison tasks). Unrelated,
  identical inner binders created distinct scope keys for the shared body.
  The candidate finishes in 105 steps / 64 tasks without merging Term pointers.
- The larger separate-fold permutation-action trial still remains pending at
  30,000,000 steps. Its comparison advances; it is not established to be a
  scheduler deadlock. This small DAG repair does not solve all comparison costs.
- The alternative joint-record trial reaches strong normalization, expanding
  erased recursive code (810,397 NF steps at a 2,000,000-step checking budget).
  Its first conversion mismatch is not yet explained; neither equality nor
  inequality of the source obligation is established by that timeout.

Assessment (agent decision): explicit motives must use the same checked
generalization map as branch bodies. Reuse that map for source aliases, captured
variables and IH associations, then abstract the captured telescope with the
existing Pi rule. Explicit motive names shadow ambient aliases. No new typing
rule, result inference from `::`, equality reflection or image format is added.
For alpha comparison, keep the existing scope unless an identical binder really
shadows a nonidentity pair. Scope lookup remains fuel-accounted and serializable.

Plan and verification:

- [x] Add permanent nested-Match tests: general flip/replacement, multiple
  captures, motive-name shadowing, actual values, incorrect motives/post-checks,
  assertion-free source, ordinary/retained images and 0/100/1,000-step resume.
- [x] Add shared-DAG, bound/free and shadowing counterexamples; save/resume at
  every comparison step, including partially inspected Lambda scopes.
- [x] Focused O2, Debug and ASan/UBSan suites and Core tests pass locally.
- [x] Complete the clean full acceptance gate: 1,507.745 seconds, exit 0;
  source compatibility 63/63. Publish this verified compiler epoch.
- [ ] Complete F2's generic permutation/action bridge. A source-only joint
  `reordering` witness trial bundles output size, a Fin bijection and position
  evidence in one induction. Its complete constructor/induction checks are still
  pending; it is not an accepted library. The composition projection requires a
  separately proved pointwise law, not conversion of a neutral Match.

Logs: `/tmp/a-program-generalized-{motive-focused,debug,asan,debug-core,asan-core,
eval-io,acceptance}.log`. Unverified source experiments remain in the detached
candidate's `src/prototype/`, outside publication. F2-F5 and broad P4/P5 stay open.

Change accounting against `dd941dd`, excluding unpublished experiments:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/graph.c` | 33 | 17 | +16 |
| `src/synthesis.c` | 44 | 18 | +26 |
| `tests/core.c` | 21 | 0 | +21 |
| `tests/eval_io.c` | 16 | 1 | +15 |
| `tests/acceptance/generalized-match-motive.p` | 31 | 0 | +31 |
| `tests/generalized_match_motive.sh` | 44 | 0 | +44 |
| `src/Makefile` | 6 | 0 | +6 |

Implementation C: **+77/-35, net +42**. Tests: **+112/-1, net +111**.
Build: **+6/-0**. Non-document total: **+195/-36, net +159**.
This is a correctness/performance checkpoint, not completion of the authority
refactor or a net code-reduction claim. The full gate includes the new tests;
Debug and ASan/UBSan focused suites and Core tests also pass. No new surface
syntax, proof rule, serialized state field or expected-type synthesis is added.
This plan: **+73/-4, net +69**. Complete checkpoint: **+268/-40,
net +228**.

### F2 Prerequisite: Constructor Sequencing

**Subjective (User):** preserve independently synthesized terms, `::` as a
post-check, one typing authority, and ordinary left-to-right effects. Do not
substitute passing examples for the general Fin action theorem.

**Objective (Code), `b590dbc`:**
[`indexed_constructor_sequence.p`](../src/prototype/indexed_constructor_sequence.p)
isolates a pure call whose later proof depends on a computed earlier field.
It rejects in 3,091 steps, also without its post-check. The nonindexed counterpart
passes in 3,160 steps. Debugging confirms the checker compares `same A x x`
with `same A (unbox A b) x`, where `b` is a fresh sequencing variable, not the
supplied `boxed A x`. `prepare_constructor_spine` collects argument computations;
`application_bind` abstracts their results before `constructor_application_step`
checks all fields together. This is not a missing equality axiom or `::` hint.

**Assessment (agent):** index recovery from argument classifiers is valid;
batching runtime sequencing with it loses a dependent argument's known result.
Disabling spine collection makes the diagnostic pass (3,240 steps) and the
List/Vec record identity check pass (773,636), but breaks the valid existing
`inferred-index-later-recovery.p`. That trial is rejected, not a repair.
The full reordering proof still rejects after this diagnostic change; the
remaining source proof must be checked independently. Do not promote the
30-million-step timeout to a performance-only diagnosis or relax conversion.

**Plan:**
- [x] Isolate the failure and test the ordinary/Indexed and asserted/unasserted
  controls. Preserve the small reproducer; do not bless its rejection as correct.
- [x] Separate signature-only index recovery from value sequencing. Reuse the
  ordinary checked curried application/sequence path after specialization,
  without a second acceptance engine, binding-equality assumptions or eager
  evaluation of effectful arguments during checking.
- [x] Preserve later-field recovery, typed aliases and independently requested
  partial applications; effectful partial applications must execute their prefix
  once at the existing point, not under a newly introduced Lambda.
- [x] Require the reproducer, wrong proof/nominal/index controls, existing
  inferred-index effects, ordinary/retained images, and full acceptance to pass
  before publishing the compiler repair.
- [x] Then finish F2's actual-value action, including the rebase prerequisite below.

This diagnostic checkpoint changes no accepted implementation or test outcome.
Against `b590dbc`: this plan **+44/-1**, prototype diagnostic **+13/-0**;
total **+57/-1, net +56**. Accepted C/headers and permanent tests: **0**.

Verified repair on `e68b2f1`: constructor specialization reads independently
checked argument classifiers; ordinary application then consumes each value in
order. No additional acceptance rule or expected-type synthesis is introduced.
Focused O2, Debug and ASan/UBSan tests pass, including later-field recovery,
multi-index effectful partial calls, wrong proofs, and ordinary/retained image
resume. The complete acceptance gate passes in **1,657.512 seconds**. Logs:
`/tmp/a-program-constructor-sequence-{focused,debug,asan,acceptance}.log`.
The source-level F2 action and the separate rebase repair are not part of this
compiler checkpoint. Against `e68b2f1`, `src/synthesis.c`: **+86/-41**;
`src/Makefile`: **+5/-0**; permanent source/shell tests: **+120/-0**.
Non-document total: **+211/-41, net +170**. No new runtime node, proof rule,
serialized field or expected-type synthesis is added.

### F2 Prerequisite: Returned-Value Rebase Progress

**Subjective (User):** one typed authority; follow the existing computation and
its checked structure, rather than recreate or repeatedly solve the same state.

**Objective (Code), `e68b2f1` plus constructor repair:** the open source theorem
from List permutation to a bijection and its value law checks (1,155,037 steps
with its provider). The closed three-element composition does not finish.
Debugging fixes synthesis at step 1,197,635 while `typed_rebase_step` repeats
three states: derived RETURN value, its reindexed image, and that same value
with the pending map. More than 16 million local query steps make no progress.
`structural_input` synchronously drains this query, so the outer synthesis
budget does not interrupt it. The diagnostic O2/Debug runs were stopped, not
recorded as successful checks or mere budget exhaustion.

**Assessment (agent):** this is a typed-provenance traversal cycle, not a missing
equality axiom, a reason to weaken `::`, or established alpha-comparison slowness.
Rebase the accepted computation and invert its RETURN in the destination scope;
do not extract a value that points back to the same computation and start again.
The constructor repair was verified and published separately as `9a8a567`.
The next candidate changes only this traversal: transport the computation with
the pending map, rebase it, then invert its checked RETURN at the destination.
The existing `image_boundary` checks the result's Core and classifier. There is
no cycle cutoff, new proof rule, trusted result or alternate evidence store.

**Plan:**
- [x] Establish the repeated typed-query states and separate them from the open
  theorem, which already checks. Keep F2 incomplete until closed laws also pass.
- [x] Localize the reproducer and implement a structurally progressing query
  using existing checked reindex/RETURN rules, without a second evidence store.
  The acceptance prefix through `certificate` passes on `9a8a567` (1,181,452
  steps); requesting its size/positions reproduces the cycle. A 15-second
  external timeout confirms the old binary does not complete that prefix.
- [x] Check context/classifier preservation, rejection of captured free binders,
  bounded local progress, and ordinary/retained images under split budgets.
- [x] Repeat affected Debug/sanitizer tests and the full gate before publishing
  a second compiler epoch; then finish the actual-value F2 acceptance suite.

### F2 Actual-Value Bridge Checkpoint, 2026-09-27

Agent implementation decision on `9a8a567` plus the rebase repair:
`tests/fixtures/finite_permutation_views.p` defines the general source theorem
`permutation_reordering`. Given `permutation A xs ys` and `list_size A n xs`, it
constructs a dependent record containing `list_size A n ys`, a bijection on
`Fin n`, and a `vec_at` witness for every target position and its source value.
Its map goes **target position to source position**. Nil, keep, swap and compose
are proved by induction; no list is replaced by a separately computed result.
`reordering_observe` and its reverse transport an explicit `P : A -> @` between
the corresponding actual lookup values. This is not a claim of arbitrary Higher
Identity, proof irrelevance, stability, or a completed common sorting backend.

The endpoints are fixed parameters of this record, not inferred indices. The
discarded indexed-record trial introduced unnecessary endpoint refinement at
composition. A generic `reordering_make` function names its ordinary constructor;
direct namespace selection on a computed parameter remains unsupported and is
not claimed fixed. Explicit Match motives supply source information; `::` does
not supply it, as the assertion-free suite verifies.

Focused O2 suite passes in **102.318 seconds**. The imported open library checks
in **1,155,037 steps**, the closed acceptance in **2,130,184**. The duplicate-value
example `[2,1,1] -> [1,1,2]` has origin positions `[1,2,0]`, verified after image
readback at chunks 1 and 64. Wrong length, omitted/duplicated values, wrong value
transport, wrong origin and duplicated positions reject. Ordinary/retained
images and 0/100-step resumes pass, including resumed rejection. These are
checker measurements, not sorting runtime costs. Debug also passes in
**167.994 seconds**; the small Core suite passes in Debug and ASan/UBSan.
The sanitizer source suite passes in **307.781 seconds**. The full acceptance
gate passes in **1,834.057 seconds**, including source compatibility **63/63**,
Core, identity/evaluation images, general Sorted/permutation, derived-LT,
Local/Strong and the new Fin checks. The new shell test bounds each command at 120
seconds: outer synthesis fuel alone cannot interrupt the reproduced local-query
cycle. The final guarded suite passes in O2 (**111.022 seconds**) and ASan/UBSan
(**314.086 seconds**); no compiler timeout was added.
It was installed before the full gate reached this new test, with all compiler
and other test inputs unchanged.
Logs: `/tmp/a-program-fin-action-{focused-bounded,debug,asan-bounded,
asan-core,acceptance}.log`. F3/F4 and broad authority work remain open;
#41 does not close on this F2 checkpoint.

The small Core test covers context/classifier preservation, captured-binder
rejection, finite local progress and cache reuse. It does **not** reproduce the
original three-state cycle by itself: simple returned values retain a direct
typed child. The permanent composed-permutation source is that regression test;
do not describe the simpler Core control as a minimized reproduction.

Change accounting for this candidate, against `9a8a567`, excluding inherited
root edits and unpublished prototype experiments:

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `src/evidence.c` | 9 | 8 | +1 |
| `src/Makefile` | 5 | 0 | +5 |
| `tests/core.c` | 49 | 0 | +49 |
| `tests/fixtures/finite_permutation_views.p` | 238 | 0 | +238 |
| `tests/acceptance/finite-permutation-views.p` | 82 | 0 | +82 |
| `tests/finite_permutation_views.sh` | 79 | 0 | +79 |

Compiler C: **+9/-8, net +1**. Source proof library: **+238**; verification
code/fixtures: **+210**; build: **+5**. Non-document total: **+462/-8, net +454**.
The library addition is new functionality, not a claim that compiler size fell.

## F5. Verification and Publication

### Subjective (User)

Use progress-tracked Markdown and publish tested milestones to Main.

### Objective (Code)

`src/Makefile:check-acceptance` is the permanent gate. Existing source-image
tests cover ordinary/retained images and budgeted resume. No new image format
is needed for source library definitions.

Promotion audit at `72621ec`, 2026-09-28: Quick/Insertion, Merge and value gates
duplicate the shared status checker. The prototype comparator includes the
existing `tests/program.c` with a bounded-reader substitution; it does not
implement another typed comparison. Readback tests condition some assertions
on `PG_SUPPORT_CANDIDATE`, so copying them without that flag would lose coverage.

### Assessment

Successful examples alone cannot establish view laws or general sorting.
Small finite checks are boundary tests, not substitutes for open proofs.

Agent decision: share the prototype runners now; adopt the existing comparison
tool's reader parameter directly upon promotion and remove the include wrapper.
Do not create another checker, make candidate assertions optional, or preserve
overlay implementations beside adopted owners. Promotion still needs approval.

### Plan

- [x] F1: add a focused permanent target to `check-acceptance`; reuse program/image
  comparison tools instead of adding another checker or proof runner.
- [x] Audit negative coverage: `tests/finite_positions.sh` checks bad bijections
  and inverses; `finite_list_views.sh` checks refill order/shape/length;
  `finite_permutation_views.sh` checks omitted/duplicated positions and values.
  Prototype backend gates add wrong function/result/comparator and Local-to-Strong
  controls. Their accepted-build integration remains unchecked below.
- [x] Share prototype status/comparison helpers without changing budgets, reader
  bounds, assertions or fixture selection. Explicitly reject exit-code mismatch
  inside an `if`; two helper controls pass in O2 and ASan/UBSan. Existing top-level calls already
  failed through `set -e`, so prior gate results are not invalidated.
- [x] Re-run all five backend gates, large rejected-image and same-domain
  rejection/resume gates, plus retained API `all`: exit 0. Logs:
  `/tmp/a-program-f5-{backends,retained,helper,sanitize-helper}.log`.
- [ ] Obtain promotion approval and perform the owner moves below.
- [ ] Register F3/F4 and reader-bound gates in accepted `check-acceptance`; run the
  promoted suite from a clean build and focused ASan/UBSan before closing #41.
- [x] F1: compare assertion-free and asserted programs; missing source information
  must stay missing. Explicit motives and constructor bounds must pass.
- [x] F1: check imports, ordinary/retained images and resume from 0/100 steps.
- [x] F1: run the full existing gate; for later compiler-change epochs repeat it,
  and for source-only
  milestones also run affected generic sorting and Local/Strong checks.
- [x] F1: record per-file additions/deletions, measured proof/evaluation work and
  nonclaims separately. Do not equate checker steps with algorithm complexity.
- [x] F1: report evidence and remaining scope on #41; close only when
  its complete implementation criteria are satisfied, not at this milestone.

Promotion map (agent proposal, not yet executed):

| Current prototype | Accepted owner / removal |
| --- | --- |
| Readback support and graph/eval patches | `src/support.{c,h}`, `src/graph.{c,h}`, `src/eval.c`; link support in every graph-only build target, not just `SOURCES` |
| Fold conversion and CLI patches | `src/conversion.c`, `src/main.c`; remove applied patch/overlay/build copies |
| Readback and conversion C tests | `tests/` test owners; make candidate assertions unconditional and register them in `src/Makefile` |
| Sorting source proofs, cases and both `stress-*.p` regressions | `tests/fixtures/finite_sorting/`; retain ordinary-result and same-domain negative tests |
| Sorting runners/provider/shared helper | `tests/finite_sorting/`; update fixture paths once, preserve individual budgets and the five-backend aggregate |
| `finite_sorting/image_compare.c` | Add explicit reader-bound configuration to `tests/program.c`; delete the macro/include wrapper and its extra binary |
| Reader-bound runner and fixtures | Accepted `tests/` owners; retain default/explicit, invalid, stdin/root, zero-step and full rejected-image checks |
| Diagnostic probes and construction benchmark | Remain optional under `src/prototype/`; point at accepted owners, without retaining alternate implementations |

No C/proof-library changes in the helper consolidation. Per-file test-code
delta against `72621ec`: `check-functions.sh` +3/-3, `check.sh` +1/-16,
`merge-check.sh` +7/-21, `value-check.sh` +1/-12, `tree-check.sh` +1/-1,
`bubble-check.sh` +1/-1, `retained-check.sh` +3/-2, `image_limit/check.sh` +3/-0:
**+20/-56, net -36**, excluding this documentation. Full compiler regression
passed at `72621ec`; this shell-only change requires the affected gates, not
an unsupported claim that the entire compiler suite was rerun.

### Milestone Accounting

Relative to `ebe0648`; inherited root-worktree experiments excluded. All entries
below are additions; there are no deletions. This adds a source proof library and
tests, not a compiler-code reduction. Accepted C implementation/header changes: 0.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `tests/fixtures/finite_positions.p` | 132 | 0 | +132 |
| `tests/acceptance/finite-positions.p` | 71 | 0 | +71 |
| `tests/finite_positions.sh` | 82 | 0 | +82 |
| `src/Makefile` | 5 | 0 | +5 |
| `src/prototype/family_domain_conversion.p` | 10 | 0 | +10 |
| `README.md` | 4 | 0 | +4 |
| Authority plan | 2 | 0 | +2 |
| Imported sorting audit (successor link only) | 2 | 0 | +2 |
| This plan | 292 | 0 | +292 |

Local logs: `/tmp/a-program-finite-positions-{focused,debug,asan}.log` and
`/tmp/a-program-finite-positions-acceptance-resumed.log`. Temporary logs and
unsuccessful Vec trials are not published; the minimal compiler diagnostic is.
Source-only changes need no wire-format migration. F2-F5 and broad P4/P5 remain
open; #41 must not close on this milestone.

## Primary References

Inspected 2026-09-27; no external prover executed. The rest of PR #42's broad
historical bibliography has not been exhaustively revalidated here.

- [Mathlib Tuple.Sort](https://leanprover-community.github.io/mathlib4_docs/Mathlib/Data/Fin/Tuple/Sort.html):
  finite-position permutation and monotonicity; its LinearOrder assumption is
  stronger than our Local contract and is not adopted wholesale.
- [Agda finite permutations](https://agda.github.io/agda-stdlib/v2.3/Data.Fin.Permutation.html):
  finite equivalences and inverse laws support the pointwise bijection interface.
- [Rocq Mergesort](https://rocq-prover.org/doc/V9.1.0/stdlib/Stdlib.Sorting.Mergesort.html):
  local sortedness and stronger transitive-order conclusions are separate.
- [F*/Pulse sorting abstraction](https://fstar-lang.org/tutorial/book/agentic/agentic_sorting_algorithms.html):
  shared proof-carrying sorting contracts across algorithms; cost is additional.
