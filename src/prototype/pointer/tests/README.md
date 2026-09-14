# Pointer prototype verification

Run `make -f src/prototype/pointer/Makefile check` from the repository root.
The prototype does not invoke the old compiler or link its solver/runtime.

`core.c` currently checks pointer-key interning, explicit alpha comparison,
closure evaluation/readback, bounded resume, dimension-map laws, and lazy cube
boundary binding. Equality in these tests is explicit: structural pointer
equality is not WHNF conversion. A reduced result never replaces the original
node's entry in the interning table.
Substitution pairing reuses checked prefix images: extending a map containing
a dependent Pi must not freshen its prefix binders again. The paired and direct
full-map APIs intern the same proof; flat array copying is not eliminated.
The pure CBPV evaluator contracts structural thunk/force eta and fold right
units, including right units under suspension. Tests retain typing for both
U(F A) and U(Pi A B), check closure capture, selected-path action and split
budgets, and reject constant-return/divergent continuations as unit candidates.
No thunk body or continuation is executed just to recognize these equations.
Strong normalization is a separate pure request over the same WHNF store,
not a different reduction policy or a change to runtime evaluation. Tests
check child-result sharing, contraction after rebuilding a parent, zero-fuel
requests, split/bulk budgets, result reuse, 10,000 nested binders and DAGs.
A retained divergent thunk has no NF result; a discarded divergent argument
does not block head-first normalization. Conversion uses shared NF work only
after its WHNF comparison fails, and issues no certificate while pending.
Typed NF uses the same directed reduction receipt and `PG_PURE_NORMALIZATION`
rule as typed WHNF. Tests distinguish head results from normalization inside
THUNK/Lambda, preserve separate annotations and contexts over shared Core,
reject foreign policies/owners and mismatched sources, and reuse completed
work across solver instances. Receipts survive work-store destruction; they
are not serialized proofs or permission to execute runtime effects.

`identity.c` checks symbolic Identity formation, diagonal reflexivity, selected
universe-family instantiation, iterated diagonal witnesses, CBPV polarity and
reindexing. It rejects using an ordinary function as a universe identification.
The same suite checks the implemented RETURN/THUNK and canonical F/U action
equations, policy-keyed WHNF caching and fixed pure conversion. It also checks
the implemented dependent Pi action and diagonal transport/lifting, not general
higher coherence or observational adequacy.
Canonical U/F field tests compute transport/lifting of a returned thunk along
two chosen universe paths, in both directions. Independently derived target
terms/classifiers agree; lexical capture, split fuel and beta-only isolation
are checked. General U/F transport also agrees with an independently typed
THUNK/FORCE/FOLD map of a neutral thunk, including substitution after mapping.
A divergent quoted body remains suspended; a separate runtime probe runs its
source exactly once per force, never during transport or through a pure cache.
The former neutral `map refl` conversion failure now has an accepted typed
regression in both directions. General dependent lifting and higher U/F
coherence remain open; these tests do not establish their completeness.
`pi_transport_candidate` derives a contravariant-domain/covariant-result
map, including an index-dependent result and its selected lifting path. It
checks destination typing, not an admitted Pi transport equation. At a
diagonal path the candidate reduces to function eta expansion while strict
transport returns its input; the test preserves the current DefEq distinction.
The plan records the required equational decision before enabling that rewrite.

`iadt.c` tests erased constructor/Match references, pointer-labelled clause
selection, saturation, capture, lazy fields/branches and policy isolation.
It also derives layouts from checked constructor-result substitutions between
field and index contexts, preserving the fixed parameter prefix. Field/index
instances use ordinary dependent substitution and composition, including
Identity-valued indices and their acted boundaries. Alternative context derivations are retained;
equal contexts need not use identical proof pointers.
Typed action on result-map images builds a checked substitution between
expanded field/index contexts after explicit classifier conversion. Both
endpoint projections commute in the tested dependent example; this is not a
general naturality or indexed-fibrancy theorem.
Branch abstraction uses ordinary Pi/Lambda proofs from synthesized computation
bodies. Its applications agree with typed body substitution and erased iota;
raw function results stay Pi computations. Selected-path action is compared by
conversion, since WHNF does not normalize a RETURN payload.
Index-motive cases are post-checked using ordinary reindex and conversion
evidence. Tests retain the independent body proof, reject wrong/missing
certificates, and cover raw Pi motives and zero-field cases without guiding
body synthesis from an expected type.
One-direction erased Match action retains selected constructor and branch
paths, including compressed diagonal prefixes. Tests compare it with checked
dependent body action, preserve captured scope/trailing applications, and check
neutral boundaries, unselected divergence, split budgets and policy isolation.
These layouts are not accepted declarations: no positivity, index refinement,
IH, source ADT synthesis or higher constructor witness is certified by this test.

