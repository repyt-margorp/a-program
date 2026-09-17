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
- [ ] G2a: Conventional LE transitivity, universal proof and invalid variants.
- [ ] G2b: Comparator theorem related to conventional LE, not only Compared.
- [ ] G3a: Universal insertion-sort Sorted proof tied to actual results.
- [ ] G3b: Universal tree-sort Sorted proof tied to actual results.
- [ ] G3c: Universal reported merge-sort Sorted proof tied to actual results.
- [ ] G3d: Universal QuickSort Sorted proof tied to actual results.
- [ ] J1: Combined regression and sanitizer gates; no ignored failures.
- [x] J2: README and source examples updated to verified syntax, with limits.
- [ ] J3: Issue requirement table, explanations, implementation references and
  justified issue/PR disposition. Pending items are not relabelled complete.
- [ ] G-PUSH: Verify and publish the issue #29 / PR #30 improvement milestone
  to Main; record remaining requirements separately from issue closure.
- [ ] R: Resume the parent duplicate-synthesis/authority refactor after J1-J3.

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
