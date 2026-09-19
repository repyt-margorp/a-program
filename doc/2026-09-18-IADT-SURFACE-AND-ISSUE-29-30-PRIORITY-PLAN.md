# IADT Surface and Issue 29 / PR 30 Priority Plan

Date: 2026-09-18.
Baseline: `3a3bf550b3e882ab650395fd612e0fa2526b45bc` plus the current dirty
worktree. Preserve the in-progress authority refactor and its tests.

## 1. Priority and Scope

The user has changed the execution order:

1. Advance **IADT constructor-index synthesis** and **issue #29 / PR #30** as
   two coordinated workstreams. These are the immediate priorities.
2. Integrate and verify both, documenting the exact supported surface rules.
3. Then resume the broad duplicate-synthesis/authority refactor. Do not make
   completing that large cleanup a prerequisite for these features.

Only local prerequisite cleanup needed by these workstreams belongs in phase 1.
Avoid unrelated schema, scheduler or artifact redesign. Implement under
`src/prototype/`; documentation changes may live in `doc/` and the README.
This is a plan, not authorization to declare the parent refactor complete.

Sources of requirements:

- [Constructor decision](2026-09-18-CONSTRUCTOR-INDEX-SYNTHESIS-DECISION.md).
- [Issue #29](https://github.com/repyt-margorp/a-program/issues/29) and
  [PR #30](https://github.com/repyt-margorp/a-program/pull/30).
- [Current reproduction/fix report](2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md).
- [Deferred Solver audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md).
- [Parent authority plan](2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md).

## 2. Verified Starting Point

| Item | Current evidence | Status |
|---|---|---|
| Index syntax | `syntax.c:116`, `declaration_body` already parses the proposed token sequence | Parser shape exists; new binding meaning absent |
| Constructor scope | `declaration_step` binds Self over the parameter scope; constructor telescopes do not inherit header-index variables | Needs change |
| Constructor use | `constructor_value_step` abstracts all schema fields; `prepare_application` consumes ordinary Pi arguments | No implicit index recovery |
| Match | `case_field_scope` normally requires surface pattern arity to equal schema field arity | Hidden fields need an explicit source-to-schema correspondence |
| Graph name collisions | Canonical local `caseN` names and unambiguous source aliases implemented in dirty worktree | Positive/negative tests pass |
| LE predecessor | Several checked index transports can compose when dependency count decreases | Open-index lemma and invalid variants tested |
| LE transitivity | Attempt recorded in the fix report remains unsupported | Must diagnose, not bypass |
| Comparator theorem | Graph-based recursive `Compared` specification passes; conventional LE Decision theorem is not established | Incomplete |
| Universal Sorted | No complete universal per-sort proofs from this investigation | Incomplete |
| PR #30 | Open, documentation-only, one Markdown file; no code patch | Critically reviewed; not merged |

The last optimized full `check-acceptance` and focused ASan/UBSan tests, including
the complete image-CLI runner, passed before this plan. These results do not
verify the new IADT surface change, which is not implemented.

## 3. Non-Negotiable Contracts

- Core remains Lambda/Application/Reference with semantic owners. No new Core
  implicit-index tag, equality interning, or value/computation Core split.
- Use pointer-labelled binders and existing checked contexts/substitutions.
  Never identify typed uses merely because their erased Core is shared.
- Synthesize from supplied arguments and their types. `::` is a post-check;
  it must not select an index, motive or proof that synthesis failed to produce.
- Header index names used by a constructor denote fresh constructor-local
  quantified variables, not a fixed result index shared by every constructor.
- Reject unrecoverable implicit indices while checking the declaration, not
  on its first constructor call. The rejected declaration must not export an
  accepted family. Pending dependencies remain pending, not rejected by timeout.
- Preserve explicit constructor fields, including existing Acc declarations.
  No nominal declaration may refer to itself by its source name instead of `*`.
- All graph, index-refinement and property witnesses are ordinary checked terms.
  Do not add LE-, Vec- or sorting-specific kernel rules.
- Loading/resaving an unfinished `.a` does not execute effects or accept proofs.
  Resumed checking follows the same Solve path as source checking.

## 4. Workstream I: IADT Surface

### I1. Binding and declaration lowering

Required source:

```text
Vec := \A : @ => @\n : Nat => {
	nil : * Nat.zero;
	cons : A -> * n -> * (Nat.succ n);
};
```

For each constructor, resolve free references to header index names, taking
ordinary lexical shadowing into account. Include the dependencies needed to
type those variables, in telescope order. Allocate fresh local binders and
lower them into ordinary explicit kernel fields before the written fields.
Unused names are not generalized: `nil` gains no `n` argument.

The implicit field is a surface convention, not a missing kernel argument.
For Vec, the checked constructor still has the information represented by
`(n : Nat) -> A -> Vec A n -> Vec A (succ n)`. Do not independently optimize
away runtime fields during this feature change.

Touch points: `syntax.c/.h` only if source spans need improvement;
`synthesis.c` declaration/constructor/telescope paths; existing source metadata
for the constructor's written-field versus kernel-field correspondence.
Do not fork the parser or introduce an alternate IADT schema checker.

### I2. Declaration-time recoverability

Compile a small immutable recovery description from the constructor's typed
field signature. Each inferred binder must have a supported path from an
explicit argument's synthesized type to that binder's index value. Reference
existing declaration/field identities; do not create a second mutable solution
table. Reuse this description at calls and when hiding fields in patterns.

Initial supported rule: direct index projection from a recognized nominal
family instance, including Self. Pure normalization may expose the family head;
retain its checked result through the existing normalization machinery.
Dependencies among inferred binders must have a finite, checked recovery order.
Do not invert arbitrary arithmetic or user functions to invent a solution.

Examples:

| Constructor signature | Decision |
|---|---|
| `nil : * Nat.zero` | Accept; fixed result, no implicit index |
| `cons : A -> * n -> * (Nat.succ n)` | Accept; tail's type supplies n |
| `mark : * n` | Reject declaration; no recovery input |
| `mark : (k : Nat) -> * k` | Accept existing explicit field form |
| `c : * (f n) -> * n`, arbitrary f | Reject unless a supported projection also independently supplies n; no assumed inverse |
| Two inputs both indexed by n | Recover once, check agreement with ordinary conversion; do not pick whichever arrives first |

Diagnostics identify the constructor and unresolved index, with the available
input paths. A mathematically recoverable but unsupported encoding is not
called inconsistent. An invalid unused constructor must fail its enclosing
declaration; an unrelated selected module member must not hide that failure.
Preserve `definitions_step`'s current checking of unselected entries.

### I3. Calls, aliases and partial applications

The intended saturated call is `(Vec A).cons a xs`. Obtain n from the typed
argument `xs`, then supply all kernel arguments in their checked telescope order
and use the existing constructor/application rules. Inspect typed arguments,
not their erased constructor shape and not an expected output type.

Collect the relevant source application spine before assigning written arguments
to kernel slots. Type synthesis of later arguments must not execute or reorder
their runtime effects. Reuse existing sequencing for computations in argument
positions; each argument effect executes once in the existing order.

Preserve direct aliases and quotation of constructor callables. Use one retained
constructor origin plus supplied typed arguments to describe a partial callable;
do not rediscover the origin by comparing erased functions. For an unresolved
partial use such as `cons a`, the target design retains a quantified index in
its synthesized callable signature and eta-expands with ordinary Lambda/App
as needed. It must not assign an arbitrary n or depend on later `::` checks.
Its later saturated application uses the same recovery description.

Before implementing I3 broadly, verify the partial-call design on `cons`,
`cons a`, an alias of each, and a higher-order wrapper. Passing the underlying
explicit Pi function through an ordinary higher-order parameter must respect
that parameter's declared signature, not silently rewrite all function types
to have implicit arguments. If the retained callable information cannot support
these cases without another authority store, revise this subsection and report
the concrete limitation; do not claim complete currying support from saturated
tests alone.

The later-recovery probe below makes application-spine collection a required
part of I3, not an optimization. For `mk : Vec Nat (succ n) -> Vec Nat n -> D n`,
`mk one nil` must recover n from the second argument before checking the first.
Do not let the outer application overwrite the synthesis result of `mk one`.
An isolated `mk one` cannot in general be abstracted as a function valid for
every n: its captured `one` need not have type `Vec Nat (succ n)` for arbitrary
n. Such a partial use needs its own synthesizable contract; it is not justified
by the successful, index-independent `cons a` case. Preserve the supported
direct-projection rule without introducing inverse arithmetic or extra proof
parameters silently. Whole-spine calls and independently named partial calls
must have distinct, explicit acceptance tests.

Touch points: `prepare_application`, `constructor_value_step`, reference/member
resolution, `source_metadata`, and existing application/sequence state. Keep the
recovery description descriptive and immutable; acceptance stays in evidence.

### I4. Match, IH and persistence

For a constructor declared with implicit n, `@cons head tail => ...` binds only
its written fields. The kernel branch still binds the complete field telescope.
Map written positions to their actual binder objects; do not shift IH positions
using ad hoc argument counts. The declaration header's name must not capture
a variable in the use site's scope. IH `*tail` still denotes the recursive field.

Existing explicit declarations keep their existing pattern arity. The new rule
does not silently drop the explicit k in an old declaration. Graph generation
must consume the checked complete branch while using source-field metadata only
for naming. Generated graph constructors must not acquire the new inference
convention merely because they are IADTs.

Preserve the recovery and field-visibility description across source-only,
partial, and checked-source-root images, either by deterministic reconstruction from
retained declaration syntax or one inert source payload. Choose the existing
owner, not both. Keep the description separate from evidence of declaration
admission. If the wire payload changes, increment the format coherently and
reject incompatible images; update `source_io`, `syntax_io` or declaration I/O
only where the chosen representation actually requires it.

Root boundary clarified by verification: `pg_synthesis_evidence` exports a
bare proof, not a source interface. It has the explicit kernel Pi telescope;
it does not promise constructor spellings or implicit source arguments.
"Typed-root" in this surface requirement means a checked source producer,
which retains that interface even when its containing module is not a root.
Do not reconstruct source names or hidden-argument conventions from an erased
proof. Test both contracts: inferred calls through saved source producers and
explicit calls through raw proof roots, with inert resave in both cases.

## 5. Workstream G: Issue 29 / PR 30

### G1. Finish and review the two isolated fixes

Retain the current collision and predecessor tests. Review `caseN` alias
precedence, nested/helper leaves and declaration-local identity after reload.
Review the strict-decrease bound in `index_transport_progress`: it is an
incomplete search heuristic, not a new definitional equality or general solver.
Record cases it cannot solve instead of weakening the kernel.

### G2. LE transitivity and comparator correctness

Reduce the unsupported transitivity attempt using conventional two-constructor
LE. Trace `match_recursive_motive_step`, `match_candidate_step`,
`match_dependent_motive_step`, branch generalization and scoped index transport.
Distinguish an incorrectly used IH from a failure to synthesize a valid motive.
The final `path_context` guard is a symptom boundary, not a guard to delete.

Construct an ordinary open-variable transitivity proof, then prove that the
reported Nat comparator's result entails the intended order (including the
false case and equality/duplicates). The current `Compared` graph specification
does not substitute for this theorem. Do not change LE by adding transitivity
as a constructor without separately proving equivalence of that specification.

Any generic implementation correction must have a minimal independent fixture,
an invalid sibling that remains rejected, and saved/resumed coverage. Reuse
ordinary motive, substitution and Identity operations; no new global refinement
authority or function-specific proof shortcut.

### G3. Universal sort properties

Use PR #30's provider snapshot as a versioned fixture under the prototype tests;
record its original hash and any mechanical spelling migration. Critically check
its actual algorithms: the reported merge uses repeated insertion, not standard
linear merge. Do not silently swap implementations to make a theorem easier.

Build the required lemmas as A Program terms, with generic graph induction where
appropriate: insertion-bound preservation, tree bounds/traversal, merge
preservation, and partition/pivot/append properties for QuickSort. Start with
insertion sort to validate the full proof route, then cover the other providers.

For each of the four reported sorts, retain a universal theorem over arbitrary
input and output with a graph witness, and connect that witness to the actual
function execution. A suitable schematic target is:

```text
(xs : List Nat) -> (ys : List Nat) -> @sort xs ys -> Sorted ys
```

Use witness packets to consume the theorem for actual results. Twelve closed
examples alone do not discharge the universal theorem. Sortedness alone does
not prove permutation, stability or complexity; keep those claims separate.
Wrong endpoints, unsorted alleged results and incorrect graph leaves must fail.

### G4. Resolve the report honestly

Maintain a requirement-to-test table for the issue's A/B requests, transitivity,
comparator theorem, four universal properties and artifact behavior. If part of
the report is mistaken, provide a reproducible explanation and the correct
spelling; do not classify a genuine unsupported valid proof as user error.

PR #30 is a historical investigation, not a code repair. Preserve its revision
qualifiers; add current resolution links rather than rewrite old measurements
as current facts. Post verified progress to #29 at stable milestones. Close it
only when its agreed requirements are covered, or after an explicit documented
scope decision by the user. Do not close just because A/B pass. Review/merge the
documentation PR as a separate step and link the actual implementation commits.

## 6. Coordination and Regression Matrix

The two streams can progress independently on fixtures and diagnosis, but both
edit `synthesis.c`, source metadata, and image tests. Integrate changes one at a
time into the shared worktree; do not let concurrent writers overwrite them or
run a build while implementation files are being edited. Before changing shared
scope/arity logic, run both streams' focused tests.

| Test group | Required result |
|---|---|
| New Vec declaration | nil/cons synthesize, no explicit n at ordinary full call |
| Symbolic/dependent indices | Correct scoped projections; no numeric evaluation requirement |
| Invalid declarations | Tag.mark, cyclic/unsupported recovery, and shadowing mistakes fail during declaration checking |
| Unselected declaration | A selected unrelated member cannot conceal invalid Tag |
| Old explicit syntax | Acc, Vec and existing constructor/pattern arities retain meaning |
| Conflicting input indices | Rejected independently of `::` and request order |
| Partial/aliased callable | Synthesized signature and later application remain coherent |
| Effects in arguments | Same order/count before and after implicit elaboration |
| Match/IH | Hidden-index field mapping preserves recursive hypotheses |
| Graph properties | Collision/helper fixtures and new Vec graph/witness tests pass together |
| LE and sort proofs | Universal positive proofs plus minimal negative counterparts |
| Images | Save budgets 0/100/completed, inert resave, source/typed roots, chunks 1/64 and imports |
| Isolation | Different nominal declarations, Bool/Nat typed identities, alternate proofs and independently rejected roots remain distinct |

Use the existing `program_test`, `synthesis_test`, `iadt_test`, `source_io_test`
and shell runners. Do not create a second audit framework. Run focused debug
checks after each change, then optimized full `check-acceptance` and the affected
ASan/UBSan matrix. Confirm tests assert results and proof rejection, not merely
successful parsing. Record revision, flags, command, exit status and work counts.

## 7. Progress Sheet

Checkboxes mean implementation plus the stated verification, not just a design.

- [x] P0: Read current implementation, #29 and PR #30; record changed priority.
- [x] P1: Record constructor-index declaration rejection decision.
- [x] I1: Scoped index-name generalization and explicit kernel lowering.
- [x] I2: Declaration-time recovery validation and diagnostics.
- [x] I3: Constructor calls, aliases, quotation and partial-application contract.
- [x] I4: Match/IH mapping, generated graphs and image preservation.
- [x] S-PUSH: Verify and publish the completed IADT surface milestone to Main
  (`1f1f134`, 2026-09-18; publication record below).
- [x] G1a: Isolated graph collision and LE predecessor fixes in dirty worktree;
  targeted and optimized full acceptance passed before IADT changes.
- [x] G1b: Revalidate those fixes with new IADT elaboration; document alias policy.
- [x] G2a: Conventional LE transitivity, universal proof and invalid variants.
- [x] G2b: Comparator theorem related to conventional LE, not only Compared.
- [x] G3a: Universal insertion-sort Sorted proof tied to actual results
  (exact PR #30 provider; source/image/negative and sanitizer gates below).
- [x] G3b: Universal tree-sort Sorted proof tied to actual results
  (exact provider; full optimized and affected sanitizer gates below).
- [x] G3c: Universal reported merge-sort Sorted proof tied to actual results
  (ordinary fuel bounds and direct graph induction; verified gates below).
- [x] G3d: Universal QuickSort Sorted proof tied to actual results.
- [x] J1: Combined regression and sanitizer gates; no ignored failures.
- [x] J2: README and source examples updated to verified syntax, with limits.
- [x] J3: Issue requirement table, explanations, implementation references and
  justified issue/PR disposition. Pending items are not relabelled complete.
- [x] G-PUSH: Publish the verified G1/G2 improvement milestone to Main
  (`1590b2f`, merged report `5d9fa03`), followed by the separately verified
  G3 milestones and #29 closure recorded below. The earlier pending status is
  historical, not the current checklist state.
- [ ] R: Resume the parent duplicate-synthesis/authority refactor after J1-J3.
  The parent's A1-A3 contract audit at `58c9295` distinguishes completed
  allocation transport from remaining whole-job scans and structural consumers.
  Do not restart solved alias repairs or equate shared allocation with shared
  acceptance. The publication policy below still applies per completed epoch.

At each milestone, append a short dated entry containing changed files, tests,
remaining blockers and any justified plan adjustment. Report per-file added,
deleted and net lines separately for implementation/headers, tests, build rules
and documentation; include untracked files. Do not shrink tests to meet a LOC
target. Compare predecessor, length and imported QuickSort work/allocation costs
so implicit elaboration does not silently multiply solver work.

### 2026-09-18: Declaration and pattern foundation

- `syntax.c/.h`: constructor-local generalization of free header indices and
  their domain dependencies, with lexical shadowing. No new AST or Core tag.
  `tests/reader.c` covers member names, Lambda/Pi/pattern/block shadowing,
  nested declarations, unused indices and dependent header domains.
- `synthesis.c`: constructor telescopes use that expansion. Direct index
  projections are compiled from checked field classifiers, not source spelling
  or expected results. Unrecoverable declarations reject before nominal exports.
  Declaration members retain the source-field correspondence and projection
  paths together; no second classifier solution table was added.
- Positional Match binds written fields only; implicit fields remain ordinary
  unnamed kernel bindings. An open Vec length function using `*tail` passes.
  Explicit old constructor fields keep their original pattern arity.
- Optimized `reader_test` and `synthesis_test` pass. New positive declaration
  fixture and three declaration-rejection fixtures pass `program_test`;
  source equality passes with chunks 1/64 (1721 steps).
- ASan/UBSan `reader_test` and `synthesis_test` also pass. Non-indexed
  declarations bypass the new free-index scan entirely.
- The complete optimized `tests/image_cli.sh` runner passes, including the new
  fixtures at save budgets 0/100/completed, inert resave and resumed checking.
  Log: `/tmp/a-program-iadt-surface-image-tests.log`. This runner also covers
  the existing issue #29 graph and predecessor repairs.
- I2 remains open for diagnostics and recovery coverage. I3 is not implemented:
  constructor callables still expose explicit kernel arguments. I4 is partial:
  hidden-field Match and declaration images work, but new constructor calls,
  aliases, graph/witness integration and their images must still be verified.
  No Surface completion claim, README change, commit or Main push yet.
- #29: a right-recursive transitivity experiment remained unsupported; a
  factored step helper currently rejects. The preserved standalone probe is
  `src/prototype/pointer/tests/le-transitivity-probe.p`, deliberately outside
  the passing acceptance list. See the issue report for the traced boundary.

### 2026-09-18: Constructor calls and partial application, in progress

- Baseline remains `3a3bf550b3e882ab650395fd612e0fa2526b45bc`; no commit or
  publication yet. These entries supersede the earlier "I3 is not implemented"
  status, but do not mark I3 complete.
- `synthesis.c` retains the immutable source calling convention on constructor
  callables. Recovered indices are ordinary checked arguments. Still-unresolved
  indices are abstracted using existing context extension, Lambda and Pi rules.
  No new Core kind, equality rule or classifier solution table was introduced.
- Recovery positions are relative to the family's index telescope, not absolute
  erased APP positions; captured parameters are not mistaken for indices.
- The expanded `inferred-index-declaration.p` tests direct calls, named partials,
  quoted aliases, open arguments and an explicit higher-order Pi wrapper. Its
  source equality passes at chunks 1/64 (5585 steps).
- `inferred-index-two-inputs.p` recovers two indices from different inputs,
  including a partial callable (2776 steps, chunks 1/64). A conflicting second
  input in `inferred-index-disagreement.p` rejects through ordinary checking.
- `inferred-index-effects.p` exposed loss of calling-convention provenance at
  a block result binder. The fix follows existing binding/producer input edges;
  it neither substitutes the computation for the bound value nor executes it
  again. Its handler trace now proves head/between/tail effects occur once in
  order (5424 steps, chunks 1/64). Lambda parameters retain explicit contracts.
- Before the block-binder fix, the complete optimized image CLI runner passed
  with direct/partial/two-index fixtures, including unfinished images and inert
  resaves. The final exact-state full acceptance/sanitizer results follow below
  when completed; earlier passes are not evidence for later edits.
- Confirmed remaining I3 failure:
  `tests/inferred-index-later-recovery-probe.p` rejects (2243 steps) although
  `D.mk one nil` has a supported direct recovery path from the second field.
  The per-argument implementation checks `one` against a still-abstract n too
  early. The probe is intentionally outside the passing acceptance manifest;
  it is not an expected rejection or a completed feature. Implement whole-spine
  recovery without feeding expected results back into synthesis.
- I2 diagnostics, I3 full-spine recovery, I4 graph/witness coverage, README and
  the Surface publication gate remain open. #29 and PR #30 remain open too;
  their remote state was rechecked without altering or closing them.

Exact-state verification for this entry (all commands exited 0):

```sh
make -f src/prototype/pointer/Makefile BUILD=/tmp/a-program-iadt-surface check-acceptance
make -j2 -f src/prototype/pointer/Makefile BUILD=/tmp/a-program-iadt-asan CFLAGS='-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' /tmp/a-program-iadt-asan/synthesis_test /tmp/a-program-iadt-asan/program_test
/tmp/a-program-iadt-asan/synthesis_test
/tmp/a-program-iadt-asan/program_test --equal src/prototype/pointer/tests/acceptance/inferred-index-declaration.p main expected
/tmp/a-program-iadt-asan/program_test --equal src/prototype/pointer/tests/acceptance/inferred-index-two-inputs.p main expected
/tmp/a-program-iadt-asan/program_test --equal src/prototype/pointer/tests/acceptance/inferred-index-effects.p main expected
/tmp/a-program-iadt-asan/program_test --reject src/prototype/pointer/tests/acceptance/inferred-index-disagreement.p
bash src/prototype/pointer/tests/image_cli.sh /tmp/a-program-iadt-surface/source_io_test /tmp/a-program-iadt-surface/pointer-check /tmp/a-program-iadt-asan/program_test
```

Logs: `/tmp/a-program-iadt-surface-acceptance.log`,
`/tmp/a-program-iadt-asan-synthesis.log`,
`/tmp/a-program-iadt-asan-image-tests.log`. The last runner uses optimized
writers and an ASan/UBSan reader/comparator; it is not an all-sanitized writer
test. The optimized full suite includes the existing graph-name collision and
LE predecessor positive/negative tests. Their source alias policy is unchanged:
owner-local `caseN` names take precedence, and a source constructor alias is
exported only when unique (see the #29 report). This completes G1b, not G2/G3
or the combined feature gate. The failing later-recovery probe remains recorded
above and is excluded from any claim of complete Surface acceptance.

### 2026-09-18: Whole-spine recovery and indexed motives

- The earlier later-recovery failure is fixed. The permanent fixture is now
  `tests/acceptance/inferred-index-later-recovery.p` (2760 steps, chunks 1/64).
  Its whole application gathers written arguments, recovers from the second
  field and checks the earlier field without altering a separately requested
  partial application. The invalid earlier-field and isolated dependent-partial
  variants are separate rejection tests.
- Recovery covers dependent indices and preserves effect order. The new
  dependent-index fixture passes (2945 steps); the effectful spine's trace
  passes (7405 steps). Wrong nominal families and conflicting indices reject.
- Generated graph fields now map written pattern names to actual checked field
  binders, including hidden indices. The length property fixture passes (9105
  steps). Indexed copy and its generated witness pass (5018 steps).
- IH demand discovery no longer treats a hidden index's raw Pi domain as the
  next written argument domain. Recursive motive candidates consider the whole
  index/scrutinee telescope, and still check all branches before acceptance.
- That broader motive search exposed an existing conversion over-normalization
  problem: `legacy-partition-property-wrong.p` remained pending at one million
  steps. Weak comparison already found `n` versus `succ n`, but its strong
  fallback normalized a shared recursive domain before reaching that mismatch.
- Strong comparison now descends structurally through rigid Lambda, variable,
  classifier, nominal-family/constructor and Return/Request heads. It still
  compares every child and normalizes reducible oracle applications. No new
  equality, Core tag or acceptance shortcut is introduced. Classifier rigidity
  belongs to its existing semantic owner; unknown heads remain conservative.
  Core tests cover rejection without unfolding a shared recursive domain and
  successful child/parent contraction beneath a rigid Pi. The historical
  partition counterexample now rejects in 80652 steps.
- Full optimized `check-acceptance` passed after this repair:
  `/tmp/a-program-iadt-rigid-acceptance.log`. ASan/UBSan `core_test`,
  `synthesis_test` and the focused graph/copy/later-recovery/dependent/effect
  tests also pass. This supersedes the failed publication-gate run below,
  not the remaining Surface requirements.
- Added `inferred-index-import.p`: imported direct/quoted/partial constructors,
  Match and IH work together. `compatibility.sh` now checks source-only,
  partial and completed images, inert resave and split resumed evaluation.
  Completed-image ASan/UBSan comparisons pass for both result roots.
- Remaining Surface work: declaration diagnostic reporting, final image/root
  coverage review, README and the exact final publication gates. S-PUSH is
  still unchecked; #29's universal proofs and the parent refactor remain open.

### 2026-09-18: Surface completion candidate

- I2 diagnostics read the retained recovery description; no extra mutable
  error/solution store or synthesis pass was added. Rejected declarations name
  their constructor and missing index and list supported paths for other
  indices. Unsupported inversion is not described as a logical contradiction.
  `cli.sh` checks missing/partially recoverable/unused-invalid declarations,
  absence of diagnostics before Solve, and identical diagnostics after reload.
- `inferred-index-import.p` and `compatibility.sh` cover imported callable
  conventions, hidden-field Match/IH and source/partial/completed image resaves.
  The complete compatibility script passed with optimized writers and an
  ASan/UBSan evaluator: `/tmp/a-program-iadt-rigid-compatibility-asan.log`.
- `source_io_test constructor-inputs` additionally saves accepted definitions
  without their containing module. Their source conventions survive. Separate
  bare-proof exports keep the explicit Pi contract and reject an omitted
  argument. This distinction is documented in I4; the initial test that
  expected source member names from a bare proof was invalid, not evidence for
  adding another source reconstruction path.
- README now presents the inferred `cons : A -> * n -> * (succ n)` form,
  its calls/patterns, declaration rejection rule and explicit compatibility.
- Final optimized `check-acceptance` passed, including all added diagnostics,
  import and root tests: `/tmp/a-program-iadt-surface-final-acceptance.log`.
  Final sanitizer confirmation and publication are still required before
  checking S-PUSH. No universal Sorted theorem is claimed by I1-I4.

## 8. Deferred Work and Completion Boundary

Do not implement the proposed `(@f).witness` surface replacement during these
streams; keep `@f`, global `*f`, and IH `*k` stable while their proof consumers
are verified. Generic pending-structure reconstruction removal, declaration
allocation cleanup and broad scheduler changes resume afterward unless a
specific failing requirement proves a narrowly scoped prerequisite necessary.

### Publication policy (user decision, 2026-09-18)

Reconfirmed on 2026-09-19: Surface, #29/#30 improvements, and substantial
refactoring epochs are separate Main publication milestones. Local commits
are not publication gates; incomplete or failing work stays unpublished.

Publish coherent milestones rather than wait for the whole refactor:

1. IADT surface: after I1-I4 and their regression gates pass, update the README,
   commit the completed surface change and push Main. Do not wait for G2-G3.
2. Issue #29 / PR #30: after a coherent, explicitly documented improvement is
   verified, commit and push Main. State which requirements remain; publication
   alone does not authorize closing #29 or claiming all four theorems complete.
3. Refactoring: define each substantial epoch's scope and acceptance criteria
   before implementation. At its completion, verify and push Main, then begin
   the next epoch. Do not publish arbitrary intermediate edits as an epoch.

For every push, run the existing full optimized acceptance suite and the focused
new positive/negative tests on the exact candidate revision, plus affected
debug, sanitizer and image checks. Include already integrated changes from the
other stream in regression testing. Record commands, results, commit, published
Main revision, outstanding work and per-file LOC changes. Recheck remote Main
before integration; do not force-push or overwrite unrelated work. Shared-file
changes must form a coherent tested commit, not a selected untested patch.

This supersedes the earlier all-work-complete publication restriction. Overall
parent completion criteria, including its net-negative refactor gate, remain
separate from milestone publication. An epoch push is not a claim that the whole
refactor is finished; do not waive remaining cleanup by counting feature/test
lines differently.

Earlier publication gate failure (2026-09-18): the optimized `check-acceptance`
run exited 2 at `check-source-compatibility`. Its initial source/result matrix
reported 63/63 passed, but that is only the first part of `compatibility.sh`;
the subsequent checks had not all passed. The failing partition counterexample
and its repair are recorded in the whole-spine entry above. Log:
`/tmp/a-program-iadt-spine-acceptance.log` (local, temporary evidence).
At that failed gate S-PUSH remained unchecked; the successful publication below
supersedes this historical status.
Do not relax existing expectations merely to open this publication gate.

### Surface validation and change accounting

Publication: commit `1f1f134` was pushed atomically to `origin/main` and
`origin/rewrite/pointer-core-hott` on 2026-09-18, without force. The old Main
remains archived by `old-version/2026-09-14-main`. Both remote branches accepted
the fast-forward. This record is a documentation-only follow-up; tested source
and tests are unchanged. The feature commit contains 5187 additions and 764
deletions across 60 files, including its documentation/accounting section.
Issue #29 and PR #30 remain open: G2/G3 and the broader refactor are not complete.
The next issue milestone must discharge conventional LE/comparator and actual
sort-result property obligations without new theorem-specific kernel shortcuts.

The final optimized acceptance gate and final ASan/UBSan image CLI both passed
(exit 0). The latter used sanitized source writer, checker and evaluator binaries.
Sanitized synthesis, constructor-root exports and CLI diagnostic tests also passed.
No source edits followed these final builds. Local logs:

- `/tmp/a-program-iadt-surface-final-acceptance.log`
- `/tmp/a-program-iadt-final-asan-images.log`
- `/tmp/a-program-iadt-final-asan-synthesis.log`
- `/tmp/a-program-iadt-final-asan-roots.log`
- `/tmp/a-program-iadt-final-asan-cli.log`

The following snapshot is relative to R76 (`3a3bf550`), before adding this
accounting section. It includes accumulated typed-evidence prerequisites and
G1 repairs, not just the IADT surface. This is a feature milestone, not the
parent refactor's net-negative completion. The open LE transitivity probe is
an investigation fixture, explicitly excluded from passing acceptance claims.

| Category | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| Implementation | 1488 | 607 | 881 |
| Tests and investigation fixtures | 1448 | 145 | 1303 |
| Build | 27 | 1 | 26 |
| Documentation | 2131 | 11 | 2120 |
| Total before this accounting section | 5094 | 764 | 4330 |

Per-file counts exclude this accounting/publication record itself. Final commit
counts are available with `git show --numstat`.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `README.md` | 14 | 1 | 13 |
| `doc/2026-08-13T19-33-00-EXPLICIT-INDEX-FAMILY-SURFACE-AND-ACC-IMPLEMENTATION-PLAN.md` | 6 | 0 | 6 |
| `doc/2026-09-16-TYPED-STRUCTURE-AND-EVIDENCE-REFACTOR-PLAN.md` | 23 | 10 | 13 |
| `doc/2026-09-17-TYPED-DATA-AUTHORITY-REAUDIT-PLAN.md` | 1156 | 0 | 1156 |
| `doc/2026-09-18-CONSTRUCTOR-INDEX-SYNTHESIS-DECISION.md` | 62 | 0 | 62 |
| `doc/2026-09-18-IADT-SURFACE-AND-ISSUE-29-30-PRIORITY-PLAN.md` | 542 | 0 | 542 |
| `doc/2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md` | 179 | 0 | 179 |
| `doc/2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md` | 149 | 0 | 149 |
| `src/prototype/pointer/Makefile` | 27 | 1 | 26 |
| `src/prototype/pointer/classifier.c` | 10 | 0 | 10 |
| `src/prototype/pointer/classifier.h` | 2 | 0 | 2 |
| `src/prototype/pointer/conversion.c` | 20 | 0 | 20 |
| `src/prototype/pointer/derivation.c` | 1 | 1 | 0 |
| `src/prototype/pointer/evidence.c` | 39 | 34 | 5 |
| `src/prototype/pointer/evidence.h` | 22 | 1 | 21 |
| `src/prototype/pointer/function_graph.c` | 7 | 3 | 4 |
| `src/prototype/pointer/main.c` | 18 | 0 | 18 |
| `src/prototype/pointer/source_io.c` | 261 | 61 | 200 |
| `src/prototype/pointer/source_io.h` | 19 | 8 | 11 |
| `src/prototype/pointer/syntax.c` | 125 | 0 | 125 |
| `src/prototype/pointer/syntax.h` | 7 | 0 | 7 |
| `src/prototype/pointer/synthesis.c` | 911 | 476 | 435 |
| `src/prototype/pointer/synthesis.h` | 46 | 23 | 23 |
| `src/prototype/pointer/tests/acceptance/function-graph-branch-name-collision.p` | 20 | 0 | 20 |
| `src/prototype/pointer/tests/acceptance/graph-canonical-leaf-name.p` | 13 | 0 | 13 |
| `src/prototype/pointer/tests/acceptance/graph-comparison-leaves.p` | 34 | 0 | 34 |
| `src/prototype/pointer/tests/acceptance/graph-duplicate-leaf-ambiguous.p` | 5 | 0 | 5 |
| `src/prototype/pointer/tests/acceptance/graph-duplicate-leaf-wrong.p` | 5 | 0 | 5 |
| `src/prototype/pointer/tests/acceptance/graph-duplicate-leaf.p` | 31 | 0 | 31 |
| `src/prototype/pointer/tests/acceptance/graph-helper-leaf.p` | 14 | 0 | 14 |
| `src/prototype/pointer/tests/acceptance/inferred-index-copy.p` | 11 | 0 | 11 |
| `src/prototype/pointer/tests/acceptance/inferred-index-declaration.p` | 21 | 0 | 21 |
| `src/prototype/pointer/tests/acceptance/inferred-index-dependent.p` | 10 | 0 | 10 |
| `src/prototype/pointer/tests/acceptance/inferred-index-disagreement.p` | 9 | 0 | 9 |
| `src/prototype/pointer/tests/acceptance/inferred-index-earlier-wrong.p` | 5 | 0 | 5 |
| `src/prototype/pointer/tests/acceptance/inferred-index-effects.p` | 25 | 0 | 25 |
| `src/prototype/pointer/tests/acceptance/inferred-index-graph.p` | 26 | 0 | 26 |
| `src/prototype/pointer/tests/acceptance/inferred-index-import.p` | 21 | 0 | 21 |
| `src/prototype/pointer/tests/acceptance/inferred-index-later-recovery.p` | 12 | 0 | 12 |
| `src/prototype/pointer/tests/acceptance/inferred-index-no-inverse.p` | 3 | 0 | 3 |
| `src/prototype/pointer/tests/acceptance/inferred-index-partial-dependent.p` | 8 | 0 | 8 |
| `src/prototype/pointer/tests/acceptance/inferred-index-partially-recoverable.p` | 4 | 0 | 4 |
| `src/prototype/pointer/tests/acceptance/inferred-index-two-inputs.p` | 13 | 0 | 13 |
| `src/prototype/pointer/tests/acceptance/inferred-index-unrecoverable.p` | 3 | 0 | 3 |
| `src/prototype/pointer/tests/acceptance/inferred-index-unselected-invalid.p` | 5 | 0 | 5 |
| `src/prototype/pointer/tests/acceptance/inferred-index-wrong-family.p` | 5 | 0 | 5 |
| `src/prototype/pointer/tests/acceptance/le-predecessor-invalid.p` | 9 | 0 | 9 |
| `src/prototype/pointer/tests/acceptance/le-predecessor-wrong.p` | 9 | 0 | 9 |
| `src/prototype/pointer/tests/acceptance/le-predecessor.p` | 13 | 0 | 13 |
| `src/prototype/pointer/tests/cli.sh` | 20 | 0 | 20 |
| `src/prototype/pointer/tests/compatibility.sh` | 13 | 0 | 13 |
| `src/prototype/pointer/tests/core.c` | 30 | 0 | 30 |
| `src/prototype/pointer/tests/iadt.c` | 15 | 3 | 12 |
| `src/prototype/pointer/tests/image_cli.sh` | 14 | 1 | 13 |
| `src/prototype/pointer/tests/le-transitivity-probe.p` | 18 | 0 | 18 |
| `src/prototype/pointer/tests/reader.c` | 50 | 0 | 50 |
| `src/prototype/pointer/tests/seed.c` | 1 | 1 | 0 |
| `src/prototype/pointer/tests/source_io.c` | 798 | 117 | 681 |
| `src/prototype/pointer/tests/source_io.sh` | 2 | 1 | 1 |
| `src/prototype/pointer/tests/synthesis.c` | 153 | 22 | 131 |

### 2026-09-18: G2 verified milestone

Published: implementation `1590b2f` and documentation-only PR #30 merge
`5d9fa03` are on Main and the default rewrite branch (fast-forward, no force).
GitHub confirms #30 is merged. #29 remains open for G3's universal sort proofs.
The report's embedded provider hash was independently verified as
`b859e517843b40a9448e26e28ab9d26f926e4485d6e1f62a6ef763d95aa3639c`.
No source or test changes occurred between final verification and publication.

The scope diagnosis and exact tests are in the
[issue report](2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md#2026-09-18-follow-up-retaining-independent-later-fields).
The sole implementation change is a checked scope map in `synthesis.c` that
retains independent later declarations while varying an earlier index. Core,
Identity rules and artifact schema are unchanged. The original two-constructor
LE now admits an ordinary universal transitivity proof. Graph induction proves
that the comparator's true result carries LE and its false result carries
strict reverse order, including equal/duplicate inputs on the true side.

Verification: full optimized `check-acceptance`, focused debug checks,
ASan/UBSan synthesis and source-origin/root checks, and the complete all-sanitized
image CLI passed. Logs are `/tmp/a-program-g2-final-acceptance.log` and
`/tmp/a-program-g2-asan-images.log`. G3a-G3d remain open; finite result checks
are consumers of G2's open-variable proofs, not substitutes for sort proofs.

PR #30 at `bd315b0` was reviewed again: its branch adds only the historical
710-line report. Its two diagnosed defects have reproducible repairs; its
universal-proof caveats are correct. Preserve the report unchanged on merge,
link this resolution separately, and leave #29 open. The reported merge
algorithm is insertion-based; the G3 tests must not replace it by another sort.

Change counts below compare with `d5a02de`, before this progress/accounting
entry and final validation wording. Implementation: +39/-4 (net +35);
existing build runner: +11/-0; tests: +107/-2 (net +105, including a recognized
fixture rename); issue report: +64/-0. This is feature repair, not completion
of the net-negative authority cleanup. PR documentation adds 710 lines separately.

| File | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `doc/2026-09-18-ISSUE-29-GRAPH-EXPORT-AND-INDEX-TRANSPORT.md` | 64 | 0 | 64 |
| `src/prototype/pointer/Makefile` | 11 | 0 | 11 |
| `src/prototype/pointer/synthesis.c` | 39 | 4 | 35 |
| `src/prototype/pointer/tests/acceptance/comparator-order-wrong.p` | 24 | 0 | 24 |
| `src/prototype/pointer/tests/acceptance/comparator-order.p` | 38 | 0 | 38 |
| `src/prototype/pointer/tests/acceptance/indexed-later-scope-wrong.p` | 8 | 0 | 8 |
| `src/prototype/pointer/tests/acceptance/indexed-later-scope.p` | 14 | 0 | 14 |
| `src/prototype/pointer/tests/acceptance/le-transitivity-wrong.p` | 13 | 0 | 13 |
| `src/prototype/pointer/tests/{le-transitivity-probe.p => acceptance/le-transitivity.p}` | 6 | 1 | 5 |
| `src/prototype/pointer/tests/image_cli.sh` | 4 | 1 | 3 |

### 2026-09-18: G3 insertion proof and remaining helper boundary

G3a remains open. The current worktree admits ordinary universal
`insert_bound` and `insert_sorted` terms over the exact PR #30 `insertNat`.
The historical provider is now `tests/fixtures/sorted-proof-provider.p`, SHA256
`b859e517843b40a9448e26e28ab9d26f926e4485d6e1f62a6ef763d95aa3639c`;
no algorithm or intrinsic spelling was changed. Its legacy namespace spelling
is explicitly enabled in the test, not accepted by default.

Two implementation gaps were isolated:

- `pg_function_graph_source` stopped at typed partial applications and at
  variables whose checked substitution image was a known callable. Follow
  those existing typed edges, lift the mapped Lambda binder, and reuse the
  ordinary graph owner. An unknown callable still uses the existing parameter
  graph. Pure weakening must not allocate a different source.
- `pattern_index_type` generalized Pi results but not their domains. For
  example, matching `n` and returning `Box n -> Nat` failed when each branch
  annotated its own constructor index. Recurse into the domain too. When its
  generalized type changes, only a checked independent codomain may be
  weakened into the new binder scope; otherwise this candidate fails.
  Reinstantiating the complete candidate must still reproduce the original
  branch type. This is not a new equality or an expected-type inference rule.

The independent `indexed-motive-domain.p` regression is unsupported on the
published G2 binary (1,246 steps), accepted by the worktree (1,645 steps), and
its wrong-index sibling is rejected (1,384 steps). The insertion theorem's
execution consumer normalizes through the constructed Sorted evidence to three
successors. The completed gates are recorded below.

The next obstacle is distinct from motive inference: `@insertionSort` retains
a helper witness for the generic `@insertBy` instantiated at Nat/comparator,
whereas the new theorem consumes `@insertNat`, generated after typed partial
application. Those are distinct nominal graph declarations. The direct
consumer below is rejected, correctly refusing to identify
the two witnesses. Before G3a can close, provide a checked relationship or a
coherent shared graph construction that preserves both public contracts.
Do not merge declarations by Core/WHNF equality or replace the provider.
This also matters for merge sort and QuickSort's generic helpers.

- [x] Extract and hash the exact provider; construct universal insertion lemmas.
- [x] Diagnose partial-source and Pi-domain synthesis gaps with independent input.
- [x] Verify all added source/image/negative tests and the existing full suite.
- [x] Consume the generic helper graph through its own checked theorem instead
  of identifying it with the specialized graph (G3a entry below).
- [x] Complete the insertion-sort theorem and actual execution consumers (G3a).
- [ ] Complete G3b-G3d, then J1/J3; #29 stays open.

Remaining connection probe (with provider and insertion-property definitions
available through imports; rejected at 139,757 steps with a one-million budget):

```a-program
import Nat;
import List;
import insertionSort;
import Sorted;
import insert_sorted;
sort_correct := \xs:List Nat => \ys:List Nat => \graph:@insertionSort xs ys => graph
	@case0 => Sorted.nil
	@case1 head tail sorted rest result inserted => insert_sorted head sorted result inserted *rest;
sort_correct :: (xs:List Nat)->(ys:List Nat)->@insertionSort xs ys->Sorted ys;
```

Verification completed on the worktree:

- Full optimized `check-acceptance`: `/tmp/a-program-g3-final-acceptance.log`.
- ASan/UBSan `program_test`, including repeated source lookup with no further
  proof/occurrence allocation: `/tmp/a-program-g3-asan-program.log`.
- All-sanitized image CLI, including new positive and negative source fixtures:
  `/tmp/a-program-g3-asan-images.log`.
- Exact-provider insertion theorem consumers, completed and partial images,
  inert resaves and a rejected wrong-index theorem, both optimized and
  sanitized: `/tmp/a-program-g3-sort.log`, `/tmp/a-program-g3-asan-sort.log`.
  The packet's result and the original `insertNat` execution both normalize to
  `[zero, one, two]`. Sorted evidence is separately eliminated in empty,
  middle, duplicate and after-tail insertion cases. Solve chunks 1 and 64 agree.

Two initial gate failures were corrected explicitly: the new source test needed
an explicit budget above the CLI default; an old C assertion required weakened
function sources to be unsupported. The replacement checks the exact original
source identity and stable repeated lookup, not merely acceptance.

GitHub #29 was found closed immediately after PR #30's documentation merge,
despite the progress comment saying it remained open. It was reopened with an
explanation; the remaining universal proof requirements are unchanged.

Changes relative to `e29b7c6`, excluding this progress document: implementation
+71/-15 (net +56), tests and fixtures +518/-2 (net +516), prototype build runner
+10/-2 (net +8). The 302-line provider is the verbatim historical test input,
not added compiler machinery. This feature repair is not the later net-negative
authority refactor.

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 22 | 8 | 14 |
| `function_graph.c` | 49 | 7 | 42 |
| `Makefile` | 10 | 2 | 8 |
| `tests/program.c` | 5 | 1 | 4 |
| `tests/image_cli.sh` | 3 | 1 | 2 |
| `tests/sort_insertion.sh` | 37 | 0 | 37 |
| `tests/fixtures/sorted-proof-provider.p` | 302 | 0 | 302 |
| `tests/acceptance/function-graph-partial-source.p` | 23 | 0 | 23 |
| `tests/acceptance/function-graph-partial-source-wrong.p` | 8 | 0 | 8 |
| `tests/acceptance/indexed-motive-domain.p` | 9 | 0 | 9 |
| `tests/acceptance/indexed-motive-domain-wrong.p` | 6 | 0 | 6 |
| `tests/acceptance/sort-insertion-property.p` | 118 | 0 | 118 |
| `tests/acceptance/sort-insertion-property-wrong.p` | 7 | 0 | 7 |

### 2026-09-18: G3a neutral conversion and checked index factoring

The direct comparator investigation has become the acceptance fixture
`src/prototype/pointer/tests/acceptance/sort-insertion-sort-property.p`.
Its imports require the exact provider plus the previously checked insertion
lemmas. Reproduce without changing either algorithm:

```sh
sed '/^import /d' \
  src/prototype/pointer/tests/fixtures/sorted-proof-provider.p \
  src/prototype/pointer/tests/acceptance/sort-insertion-property.p \
  > /tmp/a-program-g3-proof-provider.p
pointer-check --legacy-intrinsic-dot --steps 1000000 \
  --imports /tmp/a-program-g3-proof-provider.p \
  src/prototype/pointer/tests/acceptance/sort-insertion-sort-property.p
```

The first 49 lines establish an ordinary
`Direct x y` wrapper whose field is `Decision x y (natLessOrEqual x y)`, and
lower-bound preservation for the generic `@insertBy` graph. No nominal graphs
are identified, and no expected type is used to choose a motive.

Previously the full probe remained pending after one million steps. Conversion
was strongly normalizing unselected recursive branches of a neutral Match.
The independent `open-recursive-conversion-wrong.p` reproduces this without any
sorting: a constant recursive Bool function is not definitionally its result
at an open Nat variable. Baseline `006a969` remains pending at 100,000 steps;
the correction rejects in 1,083 steps. The closed-input sibling is accepted
in 1,422 steps.

The correction uses the shared WHNF result before requesting NF, and recognizes
neutral binder-headed operands under Match, Force and total-result projection.
Congruence still compares their children. It neither adds a computation rule nor
reflects Identity into DefEq. Fold is deliberately excluded: reducing its
continuation can expose the right-unit rule even with an open first operand.
Unit tests cover that boundary, reducible scrutinees/branches, divergent unused
branches, and Solve chunk sizes 1, 7 and 64. Source/image tests cover both the
closed valid case and rejection after resuming an unfinished image.

The conversion correction first made the sorting probe reject in 181,705 steps,
rather than consuming the budget indefinitely. This exposed a separate gap:
`pg_prove_pattern_type` abstracts whole index images by structural alpha
comparison. The callback trace exposes an unreduced invocation while the
Decision field contains an unfolded invocation. The attempted family remains
constant in that index; its checked transport therefore cannot change the Bool
result. The existing index-transport search already normalizes both endpoints.
It now also tries those checked WHNF images when factoring the family. The
transport itself retains its ORIGINAL endpoints and Identity path, and checks
the value's source type by ordinary conversion. No equality is assumed from a
candidate, and no new proof rule, Core tag or authority table is introduced.

The independent `indexed-normalized-transport.p` fails on `006a969` (3,498
steps) and passes after the change (4,002 steps). Its wrong-target sibling is
rejected (3,348 steps). This is a bounded additional factoring candidate, not
general higher-order unification or a promise to recognize every convertible
presentation of an index. In particular both endpoints and arbitrary type
subexpressions need not already share WHNF syntax.

The source proof extracts LE in the callback's own branch context, then returns
that evidence through Match. It no longer attempts to change the callback
argument index and Bool result simultaneously. The ordinary theorem is:

```a-program
sort_correct :: (xs:List Nat)->(ys:List Nat)->@insertionSort xs ys->Sorted ys;
```

The complete fixture is accepted in 197,783 steps. `tests/sort_insertion.sh`
checks actual `*insertionSort` packets for empty, singleton, duplicate-containing
and already-sorted inputs, recursively consumes Sorted evidence, and compares
packet/direct results with the expected list. It verifies Solve chunks 1/64,
unfinished/completed images and inert resaves. A claim that the INPUT list is
sorted is rejected (198,298 steps). No algorithm in the frozen provider changed.
An initial test-runner failure was a missing exported expected singleton value;
the fixture now explicitly exports it instead of assuming imports re-export.

- [x] Independent reproduction and bounded conversion fix.
- [x] Conversion-only full optimized and sanitized image gates.
- [x] Resolve this checked index factoring case without changing the kernel rules.
- [x] Universal insertionSort theorem and source/image/negative consumers
      (`/tmp/a-program-g3-factor-sort.log`).
- [x] Rerun full optimized acceptance on both changes
      (`/tmp/a-program-g3a-final-acceptance.log`).
- [x] ASan/UBSan synthesis, program and actual sort proof/image/negative tests
      (`/tmp/a-program-g3a-asan-{synthesis,program,sort}.log`).
- [x] Complete the full sanitized image runner (`/tmp/a-program-g3a-asan-images.log`).
- [x] G3a publication gates; G3b-G3d and the final J1/J3 obligations remain open.

The Surface and G1/G2 publications stand. This correction does not close #29
and does not authorize starting the deferred broad authority refactor.

Changes against `006a969`, excluding this progress document: implementation
+62/-25 (net +37); tests and proof fixtures +195/-1 (net +194).

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `conversion.c` | 32 | 15 | 17 |
| `synthesis.c` | 30 | 10 | 20 |
| `tests/synthesis.c` | 50 | 0 | 50 |
| `tests/image_cli.sh` | 3 | 1 | 2 |
| `tests/sort_insertion.sh` | 23 | 0 | 23 |
| `tests/acceptance/open-recursive-conversion.p` | 6 | 0 | 6 |
| `tests/acceptance/open-recursive-conversion-wrong.p` | 6 | 0 | 6 |
| `tests/acceptance/indexed-normalized-transport.p` | 11 | 0 | 11 |
| `tests/acceptance/indexed-normalized-transport-wrong.p` | 9 | 0 | 9 |

## G3b: Tree-Sort Proof (2026-09-18)

`sort-tree-property.p` proves, for the unchanged PR #30 provider:

```a-program
tree_correct :: (xs:List Nat)->(ys:List Nat)->
    @treeSort Nat (&natLessOrEqual) xs ys->Sorted ys;
```

Ordinary IADTs express upper/lower bounds and ordered trees. Graph induction
proves insertion preserves these invariants, building creates an ordered tree,
and traversal produces a Sorted list. Append lemmas preserve list bounds and
sortedness across a separating pivot. No tree-sort rule, alternate algorithm,
finite enumeration of inputs, or order axiom is added to the kernel.

The first append proof exposed an independent Pi-result extraction gap. After
generalizing an earlier domain, selecting a later independent codomain through
an unnecessary bound argument could lose the retained nominal formation.
`pattern_index_type` now prefers checked constant-codomain projection when
independence holds. Dependent codomains still require the unchanged domain and
ordinary typed substitution. Reinstantiation of the candidate is still checked.
This reorders existing rules; it adds no conversion equation, inference from
`::`, replacement of raw binders, or copied typing authority. It does not claim
to solve general dependent-domain inversion or every typed-selection problem.

The standalone `indexed-independent-pi-motive.p` reproduces the problem without
ordering or sorting: baseline `ffda902` is unsupported in 8,068 steps; the fixed
implementation accepts its unannotated recursive definition and checks its
separate type expectation in 11,091 steps. The whole tree proof is accepted in 403,465
steps. Actual execution consumers include empty, singleton, duplicate, mixed,
ascending and descending inputs. A negative consumer falsely ascribes the
output's Sorted evidence to the arbitrary input.

An initial runner failure was its 1,000,000-step comparison budget: consuming
the ascending four-element proof takes approximately 1,082,000 steps. The test
binary now accepts an explicit comparison budget; only the completed tree
comparisons request 2,000,000. The default remains unchanged. Inputs and proof
consumption are not reduced to make the test pass.

- [x] Universal source theorem and isolated baseline reproduction.
- [x] Extended source/image/negative tree runner, chunks 1/64 and inert resaves.
- [x] Full optimized acceptance (`/tmp/a-program-g3b-acceptance.log`, exit 0).
- [x] ASan/UBSan synthesis, program and extended sort runner
      (`/tmp/a-program-g3b-asan-{synthesis,program,sort}.log`, all exit 0).
- [x] Full sanitized image runner (`/tmp/a-program-g3b-asan-images.log`, exit 0).
- [x] Record final counts and pass the Main publication gates.

Verification uses `make -f src/prototype/pointer/Makefile -j2
BUILD=/tmp/a-program-g3-tree check-acceptance` with default `-O2 -Werror`.
The sanitizer binaries use `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -no-pie`, with `ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1`. All four sanitized runs above exit 0.
G3b was published atomically to Main and the active rewrite branch in
`271c643`; #29 remains open with the remaining obligations stated.

G3c/G3d and final J1/J3 remain open. No issue closure or broad authority refactor
is authorized by this intermediate result.

The optimized image comparisons consume 1,082,155 steps for the ascending input
and 976,328 for the descending input, identically at chunk sizes 1 and 64.
The invalid input-index theorem is rejected in 404,253 steps. Explicit test
budget 1 remains a failure, and budget 0 is rejected as invalid; increasing a
budget does not change acceptance or equality criteria.

Changes against `ffda902`, excluding this document: implementation +6/-6 (net
0), tests/proofs +268/-2 (net +266). There is no new implementation module.

| File under `src/prototype/pointer/` | Added | Removed | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 6 | 6 | 0 |
| `tests/program.c` | 16 | 2 | 14 |
| `tests/image_cli.sh` | 1 | 0 | 1 |
| `tests/sort_insertion.sh` | 24 | 0 | 24 |
| `tests/acceptance/indexed-independent-pi-motive.p` | 21 | 0 | 21 |
| `tests/acceptance/sort-tree-property.p` | 197 | 0 | 197 |
| `tests/acceptance/sort-tree-property-wrong.p` | 9 | 0 | 9 |

## G3c: Fuelled Merge-Sort Proof (2026-09-18)

`sort-merge-property.p` proves for the unchanged PR #30 implementation:

```a-program
fuel_correct :: (fuel:Nat)->(xs:List Nat)->(ys:List Nat)->
    @mergeSortFuel (&natLessOrEqual) fuel xs ys->Fits fuel xs->Sorted ys;
merge_correct :: (xs:List Nat)->(ys:List Nat)->
    @mergeSort (&natLessOrEqual) xs ys->Sorted ys;
```

`Fits fuel xs` is an ordinary IADT expressing a list-length upper bound, not
a built-in termination predicate. Induction uses `@splitAlternating` directly;
`FitsPair` bounds both outputs. On a nonempty input the right output
fits the predecessor fuel. `MeasurementFits` relates the actual `measure`
packet to this bound. The zero-fuel Sorted proof uses that `Fits Nat.zero xs`
forces an empty input. The program itself still returns any supplied input at
zero fuel; it is not assumed to sort arbitrary lists.

The reported `mergeBy` inserts every left element into the right list. Its
result is therefore sorted whenever the right input is sorted, without an
assumption on the left input. This property suffices for graph induction on
the exact fuelled program. Neither a conventional linear merge algorithm nor
a different sorting program is substituted for the reported implementation.

All these definitions use existing source IADTs, Match/IH, typed graph evidence
and postchecking. There is no kernel, solver or artifact-format change. The
source proof for four mixed elements can be fully consumed in 634,043 steps.
The shared tree/merge runner checks empty, singleton, duplicates, ascending and
descending inputs, execution-packet/direct-result agreement, chunks 1/64 and
unfinished/completed images. Independent negatives cover an incorrect input
Sorted claim and a forged zero-fuel bound for a nonempty unsorted input,
including rejection after inert resaving and resuming budgets 0/100.

An initial `SplitOf` relation merely copied the generated graph's structure.
It and its conversion proof were removed before publication. Direct graph
induction saves ten source lines and lowers this concrete proof-consumption
work from 644,108 to 634,043 steps, with the same theorem and no kernel change.

- [x] Universal theorem and actual source execution-proof consumption.
- [x] Final full optimized acceptance, including invalid image resumptions
      (`/tmp/a-program-g3c-final-acceptance.log`, exit 0).
- [x] ASan/UBSan shared sort runner; no implementation files changed
      (`/tmp/a-program-g3c-final-asan-sort.log`, exit 0).
- [x] Record verified results and pass G3c publication gates.

The optimized command is the same `check-acceptance` command recorded for G3b.
The sanitizer run uses those unchanged implementation binaries with leak
checking and halt-on-undefined-behavior enabled. G3c is ready for Main
publication; #29 remains open for G3d/J1/J3. This does not start the deferred
authority refactor or claim permutation/stability/complexity theorems.

Against `271c643`, excluding this document: implementation +0/-0;
tests/proofs +158/-20 (net +138). Per file: `sort_insertion.sh` +36/-20,
`sort-merge-property.p` +103/-0, `sort-merge-property-wrong.p` +9/-0,
`sort-merge-fuel-wrong.p` +10/-0. The common runner avoids a second copy of
the same image/normalization checks.

Final optimized counts: source module 281,264 steps; mixed/ascending/descending
image proof consumption 634,349 / 591,112 / 645,101, each identical at chunks
1/64. The wrong fuel bound is rejected at 283,190 steps (283,495 after image
resumption), and the wrong input-index theorem at 282,021 (282,326 resumed).
| `tests/acceptance/sort-insertion-sort-property.p` | 79 | 0 | 79 |
| `tests/acceptance/sort-insertion-sort-property-wrong.p` | 8 | 0 | 8 |

### 2026-09-18: G3d indexed-field transport boundary

G3c was published as `794ec6a` to Main and the active rewrite branch. G3d
remains open; `sort-quick-property.p` is an unfinished working proof, not an
acceptance claim. The frozen provider is unchanged.

The first QuickSort helper exposed two different problems:

- The draft passed `&P` where `P : Nat -> @` was already a suspended value.
  A minimal `Box P` example accepts `P` and rejects the extra thunk. This was
  a source error, fixed without changing coercion policy.
- Extracting a proof of `P head` from an indexed `SizedAll` constructor was
  unsupported even after that correction. Its Match has paths for both size
  and the sized-list value. `constructor_transport_step` constructed its
  transport family over the left endpoint's fixed fiber, so the right endpoint
  could not be paired into that context. This is a real missing synthesis case.

The repair lifts the complete existing Identity boundary telescope into the
current context, then extends it with the compared value. Left/right maps and
all prefix paths feed the existing family-transport checker. Original endpoint
terms, nominal schema field binders and the final result check are preserved.
No new Core tag, Identity rule, trusted cast, UIP, expected-type inference or
QuickSort-specific rule is introduced.

Independent regression `indexed-dependent-field-path.p` states extraction for
arbitrary `P`, not just a closed numeric result. The initial one-prefix case was
unsupported on baseline `794ec6a` at 5,222 steps and accepted at 5,852 after the
repair. The final fixture also covers a dependent prefix `(n, xs : Vec n)`:
baseline is unsupported at 9,036 steps; the repair accepts it at 11,367. Actual
dependent-prefix result comparison passes at chunks 1/64, both 12,449 steps.
Wrong input index and using
the data field itself as arbitrary `P` evidence are rejected. The fixtures are
registered in the existing source/image runners. Full gates are still pending
at this entry; do not infer publication readiness from this focused result.

Still required before G3d completion:

- [x] Reproduce independent field extraction and preserve the boundary's paths.
- [x] Verify the changed shared transport with full optimized/image/sanitizer gates.
- [ ] Handle a field whose own type depends on earlier constructor fields, or
  construct the same universal partition/sort theorem without requiring that
  inversion. `sized_rest`, `part_left` and `part_right` remain unsupported.
- [ ] Complete generic property preservation, partition order, recursive
  QuickSort order and actual execution-packet checks for the exact provider.
- [x] Publish only after the G3d milestone and its stated gates are complete.

For dependent fields, do not turn `tail : SizedList k` into `SizedList n` by
retagging it, nor assume the size path is reflexivity. The remaining route must
retain the preceding size path and use a dependent family transport (potentially
over the whole constructor telescope). Independent-field extraction alone does
not establish this case. An alternative recursive type predicate was tried:
it moved the difficulty into type normalization/motive synthesis and did not
complete the theorem. That trial was removed; it is not counted as a solution.

The full ASan/UBSan image runner passed in
`/tmp/a-program-g3d-asan-images.log`, as did `synthesis_test` and `program_test`
in the corresponding `-asan-synthesis.log` and `-asan-program.log`. The later
dependent-prefix extension also passed focused source/image comparisons under
the same sanitizer binaries: save budgets 0/100/completed, byte-identical inert
resave and both exported results at chunks 1/64. Loading a completed source
image at zero Solve steps reports `pending`, as in the existing image runner;
it is not interpreted as proof failure or as already-accepted evidence.

Final working-tree gates (all exit 0; no publication yet):

| Gate | Record |
|---|---|
| Full optimized `check-acceptance`, final implementation and fixtures | `/tmp/a-program-g3d-final-acceptance.log` |
| ASan/UBSan full source/image runner | `/tmp/a-program-g3d-asan-images.log` |
| ASan/UBSan shared sort runner | `/tmp/a-program-g3d-asan-sort.log` |
| ASan/UBSan synthesis and program unit suites | `/tmp/a-program-g3d-asan-synthesis.log`, `/tmp/a-program-g3d-asan-program.log` |
| Extended dependent-prefix source/image matrix | `/tmp/a-program-g3d-dependent-{0,100,1000000}{,-resaved}.a`, exact results at chunks 1/64 |

The optimized command used `make -f src/prototype/pointer/Makefile -j2
BUILD=/tmp/a-program-g3d CFLAGS='-std=c11 -Wall -Wextra -Werror -O2'
check-acceptance`. Sanitizer binaries used `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`, with leak detection and halt-on-error.
The image runner precedes the final dependent-prefix extension; that extension
was checked separately under the same sanitizer binaries and is included in the
final full optimized runner. This is a scope qualification, not a skipped test.

Working diff against `794ec6a`, including untracked proof/fixture files:

| File under `src/prototype/pointer/` | Added | Removed | Net |
|---|---:|---:|---:|
| `synthesis.c` | 67 | 11 | 56 |
| `Makefile` | 4 | 0 | 4 |
| `tests/image_cli.sh` | 2 | 1 | 1 |
| `tests/acceptance/indexed-dependent-field-path.p` | 26 | 0 | 26 |
| `tests/acceptance/indexed-dependent-field-path-wrong.p` | 12 | 0 | 12 |
| `tests/acceptance/indexed-dependent-field-evidence-wrong.p` | 10 | 0 | 10 |
| `tests/acceptance/sort-quick-property.p` (unfinished draft) | 83 | 0 | 83 |

Implementation: +67/-11 (net +56); build: +4/-0; tests/proofs: +133/-1
(net +132), of which 83 lines are not a completed theorem. This plan adds
98 documentation lines, counted separately. G3d, J1/J3 and R remain open;
neither passing existing tests nor this helper repair closes #29.

### 2026-09-18: G3d dependent telescope and captured helper progress

This entry supersedes the earlier `sized_rest`/`part_left`/`part_right`
unsupported status, not the outstanding universal QuickSort requirement.

The indexed-result synthesizer now derives a type-case family over a whole
constructor telescope. It removes the source field binders from the candidate
base, factors the existing result classifier through the constructor fields,
and transports the value along the full checked Identity boundary. Dependent
prefix paths are retained. The target type is normalized and checked by the
ordinary rules. No homogeneous equality is asserted between different fibers;
no UIP, equality reflection, expected-type motive synthesis or new kernel rule
was added. The existing distinct-variable pattern-factorization check remains.

Automatic result inference no longer scans separately for homogeneous
identities of every field. The explicit `constructor_field_identity` API stays:
its caller chooses a particular observer and path, unlike result inference.
Removing it initially broke the `iadt_test` build; restoring it retains the
existing observer/path, wrong-endpoint and nominal-schema regression coverage.
This is an intentional distinction between two derivations, not duplicate
authority. Both use the shared boundary-lifting implementation.

`indexed-dependent-tail-path.p` checks a dependent tail and an arbitrary
evidence payload; its negative companion must reject substituting an unrelated
tail. The previous independent-field and dependent-prefix regressions remain.
The repaired optimized suite before the helper change reached its final syntax
inventory in `/tmp/a-program-g3d-telescope-acceptance.log`; its process was
already terminal when observation resumed, so no captured exit status is claimed
for that run. The following fresh full run is the publication-relevant gate.

A second independent blocker was isolated at `case_branch`: expanding a
nonrecursive helper Match could refine a captured parameter, making the later
projection from the branch Context to the fixed graph parameter Context
undefined. For `@partitionByDecision`, this was a null prefix before dependent
substitution, not a failed equality between result classifiers.

An initial attempt to retain every Match helper passed the new example but
changed existing public graph constructor layouts. The full acceptance run
`/tmp/a-program-g3d-helper-acceptance.log` exited 2 at source compatibility:
the legacy partition property no longer checked. That attempt is superseded.
The current policy retains nonrecursive helper calls only when their actual
discriminant belongs to the fixed graph parameter telescope. Local inputs still
split as before; recursive helper calls continue to use their shared graphs.
The retained call uses the existing graph/witness owner and substitution checks.
It does not turn a failed projection into an unchecked map.

Focused evidence:

- The standalone captured-Match fixture is unsupported at published `794ec6a`
  (2,107 steps); the new implementation accepts it (6,054 steps). Both branches
  of its execution witness normalize, and the wrong-output claim is rejected
  (3,932 steps).
- The unchanged legacy partition proof accepts again (93,731 steps).
- `@partitionByDecision` accepts (61,373 steps with the exact PR #30 provider).
- The QuickSort draft now proves `append_all`, `lower_all`, `upper_all`,
  `decision_all` and `partition_all`, including their explicit postchecks
  (322,667 steps with the mechanically assembled provider). These are generic
  element-property preservation lemmas, not yet a universal Sorted theorem.

Progress and remaining gates:

- [x] Replace the dependent-field inference dead end with whole-telescope
  transport using existing typed terms and Identity rules.
- [x] Preserve explicit field-observer tests and API.
- [x] Isolate and repair captured nonrecursive helper handling without changing
  the existing local-split graph layout.
- [x] Add captured-Match positive and wrong-result regression fixtures.
- [x] Pass a fresh full optimized gate after the helper repair.
- [x] Check the new dependent-tail and captured-helper paths with sanitizers
  and pending/completed images.
- [x] Complete partition order bounds, QuickSort Acc graph induction, public
  QuickSort Sorted proof and actual-result packets; finish G3d/J1.
- [x] Finish J3 publication/issue disposition using the requirement table below.
- [x] Publish the completed G3d milestone; #29 is closed. R has not begun.

The fresh full optimized `check-acceptance` passed (exit 0), recorded in
`/tmp/a-program-g3d-captured-acceptance.log`, using
`BUILD=/tmp/a-program-g3d-telescope-opt` and the same `-O2` flags as above.
The final captured-helper positive/negative fixtures are included. In particular,
the legacy partition property, QuickSort property clients and all three already
published universal sort proofs pass without changing their providers or claims.
The sanitizer `program_test` and QuickSort preservation draft also passed;
the full sanitizer image runner is a separate gate, recorded below when complete.

Same-input Solve work comparison (published `794ec6a` / current working tree):

| Input | Before | After |
|---|---:|---:|
| Original IF8 QuickSort provider | 48,320 | 48,735 |
| Legacy partition property with that provider | 93,316 | 93,731 |

These are solver transitions, not wall-clock or allocation measurements. The
415-step increase must not be advertised as zero-cost; it is approximately
0.86% and 0.44%, respectively.

Working changes against `794ec6a` at this checkpoint, including untracked files:

| File under `src/prototype/pointer/` | Added | Removed | Net |
|---|---:|---:|---:|
| `function_graph.c` | 22 | 1 | 21 |
| `synthesis.c` | 224 | 57 | 167 |
| `synthesis.h` | 3 | 5 | -2 |
| `Makefile` | 9 | 0 | 9 |
| `tests/image_cli.sh` | 4 | 1 | 3 |
| `tests/acceptance/indexed-dependent-field-path.p` | 26 | 0 | 26 |
| `tests/acceptance/indexed-dependent-field-path-wrong.p` | 12 | 0 | 12 |
| `tests/acceptance/indexed-dependent-field-evidence-wrong.p` | 10 | 0 | 10 |
| `tests/acceptance/indexed-dependent-tail-path.p` | 14 | 0 | 14 |
| `tests/acceptance/indexed-dependent-tail-path-wrong.p` | 11 | 0 | 11 |
| `tests/acceptance/function-graph-captured-match.p` | 18 | 0 | 18 |
| `tests/acceptance/function-graph-captured-match-wrong.p` | 9 | 0 | 9 |
| `tests/acceptance/sort-quick-property.p` (preservation draft) | 112 | 0 | 112 |

Implementation/headers: +249/-63 (net +186); build: +9/-0; tests/proofs:
+216/-1 (net +215). Documentation is counted separately. This is correctness
foundation work for G3d, not a claim to have completed the net-negative R epoch.

### 2026-09-18: Universal QuickSort theorem and conversion gate

The preservation draft has now been extended to an ordinary universal proof:

```text
quick_correct :
  (xs : List Nat) -> (ys : List Nat) ->
  @quickSort Nat (&natLessOrEqual) xs ys -> Sorted ys
```

`partition_ordered` proves both pivot bounds; `quick_all` proves that the Acc
implementation preserves an arbitrary element predicate. `quick_sorted` uses
both recursive graph IHs, preserved pivot bounds and the existing general
append theorem. `quick_correct` eliminates the public QuickSort graph to use
that result. The sort, Acc and comparator in the exact PR #30 provider remain
unchanged. No new theorem-specific kernel rule, order axiom, Returns predicate
or expected-type proof search was introduced.

The source checks in 440,684 transitions, including its execution examples.
The mixed four-element packet yields the expected Sorted evidence and exact
output. Reading the proof takes about 3.47 million transitions; the earlier
two-million-step test was genuinely pending, not a failed theorem. The shared
runner uses a ten-million comparison budget for QuickSort, retains two million
for tree/merge, and reports actual work at chunks 1 and 64. This higher budget
is not a performance improvement. Empty, singleton, duplicate, ascending and
descending cases exercise the same universal proof; finite tests do not replace
that proof. Sortedness alone is not a permutation, stability or complexity claim.

The additional wrong-comparator test uncovered a conversion scheduling defect:
strong comparison descended into NF beneath a thunk containing a recursive
function, unfolding the fixpoint beneath its Lambda indefinitely. The source
remained pending at five million transitions. The comparison work was observed
at NF stack depth 14,022 after one million source transitions. This was not an
unresolved graph property or an unsupported comparator theorem.

`conversion.c` now exposes only the suspended computation's WHNF; when that is
a Lambda, congruence compares the Lambda without first normalizing its entire
body. Thunk is not declared globally rigid: thunk/force eta and potentially
divergent suspended computations keep their existing rules. The Core suite
checks those old positive and pending cases. Reduction semantics, evaluation
profile and artifact format are unchanged; only conversion's demand strategy
changes. The small `recursive-thunk-conversion-wrong.p` is pending at 100,000
steps in published `794ec6a`, and rejected at 1,412 in this implementation.
The actual wrong-comparator proof is now rejected at 441,574 transitions.

Failed gate records are retained: `/tmp/a-program-g3d-quick-sort.log`,
`/tmp/a-program-g3d-final-full.log` and
`/tmp/a-program-g3d-final-asan-sort.log` stopped at the wrong-comparator pending
case. They are superseded only by the final runs listed below. The earlier
successful sanitizer image run covers the pre-conversion revision; it does not
by itself verify this last conversion change.

Issue #29 requirement-to-test audit (publication gates still apply):

| Requirement | Permanent evidence |
|---|---|
| Distinct nested graph leaves, helper factoring, elimination and witnesses | `graph-duplicate-leaf.p`, `graph-helper-leaf.p`, `function-graph-helper-call.p`; wrong/ambiguous companions; source and image runners |
| Open-index LE predecessor inversion and impossible branches | `le-predecessor.p`, `le-predecessor-wrong.p`, `le-predecessor-invalid.p` |
| Conventional LE transitivity | `le-transitivity.p` and its wrong-endpoint companion |
| Boolean comparator connected to conventional order | `comparator-order.p`, `sort-insertion-property.p` and `sort-insertion-sort-property.p` (`Decision`, `direct`, `unwrap`); wrong comparator/order claims |
| Universal insertion-sort theorem | `sort-insertion-sort-property.p` (`sort_correct`) |
| Universal tree-sort theorem | `sort-tree-property.p` (`tree_correct`) |
| Universal reported merge-sort theorem | `sort-merge-property.p` (`merge_correct`); insufficient-fuel negative |
| Universal fuel-free QuickSort theorem | `sort-quick-property.p` (`partition_ordered`, `quick_all`, `quick_sorted`, `quick_correct`) |
| Connection to actual execution, not an independently written expected result | Each proof consumes `*sort` packets; `sort_insertion.sh` compares proof readers, packet values and direct values |
| Rejection of invalid claims | Per-sort wrong-input tests, QuickSort wrong-comparator test, graph leaf/evidence negatives |
| Pending/completed persistence and unchanged provider | `sort_insertion.sh` checks the provider SHA-256, save budgets 0/100/completed, inert byte resaves, resumed checks and wrong claims; `image_cli.sh` covers the independent compiler regressions |

The historical report's A/B defects were valid. Its direct comparator attempt
was an unfinished attempt, not proof of impossibility; the current explicit
Decision proof supplies a supported construction without weakening `::`.
PR #30 was already merged as `5d9fa03`; do not reclassify that documentation
merge as the implementation repair. Close #29 only after the final gates and
publication, with a link to this table and the actual implementation commits.

Final G3d gates, all exit 0, on unchanged implementation/tests:

| Gate | Log under `/tmp/` |
|---|---|
| Full optimized `check-acceptance`, including all four universal sort proofs | `a-program-g3d-publish-full.log` |
| ASan/UBSan complete shared sort runner | `a-program-g3d-publish-asan-sort.log` |
| ASan/UBSan complete source/image runner | `a-program-g3d-publish-asan-images.log` |
| ASan/UBSan Core and Program unit suites | `a-program-g3d-publish-asan-core.log`, `a-program-g3d-publish-asan-program.log` |
| Debug Core suite | `a-program-g3d-conversion-core.log` |

Builds use the optimized and sanitizer flags recorded above. The final sanitizer
executables are in `/tmp/a-program-g3d-telescope-asan`; all runs enable leak
detection and halt on errors. The source/image runner includes all the new
dependent-field, captured-helper and recursive-thunk negative regressions.
`git diff --check` passes. No test expectation was weakened to pass a gate.

Same-input allocation counts were inspected at `main.c:395` immediately after
Solve, using the pre-G3d debug compiler and the final debug compiler. The G3c
commit only changed proof/tests/docs, so its compiler matches the saved G3b
baseline. Counts are interned records, not bytes or peak RSS:

| Input | Steps before/after | Terms | Typed occurrences | Contexts | Maps | Proofs |
|---|---:|---:|---:|---:|---:|---:|
| `examples/06_pred.p` | 758 / 758 | 120 / 120 | 157 / 157 | 18 / 18 | 47 / 47 | 224 / 224 |
| `length-output-proof.p` | 9,638 / 9,638 | 2,031 / 2,031 | 3,574 / 3,574 | 334 / 334 | 1,359 / 1,359 | 4,941 / 4,941 |
| Original IF8 QuickSort | 48,320 / 48,735 | 11,577 / 11,990 | 19,186 / 19,522 | 1,348 / 1,366 | 4,805 / 4,943 | 22,915 / 23,315 |

Final source/test changes against `794ec6a` (documentation separate):

| File under `src/prototype/pointer/` | Added | Removed | Net |
|---|---:|---:|---:|
| `conversion.c` | 17 | 0 | 17 |
| `function_graph.c` | 22 | 1 | 21 |
| `synthesis.c` | 224 | 57 | 167 |
| `synthesis.h` | 3 | 5 | -2 |
| `Makefile` | 10 | 0 | 10 |
| `tests/image_cli.sh` | 4 | 1 | 3 |
| `tests/sort_insertion.sh` | 17 | 7 | 10 |
| `tests/acceptance/indexed-dependent-field-path.p` | 26 | 0 | 26 |
| `tests/acceptance/indexed-dependent-field-path-wrong.p` | 12 | 0 | 12 |
| `tests/acceptance/indexed-dependent-field-evidence-wrong.p` | 10 | 0 | 10 |
| `tests/acceptance/indexed-dependent-tail-path.p` | 14 | 0 | 14 |
| `tests/acceptance/indexed-dependent-tail-path-wrong.p` | 11 | 0 | 11 |
| `tests/acceptance/function-graph-captured-match.p` | 18 | 0 | 18 |
| `tests/acceptance/function-graph-captured-match-wrong.p` | 9 | 0 | 9 |
| `tests/acceptance/recursive-thunk-conversion-wrong.p` | 8 | 0 | 8 |
| `tests/acceptance/sort-quick-property.p` | 209 | 0 | 209 |
| `tests/acceptance/sort-quick-property-wrong.p` | 9 | 0 | 9 |
| `tests/acceptance/sort-quick-comparator-wrong.p` | 9 | 0 | 9 |

Implementation/headers: +266/-63 (net +203); build: +10/-0; tests/proofs:
+356/-8 (net +348). This is a correctness/feature milestone, not the deferred
authority refactor or a claimed code-size reduction. The README now distinguishes
the separate universal Sorted proofs from mere execution witnesses. Publication
and issue disposition are recorded after their remote operations succeed.

### 2026-09-18: G3d publication and issue disposition

Commit `6ba6cf306442d4a022c191100188f0ff174489d8` was pushed atomically to
`origin/main` and `origin/rewrite/pointer-core-hott`, fast-forward from `794ec6a`
without force. Both remote tips were verified afterward. No implementation or
test changes occurred between the final gates and that commit.

The [resolution report](https://github.com/repyt-margorp/a-program/issues/29#issuecomment-5721616118)
links the implementation, requirement table, tests and limitations. Issue #29
was then closed as completed and its remote CLOSED state verified. PR #30 had
already been merged as documentation; it was not merged again. This supersedes
earlier chronological entries saying G3d/J1/J3 or #29 were still pending.

The implementation commit changes documentation separately: README +11/-6;
this plan +348/-2. Together with the source/test counts above, that commit is
+991/-79 (net +912); source/build/tests alone are +632/-71 (net +561).
This following publication-record update is documentation only.

Next: resume R against the current typed-data authority plan and deferred Solver
audit, rechecking their premises against this implementation. This milestone
does not complete that parent goal or waive its cleanup/acceptance conditions.

### 2026-09-18: R declaration-allocation epoch

Completed the declaration dependency removal documented in the
[Solver audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#declaration-epoch-implementation-and-measurements).
Source images retain the nominal declaration without its allocation-only
formation derivation. Ordinary synthesis verifies every field and result index;
independently selected proof roots remain independently checked. No equality
reflection, new Core tag or acceptance cache is added.

- [x] Remove `allocation_origin` and declaration restoration through proof jobs.
- [x] Preserve alpha-renamed fields, reject incompatible fields, and test two
  inert resaves with both valid and invalid independent roots.
- [x] Full optimized `check-acceptance`; affected debug and ASan/UBSan suites.
- [x] Record per-file LOC and retained QuickSort work/image measurements.
- [x] Publish this completed epoch to Main and verify both remote tips.

Source-image formats are now APGSRC62/63 and reduction records APGRCP3;
older images require regeneration. Implementation/header delta is +137/-72
(net +65); tests +135/-9 (net +126). This is removal of a specific duplicate
dependency, not completion of R or a claim of overall code-size reduction.

Publication: `121d6e5b04620b590b839f1ff98f008d5e1fa666`, atomically pushed to
`origin/main` and `origin/rewrite/pointer-core-hott`, fast-forward from `28e1837`.
Both remote tips were verified. No implementation/test edits occurred after the
final gates. Documentation in that commit is +185/-2, separate from source and
tests; total +457/-83. This publication-record update is documentation only.

### R: Accepted structural projections

- [x] Use the retained typed occurrence before traversing completed producers'
  recipes; share extraction for term, type and classifier queries.
- [x] Test allocation-free projections and symbolic/closed effect snapshot
  agreement without replacing the original producer's checks.
- [x] Debug and ASan/UBSan synthesis suites; same-input QuickSort work counts.
- [x] Full optimized acceptance, including imported/resaved QuickSort proofs.
- [x] Main publication and remote-tip verification.

The [audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#accepted-structural-projections-after-83249f1)
records the internal snapshot-contract adjustment and measured request reduction.
This is progress on target 1, not completion of the pending-construction cleanup
or the broad R goal. Source-image formats are unchanged by this epoch.

Published as `607058c`, atomically fast-forwarding both `origin/main` and
`origin/rewrite/pointer-core-hott` from `83249f1`; remote tips verified.
Implementation/headers +30/-16, tests +58/-0, documentation +68/-0.
No implementation or test edits followed the final verification runs.

### R: Shared pending body adaptation

- [x] Prepare the existing body adapter once as ordinary VALUE_FROM_TYPE/RETURN
  rules or a raw computation; delete separate term/type adaptation branches.
- [x] Test shared pending rules, effect-closure progress, unchanged computations
  and rejection of the wrong Context; debug and sanitizer synthesis pass.
- [x] Full optimized acceptance, including 63/63 compatibility cases and the
  concrete Acc regression; final debug and ASan/UBSan synthesis suites.
- [x] Milestone publication and remote-tip verification.

The [body audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#body-adaptation-one-prepared-rule-after-cee33c3)
records the net -14 implementation lines and the modest scheduling cost. This
does not complete the broader pending-construction cleanup or R.

This epoch removes the three independent body-adaptation paths in favor of
ordinary shared checking rules. A premature application-polarity assumption
found by full compatibility testing was corrected before publication. No new
Core tag, proof rule, acceptance authority or wire format is introduced.
Implementation: `synthesis.c` +46/-60; tests: `tests/synthesis.c` +29/-0.
The final QuickSort delta is +39 requests/+720 steps, with unchanged proof,
occurrence and Term counts. No implementation/test edits followed these gates.

Published as `fe7b3e9d681863eae0378938cc41b1ed47841158`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `cee33c3`; both
remote tips verified. Documentation in that commit is +86/-3, separate from
implementation and tests; total +161/-63. This publication record is doc-only.

### R: Shared preparation notifications

- [x] Replace consumer prerequisite-following/polling with existing producer
  subscriptions; share readiness between subscription and publication.
- [x] Remove per-adapter publication calls; add direct-dependency and idle-queue
  regression coverage. Existing pending-effect/cycle and debug synthesis pass.
- [x] Full optimized acceptance and affected ASan/UBSan synthesis.
- [x] Publish the completed epoch to Main and verify remote tips.

The [notification audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#preparation-notifications-after-7c7a58b)
records the unchanged proof/node counts, lower work counts and per-file delta.
This does not complete R's pending-structure or remaining authority cleanup.

Published as `95d8d996b767fd7529584cfd7b9aed9b47aec821`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `7c7a58b`; both
remote tips verified. Implementation +14/-17, tests +28/-0, documentation
+49/-0; total +91/-17. This publication-record update is documentation only.

### R: Sequence choice owned by its source adapter

- [x] Publish only the selected sequence construction; retain the original
  Context/premise checks and the checked pure/effectful fallback rules.
- [x] Delete independent provisional-FOLD decisions from term/classifier
  projections. Test constant/dependent continuation snapshots against evidence.
- [x] Debug and ASan/UBSan synthesis suites; record work and per-file deltas.
- [x] Full optimized acceptance, including 63/63 compatibility cases.
- [x] Publication with remote-tip verification.

The [sequence audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#sequence-construction-choice-after-a37b283)
records both the authority consolidation and increased descriptive work.
Pending-construction sharing and the broader R acceptance gates remain open.

Published as `8a97c8babc1f5c7b48ac37e1c4132a3dad5095c8`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `a37b283`; both
remote tips verified. Implementation +76/-76, tests +27/-0, documentation
+61/-0; total +164/-76. This publication-record update is documentation only.

### R: Direct substitution telescope checking

- [x] Remove the temporary declaration-chain copy from substitution admission.
- [x] Reuse the existing exact proof index before allocating typed images;
  preserve alternate derivations and dependent classifier checking.
- [x] Debug Core regression, including invalid first/last dependent images.
- [x] Measure same-input allocation/work/node counts against `7eeefb8`.
- [x] Full optimized acceptance and affected ASan/UBSan Core suite.
- [x] Publish and verify both remote tips after all gates pass.

The [scope-copy audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#substitution-telescope-checking-after-7eeefb8)
records the eliminated allocation/traversal and rejected extra-cache proposal.
Composition arrays and nested synchronous wrappers remain; this is not the
completion of the broader R goal.

Published as `5ec308a28ac7fc47b7b170173c400e48c23afcf0`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `7eeefb8`;
both remote tips verified. Implementation +10/-11, tests +15/-0,
documentation +82/-1; total +107/-12. This publication record changes
documentation only.

### R: Shared type-Term construction and handler projection

- [x] Share the pending construction request for known type-forming derivations
  between type and ordinary Term views; keep source/projection type guards.
- [x] Delete the handler-specific carrier projection and dependency polling;
  use the prepared zero-clause/nonzero-clause rule's classifier projection.
- [x] Debug synthesis, both pending request orders, invalid value-as-type guard,
  immutable effect snapshots and same-input work/node measurements.
- [x] Final optimized acceptance (63/63 compatibility) and ASan/UBSan synthesis.
- [x] Publish and verify Main and the active rewrite tip after all gates pass.

See the [shared construction audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#shared-type-term-construction-after-b21dc46).
This unifies descriptive work, not erased Core and typing or proof acceptance.
The broader pending-classifier and net-negative refactoring gates remain open.

Published as `1a08868a365dfd98d32c75b33d13e5d310fb4986`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `b21dc46`;
both remote tips verified. Implementation/header +39/-37, tests +21/-0,
documentation +72/-0; total +132/-37. This publication record is documentation only.

### R: Accepted Context declaration projection

- [x] Read accepted Context declarations directly by binder identity; remove
  the superseded late lookup and the pending Pi-domain reconstruction branch.
- [x] Route pending Pi scopes through their prepared ordinary extension rule.
- [x] Test that an accepted annotation is not structurally reconstructed;
  verify pending effect closure and compare QuickSort work/node counts.
- [x] Full optimized acceptance (63/63 compatibility) and affected ASan/UBSan synthesis.
- [x] Publish and verify remote tips after all gates pass.

See the [Context projection audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#declared-types-from-accepted-contexts-after-b4b674b).
Source family-parameter adaptation remains intentionally distinct. This is not
completion of the broader R pending-construction or net-negative gates.

Published as `2cc3e5a62952f731c40eadf6d777ed7ea4d4d1ad`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `b4b674b`;
both remote tips verified. Implementation +11/-13, tests +45/-0,
documentation +66/-0; total +122/-13. This publication record is documentation only.

### 2026-09-18: Shared Context projection action

- [x] Move projection completion into the existing occurrence action; remove
  the private typed-input projection branch. No new acceptance authority.
- [x] Verify shared one-step identity/weakening and no Core substitution work;
  keep non-identity map checks and independent proof validation.
- [x] Debug Core, full optimized and full ASan/UBSan acceptance pass. Repeat
  Core/synthesis/source images with leak detection and halt-on-error enabled.
- [x] Publish this completed epoch and verify both remote tips (`fe62337b`).

`typing.c` +5/-9 (net -4); `tests/core.c` +12/-1 (net +11).
The incremental Solver audit records exact commands, work/node counts and
remaining cumulative growth. Neither the pending-classifier cleanup nor R's
overall acceptance and net-negative gates are closed by this epoch.

Published as `fe62337bf782baaf85de1d6a3fe10558b1225387`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `8503fd6`; both
remote tips were verified. Commit totals: implementation +5/-9, tests +12/-1,
documentation +77/-0; overall +94/-10. This publication record is documentation
only and does not change the verified implementation.

### R: Single source value-kind classification

- [x] Remove repeated recipe traversal/judgement lookup; prefer accepted typed
  results while retaining pending effect preparation and independent checks.
- [x] Debug regression: pending/accepted type, value, raw Lambda and block
  bodies agree. IF8 and length proof work/node counts are unchanged.
- [x] Full optimized acceptance (63/63 compatibility), ASan/UBSan synthesis
  and complete source-image tests, with leak detection and halt-on-error.
- [x] Publish the verified epoch and confirm both remote tips (`50a7d1f`).

See the [classification audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#source-value-kind-from-accepted-judgements-after-9fc2ecd).
Implementation net -19 lines; tests +31. This does not close the broader R gates.

Published as `50a7d1fe208846cae6d7727d162193f8461e4dd9`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `9fc2ecd`; both
remote tips verified. Commit totals: implementation +13/-32, tests +31/-0,
documentation +61/-0. This publication record changes documentation only.

### R: One lexical binding origin

- [x] Unify explicit/automatic binder registration; delete the writer's
  duplicate BINDING_JOB origin path, not Context or independent proof checks.
- [x] Debug synthesis/source-image tests, callback counts and bidirectional
  compatibility with the preceding image writer/reader.
- [x] Full optimized acceptance (63/63 compatibility), ASan/UBSan synthesis,
  source images, nested handlers and handler save boundaries.
- [x] Publish and verify both remote tips (`54b12df`).

See the [binding-origin audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#one-lexical-binding-origin-after-abeedc6).
Implementation/header net -13, tests +23. Global origin traversal remains open.

Published as `54b12df71c66e7dc5214e2e02900871b33bd8950`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `abeedc6`; both
remote tips verified. Commit totals: implementation/header +16/-29,
tests +23/-0, documentation +84/-0. This publication record is documentation only.

### R: Stream lexical allocation addresses

- [x] Remove per-application scope snapshots and intermediate scope copies;
  use one registration path for source scopes and imported address arrays.
- [x] Debug synthesis and same-input work/arena measurements; nested lexical
  and array lookup share the exact binding record.
- [x] Full optimized acceptance (63/63 compatibility), ASan/UBSan synthesis,
  source images, nested handlers and handler save boundaries.
- [x] Publish and verify both remote tips (`1760387`).

See the [address audit](2026-09-18-INCREMENTAL-SOLVER-DUPLICATION-AUDIT.md#stream-lexical-addresses-after-c911f16).
Implementation +10 lines, tests +5; persistent application state and temporary
arrays are reduced. This does not meet the cumulative source-reduction gate.

Published as `176038784be54736260f29a49d013d5c1d2ef214`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `c911f16`; both
remote tips verified. Commit totals: implementation +48/-38, tests +5/-0,
documentation +70/-0. This publication record changes documentation only.

### R: Remove Handler proof-shape allocation fallback

- [x] Delete the unused production fallback; keep Handler return variables in
  the existing lexical source-binding registry, not another allocation path.
- [x] Replace tautological reader/operand assertions with registry/typed-Lambda
  agreement throughout the source-image boundary test. Keep semantic checks.
- [x] Run affected debug and sanitizer tests and full optimized acceptance.
- [x] Publish this completed epoch and verify both remote tips (`59b38e9`).

No source-image format, proof rule, handler semantics or source syntax changes.
The broader pending-construction and output-sensitive export audit remains open.

Debug Handler boundaries, full optimized `check-acceptance` (63/63 source
compatibility and universal QuickSort), and ASan/UBSan full source-image,
nested-Handler and boundary tests pass. Boundary coverage is 4,240 snapshots.
Sanitizers enable leak detection and halt-on-error. Logs are
`/tmp/a-program-authority-handler-binding-{boundaries,acceptance,asan-source,asan-nesting,asan-boundaries}.log`;
builds use the corresponding base, `-opt` and `-asan` directories. Normalized
`export results:` records match the preceding address-streaming epoch exactly,
including Solve steps. No wall-clock speedup is claimed.

Per-file changes: `synthesis.c` +0/-10; `synthesis.h` +2/-0;
`tests/source_io.c` +21/-8. Implementation/header net -8; tests net +13.
Cumulative implementation/header delta is +2,264/-1,025 (net +1,239) from
R76 `3a3bf550`, and +7,222/-3,567 (net +3,655) from R0 `4657cc6`.
The overall source-reduction gate remains unmet.

Published as `59b38e9cc950202d4d8d47cbb7009b6b0759b514`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `ee54b8c`; both
remote tips verified. Commit totals: implementation/header +2/-10, tests
+21/-8, documentation +47/-9. This publication record is documentation only.

### R: Stream constructor Context addresses

- [x] Delete Context-to-array copying and the duplicate stored scope/length;
  retain one binding-registration algorithm for source, Context and wire input.
- [x] Verify ordered nested Context/array lookup and identical work/node counts
  on the original IF8 QuickSort and the length property proof.
- [x] Measure ordinary and retained export traversal. Withdraw lazy index
  initialization because prelude references leave all scan counts unchanged.
  Keep regression coverage for references first discovered through reductions
  and independently selected proof roots; do not add an unproven index owner.
- [x] Complete affected debug/sanitizer and full optimized acceptance.
- [x] Publish the completed epoch and verify both remote tips (`f467b34`).

Compared with `8d4c731`, strict debug builds at `main.c:395` with `--steps
1000000` give unchanged IF8 Solve/jobs/bindings/proofs/occurrences/Terms:
47,046 / 15,319 / 344 / 23,315 / 19,522 / 11,969. Graph arena used bytes
decrease 19,762,560 -> 19,755,840; capacity 19,922,944 -> 19,906,560 and
blocks 1,216 -> 1,215. For `length-output-proof.p`, the same counts stay
9,569 / 3,430 / 86 / 4,941 / 3,574 / 2,031; used bytes decrease
4,211,040 -> 4,208,736, capacity 4,259,840 -> 4,243,456, blocks 260 -> 259.
Arena sums exclude hash-table allocations; this is not a wall-clock speedup.
The baseline is a `git archive` of `8d4c731`, built with the same `-O0 -g`
flags under `/tmp/a-program-authority-context-address-baseline`.

The first two optimized runs were deliberately interrupted while investigating
the export-index trial; neither is counted as a passing verification run.
The trial was removed entirely from `source_io.c` before final verification.
The broad pending-construction, output-sensitive traversal and cumulative
source-reduction gates remain open.

Final verification: debug synthesis, full source-image and normalization tests;
full optimized `check-acceptance`, including 63/63 compatibility, universal
QuickSort and retained proof/image tests; ASan/UBSan synthesis, full source
images, normalization, nested handlers and all 4,240 boundary snapshots pass.
Sanitizers enable `detect_leaks=1:halt_on_error=1` and UBSan halt-on-error.
Builds/logs use `/tmp/a-program-authority-context-address`, with `-opt`/`-asan`
build suffixes. The completed optimized log is
`/tmp/a-program-authority-context-address-acceptance-verified.log`.
Export-result records, including steps, exactly match the preceding Handler
cleanup after removing temporary filenames; no acceptance status is weakened.

Per-file source changes: `synthesis.c` +20/-23 (net -3),
`tests/synthesis.c` +12/-0, `tests/source_io.c` +14/-0.
The withdrawn writer change contributes zero lines. Cumulative implementation
and header changes are +2,259/-1,023 (net +1,236) from R76 `3a3bf550`, and
+7,219/-3,567 (net +3,652) from R0 `4657cc6`. The net-negative gate is unmet.

Published as `f467b34ddee717e2dfe33dcb28d762285a3c65ec`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `8d4c731`; both
remote tips verified. Commit totals: implementation +20/-23, tests +26/-0,
documentation +62/-0. This publication record is documentation only.

### 2026-09-18: A1 reference audit and constructor allocation view

- [x] Complete the parent's index-alias identity trace, including independent
  retained map/original-premise roots and two inert resaves.
- [x] Classify pending/accepted source references, explicit allocation inputs,
  hidden Fold binders and fresh/restored Match descriptions in the parent A1
  table. No new allocation cache or wire record is needed for these cases.
- [x] Remove the duplicate constructor-child traversal from
  `pg_synthesis_allocation_object`; use the existing member/constructor view.
  That view reads accepted Context-map destinations instead of proof slots.
- [x] Run the full optimized and affected debug/sanitizer gates.
- [x] Publish the verified epoch and record remote tips.

All commands exit zero: strict debug `source_io_test context-scopes` and
`tests/source_io.sh`; full optimized `check-acceptance`; ASan/UBSan
`synthesis_test`, `tests/source_io.sh`, `normalization`, `handler-nesting` and
`handler-boundaries` (4,240 snapshots). Builds/logs use the prefix
`/tmp/a-program-authority-index-trace`; optimized log suffix `-acceptance.log`,
sanitizer build suffix `-asan`, and sanitizer log suffixes
`-asan-{source,synthesis,normalization,nesting,boundaries}.log`.
Flags: debug `-O0 -g`, optimized `-O2`, sanitizer
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`,
all with `-std=c11 -Wall -Wextra -Werror`. Sanitizers enable leak detection
and halt-on-error. Full acceptance includes 63/63 compatibility, universal
QuickSort, retained proofs/images and negative cases. Its `export results:`
records match the preceding Context-address epoch including step counts after
normalizing temporary paths. No elapsed-time or allocation improvement is claimed.

Per-file implementation: `synthesis.c` +8/-11, `synthesis.h` +2/-1,
`source_io.c` +3/-3; net -2. `tests/source_io.c` is +92/-5 (net +87).
Cumulative implementation/header deltas: R76 `3a3bf550` +2,262/-1,028
(net +1,234); R0 `4657cc6` +7,223/-3,573 (net +3,650).
The net-negative gate is still unmet. Closing A1 is not completion of R:
selected-root traversal, the remaining structural-consumer audit and A5's
final performance/verification work remain open. The next structural change
must address the measured whole-store source-origin search, rather than add
another persistent accepted-allocation authority or weaken image checks.

Published as `d47f4ee9075b11ea6dbd8b6412be972a88122f35`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `7c6b8fe`; both
remote tips verified. Commit totals: implementation/header +13/-15,
tests +92/-5, documentation +85/-7. This publication record is doc-only.

### 2026-09-19: Selected source-reference discovery

- [x] Delete unconditional all-job/all-binding discovery from source writing.
- [x] Index borrowed inputs by source syntax or allocated binder; keep all
  allocation/type/acceptance data at its existing owner.
- [x] Follow syntax/object frontiers, including late proof/reduction edges.
- [x] Add source-free constructor-field Context retention and indexed-read
  regressions; retain the existing selected-root and boundary checks.
- [x] Measure discovery work and the additional index space against `f8fb837`.
- [x] Full optimized acceptance and affected debug/sanitizer gates.
- [x] Publish the verified epoch and record remote tips.

The parent's A3 entry records the failed constructor-keyed draft, the binder
reverse-lookup correction, exact work/arena counts and the remaining
same-syntax/different-scope candidate bound. No wire or kernel rule changes.
The index contains references only, not a second accepted allocation authority.

Verification uses `/tmp/a-program-authority-source-sites`, `-opt` and `-asan`
builds. All exit zero: final strict debug `tests/source_io.sh`; full optimized
`check-acceptance` (`-acceptance.log`), including 63/63 compatibility, universal
sorting proofs and images; ASan/UBSan `synthesis_test`, full `source_io.sh`,
`normalization`, `handler-nesting`, and all 4,240 `handler-boundaries` snapshots.
Flags and sanitizer environment are identical to the preceding epoch. Logs
use `-source-final.log` and `-asan-{source,synthesis,normalization,nesting,boundaries}.log`.
Normalized `export results:` records, including Solve steps, exactly match
the preceding index-alias audit. No implementation/test edits followed them.

Per-file implementation/header: `source_io.c` +49/-31, `synthesis.c` +52/-17,
`synthesis.h` +9/-0; total +110/-48 (net +62). `tests/source_io.c` +29/-0.
Cumulative implementation/header changes: R76 `3a3bf550` +2,357/-1,061
(net +1,296); R0 `4657cc6` +7,316/-3,604 (net +3,712).
This epoch reduces repeated search, not source size. The net-negative gate,
remaining A3/A4 audit and final A5 gates remain open; R is not complete.

Published as `02df61ced964f5dd23fd1fc9fed899f73f740f37`, atomically
fast-forwarding Main and `rewrite/pointer-core-hott` from `f8fb837`; both
remote tips verified. Commit totals: implementation/header +110/-48,
tests +29/-0, documentation +73/-5. This publication record is doc-only.

### 2026-09-19: Shared-syntax source isolation and candidate insertion

- [x] Add the 128-sibling selected-image regression to `member_use_origins`.
- [x] Remove quadratic duplicate-edge discovery from source image collection.
- [x] Pass full optimized acceptance and affected debug/sanitizer gates.
- [x] Publish the verified change with the next source-traversal milestone (`a9b24b2`).

Baseline: `605d11b`. The parent A3 entry records the uniqueness argument,
16,785 removed insertion comparisons, unchanged callbacks and remaining linear
candidate bound. Core, proof rules and image format are unchanged.

Verification (all exit 0): strict debug complete `tests/source_io.sh`; optimized
`make -f src/prototype/pointer/Makefile -j2
BUILD=/tmp/a-program-authority-source-sites-opt check-acceptance`; ASan/UBSan
complete `source_io.sh`, `normalization`, `handler-nesting`, `handler-boundaries`
(4,240 snapshots). Flags match the preceding epoch: debug `-O0 -g`, optimized
`-O2`, sanitizer `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-pie -no-pie`, all with C11/Wall/Wextra/Werror; leak checking and halt on
sanitizer errors enabled. Logs: `/tmp/a-program-authority-siblings-*.log`.
All normalized `export results:` records, including Solve steps, match the
preceding full acceptance log. Compatibility remains 63/63.

Per-file implementation: `source_io.c` +5/-10 (net -5); tests: `source_io.c`
+28/-0. Cumulative implementation/header counts excluding tests: R76
`3a3bf550` +2,355/-1,064 (net +1,291); R0 `4657cc6` +7,314/-3,607
(net +3,707). The parent net-negative gate and remaining A3/A4/A5 work are
still open. Keep this small verified correction for the next substantial
source-traversal publication; do not relabel it as completion of R.

### 2026-09-19: Family quotation respects incremental origin work

- [x] Reproduce synchronous origin work escaping the one-step source adapter.
- [x] Use the existing budgeted query in `family_function_step`, without adding
  job roles, state fields, proof rules or another cache.
- [x] Strict debug `synthesis_test`, full optimized `check-acceptance`, and
  ASan/UBSan `synthesis_test` plus complete `source_io.sh` pass (exit 0).
- [x] Publish with the next substantial authority-refactoring milestone (`a9b24b2`).

Baseline: `6a781cc`. `source_telescopes`' new per-step assertion fails on the
old implementation (exit 134), then passes with the adapter change. Logs:
`/tmp/a-program-authority-family-query-{before,after,acceptance,asan-synthesis,asan-source}.log`.
Build flags match the preceding entry. Normalized `export results:` including
Solve steps are identical to the preceding acceptance run; compatibility is
63/63 and the universal QuickSort tests pass. No wall-time improvement claimed.

Per-file counts: `synthesis.c` +4/-3 (net +1); `tests/synthesis.c` +11/-1
(net +10). The two unpublished corrections together reduce implementation by
four lines, but R76-to-current implementation/header growth remains +1,292.
A3/A4/A5 and the parent net-negative gate remain open. The parent A4 entry
identifies remaining synchronous graph consumers without calling shared,
interned requests duplicate semantic authorities.

### 2026-09-19: Specialization reuse and remaining representation cost

- [x] Verify capture-avoiding function specialization shares the existing typed
  body and context lift, including 100 repeated requests at chunks 1/64.
- [x] Strict debug and ASan/UBSan `program_test`, full optimized
  `check-acceptance`: exit 0. Compatibility remains 63/63; universal QuickSort
  and image cases pass. Normalized `export results:` including step counts
  exactly match the preceding family-query acceptance run.
- [x] Repeat the R0/current source and zero-work-image diagnostic matrix.
- [x] Publish with a substantial refactoring epoch, not this test-only audit (`a9b24b2`).

The re-audit plan's A4/A5 entries record the rejected extra specialization API,
the regression's exact reuse checks, timings and node counts. Implementation
is unchanged (`555b13d`); `tests/program.c` adds 35 lines. Verification logs:
`/tmp/a-program-authority-specialized-view-{debug-verified,asan,acceptance}.log`.
Build flags and sanitizer environment match the preceding entry.

R0/current median QuickSort source time is .859/.326 seconds, but length is
.0093/.0122 and function-field .0121/.0213. The latter's typed subjects grow
4524 -> 6717, proofs 7927 -> 9371. These are investigation inputs, not grounds
for merging differently typed uses or dropping certificates. A3 scope
enumeration, A4's remaining consumers, full final A5 gates and cumulative
net-negative implementation/header LOC remain open.

### 2026-09-19: One structural check for shared context lifts

- [x] Centralize lift parent/freshness checks at request creation, retaining
  proof ownership and logical substitution validation. Remove the private
  builder/adapter copies and the repeated scope walk on an intern hit.
- [x] Verify valid reuse interleaved with wrong-parent, occupied-binder and
  null-binder requests; no extra lift or proof records appear.
- [x] Strict debug `core_test`, full optimized `check-acceptance`, ASan/UBSan
  `core_test`, `iadt_test`, `synthesis_test`, `program_test`, complete
  `source_io.sh`: all exit 0. Flags/environment match preceding entries.
- [x] Publish with the next substantial refactoring epoch (`a9b24b2`).

Baseline: `c49901d`. A4 records the exact check ownership and 1,589 -> 436
scope lookups measured on the same function-field input. Logs use
`/tmp/a-program-authority-intern-{before-counts,after-counts,core,acceptance,asan-*}.log`.
Compatibility is 63/63; normalized export results, including Solve steps,
match the preceding full acceptance log. No general speedup is claimed.
Per-file implementation: `typing.c` +3/-3, `evidence.c` +0/-2 (net -2);
`tests/core.c` +9/-0. Cumulative implementation/header delta is still +1,290
from R76, +3,706 from R0. Broad A3/A4/A5 and net-negative gates remain open.

### 2026-09-19: Reuse validated Context-map structure

- [x] Move source-length validation after exact map lookup; retain image
  validation and reject incorrect image counts/Contexts. A4 records the
  measured 4,021 -> 2,657 length walks with unchanged Solve steps.
- [x] Strict debug `core_test`, full optimized `check-acceptance`, ASan/UBSan
  core/IADT/synthesis/program and complete `source_io.sh`: all exit 0.
  Flags/environment match the preceding entry; logs use
  `/tmp/a-program-authority-map-hit-*.log`. Normalized export results and
  Solve steps exactly match the preceding full acceptance run.
- [x] Publish with a substantial refactoring epoch, not this small correction (`a9b24b2`).

Baseline: `923a220`. `typing.c` +2/-2; `tests/core.c` +5/-0.
Cumulative implementation/header LOC and remaining gates are unchanged.

### 2026-09-19: Suspend shared function-graph input queries

- [x] Replace synchronous input/origin query draining in computation views;
  distinguish pending from unsupported in preparation, cases and helper calls.
  Retain the borrowed in-flight query so suspension does not repeatedly inspect
  the surrounding view. No new worker, semantic cache or acceptance rule.
- [x] The new `graded_function_graph` budget regression fails on `322026f`
  (exit 134), then passes. Strict debug `program_test`, full optimized
  `check-acceptance`, ASan/UBSan program/synthesis, complete `source_io.sh` and
  complete `sort_insertion.sh` all exit 0 on the final implementation.
  Compatibility is 63/63; exported results match the preceding acceptance run
  after excluding the intentionally changed Solve transition counts.
- [x] Publish with the next substantial refactoring epoch (`a9b24b2`).

Logs: `/tmp/a-program-authority-graph-view-park-*.log`; flags/environment match
the preceding entries. The initial unparked draft's additional sanitizer sort
run was deliberately stopped after its repeated-view regression was measured;
the final complete run above supersedes it. A4 records before/draft/final call
counts and unchanged proof/occurrence/query totals. Five alternating strict-debug
source runs give baseline/final medians: function-field .02190/.02471 seconds;
QuickSort .33307/.32676 seconds. These short diagnostics are not a speedup claim;
the function-field variation remains relevant to the final performance audit.

`function_graph.c` +33/-10 (net +23); `tests/program.c` +4/-0. Cumulative
implementation/header deltas: R76 +2,396/-1,083 (net +1,313); R0 +7,342/-3,613
(net +3,729). A3's scope-sensitive discovery, remaining A4 consumers, final A5
gates and the parent's net-negative requirement remain open.

### 2026-09-19: Discover shared Context dependencies without packing

- [x] Remove intermediate allocation-payload packing from source discovery.
  Reuse one writer-local Context DAG and the packer's parent/index iterator;
  retain final wire ordering and structural validation. No image format change.
- [x] Add shared-parent/family-index traversal coverage (128 repeated additions,
  five nodes, no Core/evidence allocation). Strict debug complete source and
  graph-acceptance scripts, full optimized `check-acceptance`, and ASan/UBSan
  complete source/graph-acceptance/derivation scripts all exit 0.
- [x] Publish with the next substantial source/authority-refactoring epoch (`a9b24b2`).

Baseline: `8d0ddc9`; logs: `/tmp/a-program-authority-context-discovery-*.log`.
Flags/environment match preceding entries. Compatibility is 63/63; normalized
export results including Solve steps exactly match the previous acceptance run.
The before/after retained IF8 images are byte-identical (420,556 bytes).
A3 records removed packing calls and the measured memory tradeoff.

Per-file implementation: `context_payload.c` +2/-2, `context_payload.h` +3/-0,
`source_io.c` +13/-10 (total net +6); tests: `graph_acceptance.c` +11/-0.
Cumulative implementation/header deltas: R76 +2,413/-1,094 (net +1,319);
R0 +7,359/-3,624 (net +3,735). Remaining A3/A4/A5 and net-negative gates stay open.

### 2026-09-19: Shared queries and direct dependency collection publication

This publication groups the seven previously unpublished corrections with the
direct dependency collector, not one push per small edit. The coherent scope
is reuse of existing typed requests and immutable dependency structure:

- [x] Remove quadratic candidate insertion; preserve sibling-source isolation.
- [x] Advance family/graph view queries through existing Solve work, retaining
  the pending shared query instead of rediscovering its surrounding structure.
- [x] Verify capture-avoiding specialization reuse; validate Context lifts once
  and skip repeated Context-length walks on exact map intern hits.
- [x] Discover shared Context dependencies once. Remove disposable derivation
  wire payloads from dependency selection and the second root-array traversal.
- [x] Full optimized acceptance: exit 0, compatibility 63/63, insertion/tree/
  merge/quick universal proofs pass. Normalized result records including Solve
  steps match `authority-context-discovery-acceptance.log`.
- [x] Strict debug and ASan/UBSan Source, Derivation and Graph scripts: exit 0.
  Repeated direct collection matches packed object dependencies; no new proof
  acceptance or Solve occurs during discovery. Retained IF8 output is unchanged;
  function-field inert resave preserves the same input image exactly.
- [x] Publish the grouped epoch and verify both remote tips (`a9b24b2`).

The re-audit's A3 entry records exact commands/build flags via the previous
entries, logs, 697 removed intermediate payload calls, arena savings and the
non-deterministic fresh-allocation comparison that was not a valid byte-stability
test. No inference rule, source syntax or image format changes in this epoch.

Per-file counts from remote baseline `605d11b`, before this documentation entry:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| context_payload.c | 26 | 7 | +19 |
| context_payload.h | 9 | 0 | +9 |
| derivation_io.c | 58 | 38 | +20 |
| derivation_io.h | 7 | 3 | +4 |
| evidence.c | 0 | 2 | -2 |
| function_graph.c | 33 | 10 | +23 |
| source_io.c | 19 | 28 | -9 |
| synthesis.c | 4 | 3 | +1 |
| typing.c | 5 | 5 | 0 |
| **Implementation/headers** | **161** | **96** | **+65** |
| tests/core.c | 14 | 0 | +14 |
| tests/derivation_io.c | 37 | 0 | +37 |
| tests/graph_acceptance.c | 23 | 0 | +23 |
| tests/program.c | 39 | 0 | +39 |
| tests/source_io.c | 28 | 0 | +28 |
| tests/synthesis.c | 11 | 1 | +10 |
| **Tests** | **152** | **1** | **+151** |

Cumulative implementation/header changes are R76 +2,510/-1,149 (net +1,361),
R0 +7,455/-3,678 (net +3,777). The overall source-reduction requirement is not
met. A3's scope-sensitive source selection, A4's remaining structural consumers
and final A5 acceptance remain open; publishing this epoch does not complete R.

Published atomically without force from `605d11b` to `a9b24b2`; remote Main
and `rewrite/pointer-core-hott` both verified at that revision. Documentation
through that implementation commit is +422/-1, separate from source/test counts
above. This follow-up only records publication and checks the grouped entries.

### 2026-09-19: Lexically bounded source-origin discovery

- [x] Replace global syntax-keyed allocation lookup with existing lexical-root
  keys; retain exact syntax, binder and scope reachability checks.
- [x] Keep dispatch in syntax-frontier order and detach event waiter lists
  before callbacks can grow/rehash the dependency index.
- [x] Test 128 unrelated scopes, append's recursive constructor origin, and
  two byte-identical inert retained QuickSort resaves without Solve.
- [x] Strict debug source checks; full optimized `check-acceptance`;
  source/image ASan/UBSan checks (all exit 0).
- [x] Group publication with the source-transport/query-resumption epoch below.

The parent A3 entry records rejected draft behavior, exact work/space counts
and remaining bounds. No semantic, source syntax or image-format change.
Optimized flags: `-std=c11 -Wall -Wextra -Werror -O2`; sanitizer flags add
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
Sanitizers use `detect_leaks=1:halt_on_error=1` and UBSan `halt_on_error=1`.
Logs: `/tmp/a-program-authority-lexical-roots-final-{acceptance,asan-source,asan-image}.log`.
Normalized export results, including step counts, match the preceding direct
dependency collector run. Compatibility remains 63/63.

| File (under `src/prototype/pointer/`) | Added | Deleted | Net |
|---|---:|---:|---:|
| `source_io.c` | 65 | 12 | +53 |
| `synthesis.c` | 11 | 1 | +10 |
| `synthesis.h` | 4 | 1 | +3 |
| `tests/image_cli.sh` | 14 | 0 | +14 |
| `tests/source_io.c` | 51 | 1 | +50 |

Implementation/headers +80/-14; tests +65/-1. Documentation accounting is
separate at publication. Cumulative implementation/header net is +1,427 from
R76 (`3a3bf550`) and +3,843 from R0 (`4657cc6`): the reduction gate is not met.
Remaining A3/A4/A5 work is not closed by this local correction.

### 2026-09-19: Match export as an ordered origin subset

- [x] Delete the separate writer Match DAG and second allocation projection.
  Keep its first projection in a temporary ordered list; wire IDs follow the
  existing origin order. No new persistent state or format/version change.
- [x] Add nested-Match coverage to the existing origin test: three inert
  resaves, exact restored allocation checks, alpha-equivalence and normal forms.
  The original single-Match pointer-equality/invalid-input checks remain.
- [x] Final strict-debug source script, full optimized `check-acceptance`,
  ASan/UBSan source and image scripts: all exit 0. Flags match the preceding
  entry. Logs: `/tmp/a-program-authority-match-origin-stream-final-*`.
- [x] Publish with the source-transport/query-resumption epoch below.

IF8 retained QuickSort bytes match `5a853e0` exactly. Match-allocation queries
fall 14 -> 7; selected arena usage falls 896 bytes plus removal of a 512-byte
bucket table and 16,384 bytes of reserved arena. No elapsed-speedup claim.
Normalized export results/steps match the preceding full run; compatibility
63/63 and all universal sorting proof tests pass. The new nested test's first
draft incorrectly assumed exact regenerated Core identity and only two solved
candidates; both assumptions fail on the previous writer too. The parent A3
entry explains the corrected assertions and the still-unproven reuse bound.

Per-file diff from `5a853e0`: `source_io.c` +33/-27 (net +6),
`tests/source_io.c` +23/-7 (net +16). Documentation is accounted separately.
Cumulative implementation/header changes: R76 +2,584/-1,151 (net +1,433),
R0 +7,529/-3,680 (net +3,849). The net-negative gate and remaining A3/A4/A5
requirements are still open; neither local commit completes R.

### 2026-09-19: Shared structural Context-map extension

- [x] Replace separate lift/instantiation image-array construction with one
  private structural helper. Preserve image checking and alternative proof DAGs.
- [x] Add canonical-map/request identity and no-proof-allocation regressions.
- [x] Strict debug Core; full optimized acceptance; ASan/UBSan Core and complete
  source-image runner: exit 0. Compatibility 63/63 and all four universal sort
  proof suites pass. Normalized exported results and steps match `fa13cb7`.
- [x] Publish with the grouped epoch below; this is not final A3-A5 acceptance.

Commands use `make -f src/prototype/pointer/Makefile -j2`, with debug build
`/tmp/a-program-authority-context-extend` and flags
`-std=c11 -Wall -Wextra -Werror -O0 -g` (target `core_test`), optimized build
`/tmp/a-program-authority-source-sites-opt` and `-O2` (target `check-acceptance`).
Sanitizer build `/tmp/a-program-authority-source-sites-asan` uses
`-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`; run `core_test` and
`bash src/prototype/pointer/tests/source_io.sh <build>/source_io_test` with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1`.
Logs: `/tmp/a-program-authority-context-extend-{core,opt,asan-core,asan-source}.log`.
The initial added test reused a local name and failed compilation; the name
was corrected and all listed tests rerun against freshly built binaries.

GDB on `function-graph-function-field.p --steps 1000000` records unchanged
Solve steps (12,809), map interning calls (4,021), projection requests (2,081),
instantiation requests (156), and all other recorded consumer counts. This
consolidates code, not computation or elapsed time. Count log:
`/tmp/a-program-authority-context-extend-counts.log`.

Delta from `fa13cb7`: `typing.c` +22/-22 (net 0), `tests/core.c` +21/-0.
Documentation is separate. Implementation/header net changes remain +1,433
from R76 and +3,849 from R0. No code-reduction gate is claimed complete.

### 2026-09-19: One producer projection for source transport

- [x] Unify producer classification across dependency enumeration, input
  collection and encoding. Keep a stack-local read-only view; do not cache
  results or change source-image modes, logical rules or acceptance.
- [x] Strict debug source runner and normalization tests; optimized full
  `check-acceptance`: exit 0, compatibility 63/63 and four universal sort proofs.
  Exported results and Solve counts match `5231d51`'s full run.
- [x] Eleven `retained-write` fixtures produce byte-identical images to the
  pre-refactor writer (lambda, family, append, index-alias, function-field,
  nominal, nullary, application, constructor, match, fold).
- [x] ASan/UBSan source, normalization and complete image CLI checks: exit 0.
- [x] Group publication with the epoch below. The lexical candidate bound
  and A3-A5 remain open.

Existing permanent tests cover the changed cases: `normalization_requests`
checks four WHNF/NF/force modes and invalid Contexts at pending/settled states;
`annotation_sources` and source/image runners retain independent failing checks,
constructor/operation wrappers, definitions and inert resaves. No tests were
removed or weakened. Debug build `/tmp/a-program-authority-producer-view` uses
`-std=c11 -Wall -Wextra -Werror -O0 -g`. Optimized/sanitizer builds, commands and
flags are the same as the preceding entry. Logs use prefix
`/tmp/a-program-authority-producer-view-`.

`source_io.c`: +54/-69, net -15; tests unchanged. Documentation is separate.
Cumulative implementation/header net remains +1,418 from R76 and +3,834 from
R0; the whole-refactor net-negative requirement is not satisfied.

### 2026-09-19: Grouped-epoch verification checkpoint

- [x] Full debug and ASan/UBSan acceptance at `dd9cabc`, supplementing the
  already passed optimized full gate: all exit 0; source compatibility 63/63.
- [x] Repeat the R0/current append, length, function-field and imported QuickSort
  source/image timing and allocation comparison. See parent A5 for exact
  commands, results and remaining small-input regressions.
- [ ] Complete the remaining A3/A4 cleanup and A5 requirements; publish only a
  substantial tested epoch, not each local helper consolidation.

Surface and Issue29/PR30 publication milestones remain complete. This checkpoint
does not publish the four local implementation commits or close the remaining
refactoring gates. No implementation or test files changed during verification.

### 2026-09-19: Resume helper inspection without rebuilding its prefix

- [x] Add a projected-application/Fold regression in `graded_function_graph`:
  each graph step advances the selected origin query by at most one step.
  It fails before repair (exit 134) and passes afterward. Simpler initial test
  drafts did not exercise a multi-step helper origin and were replaced.
- [x] Share the one-step construction-origin adapter with computation views.
  Preserve the helper's local cursor/arguments while the shared query waits;
  do not add a result cache, semantic descriptor, proof authority or wire tag.
- [x] Debug program runner; optimized full `check-acceptance`; sanitizer program
  runner: all exit 0. Compatibility 63/63 and all four sorting proofs pass.
- [x] Publish with the epoch below; A3/A4/A5 and the reduction gate remain
  open. This is not a speedup or a constant-cost guarantee for every graph step.

Commands use the existing pointer Makefile with `-j2`: debug build
`/tmp/a-program-authority-helper-origin`, strict C11/O0/g, targets `program_test`
and `pointer-check`; optimized build `/tmp/a-program-authority-source-sites-opt`,
strict C11/O2, target `check-acceptance`; sanitizer build
`/tmp/a-program-authority-source-sites-asan`, the existing O1 ASan/UBSan flags,
target `program_test`. Run the latter with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
Final logs use `/tmp/a-program-authority-helper-cursor-{program,opt,asan}.log`.
Earlier `helper-origin-*` logs describe the withdrawn retry-from-start draft.

Five alternating fresh-process O0/g samples compare `dd9cabc` against the final
cursor implementation, with 1,000,000 steps and per-version zero-step images.
No concurrent build/test ran during timing. Raw samples:
`/tmp/a-program-authority-helper-cursor-matrix.log`.

| Input | Source median seconds before/after | Image median seconds before/after | Source steps before/after |
|---|---|---|---|
| Vec-append | .014570 / .015171 | .014631 / .016515 | 19935 / 19935 |
| length | .014837 / .013021 | .014120 / .012328 | 9694 / 9702 |
| function-field | .021709 / .023063 | .022522 / .021820 | 12809 / 12883 |
| QuickSort-property | .332658 / .334174 | .333060 / .332589 | 146809 / 147083 |

All 80 solves exited 0. Small timings are noisy; do not infer a speedup.
Function-field and QuickSort retain exactly the preceding Core/occurrence/proof,
Context and synthesis-job counts. Extra steps expose previously synchronous
query work. GDB helper allocation logs show 29 direct allocations before/after,
despite 39 pending resumptions; no repeated argument-prefix allocation.
Logs: `/tmp/a-program-authority-helper-cursor-{before,after}-allocs.log` and
`/tmp/a-program-authority-helper-cursor-{function_field,quicksort}-counts.log`.

Implementation: `function_graph.c` +91/-65 (net +26). Tests: `program.c` +25/-0.
Documentation is separate. Cumulative implementation/header net is +1,444
from R76 and +3,860 from R0; net-negative completion is not established.

### 2026-09-19: Source transport and query-resumption publication gate

This epoch groups `5a853e0`, `fa13cb7`, `5231d51`, `dd9cabc`, `fcd9d25`
and their verification records, relative to published Main `a0cbc2e`.
It bounds named-scope origin discovery, removes duplicate Match transport and
producer classification, shares structural map extension, and suspends helper
inspection without rebuilding its argument prefix. No source syntax, kernel
rule, structural equality policy or image format changes.

- [x] Extend the helper regression to cancel at every step up to completion
  using fresh mapped inputs. Resume the shared origin query after destroying
  the graph work that borrowed it. The destroyed graph handle retains no state.
  GDB confirms four destructions with a nonempty helper argument list, not
  merely cancellation before allocation. Sanitizer leak checking passes.
- [x] Full strict-debug, optimized and ASan/UBSan `check-acceptance`, all exit 0.
  Each passes compatibility 63/63, four universal sorting proof suites, source
  image modes and existing positive/negative boundary tests. No sanitizer or
  runtime-error diagnostics. Syntax inventory independently matches 158 entries.
- [x] Normalized exported results, including steps, agree with `fcd9d25`'s
  optimized run. Cancellation coverage changes tests only.
- [x] Commit, atomically publish to Main/rewrite, and verify remote revisions.

Commands: `make -f src/prototype/pointer/Makefile -j2 check-acceptance`, with
`BUILD=/tmp/a-program-authority-helper-origin` and strict C11/O0/g;
`BUILD=/tmp/a-program-authority-source-sites-opt` and strict C11/O2;
`BUILD=/tmp/a-program-authority-source-sites-asan` and strict C11/O1/g,
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
The entire sanitizer make invocation inherits
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
Logs: `/tmp/a-program-authority-resumption-epoch-{debug,opt,asan}.log`;
cancellation inspection: `/tmp/a-program-authority-helper-cancel-phases.log`.
No source/test edits occurred during these final gates.

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| function_graph.c | 91 | 65 | +26 |
| source_io.c | 152 | 108 | +44 |
| synthesis.c | 11 | 1 | +10 |
| synthesis.h | 4 | 1 | +3 |
| typing.c | 22 | 22 | 0 |
| tests/core.c | 21 | 0 | +21 |
| tests/image_cli.sh | 14 | 0 | +14 |
| tests/program.c | 35 | 0 | +35 |
| tests/source_io.c | 74 | 8 | +66 |

Implementation/headers: +280/-197 (net +83); tests: +144/-8 (net +136).
Documentation is separate. Overall cleanup is not complete: A3's transparent
descendant candidate bound, A4's remaining consumer audit, final performance
regressions and the net-negative requirement remain open. The preceding timing
checkpoint still describes this implementation; this is not a speedup claim.

Published atomically without force from `a0cbc2e` to
`f8e71f1311203ea0e772de1dfa0d5a10558c187b`; both remote Main and
`rewrite/pointer-core-hott` were checked at that revision. This supersedes
the preceding local-only checkpoint descriptions for the grouped commits.
Documentation at that publication: authority plan +153/-9; priority plan
+240/-0. Overall epoch +817/-214, net +603, including documentation and tests.
These counts exclude this documentation-only publication follow-up.

### 2026-09-19: Count source payloads at collection (local)

- [x] Remove the separate scope and binding payload-count traversals. Accumulate
  slot counts when the existing source frontier first reaches each input;
  skip already collected bindings through the existing DAG. Verify emitted
  context/reference counts. No new index, solver state, format or Core rule.
- [x] Extend source-binding tests with repeated roots across unsolved, partially
  solved, accepted and rejected annotations, including two inert resaves.
- [x] Strict-debug `source_io.sh`; optimized full `check-acceptance`: exit 0,
  compatibility 63/63 and all four universal sorting proof suites pass.
  Normalized exported results, including steps, match the preceding epoch.
- [x] Eleven retained fixtures: old/new writers resaving the same input produce
  byte-identical output. The original fresh Match image changes four ID bytes
  on its first resave with the old binary too; this comparison does not claim
  fresh-write/resave byte identity for that baseline case.
- [x] ASan/UBSan source-image and Handler-boundary suites: exit 0 with explicit
  leak detection and halt-on-error options; 4,240 Handler snapshots preserve
  status/effects. This is affected-suite sanitizer coverage, not a new full run.
- [x] Include in the established-input reuse publication epoch (`a093e54`).

Against `4b4fba7`, implementation `source_io.c`: +12/-17 (net -5); test
`tests/source_io.c`: +6/-3 (net +3). GDB on retained function-field writing:
`environment` calls 126 -> 105; `pg_synthesis_binding_input` calls 21 -> 18.
This removes work; no wall-time speedup is claimed. Logs and temporary images
use `/tmp/a-program-authority-payload-count-*`; debug build uses the same
prefix, O2 uses `authority-source-sites-opt`. These local results do not close
the broader A3/A4 audit or the cumulative net-negative gate.

### 2026-09-19: Explicit source-origin continuations (local)

- [x] Remove allocation probing from source-reference enumeration; select syntax
  before reading the producer's allocation. Keep the available-allocation visitor
  unchanged. Existing writer-local waiters hold the callback/input to resume,
  replacing key-based phase reconstruction and another source-header inspection.
  No new table, solver, persistent authority or format is introduced.
- [x] Add 128 unselected transparent scopes with pending, descriptively allocated
  member uses sharing the selected syntax/binder. Saving the selected root is
  byte-identical and does not advance Solve or produce proofs.
- [x] Strict-debug source-image suite and eleven controlled old/new retained
  resaves pass. All eleven resaved outputs are byte-identical.
- [x] Final optimized full acceptance: compatibility 63/63 and all four universal
  sorting proof suites pass; normalized exported results/steps match `135dc55`.
- [x] ASan/UBSan `source_io.sh`, `handler-boundaries` (4,240 snapshots) and
  `prepared-module` (1,118 snapshots) pass with leak detection and halt-on-error.
  These are affected-suite sanitizer gates, not a new full sanitizer run.
- [x] Publish with the established-input reuse epoch (`a093e54`), not separately.

GDB comparison against `135dc55`: the 128-scope case makes 267 -> 134 allocation
reads (6 without the added scopes in the new writer); environment reads remain
224. Ordinary retained function-field writing: allocation reads 17 -> 12,
source-input reads 100 -> 91, environment reads unchanged at 109. Waiter calls
increase 12 -> 15 because unprepared candidates can await syntax. No wall-time
or overall-memory improvement is claimed. Candidate enumeration remains open.

A rejected draft checked scope before object reachability. It cut the 128-scope
allocation reads to 6, but increased ordinary environment reads 109 -> 117.
The final ordering avoids that additional ancestry work. Initial testing also
caught a test visitor assuming every source candidate already had an allocation;
that consumer now explicitly selects available allocations under the new API.

Implementation/header diff: `source_io.c` +29/-38, `synthesis.c` +6/-11,
`synthesis.h` +3/-3; total net -14. Test `tests/source_io.c` +16/-0.
Logs/images use `/tmp/a-program-authority-origin-order-*`; `final-counts` and
`access-final` describe the final implementation, not the rejected draft.

### 2026-09-19: One checked call-telescope algorithm (local)

- [x] Replace four Pi-domain/context/application loops in `evidence.c` and
  `function_graph.c` with `pg_prove_call_telescope`. Reuse accepted application
  classifiers, including dependent domains, instead of a second codomain path
  in callable-parameter eta expansion. No new rule, solver, cache or wire format.
- [x] Test dependent telescope application against ordinary APP construction,
  fresh versus retained binders, 100 repeated requests without proof/occurrence
  growth, zero-argument identity, invalid allocation/context/category, and output
  preservation on failure. Allocation annotations are deliberately unrelated:
  only their binder identities may influence construction, never their types.
- [x] Strict-debug Core/program tests and optimized full acceptance pass:
  compatibility 63/63 and all four universal sorting proof suites. Normalized
  exported results including Solve steps match the previous full run exactly.
- [x] ASan/UBSan Core, IADT, program, callable-parameter, dependent function-field
  and invalid-callable tests pass with explicit leak detection/halt-on-error.
  This is affected-suite coverage, not a new full sanitizer acceptance run.
- [x] Publish with the established-input reuse epoch (`a093e54`). Broader A3/A4 and the cumulative
  net-negative implementation gate are not closed by this consolidation.

The initial new test incorrectly required the generated Context *receipt* to
equal an independently constructed receipt. Pi-domain formation can supply a
different derivation of the same Context. The test now checks Context identity,
the exact application Core/classifier, reconstruction of the derivation, and
exact receipt reuse for repeated identical requests. Existing tests were not
weakened; alternative accepted proofs remain retained.

GDB, `function-graph-callable-parameter.p`, both chunk sizes 1 and 64:
within `parameter_source`, Pi-codomain calls 12 -> 6 and projection calls
18 -> 12; classifier-query calls 48 -> 54. Each run retains 9,388 Core terms,
6,306 typed occurrences, 538 contexts and 4,766 jobs; proof records 8,619 ->
8,613, Solve steps unchanged at 16,294. This deletes redundant work, not
existing proofs. No wall-time improvement or bounded-per-step guarantee is
claimed. Baseline is the previously verified `authority-helper-origin` debug
binary; the two intervening source-writer commits do not change these paths.

Diff against `1dd755c`: `evidence.c` +34/-30, `evidence.h` +10/-0,
`function_graph.c` +11/-25; implementation/header net 0. Tests +39/-0.
The reduction is three duplicated algorithms, not a net LOC reduction yet.
Logs and the new debug binaries use `/tmp/a-program-authority-call-telescope-*`.

### 2026-09-19: Repair the Pi formation lookup key (local)

- [x] Audit early `find_record` calls against their insertion keys. Pi formation
  searched with a NULL output but inserted with its constructed output, defeating
  early reuse. Its exact context-extension and codomain premises determine that
  output. Add `PG_PI_FORM` to the existing derived-output rules; no new cache,
  evidence authority, inference rule or format is needed.
- [x] Test two alternative codomain derivations with the same typed conclusion:
  retain distinct Pi receipts, reuse each exact receipt for 100 repeated requests,
  preserve premises, reconstruct both, and reject wrong context/category inputs.
  This protects key semantics; it is not a previously failing semantic test.
- [x] Strict-debug Core/program tests pass.
- [x] Optimized full acceptance: compatibility 63/63 and all four universal
  sorting proof suites pass. Normalized exported results and Solve steps match
  the preceding call-telescope run exactly.
- [x] ASan/UBSan Core, IADT, program, callable-parameter, function-field and
  `derivation_io.sh` pass with explicit leak detection and halt-on-error.
  This is affected-suite coverage, not a new full sanitizer acceptance run.
- [x] Include in the established-input reuse publication epoch (`a093e54`).

GDB on the new Core test: 306 Pi requests, 93 `binding_level` calls. A controlled
debugger run forcing only Pi's `derived_output` result back to the old false
value has the same 306 requests and 301 `binding_level` calls; both runs pass.
That function has only the Pi constructor as a caller. The experiment therefore
isolates 208 avoided reconstructions without adding production counters. On
`function-graph-function-field.p`, both old and new binaries make 606 Pi requests
and 606 such calls; Solve remains 13,531 steps for chunks 1 and 64. Do not infer
a general wall-time improvement from the repeated-request test.

Implementation: `evidence.c` +1/-1 (net 0); test `tests/core.c` +19/-0.
Logs: `/tmp/a-program-authority-pi-key-*`. Broader A3-A5, lexical enumeration,
small-case performance and cumulative net-negative implementation remain open.

### 2026-09-19: Reuse the structural projection in substitution introduction

- [x] Replace the evidence layer's separate ancestor/arity walk and binder
  enumeration with the existing `pg_context_map_projection` result. Its ordered
  images supply the binders; ordinary variable and substitution rules still
  check them. Do not accept descriptive maps or select an arbitrary existing
  substitution proof in place of the caller's source/destination receipts.
- [x] Test dependent projections, repeated requests, alternative Context
  derivations and invalid source/destination pairs. Measure avoided Context
  traversals separately from allocations and total Solve steps.
- [x] Run strict-debug Core/program, full optimized acceptance and affected
  sanitizer/serialization tests. Compatibility 63/63 and all four universal
  sorting proof suites pass; normalized exported results/Solve steps match the
  preceding Pi-key run. ASan/UBSan Core, IADT, program, callable-parameter,
  function-field and `derivation_io.sh` pass with leak detection/halt-on-error.
- [x] Group publication with the established-input reuse epoch (`a093e54`).

This uses the existing structural map interner and projection index, not a new
cache or proof rule. The proposed deletion is the duplicate projection recipe;
proof checking remains a distinct responsibility. General map validation and
lexical source-candidate enumeration are not covered by this change.

Strict-debug Core/program tests pass. The added test uses two distinct receipts
for each of the same source/destination Contexts, checks exact premise retention,
shared structural map identity, 100 repeated requests per receipt, no additional
proofs/maps on repetition, reconstruction, and reversed/unrelated scope rejection.
Variable-proof construction remains in reverse telescope order as before.

GDB on `function-graph-function-field.p`, chunks 1 and 64 together:
1,044 projection requests before and after. Context-extension calls beneath
these requests fall from 3,550 to 2,306; traversed parent links from 4,912 to
3,952. The old wrapper's 2,088 direct extension calls disappear. Some first-use
work moves to the common structural factory, so those 2,088 calls are not all
net savings. Solve stays 13,531 steps per chunk size.

Per-run retained counts are unchanged: 5,634 Core terms, 6,808 typed occurrences,
9,521 proofs, 667 Contexts, 2,700 Context maps and 4,158 jobs. The existing
projection index grows from 759 to 943 entries: common factory reuse costs 184
additional borrowed index entries on this input. No memory or wall-time win is
claimed. Implementation `evidence.c` +8/-10 (net -2); `tests/core.c` +22/-0.
Logs and debug binaries: `/tmp/a-program-authority-projection-owner-*`.

### 2026-09-19: Established-input reuse epoch, publication checkpoint

Freeze the implementation after the projection-owner change and verify the
group, relative to published `4b4fba7`: first-collection payload counts,
source-origin continuation reuse, one checked call-telescope algorithm, exact
Pi-premise reuse, and structural projection reuse. The individual changes above
are not separate publication epochs. This group removes duplicate traversal and
construction recipes while retaining source inputs, typed structure and exact
proof premises in their existing owners; it adds no second solver or format.

- [x] Optimized full acceptance on the final implementation.
- [x] Strict-debug full acceptance on the same implementation. All normalized
  exported results and Solve steps match the optimized run exactly.
- [x] ASan/UBSan full acceptance on the same implementation, with leak detection
  and halt-on-error. The full make invocation exited 0. All normalized exported
  results and Solve steps match debug and optimized runs exactly; compatibility
  63/63 and all four universal sorting proof suites pass.
- [x] Record per-file implementation/header, test and documentation deltas.
- [x] Recheck remote tips, publish the group to Main/rewrite without force, and
  verify both remote revisions. Preserve all open parent gates.

A3/A4/A5, output-sensitive lexical enumeration, small-input performance against
R0 and cumulative net-negative implementation/header LOC remain incomplete.

Frozen implementation revision: `497452f`. Relative to published `4b4fba7`:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| evidence.c | 43 | 41 | +2 |
| evidence.h | 10 | 0 | +10 |
| function_graph.c | 11 | 25 | -14 |
| source_io.c | 41 | 55 | -14 |
| synthesis.c | 6 | 11 | -5 |
| synthesis.h | 3 | 3 | 0 |
| Implementation/headers | 114 | 135 | -21 |
| tests/core.c | 80 | 0 | +80 |
| tests/source_io.c | 22 | 3 | +19 |
| Tests | 102 | 3 | +99 |

Documentation through `497452f`, excluding this later accounting/publication
record: authority plan +44/-0; priority plan +202/-0. No build-rule changes.
Across the entire refactor, implementation/headers remain +2,780/-1,357 (net
+1,423) from R76 `3a3bf55`, and +7,679/-3,840 (net +3,839) from R0 `4657cc6`.
The grouped -21 reduction does not satisfy the cumulative reduction gate.

Final verification uses C11 with `-Wall -Wextra -Werror`; debug `-O0 -g`,
optimized `-O2`, sanitizer `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`. The entire sanitizer make invocation
inherits `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. All use the prototype Makefile's existing
`check-acceptance` target. Logs: optimized
`/tmp/a-program-authority-projection-owner-opt.log`, debug/sanitizer
`/tmp/a-program-authority-established-inputs-{debug,asan}.log`.

All three full acceptance invocations exited 0. There were no source/test edits
between the frozen revision and these gates. This completes verification of the
five-change publication group, not the remaining parent refactor obligations.

Published atomically without force from `4b4fba7` to
`a093e54d1ca8d32d7d8e80509ab820e818cc1ec3`; `git ls-remote` confirmed both
Main and `rewrite/pointer-core-hott` at that revision. Documentation at
publication: authority plan +44/-0; priority plan +239/-0. Implementation and
test totals are unchanged from the table above. This publication-record update
only checks the grouped entries and records the verified remote revisions.

### 2026-09-19: Profile the remaining small-input overhead (local)

Current `cbf81fe` was profiled with C11/O2/g/pg on 100 fresh function-field
source solves. Gprof records 6,820,400 index insertions and 10,864,400 candidate
lookups across those processes; only 0.48 seconds were sampled, so percentage
rankings are diagnostic, not reliable wall-time estimates. Reports:
`/tmp/a-program-authority-current-profile-function-field.{log,gprof}`.

- [x] Inspect all index-capacity writers: initialized capacity is 64 and growth
  doubles it with overflow checking. No other prototype code sets an index
  capacity. Replace remainder division with the equivalent mask after the
  unchanged hash avalanche. No key, equality, collision or bucket ordering changes.
- [x] Extend existing aligned-key/collision/rehash test with capacity-invariant
  checks through growth. Do not introduce another index or benchmark framework.
- [x] Measure baseline/current in alternating fresh processes without profiling
  instrumentation or concurrent acceptance builds; report noise and limitations.
- [x] Run full optimized and affected debug/sanitizer gates and record counts.
- [x] Included in the grouped `5f9cd1f` publication, not an isolated epoch.

This is shared-index cost removal within A5, not completion of A3/A4 or the
small-input regression comparison against R0. It does not justify dropping
validation or interning WHNF-equivalent terms.

Uninstrumented O2 source solves, 30 samples per version/input after two warm-up
pairs, alternating order, no concurrent acceptance build/run during timing:

| Input | Baseline/current median seconds | Solve steps (both) |
|---|---|---:|
| Vec append | .010829 / .010898 | 19935 |
| generated length | .009541 / .009140 | 9702 |
| function field | .014412 / .013496 | 12883 |
| imported QuickSort property | .211442 / .211201 | 147083 |

These are fresh-process wall times including launch/loading, not CPU-isolated
measurements. The first partial run also had an insignificant reverse difference
for length. Do not claim a general speedup or closure of the R0 regression.
The valid matrix is `/tmp/a-program-authority-index-mask-timing-final.jsonl`;
all 256 processes exit 0 with identical per-input stdout. Two preliminary
harness attempts omitted the Vec provider and then the legacy intrinsic option
needed by QuickSort's provider; they are not implementation failures or valid
timing matrices. The corrected commands supply each original provider with
`--imports` and `--legacy-intrinsic-dot`, without rewriting fixture syntax.

Full optimized `check-acceptance` exited 0, including 63/63 compatibility. All
2,460 sorted export-result records match the preceding publication gate after
normalizing temporary directory names; step counts are preserved. Debug Core
and ASan/UBSan Core, derivation/source image scripts, handler-boundaries and
prepared-module checks also exited 0. Sanitizers used leak detection and
halt-on-error; flags are unchanged from the preceding publication. Logs/builds
use `/tmp/a-program-authority-index-mask-{opt,debug,asan}`. No source/test edits
occurred during these gates. Local delta: `graph.c` +1/-1, `graph.h` +1/-0,
`tests/core.c` +4/-1. No Main push for this micro-optimization; A3-A5 stay open.

### 2026-09-19: Unify source preparation inspection (local)

`prepared_source_rule` and `source_preparing` independently dispatch on the
same producer roles and syntax to choose the rule and its readiness. Replace
them with one read-only projection returning the existing rule and optionally
its preparation state. No retained descriptor, scheduler state or new authority.
Preserve the distinction between no rule yet and a ready namespace/non-rule
producer, post-check independence, and notification timing. Inspecting only
the rule must not perform additional name lookups.

- [x] Consolidate the dispatch, including application stages and block tails.
- [x] Extend pending/accepted body-kind checks; retain direct producer
  subscription, idle queue, effect-cycle and namespace tests.
- [x] Run debug synthesis, full optimized acceptance and affected sanitizers;
  compare normalized results/steps and report implementation/test LOC.
- [x] Grouped in `5f9cd1f`; this does not close A3-A5 or R2-R5.

The same projection now supplies the rule directly to waiting consumers;
remove their second read after preparation. On the function-field fixture,
old `source_preparing`/`prepared_source_rule` calls total 8,814 + 571; new
`source_rule` calls total 8,997. Terms/proofs/occurrences/Contexts/maps/jobs
remain 5,143/9,371/6,717/658/2,657/4,111, with 12,883 Solve steps. This is a
read-count reduction, not a measured wall-time speedup. GDB reports:
`/tmp/a-program-authority-source-rule-{baseline,final}-state.log`.

Final debug synthesis, full O2 acceptance, ASan/UBSan synthesis and source-image
scripts, 4,240 handler snapshots and 1,118 module snapshots exit 0. O2 retains
63/63 compatibility; all 2,460 normalized export-result records and steps match
the index-mask baseline. Logs use `authority-source-rule-single-read-*` under
`/tmp/a-program-`; sanitizer flags/options are unchanged. A draft test placed a
definitions block on an assignment RHS, rejected by both the baseline parser
and this version. Remove that invalid fixture, not its parser rejection; the
final additions cover host literals and selected computation-block termination.

Delta from `ba8a628`: `synthesis.c` +73/-78 (net -5), `tests/synthesis.c` +2/-1
(net +1). Cumulative implementation/headers: R76 +2,837/-1,418 (net +1,419);
R0 +7,736/-3,901 (net +3,835). The overall reduction gate remains unsatisfied.

### 2026-09-19: Resume nominal Context transport without rediscovery

- [x] Reuse the inductive query's existing value slot after nominal resolution;
  retain ordinary Context transport, index checks and final publication guard.
- [x] Assert that each pending step in the 32-scope IADT regression exposes no
  instance. Existing alternate-proof, foreign-owner and chunk-size tests remain.
- [x] Debug IADT, full O2 acceptance, ASan/UBSan IADT and source-image script pass.
- [x] Published in `5f9cd1f` with the shared-query/ownership epoch.

Against `c3889bb`, GDB on `function-graph-function-field.p` records nominal
lookups 667 -> 279, successful lookups 501 -> 113 across the same 113 queries,
and maximum successful lookups per query 10 -> 1. Structural-subject calls
decrease 5,613 -> 5,225. Query transitions remain 969; Solve remains 12,883
steps. Terms/proofs/occurrences/Contexts/maps/jobs remain respectively
5,143/9,371/6,717/658/2,657/4,111. No wall-time or memory improvement is claimed.

Logs: `/tmp/a-program-authority-nominal-resumption-{before,after}.log`,
`...-debug-iadt.log`, `...-opt.log`, `...-asan-{iadt,source}.log`; successful
lookup counts: `/tmp/a-program-authority-nominal-success-{before,after}.log`.
Build with the pointer Makefile: debug `-O0 -g`, full `check-acceptance` `-O2`,
and sanitizers `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-pie -no-pie`, all with `-std=c11 -Wall -Wextra -Werror`.
Sanitizers use `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. All final commands exit 0. All 2,460 normalized
export-result records, including steps, match the preceding completed O2 run.

Delta: `evidence.c` +6/-1 (net +5), `tests/iadt.c` +3/-1 (net +2).
A3's transparent-scope candidate scan, remaining A4/A5 work and the cumulative
net-negative gate remain open. No extra lexical index or scope-by-syntax join
was added: an erased allocation pointer does not identify its lexical use.

### 2026-09-19: Yield shared beta work during graph-head preparation

- [x] Replace three synchronous application-body calls in `prepare_head` with
  the existing typed query and graph worker's `view` slot. Share waiting with
  its input/origin readers; no new job kind, cache, field or artifact format.
- [x] Add fresh-scope cancellation at every preparation boundary, assert at
  most one beta-query transition per graph turn, and resume the same query
  after destroying the graph worker. Check its actual Return result.
- [x] Debug program tests, full O2 acceptance, ASan/UBSan program tests and
  complete source-image script exit 0, using the preceding checkpoint's flags.
- [x] Published in grouped refactoring epoch `5f9cd1f`; A3-A5 remain open.

The new regression fails on `6114ebc`: cancellation limit 3 observes two query
transitions in one turn. The fixed version passes for outer chunks 1/64.
All 2,460 normalized export-result records agree when scheduling counts are
compared separately: 2,096 are unchanged; 364 increase by 1, 6, 7, 10, 14 or
19 steps. No records disappear. On `function-graph-exposed-match.p`, GDB shows
the same 10,882 typed-query transitions and the same Terms/proofs/occurrences/
Contexts/maps/queries/jobs: 3,464/7,006/4,866/464/1,971/1,166/4,009. Outer Solve
turns change 11,674 -> 11,680 because previously hidden progress now yields.
This is a progress-control correction, not a measured speedup or a bound on
every synchronous kernel suboperation.

Logs: `/tmp/a-program-authority-head-yield-{before,debug}-program.log`,
`...-before-bound.log`, `...-{before,after}-counts.log`, `...-opt.log`, and
`...-asan-{program,source}.log`. Implementation: `function_graph.c` +23/-6
(net +17); tests: `tests/program.c` +32/-0. The cumulative reduction gate is
still unmet. The source-writer lexical candidate scan is unchanged.

### 2026-09-19: One shared substitution input/result root

- [x] Allocate the readback root directly in the shared substitution owner's
  arena, alongside its input environment. Remove the request's duplicate root
  and completion-time result transfer. Only temporary traversal edges are
  cleared at completion; standalone/imported traversals retain their owner.
- [x] Assert stable root/input addresses at every suspension boundary, no
  initial scratch arena, and no dangling traversal edges after completion.
  Existing cancellation, capture, identity-prefix, serialization and resumed
  computation checks remain. The address regression fails on `62a340a`.
- [x] Focused debug Core and complete evaluation-image tests pass. Full O2
  `check-acceptance check-eval-io` exits 0; all 2,460 normalized export records,
  including steps, match the preceding completed run.
- [x] Full debug acceptance exits 0 with the same 2,460 exports and step counts.
  On function-field, all 1,718 shared substitutions formerly allocated scratch
  storage at initialization; none now do. The 170 empty substitutions finish
  without it; nonempty traversals may still allocate scratch when advanced.
  Core/proof/occurrence/Context/map/query/job counts and 16,998 query transitions,
  12,883 Solve steps are unchanged. This is not a peak-memory or speed claim.
- [x] Full ASan/UBSan acceptance and the before/after timing check complete.
- [x] Published with the shared-query/ownership epoch in `5f9cd1f`.

No new proof rule, query kind, result cache or image format. The optional arena
pointer replaces the old retained-root pointer; it describes storage lifetime,
not a second answer. A3's lexical candidate bound, the remaining consumer audit,
baseline performance comparison and the cumulative net-negative gate stay open.

All three builds ran `check-acceptance check-eval-io`, exit 0, including 63/63
source compatibility. ASan/UBSan used the preceding CFLAGS with default runtime
options; no sanitizer/runtime-error diagnostic occurred. Core and the complete
evaluation-image script additionally pass with explicit leak detection and
ASan/UBSan halt-on-error. Logs: `/tmp/a-program-authority-single-root-{debug,opt,asan}.log`,
`...-asan-{core,eval}.log`, `...-{before,after}-{counts,owner}.log`.
The input arena uses 322,656 -> 317,216 bytes; reserved capacity stays 327,680.
This is the function-field owner's final snapshot, not process peak RSS.

Strict debug timing against `62a340a`, 32 alternating fresh-process pairs per
input, first two pairs excluded, no concurrent build/test: median seconds
before/after are Vec-append .015421/.015170, generated-length .012628/.012811,
function-field .020069/.020642, QuickSort-property .333207/.335297. Steps agree
(19935, 9702, 12883, 147090). No elapsed-time improvement is established.
Raw samples: `/tmp/a-program-authority-single-root-timing.jsonl`.

Grouped delta from the previous Main `cbf81fe` (implementation/test files):

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| eval.c | 18 | 17 | +1 |
| eval_internal.h | 2 | 2 | 0 |
| evidence.c | 6 | 1 | +5 |
| function_graph.c | 23 | 6 | +17 |
| graph.c | 1 | 1 | 0 |
| graph.h | 1 | 0 | +1 |
| synthesis.c | 73 | 78 | -5 |
| tests/core.c | 4 | 1 | +3 |
| tests/eval_io.c | 5 | 0 | +5 |
| tests/iadt.c | 3 | 1 | +2 |
| tests/program.c | 32 | 0 | +32 |
| tests/synthesis.c | 2 | 1 | +1 |

Implementation/header total: +124/-105 (net +19); tests: +46/-3 (net +43).
Cumulative implementation/header totals: R76 +2,883/-1,441 (net +1,442),
R0 +7,781/-3,923 (net +3,858). These increases do not satisfy the reduction
gate. The grouped milestone verifies ownership/resumption cleanup, not overall
refactor completion. Documentation deltas are separate from all totals above.

Publication: atomic push from `cbf81fe` through implementation `5f9cd1f` to
`main` and `rewrite/pointer-core-hott` succeeded. `git ls-remote` confirmed both
at `5f9cd1ff829193434e8303d5235081b722df31a4`. This follow-up records publication
only; the tested implementation is unchanged. No final acceptance gate is waived.

Documentation-only delta from `cbf81fe`: authority plan +36/-0; priority plan
+225/-0 (including this publication record). Neither is implementation code.

### 2026-09-19: Withdrawn binder-frontier experiment

Publication policy remains milestone-based: Surface, Issue #29/PR #30, then
substantial refactor epochs, each after its existing acceptance tests pass.
No failed experiment is published to Main. The following trial was withdrawn;
implementation and tests remain those of `192f4b9`.

- [x] Measure the member-only trial: 128 unused Lambda scopes increased save
  callbacks from 4 to 132 on the baseline; binder-frontier discovery held them
  at 4. The narrow fixture and debug source/normalization tests passed.
- [x] Reject the trial after full optimized acceptance failed retained
  QuickSort inert resave byte equality. The narrower tests were insufficient.
- [x] Test per-key append-order reference groups as a possible repair. This
  still failed: both images have 107 scopes, 157 producers and 76 origins,
  but producer records at offsets 13043 and 13091 exchange syntax IDs 68/74
  under scope 29. Equal counts do not prove all other differences harmless.
- [x] Remove the experimental implementation and its candidate-count assertion;
  retain the existing inert-resave, binding-count and exact-Core tests intact.
- [ ] Replace lexical candidate scans only after reachability and deterministic
  discovery order are established together. A3 is not complete.

Two broader trials also failed. Gating nominal/induction origins on binders
lost the outer allocation needed by the retained exact-Core fixture: erased
layouts need not retain those binders. Seeding bindings from selected syntax
repaired that case but broke ordinary recompute images' zero-binding contract.
Neither a second syntax index nor an artifact-mode exception was retained.

Next design check: a saved producer can be recreated before its restored
origin allocation. Registration order is therefore not a serialization order.
Trace the existing syntax/object dependency frontier before proposing another
index. Preserve typed source identity, lexical ancestry and erased-layout
reachability; do not sort by process addresses or run Solve from the writer.
Validation must cover fresh save, two zero-step resaves, retained/recompute
modes, field-only origins and unrelated scopes before full acceptance.

Evidence under `/tmp/a-program-authority-binder-frontier-`:
`before-{build,test,counts}.log`, `after-counts.log`,
`debug-{source,normalization}.log`; both `opt.log` and `opt-final.log` FAILED.
The append-order trial's `group.a` and `group-resaved.a` are each 420556 bytes
and first differ at byte 13052. These are local diagnostic artifacts, not
accepted image fixtures or proof of semantic equivalence.

Withdrawal verification: rebuilt the restored CLI, source-image and program
tests with strict `-O0 -g -Wall -Wextra -Werror`; the complete `image_cli.sh`
passed (exit 0), including both zero-step QuickSort resaves. Logs:
`...-withdraw-build.log`, `...-withdraw-image.log`. No source/test diff remains;
only this audit and the parent-plan correction are pending. Remote Main and
rewrite both remain `192f4b946f4be68c3ef25f257840dad8b1106b43`.

### 2026-09-19: Suspend helper argument-spine beta inspection (local)

- [x] Replace the synchronous helper Match application loop with the existing
  shared `application_body` query adapter and `s->view` wait slot. One pointer
  in the existing helper cursor retains its next argument; no result registry,
  job kind, kernel tag or persistence format is added.
- [x] Add cancellation tests for both arguments of a captured-parameter helper.
  Cancel at every graph boundary, then resume the same queries without their
  former graph owner. Each tracked query advances at most once per graph turn.
  This is not a claim that all synchronous kernel work is globally fuel-bounded.
- [x] Demonstrate the regression against `192f4b9`: exit 134 at the per-query
  step assertion. Strict debug and ASan/UBSan program tests pass after the fix.
  The test must borrow the original call's converted arguments, not reconstruct
  variables with the same Core and assume identical typed occurrences.
- [x] Optimized `check-acceptance check-eval-io` passes, including 63/63 source
  compatibility and retained QuickSort resaves. All 2,460 normalized result
  records agree after excluding steps. Step deltas: 2,278 unchanged, 82 +1,
  82 +13 and 18 +21. These are scheduling changes, not equality exceptions.
- [x] Published in the grouped `3340dde` milestone, not a Main push for one
  helper. A3, remaining A4 consumers and final A5 gates stay open.

GDB on the imported QuickSort-property fixture: Core terms 135287, proofs
93435, occurrences 82230, Contexts 4389, Context maps 15764, typed queries 9925,
jobs 37388 and query steps 336649 are unchanged. Outer Solve steps increase
147090 -> 147103. No elapsed-time improvement is claimed. Implementation delta
is +15/-6 (net +9); the cancellation test is counted separately. This is not
completion of the cumulative source-reduction requirement.

Evidence prefix `/tmp/a-program-authority-helper-apply-`: `program.log`,
`before-program.log`, `opt.log`, `asan-program.log`,
`single-root-counts.log`, `helper-apply-counts.log`. The sanitizer program run
uses explicit leak detection and ASan/UBSan halt-on-error. Final test-only
lifetime cleanup was rechecked: strict debug and sanitizer program tests and
optimized `check-acceptance check-eval-io` (`opt-final.log`) all exit 0. The two
optimized runs have identical normalized exports including steps.

Local delta from `192f4b9`: `function_graph.c` +15/-6; `tests/program.c` +61/-0.
Cumulative implementation/header deltas are R76 +2892/-1441 (net +1451),
R0 +7790/-3923 (net +3867). Documentation is separate. The overall reduction
gate remains unmet; Main is deliberately not advanced for this single helper.

### 2026-09-19: Skip repeated Identity inspection across lexical names (local)

- [x] In `source_has_identity`, skip adjacent scopes with the identical
  `context_job` pointer before inspecting its Context extension. Lexical scopes
  remain distinct; no new cache, result authority or serialization rule is added.
- [x] Extend the named-transport regression with 128 shadowing names. Existing
  checks still distinguish relation witnesses and reject invalid transport.
- [x] Strict debug and ASan/UBSan synthesis tests pass, with leak detection and
  halt-on-error enabled for the sanitizer run.
- [x] Optimized `check-acceptance check-eval-io` passes. All 2,460 normalized
  export records, including steps, match the preceding helper-application run.
- [x] Included in the tested `3340dde` refactoring epoch, not an isolated
  cleanup push. A3/A4/A5 and the overall reduction gate remain open.

For the imported QuickSort-property fixture, GDB records 725 calls before and
after. Identity checks decrease from 6,910 to 5,943; adjacent repeated Context
checks decrease from 967 to zero. Solve remains 147,103 steps. No wall-clock
speedup is claimed. Delta: `synthesis.c` +2/-0, `tests/synthesis.c` +10/-0;
documentation is separate. This does not address source-origin reachability.

Evidence: `/tmp/a-program-authority-identity-scope-{before,after}.log`,
`...-synthesis.log`, `...-asan-synthesis.log`, and `...-opt.log`.

### 2026-09-19: Reuse the inspected application callee (local)

- [x] Remove the source application's second classifier-normalization layer.
  `prepare_application` already normalizes the callee to inspect its Pi shape;
  build APP from that producer instead of calling the unchecked-input API again.
  Share the domain job with its constraint and argument check. Keep Identity
  transport, constructor inference, CBPV sequencing and post-synthesis checks.
- [x] Extend dependent application testing at chunks 1/64: after source Solve,
  constructing each of its two applications through the direct API adds no
  jobs and returns the existing accepted application. On `f783742` this test
  fails at the job-count assertion (exit 134); with the change it passes.
- [x] Strict debug synthesis test, full optimized `check-acceptance
  check-eval-io`, and ASan/UBSan synthesis plus complete `source_io.sh` pass.
  Sanitizers use explicit leak detection and halt-on-error. Compatibility is
  63/63. All 2,460 normalized export records agree with the preceding run after
  excluding steps; existing inert-resave equality gates pass unchanged.
- [x] Included in publication epoch `3340dde`. This fix does not complete A3's
  source reachability, A4's remaining consumer audit or A5.

No global idempotence shortcut, new job/tag/cache, or expected-type input is
introduced. The generic application API still checks and normalizes its input.
`CLASSIFIER_CONSTRAINT_JOB` propagates equations for open result types; it is
not removed merely because APP also checks the final argument. Structural Pi
inspection is not acceptance evidence and the ordinary kernel rule still runs.

On the imported QuickSort-property fixture, jobs decrease 37,388 -> 34,874 and
Solve steps 147,103 -> 142,031. Proofs 93,435, typed occurrences 82,230, Contexts
4,389, maps 15,764, typed queries 9,925 and query steps 336,649 are unchanged.
Core terms increase 135,287 -> 135,290; do not claim identical intermediate
allocation. For `indexed-dependent-environment.p`, jobs decrease 3,202 ->
3,045 and steps 9,480 -> 9,144; typed queries decrease 581 -> 580. Its other
measured structure counts are unchanged.

Alternating fresh-process strict-debug comparison, 32 pairs per fixture with
the first two pairs excluded (seconds, median):

| Input | Before | After |
| --- | ---: | ---: |
| Vec append | 0.01555 | 0.01422 |
| Length proof | 0.01322 | 0.01202 |
| Function field | 0.02085 | 0.02101 |
| QuickSort property | 0.32700 | 0.32548 |

These local timings do not establish a universal speedup or close the original
baseline performance gate. Different tests reuse temporary image basenames;
sorted export logs establish result multisets, not per-fixture step pairing.
The work counts above instead use fixed, individually identified inputs.

Evidence prefix `/tmp/a-program-authority-application-owner-`: `build.log`,
`synthesis.log`, `before-test.log`, `opt.log`, `asan-build.log`,
`asan-synthesis.log`, `asan-source.log`, `before-counts.log`, `counts.log`,
`dependent-{before,after}.log`, and `timing.log`.
Implementation delta: `synthesis.c` +10/-11 (net -1); tests +33/-0, docs separate.
The cumulative implementation reduction requirement remains unmet.

### 2026-09-19: Member binder frontier with syntax-ordered dispatch

This supersedes the withdrawn member-only experiment, not the broader A3 gate.
Reinspection of its two saved files found exactly four changed bytes: the two
producer and two origin records exchange syntax IDs 68/74. The rest of the
420,556-byte files agree. Registration order is not source serialization order.

- [x] Group qualified-member candidates under their nearest binder or opaque
  lexical scope. The writer visits reached objects as well as selected scopes.
  Declaration/Match grouping remains unchanged: erased layouts can omit their
  lexical binders, so applying the member shortcut to them loses allocations.
- [x] Order each reached reference batch by the writer's selected syntax IDs
  before dispatch. Unreached syntax still waits on the existing frontier.
  Keep exact lexical ancestry checks. No process-address sort, new persistent
  owner, artifact flag, Solve/replay path or wire format is introduced.
- [x] Keep all references, including distinct scopes sharing syntax. Sorting
  does not intern or discard them. This is a deterministic discovery repair
  for this boundary, not a general canonicalizer of arbitrary lexical graphs.
- [x] Extend the existing member-origin test: after 128 unused binder scopes,
  selected lookup visits exactly the same origins. The new assertion fails on
  `4a6a777` (exit 134, multiple qualified origins); it passes with this change.
  Its ordinary-image byte equality and unchanged Solve/proof counts remain.
- [x] Full optimized `check-acceptance check-eval-io` passes, including 63/63
  compatibility. All 2,460 normalized export records, including steps, match
  `application-owner-opt.log`. Strict debug `source_io.sh`, `image_cli.sh`
  and `program_test` pass. Both retained QuickSort zero-step resaves are byte
  identical; the selected recursive append origin and exact-Core checks remain.
- [x] Affected ASan/UBSan source/image/program verification passes with leak
  detection and halt-on-error enabled. Group with the preceding application
  changes under the publication gate below, not as a separate helper.

GDB confirms four source-origin callbacks before and after the unused binders;
the old writer needed 132 afterward. Retained QuickSort dispatch uses 25 batches
with 148 candidate references; the largest temporary array is 1,024 bytes.
This is not a peak-RSS reduction. The two equal selected-syntax keys observed
there are preliminary unsupported and accepted Match candidates before the
syntax frontier starts; neither is silently removed. Broader lexical bound
and ordering analysis remains part of A3.

Evidence prefix `/tmp/a-program-authority-binder-order-`: `before-test.log`,
`build.log`, `final-build.log`, `final-source.log`, `debug-image.log`,
`debug-program.log`, `opt.log`, `counts.log`, `batches.log`, `ties.log`,
`asan-build.log`, `asan-program.log`, `asan-source.log`, `asan-image.log`.
The first tie-profile attempt stopped in GDB on a null syntax frontier; the
corrected diagnostic checks that pointer and produces `ties.log`. It did not
modify source or establish a failing compiler test.

### Grouped Publication Gate: Typed Application and Member Discovery

Group the helper beta-query resumption (`0c8b352`), repeated Identity inspection
removal (`f783742`), application-producer reuse (`4a6a777`) and member-frontier
change above. They reuse existing computation/typing/source owners and remove
duplicate work without claiming completion of the parent refactor.

- [x] Full optimized acceptance on the combined implementation.
- [x] Debug program/source/image tests, including both previously failing
  boundary regressions and dependent application tests recorded above.
- [x] Complete affected sanitizer tests with leak/halt-on-error enabled.
- [x] Verify remote tips, commit, atomically push Main/rewrite without force,
  and record the resulting revisions.

Relative to published Main `192f4b9` (implementation, headers and tests):

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `function_graph.c` | 15 | 6 | +9 |
| `source_io.c` | 61 | 2 | +59 |
| `synthesis.c` | 17 | 14 | +3 |
| `synthesis.h` | 5 | 3 | +2 |
| `tests/program.c` | 61 | 0 | +61 |
| `tests/source_io.c` | 3 | 0 | +3 |
| `tests/synthesis.c` | 43 | 0 | +43 |

Implementation/headers +98/-25, net +73; tests +107/-0. Documentation is counted
separately. This epoch reduces measured repeated work, not source line count.
A3's declaration/Match lexical bound, remaining A4 consumers, original-baseline
performance and cumulative net-negative code gates remain open.

Cumulative implementation/headers: R76 (`3a3bf550`) +2,962/-1,447 (net +1,515);
R0 (`4657cc6`) +7,860/-3,929 (net +3,931). Neither reduction gate is satisfied.

Publication verified: the atomic push advanced both `origin/main` and
`origin/rewrite/pointer-core-hott` from `192f4b9` to
`3340ddea736091e3107c16fa96f3da635730844d`. Remote tips were fetched and checked
for fast-forward ancestry beforehand and confirmed with `git ls-remote`
afterward. Through that implementation commit, documentation adds 34 lines in
the authority plan and 249 lines in this priority plan; these are separate from
implementation and test counts. This follow-up records publication only.

### 2026-09-19: Resume scoped inputs and index images (local)

- [x] Delete `evidence.c:scope_image`'s synchronous scope traversal. Selected
  inputs and nominal index arguments use the same `scope_image_step` and the
  existing typed-query dependency mechanism. Immutable frames are borrowed;
  each owner retains its cursor/value rather than rebuilding earlier work.
- [x] Retain the partially instantiated family and index images while nominal
  queries yield. No new query kind, scheduler, logical rule or wire format.
- [x] Replace `index_constructor_candidate`'s private parameter-image allocation
  and rebase loop with the existing checked substitution-rebase API.
- [x] Extend `tests/iadt.c:index_paths`: restricting a 64-constructor index must
  not finish a nested rebase query inside one selected-input transition.
  The unchanged per-turn assertion failed on pre-change implementation
  `4a60149` (exit 134), and passes now. Completed requests reuse the same query
  without additional proofs, queries or transitions.
- [x] Strict O0/g core, IADT and synthesis tests; full O2 `check-acceptance
  check-eval-io`; ASan/UBSan core, IADT and synthesis with leak detection and
  halt-on-error all exit 0. Compatibility is 63/63. All 2,460 normalized export
  result records match the previous epoch after removing scheduling counts.
  Logs: `/tmp/a-program-authority-scoped-query-{opt,asan-core,asan-iadt,asan-synthesis}.log`.

QuickSort property diagnostics retain exactly 135,290 Core terms, 82,230 typed
subjects, 93,435 proofs, 4,389 Contexts, 15,764 maps, 9,925 typed queries and
34,874 synthesis jobs. Summed query counters change 336,649 -> 341,847; outer
Solve steps 142,031 -> 142,087. These counters are not CPU time. Resume fields
and wait frames have a storage cost; unchanged object counts do not prove
unchanged memory use. This does not bound all synchronous kernel subchecks.

Strict-debug fresh-process timing: 12 alternating pairs, discard the first two,
median seconds before/after: Vec append .01617/.01451; length .01218/.01214;
function-field .02096/.01975; QuickSort property .32621/.32028. Every run exits
0. No concurrent build/test ran during measurement. This small sample is not
a general speedup claim or the final R0 performance gate. Raw samples and GDB
counts: `/tmp/a-program-authority-scoped-query-{benchmark,before-counts,after-counts}.log`.

Delta from `4a60149`: `evidence.c` +74/-39; `synthesis.c` +1/-8;
`tests/iadt.c` +20/-2. Implementation net +28; tests net +18. Documentation is
separate. The whole-plan net-negative requirement remains unmet.
Cumulative implementation/headers: R76 +3,029/-1,486 (net +1,543);
R0 +7,901/-3,942 (net +3,959). Neither reduction gate is waived.

Not merged conceptually: `index_transport_scope` drops selected binders while
retaining later independent declarations; it is not an ordinary projection.
Constructor boundary telescope lifting also cannot be removed just because
both operations manipulate Contexts. Remaining map-restriction/query loops
must preserve partial binder allocation before becoming resumable. A3, the
rest of A4, A5 and parent R2-R5 remain open. This local increment is not pushed
alone; group it with the next substantive verified refactoring epoch.

### 2026-09-19: Discover nominal allocations by their existing addresses

- [x] Remove declaration/Match registration under the nearest opaque lexical
  scope. Index their actual family/erased-matcher/induction addresses in the
  existing source-reference store, including imported allocations before Solve.
  Exact `(key, producer, binding)` registration is idempotent; it retains no
  copied result, acceptance flag or second allocation authority.
- [x] Keep syntax and exact lexical ancestry filtering. Delete the nominal
  writer's second object-wait phase; skip already collected origins before
  sorting a reference batch. Qualified members retain their binder boundary.
  Global diagnostic enumeration still reports each producer once, not once
  per family/matcher reference.
- [x] Test direct Match lookup before/after Solve and repeated inert resaves;
  family and erased-matcher lookup; 128 unused transparent declaration scopes;
  and 128 unrelated names explicitly borrowing the selected nominal object.
  The latter must not survive as extra declarations in the selected image.
  The direct-lookup regression fails against archived `81287c7` (exit 134).
- [x] Full optimized acceptance/evaluation-image gate exits 0, compatibility
  63/63. All 2,460 export records, including steps, match the preceding scoped
  query run. Strict-debug source/image/program checks pass.
- [x] ASan/UBSan source/image/program gates exit 0 with leak detection and
  halt-on-error enabled. Logs use the same prefix with `asan-source`,
  `asan-image` and `asan-program`. Earlier scoped-query core/IADT/synthesis
  sanitizer coverage is recorded above; this is not a full ASan acceptance run.
- [x] Published this grouped epoch with `81287c7` in implementation commit
  `6293a791427759edd16f5e01725ec1e3b3db2502`. Both Main and rewrite were advanced
  atomically from `4a60149`, without force, and verified by `git ls-remote`.

Retained IF8/main diagnostic: origin references dispatched 148 -> 139; batches
25 -> 40; largest temporary batch remains 1,024 bytes. Thus this removes broad
lexical discovery, not all overhead or every candidate inspection. When many
distinct source uses borrow one reached address, each remains a candidate for
the lexical filter; this does not close A3's strict output-bound question.

Freshly compiling IF8 with the two versions produces different record order.
Both the preceding version's image and the new image resave byte-for-byte
under the new binary with `--load --steps 0 --retain-reductions` (exit 3 means
pending; no Solve). The existing two-resave and both-reader-mode gates pass.
No wire-format change or evidence acceptance during writing was introduced.

Evidence prefix `/tmp/a-program-authority-nominal-frontier-`: `source-final`,
`image`, `program`, `opt-gate`, `before-test`, `{before,after}-batches` and
`{before,after}-resave` logs. The first `parameter-origins` manual invocation
used a nonexistent CLI mode and failed its argument assertion; the supported
`source_io.sh` write mode executes that test and passes.

Combined delta from published `4a60149`, before documentation:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| `evidence.c` | 74 | 39 | +35 |
| `source_io.c` | 8 | 7 | +1 |
| `synthesis.c` | 39 | 32 | +7 |
| `synthesis.h` | 6 | 7 | -1 |
| `tests/iadt.c` | 20 | 2 | +18 |
| `tests/source_io.c` | 65 | 0 | +65 |

Implementation/header +127/-85 (net +42); tests +85/-2 (net +83).
Cumulative implementation/headers: R76 +3,046/-1,489 (net +1,557);
R0 +7,921/-3,948 (net +3,973). Documentation is counted separately.
This epoch does not meet the cumulative net-negative gate or complete A3-A5
and parent R2-R5. Remaining synchronous map restriction must retain its partial
images rather than restart them, and the original-baseline performance/code
reduction review is still required.
Through implementation `6293a79`, documentation adds 19 lines in the authority
plan and 113 in this priority plan, separate from implementation/test totals.

### 2026-09-19: Shared map restriction and suspended dependencies

Baseline: published `1bfeeb6`. This is an A4 epoch, not completion of A3-A5.

- [x] Delete the independent map-image loop, `rebase_image` and `scope_map_step`.
  `pg_substitution_rebase_request` uses the existing typed-query index, rebase
  state and dependency driver. Nominal restriction now yields instead of
  synchronously finishing all images. The synchronous public adapter requests
  the same work; repeated calls do not reconstruct the image array or map.
- [x] Keep explicit map and target-Context proof inputs in the request key.
  Value-image queries still share by typed subject/target Context. Tests retain
  distinct derivations of the same structural map and their exact Context
  premises. No new Core former, logical rule, artifact format or scheduler.
  One internal map-rebase work kind distinguishes map results from term results.
- [x] Fix shared-query resumption: a consumer cannot run while its dependency
  is pending, even if another caller previously advanced that consumer. The
  old driver entered `typed_rebase_step` early and cached failure. The existing
  driver now follows the dependency first; no restart or recovery path is added.
- [x] Extend IADT tests for budgets 0/1/64, shared images, partial work adopted
  by another caller, repeated completed requests, distinct Context proofs,
  unavailable restrictions and foreign/invalid inputs. An existing-API-only
  regression fails on archived `1bfeeb6` at `pg_typed_query_result(outer_query)`
  (exit 134), and passes after the fix. Log:
  `/tmp/a-program-authority-shared-query-before-test.log`.
- [x] Full O2 `check-acceptance check-eval-io`: exit 0, compatibility 63/63.
  All 2,460 normalized exported results match the previous epoch; only the
  twelve `successor.a` records change step counts (+28 each), due to visible
  query scheduling. A second full run with the final IADT assertion also
  passes; its 2,460 exported records/steps match the first run exactly.
- [x] Strict O0/g core/IADT/synthesis and ASan/UBSan core/IADT/synthesis/program,
  complete `source_io.sh` and `image_cli.sh` pass. Sanitizers use leak detection
  and halt-on-error; no diagnostic is reported. This is not full sanitizer
  acceptance. Logs: `/tmp/a-program-authority-map-query-` followed by
  `opt-gate`, `opt-gate-final`, `opt-iadt`, `debug-{core,iadt,synthesis}` or
  `asan-{core,iadt,synthesis,program,source,image}` and `.log`.

Diagnostics: QuickSort retains 135,290 Core terms, 82,230 typed subjects,
93,435 proofs, 4,389 Contexts, 15,764 maps and 34,874 synthesis jobs. Queries
grow 9,925 -> 9,957 (32 map requests); summed query steps 341,847 -> 341,953;
outer Solve remains 142,087. Query headers remain 240 bytes, but new requests
and retained image prefixes consume storage. This is not a memory-reduction
claim. Counts: `/tmp/a-program-authority-map-query-{before,after}-counts.log`.

Strict-debug timing, 12 alternating fresh-process pairs, first two discarded,
no concurrent builds/tests: medians before/after in seconds are Vec append
.01444/.01474, length .01207/.01249, function-field .02192/.02026, QuickSort
.32841/.32425. Every process exits 0. These noisy samples establish no general
speedup and do not replace the R0 performance gate. Raw samples:
`/tmp/a-program-authority-map-query-benchmark.log`.

Per-file delta from baseline: `evidence.c` +60/-45, `evidence.h` +5/-0,
`tests/iadt.c` +70/-3. Implementation/header net +20; tests net +67.
Cumulative implementation/header: R76 +3,111/-1,534 (net +1,577), R0
+7,951/-3,958 (net +3,993). Documentation is separate. The cumulative
net-negative gate remains unmet. General Identity transport, function-graph
preparation and synchronous kernel checking require further audit; inserting
waits into code that allocates fresh binders must not restart those prefixes.

### 2026-09-19: Share the completed constructor map in the IH scope (local)

Baseline: published `32940ea`. Constructor fields already form a checked map;
after extending their destination with IH binders, only its relocation remains.
Previously `induction_scope_step` scheduled projection jobs for every image and
rebuilt the map prefix through general substitution synthesis. The direct
`prove_induction_scope` instead allocated a projection map and composed it.
Both now use the existing map-rebase query. Solver advances it one transition
per turn, retaining the already constructed IH scope. No new tag, authority,
wire version or binder-allocation path is introduced. Telescope instantiation
and IH formation still check different obligations and are not merged.

- [x] Remove both duplicate map-construction paths.
- [x] Test zero/one-IH scopes against independent projection/composition,
  shared query result identity, and repeated lookup without additional proofs
  or queries. Existing Acc/function-field and dependent-index tests still pass.
- [x] Optimized full `check-acceptance`, including 63/63 source compatibility
  and all four universal sorting proofs. All 2,460 exported comparison records
  match the baseline after normalizing temporary paths and scheduling counts.
- [x] Strict `-O0 -g` synthesis/IADT suites.
- [x] ASan/UBSan synthesis, IADT, program and complete `source_io.sh` suites.
- [x] ASan/UBSan complete `image_cli.sh` suite; all listed gates exit 0.
- [x] Group with the checked-map/input-registration reuse epoch below rather
  than publish this helper change alone.

Builds/logs: `/tmp/a-program-authority-ih-map-{opt,debug,asan}` and
`/tmp/a-program-authority-ih-map-*.log`. Optimized gate command:
`make -f src/prototype/pointer/Makefile -j2 BUILD=/tmp/a-program-authority-ih-map-opt check-acceptance`.
Sanitizer flags: `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`,
with C11 and strict warnings, leak detection and halt-on-error enabled.
This focused sanitizer matrix is not the parent's full sanitizer acceptance.

Same imported QuickSort property, 1,000,000 fuel, strict debug before/after:
jobs 34,874 -> 34,199; proofs 93,435 -> 93,063; occurrences 82,230 -> 82,091;
maps 15,764 -> 15,684; typed queries 9,957 -> 10,467 (map queries 32 -> 107).
Core terms stay 135,290 and Contexts 4,389. Query transitions increase
341,953 -> 343,348; outer Solve decreases 142,087 -> 140,836. Program graph
arena used bytes decrease 69,195,584 -> 68,837,792, excluding non-arena heaps.
Counts/arena logs are `...-before-counts.log`, `...-after-counts.log` and the
corresponding `...-arena.log` files; both processes exit normally.

Twelve alternating fresh-process pairs (discard first two), median seconds:
Vec append .01559/.01421; length .01199/.01365; function-field .02100/.02156;
QuickSort .32075/.32404. All exit 0. No general wall-clock speedup is established;
raw samples are `...-benchmark.log`. This does not replace the R0 matrix.

Per-file delta: `evidence.c` +1/-2, `synthesis.c` +4/-11 (implementation net -8),
`tests/synthesis.c` +13/-0. Cumulative implementation/header deltas remain
R76 +3,119/-1,550 (net +1,569), R0 +7,956/-3,971 (net +3,985).
Documentation is separate. A3's candidate bound, remaining A4 consumers,
final A5 gates and the cumulative net-negative requirement remain open.

### 2026-09-19: One-shot source reference registration (local)

Baseline: local `1293e50`, after the checked IH scope-map reuse change.
Source references distinguish lexical uses even when their allocation pointer
is shared. That distinction remains necessary; allocation reachability alone
cannot select a lexical origin. However, registration need not search all
earlier uses of that allocation to rediscover its own request.

`register_source_reference` now inserts directly. Its callers publish once:
binding/request interning publishes new nodes, imported nominal/Match inputs
publish on first attachment, and fresh allocations publish on first finish.
`finish` uses the existing pending/attached state to avoid re-registering
imported or already finished inputs. No persistent flag, cache or index is added.
Repeated source requests and attachment still return the existing owner.

- [x] Remove the duplicate scan and imported-input finish registrations.
- [x] Extend the 128-sibling declaration test with repeated-attachment checks.
  Existing tests check exactly one reference before and after image restoration.
- [x] Strict `-O0 -g` synthesis and full `source_io.sh` pass.
- [x] Optimized full acceptance: 63/63 compatibility and all four sorting
  proof suites. All 2,460 exported comparison records, including Solve steps,
  match `1293e50` after temporary-path normalization.
- [x] ASan/UBSan synthesis, program, full `source_io.sh` and `image_cli.sh`.
  All gates exit 0; leak detection and halt-on-error are enabled. Flags match
  the previous checkpoint. This is not the full parent sanitizer gate.
- [x] Group with `1293e50` as the checked-map/input-registration reuse epoch.
  Verify publication gates and remote baseline `32940ea` on Main/rewrite;
  publish both branches atomically without force. Remote Git state records
  publication, not this pre-push checklist. A3-A5 remain open.

GDB on `source_io_test write` before/after: 433 -> 431 registration calls;
16,589 duplicate-key comparisons -> deleted loop. The old scan found only two
duplicates, both allocation re-registration. Role/binding calls stay 18/151;
allocation calls decrease 264 -> 262. Both processes exit 0 and their source
images compare byte-for-byte equal. This is a work-count result, not a timing
or retained-output complexity claim. Logs/builds use
`/tmp/a-program-authority-reference-{audit,once}*`.

Local code delta: `synthesis.c` +6/-5, `tests/source_io.c` +4/-0. This adds one
implementation line while deleting the quadratic search; documents are separate.
The grouped epoch includes the preceding -8 implementation lines. Do not
equate this small reduction with the parent's cumulative net-negative gate.

Grouped delta from published `32940ea`: `evidence.c` +1/-2, `synthesis.c`
+10/-16, `tests/synthesis.c` +13/-0, `tests/source_io.c` +4/-0.
Implementation/header net -7; tests +17; documentation is separate.
Cumulative implementation/header totals remain R76 +3,120/-1,550 (net +1,570),
R0 +7,954/-3,968 (net +3,986). The original reduction gate is still unsatisfied.
QuickSort retains exactly the semantic table/work counts measured at `1293e50`;
see `...-graph-counts.log`. Neither this count result nor the registration
comparison count establishes a general elapsed-time improvement.

### 2026-09-19: Weakening composition trial withdrawn

Baseline: published `5fa8a55`. Length has 499 projected typed subjects, 149
with consecutive weakenings (depths up to six). An experimental composition
reduced length occurrences 3540 -> 3400 and proofs 4864 -> 4579. These are
withdrawn-trial results, not current implementation improvements.

The first trial normalized `pg_occurrence_projection`. Optimized acceptance
and focused sanitizer tests passed after adjusting two history-dependent IADT
scheduling assertions; all 2460 exported results matched the baseline except
step counts. However, a new explicit-map boundary test then exposed a real
regression: `pg_prove_structural_subject` could no longer certify an otherwise
valid, explicitly constructed two-stage projection recipe. Its `PG_REINDEX`
result had been canonicalized into a different typed subject. Existing tests
alone therefore did not establish semantic preservation.

A second trial left supplied maps intact and normalized only the ordinary
projection producer. The explicit-map test passed, but the existing IADT
`indexed_match` test failed at `refined_match` during elimination reindexing.
The structural lift and proof construction paths must agree on the same typed
map images, not merely equal erased Core/classifier results. Local composition
at just one producer or consumer is insufficient. Do not add an acceptance
fallback, Core-key lookup or a second normalization cache to conceal this.

- [x] Withdraw both implementations and restore the original IADT scheduling
  assertions. No new API, tag or normalization rule remains.
- [x] Retain a focused Core regression: direct/indirect weakening has the same
  Core/classifier/context but retains independent derivations; explicitly
  supplied nested maps remain checkable without a previously accepted proof,
  and noncanonical typed images retain their recipe. Repeated lookups do not
  allocate more proofs/subjects.
- [x] Verify the retained regression on the unchanged implementation: strict
  O0/g Core and IADT pass; O2 Core and ASan/UBSan Core pass. Sanitizers use O1/g,
  frame pointers, non-PIE, leak detection and halt-on-error. Logs use
  `/tmp/a-program-authority-projection-restored-*`. Full acceptance of the
  withdrawn trial is not a gate for this restored test-only checkpoint.
- [x] Before another composition change, audit `context_map_extend`,
  `map_lift_work`, `substitution_build`, structural subject admission and
  elimination reindexing together. Define one representation contract that
  preserves explicit input admission as well as generated-output sharing.
  Completed by the `a8aa05f` joint audit below. No projection normalization
  was reintroduced; the generic A3/A4 cleanup and original gates remain open.

Reproductions: `/tmp/a-program-authority-projection-raw-regression.log`
(explicit-map failure), `/tmp/a-program-authority-projection-direct-iadt.log`
(dependent Match failure). Trial measurements and preliminary gates use
`/tmp/a-program-authority-projection-normal-*`; they do not certify the
subsequently revised trial or the final retained tests. The earlier isolated
old-code comparison was rebuilt after discarding a header-mismatch build.

Final implementation/header delta versus `5fa8a55`: zero. Core regression:
+47/-0 lines; documentation is separate. Original R76/R0 implementation totals
remain +1570/+3986 net. No Main publication or A3-A5 completion follows from
this rejected optimization.

### 2026-09-19: Context declarations, not proof ancestry (local)

Baseline: local `82503d8`, whose implementation is published `5fa8a55`.
`substitution_build` now checks the judgement and dependent classifier from
`pg_context.parent/declared_type/judgement`. The accepted source Context and
its extension count establish this chain before traversal. Remove the
single-use `binding_judgement` adapter. Retain all supplied proof premises,
the same structural map and the same classifier substitution/checks. This is
not the withdrawn weakening normalization and introduces no new cache.

Delete `pg_prove_induction_case` and its declaration. Repository-wide symbol
search found no caller; the September 9 migration record in the pointer-core
plan explains its replacement by ordinary scheduled rules. The four-constructor
Tree source test still covers branches ignoring one or two IH arguments.
Induction scope formation, branch checking and the underlying rules remain.

- [x] Add a dependent-map regression with two proofs of the same source
  Context: share the structural map, retain each exact source proof, and
  reconstruct both derivations. Existing wrong-image, wrong-judgement,
  empty/prefix-map and family tests remain unchanged.
- [x] Strict O0/g Core and IADT tests pass.
- [x] Full O2 `check-acceptance` exits 0: compatibility 63/63 and all four
  sorting proof suites. All 2460 export records, including Solve steps,
  match `5fa8a55` after temporary-directory normalization and sorting.
- [x] ASan/UBSan Core, IADT and full `source_io.sh` exit 0, using O1/g,
  frame pointers, non-PIE, leak detection and halt-on-error. These are affected
  component checks, not the parent's full final sanitizer gate.

Logs/builds: `/tmp/a-program-authority-context-declaration-*`.
Implementation/header delta: `evidence.c` +4/-25, `evidence.h` +0/-8,
net -29; `tests/core.c` +10/-0; documents separate. No elapsed-time speedup
is claimed. Group Main publication with a substantive verified epoch.
Cumulative implementation/header net remains positive: R76 +1541; R0 +3957.
Neither original reduction gate is satisfied.

Further audit: `pg_prove_abstract`, IH motive abstraction and
`pg_prove_match_branch_type` need Context *proofs* to construct Pi/lambda
premises; replacing those walks with bare Context pointers would discard
requested derivations. Conversely `index_rebase` inside constructor transport
still drains an existing shared query synchronously. That is a budgeting
question, not evidence of another answer authority. Its enclosing dependent
telescope must retain completed images across suspension before moving this
work into existing scheduling. No extra scheduler or acceptance cache follows
from this audit. A3-A5 and the original cumulative reduction gates remain open.

### 2026-09-19: Preserve unchanged map derivations (local)

Baseline: `b143a53`. An accepted substitution already in the requested exact
destination Context proof needs no relocation. Previously `TYPED_MAP_REBASE`
still rebased every image, then constructed a substitution. Image queries are
keyed by typed subject and can return another derivation of that subject;
consequently this unnecessary reconstruction could replace the caller's map
proof. The new Core regression fails on the old implementation at exact map
result equality (exit 134), not just at a timing or step-count assertion.

- [x] Return the supplied map from the existing query when its destination
  *proof pointer* is the requested Context proof. Arguments are already
  ownership/rule checked at request admission. No Core reduction, type equality
  or first-proof lookup justifies this case; it is an identity operation.
- [x] Test two maps with identical structural images but different explicit
  image proofs. Both return themselves, in one step, without image queries or
  additional proofs. Repeated requests reuse the completed query.
- [x] Test a different proof of the same destination Context. It must still
  produce a distinct map receipt with that requested proof as premise; verify
  its shared structural map and reconstruct the derivation.
- [x] Strict O0/g Core and IADT tests pass.
- [x] Full O2 `check-acceptance` exits 0: compatibility 63/63 and four sorting
  proof suites. All 2460 export records agree with `b143a53` after temporary
  directory normalization and excluding changed step counts.
- [x] ASan/UBSan Core, IADT, complete `derivation_io.sh` and `source_io.sh`
  exit 0. Use O1/g, frame pointers, non-PIE, leak detection and halt-on-error.
- [x] Group with `82503d8` and `b143a53` as an explicit-map preservation epoch.
  Publication gates pass and both remote branches were checked at `5fa8a55`;
  publish Main/rewrite atomically without force. Git remote state records the
  publication outcome, not this pre-push checklist. A3-A5 remain open.

GDB fixed-source measurements compare the existing `5fa8a55` debug reference
binary with this change. The intervening `b143a53` declaration-chain cleanup
preserved all exported results and Solve steps; this table does not claim a
separate allocation measurement of `b143a53`:

| Input | Typed queries before/after | Solve steps before/after | Arena used before/after |
| --- | ---: | ---: | ---: |
| length | 1017 / 1011 | 9104 / 9094 | 4052864 / 4050368 |
| function field | 1803 / 1772 | 12334 / 12316 | 6492096 / 6480864 |
| Vec append | 743 / 735 | 19318 / 19312 | 4459552 / 4456352 |
| QuickSort property | 10467 / 10417 | 140836 / 140790 | 68837792 / 68808416 |

All eight processes exit normally. Core/Context/Solve-job counts are unchanged.
The first three inputs also retain identical proof/occurrence/map counts.
QuickSort proofs change 93063 -> 93016, occurrences 82091 -> 82073, maps
15684 -> 15670 and occurrence actions 28052 -> 28045. These are concrete work
and allocation results, not an elapsed-time claim or final baseline gate.

Evidence: `/tmp/a-program-authority-rebase-identity-{before,debug-core,debug-iadt,counts}.log`.
Implementation change: `evidence.c` +5/-0; Core tests +23/-0; documents separate.
The grouped implementation/header change since Main `5fa8a55` is net -24.
Per file: `evidence.c` +9/-25, `evidence.h` +0/-8, `tests/core.c` +80/-0.
The Core total includes the retained explicit-projection regression from the
withdrawn trial. Cumulative implementation/header totals remain R76
+3124/-1578 (net +1546), R0 +7955/-3993 (net +3962); documents are separate.
Optimized log: `/tmp/a-program-authority-rebase-identity-opt.log`;
sanitizer logs: `/tmp/a-program-authority-rebase-identity-asan-{core,iadt,derivation,source}.log`.
This does not meet the overall net-negative gate or repair source-origin
enumeration: allocation-only lookup still inspects unselected lexical aliases.
Changing that lookup to syntax-only would instead scan unrelated uses sharing
syntax. Keep both reachability conditions; neither key alone establishes the
required bound. No such source-writer change is included in this epoch.

### 2026-09-19: Compact role-exclusive private work

Baseline: Main `c0f27b3`. This is physical work-storage compaction, not another
answer authority, cache, query kind, proof rule or serialized representation.

- [x] Audit private work against the immutable query kind / synthesis role.
  Share storage only between mutually exclusive workers. Input queries retain
  both field and selection pointers; body/head queries retain both association
  and fold pointers. These pairs must not alias each other internally.
- [x] Share synthesis storage between conversion, reindex, function graph,
  Identity face, Identity formation and normalization work. Destroy owned work
  according to the existing role. Reindex and normalization references are
  borrowed from their graph owners. Keep certificates and inductive-instance
  results outside the union: they can coexist with private work.
- [x] Keep typed occurrences unchanged. Origin, selection and scoped operands
  can coexist; a raw classifier is also needed before accepted type evidence
  exists. Compressing these by a guessed exclusion would change admissible
  input, not merely representation.
- [x] Full strict debug `check-acceptance` exits 0. Compatibility is 63/63;
  sorting/proof suites pass. All 2460 export records, including Solve steps
  and multiplicity, match the preceding published implementation after only
  temporary-directory normalization.
- [x] Full optimized `check-acceptance` exits 0 with the same 2460 export
  records, including steps. Log: `/tmp/a-program-authority-private-storage-opt.log`.
- [x] Full ASan/UBSan `check-acceptance` exits 0, including existing cancellation,
  resumption, shared-query and image-boundary tests. Flags: strict C11, O1/g,
  address/undefined sanitizers, frame pointers, non-PIE; leak detection and
  halt-on-error enabled. All 2460 export records and steps match the published
  baseline and both other configurations. No sanitizer diagnostics.
  Log: `/tmp/a-program-authority-private-storage-asan.log`.
- [x] Report the final diff and prepare the verified storage epoch for Main.
  Both remote branches were checked at `c0f27b3`; publish atomically without
  force. The Git remote records the push outcome, not this pre-push checklist.
  Original A3-A5 and parent R2-R5 gates remain open.

On this machine, query headers change 240 -> 200 bytes and synthesis job
headers 456 -> 416 bytes. Allocation alignment means these size differences
are not a per-object arena-saving formula. GDB measurements use fresh debug
processes with the same fixed sources and `--steps 1000000`:

| Input | Arena used before | Arena used after | Saved bytes | Solve steps (both) |
| --- | ---: | ---: | ---: | ---: |
| length | 4050368 | 3910464 | 139904 | 9094 |
| function field | 6480864 | 6292576 | 188288 | 12316 |
| Vec append | 4456352 | 4312000 | 144352 | 19312 |
| imported QuickSort property | 68808416 | 67301152 | 1507264 | 140790 |

Core Terms, proofs, typed occurrences, Contexts, maps, occurrence actions,
typed queries and synthesis job counts are identical in every pair. All eight
processes exit normally. This is retained arena usage, not peak RSS or an
elapsed-time speedup. Counts: `/tmp/a-program-authority-private-storage-counts.log`.
Append counts: `/tmp/a-program-authority-private-storage-append-counts.log`.
The query-only intermediate was also tested and measured separately:
`/tmp/a-program-authority-query-storage-{debug,counts}.log`.
Full debug log: `/tmp/a-program-authority-private-storage-debug.log`.

No public layout/API is added just to inspect these private sizes. Existing
behavioral tests remain the regression contract; the representation audit and
external allocation measurement complement them rather than replacing them.

Five alternating fresh-process O2 timing samples, following one warm-up per
binary, give before/after median seconds: length .00777/.00886, function-field
.01560/.01378, QuickSort .20791/.20875. All outputs and steps agree. No tests
or builds ran concurrently with this measurement. Small-input spread is large;
these samples do not establish a speedup (or a general latency improvement).
Log: `/tmp/a-program-authority-private-storage-timing.log`.

Implementation/header diff from `c0f27b3`: `evidence.c` +9/-7, `synthesis.c`
+16/-10; total +25/-17, net +8. Tests unchanged; documentation is separate.
Cumulative implementation/header totals: R76 +3146/-1592 (net +1554), R0
+7975/-4005 (net +3970). Memory savings do not satisfy the source-reduction gate.

### 2026-09-19: Schedule function-graph input preparation

Baseline: Main `f73c26b`. `pg_function_graph_init` previously called
`prepare_graph` for a directly retained Match. That synchronously resolved its
IADT instance and rebased its parameter map before any graph budget was spent.
For an exposed Match the same work ran inside one later graph turn. These were
shared queries already, but their caller drained them through synchronous APIs.

- [x] Move preparation to `pg_function_graph_advance`. Replace the existing
  ready flag with head/input/parameters/ready phases; retain the existing
  context prefix, index arguments and shared-query wait slot. No new worker
  allocation, query kind, result cache, proof rule or wire format.
- [x] Request the existing inductive and map-rebase queries. Resume them through
  `await_view`, retaining completed preparation instead of reconstructing it.
  The public readiness/layout contract already requires advancing until ready;
  tests no longer assume that initialization supplies that readiness.
- [x] Keep direct accepted Match/induction bodies as known heads. The first
  draft rediscovered their construction origins and added 2/6/6 queries on
  length/function-field/QuickSort. Remove that extra discovery at the shared
  head-inspection entry, after any pending argument application. The draft's
  full O2 acceptance passed; that is not a reason to keep redundant work.
- [x] Add a cold indexed-Vec regression at every preparation cutoff. Check no
  parameter-query progress during initialization or budget zero, at most one
  query transition per graph turn, no premature graph formation, exact target
  Context evidence, and independent resumption after destroying the graph owner.
- [x] Run that regression against archived `f73c26b`: exit 134 at initialization.
  GDB confirms that the parameter query has already taken 3 steps and the old
  initializer reports prepared. Current focused strict-debug tests pass.
- [x] Complete strict debug and O2 acceptance. All 2460 export records match
  between configurations, including steps. They match the published baseline
  after excluding scheduling steps. Compatibility remains 63/63 and all four
  sort proof suites pass. The final tightened cutoff assertion also passes in
  the rebuilt debug program suite.
- [x] Complete ASan-UBSan acceptance: strict C11, O1/debug information,
  address/undefined sanitizers, frame pointers, non-PIE, leak detection and
  halt-on-error enabled. Exit 0, no sanitizer diagnostics; all 2460 export
  records and steps match debug/O2, with compatibility 63/63 and four sort
  proof suites passing. Log: `asan.log` under the prefix below.
- [x] Compare elapsed time with the published baseline, as recorded below.
- [x] Prepare the verified epoch for atomic Main/rewrite publication without
  force. Both remote tips were checked at `f73c26b`; Git records the actual push
  outcome. A3-A5 and cumulative reduction gates stay open.

The boundary is deliberately narrow: signature recovery, captured-eliminator
abstraction, case planning and other kernel calls are not made universally
constant-work by this change. Core/typed layers remain distinct; preparation
consumes accepted typed inputs and has no authority to accept a pending proof.

Fixed-source debug counts after removing redundant head discovery:

| Input | Solve steps before/after | Typed queries (both) | Arena used (both) |
| --- | ---: | ---: | ---: |
| length | 9094 / 9097 | 1011 | 3910464 |
| function field | 12316 / 12344 | 1772 | 6292576 |
| Vec append | 19312 / 19312 | 735 | 4312000 |
| imported QuickSort property | 140790 / 140878 | 10417 | 67301152 |

Core/proof/occurrence/Context/map/action/job counts are also unchanged. Summed
typed-query transitions are identical: 11129/17871/26903/343194 respectively.
The graph work header remains 584 bytes. Thus the extra outer Solve turns do
not represent duplicated query work. These are scheduling/allocation
measurements, not a speedup claim. Logs use
`/tmp/a-program-authority-graph-preparation-`: `before-test.log`,
`before-state.log`, `draft-opt.log`, `draft-counts.log`, `counts.log`,
`work-counts.log`, `debug.log`, `opt.log`, `final-program.log`.
Final full gates are tracked above, not implied by the draft.

Five alternating O2 samples were noisy, with a higher function-field median.
A further 31 alternating fresh-process samples per binary, after warm-up and
with no concurrent tests/builds, give median seconds before/after:
length .00916/.00875, function-field .01376/.01439, Vec append .00997/.00994,
QuickSort .21299/.21374. Function-field ranges are .01007-.02114 and
.01079-.01930; QuickSort ranges .19997-.23621 and .19702-.24134. These results
do not establish a speedup or discharge the original performance gates.
Logs: `timing.log` and `timing-31.log` under the same prefix.

Implementation/header diff from `f73c26b`: `function_graph.c` +63/-40,
`function_graph.h` +2/-1; total +65/-41, net +24. `tests/program.c` adds 59
lines; documentation is separate. Cumulative implementation/header totals:
R76 +3211/-1633 (net +1578), R0 +8040/-4046 (net +3994).
This epoch does not meet the cumulative source-reduction gate.

### 2026-09-19: Reuse checked map endpoints without reconstructing a lift

Baseline: Main `8820a47`. `map_dependency` previously tried a structural lift
even when both endpoint Contexts and the strengthened prefix map were already
checked. This did not provide another semantic authority, but could allocate
and finish unnecessary lift work before pairing the same checked prefix.

- [x] Reject the first simplification, which skipped prefix reuse whenever the
  destination was checked. The existing `family_instance_test` failed: ordinary
  image checking chose `PG_VARIABLE`, while explicit lifting used
  `PG_CONTEXT_PROJECTION` for the same typed image. The endpoint Contexts and
  structural map agreed, but the premise DAGs and resulting receipts differed.
  Do not weaken that existing sharing test or merge those proof records.
- [x] Keep the checked prefix and pair it with the known destination directly.
  Only an unknown destination requires structural lifting and its dependencies.
  Require the resulting exact map to agree; otherwise check its described
  images through ordinary substitution. Remove the `map_lift_work` adapter.
  No new cache, rule, scheduler, tag or map-normalization policy.
- [x] Extend the existing descriptive-map regression to known and unknown
  destinations. The known case must not allocate a structural lift; both cases
  must reuse the exact explicit-lift receipt and reconstruct its derivation.
  The new known-case assertion fails on the baseline (exit 134); the corrected
  strict-debug Core suite passes, including nested signatures, raw nested lifts,
  incompatible images, alternate proofs and wrong destinations.
- [x] Full strict-debug acceptance passes. All 2460 export records, including
  steps, equal the preceding Main; compatibility is 63/63 and all sort proof
  suites pass. Final Core tests also retry a wrong image after its destination
  becomes checked, and still reject it without another lift. The first test
  draft used Universe 0 instead of the existing Universe 1; projecting the
  existing type proof fixes that setup error, not a kernel rejection.
- [x] Fixed-source work comparison: length, function-field, Vec append and
  imported QuickSort have identical Core/proof/occurrence/Context/map/lift/query/
  action/job counts, query transitions, arena usage and Solve steps. The targeted
  known-destination experiment instead changes new lift work 1 -> 0, retaining
  exactly four new proof records in both versions. Logs: `counts.log`,
  `before-state.log`, `after-state.log` under the prefix below.
- [x] Full strict O2 acceptance passes, including the final negative test;
  all 2460 export records and steps match debug and Main `8820a47`.
- [x] Run 31 alternating fresh-process O2 samples per binary after warm-up,
  with no concurrent build/test. Median seconds before/after: length
  .00927/.00880, function-field .01462/.01398, Vec append .00984/.01031,
  QuickSort .20612/.20420. The measured work is unchanged and time varies;
  these samples do not establish a general speedup. Log: `timing.log`.
- [x] Complete map-admission-only ASan/UBSan acceptance with leak detection.
  All 2460 export records and steps match debug. Publish only after the combined
  epoch below has passed its own final checks and diff report.

Logs: `/tmp/a-program-authority-map-admission-` followed by `trial-test.log`,
`trial-state.log`, `before-test.log`, `debug-test.log`, `debug.log` and
`final-core-checked.log`. The test-setup failure is in `final-core.log`.
The trial is withdrawn, not a changed acceptance rule. Other A3-A5 gates,
including strict selected-root enumeration and cumulative LOC reduction, remain.

#### Grouped publication: remaining role-exclusive work pointers

Do not publish the map-admission change as a separate small storage epoch.
Group it with the following audited reuse of the existing private-work union,
then rerun final acceptance for the combined tree. No new discriminant is needed.

| Work pointer | Existing owning roles |
| --- | --- |
| `index_transport` | `INDEX_TRANSPORT_JOB`, `INDEX_RESULT_JOB` |
| `substitution` | `CONSTRUCTOR_SCOPE_JOB`, `INDUCTION_SCOPE_JOB`, `DATA_RESULT_JOB`, `SUBSTITUTION_JOB` |
| `family` | `FAMILY_ACTION_JOB` |
| `derivation_input` | `DERIVATION_INPUT_JOB` |
| `fold_structure` | `TERM_STRUCTURE_JOB` |
| `effect_substitution` | `EFFECT_SUBSTITUTION_JOB` |

- [x] Audit writes, cross-job readers and destruction against immutable roles.
  `pg_synthesis_effect_substitution_result` checks its role before reading.
  Constructor/induction allocation restoration reads only the relevant scope
  job. Structural term synthesis and family action do not use the union's
  other alternatives on the same job.
- [x] Move these six pointers into the existing union. Dispatch freeing effect
  substitution's heap bindings by its existing role; other pointed-to state is
  graph-owned. Retain `derivation` separately: `derivation_step` uses it together
  with `normalization_receipt`'s `normalizing` pointer. Keep source allocation,
  accepted result, classifier, proof and diagnostic fields unchanged.
  Existing `tests/synthesis.c` effect-substitution checks cover shared requests,
  completed and pending destruction, and foreign-role result rejection. Retain
  them rather than adding a public layout API solely for this private change.
- [x] Verify final debug/O2/ASan-UBSan acceptance, unchanged export records and
  work counts, actual private-header/arena sizes, timing and cumulative LOC.
- [x] Prepare the combined verified epoch for atomic Main/rewrite publication
  without force. Verify both remote tips after pushing the enclosing commit;
  the remote records the publication outcome. Keep remaining A3-A5 gates open.

Combined candidate measurements against Main `8820a47`:

- Debug, O2 and ASan/UBSan full acceptance pass (exit 0), including leak
  detection, 63/63 compatibility cases and the sorting property suites. All
  2460 normalized export records, including steps, match Main and all three
  configurations. Logs use the preceding prefix plus `combined-debug.log`,
  `combined-opt.log` and `combined-asan.log`. No sanitizer diagnostic occurred.
- Private job header: 416 -> 368 bytes. Arena used bytes: length
  3,910,464 -> 3,727,136; function-field 6,292,576 -> 6,069,696; Vec append
  4,312,000 -> 4,103,808; QuickSort 67,301,152 -> 65,289,952.
  All recorded Core/proof/occurrence/Context/map/query/action/lift/job counts,
  Solve steps and query transitions are identical. This is storage reuse,
  not removal of accepted evidence or an alternate authority.
- 31 alternating O2 samples per binary, warm-up excluded, no concurrent build:
  median seconds before/after length .00871/.00899, function-field
  .01363/.01342, Vec append .01158/.01097, QuickSort .20710/.20685.
  No general speedup claim. Full samples and GDB counts use the preceding log
  prefix plus `combined-timing.log` and `combined-counts.log`.
- Implementation: `evidence.c` +19/-25; `synthesis.c` +9/-9; total -6 lines.
  Regression tests: `tests/core.c` +33/-21, net +12. Documentation is separate.
  Cumulative implementation/header diff: R76 `3a3bf550` +3239/-1667,
  net +1572; R0 `4657cc6` +8040/-4052, net +3988. The original net-negative
  gate remains unmet. This publication does not close that gate or A3-A5.
  Documentation-only diff: parent authority plan +9/-0; this priority plan
  +113/-0. These lines are excluded from implementation and test counts.

### A4 epoch: shared incremental function-source inspection

Baseline: Main `fdc28e1`. `pg_function_graph_init` still drains construction
origin/application queries through `pg_function_graph_source` before its first
budgeted advance. This is an actual synchronous caller, not a second proof
authority. The public synchronous API remains useful; its traversal must be
shared with preparation rather than copied into a second implementation.

- [x] Extract one caller-owned source cursor over the existing typed origin/body
  queries. Keep specialization, capture avoidance and exact map premises.
  Its synchronous wrapper drains that same cursor; graph preparation resumes
  it. Do not add another semantic query kind, accepted result cache or Core tag.
- [x] Initialization records its owned input only. Preparation obtains the
  outer Lambda and then leading raw Lambda parameters before the existing
  head/input/parameter phases. Reuse role-exclusive private storage with the
  later helper cursor; destruction must distinguish these lifetimes.
- [x] Add a cold projected-source regression: init/zero-budget perform no
  query transitions, each source-query advance is bounded, cancellation does
  not invalidate the shared query, and completion agrees with the synchronous
  source API. Preserve unsupported effectful functions after preparation.
- [x] Extend the same cursor to surface `@f`/`*f` and concrete-helper source
  lookup; initialization alone would have left the source caller synchronous.
  Name resolution occurs once before suspension. Six nested force/thunk
  wrappers exercise surface inspection with one-step Solve; both graph and
  witness finish. Cursor ownership is checked before advancing borrowed work.
- [x] Run focused then full acceptance, compare semantic outputs separately
  from scheduling counts, measure work/storage/time and cumulative code diff.
  Prepare this verified coherent epoch for publication. A3's lexical candidate bound and
  original cumulative reduction gates remain open, not waived by this work.

The focused direct-API regression fails on `fdc28e1`: initialization creates
10 proofs and 2 queries and advances its cold origin query 3 times. The first
cursor version creates/advances none there, with the graph-state header still
584 bytes. The existing pending-helper supply test then exposed an assumption
that source inspection had already completed: it read the dependency's source
before checking witness completion. Reverse those checks, keeping the same
rejection rule. Its initial failure is retained in `failure.log`.

Logs use `/tmp/a-program-authority-source-cursor-`: `before-test.log`,
`before-state.log`, `after-state.log` and `test.log`. The first full debug run
(`debug.log`) passes but predates extending the cursor to surface/helper
callers. `all-callers-test.log` passes those additions; the final combined
verification below supersedes it. No stronger bound is claimed
for synchronous checked specialization/constructor rules inside a cursor step.

Final-candidate checks against `fdc28e1`:

- Full debug and O2 acceptance pass (exit 0). Their 2,460 normalized export
  records agree including steps; baseline semantic exports agree after removing
  scheduling counts. The controlled `before-surface` build uses the new cursor
  and tests but retains baseline `synthesis.c`: it fails the six-wrapper
  one-step assertion. Thus repairing initialization alone is insufficient.
- Core/proof/occurrence/Context/map/query/action/lift/job counts and typed-query
  transitions are unchanged on length, function-field, Vec append and QuickSort.
  Solve turns change by +8/+15/0/+45 as formerly synchronous work is scheduled.
  Job header remains 368 bytes; function-graph state changes 584 -> 600 bytes.
  Arena used bytes change by +224/+224/0/+352, respectively; capacities match.
  Counts are in `all-callers-counts.log` under the preceding log prefix.
- Implementation/header: `function_graph.c` +99/-68, `function_graph.h` +12/-0,
  `synthesis.c` +26/-20; total +137/-88, net +49. Tests: `tests/program.c`
  +44/-2, net +42. Documentation is separate. Cumulative implementation/header
  from R76 `3a3bf550`: +3326/-1705, net +1621; from R0 `4657cc6`:
  +8115/-4078, net +4037. Neither cumulative reduction gate is satisfied.

- Final debug/O2/ASan-UBSan full acceptance passes (all exit 0), with 63/63
  compatibility cases, sort properties and image checks. All three sets of
  2,460 normalized export records agree including steps. Parallel make output
  can end with different targets; compare record multisets, not final-line order.
  No sanitizer diagnostic occurred. Commands use
  `make -f src/prototype/pointer/Makefile BUILD=/tmp/a-program-authority-source-cursor-{debug,opt,asan} -j2 check-acceptance`
  with strict C11 warnings/errors and respectively `-O0 -g`, `-O2`, or
  `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
  Sanitizer environment: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`. Logs: `all-callers-{debug,opt,asan}.log`.
- 31 alternating fresh-process O2 samples per binary, one warm-up excluded,
  no concurrent test/build: median seconds before/after length .00925/.00855,
  function-field .01440/.01409, Vec append .01054/.00979, QuickSort .21450/.21219.
  No general speedup claim: typed work is unchanged, and Vec append does not
  use this change. Full samples: `all-callers-timing.log`.
- [x] Verify the exact candidate, update parent A4 and prepare one commit for
  atomic Main/rewrite publication without force. Git remote tips record the
  publication outcome. This does not close A3-A5 or parent R2-R5.
  Documentation: parent authority plan +8/-0; this priority plan +85/-0.
  Excluded from all implementation/header and test totals above.

### A4 group: branch preparation and symbolic application

Baseline: `a7cfde6`. A3 reinspection still finds that one nominal address can
have distinct lexical uses. Object-only deduplication is invalid; an independent
scope-by-object cross product is not the requested selected-root traversal.
No source-selection change is adopted in this group, and A3 remains open.

`plan_case` currently allocates every field/IH/remaining-argument scope and
drains their typed beta queries in one graph turn. Continue the same telescope
through the existing application-body query and wait slot. Preserve the exact
field/IH/argument order and checked Context/proof premises.

- [x] Separate descriptive case setup from incremental telescope extension.
  Retain the next slot and its allocated Context while its query is pending;
  do not freshen binders again on resumption. Reuse storage with the later leaf
  number only if their lifetimes are disjoint. No new Core tag or proof rule.
- [x] Add a multi-field cold-case regression: setup does not construct the
  telescope wholesale, cancellation is safe at each boundary, and chunk 1/64
  completion still constructs the ordinary graph witness and expected result.
- [x] Preserve a returned branch value until its continuation's typed beta
  query completes. Reuse the existing output field and continuation stack,
  not a second result cache or synthetic RETURN node used as scheduler state.
  A leaf is ready only after its continuation stack is empty. Ordinary and
  stacked APP exposure use the same application query/wait slot.
- [x] Extend the cold Fold regression to its return continuation: before the
  repair, one graph turn drains multiple query transitions. Check one-step
  progression, independent completion after cancellation, and an actually used
  continuation query at graph completion. Both old-code failures are retained
  in `/tmp/a-program-authority-case-scopes-{before,return-before}-test.log`.
- [x] Compare old/new query work, proof/Context allocation and output. Run the
  focused and full acceptance gates before publication; report source/test/doc
  LOC separately. Keep the broader A3-A5 and cumulative reduction gates open.

Current measured candidate:

- Focused `program_test`, full strict debug and O2 acceptance pass (exit 0).
  Both final full logs contain 2,460 matching normalized export records,
  including scheduling counts. Baseline `a7cfde6` semantic exports match after
  removing steps. The cold four-field setup and cold Fold-continuation tests
  each fail before their respective repair. They cover cancellation and actual
  graph/witness completion, not merely empty-budget no-ops.
- Graph-state/case/job headers remain 600/264/368 bytes. On length,
  function-field, Vec append and QuickSort, all Core/proof/occurrence/Context/
  map/query/action/lift counts and typed-query transitions match the baseline.
  Solve turns change by +15/+107/0/+5465: beta work is now scheduled.
  Arena used bytes are unchanged except QuickSort +608; capacities match.
- QuickSort has one additional `CLASSIFIER_STRUCTURE_JOB` (3,930 -> 3,931);
  its input is a `PG_CONTEXT_PROJECTION` recipe (410 -> 411 such inputs).
  Accepted/pending structural inspection remains timing-sensitive in the
  existing Solver. Do not hide this by skipping projection validation or claim
  all request sharing complete. All accepted object counts remain unchanged.
  Further lineage inspection identifies a source definition named `A` as that
  projection's operand (four -> five structural projection requests).
- Implementation `function_graph.c`: +49/-24, net +25. Test `tests/program.c`:
  +51/-1, net +50. Cumulative implementation/header: R76 `3a3bf550`
  +3375/-1729, net +1646; R0 `4657cc6` +8164/-4102, net +4062. Original
  net-negative gates remain unmet. Documentation is counted separately.

Logs use `/tmp/a-program-authority-case-scopes-`: `debug.log`, `opt.log`,
`asan.log`, `counts.log`, `roles.log`, `shape-inputs.log`, `shape-lineage.log`,
and the two failing-before logs above.

Final publication checks:

- Debug, O2 and ASan/UBSan full acceptance all exit 0; each passes 63/63
  compatibility cases and the sorting property/image suites. All 2,460
  normalized export records match across configurations including steps.
  No sanitizer diagnostic occurs. Command:
  `make -f src/prototype/pointer/Makefile BUILD=/tmp/a-program-authority-case-scopes-{debug,opt,asan} -j2 check-acceptance`,
  with strict C11 warnings/errors and respectively `-O0 -g`, `-O2`, or
  `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie`.
  Sanitizer environment is `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`.
- Two idle fresh-process O2 comparisons use 31 alternating pairs each, one
  warm-up excluded; the second reverses starting order. Median seconds
  before/after in run 1 / run 2: length .00831/.00915 / .00900/.00871;
  function-field .01363/.01428 / .01410/.01478; Vec append .01023/.00994 /
  .00956/.01045; QuickSort .21287/.21408 / .20993/.20991. Small-input direction
  is unstable, including the unaffected Vec control. No general speedup or
  completed original-baseline performance gate is claimed. Full samples are
  `timing.log` and `timing-repeat.log` under the preceding prefix.
- [x] Prepare the tested group for an atomic Main/rewrite push without force;
  the Git remote records the publication result. Original A3-A5 and parent
  R2-R5 remain open. This group does not claim constant-time kernel subrules
  or remove the remaining synchronous schema helper application.
  Documentation-only diff: parent authority plan +11/-0; priority plan +88/-0.
  These lines are excluded from implementation/header and test totals.

### 2026-09-19: projection snapshot and lexical-index counterexamples

Baseline: `ef5e53daa507140ad5868999c5b7be6d67dec399`. These are audit results
for the remaining A3/A4 work, not a completed refactoring epoch.

- [x] Add the classifier counterpart of the existing type-snapshot regression
  in `tests/synthesis.c:pending_effect_contexts`. Before effect closure, a
  Lambda classifier contains a symbolic row. After closure, a newly accepted
  Context projection exposes its checked classifier; the earlier query remains
  immutable. Applying the effect substitution relates the two structures.
  Repeated requests share each exact query; neither query is acceptance proof.
- [x] Test unconditional projection redirection in
  `pg_synthesis_classifier_structure`: follow `PG_CONTEXT_PROJECTION` premise
  1 before interning the request. It fails the new accepted-classifier assertion
  (exit 134, `/tmp/a-program-authority-classifier-collapse-trial.log`). Revert
  that implementation. The previous epoch's one extra structure job is not, by
  itself, evidence of duplicate authority. Do not erase this snapshot distinction
  just to make request counts independent of scheduling.
- [x] Test a single lexical reference index for member/declaration/Match:
  use the member's nearest-binder/named-scope key for all three, then wait on
  family/matcher/self reachability in the writer. This fails existing
  `source_io_test constructor-inputs`, in `member_use_origins`, after inert
  resaves of `Box:=&(\A:@=>@{mk:A->*;}); r:=&(Box Nat).mk`.
  The reconstructed forced source has a different constructor pointer from
  the retained reduction source; GDB confirms `pg_alpha_equal == 0`, not just
  fresh bound names. The failed trial is withdrawn, including its changed
  reference-index tests. Logs/patch: `/tmp/a-program-authority-lexical-allocations-`
  `source.log`, `core-diff.log`, `trial.patch`.
- [ ] Next A3 prerequisite: preserve the selected source's dependency on a
  generative allocation when specialization removes its defining parameter
  binder from the reached Core. Scope-only lookup loses this dependency;
  address-only lookup visits unselected aliases. Inspect retained source
  producer edges and their Context actions together before replacing either.
  Keep the 128 unrelated shared-allocation cases and exact source/receipt
  identity through inert resaves. No new allocation authority or name-based
  equality shortcut is authorized by this audit.

Final retained change: `tests/synthesis.c` +14/-0; implementation/headers
unchanged. Debug (`-O0 -g`), optimized (`-O2`) and ASan/UBSan (`-O1 -g`, leak
checking and halt-on-error enabled) `synthesis_test` all exit zero. Final debug
`tests/source_io.sh` also exits zero after withdrawing the lexical-index trial.
Builds use `src/prototype/pointer/Makefile`, strict C11 warnings, and prefix
`/tmp/a-program-authority-classifier-snapshot-`; logs end in `debug.log`,
`opt.log`, `asan.log`, and `source.log`. The failed trial binaries/logs are
separate from these final tests. No full new publication gate or performance
claim is made for this test-only change. Keep it local until a substantial
verified epoch; A3-A5, parent R2-R5, and both cumulative LOC gates remain open.

### 2026-09-19: declaration dependencies without per-alias address lookup

Baseline: local `e5d1f28`; published implementation `ef5e53d`.

- [x] Recover the preceding entry's erased-parameter dependency from the raw
  declaration's existing parameter Context. Declaration/member source uses
  share the nearest-binder/named-scope key. Family and matcher entries reference
  the raw declaration once per family, not every source alias. No new Core tag,
  proof, result cache, source reconstruction or acceptance authority is added.
- [x] Preserve distinct nominal families sharing one erased layout. Extend
  `parameter_origins` to check exact-family metadata, 128 unrelated aliases,
  repeated registration, nonempty lexical candidate lookup, identical selected
  bytes before/after adding aliases, no Solve/proofs during saving, and two inert
  resaves followed by ordinary checking. Existing constructor-source identity,
  rejected/retyped members and independent acceptance checks remain unchanged.
- [x] Reject the broad parameter-discovery draft: it also visited members in
  the parameter Context, exchanging origins 24/25 (syntax 816 declaration and
  syntax 400 member) in the common retained QuickSort image. A generic ordering
  fix is unnecessary: restrict that callback to the exact declaration family.
  Corrected old-image resave is byte-identical; the draft only stabilized after
  changing the first saved image and did not meet the unchanged-format gate.
- [x] Final debug `tests/source_io.sh`; final optimized `check-acceptance`:
  exit zero, compatibility 63/63, all sort properties/negative claims. All 2,460
  normalized export results, including steps, equal the published epoch's run.
- [x] Affected ASan/UBSan `source_io.sh`, `synthesis_test`, `program_test` and
  `image_origins.sh` pass with leak detection and halt-on-error. An initial
  origin-script invocation lacked `pointer-check` (exit 127); the completed
  CLI build and successful rerun supersede it. Sanitized old-image resave also
  remains byte-identical, with the expected pending exit 3 and zero Solve steps.
- [x] Finish the affected gates and prepare a local commit. No Main push for
  this partial A3 increment; the local Git log records its commit identity.

Builds use `src/prototype/pointer/Makefile`, C11 with `-Wall -Wextra -Werror`,
debug `-O0 -g`, optimized `-O2`, sanitizer `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`. Final debug/sanitizer prefixes are
`/tmp/a-program-authority-declaration-filter-{debug,asan}`; the optimized build
reuses `declaration-parameters-opt`. Final logs use `declaration-filter-` with
`source.log`, `opt.log`, `asan-source.log`, `asan-synthesis.log`, `asan-program.log`
and `asan-origins.log`. Earlier `declaration-parameters-*` logs describe the
superseded broad-discovery draft, not final verification.

Common retained QuickSort save versus `ef5e53d`: source candidate callbacks
234 -> 241; scope dispatch 221 -> 225; binding callbacks remain 780. The new
parameter callback runs 26 times. Thus this bounds shared-alias discovery; it
does not claim every save does less work. Compile steps, typed-query work,
Core/proof/Context/map counts are unchanged for length, function-field, Vec
append and imported QuickSort. Arena used bytes increase by 256/320/128/832
respectively; reserved capacities and job/occurrence/proof sizes are unchanged.

Two sets of 31 alternating optimized source+retained-save pairs (one warmup
per binary), medians old/new milliseconds: length 9.116/8.859, 8.691/8.720;
function-field 13.977/15.184, 15.919/14.696; Vec 11.638/11.397, 12.160/12.533;
QuickSort 213.987/217.961, 214.029/214.649. No robust speedup is established.
Counters and timings are in `declaration-filter-save-counts.log`,
`declaration-filter-counts.log`, and `declaration-filter-*-timing*.log`.

Implementation/header diff: `source_io.c` +31/-4, `synthesis.c` +53/-25,
`synthesis.h` +5/-4, total +89/-33 (net +56). Tests: `source_io.c` +81/-10
(net +71). Documentation excluded. Cumulative implementation/header totals:
R76 +3,431/-1,729 (net +1,702); R0 +8,218/-4,100 (net +4,118). The required
net-negative gates are not met. Match/Self lookup, general lexical bounds,
remaining A4 consumers, final A5 and parent R2-R5 stay open. Do not publish this
partial A3 increment alone or redefine overall completion around these tests.

### 2026-09-19: Allocation-context discovery epoch

Group the preceding local `e5d1f28`/`6fb1db6` work with the Match extension.
Scope: declaration, member and Match lexical discovery share one path while
preserving erased allocation dependencies. This is an A3 increment, not its
general output-bound result or completion of A4/A5 and parent R2-R5.

- [x] Index Match uses by their lexical scope, as for declarations/members.
  Family/matcher/Self metadata references existing dependency Contexts, once
  per exact allocation/Context pair. Do not merge different Contexts, source
  obligations, motives or proofs. Remove the writer's Match-specific dispatch.
- [x] Add 128 pending aliases of a saved Match allocation in unrelated named
  scopes. Reattaching each input creates no index duplicates; selected lexical
  candidates and saved bytes do not change. Self lookup exposes one Context
  record, not 128 source jobs. Saving adds neither Solve steps nor proofs.
  Keep the existing independently synthesized stronger source motive, weaker
  retained theorem, invalid theorem, chunks 1/64 and two inert resaves.
- [x] Strict debug `source_io.sh` and full optimized `check-acceptance` pass.
  Compatibility is 63/63; all 2,460 normalized export records, including steps
  and multiplicity, match `6fb1db6` and its published baseline comparison.
- [x] Affected ASan/UBSan `source_io.sh`, `synthesis_test`, `program_test`,
  `image_origins.sh` and the existing `genericMain one` helper-graph check pass.
  Leak detection and halt-on-error are enabled. Old retained QuickSort resaves
  are byte-identical in debug and sanitized builds (pending exit 3, zero steps).
- [x] Prepare one lexical-allocation discovery publication epoch. Remote Main
  and rewrite were checked at `ef5e53d`; recheck immediately before an atomic,
  non-force push. Git records the final commit/publication result. Do not infer
  broader completion from this milestone or waive either cumulative LOC gate.

The unrelated helper-family shortcut trial is withdrawn. Replacing
`pg_prove_application_body` plus its family-application fallback with only
family application made `function-graph-helper-call.p` unsupported: 23,488
steps versus the unchanged implementation's successful 24,162-step check.
GDB finds no failed direct family application; `pg_prove_substitution_extend`
returns NULL in `witness_calls` for genericTail's cons branch. Removing the
typed beta step alone therefore does not preserve this consumer's contract.
Do not weaken substitution checking, add an acceptance fallback, or report
the trial as a completed synchronous-query removal. A4 retains this work.

Verification prefix: `/tmp/a-program-authority-allocation-context-`, using
`src/prototype/pointer/Makefile`, C11 and `-Wall -Wextra -Werror`. Debug is
`-O0 -g`, optimized `-O2`, sanitizer `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`. Logs: `source.log`, `opt.log`,
`asan-{build,source,synthesis,program,origins,helper,common}.log`, `counts.log`,
`save-counts.log`, `timing.log`. The separate withdrawn trial logs use
`/tmp/a-program-authority-helper-family-`; they are not final verification.

Compile Core/proof/Context/map/job counts, typed-query work and Solve steps
remain unchanged against `6fb1db6` for length, function-field, Vec append and
QuickSort. Arena used bytes increase by 320/384/128/768 respectively; reserved
capacity is unchanged. Against published `ef5e53d`, retained QuickSort saving
changes source callbacks 234 -> 252, scope dispatch 221 -> 234 and origin calls
142 -> 160; binding calls remain 780. The new Context callback runs 27 times.
This removes per-alias address discovery, not all redundant Context-prefix
walks, and is not a claim of fewer visits on every program.

31 alternating optimized source+retained-save pairs, one warmup per binary,
medians published/current milliseconds: 01_bool .894/.918; 07_add 1.969/1.960;
length 9.532/8.645; function-field 13.145/13.708; Vec 10.497/10.769;
QuickSort 206.457/207.177. No concurrent build/test ran during timing. This
single noisy sample does not establish an overall speedup or satisfy the
original-baseline A5 performance gate.

Epoch implementation/headers since `ef5e53d`: `source_io.c` +31/-7,
`synthesis.c` +55/-25, `synthesis.h` +5/-4; total +91/-36, net +55.
Tests: `source_io.c` +133/-11, `synthesis.c` +14/-0, net +136. Documentation
is separate. This final Match increment alone is implementation +39/-40,
net -1. Cumulative implementation remains R76 +3,430/-1,729 (net +1,701),
R0 +8,217/-4,100 (net +4,117). Neither original net-negative gate is met.

### 2026-09-19: Identity boundary declaration authority

Baseline: published `89c011f`. This is a remaining A4 structural consumer,
not a new Identity rule or a change to the meaning of type-family equality.

- [x] Reproduce the proof-layout assumption in `pg_prove_family_identity_type`.
  A valid family Context and substitution plus an invalid path reaches
  `family_action_core` with a Context receipt instead of a type receipt.
  The new `dependent_families` regression exits 139 before the correction.
- [x] Read each binding's declared type and parent from the immutable Context.
  The computational helper takes Core/Context, not a receipt from which to
  reconstruct them. Exact family, map, endpoint and path proofs stay premises.
  Do not add rule-specific fallback branches or reject all family Contexts.
- [x] Focused strict-debug Identity and Core tests pass after the correction.
  Extend the regression to accept unchanged common family parameters and to
  assert that rejecting the malformed boundary adds no accepted proof.
- [x] Full optimized acceptance, final focused debug and affected ASan/UBSan
  Identity/Core/identity_io/derivation_io checks pass. All 2,460 normalized
  export records and Solve steps equal published `89c011f`. Keep this local
  until a substantial verified publication epoch.

The temporary forward-ordered declaration array remains: later path types
depend on already checked earlier paths. It now borrows Context declarations,
not heterogeneous proof-premise layouts. Reversing validation merely to remove
the array would permit inspection of earlier unchecked path inputs.
Initial reproduction and debug logs: `/tmp/a-program-authority-identity-context-`
`{before,build,debug-identity,debug-core}.log`. Original A3-A5, parent R2-R5 and
both cumulative net-negative requirements remain open.

Builds use `src/prototype/pointer/Makefile`, strict C11 warnings, `-O0 -g`,
`-O2`, and `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-pie -no-pie`; sanitizer runs enable leak detection and halt-on-error.
Final logs use the same prefix: `opt.log`, `asan-build.log`,
`asan-{identity,core,identity-io,derivation-io}.log`, and the debug logs above.
The reproducer exercises rejection at the checked C API, not failure of a
previously accepted surface program. The positive case keeps the family in
the unchanged common prefix; it does not establish general family-path rules.
Implementation: `evidence.c` +8/-10 (net -2); tests: `identity.c` +17/-0;
documentation is separate. No new persistent structure or wire tag is added.

### 2026-09-19: Shared Match planning through typed iota

Baseline: local `bbe3fb6`, published `89c011f`. Scope: the two synchronous
elimination-body calls in function-graph planning, not all atomic kernel rules.

- [x] Reproduce the budget violation with a constant Bool Match inside a Fold.
  On the preceding implementation, `suspended_match_body` fails its query-step
  delta assertion (exit 134). Fresh programs keep the observed query cold.
- [x] Use `pg_elimination_body_request` and the graph's existing `await_view`.
  Split children retain their checked reindexed elimination as the existing
  computation field. The same ordinary planning path exposes its body later;
  remove the separate eager loop's body completion. No new state/cursor/cache.
- [x] Cancel the graph at each progress boundary, then finish the same shared
  typed query independently. Focused debug `program_test` passes, including
  existing nested splitting, helpers, graph witnesses and rejection tests.
- [x] Full optimized acceptance (including 63/63 source compatibility cases
  and all four sorting/property/image suites), focused debug program checks,
  and affected ASan/UBSan program, Identity, source/image-origin checks pass.
  All 2,460 normalized exported results agree with `bbe3fb6`; 608 records have
  changed Solve step counts. The old retained QuickSort image loads inertly
  and resaves byte-identically. Group with the verified Identity declaration
  correction for one A4 consumer epoch; A3-A5 and R2-R5 remain open.

Fixed-source debug counters (published/new): length, function-field and Vec
append are unchanged, including arena usage and Solve/typed-query work.
QuickSort Solve steps are 146,388/146,537; typed-query steps 343,194/343,193;
queries 10,417/10,427; proofs 93,016/93,013; occurrences 82,073/82,070; occurrence
actions 28,045/28,042. Core, Context, map, lift and synthesis-job counts are
unchanged. Arena used bytes increase 65,292,512 -> 65,294,208; reserved capacity
stays 65,617,920. This is scheduling granularity, not a demonstrated speedup.
Logs/builds use `/tmp/a-program-authority-graph-iota-` with `before.log`,
`build.log`, `debug.log`, `counts.log`, `opt.log`,
`export-comparison.log`, `asan-{program,identity,source,origins,common}.log`.
Sanitizers enable leak detection and halt-on-error; the inert zero-step load
returns the expected pending exit 3, not an acceptance failure.

The preceding synchronous wrappers already used the same interned query.
This change removes hidden draining, not a second accepted-answer authority.

Timing used fresh source CLI processes, `--steps 1000000
--legacy-intrinsic-dot`, stdout discarded, **without image saving**. Two
31-pair unpinned runs measured function-field medians 13.793/14.472 ms and
13.008/14.156 ms (published/candidate). Follow-up three-way runs pinned to
CPU 2, 101 samples per binary, alternating order, gave these medians:

| Input/run | Published `89c011f` ms | Identity-only `bbe3fb6` ms | Candidate ms |
|---|---:|---:|---:|
| length / 1 | 5.575 | 5.716 | 5.659 |
| length / 2 | 5.527 | 5.629 | 5.550 |
| function-field / 1 | 9.868 | 10.033 | 10.142 |
| function-field / 2 | 9.875 | 9.917 | 9.793 |

The slowdown is not stable under controlled affinity; this neither proves
zero overhead nor establishes a speedup. Fixed-source work/allocation counters
for both small cases are unchanged. Keep the original A5 performance gate
open. Raw samples: the same prefix with `timing{,-repeat}.log`,
`three-way.log`, and `pinned{,-repeat}.log`.

Epoch delta from `89c011f`: `evidence.c` +8/-10; `function_graph.c` +4/-3;
implementation net -1. Tests separately: `identity.c` +17/-0 and `program.c`
+38/-0. Cumulative implementation/header net remains +1,700 from R76 and
+4,116 from R0, so neither overall net-negative gate is satisfied.

### 2026-09-19: Shared substitution state ownership (local)

Baseline: published `e6029f0`. This is physical ownership consolidation, not
removal of duplicate proof authority or a new substitution algorithm.

- [x] Measure before selecting a change. Function-field makes only six index
  prefix checks directly from Context interning; do not present that small
  lookup-order issue as the principal performance cause. `-O2 -g -pg` profiles
  of 50 function-field and 10 imported QuickSort source processes instead show
  substantial substitution/readback/index work. Sampling is too sparse for
  precise small-input percentage claims; these are not benchmark binaries.
- [x] Place shared substitution state in the same existing input arena as its
  request, root and environment. Remove the separate per-request `calloc/free`
  ownership. The existing `input_storage` pointer determines lifetime; no new
  ownership tag, cache, proof rule or wire field is added. Standalone and
  deserialized substitutions still own their state individually. Scratch
  traversal storage is released on completion or cancellation as before.
- [x] Focused strict-debug Core and complete evaluation-image runner pass.
  Extend the existing every-cut shared-image test to check stable state and
  borrowed ownership before/after progress, and independent restored ownership.
- [x] Full optimized acceptance and affected ASan/UBSan Core, evaluation-image
  and program tests pass. All 2,460 exported results and Solve steps match
  `e6029f0`. The old retained QuickSort image resaves byte-identically without
  Solve (expected pending exit 3). Repeat timing is recorded below.
- [x] Incorporate this verified local change into the shared-query
  lifetime/schema-resumption epoch below, not an isolated ownership push.
  A3-A5/R2-R5 remain open; the final epoch gates govern publication.

Imported QuickSort: requests 28,606, Solve steps 146,537, proofs 93,013 and
occurrences 82,070 are unchanged. Each state is 128 bytes. Input arena used
bytes move 8,048,576 -> 11,710,144, exactly the 3,661,568 formerly requested
as separate state allocations. Arena capacity moves 8,093,696 -> 11,763,712
(494 -> 718 blocks). This is not a 3.66 MB total-memory increase: the old
separate states must be counted too. Allocator overhead/RSS are not measured.

First 31-pair source timing, strict O2, CPU 2, alternating fresh processes:
length 5.715/5.698 ms; function-field 9.911/9.813; Vec append 7.275/6.868;
QuickSort 209.788/205.453. Small controls: Bool .503/.509; add .953/.970.
These are preliminary medians, not a demonstrated general speedup.
The same 31-pair protocol repeated after all tests finished gives length
5.578/5.649 ms; function-field 10.029/9.838; Vec 7.020/7.229; QuickSort
210.444/205.265; Bool .537/.540; add .970/.980. QuickSort improves about 2%
in both runs, but the Vec result reverses direction. Do not extrapolate this
to a general speedup or substitute it for the original A5 baseline matrix.
Logs/builds: `/tmp/a-program-authority-substitution-owner-` with
`debug-{build,core,io}.log`, `counts-{before,after}.log`,
`timing{,-repeat}.log`, `opt.log`, `common.log` and
`asan-{build,core,io,program}.log`. Strict warnings are enabled; ASan/UBSan uses
O1/g, non-PIE, frame pointers, leak detection and halt-on-error.
Earlier profiles use `/tmp/a-program-authority-consumer-profile-`
`{build,field,quick}.log`; the Context trace is
`/tmp/a-program-authority-context-intern-before.log`.

Implementation/header delta: `eval.c` +3/-2 and `eval_internal.h` +1/-1,
net +1; existing evaluation-image tests +4/-0; documentation separate.
Cumulative implementation/header net is +1,701 from R76 and +4,117 from R0.
The overall reduction requirement remains unmet; this change unifies lifetime
management, not the representations or number of substitution results.

### 2026-09-19: Explicit action recipes and dependent weakening audit

Baseline: local `a8aa05f`. The joint audit resolves the earlier uncertain
normalization proposal; it does not claim all reconstruction is necessary.

| Path | Current contract and disposition |
|---|---|
| `pg_occurrence_projection` / unprojection | Preserve the supplied source/map recipe. Cancellation checks an exact weakening, not only equal erased results. Keep this distinction. |
| `context_map_extend` / `pg_context_lift_request` | Build maps from typed images, retaining their classifier and dependency edges. Freshening/index lifting is structural work, not proof-history reconstruction. |
| `substitution_build` | Check dependent suffix declarations against supplied images; preserve their proof premises. An accepted prefix avoids repeating its classifier checks. |
| `map_dependency` / `structural_dependency` | Admit unaccepted explicit structures through ordinary rules, using the same conclusion index. Returning a different canonical subject is not admission of the requested subject. |
| `elimination_instance` / mapped input action | Lift the generic motive's indices and scrutinee together, retaining the nominal formation separately. Composing only erased output maps loses this agreement. |

Direct and indirect weakening may have identical Core, classifier and Context
while carrying different explicit recipes. Interning each exact recipe and
sharing each exact action is required; identifying these two recipes is not
part of this repair. A future representation change must preserve admission
and dependent input access together, rather than weaken either check.

- [x] Extend `tests/iadt.c:indexed_match` with direct and two-stage weakening
  into the same enlarged Context. Check equal Core/classifier, distinct exact
  premise chains, and ordinary derivation validation. Run both through the
  existing chunked motive-opening checks: dependent types, fresh binders,
  unchanged nominal formation and repeat-query work reuse.
- [x] Strict-debug IADT test passes on the current implementation.
- [x] Optimized IADT/full `check-acceptance` and affected ASan/UBSan IADT/Core
  checks pass, with leak detection and halt-on-error enabled. Strict-debug
  Core also passes. All 2,460 exported results, including Solve steps, match
  the preceding `a8aa05f` acceptance run. Logs use
  `/tmp/a-program-authority-dependent-weakening-` with
  `{debug,opt}-build.log`, `{debug,opt,asan}.log`,
  `{debug,asan}-core.log` and `acceptance.log`.
- [ ] Group publication with a substantive implementation epoch, not this
  audit. Remote Main/rewrite remain `e6029f0`; no push follows from these tests.

This audit changes no implementation/header lines. `tests/iadt.c` adds 11
and removes 1 line (net +10); documentation is accounted separately.
Cumulative implementation/header net remains +1,701 from R76 and +4,117 from
R0. A3-A5/R2-R5 and the overall reduction requirement remain open.

Other inspected proof reads are not interchangeable with raw Context data:
`binding_level` reads certified Universe bounds, which `declared_type` alone
does not store. `typed_field_step` uses a constructor's checked field telescope
and preceding field reductions to justify a changed dependent classifier.
Neither is a reason to invent another classifier cache or discard conversion
premises. This does not certify every evidence consumer as minimal.

A separate measurement found 1,029 empty versus 27,577 nonempty substitution
initializations for imported QuickSort. Do not add a second trivial-substitution
engine on this evidence: empty requests are already exact-key shared and this
count alone does not demonstrate a material bottleneck. No such code change
was made. Log: `/tmp/a-program-authority-substitution-empty-before.log`.

### 2026-09-19: Parameter-origin join measurement

Baseline: `21faa54`. Do not replace the existing allocation/Context join with
an arbitrary first origin or a new completion cache. A direct allocation-to-job
index would need to preserve lexical selection and all recorded dependency
Contexts, including unaccepted imported inputs, not merely the first job's
scope. The current code does not establish a one-origin invariant.

An attempted empty-Context regression was withdrawn before implementation:
even the surface declaration `D:=@{z:*;};` retains an internal Universe binder
in its declaration parameters. Its exported type has an empty Context, but
that is not the declaration's allocation Context. The test failed at this
incorrect premise, not at the proposed absence of reverse references. No
registration change or weakened test remains.

GDB callback counts on the existing strict-debug binary:

| Save | Parameter callbacks | Distinct allocation/Context pairs | Prefix steps | Lexical candidates / matching candidates |
|---|---:|---:|---:|---:|
| Existing retained QuickSort image, zero Solve | 27 | 14 | 52 | 25 / 21 |
| Fresh imported QuickSort property, retained save | 27 | 14 | 52 | 43 / 21 |

Each pair is visited at most twice, through family and matcher reachability.
The inert save returns pending (exit 3) and is byte-identical to its input;
the fresh run completes at 146,537 Solve steps. These counts identify repeated
work but do not establish a material runtime bottleneck or a worst-case bound.
Do not add another index on this measurement alone. A3 remains open for
unrelated candidates, shared matchers and parameter-prefix traversal; A4's
remaining synchronous typed consumers are a separate work item.

Logs: `/tmp/a-program-authority-parameter-audit{,-fresh}.log`; inert input:
`/tmp/a-program-authority-case-scopes-common.a`; output:
`/tmp/a-program-authority-parameter-audit-resave.a`. The withdrawn test's
diagnostic is `/tmp/a-program-authority-empty-parameters-before.log`.
The restored strict-debug source-image binary passes `nominal-write`;
logs: `/tmp/a-program-authority-parameter-audit-restored{,-build}.log`.
This audit changes documentation only and is not a Main publication epoch.

### 2026-09-19: Incremental helper-schema application

Baseline: `59e0677`. `helper_application(..., witness=0)` synchronously drains
typed application-body queries during `case_branch`. Simply returning pending
would reconstruct the case's fresh binders and maps. Retain that construction
position, not another query result authority.

- [x] Keep one private case cursor for the current schema construction. Reuse
  the state pointer occupied by helper discovery, which ends before Self is
  formed. Dependency lookup/supply must reject the later schema phase.
- [x] Preserve the current map, Context, call order and helper argument while
  waiting through the existing `application_body`/shared-query wait slot.
  Initialize the case and allocate its argument buffer only once. Later calls
  consume earlier result variables through the same substitution pairing.
- [x] Separate selecting/reindexing the helper from applying it. Schema beta
  uses the existing typed query with ordinary family-application fallback;
  witness application keeps its ordinary APP rule. No new proof rule, Core
  tag, global scheduler, acceptance cache or image field is introduced.
- [x] Test cancellation across every graph step for two dependent helper calls,
  generic parameter application, zero-budget stability, late supply rejection
  and final witness construction; retain all existing graph/sort tests.
- [x] Measure old/new query draining and allocation counts; run full optimized
  acceptance and affected debug, sanitizer and inert-image checks. Report any
  increased cursor/storage cost and changed scheduling separately from results.
- [x] Prepare the tested shared-query lifetime/schema-resumption epoch for
  publication with the preceding local commits. Recheck remote Main/rewrite
  at `e6029f0` and use a non-force atomic push; Git records its outcome.
  A3-A5, R2-R5 and the cumulative net-negative requirement remain open.

Initial verification: strict-debug program, full O2 acceptance, ASan/UBSan
program and `image_origins.sh` pass. The 2,460 exported results match the
preceding acceptance run after removing step counts; 384 records have changed
Solve counts. The retained common image still resaves byte-identically at zero
Solve (pending exit 3). Final reruns also pass after whitespace cleanup and
an explicit assertion that the cancellation fixture actually supplies a helper.
GDB confirms two retained helper calls and a parameterized helper Context,
rather than a fixture which only inlines both calls.

Imported QuickSort measures 20 helper-schema application queries, performing
47 query steps before and after. Previously a synchronous call advanced up to
3 steps with an unbounded budget; now each measured advance is at most 1 step,
including resumption outside `case_branch`. The schema function is entered
21 -> 41 times; enclosing Solve steps rise 146,537 -> 146,564. This is finer
scheduling, not reduced query work. Other checked kernel subrules are outside
this local bound.

Proofs 93,013, occurrences 82,070, Contexts 4,389, maps 15,670, lifts 3,757,
actions 28,042, queries 10,427 and the main/substitution arena sizes are
unchanged. The graph state stays 600 bytes. Eight graph-private arenas use
22,720 -> 23,232 bytes (+512 total, 64 aligned bytes per cursor); their total
capacity remains 131,072. The cursor is a resumable activation, not another
classifier, evidence store or query cache.

First CPU-2-pinned 31-pair alternating fresh-process O2 medians (old/new ms):
Bool .488/.494; add .930/.956; length 5.665/5.694; function-field 9.714/9.970;
Vec append 7.022/7.153; QuickSort 205.112/205.404. No speedup is claimed.
The post-test repeat gives Bool .519/.515; add .935/.950; length 5.537/5.710;
function-field 9.790/9.745; Vec 7.094/7.072; QuickSort 203.822/205.397.
Small differences vary by workload/run; length is 3.1% slower in the repeat
and QuickSort 0.8% slower. This is not a speed optimization or a replacement
for the original A5 baseline matrix.

Logs use `/tmp/a-program-authority-helper-schema-` with `debug.log`,
`opt.log`, `asan.log`, `asan-origins.log`, `common.log`,
`query-{before,after,resume}.log`, `counts-{before,after}.log`,
`private-{before,after}.log` and `timing.log`. The `query-after` probe only
intercepted the old wrapper's caller, so its zero is not a work count; the
`query-resume` probe follows the current shared wait path and records all 47.
Final exact-candidate logs are `publish-opt.log`, `publish-debug.log`,
`publish-asan.log`, `final-asan-origins.log`, `publish-common.log` and
`timing-repeat.log`. Final O2 results again match all 2,460 exports, with 384
changed step records. ASan/UBSan enable leak detection and halt-on-error.

Implementation delta: `function_graph.c` +59/-37 (net +22). Existing program
tests +47/-0; documentation separate. Cumulative implementation/header net is
+1,723 from R76 and +4,139 from R0. The overall reduction gate remains unmet.

Publication scope is one shared-query lifetime/schema-resumption epoch:
combine the local substitution-state ownership change (`a8aa05f`), the
dependent weakening regression (`21faa54`) and this helper-schema migration.
The intervening `59e0677` is an audit, not an additional implementation change.
Do not describe this group as completing A3-A5 or parent R2-R5.

| File relative to published Main `e6029f0` | Added | Deleted | Net |
|---|---:|---:|---:|
| `eval.c` | 3 | 2 | +1 |
| `eval_internal.h` | 1 | 1 | 0 |
| `function_graph.c` | 59 | 37 | +22 |
| Implementation total | 63 | 40 | +23 |
| `tests/eval_io.c` | 4 | 0 | +4 |
| `tests/iadt.c` | 11 | 1 | +10 |
| `tests/program.c` | 47 | 0 | +47 |

All paths are under `src/prototype/pointer/`. Documentation is separate.

### 2026-09-19: Shared binder-to-image lookup

Baseline: published `0082312`. Typed-map image selection, occurrence action
and substitution-evidence image selection independently scanned the same
immutable binding array. Replace them with `pg_context_map_lookup`, returning
a borrowed slot in the existing `map->images` array, or NULL. The slot is not
a new binding identity, De Bruijn representation, stored index or cache.

- [x] Delete the separate lookup loops in `action_result` and
  `pg_substitution_image`. Typed consumers dereference the slot; evidence
  selects the corresponding premise of the requested substitution derivation.
  Do not search for the first proof of an equal image.
- [x] Remove the old image-only API and migrate all production/test callers.
  Preserve NULL/foreign-owner behavior, exact binder pointers and mapped
  classifier-boundary checks. Image payload tests still compare payloads,
  not the now-distinct slot addresses.
- [x] Add a boundary test: two source binders map to the very same typed
  occurrence, but with distinct supplied proofs. Both proof orders retain
  their exact premises and pass ordinary derivation validation.
- [x] Strict-debug Core passes.
- [x] Complete source-image, full optimized and affected sanitizer gates;
  compare exported results and inert image bytes before local commit.
- [x] Group this small consolidation with the weakening materialization epoch below, not an
  isolated Main push. The broader A3-A5/R2-R5 requirements remain open.

The lookup remains linear and allocation-free. This unifies one algorithm;
it does not establish a speedup or the remaining output-sensitive A3 bound.
Implementation/header delta: `evidence.c` +6/-7, `typing.c` +16/-17,
`typing.h` +3/-2, net -1. Tests: `core.c` +20/-5, `source_io.c` +6/-6.
Documentation is separate. Cumulative implementation/header net remains
+1,722 from R76 and +4,138 from R0, so the overall reduction gate is unmet.

Verification: strict-debug Core and the complete `tests/source_io.sh` pass.
ASan/UBSan Core and complete source-image checks pass with leak detection and
halt-on-error enabled. Full `check-acceptance` passes at `-O2 -Werror` (exit 0;
`/tmp/a-program-authority-map-lookup-final-opt.log`). Its 2,460 export records,
including Solve steps and multiplicities, equal the published `0082312` run
after normalizing temporary directory names and parallel output order. A
line-order comparison differed; the record multisets have no missing or extra
entries. No semantic or step-count difference was hidden by the comparison.
The existing retained QuickSort image resaves at zero Solve steps (expected
pending exit 3) byte-for-byte unchanged. Sanitizer source-image log:
`/tmp/a-program-authority-map-lookup-asan-source.log`.

Main and the remote rewrite branch remain at `0082312`; this verified small
consolidation is local work pending a substantive publication epoch, consistent
with the publication policy above. No overall refactoring gate is closed.

### 2026-09-19: Materialize only retained weakening maps

On `ccd0f7b`, instrumented fresh length solving identifies 832 new occurrences
and 224 maps from `pg_context_map_projection -> context_map_extend`. This is
construction of all variables of the destination prefix before selecting an
image, not repeated acceptance of one proof. Raw counts/callers:
`/tmp/a-program-authority-map-creators-length.log` (normal exit, 9,120 steps).

- [x] Add destination-based weakening which constructs the identical declared
  variable directly when its judgement and classifier match. Share its variable
  constructor with explicit projection maps. Preserve oldest-first selection
  even in unchecked contexts containing duplicate binder pointers.
- [x] Use it in ordinary weakening proofs and map extension. Do not change
  explicit-map action, normalize map recipes, discard alternate proofs, or
  infer acceptance from variable structure. Non-variable/converted inputs
  retain the existing explicit map. Keep the extension check for empty prefixes.
- [x] Add Core boundaries comparing exact results with explicit projection:
  ordinary and annotated variables, different classifier/sort, Lambda,
  identity, duplicate raw binders, invalid prefix and NULL input. A variable
  in a 33-binding context must add one occurrence and no full map or proof.
- [x] Run debug Core/IADT/source-image, optimized acceptance, affected
  sanitizers and byte-identical inert resaves; inspect all export/step changes.
- [x] Measure graph/occurrence/map/proof counts and repeated timings against
  `ccd0f7b`; retain no optimization merely because one small case improves.
- [x] Record per-file deltas and decide whether this and the preceding lookup
  consolidation constitute a tested publication epoch. A3-A5/R2-R5 stay open.

Final code limits declaration lookup to `PG_BINDER`: semantic constants cannot
be Context binders. Initial measurements with unnecessary constant scans were
mixed; they are not the final timing baseline. No retained structure, cache,
tag, proof rule or image version is added. Explicit maps still use their own
images and the requested proof derivation. The new entry point is an allocation
choice for the existing projection, not a second weakening semantics.

Final tests all exit 0: debug Core/IADT/complete source-image suite; ASan/UBSan
Core/IADT/complete source-image and `image_origins.sh` with leak detection and
halt-on-error; full optimized `check-acceptance`. Logs share prefix
`/tmp/a-program-authority-weaken-final-` (`opt.log`, `debug-*.log`, `asan-*.log`).
The 2,460 export records equal `ccd0f7b`, including multiplicities and steps,
after normalizing temporary paths and parallel output order. The old retained
QuickSort image resaves byte-identically at zero steps (expected pending exit
3). No implementation/test edits follow these final gates.

Fresh source measurements, same checked definitions before/after:

| Input | Occurrences before/after | Maps before/after | Main arena used bytes before/after | Proofs | Solve steps |
|---|---:|---:|---:|---:|---:|
| length | 3540 / 3338 | 1343 / 1231 | 3727936 / 3680224 | 4864 | 9120 |
| function-field | 6687 / 6398 | 2632 / 2392 | 6070624 / 5992320 | 9284 | 12466 |
| imported QuickSort | 82070 / 78818 | 15670 / 13540 | 65294208 / 63910080 | 93013 | 146564 |

Proofs, Contexts, lift/action/query counts, substitution-arena bytes and Solve
steps do not change. Main-arena reduction is 47,712 / 78,304 / 1,384,128 bytes.
These are directly counted arena allocations, not peak RSS estimates. Logs:
`/tmp/a-program-authority-weaken-{length,field,qsort}-counts-{before,final}.log`.

Implementation/header changes in this step: `typing.c` +30/-8, `typing.h`
+4/-0, `evidence.c` +2/-3, net **+25**; tests `core.c` +43/-0. Combined with
`ccd0f7b`, implementation/header delta from published `0082312` is +61/-37,
net **+24**. Cumulative nets remain **+1,747** from R76 and **+4,163** from R0.
Memory reduction does not satisfy the outstanding code-reduction requirement.

Two final timing runs, each 31 alternating fresh processes per binary/input,
pinned to CPU 2, identical O2 flags (milliseconds, before/after medians):

| Input | Run 1 | Run 2 |
|---|---:|---:|
| Bool | .493 / .505 | .495 / .493 |
| add | .913 / .912 | .925 / .936 |
| length | 5.723 / 5.736 | 5.709 / 5.580 |
| function-field | 9.771 / 10.063 | 9.829 / 9.848 |
| Vec append | 7.221 / 7.171 | 7.041 / 6.973 |
| QuickSort | 206.428 / 205.635 | 205.691 / 205.115 |

Logs: `/tmp/a-program-authority-weaken-final-timing{,-repeat}.log`. Timings
are mixed; no general speedup is established. The small QuickSort difference
is not a replacement for A5's R0 source/image/performance matrix. Retain this
change for verified removal of unused intermediate structures and exact proof
preservation, not an asserted timing improvement.

Publication scope: the exact-image lookup (`ccd0f7b`) and this weakening
materialization form one typed-substitution epoch. All existing full optimized
acceptance tests and affected debug/sanitizer/image gates have passed. Remote
Main/rewrite were verified at `0082312`; recheck and publish the tested commit
atomically without force. Git records the publication result. No issue closure
or A3-A5/R2-R5 completion follows from this epoch.

### 2026-09-19: borrowed accepted premises (local A4 cleanup)

Baseline: published `193708a`. Seven production consumers copy a contiguous
slice of immutable accepted premises without changing it: constructor rule
reconstruction; constructor synthesis; family-path common substitutions;
function-graph case fields, captured branches, packet arguments and leaf fields.
Borrow the existing ordered array through `pg_evidence_premises` instead.
Keep ordinary rule checks and exact derivation identity; the accessor creates
no conclusion, cache, snapshot, rule or lifetime. Storage remains owned by the
typing graph. Empty slices are legal only with zero count.

Do not apply this to arrays extended with new results, reindexed images,
Identity boundary projections, reordered fields, or negative tests that modify
premises. Those are actual transformations, not redundant copies. In particular
substitution composition still computes each image using the shared action.

- [x] Remove the seven copy-only allocations and their loops/error paths.
- [x] Exercise the borrowed view through existing Core derivation reconstruction
  and IADT source/image/small-budget rule checks; retain mutable negative cases.
- [x] Run strict debug Core/IADT/source-image, affected ASan/UBSan checks and
  full optimized acceptance; compare exports, steps and inert retained images.
- [x] Record final deltas/results below and retain this cleanup locally.
- [ ] Group Main publication with a substantive completed epoch; do not treat
  this cleanup as completion of A3-A5/R2-R5.

Composition audit before this edit: QuickSort makes 3,794 composition calls
across 3,037 exact proof pairs, visiting 18,650 images; repeat pairs account for
1,195 image visits. Constructed-field transport makes 265 calls across 163 pairs,
with 168 image visits and 21 repeat visits. Logs:
`/tmp/a-program-authority-composition-counts-{qsort,transport}.log`.
These counts do not justify a new persistent composition-result cache or a
second acceptance authority. The current change removes copied storage instead;
it makes no speedup or overall code-reduction claim.

Final checks exit 0: debug Core/IADT/source-image; ASan/UBSan Core/IADT,
synthesis, Identity, source-image and image origins (leak detection and
halt-on-error); full O2 `check-acceptance`. Logs use prefix
`/tmp/a-program-authority-premises-`. Initial IADT compilation caught the
negative test's deliberate premise mutation; its owned copy is retained.
The first origins script lacked its sibling CLI binary; after building it,
`asan-origins-final.log` passes. Neither failure is suppressed.
All 2,460 export records, including steps and multiplicities, equal the
published baseline after temporary-path/order normalization. Zero-step retained
QuickSort resave returns pending (3) and is byte-identical to the input image.

| File | Added | Deleted | Net |
|---|---:|---:|---:|
| `derivation.c` | 2 | 6 | -4 |
| `evidence.c` | 1 | 0 | +1 |
| `evidence.h` | 3 | 0 | +3 |
| `function_graph.c` | 4 | 12 | -8 |
| `synthesis.c` | 2 | 11 | -9 |
| **Implementation/headers** | **12** | **29** | **-17** |
| `tests/core.c` | 4 | 0 | +4 |
| `tests/iadt.c` | 1 | 1 | 0 |

Documentation is separate. Cumulative implementation/header delta remains
R76 +3,571/-1,841 (net +1,730), R0 +8,326/-4,180 (net +4,146).
The overall net-negative gate is still unmet. CPU-2-pinned O2 timings, 31
alternating fresh processes per input, medians in milliseconds before/after:
Bool .491/.504; add .933/.917; length 5.542/5.638; function-field 9.863/9.746;
Vec append 7.181/6.925; QuickSort 188.980/188.550. Mixed differences establish
no general speedup; this is not A5's R0 performance matrix. Timing log:
`/tmp/a-program-authority-premises-timing.log`. No Main push or issue closure
follows from this local cleanup. A3-A5/R2-R5 remain open.

### 2026-09-19: A3 matcher-origin counterexample

Baseline: `8ee3826`. Do not remove matcher-to-declaration dependency discovery
merely because a runtime layout does not determine a nominal family. Trial
removal of both that reverse edge and `index_origin`'s matcher waiter fails
`member_use_origins`; removing either alone also fails. The exact mismatch is
a constructor semantic pointer, not just alpha-renamed binders. QuickSort's
inert retained-image byte comparison and Match-origin tests still pass, so
those checks alone are insufficient. All trial production edits are withdrawn.

Reduced source (no sibling aliases or deliberately invalid roots needed):

```text
{{ Nat:=@{zero:*;}; Box:=&(\A:@=>@{mk:A->*;}); r:=&(Box Nat).mk; }}.r
```

With only the reverse edge removed, write and two inert resaves succeed;
`retained-recompute` then fails with `alpha_equal=0`, first difference
`aabafreference`. A generative constructor layout is not reconstructible from
the specialized wrapper's field Context alone. Its uninstantiated declaration
must retain its original allocation. Matcher discovery currently recovers that
dependency; it is not a rule equating families with the same layout. Evidence:
`/tmp/a-program-authority-nominal-origin-constructor-{2,3}.log`,
`/tmp/a-program-authority-nominal-origin-difference.log`,
`/tmp/a-program-origin-minimal-failure.log`.

- [x] Add `specialized-constructor` to the existing cross-process retention
  matrix, for source-only/source-plus-proof roots and checked reuse/recompute.
- [x] Show the minimal source fails after deleting reverse matcher discovery;
  restore production code and pass the complete debug source-image suite.
- [x] Run the same matrix under O2 and ASan/UBSan. No test is weakened.
- [x] Before replacing the reverse join, trace the existing constructor input's
  formation reference through generic application and qualified-member source
  transport. Establish whether it can retain the precise declaration allocation
  without building a proof solely for discovery. Do not add a new provenance
  cache or merge layouts as a substitute for this edge.
  Resolved by the direct-origin registration below: the pre-Solve allocation
  already supplies the Context; no formation evidence is needed for discovery.

Current join counts on `parameter_origins`: before/after 128 unrelated aliases
and one extra nominal family sharing a matcher, parameter callbacks 2/3,
Context-prefix visits 6/9, allocation candidates 2/3; origin attempts remain 4
and wait requests 4 (two repeated tuples). The 128 aliases themselves do not
increase this work. The extra family does, identifying the remaining join
cost rather than justifying deletion of its semantics. Log:
`/tmp/a-program-origin-join-counts.log`. This audit does not close A3 or trigger
Main publication; the previous premise cleanup remains local too.

Final complete `source_io.sh` runs pass in strict debug, O2 and ASan/UBSan
(leak detection and halt-on-error). Logs:
`/tmp/a-program-authority-nominal-origin-final-{debug,opt,asan}.log`.
Only tests change: `tests/source_io.c` +2/-0 and `tests/source_io.sh` +1/-1.
Production/header delta is zero; no fresh full-acceptance claim is made for
this test-only checkpoint. Production is exactly the previously tested `8ee3826`.

Initial edge trace: `pg_synthesis_constructor_input` exposes formation and
parameter producers on a prepared constructor wrapper. In contrast,
`pg_synthesis_member_allocation` can read an imported qualified member before
Solve solely from its retained `(prefix, end)` Context pair. Its wrapper is
not yet available. A writer cannot simply demand the former from the latter
without introducing preparation or retaining proofs solely to recover an
allocation again. `pg_data_layout` contains constructor arities, not the
nominal declaration's parameter/field Contexts, so there is no unique inverse
from the constructor pointer either. The next design must preserve the precise
source allocation dependency through this pre-Solve boundary, using an existing
edge if available; the general matcher reverse scan remains until then.

### 2026-09-19: direct allocation-origin registration

Baseline: `b2bbf33`. Preserve matcher discovery, but resolve the source binding
relation once when its owning declaration/Match input registers. Store a direct
reference to that same source job under its family/matcher/Self when its lexical
binder belongs to its allocation Context. Unrelated named aliases retain their
ordinary lexical references, not reverse allocation-origin edges. Explicitly
selected aliases and invalid roots still serialize through the producer graph.

Replace `ALLOCATION_CONTEXT` entries with `ALLOCATION_ORIGIN`, remove the copied
Context pointer and parameter visitor, and delete the writer's Context-prefix
scan plus object/candidate join. This changes no Core tag, typed rule, equality,
acceptance, descriptor or wire format. Raw imported allocations register the
same edges before Solve. A direct edge selects only a candidate: syntax and
lexical reachability checks and ordered serialization remain unchanged.

- [x] Implement direct edges using the existing source-reference index; remove
  `register_allocation_context` and writer parameter/allocation batch callbacks.
- [x] Pass focused debug source-image tests, including specialized constructors,
  and byte-identical inert retained QuickSort resave.
- [x] Extend independent same-matcher declarations to 128 and verify constant
  candidate counts, unchanged selected bytes, aliases and invalid-root behavior.
- [x] Verify the full optimized suite, affected debug/sanitizer/image gates;
  compare exact exports/steps, allocation counts and source/save timings.
- [x] Record per-file deltas and limitations before deciding publication of this
  epoch together with the preceding local premise cleanup and provenance tests.

Do not claim every A3 bound is solved: eligible inputs sharing a source binder
still need syntax/ancestry filtering. Match origins use their own immutable
prefix, not a union of other uses' Contexts. Required checks include the existing
retained/modified Match inputs and independent rejected roots; shared Self does
not authorize choosing a different source occurrence's binding context.

Validation: `make -C src/prototype/pointer -j2
BUILD=/tmp/a-program-authority-origin-edge-opt check-acceptance` passes. All
2,460 export records, including result and step count, match the previous
premise-cleanup full suite after normalizing only temporary paths and order.
Strict O0 debug and ASan/UBSan core, IADT, synthesis, Identity and complete
`source_io.sh` pass. Sanitizers use `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-pie -no-pie`, leak detection and halt-on-error.
Logs: `/tmp/a-program-authority-origin-edge-{opt,debug-*,asan-*}.log`.
Retained QuickSort read/write at zero steps is byte-identical to
`/tmp/a-program-authority-case-scopes-common.a`; pending remains pending.

Fresh source measurements against published `193708a2` (including the earlier
premise cleanup in this epoch): source-reference entries fall from 142 to 134
for length, 192 to 182 for function-field, and 1,332 to 1,326 for imported
QuickSort. Main-arena used bytes fall by 512, 640 and 384 respectively; reserved
capacity is unchanged. Proofs, typed occurrences, Contexts, maps, lifts, actions,
typed queries and substitution storage are unchanged. Solve steps remain
9,120 / 12,466 / 146,564. Counts: `...-{length,field,qsort}-{before,after}.log`.

O2 timing: CPU 2, one warmup and 31 alternating fresh-process pairs, no concurrent
tests/builds. Medians in milliseconds, published Main / this epoch:

| Input | Source | Source plus save |
|---|---:|---:|
| Bool | .511 / .509 | not measured |
| add | .947 / .971 | not measured |
| length | 5.725 / 5.719 | 6.266 / 6.226 |
| function-field | 9.823 / 9.724 | not measured |
| Vec append | 6.904 / 7.086 | not measured |
| imported QuickSort property | 204.321 / 203.736 | 195.228 / 194.741 |

Source and source/save are separate timing blocks, not subtractable component
costs. Mixed small changes do not establish a general speedup. This is not a
replacement for the R0 performance gate. Raw samples: `...-timing.log`.

Per-file epoch delta from published `193708a2`, excluding documentation:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| `derivation.c` | 2 | 6 | -4 |
| `evidence.c` | 1 | 0 | +1 |
| `evidence.h` | 3 | 0 | +3 |
| `function_graph.c` | 4 | 12 | -8 |
| `source_io.c` | 1 | 26 | -25 |
| `synthesis.c` | 20 | 40 | -20 |
| `synthesis.h` | 3 | 4 | -1 |
| `tests/core.c` | 4 | 0 | +4 |
| `tests/iadt.c` | 1 | 1 | 0 |
| `tests/source_io.c` | 28 | 58 | -30 |
| `tests/source_io.sh` | 1 | 1 | 0 |

Implementation/header total: +34/-88, net **-54**. Tests: +34/-60, net **-26**;
replaced descriptor-inspection helpers are removed, negative cases retained.
This direct-origin step alone removes 37 implementation/header lines and 32
test lines. Cumulative implementation/header totals remain positive:
R76 `3a3bf550` +3,536/-1,843 = **+1,693**; R0 `4657cc6`
+8,290/-4,181 = **+4,109**. The original reduction gate remains unmet.

Publication scope: immutable premise borrowing (`8ee3826`), the specialized
constructor regression (`b2bbf33`), and direct allocation-origin registration.
No Core or artifact format changes. This coherent removal of reconstruction
and copy-only paths does not complete A3-A5 or parent R2-R5.

The affected ASan/UBSan `image_origins.sh` gate also passes
(`...-asan-origins-final.log`). Its initial invocation lacked the CLI binary;
after building that binary with identical sanitizer flags, the complete runner
passes. No test input or assertion was changed to pass it.
All epoch publication gates pass. Prepare the commit and atomic non-force push
to Main and `rewrite/pointer-core-hott`; the Git remote records its resulting
revision. Remaining A3-A5/R2-R5 gates above stay open.

### 2026-09-19: Lambda structure without classifier reconstruction

Baseline: published `f3da3c3`. `term_structure_step` currently requests the
entire Pi structure just to recover a Lambda binder. A known binding and body
can therefore wait for unrelated codomain normalization. This is a removable
computation/type dependency, not permission to accept the Lambda without Pi
formation or body-classifier agreement.

- [x] Reproduce with a pending effect Context and normalized codomain: the
  new regression cannot finish its Core-only query on the baseline (debug
  `...-lambda-binding-before.log`, bounded test-driver assertion).
- [x] Share binding-address extraction with Pi construction. Read the existing
  Context/input binding, keeping opaque computed Pi inputs dependent on the
  producer that actually determines that binding. No new Core/job tag or store.
- [x] Verify the valid Lambda after effect closure and reject a value body
  under the same Pi; both provisional Core snapshots remain non-evidence.
- [x] Run affected debug/sanitizer tests and full optimized acceptance; compare
  result records, work counts and LOC. Publish only with completed epoch gates.

Other pending classifier, transport and synchronous-query paths remain open.

Implementation: one read-only `context_binding` helper replaces Pi's local
extraction; Lambda uses that same address without requesting Pi structure.
When a Pi is opaque, its preparation/acceptance must first expose the binding.
The existing term query retains the binder while its body progresses. There is
no new request kind, field, proof rule, snapshot store or Core encoding.
`pg_prove_lambda` still checks Pi formation, body polarity, Context and classifier.

The regression uses the existing pending-effect fixture at chunks 1/64. A
normalized codomain cannot finish before effect closure, yet both a valid and
an invalid raw Lambda have constructible Core. A subsequent explicit Pi query
allocates its first request, proving Lambda did not already request it. Closing
the equation accepts the valid Lambda with exactly the earlier Core and rejects
the raw value body; neither provisional snapshot changes into a certificate.
Source Lambda bodies still receive their ordinary implicit Return elaboration.

Strict debug and ASan/UBSan synthesis tests pass, including the final extra
no-Pi-request assertion. Complete debug/sanitizer source-image runners and
sanitizer `image_origins.sh` also pass; zero-work retained QuickSort resave is
byte-identical to the preceding image. Flags match the direct-origin epoch.
Logs: `/tmp/a-program-authority-lambda-binding-{debug-final,asan-synthesis-final,
debug-source,asan-source,asan-origins}.log`.

Fresh length/function-field/imported-QuickSort measurements retain exactly
9,120 / 12,466 / 146,564 Solve steps, 4,864 / 9,284 / 93,013 proofs and
3,338 / 6,398 / 78,818 typed occurrences. Context/map/lift/query counts and both
arena sizes also match the preceding direct-origin measurements. This removes
an unnecessary dependency in pending structural queries, not a demonstrated
speed bottleneck in those three completed programs. No wall-time speedup is
claimed. GDB counts: `...-{length,field,qsort}-after.log`.

Delta from `f3da3c3`: `synthesis.c` +62/-21 (net +41),
`tests/synthesis.c` +29/-0; documentation separate. Cumulative implementation/
header totals remain R76 +3,597/-1,863 = +1,734, R0 +8,352/-4,202 = +4,150.
Neither net-negative gate is met. Keep this limited correction local for the
next coherent epoch rather than treating it as completion of A3-A5/R2-R5.

Final full optimized `make -C src/prototype/pointer -j2
BUILD=/tmp/a-program-authority-lambda-binding-opt check-acceptance` passes
(`...-opt-final.log`). All 2,460 export records, including steps, match the
published direct-origin epoch after temporary-path/order normalization only.
Final synthesis regressions also pass in debug and ASan/UBSan. The working tree
is prepared as one local commit; Main remains at `f3da3c3`, with no independent
publication for this partial pending-structure correction.

### 2026-09-19: shared compound-term structural assembly

Baseline: local `fa8e1b5`. App, request and zero-clause Fold duplicate the
Handler structure builder's operand request/wait/forward logic. Consolidate
that assembly in the existing structural-query implementation. Core builders
and checked theorem rules remain explicit and unchanged; zero-clause Fold
allocates no clause state. Preserve existing failure status classification.

- [x] Remove the second operand assembly path; introduce no job, field or index.
- [x] Fix exact 0/1/2-clause Core expectations in the existing pending-effect
  tests, including reuse of already requested child structures at chunks 1/64.
- [x] Run debug/sanitizer synthesis and source-image tests, full optimized
  acceptance, and compare exports/steps and implementation/test LOC.
- [x] Record validation and publication scope together with the preceding
  Lambda dependency correction. Do not equate this with completion of A3-A5.

Validation: full optimized `check-acceptance` exits 0; all 2,460 export records
(including Solve steps) match the preceding Lambda correction after temporary
path/order normalization. Debug and ASan/UBSan synthesis and source-image
runners, and sanitizer `image_origins.sh`, exit 0. Logs use the prefix
`/tmp/a-program-authority-compound-` with suffixes `opt-final.log`,
`debug-final.log`, `debug-source.log`, `asan-synthesis.log`, `asan-source.log`
and `asan-origins.log`. Zero-step QuickSort resave with `--retain-reductions`
is byte-identical; omitting that option intentionally changes the image policy.

This consolidation: `synthesis.c` +32/-36 (net -4); synthesis tests +11/-0.
Together with unpublished `fa8e1b5`, relative to Main `f3da3c3`: implementation
+91/-54 (net +37), tests +40/-0; documentation counted separately. Neither
the cumulative net-negative gate nor A3-A5 is complete. Keep these changes
local until a substantial completed refactoring epoch; passing this narrow
cleanup's tests does not alone make it such an epoch. Both remote branches
still point to `f3da3c3`; no additional Main push is made here.

### 2026-09-19: preserve DAG sharing during independence checks

Baseline: local `363beb7`. A4's synchronous index-transport audit exposed a
lower-level duplication: `pg_term_independent` uses the structural comparison
walker with a single `x -> absent` scope. Unrelated lambdas added identity scope
entries, giving the same shared body a different cache key on every path.
A linear-size diamond DAG could therefore cause tree-size work. This is not a
new equality or a reason to intern alpha/WHNF-equivalent Core terms.

For comparison of a term with itself under that single absent binding, preserve
the scope across other binders; the matching binder discharges the question.
Other alpha-comparison scopes retain their existing shadowing rules. No new
query kind, semantic authority, persistent format or global cache is needed.

- [x] Reproduce on the baseline: 32 diamond levels fail to finish within 1,000
  transitions (`...-independence-before.log`, exit 134 at the new assertion).
- [x] Share the existing scope key; debug regression completes with exactly
  97 tasks. Binding the absent variable skips its entire body (one task).
- [x] Check free/bound occurrences and every save/resume boundary for shared
  positive/negative DAGs; debug core and comparison/evaluation image tests pass.
- [x] Run full optimized acceptance and affected sanitizer checks; compare
  exports, work counts, retained images, timing and per-file LOC.
- [x] Decide publication of the pending-structure/DAG-sharing epoch only
  after those gates pass. A3, remaining synchronous transport construction,
  A5 and the cumulative net-negative requirement remain open.

Logs/builds use `/tmp/a-program-authority-independence-*`. The separate
`index_transport_scope` algorithm cannot be replaced by ordinary projection:
it keeps later independent declarations while omitting selected binders. Its
resumption still needs an explicit retained cursor, not a restarted scan.

Epoch validation: full optimized `check-acceptance` exits 0. All 2,460 export
records including Solve steps match `363beb7`. Strict-debug core/evaluation
image tests and ASan/UBSan core, synthesis, Identity, IADT and evaluation image
tests exit 0 (leak detection and halt-on-error enabled). Retained QuickSort
zero-step resave is byte-identical. The length/function-field/QuickSort counters
and arena snapshots exactly match the preceding Lambda audit. Logs are
`...-opt.log`, `...-asan-{core,synthesis,identity,iadt,eval-io}.log`,
`...-{length,field,qsort}-counts.log` and `...-timing.log` under the prefix above.

Isolated O2 timing, CPU 2, 31 alternating pairs after warmup, medians in ms
(published `f3da3c3` / candidate): Bool .489/.484; add .916/.924;
length 5.857/5.672; function-field 9.763/9.975; Vec append 7.125/7.003;
QuickSort 188.463/190.954; length save 6.400/6.235;
QuickSort save 206.312/206.848. This is not a general speedup claim or the
final R0 gate. The measured improvement is removal of path-expanded work in
the diamond regression, not fewer accepted proofs or outer Solve steps.

Publication scope: the two preceding local commits plus this DAG-sharing fix
form one pending-structure/shared-walk epoch. Per-file code delta from Main:

| File (`src/prototype/pointer/`) | Added | Deleted | Net |
|---|---:|---:|---:|
| `graph.c` | 6 | 0 | +6 |
| `synthesis.c` | 91 | 54 | +37 |
| `tests/core.c` | 18 | 1 | +17 |
| `tests/eval_io.c` | 9 | 1 | +8 |
| `tests/synthesis.c` | 40 | 0 | +40 |

Implementation net +43; tests net +65; documentation separate. Cumulative
implementation/header deltas: R76 +3,630/-1,894 = +1,736; R0 +8,385/-4,233
= +4,152. Neither net-negative gate is met. Recheck remote Main/rewrite at
`f3da3c3` and publish the tested epoch atomically without force; Git records
the resulting revision. No A3-A5/R2-R5 completion or new issue closure follows.

Documentation delta for this epoch: this priority plan +166/-0 lines and
the parent authority plan +15/-0 lines, excluded from implementation counts.

### A3 selected syntax frontier (2026-09-19)

The exact-scope-only draft was rejected: it lost 80 QuickSort member origins,
including the recursive append use checked by `retained-append-origin`.
Keeping binder keys fixed that loss, but exact-scope-only declaration discovery
still rejected the restored mapped-alias root. Neither test was relaxed.
The invalid nested-block fixture was corrected to a root selection before
drawing conclusions from it. Trial logs use `authority-member-scope-*`.

The corrected common rule indexes a definition/handler scope by its existing
container syntax, a binding scope by its own binder, and other scopes by their
own address. The writer visits these existing references at the selected syntax
frontier, as it already does for scopes and objects. Remove the upward scope
walk; do not reconstruct proofs, add an authority, or identify nominal families.
Syntax/object/lexical ancestry filters remain necessary. In particular, sharing
one block syntax across different environments still needs a candidate-bound
audit; this does not close A3.

- [x] Add 128 unselected definition scopes to each declaration, Match and member
  regression. Selected callback counts and saved bytes stay fixed, without Solve.
- [x] Strict-debug source/image tests; complete O2 `check-acceptance` exits 0.
  All 2,460 export records, including Solve steps, match published `0edf180`.
- [x] Existing retained QuickSort inert resave is byte-identical. Its 206 scopes,
  140 origins, 315 producers and 42 rules are preserved, including append.
- [x] length/function-field/QuickSort work and arena counts exactly match the
  preceding epoch. There is no new retained store or additional source work.
- [x] ASan/UBSan source and full image-CLI scripts exit 0, with leak detection
  and halt-on-error enabled. Run isolated O2 timing after all other tests finish.
- [x] Group this correction with the tested resumable-transport epoch below.
  It does not close the remaining same-syntax candidate audit.

Final logs use `/tmp/a-program-authority-syntax-frontier-*`: `source.log`,
`opt.log`, `counts.log` and `{length,field,qsort}-counts.log`. The old failed
trial's smaller image and lower replay work are not adopted performance results.
A4 synchronous consumers, A5 and cumulative net-negative gates remain open.

Timing (`timing.log`): CPU 2, warmup then 31 alternating pairs, median ms for
published/candidate: Bool .492/.492; add .903/.920; length 5.515/5.733;
function-field 9.781/9.676; Vec append 6.918/7.124; QuickSort 205.082/204.793;
length save 6.229/6.225; QuickSort save 206.154/206.229. No general speedup is
claimed. A3's measured gain is exclusion of the added unselected blocks.
Sanitizer logs: `asan-source.log`, `asan-image-cli.log` under the final prefix.

Per-file additions/deletions: `source_io.c` +1/-0, `synthesis.c` +2/-4,
`synthesis.h` +3/-2, `tests/source_io.c` +38/-3. Implementation/header net 0;
tests net +35. Cumulative implementation deltas remain R76 +1,736 and R0
+4,152. Documentation is separate; no completion or issue closure follows.

### A4 resumable transport measure (2026-09-19)

Baseline `ce2985c`. Preserve the existing finite decreasing-dependency test;
do not replace it with a new refinement or acceptance rule. Its structural
comparison now advances one transition per Solve turn. A private cursor retains
the Context field, comparison phase and two counts; cancellation frees the live
comparison. No new job kind, persistent record or proof authority is introduced.
Constructor boundary prefixes now borrow accepted map images and use the shared
map-rebase query, deleting their temporary image array and per-image loop.

- [x] Add dependent prefix tests at lengths 0/1/2: zero fuel, shared requests,
  exact checked map, and rejection when the discarded binder is still required.
- [x] Save/discard/reload at 15 source-Solve cuts in the dependent-field fixture;
  all results equal the complete normal form. Debugger inspection confirms live
  comparisons at five cuts, across result and argument transport phases.
- [x] Strict-debug synthesis/IADT tests and full optimized `check-acceptance`
  pass. All 2,460 export result records agree after path/order normalization
  and removal of step counts, which now include the formerly hidden traversal.
- [x] Existing retained QuickSort zero-step resave remains byte-identical.
- [x] ASan/UBSan synthesis, IADT, source-image and full image-CLI tests exit 0
  with leak detection and halt-on-error enabled; isolated timing is below.
- [x] Published with `ce2985c` as epoch `df53645` to Main and rewrite;
  leave A3's shared-syntax environment bound, A4's remaining synchronous builders
  and A5's cumulative/performance gates open.

The initial cut test incorrectly used the source-plus-NF step count to choose
source-only cuts: its last cut had already completed. Separate those measurements;
do not relax the required pending status. The first full run failed that test;
`transport-progress-opt-final.log` is the successful corrected run.

QuickSort property counters, baseline/current: Solve 146,564/166,687;
jobs 34,200/34,203; map/proof counts each +3; typed queries +6; graph arena
used +4,416 bytes. Contexts, occurrences, source references and substitution
arena are unchanged. Length/function-field counters are entirely unchanged.
The extra prefix proof/map is ordinary checked data, not a copied answer;
the extra Solve turns account for actual existing comparison work.

Per-file additions/deletions versus Main `0edf180`, including `ce2985c`:
`source_io.c` +1/-0; `synthesis.c` +58/-28; `synthesis.h` +3/-2;
`tests/source_io.c` +38/-3; `tests/synthesis.c` +37/-0;
`tests/image_cli.sh` +17/-0. Implementation/header net +32; tests net +89.
Cumulative implementation/header deltas: R76 +3,662/-1,894 = +1,768;
R0 +8,417/-4,233 = +4,184. Neither net-negative requirement is met.

Logs/builds use `/tmp/a-program-authority-transport-progress-`: `opt-final.log`,
`cuts-final.log`, `{length,field,qsort}-counts.log`, `export-step-diff.log`,
`asan-source.log` and `asan-image-cli-final.log`. Debug binaries use
`/tmp/a-program-authority-transport-map-debug`; the comparison baseline is
`/tmp/a-program-authority-transport-map-before` (debug) / `member-scope-opt` (O2).

Timing (`timing.log`): CPU 2, warmup then 31 alternating pairs, median ms
baseline/current: Bool .491/.503; add .904/.901; length 5.501/5.419;
function-field 9.296/9.188; Vec append 6.816/6.747; QuickSort 167.816/167.203;
length save 5.622/5.592; QuickSort save 169.120/169.260. No general speedup
is claimed. This isolates the transport change, not A5's cumulative R0 gate.
Both remote heads were rechecked at `0edf180`; the atomic, non-force push of
`df53645` succeeded. No issue closure or A3-A5/R2-R5 completion is claimed.

### A3 exact definition environments (2026-09-19)

Baseline Main `d40900a`. The same block syntax in 128 unselected environments
raised source-writer candidate callbacks from 6 to 134, although bytes and Solve
work stayed unchanged. Syntax identity does not identify a lexical instance.

Use the existing definition environment pointer as the allocation reference key.
Selected definition producers expose their already prepared environment through
a read-only view. The writer follows these edges without constructing scopes or
proofs. Its temporary pointer-keyed visited set prevents repeated enumeration
when several producers/retained scopes reach the same environment. This set is
destroyed after writing and is neither serialized data nor checking authority.

The first draft eagerly retained every prepared environment. It passed the
focused tests and old QuickSort resave but failed `sort_insertion.sh`'s partial
inert resave: readback preparation added an empty environment. Withdraw that
behavior, not the test. The final traversal reads the environment's references
and retains it only when an actual selected origin needs it.

- [x] Add the shared-block/128-environment case to `member_use_origins`, including
  fixed selected-candidate counts, identical bytes and no job/scope/proof/Solve
  growth during save. Actual writer callbacks stay 6, measured with GDB.
- [x] Debug source suite, mapped aliases, recursive append and old retained
  QuickSort inert resave pass. Insertion-property 0/100-step inert saves agree.
- [x] Delete the separate allocation-prefix search loop. Only binder keys can
  match a Context binding; other keys skip that walk. Binder search uses the
  existing lookup. QuickSort's 3 non-binder visits are removed; 10 binder lookups
  over 20 fields remain. This small removal is not a general speedup claim.
- [x] Final optimized acceptance and ASan/UBSan source/image-CLI suites finish
  successfully. All 2,460 export records match the published epoch, including
  steps, after temporary-path/order normalization only. No sanitizer diagnostics.
- [x] Length, function-field and QuickSort work/allocation counters match the
  published epoch. Measure isolated timing and review the final code diff.
- [x] Published the exact-environment source traversal epoch `b3effd0` atomically
  to Main and rewrite; both remote heads verified. A3-A5/R2-R5 remain open.

Per-file code delta: `source_io.c` +10/-2; `synthesis.c` +14/-8;
`synthesis.h` +4/-2; `tests/source_io.c` +21/-1. Implementation/header net +16,
tests net +20. Cumulative implementation/header totals: R76 +3,677/-1,893
= +1,784; R0 +8,437/-4,237 = +4,200. Reduction gates remain unmet.

Final logs use `/tmp/a-program-authority-definition-environment-`, including
`debug-source-final.log`, `candidates-final.log`, `opt-final.log`,
`asan-source-final.log`, `asan-image-cli-final.log`, and per-case `*-counts.log`.
The earlier `opt.log` is the failed eager-retention trial, not a passed gate.

Timing (`timing.log`): CPU 2, warmup plus 31 alternating pairs, median ms
published/current: Bool .498/.499; add .919/.908; length 5.593/5.692;
function-field 9.824/9.790; Vec append 7.062/7.050; QuickSort 206.523/205.888;
length save 6.217/6.326; QuickSort save 206.832/206.483. These measurements
do not establish a general speedup or replace A5's cumulative baseline gate.
The epoch removes foreign-environment traversal without changing saved bytes,
accepted evidence or Solve work. Both remote heads were checked at `d40900a`.

### A3 exact handler environments (2026-09-19)

Baseline Main `7ed0b29`. Handler allocation references still used shared syntax
as their key. The new `handler_environment_origins` test adds 128 independent
environments using that same handler syntax; selected lexical candidate counts
grow before the correction. The first test draft incorrectly passed a bare
expression to the program parser; the corrected reproducer fails the intended
candidate-bound assertion, not parsing.

Use the actual scope/binder key for every local environment. Generalize the
existing read-only prepared-environment view to include handler producers,
without another store, preparation path, proof rule or artifact format. Once
no reference registration uses a syntax key, its lookup during syntax traversal
is redundant. Syntax-dependent origin wakeups and ordering must remain.

- [x] Reproduce the candidate-bound failure and correct handler keying.
- [x] Preserve selected image bytes and job/scope/proof/Solve counts during save.
- [x] Initial debug source and optimized full acceptance pass; all 2,460 export
  records, including steps, match Main. Existing retained QuickSort inert save
  is byte-identical. These results precede the syntax-lookup deletion.
- [x] Remove obsolete syntax-key reference lookup. Final optimized acceptance
  passes; 2,460 exports including steps still match Main. Debug source/handler
  tests and the old retained QuickSort zero-step byte comparison pass.
- [x] ASan/UBSan source and handler suites pass, including 4,083 handler
  snapshots across four cases. Final image-CLI sanitizer run exits 0 too.
- [x] Record affected sanitizers, counters, timing and per-file/cumulative LOC.
- [x] Keep this verified cleanup local until the next substantive epoch;
  Main and rewrite remain at `7ed0b29`, verified remotely. No new issue closure
  or A3-A5/R2-R5 completion is claimed.

Logs use `/tmp/a-program-authority-handler-environment-`. The `before.log`
failure is expected; `opt.log` and `asan-image.log` are the intermediate runs.
Shared-binder/allocation reachability bounds and A4/A5 remain open.

After syntax-key lookup removal, debugger snapshots of 31 constructor saves
show exactly one fewer temporary visited key per syntax node (348 total).
Retained scope/origin counts are unchanged; these are temporary traversal
entries, not accepted evidence. Length/function-field/QuickSort counters and
graph/substitution arena usage exactly match Main. Final logs add `-final`;
counter logs are `{length,field,qsort}-counts.log`, and the visited-set audit
is `reference-{before,after}.log`.

Per-file code delta: `source_io.c` +1/-2; `synthesis.c` +9/-5;
`synthesis.h` +5/-5; `tests/source_io.c` +39/-1. Implementation/header net +3,
tests net +38. Cumulative implementation/header: R76 +3,680/-1,893 = +1,787;
R0 +8,437/-4,234 = +4,203. Neither reduction gate is met.

Timing (`timing.log`): CPU 2, warmup plus 31 alternating O2 pairs, median ms
Main/current: Bool .498/.495; add .918/.920; length 5.826/5.871;
function-field 9.858/9.813; Vec append 7.137/7.217; QuickSort 205.097/206.495;
length save 6.389/6.283; QuickSort save 208.830/208.233. No general speedup
is established. Implementation and tests were unchanged during final runs.

### A3 shared-binder counterexample (2026-09-19)

Baseline local `e971051`; Main remains `7ed0b29`. Extend the handler environment
fixture to a Lambda with the same syntax/binder but 128 distinct outer name
environments. The new standalone command `source_io_test binder-environment-bound`
exits 1: selected lexical candidates grow from 2 to 130. The selected field
constructor is synthesized first; pending foreign uses explicitly share its
allocation. This strengthens the initial nullary-constructor case (1 -> 129):
delaying registration until an allocation exists cannot resolve it. Image bytes and
job/scope/proof/Solve counts remain unchanged during saving. This is an
unresolved traversal-cost gate, not an incorrect accepted typing derivation.
It is deliberately outside the passing acceptance target until repaired;
its failure must not be described as a passing negative test.

Rejected trials, all removed from implementation:

1. Key allocations only by exact source scope, retaining existing declaration
   origin edges: the lexical counterexample passes, but an old retained
   QuickSort inert resave drops from 140 origins/206 scopes to 60/76.
2. Add constructor-wrapper origin edges when the scope binder belongs to its
   parameter Context: 129 origins/173 scopes remain. Eleven qualified uses
   are still missing; a closed wrapper need not depend on the ambient binder.
3. Admit those closed wrappers too: counts return to 140/206, but saved bytes
   still differ and `match_motive_authority` loses alias-isolated byte equality.
   Equal counts are not proof of correct provenance or stable ordering.

The implementation is restored exactly to `e971051`. Debug source/handler
suites pass; the old retained QuickSort image again resaves byte-identically
at zero steps. Only the reproducible open gate and this audit are retained.
Logs use `/tmp/a-program-authority-binder-environment-`: `bound-final.log`,
`debug-source-final.log`, `handler-final.log`, `restored.log`, plus failed
`debug-source-direct.log` and `origins-{before,after}.log` for the withdrawn trial.

- [x] Reproduce shared-binder candidate growth without changing source meaning.
- [x] Reject the key-only repair and the broad direct-origin fallback.
- [x] Trace the existing binding declaration and typed/source dependency edges
  that select a lexical environment after specialization. Binder identity alone
  is insufficient; do not replace it with one canonical proof or environment.
- [x] Make selection require both the relevant lexical use and allocation
  reachability, preserving existing origin order. Reuse immutable inputs; do not
  add a second acceptance store, global alias scan or normalization fallback.
- [x] Pass `binder-environment-bound`, add it to ordinary acceptance, and retain
  the Match-alias and old QuickSort byte gates before closing this A3 item.

This audit does not complete A3 or justify a new Main publication. It also does
not authorize removing needed source-origin data merely to reduce saved counts.

Final focused checks: debug/O2/ASan+UBSan source and handler suites exit 0;
the standalone bound gate exits 1 in all three builds with `2 -> 130`, without
sanitizer diagnostics. No full acceptance rerun or speedup is claimed for this
test-only audit. Implementation/header delta is zero; `tests/source_io.c`
adds 37/deletes 10 lines (net +27). The next audit starts with
`source_binding_intern`, `pg_synthesis_binding_input` and the existing prepared
environment view, then `collect_inputs`/`collect_origin`; source allocation,
typed binding and lexical environment must not be collapsed into one identity.

### A3 retained computation inputs (2026-09-19)

Continue from `dd0ea69`. A further rejected candidate grouped allocation
references at the nearest non-binder source parent. It made the standalone
binder bound pass, but `member_use_origins` found additional unselected
transparent children and the old QuickSort resave changed bytes. That trial
is fully withdrawn: lexical ancestors are not interchangeable source uses.

The next check compares a re-synthesized named computation with the actual
input pointers in its retained reduction archive. Printed NF equality and
inert byte equality alone do not establish this property. New diagnostic:

```sh
source_io_test retained-name-input image.a main
```

The image must have retained a normalization of that name. The command does
ordinary Solve and returns nonzero if no retained input is the exact Core;
alpha matches are diagnostic only, never an interning or acceptance fallback.
List 09 passes (two exact matches). A freshly saved QuickSort fixture initially
has zero exact matches and one alpha match. Debugger tracing identifies its
first difference as the freshly allocated sequencing binder in `match_step`.

Unify this binder with the existing source-binding mechanism (syntax, outer
binder sequence, slot zero), as already used for Application/Lambda. The source
binding reader permits slot zero for ordinary Match, not handler syntax. No
new store, proof rule or wire record is added. `computed-match` is a permanent
source IO fixture covering typed/untyped roots, inert resaves, re-computation
and checked NF reuse. With the original implementation it aborts on exact
input comparison, despite alpha equality; the corrected implementation passes.
Conflicting binder attachment and nonzero Match slots must still be rejected.

- [x] Locate the first mismatch with a named-input check and a smaller fixture.
- [x] Replace Match sequencing's fresh allocation with the common source site.
- [x] Add the smaller fixture to both retained writer modes and List named-input
  checks to the image CLI suite.
- [x] Full optimized acceptance and affected debug/sanitizer checks pass.
  All 2,460 export result lines, including Solve steps, match published Main.
  Debug/ASan+UBSan source suites and the sanitizer handler/image CLI suites
  pass; the old QuickSort zero-step resave remains byte-identical.
- [x] Classify the remaining named-input mismatch before changing allocation.
  The index-path/transport mismatch is a separate recomputation/reuse question,
  not a demonstrated loss of saved source identity; see the correction below.
- [ ] Resolve the separate shared-binder candidate bound before closing A3.

The older `/tmp/a-program-authority-case-scopes-common.a` has no exact or alpha
match under this new check even before the repair. Its historical producer
provenance needs separate analysis; do not claim this patch retroactively
recovers missing old addresses. Its existing inert-byte gate remains required.
The new QuickSort diagnostic measures exact retained-input reuse; it is not a
new correctness requirement for independently generated proof binders.
No new Main publication or overall A3-A5 completion follows from this entry.

The corrected Match sequencing allocation was a source-identity defect, not
evidence of an accepted false typing derivation. Logs use
`/tmp/a-program-authority-retained-input-`: `opt.log`,
`debug-source.log`, `asan-source.log`, `asan-handler.log`, `asan-image.log` and
`before-failure.log`. The original smaller fixture fails with exit 134; the
new named QuickSort diagnostic remains nonzero, independently of ordinary
acceptance. Source QuickSort work counts are unchanged: 34,203 jobs, 166,687
steps and 93,016 proofs. The shared source-address records add three reference
entries and 512 used arena bytes (reserved capacity unchanged); Context, map,
typed-query and substitution-arena counts are unchanged too.

Per-file code delta from `dd0ea69`: `synthesis.c` +5/-1; `source_io.h` +1/-1;
`tests/source_io.c` +38/-0; `tests/source_io.sh` +1/-1;
`tests/image_cli.sh` +2/-0. Implementation/header net +4, tests net +40.
Cumulative implementation/header: R76 +3,686/-1,895 = +1,791;
R0 +8,443/-4,236 = +4,207. Neither cumulative reduction gate is met.

Timing (`timing.log`), CPU 2, warmup and 31 alternating O2 pairs, Main/current
median ms: Bool .499/.497; add .906/.920; length 5.890/5.793;
function-field 10.050/9.841; Vec append 6.966/6.987; QuickSort 191.894/191.035;
length save 6.201/6.148; QuickSort save 198.297/195.142. No general speedup is
claimed; source/test files were unchanged throughout final verification.

Smaller index-path diagnostic: save `tests/acceptance/indexed-rigid-refutation.p`
with `pointer-check --nf main --retain-reductions --save image.a`, then run
`source_io_test retained-name-input image.a main`. It returns 1 with zero exact
matches, two alpha matches and 7,696 steps (`rigid-{write,check}.log`). Use this
case before the whole QuickSort when measuring generated-binder reuse.
Keep this correction local with the preceding handler cleanup until that
substantive allocation/provenance epoch is complete and verified.

Audit correction: a trial assigned extra source-binding slots to generic Match
paths and branch substitution lifts. The diagnostic still failed: rigid Match
had zero exact/two alpha matches (7,696 steps), QuickSort zero exact/one alpha
match (153,894 steps). Debugger tracing then reached the fresh variable created
by `constructor_transport_context`, not a source binder. This is part of a
generated type-family proof context. Adding source identities there solely to
make the whole elaborated Core pointer-identical would impose a new contract.

The parent plan already excludes independently fresh reduction allocations
from its exact source-identity requirement (section 6 and the nested-Match
audit). The trial is withdrawn, including its relaxed slot validation. Keep
the original source-allocation, computed-Match, List retained-input and inert
byte tests unchanged. Alpha equality remains diagnostic, not interning or
proof acceptance. The old image with no alpha match still requires separate
provenance analysis; this correction does not explain that result.

Next A3 work remains the shared-binder candidate bound (`2 -> 130`): index
lexical uses by their actual source environment while preserving the defining
input of erased allocations. Do not stabilize every generated proof binder,
invent another evidence authority, or remove needed origins to pass this bound.
No Main push or A3/A4/A5 completion is implied by this audit correction.

After withdrawing the trial, implementation/tests match `a33d073` exactly.
The strict debug rebuild, `tests/source_io.sh` and `handler-origins` pass;
`binder-environment-bound` still exits 1 with `2 -> 130`. Source-suite log:
`/tmp/a-program-authority-source-identity-audit-source.log`. This is a plan
correction, not another full acceptance run or a completed refactoring epoch.

### A3 lexical allocation edges (2026-09-19)

Baseline `190a5d3`; publication baseline `7ed0b29`. Allocate references under
the exact source environment, never the shared binder. On first interning of a
binder environment, index its existing parent edge in `source_references`.
The writer follows it only after reaching the explicit binding syntax, if any,
and its binder on the existing object frontier. Merely reaching a parent does
not save its children. There is no new proof rule, acceptance store, Core node,
artifact record, Solve call, or global environment scan.

Waiting for the frontier matters: the first trial followed binders already
present in the graph before their turn and changed inert-save order. The final
path uses the existing dependency callback, preserving the old QuickSort image
byte-for-byte. Declaration/Match defining-origin edges remain unchanged; do not
replace them with every alias sharing an allocation.

- [x] Delete the scope-or-binder key switch and its duplicate test helper.
- [x] Add the previously failing `binder-environment-bound` to `source_io.sh`.
  Assert that foreign environments do not add selected-parent child edges,
  selected allocation candidates, saved bytes, or Save-time Solve/proof work.
- [x] Strict debug source and handler tests, including 4,083 handler snapshots.
- [x] Full O2 `check-acceptance`; all 2,460 export/step records match Main.
- [x] Affected sanitizer/image checks and isolated timing.
- [x] Remove sibling-environment enumeration before publishing; see the
  measured counterexample below. Passing existing tests is insufficient.
- [x] Publish together with the exact handler environment and Match sequencing
  corrections as one lexical allocation/provenance epoch, after all gates pass.

Debugger measurement of the actual writer, before/after 128 foreign uses:
old `index_scope_origin`/`collect_origin` calls each grow `4 -> 132`; new calls
stay `4 -> 4`. New environment callbacks stay `7 -> 7`, source-reference index
visits `66 -> 66`. Thus the bound is not merely hidden by changing a test key.
QuickSort property synthesis retains 34,203 jobs, 166,687 steps, 93,016 proofs,
and all Context/map/action/query counts. Parent edges add 516 source references
and 33,024 used graph-arena bytes; reserved bytes increase 32,768. This measures
selected arena storage, not peak RSS or a general performance improvement.

Logs: `/tmp/a-program-authority-lexical-edges-{opt,source-final,counts-final}.log`.
The earlier rejected trial log is `source.log`; the final source suite passes.
This closes the specific shared-binder candidate bound, not all A3-A5/R2-R5 or
the cumulative net-negative implementation criterion.

Publication hold: profiling the existing `member_use_origins` fixture exposes
a cost transfer. With 128 unused Lambda children of the selected parent, the
new `index_environment` callbacks grow `5 -> 133`. Actual allocation callbacks
remain `3 -> 3`, origin checks `4 -> 4`, and saved bytes are unchanged. The old
writer did not enumerate these children. Therefore the direct lexical key is
correct, but unconditional parent-to-child enumeration is not the completed
output-sensitive algorithm. Keep this change local; do not call the broader
reachability requirement solved merely because the foreign-parent case passes.

Next: select the needed child through the retained producer/typed dependency
and its existing binding address, instead of enumerating all binder children
of a reached parent. Preserve both dimensions of the counterexample: shared
syntax/binder under foreign parents and unrelated binders under one selected
parent. Moving filtering into the visitor callback would hide the count, not
remove the scan. No first-environment authority or fresh proof reconstruction
is permitted as a workaround.

Final affected checks all exit 0: strict debug source/handler suites, optimized
full acceptance, and ASan/UBSan source/handler/image suites with leak detection
and halt-on-error. Sanitizer logs use the same prefix with
`asan-{source,handler,image}.log`. No source/test edits occurred during checks.
The unbounded sibling scan above is measured by debugger callbacks in the
existing fixture, not treated as a passing negative test.

Epoch delta from Main `7ed0b29` (including unpublished earlier corrections):

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| source_io.c | 24 | 3 | +21 |
| source_io.h | 1 | 1 | 0 |
| synthesis.c | 25 | 17 | +8 |
| synthesis.h | 11 | 7 | +4 |
| tests/image_cli.sh | 2 | 0 | +2 |
| tests/source_io.c | 136 | 14 | +122 |
| tests/source_io.sh | 2 | 1 | +1 |

Implementation/header net +33; tests net +125. Cumulative implementation/header
R76 +3,711/-1,894 = +1,817; R0 +8,469/-4,236 = +4,233. Neither cumulative code
reduction gate is met. Documentation is accounted separately at commit.

Timing (`timing.log`): fresh Main `7ed0b29` O2 binary versus candidate, CPU 2,
31 alternating pairs after validation ended. Median ms: Bool .491/.492;
add .951/.936; length 5.767/5.626; function-field 9.610/9.749;
Vec append 7.169/6.962; QuickSort 183.526/184.179; length save 6.168/6.131;
QuickSort save 202.914/203.579. No general speedup is claimed. Documentation
delta from Main: parent plan +24/-0; this priority plan +311/-0, including this
accounting entry. No publication is made while the sibling scan remains open.

### A3 paired environment lookup (2026-09-19)

Replace the unfiltered parent-child visitor with exact `(parent, binder)`
hash lookup in the existing source-reference index. A unary membership entry
marks binders having source environments; it contains no chosen environment or
proof. The writer joins reached environments with reached marked binders, once
per pair. Snapshotting frontier tails prevents callbacks from revisiting newly
added pairs. Nonmatching children are not enumerated. Explicit binding syntax
still gates selection; examining a scope does not make it an image root.

The first paired trial queried every reached Core binder. It passed correctness
checks but regressed inert QuickSort resave from 10.000 to 14.927 ms. Reject that
loop: the membership index now excludes proof-local/non-source binders before
the join. A second isolated 101-pair O2 comparison measured 10.109/9.930 ms.
This is not a claim of linear output-sensitive complexity: expected hash probes
are `E * B`, where both sets are selected environments/marked source binders,
not the total source universe. Further selected-graph scaling remains measurable.

- [x] Keep exact lexical keys, immutable origin edges and existing wire format.
- [x] Extend the permanent bound fixture with 128 unused same-parent binders
  as well as 128 foreign-parent uses of the same binder/allocation.
- [x] Debug source suite and optimized full acceptance pass; all 2,460
  export/step records match Main. Old retained QuickSort resaves byte-identically.
- [x] Finish sanitizer gates and final isolated timing before publication.

Actual writer counters: the existing sibling fixture keeps environment callbacks
`4 -> 4` and paired lookups `88 -> 88`; the combined fixture keeps them `5 -> 5`
and `140 -> 140`. Both preserve source-reference visits and allocation/origin
callbacks. Profiles use `/tmp/a-program-authority-paired-lookup-` with
`sibling-profile.log` and `combined-profile.log`. Failed-trial timing is
`inert-timing.log`; corrected timing is `inert-timing2.log`.

QuickSort property synthesis still takes 34,203 jobs, 166,687 steps and 93,016
proofs, with unchanged Context/map/action/query counts. Membership adds 514
index entries and 32,896 used graph bytes versus `365bb4d`; no acceptance data
is copied. The writer's new binder set is temporary and freed after saving.
Total source-reference entries are 2,359; graph used bytes 63,980,544, reserved
64,274,432. Record these costs, rather than describing the index as free.

This completes the two measured unrelated-environment bounds. It does not
complete A4/A5/R2-R5, repair general fresh-proof reduction reuse, or meet the
cumulative implementation-line reduction requirement.

Final isolated timing against a fresh strict O2 build of published Main
`7ed0b29`, CPU 2, 31 alternating pairs (median milliseconds, Main/current):
Bool .496/.494; add .930/.907; length 5.578/5.798; function-field 9.751/9.666;
Vec append 7.010/7.059; QuickSort 190.257/191.461; length save 6.325/6.175;
QuickSort save 205.667/205.763. The separate 101-pair inert retained QuickSort
resave comparison is 10.206/10.201 ms. No general speedup is claimed; retain
the length timing variation for future comparisons. Raw logs use the same
prefix with `final-timing.log` and `final-inert-timing.log`.

Final code delta from Main `7ed0b29`, including the unpublished handler,
Match sequencing and exact lexical allocation corrections:

| File under `src/prototype/pointer/` | Added | Deleted | Net |
|---|---:|---:|---:|
| source_io.c | 56 | 10 | +46 |
| source_io.h | 1 | 1 | 0 |
| synthesis.c | 58 | 22 | +36 |
| synthesis.h | 13 | 7 | +6 |
| tests/image_cli.sh | 2 | 0 | +2 |
| tests/source_io.c | 149 | 14 | +135 |
| tests/source_io.sh | 2 | 1 | +1 |

Implementation/headers +128/-40 = +88; tests +153/-15 = +138. Cumulative
implementation/header deltas remain positive: R76 `3a3bf55` +3,769/-1,897
= +1,872; R0 `4657cc6` +8,534/-4,246 = +4,288. Do not count this correctness
and bounded-discovery epoch as satisfying the cumulative reduction gate.
Documentation changes are separate from these source totals.

Final gates pass: strict debug source/handler suites, optimized full acceptance,
and ASan/UBSan source/handler/image suites with leak detection and halt-on-error.
The final image and debug handler reruns also exit 0, recorded in
`asan-image-final.log` and `debug-handler-final.log` under the same log prefix.
All 2,460 normalized export/step records still match Main. No source/test edits
occurred after the verification builds. Prepare one atomic non-force publication
with `e971051` through `365bb4d`; record the actual resulting commit below.

Published as `09e7ce2dc42b9a08454a02b4f8900d155e5447db` on 2026-09-19.
The atomic non-force push advanced Main and `rewrite/pointer-core-hott` from
`7ed0b29`; both remote heads were verified. This is the completed lexical
allocation/provenance epoch, not completion of A4/A5/R2-R5. No issue status
changes accompany it. Documentation through that implementation commit:
parent plan +34/-1; priority plan +390/-0. This publication note is additional
documentation only; implementation and test totals above are unchanged.

### A4 read-only accepted-input validation (2026-09-19)

Baseline `b0c58e1`. `pg_synthesis_name` called `pg_prove_projection` merely
to decide whether accepted evidence was usable in the parent scope, then
discarded the projected proof and stored the original producer. Registration
therefore allocated an unrequested weakening proof/typed occurrence before a
reference existed. Check the already accepted Context's exact prefix relation
instead. Ordinary name synthesis still constructs/checks the needed projection;
this is not erased-Core lookup, acceptance by a name, or structural interning
of distinct Contexts. Invalid siblings and escaping variables still reject.

Use the existing `typed_input` predicate for same-Context ownership/judgement
validation in classifier normalization and reflexivity/family action. It now
reads accepted data rather than calling a proof constructor as a predicate.
No new semantic owner, cached validity flag, proof rule or work kind is added.

- [x] Add a `pending_names` no-allocation regression. It fails on `b0c58e1`
  (exit 134) and passes after the repair; the subsequent reference still agrees
  with explicit projection. Keep same-depth sibling rejection in the same test.
- [x] Complete full optimized acceptance and affected debug/sanitizer gates;
  compare exported results/steps and report per-file changes.
- [ ] Group publication with a substantive verified A4 epoch. This small
  cleanup alone does not trigger another Main push or complete A4/R2-R5.

The remaining `index_transport_scope` audit found an intentional oldest-first
dependency check, not just a redundant array. Its `index_rebase` calls still
drain typed queries synchronously, as does boundary telescope reconstruction.
Moving that work must preserve the accepted map and candidate failure behavior;
do not delete dependent-field checks or restart a partially built telescope on
each Solve turn. Those consumers remain open, separate from this naming repair.

Verification: full strict O2 `check-acceptance` exits 0; its 2,460 normalized
export/step records match `b0c58e1`'s paired-lookup run exactly. Strict debug and
ASan/UBSan `synthesis_test` plus `tests/source_io.sh` exit 0. Sanitizers use O1/g,
frame pointers, non-PIE, leak detection and halt-on-error. Logs are under
`/tmp/a-program-authority-name-read-`: `before.log` (expected old-code failure),
`acceptance.log`, `debug-{synthesis,source}.log`, and
`asan-{synthesis,source}.log`. No source/test edits occurred during final runs.

Per-file code delta from `b0c58e1`: `synthesis.c` +9/-14, `synthesis.h` +2/-0,
`tests/synthesis.c` +12/-0. Implementation/headers net -3, tests net +12;
documentation is separate. No wall-clock speedup is claimed. The cumulative
implementation/header totals remain R76 +3,784/-1,915 = +1,869 and
R0 +8,536/-4,251 = +4,285; neither reduction gate is met. Keep this verified
cleanup local pending the next substantive A4 publication epoch.

### A4 resumable index-transport scope preparation (2026-09-19)

Baseline: local `56e3c73`. Replace the scope builder's unbounded rebase drain
with retained progress: accepted map, field cursor and the existing typed query.
The oldest-first dependency check is unchanged. Failed domains are still
omitted; later independent declarations remain available. Direct/normalized
attempts share the same prepared scope. Constructor-field attempts retain the
field values across suspension. No new proof rule, semantic cache or image
authority is introduced; private progress is discarded on image reconstruction.

- [x] Advance scope rebase queries with budget 1 without restarting preparation.
- [x] Extend image tests to 15 cuts each for direct, normalized and dependent
  constructor-field transport. Compare resumed normal forms with complete runs.
- [x] Full strict O2 acceptance exits 0; all 2,460 export results match the
  preceding implementation after ignoring scheduling steps and temporary paths.
  Strict debug synthesis/source tests and ASan/UBSan synthesis/source/image
  tests pass. Sanitizers use leak detection and halt-on-error.
- [x] Complete boundary telescope preparation and the branch/pattern audit before
  publishing a substantive transport epoch with the accepted-input cleanup.

A debug probe on `indexed-dependent-field-path.p` finds 16 initializations for
16 distinct scopes, 251 scope advances and 219 query advances, each budget 1.
There are 175 same-field resumptions with an unchanged accepted map. This is
not a bound on every other operation inside a Solve turn. QuickSort keeps
34,203 jobs, 93,016 proofs, 10,433 queries and unchanged Context/map/action
counts. Steps increase 166,687 -> 167,112; graph used bytes increase by 2,048
to 63,982,592, with unchanged reserved bytes and substitution storage.

Isolated strict O2 timing against a fresh `56e3c73` build, CPU 2, 31 alternating
pairs, median milliseconds before/after: Bool .525/.509; add .925/.928;
length 5.856/5.693; function-field 9.647/10.046; Vec append 6.896/7.305;
QuickSort 182.098/183.833; length save 5.904/5.873; QuickSort save
190.806/192.251. Do not claim a speedup or dismiss the measured increases.
Logs use `/tmp/a-program-authority-scope-progress-`: `acceptance-final.log`,
`asan-{synthesis,source,image}.log`, `profile.log`, `counts-{before,after}.log`
and `timing.log`. No source/test edits occurred during final verification.

Per-file source delta from `56e3c73`: `synthesis.c` +123/-54 (net +69);
`tests/image_cli.sh` +19/-16 (net +3). Documentation is separate. Cumulative
implementation/headers remain R76 +3,859/-1,921 = +1,938 and
R0 +8,611/-4,257 = +4,354. Neither net-negative gate is met. This deliberately
retained continuation state is not duplicate accepted evidence, but its cost
must remain visible. A4/A5 and parent R2-R5 remain unfinished; no Main push or
issue closure is claimed for this local checkpoint.

### A4 shared boundary telescope continuation (2026-09-19)

Baseline `b67cdff`. Replace `constructor_transport_context` with one retained
continuation used by both constructor disjointness/injectivity and index-field
transport. It advances the existing rebase, Context lift and occurrence-action
work with budget 1, then constructs ordinary checked maps. Binder allocation
belongs to the unfinished lift, not each wakeup. Delete `index_rebase` and
the unused identity-map construction on dependent boundaries. Delay field
Identity/reflexivity construction until boundary preparation is complete.
The remaining parameter-map rebase also uses its shared budgeted query;
`synthesis.c` no longer calls the synchronous map-rebase adapter.

- [x] Strict debug synthesis, IADT and source suites pass.
- [x] Full strict O2 acceptance exits 0. All 2,460 export records agree with
  the preceding checkpoint after removing step counts and temporary paths.
- [x] ASan/UBSan synthesis, IADT, source and image suites exit 0 with leak
  detection and halt-on-error, including the existing 45 transport image cuts.
- [x] Finish the separate branch/pattern consumer audit and group publication
  with the preceding local changes. Overall A4/A5/R2-R5 remain open.

On `indexed-dependent-field-path.p`, the debug probe observes eight boundary
initializations, 168 advances, 44 query, 28 lift and 104 occurrence-action
advances (each direct call budget 1). All 20 boundary binder allocations are
unique by continuation/field; none repeats across the 148 resumed positions.
This does not assert that all subordinate proof construction is budgeted.
QuickSort retains 34,203 jobs and 10,433 queries. Against `b67cdff`, proofs
decrease 93,016 -> 93,001; occurrences 78,818 -> 78,808; maps 13,543 -> 13,538.
Graph used bytes fall 63,982,592 -> 63,978,912; reserved/substitution storage
and Context/lift/action counts stay unchanged. Steps rise 167,112 -> 167,211.

Isolated O2 timing, fresh `b67cdff` build/current, CPU 2, 31 alternating pairs,
median milliseconds: Bool .499/.500; add .906/.904; length 5.456/5.416;
function-field 9.219/9.071; Vec append 6.815/6.853; QuickSort 166.263/166.822;
length save 5.655/5.625; QuickSort save 167.593/168.386. No general speedup is
claimed. Logs use `/tmp/a-program-authority-boundary-progress-` with suffixes
`acceptance-final.log`, `debug-{synthesis,iadt,source}-final.log`,
`asan-{synthesis,iadt,source,image}-final.log`, `profile-final.log`,
`counts-final.log` and `timing.log`. No source edits followed those builds.

Implementation delta: `synthesis.c` +112/-63 = +49; tests unchanged, relying
on the preceding cut/resume extension and existing positive/negative IADT
transport tests. Documentation is separate. Cumulative implementation/headers:
R76 +3,905/-1,918 = +1,987; R0 +8,665/-4,262 = +4,403. The reduction gates
are still unmet; this continuation is not a second accepted-evidence store.

Next audit: `pg_prove_pattern_type` has seven synthesis call sites, but the
QuickSort probe finds 38 calls with 38 distinct `(prefix, pattern, body)` proof
tuples (`/tmp/a-program-authority-pattern-request-profile.log`). This input
does not justify a new result cache. Preserve its injective variable-image
check, fixed prefix, total typed inverse, dependent-field discharge and final
substitution-back check when considering resumable work. Do not replace that
partial proof-producing solver with unchecked structural substitution or
infer redundant computation merely from the number of call sites.

### A4 branch/pattern consumer audit (2026-09-19)

Measured strict debug `9532e42` on the same retained QuickSort property input.
`pattern_index_type` makes 113 reindex requests for 76 distinct exact
`(substitution, proof)` pairs. All 37 repeats return accepted proofs without
increasing proof, occurrence, map, typed-query or occurrence-action counts.
`pg_prove_reindex` already looks up the receipt before requesting its action.
Do not add a second pattern-image cache for these calls. This measurement does
not make alpha comparison or the whole pattern solver constant-time.

Constructor scope calls are distinct within `match_validate_branch` (20/20)
and `constructor_transport_step` (6/6). `index_constructor_candidate` makes
16 calls for 12 exact `(formation, constructor, parameters)` tuples: the four
repeats belong to two different consumer jobs, not repeated wakeups of one job.
The synchronous scope builder allocates fresh field binders on each call.
`constructor_pattern` also has eight repeated tuples among 51 calls; its
existing checked constructors must be inspected before adding retained state.

- [x] Distinguish repeated accepted-proof lookup from scope reconstruction.
- [x] Examine sharing constructor-scope preparation through the existing
  scheduled scope builder, preserving explicit retained allocations and proof
  premises. Retain completed branch construction across suspension; do not
  replay preceding clauses or add a competing accepted-scope registry.
- [x] Verify the integrated transport epoch and publish it with the preceding
  three local commits. Neither this audit nor those commits alone closes A4.

No implementation or test edits in this audit. GDB runs exit normally at
167,211 Solve steps. Logs: `/tmp/a-program-authority-pattern-consumer-audit.log`
and `/tmp/a-program-authority-pattern-scope-duplicates.log`. Existing finite
pattern inversion and branch proof construction remain synchronous; replacing
them wholesale is not justified by these measurements.

### A4 transport preparation epoch verification (2026-09-19)

Use the existing scheduled constructor-scope producer for both transport branch
builders. Retain producer references and a readiness cursor in their existing
private work; subscribe to completion instead of polling or replaying preceding
branches. Build branch bodies only after their scopes are ready. Explicit
allocation/proof inputs and kernel formation rules are unchanged. This removes
two direct synchronous scope-construction paths, not the standalone kernel API.
Pattern reindex retains its existing receipt lookup; no second cache is added.

- [x] Regression: extract the generated type-case from disjointness/injectivity
  evidence and check that its constructor scopes are already complete, with
  the same field binder and no new jobs. Fails on `8d9f97e` (exit 134), passes
  with this change at chunks 1/64 alongside distinct-path and invalid-input cases.
- [x] Strict debug IADT, synthesis and source suites; full strict O2
  `check-acceptance`; ASan/UBSan IADT, synthesis, source and image suites pass.
  Sanitizers use O1/g, non-PIE, frame pointers, leak detection and halt-on-error.
  All 2,460 export results agree with `9532e42`, ignoring steps and temporary
  paths. The image suite includes the preceding 45 transport cut/resume cases.
- [x] Compare fresh O2 builds of `8d9f97e` and the candidate on CPU 2, 31
  alternating pairs, with no other agent build/test running during measurement.
- [x] Publish this integrated epoch with `56e3c73`, `b67cdff`, `9532e42` and
  `8d9f97e`; verify both remote tips. A4/A5 and parent R2-R5 remain open.

QuickSort: 22 transport scope requests use 18 producers, sharing all four
repeated keys. Against `8d9f97e`, Contexts 4,389 -> 4,387; maps 13,538 ->
13,533; proofs 93,001 -> 92,998; occurrences 78,808 -> 78,807. Jobs rise
34,203 -> 34,286 and steps 167,211 -> 167,364. Queries remain 10,433.
Graph used bytes rise 63,978,912 -> 64,016,896; reserved 64,274,432 ->
64,323,584. Substitution used bytes fall 11,710,144 -> 11,709,568, with
reserved 11,763,712 unchanged. Shared preparation has a scheduling/storage cost.

Median milliseconds before/after: Bool .527/.521; add .957/1.017;
length 5.786/5.743; function-field 9.968/9.643; Vec append 7.174/7.395;
QuickSort 198.813/194.413; length save 6.163/6.205; QuickSort save
206.446/205.776. These mixed results do not establish a general speedup.
Logs use `/tmp/a-program-authority-branch-scopes-`: `before.log`,
`{iadt,synthesis,source}-final.log`, `acceptance.log`,
`asan-{iadt,synthesis,source,image}.log`, `reuse.log`, `counts.log`, `timing.log`.
No implementation/test edits followed final verification builds.

This change: `synthesis.c` +50/-18 = +32; `tests/iadt.c` +28/-0.
Integrated epoch versus Main `b0c58e1`: `synthesis.c` +284/-139 = +145;
`synthesis.h` +2/-0; `tests/iadt.c` +28/-0; `tests/synthesis.c` +12/-0;
`tests/image_cli.sh` +19/-16 = +3. Documentation is separate.
Cumulative implementation/headers: R76 +3,941/-1,922 = +2,019;
R0 +8,700/-4,265 = +4,435. Neither net-negative gate is met. Remaining
Match/type-case scope validation is still synchronous and must be audited on
its own inputs; this epoch does not authorize removing those checks.

Publication: `510a95b98f8fcf445f21a1bc22f9ec7cc03051fd` was atomically
fast-forwarded to Main and `rewrite/pointer-core-hott` from `b0c58e1`.
`git ls-remote` confirmed both tips. This publication note changes no tested
implementation or fixtures; the broader A4/A5/R2-R5 completion gates stay open.

### A4 type-case validation work (2026-09-19)

Baseline `8bd0d8c`. Type-case currently projects/applies each accepted branch
in a fresh constructor scope solely to check its signature. Check that same
signature against the accepted constructor telescope instead: close its fields
with the observed result Universe, then substitute the checked parameters/Self.
Use existing `pg_context_signature`, substitution and alpha comparison; do not
introduce another scope cache, equality rule or untyped acceptance shortcut.
Branch ownership, Context, polarity, full arity, dependent domains and result
Universe must still check. Ordinary Match/induction checking is not changed.

- [x] Remove fresh validation scopes and discarded branch-application proofs.
- [x] Test no new Contexts on first acceptance, dependent-domain mismatch at
  the same arity, Universe bounds, neutral/relocated branches and repeat lookup.
  Also test a parameter image whose free binder is a schema field binder:
  substitution under the closed telescope must avoid capture.
- [x] Run debug/O2/sanitizer/image gates and compare work, storage and timing.
- [x] Record deletion/increase counts below.
- [x] Group with the induction allocation epoch (`0137156`);
  do not treat this local change as completing A4/A5 or the reduction gates.

Verification: strict debug IADT/synthesis/source checks, full strict O2
`check-acceptance`, and ASan/UBSan IADT/synthesis/source/image checks pass.
Sanitizers use O1/g, non-PIE, frame pointers, leak detection and halt-on-error.
All 2,460 optimized export records and 1,218 sanitizer image export records
match the preceding epoch after normalizing temporary paths and step counts.
The first-acceptance no-Context regression fails against the old implementation.
Implementation and tests were unchanged during final builds/runs.

QuickSort before/after: Contexts 4,387/4,376; occurrences 78,807/78,655;
maps 13,533/13,511; lifts 3,755/3,744; actions 28,050/28,039;
proofs 92,998/92,921. Steps 167,364, jobs 34,286 and queries 10,433 are
unchanged. Graph used bytes 64,016,896/63,975,584; reserved bytes
64,323,584/64,274,432. Substitution used bytes 11,709,568/11,709,856;
reserved bytes 11,763,712 unchanged. Capture avoidance may allocate Core
binders; the claim is fewer discarded validation scopes, not zero allocation.

Timing: existing strict O2 baseline/candidate binaries, CPU 2, two independent
runs of 31 alternating pairs, with no other agent build/test during measurement.
Median milliseconds, baseline/candidate (first run; repeat):
Bool .495/.504; .499/.498. Add .965/.970; .926/.939.
Length 5.611/5.701; 5.750/5.559. Function-field 9.397/10.056;
10.112/9.777. Vec append 7.012/7.305; 7.173/7.006.
QuickSort 190.974/191.392; 204.234/203.968. Length save 6.160/6.100;
6.052/6.128. QuickSort save 206.542/206.189; 206.955/206.929.
These mixed measurements do not establish a speedup or a repeatable slowdown.
Logs: `/tmp/a-program-authority-case-signature-` followed by `acceptance.log`,
`asan-{iadt,synthesis,source,image}.log`, `counts.log`, `timing.log` and
`timing-repeat.log`.

Code delta: `evidence.c` +17/-7 = +10; `tests/iadt.c` +80/-0.
Documentation is separate. Keep this verified cleanup local, pending the next
substantial epoch. Ordinary Match/induction still needs motive instantiation
and IH checks; do not replace those with the simpler type-case signature test.

### A4 source branch scope reuse (2026-09-19)

Baseline `bc3745c`. `match_validate_branch` reconstructs kernel field/IH scopes
after source preparation already completed the corresponding producer. Use
the existing constructor/induction scope request, including retained lexical
allocations, and wait on that producer if needed. Keep branch-type construction,
conversion and the final kernel elimination check. No new scope cache or rule.

- [x] Prove the regression: asking for the completed producer's branch signature
  after source checking must create no new proofs or Contexts (Match and IH).
- [x] Replace synchronous reconstruction with the existing checked producer.
- [x] Run debug, full O2 acceptance and affected sanitizer/image checks; compare
  QuickSort work/storage, timing and LOC.
- [x] Group publication with the induction allocation epoch (`0137156`);
  A4/A5 and the reduction gates remain open.

The regression initially fails (exit 134) on the old validation path. Match and
IH cases now pass at chunks 1/64. It uses the actual elimination premises:
the structural view may choose another accepted proof of the same occurrence,
which is not the same scope request. Do not collapse those proof alternatives.
Strict debug synthesis/IADT, full strict O2 `check-acceptance`, and ASan/UBSan
synthesis/IADT/source/image checks pass. Sanitizer settings match the preceding
entry. All 2,460 optimized exports and 1,218 sanitizer image exports match the
baseline after temporary-path/step normalization. No source/test edits followed
final verification builds.

QuickSort baseline/candidate: steps 167,364/151,199; jobs 34,286/34,286;
Contexts 4,376/4,205; occurrences 78,655/76,408; maps 13,511/12,951;
lifts 3,744/3,592; actions 28,039/27,603; queries 10,433/10,204;
proofs 92,921/90,191. Graph used/reserved bytes:
63,975,584/64,274,432 -> 62,788,256/63,078,400. Substitution used/reserved:
11,709,856/11,763,712 -> 11,483,424/11,534,336. Source references stay 2,368.

Strict O2 timing, CPU 2, 31 alternating pairs, without concurrent builds/tests:
baseline/candidate median milliseconds: Bool .502/.504; add .955/.935;
length 5.724/5.367; function-field 9.752/9.257; Vec append 7.016/6.891;
QuickSort 191.752/186.644; length save 6.389/6.095;
QuickSort save 204.804/201.062. These are local measurements, not a general
speedup guarantee. Logs use `/tmp/a-program-authority-match-scope-` with
`before.log`, `{synthesis,iadt}.log`, `acceptance.log`,
`asan-{synthesis,iadt,source,image}.log`, `counts.log` and `timing.log`.

Code: `synthesis.c` +8/-9 = -1; `tests/synthesis.c` +37/-0.
Cumulative implementation/headers: R76 +3,966/-1,938 = +2,028;
R0 +8,721/-4,277 = +4,444. Documentation is separate. The broader reduction
gate remains unmet. Final kernel elimination still independently validates its
inputs; this change removes only duplicate source preparation.

### A4 induction allocation epoch (2026-09-19)

Baseline `8592050`, Main `8bd0d8c`. Finish the source-to-kernel scope reuse
boundary: source induction already checked every field/IH scope before final
elimination. Supply those exact Contexts through `pg_prove_induction_at`, with
fresh recursion/argument/Self binders only when no retained allocation was
supplied. Keep retained allocations unchanged and independently checked.
Do not attach generated allocation as an imported source input, add another
scope registry, or make the kernel trust a source acceptance flag. Standalone
kernel calls may still request fresh allocations.

- [x] Regression: source induction's accepted clause Contexts equal the scope
  producer results, at chunks 1/64; retained input and invalid variants remain
  independently checked. Demonstrate failure before implementation.
- [x] Route final source induction through the existing explicit allocation
  API; preserve ordinary Match and all motive/IH checks.
- [x] Verify full optimized acceptance, affected debug/sanitizer/image gates,
  retained/recompute behavior, baseline timing/work/storage and per-file LOC.
- [x] Publish this completed elimination-scope epoch together with `bc3745c`
  and `8592050`, only after all gates pass. This is not A4/A5/R2-R5 completion
  or fulfillment of the net-negative implementation requirement.

Verification: the new exact-clause allocation test fails on `8592050` (exit
134), then passes at chunks 1/64. Strict debug synthesis/IADT/source suites,
full strict O2 `check-acceptance`, and ASan/UBSan synthesis/IADT/source/image
checks all pass. Sanitizers use O1/g, non-PIE, frame pointers, leak detection
and halt-on-error. All 2,460 optimized and 1,218 sanitizer image export records
match Main after temporary-path/step normalization. The old retained QuickSort
image (`/tmp/a-program-authority-case-scopes-common.a`) resaves byte-identically
with zero Solve steps (CLI exit 3, `cmp` exit 0). No source/test edits followed
the final builds. No new wire format, rule, job kind or scope registry.

QuickSort versus `8592050`: steps/jobs remain 151,199/34,286. Contexts
4,205 -> 4,090; occurrences 76,408 -> 74,584; maps 12,951 -> 12,541;
lifts 3,592 -> 3,551; actions 27,603 -> 27,134; queries 10,204 -> 9,987;
proofs 90,191 -> 88,019. Graph used/reserved bytes:
62,788,256/63,078,400 -> 61,819,488/62,111,744. Substitution used/reserved:
11,483,424/11,534,336 -> 11,291,232/11,337,728. Source references remain 2,368.
Across the three-change epoch, Main's 167,364 steps and 92,998 proofs become
151,199 steps and 88,019 proofs; jobs remain 34,286.

Strict O2, CPU 2, 31 alternating pairs per comparison, no concurrent builds or
tests. Median milliseconds (previous/current; Main/current):
Bool .494/.492; .501/.496. Add .912/.926; .951/.944.
Length 5.289/5.094; 5.639/5.003. Function-field 9.337/8.778; 9.555/8.953.
Vec append 6.868/7.004; 7.251/6.664. QuickSort 189.820/185.420;
194.172/185.504. Length save 5.777/5.741; 6.240/5.743.
QuickSort save 197.946/195.294; 206.268/198.317. These local measurements
are not a universal speedup guarantee. Logs use
`/tmp/a-program-authority-induction-allocation-` with `before.log`,
`{synthesis,iadt,source}.log`, `acceptance.log`,
`asan-{synthesis,iadt,source,image}.log`, `inert.log`, `counts.log`, `timing.log`;
Main comparison: `/tmp/a-program-authority-induction-epoch-timing.log`.

This change: `synthesis.c` +31/-6 = +25; `tests/synthesis.c` +5/-0.
Epoch versus Main: `evidence.c` +17/-7 = +10; `synthesis.c` +39/-15 = +24;
`tests/iadt.c` +80/-0; `tests/synthesis.c` +42/-0. Documentation is separate.
Cumulative implementation/headers: R76 +3,997/-1,944 = +2,053;
R0 +8,751/-4,282 = +4,469. Reduced graph allocation is not reduced code size;
the net-negative gate and overall A4/A5/R2-R5 completion remain open.

Publication: `0137156d77378f24fe7c8aa429133f608ad095c5` was atomically
fast-forwarded from `8bd0d8c` to Main and `rewrite/pointer-core-hott`, without
force. Both remote tips were verified. This groups `bc3745c`, `8592050` and
the induction allocation change; it is not overall refactor completion.

### Handler construction audit (baseline `c5a584a`, 2026-09-19)

Pending structural views cannot simply wait for accepted evidence: Handler
effect equations need symbolic carriers before closure. Accepted views already
read typed occurrences. Source graph slot order and typed call execution order
also have different contracts; deleting the source walker without call-site
provenance would discard ordering/shadowing checks. Neither is removed blindly.

A concrete duplicate remains in Handler assembly: operation clauses are scanned
and resolved before effect collection, then again for final rule construction.
Use the existing clause array throughout, retaining independent rule checking.
This deletes the second cursor/scan and the reset/reinterpretation of `count`;
it adds no structure cache, job kind, proof authority or wire format.

- [x] Resolve each clause once into the existing array; collect effects from
  those producers and use them in the final handler rule.
- [x] Check inferred/explicit carriers, return-clause positions, aliases and
  duplicates, nested registration/sealing, rejected annotations, chunks 1/64.
- [x] Run full optimized acceptance and affected debug/sanitizer/source/image
  checks; measure lookup work, allocation and LOC against the baseline.
- [x] Published in the tested selected-input/scanning epoch `21cf3ff`.
  Overall A4/A5/R2-R5 and net-negative gates remain open.

The clause array retains the immutable operation producer, not its name lookup.
The clause body still checks that lookup, including a rejected annotation or
scope. An initial trial retained the lookup instead: if it failed after scanning,
final signature extraction reported ERROR instead of propagating REJECTED. The
existing `pending_effect_contexts` negative test caught this. Reading the retained
declaration and keeping the body's ordinary reference obligation removes that
timing-dependent rediscovery without bypassing acceptance. No test was weakened.

Verification: strict debug synthesis and source suites, full strict O2
`check-acceptance`, and ASan/UBSan synthesis, source, image, handler-nesting and
handler-boundaries all exit 0. Sanitizers use O1/g, non-PIE, frame pointers,
leak detection and halt-on-error. All 2,460 optimized and 1,218 sanitizer image
export records agree with baseline after path/step normalization. The boundary
suite checks 4,080 handler snapshots; new tests combine multiple clauses with
nested handling and re-emission in two clause orders, at chunks 1/64.

On `effect-application.p`, GDB counts before/after: clause producer requests
20/10, operation-origin traversals 60/40; accepted proofs, Contexts, occurrences,
maps, actions and queries are unchanged. Requests increase 6,653 -> 6,661
(six classifier-structure and two type-structure requests), steps
16,957 -> 17,019, graph used bytes 5,107,264 -> 5,112,704; reserved bytes
and substitution storage are unchanged. Earlier clause preparation changes
which pending structural views are needed. This is not a global work reduction.
CPU 2, strict O2, 31 alternating pairs: median milliseconds baseline/current
are Handler 6.078/5.948, QuickSort 185.123/188.262, Handler save 6.210/6.212.
These small mixed changes do not establish a general speedup.

Logs: `/tmp/a-program-authority-handler-clauses-` followed by `build.log`,
`synthesis.log`, `source.log`, `acceptance.log`, `asan-build.log`,
`asan-{synthesis,source,image,nesting,boundaries}.log`,
`{induction-allocation,handler-clauses}-{counts,roles}.log`, and `timing.log`.
Per-file LOC: `synthesis.c` +13/-31 = -18; `tests/synthesis.c` +2/-0;
documentation is separate. Cumulative implementation/headers are R76
+4,009/-1,974 = +2,035 and R0 +8,763/-4,312 = +4,451. The net-negative
requirement is still unmet. Publication is recorded below; the pending-structure
reconstruction audit is not complete.

### Substitution scratch-storage audit (baseline `d19f2cd`, 2026-09-19)

The current universal QuickSort workload makes 3,779 composition calls for
2,845 exact proof pairs, visiting 18,673 images, of which 1,595 are repeat visits.
A persistent pair cache would retain thousands of new requests to avoid only
8.5% of these image visits; no such cache or new query kind is justified here.
Existing occurrence actions already share their computed images. Explicit
source/destination proof choices must not be collapsed by a structural map key.

The concrete storage mismatch is transient: `substitution_build` creates 17,053
temporary arenas with 2,670,080 used / 279,396,352 reserved bytes cumulatively;
composition creates 3,779 with 220,896 / 61,915,136 bytes. Each arena reserves
at least 16KB only to own short-lived pointer arrays. These are allocation
traffic totals, not peak resident memory or persistent graph growth.

- [x] Replace those two temporary arenas with sized arrays, using the existing
  projection/map-building ownership pattern. Keep all ordinary rule checks,
  exact premise choices, structural action sharing and failure cleanup.
- [x] Check empty/repeated/dependent compositions and existing negative cases;
  run full optimized acceptance and affected debug/sanitizer/image suites.
- [x] Measure actual scratch allocation, unchanged semantic work/counts,
  paired timings and per-file LOC.
- [x] Published with the Handler cleanup in epoch `21cf3ff` after its gates
  passed. Neither change finishes A4/A5/R2-R5.

Verification: strict debug Core, full strict O2 `check-acceptance`, ASan/UBSan
Core, synthesis, source, image and handler-boundaries all exit 0. Sanitizers use
O1/g, non-PIE, frame pointers, leak detection and halt-on-error. All 2,460 O2 and
1,218 sanitizer image export records match `d19f2cd`, including Solve steps.
The new Core assertions cover empty composition and repeated dependent inputs.

Scratch payload traffic falls from 341,311,488 reserved arena bytes to
2,459,776 requested array bytes (allocator headers excluded); allocation calls
increase from 20,832 arena blocks to 32,351 arrays. Both allocated arrays are
freed on success, an existing-record hit, or rejection. No new persistent work
or acceptance index is added. QuickSort remains at 151,199 steps / 34,286 jobs /
88,019 proofs, with identical Context/map/occurrence/action/query counts and
identical graph/substitution arena totals.

Strict O2, CPU 2, 31 alternating pairs, median milliseconds baseline/current:
Bool .516/.509; add .937/.911; length 5.181/4.770; function-field 9.054/8.062;
Vec append 6.800/6.446; QuickSort 184.134/181.638; Handler 6.195/5.982;
length save 5.273/5.123; QuickSort save 184.073/180.817. These are local timing
observations, not a proportional runtime or peak-memory claim.

Logs use `/tmp/a-program-authority-map-scratch-` with `build.log`, `core.log`,
`acceptance.log`, `asan-build.log`, `asan-{core,synthesis,source,image,boundaries}.log`,
`allocations.log`, `{handler-clauses,map-scratch}-counts.log` and `timing.log`.
Before-change probes: `/tmp/a-program-authority-compose-current.log` and
`/tmp/a-program-authority-map-temporary-before.log`. Per-file LOC:
`evidence.c` +8/-7 = +1; `tests/core.c` +3/-0; documentation separate.
Overall implementation reduction and the remaining structural-consumer audit
are still unproven. This is verified local work, not a completed Main epoch.

### Recursive-field scan audit (baseline `abb3f6d`, 2026-09-19)

`iadt.c:direct_recursion` checked every application argument for Self
independence before inspecting the head. For a non-Self head, it then checked
the entire type again. Inspect the head first: a non-Self application needs
only the whole-type check; a Self application still checks every index.
This changes traversal order, not the accepted recursive-field grammar.
No normalization, cache, rule tag or persistent ownership is added.

- [x] Remove the redundant argument scans for non-Self heads.
- [x] Extend `positive_fields` for multi-index recursion, nested independent
  applications, Self-dependent arguments, shadowing and unreduced redexes.
- [x] Pass strict debug IADT and full strict O2 `check-acceptance`; all 2,460
  export records match the baseline, including Solve steps.
- [x] Pass ASan/UBSan IADT, synthesis and source suites (O1/g, non-PIE,
  leak detection and halt-on-error).
- [x] Finish sanitizer image validation and paired performance measurements.
- [x] Published as part of the verified epoch `21cf3ff`.

On the unchanged universal QuickSort input, independence calls from this
helper fall 994 -> 513, and their comparison transitions fall 36,013 -> 20,608.
Solve stays at 151,199 steps / 34,286 jobs / 88,019 proofs. Context, occurrence,
map, lift, action and typed-query counts and persistent arena sizes are unchanged.
The temporary comparison work is distinct from those persistent counts.

The same audit found 495 Pi-application independence calls (352 exact pairs).
This does not establish that a new per-query scheduler state or cache would
improve those paths. Pending structure remains necessary for effect equations;
this change does not finish target 1's construction-sharing audit.

Local logs: `/tmp/a-program-authority-recursive-spine-` with `build.log`,
`iadt.log`, `acceptance.log`, `asan-build.log`, `asan-{iadt,synthesis,source,image}.log`,
`counts.log`, `{before,after}-work.log` and `objects.log`. The original call-site
profile is `/tmp/a-program-authority-independence-audit.log`.
The sanitizer image run exits 0; all 1,218 export records match the baseline,
including steps. With no other build/test running, strict O2 on CPU 2 and 31
alternating pairs gives median milliseconds baseline/current: Bool .482/.480;
add .916/.919; length 4.573/4.476; function-field 9.113/7.840; Vec append
6.658/6.555; QuickSort 193.308/191.373; Handler 6.076/6.161; length save
4.933/5.090; QuickSort save 189.055/186.131. Samples are in the same prefix's
`timing.log`. These mixed local timings do not establish a general speedup.
LOC: `iadt.c` +5/-2 = +3; `tests/iadt.c` +9/-0. Cumulative implementation/headers
are R76 +4,021/-1,982 = +2,039 and R0 +8,771/-4,316 = +4,455. Overall code
reduction and A4/A5/R2-R5 remain open; do not report this as a smaller codebase.

### Continuation checking and type-query resumption (baseline `748f5ea`)

Date: 2026-09-19. The pending-construction audit now lists the pure builders
already shared by synthesis and acceptance. Retain provisional effect-row
structure and independent rule checking; do not add a third typed AST to hide
the remaining dispatch. This clarification does not complete that audit.

- [x] Request/Fold use the existing checked constant-codomain operation instead
  of first repeating its independence check directly. Domain matching, closed
  effects, nondependence and totality remain checked. Add a dependent Request
  rejection beside the existing dependent Fold case.
- [x] A type-structure query selects its input once and resumes that dependency;
  accepted typed-subject lookup still takes priority. Share the forwarding
  tail, preserving allocation-error handling and type-versus-term eligibility.
- [x] Strict debug Core/synthesis, full strict O2 acceptance, ASan/UBSan
  Core/synthesis/source pass. All 2,460 O2 export records match the baseline,
  including steps. Existing pending-effect snapshot/chunk tests pass.
- [x] Final sanitizer image validation completed without reported sanitizer
  errors; all 1,218 export records match the baseline, including steps.
  Finish paired timing as recorded below.
- [x] Published with epoch `21cf3ff`, not as standalone A4 completion.

QuickSort: constant-codomain independence calls 575 -> 432 (392 unique pairs
in both; repeats 183 -> 40); type-query producer reinspection after input
selection 132 -> 0. Solve remains 151,199 steps, with identical job, Context,
map, occurrence, action, query and proof counts and persistent arena sizes.
Other structure consumers still have repeat inspections; do not extrapolate
this narrow resumption change to all producers or claim an overall complexity
bound. No new work tag, cache, accepted-result table or wire format is added.

Logs use `/tmp/a-program-authority-continuation-proof-`: `core.log`,
`resume-synthesis.log`, `counts.log`, `resume-counts.log`, `final-objects.log`,
`final-acceptance.log`, `asan-core.log`, `final-asan-{synthesis,source,image}.log`.
Before-change reinspection profile: `/tmp/a-program-authority-structure-repeat-audit.log`.
CPU 2, O2, 31 alternating pairs: median milliseconds baseline/current are Bool
.489/.487; add .908/.900; length 4.743/4.780; function-field 7.939/8.571;
Vec append 6.489/6.500; QuickSort 178.060/177.075; Handler 5.823/5.723;
length save 4.933/4.926; QuickSort save 181.110/181.154. Raw samples:
`/tmp/a-program-authority-continuation-proof-timing.jsonl`. These mixed local
measurements do not establish a general speedup; the function-field increase
remains visible rather than being hidden by the QuickSort result.
Per-file implementation: `evidence.c` +0/-3, `synthesis.c` +14/-15, net -4.
Tests: `tests/core.c` +2/-0; documentation is separate. Cumulative R76 is
+4,031/-1,996 = +2,035; R0 is +8,779/-4,328 = +4,451. Overall reduction and
remaining A4/A5/R2-R5 gates are still unmet.

### Selected structural inputs (baseline `b390a22`, 2026-09-19)

- [x] Resume classifier and declared-type requests through their selected
  dependency. A term request forwards a selected source adapter directly;
  derivation producers need no source preparation. No additional state/cache.
- [x] Preserve accepted-subject priority, classifier WHNF, symbolic effect
  snapshots, post-check boundaries, and independent rule validation. Add a
  pending normalized-application classifier regression before effect closure.
- [x] Debug synthesis and full strict O2 acceptance pass. All 2,460 export
  records match `b390a22`, including steps. ASan/UBSan synthesis/source pass.
- [x] Full debug acceptance passes and matches all O2 export records. Sanitizer
  image checks match all 1,218 baseline records, including steps; all 4,080
  Handler save/resume boundary snapshots pass. Paired timings are below.
- [x] Publish the selected-input/scanning epoch with `d19f2cd`, `abb3f6d`,
  `748f5ea`, and `b390a22` after verification; do not close A4/A5 or R2-R5.

QuickSort source reinspection after selecting a dependency: classifier
4,354 -> 0, term 469 -> 0, declared type 56 -> 0. Initial term source inspection
also falls 442 -> 247 because derivation rules are already available. Other
initial counts are unchanged. Solve remains 151,199 steps / 34,286 requests;
Context, occurrence, map, lift, action, query, proof and arena counts match the
baseline exactly. This does not eliminate initial structural interpretation or
prove the complete pending-construction audit finished.

Logs: `/tmp/a-program-authority-classifier-resume-` with `synthesis.log`,
`acceptance.log`, `debug-acceptance.log`, `asan-{synthesis,source,image}.log`,
`final-counts.log`, and `objects.log`. This increment: `synthesis.c` +33/-30,
`tests/synthesis.c` +5/-0. Epoch versus Main `c5a584a`: implementation -15 net
(`evidence.c` +8/-10, `iadt.c` +5/-2, `synthesis.c` +60/-76); tests +21/-0.
Documentation is separate. The original cumulative net-negative gate remains
unmet; this local reduction is not overall refactor completion.

O2, CPU 2, 31 alternating pairs, no concurrent test/build: medians in
milliseconds `b390a22`/current are Bool .483/.489; add .884/.880; length
4.594/4.716; function-field 8.181/8.602; Vec append 6.607/6.623; QuickSort
181.009/182.468; Handler 6.007/6.021; length save 4.995/5.129; QuickSort save
181.177/180.412. Samples: `/tmp/a-program-authority-classifier-resume-timing.jsonl`.
Reduced rediscovery does not imply a measured general speedup; small-input
increases remain part of the performance record.

The same paired protocol comparing Main `c5a584a` (the `0137156` implementation)
with this epoch gives: Bool .493/.487; add .904/.887; length 5.068/4.724;
function-field 9.052/7.914; Vec append 6.904/6.780; QuickSort 194.920/193.716;
Handler 6.194/5.897; length save 5.360/5.092; QuickSort save 198.742/195.506.
Samples: `/tmp/a-program-authority-selected-input-epoch-timing.jsonl`.
All 2,460 optimized export results match Main after temporary paths and step
counts are normalized; the Handler scheduling difference described above is
not hidden by claiming identical epoch-wide steps. Full debug and optimized
acceptance, focused sanitizer validation and the per-change audits together
verify this selected-input/scanning epoch, not completion of the whole plan.

Publication: `21cf3ff6b7d9b1f97a96f01c423247a5316735e9` was atomically
fast-forwarded from `c5a584a` to Main and `rewrite/pointer-core-hott`, without
force; both remote tips were verified. This groups five implementation commits.
Remaining A4/A5/R2-R5 and original cumulative reduction gates remain open.

### Borrowed request keys (baseline `d76537b`, 2026-09-19)

- [x] Remove temporary key arrays/arenas from rule, substitution and family-
  action requests. The existing interner reads a fixed prefix and a borrowed
  job array, then allocates flat immutable storage only for a new request.
  Fixed-input callers use the same interner; no second key table or work tag.
- [x] Preserve rule-header value keys, ordered dependency identity, owner
  checks and copied persistent inputs. Add copied-header sharing and reordered
  substitution rejection assertions. Debug and ASan/UBSan synthesis pass.
- [x] Full optimized acceptance and sanitizer source/image checks pass. All
  2,460 optimized and 1,218 sanitizer image export records match baseline,
  including Solve steps. Paired timing is recorded below.
- [x] Published with the verified storage epoch `ec31e3a`.

QuickSort previously allocated 7,492 temporary arenas here (rule 7,184,
substitution 287, family action 21) for 275,536 requested payload bytes. Those
three allocation/copy/destruction paths are deleted, rather than replaced by
another scratch allocator. Persistent request count 34,286, Solve steps 151,199,
and all Context/occurrence/map/lift/action/query/proof and arena measurements
are unchanged. No peak-memory or general speedup claim follows from scratch
allocation counts. Evidence-to-job conversion and effect-equation input arrays
have different element types/transformations and are not blindly cast into this
borrowed dependency API.

Logs: `/tmp/a-program-authority-request-spans-` with `before.log`, `objects.log`,
`synthesis.log`, `acceptance.log`, `asan-{synthesis,source,image}.log`.
Implementation: `synthesis.c` +33/-41 = -8; tests +5/-1 = +4; docs separate.
This removes concrete request-key reconstruction, not pending type formation
or independent proof checking. A4/A5/R2-R5 and cumulative reduction remain open.
The cumulative R0 implementation/header delta is +8,830/-4,384 = +4,446;
the eight-line local reduction does not satisfy the original net-negative gate.

O2, CPU 2, 31 alternating pairs without concurrent tests/builds, median
milliseconds baseline/current: Bool .485/.471; add .895/.868; length
4.640/4.762; function-field 8.078/8.224; Vec append 6.620/6.712; QuickSort
183.623/184.500; Handler 5.775/5.642; length save 5.092/5.023; QuickSort save
181.908/180.767. Samples: `/tmp/a-program-authority-request-spans-timing.jsonl`.
The local allocation-path deletion does not establish a general speedup;
small increases remain visible in this measurement.

### A4 constructor inputs from their checked map (2026-09-19)

Baseline `ad3d94c`. `pg_data_instance` already checks the constructor's dependent
telescope and retains its ordered field subjects in the resulting Context map.
`pg_prove_constructor` nevertheless copied the fields from their proofs into a
second temporary array. Borrow that map's field suffix when interning the
constructor occurrence; keep the exact instance proof as a premise. Nullary
constructors use the empty suffix. This adds no cache, tag, checking bypass or
wire-format change. Other temporary operand arrays do not necessarily have an
existing contiguous owner and are not removed by analogy alone.

- [x] Remove the duplicate projection, allocation and cleanup path.
- [x] Check dependent field order and exact agreement with the checked map.
- [x] Check that different proofs of one field give distinct constructor
  derivations while sharing the same typed subject.
- [x] Strict debug IADT tests and full strict O2 `check-acceptance` pass.
  All 2,460 export records, including Solve steps, match the baseline run.
- [x] ASan/UBSan IADT and Source IO scripts pass with leak/error halting enabled.
- [x] Sanitizer image checks pass; all 1,218 export/step records match the
  baseline. No source/test edits occurred during verification.
- [x] Isolated O2 timing: CPU 2, warmup and 31 alternating pairs.
- [x] Published with the verified storage epoch `ec31e3a`.

GDB on the baseline counts 94 operand-array allocations for function-field
(145 fields, 12 nullary cases) and 384 for universal QuickSort (1,159 fields,
75 nullary cases). Every one used a separate temporary arena, including the
zero-length arrays; this path is deleted. These are allocation-traffic counts,
not peak RSS or elapsed-time gains. Logs use
`/tmp/a-program-authority-constructor-{before,qsort-before}.log`; verification
logs use `/tmp/a-program-authority-constructor-fields-*`.

The `objects.log` QuickSort snapshot matches the baseline: 34,286 requests,
151,199 steps, 74,584 occurrences and 88,019 proofs. Graph/substitution arena
used bytes remain 61,819,488/11,291,232; no persistent structure was removed.

Implementation: `evidence.c` +7/-14 = -7; `tests/iadt.c` +15/-0. The cumulative
R0 implementation/header delta remains +8,835/-4,396 = +4,439. This does not
complete A4/A5/R2-R5 or satisfy the original net-negative gate.

Median milliseconds baseline/current: Bool .481/.492; add .853/.856;
length 4.835/4.632; function-field 7.963/7.922; Vec append 6.347/6.743;
QuickSort 178.352/177.907; Handler 5.465/5.735; length save 5.025/5.077;
QuickSort save 179.999/180.659. Raw samples are in
`/tmp/a-program-authority-constructor-fields-timing.jsonl`. The mixed results
do not establish a general speedup or discharge the R0 performance audit.

### A4 role-owned Solver storage (2026-09-19)

Baseline `1ef27d8`. Use the existing immutable request role to select private
work storage, rather than reserving independent pointers for every worker in
every request. This changes physical ownership, not request identity or the
accepted Term/occurrence/derivation layers. No new role, cache or wire field.

- [x] Audit all accesses before moving storage: definition registration, schema
  checking and handlers own disjoint state. Expression work retains separate
  block/application/Match/function-source slots within its role's storage.
- [x] Preserve simultaneous derivation premises and normalization state;
  overlaying those two would corrupt reduction checking.
- [x] Dispatch resource destruction by the existing role. Restrict handler
  failure propagation and definition lookup before reading role-owned state.
- [x] Add cancellation at every Solve boundary for accepted and rejected source
  programs, and reject definition lookup on ordinary expression producers.
- [x] Strict debug and optimized acceptance; focused sanitizer tests including
  pending handler/source/image destruction. Compare semantic outputs and steps.
- [x] Measure request size, retained memory, counts and isolated performance.
- [x] Publish with the two allocation deletions after the integrated epoch
  passes its gates (`ec31e3a`); parent A4/A5/R2-R5 remain open.

Verified on the unchanged source/test revision: full strict O0/g and O2
`check-acceptance`; ASan/UBSan synthesis, Source IO, image CLI and all 4,080
handler save boundaries, with leak/error halting. The new cancellation cases
exercise 344 accepted-program and 254 rejected-program boundaries. All 2,460
normalized export/step records match published Main and each other; all 1,218
sanitizer image records match the preceding build. No test expectation relaxed.

GDB reports request headers 368 -> 336 bytes. QuickSort keeps 34,286 requests,
151,199 steps, 74,584 occurrences and 88,019 proofs. Graph arena used bytes
fall 61,819,488 -> 60,722,336, exactly 32 bytes per request; allocated capacity
falls 62,111,744 -> 60,997,632. Substitution storage is unchanged. This is a
retained-memory reduction, not elimination of semantic obligations or evidence.

Epoch timing against published Main's strict O2 build: CPU 2, warmup and 31
alternating pairs, no concurrent tests/builds. Median milliseconds before/after:
Bool .513/.474; add .901/.863; length 4.901/4.811; function-field 8.470/8.046;
Vec append 6.630/6.418; QuickSort 193.666/190.172; Handler 5.995/5.767;
length save 5.011/5.100; QuickSort save 182.448/179.375. The save regression
is retained here; these local timings do not discharge the R0 comparison gate.
Logs: `/tmp/a-program-authority-role-storage-` with `debug.log`,
`acceptance.log`, `asan-{synthesis,source,image,handler}.log`, `objects.log`
and `timing.jsonl`.

Local implementation: `synthesis.c` +32/-16 = +16; tests +35/-0.
Combined with `ad3d94c` and `1ef27d8`, epoch versus Main `d76537b`:
`synthesis.c` +65/-57 = +8; `evidence.c` +7/-14 = -7. Implementation net
**+1**, not a LOC reduction. Tests separately: `synthesis.c` +40/-1 = +39;
`iadt.c` +15/-0. Cumulative R0 implementation/header +8,864/-4,409 = +4,455.
The original net-negative gate and overall A4/A5/R2-R5 remain unfinished.

Published on 2026-09-19: `ec31e3a22a00cb9b0344258f51e011ab216dac0a`, including
`ad3d94c` and `1ef27d8`, atomically fast-forwarded Main and
`rewrite/pointer-core-hott` from `d76537b`, without force. `git ls-remote`
confirmed both tips. This following publication-record commit changes only
documentation; the tested implementation remains unchanged.