`synthesis.c` also checks typed RETURN/THUNK content extraction through reflexive
action, including a beta-redex source, repeated action, conversion and context
substitution. The source and action requests share the ordinary job store;
extracting acted thunk code does not execute the suspended source.
Case jobs wait for independently parsed/synthesized bodies before subscribing
to shared typed-reindex jobs and checking the motive. Tests cover partial
substitution, producer/consumer order, unchanged producer evidence on rejection,
completed-job reuse, unsupported/cyclic work and suspended-job destruction.
Scheduled substitution pairing also assembles the acted dependent-index map:
each value is checked after reindexing its field type, retains conversion
evidence, and agrees under split/bulk scheduling and endpoint projection.
Result-image actions use accepted evidence as completed producers in the same
job store. Registration performs no evaluation or proof reconstruction, rejects
foreign evidence, and keeps distinct typings of a shared Core Lambda separate.
Image actions agree with the direct API; primitive action remains synchronous.
Checked name publication (`pg_synthesis_name`) supplies ordinary source names
without another binder, automatic coercion, evaluation or proof reconstruction.
Tests publish checked Eq/refl functions, synthesize dependent annotations and
second-order reflexivity, and normalize a source proof to the independently
constructed function. Different endpoints reject at post-check. Names can be
aliased/shadowed without changing old scopes; shared erased functions keep
their different classifiers. These are supplied test library functions, not
new reserved words, a built-in prelude or a completed import/CLI interface.
Transport/lifting fields are also wrapped as ordinary checked functions and
called from source in both directions. Tests compare the resulting terms and
classifiers against the primitive rules, keep different selected paths apart,
reduce diagonal transport with NF and reject reversed endpoint arguments.
Shared context-suffix abstraction uses only Pi/Lambda derivations; full versus
staged abstraction agrees, repeated inputs reuse evidence, and invalid scopes
or value bodies reject. ADT branch abstraction uses this same implementation.
The solver's ready queue is FIFO for new work, requeued transitions and woken
consumers. A long typed reduction cannot monopolize the scheduler: independent
small requests made before and during it finish while it remains pending.
Tests cover consumer wakeup, exact duplicate requests, drain/restart, cold-store
bulk versus split results, and existing cyclic dependencies. Dependency tests
wait for a particular subscription with a finite bound instead of assuming
that the most recently inserted job runs next. Primitive rules are still
synchronous, so this is transition fairness, not a wall-clock response bound.
Application demands classifier WHNF through the shared typed job before
inspecting Pi/U/F, including after FORCE and sequencing a returned callee.
Computed arguments expose their classifier through the same path. Regression
tests apply the action of identity, directly and under U/F wrappers, to checked
boundary inputs; an acted RETURN supplies a computed path argument. Result NF
is the selected witness. No callee evaluation occurs during synthesis; cached
classifier exposure is reused. The existing left-before-right FOLD test keeps
its order and now records the forced callee's projection into the argument scope.

`compatibility.tsv` is generated by `sh src/prototype/pointer/tests/inventory.sh`.
It inventories archive commit `5bdecb4`, including all tracked `.p` programs,
driver option strings, AST enum names and integration/internal checks. It does
not scan build output or infer expected behavior from a filename. In particular,
`review_pending` means neither passing nor supported by the replacement.

The initial inventory has 158 programs, 57 integration scripts, 50 internal
check source files, 33 option/source pairs and 48 AST enum names. Integration
scripts also generate inline fixtures; their cases still need individual review.
AST enum names include supporting roles, not only distinct surface forms. REPL
commands and grammar productions also need explicit review. Consequently this
inventory does not close N0.

Review each semantic case against its source and assertions. Preserve desired
values, classifiers, traces and rejection conditions; rewrite assertions about
obsolete DB layouts or pretty-print tags. New implementation results should be
stored separately from this reproducible source inventory.

The source compatibility gate includes an imported ordinary Sigma declaration
with a Bool-indexed Nat/Bool field family. It compares both field projections
after unsolved/completed image saves and one/64-step resumption. Wrong field
types and a structurally identical but distinct nominal Nat must reject before
and after saving. This is not a primitive Sigma rule. Provider-dependent
fixtures are run with their providers, not scored by standalone exit status.

One concrete parser boundary to change: the archive reader's
`build_certified_function_companion_type` checks the spelling `Bool` and builds
a `$certified.binary-bool` classifier. Its accepted surface notation belongs in
the compatibility contract; constructing and validating that classifier belongs
to typed elaboration, not the new parser. Do not reproduce that semantic branch
as a parsing rule merely because it is present in the archived implementation.
# Syntax Inventory Runner

`make -f src/prototype/pointer/Makefile parse-files` builds `.build/parse_files`.
It reads paths supplied as arguments and emits TSV rows: `parsed`,
`syntax_error`, or `io_error`. It performs no lowering, type checking or
execution. Its exit status is nonzero if any input could not be parsed.

The `program` rows in `compatibility.tsv` select the inventory. On 2026-09-07,
after adding index markers, bare annotated lambdas, imports and companion
syntax, 154 of 158 current-worktree files parsed; four did not. The remaining
files are `12_append_assoc_draft.p` and `stage1.p` through `stage3.p`.
Negative fixtures can legitimately
parse before failing semantic checks. These counts are not conformance results.
`make check` now also parses the existing examples numbered 01 through 09.

`syntax_inventory.sh` runs all 158 inputs in one process and checks their
syntax outcomes, including the exact four errors in `syntax_exclusions.tsv`.
The stage files contain unannotated lambda syntax (stage1 also lacks a named
top-level entry); the append draft contains `==`, which the existing reader
does not tokenize. They remain documented historical syntax rather than being
silently enabled. This gate does not validate AST meaning or negative typing
fixtures; those still require lowering/checking tests.
