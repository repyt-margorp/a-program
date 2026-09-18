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
