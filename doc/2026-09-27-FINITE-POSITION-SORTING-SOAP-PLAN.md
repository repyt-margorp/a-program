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
| F3 | One ordinary-result sorting specification | Pending |
| F4 | Quick/Merge/Insertion/Bubble and additional backends | Pending |
| F5 | Permanent rejection, image and regression gates | Pending |

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
- [ ] F3: package an ordinary function with its general Local and permutation
  proofs; derive Strong separately. Prove action identity/composition pointwise.
- [ ] F3: check both sort-to-List and same-shape wrappers against their laws;
  document selected traversal rather than claim arbitrary-container support.
- [ ] F4: instantiate QuickSort, InsertionSort, BubbleSort and the existing
  merge fixture. Add a separately named conventional two-front MergeSort if
  required; never silently replace the old algorithm or claim its complexity.
- [ ] F4: cover a straightforward additional backend (selection or existing
  tree sort), recording the actual relation assumptions per algorithm.
- [ ] F4: verify open-input ordinary-result theorems, then closed examples for
  empty/singleton/reversed/already-sorted/duplicate-labelled data.

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

### Assessment

Successful examples alone cannot establish view laws or general sorting.
Small finite checks are boundary tests, not substitutes for open proofs.

### Plan

- [x] Add a focused permanent target to `check-acceptance`; reuse program/image
  comparison tools instead of adding another checker or proof runner.
- [ ] Reject omitted/duplicated positions, bad inverses/refill, wrong shape/length,
  wrong result certificates, and Local-to-Strong without the required assumptions.
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
