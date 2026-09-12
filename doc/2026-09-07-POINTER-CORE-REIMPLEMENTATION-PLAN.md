# Pointer Core Reimplementation Plan

Date: 2026-09-07
Status: in progress on `rewrite/pointer-core-hott`; N0/N1 incomplete
Predecessor: `2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md` (stopped)
Failure record: `2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md`
Archived source commit: `5bdecb4` on `archive/2026-09-07-failed-single-path-refactor`
AI implementation location: `src/prototype/pointer/`

Revision: HOTT and dimensional action are foundational from N0/N1, following
the user's 2026-09-07 correction. They are not a feature to bolt on at N6.
Further correction: Core interning uses exact pointer tuples only. Alpha
comparison and normalization are explicit operations, never construction-time
criteria for merging different Lambda or semantic-object references.

## September 13 Priority Correction: Source Compatibility First

This user-directed ordering supersedes earlier checkpoint-first next steps.
The next milestone is recompiling previously accepted source programs and
proving properties of already-defined `length` and `quickSort`. It is not
eliminating recomputation during image loading. The full rewrite remains open.

- [ ] Establish the compatibility inventory from legacy integration tests and
  fixtures, with a verified working legacy revision as reference. Do not assume
  every fixture passed merely because it exists; preserve negative cases too.
- [ ] Restore general source synthesis and indexed elimination, including open
  families, dependent motives, recursive fields and Acc-based recursion. Keep
  `::` post-synthesis; do not add length/QuickSort-specific kernel rules.
- [ ] Compile the existing function-graph length fixtures and fuel-free
  QuickSort fixture, and check their runtime results. Then support an explicit
  property proof using the graph/witness of an already-defined function, not
  a separately rewritten certified implementation. Witness production alone
  does not establish a property such as preservation of length or sortedness.
- [ ] Verify property derivations and rejection of incompatible claims through
  ordinary Solve. Use the legacy function-graph and IF8 tests as starting points,
  not as substitutes for checking the actual property proof.

Source and restored inputs must continue to use the same Solve and kernel
rules; no independent Replay engine is permitted. Recomputing through that
same path is acceptable for this milestone. Defer retained-result reuse,
allocation-provenance optimization and zero-recomputation checkpoint gates
unless they block correctness or practical execution of compatibility tests.
Keep existing passing image tests. Do not trust imported completion flags.
Record progress against these items instead of expanding the checkpoint audit
for each local optimization. Main promotion still requires full acceptance.

Compatibility baseline established against a fresh build of Main `63b00eb`,
not the stale worktree `read_file.out` (which fails the length fixture).
`make -f src/prototype/pointer/Makefile check-source-compatibility` now records
18 required source/result cases using unchanged legacy fixtures: Vec, Acc,
order proofs, six function-graph examples, named cases, six QuickSort inputs,
and two incompatible-proof rejections. Main passes these checks; pointer Core
currently passes 3/18 (Vec, generated length, graph block binding); the other
cases stop at UNSUPPORTED. This gate is included in
`check-acceptance`; it is not an expected-failure test. The export comparison
checks typed values in the same Program at chunk sizes 1/64, not printed DAGs.

September 13 implementation: scoped family assumptions, saturated family
application and parameter abstraction now use ordinary pointer-Core terms
with retained typing rules. A family signature is a Pi-shaped type-layer
signature ending in Universe, not a value-side Pi or a computation with an
assumed pure result. TYPE_FAMILY evidence cannot be RETURNed, FORCEd, or used
as an ordinary value substitution image. Only supplying all checked indices
produces a fiber's VALUE_TYPE evidence. Ordinary CBPV application is unchanged.

- [x] Admit source indexed declarations by discharging the checked Self-family
  assumption after positivity/universe checks; form recursive constructors at
  the indices computed by their retained result substitutions.
- [x] Check dependent family signatures, partial/saturated application and
  substitution-sort rejection. Source tests cover recursive indexed fields,
  pure computed indices, and unsolved/solved source-image resaves through Solve.
- [x] Recover parameter-instantiated family declarations through retained
  application/abstraction evidence and the shared pure normalizer. The unchanged
  legacy Vec test now compiles, including `(Vec Nat).nil`, cons and index checks.
  One-/two-parameter source-image resaves and parameter mismatch rejection pass.
- [x] Retain saturated index substitutions when recovering indexed instances.
  Typed arguments cross their own retained projection/reindex wrappers; the
  ordinary signature substitution rule checks dependent index order and sorts.
  Recovery still checks the reconstructed fiber against its input and never
  guesses a nominal declaration from Core. Solve retains the same result.
  Tests cover changed index images, wrappers between applications, dependent
  indices, partial/unknown-family rejection, and parameterized source resaves.
- [x] Generalize the existing Match rule to a motive in the generic index
  telescope followed by its scrutinee fiber. Constructor scopes and motive
  scopes share telescope lifting. Match and direct indexed IHs share checked
  motive substitution; no Core tag, new elimination rule or Replay was added.
  Kernel tests cover an index-dependent result type and reject fixed-fiber
  motives and incorrect branches. Source tests cover nonrecursive Match,
  one-/two-step indexed recursion and source-image resaves through Solve.
- [x] Infer constant branch results across dependent field telescopes by
  discharging innermost binders first with existing Pi/codomain rules. Peeling
  outer Pi binders first incorrectly rejected `(k:Nat)->D k->Nat`.
  Match, functions and handlers now share `pg_synthesis_constant_result`:
  open the complete telescope, then discharge it in reverse. Scope requests
  retain their binder; repeated requests reuse jobs. Structural Pi application
  shares substitution with APP, including pending handler effect inference.
  Tests accept dependent parameter domains with a constant final result and
  reject escaping result dependencies, partial discharge and excess arity.
  Normal checks/examples/results and the synthesis suite under ASan/UBSan pass;
  unchanged compatibility remains 1/18. This is not dependent motive inference.
- [x] Extend IH typing and erasure from direct fields to `U(Pi ... F {} Self)`.
  The shared field-shape check rejects negative domains and effectful results.
  Ordinary Pi/application evidence opens dependent arguments; motive
  substitution uses the returned value's own indices. Constant-codomain
  checking prevents that temporary result binder from escaping. Erasure uses
  existing Force, Lambda/APP, Fold and Thunk, without an Acc primitive or Replay.
  Tests cover dependent argument telescopes, mixed direct/function fields,
  indexed function fields, index-dependent IH classifiers, value-dependent
  result rejection, and source-image resaves followed by result normalization.
  Retained Pi-body recovery now follows checked Thunk formation/inversion too.
  Normal checks/examples/results pass; IADT, indexed source resaves and the
  function-field runtime cases pass ASan/UBSan. Legacy compatibility stays 1/18.
- [x] Pass declared indexed families as ordinary CBPV function arguments.
  Checked index scopes, family application, RETURN and Lambda build the
  callable wrapper; no new Core tag or kernel admission rule is needed.
  Parameter abstraction/application and explicit `&D` use the same adapter.
  Pending ordinary quotations keep their existing structural inference path:
  eagerly wrapping every quotation hid pending effect structure and regressed
  synthesis; that approach was removed. Tests cover result equality, wrong
  domains, ordinary pending effects and three unsolved/solved image resaves.
  `check check-examples check-example-results` passes; compatibility stays 1/18.
  Synthesis, indexed source resaves and a parameterized runtime comparison
  also pass ASan/UBSan.
  This does not turn unknown CBPV functions into logical family assumptions,
  support arbitrary partially indexed families, or complete Acc.
- [x] Start graph generation from retained Lambda/case derivations, not a new
  Returns predicate. `function_graph.c` generates an ordinary indexed relation
  for constant pure results and direct structural recursive fields. A graph
  constructor contains the original fields plus recursive output/relation
  premises; its result indices come from checked substitution and normalization
  of the original branch. This establishes schema formation, not totality.
  `@f` shares a producer keyed by its retained Lambda evidence; storage aliases
  share the generated family, while independent definitions stay generative.
  Tests cover identity/length/mirror schemas, incorrect recursive-result rejection,
  source aliases and unfinished/completed source-image resaves through Solve.
  Normal checks/examples/results pass; generated graph cases and the image CLI
  also pass ASan/UBSan (the image fixture writer uses its normal checked build).
  The unchanged source compatibility gate remains 1/18, not a completed gate.
- [ ] Complete graph generation's source contract before claiming legacy
  compatibility: replace the current field-wise recursive-result layout with
  call-site-aware derivation translation (including order, repeated/unused
  calls, nested computations and higher-order recursive fields). Retain the
  accepted source derivation as the correspondence justification, not a match
  on an erased Core or an assumed equivalence of unrelated declarations.
- [x] Build the ordinary dependent result packet and `*f` witness for the
  current direct-recursion fragment, using the same relation and checked
  induction rule. Packet is an ordinary parameterized ADT with output and
  graph fields, not a primitive Sigma or Returns predicate. Register `returned`
  through the ordinary constructor namespace; aliases share the producer.
  The unchanged length fixture now compiles, and both `main` and the
  proof-consuming `certifiedMain` agree with `expected` at Solve chunks 1/64.
  Identity/length/mirror witness construction and normalization pass; altered
  output indices are rejected. Unfinished/completed `.a` resaves use the same
  Solve and preserve these source results, without a Replay path.
  A test caught an IH lookup error: induction-scope substitution images contain
  source fields, while IH variables are in its destination context extension.
  Witness construction now uses those binders, without changing the kernel.
  Normal checks/examples/results pass. Program/witness tests, the unchanged
  length result comparison, wrong-index rejection and the image CLI pass
  ASan/UBSan; the image fixture writer uses its normal checked build.
- [ ] Complete graph-case names/binders, general call-site translation and
  post-hoc property proofs. Packet production and successful consumption do
  not yet establish length preservation or sortedness for QuickSort.
- [ ] Infer genuinely dependent source motives, rather than only checking a
  supplied dependent motive in the kernel; restore open family parameters,
  general dependent IH results and function graph/witness generation.
  Function-field IH support is not completion of Acc or the property milestone:
  the unchanged Acc source still stops at the neutral type-family parameter R.

Do not resolve that blocker by assuming an empty effect row proves totality.
[Effect Handlers, Evidently, extended appendix A](https://xnning.github.io/papers/icfp20evidently-appendix.pdf)
shows divergence through negative effect signatures. This is a warning about
that inference, not a counterexample established for A Program's declaration
discipline. The family adapter above instead builds a checked function from
an existing declaration; general neutral type computation remains unresolved.

The compatibility gate is 1/18; remaining cases are UNSUPPORTED, not expected failures.
The generated length fixture still stops at `*length` resolution. These
formation changes are prerequisite work, not completion of Acc/QuickSort
or the post-hoc property-proof milestone. No independent Replay was added.
Validation: `check check-examples check-example-results` passes. The scoped
family kernel tests and indexed source/resave tests also pass ASan/UBSan.
Index recovery and indexed Match/direct-IH changes pass the same checks;
compatibility remains 1/18. Added fixtures are not substituted for legacy gates.
One disputed legacy fixture, `negative/function_graph_computation_index.p`,
is accepted by Main despite its integration script expecting rejection. It is
not counted as a verified negative case; audit its computed index semantics
instead of deriving the expectation from its directory name.

### September 9: Reduction Evidence and Work Reuse Audit

At the start of this audit, `eval.c` issued a `pg_reduction_certificate` containing only source,
target, policy and WHNF/NF kind. Its validity comes from construction by the
local evaluator. `derivation_step` reconstructs a certificate by running that
same evaluator and checking endpoints. This is not an independently retained
reduction derivation. Serializing these four fields cannot safely seed an
accepted result cache. In particular, the APGSRC11 request codec is still not
a completed-work CHECKPOINT implementation.

- [x] Avoid rediscovering materialized WHNF locally: register its exact target
  pointer as its own answer under the same immutable policy, in the existing
  WHNF index. Keep distinct input/result receipts and preserve steps already
  consumed by a pending target job. Release its superseded temporary machine.
- [x] Test zero-step target reuse, policy isolation, input/result distinction,
  and an already-pending target completed by another request.
- [ ] For persisted reuse, retain checkable reduction dependencies, not only
  endpoint claims. Specify beta/environment substitution, semantic dispatcher
  operations, materialization and NF congruence before defining their codec.
  The existing transition/rule implementation must remain the authority;
  do not introduce a second evaluator or trust imported completion flags.
- [x] Retain completed NF phases as immutable predecessor/head/child/rebuilt
  references owned by the output graph. Child receipts are shared, not copied.
  A cached reflexive normal-form receipt references the completed reduction
  establishing its normality; it must not form a cycle with that reduction.
- [x] Check NF phase linkage, congruent Lambda/APP rebuilding, child identity,
  fixed policies and receipt lifetime after the evaluator store is destroyed.
- [x] Traverse the complete receipt dependency DAG with the existing DAG
  utility, including reflexive normality links. Check acyclicity, topological
  premise order, policy/kind agreement and shared-child growth. A 20-level
  duplicated-child example stays within 100 receipts; a 10,000-Lambda example
  plus the other selected proofs stays within 40,000. These are regression
  bounds, not language limits or a general complexity proof.
- [ ] Retain the WHNF execution basis and relocation/acceptance protocol. NF
  phases still end in locally-issued WHNF receipts, not a complete portable
  derivation. This change alone cannot authorize imported cache results.
- [ ] Distinguish completed-result evidence from an unfinished machine state.
  Current demand/defer frames contain callbacks and borrowed state; dumping
  these addresses cannot reconstruct a machine in another process. Define
  relocatable operation descriptors and prerequisite edges for supported work,
  preserving nominal/binder identity and fixed pure policy semantics.
- [x] Give each existing deferred auxiliary algorithm one immutable work
  descriptor. Fold (1), symmetry (2) and Identity (7) now register descriptor
  and invocation state, instead of copying three callbacks into every task.
  Polling, fuel, resume and cleanup still use the same evaluator; no Core tag,
  secondary evaluator or portable callback address has been introduced.
- [ ] Describe and relocate each work algorithm's semantic inputs and cursors.
  Descriptor factoring alone is not serialization. Demand frames still retain
  resume callbacks; the remaining semantic continuations and graph references
  must be represented before those states can be saved.
- [x] Remove nested resume pointers from `family_scope_work` and
  `family_result_work`. Force/field scope and result descriptors identify the
  continuation directly, while sharing the two existing polling algorithms.
  There are now 12 built-in deferred descriptors for 10 polling algorithms.
  No new state variants, reduction rules or Core forms are needed. The two
  callback-forwarding wrappers and the two per-invocation pointers are gone.
- [ ] Measure storage and reconstruction costs against recomputation; test
  changed inputs/policies and interrupted materialization before closing the
  original checkpoint gate. A request recipe alone does not meet this gate.

The local WHNF reuse change creates no public cache-admission API. Only a result
actually materialized by this work store can populate its reflexive entry.
No normal forms or alpha-equivalent terms are merged by Core interning.
Verification: `check check-prepared-modules` passes; `core_test` also passes
with ASan/UBSan. The focused case saves all work on a fresh target request
(zero additional steps), and preserves one already-charged step when replacing
a pending target machine. This is not a measured whole-compiler speedup.

NF dependency verification: `check check-prepared-modules` passes after the
receipt extension; the expanded Core test passes with ASan/UBSan, including
access to retained phase/normality edges after destroying both work stores.
WHNF/NF results and split-budget behavior remain covered. No new image format
or externally supplied certificate admission is introduced here.
The complete-DAG test extension was separately verified with `check` and
ASan/UBSan `core_test`; every enumerated dependency precedes its dependent
receipt, including head and child edges, not only normality links.

Deferred-operation verification: `check check-prepared-modules` passes,
including 758 module save boundaries. Core tests reject incomplete descriptors
without transferring ownership and interleave two invocations sharing one
descriptor without sharing their progress or results. Existing split-budget,
cancellation, failed-poll and reentrant-resume cases remain enabled. ASan/UBSan
`core_test` and `identity_test` pass. This change does not retain more work in
APGSRC11 yet.

Nested-continuation verification: the force-family tests now cancel and read
back a separate evaluator at each auxiliary-task entry, then check its saved
term against the explicit transport recipe. The uninterrupted evaluator
continues independently. Both transport directions and dependent/nondependent
codomains are covered, alongside the existing field-family all-cut tests.
`check` and the 758 prepared-module boundaries pass after the implementation
change; the expanded test passes in `check` and ASan/UBSan `identity_test`.
This is term readback coverage,
not a claim that auxiliary cursors survive an image round trip.

WHNF preparation audit: `eval.c:step` uses persistent environment/argument
links for beta work, but `resume_frame` also reconstructs demanded argument
prefixes and `task_step` runs borrowed poll/resume/destroy callbacks. The current
built-in semantic dispatchers contain 21 demand/defer registration sites:
`computation.c` 3, `iadt.c` 2, `symmetry.c` 3, `identity.c` 13. Thus beta-only
machine serialization would leave most of the required pure semantics outside
CHECKPOINT. `pg_eval_readback` is a term reconstruction, not a serialization
of pending auxiliary work; its documented caller-preserving behavior may lose
that work without changing the reconstructed term.

Next implementation boundary: model the existing semantic work operations and
their immutable inputs explicitly, retaining partial reconstruction cursors
only as resumable work. Reuse the current dispatch computations and operation
objects; do not persist raw C callback/state addresses or introduce alternative
Core Lambda/APP tags. Evidence for a completed WHNF must relate its initial
closure/arguments to its materialized result, including demanded work and
capture-avoiding readback. Neither the runtime cursor nor a saved endpoint pair
is that evidence by itself. General dispatcher coverage and proof transport
remain required before a completed-checkpoint claim.

#### Checkpoint Payload Audit After `f838488`

- [x] Add `eval_io.c` configuration-fragment transport (`APGCFG1`): a set of
  head closures and argument roots shares environment and argument DAGs and one
  existing Term/descriptor relocation table. `pg_environment`/`pg_argument`
  definitions are shared in `eval.h`, not duplicated in the codec. The codec
  never reduces, alpha-interns, writes C addresses or accepts evidence.
- [x] Test separate writer/reader processes, byte-identical unsolved resave,
  lexical capture avoidance, shared captured/parent links and argument tails,
  source destruction, 10,000 shared environment links, cycles, invalid table
  references and empty roots. A suspended beta evaluator resumes from each
  tested cut with exactly the remaining transitions, not from its source term.
  `check-eval-io` is part of `check`; ASan/UBSan also runs the process round trip.
  Verification: `check check-prepared-modules` passes; the expanded invalid-link
  cases pass in normal and ASan/UBSan `check-eval-io` runs.
- [ ] Integrate these references with materialization, comparison, Demand and
  deferred-work payloads in the same image relocation context. Concatenating
  independently relocated fragments would duplicate shared binders and is not
  sufficient. `APGCFG1` is a component format, not a new user file extension or
  full `.a` CHECKPOINT. No scheduler, completion flag or policy is restored by
  this component, and it cannot seed a WHNF/NF acceptance cache.
- [x] Retain standalone substitution work (`APGSUB2`, superseding `APGSUB1`) with the same
  `readback_entry`/`readback_context` structures used by the evaluator.
  `eval_internal.h` shares their layout with the codec, rather than defining a
  second readback machine. Save child dependencies, pending order, cursor,
  fresh binder, completed results and charged steps. Rebuild the pointer-keyed
  index with the evaluator's lookup/hash logic; resume through the existing
  `pg_substitution_advance`. Failed work is not exported by this component.
- [x] Check every cut of a shared-DAG capture-avoiding substitution, including
  the completed cut: exact remaining work, byte-identical unsolved resave,
  fresh-process resume after binder allocation, and rejection of invalid
  stages/child links/pending cycles. Compare free variables under the explicit
  input relocation; alpha comparison alone does not identify free binders in
  different graphs. `check check-prepared-modules` and normal/ASan/UBSan
  `check-eval-io` pass. A missing scratch Term-index initialization found by
  these tests was corrected before acceptance of this change.
- [x] Generalize retained readback to materialization (`APGMAT1`): preserve
  the current entry, remaining arguments, partial application spine, charged
  readback steps and completed cache entries from earlier arguments. Use the
  same readback codec and evaluator structures as substitution, with one Term
  relocation table for the work and input. Resume with `pg_materialize_step`,
  not a second evaluator. Shared readback cleanup also replaces duplicated
  destruction code. The common header now has state flags; old `APGSUB1` is
  rejected rather than interpreted as `APGSUB2`.
- [x] Test every transition boundary of multi-argument materialization, two
  consecutive saves without execution, a separate writer/reader process,
  capture avoidance, shared repeated closure results, exact remaining steps,
  retained cache counts and invalid completion flags. Normal and ASan/UBSan
  `check-eval-io` pass; `check check-prepared-modules` passes, including 758
  prepared-module save boundaries. These are component tests, not full source
  acceptance. Materialization cache-root order is not byte-canonical across
  processes; graph sharing, results and remaining work are the invariants.
- [x] Expose the existing materialization owner path for WHNF-job composition:
  additional configurations share the input's environment/argument forest and
  final Term-owner callback. Standalone APIs delegate with zero extra roots;
  no second readback traversal, state representation or reduction rule. Extend
  every readback cut with repeated owner roots and a shared argument tail,
  resave twice, and verify exact aliases, remaining steps and cache counts.
  Wrong configuration counts and missing owner Term roots clear all outputs and
  release reconstructed readback indexes. This prepares job transport; it does
  not publish imported work into the accepted WHNF/NF store.
  Verification: normal `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass with these ownership/rejection tests.
- [ ] Integrate raw materialization work with whole evaluator/Solve checkpoints,
  including Demand/deferred continuations and shared cross-component relocation.
  None of `APGSUB2`, `APGMAT1` or `APGCFG1` establishes the provenance of saved
  completed results or seeds accepted normalization caches. Import must use the
  ordinary computation/kernel rules, not introduce a separate Replay verifier.
- [x] Enforce the complete pending-work partition on substitution import.
  A reproduced missing-parent queue previously reached `DONE` with no root
  result. The substitution reader requires every record to be reachable from the root,
  and precisely the unfinished records to occur in dependency order in the
  pending chain. Reverse reachability and chain scans are linear in retained
  records; neither re-evaluates substitution. Regression tests remove a parent,
  remove a child, and select an incomplete reachable subgraph. Valid all-cut
  resumption and ASan/UBSan `check-eval-io` pass. This checks work-graph shape,
  not the semantic provenance of stored completed results.
  Materialization additionally retains completed entries outside its current
  root for earlier argument reuse; unfinished entries must still be reachable
  and occur exactly once in the pending chain.

The callback refactor is not the remaining implementation plan by itself.
Inspection of the actual work records found the following transitive payloads.
These are existing execution structures to relocate, not new Core node kinds.

- [x] Add raw structural comparison/independence transport (`APGCMP2`). Share
  `alpha_entry`, `binder_pair` and comparison-state layouts in
  `graph_internal.h`; rebuild the seen index with the comparison owner's
  lookup/hash functions. Retain completed entries, pending links, scope/cursor,
  stage, status and charged steps. Resume through `pg_comparison_advance`.
  The structural walk's normalized endpoints follow from its stage and inputs;
  they are reconstructed, not stored as independent results. No alpha interning
  or comparison runs during import.
- [x] Exercise every transition boundary for equal/unequal shared lambda DAGs
  and bound/free independence checks, including completed states and cursor
  traversal through multiple binders. Test two inert resaves with old graphs
  destroyed, separate writer/reader processes, empty completed work, invalid
  pending links/stages and rejection of normalization callbacks. Normal and
  ASan/UBSan `check-eval-io` pass. Remaining transition counts and seen-entry
  counts match uninterrupted execution. `check check-prepared-modules` also
  passes after this change; broader source acceptance remains incomplete.
- [x] Include owning-operation Term roots in the comparison's relocation table.
  `APGCMP2` replaces the standalone `APGCMP1` header with an explicit extra-root
  count. This retains owner inputs even for a zero-entry completed comparison.
  Tests retain an additional enclosing lambda/application, destroy the old
  graph at each resave, and require exact restored binder and Term sharing with
  the comparison entries at every tested cut. Separate processes also retain
  owner inputs. Old headers, out-of-limit root counts and failed imports clear
  outputs; no owner root is interpreted or compared during import.
  Verification: normal/ASan/UBSan `check-eval-io` and
  `check check-prepared-modules` pass with shared owner roots enabled.
- [ ] Integrate comparisons with the owning deferred operation and image-wide
  relocation. `APGCMP2` is raw work, not an equality certificate. Normalizing
  comparisons are explicitly unsupported by this component until their owning
  evaluator/policy state is retained; silently restoring them as structural
  comparisons would change semantics. These additions do not complete N5 or
  authorize a Main push.
- [x] Retain the existing Identity action-body payload (`APGIBD1`) through
  `identity_io.c`. `identity_internal.h` shares its actual scope/binding/work
  layout with the execution owner. Save phase, arity, position, cursor and
  owner roots through `APGCMP2`, preserving one Term/binder relocation table.
  Retain only initialized bindings still needed by collect/wrap; already
  wrapped binders live in the partial answer, and completed unused arrays need
  not be duplicated. Rebind arena/output ownership on import.
- [x] Test all cuts in compare/collect/wrap/ready for unchanged and changed
  bodies, double inert resave, destruction of prior arenas/graphs, exact
  remaining polls, shared caller source, separate processes and invalid phase,
  arity, position and cursor flags. Restore the payload into the existing
  `pg_eval_defer` mechanism and verify the original operation's resume result.
  Normal/ASan/UBSan `check-identity-io` pass; this gate is included in `check`.
  The unchanged case uses distinct alpha-equivalent inner lambdas, not merely
  the same Term pointer. `check check-prepared-modules` passes; the strengthened
  alpha case also passes both normal and sanitized component runs.
- [ ] Preserve complete machine configurations and all other deferred/Demand
  payloads in one shared image graph. The action-body tests construct component
  inputs and a caller; they do not yet demonstrate a source-level `.a`
  checkpoint. Saved comparison results remain raw work, not accepted evidence.
- [x] Compose the existing configuration-forest codec with the action-body
  payload. Configuration Term roots are handed to the owning payload codec,
  which combines them with its comparison and body roots before a single Term
  table is written. The plain configuration API uses the same implementation
  and keeps its format; no environment decoder or evaluator is duplicated.
  The execution owner selects the component reader explicitly; no C callback
  address is serialized or accepted from the stream.
- [x] At all tested body-work cuts, retain captured caller environments and
  shared argument tails together with body/comparison state. Two inert resaves
  destroy old graphs and still preserve exact source/binder sharing across all
  owners. Separate processes resume through the existing deferred operation.
  An invalid environment binder detected after successful body decoding clears
  outputs and releases the already-restored comparison. Normal/ASan/UBSan
  `check-eval-io check-identity-io` pass. Machine flags, policy, Demand frames,
  other auxiliary algorithms and source-level checkpoint wiring remain open.
  `check check-prepared-modules` also passes after this composition change.

| Existing owner | State that must survive a retained-work save |
| --- | --- |
| `eval.c:pg_environment` | Binder, value closure and parent; preserve shared parent and captured-environment references. |
| `eval.c:pg_argument` | Closure and next link, including tails shared with suspended callers. |
| `eval_internal.h:pg_eval_frame` | Caller, original arguments, target argument pointer (NULL for auxiliary demand), continuation and its state, parent, answer materialization and prefix cursor. The private copied prefix is derivable from original arguments up to cursor. |
| `eval.c:readback_context` | Entries keyed by term/environment, their result or pending stage, child edges, fresh binder, environment cursor and pending order. Recreating only the root redoes completed substitution work. |
| `eval.c:materialization` | Readback entries, selected entry, remaining arguments, partial spine and completion state. Both final WHNF output and demanded answers own one. |
| `eval.c:pg_whnf_job` | Original input/policy, machine or completed receipt, output materialization and charged steps. |
| `eval.c:pg_nf_job` | Original input/policy, current body/head/children, phase dependencies and explicit traversal stack, including suspended recheck work. |

- [x] Replace the Demand frame's target index and copied-count arithmetic with
  the already selected argument node pointer. The public positional request
  resolves once; execution and diagnostic readback compare argument pointers.
  NULL is an auxiliary demand, not an integer sentinel. Move the actual frame
  layout to `eval_internal.h` for shared execution/transport ownership.
- [x] Establish the suspended-prefix invariant: each private prefix link copies
  exactly the original closure, and its final next link is the original cursor.
  The selected argument is replaced only in the frame-removing transition.
  Core tests visit all 64 prefix lengths in the long-argument example and the
  auxiliary-demand cuts, checking pointers, values, environments and unchanged
  tail sharing. Normal Core tests and ASan/UBSan Core tests pass.
  `check check-prepared-modules` also passes after the pointer-target change.
- [x] Encode frame arguments, target and cursor in the common configuration
  forest. During relocation rebuild private mutable prefix links from arguments
  up to cursor, rather than save another copy of their closures or cast imported
  read-only argument pointers to mutable ones. This rebuild is storage relocation,
  not beta/iota evaluation; it must not change charged computation steps. Retain
  the answer's actual readback work in the same component (`APGFRM1`). A common
  materialization codec handles its fixed owner configurations, and execution
  and restoration use the same one-link prefix builder. Existing `APGMAT1`
  and `APGSUB2` layouts remain unchanged.
- [x] Test each active single-frame cut from demand creation through capture-
  avoiding answer readback and prefix copying, for positional and auxiliary
  demands. Double inert resave preserves shared environments, remaining steps
  and exactly one final continuation call. Restore the known test continuation
  explicitly; no callback address is decoded. Invalid auxiliary/prefix state
  is rejected after readback decoding with outputs cleared and resources freed.
  Normal/ASan/UBSan `check-eval-io` pass.
  `check check-prepared-modules` passes after the frame-data codec change.
- [x] Retain parent-stack data (`APGFST1`) with the active answer and one shared
  configuration/Term table. Under ordinary scheduler transitions a child runs
  before its parent's answer readback begins; parent answer contexts are empty.
  The writer checks this instead of silently dropping parent progress. Parent
  order is collected iteratively with cycle rejection. Single-frame and stack
  codecs reuse frame geometry restoration and the same prefix builder.
- [x] Check every active cut of three nested positional and auxiliary demands,
  parent emptiness, shared captured environments across levels, double inert
  resave, exact remaining steps and remaining continuation-call counts. A
  separate process resumes a three-frame stack during capture-avoiding readback.
  Cyclic parents, started parent answers and inconsistent auxiliary prefixes
  are rejected. Normal/ASan/UBSan `check-eval-io` pass.
  `check check-prepared-modules` also passes after the parent-stack change,
  including all 758 prepared-module save boundaries. This is component and
  regression evidence, not completion of the source-level checkpoint gate.
- [ ] Connect frame data to continuation-owner, policy and machine
  state retention. `pg_eval_frame_payload_read` intentionally leaves parent,
  continuation and state unset; it is data relocation, not a complete runnable image.
  The stack variant restores parent links, but still leaves continuation/state unset.
  Tests supply surrounding machine flags and known continuations externally;
  arbitrary continuation owners and source-level `.a` checkpoints remain open.

Continuation retention must include its actual owner data, not just a callback
label: current Demand sites retain Identity `action_scope`; other production
Demand continuations have no independent state. Fold and symmetry owners are
available directly from the restored caller reference, without evaluation.
Restore actual scope data through the same shared graph as caller closures.
Do not recover it by rerunning dispatch or serialize
C addresses. Reading remains inert; resumption invokes the existing evaluator.
There is no separate Replay evaluator: imported progress needs established
provenance before it can support accepted evidence, but reproducing the original
search history is not required. Whole-machine retention remains incomplete until
these owner payloads, deferred tasks and policy are connected together.

- [x] Add raw `action_scope` ownership transport (now `APGISC3`): source/body,
  arity, allocation state and each initialized source/endpoint/relation binder.
  Optional extra Term roots share one existing descriptor table. Missing binder
  fields remain missing; importing does not run `prepare_bindings`, scope
  analysis, or Act. This retains the actual existing structure, not another
  typed scope representation. It does not establish accepted Identity evidence.
- [x] Test unallocated bindings, all 16 field-presence masks, zero/nonzero arity,
  repeated binder pointers across entries and source/extra-root sharing after
  destroying and restoring both arenas twice. Unknown field masks fail with
  cleared outputs. Normal and ASan/UBSan `check-identity-io` pass. The full
  `check check-prepared-modules` regression run also passes; final allocation
  simplification and zero-arity cases were rechecked by both component builds.
- [x] Compose scope roots with Demand-frame/configuration roots. The existing
  readback/materialization codec now passes its ordered Term roots to the same
  owning-payload callback contract as configuration forests. Plain callers use
  the existing descriptor adapter; their formats and transition functions are
  unchanged. Stack, readback and scope use one Term table, not separately
  decoded tables with later binder reconciliation.
- [x] At every active cut of an auxiliary Demand, save a partially prepared
  scope together with captured caller bindings and answer readback. Destroy
  both arenas and resave twice; assert scope/caller/binder sharing, capture
  avoidance, identical total steps and one continuation call. An owner decoded
  successfully followed by outer configuration failure leaves frame outputs
  empty. Normal and ASan/UBSan component tests pass, as does the full
  `check check-prepared-modules` run. The added outer-failure assertion was
  rechecked in both component builds after that run began.
- [x] Bind production Demand continuations and owner scopes in the raw
  `APGCON2` stack envelope (`computation_io`). Resolve exact versioned names
  through the existing module-owned resolver. Identity owns the predicate for
  its four scope-bearing continuations; the other seven require NULL state.
  Shared scopes, frame data and answer readback retain one Term table. Unknown
  names and mismatched payload presence are rejected. This is structural
  relocation, not validation of arbitrary imported computation history.
- [x] Round-trip all eleven named continuation bindings and shared Identity
  scopes twice after arena destruction. Reject unknown names and non-NULL
  state on a stateless continuation. For a real suspended Force, retain every
  active-frame cut twice and resume through the original evaluator; assert the
  exact result pointer (relocated through a captured argument) and total steps.
  Normal and ASan/UBSan component tests pass, as does
  `check check-prepared-modules`. The synthetic name/scope fixture tests data
  linkage only; it does not claim semantic validity for fabricated frames.
- [ ] Retain machine flags, policy and deferred work before claiming
  whole-machine or source-level CHECKPOINT. The test still restores flags and
  selects the existing pure evaluator externally, not from a host address.

- [x] Replace per-frame bare resume callbacks with immutable owner-controlled
  `pg_eval_continuation` pointers. Each of the eleven production algorithms has
  a versioned name and its existing resume implementation. Demand creation and
  return still follow the same transition functions and borrow the same state;
  there is no second continuation executor or new Core Term tag. Plain test
  continuations use the same API.
- [x] Provide module-owned exact-name resolution and a pure-dispatcher resolver
  over the existing data/Identity/symmetry delegates. No mutable global registry
  or dynamic code loading is introduced. These names select algorithms, not
  invocation state, admitted policies, or accepted evidence. Data-only readers
  still leave continuation pointers unset.
- [x] Check all eleven production names for exact round trips and distinct
  descriptor identities; reject unknown versions, test-only names and names
  outside a module's ownership. `check check-prepared-modules` passes after
  migration. ASan/UBSan Core execution and `check-identity-io check-eval-io`
  also pass, including existing split-fuel, capture and continuation-count tests.
- [x] Connect the descriptors to scope payloads in `computation_io`; Fold and
  symmetry owners remain in the caller Term table. A valid Identity name and
  well-formed scope record do not prove the pending computation valid. Keep
  evidence admission separate from raw transport.

- [x] Remove redundant Fold-handler and symmetry-owner pointers from Demand
  state. The existing answer continuation reads the owner from the caller
  reference that `resume_frame` already restores. No owner search, Term
  evaluation, clause scan, or new semantic rule is needed. Core regressions
  observe both kinds of suspended frame with NULL state, then verify the
  existing nested/captured and split-step results. Normal and sanitized Core
  execution pass. `check check-prepared-modules` also passes after this change.
- [x] Add raw multi-clause Fold layout transport to `pg_builtin_graph_codec`
  (`computation-handler/v1`), distinct from a typed handler signature. The
  actual immutable label/position array is exposed read-only, not copied into
  another owner structure. Relocation passes original clause positions to the
  existing handler interner, which rebuilds destination pointer ordering. Clause
  bodies and other roots share the ordinary Term table. Body validation remains
  at `pg_computation_fold`; layout-only restoration creates no placeholder body.
- [x] Test both clause selections after two arena-destroying round trips and in
  a separate process. Generative labels with identical payload records remain
  distinct. Repeated roots and handler owners stay shared; reversed transport
  entry order recovers the same layout. Duplicate positions/labels, out-of-range
  positions and missing source clause bodies are rejected. Zero-clause Fold
  keeps its existing fixed descriptor. Normal/ASan/UBSan `check-identity-io`
  pass, as does `check check-prepared-modules`; the strengthened generative-label
  fixture was rechecked in both component builds after that regression run began.
- [x] Add common symmetry-owner transport (`symmetry/v1`) with scalar axis
  payloads, independent of the classifier-name scratch-buffer size. Expose the
  actual immutable owner axes; regular construction, textual-name resolution
  and payload restoration share permutation validation and the existing exact
  interner. Transport checks host-width conversion; the owner checks range and
  uniqueness. Identity/fixed-prefix reduction remains evaluation, not import.
- [x] Round-trip 128-axis reversal composition and the zero-axis identity in
  the same table as raw Fold roots. Two arena-destroying saves and a separate
  process preserve raw applications; subsequent pure evaluation yields the
  expected shared values. Duplicate/out-of-range axes are rejected without
  registering an owner. Normal/ASan/UBSan `check-identity-io` and the full
  `check check-prepared-modules` run pass. The final host-width-check cleanup
  was rechecked in both component builds after the full run began.
- [x] Retain multiple scope roots in one `APGISC3` payload. Exact-pointer
  collectors separately track scope objects and mutable binding-array bases;
  distinct scopes can share one array, including shorter prefix views. Equal
  contents do not merge distinct scopes or arrays. NULL roots represent absent
  state. All source/body/binder/extra roots use one ordinary descriptor table.
  Single-scope APIs delegate to this codec, not a second implementation.
- [x] Verify repeated scope roots, distinct equal-content scopes/arrays, shared
  array mutations, prefix views, NULL roots and an empty forest through two
  arena-destroying resaves. Invalid root references clear outputs. Normal and
  ASan/UBSan `check-identity-io check-eval-io` pass, as does the full
  `check check-prepared-modules` run (758 module-save boundaries). This supersedes the experimental
  `APGISC1` layout; it does not change a stable artifact contract or admit raw
  progress as evidence. Whole-machine integration remains below.
- [ ] Connect retained continuation names, scopes and all deferred algorithm
  states to whole-machine checkpoint ownership. Common graph support for these
  caller references does not by itself retain the live execution that uses them.

The ten auxiliary polling algorithms also have distinct payload obligations:

- [x] Resolve portable evaluation policies by exact existing descriptor identity:
  `evaluation/beta/v1` and `evaluation/pure/v1`. One owner table supplies both
  naming and resolution; a local policy with the same dispatch pointer is not
  silently treated as either portable policy. These names describe fixed
  reduction rules, not caller-provided handlers or imported evidence.
- [x] Test policy-name byte transport, exact memo-key reuse, receipt policy and
  different WHNFs for `Force(Thunk(v))` under the two restored policies. Reject
  unknown versions, work names and unnamed local policy descriptors. This is
  policy identity support, not a complete checkpoint header: machine flags,
  work-payload dispatch, shared configuration ownership and admission remain
  required. Normal `check check-prepared-modules` (758 save boundaries) and
  ASan/UBSan `check-identity-io check-eval-io` pass.
- [x] Extend the existing frame payload and named-continuation APIs with extra
  configuration roots. Frame callers, active readback, current configuration and
  task-owned lexical links now use the same configuration forest and final Term
  table. The owner determines the extra-root count; the inner reader validates
  the total. Existing frame-only APIs delegate with zero roots and reject an
  image containing extras rather than silently dropping them. No second
  environment codec, evaluator or Core representation is introduced.
- [x] Test two destroying resaves at every scope-frame suspension: preserve
  shared environments across owner roots and caller, shared argument tails and
  distinct equal-content environments. Test extra roots through the real named
  Force/field family-result frame path, and retain exact resumed step counts.
  Reject omitted owner Terms and mismatched extra-root counts with empty outputs.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.
- [x] Connect Symmetry prefix work to an owning configuration callback. The
  standalone API delegates to the same axis/prefix validation and restoration;
  a frame owner embeds those configuration roots in the common forest instead
  of serializing a separate environment graph. No alternate polling algorithm.
- [x] Verify every prefix position under a real Force Demand frame, with two
  arena-destroying resaves, identical results and exact charged steps. Assert
  captured-environment sharing with the parent's argument closure before and
  after relocation. Semantic reference heads intentionally drop their lexical
  environment (`eval.c`), so comparing to `frame.caller.environment` was an
  incorrect initial test assumption, corrected rather than changing evaluation.
  Test rejection after frame restoration and owner cleanup. Normal
  `check check-prepared-modules` (758 save boundaries), a final regular
  `check-identity-io check-eval-io` rerun and the ASan/UBSan component gate pass.
- [x] Connect action-scope discovery through the same owning-configuration
  interface as Symmetry prefix work. The original standalone API delegates to
  the shared scalar/cursor validation; frame callbacks include the pending
  argument tail and source/body roots in the same environment forest. Reading
  neither rescans the source nor allocates replacement source binders.
- [x] Exercise all discovery positions with and without a real parent Force
  frame, two destroying resaves, shared parent-argument environments and exact
  resumed step counts. Reuse the same test frame owner for Symmetry and scope
  work. Invalid center progress is rejected even after frame restoration, with
  explicit cleanup and no published work. Normal `check check-prepared-modules`
  (758 save boundaries) and ASan/UBSan `check-identity-io check-eval-io` pass.
- [x] Retain raw evaluator headers (`APGEVL1`): exact fixed-policy name, charged
  steps, status, head-ready flag, frame presence and existing task name. An
  owning callback embeds the configuration/frame/task payload in the same graph.
  Reading initializes the final-address evaluator rather than copying a local
  machine whose task may retain pointers to its temporary arena. It registers
  ordinary work but does not poll or issue accepted normalization evidence.
  Reject unknown identities, mismatched task/frame presence and invalid state
  shapes; release attached work on failure. Unattached resources remain the
  payload owner's cleanup responsibility.
- [x] Verify the envelope under both fixed policies, empty and active task
  states, actual Force frames, head readback and final WHNF. Two destroying
  resaves preserve status/flags/steps without manually reinstating those fields;
  the same evaluator finishes in exactly the unsuspended step count. Reject an
  invalid status with empty outputs. Also reject WHNF with a live frame after
  payload restoration, exercising destruction of attached resources. Normal
  `check check-prepared-modules` (758 save boundaries) passes; after extending
  that rejection case, regular and ASan/UBSan `check-identity-io check-eval-io`
  were rerun successfully. State-shape checks are not reachability proofs.
- [x] Complete raw machine payload dispatch for all twelve named tasks in
  `machine_io.c`. Reuse APGEVL1 for policy/status/steps and descriptor selection;
  route captured tasks through the shared configuration forest and other tasks
  through frame/scope ownership. Keep a single terminal Term table. Restore at
  the final evaluator address, register its original task, and release unattached
  task resources if outer validation fails. No separate evaluator or Core tag.
- [x] Exercise all twelve task kinds through this common reader: every cut of
  the selected Fold, Symmetry, higher action, Force/field family and action-result
  fixtures is resaved twice after destroying the old machine/output. Check exact
  status, step count, descriptor and frame presence without manual restoration,
  then finish with the ordinary evaluator. A coverage mask requires all twelve.
  Reject forged WHNF headers with live frames/tasks after payload reconstruction
  and verify cleared outputs and cleanup. This tests raw state, not provenance.
  Verification: normal `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass with all twelve task coverage required.
- [ ] Integrate the machine payload into source-image CHECKPOINT and WHNF/NF
  job ownership. Retain/check completed-work evidence and pending-state provenance
  before allowing imported progress to justify accepted results. Raw machine
  dispatch alone is not full `.a` CHECKPOINT completion.
- [x] Verify the common machine API across three independent processes: writer,
  inert resaver and reader. Save a pending action-result task under a real Fold
  frame, with shared caller/argument/expected-value roots. The resaver writes
  before advancing anything; both readers then finish with budget-one calls to
  the ordinary evaluator, checking exact total steps and the expected pointer.
  This extends the previous component-only fresh-process tests to the full
  raw machine envelope, descriptor dispatch, scope and configuration ownership.
  Verification: `check check-prepared-modules` and normal/ASan/UBSan
  `check-identity-io` pass. The same fixture is shared with all-cut frame tests.
- [x] Provide image-wide Term/object relocation for raw machine forests.
  `pg_computation_machines_write/read` use one terminal graph table, with nested
  codecs writing ordered root references into it. The enclosing image owns this
  table through the existing graph codec; object descriptor ownership is unchanged.
  Serializer-created temporary Reference wrappers are retained in the writer
  arena; source Lambda/Application nodes are borrowed, not cloned. Neither alpha
  comparison nor reduction participates in relocation. The seekable format is
  `APGMFS1`; loading reconstructs data without advancing any machine.
  Tests restore four machines across two destroying resaves: shared inputs retain
  identical pointers, distinct nominal binders remain distinct, beta/pure policies
  remain separate, and pending tasks resume with exact baseline total steps.
  A malformed machine count exercises cleanup after earlier tasks were restored.
  An empty forest after an enclosing-file prefix checks zero roots/machines and
  nonzero stream offsets. Normal and ASan/UBSan
  `check-identity-io check-eval-io`, plus `check check-prepared-modules` (758
  module save boundaries), passed. These remain component gates, not full N0-N7
  acceptance or source CHECKPOINT completion.
- [ ] Connect this common table to WHNF/NF job ownership and source CHECKPOINT.
  Preserve job dependency edges and completed receipts, not just raw machines.
  A restored status flag is not evidence that a typing or reduction conclusion
  was derived. Resume and any required recomputation must use the same existing
  rules, not an independent Replay evaluator or parallel typing implementation.
- [x] Move shared image-table ownership into `pg_graph_image_write/read`.
  The machine forest supplies only its payload and resource cleanup; graph IO
  owns temporary Reference retention, terminal relocation and exact payload/end
  boundaries. This keeps APGMFS1 bytes unchanged and allows readback/job owners
  to use the same table instead of copying machine-specific table machinery.
  A payload callback's success is provisional until the outer boundary passes.
  The owner releases restored resources on either failure path.
  Verification: a temporary Reference wrapper and an independent root group
  restore to one pointer; a prematurely successful payload reader is rejected.
  `check check-prepared-modules`, ASan/UBSan `graph_io_test`, and normal plus
  ASan/UBSan `check-identity-io check-eval-io` pass.
- [ ] Serialize the actual WHNF/NF store phases, not a second job model:
  while a WHNF machine is pending, retain its existing full machine payload;
  after the machine reaches WHNF but before publication, retain materialization
  with its input closure/argument forest and charged step counts. Do not encode
  that closure independently again. Completed jobs retain receipt dependencies;
  canonical target-cache jobs may have already destroyed their machines and
  therefore must not be serialized as live evaluators. NF additionally retains
  its head/children, predecessor phases and active dependency stack, preserving
  shared jobs keyed by exact input and policy. These states are observed in
  `eval.c:whnf_step`, `nf_complete` and `pg_nf_advance`; none calls for a new
  language-level computation variant. Portable WHNF evidence remains an open
  prerequisite to publishing imported completions into accepted work stores.
- [x] Put the actual WHNF/NF/receipt layouts in the existing evaluator-private
  header, shared by the evaluator and future state codec. Factor their identical
  exact input/policy request prefix and hash lookup/allocation into one local
  registration path. WHNF and NF keep separate indexes, work and result rules;
  registering an existing job does not reset its machine, readback or steps.
  No public imported-receipt acceptance API is introduced by this preparation.
  Verification: `check check-prepared-modules` and normal/ASan/UBSan `core_test`
  pass. The new regression grows both request indexes while WHNF/NF work shares
  a divergent input, then confirms job identity and unchanged charged progress.
- [x] Retain pending WHNF evaluation and materialization in `APGWHW1` through
  `pg_whnf_pending_write/read`. Both use the existing machine envelope/policy;
  evaluation delegates to the complete built-in payload, while readback owns
  its input configuration and request root through the existing materialization
  codec. An enclosing graph image supplies one shared Term/object table.
  No receipt is serialized and completed/error jobs are rejected by this codec.
  Reading leaves the actual job unregistered, without a work owner; advancing
  it is refused. Only an owner that established progress provenance may use the
  evaluator-private attach operation. Attachment uses the same exact-key lookup
  and refuses duplicate keys instead of replacing existing jobs. It is index
  maintenance, not evidence verification or an imported cache-admission rule.
  Verification: at every pending cut, two destroying resaves preserve total
  steps for a framed Act computation and a captured Lambda with shared readback
  dependencies. Tests cover unstarted/active materialization, refusal to advance
  before attachment, duplicate-key rejection, completed-job rejection and
  cleanup after decoding inconsistent step counts. Normal
  `check check-prepared-modules` (758 module boundaries) and ASan/UBSan
  `check-identity-io` pass. No new evaluator or Core tag was introduced.
- [x] Extend pending WHNF tests across independent writer, inert resaver and
  reader processes. Cover a deferred Act result under a Fold frame and active
  capture-avoiding final readback, using the same fixtures as the all-cut test.
  The resaver writes before attaching or advancing the raw job. Readers then
  use the original WHNF executor with budget-one calls; total charged work and
  explicit alpha comparison against the relocated expected result agree.
  No source/checkpoint evidence is accepted by this test-only fixture protocol.
  Normal and ASan/UBSan `check-identity-io` pass with both process sequences.
- [ ] Connect these pending jobs, completed receipts and NF dependency edges
  to the whole work-store owner and source image. The local fixture tests below
  establish their origin by construction; they do not justify arbitrary imported
  progress. The completed WHNF basis and full source CHECKPOINT gates stay open.
- [x] Add raw reduction-record transport in `reduction_io.c` (`APGRCP2`,
  replacing the initial `APGRCP1` layout to add independent phase roots).
  Traverse receipts and phase records through the existing iterative DAG utility;
  phases are visited directly rather than repeatedly scanning predecessor lists.
  Preserve shared child receipts, phase predecessors, normality links, exact
  source/target Terms and versioned policy identities in one graph table.
  Transient wire record kinds are not new Core Terms or evaluator states.
  Reading reconstructs the existing private layouts inside an opaque archive;
  the public interface permits inert resave but exposes no accepted certificate.
  Backward typed references enforce acyclic record structure, not semantic proof
  validity. The missing WHNF execution basis is not supplied by these records.
  Verification: two destroying round trips preserve duplicate roots, shared NF
  children, canonical normality links and distinct beta/pure policies. Writer
  cycles and reader self-references fail. Normal `check check-prepared-modules`
  and ASan/UBSan `check-identity-io` pass. A test initially assumed that duplicated
  source operands imply identical immediate NF child receipts; WHNF can reduce
  the function side first, so sharing is now checked with explicit equal neutral
  operands. No normalization rule was changed to satisfy that assertion.
- [x] Retain unfinished NF phase chains as direct archive roots. Use the same
  receipt/phase dependency table, so pending phases and completed child receipts
  retain shared identity. Do not manufacture a completed NF certificate merely
  to give a partial history an archive root. Phase-only archives are supported;
  their presence does not assert completion or grant evidence acceptance.
  Verification: a real NF job stopped at `NF_RECHECK` retains its partial phase
  and aliases with a completed child across two destroying resaves. Phase-only
  roots pass; cyclic predecessors and certificate/phase root reinterpretation
  fail. Normal `check check-prepared-modules` and ASan/UBSan `check-identity-io`
  pass. Whole-job continuation/stack ownership remains unfinished.
- [x] Factor NF phase congruence into `pg_reduction_phase_rebuild`, used by
  the actual NF executor. It rebuilds Lambda/Application from head/child
  endpoints with matching reduction kinds and policy. Head-only phases do not
  establish normality; the existing executor still checks completion. Future
  retained-evidence checking must use this rule, not a parallel NF algorithm.
  This is not an imported-leaf acceptance API. Synthetic-premise tests reject
  wrong endpoint, policy, kind and child arity; the existing independent NF
  dependency assertions remain. Normal and ASan/UBSan `core_test`, plus
  `check check-prepared-modules` (758 module boundaries), pass.
- [x] Connect NF predecessor checking to that same reconstruction rule:
  the previous rebuilt Term must be the next WHNF input, with unchanged policy.
  The actual executor and APGRCP2 reader both use it. The reader checks phases
  during its existing dependency-ordered endpoint restoration, without a second
  traversal or repeated predecessor scans. Wrong rebuilt Terms, disconnected
  predecessors, changed policies and right-only children are rejected by new
  archive tests. Normal `check check-prepared-modules` (758 boundaries) and
  ASan/UBSan `check-identity-io` pass. No wire layout or Core tag changed.
  Local phase consistency is not WHNF leaf validation, a proof of final NF,
  or authority to publish imported receipts into the accepted work store.
- [x] Share the NF terminal rule between the actual executor and record
  checking. A head-only Reference result finishes; a rebuilt Lambda/Application
  needs an unchanged WHNF recheck after its child reductions. Remove the
  executor's duplicated completion branch. Record checking also verifies chain
  origins/results and canonical normality links (same kind/policy, reflexive
  endpoint, no second phase basis). A temporary origin table follows the
  existing dependency order; no persistent record field or chain rescan is
  added. Nine negative archive fixtures cover partial histories presented as
  completed NF, unnormalized parents and inconsistent endpoints/cache links.
  Normal `check check-prepared-modules` (758 boundaries) and ASan/UBSan
  `check-identity-io` pass. These checks still assume justified WHNF leaves;
  imported receipts remain opaque and cannot enter accepted evidence.
- [x] Add the recomputation-mode admission path for decoded reduction records.
  `pg_reduction_check` walks the shared record DAG, recomputes WHNF leaf inputs
  through the ordinary policy-keyed work store, and checks saved targets with
  structural alpha comparison only. Conversion would incorrectly accept a
  reducible saved target, so it is not used here. Decoding has already checked
  NF congruence, chain endpoints, completion and canonical receipt links.
  Only a completely successful check exposes certificate roots; pending,
  different and failed checks expose none. Phase-only roots remain partial
  histories, not completed certificates. The writer/checker share one collector.
  Verification: normal `check check-prepared-modules` (758 boundaries) and
  ASan/UBSan `check-identity-io` pass, including two destroying resaves of
  shared NF records, relocated Lambda binders, a convertible but non-WHNF saved
  target, budget-one checking and divergence. Rechecking with the same work
  store does not advance completed WHNF jobs. This reuses the existing evaluator
  and comparison, not an independent Replay implementation.
- [x] Reuse accepted NF receipts via `pg_nf_remember` in the ordinary exact
  input/policy job store. Local NF completion and remembered results share
  `nf_publish`/`nf_finish`, including canonical-target receipt construction.
  Existing completed answers are not replaced. Pending source/target jobs keep
  their identities and charged steps; superseded traversal stacks are released.
  This API consumes accepted evidence, not raw endpoint claims, and borrowed
  receipt graphs must outlive the work store. No second restore-only cache is
  introduced. Normal and ASan/UBSan `check-identity-io` pass: checked archives
  settle already-active NF source and target jobs without further NF steps,
  leave other-policy jobs pending, and retain an existing canonical receipt
  when another accepted proof for that key is supplied. Full normal
  `check check-prepared-modules` (758 boundaries) also passes.
- [x] Verify the existing source-to-cache connection rather than add another
  normalization path. `program.c:pg_program_normalize` delegates to
  `synthesis.c:normalization_receipt`, which already uses `program->evaluation`.
  A program regression computes an accepted NF in another work store, destroys
  that store, remembers its receipt and runs the ordinary typed normalization
  job without further NF steps. Classifier and typing-store ownership remain
  unchanged. Independently elaborated alpha-equal functions still use different
  exact Core keys; remembering one never supplies the other's typing evidence.
  Normal and ASan/UBSan `program_test`, plus normal
  `check check-prepared-modules` (758 boundaries), pass.
- [ ] Preserve the shared origin of a source normalization input and its saved
  receipt before adding completed normalization to APGSRC11. Currently
  `source_io.c:producer_child` and the normalization producer record retain
  context/subject producers, mode and force intent, not a completed Core input.
  The receipt codec restores its own Core roots. A shared relocation table is
  necessary, but must also connect those roots to the actual producer output
  (including binder/declaration origins); an unrelated reconstruction may be
  only alpha-equal. Do not fix a missed pointer-key lookup by alpha-interning or
  treating a cached Core result as source typing evidence. Test this connection
  across destroying resaves and independent processes, including force intent
  and distinct nominal declarations. The program regression above establishes
  consumption of matching Core evidence, not this source-image correspondence.
  Follow-up evidence: `tests/source_io.c:normalization_origin` retains the
  polymorphic source identity, its rule input and its forced rule input in one
  APGSRC11 image. Three destroying resaves, including one before any Solve,
  reconstruct exactly shared Core/classifier pointers; forcing the re-solved
  source also yields the retained forced Core pointer. Normal and ASan/UBSan
  `source_io_test normalization`, plus normal `check check-prepared-modules`
  (758 boundaries), pass. Thus existing origin collection and
  allocation factories already provide this correspondence for the tested
  Lambda case. Do not introduce a second binder-remapping mechanism. Extend
  the existing common derivation/Core table to retained reduction evidence;
  nominal/IADT coverage and independent-process evidence still remain open.
- [x] Add `retained_io.c` as the transport owner composing existing derivation
  and reduction codecs through `pg_graph_image_write/read` (`APGRET1`). It adds
  no rule, evaluator or acceptance flag. One terminal Core/object table serves
  both payloads; descriptor callbacks retain their original owner. Outputs are
  published only after the enclosing image boundary passes. Existing effect
  inference failure/cleanup contracts also apply to outer-boundary failures.
  Normal and ASan/UBSan `check-identity-io` pass: two destroying resaves retain
  shared rule roots, exact reduction endpoints and binder pointers; checked
  reduction results populate the ordinary NF key used by the rule input, while
  deliberately incomplete rule premises remain unaccepted. Invalid outer-table
  offsets leave all output groups unchanged. This is the shared payload owner,
  not yet its integration into source selection/origin records or CHECKPOINT.
  Full normal `check check-prepared-modules` (758 boundaries) also passes.
- [x] Connect the retained payload owner to source images through
  `pg_sources_write_retained` (APGSRC12). APGSRC11 remains the explicit
  no-retained-results path. Both use the same source/scope/origin record logic;
  APGSRC12 uses the shared derivation/reduction Core table for its final payload.
  Retained endpoint/intermediate Terms feed existing source-allocation origin
  collection, not a new binder map. Reading retains raw graph-owned records in
  `program->retained_reductions` and does not check them or seed the work store.
  The owner may explicitly check/remember NF evidence or use ordinary Solve
  without it. Tests retain a polymorphic source function and its forced NF,
  destroy/reload three times including a save before Solve, and verify that
  type synthesis reconstructs the exact saved NF input. Checked reuse adds zero
  NF steps; ignoring the saved records performs NF and yields an alpha-equal
  result with the same classifier. Full normal `check check-prepared-modules`
  (758 boundaries) and ASan/UBSan `source_io_test normalization` pass.
- [x] Exercise APGSRC12 with independent writer, two inert resavers and separate
  checked-reuse/recompute readers in the permanent `source_io.sh` gate. Reuse
  the in-process fixture/check helpers; do not maintain another test evaluator.
  Cover polymorphic identity and an identity annotated by a source Nat alongside
  a separately declared, same-shaped Other type. The checker advances with
  budget one. Reading/resaving leaves Solve and result caches untouched; after
  checking, ordinary typed normalization consumes the same exact Core input
  without NF work. Recompute readers perform NF and check the same result and
  classifier. The lambda/nominal cases pass normal and ASan/UBSan runs.
- [x] Retain constructor-only declaration origins: index both the family and
  its existing layout in the same source-origin index. The `nullary` fixture
  declares same-shaped Nat/Other and selects a thunk returning Nat.zero, whose
  Core contains no family reference. Independent writer, two resavers, checked
  reuse and recompute readers preserve the exact input and classifier in normal
  and ASan/UBSan builds. Normal
  `normalization` and `prepared-module` (758 save boundaries) also pass.
- [ ] Constructor/recursive-Match retained input identity regression (September
  9): both fixtures now run in `source_io.sh` and fail the exact source-pointer
  assertion after inert resaving. Originally the constructor case also failed
  alpha comparison: the origin index covered declaration families but not the
  computation layout referenced by constructors. Indexing the existing layout
  alongside the family restores nominal identity without putting type metadata
  in Core. Both cases now compare alpha-equal, but exact pointers still differ.
  This occurs in the recompute reader too, before installing any saved result;
  it is not caused by receipt checking. The expanded gate fails in both normal
  and ASan/UBSan builds at the same assertion; sanitized `normalization` passes.
  Preserve the exact-pointer assertion: do not turn alpha equality into an
  interning/cache lookup policy. Extend retained allocation ownership for
  generated binders (including application sequencing and constructor wrappers)
  through existing producer/rule inputs, or retain their prepared producers,
  rather than inventing a second evaluator or a post-hoc alpha remapping pass.
  Check recursive Match-generated allocations as well. Source-facing retention
  selection must not claim exact cache reuse until these regressions pass.
  Concrete allocation owners: `constructor_scope_step` lifts each field using
  a fresh binder; `application_bind` allocates the sequencing-result binder.
  Neither is a source Lambda/Pi `BINDING_JOB`, so enumerating source bindings
  cannot retain these allocations. Their accepted context-extension rule inputs
  already carry the binders, but the source restart recreates the helper jobs
  instead of reconnecting their retained producer inputs. Extend prepared
  producer ownership/transport rather than deriving a binder correspondence from
  the output Terms. Also audit `pg_prove_constructor_scope` and induction helpers
  so direct and budgeted construction obey the same allocation contract.
- [x] September 9 follow-up at `7341ab7`: run every retained-image fixture
  independently, including both checked reuse and recomputation. A failed
  constructor assertion must not hide recursive Match or later source tests.
  `source_io.sh` now shares one runner for typed/source roots and returns failure
  after collecting retention failures. Normal and ASan/UBSan runs both find four
  exact-input failures: source-only constructor/Match, each in reuse/recompute.
  All six typed-root fixtures and four other source-only fixtures pass. Later
  annotation, prepared module-obligation and nominal/operation tests also pass.
  This is expanded failure evidence, not completion of source retention.
- [ ] Reconnect constructor allocations before the owning helper can execute.
  `declaration_step` registers `CONSTRUCTOR_VALUE_JOB` while publishing members;
  `request_inputs` enqueues it immediately. `constructor_value_step` requests
  its field scope before building the saturated constructor and abstraction.
  Therefore attaching a saved scope only when a qualified source name is read
  is not a sufficient ordering contract. Preserve the owner input (formation,
  explicit constructor pointer and parameter substitution), reconnect its scope
  allocation before execution, and reuse the existing constructor-scope job.
  `resolve_member` also creates parameter-instantiated owners: do not key this
  state solely by declaration, constructor ordinal or source syntax. Do not
  infer constructor identity from its field context; nullary constructors can
  share that context. Verify both direct and instantiated member paths, inert
  resaving and scheduler-budget independence. The separate source Match/IH
  allocation connection remains required even after constructor restoration.
- [ ] September 12 counterexample: changing only `declaration_step` to pass
  its source job into `pg_synthesis_constructor_value_jobs` preserves source
  provenance but breaks existing sharing with accepted-evidence callers.
  The experiment failed `source_declarations` at the exact constructor-binder
  comparison against `pg_synthesis_constructor_scope(evidence(formation),
  constructor, evidence(parameters))`; it was reverted, not accommodated by
  weakening the test. Source and evidence producer pointers are different
  request keys even when their results coincide. Restoration must preserve
  source dependencies AND the allocation owner used by direct/instantiated
  constructor calls. Merely changing that caller, or comparing resulting
  Lambdas up to alpha-equivalence, does not fix this. Before adopting a shared
  owner design, test both registration orders, pending/restored inputs and
  source/evidence access with exact binder equality. Do not merge arbitrary
  producers or their proofs because their computed Core happens to coincide.
- [x] Add declaration-owned member allocation inputs before publication.
  `pg_synthesis_declaration_member_at` retains raw constructor/prefix/field
  references on the source declaration job. After ordinary schema formation,
  publication checks the constructor pointer against the selected source clause
  and attaches the allocation to the existing accepted-input constructor-scope
  job before it executes. No producer/result equivalence or new Core tag is
  introduced. The borrowed getter exposes restored inputs before Solve and
  complete allocations of published members. Tests restore a nominal source
  declaration in a fresh synthesis store, exercise one-step and 64-step budgets,
  preserve exact source/evidence constructor binders, recompute deliberately
  incorrect field types, and reject wrong constructor, field count, index,
  conflicting and late attachments. Normal full synthesis, constructor-input
  tests and all 758 prepared-module snapshots pass; sanitized full synthesis
  and constructor-input tests pass. Source-image connection remains open below.
- [x] Connect these declaration member inputs to source-image collection and
  restoration. Include complete member allocations when their source declaration
  is retained, use the existing shared context/raw-reference payload, and attach
  them before any Solve step. Inert resaving must preserve them even without
  published exports. Do not serialize them as accepted constructor proofs.
  Re-run all source-only retention fixtures, including recursive Match; retain
  exact-Core assertions. Parameter-instantiated members outside declaration
  publication and partial scopes still need their own actual owner inputs.
  September 12 result: APGSRC18/19 append declaration-member producer/clause
  references and store their constructor/field identities in the existing
  APGRET2 context payload. Selected allocation origins also retain their source
  producer, so the reader attaches the raw member inputs before Solve. Both
  codecs preserve these inputs without requiring published exports on resave.
  Declaration-only tests destroy the graph between three unsolved resaves;
  nullary and field-bearing members retain their identities. Invalid member
  count, owner and clause index reject without publishing reader outputs.
  Normal and ASan/UBSan source-image gates now pass source-only constructor
  reuse AND recomputation: the remaining failures are the two recursive Match
  exact-input checks. All six typed-root fixtures pass. Normal prepared-module
  coverage passes all 758 snapshots; normal and sanitized seed tests pass.
  APGDRV6 and APGRET2 are unchanged. Recursive Match allocation-owner recovery,
  instantiated members and partial-scope checkpoints are not claimed complete.
- [x] Retain recursive source Match induction allocations via the existing
  origin records and APGDRV6 payload. APGSRC20/21 recognize the elimination
  syntax at those records; no separate replay rule or Core object is added.
  `pg_synthesis_restore_match` registers the unaccepted input and ordinary source
  synthesis passes its allocation to `pg_prove_induction_at`, checking newly
  synthesized branches/motive. A source-only test destroys the program between
  three unsolved resaves, checks idempotent restoration and rejects a source
  job passed as an alleged derivation input. After Solve, recursion, argument,
  self and all induction clause-context identities match the retained input.
  Normal and sanitized full synthesis, seed and Match-origin tests pass;
  all 758 normal module snapshots pass. Both full source-image gates report
  only the same two source-only Match failures, with the other cases passing.
  The original two source-only Match assertions still fail: the first exact
  difference moved from `aaabfbff` to `aaabfbffbfbbabff` (f=function,
  a=argument, b=Lambda body). Keep that bounded diagnostic and exact assertion;
  preserving the induction expansion does not restore source branch binders.
- [ ] Restore source Match branch-scope allocations before their workers run.
  `match_step` requests constructor fields over the scrutinee's context, which
  is not necessarily the declaration-publication context. It then creates a
  motive binder and `induction_scope_step` introduces IH binders. These source
  branch contexts are distinct from the final induction expansion's clause
  contexts already retained above. Preserve each actual owner/input and do not
  substitute the final clause contexts for source branch contexts merely
  because they are alpha-equivalent. Cover neutral recursive Match and the
  separate computation-scrutinee sequencing allocation before closing retention.
- [x] Recover source branch field/IH identities from checked origin Lambdas.
  The Match job waits for its existing origin derivation through ordinary Solve,
  then reads each abstracted context from the Lambda/Pi premises. Constructor
  scope restoration validates the field prefix/count; the source motive binder
  is retained while its type is recomputed. `pg_synthesis_induction_scope_at`
  reuses the existing scope worker and context-allocation vector for the IH
  suffix. Motive substitution recomputes IH types; short/long suffixes, wrong
  prefixes, conflicting/late attachments fail. This adds no wire format, proof
  rule or alternative acceptance path. The saved branch conclusion is not used
  as the source branch result. Tests compare all leading branch Lambda binders
  after three inert resaves and check valid/wrong-type/short/long/wrong-prefix
  suffixes with one-step and 64-step budgets. Normal and sanitized full synthesis
  and expanded Match-origin tests pass; sanitized module tests pass 758 snapshots.
  Both source-image gates retain only the two known source-only Match failures.
  The two source-only Match failures remain: first difference is now
  `aaabfbffbfbbabffbba`, inside the branch body rather than its field/IH binders.
- [x] Preserve the remaining branch-body application allocation owner.
  September 12 continuation after `ce2f24d`: the suspected producer mismatch
  was confirmed. The imported scope used a derivation-input context producer;
  source IH branch synthesis replaced it with an evidence producer for the
  same context. Since lexical scope keys include that producer, this created
  a second application job and lost its saved sequencing binder. Carry the
  checked branch origin into the existing induction-branch worker and reuse
  its context producer edges, checking each resulting context against the
  independently reconstructed field/IH extension. Do not merge arbitrary
  scopes or substitute the saved branch conclusion for source synthesis.
  Tests now require exact whole-branch and final reduction-source Core
  identity after three inert resaves, followed by one-step Solve. The full
  source-image script passes normally and under ASan/UBSan, including all
  twelve typed/source-only retention fixtures; normal `make check` passes.
  This closes the four historical constructor/Match exact-input regressions
  for those fixtures, not general checkpoint transport or all Match scopes.
  ASan/UBSan full synthesis also passes. `check-acceptance` passes component
  checks, all eight available 01-09 source examples and their selected runtime
  results, then stops at the unchanged open-family gate (175/209/280/323 steps,
  DONE/UNSUPPORTED/UNSUPPORTED/UNSUPPORTED). Running its later image-origin
  and prepared-module gates separately passes, including 758 save boundaries.
  Full acceptance and Main push remain pending.
- [x] Route declaration exports and instantiated members through
  `pg_synthesis_constructor_value`. Its `_at` entry attaches the existing
  field-scope allocation before that scope executes; both entries return the
  original `CONSTRUCTOR_VALUE_JOB`, with no new Core form or proof rule.
  Tests register allocation before/after requesting the pending member, check
  exact Lambda/RETURN/constructor Core and classifier binders, recompute bogus
  retained field types, reject count/prefix/conflicting/late allocations, and
  keep nullary members as values. Full synthesis tests pass normally and under
  ASan/UBSan; normalization and all 758 prepared-module snapshots also pass.
  Source-image transport does not yet encode these owner inputs. The normal
  source-image gate still reports the same four constructor/Match failures;
  this API integration does not close that remaining requirement.
- [x] Replace the member worker's accepted-proof-only inputs with formation
  and parameter-substitution producers. `pg_synthesis_constructor_value_jobs`
  uses the existing member job and field-scope dependency; the accepted-input
  API wraps its proofs as ordinary evidence producers. The member reads their
  results only after scope validation succeeds. Tests keep both inputs pending,
  attach saved fields before Solve, check the exact generated Core, reject an
  invalid formation and reject foreign-store input jobs. This adds no job role,
  Core tag or parallel acceptance state. Producer serialization and allocation
  reconnection to source-created owners remain open; no wire version changes.
  Normal and ASan/UBSan full synthesis pass; sanitized normalization and 758
  prepared-module snapshots pass. The normal source-image gate retains the
  same four exact-input failures, with the other cases passing.
- [x] Extend the existing shared graph-image envelope to transport raw input
  Terms alongside unaccepted rules and optional reduction records. APGRET2
  uses the same Core/object relocation table for all three groups; it does not
  encode raw constructor/binder references as fabricated proof rules. Readers
  publish all outputs only after the whole envelope succeeds. Tests destroy
  the source arena between four saves, preserve cross-group pointer sharing
  and a raw-only fresh binder, omit reduction records, and save a raw-only
  image with no rules. Invalid presence flags and enclosing table corruption
  fail without publishing outputs. Identity I/O and seed tests pass normally
  and with ASan/UBSan; sanitized normalization and 758 module snapshots pass.
  Retained source images now use APGSRC15; ordinary source remains APGSRC13,
  and derivations remain APGDRV6. Correct the seed test's obsolete APGSRC11
  assertion, preserving its truncation/policy/malformed-input coverage.
  Source currently supplies no extra Terms and rejects unexpected ones on
  load: pending constructor producer records and allocation contexts still
  need to be connected to this payload. The normal source gate still reports
  the four known constructor/Match exact-input failures, not new failures.
- [x] September 12: transport explicitly selected prepared constructor members
  through the ordinary source producer DAG. APGSRC16/17 use APGRET2 for shared
  raw constructor references, complete field allocations and unaccepted rules;
  APGDRV6 is unchanged. Formation and parameter inputs remain producer edges.
  Register the existing field-scope dependency when registering the member so
  allocations can be attached before execution. Loading and repeated resaving
  do not run Solve or accept stored field annotations. Tests cover pending and
  completed inputs, deliberately incorrect stored field types, invalid labels,
  nullary values and three destructive resaves. Also fix source producers that
  are rule origins: their producer record selects the source variant only;
  separate origin records retain rule provenance. Sanitized full synthesis,
  constructor-input and normalization tests pass. The full normal source-image
  gate still reports exactly four source-only constructor/Match input-identity
  failures; its other cases pass. The source-only constructor/Match allocation
  reconnection and partial field-generation checkpoints remain outstanding;
  this explicit-input transport does not close either requirement.
- [x] Add `pg_synthesis_constructor_scope_at` as allocation input to the same
  constructor-scope job, not another synthesis rule. Share the existing
  telescope binder-list storage/attachment helper (`context_allocation`). The
  retained context supplies binder identities only: ordinary substitution lift
  recomputes field types; target prefix and field count must match. Conflicting
  or late attachments fail without replacing an existing allocation. Tests
  compare exact reconstructed contexts and substitution image Terms, not proof
  record identity (direct and scheduled derivations may differ). Deliberately
  wrong retained field types are not trusted; wrong count/prefix are rejected.
  Normal and ASan/UBSan full `synthesis_test` pass, including existing telescope
  tests. Normal `normalization` and `prepared-module` (758 boundaries) pass.
  This API is not yet connected to source-image producer transport; the
  constructor/Match retained-input regression above remains open.
- [x] September 9: `pg_synthesis_application_at` attaches the existing
  `context_allocation` representation to the ordinary source-application job.
  Sequencing consumes its binder vector in evaluation order; result-context
  rules recompute types. The context prefix and exact number of consumed
  binders must match. Conflicting/late attachments are rejected. Tests cover
  argument-only sequencing and callee-plus-argument sequencing, reconstructing
  exact Core from distinct source requests using the same allocation even when
  its stored annotations are deliberately wrong. Short/long vectors and wrong
  prefixes fail. Source-image connection follows below; the source-only
  constructor/Match retention gate remains open.
  Verification: normal and ASan/UBSan full `synthesis_test`, normal
  `source_io_test normalization` and `prepared-module` (758 boundaries) pass.
- [x] Distinguish retained typed-root checking from source re-elaboration.
  `retained-write-typed` writes both the original source root and its existing
  typed derivation root using APGSRC12. Both remain unaccepted after reading;
  source whole-module synthesis still runs. Ordinary derivation checking then
  supplies the typed input to normalization. Independent writer/two-resaver/
  reuse/recompute tests pass for field-bearing constructors as well as lambda,
  nominal and nullary fixtures. No alpha-keyed cache or binder remapper is used.
- [x] September 9: retain completed source-application allocation origins using
  the existing `(scope, syntax, rule)` origin records. The rule is the already
  constructed final result-context extension, not a new certificate kind.
  Restoring registers that ordinary unaccepted derivation; application
  preparation waits for its validation and the lexical prefix before attaching
  the same binder vector. Inert resaving preserves the original rule reference.
  A still-partial application's current context is NOT a complete allocation
  recipe: exporting it as one would reject later valid binds. Partial job
  retention remains part of the outstanding CHECKPOINT work.
  The `application` fixture uses explicit function quotation and
  `f (f Nat.zero)` with a nullary ADT. Both source-only and typed-root modes are
  included before the still-open constructor regression in `source_io.sh`.
- [ ] Complete source-origin scope coverage. Historical failure (field/IH
  scopes are resolved by APGSRC13/14 below; effect-owner state remains open):
  collecting application origins exposes
  applications inside Match branches. `pg_synthesis_environment_input` rejects
  scopes with `hypothesis_for` (also `effect_owner`) and contexts not constructed
  by the retained lexical binding jobs. Consequently `retained-write-typed ...
  match` now fails in origin collection, before the earlier source-only
  constructor gate. Do not silently discard these origins or disable the Match
  test. Retain the branch-scope producer and its ordinary induction-context
  inputs, preserving its parent and IH binding, through the shared source
  environment representation. This is source preparation state, not another
  evaluator or a new accepted proof kind. Until connected, the new application
  origin feature is partial and the full source image suite is not green.
  The independent source-only `application` writer/two-resave/reuse/recompute
  sequence passes; normal full synthesis, normalization and 758 prepared-module
  boundaries pass. Both normal and sanitized source suites reproduce the Match
  write failure. Main push remains blocked by incomplete implementation, not by
  an external dependency.
- [x] September 12: retain the handler inference boundary's source allocation
  site and parent scope in APGSRC22/23. The same `prepare_handler` initializes
  ordinary execution and inert restoration; no equations, solutions, counters
  or accepted flags are imported. Inherited scopes keep their existing parent
  owner, while an explicit saved boundary reconstructs its independent owner.
  Initialization publishes the state only after allocation succeeds. The
  environment view no longer rejects every descendant with an effect owner;
  source-origin collection follows the boundary's original syntax. The
  `handler-scopes` regression checks pending and settled return-only inference
  boundaries through three destroying inert resaves, idempotent registration,
  and one-step ordinary Solve yielding the empty effect row. This is boundary
  transport, not full handler retention or pending equation checkpointing.
  Normal `make check` and ASan/UBSan source/synthesis suites pass. After adding
  a Lambda inside the inherited scope, both source suites pass again. The
  separate full-acceptance gates below intentionally retain the exposed failures;
  no full acceptance or Main push is claimed.
- [x] Preserve return-only Fold continuation allocation. Continuation after
  `caa27e7`: the `fold` fixture exposed a fresh return-clause binder
  (`aaabinder`, alpha-equal but not exact). APGSRC24/25 retain its context
  producer in the existing elimination-origin record; no new payload table or
  Core form is added. The source return worker waits for ordinary validation,
  synthesizes the input's result context with the retained binder, checks exact
  context agreement and then reuses the original producer for lexical scope.
  The clause body is still synthesized. `rule_premise` now exposes both ordinary
  and imported rule dependencies through the same helper, without inspecting
  imported jobs as if they had the ordinary job's inline premise layout.
  `pg_synthesis_restore_elimination` names the shared Match/Fold entry point.
  Tests cover exact source/Core retention, a valid but wrong-domain context,
  conflicting and late inputs, one-step Solve and inert resaving. The `fold`
  fixture is now in both typed-root and source-only `source_io.sh` loops as well
  as the full image-origin gate. Operation clauses and general checkpointing
  remain separate open requirements.
  Verification: normal `check check-image-origins check-prepared-modules`
  passes (including 758 module save boundaries); ASan/UBSan source and full
  synthesis suites pass. The fourteen typed/source retention combinations
  include Fold. The operation-origin gate remains REJECTED after restoration;
  this change does not claim full handler transport or full acceptance.
- [x] Retain operation producer identity, not only its function derivation.
  `source_io_test operation-origins` first checks a real `pg_synthesis_operation`
  producer and a handler using its alias. After three inert resaves, the
  handler fails although the ordinary value roots succeed. Operation lookup
  follows producer provenance to `OPERATION_JOB`; a function-evidence producer
  is not an operation declaration. Preserve the operation's label, checked
  signature inputs and producer kind through the same source dependency graph.
  Do not infer operation status from the Core of an arbitrary Lambda. A first
  attempted fixture incorrectly published a function proof as an operation;
  that already failed before saving and was not evidence of an image bug.
  Resolved after `00166c5`: `OPERATION_JOB` is keyed by the label and two
  signature producers. The accepted-declaration API supplies ordinary evidence
  producers; imported signatures remain pending. The same worker validates
  them with `pg_operation_declaration_at` before constructing the wrapper.
  `pg_synthesis_operation_at` retains its two binder allocations, not trusted
  field types. APGSRC26/27 encode operation producers separately from function
  proofs while sharing the constructor wrapper's physical context/reference
  table. Core forms and kernel rules remain separate and unchanged.
  The corrected `check-operation-origins` gate is part of `check-acceptance`.
  It now checks pending and settled producers through three inert resaves,
  ordinary handler inference and the actual normalized return value. Direct
  wrapper tests check exact Core reuse, rechecking of stored field annotations,
  wrong signature rejection, invalid labels, short allocations and conflicting
  late attachments. Foreign signature evidence is rejected at registration,
  rather than first constructing a job that later rejects it.
  Verification: normal `check`, image/operation-origin gates and all 758
  prepared-module save boundaries pass. ASan/UBSan source and full synthesis
  suites pass. The eight available 01-09 examples and selected runtime results
  pass; the open-family gate remains unchanged at 175/209/280/323 steps with
  only its closed control accepted. Full acceptance and Main push remain open.
- [ ] Complete retained allocation coverage for multi-clause source handlers.
  Operation producer transport does not retain the handler's own return,
  payload/resumption or continuation-type binders. The current operation gate
  verifies recomputed behavior and declaration identity, not exact whole-handler
  Core identity against a retained normalization receipt. Add that stronger
  gate and preserve the actual scope producers before claiming full retention.
  September 12: `check-handler-origins` is now a required acceptance gate.
  It compares independently re-synthesized source with the retained derivation
  after three destructive, inert resaves and one-step ordinary Solve. Both
  one-operation and two-operation handlers accept and normalize to the expected
  value. Initially exact Core and classifier identity failed for the return
  clause and every operation clause. This was an allocation-retention failure,
  not evidence that executing the handler gives a different result. The simple
  gate now passes; it must not be weakened to alpha equality or removed from
  full acceptance.
  Initial trace: `pg_synthesis_visit_source_allocations` only exported return-only
  Fold and recursive Match eliminations. `handler_return_step` allocates one
  binder; `handler_clause_step` allocates payload/resumption binders, and
  `clause_context` allocates a response-domain binder. `pg_prove_projection`
  preserves Core/classifier pointers and is not the source of this freshening.
  Next: retain the handler's actual clause-context producer edges, attach them
  before restored handler-scope jobs can run, and reuse allocation identities
  while independently synthesizing source bodies. Do not use the saved concrete
  carrier as an expected type: current handler inference collects clause effects
  through a symbolic carrier. Preserving binders alone is insufficient if scope
  interning loses the original context producer and nested application origins.
  No separate replay checker, new Core form, wire-version change or replacement
  of source synthesis with saved evidence is justified by this failure.
  Implemented: multi-clause handler origins share the existing rule transport;
  the return binder identifies the allocation site. Restore attaches the origin
  to the owning handler before any source job runs. Ordinary Solve validates
  the origin; return clauses reuse the checked context producer, and operation
  clauses reuse payload, resumption and response-domain binders while building
  their own contexts from the newly inferred symbolic carrier. The simple
  one/two-operation gate checks exact clause Core and classifier identity.
  Still open: `check-handler-nesting`, also required by full acceptance, uses
  `(\v:D=>k v) req` and `k ((\v:D=>v) req)` in an operation clause. These first
  exposed an independent source deadlock: completed definition producers were
  excluded from early named-term projection, so a known domain waited for the
  whole handler context, which waited for effect collection from that domain.
  The ready predicate now admits completed term evidence, but not unresolved
  definitions or namespace-only results. Both nested sources now synthesize and
  normalize correctly. Their operation-clause classifiers match after resave,
  but the nested Core still differs: re-created symbolic context producers do
  not recover the saved lexical allocation sites. Do not mark handler retention
  complete until this stronger gate passes without importing carrier answers.
  Verification: normal `check`, handler-origin and operation-origin gates pass;
  ASan/UBSan full source-image and synthesis suites pass. The nested gate fails
  only exact Core reuse in the first operation clause; both ordinary evaluation
  results and all compared clause classifiers agree. Main push remains open.
  Additional source-authority check: the same saved allocation input is attached
  to a changed valid clause (`req` instead of `k req`) and an invalid clause
  (`k k`). Ordinary Solve produces different Core for the valid change, rejects
  the invalid change, and rejects allocation attachment after source completion.
  Registration remains inert and repeatable. This prevents fixing retention by
  returning the stored handler proof instead of synthesizing the source body.
  Next representation change must preserve the source clause-context producer,
  not merely its accepted `CONTEXT_EXTEND` result. The existing environment
  record loses the owning handler/clause relation when it exports both handler
  binders as generic `CONTEXT_BINDING`. Reconstruct those contexts from the same
  pending clause builder used by source synthesis, with shared symbolic carrier
  inference. Do not ignore `context_job` in scope interning, alias unrelated
  typed scopes, or seed the new effect equation with the saved solved carrier.
  Preparation step: `pg_synthesis_operation_reference_input` now borrows the
  original payload/response producers through the same cycle-checked nominal
  origin traversal as declaration lookup. It does not create evidence or require
  a completed wrapper. Clause-context construction and the final handler rule
  connect these producers directly, instead of wrapping accepted signature
  results as new inputs. Alias/reference validation remains an independent
  required dependency of every accepted clause. Tests retain pending projection
  producers, verify exact input identity before/after Solve, reject foreign
  owners and failed signatures, and keep arbitrary functions from acquiring an
  operation signature. This is preparation for shared source clause-context
  reconstruction, not completion of `check-handler-nesting`.
  Handler label collection also reads these nominal inputs without waiting for
  signature acceptance; the clause's reference check and original signature
  premises still gate final evidence. Verification: the normal suite and 758
  prepared-module boundaries passed during this change; final ASan/UBSan full
  synthesis/source-image suites and handler-origin checks pass. Nested handler
  Core reuse remains the explicit failing acceptance gate.
  Shared builder prerequisite: `pg_synthesis_handler_context` exposes the exact
  ordinary context/Pi/Thunk dependency construction previously private to clause
  synthesis. It accepts pending outer context, carrier and signature producers
  plus three explicit binders, allocates no fresh binder, and creates no proof
  or effect solution at registration. The source clause worker uses this same
  builder. Tests inspect the resumption's symbolic classifier before row closure,
  then verify exact context ancestry, binder identity and job reuse after Solve;
  invalid ownership/registration inputs and a non-computation carrier are
  rejected. No new Core, proof or scheduler job tag is added. Source-image
  clause-site records and inert reconstruction through this builder are still
  the next step; the public builder alone does not repair nested retention.
  September 13 verification: normal synthesis and handler/operation-origin
  checks pass. Final ASan/UBSan synthesis, source-image and handler-origin
  suites pass, including wrong-carrier rejection. The nested gate still fails
  only exact Core sharing; this prerequisite does not complete full acceptance.
  Inert carrier reservation: `pg_synthesis_source_handler_carrier` now reserves
  the ordinary return-body and effect-equation producers before Solve. The
  handler worker uses the same construction, so loading need not invent a
  carrier checker or import a solved effect row. Return allocation origins can
  attach after reservation but before the return scope is built. Tests reserve
  before initial Solve and after three inert resaves; changed/invalid source
  also reserves before origin attachment. Missing/duplicate return clauses are
  rejected, while allocation failures retain the distinct error status.
  Verification: normal `check`, all 758 prepared-module save boundaries and
  handler/operation-origin gates pass. ASan/UBSan full synthesis, source-image
  and handler-origin suites pass.
  Nested handler retention still fails only exact Core reuse; connecting saved
  clause-site provenance to the shared pending context builder remains open.
- [x] September 13: repair the two `check-handler-nesting` fixtures without
  weakening exact Core comparison. APGSRC28/29 retain a handler-binding recipe:
  source owner/clause, nominal operation producer, three binders, prefix slot
  and optional raw allocation origin. This is a lexical construction input,
  not a saved carrier or a new Core/proof rule. The ten-word environment record
  references the ordinary producer DAG; binder references share the existing
  graph payload. Old source formats are rejected rather than reinterpreted.
  Both ordinary synthesis and inert reconstruction use the same clause-context
  builder. Source operation lookup must select the retained nominal producer;
  identical signature types do not authorize substituting another operation.
  This exposed two scheduling assumptions. A pending named definition must
  wake its consumers when ready, instead of leaving them waiting on their
  entire effect-dependent context. Stored implicit quotation is removed through
  one shared rule producer for atomic/qualified references and `::` checking.
  Also, application allocation selects its saved prefix by binder identity
  before row closure, but validates exact typed-prefix identity before accepting
  the source application. Inferring clause effects no longer waits on itself.
  Tests retain exact clause Core/classifiers through three destructive inert
  resaves and one-step Solve, check actual NF, reject invalid slot/clause/producer
  records, conflicting allocation inputs and changed operation identity.
  This completes these nested-Lambda allocation cases, not general checkpoint
  retention at every partially evaluated handler boundary or full N5 acceptance.
  Follow-up boundary coverage: `check-handler-nesting` also runs
  `handler-boundaries`. Two nested applications, an invalid resumption argument
  and an unhandled operation are saved at every ordinary Solve transition,
  read, inertly resaved, destroyed, reread and solved one step at a time.
  All 4,067 snapshots (1,214/1,317/692/844) preserve eventual acceptance or
  rejection, result classifiers and inferred effects. Pure cases return the
  expected constructor; forwarding preserves the remaining request and its
  continuation returns that constructor after an explicit response. This is
  RECOMPUTE equivalence, not a claim that interrupted Solve work is retained.
  The seed test now checks APGSRC28 and rejects the predecessor explicitly;
  its size-limit mutation restores the current header before testing counts.
  Verification: normal `check`, image/operation/handler-origin gates and 758
  module save boundaries pass. ASan/UBSan source-image, normalization, nested
  handler, 4,067 handler-boundary and seed tests pass. The eight available
  01-09 examples and six runtime-result fixtures pass. Required open-family
  acceptance is still 1/4 (closed 176; unsupported 210/281/324 transitions).
  Delta from `2ee9e13`: implementation +293/-76 (net +217), tests +176/-16
  (net +160), prototype build +1; documentation counted separately.
- [x] September 9: construct Match field/IH scopes through the same pending
  context-binding path (`SCOPE_CONTEXT_JOB`). `pg_synthesis_bind_hypothesis`
  records only the field-to-IH binder association; it introduces no proof or
  Core rule. The shared job waits for its parent context before validating the
  extension, and checks that an associated field belongs to that parent.
  Branch construction no longer interns IH scopes by bypassing this check.
  Tests create an IH over a still-pending field scope, use it through `*x`,
  compare the exact ordinary FORCE/variable Core, and reject an unknown field
  without changing the valid scope. This prepares the shared environment
  representation; source scope export/import is still outstanding above.
  Verification: normal and ASan/UBSan full `synthesis_test`, normal
  `source_io_test normalization` and `prepared-module` (758 boundaries) pass.
- [x] September 9: APGSRC13 (ordinary) / APGSRC14 (retained reductions) add
  context-backed source bindings. The existing eight-word environment record
  carries the ordinary context-extension rule and, for IH lookup, a reference
  to the field's source scope. Binder identity comes from that rule input, not
  another object table. The environment view exposes the same context producer
  used by `SCOPE_CONTEXT_JOB`; imports rebuild bindings through the shared
  context/Hypothesis APIs without Solve or accepted flags. Lexical ancestor
  collection follows these generated context scopes instead of requiring each
  to have a Lambda/Pi syntax allocation site. Existing derivation format remains
  APGDRV6; old source versions are rejected rather than reinterpreted.
  A permanent `source_io_test context-scopes` creates pending field/IH scopes,
  destroys and resaves three times without Solve, then checks `*x` produces the
  exact FORCE of the relocated IH binder in the retained field context.
  Typed-root Match saving/resaving succeeds again. The separate source-only
  constructor exact-input gate remains open; handler `effect_owner` transport
  and restoring source Match's own generated field allocation remain unfinished.
  Verification: normal and ASan/UBSan `context-scopes` pass; both source suites
  pass the typed-root Match loop and still fail the source-only constructor
  assertion. Normal full synthesis, normalization and prepared-module (758
  boundaries) pass. No complete-source/CHECKPOINT or Main-push claim is made.
- [x] Retained recursive-elimination derivation allocation (resolved by
  APGDRV6 below). Original failure:
  `retained-write-typed <file> match` followed by inert resaves and
  `retained-check <file>` still fails exact input identity (alpha-equal).
  Unlike the source-only regression, this survives selecting the retained
  typed root. `prove_data_elimination` allocates recursion via `pg_binder`,
  invokes fresh constructor/induction scope generation, and builds the recursive
  Core; `pg_prove_derivation` invokes that same function again. These internal
  allocations are not explicit saved rule inputs/premises. Thus a replay-only
  alternative is not the solution: the ordinary rule's construction inputs are
  incomplete for exact retained graph reconstruction. Expose its allocation
  recipe/field-context premises through the common rule representation and
  retain them with the graph; validate them with the same rule. Include the
  fixed-point lowering binders in `pg_data_recursive_match` in this audit.
  Do not infer correspondence by alpha-comparing the completed Terms. Also do
  not require fresh source re-elaboration to reproduce arbitrary fresh binders:
  completed checkpoints should continue the retained typed producer graph,
  while pending source preparation needs its own retained allocations. These
  remain distinct missing parts of N5, not an excuse to drop either gate.
- [x] Make fixed-point graph construction allocation-explicit:
  `pg_data_recursive_match` now takes recursion, argument and self-application
  binders from its caller. No second constructor API/tag is added. Repeating
  the same pointer inputs returns the exact same Core without growing the Term
  table. Invalid/non-distinct binders are rejected; lexical-capture, lazy-branch
  and recursive execution tests still pass. The ordinary elimination rule
  currently allocates argument/self itself, so this removes hidden allocation
  from the Core builder but does NOT yet preserve the elimination recipe across
  an image. Retaining all allocation inputs in the common derivation remains
  required, including generated field scopes.
  The direct `pg_prove_constructor_scope_at` now shares the ordinary scope
  construction function and retains only field binder identities from a supplied
  context. Prefix/arity are checked; field types are reconstructed by ordinary
  substitution lifting. Nat and dependent `(A : Type), (x : A)` tests preserve
  exact contexts/maps, including when supplied field-type annotations are wrong.
  Wrong field counts are rejected. Normal full synthesis and normal/ASan/UBSan
  IADT tests pass. Wiring this allocation input through induction and its saved
  derivation is still outstanding; no image-format change is claimed here.
  `pg_prove_induction_scope_at` now extends the same contract to field and IH
  allocations. Recursive-field classification is computed once from the
  declaration, then reused for suffix validation and IH formation. A retained
  suffix must contain exactly the fields followed by their IHs; field types and
  thunked motives are rebuilt with the ordinary constructors. Tests cover zero,
  one and two IHs, wrong suffix length, exact reconstructed maps and deliberately
  wrong retained IH annotations. The complete elimination rule still needs to
  retain these per-clause allocation contexts and its fixed-point binder triple.
  Verification: normal full `synthesis_test` and normal/ASan/UBSan `iadt_test`
  pass. Source retained-Match transport remains an open gate, not a passed test.
  The accepted induction derivation now retains a graph-owned
  `pg_induction_allocation` (three fixed-point binders and clause context
  references). `pg_prove_induction_at` passes these through the same elimination
  checker and scope reconstruction, copying the allocation list into the proof
  owner. An existing derivation keeps its first allocation; explicit conflicts
  are rejected rather than overwriting its Core. Fixed-point binders must be
  distinct and absent from destination/clause contexts to prevent capture.
  Tests rebuild a previously unseen scrutinee with the retained allocation and
  check exact sharing of the fixed-point function, rather than only exercising
  the proof cache. Conflicting/aliased binders and missing IH scope are rejected.
  Normal full synthesis and normal/ASan/UBSan IADT tests pass. The ordinary
  derivation input/codec does not yet carry this allocation, so the saved Match
  gate remains open until that connection and independent-process tests pass.
  Verification: normal and ASan/UBSan `iadt_test`, normal full
  `synthesis_test` and `source_io_test normalization` pass. Expanded
  `source_io.sh` passes retained typed constructor cases, then still fails
  source-only constructor input identity as documented above.
- [ ] Add source-facing retention selection and bounded checking/installation
  lifecycle and constructor/Match/IADT tests for APGSRC12. The new
  API is not yet a CLI CHECKPOINT mode. Pending evaluation/synthesis work and
  no-recomputation evidence remain separate incomplete requirements.
- [x] September 9: expose the accepted induction allocation in ordinary
  `pg_derivation_parameters`, dispatch through `pg_prove_induction_at`, and
  include the allocation input in synthesis job identity. Header extraction
  retains the actual allocation. Split-budget tests reject a conflicting
  allocation as a distinct failed request without disturbing the successful
  job. This is ordinary rule checking, not a separate Replay interpreter.
  Verification: normal and ASan/UBSan `iadt_test` and normal full
  `synthesis_test` pass. The new negative request is tested with budgets 1/64.
- [x] September 9: APGDRV6 retains induction allocation inputs. Each rule's
  eight existing term slots are followed by allocation metadata/root counts,
  context metadata and term references, then the ordinary premise IDs.
  Allocation uses `context_payload` plus the three fixed-point binder roots;
  all references share the existing Core relocation table. No Core tag or
  acceptance flag is added. APGDRV5 is rejected, not silently reinterpreted.
  Derivation/retained-image readers now require the destination `pg_typing`
  owner so raw contexts share its interning table with declaration contexts
  and later proof construction. Reading creates no proofs or evaluation work.
  Dependency collection uses the same packed input, including allocation
  contexts and binders, to preserve source declaration origins.
  Normal and ASan/UBSan `derivation_io.sh` pass, including split-budget ordinary
  checking, unchanged allocation identities and rejection of aliased binders.
  The typed-root `match` fixture now passes independent writer, two inert
  resavers, checked-NF reuse (zero new NF steps) and recomputation in both builds;
  it is included in the permanent `source_io.sh` typed-root loop.
  Normal `source_io_test normalization`, `prepared-module` (758 boundaries)
  and `identity_io_test` pass. Tests that read file prefixes now use actual file
  size instead of an 8192-byte ceiling, without reducing prefix coverage.
  Full `source_io.sh` still fails the distinct source-only constructor exact
  input assertion after the typed-root loop passes. Pending source allocation
  retention and no-recomputation WHNF evidence remain open. No Main push.
- [ ] Connect pending NF jobs and shared WHNF/NF jobs to this record ownership,
  then to source CHECKPOINT. The recomputation-mode checker does not preserve
  unfinished execution provenance or supply a no-recomputation WHNF basis.
  Raw archive round trips alone never authorize certificate publication.
- [ ] Complete the separately required symbolic type-family formation contract.
  Rechecking after `2074e1b` still gives closed/open/applied/sequenced statuses
  DONE/UNSUPPORTED/UNSUPPORTED/UNSUPPORTED at 175/209/280/323 steps. The current
  `type_input` deliberately requires RETURN inversion; an abstract family
  application does not supply that premise. Preserve that negative rule while
  implementing the explicit suspended/stable type-code contract described below.
- [x] Let scope-analysis work retain additional scope roots through its existing
  visit/shadow/scope ownership table. Its own embedded scope is the first root;
  references to it are rebound to the restored work's embedded address, not an
  independent copied scope. Distinct scopes remain distinct while binding-array
  sharing is preserved. The internal API returns these extra roots explicitly
  for the eventual frame/task owner, rather than dropping their identities.
- [x] Exercise all eight scope-analysis phases with repeated embedded-scope
  references, a distinct equal-content scope sharing its binding array, and a
  NULL scope. Retain those same roots across two destroying resaves and preserve
  the original evaluation result/charged steps. Malformed work clears additional
  outputs as well. Scope-analysis/frame composition is now selected by the
  common machine payload dispatcher above.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Give all twelve existing work descriptors owner-local versioned names
  (Force/field variants share algorithms but have distinct resume descriptors).
  Identity, Symmetry and Computation resolve only their existing descriptors;
  evaluator polling still uses the same pointers/functions. Unnamed local test
  work remains executable, not implicitly portable. No new Core tag or Replay.
- [x] Check name uniqueness, unknown versions and cross-owner rejection. In
  family task save/resume tests, retain the descriptor name and resolve it back
  to the original operation after arena destruction before registering work.
  This establishes descriptor identity only; policy/flags, payload dispatch and
  imported-progress acceptance still need whole-machine integration.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Allow structural-comparison payloads to delegate their final ordered Term
  roots to an owning callback. Standalone `pg_comparison_write/read` delegate to
  the same progress, binder-pair, pending-stack and validation implementation.
  This permits shared scope/binding ownership below comparison metadata without
  duplicating the comparison walker or its Term relocation table.
- [x] Test an owning scope around pending/equal/different comparisons at every
  step with two destroying resaves. Endpoint Lambda binders and scope-array
  binders remain identical; repeated references share. Preserve comparison
  status and exact resumed steps. An owner returning fewer roots is rejected
  without publishing a comparison. Normal `check check-prepared-modules`
  (758 save boundaries) and ASan/UBSan `check-identity-io check-eval-io` pass.
- [x] Use the comparison ownership callback in action-body work (`APGIBD2`).
  Remove the private source-binder-prefix encoding and its reconstruction loop;
  retain the actual scope and binding array through the ordinary shared-scope
  table, including prepared argument triples and already wrapped entries. Extra
  scopes retain aliases to the embedded scope's final address and shared arrays.
  Configuration-only wrappers delegate with no extra scopes and reject extras
  they cannot expose. This replaces experimental `APGIBD1`, not a stable image.
- [x] Test every body-work phase with prepared binding arrays, repeated embedded
  scope roots, a distinct scope sharing the same array and a NULL root. Two
  destroying resaves preserve aliases, initialized prefixes and final resumption.
  Existing malformed-header and configuration-failure cases still reject with
  empty outputs. Normal `check check-prepared-modules` (758 save boundaries)
  passes. After adding explicit prepared-triple and already-wrapped-entry checks,
  regular and ASan/UBSan `check-identity-io check-eval-io` were rerun and pass.
- [x] Select action-body work through the common frame/task payload owner.
  Source-image wiring and accepted-progress provenance remain outstanding.

- [x] Generalize named Demand-stack ownership through owner callbacks instead
  of hard-coding `action_result_work` into the frame serializer. The existing
  action-result API delegates to the same name resolution, frame payload and
  scope-order validation. Owners embed their versioned work and all frame
  scopes in one Term table; no alternative evaluator or scope codec is added.
- [x] Retain Force/field family-result tasks under actual parent Demand frames
  using `APGFRW1` and the common stack owner interface. Test all three result
  phases with two arena-destroying resaves; preserve final alpha equality and
  exact charged steps. Use a non-right-unit Fold continuation so the parent
  cannot disappear by eta contraction. Machine flags/policy and task descriptor
  registration remain manually restored by the test, not a complete checkpoint.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Retain the actual `scope_work` (`APGSAW1`): all eight phases, used/order
  arrays, reference-shadow cursor, optional head/result, source-index mappings
  and explicit pending/seen roots. Compose `APGSVS2`/`APGSHD2`/`APGISC3` so
  source bindings, visited Terms and caller roots share one relocation table.
  Restore address-keyed indexes from retained entries, never rescan the source.
- [x] At every actual scope-task suspension of a shared, shadowed expression
  with an unused source binder, resave twice after arena destruction and resume
  the original descriptor. All eight phases occur; final alpha equality and
  exact total steps match uninterrupted execution. Reject zero arity, invalid
  phase and out-of-range order entries with empty output handles. Reader bounds
  do not prove the saved state was reached from the source. Caller flags and
  descriptor selection still come from the test; full machine acceptance is open.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Compose visit/shadow transport with the existing scope ownership table
  (`APGSVS2` / `APGSHD2` embedding `APGISC3`). Do not serialize scope binders
  in a second Term table. Restore optional scope roots only after the outer
  visit/shadow payload also validates. This supersedes the experimental v1
  envelopes; no stable artifact contract is changed.
- [x] Resave visits, shadows and multiple scope roots together twice after
  arena destruction. Distinct scopes retain a shared binding array; repeated
  scopes share their actual scope object; binding sources/triples and shadow
  binders refer to the same relocated objects. Existing empty-scope and invalid
  graph cases pass. This ownership layer is used by `APGSAW1` above.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Retain raw scope visits (`APGSVS1`) with next-list identity and shared
  shadows through `APGSHD1`, using one Term table. Repeated root pointers share;
  distinct pending records with equal `(term, shadow)` remain distinct records.
  Do not infer seen membership from the serialized visit graph or deduplicate
  pending work. Hash buckets are absent; the owning scope restores seen keys.
- [x] Resave visits twice after destroying source arenas, preserving shared
  next tails, shared/distinct shadows and extra binder Term roots. Reconstructed
  seen indexes accept distinct shadow keys and reject duplicate logical keys.
  Out-of-range roots and cyclic next IDs clear all published outputs.
  Full scope-work phases, source-index records, order/used arrays and explicit
  seen/pending selection are connected by `APGSAW1`; this is not a full checkpoint.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Retain scope shadow forests (`APGSHD1`) through the existing pointer-keyed
  DAG collector and Term relocation table. Preserve NULL roots, repeated roots,
  shared parent tails and distinct nodes with identical binder/parent contents.
  Parents precede children in the stream; reject cycles, invalid references,
  unreachable records and non-binder payloads. No alpha/content interning.
- [x] Resave a 2048-level shadow forest twice after destroying source arenas.
  Verify sharing, distinct identities and binder sharing with an extra Lambda
  root; reject out-of-range roots, cyclic parent IDs and a cyclic input graph.
  This is a scope-analysis ownership component, not complete scope-work or
  evaluator checkpoint transport. `APGSAW1` now connects visit entries, pending
  order and work arrays to the same table.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Expose the actual scope-analysis work and its original descriptor to
  transport. Share `(term, shadow)` lookup between scheduling and visitation;
  retain their different insertion timing. Restore source/visited indexes from
  retained entries without traversing source Terms. Pending entries recompute
  their hash on visitation, so no source-address hash needs serialization.
- [x] At each scope-analysis suspension of a shared, shadowed expression,
  rebuild both indexes twice after discarding buckets and cached hashes.
  Original resumption preserves final alpha equality and charged steps.
  Distinct shadows remain distinct keys; duplicate logical keys fail with empty
  indexes. This tests index reconstruction, not pointer relocation or checkpoint
  admission. `APGSAW1` above now transports the scope/shadow/visit graph and arrays.
  Normal `check check-prepared-modules` (758 save boundaries) and ASan/UBSan
  `check-identity-io check-eval-io` pass.

- [x] Retain raw Thunk-family discovery (`APGFSW2`, originally `APGFSW1`) in the actual
  `family_scope_work`, shared by Force and field resumptions. Source/body,
  optional content/value, cursor and supplied/discovered counts use one Term
  table with caller roots. No binding preparation or source rescan on import.
  Both original polling descriptors pass every discovery boundary with two
  arena-destroying resaves; reject inconsistent counts without publishing work.
  This is a polling-state test, not a full machine-resumption test.
  Normal and ASan/UBSan `check-identity-io check-eval-io` pass; so does
  `check check-prepared-modules` (758 module-save boundaries).
- [x] Put the embedded family scope and extra scope roots into the existing
  APGISC3 ownership table. Remove the private source/body/count encoding;
  retain a presence mask for discovery's initially absent endpoints using
  checked cursor placeholders. Rebind external aliases to the restored embedded
  scope, preserving repeated roots, distinct scopes and null roots. Both Force
  and field polling tests cover every boundary with two destroying resaves;
  existing actual-evaluator resumptions retain exact total steps. APGFSW2 is
  an experimental format revision, not an accepted stable checkpoint schema.
  Verification: `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass, including invalid supplied-arity rejection.
- [x] Compose Thunk-family state with the existing configuration forest and
  exercise both original resume callbacks at every actual discovery suspension.
  Dependent (not constant-folded) families reach both discovery stages. Two
  arena-destroying resaves preserve final alpha equality and exact total steps;
  the field continuation borrows the restored embedded scope successfully.
  These are erased transport fixtures, not admission of a free type relation.
  Normal and ASan/UBSan `check-identity-io check-eval-io` pass.
- [x] Connect Thunk-family work to common machine caller/frame ownership.
  Additional common-reader tests restore all flags and select the descriptor
  from the envelope, including real parent frames. They do not validate provenance.
- [x] Retain family-result construction (`APGFRW1`) using the actual
  `family_result_work` and nested `action_result_work`. Reuse `APGISC3` for
  optional scopes and shared binding-array ownership, not a duplicate binding
  format. Retain collected argument suffixes during collection and the complete
  array during wrapping/application, plus the remaining family spine and phase.
- [x] At every actual Force/field result-task suspension, save twice, destroy
  arenas, and resume through the original descriptor. All three phases occur;
  final alpha equality and exact total steps match uninterrupted evaluation.
  Empty terminal work round-trips; invalid arity, position and phase clear
  output handles. Normal and ASan/UBSan component tests pass. These remain raw
  progress tests, not source-level checkpoint acceptance or a provenance proof.
  `check check-prepared-modules` also passes (758 module-save boundaries).

- [x] Retain raw action-scope discovery (`APGASW1`) through the original
  `action_scope_work` and work descriptor. Source/body/cursor and the actual
  remaining argument-tail pointer share the caller's configuration forest;
  preserve discovered count, position and selected boundary. This phase has no
  prepared binding array; enforce that invariant rather than making another
  scope representation or inferring a cursor by rescanning on import.
- [x] At every scope-discovery cut of a two-argument action, destroy arenas and
  resave twice. Verify shared caller/source and exact argument-tail pointers,
  final value and total steps; reject a boundary offset outside triple alignment.
  Structural shape checks do not prove the imported cursor's history. Machine
  flags and descriptor registration still come from the test, not a complete
  checkpoint loader. Normal/ASan/UBSan component tests and
  `check check-prepared-modules` pass.

- [x] Retain raw higher-scope construction (`APGHSC2`, originally `APGHSC1`) using the existing
  `higher_scope_work` and original work descriptor. Preserve source/cursor,
  optional partial body, arity, Lambda count, position, collection/wrapping
  flags and all generated binders (including those already wrapped). Extra
  caller roots share one Term table. Restore no absent binder by recomputation.
- [x] Save every active higher-scope cut of a doubly acted identity application,
  destroy arenas and resave twice. All four stages occur; caller/source sharing,
  exact final value and total steps are preserved. Invalid arity, cursor
  position and flags clear output handles. This is raw task transport, not
  proof of its partial computation or whole-machine admission. Normal/ASan/UBSan
  component tests and `check check-prepared-modules` pass.
- [x] Include additional frame-scope roots through the existing APGISC3 owner
  inside APGHSC2, replacing the standalone terminal Term table. Migrate the
  internal callers rather than keeping a second format implementation. Extend
  all higher-scope task cuts with repeated/distinct/null scope roots, shared
  binding storage and references to generated binders. After two destroying
  resaves, check exact aliases, source/cursor sharing and unchanged evaluation.
  This experimental format revision does not assert stable artifact compatibility.
  Verification: `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass with the shared-scope cases.
- [x] Integrate higher-scope ownership into the common machine dispatcher.
  Common-reader tests no longer supply machine flags or the work descriptor;
  CHECKPOINT acceptance and retained-progress provenance remain outstanding.
- [x] Extend the same ownership table with the evaluator's optional live
  `action_result_work` (`APGISC3`), not a second binding codec or fictitious
  scope. Retain partial result, remaining/discard counts and the exact array
  edge. Scope/result references jointly determine retained array length; a
  consumed result prefix still aliases a longer scope array. Scope-only APIs
  delegate to this implementation and reject payloads containing an unrequested
  result task instead of silently dropping it.
- [x] Resave shared scope/result ownership twice between original result polls,
  including discard, wrapping and completion. Check shared arrays even when
  the remaining count is zero, and evaluate the completed Lambda with distinct
  arguments. Also retain a result without scopes and reject a dangling result
  array reference. This is a raw owner/polling test; connecting the task to a
  complete machine image with all other owners remains required. Normal and
  ASan/UBSan component tests pass, as does `check check-prepared-modules`;
  the final result-only and dangling-reference cases were rechecked in both
  component builds after that full run. `APGISC3` supersedes experimental v2.
- [x] Connect the optional action-result task to the same named-continuation
  stack API (`APGCON2`). Continuation scopes and the result task now enter the
  same ownership table, with frame/readback roots in its single Term table.
  Update existing callers directly; no compatibility wrapper or parallel stack
  encoder. Failure clears both frame and task output handles.
- [x] Retain a real action-result task under a waiting Fold at every task cut,
  resave twice after arena destruction and resume with the existing evaluator.
  Final value and total steps agree. The synthetic eleven-continuation fixture
  also checks task/continuation-scope array aliasing through the combined codec.
  Machine flags, policy and task registration are still restored by the test;
  other task kinds and complete source-level checkpoints remain incomplete.
  Normal/ASan/UBSan component tests and `check check-prepared-modules` pass.

- [x] Retain raw symmetry composition (`APGSYM1`) using the original
  `composition_work` and its polling/resumption descriptor. Original outer and
  inner owners, argument and caller roots share one Term table; retain the
  already-computed axis prefix and position without rerunning composition.
  An allocation length precedes the prefix and is checked against the maximum
  restored owner dimension. The Term table remains last, preserving its existing
  complete-stream validation; no second array copy or second graph decoder.
- [x] Let the symmetry-composition Term owner include shared scope metadata
  before the final graph table. The standalone APGSYM1 codec delegates to the
  same read/write implementation and descriptor adapters as Fold. Extend every
  cut/resave test with scope references to the operator and argument; require
  exact relocated pointer sharing and unchanged prefix, result and step count.
  Reject missing owner roots with cleared task/configuration outputs. The test
  scope owner is shared with Fold rather than duplicated by task kind.
  This is raw relocation, not acceptance of a saved axis prefix; production
  whole-machine task dispatch is provided by `machine_io.c` above.
  Verification: `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass after these changes.
- [x] At every task cut of a 3D reversal composed with itself, destroy arenas
  and resave twice; preserve owner/caller/argument sharing and the exact prefix.
  Resumption uses the original evaluator and produces the identical value with
  the same total steps. Reject mismatched allocation length, invalid position
  and out-of-range prefix coordinates with cleared output handles. This retains
  raw progress, not proof of a correct prefix. Whole-machine integration remains
  open; prefix-removal transport is covered below. Normal/ASan/UBSan component tests and
  `check check-prepared-modules` pass. The separately rerun open-family gate
  still fails three of four fixtures at 209/280/323 steps; no acceptance waiver.
- [x] Retain fixed-prefix removal (`APGPRF1`) through the existing `prefix_work`
  and polling/resumption descriptor. Its captured argument and caller use one
  configuration forest, preserving environment identity rather than reducing
  or flattening the closure. The scalar axis-prefix codec is shared with
  composition. Read directly into final arena-owned storage and check capacity
  against the restored owner's non-fixed dimension.
- [x] Exercise every prefix-task cut of a captured-argument example, resaving
  twice after arena destruction. Check caller/argument environment sharing,
  partial coordinates, exact final value and unchanged total steps. Reject an
  allocation length inconsistent with its owner. Normal/ASan/UBSan component
  tests and `check check-prepared-modules` pass.
  Surrounding machine flags and descriptor selection still come from the test;
  this is not whole-machine checkpoint admission.

- [x] Share the existing `fold_work` layout between computation and transport
  through `computation_internal.h`; expose its original work-operation descriptor.
  Move the existing evaluator task layout to `eval_internal.h` without changing
  dispatch, polling, resumption or destruction. No new execution representation.
- [x] Retain raw Fold construction progress (`APGFLD1`): phase, clause count and
  selection, position, head/resumption/payload/label, initialized binder prefix,
  and the partial result and response binder once created. Extra owner roots
  use one descriptor table. Reading relocates these pointers without generating
  missing binders or replaying construction. Invalid phase/index/position and
  non-binder entries are rejected; successful raw transport is not evidence.
- [x] Compose real suspended Fold work with caller configurations through the
  existing owner callbacks. At every active-task cut, destroy both arenas and
  resave twice. Both clause selections preserve exact returned pointers and
  total steps; zero-clause forwarding preserves label/payload and resumption
  behavior. All four construction phases are covered. Surrounding machine flags
  and operation selection are still supplied by the test, not a whole-machine
  checkpoint API. Normal/ASan/UBSan `check-identity-io check-eval-io` and
  `check check-prepared-modules` pass after the forwarding and rejection cases.
- [x] Route Fold's final ordered Term roots through an owner callback, so frame
  scopes can share their relocation table. The standalone APGFLD1 functions
  delegate to the same implementation; no second task representation or changed
  reduction rule. Extend every active-task cut above with a scope referencing
  the work head, payload and initialized binder. Destroy/resave twice and check
  exact pointer sharing, unchanged answers and step totals. Reject an owner
  which returns too few roots without exposing a restored work object.
  Verification: `check check-prepared-modules` and ASan/UBSan
  `check-identity-io check-eval-io` pass, including the owner rejection case.
- [x] Wire this Fold/scopes composition into the common machine task dispatcher,
  together with the other eleven work descriptors. Its tests restore machine
  flags through the reader; saved-progress provenance and complete CHECKPOINT
  acceptance remain outstanding.

| Algorithm (`*.c`) | References and progress beyond its descriptor |
| --- | --- |
| Fold (`computation`) | Head, label, payload, resumption, binder array, generated partial term and construction phase. |
| Symmetry composition (`symmetry`) | Outer/inner semantic owners, argument, partially filled axes and position. |
| Symmetry prefix (`symmetry`) | Owner, captured argument closure, partially filled axes and position. |
| Action scope (`identity`) | Source/body scope, argument cursor, source cursor, count and selected boundary. |
| Action result (`identity`) | Binding array, partially wrapped result, remaining bindings and discarded arguments. |
| Scope analysis (`identity`) | `scope_visit`/`scope_shadow` DAG, pending order, source-binding index, used/order arrays, reference cursor and construction phase. |
| Action body (`identity`) | A live `pg_comparison`, in addition to scope, answer, cursor and wrap phase. Its comparison is structural (`normalize == NULL`), not another evaluator. |
| Higher scope (`identity`) | Source/cursor/body, fresh binders, arity, collection/wrapping position and phase. |
| Family scope (`identity`) | Source/body scope, family cursor, discovered content, value and supplied count; force/field continuation belongs to the descriptor. |
| Family result (`identity`) | Embedded action-result state, family cursor, collected arguments and collect/wrap/apply phase; force/field continuation belongs to the descriptor. |

Implementation order for the remaining checkpoint work:

1. Add graph-reference relocation for environment/argument links and shared
   readback/comparison work. In `graph.c`, comparison includes `alpha_entry`
   stages, normalized endpoints, binder-pair scope/cursor and pending order.
   Retain logical entries; rebuild hash buckets from relocated pointer keys.
   Bucket placement and arena addresses are not semantic state or evidence.
2. Let the existing owning modules encode/restore their work payloads against
   that relocation context. Reuse existing Term/semantic-owner codecs; do not
   duplicate their payload formats in an evaluator-specific Term codec.
   Graph allocator pointers are rebound to the destination owners, never
   written as addresses. Keep shared binder identity across all payloads.
3. Connect the resulting graph to source-image producer roots and the existing
   scheduler. A source image saved before Solve, during materialization and
   during each auxiliary algorithm must remain resumable through the same
   transition functions. Omitting any of the above work is RECOMPUTE for that
   component, not a completed CHECKPOINT implementation.
4. Keep untrusted imported progress separate from accepted computation facts:
   pointer relocation, index reconstruction and cursor bounds do not establish
   reachability from the original input. Complete the retained reduction basis
   and validate its prerequisites through ordinary computation/rule operations
   before a restored result can issue accepted normalization evidence. Do not
   expose a cache-seeding API that trusts restored `status` or `target` fields.

Verification must compare fresh-process results and charged work, not just
readback equivalence. Include shared environment tails, suspended fresh-binder
readback, partial argument-prefix reconstruction, structural comparison inside
Identity and an unsolved save immediately after restore. This audit changes
the next implementation boundary; it does not mark any checkpoint gate done.

### September 9: Retaining Normalization Requests

Code inspection found that REPL normalization requests are not selected roots
when saving, and pending WHNF/NF jobs cannot currently be exported by the rule
codec. A completed normalization proof describes a claimed endpoint; an
unfinished normalization request has no endpoint yet. Do not invent a target
or weaken endpoint validation to serialize the latter as the former.

- [x] Use the existing WHNF/NF job roles with immutable context/term producer
  operands. Accepted-evidence convenience APIs use the same factories through
  evidence producers. No new reduction engine, role or acceptance table.
- [x] Wait for both premises, propagate their failure, and verify their context
  before starting reduction. Validate this input once, not on every reduction
  step. Expose immutable operands for the source-image producer codec.
- [x] Add tests for pending source/rule premises, request reuse, WHNF/NF
  distinction, invalid context rejection, and agreement with accepted inputs.
- [x] Extend source-image producer records to represent these requests with
  context/term producer edges and a reduction mode. Restore through the same
  factory without Solve; retain unknown endpoints as unknown.
- [x] Retain created CLI/REPL normalization requests alongside source roots, preserving
  selected-module behavior. Test save before/through/after normalization and
  unsolved resave, including invalid premises and split budgets.
- [x] Permit a named CLI/REPL request before the source producer is accepted.
  Select a whole-module-checked named producer without Solve, then decide
  one-time forcing after its classifier is accepted. Zero-budget invocations
  retain `--nf NAME`/`--whnf NAME`, rather than saving only the source root.
- [ ] Retain reusable evaluator progress/results for full CHECKPOINT support.
  Merely retaining request recipes is still RECOMPUTE, not work retention.

Verification: the full normal `check` passes after the producer-based request
change, including source/image, CLI/REPL, Identity and IADT regressions. The
focused `program_test` passes with ASan/UBSan. No image format change or full
checkpoint completion is claimed by this preparation step.

Subsequent codec work uses APGSRC10. In a normalization producer's existing
six-word record, scope is zero, the syntax slot is mode 1 (WHNF) or 2 (NF),
definition/rule slots are zero, and left/right are the context/term producer
IDs. Both are prior dependencies. This encoding does not allocate an AST or
fabricate a derivation endpoint. Other modes and mixed record fields reject.
APGSRC9 is not silently interpreted under the new format.

`source_io_test normalization`, registered in `check`, saves at every Solve
step through completion, reloads and resaves before Solve, then uses budgets
1/64. It checks root aliases, modes, valid results, invalid context rejection,
and rejects an invalid serialized mode. This is request retention only; the
CLI connection below now includes created normalization jobs in its saved root set.
Verification after the codec change: `check check-prepared-modules` passes;
the focused normalization-image test also passes with ASan/UBSan. An initial
test incorrectly assumed unchanged normalization always allocates a dedicated
normalization proof. The existing rule correctly reuses the original evidence;
the corrected test checks mode on the retained request, not a nonexistent receipt.

CLI connection: one ordered root list now holds loaded roots, incremental source
roots and created normalization requests. Batch `--save` and REPL `:save` use
that same list. A request does not change the selected source module; `:root`
can select its status/resume target explicitly. Duplicate selections may alias
the same interned job, without allocating a second solver state. Tests save
pending and completed NF requests, resave without Solve in a separate process,
resume root 2, return to source root 1, and append/load another source root.
This retains inputs, not evaluator progress, and does not close N5.
The full normal `check` and the expanded REPL test under ASan/UBSan pass after
this driver change. Main promotion remains gated by the original N0-N7 scope.

Pre-synthesis request connection: normalization retains an optional closed
one-time thunk demand using the same WHNF/NF roles and evaluator. Context and
subject premises are checked before this demand; already-raw computations and
non-thunk values remain unchanged. `pg_program_normalize` and named requests
share that path. No speculative classifier, new Core tag or expected type is
introduced. APGSRC11 adds modes 3/4 for demanded WHNF/NF to the existing record;
modes 1/2 retain raw normalization. Earlier formats are rejected explicitly.
The expanded image fixture checks all four modes at each save boundary, and
the CLI fixture covers source budget zero, separate-process root-2 resume,
REPL budget zero, and rejection of a good selection with a bad module sibling.
Verification: `check check-prepared-modules` passes after this connection;
the four-mode image boundary test and expanded REPL test pass with ASan/UBSan.
The producer-based demand does not retain evaluator transitions or establish
open-family admission, general Acc/IF8, or completion of the higher theory.

### September 9: Reusing Prepared Public Scopes

- [x] Intern the driver's derived public scope by exact
  `(parent_scope*, source_scope*, definitions_syntax*)`, using the existing
  hash index. Repeated REPL root selection must not recreate selection ASTs
  and pending jobs. Entries contain no classifier, acceptance or solver state.
- [x] Add regression coverage for repeated requests before/after Solve,
  distinct parents/modules, and foreign Program rejection.
- [x] Verify the complete normal suite and prepared-module image regressions
  after this change.

`make -s -f src/prototype/pointer/Makefile check check-prepared-modules`
passed, including CLI import/REPL and all 758 prepared-module save boundaries.
The focused `program_test` also passed with ASan/UBSan after rebuilding.
This is regression evidence for current coverage, not completion of N0-N7.

This index is derived preparation, not another artifact authority. Its selected
producers remain ordinary whole-module-checked synthesis jobs. Source images
retain those ordinary dependencies; the driver index itself is not serialized.
Loading and source compilation continue through the same Solve/rule checks,
not a separate Replay verifier. Reusing solved inputs does not mean trusting
serialized acceptance flags. Full checkpoint work retention remains unfinished.

### September 9: Open-Family Admission Before Further Checkpoint Expansion

Independent N5 driver progress: `pointer-check --imports FILE.p INPUT.p` now
parses an explicit source provider in the same Program without advancing Solve.
Its local assignments become pending selected-module producers in the existing
hidden import scope. Source `import name;` uses ordinary lookup and whole-module
checking; provider symbols are not implicitly lexically visible. Saving the
client retains those scope/producer dependencies using APGSRC9. There is no
second verifier or copied classifier table. `tests/import_cli.sh` covers repeated
imports, hidden names, missing exports/files, zero-budget save/load, invalid
provider siblings and conflicting options. Filesystem search, recursive provider
configuration, multiple-provider ambiguity, imported image merging, and REPL
are still missing; this explicit single-provider route does not complete N5.
Provider syntax errors report their own file/line/column and exit 1; I/O or
preparation failures exit 2. A nominal Bool export is normalized, saved, then
loaded and normalized after deleting its provider source; the resulting Core
prints agree. The expanded import CLI test passes normally and with ASan/UBSan.
The full `check` passed before this diagnostic/test extension; the ordinary CLI
suite also passes after it. No Main promotion or full-import completion is claimed.

REPL preparation: CLI WHNF/NF now uses `pg_program_normalize`, which requests
the existing pure normalization jobs from closed accepted evidence and forces a
stored thunk once. It does not advance Solve or add another evaluation state.
Program tests cover pending request creation, repeated request sharing, foreign
and null evidence rejection, and the forced Lambda result. `program_test`,
`tests/cli.sh` and `tests/import_cli.sh` pass.

The first `--repl INPUT` loop now supports `:solve [N]`, `:status`, `:root N`, `:whnf NAME`,
`:nf NAME`, `:save FILE.a`, and `:quit` against the same loaded Program. Commands
use common normalization and image-writing paths. Save retains every initial
image root, not just the selected one; it does not preserve effect execution.
`tests/repl.sh` verifies batch/interactive NF agreement, pending resume/save and
recovery after invalid commands. Root switching tests select a rejected root,
reject out-of-range indices without changing selection, and preserve all eight
fixture roots when resaving. Source submissions now append one parsed module
per line. The selected source root determines the lexical scope for the next
submission; `:root` therefore also selects the branch of declaration history.
History is a growable array of ordinary root pointers, not copied solver states.
Syntax errors do not append a root. Rejected and pending parsed modules remain
in history and are preserved by save. Multiline input remains missing.
The command REPL passed the full normal `check` before root switching; the
expanded root-switching script and Program API tests pass with ASan/UBSan.
The normalization API also rejects accepted evidence in an open context.

Incremental-scope preparation: `pg_program_exports` now contains the source
assignment publication previously local to the import driver. It builds NAME
scopes whose producers are whole-module-checked selections; no classifier or
accepted-state copy is made. The import driver uses this same helper. Program
tests publish a pending `id`, parse a later `copy:=id;` in the resulting scope,
and confirm shared accepted evidence after ordinary Solve. `program_test` and
the import CLI suite pass. The REPL now uses this same helper. Tests append an
alias, recover from a syntax error, save/reload and append another alias, retain
three entirely unsolved roots and normalize the last one after loading, and
confirm a rejected appended root is not discarded when selecting an older root.

Post-check follow-up: module `::` resolution now uses ordinary lexical producer
lookup after local indexing, rather than only the current block's definition
table. This permits a later REPL submission to check a prior named producer.
Local declarations still shadow outer names and hidden import configuration
does not become visible. The expected type remains a separate SOURCE_EXPECT
input; no expected classifier is passed to the original producer. REPL tests
check a valid earlier `id`, reload that check from an image, reject `id::@`,
then select the original root and normalize `id` unchanged. Binder-only outer
references without a named producer are not newly supported by this change.
Verification after the lexical post-check change: the complete normal `check`
and `check-prepared-modules` pass, including 758 save-boundary snapshots.

Recheck after `9639429`: the four family fixtures still report 175/209/280/323
steps and only the closed control succeeds. Prepared-input persistence did not
change admission. The following is a proposed typed-layer contract, not an
implemented rule or a soundness claim:

1. A symbolic type result must have an explicit universe and a computation
   premise in its original context. Denote its proposed interpretation by
   `Decode(M)`, for `M : F({}, Universe_i)`. This notation is not a request for
   a new Core tag, nor permission to extract arbitrary values from computations.
2. Required closed equation: `Decode(RETURN(A)) = A`. Required substitution
   equation: `Decode(M)[sigma] = Decode(M[sigma])`. Neither equation identifies
   `M` itself with a value. Whether the second equation is definitional or has
   explicit object evidence must be fixed before adding a kernel rule.
3. A pure-row premise alone does not establish a total universe-valued result.
   The design must specify whether decoding requires stable/total evidence or
   denotes a suspended type with restricted elimination. In the latter case,
   formation must not silently provide inhabitants or an inverse of RETURN.
4. For `T := M; N`, checking `N` under `T : Universe_i` is only local checking.
   If its classifier is `C(T)`, the block cannot publish `C(T)` in the outer
   context. A proposed suspended classifier `SequenceType(M, T.C(T))` needs
   formation, substitution and `SequenceType(RETURN(A), T.C(T)) = C(A)` rules.
   It is not justified by changing the independence test in `pg_prove_fold`.
5. Act must act on the computation premise and family together, with boundaries
   agreeing after substitution. Applying ordinary value action to the Core of
   `M` is not such a derivation. Retained inputs must reconstruct the same rule.

Implementation order: settle (3), prove the closed/substitution equations in
the chosen interpretation, then add evidence constructors and their derivation
input cases. Only then connect `type_input`/block synthesis. Tests must include
closed family substitution, dependent block escape, nonempty/unknown effects,
and dimensional boundary agreement. Keep both the positive source acceptance
gate and existing neutral RETURN-inversion rejection tests; passing one by
weakening the other is not completion.

Regression added to `tests/synthesis.c`: an open forced thunk of a pure
Universe-returning computation is rejected by both value/type conversion APIs
and RETURN inversion. A closed RETURN of a type code is likewise not itself a
type, but its extracted value is. The complete synthesis test passes. These
checks constrain existing APIs; they do not forbid a separately justified
suspended type representation or claim open-family admission is implemented.

Code-level constraint found during that review: `pg_prove_value_type` accepts a
Universe-classified VALUE, while `pg_prove_type_value` turns any VALUE_TYPE back
into a VALUE with the *same subject Core*. Consequently a new formation rule
that simply labels the computation Core `M` as VALUE_TYPE also makes that same
computation a Universe value through the existing inverse view. Restricting the
new entry point to type positions would not prevent this escape. Do not implement
symbolic type admission by relabelling the computation occurrence.

If suspended decoding is chosen, its Core must represent the suspended *type
code*, distinct from executing `M`, using the existing reference/application
representation. Its closed reduction must be explicit, never an interning rule.
This does not require a second Lambda/Pi graph or a new Core node kind. It does
require handling the code in conversion, typed substitution, dimensional action
and descriptor transport before exposing it to source synthesis.

In particular, do not add `RETURN(code(Decode(M))) = M` as an unrestricted
computation equation. In an interpretation permitting pure divergence, the left
can return a suspended code while the right diverges. Existing erased-Core
tests in `tests/core.c` demonstrate the suspension/execution distinction with
`quoted_omega` and its FORCE; they do not prove that omega has a type here.
The desired equation `Decode(RETURN(A)) = A` is a one-way decoding computation
law, not justification for this stronger inverse law. Universe observability
and Act coherence of the suspended code remain proof obligations; an empty-row
test supplies neither. No such formation rule has been added yet.

Additional literature checked on September 9:

- Matthijs Vakar, *An Effectful Treatment of Dependent Types* (2016),
  [abstract and paper](https://arxiv.org/abs/1603.04298). The abstract explicitly
  distinguishes dCBPV- from dependent Kleisli extension and notes extra subtyping
  conditions for some effects. It is not a theorem about this kernel.
- *ANF preserves dependent types up to extensional equality*, JFP 32 (2022),
  [published paper](https://www.cambridge.org/core/journals/journal-of-functional-programming/article/anf-preserves-dependent-types-up-to-extensional-equality/73FC888A23E5E87BAE16B158ABE349C8).
  This is relevant to naming computation results during lowering, but its
  extensional equality target must not be silently imported as equality
  reflection in A Program. The contract above remains our design obligation.

Audit at `ce1c35f`: `check-open-families` still fails with `unsupported
steps=209`. Debugging locates the first failure in `contents_step`, requested
by `domain_step` through `type_input`. The application `F A` has computation
typing, but its open head cannot reduce to `RETURN(value)`.
`pg_prove_return_value` correctly requires that introduction shape; weakening
this rule would silently identify computations with their returned values.
The closed application `(\B : @ => B) A` succeeds at 175 steps.

This is not an image relocation or replay defect. The required open-family
fixture remains a failing acceptance gate, not an optional unsupported feature.

Follow-up to `75013f2`: `check-open-families` now checks four required source
cases, without an expected-failure exemption:

| Source fixture | Observed outcome | Required distinction |
| --- | --- | --- |
| `closed-family.p` | DONE, 175 steps | Closed type computation already exposes RETURN |
| `open-family.p` | UNSUPPORTED, 209 steps | An abstract family needs symbolic type use |
| `applied-open-family.p` | UNSUPPORTED, 280 steps | Supplying a closed family does not bypass checking the open function body |
| `sequenced-open-family.p` | UNSUPPORTED, 323 steps | Naming the future result in a block does not make dependent sequencing available |

The last case has `T := F A; \x:T => x` in the block. The corresponding
internal example already exists in `tests/synthesis.c`. Its UNSUPPORTED assertion
records the current limitation, not a permanent language prohibition. The new
acceptance fixture deliberately requires success so this gap cannot disappear
behind a green component test.

Two connected rule boundaries must be solved, not patched independently:

- `synthesis.c:type_input` requests canonical result evidence before admitting a
  domain. `evidence.c:pg_prove_return_value` is constructor inversion, not a
  general operation for observing a neutral computation's result.
- `evidence.c:pg_prove_fold` requires `pg_pi_constant_codomain`. If the block body
  has classifier `Pi(T, F T)` depending on the preceding bound result `T`, ordinary
  nondependent fold cannot give the entire block that classifier in the outer
  context. A fresh result binder must not escape without a typing rule.

Theory check: P. M. Pedrot and N. Tabareau, *The Fire Triangle: How to Mix
Substitution, Dependent Elimination, and Effects*, POPL 2020, sections 3.5 and 7,
[author PDF](https://www.xn--pdrot-bsa.fr/articles/dcbpv.pdf), accessed 2026-09-09.
Their thunkability is a semantic compatibility property, not the act of wrapping
a term in `thunk`. Figure 8 uses an additional judgement and type constructions;
section 7 explicitly discusses extensional target theories and coherence
difficulties for an intensional treatment. It therefore does not license an
empty-row check followed by unconditional result extraction in this kernel.

A Program-specific conclusion: a stronger stable-family contract is one route
to value-type use, but is not the only possible language design. Suspending a
dependent computation type is another route and needs its own formation and
substitution rules; it cannot be implemented by placing a free result variable
in the existing fold conclusion. Before selecting either route, write the
application/closed-substitution equations and Act obligations together. Neither
route may introduce equality reflection, a second value-side Lambda/APP Core,
or an unstated totality promise on all existing pure arrows. These requirements
refine the checklist below; no such new rule is claimed implemented here.

Implementation follow-up to `9745e29`: the independent-codomain structural test
is now shared as `classifier.c:pg_pi_constant_codomain`. Kernel Pi projection,
request/fold/handler checks and provisional continuation-effect synthesis use
the same operation. It neither normalizes nor supplies formation evidence.
Tests cover constant and dependent codomains, invalid inputs, and a structurally
dependent beta-redex whose eventual result would be constant. The existing
checked-RETURN-to-APP source route remains unchanged; no dependent Fold rule
or universal extraction from a neutral computation was added.

`check`, `check-examples`, `check-example-results`, `check-image-origins` and
rebuilt ASan/UBSan Core tests pass. The four family acceptance outcomes above
are unchanged. Implementation delta: +22/-20 lines; Core tests: +8/-0;
documentation excluded. This is shared rule plumbing, not open-family completion.

- [x] Identify the first failing producer rather than inferring failure from
  aggregate status. Existing neutral-force tests deliberately reject extracting
  an arbitrary returned value from a neutral computation.
- [ ] Specify symbolic pure-result formation in the typed layer: its context,
  universe, substitution and reduction equations, and admissibility conditions.
  An empty effect row alone must not be assumed to prove termination. Do not add
  a separate value-side Lambda/APP or coerce an arbitrary computation to a type.
- [ ] Implement those rules through ordinary synthesis/kernel acceptance and
  retained derivation inputs, then make `check-open-families` pass. Keep Core
  interning structural, and preserve `::` as a post-synthesis check.
- [ ] Cover substitution of a closed family into the open family, effectful
  rejection, and dimensional action on the resulting dependent classifier.

Independent of this missing admission rule, image transport must preserve the
unresolved source problem. The CLI regression compares direct Solve with fresh
load/Solve after saving at budgets 0/100 and resaving at budget 0, for both the
closed control and open fixture. This is a transport check, not evidence that
open-family typing or retained-result CHECKPOINT is complete. No separate replay
engine or stored acceptance flag is introduced.

Verification: `make -s -f src/prototype/pointer/Makefile check` and the standalone
`tests/image_cli.sh` invocation pass. The full acceptance gate is still incomplete;
the open-family result above remains a required correction before Main promotion.

### September 9: Retained Proof Reuse Is Not Source Checkpointing

Module-only checkpoint audit after `4f55c89`: the new required
`check-prepared-modules` gate prepares a real module annotation, saves only the
module root, destroys the program, and restores it. Re-requesting the known
definition/type/annotation inputs currently creates four missing jobs in round 0.
It then requires the same property after an unsolved resave, and eventual Solve
must use the same annotation producer. This gate belongs to `check-acceptance`;
it is not an expected failure in the passing component suite.

A temporary experiment enumerated `pg_synthesis_definition_entry` from the
exporter's producer dependency callback. It passed round 0 but lost the same
four jobs in round 1. The experiment was removed. `state->entries` is populated
by registration/activation, whereas restored source roots have not performed
those transitions. An orphan producer table does not preserve the module-to-input
relationship, even though explicit selection of those producers still works.

Implementation progress (September 9, shared input preparation):

- [x] Add `pg_synthesis_retain_definition_input` using the existing registration
  state and canonical producer jobs. Retention does not advance Solve, import
  acceptance, or restore registration/activation cursors.
- [x] Use the same entry attachment helper during ordinary registration.
  Registration reconstructs each definition/import producer and each post-check
  producer and rejects a conflicting retained edge. In particular, a non-null
  retained `::` entry must not bypass expectation construction.
- [x] Test retained correct inputs, idempotent attachment, conflicting entries,
  invalid indices/roots, and mismatched definition/expectation producers. The
  complete `synthesis_test` passes, including the new retained-input cases.
- [x] Connect module-to-entry relationships to the image producer graph and
  restore them without running Solve. APGSRC9 adds a bounded table of triples
  `(module producer, source item index, entry producer)`. Entry producers precede
  the module in dependency order. The reader attaches these inputs through the
  same retention API, without restoring acceptance or registration cursors.
- [x] `check-prepared-modules` passes both fresh load and unsolved resave rounds,
  followed by ordinary Solve using the exact same annotation producer. This
  establishes prepared module input retention, not complete CHECKPOINT support.

Verification: `make -s -f src/prototype/pointer/Makefile check
check-prepared-modules` passes with APGSRC9. The full suite exposed an empty-entry
DAG traversal bug (fixed by the existing skipped-slot protocol), and an obsolete
CLI assertion that solving must leave the image byte-identical. The CLI now
checks byte identity only for an unsolved resave; ordinary Solve may retain newly
prepared inputs. Existing root-result, rejected-sibling and normalization checks
remain in place. Open-family admission and normalization CHECKPOINT remain open.

Boundary verification: the module-only fixture now changes saved module owner,
source-item index, cyclic producer reference and a well-formed but incorrect
annotation producer. The first three fail reconstruction; the fourth loads with
zero Solve steps and no accepted root, then rejects through ordinary registration.
Both checkpoint rounds run these cases. `check-prepared-modules` and the complete
source-image test script pass with ASan/UBSan as well as the normal build.

Step-boundary coverage: `check-prepared-modules` additionally saves before the
first Solve step and after every individual step until the queue is quiescent.
The valid, missing-name and cyclic-sibling modules produce 247, 254 and 257
snapshots respectively. Each is loaded with zero Solve steps/no accepted root,
then solved using alternating budgets 1/64; all 758 preserve DONE, REJECTED or
PENDING respectively. This tests partial registration/activation boundaries,
not complete reuse of all normalization or typing work.

Current verification after adding these cases: `check-examples` passes all eight
files currently selected by `examples/0[1-9]_*.p`; `check-example-results` passes
six result fixtures with budgets 1 and 10000; `check-image-origins` passes. This
does not cover IF8, abstract family admission, filesystem import resolution or
REPL parity. In particular, `main.c` still accepts one source/image input and
does not resolve source import providers from files. `pg_program_source` and
the module/import APIs are in-memory mechanisms, not evidence of a complete
module-loading driver. N5 remains incomplete despite the image regressions.

Remaining implementation requirements:

- Represent a module's prepared input relationships independently of its
  mutable registration/activation cursors. Use the existing canonical jobs,
  not copies of their status, evidence, or a second accepted module table.
- Make ordinary preparation and restoration construct that same input graph.
  Validate source-item/scope/input correspondence; a stored edge is not evidence
  that a definition or `::` was accepted. Keep whole-module checking intact.
- Export from those relationships, not only from `state->entries` visible after
  Solve. Preserve the relationships during an unsolved read/write cycle.
- Pass `check-prepared-modules`, including round 1, before claiming prepared
  module retention. Do not move name indexing into an unbudgeted eager loop
  merely to populate entries at load time. Normalization retention remains a
  separate unfinished part of CHECKPOINT.

Follow-up to `c400fb7`: `APGSRC8` makes NAME and MODULE scope records reference
the shared producer table. Their last u64 is now a producer ID; for BINDING it
remains a raw-rule ID. Other scope fields and all table layouts remain unchanged.
Source and raw-rule names use the same path as prepared annotation names. Old
version-7 files reject; this does not change the separate derivation wire format.

Export drains newly discovered scopes/producers with retained cursors, including
lexical-only dependencies. Import constructs a temporary combined dependency
DAG over the two tables, rejects cross-table cycles, then calls the existing
scope/producer factories in dependency order. The temporary ordering graph is
freed on success and failure. Nominal origins are attached before any Solve
step, including when their source producer was already reconstructed.

Regression cases include a lexical name pointing at a pending annotation,
exact alias/producer sharing after fresh load, unsolved resave, consumer-only
roots, propagation of wrong-target rejection, and a corrupted name pointing
back to its consumer. Source-level recursive definitions remain legal pending
inputs; they are not cyclic immutable construction recipes.

Follow-up depth regression after `6a392eb`: 512 alternating lexical-name and
prepared-annotation dependencies save as exactly 513 scopes and 1026 producers,
with two identical selected roots. After destroying the original program,
loading preserves a single shared reference syntax and type producer throughout
the chain. Chunk sizes 1 and 64 produce the same value and transition count.
The normal and rebuilt ASan/UBSan source-image suites pass. The initial fixture
omitted identifier `text_length` and was correctly rejected; correcting the
fixture required no implementation change. This test does not establish
retention of solved work or complete CHECKPOINT support.

Validation after the temporary-order storage adjustment: `check`,
`check-examples`, `check-example-results`, `check-image-origins`, and rebuilt
ASan/UBSan source-image/origin suites pass. The sanitizer origin script initially
lacked its `pointer-check` binary; building that target and rerunning the script
passed. `check-open-families` remains 1/4, with the three unsupported outcomes
listed above. Implementation/header delta: +124/-81 lines; tests: +64/-7;
documentation excluded. Full source preparation, normalization retention and
the original N0--N7 acceptance requirements remain incomplete.

Previous version-7 layout and the tests it introduced:

Follow-up after `5890e3e`: `APGSRC7` separates selected root IDs from a shared
producer table. Leaves retain the previous source-expression, definition and
raw-rule inputs. Prepared source annotations retain their scope and two
preceding producer IDs; nested annotations reuse operands rather than copying
their subgraphs. Reading calls `pg_synthesis_source_expect`, without advancing
Solve or importing an acceptance flag. Source environments and nominal origins
continue using the existing syntax/Core/derivation tables.

Wire layout: magic; policy, scope_count, selection_count, origin_count,
producer_count (u64); existing scope records; selection IDs; producer records
of six u64s `(scope, syntax, definitions, rule, left, right)`; existing origin,
syntax and derivation sections. Leaf records use the first four fields as
before; annotation records require scope, left and right only. Both operands
must precede their parent. Mixed records, missing operands, forward/self edges
and out-of-range selections reject. The aggregate record quota includes the
producer fields. Old version-6 headers reject explicitly.

- [x] Save pending source/rule/annotation graphs without running Solve.
- [x] Preserve selected order, duplicate roots and shared nested operands.
- [x] Fresh-process read and unsolved resave at budgets 1/64; wrong target
  rejects through ordinary Solve while valid siblings remain accepted.
- [x] Reject cyclic, incomplete and mixed annotation records before Solve.
- [x] Round-trip actual module-generated annotation producers, not only manually
  assembled expectations. Restore the module, its definition and its annotation
  as selected roots; registration must recover the exact same producer pointers.
  Cover valid modules, rejected unrelated definitions, cyclic siblings and an
  incorrect annotation. Save both after registration and after draining ready
  work, resave without Solve in a fresh process, then advance in chunks 1/64.
  Loading supplies no root evidence; accepted/rejected/pending outcomes are
  recovered through ordinary Solve, not serialized acceptance flags.
- [x] Generalize lexical named/module-producer environments to the shared
  producer DAG, including prepared annotations and lexical-only dependencies.
- [ ] Generalize remaining preparation kinds; unsupported export cases must
  still fail explicitly.
- [ ] Preserve complete module preparation and retained normalization work.
  This is the first prepared-annotation transport, not complete CHECKPOINT.

The no-acceptance test permits only the ordinary empty-context proof created by
program initialization; none of the restored roots has evidence before Solve.

Validation: `check`, `check-examples`, `check-example-results`, and
`check-image-origins` pass. Rebuilt ASan/UBSan `source_io_test` passes the
fresh-process source-image suite, including malformed annotation edges. The
open-family/full-language gate is still incomplete; no Main promotion follows.

Follow-up to `dd29ede`: the module cases above pass in the normal and rebuilt
ASan/UBSan source-image suites. `check`, `check-examples` (8 source checks),
`check-example-results` (6 execution fixtures at two budgets), and
`check-image-origins` pass. `check-open-families` was rerun and still reports
`unsupported steps=209`. This change adds regression tests, not a Replay engine
or retention of solved source results. No claim of complete CHECKPOINT follows
from reconstituting the same immutable input producers.

Follow-up after `d539c07`: `pg_synthesis_source_expect` is now the common
scope/term-producer/type-producer factory for expression annotations, module
obligations and annotated computation-block entries. The remaining block-local
synthetic EXPECT allocation is removed. Existing source-reference adaptations
remain in the single worker, above Core and before ordinary post-checking.
Direct requests and expression lowering share the same interned job; malformed
NULL inputs reject without constructing another annotation. Existing annotated
block evaluation fixtures remain part of verification. This prepares one
annotation dependency shape for image transport; it does not yet serialize the
prepared producer graph or complete CHECKPOINT.

Validation: `check`, eight source examples and six execution fixtures (two
budgets each) pass. Example 09 now takes 3473 solver transitions versus 3463
in the preceding revision: the explicit expression adapter adds scheduling
work. This change unifies producer representation; it is not a speedup claim.

Follow-up after `ae30471`: module `name :: Type` entries now reference the
existing definition/import producer and independently synthesized type producer.
Registration no longer fabricates an ATOM and EXPECT syntax tree per obligation.
The source-annotation job is interned by scope and these two producers; ordinary
expression annotations and module annotations share `source_expect_step`.
It performs the existing source polarity adaptations and delegates acceptance
to `pg_synthesis_expect`. The internal job role replaces synthetic-source work;
it adds no Core constructor, kernel rule or competing accepted-state store.
Context projection is retained explicitly for names supplied by an outer scope.
`source_reference_proof` shares the existing implicit-storage-thunk adjustment
with normal name lookup. The first direct-producer version missed this and
rejected example 09; the correction and a focused implicit-thunk annotation
regression retain the original meaning without fabricating syntax nodes.

- [x] Preserve producer-first synthesis and post-check-only `::` behavior.
- [x] Reuse one obligation job for repeated entries sharing exact source inputs.
- [ ] Transport these preparation inputs along with the rest of a module's
  producer graph. Do not mistake this simplification for completed CHECKPOINT.

Validation: `check`, `check-examples` (8 source inputs), and
`check-example-results` (6 execution fixtures at two budgets) pass after the
correction. Open-family admission and full N0--N7 acceptance remain incomplete.

Implementation after `d65b4fc`: `pg_synthesis_definition_entry` exposes each
module entry by source position using the existing syntax and registration
arrays. It includes assignments, imports and post-synthesis `::` obligations;
the selected `.name` view cannot hide the other entries. An unprepared entry
has a NULL producer, distinct from end-of-module. Queries allocate nothing,
register no names and advance no work. The view remains available after
rejection. No copied export registry or new accepted-state authority was added.

- [x] Check zero-fuel queries leave jobs/scopes/terms/proofs/steps unchanged.
- [x] Track producer pointer stability across single-step preparation.
- [x] Preserve shared repeated imports and their separate `::` obligation.
- [x] Keep failed and cyclic/pending unselected entries observable.
- [ ] Use this view in prepared-module transport, retaining unfinished entries
  and namespace exports as well as completed rule inputs. The view alone is
  not CHECKPOINT serialization and does not skip source synthesis.

Validation: rebuilt `synthesis_test` and full `make -s -f
src/prototype/pointer/Makefile check` pass. The full reimplementation acceptance
gate and Main promotion remain open.

Audit after `55c52f5`: `derivation_input_step` translates retained headers and
premise references into `pg_synthesis_rule`, the same rule factory used by source
synthesis. There is no independent replay checker. A new regression exports a
source-synthesized identity derivation, checks it through a fresh synthesis store
sharing the typing store, and requires the exact original accepted evidence
without growth of the proof table. Repeating the retained input requires no
additional solver transitions. Changing the retained header to APP rejects
without invalidating either previously accepted root. Fresh-process admission
remains covered separately by the existing producer image test.

This establishes reuse of accepted evidence, not avoidance of rule traversal or
normalization. The current source image reconstructs source jobs; retained
allocation origins are not a cache of the complete source derivation. Do not
add a source-to-proof attachment that simply trusts the saved correspondence.
The next CHECKPOINT change must preserve the prepared producer graph, including
module exports and outstanding obligations, not only selected final proofs.
Nominal namespace/constructor exports must survive without re-discovering them
from erased Core. Keep unfinished source producers and prepared rule producers
in the same scheduler; implement source/rule transport as adapters, not a second
acceptance engine. This requirement is still open.

Validation: standalone producer export and the full `check` target pass,
including fresh-process producer admission at budgets 1/64. No runtime code or
wire format was changed in this audit; CHECKPOINT remains incomplete.

Theory references checked during the open-family investigation:
[Vakar, A Framework for Dependent Types and Effects](https://arxiv.org/abs/1512.08009)
distinguishes dCBPV- from the dependent sequencing extension dCBPV+;
[Narya typechecking details](https://narya.readthedocs.io/en/latest/typechecking.html)
describes dependent function syntax and let-bound definitions. Neither source
establishes A Program's proposed extraction of a neutral pure computation into
a universe value. That extension needs its own substitution and computation
rules; it cannot be justified merely by citing CBPV or Narya.

### September 9: Publish Only Completed Images

N5 file-publication correction after `3f2a552`: the CLI previously opened the
destination with `wb` before exporting, destroying an existing image on export
or I/O failure. `save_image` now writes to a unique sibling temporary file,
closes it successfully, then publishes with `rename`. Failure removes the
temporary file without modifying the existing destination. This is a POSIX CLI
filesystem adapter, not another image codec or acceptance path.

- [x] Inject a file-size-limit failure while loading and saving the same path;
  preserve the existing bytes and remove temporary output.
- [x] Verify successful in-place save through fresh load and List NF agreement.
- [x] Verify failed publication to a directory preserves it and cleans up.

The new regression fails with the pre-change debug binary: the saved image is
truncated at 1024 bytes. It passes with the new binary. Replacement uses a new
file (mode 0600); hard-link aliases keep the old file and a destination symlink
is replaced, not followed. This guarantees completed-file publication during
normal operation, not power-loss durability: no directory/file fsync guarantee
is claimed. Retained-result CHECKPOINT and open-family admission remain open.

Validation: rebuilt CLI, extended `image_cli.sh`, and the full `check` target
pass. Full N0--N7 acceptance is not claimed.

### September 9: Nominal Derivation Parameters in the Shared Image

Continuation after `a995988`. `APGDRV` version 5 adds declaration and constructor
references to the existing term-reference table. The reader checks their object
kinds and applicable rule, then produces ordinary unaccepted rule inputs.
Formation and constructor acceptance still go through the common Solve and
the existing kernel constructors. No nominal replay engine was introduced.

- [x] Serialize nominal rule parameters without copying accepted flags or
  creating a separate family/context relocation table.
- [x] Fresh-process formation and zero/successor introduction at budgets 1/64.
- [x] Preserve repeated formation roots and constructor ownership; require zero
  accepted proofs before Solve and reject a substituted wrong-arity constructor.
- [x] Update record-grammar tests, retaining explicit rejection when callers
  supply a codec that cannot transport nominal declarations.
- [x] Add whole-file Match/direct-IH derivation cases and their iota results.
- [ ] Complete unfinished source/module checkpointing and general indexed rules.

Normal `check`, `check-examples`, `check-example-results` and rebuilt ASan/UBSan
derivation-image tests pass. Existing stored-derivation Solve remains 565 steps.
The general reimplementation goal and Main promotion remain incomplete.

Follow-up after `e1e9bb3`: the fresh-process nominal fixture now retains a
predecessor Match and a two-step direct-IH countdown. Both are checked by common
Solve at budgets 1/64 and normalized to the relocated family's zero constructor.
Swapping Match/induction rule names while retaining their branch premises is
rejected without changing the already accepted roots. No runtime or kernel
special case was needed. Normal checks, eight source cases, six execution
fixtures and rebuilt ASan/UBSan derivation-image tests pass.

At this checkpoint `seed.c` stored source bytes and policy only; section 8's
follow-up replaces those bytes with unresolved syntax. `program.c` owns a live
source graph but does not serialize modules/scopes. Neither is the final `.a`
checkpoint. Preserve unresolved
source/module inputs and producer references before claiming whole-program
resumption; keep nominal rule transport on the path above.

### September 9: Shared Context Payload and Nominal Family Relocation

Continuation after `fb3fc1e`. Context relocation now has one pack/unpack
algorithm, shared by standalone context images and nominal family descriptors.
The descriptor carries the declaration contexts, erased layout and result
images through one Core relocation table. Its temporary pointer-keyed packing
cache is not another semantic authority. Reconstruction uses the destination
context interner and creates no accepted evidence.

- [x] Extract `context_payload.c`; make `context_io.c` a framing adapter.
- [x] Add `declaration_io.c` for inert family descriptors and delegate existing
  descriptor kinds to their existing implementation.
- [x] Preserve repeated family references and shared contexts, including an
  external context whose classifier references the transported family.
- [x] Keep distinct, structurally identical declarations nominally distinct.
- [x] Test fresh-process formation, changed-map rejection, incomplete input,
  no evidence on load and the empty-context zero-budget boundary.
- [ ] Integrate these descriptors and nominal rule parameters into whole-file
  derivation transport, then feed them through the existing common Solve.

The extracted context framing is `APGCTX` version 1; `APGCORE` remains version
1. This is not completion of nominal derivation I/O or a general `.a` checkpoint.
Normal `check`, `check-examples` and `check-example-results` pass. Rebuilt
ASan/UBSan graph-acceptance and occurrence-image tests also pass.

Replay clarification: there must not be an independent artifact typing engine.
Loaded inputs resume ordinary Solve. Stored derivations can avoid rediscovering
proofs, but loading their pointer graph does not establish their validity.
Verification uses the same named kernel rules as fresh synthesis, not a second
set of acceptance rules. Recompute/cache policies must not change this invariant.

### September 9: Constructor and Elimination Inputs Through Common Solve

Continuation after `13a25cf`. Constructor introduction, Match and direct IH
elimination now use the common derivation dispatcher. No new logical rule is
introduced. Constructor inputs retain the constructor pointer and recover field
evidence from the already retained field substitution. Match/IH inputs retain
the motive, formation, parameter map, scrutinee, motive context, branches and
result formation. Reconstructed evidence must retain every supplied premise;
the dispatcher cannot silently replace a supplied result formation.

- [x] Extract constructor pointers and include them in exact producer keys.
- [x] Reconstruct all three rules using the existing named kernel constructors.
- [x] Reuse original evidence through ordinary Solve at budgets 1/64; reject
  wrong premise counts and substituted field-map/result-formation premises.
- [x] Keep nominal derivation I/O explicitly unsupported until family relocation
  is implemented, rather than write an image omitting semantic parameters.
- [ ] Complete family-reference relocation and whole-file nominal derivations.

This covers only the existing zero-index/direct-IH fragment. It does not add
indexed elimination, generalized recursion or a new reduction rule.
Normal `check`, eight source checks and six runtime fixtures pass. Rebuilt
ASan/UBSan IADT and derivation-image tests pass. Implementation C/header:
+36/-4; test C: +44/-3; prototype test build: +2/-2; documentation separate.

### September 9: Nominal Formation Through Common Solve

Continuation after `d100db7`. The common derivation input can now carry an inert
declaration for `PG_INDUCTIVE_FORM`. Its premises are the distinguished Self
context and constructor-result substitutions. The rule checks those premises
against the declaration, then calls the existing zero-index inductive formation
rule. This does not extend positivity, universes or indexed formation.

The producer key includes declaration identity. Accepted formation keys now use
that same declaration identity plus their ordinary context/premise keys, rather
than the address of a freshly reconstructed checked-schema wrapper. The evidence
still retains the checked schema for elimination; no accepted record is mutated.

- [x] Extract formation inputs and run them through the ordinary rule producer.
- [x] Reuse existing evidence for the same declaration and premises; do not
  conflate fresh declarations with equal payloads or shared erased layouts.
- [x] Reject missing declarations and retain the source example synthesis path.
- [x] Explicitly reject nominal formation in derivation I/O until its family
  references can be relocated; do not silently omit the new input parameter.
- [ ] Integrate full family relocation and constructor/Match/IH rule inputs.
- [ ] Finish file-to-Solve round trips and whole-module checkpoint support.

Normal `check`, eight source checks and six runtime fixtures pass; rebuilt
ASan/UBSan IADT and derivation-image suites pass. The reuse test isolates its
queue from unrelated source preparation, which can legitimately add evidence.
Implementation C/header: +34/-4; test C: +50/-1; documentation separate.

### September 9: Declaration Payload Round Trip

Continuation after `e5a1bd7`. `pg_data_declaration_pack/unpack` projects the
immutable declaration to shared context/term root slices: parameter and index
contexts, constructor field contexts, matcher, and ordered result images.
Unpack checks structural counts and reuses the loaded layout through the
ordinary declaration builder. No second schema format, proof store or replay
interpreter is introduced. Packing uses caller storage without changing the
source graph; the declaration owns copied arrays, not the transport slice.

- [x] Fresh-process payload round trip for a recursive Nat-shaped declaration
  whose constructor references are also present in an additional Core root.
- [x] No evidence on load/unpack; reconstruct local context/substitution proofs
  and use the existing schema-check and inductive-formation rules afterwards.
- [x] Reject changed result images when attaching the original premises;
  malformed counts and wrong layout roots cannot create a declaration.
- [ ] Relocate nominal family references themselves in the same graph. Payload
  unpack currently creates a fresh family; it is not a complete module linker.
- [ ] Carry the formation inputs through common Solve and derivation I/O rather
  than the fixture explicitly constructing ordinary kernel premises.

The fixture establishes a declaration-payload boundary, not general IADT
admission, complete `.a` support or completion of N0-N7.
Normal `check`, eight source checks and six runtime fixtures pass; rebuilt
ASan/UBSan graph-image tests pass. Implementation C/header: +62/-0; test C:
+73/-3; shell: +2/-0; documentation separate.

### September 9: Context and Layout Relocation Share One Core Table

Continuation after `f9a7272`. Context transport now uses the existing generic
descriptor codec, retaining the name/resolver APIs as thin adapters to the same
implementation. No second context reader or proof checker is introduced.
Declared types, context binders and additional computation roots share one Core
table, including constructor/layout payloads. APGCTX metadata is unchanged.

`pg_data_declaration_at_layout` connects inert declaration inputs to an already
relocated layout after checking count and every field arity. It uses the same
declaration builder as fresh construction and does not create another layout
or accept evidence. The family is fresh: full relocation of references to a
nominal family is still a separate incomplete step, not silently solved here.

- [x] Preserve shared context prefixes, selected roots, binders and constructors
  across a fresh-process round trip; resume erased iota at budgets 1/100.
- [x] Retain deliberately unverified annotations without producing evidence.
- [x] Reuse loaded layout pointers and reject mismatched field counts/arities.
- [x] Reject truncated context/descriptor images without publishing outputs.
- [ ] Complete nominal family/context dependency relocation and connect loaded
  schema premises to common formation producers; full checkpoint remains open.

Verification: final normal `check`, eight source checks and six runtime fixtures
pass. Rebuilt ASan/UBSan graph-image and IADT suites pass. Implementation
C/header: +61/-5; test C: +30/-8; documentation separate.

### September 9: Inert Nominal Declaration Before Schema Acceptance

Continuation after `24ccbb6`. Inspection found that `pg_data_schema` embedded
both the nominal family object and checked premises. Restoring only its family
label and erased arities would allow unrelated schemas to be attached to the
same identity. That shortcut is rejected: equal arities do not fix a type.

`pg_data_declaration` now owns the inert identity, erased layout and immutable
declared contexts/result-image terms. Its construction checks structural shape,
not typing, positivity or universes. `pg_data_schema_check` requires locally
accepted signature/substitution premises matching those exact inputs. It does
not mutate the declaration or accept nominal formation. Existing generative
schema construction uses the same builder and retains fresh family identities.
The runtime layout still contains no typed context or proof; Core interning
still compares reference pointers, never declaration contents.

- [x] Separate the complete inert declaration from checked schema premises.
- [x] Check exact contexts and result images, not layout shape alone.
- [x] Test array ownership, fresh identities, repeated attachment to the same
  identity, wrong images/counts, foreign evidence and absence of new proofs.
- [ ] Encode declaration contexts/images using the shared relocation graph.
- [ ] Preserve its layout pointers across import rather than allocate a second
  layout when attaching restored premises.
- [ ] Connect these inputs to ordinary schema/formation producers and complete
  nominal derivation file round trips. Current codecs still reject them.

This is a prerequisite for checkpointing, not a second Replay implementation.
Normal `check`, eight source checks and six runtime fixtures pass with unchanged
example Solve steps. The final IADT tests also pass under rebuilt ASan/UBSan.
Implementation C/header: +126/-16; test C: +26/-0; documentation separate.

### September 9: Erased Constructor Layout Transport

Continuation after `e6a35dc`. Erased constructors and their matcher can now
travel through the shared Core relocation table. A layout carries constructor
arities; each constructor references its owning layout and position. Separate
layouts with identical arities remain separate, while repeated constructor
references retain sharing. Positions are transport metadata, not replacement
runtime identities: execution continues to select constructors by pointers.

The generic descriptor payload adds unsigned scalar metadata rather than
encoding arities as Universe or Lambda terms. The experimental APGCORE header
is version 1; previous layouts are rejected. Scalars consume the existing
reader item budget. No new Core term tag or typed evidence is introduced.
APGDRV remains version 4 with the updated nested Core format.

- [x] Transport empty and nonempty erased layouts and shared constructor roots.
- [x] Resume iota in a separate process using the ordinary pure evaluator, with
  budgets 1/100; retain neutrality for a constructor from a different layout.
- [x] Reject missing/out-of-range constructor metadata and unexpected scalar
  payloads; reject truncated images without publishing roots.
- [x] Keep source term/object counts unchanged during export and create no
  typing evidence during import or erased evaluation.
- [ ] Transport nominal schema inputs and their dependent formation premises.
- [ ] Admit restored constructor/Match/IH derivations through ordinary Solve.
- [ ] Preserve complete unfinished modules, imports and CLI `.a` checkpoints.

Replay clarification: do not implement a second rule interpreter or reconstruct
the original search history. Loading relocates inert graph/input structures;
ordinary Solve accepts derivations and advances unresolved obligations. A saved
acceptance flag alone is not evidence. Retention modes may omit recomputable
work or retain derivations, but cannot change the rule for accepting them.
Checking saved premises need not repeat proof search; reduction obligations may
still require computation. This does not yet claim a complete checkpoint mode.

Verification: normal `check`, eight example source checks and six runtime
fixtures pass. Rebuilt ASan/UBSan graph and derivation image suites pass,
including cross-process layout identity and resumed iota. Nominal IADT typing
transport remains explicitly unsupported; these tests do not establish it.

### September 9: Read-Only Producer Export

Continuation after `055a3af`. `pg_derivation_input` now belongs to the common
derivation module, not I/O. `pg_derivation_input_header` extracts an unaccepted
header from evidence, replacing receipts by endpoint obligations. The writer
and producer export use this same extraction; synthesis no longer includes the
I/O interface merely to describe its rule inputs.

`pg_synthesis_export_rules` builds a temporary transport DAG from selected
ordinary rule producers, their prepared input aliases, and completed proof
jobs. Source elaboration that is still incomplete returns not-ready instead
of silently exporting its provisional term as the complete computation.
The exporter traverses jobs and existing evidence with the shared iterative DAG
collector. It allocates neither source jobs nor new typing evidence and does
not advance Solve. Temporary ordering tables are discarded; the caller owns
the transport inputs, which borrow Core objects until serialization finishes.

Distinct reached effect workers contribute immutable equation definitions to
the temporary image worker. Each worker is visited once. Distinct workers
redefining the same equation parameter are rejected, even if seeds agree;
their different contribution sets cannot be silently unioned into one meaning.
The temporary worker remains unsealed. It is not a second live solver authority.

- [x] Move the common rule input out of I/O and share evidence-header extraction.
- [x] Export selected prepared rules without source mutation or proof acceptance.
- [x] Transport a function synthesized from actual source together with two
  pending F formations from distinct effect workers through APGDRV v4.
- [x] Restore in a fresh process, park unsealed formations, then finish via
  ordinary Solve at budgets 1/64 and use the restored function from source.
- [x] Reject unprepared source jobs without publishing roots; detect overlapping
  equation authorities rather than merging their definitions.
- [ ] Preserve arbitrary unfinished source preparation, all module obligations,
  nominal datatype schemas and imports in the full program checkpoint.
- [ ] Complete full CLI/REPL `.a` support and the original acceptance gates.

The export above is deliberately a selected derivation transport view, not a
claim that all source jobs or a complete module can already be checkpointed.
Completed proof jobs may supply their accepted derivation; preparation and
solver search histories are not substituted for that derivation.

Verification: normal `check`, eight example source checks and six runtime
fixtures pass. After the final test extension, rebuilt normal and ASan/UBSan
producer-image fixtures also pass: a source application of the loaded identity
normalizes to its original Lambda up to explicit alpha comparison. Export leaves
source job/proof/term counts and scheduler steps unchanged. Implementation
C/header: +182/-34; test C: +114/-3; shell: +5/-0; documentation separate.
The original full-language and program-checkpoint gates remain open.

### September 9: Open-Row Derivation File Round Trips

Continuation after `abdbaf4`. Experimental APGDRV v4 adds an effect-definition
root slice after the derivation roots and before the single nested Core image.
An F-rule's effect argument is either a closed row reference or its unresolved
equation parameter. Seeds, masks and endpoints use that same relocation table;
the existing descriptor codec reconstructs operation labels and closed rows.
Older experimental APGDRV versions are rejected, not silently reinterpreted.

The ordinary reader/writer implementations handle both closed and open input
DAGs. A supplied effect worker is restored through the existing unpack API;
without it, an image containing definitions is rejected. The worker remains
unsealed and no evidence is created. On any failure it is poisoned, including
late input validation failures after the equations were already reconstructed.
Outputs publish only on full success. Neither solver approximations nor saved
sealing/acceptance flags enter the image. The caller must establish contribution
completeness before sealing, then ordinary Solve handles the loaded inputs.

- [x] Save open-row input parameters and immutable equation definitions together.
- [x] Fresh-process restore of shared Pi/U/F derivations at budgets 1/64, after
  partial effect solving in the writing process.
- [x] Preserve one parameter across distinct contexts, shared premise roots,
  and two distinct same-signature operation labels across a masked cycle.
- [x] Check pre-seal structural queries without formation evidence; after
  sealing, ordinary Solve accepts the Pi and rejects an invalid F premise.
- [x] Reject every truncated fixture, missing worker, and a complete image with
  invalid rule parameters. A late failure cannot expose a partial equation solve.
- [ ] Export all live source producers and their contribution-completeness
  dependencies, including nominal schema/import descriptors, in a program image.
- [ ] Connect full `.a` checkpoint/CLI resumption and close the original gates.

This is an end-to-end rule-DAG/effect-definition transport test, not yet a full
source-program checkpoint. It deliberately resumes effect calculation from
definitions rather than trusting a previously computed least solution.

Verification: normal `check`, eight example source checks and six runtime
fixtures passed. Rebuilt ASan/UBSan derivation-image fixtures passed, including
all prefixes and late validation failure after effect reconstruction. Closed
derivation fixture steps remain 565; source example counts are unchanged.
Implementation C/header: +81/-11; test C: +134/-2; shell: +5/-0; docs separate.
N5 and the full reimplementation goal remain incomplete.

### September 9: Stored Inputs Use Ordinary Rule Producers

Continuation after `05b04d4`. Stored rule DAGs now expand, under the normal fuel
budget, into the same structurally keyed rule jobs used by source elaboration.
The rule evaluator no longer selects between stored-premise and producer-premise
layouts. Expansion retains one preparation cursor; it does not check a theorem
or duplicate kernel acceptance. Prepared child producers are shared before their
proofs finish. Existing dependency subscriptions also notify preparation for
these input jobs, avoiding recursive traversal or repeated ancestor polling.

An unresolved F-formation input retains only an `effect_parameter` graph object.
Its invocation supplies the effect worker separately. Expansion resolves that
object to the registered equation, then requests ordinary F formation with the
existing effect-result dependency. A missing site, conflicting closed row, or
wrong rule is rejected; unknown is never empty. Structural Pi/U/F and variable
classifier queries may finish while their formation proofs remain pending.

- [x] Route stored inputs through ordinary rule producers; no separate Replay.
- [x] Resolve restored equation identities without storing worker pointers in
  immutable rule inputs or publishing provisional formation evidence.
- [x] Add tests at budgets 1/64 for pending Pi structure and variable classifiers,
  post-seal acceptance, missing/conflicting parameter rejection, and reuse of
  the same rule jobs by direct callers (no additional solver cells).
- [x] Bound scheduler work on 2,048 nested stored projections to a linear test
  limit; source and image expansion must not rewalk the ancestor chain.
- [x] Encode unresolved rule parameters and effect definition slices together
  in the derivation image's shared Core relocation table (APGDRV v4, above).
  At this earlier commit APGDRV v3 still refused open-row inputs; full program
  producer export remains separately incomplete.
- [ ] Complete live source-job export, contribution-completeness restoration,
  full checkpoint/CLI/import support and all original N0-N7 gates.

The new open-row synthesis regression reconstructs equation definitions in
memory, destroys the original worker, and only then expands the stored inputs.
It is not yet an end-to-end open-row file round trip. Closed APGDRV fixtures
continue to exercise the same file reader and ordinary Solve.

Verification: normal components, eight example source checks and six runtime
fixtures pass. Rebuilt ASan/UBSan synthesis and derivation-image fixtures pass.
The small stored-derivation fixture now uses 565 scheduler steps (previously
365): preparation is an explicit budgeted phase, not a speedup claim. The
2,048-depth regression bounds total transitions rather than elapsed time.
Source example step counts are unchanged. The open-family acceptance gate
still reports unsupported after 208 steps; the full rewrite is not complete.
Implementation C/header: +112/-16; test C: +103/-0; documentation separate.

### September 9: Effect Definitions in the Shared Image Graph

Continuation after `d12d312`. Independent Replay remains excluded. Effect
definitions can now be packed as a root slice in the ordinary Core relocation
table: `(parameter, seed)` pairs followed by `(source, mask, target)` triples.
Classifier roots use those same parameter objects. The slice is a temporary
transport view, not a second mutable equation authority or an additional Core
former. Constant-source lookup aliases and approximate solutions are omitted.

Unpacking into an empty worker invokes the existing equation/dependency
constructors. It neither seals the worker nor accepts typing evidence. The
program image owner must establish that all contributions are present before
sealing. Invalid or partial restoration poisons the worker, preventing a
solution of an accidentally incomplete graph from being exposed. Later Solve
uses the existing positive-set worklist; no file-specific solver is added.

- [x] Pack immutable definitions and restore relocated parameter identities.
- [x] Add fresh-process tests sharing a pending classifier parameter with its
  equation, including cyclic dependencies and a masked constant contribution.
- [x] Test malformed tuple counts, duplicate sites and a late undeclared edge;
  failed workers cannot advance into an accepted solution.
- [ ] Connect these slices and pending rule producers to the full program image,
  retaining the image's contribution-completeness condition.
- [ ] Complete full N5 checkpoint/CLI/import and original language gates.

The test envelope carries the equation count plus the shared Core roots. This
does not yet define a complete `.a` program format. It exercises reconstruction
after partial solving with budgets 1/100, not retention of solver approximations.

Verification: normal `check`, eight example source checks and six runtime
fixtures passed. The final graph acceptance fixtures passed after rebuilding
both normal and ASan/UBSan binaries. Implementation C/header: +87/-0; test C:
+81/-3; test shell: +3/-0; documentation separate. N5 remains incomplete.

### September 8 handler effect-equation boundary

#### September 9 audit: remaining source-inference cycle

Historical audit against `9777de9`; the sites below describe that revision,
not the current implementation. By `44307c5`, clause contexts and ordinary
operation functions use pending derivation jobs. Source handlers collect
masked input, return and clause effect dependencies before sealing their row.
The unfinished integration is no longer the absence of clause-context jobs.
Current handler acceptance uses the common derivation dispatcher with an
interned ordered declaration signature. General pending inference and descriptor
transport must still pass the full source/image gates. The original cycle was:

```
accepted resumption context requires closed G
    -> clause source synthesis requires that context
    -> clause effects contribute to G
```

Concrete sites:

- `synthesis.c:handler_clause_step` calls `pg_prove_handler_context` before
  binding the payload/resumption names and requesting the clause body.
- `evidence.c:pg_prove_handler_context` requires accepted `F G C` formation.
- `synthesis.c:raw_application_step` waits for both accepted operands before
  recovering the Pi domain/codomain; `open_continuation` likewise requires
  accepted F formation before introducing a result binder.
- `classifier.h` explicitly represents CLOSED sets, not row metavariables.
- `typing.h` already has unaccepted `pg_context` and `pg_occurrence` structures;
  the missing facility is their use by pending synthesis, not another ContextDB.
- `derivation_io.h:pg_derivation_input` already represents unaccepted rule
  applications, but its parameters currently contain a closed effects pointer
  and its premises are fixed rule inputs. It cannot yet refer to a pending
  effect-equation result as a parameter producer.

Do not add another complete syntax walker to guess effects, or keep extending
explicit-carrier wrappers and call that automatic source inference. Higher-order
values carry latent effects inside `U(Pi(A,F G C))`: inspecting only the outer
effect of an expression loses these dependencies. A quoted resumption is a value
but still contains G in its callable classifier.

A fresh operation label is NOT a sound encoding of an unknown row. Let rho be
such a label and H = {print}. Computing `{rho} minus H` retains rho. Substituting
rho := {print} afterwards yields {print}, whereas substituting before subtraction
yields the empty set. Unknown rows must retain the subtraction dependency, not
be treated as nominal operations by closed-row set operations. Similarly, first
checking k under an empty G can incorrectly discharge a pure-effect expectation;
later widening its type would invalidate that previously accepted derivation.

Next implementation sequence (prerequisite for automatic source handlers):

- [x] Expose the existing F application spine independently of closed-row
  interpretation (`pg_effect_type_spine` and its view). Closed construction and
  inspection delegate to this one layout; kernel callers still reject unknown
  rows. A graph-owned binder reference can occur inside latent U/Pi classifiers
  and be replaced by ordinary capture-avoiding substitution, without mutating
  the pending graph. Tests also retain it in unaccepted Context/Occurrence
  records without adding accepted evidence. This is structural infrastructure,
  not an effect-variable formation rule or completed source inference.
- [ ] Represent pending classifier parameters as references to existing
  producers/equation sites in the unaccepted graph. Keep exact pointer interning
  and the same Lambda/APP/Reference Core; do not add a second Pi/Context family.
  A pending row must not pass the closed-row view or yield accepted F formation.
  Specify owner lifetime/relocation before inserting equation references into
  longer-lived Core: current equation storage is freed with its worker.
  Implemented prerequisite: each equation now owns a distinct graph-allocated
  binder parameter exposed by `pg_effect_equation_parameter`. Its object has
  no pointer back into the worker. Tests substitute converged rows into F
  spines and continue inspecting/substituting the original graph after worker
  destruction. The original parameter remains unresolved structurally; no
  accepted evidence is produced. Connecting this to pending source/rule jobs
  and serializing the equation-to-parameter association remain open.
  `pg_synthesis_effect_substitution` now awaits the sealed row producer and
  feeds selected parameter images to the existing resumable substitution
  walker. Its separate result accessor returns an unaccepted term, never
  evidence. Exact requests share work; temporary image arrays are released
  after initialization. Tests compare the result with ordinary substitution,
  reject foreign equations, preserve the input graph, and destroy work during
  image preparation/traversal. Source classifier propagation and checkpoint
  relocation are still open; this is the materialization operation they need.
  The worker now indexes row-term references back to their equation sites.
  `pg_effect_contribution` turns a structural F row parameter (including one
  recovered from a latent U/F classifier) into an ordinary masked dependency.
  Closed row sources and repeated edges are shared; foreign/unknown parameters
  are rejected without being interpreted as operation labels or empty rows.
  The index dies with the worker and introduces no worker pointer into Core.
  Exhaustive three-label seed/mask tests retain the dependency until convergence,
  including the case where an unknown row later contains a handled operation.
  Automatic source classifier propagation and equation transport remain open.
  `pg_synthesis_effect_contribution` now awaits the existing structural
  formation producer and registers its F row (minus a supplied closed mask)
  through `pg_effect_contribution`. It neither waits for the original proof
  nor runs a separate syntax/effect walker. Exact work/target/mask/producer
  requests share one scheduling job; the existing equation graph owns the
  dependency and solution. Completion is registration, not accepted evidence.
  The owner must finish all registrations before sealing; late jobs reject.
  Tests register both masked and unmasked edges from an unaccepted carrier,
  reject non-F formation, keep its evidence pending, and check both converged
  results and completed-request reuse after sealing. This connects structural
  jobs to equations; automatic handler contribution enumeration, pending block
  assembly and equation transport are still unfinished.
  Normal checks, all eight source/six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass; existing example transition counts are unchanged.
  Validation: normal `check`, eight example synthesis cases, six runtime result
  cases, and rebuilt ASan/UBSan synthesis tests pass for this change.
- [ ] Generalize existing unaccepted rule inputs to await parameter producers
  as well as premise producers. Reuse ordinary `pg_prove_derivation` acceptance;
  do not add provisional evidence or a second rule checker. A loaded `.a` uses
  the same producer graph, with no separate Replay semantics.
  Implemented prerequisite: `pg_synthesis_rule` supplies premise producers to
  the same `DERIVATION_JOB` evaluator used by loaded derivations. F formation
  may await a sealed effect equation instead of a closed row parameter; supplying
  both is rejected. No provisional conclusion or new kernel rule is added.
  Handler carrier construction now uses this path. Tests cover chained pending
  universe/F formation, exact request reuse, foreign equations, inappropriate
  parameter rules and conflicting row sources. Loaded derivation inputs retain
  their existing wire format; pending parameter transport/source generation
  and more general classifier constraints remain unfinished.
  Producer-rule calls now share by rule/header fields and exact premise/effect
  producer pointers, rather than the allocation address of the input header.
  Headers are copied on first registration; source variable, quotation and
  handler-carrier preparation use temporary headers. Different premise DAGs,
  binders, row equation sites, levels and conversion endpoints remain distinct.
  This is exact rule-call sharing, not proof irrelevance or Core normalization.
  Loaded recursive rule inputs retain their separate input-DAG identity until
  traversal; no eager recursive interning or second acceptance checker is added.
  Tests cover independently allocated equivalent headers, distinct row sites
  and levels, and mutation of caller storage after registration.
  Validation: normal `check`, eight example synthesis cases, six runtime result
  cases and rebuilt ASan/UBSan synthesis tests pass. List synthesis takes 977
  transitions instead of 979; this is not a general performance claim.
- [ ] Let source binders retain the unaccepted context and its formation
  producer. Generate body constraints without claiming that this context is
  accepted. Allocate each binder once; publishing the eventual formation must
  neither rename it nor mutate an earlier accepted context.
  Verified prerequisite: a pending rule DAG now forms `F G A`, `U(F G A)`,
  context `k : U(F G A)`, `force k`, and its enclosing Lambda after the effect
  equation resolves. Budget-1 and budget-64 runs use existing Context/variable/
  Pi/Lambda rules, allocate no evidence while assembling jobs, and preserve the
  supplied k binder. No additional Context representation was needed for this
  delayed-acceptance test. Source constraint generation before that acceptance
  is still missing; this fixture must not be counted as closing the source gate.
  Source attachment is now available through `pg_synthesis_bind_context`: it
  reserves a lexical binder over the existing context producer and shares the
  same parent/binder validation as accepted `pg_synthesis_bind`. Source variable
  requests wait and recover the ordinary variable evidence. Tests reject wrong
  binders and unrelated parent contexts. This does not yet let body constraint
  generation run ahead of context acceptance; it adds no provisional proof or
  alternate Context representation.
  Ordinary lexical identifier references now build their unaccepted VARIABLE
  rule before waiting on context formation. The test keeps effect closure
  pending and verifies this additional rule dependency exists without a result.
  This is the first source rule migrated, not a general pre-acceptance classifier
  generator; APP/Lambda and symbolic classifier propagation remain open.
  Scheduling now counts those rule transitions: examples still pass (List 989
  versus 977 transitions), and open-family remains unsupported at 94 rather than
  88 transitions. This change is not claimed as a performance improvement.
  Follow-up: one `prepare_expression` path now reserves Lambda/Pi bindings and
  both child producers for APP/EXPECT (the one child for quotation) before
  context acceptance. It replaces the old left-completes-before-right staging;
  kernel acceptance still awaits the premises. Self/IH shared syntax is resolved
  before ordinary APP preparation. The pending-context test confirms Lambda
  binding/body requests already exist without an accepted binding. Examples
  remain 8/8 with six checked runtime results; List now uses 979 transitions,
  and open-family remains unsupported at 101. Symbolic classifier propagation
  and automatic handler equation generation are still not implemented.
- [ ] Use the existing expression traversal for APP, force/thunk, sequencing
  and clause bodies to construct pending rule applications and row dependencies.
  Carry latent effects through callable types, not a flat side table keyed by
  erased Core. Keep `::` exclusively downstream of independent synthesis.
  `pg_synthesis_type_structure` now projects the structural subject of pending
  Universe/F/U/Pi formation rules (and context projections) using the existing
  producer-premise graph. Unknown rules await accepted formation instead of
  guessing. Dynamic F rows remain parameter references regardless of whether
  closure happened before or after the request. The result is a raw Term, not
  an occurrence annotation or evidence; its original formation still must pass.
  Tests construct a symbolic Pi before sealing, then compare its row-substituted
  form with the kernel's result using explicit alpha comparison (substitution
  freshens binders). An invalid U(Universe) structure can be built but its
  formation is rejected. No extra Core tag or Context representation is added.
  Source variable/application classifier propagation is still outstanding.
  Rule-level classifier projection now covers VARIABLE, FORCE, THUNK and Lambda.
  VARIABLE follows the context-producer chain by exact binder identity and
  projects the selected declaration's formation structure. Scope guards do not
  manufacture evidence; their underlying declaration input may be inspected
  but the original guard must still pass. Traversal uses shared scheduled jobs,
  not a second Context container. FORCE removes the structural U wrapper; THUNK
  preserves the full latent classifier beneath U. Tests obtain these structures
  before sealing and follow an outer binder through an intervening declaration,
  then complete the original accepted variable/formation jobs. Source APP and
  dependent result substitution still need integration with these projections.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass for the classifier projections.
  APP classifier projection now substitutes the independently obtained argument
  subject into the structural Pi codomain using the existing budgeted,
  capture-avoiding substitution walker. Pending subjects support variables,
  value/type bridges, context projections and unary CBPV structure; they never
  execute a computation to obtain its value. Unknown subject rules await their
  original producer. Tests use `f : U(Pi(A : Universe, F rho A))` and a distinct
  variable B: before sealing, APP has structural classifier `F rho B` while both
  operand/application proofs remain pending; after sealing the ordinary kernel
  accepts the corresponding `F E B` judgement. This is rule-graph propagation,
  not completed source APP elaboration or automatic handler effect inference.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass for dependent APP projection.
  Prepared source VARIABLE and quotation now expose their existing rule
  producers to structural subject/classifier jobs. There is no second source
  traversal or name resolver. Tests read a source variable's symbolic U/F type
  and exact binder term before context acceptance; a source quotation's raw
  structure remains distinct from the later rejection of quoting a value.
  Unprepared/unsupported source forms still await their original producer.
  General source APP adaptation (including sequential arguments), aliases and
  Lambda rule construction before acceptance remain to be connected.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass for these source projections.
  Lambda/Pi binding reservation now registers the independent domain producer
  immediately, before the outer context is accepted. Source `@` prepares an
  ordinary UNIVERSE_FORM rule just as lexical variables prepare VARIABLE rules;
  the old direct source universe acceptance path is removed. Formation-structure
  projection follows that same prepared rule. Tests inspect the domain of a
  source Lambda in an unresolved effect-dependent context without accepting
  either the domain or context. This does not yet finish pending domain adapters
  for computed type expressions or source Lambda rule assembly.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass. List now uses 984 transitions
  rather than 977; this restructuring is not a performance improvement claim.
  Source binder domain adaptation is now a shared producer, using the same
  checked type-input/value-type operations previously embedded in binding_step.
  Context acceptance consumes that producer's result. Structural declaration
  lookup can inspect supported source Universe/variable domains before that
  acceptance; computed domains still await checked type evaluation. Tests obtain
  the classifiers of x in `lambda x : Universe` and y in nested `lambda y : x`
  under an unaccepted outer context, then verify the eventual ordinary variable
  judgements. No unresolved domain is published as accepted formation.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass, including dependent source domains.
  Source Lambda now prepares a shared body-adaptation producer, classifier
  formation recovery, Pi formation and Lambda introduction rule DAG before
  context acceptance. The former direct source Lambda proof-construction path
  is removed. Structural projection handles the known value/computation rule
  cases of the body adapter; unknown cases await its original acceptance.
  Tests read source Lambda/quotation classifiers before sealing and cover raw
  nested dependent lambdas without an extra U/F wrapper between their Pis.
  The scheduler fairness test now checks at each bounded transition that later
  Lambda work completes while the older slow reduction remains pending, rather
  than assuming its former fixed 32-transition proof-construction cost.
  Source APP adaptation, blocks and automatic handler equation sealing remain
  open; these changes do not complete those source workflows.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass. List synthesis now takes 1044
  transitions as proof recovery/acceptance is exposed to scheduling; no speedup
  is claimed from this source Lambda migration.
  Pending term projection now constructs Lambda/APP subjects without reduction,
  using the same body-rule polarity classification as classifier projection.
  RETURN and value/type bridge classifiers propagate through their premises.
  Tests obtain nested dependent Lambda and dependent APP subjects before effect
  sealing, assert that no producer evidence exists yet, and compare exact Core
  pointers with eventual accepted subjects. Structural availability is not
  proof acceptance. No Core tag, second checker or Replay path was introduced.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass. Example transition counts are
  unchanged; source APP adaptation, blocks and automatic handler sealing remain
  incomplete.
  Raw application now assembles classifier normalization, classifier formation,
  PI_DOMAIN, post-check and APP_ELIM producers. The separate APPLICATION_JOB and
  direct raw-application proof-construction function are removed. Both context
  and operands may be pending; accepted-context callers use the same graph.
  Classifier normalization now awaits context/term producers and shares its
  accepted-input work. Term projection through normalization and post-check
  preserves the original subject without asserting that either check passed.
  Tests build this application before effect sealing, obtain its exact Core,
  then verify its accepted judgement and retained argument-conversion premise.
  A directly supplied APP derivation is not required to be the same proof.
  API scheduling change: re-requesting application with accepted operands may
  need Solve transitions, rather than immediately reporting DONE. A regression
  test verifies the same final evidence and no additional normalization jobs;
  this does not promise zero additional rule scheduling. Source force/sequence
  adaptation and automatic handler effect inference remain unfinished.
  Validation: normal `check`, eight example synthesis cases, six runtime cases,
  and rebuilt ASan/UBSan synthesis tests pass. List transitions rise from 1044
  to 1156 as ordinary rule dependencies become scheduled; no speedup is claimed.
  Classifier projection now follows explicit normalization producers through
  shared pure WHNF work, and post-check producers through their target formation.
  PI_DOMAIN projects its existing Pi spine without accepting that formation.
  The pending-context test obtains the post-checked application's symbolic
  result before sealing; a separate post-check exposes its target but is later
  rejected for mismatched contexts. Candidate structure never certifies typing.
  Normal `check`, eight example synthesis cases, six runtime cases and rebuilt
  ASan/UBSan synthesis tests pass; example transition counts are unchanged.
  This connects pending raw application classifier propagation, not source
  force/sequencing adaptation or automatic handler inference.
  `pg_synthesis_result_context` now assembles classifier formation,
  RETURN_CONTENT and CONTEXT_EXTEND producers for a chosen continuation binder.
  RETURN_CONTENT structurally projects A from F E A without closing E. A source
  variable attached to this pending context exposes A before effect sealing;
  ordinary context/variable evidence is accepted afterwards with the same binder.
  This prepares a hypothetical continuation argument, not the execution result
  of M. Tests reject passing a value instead of an F computation, and verify
  exact request reuse and the accepted parent context. No Core/job tag or second
  Context representation is added. Existing source block/open_continuation paths
  still require migration to these producers; fold closure is not yet connected.
  `pg_synthesis_lambda_body` now shares body adaptation, classifier recovery,
  Pi formation and Lambda introduction between source lambdas, continuation
  closure and handler return clauses. The synchronous continuation_function
  implementation is removed. Tests read an unsealed continuation classifier
  and later compare its accepted judgement with ordinary Lambda derivation.
  Source open_continuation still waits for accepted computation/context, and
  fold acceptance/dependent pure-result fallback remain unchanged. These are
  remaining migration sites, not a completed source handler pipeline.
  Normal checks, examples and rebuilt ASan/UBSan synthesis tests pass. List
  synthesis transitions increase from 1156 to 1233 as continuation formation
  becomes scheduled; this is not an execution-speed improvement.
  Handler-return migration exposed an overconstraint in derivation.c: every
  rule request was required to retain its requested proof shape, rejecting a
  valid identity projection that returns its input evidence. Corrected uniformly
  for producer and loaded inputs: ordinary pg_prove_* constructors validate all
  arguments and may return canonical evidence. Transport, reflexivity, lift and
  family action retain exact formation-premise checks because their encodings
  supply additional formation choices reconstructed by the constructor adapter.
  Existing wrong-direction/premise rejection tests remain unchanged and pass.
  There is no source-only bypass, second checker or new input-mode flag.
  The writer serializes the actual accepted evidence, not a claim that the
  requested rule shape survived. Loaded inputs compute their own conclusions;
  separately declared export conclusions still require matching before exposure.
  Pending equation/parameter checkpoint transport remains unfinished.
  Handler return clauses now prepare projection, result context, source body and
  Lambda producers before input/context acceptance; accepted continuation frames
  are obtained only at completion. Tests obtain the return classifier before row
  sealing, preserve its binder through acceptance, and run a canonicalizing
  projection request through the loaded-input Solve path as well. Normal checks,
  examples and rebuilt ASan/UBSan synthesis tests pass. Operation-clause generation
  and full automatic handler effect inference remain incomplete.
  Operation clauses now prepare payload/resumption contexts and their two
  Lambda producers before carrier acceptance. Read-only signature accessors
  expose the declaration's existing evidence; no signature copy is introduced.
  The old synchronous handler-context/abstract calls are removed from this source
  path. Carrier F-shape and context checks remain mandatory before publishing
  the clause result. The clause keeps its initially allocated binders instead of
  redirecting to another carrier producer after acceptance. Distinct producer
  requests may therefore yield alpha-equivalent, not pointer-identical proofs;
  exact repeated requests still share. Tests track binder identity across sealing
  and the symbolic-to-closed row inside the latent resumption type.
  The pre-sealing source test uses `@Op req resume => req`. Existing `k req`
  cases still pass after acceptance, but source APP preparation before carrier
  closure remains missing. Operation-name lookup also still needs its outer
  source context. Normal checks, examples and rebuilt ASan/UBSan synthesis tests
  pass; this is not completion of automatic handler inference.
  Source application now prepares Pi/U(Pi) calls with known value arguments
  before context acceptance, using classifier-normalization, optional FORCE,
  optional type/value bridging and the same post-checked APP rule producer.
  Argument classification reuses body-rule polarity or an already accepted
  judgement; expected domains never drive operand synthesis. The pending clause
  test now uses `@Op req resume => resume req`, retaining the symbolic row in
  both its result and latent continuation type until sealing. Known direct-call
  preparation is exposed to structural projection after its rule link exists.
  Sequencing/computed arguments and unsupported structural producers still use
  the accepted-operand application path. This coexistence is transitional, not
  the final source architecture; automatic equation collection/sealing and
  preparation readiness across all source forms remain open. Normal checks,
  examples and rebuilt ASan/UBSan synthesis tests pass. List transitions increase
  from 1233 to 1662 due to preparatory work and retained fallback scheduling;
  no performance improvement is claimed.
  A follow-up readiness regression removes the test's explicit queue drain
  before requesting operation-clause structure. The old path exhausted its
  10000-step budget: a metadata consumer waited for final clause acceptance
  instead of the prerequisite that prepares its rule. Structural consumers now
  follow that existing prerequisite during preparation, then use the published
  rule link. A producer already queued is not enqueued twice. Lambda/APP source
  polarity is computational and quotation polarity is value even before their
  rules are prepared; this classification does not establish typing evidence.
  The test requests both classifier and Core immediately for `resume req`,
  obtains them before sealing, and checks exact Core identity after acceptance.
  Normal checks, eight source cases, six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass. List uses 1668
  transitions; this is a readiness fix, not a performance improvement. General
  pending blocks/sequencing and automatic handler equation collection/sealing
  remain incomplete; no full-inference milestone is marked complete here.
  Zero-clause FOLD now exposes its Core from premise producers before their
  evidence is accepted. Structural projection and `pg_prove_fold` both use
  `pg_computation_fold`; there is no second fold layout or new Core tag.
  The pending-context test sequences a forced computation through a result
  binder, obtains the Core before sealing and compares it by pointer after
  acceptance. A value supplied as the input also permits raw construction but
  must be rejected by the ordinary FOLD rule after sealing. Construction is
  not a certificate. Source `block_step` still awaits each statement; the next
  missing connection is pending frame/rule assembly plus the structural row
  equation `E_sequence = E_input union E_continuation`. Its symbolic row must
  remain an equation dependency, not be guessed empty or forced closed by
  waiting for the sequence's own proof. General block migration remains open.
  `pg_synthesis_sequence(context, input, continuation)` now shares source
  sequencing over producer identities. It retains the existing checked FOLD
  path for computation inputs, APP for known values, and checked pure RETURN
  extraction for dependent results that cannot be folded. No effectful result
  is invented as a value. The continuation is independently synthesized.
  `close_continuation` delegates to this job instead of storing a second
  continuation-proof state in each parent. Return-only handlers pass their
  existing input/continuation producers directly; their redundant accepted
  continuation-frame allocation is removed. Tests request sequencing while
  contexts/effects are pending, reuse the exact request and resulting FOLD
  proof, and reject wrong contexts/non-term inputs. The job still awaits its
  premises before selecting FOLD/APP; pending block preparation and structural
  sequence-effect equations remain open. List transitions are 1682 rather than
  1668; this scheduling migration is not a performance improvement.
  Normal checks, eight source cases, six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass. Implementation C: +57/-37; header: +7; test C: +7.
  Block source assembly now creates statement producers and named result
  contexts before their proofs finish, then assembles continuation/sequence
  producers in reverse. The old accepted-statement/block-frame path is removed.
  Pending block frames contain producer references, not copied evidence;
  accepted continuation frames still used by source application are separate
  migration work, not a new permanent semantic layer. Unnamed statements do
  not extend the source scope: after checking, values are discarded unchanged,
  while computations project the following body into a result context and use
  the same sequence job. This preserves the existing no-dead-scope test rather
  than weakening it. Source sequence now normalizes computation classifiers
  through shared work, preserving subjects, before attempting FOLD.
  Single-statement blocks expose prepared classifier/Core structure even with
  pending outer effects. Multi-statement tests resume after sealing and check
  the normalized result. Raw Pi result binding is rejected by RETURN_CONTENT
  rather than the old block wrapper's unsupported status. Non-F computations
  are not silently thunked. General multi-statement classifier structure still
  needs sequence row equations and source-sequence structural projection;
  automatic handler effect inference is not complete.
  Normal checks, eight source cases, six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass. Example transition counts remain unchanged (List 1682).
  Implementation C: +78/-46; test C: +14/-1. No Core tags or kernel rules added.
  Value-input sequence preparation now emits the ordinary post-checked APP
  producer before context acceptance. Source APP and sequence share the same
  structural value/type-value classification; no expected domain participates
  in it. The redundant late value branch is removed from sequence evaluation.
  Structural consumers follow the prepared sequence application. Classifier
  normalization preserves its input polarity, and BODY/sequence adapters produce
  computations; failing to propagate these facts had left a metadata consumer
  waiting for final context evidence. A regression exhausted 10000 transitions
  before this correction. The test now obtains both classifier and exact Core
  for `{ a := k; b := a; b; }` while k's latent effect row is unresolved, then
  verifies Core identity after acceptance. This does not infer the effect union
  of computation-input sequences; their equation connection remains open.
  September 9: row unions now have an unaccepted APP-spine representation
  with a fixed `solver/effect-union/v1` descriptor. Exact pointer tuples share;
  even two closed operands are not evaluated or identified with their union
  during construction. No new Core tag or kernel conversion rule is added.
  Shared row-contribution jobs interpret this DAG into the existing equation
  graph, distributing a fixed mask over each union. Formation-based collection
  delegates to these same jobs. Unknown leaves reject; unfinished jobs must not
  be treated as successful registrations when the coordinator seals equations.
  A rejected expression can have registered valid leaves already, so those
  edges alone never establish acceptance of its producer.
  Tests cover a 64-level shared diamond within 512 scheduler advances, exact
  request reuse, no premature evidence, closed-union non-reduction, unknown
  leaf rejection, and preservation/removal of distinct labels after sealing.
  FOLD classifier projection now constructs `F (E_input union E_body) B`
  from pending premise classifiers when the continuation has an independent
  F codomain. Row-contribution jobs consume that expression before acceptance;
  the converged row is checked against the eventual ordinary FOLD conclusion.
  This projection does not certify the input/continuation domain match: tests
  construct a mismatched-domain skeleton and require ordinary FOLD rejection.
  Other codomain forms still await the original producer rather than assume
  purity or invent an effect-dependent result value.
  Source sequence now prepares classifier normalization and an ordinary
  FOLD_ELIM producer from computation inputs before context acceptance. Its
  direct pg_prove_fold path is removed; checked pure RETURN extraction remains
  the fallback when the FOLD rule rejects a dependent continuation. Structural
  consumers follow the prepared rule; final acceptance still checks the source
  context and all premises. Unknown input polarity awaits its original producer,
  never an expected argument type or a guessed empty row.
  Tests obtain source-sequence Core/classifier before sealing, compare the Core
  and eventual proof with direct FOLD, and check the actual source clause
  `@Op req resume => { x := resume req; x; }` both before and after row closure.
  Automatic handler collection/sealing and pending image transport remain open.
  These changes do not complete those milestones.
  September 9, after `35ed151`: a regression exposed unstable source-sequence
  Core projection. `(\a : @ => \b : a => b) ((\a : @ => a) result)` under an
  unresolved outer effect context published FOLD before acceptance, but its
  dependent continuation required the existing checked pure-APP fallback.
  BODY now preserves the sequence producer instead of bypassing that choice.
  Sequence Core projection checks structural fold eligibility before exposing
  the candidate; otherwise it awaits the original sequence's final decision.
  This check does not accept evidence. Fixed raw FOLD candidates retain their
  structural projection, and ordinary proof acceptance remains authoritative.
  The regression requires no provisional Core before row sealing and exact
  projected/accepted Core identity afterwards, with split scheduling. Named
  Identity programs also compare early projection with the accepted subject.
  Pending effect transport and general source completion remain open.
  Verification passed: `check check-examples check-example-results` and rebuilt
  ASan/UBSan `synthesis_test`. Eight source cases and six runtime cases retain
  their previous results and transition counts. Implementation C: +24/-0;
  test C: +15/-1; documentation counted separately.
  Callable result recovery is now composed uniformly from classifier formation
  and a declared number of ordinary PI_CONSTANT_CODOMAIN producers. Handler
  carrier construction uses the same helper. One Pi is removed for a return
  clause, two for an operation clause; neither creates a new inference rule.
  Tests register both return and computation-block clause effects into one
  target before sealing, reject a remaining Pi as a non-F contribution, and
  check the converged row against the accepted clause result. The callable's
  original proof remains pending throughout structural collection.
  September 9: a handler without an explicit carrier now owns its positive
  equation work. It resolves operation aliases, rejects duplicate labels and
  missing/duplicate return clauses, derives the return result independently,
  and collects input-minus-handled, return and clause rows. Only after all
  registrations succeed does it seal the equations and request ordinary Solve;
  final acceptance still uses pg_prove_handler and the original clause proofs.
  Surface eliminations containing #.return and operation clauses enter this
  same handler job. No host operation runs during collection. Work destruction
  releases owned equations even when inference is pending or rejected.
  Integration exposed an existing wait cycle for accepted names referenced
  from unresolved clause contexts. Such references now prepare ordinary
  context-projection rules before acceptance. Polarity follows that same
  producer/projection chain, preserving the original operation-name source.
  Non-term bindings remain unsupported; implicit definition quotation retains
  its existing separate adaptation and is not silently treated as a value.
  Tests compare automatic and explicit-carrier handlers for three clause
  orders, aliases, simultaneous handling, invalid clauses and duplicate labels.
  A clause reissuing its own operation retains that effect in the inferred row.
  This completes the tested closed-outer-context source path, not all handlers:
  nested inference under unresolved outer contexts, arbitrary computed source
  arguments and pending equation image transport still need implementation.
  Carrier construction now takes the existing context producer directly,
  rather than requiring accepted context evidence and immediately wrapping it
  again. All call sites use this one API. Tests obtain the carrier structure
  under an unsealed outer context, retain the original pending context, then
  compare its accepted result with ordinary context projection after sealing.
  This removes an acceptance prerequisite, not the nested-equation problem:
  inner rows that mention outer parameters need a common dependency component
  (or an equivalent symbolic equation connection), with closure only after all
  participating handlers register their contributions. A separate inner worker
  must not treat an unrecognized outer parameter as empty or seal that outer
  worker. Removing the scope wait alone would therefore be insufficient.
  Handler classifier projection now forwards its supplied or independently
  prepared carrier's structural formation instead of awaiting handler evidence.
  While preparation is incomplete it follows the handler's actual prerequisite;
  it does not enqueue the handler twice or report a missing classifier as done.
  Surface handler expressions expose the same producer, and handler syntax
  detection is shared with source dispatch. Tests for three clause orders obtain
  the same symbolic classifier through both APIs before either handler proof
  completes. This exposes the row reference needed by a future shared nested
  equation component; it does not yet connect independently owned workers.
  Operation identity lookup now shares its producer-origin traversal with
  checked reference resolution. Structural lookup can find a known declaration
  through an unresolved lexical reference, without executing or accepting that
  reference. Cyclic chains stop, incomplete definition indexes do not reveal
  shadowed outer names, and rejected reference jobs expose no declaration.
  Clause preparation uses the signature before outer context acceptance, then
  awaits both its Lambda proof and the original operation-reference validation
  before accepting. Tests construct clauses under pending outer contexts,
  compare their eventual proofs with ordinary projection, retain pending status
  after structural lookup, and reject incorrect operation-head expectations.
  This removes the signature/context wait, not the still-open shared equation
  ownership and closure protocol for general nested handlers.
  September 9 follow-up: inferred handler dispatch and operation-label scanning
  now prepare before the surrounding context is accepted. The source expression
  forwards to the same handler job; its former post-context dispatch is removed.
  Label scanning uses structural declaration lookup, while final acceptance
  still checks the original operation reference, clause proofs and carrier.
  A regression obtains the classifier of a whole source handler under an
  unsealed effect-dependent context without obtaining its proof, then accepts
  it after the context is solved. An invalid binding context rejects the same
  handler. Normal checks, eight source cases, six runtime cases and rebuilt
  ASan/UBSan synthesis tests pass.
  This checkpoint established pending-context preparation. Shared nested
  equations are implemented by the subsequent September 9 entry below.
  Normal component checks, eight source examples, six execution cases and
  rebuilt ASan/UBSan synthesis tests pass. List now takes 1718 transitions;
  explicit projection jobs change scheduling, not a claimed runtime speedup.
  Normal checks, eight source cases and six runtime cases pass. List synthesis
  now takes 1717 transitions rather than 1682 as FOLD acceptance is scheduled;
  this is not a performance improvement claim.
  Normal checks, eight source/six runtime cases and rebuilt normal/ASan/UBSan
  synthesis tests pass. Wrong-context value application is rejected. Existing
  example transition counts remain unchanged (List 1682).
  Normal checks, eight source cases, six runtime cases and rebuilt normal and
  ASan/UBSan synthesis tests (including invalid-input rejection) pass.
  Normal `check`, eight example synthesis cases, six runtime cases and rebuilt
  ASan/UBSan synthesis tests pass.
  Validation: normal `check`, eight example synthesis cases, six runtime cases
  and rebuilt ASan/UBSan synthesis tests pass after correcting the test's
  pointer-equality assumption about capture-avoiding substitution.
  Source quotation now prepares a THUNK_INTRO rule with its independent operand
  producer before context acceptance, and forwards the ordinary rule result.
  This replaces direct source-side thunk acceptance; loaded and source rules use
  the same evaluator. Pending-context tests cover delayed quotation, preservation
  of its operand classifier under U, and rejection of quoting an existing value.
  `make check`, eight example synthesis cases and six runtime result cases pass.
  This does not yet expose symbolic classifier conclusions or generate handler
  row dependencies; the overall item remains open.
- [ ] Seal effect work only when all reachable contributions are generated.
  Resolve row parameters, then check the retained rule applications using the
  ordinary kernel. Solve must not resynthesize the source under successive G
  guesses; accepted proofs stay immutable.
  Corrected an API ordering obstacle: effect producers, parameterized rules and
  substitutions can now be registered BEFORE sealing. An unsealed effect job
  parks off the ready queue instead of repeatedly consuming Solve budget.
  After sealing (or construction failure), the owner re-requests
  `pg_synthesis_effect_inference` to wake that same producer once. Sealed-at-entry
  use remains supported. Tests park rule/substitution consumers, add a further
  dependency, then notify sealing twice and finish with the expected least row.
  Source traversal still needs to determine when all contributions are present;
  this change removes premature-seal requirements, not that outstanding task.
  Validation: `check`, example synthesis/runtime gates and rebuilt ASan/UBSan
  synthesis tests pass with pre-seal consumers and repeated notification.
- [ ] Connect this path to ordinary handler source dispatch. Retain regressions
  for reissuing clauses, nested masks, quoted/passed resumptions, invalid pure
  expectations, aliases, and split budgets. The hand-supplied effect graph in
  the current fixture is not evidence that this gate passes.

This is an implementation gap in pending elaboration, not a contradiction in
the deep-handler equation or a reason to change its kernel typing rule. The
existing closed-effect worker and final handler checker remain reusable. This
audit supersedes any expectation that wiring the last carrier API alone would
finish general handler source support.

- [x] September 9: `pg_synthesis_handler_carrier(context, returned, work,
  equation)` now assembles classifier formation, constant Pi codomain,
  return-content elimination and equation-parameterized F formation as ordinary
  rule jobs. The separate HANDLER_CARRIER_JOB and its synchronous proof-builder
  step are removed. Structural projection handles constant Pi codomains using
  the existing binder-independence check, without accepting the source proof.
  A pending-context test recovers the symbolic carrier before the return
  continuation is accepted, then checks its judgement after sealing. The
  reissuing-handler fixture still feeds this producer into whole-handler
  synthesis. Dependent codomain extraction is now rejected by the ordinary rule,
  rather than reported as an unsupported wrapper operation. Non-Pi inputs
  lacking classifier recovery remain unsupported; no coercion is added.
  Normal checks, rebuilt ASan/UBSan synthesis tests and all eight source/six
  runtime cases pass with unchanged example transition counts. Implementation
  C changes by +17/-31; test C by +9/-1. The effect graph is still explicitly supplied: automatic generation
  of its contributions and latent callable effects remains unfinished.
- [x] Assemble parsed multi-clause handlers through
  `pg_synthesis_handler(scope, carrier_producer, syntax)`. Share the existing
  return/clause jobs and nominal operation resolver; finish with the existing
  kernel handler rule. Require exactly one explicit `#.return`, in any position.
  Tests execute Fetch/Op handlers in three clause orders and check producer
  reuse, duplicate operation aliases, missing/duplicate returns and incompatible
  clause results. This internal API takes an independently supplied carrier;
  ordinary source dispatch must not guess that carrier from `::` or assume an
  empty effect row. Automatic source carrier/effect inference remains open.
- [x] Add shared `pg_synthesis_handler_clause(scope, carrier_producer, clause)`.
  It awaits operation alias resolution and the independently supplied carrier,
  constructs the existing checked payload/resumption context, and synthesizes
  the body without an expected codomain. Equivalent carrier producers converge
  on accepted evidence before allocating clause binders. Final handler checking
  remains separate: a well-typed body with the wrong result carrier is rejected
  by `pg_prove_handler`, not coerced during synthesis.
  Fixtures now use this API for alias-based resumption and reissuing clauses,
  rather than manually constructing their source binder scopes. Invalid carrier,
  binder arity and non-operation labels are rejected.
- [x] Centralize deep-handler edges in `pg_effect_handler_dependencies`:
  subtract the handled set only from the input; preserve return/clause effects.
  Validate every equation owner before adding any edge. Recursive resumption
  dependencies use the same ordinary effect graph, not a second solver.
- [x] Exercise the inferred row through `pg_prove_handler_context`, source
  clause synthesis, `pg_prove_handler` and normalization. The fixture clause
  `{x := Op req; k x;}` reissues the handled operation: its output row retains
  Op, the emitted request escapes the inner handler, and an outer handler can
  consume it and obtain the original value. An empty carrier is rejected.
- [x] September 9: lexically nested inferred handlers share one equation worker.
  The source scope carries the inference owner, not the Core or accepted proof.
  Each handler registers its own equation and contribution edges; the owner
  seals only when all participating handlers finish registration. Reissued
  clause effects are not masked; only the input contribution subtracts labels.
  A failed registration wakes the shared worker and propagates failure instead
  of leaving other carriers waiting indefinitely. Only the owner destroys work.
  Existing scopes with the same owner are reused without extra scope wrappers.
  An exposed handler is a computation before acceptance; missing this polarity
  fact previously made an enclosing body's classifier await its own effect
  solution. No new term tag, inference rule, Replay engine or expected-type
  producer inference is introduced.
  Seven fixtures run with chunks 1 and 64: outer resumption as inner input,
  reordered/aliased clauses, latent and executed reissued operations, three
  nested handlers, duplicate inner labels, and incompatible inner result types.
  They check inferred rows, actual returned/requested Core and rejection.
  Normal component checks, eight source examples, six runtime cases and rebuilt
  ASan/UBSan synthesis tests pass; example transition counts are unchanged.
  This does not establish inference across arbitrary imported producers or
  newly demanded bodies after component sealing; foreign symbolic row references
  remain rejected, not guessed empty. General callable/dependent carriers and
  equation relocation in pending `.a` images remain separate open gates.
- [x] September 9: return-only source folds prepare through the same sequence
  producer before context or continuation acceptance. The late source dispatch
  and continuation-proof wait are removed. Structural consumers follow that
  ordinary producer rather than wait for a completed source expression.
  Tests obtain its symbolic result under an unsealed effect-dependent context,
  then accept and normalize after sealing. A thunk input stays a returned thunk,
  not an implicitly executed computation. Nested return-only clauses consume
  outer resumptions; effects emitted in their return body remain in the output
  row and normalize to an escaping request. Both nested fixtures run in chunks
  1 and 64. No new proof rule, Core constructor or equation worker is added.
  Normal checks, eight source/six runtime cases and rebuilt ASan/UBSan synthesis
  tests pass. Implementation C changes by +8/-14; regression tests by +16/-0.
- [x] September 9: a structurally known Pi callee now accepts a computation
  argument through pending result-context, projection, ordinary APP, Lambda
  and sequence jobs. The result binder is allocated once after both classifier
  structures are available. Source proof acceptance is not a prerequisite for
  collecting the argument/body effect union. Existing sequence checking retains
  the checked pure-return fallback for dependent results; no expected argument
  type is used to synthesize the argument and no new proof rule is introduced.
  Named value-type domains also expose their existing type structure before
  context acceptance, as literal universes and bound type variables already did.
  Tests inspect a nested source application's classifier under a pending
  context, later check/normalize it, and run handler cases for pending resumption
  arguments, emitted effects, wrong argument types, and requests in both the
  argument and body. Handler cases use chunks 1 and 64.
  Both raw and thunked Pi callees are exercised. Normal component checks,
  eight source cases, six runtime cases and rebuilt ASan/UBSan synthesis pass.
  Moving synchronous work into ordinary jobs raises transition counts: 05
  350->389, 06 272->311, 07 744->921, 09 1718->1987. These are scheduling counts,
  not evidence of a wall-time speedup. Implementation C changes +31/-9, tests
  +17/-0. Source 01-04 and higher Identity transition counts are unchanged.
  This checkpoint left effectful callee discovery on the accepted-operand path;
  the following entry removes that path. General dependent effects and N4 are
  not completed by either entry.
- [x] September 9: source APP now has one pending preparation path. It exposes
  the callee through U/F before sequencing the argument, storing both kinds of
  result bindings in the same frame structure used by blocks. The frames close
  through ordinary Lambda and sequence jobs in reverse order. No recursive C
  elaboration, generated surface syntax, new Core constructor or kernel rule is
  used. Removed application_step, sequence_operand and the fallback stage.
  The remaining accepted-input frame belongs only to Match; rename it accordingly
  and remove its unused value and parent fields. Match preparation remains open.
  Removing the fallback exposed a provisional-classifier bug: an inapplicable
  FOLD candidate was propagated as failure before the sequence's checked pure
  APP fallback finished. Classifier projection now awaits the sequence's final
  evidence in that case; actual errors still propagate. The existing higher
  named Identity example reproduces this dependency and passes again.
  Added split/bulk regressions for Fetch as callee with a pending resumption
  argument, repeated U/F exposure of a delayed function, and rejection of a
  computed nonfunction. Accepted evidence still validates both original operands.
  Normal checks, eight source cases, six execution cases and rebuilt ASan/UBSan
  synthesis pass. Implementation C changes +109/-138; tests +3/-0. Example
  transition counts are 295, 63, 295, 591, 396, 317, 975, 2203 (01-07, 09).
  This is a pipeline unification, not a demonstrated wall-time improvement.
  This does not establish all structural subject projections for dependent
  sequence fallback; audit those separately before retaining them in `.a`.
- [ ] Automatic equation generation from arbitrary source bodies remains open.
  September 9, after `d97b722`: removed the dedicated accepted-evidence
  `match_frame` and synchronous `open_match_input` formation path. Computed
  Match scrutinees now use the block/application frame and ordinary result
  context producer. The binder is allocated once, context formation is awaited,
  and closing uses its original domain/context premises through Lambda and
  sequence jobs. No new Core node or proof rule is introduced.
  Tests cover nested computed scrutinees, preservation of a requested operation
  in the output row, and resuming that request through a source handler to the
  expected constructor. The ordinary Match branch/motive path still requires
  accepted contexts and instance metadata: this is a shared-context prerequisite,
  not completion of pending Match inference. Next, migrate instance/constructor
  scope and branch-motive producers without inventing provisional evidence.
  Invalid result-context formation now propagates the ordinary producer's
  rejection/error instead of collapsing every failure to unsupported.
  Implementation C: +23/-35; regression C: +17/-1; documentation separate.
  Validation: component checks, eight source cases and six runtime cases pass;
  expanded synthesis tests pass in normal and rebuilt ASan/UBSan builds.
  The earlier explicit-carrier fixture supplies the graph before checking the source body;
  it does not infer latent callable effects or a general dependent carrier.

  September 9, after `b4eb838`: source branch abstraction now builds ordinary
  Lambda producers with `pg_synthesis_abstract`. It retains the accepted field
  context's binders and can register the derivation before the body completes.
  Constant-motive and induction-branch synthesis share this operation; constant
  codomain removal uses the same producer chain as handler result inference.
  The source synthesizer no longer calls the synchronous `pg_prove_abstract`.
  BODY checks an explicitly supplied context even for zero abstractions, so an
  empty suffix cannot silently accept a body from a different context.
  Tests check pending construction, exact producer reuse, retained Core identity
  against direct accepted abstraction, invalid prefixes and zero-suffix scope
  rejection. Constructor scope still synchronously extends/lifts substitutions;
  it is not a primitive rule to rename into a new kernel rule. Pending scope
  formation and general dependent branch motives remain open. No Replay,
  expected-type inference or provisional evidence is introduced.
  Validation passed: normal component checks, eight source cases, six runtime
  cases and rebuilt ASan/UBSan synthesis tests. Example transitions for 04-07
  and 09 are 625, 409, 336, 1006 and 2246; splitting synchronous derivations into
  producer work increases these scheduling counts, not the runtime results.
  This is not a wall-time speedup claim. Implementation C/header: +52/-28;
  tests: +11/-0; documentation separate.

  September 9, after `9f72ac9`: substitution formation now accepts pending
  source/destination context producers through `pg_synthesis_substitution_jobs`.
  Accepted context inputs delegate to this same worker. It awaits both contexts,
  checks their judgements and source arity, then uses the existing ordered image
  synthesis, conversion and pairing operations. No partial map is published.
  Tests suspend an identity map under an unsealed effect row, then check its
  context endpoints, image judgement, accepted reconstruction and agreement
  with the accepted-input API. Different image proof paths need not have the
  same proof pointer; reconstructing from the actual checked premises must.
  Wrong arity and non-context producers reject. Constructor scope lifting still
  needs composition from these producers; this prerequisite does not complete
  pending Match or pending artifact transport.
  Normal components, eight source cases, six runtime cases and rebuilt
  ASan/UBSan synthesis tests pass; example transition counts are unchanged.
  Implementation C/header: +31/-1; tests: +23/-0; documentation separate.

  September 9, after `efa3c71`: constructor field scope requests now await
  formation and parameter-map producers, instantiate Self through reindex and
  value coercion, then lift fields through ordinary substitution producers.
  Each field's destination binder is allocated once when its lift is requested.
  Exact scope requests share the complete job and its binders. Source Match
  now awaits this scope producer instead of calling `pg_prove_constructor_scope`.
  `pg_synthesis_substitution_lift` composes reindex, context extension, variable,
  projection and image checking; no new evidence rule or Core constructor was
  added. Tests compare dependent lift images and contexts with the accepted
  reference implementation, check exact request reuse, scope signatures and
  rejection of unrelated extensions/constructor labels. Fresh scopes are not
  equated by alpha or normalization. Named constructor wrappers and IH-scope
  preparation still contain synchronous derived operations; general pending
  Match inference and indexed-family admission remain open.
  Normal checks, eight source cases, six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass. Source 04-07 and 09 take 786, 447, 397, 1065 and 2410
  scheduler transitions; separating previously synchronous work changes these
  counts without changing the runtime results. No speedup is claimed.
  Implementation C/header: +119/-4; tests: +31/-1; documentation separate.

  September 9, after `99a0165`: named constructor wrappers now await the same
  field-scope producer used by Match, form the lifted parameter substitution,
  introduce the constructor using the existing rule and abstract through
  ordinary Lambda producers. Nullary constructors return the value directly.
  The source path no longer calls `pg_prove_constructor_function`. A regression
  checks that the exported successor's Pi binder is the shared scope binder.
  This exposed a retained-evidence gap: direct Match on constructed `List Nat`
  failed when rebasing a parameter image wrapped by post-check conversions.
  `rebase_image` now follows TYPE_CONVERSION's original premise while retaining
  the final subject/classifier alpha checks. It does not introduce strengthening
  or accept a differently typed image. The existing direct recursive List Match
  test reproduced this failure and passes with the repair. General pending
  instance recovery, IH-scope producers and indexed-family admission remain open.
  Normal checks, eight source cases, six runtime cases and rebuilt ASan/UBSan
  synthesis tests pass. Example transitions (01-07, 09) are 318, 142, 318, 1203,
  541, 490, 1158 and 2897. These include the newly scheduled constructor work;
  runtime results are unchanged and no wall-time improvement is asserted.
  Implementation C: +36/-6; tests: +8/-0; documentation separate.

  September 9, after `2fd2628`: source IH branches now request a shared
  induction-scope producer instead of synchronously constructing that scope.
  It reuses constructor field scopes, checks the motive against the nominal
  family, and instantiates each direct recursive field through ordinary
  substitution, reindex, Thunk formation and Context extension jobs. The final
  substitution projects the same original field images into the extended
  scope. Exact repeated requests reuse binders; no Core tag, proof rule or
  Replay engine was introduced. Tests check reuse, zero/successor IH counts,
  the thunked motive classifier and rejection of a non-computation motive.
  Normal components, eight source checks and six execution fixtures pass.
  Rebuilt ASan/UBSan synthesis tests also pass.
  Example 07 and 09 now require 1246 and 3021 scheduler transitions because
  previously synchronous scope work is scheduled; runtime results are unchanged.
  General recursive fields, pending instance recovery, source indexed-family
  admission and checkpoint retention remain open. The existing synchronous
  kernel scope checker is still used by induction-rule verification and by
  `pg_prove_induction_case`; this change does not claim their migration.
  Implementation C/header: +120/-4; tests: +21/-0; documentation separate.

  September 9, after `902a718`: non-IH source branches in an induction now
  await the same induction scope, project their independently synthesized
  function, apply the original fields through ordinary application jobs and
  abstract over the extended context. Source synthesis no longer calls
  `pg_prove_induction_case`. Motive context extension and motive projection
  likewise use ordinary pending rule jobs. The kernel's induction verification
  remains unchanged; no new job role or Core node was necessary.
  A four-constructor Tree regression checks recursive branches that ignore
  one or two IHs while still using the field values in the correct order.
  Both resulting programs normalize to the independently constructed expected
  Tree under single-step Solve. Normal components, eight source checks and six
  execution fixtures pass. Example transitions (01-07, 09): 318, 142, 318,
  1211, 557, 498, 1271, 3059. These schedule formerly synchronous work and are
  not evidence of a performance improvement. Pending instance recovery and
  general dependent motive inference remain open.
  Rebuilt ASan/UBSan synthesis checks pass as well.
  Implementation C: +47/-16; tests: +22/-0; documentation separate.

  September 9, after `0d3417c`: classifier recovery now has one resumable
  retained-premise traversal. `pg_prove_classifier` drives this worker to
  completion; source Solve advances the same worker one traversal step at a
  time. Match, classifier normalization, reflexivity and family action request
  the existing shared classifier-formation job instead of synchronous recovery.
  Variable declaration lookup is also interruptible. This adds no proof rule,
  alternate checker, Core tag or trusted result flag. Traversal fuel does not
  bound the internal cost of individual kernel reindex/projection operations.
  Tests resume the 40,000-premise example in chunks of 1 and 64, check identical
  accepted formation without new proof/Term allocation, cancel partial work,
  reject invalid initialization, and suspend a 64-extension variable lookup.
  Normal components, eight source checks, six execution fixtures and the rebuilt
  focused Core test pass. Source transitions (01-07, 09): 345, 144, 345, 1274,
  655, 552, 1474, 3411; no speedup is asserted by scheduling previously hidden
  traversal steps. Rechecked `check-open-families`: unsupported at 208 steps.
  Its missing stable symbolic result contract is not an IADT parser failure
  and is not repaired by relaxing purity or pretending a neutral computation
  is a RETURN. General IADT admission and checkpoint retention remain open.
  Rebuilt ASan/UBSan Core and synthesis tests pass, including partial-work cleanup.
  Implementation C/header: +119/-46; tests: +29/-0; documentation separate.

  September 9, after `fd79963`: nominal-instance requests accept pending type
  producers and converge on one request keyed by the accepted type evidence.
  Match, constructor scope and qualified constructor lookup reuse this result
  instead of independently walking the same retained provenance. The result
  remains the selected schema, formation and checked parameter substitution;
  no lookup by erased Core, new proof rule or persistent authority is added.
  Tests register before declaration acceptance, check that no result is exposed
  early, compare all three outputs with kernel recovery, verify convergence
  onto the accepted-evidence request and preserve output on unsupported inputs.
  Normal components, eight source checks and six execution fixtures pass.
  This shares repeated recovery; `pg_inductive_instance` still walks its
  internal provenance synchronously. Full traversal fuel, general open-family
  formation, IADT admission and checkpoint retention remain open.
  Rebuilt ASan/UBSan synthesis tests pass.
  Implementation C/header: +67/-6; tests: +17/-0; documentation separate.

  September 9, after `530b77a`: nominal-instance recovery now suspends between
  retained proof wrappers and between substitution-history frames. The
  synchronous `pg_inductive_instance` and shared Solve job drive the same
  worker; application/Pi recovery and the worker share one map-frame operation.
  Final parameter-context and nominal endpoint checks precede result exposure.
  Tests suspend a 32-projection chain before map creation and before its final
  check, resume in chunks of 1/7/64, compare exact recovered evidence with the
  synchronous API, verify no additional retained allocation on repeated recovery,
  cancel during map composition and reject invalid initialization.
  Normal components, eight source checks and six execution fixtures pass.
  The worker counts outer traversal steps, not the cost of nested application
  or Pi-body recovery, substitution composition or alpha comparison. Those
  costs and the general open-family/IADT/checkpoint gates remain open; no new
  formation rule or Core tag was added. Example 07/09 take 1487/3450 scheduler
  transitions with unchanged runtime results, not a claimed speed improvement.
  Rebuilt ASan/UBSan IADT and synthesis tests also pass.
  Implementation C/header: +120/-55; tests: +39/-0; documentation separate.

  September 9, after `f0d26a5`: ordinary derivation dispatch now supports
  REQUEST_INTRO with the selected operation declaration as a borrowed parameter.
  Its four supplied premises must exactly match the signature and operands
  retained by the existing request rule. Solve's shared rule key includes
  the declaration pointer, so identical signatures never merge distinct labels.
  Tests check reconstruction, wrong arity/signature rejection, distinct-label
  requests and repeated job reuse. No request is executed by this acceptance.
  Normal components, eight source checks and six execution fixtures pass.
  The current derivation codec explicitly rejects this descriptor-bearing
  parameter until operation relocation is implemented; a new writer regression
  verifies that boundary, and the rebuilt derivation image tests pass.
  This enables common local rule checking, not effectful CHECKPOINT support.
  Handler descriptors, nominal descriptors and general source/image parity
  remain open. Rebuilt ASan/UBSan Core and synthesis tests pass.
  Implementation C/header: +16/-2; tests: +48/-1;
  documentation separate.

  September 9, after `dbec4ab`: request classifier structure now propagates
  the operation's singleton effect joined with the continuation's effect row
  before that row is solved. Request and sequencing use the same constant
  continuation-codomain projection; no new Core tag or acceptance rule is added.
  The projection is structural information, not evidence. A request with an
  invalid payload may expose that structure but must still fail ordinary
  REQUEST_INTRO checking after its premises become available.
  Regressions exercise an unsealed row, a pending continuation/context, row
  dependency collection, final union and invalid-payload rejection. They run
  with scheduler chunks 1 and 64. Normal components, eight source checks and
  six execution fixtures pass; the final negative regression also passes in
  the rebuilt synthesis suite.
  The rebuilt ASan/UBSan synthesis suite passes as well.
  Implementation C: +27/-7; test C: +45/-0; documentation separate.
  This is local request constraint propagation, not complete source operation
  elaboration, handler dependency generation or descriptor checkpoint transport.
  These and the existing open-family/IADT/higher gates remain required.

  September 9, after `33b2677`: source operation functions now prepare ordinary
  Context/Variable/Return/Lambda/Request jobs instead of synchronously building
  the entire derivation through `pg_prove_operation_function`. A shared operation
  job creates its two binders once and waits for its ordinary Lambda producer.
  Foreign signature evidence remains rejected at the entry boundary.
  Request term projection now uses payload and continuation structure directly,
  without waiting for accepted evidence or executing the request. After effect
  solving, the accepted occurrence retains that exact Core pointer.
  Tests cover pause before acceptance, repeated job/binder reuse, pending request
  structure, and the existing source operation/alias/handler execution cases.
  Normal components, eight source checks and six execution fixtures pass.
  The rebuilt ASan/UBSan synthesis suite also passes.
  The synchronous kernel convenience constructor remains available; the source
  path no longer calls it. This changes scheduling, not request semantics or
  the operation signature rules. General descriptor transport, pending handler
  preparation and the full acceptance gates remain open.
  Implementation C: +42/-5; tests: +20/-0; documentation separate.

  September 9, after `44307c5`: a regression with a Lambda whose body is a
  request and whose context contains an unsealed effect row exhausted the
  10,000-step test limit while requesting only term structure. REQUEST_INTRO
  was absent from structural computation polarity, so BODY_JOB waited for
  acceptance. It now follows the same computation-body path as Return/Fold.
  Prepared operation functions likewise expose their ordinary Lambda producer
  through the existing source preparation protocol. The regression obtains
  both Core and classifier structure before acceptance, then checks exact
  Core reuse after solving, with scheduler chunks 1 and 64.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan synthesis pass. No operation execution occurs during projection.
  Implementation C: +6/-1; tests: +11/-0; documentation separate.
  Handler common-rule dispatch still requires an ordered declaration descriptor
  representation and exact signature-premise checks; reconstructing declarations
  from equal payload/response types would incorrectly merge nominal operations.

  September 9, after `5dddede`: handler evidence retains an immutable ordered
  operation signature, interned by declaration pointers in the existing graph
  object index. Bodies remain ordinary premises. Common derivation dispatch
  reconstructs the handler through `pg_prove_handler` and checks all retained
  signature/body premises exactly. The shared Solve key includes this signature.
  Source handlers now submit that ordinary rule job; zero-clause handlers use
  Fold plus effect subsumption. No new Core term tag or effect execution is added.
  Tests cover declaration order, repeated reuse, identical-signature distinct
  operations, wrong arity/premise rejection, and source multi-clause handlers.
  The derivation writer explicitly refuses handler descriptors until relocation
  is implemented; local parameter extraction is not checkpoint support.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan Core and synthesis tests pass. The derivation-image regression
  also passes, including refusal of a pure-input handler with no request premise.
  Implementation C/header: +118/-9; tests: +43/-1; documentation separate.

  September 9, after `7016c0c`: raw handler and effect-subsumption jobs now
  project classifier structure from their carrier producer before acceptance.
  A new unsealed-row fixture previously exhausted the 10,000-step test limit
  waiting for handler acceptance. It now exposes the symbolic F row and allows
  enclosing Lambda classifier structure to be formed without closing that row.
  Both rules are recognized as computation bodies. Final rule checking remains
  mandatory: a malformed return clause exposes the same structure but is
  rejected after solving. Tests use chunks 1 and 64 and check that no pending
  projection publishes evidence. This does not yet add structural raw handler
  Core projection or descriptor relocation.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan synthesis pass.
  Implementation C: +5/-1; tests: +41/-0; documentation separate.

  September 9, after `a8b683d`: raw handler term projection now waits on the
  source/return structures and visits each clause once, retaining its position
  across scheduler yields. The completed parts use the existing Core Fold
  constructor; no reduction or evidence acceptance occurs in this worker.
  Effect subsumption projects the unchanged input Core. Unsealed-row tests
  cover one and two clauses (distinct operations with identical signatures),
  budgets 1/64, no premature proof and exact Core reuse after acceptance.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan synthesis pass. `check-open-families` remains unsupported after
  208 steps, so full acceptance and the Main push remain unfulfilled.
  Implementation C: +51/-0; tests: +22/-0; documentation separate.

  September 9, after `8bfbfaa`: inferred source handlers now retain clause
  producers rather than snapshots of accepted clause proofs. The ordinary
  handler rule receives those jobs directly; carrier, return and clause proof
  waits are handled by its premise dependencies. Source preparation exposes
  that rule before acceptance. Clause jobs still validate their operation aliases,
  so seeing a nominal descriptor early cannot bypass a failing alias check.
  Unresolved-context fixtures obtain the Core of one- and two-operation surface
  handlers, then check exact Core reuse after acceptance at chunks 1/64.
  At this revision, explicit supplied-carrier requests still used the
  accepted-carrier canonical entry (removed in the next entry below).
  Descriptor transport and the open-family/full-source gates
  remain open; this is not completion of N0-N7.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan synthesis pass, including existing invalid operation-alias cases.
  Implementation C: +18/-16; tests: +14/-0; documentation separate.

  September 9, after `fad1dae`: explicit-carrier handlers retain their original
  producer rather than waiting for its acceptance and redirecting to a second
  handler keyed by the accepted proof. Named pending term producers likewise
  use ordinary context-projection jobs when their term polarity is structurally
  available. Namespace/definition resolution remains separate from term
  projection; this does not infer a classifier from a Core pointer.
  A source handler over a named pending FORCE now exposes its Core before the
  carrier is accepted and keeps that exact Core afterwards. The previous path
  exhausted the fixture's 10,000 scheduler-step limit. Tests also require exact
  request reuse for the original carrier producer; independently supplied proof
  producers may establish the same judgement without sharing fresh binders.
  Escaping an open-context variable rejects through the common derivation rule,
  both before and after its producer finishes. The former pending-name fallback
  reported an internal ERROR for this invalid projection; the test now requires
  REJECTED and independently checks the kernel projection cannot be constructed.
  Normal `check`, eight example source checks and six runtime fixtures pass;
  example transition counts are unchanged. `check-open-families` still reports
  unsupported at 208 steps. Descriptor checkpoint transport and the remaining
  N0-N7 requirements stay open; this change does not authorize a Main push.
  Rebuilt ASan/UBSan synthesis tests also pass. Implementation C: +17/-16;
  test C: +25/-2; documentation counted separately.

  September 9, after `e113b75`: operation relocation exposed a representation
  dependency: the nominal label was embedded in a checked declaration holding
  accepted signature evidence. It could not be restored independently of those
  proofs. Labels now own immutable raw payload/response terms only; creating a
  label does not accept a type or execute a request. A checked declaration binds
  that label to local closed value-type proofs whose subjects are exactly its
  stored signature terms. Distinct labels with identical signatures stay
  distinct; conflicting signatures cannot be installed on an existing label.
  Exact label/proof tuples reuse a declaration through the existing object
  index. Different typing stores need their own signature evidence even when
  they share the Core graph and label.

  REQUEST rule inputs now retain the label, not an already checked declaration.
  HANDLER signatures likewise retain only the ordered label array; signature
  proofs remain the ordinary premises. The common derivation dispatcher checks
  those premises and obtains declarations through the same constructor used by
  local source requests. This removes one temporary declaration array from
  handler evidence construction. It adds neither a Core term tag nor a second
  checking/Replay algorithm. The label-signature pointer check is descriptor
  fidelity, not a new DefEq rule or normalization-based label interning.

  Tests cover fresh raw labels without additional evidence, same-tuple reuse,
  payload/response mismatch rejection, local evidence ownership across typing
  stores, and a REQUEST job whose label has no pre-existing checked declaration.
  The wire codec still explicitly refuses these parameters: raw label/signature
  relocation, closed-row relocation and pending-image transport must be connected
  before removing that refusal. This is an N5 prerequisite, not a checkpoint or
  full-language completion claim.
  Normal components, eight source checks, six runtime fixtures and rebuilt
  ASan/UBSan Core and synthesis tests pass. Example transition counts are
  unchanged; the open-family gate remains unsupported after 208 steps.
  Implementation C/header: +104/-41; test C: +36/-13; docs separate.

  September 9, after `0bb7e3d`: the existing Core codec now supports inert
  descriptor payloads in its one Term relocation table. Owner callbacks enumerate
  immutable Term dependencies and reconstruct an object; they cannot confer
  typing acceptance. The old external-name APIs are wrappers over this same
  implementation, not another reader/writer. Temporary reference terms belong
  to a scratch arena, leaving the source graph unchanged. The iterative DAG
  collector orders signature dependencies before uses; descriptor argument edges
  count against the read limit. Unresolved/cyclic references and object records
  collapsed to one pointer cannot be accepted. Recursive descriptor schemes
  are not implemented by this acyclic payload format.

  Built-in descriptor callbacks support fresh operation labels and closed effect
  rows, with existing classifier/computation/Identity formers resolved by their
  versioned names. Graph object tags 3/4 identify payload-bearing binders/semantic
  objects; their zero-terminated Term IDs share the ordinary Term table. Older
  external-only readers refuse those tags rather than interpreting them as
  addresses. Identical operation signatures do not merge nominal labels. A row
  is rebuilt from the relocated label references, not from host pointer values.

  Fresh-process tests retain two distinct same-signature operations, shared
  REQUEST roots, row membership and a nested U/F operation signature. Reading
  creates no typing evidence; later local declaration checking accepts the
  correct signature and rejects a changed one. All truncated prefixes and a
  complete image lacking its descriptor restorer are rejected without publishing
  outputs. This is raw graph transport only: derivation parameter encoding,
  pending-work checkpointing, imported nominal identities and general IADT image
  schemas remain open. N5 and the full-language goal are not complete.
  Normal `check`, eight example source checks and six execution fixtures pass;
  rebuilt ASan/UBSan graph I/O and fresh-process graph acceptance tests pass.
  Implementation C/header: +264/-34; test C: +76/-3; shell: +2/-0;
  prototype build: +1/-1; documentation separate.

  September 9, after `c25a119`: experimental APGDRV v3 carries operation-label
  and ordered handler-signature references in the same Core table as binder,
  effect-row and conversion/reduction endpoint references. Handler signatures
  use the inert descriptor codec and contain no accepted proofs. Reading restores
  unaccepted rule inputs; `pg_synthesis_derivation` uses ordinary REQUEST/HANDLER
  constructors after the signature premises finish. The external-only API remains
  a wrapper over the same codec; it still refuses objects that its supplied owner
  cannot name/resolve. There is no fallback silently dropping these parameters.
  Previous experimental versions are rejected instead of guessing record layout.

  Fresh-process regressions save a request and a two-clause handler with distinct
  same-signature operations and a shared root. Budgets 1/64 accept the same result;
  the request and handler use the same relocated label, and normalizing the
  handled request produces the expected returned Universe term. Reading alone
  leaves the typing proof store empty. Replacing a loaded request's signature
  premise with the wrong Universe rejects through the ordinary solver. Existing
  prefix, row-field and version rejection tests use the v3 record grammar.
  This is retained derivation transport, not a full program CHECKPOINT: pending
  equations, program work, imported nominal contracts and IADT schema parameters
  remain open. Their absence must not be hidden by marking N5 complete.
  Normal components, eight source checks and six runtime fixtures pass, as do
  rebuilt ASan/UBSan derivation I/O tests. The open-family gate still reports
  unsupported after 208 steps. Implementation C/header: +80/-17;
  test C: +91/-7; test shell: +5/-0; documentation separate.

  September 9, after `8dc108d`: `pg_derivation_inputs_write` saves ordinary
  unaccepted rule DAGs in the same APGDRV v3 grammar as retained evidence. One
  writer traverses either source via its premise accessor; accepted evidence
  supplies a transient rule header without copying its premise arrays or
  modifying its store. No accepted-state bit, proof-search pass or alternate
  checker is introduced. Unaccepted inputs carry normalization/conversion
  endpoints, not borrowed local certificate pointers. Cyclic input DAGs fail
  the shared iterative collector.

  Fresh-process tests construct and save raw context/Universe/RETURN/FORCE/NF
  rule inputs without creating any typing evidence. Shared roots remain shared,
  reading and scheduling create no accepted evidence, and ordinary Solve at
  budgets 1/64 accepts RETURN and rejects FORCE of a non-thunk. A retained NF
  obligation is checked by subsequent Solve, not by reading; when it does not change the term, the
  existing kernel correctly reuses source evidence rather than adding a vacuous
  normalization derivation. This saves static pending rule graphs only. It does
  not yet export live source-job dependencies, effect-equation workers or a full
  program checkpoint; those N5 requirements remain open.
  Normal components, eight source checks, six execution fixtures and rebuilt
  ASan/UBSan derivation I/O tests pass. The open-family gate remains unsupported
  after 208 transitions. Implementation C/header: +65/-23; test C: +69/-2;
  test shell: +5/-0; documentation separate.

  September 9, after `8fc3b5f`: effect equation registration now accepts an
  existing plain parameter through `pg_effect_equation_at`. Fresh creation
  delegates to this same path. Exact parameter/seed reuse returns the existing
  site, including after sealing; a different seed or a new post-seal site rejects.
  The source index keys the referenced object directly, so registration/querying
  does not intern an auxiliary REFERENCE merely to find its equation.

  A read-only visitor exposes original equation seeds followed by dependency
  edges using the existing indices. It does not export mutable approximations,
  scheduler cursors or the disposable closed-row source cache. No second
  equation store was added. Reconstruction tests copy a partially advanced
  cyclic/masked graph into a new worker, destroy the original, seal and solve
  from its original seeds, obtaining the same least closure. The zero seed
  remains zero even when the computed result grows. Parameter ownership,
  conflicting seeds and fresh operation-label misuse are also checked.
  This is the prerequisite for pending-equation image transport, not that codec
  or a live source-job checkpoint. Those N5 requirements remain open.
  Normal components, eight example source checks, six runtime fixtures and
  rebuilt ASan/UBSan synthesis tests pass. Implementation C/header: +62/-14;
  test C: +58/-0; documentation separate.

Verification: regular components, eight example checks, six execution fixtures
and rebuilt ASan/UBSan source synthesis pass. The open-family gate remains
unsupported at 208 transitions; this does not establish full source acceptance.

### September 8 positive effect-equation closure

The next surface-handler obstacle is circular effect inference, not merely
clause parsing. `pg_prove_handler_context` requires `F G C` to bind the deep
resumption, while clause synthesis contributes constraints on G. The existing
kernel checks a supplied closed G; it does not infer this fixed point.

- [x] Add positive set-equation work to ordinary Solve. An equation has an
  immutable closed seed and incoming `(source, constant_mask, target)` edges:
  `target = seed union UNION(source minus mask)`. Repeated exact edges are
  interned; only changed sources revisit outgoing edges. Sites have distinct
  pointer identity, not equality based on their current approximation.
- [x] Retain source equations separately from their one mutable approximation.
  Construction must be sealed before Solve; no row is exposed until the finite
  system converges. A partially constructed or pending equation is never a
  certified empty effect row. Results are not typing certificates.
- [ ] Generate these equations from source clause constraints before constructing
  accepted resumption contexts. Result-type constraints and latent function
  effects still need representation; this worker alone does not synthesize a
  complete handler. Do not bind k under a guessed empty row to bypass this step.
- [ ] Add declaration/equation relocation to `.a`; present work is invocation
  local, and no checkpoint capability is claimed.

For the current first-order, nondependent carrier, generation must account for
`G >= (input_effects minus handled) union return_effects union clause_effects`.
A resumed k contributes G. Clause-issued operations are outside this handler,
so handled labels are removed from the input edge, NOT from all clause effects.
Nested handlers may mask their own input edges. All masks must be closed.

Termination argument for this worker: only labels in the finite seed union can
enter any approximation; each update strictly adds labels. Union and subtraction
of a constant set are monotone. Fair propagation reaches the least solution.
This is not a termination proof for programs, general row unification, open-row
polymorphism, or a proof that source constraint generation is sound. An empty
unseeded cycle is the least solution of an explicitly complete equation system,
not the answer to an unspecified effect metavariable. One budget transition
visits an edge (or an empty adjacency); set union/interner costs remain unbounded
in wall time, as with other allocation operations.

References checked on 2026-09-08:

- [Leijen, Koka: Programming with Row Polymorphic Effect Types (2014)](https://arxiv.org/abs/1406.2061).
  Its inference uses row polymorphism with duplicate labels. A Program currently
  uses idempotent sets, so that unifier is not adopted unchanged.
- [Plotkin and Pretnar, Handling Algebraic Effects (2013)](https://arxiv.org/abs/1312.1399).
  The model/homomorphism account motivates checking the output carrier; it is
  not a ready-made inference algorithm for this pointer implementation.

The positive set-equation algorithm and its boundary above are our engineering
construction for the current closed-set model, not a claimed implementation of
either paper's full calculus.

Verification: 128 combinations of seeds, masks and edge insertion order compare
against direct finite-set expectations, including mutual/self cycles, duplicate
edges, sealed mutation rejection, foreign equation rejection and split Solve.
Regular components and rebuilt ASan/UBSan `synthesis_test` pass. Eight example
checks and six execution fixtures pass; open-family still fails at 88 steps.

### September 8 nominal operation reference resolution

- [x] Resolve an operation label through the existing producer graph with a
  shared, budgeted `pg_synthesis_operation_reference` job. Its output points
  back to the checked operation producer; no declaration metadata is copied
  into Core, contexts, or every expression job, and no new proof is invented.
- [x] Preserve identity through lexical references, definitions, explicit
  quotation and successful source expectations. A selected definition block
  waits for whole-module validity before exposing its selected declaration.
- [x] Test pending resolution, request reuse, quoted aliases, selected exports,
  post-synthesis checking, missing names, and rejection of Lambda/application
  results and bare evidence registration as nominal operation labels.
- [ ] Connect the resolver to general handler clause elaboration together with
  output carrier/effect constraints. Dynamic operation parameters, imported
  declaration relocation and full surface handler synthesis are not complete.

The distinction here is nominal declaration identity, not equality of callable
functions: a function invoking an operation does not become that operation's
label. Alias resolution never evaluates or pattern-matches its erased Core.

Verification: regular components, eight examples, six execution fixtures and
rebuilt ASan/UBSan source synthesis pass. The unchanged open-family acceptance
failure remains `unsupported steps=88`; the full goal remains incomplete.

### September 8 shared operation producers

- [x] Add `pg_synthesis_operation`, keyed by the exact declaration pointer in
  the existing job index. Requesting it creates no Core or accepted proof;
  ordinary Solve builds the existing Lambda/request/RETURN callable once.
  Repeated driver names use `pg_synthesis_name_job` and share this producer.
- [x] Test pending registration, exact job reuse, unchanged Core/proof counts
  on repeated completed requests, source-name reuse, and rejection of a
  declaration whose signature evidence belongs to another typing store.
- [ ] General clause integration and output-carrier synthesis remain open;
  the nominal alias resolver is recorded above. Sharing the callable producer is not
  permission to classify arbitrary functions as operation declarations by
  inspecting their erased Core, and adds no such heuristic.

Verification: regular components, eight examples, six execution fixtures and
rebuilt ASan/UBSan `synthesis_test` pass. Full acceptance still fails the known
`open-family.p` gate at 88 transitions. No Main promotion is authorized by
these partial results.

### September 8 effectful callee sequencing

- [x] Remove the empty-row-only gate on a callee of type `F E (U Pi)`.
  Both operands now use the existing checked continuation/fold path; this is
  not an effectful computation-to-value cast or eager compile-time execution.
- [x] Test `(Fetch Arg) Arg` where Fetch returns a quoted function, and
  `(Fetch Arg) (Op Arg)` where both operands have effects. Check the combined
  row, callee-first request order, and final value after checked handlers.
  Continue rejecting a computation whose result is not callable.

Regular components, eight example checks and six result fixtures pass.
The rebuilt ASan/UBSan source synthesis suite also passes.
The full acceptance gate remains open at `open-family.p`, unsupported at 88
transitions. Operation aliases and source handler carrier inference remain
separate unfinished work; no Core-shape heuristic was added to identify an
operation declaration from an arbitrary function.

### September 8 return-only surface handler

- [x] Share independent return-clause synthesis through
  `pg_synthesis_handler_return(scope, input_producer, clause)`. It produces the
  raw continuation Lambda and its Pi classifier, using only the input result
  domain to bind the return variable. No expected codomain is supplied.
  Return-only handler elaboration now consumes that same producer; continuation
  Lambda construction is shared with ordinary sequencing. This prepares the
  result-type input for full handler constraints without guessing output effects.
  Tests recover a changed return carrier independently, check exact job reuse,
  and preserve dependent-result handling: a known pure RETURN can instantiate
  it, while an unresolved operation result remains unsupported.
- [x] Elaborate `M @#.return x => body` through the existing continuation
  context and `pg_prove_fold`, without a new Core node or proof rule. Synthesize
  the body independently; value bodies use the existing RETURN insertion.
  The return label is explicitly intrinsic-qualified, not an unqualified keyword.
- [x] Cover pure return mapping, changed result type, unhandled operation
  forwarding, and rejection of missing or extra return binders. Ordinary ADT
  elimination keeps its existing path.
- [ ] General operation-clause source elaboration remains open. The checked
  multi-clause kernel API is not evidence of surface carrier inference or
  operation alias/signature resolution. Do not guess an empty output effect row.

Verification: regular component `check` and rebuilt ASan/UBSan `synthesis_test`
pass. Eight unchanged examples and six execution fixtures pass; the full
acceptance gate still fails `open-family.p` with `unsupported steps=88`.
This is partial progress, not completion of the reimplementation.

Replay clarification: keep one Solve/acceptance implementation. Restoring saved
derivations may check their premises with those same rules, but must not create
an independent replay rule engine or trust a serialized completion flag.

### September 8 explicit closed effect rows

- [x] Change the shared computation classifier from unary `F A` to `F E A`.
  Existing pure formation/conversion/action paths use the explicit empty-row
  instance through `pg_return_type`. Its view rejects nonempty rows, so this
  migration does not authorize effectful computation in existing pure rules.
- [x] Intern closed sets of exact operation pointers in the existing semantic
  object index. Canonicalize label order and duplicates, merge unions linearly,
  and keep NULL invalid rather than interpreting it as an empty set. Rows are
  immutable semantic inputs, not another mutable solver-state authority.
- [x] Test union commutativity/associativity/idempotence/unit, exact set reuse,
  invalid inputs, distinct pure/effectful classifiers, and pure-view rejection.
  Update the former descriptor to `kernel/return-type/v2`; reject the old name
  instead of decoding a different arity under it. Empty rows have a fixed
  descriptor; nonempty label relocation still requires owner-aware transport.
  Regular components, eight unchanged examples and six result fixtures pass.
  Full acceptance still fails open-family, now at 88 rather than 86 transitions
  because the classifier has an additional structural application.
  The complete rebuilt ASan/UBSan component `check` also passes, including
  higher action, derivation transport and source synthesis tests.
- [x] Admit closed nonempty rows through the same `PG_RETURN_TYPE_FORM` rule.
  `pg_prove_effect_type` takes an explicit row; `pg_prove_return_type` is its
  empty-row specialization. The subject Core retains the row, and generic
  derivation parameters recover it from that subject rather than maintaining
  another evidence field. NULL is not an empty row. Pure-only evaluation views
  and value extraction continue to reject effectful classifiers.
- [x] Retain that parameter in the experimental APGDRV version 2 codec.
  Missing rows, non-row references, rows on unrelated rules, and version 1
  inputs are rejected. Decoding creates unaccepted inputs; the ordinary Solve
  queue calls the same formation rule. A two-arena fixture verifies relocated
  nonempty rows and acceptance only after Solve, plus missing-row and old-version
  rejection. Its descriptor resolver explicitly supplies the destination row;
  this is not yet general operation-signature or row-schema serialization.
  Regular `check` passes; `check-acceptance` passes eight unchanged examples
  and six execution fixtures before the existing open-family failure at 88
  transitions. Complete program-image CHECKPOINT support remains open.
  The rebuilt ASan/UBSan component suite also passes, including the new
  transport rejection fixtures and three-dimensional action tests.
- [x] Extend existing zero-clause `PG_FOLD_ELIM` to closed-row sequencing:
  `M : F E A`, `K : Pi(x:A,F G B)` with B independent of x gives
  `fold(M,K) : F (E union G) B`. Classifier recovery reconstructs that union,
  rather than returning K's codomain and losing E. Formation inversion
  `PG_RETURN_CONTENT` now extracts the *type* A from `F E A` for any closed E;
  unlike `PG_RETURN_VALUE`, this neither executes M nor extracts its value.
  This corrects the preceding overly restrictive formation-inversion test.
  Tests cover distinct-row union, exact evidence reuse, classifier recovery,
  rejection of value extraction and two-arena derivation transport through
  ordinary Solve. No new Core kind, proof rule or effect-state cache is added.
  The full ASan/UBSan component suite passes. The final exact-classifier reuse
  fast path additionally passes the rebuilt sanitizer Core suite; it retains
  the original continuation-codomain formation when the union changes nothing.
  Final optimized acceptance passes components, eight examples and six result
  fixtures, then fails the unchanged open-family admission at 88 transitions.
- [ ] Extend nonempty-row sequencing to general computation carriers if the
  chosen effect semantics requires it. Existing pure-source sequencing into
  raw Pi is retained; a nonempty source into raw Pi is rejected rather than
  erasing effects or moving their execution underneath a Lambda. This remains
  an explicit admission limitation, not a claim of complete CBPV effects.
- [x] Add directed closed-row subsumption as `PG_EFFECT_SUBSUMPTION`:
  `M : F E A`, a checked formation of `F G A`, and `E subset G` yield the
  same computation occurrence at `F G A`. Rows here bound possible requests;
  they are not assertions that every listed operation executes. This does not
  add requests, normalize code, identify types by DefEq, or remove effects.
  Both source evidence and target formation remain immutable premises. The
  existing derivation dispatcher and codec transport this named kernel rule;
  no additional row payload or acceptance flag is serialized for it.
  Classifier recovery uses the retained target formation directly.
  Inclusion uses one allocation-free merge scan over canonical closed sets;
  NULL remains invalid. Tests cover all 64 pairs of three-label subsets,
  directed widening, unchanged occurrence, exact proof reuse, changed-result
  rejection, context mismatch, narrowing rejection, and two-arena ordinary
  Solve of a widened RETURN. This supplies an explicit rule, not automatic
  effect inference or permission for expected types to guide synthesis.
  Optimized components, eight examples and six execution fixtures pass;
  full acceptance still fails open-family at 88 transitions. The complete
  rebuilt ASan/UBSan component suite passes with this rule and its transport.
- [x] Connect explicit closed-row subsumption to ordinary post-synthesis
  expectation. For exposed `F E A` and target `F G B` with `E subset G`,
  first check conversion against `F E B`, then retain the directed widening
  proof to `F G B`. The conversion job remains symmetric and unchanged.
  Source `M :: T` uses that same expectation job; neither producer receives
  the expected type, and no RETURN/THUNK/FORCE or runtime wrapper is inserted.
  Tests cover pending independent producers, repeated job reuse, source `::`,
  unchanged producer evidence/Core, rejection of narrowing and a different
  result type, and the fact that the widened classifiers are not DefEq.
  This does not yet infer row metavariables or expose a neutral computation
  classifier merely from its expected type. Non-exposed heads still follow
  ordinary conversion; general subsumption through such heads remains open.
  Optimized components, eight examples and six result fixtures pass; the
  open-family gate remains unsupported at 88 transitions.
  The rebuilt ASan/UBSan synthesis suite passes, including this source
  expectation fixture and the existing three-dimensional application actions.
- [x] Add closed generative operation declarations and `PG_REQUEST_INTRO`.
  Each declaration has a fresh opaque label and retains exactly its checked
  payload/response type formations. Both must be closed value types. Reusing
  the declaration preserves identity; equal signatures do not merge distinct
  declarations, and callers cannot reassign a label to another signature.
  A request checks its value payload against A and its raw continuation against
  `Pi(B,F E C)` with constant codomain, then produces `F ({op} union E) C`.
  The proof retains signature formations, payload and continuation premises;
  its nominal declaration key is not an executable callback or success flag.
  Classifier recovery uses the same continuation-codomain/row reconstruction
  as sequencing. Core remains the existing request APP spine.
- [x] Derive `Lambda a. request op a (Lambda b. RETURN b)` using ordinary
  evidence rules. Like constructor wrappers, construct it once per named
  producer; fresh lexical binders are not alpha-interned. Driver-supplied
  operation names and aliases now work through ordinary source application,
  including `{x := Op Arg; x;}`. Tests check effect retention and that pure
  normalization reaches an inert request with the same label and payload.
  Additional tests cover distinct nominal identities, exact request-proof
  reuse, wrong payload/response types, quoted-continuation rejection, open
  signatures, foreign proof stores, continuation effect union and regularity.
  Optimized components, eight unchanged examples and six result fixtures pass;
  full acceptance still fails open-family at 88 transitions. The complete
  rebuilt ASan/UBSan component suite passes, including typed requests and
  driver-supplied source operation applications/aliases/sequencing.
- [ ] Transport nominal operation declarations and their signature premises
  before admitting `PG_REQUEST_INTRO` into the derivation codec. It currently
  rejects export rather than reconstructing acceptance from a raw label.
  Parameterized/dependent signatures, declaration surface syntax, host request
  handling and higher action on these proofs remain open. The source tests
  use driver-supplied checked operations, not implemented terminal intrinsics.
- [x] Add nondependent multi-clause `PG_HANDLER_ELIM` over the existing raw
  deep fold. A checked carrier `F G C` is explicit; source, return and clause
  terms are already synthesized evidence, not terms inferred from that carrier.
  Return has domain equal to the source result type and returns C within G.
  A clause for `op : A -> B` takes A and `U(Pi(B,F G C))`, then returns C
  within G. Local payload/resumption binders may not escape in the carrier.
  Check `input_effects minus handled_labels subset G`; every clause and return
  effect must also fit G. Duplicate labels are rejected by the shared raw
  layout builder. Zero clauses use existing sequencing and subsumption rules.
  Formation of the result is the retained carrier premise; no copied classifier
  authority or second execution engine is introduced.
  Closed-set difference shares the existing row interner and a sorted scan.
  Tests cover all 64 three-label set differences, two-request deep resumption,
  clause order, duplicate rejection, unhandled forwarding, wrong resumption
  effects, unreported clause/return effects, zero clauses and simultaneous
  operation swaps without recapturing clause-emitted requests.
  Optimized components, eight examples and six result fixtures pass before
  the unchanged open-family failure at 88 transitions. The complete rebuilt
  ASan/UBSan component suite passes, including the typed handler regressions.
- [x] Centralize handler binder-context construction in
  `pg_prove_handler_context`: from the selected operation and checked carrier,
  extend the existing context by payload:A and resume:U(Pi(B,carrier)). Use
  ordinary context extension, projection, Pi and U formation, not another
  environment representation or a new proof rule. This establishes only the
  binder types; body synthesis and final handler checking remain separate.
  Replace the test's manual construction by this helper and ordinary
  `pg_prove_abstract`. Test an open enclosing context, invalid carriers,
  context mismatch and duplicate binders. A source `k req` body is synthesized
  under these bindings, abstracted, checked as a handler clause and executed
  with a source operation application and return clause.
  Optimized components, eight examples and six execution fixtures pass before
  the unchanged open-family failure at 88 transitions. Rebuilt ASan/UBSan Core
  and synthesis suites pass. Whole `M @...` handler elaboration is not yet
  connected; this test supplies the checked carrier and selected declaration.
- [ ] Infer/check the carrier and clause binders from surface handler syntax
  using constraints, and transport nominal handler evidence. The typed kernel
  entry currently requires explicit checked clause functions and a carrier;
  the derivation codec rejects it until nominal signature relocation exists.
  Dependent carriers, row metavariables, handler capabilities, and higher
  action on handler evidence remain open.
- [ ] Add row constraints,
  source application/handler elaboration and signature transport. This step
  supplies the shared representation; it does not yet admit effectful source
  programs, prove termination from an empty row, or implement open row metas.

Delta excluding documentation: `classifier.c` +132/-3, `classifier.h` +16/-0,
`tests/core.c` +48/-0. No Core tag or value-side Pi is introduced.

### September 8 inert requests and fold forwarding

- [x] Represent requests as `APP(APP(APP(request, label), payload), k)`.
  The fixed request object has descriptor `kernel/request/v1`; operation
  identity is the label pointer. Payload and continuation remain visible Core
  edges. Builders/views neither establish a signature nor accept a judgement.
- [x] Extend the existing zero-clause fold reducer with
  `fold(request op a k, R) = request op a (lambda x. fold(k x, R))`.
  Capture R through the ordinary evaluator closure, without evaluating it.
  Reuse the existing demand/readback machinery. Beta-only evaluation remains
  neutral; the structural pure policy never executes a host operation.
- [x] Test exact request interning, distinct same-class labels, two sequential
  forwarded requests, captured R, divergent payload/R remaining suspended,
  split budgets and restarting every pending readback. Explicit alpha comparison
  checks reified binders; readback is not required to intern by alpha equality.
  Optimized acceptance passes components, eight source examples and six result
  fixtures before the unchanged open-family failure at 86 steps. Rebuilt
  ASan/UBSan core tests pass.
- [x] Extend the raw reducer with a simultaneous operation-clause set after
  `469a815`. `pg_computation_fold` constructs the existing zero-clause form or
  an exact-pointer-interned clause layout applied to M, R and the clause terms.
  Layouts hold only label/argument-position pairs; all executable code and
  captured terms remain ordinary APP operands. Label selection is binary
  search, duplicate labels are rejected before publishing a layout.
  The same fold callback handles RETURN, selected requests and forwarding.
  A selected clause receives payload and `THUNK(lambda x. H(k x))`; its body
  runs outside H. Unhandled requests receive the raw `lambda x. H(k x)`.
  No nested one-clause translation, host callback or new Core kind is added.
- [x] Verify simultaneous two-label swaps without recapture, deep continuation
  invocation, unhandled forwarding back into H, two invocations of one
  continuation, an unused divergent clause, exact layout reuse and pending
  readback restart. Components, eight examples and six result fixtures pass;
  full acceptance still fails open-family at 86 transitions. Rebuilt ASan/UBSan
  core tests pass. This extension changes `computation.c` by +123/-14,
  `computation.h` by +11/-0 and `tests/core.c` by +67/-0; docs excluded.
- [ ] Clause-layout relocation and typed/surface admission remain unimplemented.
- [x] After `eec2f5c`, move runtime continuation-template construction to the
  existing `pg_eval_defer` worker protocol. Binder allocation, argument-spine
  construction and abstraction advance incrementally; pending readback keeps
  the caller and cancellation releases the evaluator-owned temporary state.
  A 256-clause regression fails on the preceding synchronous builder because
  one advance creates more than 16 Core nodes; it passes with the worker.
  Bulk/split step counts agree, and cancellation during construction is tested.
  This does not bound allocation/hash-table cost or the existing synchronous
  argument-arity/consumption scans in the evaluator interface. No new queue,
  typing rule or semantic reduction equation is introduced.
  Components, eight examples and six result fixtures pass; the complete gate
  still fails open-family at 86 transitions. Rebuilt ASan/UBSan core tests pass.
  Implementation +74/-23 and tests
  +34/-0, excluding documentation.
- [ ] Add checked operation signatures, effect rows, source alias/application
  elaboration, typed fold output-carrier rules, and explicit runtime handlers.
  Requests currently have no source admission rule. This does not establish
  effectful conversion, higher action on requests, or nominal image transport.

Delta excluding documentation: `computation.c` +50/-3, `computation.h` +12/-1,
`tests/core.c` +73/-0. N4 and the full goal remain incomplete.

### September 8 classifier recovery stack removal

- [x] Replace recursive calls in `pg_prove_classifier` with an iterative walk
  along retained premises and temporary continuation frames. Formation leaves
  and reconstruction still use the existing kernel rules. No classifier cache,
  new proof rule, Core tag, evaluator or Replay path is added.
- [x] Test 40,000 retained-premise steps from nested RETURN/THUNK evidence;
  the recovered formation has the exact original classifier Core. A second
  recovery returns the same evidence without adding Core terms or proofs.
  The temporary traversal is still repeated; this does not claim zero work.
- [x] Run component `check`, rebuilt ASan/UBSan core tests, and the complete
  acceptance command. Components, eight unchanged examples and six main-result
  checks pass. Acceptance still fails at open-family after 86 transitions.
- [ ] Recovery remains synchronous and uses temporary memory proportional to
  the traversed premise depth. This change removes its C-stack dependence,
  not all synchronous kernel work or the pending open-family formation rule.

Delta excluding documentation: `evidence.c` +84/-45, `tests/core.c` +30/-0.
General indexed formation and Pi-shaped IH still require their planned rules;
the zero-index restrictions were not removed to manufacture acceptance.

### Source acceptance recheck after `9e1984a`

The recent storage work does not close source IADT admission. Do not use the
green component `check` target as evidence that the replacement runs existing
programs. `make -f src/prototype/pointer/Makefile check-examples` now invokes
ordinary `pointer-check` on every existing 01--09 example, with one million
Solve transitions per file. It requires success, not an expected unsupported
baseline. `check-acceptance` combines this gate with the component suite.
It is a necessary gate, not sufficient evidence for execution, effects, IF8,
higher coherence or full `.a` support.

`check-example-results` now executes the six unchanged examples with `main`
(03, 04, 05, 06, 07, 09) through their accepted evidence and the common pure
NF jobs. It checks Bool.true, Bool.true, Nat 1, Nat 0, Nat 5 and Nat 3
respectively. Each file is solved/executed with chunks of one and 10,000
transitions. Expected constructors are resolved in the source declaration and
applied with checked APP/RETURN operations; constructor ordinals, host integers
and unrelated nominal types are not substituted for the result. This gate is
part of `check-acceptance`, independently of the typechecking-only gate.
01/02 have no main and remain library/typechecking fixtures. This is pure
execution coverage, not a completed host-effect CLI or `.a` execution path.
Verification after `c3c5317`: all six result fixtures passed in optimized and
ASan/UBSan builds, with both budgets. Full `check-acceptance` passed components,
01--09 typechecking and these result checks before failing open-family. Delta:
test code +90/-1, prototype Makefile +10/-2; no kernel/runtime change.

After restoring computational references to implicitly quoted definitions,
the existing 01--09 source gate is **8/8**. This checks typing, not execution
results. Full `check-acceptance` still fails the open-family fixture
(`unsupported steps=86`); source compatibility remains incomplete:

| Example | Status | Solve transitions |
| --- | --- | ---: |
| 01_bool | done | 163 |
| 02_nat | done | 64 |
| 03_main | done | 163 |
| 04_match | done | 386 |
| 05_bool_to_nat | done | 250 |
| 06_pred | done | 199 |
| 07_add | done | 450 |
| 09_list_induction | done | 944 |

### Applied family provenance progress

- [x] Recover a known Lambda body through retained typed substitutions and
  force/thunk introductions with `pg_prove_application_body`. This derived
  helper uses existing substitution and reindex rules, not a new Core form
  or a lookup from erased terms to classifiers.
- [x] Follow returned, normalized type-family applications in
  `pg_inductive_instance`. The reconstructed nominal instance must still
  match the requested type; evidence traversal does not equate arbitrary
  normalized terms or change pointer interning.
- [x] Check source Match over `List Nat`, repeated proof reuse, the resulting
  constructor reduction and rejection of an argument from another nominal
  family with the same constructor shape.
- [x] Resolve members of applied families such as `(List Bool).nil`.
  Constructor producer keys now contain the formation, constructor pointer
  and checked parameter substitution. Ordinary declarations use the identity
  substitution; applied families use their recovered substitution. Both use
  the same constructor function builder. Non-atomic qualified roots request
  ordinary synthesis, and pure type computations use existing RETURN jobs.
  Declaration exports provide names only, not classifier authority.
- [x] Expand nested retained Pi codomain eliminations with an iterative
  substitution stack. This permits direct Match on a curried constructor's
  result, without a special List rule or erased-Core classifier lookup.
- [ ] Support general computed-function provenance and recursive IH; the
  current helper only follows retained Lambda introductions. Unavailable
  provenance is not evidence that the program is ill-typed.

Verification: optimized component `check` and ASan/UBSan synthesis passed
after this integration. Tests include applied members, projected contexts,
computed family aliases, missing members, wrong nominal arguments and
evaluation of a direct constructor Match to `Nat.zero`. The unchanged source
gate is 6/8 as listed above. IADT ASan/UBSan also passed at the preceding
application-provenance checkpoint. This is not full acceptance or completion
of the rewrite.

### Recursive elimination implementation boundary

- [x] Add `pg_data_recursive_match` as an erased template builder. A free
  recursive-function binder in branch terms is closed by ordinary untyped
  Lambda/Application self-application; the existing pointer-labelled matcher
  still selects cases. There is no new Core tag, hidden closure payload,
  evaluator recursion loop or accepted fixed-point typing axiom.
- [x] Exercise 64 recursive steps with one-step normalization budgets, lexical
  capture, an unselected divergent branch and invalid template inputs.
- [x] Introduce `pg_prove_induction_scope` for direct zero-index Self fields.
  It projects the existing field substitution into a context extended by
  `IH : U(M(field))` assumptions, in field order. The motive substitution is
  checked even for a dependent Identity-valued motive. Assumptions cannot be
  projected out of their scope. Unsupported recursive shapes are rejected by
  this helper rather than silently treated as nonrecursive fields.
- [x] Add `PG_INDUCTION_ELIM` / `pg_prove_induction` for that direct fragment.
  Case and induction retain distinct rules while sharing nominal scrutinee,
  motive, coverage and branch-classifier checks. Branches abstract fields then
  IH values; no unrestricted recursive-function typing assumption is exposed.
  A shared field classification governs both IH formation and erasure.
  Erasure supplies `thunk(rec field)` only for those declared recursive fields.
  Repeated accepted requests reuse the proof before creating fresh scopes.
  Classifier inversion retains the output formation. Wire encoding explicitly
  remains unsupported until nominal schema transport is implemented.
- [x] Add the shared `pg_synthesis_induction_branch` job for a known checked
  motive. It builds the field/IH telescope once, synthesizes the clause body
  independently and abstracts the resulting function. Whole-induction branch
  checking remains in the kernel. Source `*k` selects the IH by the resolved
  field binder, then uses ordinary FORCE. Shadowing `k` does not select an
  outer IH. Conditional declaration Self application retains its old meaning.
  Scope interning includes the IH-to-field association; it is lexical metadata,
  not another accepted classifier. Direct-field classification is shared by
  source binding, kernel IH formation and erasure.
- [x] Fix marked-operand precedence: `f *x` now parses as `f (*x)`, rather
  than `(f *) x`. The marker remains an ordinary source APP shape, so
  declaration `* i` still resolves as indexed Self. Bare terminal Self is
  unchanged. Parser tests cover spacing/parentheses; an explicit-motive
  induction test synthesizes `Nat.succ *k` and normalizes to the original Nat.
- [x] Connect ordinary Match to direct IH branch jobs for independently
  synthesized constant motives. Lexical marker dependency discovery delays
  only branches using their own fields' IHs. Independent branch producers
  establish the motive; the checked induction scope then supplies IH types.
  Existing fields-only branch proofs are adapted by ordinary projection,
  application and abstraction, not synthesized twice. Discovery respects
  Lambda/block/pattern shadowing and the selected block prefix. Its traversal
  is currently synchronous and must be included in future fuel accounting.
- [ ] Generalize motive solving beyond the independent constant seed fragment.
  No independent seed or a dependent result remains unsupported. Do not install
  an expected result as a synthesized motive or retry accepted proofs with a
  different classifier. Retain actual branch constraints and dependencies.
- [x] Extract `pg_synthesis_constant_motive` as a shared producer keyed by the
  destination context, field context and independent body job. Ordinary Match
  now consumes this producer instead of reimplementing binder removal. Each
  Pi codomain independence step advances separately on the Solve queue. The
  accepted branch function and resulting formation are retained by that job;
  no expected type is an input. Remove the redundant stored field count from
  Match branches. A genuinely field-dependent result remains unsupported by
  this constant-motive fragment. This is not a general unification solver or
  evidence that an arbitrary branch equation uniquely determines a motive.
- [ ] Add a typed induction rule with retained field/IH contexts and motive
  instantiations. The raw recursive-function binder must NOT become an
  unrestricted source binding. A source `*k` must be justified by the admitted
  recursive field and its telescope, including Pi-shaped recursive fields for
  Acc; a same-typed arbitrary argument is not a structural decrease proof.
- [ ] Derive the branch IH computation classifier from that field and the
  motive. Source synthesis must retain unresolved motive constraints when
  necessary, rather than accepting an expected type as synthesis evidence.
- [x] Instantiate the erased template from the checked direct induction
  derivation; verify a two-step Nat countdown and rejection of swapped case/
  induction branch signatures. No unrestricted fixed-point typing rule added.
- [ ] Generalize beyond direct recursive fields and establish higher induction.
  Ordinary substitution keeps branch/captured operands visible;
  record all field and IH premises in the immutable proof DAG. Establish the
  selected-constructor computation rule and dimensional action compatibility
  before claiming typed higher induction or artifact support.

The raw builder intentionally accepts templates that can diverge: Core is
untyped. Its existence proves neither termination nor datatype fibrancy.
It is not exposed as a source-language general recursion primitive. Existing
07/09 acceptance remains open: the source connection below does not close
definition-use compatibility. Indexed/Pi-shaped recursive IH and general higher
induction remain open.
Verification: optimized `check` and ASan/UBSan IADT tests passed; the source
acceptance gate remains 6/8 with unchanged transition counts.
The subsequent source-IH integration passed optimized `check`, the rebuilt
synthesis test including function-valued motives, and ASan/UBSan synthesis.
Tests connect parsed `*k` and `*k m` branches to checked induction and execution,
verify request reuse and reject accidental capture after field-name shadowing.
The ordinary source gate remains 6/8; these explicit-motive tests do not replace it.
The marked-operand correction also passed optimized `check` and ASan/UBSan
reader/synthesis tests. The source gate still has the same 6/8 result.
The constant-motive producer passed optimized `check` and ASan/UBSan synthesis,
including pending requests, reuse and a field-dependent rejection boundary.
The source gate remains 6/8; its current transition counts are in the top table.

September 8, after `54e99b4`: ordinary source tests now construct and evaluate
Nat copy, reordered clauses, addition with a raw Pi motive, an IH used inside
a sequential block, and `List Nat` length. A block selector excludes an unused
tail containing `*k`; shadowed Lambda/block names cannot capture an outer IH.
These are computation-result tests, not only acceptance tests.

The broader gate is still incomplete. Keep the following obstructions open:

- [x] Definition quotation and use policy: storage still quotes a raw
  computation under the implicit policy. A source reference to such a producer
  uses ordinary FORCE before projection, preserving its original computational
  meaning. The producer's independently synthesized RHS determines this step;
  no expected classifier is consulted and no new mutable polarity flag exists.
  Explicitly quoted definitions remain values. Strict mode still rejects raw
  computation definitions. Definition selection/storage APIs retain their
  stored values. Tests cover computed arguments, aliases, sequential blocks,
  explicitly quoting a named function for higher-order use, post-checks and
  rejection of explicit returning thunks used as ordinary value arguments.
- [x] Nominal provenance through constant codomain elimination of a retained
  `PG_PI_FORM`: the traversal retains the context-removal boundary, then
  reconstructs the nominal parameter substitution in its parent context.
  Images must have an existing proof there (possibly after stripping retained
  projection/reindex wrappers or following an exact variable image). Both Core
  and classifier are checked against the original image before accepting the
  recovered proof; the entire substitution is checked normally afterwards.
  This is not general strengthening or untyped erasure of a context dependency.
  Tests directly Match nested computed Nat/List constructors, verify recursive
  results, and accept an open `List A` parameter in the same path.
- [ ] Generalize constant-codomain provenance when the Pi formation itself is
  derived rather than a retained `PG_PI_FORM`, and image recovery beyond the
  supported retained unary/substitution spine. Missing evidence remains
  unsupported; do not manufacture a parameter proof from its erased Core.
  September 9, after `1eb7977`: recovery now commutes constant-codomain
  elimination with retained projection/reindex and expands retained curried
  Pi elimination using its original formation and argument. A regression
  reproduced failure on projected Pi before the change; all three cases now
  recover the same nominal Nat and parameter context. Both original and
  reindexed genuinely dependent Pi types
  still reject constant-codomain elimination. The worker reuses existing rules
  and retains its final context/endpoint checks; it does not infer a declaration
  from Core. Arbitrary converted/normalized Pi provenance and broader image
  recovery remain open, so this overall item is not checked off.
  Normal components, eight source checks and six runtime fixtures pass with
  unchanged example transition counts. Rebuilt ASan/UBSan IADT and synthesis
  tests pass. Implementation C: +14/-0; tests:
  +28/-0; documentation separate.
- [x] Nominal provenance of recursive fields through the checked Self map:
  `pg_substitution_image` borrows the accepted image of an exact source binder.
  The retained-evidence traversal consumes one substitution/projection frame
  when it reaches a variable, rather than trying to discover a type from Core.
  The same operation supports retained function-body provenance. Open variables
  without an image remain unresolved. No new Core/proof rule or answer registry
  is introduced. Source tests now evaluate nested Match using an outer IH and
  an inner shadowing IH, both over two successors. Image lookup tests check
  absent/invalid inputs and no new term/proof allocation. The current lookup
  is linear in a substitution's stored bindings, not a new persistent index.

The original constant-codomain and recursive-field fixtures are now execution
success tests. General derived-Pi provenance and open family result formation
remain separate unmet requirements.
Verification of this connection: optimized `check` and rebuilt ASan/UBSan
`synthesis_test` passed. `check-examples` still fails 07/09 as recorded above.
Implementation delta: `evidence.c` +16, `evidence.h` +7, `synthesis.c` +175/-14;
tests +42. Documentation is counted separately. No Main promotion or full
goal-completion claim is justified by this checkpoint.
After `a6aa273`, the substitution-image correction passed optimized `check`
and rebuilt ASan/UBSan `synthesis_test`, including a substituted function thunk
whose Lambda body is recovered without evaluation. An open function variable
without a supplied image still has no recoverable body. Source acceptance is
unchanged at 6/8. Delta: implementation +36 lines; tests +38/-5; documentation
separate. The constant-codomain and definition-use issues remain open.
After `12a4bcc`, the definition-use correction above closes the 01--09 gate
without changing its source files. The constant-codomain provenance issue and
general open type-family synthesis remain open. Implementation delta +6 lines;
tests +12/-1; documentation separate. No independent Replay path was added.
Optimized component tests and rebuilt ASan/UBSan synthesis passed.
`check-acceptance` reached the open-family failure after passing those component
tests and all eight unchanged examples; it did not pass as a whole.
After `17e202a`, retained constant-codomain provenance passed component tests,
all eight source checks, all six example results, and rebuilt ASan/UBSan
synthesis. Full acceptance still fails open-family at 86 transitions.
Implementation delta: `evidence.c` +47; tests +10/-4; documentation separate.
Context image recovery is an explicit synchronous traversal; budgeting and
sharing of the broader derived-formation normalization remain required.

Code-level obstruction and implementation order:

1. Ordinary DECLARATION dispatch now admits checked zero-index families.
   Qualified constructor names now use its isolated source export scope.
   Nonrecursive ELIMINATION handles value and supported computation scrutinees
   with constant computation motives. Known zero-index family applications
   now expose their constructors. Indexed declarations and arbitrary computed
   family provenance remain unsupported. The separate `pg_synthesis_data_schema` job still builds
   a conditional schema only; its completion never implies type admission.
2. `pg_data_schema` in `iadt.c` validates field/result substitutions and derives
   erased layout arities. Zero-index nominal formation, constructor membership
   and dependent case elimination now have evidence rules (see the progress
   section below). Indexed formation, recursive IH and source integration remain
   missing. Connecting syntax directly to an unadmitted schema is still invalid.
3. Implement recursive Self-family checking with the fixed parameter prefix
   and explicit indices, strict positivity, universe obligations and retained
   evidence. The source declaration's own name must not substitute for `*`.
   Keep this information in the typed declaration; the Core layout remains
   an erased pointer-labelled execution descriptor.
4. Establish nominal family formation and constructor membership from the
   admitted schema, then expose those derivations through ordinary synthesis.
   Do not assert general indexed fibrancy from a result substitution or an
   erased matcher; the N2/N3 higher obligations below remain prerequisites
   wherever a rule depends on them.
5. Add typed Match/IH with independently synthesized branches, retained motive
   checking and subject reduction through typed substitution. Only then wire
   source elimination to those rules and require this source gate to pass.
   Add execution-result and negative-admission checks as further gates, not as
   replacements for checking the unchanged sources.

Prioritize these missing admission rules and general typed symmetry over more
isolated storage conveniences. The failing gate is intentionally retained;
unsupported must not be changed to success without the required evidence.

- [x] After `9d6d361`, implement `pg_data_field_positive` as the conservative
  syntactic condition needed by recursive field admission. It follows existing
  Pi/F/U views and uses existing binder-aware independence checking. Self must
  be a dedicated binder; a recursive application must supply exactly the index
  arity, with Self-independent index arguments. Pi domains must be independent;
  only codomains may recurse. Unknown constructors receive no guessed variance.
  The traversal is iterative, adds no graph tags or persistent schema cache,
  and does not normalize or allocate accepted evidence. Zero means this check
  did not establish the condition, not proof of semantic impossibility.
  Tests cover direct/indexed and Acc-shaped output recursion, negative and
  double-negative fields, opaque applications, lexical shadowing, a redex
  which would discard Self, invalid inputs and 10,000 F/U wrapper pairs.
  Optimized and ASan/UBSan IADT suites pass. Full source acceptance remains
  failing and was not reclassified as a successful negative test.
- [ ] Integrate this condition with checked, scoped Self-family formation and
  declaration admission. A positive raw expression need not itself be a valid
  classifier; formation, universe bounds and higher obligations must still be
  established. Nested positive type constructors need checked variance rules,
  not blanket acceptance. No source declaration is admitted by this helper
  alone; the 0/8 source gate is not fixed by this isolated prerequisite.

Positivity integration audit after `c1e520f`:

While auditing nominal Self formation, rechecked an ordinary open family after
`855f75a`: `f := &(\F : (@ -> @) => \A : @ => \x : (F A) => x);`
returns unsupported after 86 transitions. This is not a parser or fuel failure.
`type_input` requires `pg_synthesis_return`; the neutral application of the
function parameter cannot expose a canonical RETURN. Thus ordinary symbolic
type-family use is missing independently of recursive datatype admission.

- [ ] Close ordinary open type-family use. `check-open-families` checks the new
  source fixture and is part of
  `check-acceptance`; unsupported remains failure. This concretizes the existing
  N2 neutral/open pure family obligation below, rather than a new feature request.
- [ ] Define the retained formation/totality requirements for observing an open
  pure family result as a type. A `Comp A` classifier alone must not silently
  give a value of A. Preserve symbolic dependence without executing a future
  effect, and keep `::` post-synthesis. Do not add ValuePi or an unproved
  computation-to-value coercion to make this example green.

Dependency correction after `8a53197`: that open-function issue is not a
prerequisite for every nominal family formation. `data_result_start` already
checks `* indices` by building a substitution into the declared index context;
it does not execute an arbitrary function returning a Universe value. The two
paths share ordinary typed substitution, not necessarily an elimination rule.
The earlier requirement that both use the same rule was too strong.

#### September 8 open-family rule audit

Revalidated at `e4e3c51`: `make -f src/prototype/pointer/Makefile
check-open-families` fails with `unsupported steps=86`. This positive acceptance
requirement remains open. More fuel, artifact loading, or another Replay path
cannot supply the missing formation rule.

Rule boundary audited at `e4e3c51`, with the closed-row update noted below:

| Location | Established fact | Missing fact |
| --- | --- | --- |
| `classifier.h:pg_return_type` | A computation has a specified result type; the later closed-row migration explicitly selects the empty effect set | A totality/stable-result contract is still missing; an empty effect set alone does not provide it |
| `evidence.c:pg_prove_application` | Applying a checked Pi substitutes the argument into its codomain | It does not extract a value from the resulting computation |
| `evidence.c:pg_prove_return_value` | An accepted canonical RETURN exposes its argument | A neutral computation is not a RETURN constructor |
| `synthesis.c:type_input` | Type use waits for the ordinary returned-value job | An open family cannot presently form a symbolic result type |
| `tests/synthesis.c`, neutral `m` fixture | Unknown `m : U(F A)` cannot be unthunked or observed as a returned value; substitution of a concrete thunk later permits progress | This negative test is not a solution to the positive open-family requirement |

Reference inspected: Matthijs Vakar, *An Effectful Treatment of Dependent
Types*, arXiv:1603.04298v1, sections 1--3 and abstract,
<https://arxiv.org/html/1603.04298> (accessed September 8, 2026).
The paper distinguishes dependence on values, including thunks, from dependence
on computations. Its dCBPV+ adds dependent Kleisli extension; normalization and
subject reduction depend on the effects admitted. This is not a general license
to turn an arbitrary computation into a value.

The following is an A Program design obligation, not a theorem imported from
that paper. To admit the existing open-family syntax, its function parameter
must carry enough information to justify symbolic type use under every allowed
substitution. Merely observing that today's implementations contain no typed
print operation does not establish that contract. Purity alone also does not
establish termination or production of a result.

Implement this prerequisite in the following order:

- [ ] Specify the contract of the source arrow used by an open type family.
  Decide whether it denotes a pure total fragment, or carries a separately
  justified stability/result obligation. State its introduction and application
  rules before adding a kernel constructor. Do not infer this contract from
  `::`, a closed example's normal form, or a scan for operation nodes.
- [ ] Specify symbolic result formation and substitution together. Under a
  substitution that makes the computation `RETURN A`, its symbolic result must
  agree with A. Substitution of an effectful or partial computation must not
  satisfy a stronger contract without evidence. No universal `U(F A) -> A`
  coercion follows from the present rules.
- [ ] Establish compatibility with dependent Pi formation, ordinary reindex,
  and the accepted induction fragment. A successful syntactic purity check
  alone is not a totality argument. Keep one Pi representation and the three
  existing Core forms; place justification above Core, not in pointer interning.
- [ ] Define the rule's Act behavior and relocatable premise structure before
  accepting it in saved derivations. Use the same kernel rule from source and
  loaded inputs. A serialized flag must not supply the missing contract.
- [ ] Add positive open-family tests, substitution-to-RETURN agreement, nested
  family dependence and `::` non-interference tests. Retain the unrestricted
  neutral-computation negative fixture; add effect/partiality counterexamples
  when their checked syntax is available. Run the full acceptance gate, not
  only the new fixture.

No new rule or claimed totality theorem is introduced by this audit. The
remaining choice concerns what the annotated family promises, not a need for
a second value-side evaluator, ValuePi, or a separate Replay engine.

Next declaration-admission contract:

- [x] After `20148f3`, extract `pg_data_signature` from the schema's parameter
  and index context references. Its constructor checks ownership and the exact
  context-prefix relation; it adds no accepted evidence or Core object. The
  declaration producer builds it after the index telescope and before any
  constructor producer. `pg_data_schema` now takes this signature instead of
  repeating the two contexts; no compatibility overload or copied context
  arrays remain. Existing result-map checks and erased layouts are unchanged.
  Tests reuse one signature for two fresh schemas, check distinct constructor
  identities and unchanged proof counts, and reject foreign owners, invalid
  prefixes and malformed result maps. This is the scoped signature preparation,
  not Self-family formation, a Universe axiom, or completed source admission.
  Full component `make check` and ASan/UBSan IADT tests pass. This is not a
  passing `check-acceptance`; its source admission obligations remain open.
- [ ] Add the scoped Self assumption and its discharge using that signature;
  the preparation above intentionally carries no acceptance flag or mutable
  constructor list. Universe bounds and formation evidence are still required.
- [x] After `78f3b53`, instantiate a signature's index telescope from a checked
  parameter substitution and independently checked index values. Share the
  suffix assembly with constructor field instantiation through
  `pg_prove_substitution_extend`, which delegates to the existing simultaneous
  substitution rule. The full typed context determines the remaining arity;
  constructor instantiation no longer reads the erased runtime arity for this
  semantic check. No new evidence rule or normalization policy is introduced.
  Tests instantiate dependent `(i : A, q : Id A i i)` before constructing a
  schema, compare its images with the constructor result-map composition,
  preserve zero-index identity and repeated evidence sharing, and reject
  mismatched counts, wrong dependent values, foreign stores and invalid maps.
  This produces substitution evidence only, not nominal type formation.
  Full component `make check` and ASan/UBSan IADT tests pass. Source declaration
  acceptance, typed symmetry and the other full-plan gates remain incomplete.
- [x] After `44c61ca`, remove repeated validation of the accepted substitution
  prefix. Full substitution and extension use one builder and the same flat
  evidence key; extension checks only new images and their dependent domains.
  The prefix must belong to this typing store and its exact source context must
  be a prefix of the supplied context. No accepted flag or new rule is added.
  A regression changes the source context derivation without changing its
  context, retaining a nonidentity substitution through a dependent Pi type:
  extension creates no new Core terms and ordinary construction finds the same
  evidence. Flat premise/binding arrays are still copied, not claimed O(1).
  Full component `make check` and ASan/UBSan IADT tests pass. These tests do not
  establish source datatype admission or completion of `check-acceptance`.
- [x] After `931e731`, connect field positivity to the complete prepared schema
  through `pg_data_schema_positive`. It reads declared field types from the
  existing checked contexts and derives recursive arity from the index
  telescope, without copying a semantic schema. A temporary pointer index
  avoids revisiting shared field-context suffixes across constructors.
  Tests cover empty/null input, shared positive fields and a later constructor
  with a negative function domain; checking allocates no Core or evidence.
  This is a syntactic condition only. It neither scopes/discharges Self nor
  admits a type into a universe. The pending admission item above remains open.
  Component `make check` and rebuilt optimized/ASan/UBSan IADT tests passed.
- [x] After `51c9adb`, derive the constructor-field universe lower bound from
  the existing context-extension formation premises (`pg_data_schema_field_level`).
  No classifier synthesis, Core evaluation or stored copy of the bound is
  added. Empty fields require bound zero; parameters and index domains are
  not constructor fields and are not included. The bound is necessary for
  predicative admission, not a new universe formation or cumulativity proof.
  Positivity and bound extraction share one traversal of retained field
  derivations, with a temporary pointer index. Distinct derivations of the
  same context remain distinct inputs; shared derivation tails are visited once.
  Regressions cover field bounds zero/one, unchanged output on failure,
  no new Core/evidence, and a large index universe with no stored fields.
  Scoped Self, admission/discharge and higher datatype action are still open.
  Component `make check` and ASan/UBSan IADT tests passed; the full source
  acceptance gate remains unfulfilled.

Admission audit after `8b3105b`:

- `constructor_step` synthesizes its telescope before `data_result_start`
  recognizes the final `* indices`. Consequently the latter does not supply
  a Self assumption to field checking. A successful result map is not the
  missing recursive formation rule. Ordinary reference synthesis still rejects
  a field-position `*` as unsupported.
- `pg_prove_context_extension` retains field formation and its concrete
  universe classifier. `pg_universe` currently takes a numeric level; there is
  no symbolic Self-level obligation in this path. Do not check Self at level
  zero, compute a larger field bound afterwards, and change Self's accepted
  classifier. For example a stored universe-valued field requires level one,
  whereas a field of the assumed small type parameter requires level zero.
- Before final recursive evidence construction, retain and solve the Self
  universe obligation along with the scoped signature. Construct the accepted
  premises with the chosen level; no overwrite or unchecked universe lift is
  allowed. The existing field-level helper verifies concrete premises but does
  not implement this pre-admission constraint stage. This is part of the open
  admission task, not a reason to add another cache/helper-only milestone.
- Source regressions compare a directly written universe field with the same
  field produced by pure application under `A : @`, and reject passing `@`
  itself to a function requiring an element of `@`. They check schema bounds,
  not nominal membership, and continue to require no expression proof from a
  schema-only job.
  Optimized and ASan/UBSan synthesis suites passed with these source cases.

### Next Implementation Boundary: Pending Recursive Formation

Zero-index inductive rules after `d952804`:

Source Match after `9e1984a`:

Computed scrutinees after `7ab727d`:

- [x] Open the existing sequencing continuation for a computation scrutinee,
  synthesize branches under its result binder, and close with the same
  continuation/FOLD mechanism used by applications and blocks. A source Match
  does not require the scrutinee to be executed during synthesis.
- [x] Extend nominal provenance recovery through RETURN-type inversion and
  codomain instantiation of a directly retained Pi formation. The latter uses
  ordinary projection, pairing and reindex evidence, not raw Core lookup.
- [x] Test a computation block and a successor application as scrutinees.
  Their synthesized evidence is FOLD, and explicit subsequent normalization
  yields the expected predecessor value with its original nominal classifier.
  Full component `check` and rebuilt ASan/UBSan synthesis tests pass; unchanged
  examples remain 5/8. This is not full source or runtime acceptance.
- [ ] Broaden formation inversion for noncanonical Pi provenance and dependent
  sequencing. This does not complete arbitrary effectful source execution,
  indexed family instances, recursive IH, or dependent motive inference.

- [x] Recover the scrutinee's admitted family from its classifier evidence and
  get lexical labels from that formation's existing source export scope. This
  metadata never determines membership or replaces the semantic schema.
  Check constructor identity and coverage before forming any Match evidence.
- [x] Share instantiated constructor field contexts with introduction; bind
  positional pattern names to their fresh field binders. Synthesize every
  branch without a supplied expected type, then abstract its fields. Recover
  a constant computation motive via Pi codomain inversion and post-check all
  branches through `pg_prove_match`. Raw function results stay computations.
- [x] Resolve layout positions in constant time from the constructor pointer,
  using the same operation as erased Match assembly. Do not scan every
  constructor again for each clause or store another ordinal authority.
- [x] Test reordered and qualified clauses, predecessor reduction, raw-Lambda
  branch results followed by application, wrong/missing/repeated constructors,
  field arity and leakage of a field name into another branch. Existing 05 and
  06 are accepted unchanged; the overall source gate is still incomplete.
  Full component `check` and rebuilt ASan/UBSan synthesis tests pass.
- [ ] Complete general computed-scrutinee provenance, infer dependent motives,
  support named selectors and general label aliases, and implement
  recursive IH. Empty elimination still needs a synthesizable result source;
  `::` must not supply one before synthesis. Differing branch result types
  remain unsupported here, not a claimed inconsistency: a dependent motive
  may relate them. Indexed families and higher equations remain open.

Typed instance recovery after `f50f399`:

- [x] Recover the original nominal formation and checked parameter map from
  formation evidence under reindex, projection, type/value coercions and type
  conversion. Walk the retained proof chain iteratively; compose substitutions
  in order. Check that the reconstructed instance matches the input type.
  No global classifier lookup, new Core tag or new evidence rule is introduced.
- [x] Centralize prefix projection/identity substitution using ordinary
  variable/substitution evidence. Constructor producers and Match motive
  instantiation now share it; the latter is projection followed by pairing.
- [x] Test a variable's nominal classifier, instantiated Box, projection then
  substitution back to the empty context, type/value wrappers, exact repeated
  evidence reuse, invalid projection direction, and unchanged output when
  provenance cannot be recovered.
  Full component `check` and rebuilt ASan/UBSan IADT tests pass. This supplies
  typed instance recovery, not source Match admission or full acceptance.
- [ ] Use this recovery in source Match and general constructor instantiation.
  An unavailable provenance chain is unsupported, not evidence that the type
  is non-inductive. Recovery through arbitrary computation/normalization and
  nominal `.a` transport still require their corresponding typed rules.

Qualified constructor publication after `5833634`:

- [x] Publish constructor producers only after nominal formation succeeds.
  Reuse ordinary source export scopes and name-job resolution; exports contain
  only members, never enclosing lexical names. The producer key includes the
  admitted formation, constructor pointer and parameter context. One producer
  derives one wrapper, shared by references rather than freshening on each use.
- [x] Nullary members synthesize values; members with fields synthesize raw
  curried computations through the existing constructor-function derivation.
  References, definitions, definition selections and successful post-expect
  expressions preserve source exports. This is lexical metadata, not reverse
  classifier lookup by Core pointer or an alternative schema authority.
- [x] Test qualified and post-expect aliases, repeated wrapper identity,
  two nested successor applications and their result classifier, absent
  unqualified names, no lexical fallback from a member lookup, and rejection
  of a different nominal family's constructor argument.
  Full component `check` and rebuilt ASan/UBSan synthesis tests pass. The
  unchanged source gate is 3/8 as recorded above, not full acceptance.
- [ ] Instantiate exported constructor signatures for general applied family
  values; source exports are not yet transported through arbitrary application
  or serialized nominal `.a` descriptors. Source Match/IH remain open.

Source formation after `cf41ee8`:

- [x] Connect zero-index DECLARATION to conditional schema checking and the
  existing inductive formation rule. Start with a concrete Self universe
  candidate zero. If the retained field formations require a greater bound,
  allocate a *new* Self binder/context and schema job at that bound. Only a
  schema passing kernel positivity and universe checks becomes an expression
  result. Do not overwrite an accepted classifier or use `::` as input.
- [x] Test empty and recursive small types, dependent stored-type fields at
  universe one, recursive fields alongside such stored types, nominal
  distinction between source declarations, shared-job reuse, and no premature
  publication while solving. `examples/02_nat.p` is now accepted unchanged.
  Full component `check` and rebuilt ASan/UBSan synthesis tests pass. The
  existing case-producer regression now distinguishes an admitted type used
  incorrectly as a computation from an unsupported indexed declaration.
- [ ] General Self universe obligations: candidate construction can fail before
  a field bound is available, so failure of this search remains unsupported,
  not proof of ill-typedness. This concrete ascending-candidate implementation
  is sound on success, but is not a complete symbolic universe solver. Pending
  indexed formation and higher rules must not be replaced by this fragment.
- [ ] Publish qualified constructors from the admitted typed declaration, then
  connect source Match and recursive IH. Never discover nominal membership by
  looking up an erased constructor Core's classifier.

Dependent case elimination after `581ab95`:

- [x] Add `PG_MATCH_ELIM` for the admitted zero-index families. Given an
  independently formed computation motive in `Delta,z:D`, check every already
  synthesized branch at `Pi(fields, motive[constructor fields/z])`. Substitute
  the scrutinee in the conclusion. No expected-type-guided branch synthesis,
  recursive IH, value-side Pi, or new Core node is introduced.
- [x] Share fresh field-context construction with constructor function
  derivation. Retain the original formation, parameter map, motive/context,
  scrutinee, all branches and output formation as immutable premises. Repeated
  accepted requests use their proof key before fresh field-context construction.
  Alpha checking compares field binders without merging their Core identities.
- [x] Add tests for predecessor iota and typed subject reduction, neutral
  scrutinees under an outer context, an Identity-dependent result motive,
  raw-Pi branch results followed by application, instantiated Box parameters,
  missing/wrong-arity branches, nominal mismatch and repeated-request reuse.
  Full component `check` and rebuilt ASan/UBSan IADT tests pass. A fresh
  `check-examples` still reports 0/8 unsupported at the unchanged transition
  counts above; this is not a completed source-language implementation.
- [ ] Complete source Match dispatch, recursive IH, indexed elimination and
  datatype higher equations. This rule supplies dependent *case analysis*,
  not recursive induction. Ordinary examples are not claimed to pass.
- [ ] Extend nominal `.a` descriptor storage. The new Match rule, like its
  nominal formation premises, is explicitly rejected by the existing generic
  codec until the schema/object transport is implemented. No second Replay
  engine is planned: stored inputs must use the same checked rules as source.

Constructor function derivation after `75c100c`:

- [x] Derive a curried constructor computation with ordinary checked
  substitution lifting, constructor introduction, RETURN, and Pi/Lambda
  abstraction. Fresh field binders avoid capture by the destination parameter
  context. Zero fields gives RETURN; there is no value-side Pi or additional
  kernel rule. This synchronous builder creates a fresh lexical abstraction;
  source integration must request it once per shared constructor producer.
- [x] Test successor application reducing to RETURN of the admitted successor,
  zero-field inversion, and a two-field dependent constructor taking a type
  followed by its element. A named derived successor also works through
  ordinary source application. Constructor names in these source tests are
  supplied explicitly; automatic qualified-name publication remains open.
  Full component `make check` and rebuilt ASan/UBSan IADT/synthesis tests pass.

- [x] Implement `PG_INDUCTIVE_FORM` with retained conditional Self-context and
  constructor result-map premises. The signature must have zero indices and
  end in `Self : Universe_l`; positivity and every stored-field universe bound
  are checked before admission. The conclusion discharges exactly that final
  binder. This is the explicit inductive formation rule, not projection or a
  substitution accepting an arbitrary replacement for Self.
- [x] Give each schema a fresh nominal family reference. All remaining
  parameters occur explicitly in its APP spine; they are not hidden free
  variables in a descriptor. Repeated formation reuses evidence, whereas a
  distinct schema with the same shape denotes a different type. Include the
  certificate pointer in proof hashing as well as equality, so these distinct
  nominal derivations do not all collide when their premises coincide.
- [x] Implement `PG_CONSTRUCTOR_INTRO`: instantiate the retained outer
  parameters, substitute the admitted type for Self, and check actual field
  values through ordinary typed substitution. The result classifier comes from
  the instantiated formation, not a caller's expected type. Classifier recovery
  reads that retained formation. Core constructors still contain erased fields.
- [x] Test Nat zero/successor, exact repeated-evidence reuse, distinct nominal
  schemas, wrong fields/arity, negative fields, universe-zero rejection of a
  stored universe and corresponding universe-one admission, explicit Box
  parameters and mismatched Box instances. Indexed input remains rejected by
  this rule. Feed a source-synthesized conditional Nat schema through formation
  and use the admitted Nat/zero in ordinary source Lambda application.
  Full component `make check` and rebuilt ASan/UBSan IADT/synthesis tests pass.
- [ ] Connect ordinary DECLARATION dispatch, automatic Self/universe obligation
  generation, and qualified constructor names to these rules. The source test
  above explicitly supplies the kernel formation; unchanged examples are not
  thereby accepted. Add typed Match/IH and runtime-result checks afterwards.
- [ ] Extend the nominal descriptor/rules to indexed formation and the required
  datatype Identity/action, transport, lifting and higher coherence. None is
  inferred merely from these new rule tags. Raw higher reductions can remain
  neutral at this intermediate checkpoint, not falsely prove conversion.
- [ ] Transport nominal schemas and these rule applications through `.a`.
  `pg_derivation_parameters` currently rejects the two new rules explicitly;
  their schema/object certificates must not be omitted or serialized as host
  pointers. No unsupported nominal proof is silently exported as accepted.

Ownership audit after `2e71ac3`:

Lifetime correction after `0608b0d`:

- [x] Reproduce that `pg_typing_destroy` followed by reinitialization at the
  same C address could identify surviving arena-owned evidence with the new
  store. The regression failed even immediately after destruction: the old
  ownership predicate compared only the reusable container address.
- [x] Allocate a fresh graph-owned `owner_key` for each typing initialization.
  Evidence retains that pointer, while destroy clears the container's key.
  No numeric generation counter, serialized host pointer or global registry
  is introduced. Ordinary ownership checking stays constant-time and exact.
- [x] Remove the separate owner field from `pg_data_signature`; its retained
  parameter evidence supplies ownership through the common predicate. Reject
  an old signature after store reinitialization, including the zero-constructor
  case which would otherwise skip per-constructor premise checks.
  Final component `make check` and rebuilt ASan/UBSan Core/IADT tests passed.
  This repairs lifetime isolation, not recursive formation or source acceptance.
- [ ] Audit other pending-work owner pointers for the same lifecycle pattern
  before allowing their handles to cross image/resume or temporary-store
  boundaries. This fix concerns evidence/signatures, not every work handle.

Solve-handle lifetime correction after `1986ad2`:

- [x] Reproduce that a source scope from a destroyed Solve instance was
  accepted after reinitializing the same `pg_synthesis` storage. Both source
  scopes and pending/completed job handles now retain a fresh graph-owned
  initialization key, rather than the mutable container's C address.
- [x] Reject old scopes, old completed jobs, old pending jobs and old import
  scopes at request boundaries, before allocating new work. Retain the
  existing pointer-key job interning and ordinary scheduler; no generation
  table or separate Replay queue is added.
- [x] Preserve accepted evidence across Solve disposal when its typing store
  remains alive. A new `pg_synthesis_evidence` wrapper can use that evidence,
  and ordinary resynthesis finds the identical accepted universe derivation.
  Invalid work handles must not force valid proof results to be discarded.
  Full component `make check` and rebuilt ASan/UBSan synthesis tests passed.
- The outstanding broader handle audit above remains open. Syntax buffers,
  typing, classifiers and normalization work must still outlive the Solve
  instance as documented; this change does not permit violating those inputs'
  lifetimes or claim complete image checkpoint/resumption.

- [x] Route all 38 primitive evidence ownership guards through the existing
  `pg_evidence_owned_by` predicate, already used by generic derivation
  reconstruction. Its meaning remains exact typing-store identity, including
  rejection of NULL. No ancestor-store acceptance, copied proof or exception
  for a shared Core graph is introduced.
- [x] Add a regression with two typing stores on the same Core graph. Their
  empty contexts and universe Core terms coincide, but their accepted evidence
  does not. Formation, projection and substitution reject foreign premises
  without adding proof records; each store still interns its own derivations.
  Full component `make check` and rebuilt ASan/UBSan Core tests passed.
- A scoped temporary typing store is not by itself the mathematical discharge
  rule for a recursive signature. It would also require an explicit, checked
  bridge for the admitted declaration. Conversely, adding type-family
  assumptions to ordinary value contexts requires changing their formation
  and substitution rules. Neither representation is authorized by simply
  relaxing ownership checks. Keep the predicate exact until that logical
  contract has been implemented; this consolidation changes no theorem.

Conditional field synthesis after `2f59743`:

- [x] Allow the existing checked `pg_synthesis_bind` to expose an explicitly
  supplied universe-classified binder as the special `*` token. Reference
  synthesis uses ordinary VARIABLE and TYPE_FROM_VALUE evidence. Punctuation
  spelling is canonicalized in the scope key, as for the intrinsic root.
  No new Context kind, proof rule, acceptance flag or nominal type is added.
- [x] Exercise `@{zero:*; succ:*->*;}` under the explicit assumption
  `Self : Universe_0`. The result is a conditional schema with Self still in
  its parameter context; the successor field is exactly that binder reference.
  Positivity and field-level checks succeed, but the job has no expression
  proof. Reading Self cannot be weakened into the empty context. Shadowing
  `*` with an element of Self is rejected; an unbound `*` remains unsupported.
  Full component `make check` and rebuilt ASan/UBSan synthesis tests passed.
  These results do not establish ordinary source declaration acceptance.
- [ ] Discharge that assumption through the actual inductive formation rule.
  The conditional schema above is a family parameterized by an arbitrary type,
  not its fixpoint. It must not be exported as a closed Nat declaration.
  Source declarations still do not allocate Self automatically. General
  indexed Self signatures, pending universe constraints and nominal HOTT
  action remain required; this change does not manufacture them from a Pi.

Formation audit at `2f59743` (after pending telescope scheduling):

Scheduling correction after `0380589`: do not interpret this audit as requiring
all higher datatype computation to be finished before implementing any
inductive formation rule. The pinned Narya documentation
[`docs/source/hott.rst`](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/docs/source/hott.rst)
describes a partial implementation of transport/lifting computation, and
separately notes the fibrant-replacement problem for indexed inductives.
Its `check_data` still implements datatype checking. These are distinct
implementation obligations. This observation is not a soundness proof for an
A Program rule, nor permission to call an arbitrary relation Identity.

Our next concrete rule-development path is:

1. For zero indices, use the already implemented conditional field judgement
   in `Gamma, Self : Universe_l`. Retain that context and every constructor's
   field/result derivation as the premises of inductive formation. Require
   syntactic strict positivity and field universe bounds at most `l`; do not
   infer a fixpoint solely from the existence of a schema. This is the
   ordinary strictly-positive initial-algebra formation obligation, not an
   ordinary substitution that can choose any type for Self.
2. Allocate a nominal family descriptor whose applications expose every free
   parameter as a Core operand. Close the conclusion over Gamma, explicitly
   discharging the distinguished Self assumption. Constructor membership
   subsequently substitutes this admitted family into the retained field
   telescope using the existing checked substitution rules. The resulting
   evidence DAG must remain acyclic: hypothetical field premises do not depend
   on their final nominal formation proof.
3. Implement that rule and membership together with their ordinary source
   integration tests, while tracking the corresponding Identity/action,
   transport, lifting and subject-reduction equations as unfinished. Do not
   invent a conversion result when an equation is missing. Source acceptance
   of a formation is not evidence of complete HOTT execution or the final goal.
4. Extend to indexed signatures with the explicit conditional family judgement
   and index-boundary/fibrancy construction. Do not encode that signature as
   a CBPV value Pi, or equate the zero-index implementation with completion of
   Acc/Vec/IF8. Those requirements remain in the full acceptance gate.

This corrects an implementation-order restriction, not the final scope. The
kernel rule and its semantic justification still have to be supplied; the
steps above are not checked boxes or permission to trust a schema flag.

- Do not implement a schema-to-Universe shortcut as the next admission rule.
  `pg_prove_type_value` turns any accepted value-type formation into a universe
  value. `pg_prove_reflexivity` and `pg_prove_identity_transport` then expose
  Identity and transport without a datatype-specific capability check. A new
  formation therefore commits to their meaning as well as ordinary membership.
  A stuck reduction alone would not prove inconsistency, but omitting datatype
  action would not meet the requested computational HOTT implementation.
- `iadt.c` currently supplies erased constructor/matcher descriptors and
  `pg_data_action` for an acted matcher. It does not supply a nominal family
  descriptor with dependent Identity/transport behavior. A checked result
  substitution is not that descriptor or a proof of those rules.
- A nonrecursive declaration is a case of the same justified admission rule,
  not a separate blanket schema-to-Universe axiom.
  For an indexed constructor, the field telescope and its result substitution
  must also determine how the index boundary is respected by action and
  elimination. Do not erase that obligation by testing only unindexed Bool.
- The pinned Narya checkout at
  `c7c92b4ec01ae2f528b97207256549242bd21334`,
  `lib/core/check.ml:2258` (`check_data`), checks constructor fields inside a
  scoped `run_with_definition` for the declaration currently being checked.
  It separately checks the output head and index arity. This is evidence for
  the need for scoped recursive checking, not permission to mutate accepted
  A Program evidence or to copy Narya's checking-directed surface policy.
  Source: https://github.com/mikeshulman/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/check.ml
- Next implementation must specify the conditional judgement used by field
  checking and the admission rule which discharges it, together with nominal
  family action. Ordinary `pg_context` contains value declarations, not an
  implicit total type-family assumption. An owned pending job is not a proof
  of such an assumption. Do not add another helper-only milestone here.

Rechecked `check-examples` at this revision: still 0/8, unsupported with the
same transition counts recorded above. This audit adds no accepted formation
and does not advance the source-acceptance checkbox.

Audit at `b87f7b1`: the three recent schema helpers do not implement recursive
declaration checking. Re-running `check-examples` still gives 0/8, with the same
transition counts above; `check-open-families` still fails at 86 transitions.
These are missing implementations, not successful negative tests or a resource
limit. Stop adding independent schema-helper checkpoints as a substitute for
the following source-to-evidence path.

The concrete dependency is in `synthesis.c`: `pg_source_scope.context` is an
accepted context proof; `intern_scope` requires it; `pg_synthesis_bind` checks
the extension through `pg_prove_variable`. `binding_step` waits for a completed
domain proof before allocating the binder and inner scope. `constructor_step`
uses that path for all fields before requesting the result map. Thus the
current producer graph can wait for external definitions, but cannot express
the provisional Self-family formation needed inside its own constructor fields.
Changing the final result-map checker cannot repair this ordering.

- [ ] Represent the pending declaration's Self signature, index applications
  and universe obligations in its existing synthesis work, before requiring
  accepted field-context proofs. Reuse binding pointers and source expressions;
  do not introduce a second erased computation graph or accepted placeholder
  proofs. A task dependency by itself is not a logical Self assumption.
- [ ] Specify and implement the scoped formation rule and its discharge.
  Field derivations may use the declared signature conditionally, but a public
  formation must retain the checked discharge. The implementation must make
  escaping an undischarged assumption impossible; merely keeping its source
  name private is not sufficient. Do not automatically turn a signature into
  an ordinary CBPV function or a universe inhabitant.
- [ ] Generate the pending universe conditions, solve them through the same
  work scheduling, and build final immutable derivations at the resulting
  levels. Keep syntax/name resolution, pending constraints and accepted evidence
  distinct without duplicating their authority. A global rewrite of all source
  scopes is not yet justified: first implement the actual recursive formation
  contract, then change shared scope APIs where that contract requires it.
- [ ] Exercise the complete path on `@{zero:*; succ:*->*;}`, a field which
  stores a universe, and an indexed constructor using `* index`. A rejected
  negative recursive field must not publish a formation. Check split budgets,
  repeated requests, two distinct nominal declarations and `::` post-checking.
- [ ] Add constructor membership and typed Match/IH, then close the existing
  source and execution gates. No helper success stands in for these gates.

This is not a requirement to solve arbitrary open computation-valued families
before admitting every nominal declaration. That separate issue remains open.
Likewise nonrecursive closed declarations still need a formation rule, but do
not require a hypothetical Self assumption. The implementation must not invent
either dependency solely to force all cases through a special-case workaround.

First source-structure change after `3b2b3f3`: binding-job creation now reserves
its binder pointer, before the domain is solved. `pg_synthesis_binding` and
`pg_synthesis_binding_binder` expose that existing shared job/identity, and
ordinary Lambda/Pi synthesis and telescope opening use the same request.
`binding_step` no longer allocates a new binder after checking the domain.
Requests create no context or evidence and perform no Solve transitions.
Pending and rejected domains retain their reserved pointer without yielding
an accepted variable; successful context extension uses exactly that pointer.
Tests exercise both expression/telescope request orders, cycles, rejection and
repeated requests. The parent source scope still requires accepted context
evidence: reservation is the first separation of lexical identity from proof,
not completion of provisional Self formation, universe solving or admission.
Component `make check` and the ASan/UBSan synthesis suite passed. No source
acceptance gate is marked complete by this change.

After `b3ad153`, source scopes retain a context producer instead of copying its
accepted proof pointer. Checked external contexts enter through the existing
evidence producer; pending bindings refer to their own binding job. The one
source-context accessor returns evidence only for a completed context producer.
There is no parallel pending-context database or mutable proof snapshot.

`pg_synthesis_binding_scope` exposes lexical structure while the domain is
pending. Telescope opening now constructs its binding chain incrementally,
without waiting between domains, then awaits the final context producer.
Ordinary source evaluation/typing jobs subscribe to their context producer
before invoking any proof rule. Failed context production propagates failure,
and namespace publication requires an actually checked closed context.

Regressions request bodies before their binding proofs, confirm they wake with
the correct context, retain two nested binders across an unresolved domain
cycle, reject bodies under failed domains, and reject publishing pending scopes
as closed namespaces. A test's absolute job count was replaced with its actual
invariant (one new expression job), because roots now also have a shared
completed context producer. This prepares pending declaration structure; it
does not supply a Self formation rule or solve recursive universe obligations.
Component `make check` and ASan/UBSan synthesis tests passed. Source admission
and the remaining full-plan acceptance requirements are still unfulfilled.

After `2c39e29`, telescope structure is a shared source producer distinct from
the request for its context proof. `pg_synthesis_telescope_structure` opens one
binding per transition and exposes scope/body without claiming evidence.
The ordinary checked telescope consumes that structure and awaits the final
context producer; it does not rebuild the binder chain. Both run on the same
queue and retain the same binding jobs, with no extra Core/proof former.

Source schema preparation can now start constructor-field jobs after index
structure is known, even while an index domain remains pending. Constructor
result-map checking still awaits the checked index telescope. The checked
`pg_data_signature` is created at schema assembly, not installed as a pending
assumption; its unused intermediate state pointer was removed. Pending Self
formation will need the structural inputs, not a falsely certified signature.

Tests demonstrate complete structure with a pending context, independent
field formation under an unresolved index domain, and withholding even an
empty-constructor schema until its index context is checked. This changes work
scheduling, not which nominal declarations are admitted. Scoped Self, universe
constraints, admission and typed elimination remain unfinished.
Component `make check` and ASan/UBSan synthesis tests passed with the new
producer scheduling and pending-index regressions.

- A scoped family signature supplies its fixed parameter context and index
  telescope. Instantiating that signature consumes checked index images and
  forms a type symbolically. It is not APP elimination of a `Comp Universe`
  value and must not require canonical RETURN readback.
- Field checking may refer to that signature, but it must not thereby obtain
  constructor membership, a Match rule, an IH, or general indexed fibrancy.
  Admission must retain and discharge the recursive signature assumptions
  after all fields, index maps, universe and positivity obligations are checked.
- Do not expose a globally accepted partial declaration or mutate an accepted
  evidence record when the remaining constructors arrive. Use the existing
  declaration producer and immutable premises as the publication boundary.
- This needs no separate ValuePi Core node. The current raw field/result
  telescope and binding pointers remain the data to use; the additional work
  is the checked declaration rule and its scoped assumptions.
- An arbitrary source field such as `R y x` can still encounter the independent
  open-function boundary. Acc acceptance must close that obligation too; Bool
  and a direct Self field must not be held behind an unrelated generic RUN
  operator. Keep both acceptance gates failing until their actual rules exist.

- The pinned Narya
  [`positivity.ml`](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/positivity.ml)
  records recursive references during checking, including dependencies hidden
  in let values and unresolved holes. Its introduction explicitly describes
  strict positivity checking as future work at this revision. Do not cite it
  as an implemented strict-positivity algorithm to copy.
- A Program's `reference_step` projects a completed producer's evidence,
  preserving its subject Core rather than replacing a definition by a new
  opaque constant. New synthesis tests publish pending positive and negative
  field classifiers through an alias, await ordinary Solve and check that the
  alias retains both the exact Core and its Self occurrence polarity. This
  establishes the current ordinary-name path, not arbitrary opaque definitions.
  Optimized and ASan/UBSan synthesis suites pass with the new fixture. Since
  these fixtures share proof stores, changed aggregate transition counts are
  not claimed as a performance improvement from this test-only change.
- Strengthen the positivity API contract accordingly: syntactic independence
  cannot discharge unresolved/hidden definition dependencies. Admission must
  wait for producers and preserve their dependencies, rather than treating
  absence of a visible Self pointer as semantic independence. No new recursion
  flag cache or parallel resolution engine is justified by the reference code.
- Narya's local `check_data` also scopes the under-construction declaration
  during constructor checking. For A Program, the outstanding equivalent is
  a scoped Self-family signature with explicit indices and discharged admission
  premises, not a globally accepted partial datatype or an ordinary CBPV
  function value. This remains an implementation obligation; the alias tests
  do not implement it or admit indexed recursion.

### Progress: Identity-prefix permutation reduction (2026-09-08)

### Next implementation contract: typed permutation (2026-09-08)

Implemented after `e88e280`: formation recovery now recognizes an Identity
instance whose selected family has an explicit `PG_REFLEXIVITY` derivation.
Its premise supplies A, so ordinary `pg_prove_identity_type` reconstructs
`Id A x y` with exactly the same Core subject as `(refl A) x y`. This exposes
retained inner Identity formations without inspecting APP arity or accepting
an arbitrary family as reflexivity. The existing projection/substitution
reconstruction then applies unchanged. No new Core tag or equality rule.

The new regression instantiates a reflexive family over an Identity type,
checks both inner endpoints, exact recovered Core identity, and rejection of
an excessive depth. Existing opaque-family tests remain unchanged. This
removes one formation-recovery obstruction, not the missing typed central
symmetry rule described below.
Verification: complete pointer `make check` and ASan/UBSan `identity_test`
passed. The higher-application comparison maximum remains 243,489 steps.

Follow-up: the same recovery now follows family projection/reindex premises,
not only an immediately visible reflexivity rule. A shared `retained_origin`
helper composes the retained substitutions for both formation and family
recovery; the reconstructed type is reindexed before endpoint checking.
Regressions cover both ways to move a family into an extended context and
verify that the recovered formation keeps that context and its exact Core.
This is not recovery from an unproved conversion or an opaque family assumption.

After `b280060`, explicit `PG_FAMILY_ACTION` families also recover their
source type and retained boundary maps/paths through ordinary family Identity
formation. The same reconstruction helper applies any outer substitution;
no separate family-map authority is stored. `PG_TYPE_CONVERSION` is followed
to its term premise because that accepted rule preserves the subject and
context, not because any equality witness supplies an action.
New tests cover a closed action instance, nested endpoints, and projected and
reindexed action families with exact recovered Core and destination context.
These tests do not yet establish recovery for every nonconstant dependent
family, or general typed center symmetry. Those acceptance gates remain open.
Complete pointer `make check` and ASan/UBSan `identity_test` pass this change.

Nonconstant follow-up: `dependent_instance_boundary` acts on
`A : U1 |- Id U1 A A` along an assumed path between distinct type variables.
The instance endpoints have non-alpha-equal classifiers. Formation recovery
retains the exact selected path and both substitutions, and obtains both
inner endpoints without changing the instance Core. The test derives the
family's Universe from classifier evidence rather than assuming a numeric
level. This establishes the one-parameter case, not arbitrary higher action.
The same fixture now also covers `A : U1, x : A |- Id A x x`. Recovery
retains both the type path and the dependent value path, their declaration
order and both substitutions; the endpoint classifiers remain distinct.
The production recovery code required no additional case for this extension.

Rechecked `action.h`, `evidence.c` and `square_template_jobs` /
`dependent_cube_substitution` after `253e690`. The current proper-face API
explicitly excludes centers. `PG_IDENTITY_INSTANCE` retains a checked family
value, whereas `PG_IDENTITY_FORM` retains a type formation. Both use APP in
Core; APP arity cannot distinguish their semantic roles. Keep that distinction
in the existing derivations, not in new Value/Computation Core tags.

The pinned Narya source `c7c92b4ec01ae2f528b97207256549242bd21334`,
[`act.ml`, `act_normal`, `gact_ty`, `gact_ty_instargs`](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/act.ml),
distinguishes acting on a type as a term from reconstructing the classifier
of an acted term. The latter acts on instantiated dimensions. For symmetries
its boundary construction does not require the central term; demanding that
term would create a cycle for neutrals. This is a design constraint to adapt,
not a claim that A Program already implements Narya's typing rules.

Required implementation sequence within N2:

- [x] After `c5379a9`, test the distinction between simultaneous dependent
  variation and iterated Identity before admitting typed permutations. The
  `A : U1, x : A` fixture acts on x along two path premises, but has only one
  direction: outer endpoint extraction succeeds and requesting an inner
  direction is unsupported on both sides. The same fixture separately acts
  on an actual Identity type and recovers the second direction successfully.
  Thus `path_count == 2` is not dimensional evidence; the retained formation,
  not premise count or raw APP arity, must establish the dimensions on which
  a symmetry acts. Existing production rules pass without weakening recovery
  or adding another Core/formation tag. This does not implement central symmetry.
  Optimized and ASan/UBSan Identity suites passed; the comparison maximum
  remains 205,367 transitions.
- [x] After `fd902c5`, filter Act scope references through the existing source
  binder index before checking lexical shadows. Ambient/semantic references and
  already discovered sources cannot alter first-use order. Only an undiscovered
  source enters the suspended shadow traversal; retain its source position
  instead of looking it up again afterwards. This removes the redundant final
  used-state branch without changing source order or shadowing. Existing tests
  exercise shared subterms under different shadows, duplicate source binders,
  unused boundaries, renaming, deep scopes and every suspension boundary.
  Full pointer `make check` and ASan/UBSan Identity tests pass. The higher
  function comparison maximum decreases from 215,677 to 205,367 transitions.
  No Core/typing rule changes; general typed symmetry remains open.
- [x] After `7a56733`, skip lexical environments for semantic references in
  evaluation and readback. Lambda and explicit substitution construction only
  bind `PG_BINDER`; semantic references could never match an environment entry.
  Readback therefore keys these constants without an irrelevant environment,
  while ordinary/owned binders retain capture-avoiding substitution unchanged.
  Tests reject a semantic substitution key and check a constant under 64
  bindings: substitution returns the identical pointer without traversal;
  beta evaluation takes 64 APP + 64 Lambda + one head step, not a further
  64 failed environment lookups. Full pointer `make check` and ASan/UBSan Core
  and synthesis tests pass. Unary cube counts become 2,079 / 83,456 / 5,429,901; binary counts
  become 6,356 / 337,312 / 24,330,848, agreeing at chunks 1 and 64. The higher
  function comparison maximum is 215,677. This reduces redundant traversal,
  not the unresolved general typed symmetry or all fresh-binder readback.
- [x] After `a65dd1e`, retain the current shared normalization job in each
  conversion instead of searching its index on every suspension step. Switching
  endpoint changes the borrowed handle; the WHNF-to-NF fallback clears it before
  requesting the stronger mode. No result cache, acceptance rule or scheduling
  step is added. `-O2 -pg` synthesis runs before/after this isolated change
  reduced `pg_whnf_request` calls from 67,566,269 to 129,705. Core intern calls
  remained 11,060,609 and index insertions 18,968,153: materialization cost is
  not solved by this change. The before profile attributed 50.00% sampled self
  time to intern and 15.39% to index insertion; these are instrumented samples,
  not stable wall-clock benchmarks. Only completed runs' matching `gmon.out`
  and binaries were used. Full pointer `make check`, profiled synthesis and
  ASan/UBSan Core tests passed with unchanged cubic solver step counts.
  General typed symmetry and the high-dimensional reconstruction cost remain
  open; neither is replaced by a more permissive conversion rule.
- [x] After `c638e8c`, compute cube-slot permutation directly from coordinates.
  The previous helper interned an input face, composed face and ordered face
  merely to obtain the new slot and intrinsic orientation. Decode once, collect
  endpoint digits and surviving axes, then intern only the output orientation.
  Existing three-dimensional tests cover all six permutations and 27 slots,
  inverse restoration and equality with general face composition/factorization.
  A new allocation regression permits at most one new map per slot request.
  This removes intermediate persistent geometry, not a typing distinction;
  it does not implement the missing typed center symmetry or demonstrate a
  source-level speedup. Permutation validation still uses the shared map owner.
  Optimized and ASan/UBSan Core tests passed.
- [x] Audit opaque family instantiation before extending symmetry acceptance.
  With assumed `A,B : Universe`, `x : A`, `y : B`, `r : Id Universe A B`,
  the existing instance/formation/face jobs preserve `r x y` and recover x/y
  as its supplied outer endpoints. A request for an extra direction is
  unsupported without retracting that accepted instance. The new regression
  passes without a production change: outer endpoint extraction was not a
  missing rule. Do not replace an arbitrary selected r by refl or infer a
  higher cube from its APP spine. General symmetry needs retained dimensional
  formation premises; not every opaque-family limitation is a recovery bug.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [x] Elide identity-only alpha-comparison scopes instead of rebuilding
  their shared subgraphs. Equal binder pointers in an empty correspondence
  need no map; under a nonidentity correspondence the pair must remain to
  mask outer bindings. After explicit normalization, identical pointers in
  an empty correspondence also finish that comparison task immediately.
  Tests check four tasks for a shared forty-node DAG under identical binders,
  one task when beta exposes the same DAG, and positive/negative shadowing
  cases. Independence checking retains its nonidentity sentinel scope.
  This does not alpha-intern terms or equate distinct normal forms. Current
  cubic application counts are unchanged, so it does not close the measured
  cubic-cost obligation.
  Complete pointer `make check` and ASan/UBSan `core_test` passed.
- [x] Correct the first-binder-only limitation of `639214a`. A curried
  `lambda x. lambda y. RETURN(x)` still remained neutral in dimensions two
  and three after all arguments were supplied. Scan all leading Lambdas of
  the retained Act source and expose one complete boundary cube per binder,
  using the same suspended scope worker and checked size multiplication.
  Do not infer a function from a bare APP argument count.
  Twice-acted first/second projections now pass every suspension point,
  retained resumption and readback/recomputation with distinct arguments.
  Typed dependent cube applications test one and two curried arguments in
  dimensions one through three. The two-argument totals are
  7,256 / 395,551 / 27,721,123 transitions for both scheduling chunks;
  its separate test ceiling is fifty million. Unary totals become
  2,339 / 94,825 / 6,185,924 from the additional binder traversal.
  This repairs Act application, not the still-missing typed symmetry rule.
  The large cubic cost remains an explicit performance obligation.
  Complete pointer `make check` and ASan/UBSan `identity_test` and
  `synthesis_test` passed; the existing higher-function maximum is now
  255,053 comparison transitions (previously 255,049).
- [x] Remove aligned-pointer bucket clustering in the shared index. Profiling
  `synthesis_test` at `639214a` with `-O2 -pg` attributed 72.85% of sampled
  self time to Term `intern` (2,377,343 calls). The old modulo selection used
  only low bits of pointer-derived hashes. A deterministic 1,024-key test
  varying high bits occupied one bucket with a 1,024-entry chain before the
  fix, versus 638 buckets and maximum chain six after avalanche mixing at
  bucket selection. Lookup, insertion and growth use the same function;
  stored hashes and exact-key equality remain unchanged. Tests retain equal
  hash collisions and find every entry after another resize. No new cache,
  Core equality or acceptance rule is added.
  Single uninstrumented `time .../.build/synthesis_test` runs measured
  2.169 s before versus 0.894 s after (indicative, not a controlled benchmark).
  All cube solver step counts remain unchanged: this addresses lookup cost,
  not the outstanding cubic transition count or typed symmetry.
  Complete pointer `make check` passed under both `-O2` and ASan/UBSan.
- [x] Repair iterated action application on known Lambdas. The new dependent
  cube test exposed `Act(Act(lambda y. RETURN(y)))` with nine arguments staying
  neutral: scope discovery could not see the inner action's function binders.
  With a complete outer triple, scan the retained Act chain for a Lambda,
  eta-expose the first binder's 3^n boundary arguments, and reuse scoped Act.
  Discovery/construction are suspended; scratch arrays use the evaluator arena.
  Opaque sources and unapplied actions are not treated as functions.
  This is local function eta exposure under Act, not global eta interning,
  an arbitrary APP-arity rule or a new equality reflection rule.
- [x] Test dependent cube Lambda application in dimensions 1, 2 and 3, with
  both the type and value cubes varying. Feed 3, 9 and 27 independently typed
  arguments through the shared raw application jobs, evaluate RETURN's content,
  and post-check its classifier against the center's type. RETURN extraction
  alone is not normalization of its content; alpha comparison of unnormalized
  classifiers is not the acceptance criterion. One-step and 64-step scheduling
  agree: 2,339 / 94,824 / 6,185,814 solver transitions respectively.
- [x] Test every evaluator suspension point of a twice-acted identity Lambda
  applied to nine distinct boundary variables. Both retained-work resumption
  and readback/recomputation yield the ninth variable; the unapplied action
  and prefixes shorter than one triple remain neutral. This exercises the
  new scope worker without assuming typed central symmetry.
  Complete pointer `make check` and ASan/UBSan `identity_test` and
  `synthesis_test` passed. The syntax inventory remains parsing-only evidence.
- [ ] Reduce the measured cubic application cost. The new test has an explicit
  ten-million transition ceiling; other tests retain their existing limits.
  Passing this budget is not a performance claim or general HOTT acceptance.
  The pre-existing higher-function comparison maximum also changed from
  243,489 to 255,049 transitions with the new exposure; retain this regression
  measurement when optimizing rather than claiming unchanged evaluation cost.
- [x] Exercise the combined pending family -> instance -> formation -> face
  pipeline on a reflexive family over a line. Requests do not advance the
  solver or publish partial results; the resulting square retains its Core
  through explicit formation recovery, and its edge agrees with the original
  path. A face request on the recovered formation is already completed with
  the same evidence. This test covers a degenerate square, not general central
  symmetry, and required no additional production rule.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [x] Schedule selected-family instantiation through
  `pg_synthesis_identity_instance`. It waits for three independent value
  producers, exposes the family's classifier, obtains its two endpoint types,
  and reuses EXPECT separately for each endpoint before ordinary
  `PG_IDENTITY_INSTANCE` acceptance. No homogeneous family is substituted.
  Tests use an assumed `r : Id U1 A B`, `x : A`, `y : B` with distinct
  endpoint classifiers; they check retained family, pending/canonical reuse,
  swapped endpoints, a non-family value and a computation endpoint. This is
  not Pi application or a new symmetry/transport rule.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [x] Route continuation closing through the same raw application job.
  A nondependent fold still uses its ordinary rule; when an actual return
  value is required, its pending producer feeds the application rather than
  a separate synchronous APP path. `synthesis.c` now has one
  `pg_prove_application` call site. A two-argument dependent identity test
  checks that the first value substitutes the second domain under both
  one-step and 64-step scheduling, against independently constructed evidence.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [x] Expose raw typed application on the shared synthesis queue with
  `pg_synthesis_application`. Independently completed producers converge on
  their evidence pointers, expose the callee classifier, use the existing
  EXPECT job for the domain, and construct ordinary APP evidence. Surface
  application now uses this job after its coercion/sequencing preparation;
  it no longer has its own final domain comparison. The raw API inserts no
  force/thunk/return or sequencing. Tests cover pending producers, canonical
  reuse, shared Core with distinct classifiers, wrong contexts, computation
  arguments, thunked callees and completed namespace-only producers. This
  supports typed construction of future permuted applications; it does not
  yet certify a permuted center or a family instantiation.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [x] Add `pg_dimension_cube_permute_slot` for moving a known cube argument:
  compose its face with the permutation, factor through an ordered face, and
  return that source slot plus the intrinsic orientation. This is geometry
  only, including the center; it neither constructs nor accepts an acted term.
  Cubic tests check all 27 slots under six permutations, independent numeric
  positions, bijectivity, face recomposition and inverse orientations.
  Complete pointer `make check` and ASan/UBSan `core_test` passed.
- [ ] Do not dispatch a new generic APP reduction merely because 3^n
  arguments are available. `pg_prove_application` requires a computation Pi
  and a value; `pg_prove_identity_instance` takes a value family over Universe
  and two endpoints, but both erase to APP. The typed action builder must
  select the applicable construction from retained evidence. Keep this
  distinction above Core; do not introduce ValuePi or query a global
  Core-to-classifier table. The slot planner does not resolve this obligation.
- [x] Share the existing cube argument order through
  `pg_dimension_cube_coordinates`: lexicographic endpoint-zero, endpoint-one,
  axis, with the final coordinate varying fastest. `cube_action` now uses
  this allocation-free decoder instead of its private ternary loop. Slots are
  traversal positions, not new persistent binder IDs; the resulting maps and
  bindings still use the existing pointer interner. Tests exhaust dimensions
  zero through five, center identity, invalid slots and arithmetic overflow.
  Complete pointer `make check` and ASan/UBSan `core_test` passed; existing
  dependent cube action/classifier comparisons remain unchanged.
- [ ] Connect permutation evaluation with the supplied argument boundaries
  of Act before adding a special equation for `S(Act(Act(t)))`. The existing
  permutation evaluator composes permutations and removes fixed prefixes;
  `symmetry_answer` otherwise leaves the application neutral. It does not
  implement the action on a function's supplied higher boundary arguments.
  Keep partial applications neutral, as the existing Act evaluator does.
- [ ] Check application/Act/permutation critical pairs with complete and
  incomplete boundary triples, captures and independently evaluated arguments.
  Test the target classifier as well as Core results. A rewrite recognizing
  only a syntactic chain of Act nodes would not discharge these obligations.
- [x] Expose `pg_symmetry_view` over exactly one raw symmetry application.
  The Core-owned descriptor supplies its dimension and axes, including fixed
  prefixes; no duplicate descriptor is added to evidence. Inspection neither
  reduces nor accepts the term. Tests cover all cubic permutations, no graph
  allocation, bare operators, additional applications and unchanged failure
  outputs. This is an input inspector for the future central rule, not that rule.
  Complete pointer `make check` and ASan/UBSan `core_test` passed.
- [x] Add `pg_synthesis_permutation_source_face`: factor the composed proper
  face using the existing dimension algebra and request its ordered source
  boundary from a pending formation producer. Return the lower-dimensional
  intrinsic permutation separately; the source evidence is not yet an acted
  boundary. This introduces no job role, Core form or typing rule. Tests cover
  all 26 proper faces under six cubic permutations, exact existing-job reuse,
  excluded centers, and a pending square producer with no eager solve.
  Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.
- [ ] Request a typed action with the accepted occurrence/formation and a
  permutation, never a bare Core pointer plus a guessed classifier.
- [ ] For each target proper face f, factor permutation composed with f into
  ordered face o and intrinsic permutation i. Obtain the source face at o
  using the existing face job; recursively act on it by i. The recursive
  dimension is strictly smaller because f is proper. Identity i reuses the
  existing evidence producer. Do not introduce a square-only acceptance rule.
- [ ] Reconstruct the target formation from those checked boundary images
  through the existing substitution/reindex jobs. Preserve selected families,
  ambient context and polarity. Do not substitute S(classifier) for this step.
- [ ] Specify and check the central symmetry inference using that boundary
  construction. Its subject is S(term), not the old term with a new type.
  The certificate must retain enough premises to check the construction;
  completed job status or matching geometric binders is not a premise.
- [ ] Discharge preservation obligations for prefix reduction and composition
  before using those raw equations on newly admitted typed symmetry evidence.
- [ ] Add a positive dependent square substitution using acted centers, while
  retaining the negative test that supplies unacted centers. Then test cubic
  proper faces, substitution naturality and composition with independent
  synthesis/fuel splits. A closed degenerate square is insufficient evidence.

The existing negative test localizes the first missing central derivation:
the eight proper faces of the type square substitute successfully, but its
ninth, unacted center does not. This is not a reason to weaken conversion.
Opaque families require a rule for recovering their instantiated boundary from
typed evidence; absence of a retained inner formation remains unsupported,
not proof that the requested equality is empty. No runtime or kernel behavior
is changed by this contract, and N2 remains incomplete.

Evaluation audit after `ddb0238`: a probe using the ordinary pure evaluator
on `Act(Act(lambda x. x))`, then on its transposition before/after that WHNF,
reported `expanded_is_lambda=0`, both transpositions still recognized by
`pg_symmetry_view`, and alpha-equal results. Thus the conjectured immediate
Lambda-expansion critical pair is **not** an observed current bug. Inspection
of `identity.c:action_source_body` confirms that complete boundary arguments
matter; an unapplied Act is not an eager Lambda translation. Do not describe
this probe as proof of general confluence or typed symmetry.

The [Narya symmetry/degeneracy documentation](https://narya.readthedocs.io/en/latest/observational.html#symmetries-and-degeneracies)
and the pinned `act.ml` source cited above motivate treating these as a
compositional action rather than isolated simplifications. This is guidance
for A Program's missing rule, not an imported soundness theorem: our raw
curried APP encoding must specify how the supplied boundary arguments move,
including their intrinsic orientations. The next rule must agree with the
existing dimension factorization and retain the selected typed families.

### Prefix-reduction verification

Endpoint-budget follow-up after `bcba4aa`: the endpoint worker now retains
its formation-origin cursor and composed substitution between transitions.
Each outer retained projection/reindex/conversion/presentation wrapper is
visited in its own transition; synchronous formation recovery drains the
same `origin_step` implementation. No second proof traversal or result store.
The 128-wrapper regression checks every budget split and delayed publication
of the endpoint. Existing cube endpoint tests now measure traversal steps
before checking all splits, replacing the obsolete `2*depth+1` assumption.
Inner selected-family recovery and primitive proof/substitution construction
remain synchronous. This is not a
wall-clock fuel bound and does not complete N2 or typed center symmetry.
Verification: complete pointer `make check` and ASan/UBSan `identity_test`
passed; higher-application comparison maximum remains 243,489 transitions.

Proper-face follow-up: layer validation now also uses `origin_step`, retaining
its cursor/map between transitions. The first recovered formation feeds
endpoint selection instead of traversing the original outer wrappers again.
The 128-wrapper face case completes in 131 transitions at every fuel split;
passing value evidence instead of type formation is rejected at initialization.
No persistent cache or independent boundary authority is introduced.
Complete pointer `make check` and ASan/UBSan `identity_test` passed.

Selected-family follow-up: one `formation_origin` cursor now records both
outer and family origins and their composed substitutions. Synchronous
recovery, endpoint traversal and proper-face validation use the same step
function. This supersedes the synchronous family-origin traversal limitation
above; reconstruction and individual proof rules remain synchronous.
A 64-conversion family chain checks every fuel split and publishes no partial
endpoint. The cursor is invocation-local, not another accepted-result store.
Complete pointer `make check` and ASan/UBSan `identity_test` passed.

Formation recovery now has an init/advance/result/destroy API for scheduling
as its own producer. It uses the same origin cursor as face and endpoint
traversal. The synchronous API drains a stack-owned worker, without additional
heap allocation. Requesting work does not reconstruct evidence. Tests cover
every split of 128 retained wrappers, exact agreement with the synchronous
result, invalid value input, and persistent unsupported status without a
published result. This API alone does not implement typed symmetry or image persistence.
Complete pointer `make check` and ASan/UBSan `identity_test` passed.

Shared solver integration: `pg_synthesis_identity_formation` accepts a pending
producer and schedules the existing worker. Requests converge on the accepted
formation-evidence pointer before traversal; this does not merge Core terms or
different typing occurrences. Face jobs share that recovery rather than each
repeating its outer traversal. The ordinary queue owns suspension, dependency
wakeup and cleanup. Invalid evidence kinds reject; unsupported formation
recovery remains unsupported, not a proof of inequality. Tests cover the
pending substitution -> reindex -> formation -> face pipeline, completed-input
sharing, and both failure categories. Typed center symmetry remains open.
Complete pointer `make check` and ASan/UBSan `synthesis_test` passed.

Face execution now also converges on the recovered formation-evidence pointer,
context and selected face. Different retained wrappers can therefore share
the face traversal, not only formation recovery. This is exact evidence-key
sharing, not WHNF interning or merging typed occurrences. The original request
remains a dependency on the canonical job. Proof-result forwarding is shared
with expectation, reindex and formation jobs; namespace/schema payloads are
not routed through this helper. Tests require the recovered face request to
be already complete and a separately wrapped input to return the same evidence.
Typed center symmetry and persistence remain open.
Complete pointer `make check` and ASan/UBSan `synthesis_test` passed for this change.

- [x] Preserve exact descriptor/application interning: a permutation with
  fixed leading axes remains a distinct raw node from its shorter form.
- [x] Reduce fixed leading axes during evaluation, with one coordinate per
  deferred copy transition and the original argument closure retained.
- [x] Compose permutations of unequal dimensions by extending the shorter
  one with fixed leading axes. Prefix reduction alone failed the existing
  three-dimensional composition regression; both rules are necessary.
- [x] Test captured arguments, trailing applications, every fuel split and
  suspended readback/restart, including a 126-axis identity prefix.
- [x] Compare 324 combinations of three-axis permutations and independent
  zero/one/two-axis prefix extensions against explicit map composition.
  This includes noncommuting cycles, not only the involutive swap case.
- [x] Run the complete pointer `make check` and ASan/UBSan `core_test`.
- [ ] Supply the typed central action and its classifier preservation rule.

The raw convention follows [Narya's symmetry specification](https://narya.readthedocs.io/en/latest/observational.html#symmetries-and-degeneracies):
leading identity extensions denote the same action on the last dimensions.
For example, one-based `132` reduces to `21`. This is an explicit computation
equation, not a new interning criterion. It does not by itself establish the
typed center permutation, dependent classifier reconstruction, or higher
coherence. All N0-N7 completion gates remain subject to their existing scope;
syntax inventory success is not source semantic acceptance. No main push.

### Investigation record: neutral higher application (2026-09-08)

The failure states below describe the investigation before the residual-scope
normalization described at the end of this record; do not read them as the
latest test result.

This checkpoint is not an acceptance result. HEAD before these edits is
`e042fea`; the working tree adds a deliberately failing positive regression.
The normal Identity suite currently fails in `generated_contexts`, dimension
2, identity axis order, comparing the classifier of acted neutral application
against application of the checked central function. N2 remains open. Do not
weaken conversion, change this assertion into an expected inequality, or push
this state to main.

The source context is `A : Universe, x : A, f : U(Pi(A, F A))`. Compare:

1. Cube action on the checked source `APP(FORCE(f), x)`.
2. FORCE the central function, then apply the checked faces of x in telescope
   order, using ordinary classifier normalization and conversion.

Dimensions 0 and 1 reach equal classifiers and terms. At dimension 2 the
classifier comparison reaches DIFFERENT after 5,522 comparison transitions
with a 1,000,000 transition limit; this is not fuel exhaustion. Strong normal
forms retain nested Act expressions. One source abstraction orders arguments
as `A-faces, x-endpoints, f-endpoints`, the other as
`A-faces, f-endpoints, x-endpoints`, with correspondingly reordered supplied
triples. Their inner applications still denote the corresponding `f x`.
This does not justify alpha equality: the remaining issue is the action rule
for instantiated higher families and its compatibility with substitution.
The dimension-3 cases are not reached by this failing run.

Before this mismatch, FORCE failed because a reducible nested Act hid the
function classifier. The working evaluator change requests that inner pure
closure on the existing demand-frame machine, rather than running a second
normalizer inside the callback. Caller arguments remain unchanged for this
auxiliary request; unfinished readback retains the original caller. This API
is unsuitable for effectful work that cannot be discarded and recomputed.
The new Core regression checks argument-list identity, callback delivery,
pending readback and every fuel split. It does not establish higher action
coherence. Source rebuilding and the alpha no-progress check in the Identity
callback remain synchronous, as do existing action-scope traversals.

Verification at this checkpoint: optimized and ASan/UBSan `core_test` pass.
`make -f src/prototype/pointer/Makefile check` passes Core, reader and synthesis,
then fails the positive Identity regression above; later test targets are not
reached. `git diff --check` passes. These working changes are uncommitted.

- [x] Reproduce the neutral application failure and distinguish it from fuel
  exhaustion and incorrect FORCE polarity.
- [x] Check auxiliary demand on the ordinary evaluator with every budget split.
- [ ] Complete and justify higher Act instantiation/substitution computation;
  preserve selected boundary evidence, not just untyped argument permutations.
- [ ] Pass the retained positive regression through dimension 3 and all tested
  orientations, then rerun the full optimized and sanitizer suites.
- [ ] Audit bounded traversal and auxiliary readback before accepting N2.

#### Reduced cause: action environments must respect declaration exchange

The next investigation reduced the mismatch to `action_scope_exchange` in
`tests/identity.c`, with no cube construction or dimension-map permutation:

```text
S = lambda x. lambda y. (Act A) x y
T = lambda y. lambda x. (Act A) x y

S x0 y0 =beta T y0 x0
S x1 y1 =beta T y1 x1

(Act S) x0 x1 px y0 y1 py
  ?= (Act T) y0 y1 py x0 x1 px
```

The endpoint equalities pass; the last comparison reports DIFFERENT after
543 transitions, including strong normalization. The new regression runs
before `generated_contexts`, so it is now the first Identity failure. This
small test is an operational equation with symbolic references, not independent
evidence of formation; the preceding checked cube example supplies the typed
motivation. Interpret x and y as independent declarations of the same A.

This exchanges declarations in one action direction, not the two directions
of a square. Therefore implementing cube-axis transposition alone does not
resolve the observed failure. The current neutral Act residual preserves a
lambda telescope and its argument triples as an ordered spine. It has no
computation rule for two such spines that represent the same binding-to-triple
assignment after declaration exchange.

The next implementation obligation is to make residual action environments
respect that assignment without identifying arbitrary lambda programs:

- Preserve association of each binder with its complete endpoint/witness
  triple. Do not sort or discard witnesses independently of their binders.
- Keep source-context exchange separate from higher-axis symmetry and its
  selected-face evidence. Do not reorder accepted dependent declarations just
  because erased Core has no classifier fields.
- A proposed residual normalization must be an explicit Act computation rule,
  not a change to pointer interning or a special comparison success case.
- Check renaming, lexical shadowing, substitution composition, partial triples
  and repeated selected paths. An ordering derived from allocation addresses
  is not alpha-stable and is not an acceptable canonicalization rule.
- Retain both positive regressions. The reduced example must not replace the
  checked dimension-2/3 application gate or stand in for higher coherence.

#### Implemented candidate: residual action environment exchange

`identity.c:order_scope` now orients administrative action environments by the
first free occurrence of each source binder in the residual body. Lambda
shadowing is tracked explicitly; a scoped DAG visitation index prevents repeated
visits without equating different binders. Marking is on visitation rather than
scheduling so sharing an argument with the function cannot change occurrence
order. Rebuild both the source abstractions and complete argument triples with
the same permutation, using ordinary closure substitution. No accepted context,
Core interning key, alpha rule or conversion success condition changes.

The proposed operational law is environment equivariance: acting on a body
under a finite binder-to-triple assignment does not depend on the storage order
of that assignment. The variable rule selects the same triple; structural
application uses the same assignments for its operands; lexical abstraction
removes shadowed bindings. A residual nested Act must retain this invariant too.
This is not an assertion that higher-dimensional axes or dependent declarations
can be exchanged without evidence. It is an A Program rule for the erased
administrative action closure; a general preservation/coherence argument remains
part of N2, and is not established by this regression alone.

Both the reduced exchange and the checked neutral application now pass through
dimensions 0-3, including all six dimension-3 axis orders. Changing the selected
center proof is separately required to remain distinguishable. The new
`expose_classifier` test helper uses a 1,000,000-transition conversion budget:
the previous 100,000 bound was reached while still PENDING in dimension 3.
Existing unrelated `convert_to` calls retain their 100,000 limit. This is a test
budget change, not an evaluator optimization or a change to acceptance rules.

Remaining engineering obligations: the scope walker and alpha no-progress check
are synchronous; building scope bindings can allocate unused administrative
binders. Integrate this traversal with bounded work and measure allocations
before N2 acceptance. Do not claim the entire action/transport/lifting fragment
complete from these passing examples.

Checkpoint verification: optimized `make check`, the same full target under
ASan/UBSan, and Identity with a 512 KiB stack pass. The syntax inventory is
still parsing evidence only, not semantic compatibility. Relative to `e042fea`,
implementation/header changes are +151/-17 lines, test C +147/-3 (documentation
excluded). This checkpoint is suitable for the rewrite branch, not main or
N0-N7 completion.

#### Follow-up: scope reuse and capture regressions

After `0a3d2ec`, split preparation of the source-binder array from allocation of
fresh administrative triple binders. `order_scope` now allocates triples only
when it actually rebuilds the environment; `action_body` rebuilding the original
source prefix allocates none. Both reuse the same `action_binding` structure,
without an additional graph or evidence representation. This removes 3*n fresh
binders per unchanged n-binding ordering check and per source-only rebuild;
it is not a measured whole-compiler speedup. Source traversal is still synchronous.

The exchange regression additionally checks alpha-renamed source binders,
lexical shadowing and a shared subterm reached both under a binder and outside
it. These are operational scope tests, supplementary to the existing typed
cube test, not new higher equality axioms. Implementation C: +14/-5;
test C: +24/-0, documentation excluded.
Optimized and ASan/UBSan full pointer checks, plus the 512 KiB Identity run,
pass after these changes. The pending bounded-work and general N2 obligations
are unchanged.

#### Bounded auxiliary work: first integration

The evaluator now accepts a pure traversal task through `pg_eval_defer`.
Each poll consumes one existing evaluator transition. The caller term and
argument spine remain unchanged while pending, so readback retains the caller
rather than serializing C task state. Completion detaches the task before
resuming ordinary evaluation; cancellation and error destroy it exactly once.
This is evaluator scheduling, not a second solver, proof rule or Replay engine.

The nested-Act alpha no-progress comparison uses the existing incremental
comparison walker through this interface instead of synchronous `pg_alpha_equal`.
Tests cover exact poll counts, every split before completion, pending readback,
duplicate-task rejection, cancellation, error cleanup, and destruction of an
actual suspended Identity comparison followed by re-evaluation of its readback.
The ordering walker, scope preparation and result rebuilding are still
synchronous: this checkpoint does not close the whole bounded-action gate.
Implementation/header diff for this step: +79/-8; test C: +80/-0, excluding
documentation. Next use the same task mechanism for the scope-order traversal
and its binder lookup/rebuild stages, rather than introducing an Identity-only
execution engine or silently charging a whole traversal as one reduction.
Verification: the latest optimized and ASan/UBSan full pointer checks pass,
as does Identity with a 512 KiB stack. The recorded cube-function comparison
maximum is now 192,056 transitions (previously 191,710); previously synchronous
comparison work is now charged, so this is not evidence of a runtime slowdown.

#### Incremental residual-scope visitation

After `e35745c`, the existing `pg_eval_defer` task owns the scope visitation
index, pending nodes, lexical-shadow cursor and source-binding lookup position.
One poll visits a node or advances one shadow/source lookup link. A per-source
visited byte replaces the former repeated search through the discovered order.
The same environment permutation is produced; no equality rule is added.
Task destruction releases the index even when traversal is cancelled.

The regression retains a 64-lambda nested body and cancels at 0, 1, 7 and 31
polls after task creation. It checks exact charged steps, still-pending work,
readback and re-evaluation. Existing alpha-renaming, shadowing, selected-proof
distinction and checked dimension-3 tests remain enabled. Initialization,
final ordering comparison and graph reconstruction are still synchronous;
allocator/hash-table operations do not have a wall-time fuel bound. Finish
those remaining traversal stages before closing the bounded-action gate.
Implementation C: +95/-49; test C: +24/-1, documentation excluded.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass.
The recorded comparison maximum is 198,563 transitions, now including the
incremental scope work. This does not change the remaining N0-N7 acceptance gates.

#### Incremental scope initialization and reconstruction

The residual-scope task now has explicit scheduling phases for source-binder
setup, visitation, source abstraction, triple application and administrative
wrapping. Each poll handles one binder or one fixed-size triple. Discovery
records whether a permutation is necessary, removing the final order rescan.
The resume callback only enters the prepared result or requests the inner body;
it no longer traverses the telescope to construct that result synchronously.
These phases are C work states, not new Core node kinds or CBPV distinctions.

The exchange test measures a whole reordering task and destroys a fresh run at
every poll boundary, checking exact fuel and unchanged caller readback even
while replacement nodes have been partially allocated. Existing typed higher
application and selected-witness regressions remain mandatory. Implementation
C: +64/-19; test C: +30/-0, excluding documentation.

This closes the source-setup and result-rebuild substeps of `order_scope` only.
Other action paths still call synchronous `action_scope`, `prune_scope`,
`prepare_bindings` and `abstract_body`; do not mark all action work bounded.
Allocation, hash growth and index cleanup also remain outside a wall-time bound.
Verification: optimized and ASan/UBSan full pointer checks and the 512 KiB
Identity run pass; the recorded comparison maximum is 199,049 transitions.

#### In-progress pruning audit (after e5e7ee8)

The working tree makes `prune_scope` poll the existing independence walker
instead of calling its synchronous wrapper, resuming body dispatch directly
when no declaration is dropped. This exposes an algorithmic issue: support is
still rediscovered separately for every source binder, then traversed again by
the residual-order task. The current full check fails at the 1,000,000-step
`expose_classifier` conversion limit in dimension 3, identity order, application
argument 1. Continuing that exact debugger-stopped comparison returns EQUAL at
1,842,740 total transitions. It is pending work, not a discovered inequality.
No global budget increase or acceptance exception has been made. These edits
are uncommitted; the previous accepted checkpoint remains `e5e7ee8`.

Next: unify the support-discovery and residual-order walks. Both need a scoped
mapping from free source binder to its retained triple. A single DAG visitation
can collect the used flags and first-occurrence order, avoiding n independent
walks followed by another ordering walk. Preserve lexical shadowing, especially
repeated pointers in the source prefix: the innermost source binder must win.
Do not turn this into context exchange or merge Core terms. For non-residual
bodies retain declaration order when pruning; only the already admitted
residual environment rule canonicalizes its ordering. Reuse the incremental
reconstruction and ordinary body dispatcher rather than maintain two pipelines.
Revisit the cancellation fixtures: a new pruning task is currently first, so
tests intended to cover ordering phases must not silently test only pruning.

#### Unified support and residual-order discovery

The working pruning experiment is superseded: `analyze_scope` now discovers
support and first-occurrence order in one scoped DAG walk. The standalone
pruning task and per-binder independence comparisons are removed. Source lookup
starts at the innermost prefix binder, preserving repeated-pointer shadowing.
If some bindings are unused, an incremental filter retains the original source
order; otherwise only a nested neutral Act head enables the existing residual
ordering rule. One reconstruction task handles both cases. Its unchanged case
resumes body dispatch directly, without repeating either analysis.

The optimized full pointer check passes with the existing 1,000,000-transition
limit unchanged. This supersedes the preceding budget failure, not the need
for general N2 verification. Existing tests exercise unused/duplicate binders,
selected proofs, shared subterms, all reordering cancellation boundaries and
checked dimensions 0-3. They now encounter one combined task, so reordering
cancellation coverage is retained rather than displaced by a preliminary task.
Relative to `e5e7ee8`, implementation C is +63/-57 (net +6); the uncommitted
per-binder asynchronous pruning implementation is not retained. Source lookup
still scans prefix entries and lexical shadows incrementally; do not claim
constant-time lookup or complete elimination of repeated compilation work.
Final verification: optimized and ASan/UBSan full pointer checks and the 512 KiB
Identity run pass. The cube-function comparison maximum is 256,454 transitions;
that metric covers different comparisons from the debugger's 1,842,740-step
classifier conversion and must not be presented as a direct speedup ratio.

#### Indexed source binding lookup

After `19da0f6`, scope support lookup uses the existing hash index, keyed by
binder pointer, instead of scanning the source prefix for each free reference.
Source initialization updates the transient entry to the innermost position
for repeated binders. Index construction and querying share one lookup helper;
the index is discarded with the work and is not a new semantic authority.
Pointer alignment bits are mixed before bucket selection.

An initial attempt charged each collision link as a separate fuel step. The
existing whole-versus-split test detected allocation-dependent step counts.
The retained version charges one lookup, consistent with other index operations;
hash collisions and growth remain outside the logical-transition budget. The
test was not weakened. A new 96-binder regression forces source-index growth,
retains the first and last bindings and supplies divergent computations for all
unused triples, checking that they are never evaluated. Implementation C:
+42/-7; test C: +19/-0. Lexical-shadow lookup remains incremental and linear.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass.
The same cube-function comparison metric is 237,379 transitions, versus 256,454
before indexing. This is a logical-work comparison, not a wall-time benchmark.

#### Incremental nested-source rebuilding

After `8fc703f`, the nested-body task collects and restores one source binder
per poll after a DIFFERENT alpha result. Its resume callback no longer calls
the synchronous source-binding and abstraction loops. An unchanged body still
returns neutral; no new action or equality equation is introduced.

A nested Act/RETURN regression cancels and destroys execution at every machine
step before WHNF, then checks each readback against the expected result using
ordinary conversion. RETURN contents may remain reducible at WHNF, so pointer
identity with the fully reduced result is deliberately not the assertion.
Implementation C: +35/-6; test C: +24/-0, documentation excluded. Initial
`action_scope` discovery and other former-specific reconstruction paths remain
separate outstanding bounded-work obligations.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass;
the cube-function comparison maximum is 238,143 charged transitions.

#### Incremental initial scope discovery

After `013e7b7`, all four initial action-scope callers share a deferred discovery
task. Each poll consumes at most one source binder and its complete boundary
triple. Partial triples leave that binder unapplied; the caller's argument list
is never mutated by discovery. Completion continues through the existing action
or endpoint rule, with no additional reduction equation or evidence authority.

The exchange regression now cancels at every machine step, rather than assuming
that scope ordering is the first deferred task. Each snapshot survives evaluator
destruction and converts to the original expression. This tests discovery as
well as ordering and reconstruction. The deep-source cancellation fixture still
checks exact charged steps across task transitions.

Optimized and ASan/UBSan pointer checks and the 512 KiB Identity run pass. The recorded
cube-function comparison maximum is 242,785 transitions (previously 238,143):
discovery now counts against fuel; this is not evidence of a speedup. General
former-specific source reconstruction and full `source_scope` scans remain
outstanding. This checkpoint does not close N2 or any full-language gate.

#### Incremental action result closure

After `99bb2c1`, `enter_action` builds administrative result lambdas through
the ordinary deferred-work interface. One poll wraps one discarded argument
or one fixed boundary triple. It retains the existing closure substitution and
does not demand any boundary computation. Zero-source actions retain their
direct entry rule. No Core constructor or proof rule is added.

The nested Act/RETURN regression checks every split both by cancelling and
converting readback, and by retaining the same evaluator and resuming it with
the remaining budget. Total charged steps agree with uninterrupted evaluation.
This is an in-process requirement for retained versus discarded work, not an
implementation or acceptance test of `.a` persistence. Implementation C:
+36/-9; test C: +9/-0. Former-specific body reconstruction and field-family
recognition remain synchronous; they still require bounded-work treatment.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test pass.
The cube-function comparison maximum is 243,623 charged transitions, up from
242,785 because result construction is now charged. N2 remains incomplete.

#### Incremental transported family closure

After `b780755`, `close_family` collects one supplied argument per poll, reuses
the action-result wrapper for one boundary triple per poll, and reapplies one
argument per poll. Both U(F) fields and forced U(Pi) transport use this task.
Completion uses their existing enter/apply rules; caller arguments remain intact
while the task is pending. The temporary argument-array size is checked before
multiplication. Family recognition and former-specific body construction remain
synchronous and are not covered by this bounded-work claim.

The checked U(F) transport fixture now cancels at every machine step in both
directions, destroys task storage and converts each readback to the separately
constructed thunk map. Existing U(Pi) tests retain their split-budget and typed
normalization checks. No new equality equation, Core form or Replay path is
introduced. Implementation C: +70/-19; test C: +18/-0. N2 remains open.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass;
the separate cube-function comparison maximum remains 243,623 transitions.

#### Incremental field-family recognition

After `d3411fd`, all three field-family recognition sites use a deferred task
that consumes one application edge or one source lambda per poll. The old
`source_scope`, `thunk_family` and `thunk_return_family` synchronous walkers are
removed. Unlike initial Act discovery, field recognition requires the entire
lambda prefix to match the number of supplied triples. Partial triples and
over/under-applied source telescopes do not enable a field equation. Missing
U/Pi/F shapes retain the former callers' neutral/error behavior.

U(F) cancellation tests now also retain and resume each suspended evaluator,
checking the same total charged steps as uninterrupted execution. Existing
forced U(Pi) tests compare split work, normalization evidence and the separately
constructed transport recipe. Implementation C: +82/-42; test C: +8/-0.
Source binding preparation and body construction remain synchronous. U(F)
recognition is still repeated after its value demand; sharing that prepared
scope across demand continuations remains a separate work-reuse obligation.
This checkpoint does not complete N2, higher coherence or image persistence.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test pass.
The separate cube-function comparison maximum remains 243,623 transitions.

#### Reuse prepared scopes across demand continuations

After `a82906c`, the existing demand APIs carry a borrowed state pointer to
their continuation. All callers migrate to this one signature; there is no
legacy adapter, alternative evaluator or second frame kind. The frame retains
state until completion; pending readback retains the original computation, not
an unvalidated serialized continuation. Callers own state lifetime, normally
through the evaluator arena.

U(F) field recognition now passes its prepared scope through the value demand
instead of rediscovering it. Left/right endpoint and nested-body demands also
reuse prepared action scopes. The zero-source route passes the original
arena-backed scope rather than a local copy, so suspended callbacks never retain
stack storage. Source binding preparation/body reconstruction remain open work.

Core demand and auxiliary-demand tests now assert state delivery, alongside
their existing split-budget, readback and callback-count tests. Identity tests
retain every-boundary cancellation/resumption checks. Implementation C/headers:
+49/-38; test C: +6/-4. The same cube-function comparison maximum decreases
from 243,623 to 243,107 charged steps; no wall-time speedup is claimed. N2 and
the full rewrite remain incomplete.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass.

#### Incremental reference-center selection

After `d854188`, initial scope discovery also selects the center for a reference
body, inspecting one source binder per poll. The final action callback no longer
rescans the whole prefix synchronously. Repeated binder pointers select the
innermost supplied triple, as before. Initial discovery now has only one caller,
so its unused generic answer/callback fields are removed rather than preserving
an obsolete abstraction. No interning or reduction equation changes.

Regressions explicitly distinguish reuse of a binder pointer from a reference
to an outer binder beneath a different binder. Existing every-step cancellation
and split-budget tests cover the additional charged traversal. Implementation C:
+20/-17; test C: +7/-0. Source binding preparation, body construction and some
argument-list operations remain synchronous; N2 is not complete.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass.
The cube-function maximum is 243,489 charged transitions, versus 243,107 before
charging reference-center selection; this is not a wall-time comparison.

After `e8bba19`, source collection and fresh boundary-binder allocation share
one preparation loop; the single-use `source_bindings` helper is removed.
Already collected source arrays are reused. Implementation C: +12/-15.
This eliminates a separate pass, not the outstanding synchronous-preparation
obligation. Existing field, source-action and cancellation tests cover both
fresh and prepopulated source arrays; no semantic rule changes.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass.

After `326d9d4`, scope analysis passes its already discovered neutral head into
action construction. The second application-spine scan is removed. Zero-source
actions return neutral after the existing former rules, without discovering a
head they cannot use for congruence. This preserves the diagonal loop guard.
Implementation C: +10/-10. Head discovery remains charged in `SCOPE_HEAD`; no
additional proof authority or normalization equation is introduced.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity run pass;
the cube-function maximum remains 243,489 charged transitions. N2 remains open.

## 1. Objective and Source of Decisions

Reimplement A Program around an erased pointer graph with Lambda, Application,
and references to bindings or semantic objects. Preserve the current surface
language and supported behavior; keep CBPV typing, effects, dependencies, and
proof checking above this representation. Typed terms, binders and declaration
objects support dimensional action from their first implementation. This is a fresh implementation, not a
rename of the current TermDB or a pointer facade over its integer IDs.

The user's `src/handmade/` and sibling `a-program-handmade` are read-only design
inputs for AI work. Do not modify, generate code in, or commit to the sibling
repository. Do not promote AI code into accepted directories. Use a separate
prototype build target and executable until replacement acceptance.

Examined handmade inputs:

| Input | Revision / content digest | Observed direction |
| --- | --- | --- |
| `src/handmade/main.c` | SHA256 `faa5a7d0b7128fd60c7417f639e4427de3f76abf6e0c5f16284971b31b97d0e8` | APP child pointers; IADT pointer; separate term/type binding |
| sibling `a-program-handmade/main.c` | repository HEAD `9a78c3db978af82e378aedb06bc8fca0271e85fc`; file SHA256 `87c6fc6be415be7870b5dda678283af1a01d6d0cfa20edfd6fd22281a5815ce2` | generic REFERENCE with object pointer; constructor and Match objects |

GitHub was also checked directly: [repyt-margorp/a-program-handmade at
9a78c3d](https://github.com/repyt-margorp/a-program-handmade/blob/9a78c3db978af82e378aedb06bc8fca0271e85fc/main.c).
Its default branch is `master`. The sibling has an uncommitted addition of the
Match-eliminator sort; distinguish that local extension from the published
source. Both are read-only references, not locations for this implementation.

The boundary to adopt is concrete: APP knows only the two terms it connects;
REFERENCE directs operation to the object that owns its semantics;
`data_type_object` refers to the IADT owner, while `binding` holds term and type
separately. Preserve this ownership in the replacement. Core must not include
IADT field layouts or query the typing solver. IADT owns its constructors and
elimination rules; typing owns admissibility and evidence. This is a direct
in-process pointer boundary, not a message protocol or a duplicated graph.

These are sketches, not a finished evaluator specification. In particular:

- `lambda.index` currently selects an environment position; it contains neither
  an abstraction body nor a binder pointer. Implement actual abstraction and
  lexical reference separately within the proposed representation.
- Application currently extends the environment and evaluates its function;
  lexical closure capture and general beta substitution still need definition.
- The sibling sketch retains stale `TERM_IADT` / `as.type_system` references,
  and gives element constructor and Match eliminator the same sort value.
- Recursive child hashing, fixed capacities, and pointer formatting are sketch
  details, not requirements to reproduce.

No claim is made that three outer node tags alone prove semantic equivalence to
CBPV. The translation and its operational obligations are specified below and
must be tested. Oracle objects still carry real semantics; moving them behind a
pointer does not eliminate those rules.

## 2. Nonnegotiable Design Rules

1. Core evaluation does not consult a classifier, proof context, or TypeView to
   choose the meaning of the same executable node. A reference's descriptor
   already determines its operational meaning.
2. Typed occurrences carry Core references, annotations, context and evidence.
   Two differently typed identities may share Core without sharing their typing
   judgement. Never recover a source classifier by searching all types of Core.
3. Synthesis derives a classifier from source structure and binding declarations.
   `::` checks the result afterward; it never supplies a missing motive/domain.
4. CBPV value/computation distinctions remain typing rules, not duplicate
   Lambda/APP node kinds. Erasure must still preserve sequencing and suspension.
5. Pointers identify live immutable objects. They do not certify typing, prove
   arbitrary equality, or provide portable artifact identities.
6. A budget exhaustion result is pending, not false or accepted. Static
   evaluation uses only the explicitly admitted pure semantics. Host effects
   execute only at an execution boundary.
7. Hash lookup followed by exact pointer-key comparison and allocation is the common
   construction path. Rule dispatch, genuine scope distinctions and errors
   remain explicit. Deleting all conditional operators is not an acceptance test.

## 3. Small Physical Model

Proposed initial modules (headers are declarative; avoid a service layer per DB):

| Module under `src/prototype/pointer/` | Owned information |
| --- | --- |
| `graph.c`, `graph.h` | stable allocation, immutable nodes, interning, binder/object references |
| `eval.c`, `eval.h` | environments, application spines, resumable reduction, pure memoization |
| `reader.c`, `reader.h` | tokens, source spans, AST, lexical names; no solving |
| `typing.c`, `typing.h` | typed occurrences, persistent contexts, constraints, derivation checking |
| `iadt.c`, `iadt.h` | declaration/constructor/Match descriptors, recursive-field schemas |
| `effect.c`, `effect.h` | request, return and fold descriptors; handler execution |
| `dimension.c`, `dimension.h` | dimension maps, boundary diagrams and action, present from N1 |
| `identity.c`, `identity.h` | Identity computation, transport/lifting and evidence, present from N2 |
| `prelude.c`, `prelude.h` | derived checked library functions; no new primitive rules, evaluator or name-resolution policy |
| `image.c`, `image.h` | program roots and pointer relocation for `.a` |
| `driver.c`, prototype-local build/test files | CLI, REPL, test runner |

Add a module only when it owns a distinct responsibility. Share storage helpers;
do not build a generic database framework before these modules need one.

Core nodes have three forms:

```text
Lambda(binder*, body*)
Application(function*, argument*)
Reference(object*)
```

A lexical variable is a reference to a binder object. Semantic references point
to immutable descriptors for constructors, eliminators, literals or operations.
Use a declared object header/discriminant and checked access, not unchecked
`void *` casts scattered through the evaluator. A descriptor may contain Core
children; graph traversal must enumerate them for substitution and persistence.

Allocate stable chunks: growing an arena cannot move referenced objects. Separate
mutable evaluation frames from immutable nodes. A closure captures a body and
its environment; sharing a term does not share one mutable runtime invocation.
Start with whole-program arena ownership and reclaim temporary frames per run.
Measure before adding fine-grained GC or environment indexes.

Binder objects are allocated before their bodies and are frozen with the binding
scope. Intern Lambda by `(binder*, body*)`, APP by `(function*, argument*)`, and
REFERENCE by `object*`, with the node kind in every key. There is no recursive
inspection of children during interning. Different binder pointers remain
different structural nodes even when the lambdas are alpha-equivalent.

When alpha comparison is explicitly required, use a binder correspondence;
never use its result to merge the input nodes. Do not adopt De Bruijn storage.
Likewise, evaluating `APP(identity, value)` may return `value`, but the original
APP remains a distinct interned node. WHNF/NF equality belongs to explicit
conversion, not allocation. Match/IADT references follow the same rule: distinct
semantic objects are not merged because their computations agree. Reindexing and
capture avoidance still exist with pointers; share unchanged subgraphs and
memoize substitution by term plus immutable environment/mapping.

IADT declarations have generative object identity. Constructors point to their
owner declaration and their field telescope, and Match clauses point to actual
constructor objects. Structurally identical Bool and Two stay distinct. Array
positions/counts and serialized reference numbers are permitted; they are not
the in-memory semantic identity. Nominal creation is not structural interning.

## 4. Reduction and CBPV Correspondence

Lambda beta reduction is shared by all callers. An applied semantic reference
collects its arguments and delegates to its fixed reducer. Reducers return a
reduced graph, a blocked neutral, an effect request, or an error with consumed
steps. Core contains no type-directed fallback path.

| Typed construct | Proposed erased representation / action |
| --- | --- |
| Lambda / APP | the two ordinary Core forms |
| constructor application | constructor reference applied to field values; saturated data is inert |
| Match / IH | eliminator/frame references applied to scrutinee and clause closures; general IADT reducer |
| return / thunk / force | references to fixed structural operations and APP spines |
| computation fold | immutable clause descriptor plus applied computation and return clause |
| pure primitive | fixed reference semantics admitted under a documented conversion policy |
| effect operation | operation reference producing a request; handler or host supplies its result |

The graph has a uniform physical vocabulary but retains the distinctions needed
for execution. Thunk must stop traversal, force must release it, and deep handler
resumption must reinstall the handler. Unhandled operations retain the transformed
continuation. A single reference protocol does not make arbitrary host callbacks
safe for conversion or serializable.

Use the existing CBPV surface elaboration policy as an explicit translation:
insert return in a computation body containing a value; sequence returning
computations where the current language does so; preserve explicit `&`; preserve
callee-force behavior. Choose evaluation order explicitly, including function
position, arguments, constructor fields, and blocks. Do not rely on unrestricted
beta reduction to establish the order of effects.

Before broadening the implementation, show for the initial pure fragment that a
typed reduction and its erased execution agree on the result, and demonstrate
effect order with traces. Record the admitted equations rather than claiming a
general CBPV equivalence theorem from tests alone.

`pg_computation_eval_init` now installs a fixed pure CBPV dispatcher on the
same lexical beta machine. The generic demand protocol suspends an applied
reference, evaluates one selected argument, materializes that WHNF, restores
the caller and resumes its operation. FORCE demands its argument and releases
the body of THUNK. RETURN and THUNK remain inert heads. Unknown arguments leave
FORCE neutral with the inspected argument retained; no repeated demand loop is
needed. Demand frames are included in pending readback. Each frame resumption
is a budgeted machine step, although materialization itself is not yet separately
budgeted. Captured environments are still handled by the shared readback path.

- [x] Pure FORCE/THUNK execution: test inert quoted divergence, released
  divergence, neutral arguments, nested demands, pending readback, external
  application arguments, captured environments and split-budget equivalence.
- [ ] Extend fixed reference semantics to constructor/Match, folds and requests;
  verify effect order and typed action compatibility. The dispatcher is not yet
  a general effect runtime, nor connected to conversion or source execution.

The zero-operation-clause `computation-fold` reference now takes M and a raw
return continuation K. It demands M through the shared evaluator protocol;
`FOLD(RETURN v,K)` applies K to v while preserving lexical closures and any
arguments outside the fold. No separate BIND Core node is added. The first
checked rule requires `M:F A`, `K:Pi(x:A,C)` with C structurally independent of x.
C may be a raw computation Pi, not only a returning computation; the left
operand must still be F A. This is pure CBPV sequencing, not executing raw Pi
as a source of a bound result. Dependence on x is checked by fresh substitution
and explicit alpha comparison, never by modifying interned node identity.

- [x] Check/execute zero-clause pure FOLD and recover its result formation;
  test neutral/divergent input, pending readback and non-value/quoted-continuation
  rejection. Reject a result classifier depending on the bound result in this
  nondependent rule; do not fabricate an effectful type-level result.
- [x] Lower a returning source argument through a fresh typed continuation and
  FOLD. Source fixtures execute `f (g x)`, including a continuation returning
  raw Pi followed by another application. Both computations are synthesized
  before sequencing; no expected type determines their synthesis.
  The curried fixture exposed a hygiene mismatch: separate substitutions
  freshened bound classifier pointers differently. Lambda introduction now
  checks structural alpha equality of its body classifier rather than exact
  pointer identity. No nodes are merged; nonstructural conversion still needs
  explicit evidence. This is a binder-renaming rule, not WHNF interning.
- [x] Sequenced arguments use the same resumable classifier comparison as
  ordinary value arguments. A block returning a quoted identity function can
  be passed to a higher-order function even when its Pi binder pointers differ;
  the continuation's APP retains explicit conversion evidence. A genuinely
  different result type is rejected. Block and argument sequencing share
  continuation opening/closing through the existing checked proof rules.
- [x] Sequence a callee returning a quoted function before its argument.
  Both operands use the same continuation-opening operation; their frames close
  in reverse order to form `FOLD(callee, \f. FOLD(argument, \x. FORCE(f) x))`.
  Tests execute returned/quoted callees, two computed operands and higher-order
  returned functions, inspect this nesting in the proof DAG, and reject
  nonfunction results and mismatched arguments. This checks the pure translation;
  observable effect-order tests still require requests and handlers.
- [ ] Add operation clauses, effect rows/forwarding, dependent sequencing and
  computed classifier normalization. These remain unsupported by this initial path.

Memoization keys include the semantic reference policy and captured environment
when relevant. Never memoize a dispatched effect as if repeated force were pure.
Keep WHNF and NF distinct. Type conversion and object Identity remain distinct.

Implemented `pg_beta_work` shares resumable beta-WHNF requests by input pointer.
Its policy is fixed: every unbound REFERENCE is neutral, including semantic
objects; no oracle dispatch occurs. Each request owns its closure machine and
one materialized answer. Repeated requests resume that machine or return the
existing answer without reduction or readback. Captured environments are local
to the request, never indexed by the current body pointer alone. Completed jobs
release closure storage; their answers live in the output graph. The input and
answer nodes remain distinct. This is request-level memoization, not yet reuse
of arbitrary intermediate closure evaluations.

- [x] Test shared pending requests, split budgets, stable completed answers,
  different captured arguments, nontermination remaining pending and index
  growth. Ordinary and ASan/UBSan checks pass.
- [x] Connect work to typed conversion using the fixed pure semantic policy;
  beta-only and pure-semantic jobs retain distinct keys. Runtime effects are
  not eligible for this conversion cache.
- [x] Budget output graph traversal and semantic-demand readback using the
  shared materializer below. Explicit diagnostic readback remains synchronous;
  callback algorithms and other work still prevent a bound on all execution.

`pointer/conversion.c` adds a separate resumable beta-conversion traversal using
the shared beta jobs. It decomposes normalized Lambda/APP/reference pairs,
tracks the binder correspondence, and memoizes visited pairs with that scope.
The pending stack is work, not an equality certificate. The result can be equal,
different, pending, or allocation error. A same-pointer shortcut is allowed only
outside a binder correspondence. No result is fed into structural interning.

- [x] Explicit beta conversion under lambdas; reject bound/free confusion; test
  alpha-renamed shared DAGs and divergence remaining pending under split fuel.
  Ordinary and ASan/UBSan checks pass.
- [x] Connect beta comparison to typed premises and record conversion evidence.
  Comparison state is private; only a completed equal comparison issues an
  immutable certificate in the program graph arena. A conversion derivation
  requires an existing term derivation, target-type formation in the same
  context and sort, and exact certificate endpoints matching the old and new
  classifiers. It preserves the original occurrence and derivation. Tests
  convert a thunked function between distinct alpha-equivalent Pi classifiers
  and apply it, reject mismatched scopes/sorts/endpoints, and retain the
  certificate after comparison/work-store destruction.
- [ ] Extend admitted comparison semantics beyond beta; attach typed HOTT rules
  and source synthesis. A certificate currently records the verified endpoints
  and fixed beta policy, not a serialized reduction trace. Image loading must
  recompute that check or validate a future retained trace using the same rules;
  it must not trust a certificate reconstructed from endpoint pointers alone.
  `DIFFERENT` currently means different under beta-only neutral-reference
  semantics, not inequality in a future owner-reduction policy or object
  Identity. There are no eta, iota, transport or effect equations in this
  comparator yet. A successful untyped comparison does not establish that
  either input is a well-formed classifier.

## 5. Foundational HOTT Action

Narya sources consulted on 2026-09-07:

- [Observational higher dimensions](https://narya.readthedocs.io/en/latest/observational.html):
  `Id`, `refl`, and `ap` share the higher-dimensional construction; dependent
  identification includes a correspondence along the base identification.
- [HOTT](https://narya.readthedocs.io/en/latest/hott.html): transport and lifting
  supply structure beyond parametricity. The documentation explicitly identifies
  limitations of current computation rules and special difficulties for indexed
  inductive types in its fibrancy construction.
- [Implementation remarks](https://narya.readthedocs.io/en/latest/remarks.html):
  Narya uses De Bruijn indices/levels and intrinsically scoped OCaml structures.
  Adopt its semantic discipline, not those representations.
- [Dimension theory](https://github.com/gwaithimirdain/narya/tree/master/lib/dim),
  [op.ml](https://github.com/gwaithimirdain/narya/blob/master/lib/dim/op.ml),
  [hott.ml](https://github.com/gwaithimirdain/narya/blob/master/lib/dim/hott.ml):
  operators have domain/codomain, act contravariantly, and compose; faces and
  degeneracies are distinct. These moving upstream references must be pinned
  to a commit when implementing the corresponding rules.

The following is the A Program adaptation, not a claim that Narya already
provides a pointer-based CBPV implementation.

**Representation.** From N1, use immutable dimension/operator objects and shared
boundary diagrams whose cells hold term/binder pointers. Start with binary
endpoints and one direction, but no fixed maximum dimension or separate 1D/2D
term tags. Materialize demanded faces instead of eagerly copying every cube.
Dimensions and array sizes may use integers; bound identities remain pointers.

**Two operations to define precisely.** Restriction by a dimensional map and
forming the higher action of a typed term are related but not interchangeable.
For `rho : m -> n`, restriction sends an n-dimensional object to dimension m.
It obeys `restrict(id,t)=t` and
`restrict(sigma,restrict(rho,t))=restrict(rho compose sigma,t)`.
Producing a higher term additionally acts on its context, classifier, and
boundary bindings; raising a dimension counter alone is not a proof.

An internal `Act` operation takes an explicit typed occurrence, dimension map
and boundary environment and produces another typed occurrence plus ordinary
Core. It cannot select an annotation from a naked erased Core pointer. Its
generated executable references contain the chosen immutable descriptors, so
subsequent Core reduction has fixed semantics. A suspended action can itself be
represented through REFERENCE and APP, with no mandatory fourth Core tag.

**Scope.** Acting on a binder produces its boundary binders and center binder in
one persistent scope diagram. Act on Lambda, APP, classifiers and substitutions
together. Check compatibility of iterated restrictions on shared faces and
commutation with substitution/beta. Share the graph traversal machinery, but do
not equate ordinary variable substitution with dimension restriction.

`pg_term_restrict_bindings` connects strict-face restriction of explicitly
listed free boundary bindings to the common term-substitution traversal.
It leaves unlisted references and opaque semantic data untouched. Identity
restriction preserves the original pointer; composed restrictions can produce
distinct alpha-equivalent lambda nodes because capture avoidance freshens
binders. No normalization or alpha interning is used to merge them.

- [x] Term-level fixture checks for identity restriction, a square-to-corner
  composite, beta/restriction commutation and protection of bound variables.
  Ordinary and ASan/UBSan checks pass; these are examples, not a general proof.
- [x] All strict-face entry points now use one validated, interned map path.
  Previously the untyped binding/Term path only counted axes, while the typed
  context path checked coordinate validity. Repeated axes could therefore pass
  the former's face test, and a copied identity map could create a distinct
  boundary binding. Tests require copied maps to reuse the canonical face and
  reject duplicate axes in binding, Term, context and composition entry points.
  Composition validates its maps before substituting coordinates. Existing
  structurally equal maps are retrieved before allocating validation scratch
  space; coordinate interning does not identify terms by reduction or alpha.
- [x] `action.c` builds a checked restriction substitution for an explicitly
  supplied typed boundary telescope. Starting from the empty context, it lifts
  the same substitution through each source declaration using the corresponding
  restricted pointer binder. Consequently later dependent declaration types
  use the earlier restricted variables. NULL binding entries preserve ordinary
  binders. No independent context-rewriting or classifier solver is added.
  Tests compare square-to-edge-to-vertex with direct restriction, including
  dependent classifiers, identity maps and invalid binder/dimension mappings.
- [x] Extend that fixture to checked Lambda/APP derivations over the boundary
  telescope. Restricting an APP agrees up to explicit alpha comparison with
  applying its separately restricted function and argument; their classifiers
  agree and regularity is recovered in the target context. Twice-restricted
  Lambdas agree with direct restriction. Executing the restricted identity
  application yields the independently derived, restricted beta result at the
  same classifier. These tests reuse context substitution/reindex and the
  existing evaluator, without a second action-specific substitution engine.
  They do not construct object Identity witnesses or establish general naturality.
- [ ] General dimension-map action and restriction inside semantic owners
  remain incomplete. The strict-face helper rejects degeneracies: forming a higher witness
  cannot be replaced by generating another free boundary variable.
  The supplied fixture types are ordinary verified telescopes used to check
  substitution preservation. A cube-associated variable is not thereby a path
  or Identity witness. Generating the actual HOTT boundary types, center types,
  witnesses and transport remains a separate required N2 gate. The generated
  contextual boundary checkpoint below implements one-direction declaration
  expansion, not all those operations.

**Identity.** Generate identity families and witnesses from this action rather
than adding an unrelated `Obs(left_type,right_type,left,right)` authority.
For heterogeneous identification retain the chosen family/correspondence and
base witness: identical endpoints can admit different identifications. Do not
erase those choices or collapse all evidence into an endpoint pair. Pointer
sharing and kernel conversion do not imply equality reflection.

### N2 implementation contract: families are not returning computations

#### Next HOTT milestone: polarized family formation, not more evaluator prerequisites

Re-audit at `a7c268f` found no acted-family formation rule. The symbolic rules
below now begin that implementation; type-former computation and transport are
still missing. The preceding fuel work alone did not implement Identity.

The next implementation must define the following together. These equations
are an A Program design proposal to validate, not established CBPV-HOTT theorems
or rules already accepted by the kernel. Here `Id_A`/`Id_C` denote the action of
a specified type family, not a global endpoint-only relation.

- [ ] Value-family action forms a value type. Computation-family action forms
  a computation type. This distinction is a judgement, not separate Core
  Lambda/APP constructors. Both retain the chosen family, its typed boundary
  substitutions and the dimension operator.
- [ ] Computational endpoints are retained as checked computational occurrences.
  Whenever an endpoint must enter a value-only context telescope, use its
  explicit thunk and U type. Do not insert raw computation into ContextDB, or
  treat a formation under a telescope as a function returning `F Universe`.
- [ ] Specify and implement the canonical pure rules:

  ```text
  Id_(U C) (THUNK M0) (THUNK M1)  computes to U (Id_C M0 M1)
  Id_(F A) (RETURN v0) (RETURN v1) computes to F (Id_A v0 v1)
  ```

  With heterogeneous boundaries, `C` and `A` above are selected acted families,
  including their lower-dimensional base evidence. They are not inferred from
  two endpoint classifiers. The equations describe classifier computation;
  they do not claim that arbitrary endpoint pairs have witnesses.
- [ ] For neutral `M0,M1 : F A`, retain a neutral computation-family instance
  with those endpoints. Forming it must neither run M0/M1 nor invent returned
  values. Its introduction/elimination and transport laws remain obligations;
  successful formation alone cannot discharge equality or termination.
- [ ] Pi action consumes a value boundary telescope `x0, x1, x01` and returns
  the acted computation codomain. Lambda and APP action must construct/consume
  this same telescope. The constant-family homogeneous equations above are
  not a substitute for dependent codomain instantiation through `x01`.
- [x] Elementary checked boundary context (after `91bc348`): given accepted
  `R : Id Universe_i A B` in Gamma, `pg_identity_context_extend` constructs
  `Gamma,x0:A,x1:B,x01:R x0 x1`. Endpoint-type regularity rules recover
  `A/B : Universe_i` from that accepted premise; shared Universe-Identity
  inspection also serves existing family instantiation. Ordinary context
  extension, projection and instantiation then construct the telescope. No
  additional Core tag, context store, arbitrary center type or relation
  constructor is introduced. This composes current rules, not a new transport
  principle or a claim that all relations are identifications.
  `tests/identity.c` covers exact proof/context reuse, binding-cube pointers,
  distinct selected R/S despite identical endpoint types, scope/owner/polarity
  and binder-capture rejection, and existing substitution lifting with R:=S:
  the center classifier changes to S x0 x1 while the variable Core stays shared.
  The same telescope supports three nested raw computation Pi/Lambda binders
  without a value-side function constructor.
  Verified with `make -f src/prototype/pointer/Makefile check` in optimized and
  ASan/UBSan builds, plus `identity_test` with a 512 KiB stack. The syntax
  inventory remains 158 reviewed parser outcomes, not semantic acceptance.
  This is only the elementary Universe-indexed value boundary; arbitrary
  higher-family instantiation, dimensional typed Act, Pi/Lambda/APP action
  reduction and transport/lifting coherence remain unchecked above/below.
- [x] One-binder dependent family Identity formation (after `dc30b00`): for
  `Gamma,x:A |- C type`, two checked substitutions with a common ambient
  prefix, and `p : Id A[sigma] x0 x1`, form
  `(refl (lambda x. C[sigma])) x0 x1 p y0 y1` at checked endpoints in
  `C[left]` and `C[right]`. `PG_FAMILY_IDENTITY_FORM` retains the original
  formation, substitutions, p and endpoint proofs. Value/computation polarity
  and the universe bound come from C. All operands remain ordinary Core;
  family abstraction does not introduce a value-side Pi or return a universe.
  This symbolic formation is an A Program rule being developed, not a theorem
  that arbitrary relations are equalities. It supplies no inhabitant or
  transport. Its lambda-body computation is provided by the shared action
  reducer recorded below, not by this formation rule.
- [x] Homogeneous dependent Pi Identity expands by the fixed pure reducer to
  `Pi x0:A. Pi x1:A. Pi p:Id A x0 x1.
  (refl (lambda x.C)) x0 x1 p (f0 x0) (f1 x1)`.
  `pg_identity_pi_type` checks that result using ordinary context extension,
  substitution pairing, reindex, family Identity and Pi formation. Conversion
  checks the original Identity against the expanded type; ordinary APP then
  accepts its three boundary arguments. There is no dedicated higher-APP rule.
  Pi Core construction now takes a graph rather than an unnecessary universe
  registry, so the reducer uses the same constructor without typed lookup.
  Administrative endpoint lambdas preserve evaluator closures; they are not
  accepted source functions binding raw computations as values.
  Tests cover C(z)=z, C(z)=F z, C(z)=Id Universe t z, different selected p/q,
  a mismatched ambient prefix, wrong endpoint polarity/scope, immutable reuse,
  and a genuinely dependent Pi with C(z)=F(Id Universe z z). The latter converts
  to its expanded type and accepts endpoint/center arguments with existing APP.
  Pi WHNF does not execute its endpoints, including an untyped divergence
  fixture; this is an evaluator test, not a termination proof for that fixture.
  The rule follows the boundary shape described in
  [Narya's function Id documentation](https://narya.readthedocs.io/en/latest/observational.html#id-of-function-types),
  checked again during this change. CBPV polarity and the use of raw Pi here
  are our adaptation. Lambda/APP action computation, heterogeneous Pi action,
  multi-binder/higher coherence and transport remain unfinished; the broad Pi
  milestone above must remain unchecked.
  Verified with the complete pointer `make check` in optimized and ASan/UBSan
  builds and `identity_test` with a 512 KiB stack. Parser inventory checks
  still do not establish end-to-end semantic acceptance of legacy examples.
- [x] September 8, after `41cf2ac`: the pure action reducer now consumes
  complete boundary triples for curried Lambda binders. A bound variable
  selects its supplied center; a free constant uses diagonal action. APP
  passes the argument's two endpoint substitutions and its action to the
  function action. Multiple curried variables use one direction rather than
  accidentally iterating refl. Incomplete triples and unreduced higher-action
  heads stay neutral. RETURN/THUNK and F/U/Pi family-body rules use the same
  scope machinery; no new Core tag, recursive graph copier or typed lookup.
  Administrative lambdas delegate capture avoidance to the existing evaluator.
  Variable/constant selection does not allocate fresh boundary binders.
  A scope rewrite is one semantic transition with work proportional to its
  curried arity; this is not a per-node or wall-clock fuel bound. Fine-grained
  scope preparation remains a budget-accounting task. Demand readback has
  subsequently moved to the shared budgeted materializer below.
  Tests cover binder selection, ignored divergent endpoints, lexical capture,
  two curried binders, composition, selected p/q, split budgets, and F/U family
  computation. Checked functions `lambda x. RETURN x` and its APP composition
  yield the supplied path via ordinary Pi conversion/APP; both result terms
  and classifiers compare with independently checked `RETURN p`.
  The checked family `C(Z)=Pi e:Z. F Z` also computes across distinct A/B to
  `Pi x0:A. Pi x1:B. Pi p01:R x0 x1. F(R x0 x1)` for the selected R.
  Changing R to S does not convert to the same expanded type.
  This does not finish arbitrary higher action or its coherence. In particular
  action on an already acted source remains neutral; neutral U observation,
  FORCE/FOLD action laws and transport are not supplied by this change.
  Typed synthesis still needs shared action-derivation jobs to expose these
  results automatically: untyped normalization alone cannot invent evidence.
  Verification: optimized and ASan/UBSan pointer `make check` pass, including
  the 158 parser outcomes. `identity_test` also passes with a 512 KiB stack,
  including a 2048-variable curried action. These are fragment tests, not a
  general coherence or end-to-end legacy-program acceptance proof.
- [x] September 8, after `082ab35`: checked contextual term action.
  `pg_prove_family_action` takes `Gamma,x:A |- t:C`, C's formation, two
  checked substitutions agreeing on Gamma, and the selected
  `p : Id A[sigma] x0 x1`. It constructs

  ```text
  act(lambda x.t[sigma]) x0 x1 p
    : act(lambda x.C[sigma]) x0 x1 p t[left] t[right]
  ```

  Ordinary reindex supplies both endpoints; the existing family Identity
  formation validates the boundary and result classifier. The immutable
  `PG_FAMILY_ACTION` derivation retains that formation and the source proof,
  transitively retaining both substitutions and p. Its polarity is t's, not
  inferred from erased Core. Regularity recovers the retained formation.
  Type/term action share one capture-avoiding abstraction helper. Exact
  immutable-premise lookup precedes reconstruction; no additional evaluator,
  Core tag, value-side Pi, arbitrary-relation witness or Replay path is added.
  This is the one-varied-binder contextual congruence rule for our polarized
  theory. The `ap` laws in [Narya's observational primitives](https://narya.readthedocs.io/en/latest/observational.html#observational-primitives)
  motivate the variable/constant tests; they do not prove this CBPV adaptation.
  Preservation, substitution coherence and general higher action remain
  metatheoretic obligations, not consequences of passing these tests.
  Tests check z:Universe acting to the selected p/q, RETURN/THUNK, a genuinely
  dependent Lambda `C(Z)=Pi e:Z.F Z` followed by three ordinary applications,
  constant ambient action, capture avoidance, exact reuse without added
  Core/proof records, and rejection of wrong classifier, polarity, scope,
  direction, ambient prefix, owner and absent premises. Results pass directed
  normalization and explicit conversion against independently typed terms.
  Verification passed: full optimized and ASan/UBSan pointer `make check`,
  plus `identity_test` with a 512 KiB stack. Parser compatibility is still
  not semantic parity. Change sizes: `evidence.c` +56/-11, `evidence.h` +7/-1
  (implementation net +51); `tests/identity.c` +93/-0, separately from docs.
  General telescope/dimension action jobs, already-acted source computation,
  transport/lifting, surface Identity and N2 acceptance remain unchecked.
- [x] September 8, after `4af636c`: neutral U observation and FORCE action.
  The fixed pure rules now include

  ```text
  Id_(U C) v0 v1 -> U(Id_C (FORCE v0) (FORCE v1))
  act(lambda xs.FORCE v) boundaries -> FORCE (act(lambda xs.v) boundaries)
  ```

  U formation constructs observations without demanding either endpoint.
  It uses the same equation for neutral and canonical values; ordinary
  FORCE/THUNK reduction recovers the previous canonical equation. Scoped U
  action retains C's chosen acted family and lower-dimensional boundary.
  `pg_identity_thunk_type` constructs the homogeneous expanded formation by
  composing existing Identity, U-content, FORCE and U-formation derivations.
  A supplied path becomes forceable only after explicit conversion to that
  U type; there is no new proof rule, Core tag or implicit classifier lookup.
  These are our polarized observational equations. The underlying thunk/force
  introduction/elimination and beta/eta laws are described in
  [Levy's CBPV lectures, equational theory, slide 93](https://www.cs.bham.ac.uk/~pbl/mgsfastlam.pdf)
  (April 18, 2026 version, accessed September 8). That reference does not
  establish the higher Identity extension; global thunk eta conversion is
  not added here. Preservation/coherence remain required.
  Tests cover neutral U endpoints and a supplied path, ordinary FORCE of its
  explicitly converted proof, typed reflexivity commuting with FORCE,
  scoped FORCE computation, dependent U(F Z) across distinct A/B with chosen
  p, owner/polarity rejection and divergent untyped endpoints never demanded
  by U WHNF. Canonical U tests now distinguish structural equality from beta
  conversion: FORCE(THUNK M) remains in the weak-head classifier. The synthesis
  content test checks that conversion explicitly while retaining the original
  unevaluated code. Neutral F observation, general scoped-family conversion,
  FOLD action, transport and arbitrary higher coherence remain incomplete.
  Verification passed: full optimized and ASan/UBSan pointer `make check`,
  `identity_test` and `synthesis_test` with a 512 KiB stack. The 158 parser
  outcomes still do not establish legacy-program semantic parity.
  Sizes excluding docs: `action.c` +11/-0, `action.h` +5/-0, `identity.c`
  +24/-15, `identity.h` +2/-2 (implementation net +25); tests `identity.c`
  +97/-2, `synthesis.c` +12/-1 (test net +106).
- [x] September 8, after `26699b0`: contextual action over dependent telescopes.
  The previous one-varied-binder API is replaced, not wrapped. Family formation
  and term action now take a counted array of center proofs for a suffix Delta
  of the source context Gamma,Delta. Both checked substitutions must agree on
  Gamma. For each declaration `xi:Ai`, the required center classifier is

  ```text
  act(lambda x0 ... x(i-1). Ai[sigma]) preceding_triples xi_left xi_right
  ```

  Earlier centers are validated before constructing this classifier. Later
  endpoint classifiers need not be equal: the selected preceding paths define
  their correspondence. The original checked telescope supplies declarations;
  no classifier is recovered from bare Core. The conclusion closes all varied
  binders before common substitution and applies one action to all triples.
  Zero centers uniformly means diagonal action after common substitution,
  including in the empty context. This arity is not a dimension count.
  The same formation/action rules retain the counted premises, and the same
  substitution/evaluator handles capture and computation. No new Core tag,
  context database or proof rule is introduced. A temporary key array is still
  allocated on lookup; cached requests add no Core or accepted proof records.
  [Narya's heterogeneous Identity account](https://narya.readthedocs.io/en/latest/observational.html#heterogeneous-identity-types)
  motivates retaining the chosen earlier paths. Our polarized telescope rule
  still needs general substitution/dimensional coherence, not just this
  executable fragment; it does not infer transport or higher fillers.
  Tests cover 0/1/2 through 8 varied declarations, Z:Universe followed by
  elements of Z, both polarities, snapshot/reuse, and rejection of missing,
  excess or mismatched centers. Changing the first path invalidates a later
  center typed over the old path. Optimized and ASan/UBSan pointer `make check`
  pass, as do Identity/synthesis tests with a 512 KiB stack. Parser outcomes
  remain distinct from semantic compatibility. General dimension-map action,
  higher source computation, transport/lifting and N2 acceptance remain open.
  Sizes: `action.c` +1/-1, `evidence.c` +75/-39, `evidence.h` +10/-7
  (implementation net +39); `tests/identity.c` +127/-39 (net +88), excluding docs.
- [x] September 8, after `c21c5ce`: generate checked contextual boundaries.
  `pg_identity_context` expands a requested source suffix in declaration order.
  Supplied binding-cube faces identify the center binders; restrictions of
  their last intrinsic axis supply left/right binders. Existing substitution
  reindexes each declared type for the endpoints, and family Identity formation
  checks the center type over all previous centers. Ordinary context extension
  declares that center. The result is an accepted context, two accepted
  substitutions into the source and center-variable evidence in the result.
  Output arrays are merely caller-owned handles; they are not a new authority.
  Failure does not overwrite them. A fixed ambient prefix stays shared.
  This generates well-formed assumptions, not inhabitants of arbitrary closed
  Identity types. No new Core/proof tag or context storage is introduced.
  Repeated requests reuse their context/substitution/evidence and add no Core
  or accepted proof records, although temporary arrays and lookups remain.
  Projecting the growing prefix is not constant-time; compiler-wide fuel and
  large-telescope profiling remain open rather than hidden by this builder.
  Tests construct the 3/9/27 typed faces of successive 1D/2D/3D cubes, verify
  endpoint substitutions and center regularity, and feed the results to typed
  action of a source vertex. They also cover a dependent suffix with a shared
  ambient type, zero expansion, repeated binders, invalid dimensions/counts,
  foreign evidence and unchanged outputs on failure. This is **not** a test
  of arbitrary higher-center computation, transport or coherence. Cube face
  order follows [Narya's higher-dimensional cubes](https://narya.readthedocs.io/en/latest/observational.html#higher-dimensional-cubes);
  our binder pointers, polarized rules and source-context construction are
  the A Program implementation, not an upstream theorem about this kernel.
  General higher-source computation and dimension-map action remain N2 work.
  Verification passed: optimized and ASan/UBSan pointer `make check`, plus
  Identity/synthesis tests with a 512 KiB stack. The 158 parser outcomes do not
  establish legacy semantic acceptance. Sizes excluding docs: `action.c`
  +102/-0, `action.h` +12/-0 (implementation net +114); `tests/identity.c`
  +88/-0. No accepted-source or handmade implementation was modified.
- [x] September 8, after `91fb95c`: syntactically unused action binders.
  The scoped comparison walker now also tests binder independence, by comparing
  a term with itself under an unmatched outer binder correspondence. Inner
  lambdas shadow that correspondence. No freshened term copy, normalization,
  intern-key change or separate support walker is needed. The same query
  replaces substitution-plus-alpha testing of a constant Pi codomain.
  Action removes unused binders and their boundary triples before inspecting
  the family former. It retains used binders in order and consumes no partial
  telescope; discarded endpoints are never demanded. This applies to complex
  constant families and already-action-headed bodies as well as F/Pi/U.
  [Narya's constant-family rule](https://narya.readthedocs.io/en/latest/observational.html#heterogeneous-identity-types)
  motivates this computation; deletion of syntactically unused coordinates in
  our erased curried scope is the A Program adaptation, not a new equality
  reflection or a metatheorem inherited from Narya.
  Previously a variable's action classifier could close a later unused binder
  while the generated center's declared type closed only its original prefix.
  Tests now check **every** source variable, including higher centers, when
  constructing the 3/9/27 declarations of 1D/2D/3D boundaries. Both subject and
  classifier pass directed normalization/conversion against the independently
  generated center. Selected higher classifiers are initially non-alpha-equal.
  Further tests cover omitted leading/trailing binders, repeated-pointer
  shadowing, neutral F endpoints, discarded divergence, incomplete triples,
  split budgets, and shared/deep DAG independence without new Core records.
  The independence query is resumable; current action and kernel consumers
  drive it synchronously. Pruning scans a remaining scope per binder, so it
  can be quadratic in curried arity. Sharing is retained within each scoped
  walk, not globally across all support queries. Fine-grained evaluator fuel
  and profiling/batching remain open; do not claim a per-node callback bound.
  This does not implement general action on acted sources, transport/lifting,
  source-level HOTT or arbitrary higher coherence. N2 remains unfinished.
  Verification: optimized and ASan/UBSan pointer `make check` pass, including
  the 158 reviewed parser outcomes (not legacy semantic acceptance). Identity
  and synthesis tests pass with a 512 KiB stack. Sizes excluding docs:
  `graph.c` +42/-7, `graph.h` +7/-0, `evidence.c` +4/-8, `identity.c` +33/-0
  (implementation net +71); tests `core.c` +25/-0, `identity.c` +43/-8
  (test net +60). Accepted-source and handmade code are unchanged.
- [x] September 8, after `84e78c7`: schedule diagonal action from source jobs.
  `pg_synthesis_reflexivity` subscribes to an existing producer in the same
  job store, waits for synthesis, recovers its checked classifier and invokes
  the existing reflexivity rule. Requests are keyed by context and producer;
  they neither execute the source nor provide an expected type to it. Accepted
  derivations retain the original source evidence. Iteration uses the same
  scheduler, not another action queue or an erased-term classifier lookup.
  Value-type formations use the existing Russell type-to-value rule. Raw
  computation-type formations are not silently treated as universe values.
  Source rejection/unsupported states propagate; dependency cycles remain
  pending without fabricating witnesses. Foreign jobs and mismatched contexts
  cannot supply evidence. Primitive classifier recovery remains synchronous.
  Tests synthesize a source APP with four pending action consumers, compare
  split/bulk results, and extract their returned acted values. A source Lambda
  action converts to a checked expanded Pi and accepts its three boundary
  arguments via ordinary APP; its returned value is separately normalized.
  This explicitly respects RETURN's WHNF boundary rather than claiming its
  contents are already normalized. Tests also cover universe values, exact
  reuse, failed `::` checks, foreign owners, unselected libraries and cycles.
  This is a scheduler API, **not new surface syntax**: general contextual
  action jobs, public Identity/transport syntax, higher computation and N2
  acceptance remain unfinished. No kernel rule, Core tag or reserved word was
  added. Sizes excluding docs: `synthesis.c` +36/-1, `synthesis.h` +7/-0
  (implementation net +42); `tests/synthesis.c` +106/-0.
  Verification passed: full optimized and ASan/UBSan pointer `make check`,
  plus synthesis/Identity tests with a 512 KiB stack. The 158 parser outcomes
  remain parser compatibility only; N2 and legacy semantic parity stay open.
- [x] September 8, after `126f0a7`: initial value-universe transport fields.
  For a **selected** accepted `R : Id Universe_i A B`, introduce checked
  one-dimensional field eliminations:

  ```text
  trr R x : B                  liftr R x : R x (trr R x)   (x : A)
  trl R y : A                  liftl R y : R (trl R y) y   (y : B)
  ```

  These are a new primitive fibrancy contract for the HOTT value universe,
  not consequences of the earlier logical-relation fragment. Arbitrary
  relations/functions or ordinary `Id A x y` do not authorize these fields.
  Future universe introductions, notably IADTs, must satisfy the contract;
  declaring a generated relation alone cannot establish fibrancy. No general
  universe-Identity introduction, glue or equivalence-to-Identity axiom is added.
  Direction is an argument to shared constructors/checkers. Four fixed field
  references use ordinary APP edges, with two evidence rules retaining the
  destination/Identity formation and selected-family premises. Regularity and
  reindex reuse existing algorithms. Core tags remain Lambda/APP/Reference.
  In our CBPV adaptation these are pure **value expressions**, suitable
  as dependent endpoints, not raw computation Pi applications or effect
  requests. This extends the value judgement's term formers without adding a
  value-side Lambda/Pi; callable source wrappers can still use Lambda/RETURN.
  Raw computation endpoints are rejected. No thunk is implicitly forced.
  We adopt diagonal regularity: `trr/trl (refl A) x` compute to x and
  `liftr/liftl (refl A) x` to `refl x`. This is an explicit A Program equation,
  not a claim that Narya implements this exact rule for every neutral type.
  The fixed pure evaluator uses its existing demand/closure machinery; unknown
  R stays neutral. Higher action on field-headed sources also stays neutral
  rather than applying ordinary Pi congruence without a uniform field rule.
  The field types follow [Narya's transport/lifting account](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting),
  checked during this change. Its higher fields and bisimulation requirements
  remain obligations here: these eliminations and diagonal tests do **not**
  establish general coherence, canonicity, transport of arbitrary acted Pi/U/F
  families, symmetry/composition, or N2 completion.
  Tests cover both directions, selected R/S distinction, endpoint and scope
  rejection, exact proof reuse, substitution R:=S, regularity, diagonal
  computation of terms and classifiers, quoted values, partial applications,
  capture, split budgets and neutral arguments not executed by field lookup.
  The divergent fixtures are untyped evaluator checks, not accepted programs.
  Verification passed: optimized and ASan/UBSan pointer `make check`, plus
  Identity/synthesis with a 512 KiB stack. The 158 reviewed syntax outcomes
  remain parser-only evidence. Sizes excluding docs: `evidence.c` +44/-0,
  `evidence.h` +12/-1, `identity.c` +53/-1, `identity.h` +10/-0
  (implementation net +117); `tests/identity.c` +114/-0.
- [x] September 8, after `2d00286`: connect derived classifiers to family
  transport through the existing scheduler. `pg_synthesis_normalize_classifier`
  accepts a checked term in its exact context, recovers its classifier formation,
  schedules shared pure WHNF, and explicitly converts the original typing proof.
  It neither runs the subject nor takes an expected type as synthesis input.
  RETURN/THUNK inversion now uses this job instead of duplicating classifier
  normalization/conversion stages. No new kernel rule, Core tag or Replay path.
  `tests/synthesis.c:family_transport` constructs, for arbitrary `A` and
  `a,b,c:A`, `p:Id A a b`, `q:Id A b c`:

  ```text
  symmetry p    = trr (ap (t |-> Id A t a) p) (refl a) : Id A b a
  composition p q = trr (ap (t |-> Id A a t) q) p      : Id A a c
  ```

  These are checked C-API derivations using existing family action and fields,
  not new primitives or approved surface notation. The resulting transport
  expressions can remain neutral; their reflexivity computations and higher
  groupoid coherence are still unimplemented. Tests check destination types,
  regularity, lifting, subject work remaining unexecuted, split/bulk scheduling,
  repeated-job reuse and wrong context/polarity/owner rejection. Separate solver
  stores currently issue separate conversion receipts: compare their accepted
  judgements, not proof pointers. Same-job reuse preserves the original result.
  `tests/synthesis.c:function_eta` also checks a non-DefEq function Identity:
  for `f:U(Pi x:Universe_0. F Universe_0)`, source `lambda x. f x` and `FORCE f`
  remain different under pure conversion, before and after constructing the
  witness. Existing Pi-Identity expansion makes their Identity classifier
  convertible to that of `refl (FORCE f)`. Converting that evidence proves the
  object Identity; it does not add function eta to DefEq. The same witness fails
  conversion to Identity with the identity function. This negative test is not
  a metatheorem of uninhabitability. No general extensionality API is claimed.
  Regularity recovery and semantic callback work remain partly synchronous;
  this job does not establish a constant-time scheduler step. N2 remains open.
  Verification passed: optimized and ASan/UBSan pointer `make check`, and
  synthesis/Identity tests with a 512 KiB stack. The 158 syntax outcomes remain
  parser-only checks, not full old-example synthesis/evaluation acceptance.
  Sizes excluding docs: `synthesis.c` +50/-31, `synthesis.h` +5/-0
  (implementation net +24); `tests/synthesis.c` +137/-0.
- [x] September 8, after `48b037b`: diagonal substitution for scoped action.
  Add the canonical reduction `ap F (refl a) -> refl (F a)`, simultaneously
  for all variables of a curried source telescope. It also applies when the
  source body is an Identity instance or a transport field, where ordinary
  action dispatch must otherwise remain neutral. The selected triples must
  have exactly the same left/right closure and a syntactic reflexivity of
  that closure; identical term pointers under different environments do not
  qualify. An arbitrary path with equal endpoints is not collapsed to refl.
  No endpoint normalization, alpha interning or checker lookup is performed
  to select the rewrite. Administrative Lambda/APP reuses ordinary capture-
  avoiding evaluation; a read-only argument cursor avoids repeated prefix
  walks while inspecting all triples. Partial applications stay neutral.
  This is the specialization of action/substitution compatibility to a
  diagonal substitution. [Narya's observational laws](https://narya.readthedocs.io/en/latest/observational.html#observational-primitives)
  explicitly include the corresponding ap/refl equation (checked September 8).
  The CBPV polarity discipline and closure implementation remain our adaptation.
  Tests now derive **and reduce** symmetry of refl and composition of two
  reflexivities, including their lift witnesses. A dependent `(Z,e:Z)` family
  `Id Z e e` checks both term and classifier computation under two diagonal
  paths. Tests also cover multiple curried arguments, an incomplete spine,
  a non-reflexive chosen loop, mismatched endpoints, captured environments,
  and split/whole step agreement. Earlier tests expecting a field-headed
  action to stay neutral now distinguish diagonal from arbitrary input paths.
  This does not implement general action on acted/field-headed sources,
  naturality for an opaque neutral callee, higher interchange/coherence, or
  nontrivial unit/associativity laws for the derived groupoid operations.
  The arity scan is linear but still one callback transition, not a wall-clock
  fuel bound. N2/N3 remain open; constructor telescopes still need checked
  declarations, dimensional action and indexed fibrancy before acceptance.
  Verification passed: optimized and ASan/UBSan pointer `make check`, plus
  Identity/synthesis with a 512 KiB stack. The 158 syntax outcomes remain
  parsing coverage rather than full legacy program acceptance.
  Sizes excluding docs: `identity.c` +24/-0, `eval.c` +11/-3,
  `eval.h` +3/-0 (implementation net +35); `tests/identity.c` +55/-0,
  `tests/synthesis.c` +11/-0.
- [x] September 8, after `51bca5b`: neutral-callee diagonal application.
  Generalize the preceding ap/refl computation to the first complete diagonal
  argument triple, without requiring a syntactic Lambda or its entire curried
  telescope. A partial application with no path remains neutral; a completed
  diagonal triple reduces even when further function arguments remain absent.
  Preserve exact closure checks: different environments and arbitrary selected
  loops are not reflexivity. Four administrative binders use ordinary beta
  evaluation, not a new substitution implementation or Core constructor.
  Two orientations are necessary. Expanding `act(f a)` back into
  `ap f (refl a)` would loop, so zero-scope neutral APP reflexivity stays intact.
  Likewise retain `act(FORCE v)` and orient `FORCE(act v)` toward it; otherwise
  normalizing a callee before its application could hide the ap/refl rule.
  Scoped non-diagonal APP/FORCE action still distributes, and canonical
  FORCE/THUNK and RETURN/THUNK action retain their computations.
  Tests cover partial telescopes, neutral and forced callees, loop rejection,
  closure separation, and checked application/result classifier regularity.
  The typed test compares whole-application normalization with callee-first
  normalization using the same evidence rules and synthesis work store.
  This is not a proof of general confluence: recognition remains syntactic
  and does not decide whether an arbitrary supplied path converts to refl.
  General higher action on acted/field-headed terms and indexed fibrancy are
  still missing. No declaration/Match implementation, `.a` loader, independent
  Replay engine, or new public syntax is introduced by this checkpoint.
  Verification: optimized and ASan/UBSan pointer `make check`; Identity and
  synthesis tests with a 512 KiB stack. Syntax inventory is still parse-only.
  Sizes excluding docs: `identity.c` +25/-22, `computation.c` +8/-0
  (implementation net +11); `tests/identity.c` +19/-2,
  `tests/synthesis.c` +30/-0 (tests net +47).
- [x] September 8, after `2d941a3`: checked heterogeneous Pi expansion.
  `pg_identity_family_pi_type` constructs the argument boundary and expanded
  computation Pi for a source family under selected telescope paths, not only
  a homogeneous Pi. For `C = Pi(x:A).D`, substitutions `s0/s1` and paths `ps`,
  its shape is:

  ```text
  Pi(x0 : A[s0]). Pi(x1 : A[s1]). Pi(p : Act(ps,A) x0 x1).
      Act(ps,p,D) (f0 x0) (f1 x1)
  ```

  Formation uses the existing Context extension, composition/pairing of
  substitutions, family Identity and Pi rules. The old homogeneous helper
  now delegates to this operation with an empty varying telescope. No new
  proof rule, Core tag, value-side Pi or conversion equation is required.
  Conversion from symbolic Identity remains a separate checked operation.
  Tests use a non-reflexive universe path between distinct type variables,
  both ordinary identity functions and an argument-dependent result
  `F(Id X x x)`. They check expanded classifiers against the independent
  reducer, checked triple application, and reduction of the first function
  action to RETURN of the supplied argument path. Reversing a path, omitting
  required paths or reusing a boundary binder rejects. Different selected
  paths between the same endpoints remain distinguishable in the result.
  Regularity tests now allow explicit alpha comparison of independently
  substituted classifiers; pointer equality would incorrectly require alpha
  interning after freshening. No kernel acceptance condition was relaxed.
  Verification: optimized pointer `make check` (2.382 s, including rebuilding
  the changed Identity test), ASan/UBSan `make check`, and Identity/synthesis
  tests with a 512 KiB stack. The 158 syntax cases are still parse-only.
  Implementation: `action.c` +47/-15, `action.h` +10/-0 (net +42);
  tests: `tests/identity.c` +84/-1 (net +83); docs counted separately.
  This is a formation/instantiation milestone, not general function transport
  or computation-family lifting. Those need their own polarized field rules;
  treating a neutral F computation as a returned value would not supply them.
  N2 and N3 remain open. Rechecked primary references:
  [dependent function Identity](https://narya.readthedocs.io/en/latest/observational.html#heterogeneous-identity-types)
  and [uniform transport/lifting](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting).
  The three-value boundary follows that account; the computation codomain and
  implementation by ordinary CBPV Pi formation are the A Program adaptation.
- [x] September 8, after `3cf93e2`: schedule selected family action.
  `pg_synthesis_family_action` waits for a producer's own synthesis, recovers
  its classifier and applies the existing checked family-action rule along
  supplied substitutions and selected paths. It uses the same ready queue,
  dependency subscription and immutable evidence store as reflexivity, not a
  second solver or Replay engine. Requests do not execute the producer or
  issue evidence; cyclic producers remain pending and failures propagate.
  The common job index now supports an ordered, variable-length pointer key.
  Every selected path is part of that key; a caller's mutable array is copied
  into the immutable request. Existing binary requests use the same index.
  Tests cover zero/two centers, parsed computation/value-type producers,
  split/bulk completion, foreign jobs, invalid boundaries and repeat reuse.
  A checked `R : Id Universe A B` is instantiated under `x:A,y:B`, then this
  type construction is acted along paths in A/B. Its computed form is
  `act R x0 x1 px y0 y1 py`, and its classifier can be normalized to a universe
  Identity between `R x0 y0` and `R x1 y1`. A further checked instantiation
  at two inhabitants constructs the corresponding square type.
  This tests selected instantiation, not merely iterated diagonal refl. A
  second path with the same endpoints yields distinct work/evidence. Since
  later center types retain earlier selected paths, replacing a path without
  updating dependent center evidence fails; explicit conversion repairs the
  constant-family test without weakening the primitive rule.
  The underlying-family account follows the primary
  [higher-dimensional cubes discussion](https://narya.readthedocs.io/en/latest/observational.html#higher-dimensional-cubes),
  rechecked September 8. The typed work scheduling is our implementation.
  This forms a square type, not a filler for arbitrary boundaries, a proof of
  higher coherence, a new Universe witness constructor or public syntax.
  Boundary paths are already accepted inputs, not pending path-producing jobs;
  primitive evidence construction and request key traversal remain synchronous.
  Verification: optimized pointer `make check` in 2.773 s and ASan/UBSan in
  9.186 s (each rebuilding the changed synthesis test), plus Identity and
  synthesis with a 512 KiB stack. The syntax inventory remains parse-only.
  Implementation: `synthesis.c` +63/-11, `synthesis.h` +8/-0 (net +60);
  tests: `tests/synthesis.c` +142/-0; documentation counted separately.
  N2/N3 remain open, particularly higher field computation, canonical
  transport/lifting, indexed fibrancy and full source-level access.
- [x] September 8, after `aeb26f7`: erased constructor/Match execution.
  `iadt.c/h` introduces immutable, generative layout references and ordinary
  APP spines for fields, scrutinee and branches. Constructor labels are object
  pointers; array positions only locate branches inside one owning layout.
  No Core tag, alpha interning, semantic integer identity or hidden branch
  traversal is added. The builder places labelled clauses in layout order;
  the interner still compares exact pointer tuples, never reduction results.
  The fixed pure evaluator demands the scrutinee using its existing machine,
  selects only an exactly saturated constructor of the same layout, and
  applies the chosen ordinary lambda to its fields. Existing closure and
  substitution machinery preserves captured variables and trailing arguments.
  Partial, foreign and neutral heads remain neutral; unused fields and cases
  are not evaluated. Beta-only WHNF keeps Match opaque, with separate job keys.
  Tests cover these boundaries, nested Match, free-field capture, split/bulk
  budgets, repeated job reuse, divergent selected cases and diagonal Act after
  iota. This last test does not supply action on neutral Match or higher data.
  Layout arities are erased runtime information, not semantic schemas or
  evidence. Before typed declaration/Match admission, derive and check them
  against the accepted field telescope and justify iota subject reduction
  through typed substitution and the indexed motive. Positivity, IH, indexed
  fibrancy, higher constructors and source ADT synthesis remain required.
  Shared demand/readback is budgeted; callback-local field/spine traversal and
  application construction are synchronous linear work, not a wall-clock bound.
  Optimized pointer `make check` passed in 2.047 s including rebuilding the
  changed test; ASan/UBSan `make check` passed in 7.435 s, also rebuilding
  that test. Iadt/Identity/synthesis pass with a 512 KiB stack.
  The 158 syntax inventory cases remain parsing checks, not execution parity.
  Implementation: `iadt.c` +105, `iadt.h` +26, `computation.c` +3/-1
  (net +133); tests: `tests/iadt.c` +121; build: +7/-2; docs separate.
  N2/N3 remain open; this is the erased execution foundation, not IADT acceptance.
- [x] September 8, after `02a7ad0`: checked field telescopes derive layouts.
  `pg_data_schema` keeps accepted Context evidence for the parameter prefix
  and each constructor's dependent field extension. No new telescope nodes,
  field-type arrays, value-side Pi or nominal type-formation rule are added.
  Arity is derived from those contexts once into the immutable erased layout;
  the evaluator still reads no typing/proof state. Fresh schema creation is
  generative, even for equal field lists. Existing binder/context pointers,
  not proof-pointer equality, determine whether the parameter prefix matches.
  `pg_data_instance` assembles parameter images and field values for the
  existing checked substitution rule. Its result is ordinary substitution
  evidence, with the same interning and dependent classifier checks, not a
  constructor-membership certificate or a second schema-specific solver.
  Tests use `A : Universe, x : A, p : Id A x x`: a proof for another field
  value is rejected; the corresponding reflexivity proof succeeds. They also
  reject computation-valued fields, foreign proof stores/owners, wrong arity
  and unrelated prefixes, and check empty telescopes and derived-layout iota.
  The existing context Act generates the two-field boundary telescope; both
  endpoint field instances reuse its substitution evidence with identical
  premises. An initial test incorrectly expected identical proofs after
  replacing a projected parameter proof by a direct variable proof; the test
  now retains the original premises, without merging distinct derivations.
  Different derivations of the same parameter context remain accepted.
  This validates sharing of field/context action, not action on a new nominal
  datatype. Result-index schemas, recursive self binding/positivity, nominal
  formation/constructor evidence, typed Match/IH and higher fibrancy remain
  open N3 work; raw field schemas cannot bypass any of these gates.
  Schema construction and the underlying substitution checker are currently
  synchronous; the latter can revisit dependent prefixes. Do not claim a new
  budgeted declaration solver or improved asymptotic substitution complexity.
  Verification: optimized pointer `make check` passed in 2.039 s including
  rebuilding the changed test, plus iadt/Identity/synthesis at a 512 KiB stack.
  ASan/UBSan `make check` passed in 6.832 s with the changed test rebuilt.
  Implementation +107 lines (`iadt.c` +80, `iadt.h` +21, evidence API +6),
  tests +114; documentation counted separately. N2/N3 remain open.
- [x] September 8, after `209284f`: constructor results are checked index maps.
  Replace the field-only schema API, rather than keeping a compatibility path.
  With parameter context Gamma, index extension Gamma.I and constructor field
  extension Gamma.Delta, each result is an ordinary checked substitution
  `r : Gamma.Delta -> Gamma.I` over Gamma. The schema stores r; its domain
  supplies the field context and derived erased arity. No result-type array,
  index evaluator or new evidence rule is introduced. Zero-constructor schemas
  retain their checked index context. Fixed parameters must map to their own
  binder references; any required conversion must already be explicit evidence.
  For an argument instance `s : Theta -> Gamma.Delta`, `pg_data_result` uses
  ordinary checked composition to obtain `r o s : Theta -> Gamma.I`. Open
  index terms remain symbolic, with no execution needed to determine a value.
  Tests cover dependent indices `i:A,q:Id A i i`, chosen field paths, repeated
  evidence reuse, wrong source/owner and parameter mutation. Context action
  on the fields gives both endpoint index maps; acting on their defining
  terms computes to the corresponding selected paths. This is action on
  existing checked terms, not a proof of fibrancy of the new indexed datatype.
  Constructor membership, recursive self/positivity, typed Match/IH, nominal
  and higher datatype acceptance remain open N3 requirements. Synchronous
  composition retains the underlying substitution checker's cost/fuel limits.
  Optimized pointer `make check` passed in 6.712 s with affected binaries
  rebuilt; iadt/Identity/synthesis passed with a 512 KiB stack.
  ASan/UBSan `make check` passed in 15.813 s, including affected rebuilds.
  Implementation: `iadt.c` +56/-16, `iadt.h` +14/-6 (net +48);
  tests +78/-18 (net +60); docs separate. N2/N3 are not closed.
- [x] September 8, after `6528356`: synthesize typed branch closures.
  `pg_data_branch` recovers a synthesized computation body's own classifier
  and abstracts the constructor field context with existing Pi formation and
  Lambda introduction. It retains the parameter prefix, adds no evidence rule
  or constructor-specific Pi, and does not infer from an expected motive.
  Raw values require an explicit RETURN before this operation; raw Pi bodies
  stay computations rather than being wrapped in F/U. Zero fields return the
  original computation proof. Repeated construction reuses the proof DAG.
  Tests apply dependent branches to checked field values, validate directed
  normalization of that application, and compare with typed body substitution,
  index-motive instantiation and erased Match execution. Wrong context, foreign
  owner and value bodies are rejected. Branches returning nested lambdas keep
  their raw Pi classifier. On selected field boundaries, the acted branch Core
  and checked body action convert to the same RETURN of the selected path.
  An initial test required identical WHNF pointers under RETURN; it was fixed
  to use explicit conversion, not by strengthening WHNF or changing interning.
  This certifies branch functions, not a complete typed Match: scrutinee
  membership, motive compatibility for all cases, recursive IH and higher
  datatype/transport rules remain required. No new nominal type is admitted.
  Construction uses synchronous existing classifier recovery/Pi checks.
  Optimized pointer `make check` passed in 2.236 s with the changed test rebuilt;
  ASan/UBSan `make check` passed in 7.612 s, also rebuilding the changed test;
  iadt/Identity/synthesis passed with a 512 KiB stack. Implementation +26 lines
  (`iadt.c` +18, `iadt.h` +8), tests +74, docs separate. N2/N3 remain open.
- [x] September 8, after `faad274`: one-direction erased Match action.
  An acted constructor carries left/right/path triples for its fields. Acting
  on its matcher selects the corresponding branch path and applies those
  triples using the same lexical branch-application helper as ordinary iota.
  The existing diagonal-prefix compression is decoded without losing any
  explicitly selected path. Different chosen paths with equal endpoints remain
  different inputs; branch paths are not replaced with reflexivity.
  A single private `match-action` semantic reference is the lowered operation,
  with matcher and branches in ordinary APP operands. It avoids expanding a
  compressed Act prefix back into the same diagonal rewrite, which would loop.
  It adds neither a Core tag, a datatype-specific higher evaluator nor a proof
  authority. Demand, closure readback, budgets and policy-keyed caching remain
  in the existing evaluator. Local spine decoding is synchronous linear work.
  Tests cover selected field/branch paths, diagonal and mixed prefixes, capture,
  trailing applications, neutral/foreign/partial/oversaturated constructors,
  unselected divergence, beta-policy isolation and split-budget step equality.
  A dependent checked field telescope also connects erased Match action to
  existing typed body action by conversion, including an Identity-valued field.
  This does not certify constructor membership or typed Match, and is not a
  general higher-constructor, transport or indexed fibrancy rule. Those N3
  obligations remain open; the next step is checked nominal family admission
  and motive/constructor compatibility using the existing context maps, with
  recursive self and higher formation obligations explicit rather than assumed.
  Optimized pointer `make check`: 2.215 s; ASan/UBSan: 7.448 s, both including
  the changed test rebuild. IADT/Identity/synthesis also pass at a 512 KiB stack.
  Implementation +90/-7 (net +83); tests +120/-6 (net +114); docs separate.
- [x] September 8, after `1134be9`: post-check an index-motive case.
  Before nominal admission, connect the already checked result maps and branch
  proofs: `pg_data_branch_motive` derives `C[r]` by ordinary reindexing, where C
  is a computation-type motive in the index context and r is the constructor
  result map. `pg_data_case` takes an independently synthesized body and an
  explicit completed conversion certificate, then uses existing conversion and
  Pi/Lambda rules. No expected type guides synthesis, no reduction runs inside
  the case check, and no new rule, mutable schema result or evaluator is added.
  Its proof DAG retains both the body derivation and the motive/result-map
  derivation; repeated requests reuse those accepted nodes. Tests cover dependent
  indices, raw Pi motives, empty field telescopes, instantiated result types,
  missing and wrong-target certificates, wrong context/polarity and foreign
  ownership. An initial test incorrectly required every comparison to start
  pending; exact comparisons can finish immediately. The corrected test checks
  missing certificates and pending certificate visibility separately.
  Optimized pointer `make check`: 2.217 s (changed test rebuilt); ASan/UBSan:
  15.856 s (affected binaries rebuilt); IADT/Identity/synthesis pass at 512 KiB.
  Implementation +33 (`iadt.c` +19, `iadt.h` +14); tests +67; docs separate.
  N2/N3 stay open: this checks an index-dependent case function, not a complete
  Match, recursive IH, a motive depending on the scrutinee itself, or nominal
  datatype fibrancy. Shared scheduler integration and checked nominal family
  formation/membership remain next, with recursive and higher obligations intact.
- [x] September 8, after `2c6dc48`: schedule independent case synthesis and
  shared typed reindexing. `pg_synthesis_data_case` interns the immutable
  schema/constructor/motive/producer tuple without computing a target or body.
  It waits for the producer, subscribes to `pg_synthesis_reindex(r,C)`, advances
  the existing conversion machine, then abstracts the checked body. Reindex
  jobs are keyed by the accepted substitution/proof pair and use the existing
  suspended reindex machine; no second substitution algorithm or work queue
  is introduced. Completed evidence remains in the ordinary proof DAG.
  Tests use parsed branch bodies with dependent field/index contexts. They
  check waiting before synthesis, suspension during substitution, producer-first
  versus consumer-first scheduling, shared completed work without new steps,
  unchanged producer evidence after a failed post-check, invalid inputs,
  unsupported-result propagation, cyclic dependencies and cancellation cleanup.
  No surface Match rule is enabled by this scheduler API. Nominal membership,
  scrutinee-dependent motives, recursive self/IH and higher datatype acceptance
  remain open N3 work. Pi abstraction and primitive premise checks are still
  synchronous; this does not claim a fully budgeted type checker.
  Optimized pointer `make check`: 3.082 s; ASan/UBSan: 9.148 s (both rebuilding
  the changed synthesis test); synthesis/IADT/Identity pass at a 512 KiB stack.
  Implementation +100/-1 (net +99); tests +109; docs separate. N2/N3 stay open.
- [x] September 8, after `c6c7b4b`: typed action on substitution images.
  `pg_identity_substitution_images` acts on selected trailing images using their
  recovered classifiers and existing family-action evidence. Outputs are
  committed to the caller's array only after all requested images succeed;
  the accepted proofs themselves remain shared immutable DAG nodes. No new
  relation, map-equality rule or datatype-specific action evaluator is added.
  The indexed-schema test now constructs a map between the expanded field and
  index contexts, including an Identity-valued index. Its two endpoint
  projections agree with composing the original result map with each field
  projection. Direct alpha-only substitution initially rejected the center
  classifiers: action followed by substitution requires conversion. Explicit
  existing conversion proofs followed by substitution pairing close the example;
  the primitive substitution rule is not weakened. Tests also check diagonal
  action, proof reuse, invalid paths/ownership/arity and unchanged output arrays
  on failure. A test-local duplicate variable name was corrected during build.
  This finite diagram is not a general naturality or fibrancy theorem. The
  review therefore does not promote raw result schemas to HOTT universe values:
  they still lack transport/lifting along arbitrary index paths. Narya's
  [indexed fibrancy caveat](https://narya.readthedocs.io/en/latest/hott.html#hott-inside-parametricity)
  remains relevant; its replacement construction is not supplied by this helper.
  Nominal admission must retain those obligations, not silently equate a
  relation-preserving schema with an equality-supporting datatype. N2/N3 remain
  open. This helper uses synchronous regularity/action checks; scheduler-level
  budgeted image action and the full datatype transport contract remain work.
  Optimized pointer `make check`: 2.514 s (changed test rebuilt); ASan/UBSan:
  16.224 s (affected binaries rebuilt); IADT/Identity/synthesis pass at 512 KiB.
  Implementation +40 (`action.c` +29, `action.h` +11); tests +58/-5; docs separate.
- [x] September 8, after `418ff93`: schedule conversion-aware substitution
  pairing. `pg_synthesis_substitution_pair` interns the checked prefix map,
  source extension and independently typed image. It subscribes to the common
  reindex job for the dependent field type, uses the existing comparison machine,
  and calls the existing pairing rule only with accepted conversion evidence.
  No image synthesis is guided by the target, and no Core/evidence rule is added.
  The synthesis test constructs an acted result map with a dependent
  Identity-valued index through these jobs, checks both endpoint projection
  equations, compares split and bulk results, and checks that repeated requests
  take no new solver steps. Invalid input shapes and mismatched image types are
  rejected without changing the prefix proof. Thus the conversion/pairing loop
  previously demonstrated in the IADT test is available to compiler work too.
  Reindex and comparison are suspendable; the pairing rule still constructs
  flat premise/image arrays and checks them synchronously. Repeated extension
  can therefore retain quadratic aggregate prefix cost; this checkpoint neither
  hides that limitation nor creates a second persistent-map representation.
  Optimized pointer `make check`: 3.151 s (changed synthesis test rebuilt);
  ASan/UBSan: 16.683 s (affected binaries rebuilt); synthesis/IADT/Identity pass
  at 512 KiB. Implementation +40/-1 (net +39); tests +74/-3; docs separate.
  N2/N3 remain open: complete nominal admission, recursive fields/IH and indexed
  transport/lifting still require their own rules and verification.
- [x] September 8, after `40937eb`: reuse checked substitution prefixes in
  pairing/lifting. Accepted prefix images retain their terms and classifiers
  under projection; only the added declaration needs reindexing and an alpha
  check. Use the existing reindex evidence (including the scheduler's result),
  and intern the same ordinary substitution with the same flat image premises.
  No second map representation, proof rule or unchecked acceptance is added.
  The regression extends a map whose prefix contains a dependent thunked Pi.
  With the final domain already reindexed, pairing adds exactly one proof and
  zero Core terms; the direct full-map API returns that same proof. Compiling
  this test against `40937eb` evidence.c fails the zero-new-terms assertion:
  the old path unnecessarily freshens binders in the already checked prefix.
  Existing rejection, lift, dependent image and scheduled conversion tests pass.
  Flat premise/binding arrays still copy on extension, and projection/alpha
  checks remain synchronous; this removes prefix type recomputation, not the
  quadratic aggregate storage cost or all unbudgeted work. N2/N3 remain open.
  Optimized pointer `make check`: 2.291 s (changed core test rebuilt);
  ASan/UBSan: 16.801 s (affected binaries rebuilt); core/IADT/Identity/synthesis
  pass at 512 KiB. These are verification durations, not a comparative speedup.
  Implementation `evidence.c` +27/-8; tests `core.c` +19/-0; docs separate.
- [x] September 8, after `5216661`: accepted evidence can seed the existing
  synthesis dependency graph through `pg_synthesis_evidence`. Registration is
  already complete, keyed by the exact owned evidence pointer, and never queued
  for resynthesis, reduction or Replay. Consumers use ordinary subscriptions.
  The dependent-index result-map test now schedules each image's action using
  this input and the existing family-action job, then assembles the map using
  conversion-aware pairing. Split/bulk execution agrees with direct image action
  and preserves both endpoint projections. No datatype-specific action job or
  second checker is introduced. A same-Core Lambda with two different universe
  annotations produces distinct inputs; registration adds no Core/proof records
  or solver steps, repeated requests share, and foreign/null evidence rejects.
  Suspension is between image jobs: regularity and the family-action primitive
  still run synchronously within a step. This does not solve higher fibrancy or
  implement `.a` loading; a future loader must establish accepted evidence before
  registering it here. N2/N3/N5 remain open.
  Optimized pointer `make check`: 3.134 s (changed synthesis test rebuilt);
  ASan/UBSan: 17.149 s (affected binaries rebuilt); synthesis/Identity/IADT pass
  at 512 KiB. Implementation `synthesis.c` +13/-2, `synthesis.h` +6/-0
  (net +17); tests `synthesis.c` +68/-2; docs separate.
- [x] September 8, after `5de7483`: canonical U/F transport and lifting.
  Let `R = Act(lambda delta. A) boundary` and
  `Q = Act(lambda delta. U(F A)) boundary`. For each of the four fields `h`,
  the fixed pure reducer now computes
  `h(Q, THUNK(RETURN(v))) -> THUNK(RETURN(h(R,v)))`.
  Complete selected boundary triples are retained through ordinary Lambda/APP
  substitution. The Core tags and accepted evidence rules are unchanged.
  Typed tests derive both sides independently, convert their classifiers, and
  compare their terms for both directions and lifting/transport, using two
  distinct selected universe paths. They cover lexical capture, split/bulk
  evaluation, beta-only policy isolation and a suspended divergent payload.
  The new conversion test fails against `5de7483` identity.c.
  This is a canonical computation equation, not general U/F fibrancy. Unknown
  quoted bodies stay suspended, including bodies whose RETURN would require
  further reduction; completeness of conversion is not established here.
  A general transport by `THUNK(FOLD(FORCE(u), lambda x. RETURN(tr(R,x))))`
  must agree with diagonal transport: for reflexivity this needs the U/F eta
  and fold right-unit equations, not just beta. General lifting also needs its
  dependent computation contract without duplicating an effectful evaluation.
  Implement/check those obligations before extending the rule to arbitrary
  thunks; do not delete the general U/F/Pi and higher requirements from N2.
  Rechecked [Narya transport/lifting](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting):
  four fields of a selected universe identification and their uniform higher
  instances. The polarized U/F equation and its eta obligation are A Program
  adaptations, not a claim that Narya supplies CBPV rules.
  Optimized pointer `make check`: 7.508 s (affected binaries rebuilt);
  ASan/UBSan `make check`: 17.074 s (affected binaries rebuilt);
  Identity/synthesis/IADT pass at 512 KiB.
  Implementation `identity.c` +54/-2, `identity.h` +3/-0 (net +55);
  tests `identity.c` +76/-0; docs separate. N2/N3 remain open.
- [x] September 8, after `555e6ac`: structural U eta and F right unit.
  The fixed pure evaluator contracts `THUNK(FORCE(v))` to `v` and
  `FOLD(M, lambda x. RETURN(x))` to `M`, preserving the operand's closure.
  Right-unit removal is also allowed under THUNK, without executing its body.
  `pg_computation_eta` shares these structural contractions with Act under
  binders; a selected input path is preserved when acting on an eta expansion.
  This is explicit reduction, never conversion-based Core interning.
  The equations are supported by the typed CBPV equational theory in
  [McDermott/Mycroft, Fig. 5](https://dylanm.org/value-name.pdf).
  Sharing the contraction with dimensional action is our implementation choice;
  this does not establish the full dependent/higher equational theory.
  Tests check accepted typings for neutral U(F A) and U(Pi A B) values, the
  combined thunk/fold/force contraction, captured scopes, one-step/bulk agreement,
  beta-only isolation and chosen-path action. Constant-return and divergent
  continuations are not mistaken for the unit. Suspended divergence is not run.
  The old assertion retaining `FOLD(neutral, return)` now expects the neutral
  source, as required by the added equation. The new typed eta test fails with
  `555e6ac` computation/identity sources. During development, a misplaced cache
  destruction in the new test was corrected, and its composite Act case exposed
  the need to share right-unit contraction beneath THUNK too.
  These are structural eta cases, not a decision procedure recognizing every
  continuation convertible to the unit. General U/F transport, dependent
  lifting and higher coherence remain open; no eager continuation evaluation
  may be used to bypass those obligations. N2/N3 remain open.
  Optimized pointer `make check`: 7.392 s (affected binaries rebuilt);
  ASan/UBSan `make check`: 17.278 s (affected binaries rebuilt);
  core/Identity/synthesis/IADT pass at 512 KiB.
  Implementation `computation.c` +54/-11, `computation.h` +3/-0,
  `identity.c` +5/-0 (net +51); tests `core.c` +74/-2; docs separate.
- [x] September 8, after `8387b88`: transport suspended U/F computations.
  For `Q = Act(lambda A. U(F A)) A0 A1 R`, either transport direction on
  `u : U(F A_source)` constructs
  `THUNK(FOLD(FORCE(u), lambda x. RETURN(tr_direction(R, x))))`.
  This uses the existing nondependent FOLD: its result type `F A_target`
  does not depend on `x`. Source computation occurs once under suspension.
  The previous canonical returned-thunk equation remains a specialization;
  lifting of an unknown suspended computation still stays neutral.
  No new Core tag, evidence rule, handler, Replay path or type lookup is added.
  Tests independently derive the mapped thunk with ordinary accepted Pi,
  FORCE/FOLD/RETURN/THUNK rules for both directions and two selected paths.
  They check classifiers, normalization evidence, substitution after mapping,
  capture, split budgets, beta-only isolation and suspended divergence.
  A runtime-only counting dispatcher observes zero executions on transport
  and exactly one per force; it never enters the pure memo/evidence store.
  The neutral mapped-thunk comparison fails with the `8387b88` identity source.
  Optimized pointer `make check`: 7.535 s; ASan/UBSan: 17.061 s (both rebuild
  affected binaries); core/Identity/synthesis/IADT pass at 512 KiB stack.
  Implementation `identity.c` +16/-8, `identity.h` +3/-1 (net +10);
  tests `identity.c` +87/-5; documentation separate. N2/N3 remain open.
- [x] September 8, after `c9fcc1a`: close the concrete neutral `map refl`
  conversion gap. The typed regression in `transport_fields` compares direct
  transport with the independently typed U/F map in both directions; the old
  conversion source fails this test. No Identity-specific comparison exception
  was added. Failed WHNF comparison now requests budgeted strong normalization
  through the same pure evaluator, then the same scoped structural comparator.
  NF jobs share exact `(input pointer, policy pointer)` keys and WHNF work.
  They normalize children after exposing the head and reduce rebuilt parents
  again, so a reduced continuation can expose the existing fold right unit.
  Completed normal forms are registered as their own answers without another
  traversal. Explicit dependency stacks avoid recursive C descent; interleaved
  roots reuse child progress. NF step counts charge transitions to the root
  being advanced, not again to every ancestor of a shared dependency.
  Runtime WHNF behavior and its suspension discipline are unchanged. Only
  pure normalization descends into retained Lambda/THUNK bodies; divergence
  stays pending without a certificate. The fallback does not prove confluence,
  strong normalization or completeness for unimplemented higher equations.
  General dependent lifting and higher field action/coherence remain open.
  Tests cover generic beta-hidden CBPV eta (not only Identity), exact sharing,
  result reuse, policy separation, split/bulk NF and comparison, 10,000 nested
  binders, a shared DAG, retained/discarded divergence and accepted typings.
  Distinct WHNF jobs can still revisit overlapping application spines; shared
  NF results do not justify a claim of globally linear normalization cost.
  Optimized pointer `make check`: 7.558 s with affected binaries rebuilt;
  final added test run: 2.645 s with only core rebuilt. ASan/UBSan: 16.936 s
  with affected binaries rebuilt; core/Identity/synthesis/IADT pass at 512 KiB.
  Implementation `eval.c` +137/-1, `eval.h` +18/-0, `conversion.c` +30/-6,
  `conversion.h` +5/-3 (net +180); tests `core.c` +90, `identity.c` +9;
  documentation separate. N2/N3/N5 remain open, including NF CLI integration.
- [x] September 8, after `cece74a`: fix source application of computed
  classifiers. A named reflexive action of `lambda T:Universe0. RETURN T`
  was rejected by the old direct Pi inspection although its classifier reduces
  to the accepted boundary telescope. The regression failed before the change.
  Application now exposes the callee classifier through the existing typed
  normalization job, prepares its argument, post-checks, applies and closes its
  sequencing frames. After FORCE or a returned callee, classifier exposure is
  requested again for the new typed input; computation arguments use the same
  mechanism. No expected type guides either source operand's synthesis, no
  source computation executes to expose its classifier, and no new proof rule
  or Core tag is introduced. Tests cover raw/quoted/returned/nested callees,
  acted RETURN arguments, selected result witnesses and cache reuse. Existing
  callee-before-argument FOLD order remains unchanged; its proof now explicitly
  projects the forced callee into the argument context. Final witness comparison
  uses NF since RETURN extraction does not normalize its payload. Public
  prelude/imports, effects, full HOTT and N2 acceptance remain unfinished.
  Optimized check: 3.355 s; ASan/UBSan: 9.461 s (synthesis rebuilt).
  Core/Identity/synthesis/IADT pass with a 512 KiB stack.
  Implementation `synthesis.c` +85/-48 (net +37); tests `synthesis.c` +45/-1;
  documentation separate. These timings are not a whole-compiler speed claim.
- [x] September 8, after `7541d2b`: computation blocks now use the same
  classifier-exposure job as source application before sequencing. A checked
  action with classifier `Id (F Universe0) (RETURN A) (RETURN A)` previously
  failed as a non-final statement although its classifier reduces to `F (...)`.
  The new regression failed on the rebuilt original implementation, then passed
  with shared exposure. Tests cover named/unnamed and nested statements,
  selected prefixes, preserved quotation, and rejection of raw Pi sequencing.
  They also check that synthesis leaves the source reduction pending and that
  the accepted FOLD keeps the original source term. No implicit-force policy,
  proof rule, Core tag or Replay path changes. Optimized check: 3.241 s;
  ASan/UBSan: 9.648 s (synthesis rebuilt); synthesis passes at 512 KiB stack.
  Implementation `synthesis.c` +2/-0; tests `synthesis.c` +26/-0; docs separate.
  N2/N4 remain open; this does not implement effects or full block compatibility.
- [x] September 8, after `99bf232`: extract the test-only Identity function
  builders into `pg_identity_library(typing, classifiers, level)`. The six
  closed exports are ordinary checked Lambda terms: equality, reflexivity,
  right/left transport and right/left lifting. At value universe `U_i`, their
  conceptual types include `Eq : (A:U_i) -> A -> A -> F U_i`,
  `refl : (A:U_i) -> (x:A) -> F (Id A x x)`, and
  `trr : (A B:U_i) -> Id U_i A B -> A -> F B`; lifting retains the selected
  path. These compose existing primitive proofs and telescope abstraction,
  without special source-name rules or an unchecked witness constructor.
  The caller publishes exports with `pg_synthesis_name` and chooses names;
  this does not fix an intrinsic namespace or introduce reserved syntax.
  Retaining the library shares its proofs. Separate builds use fresh lexical
  binders, remain alpha-equivalent and are not conversion-interned together.
  Construction is synchronous finite library assembly, not source solving.
  Tests now consume these exports, compare transport fields against independent
  checked constructions, cover levels 0-2 and invalid graph/level inputs, and
  execute a source transport using a universe-level reflexivity function.
  `refl1 (@) A` requires parentheses: bare `@ A` parses as a graph companion,
  which caused the first new fixture's failure, not a classifier defect.
  Lower-level `refl (@) A` is correctly rejected. Existing higher reflexivity,
  chosen-path separation and negative post-check fixtures remain enabled.
  Optimized check: 7.851 s with affected binaries rebuilt, final added tests
  3.097 s with synthesis rebuilt; ASan/UBSan: 17.748 s. All four computation
  suites pass at 512 KiB stack. Implementation `prelude.c` +53, `prelude.h` +20,
  Makefile +1/-1; tests `synthesis.c` +70/-24; documentation separate.
  Public CLI/import installation, general Act syntax, Pi transport, arbitrary
  higher coherence and N2 acceptance remain open. No Replay engine is added.
- [x] September 8, after `cb0e2df`: add the library's selected-family instance
  function `(A B:U_i) -> (R:Id U_i A B) -> (x:A) -> (y:B) -> F U_i`.
  Its returned type is exactly `R x y`, assembled with ordinary accepted
  Identity instantiation, RETURN and telescope abstraction. It does not infer
  a relation from the endpoints or add a new application/coercion rule.
  Publishing it as the ordinary name `instance` lets source code post-check
  `liftr A B r x :: instance A B r x (trr A B r x)` and its left counterpart.
  Source Lambda domains also retain this selected family. Tests compare that
  domain with independently constructed primitive evidence and reject another
  path `s` with the same endpoints, reversed endpoint types and an arbitrary
  quoted relation offered as a universe-Identity witness. Library checks cover
  this seventh export at levels 0-2, including fresh-binder/non-interning checks.
  One composed annotation needed 1,232 scheduler transitions, exceeding the
  test helper's old 1,000-transition guard; continuing the same job completed
  after 233 more transitions. The test-only guard is now 10,000. No solver fuel,
  acceptance condition or expected-type synthesis rule was changed.
  Final optimized check: 3.238 s; ASan/UBSan: 8.997 s (synthesis rebuilt);
  synthesis passes at 512 KiB stack. Implementation `prelude.c` +10,
  `prelude.h` +3; tests `synthesis.c` +25/-4; documentation separate.
  Direct surface `R x y`, automatic library installation, CLI/imports, general
  Pi transport and higher coherence remain open. This is not N2 completion.
- [x] September 8, after `41522bc`: expose homogeneous symmetry and path
  composition as ordinary library functions. Symmetry transports `refl x`
  along `p:x=y` in `t |-> Id A t x`; composition transports `p:x=y` along
  `q:y=z` in `t |-> Id A x t`. One shared recipe constructs their checked
  contexts, selected substitutions, Act, transport and Lambda abstractions.
  Act's classifier is converted to its universe Identity using a certificate
  from the caller's shared pure normalizer. `pg_identity_library` therefore
  now takes that work store explicitly; no local evaluator, solver or new
  equality rule is introduced. Only this fixed library assembly is synchronous;
  it does not execute arbitrary source terms or alter source fuel accounting.
  Source tests call the exports under ordinary names `sym` and `trans`, check
  their dependent result types with `::`, normalize them against the derived
  library terms, and verify their refl computations. Incorrect orientation and
  noncomposable paths reject. Existing independent `family_transport` tests
  remain in place. All nine exports are checked at levels 0-2; graph ownership
  and normalization-store ownership must agree. Registering names itself still
  performs no solving or evaluation.
  Final optimized check: 8.338 s; ASan/UBSan: 18.547 s with affected binaries
  rebuilt (builds overlapped, not performance-comparison measurements). All
  four computation suites pass at 512 KiB stack. Implementation `prelude.c`
  +57/-2, `prelude.h` +7/-2; tests `synthesis.c` +50/-15; docs separate.
  Associativity, inverse laws, higher coherence, general Pi transport and N2
  acceptance remain open; these functions do not establish those laws.
- [x] September 8, after `eb183ec`: expose computation congruence as the
  library's tenth ordinary function (`congruence`, named `ap` in source tests).
  For `f:U(Pi A (F B))`, `x,y:A` and `p:Id A x y`, it produces computation
  Identity between `APP(FORCE f,x)` and `APP(FORCE f,y)`. Its construction uses
  reflexivity of `FORCE f`, the existing checked Pi Identity expansion, explicit
  conversion and ordinary telescope abstraction. The result is not wrapped in
  an invented returned-value equality. Fixed library conversions now share one
  helper and the caller's pure work store; no new proof rule, Core tag, source
  name rule or DefEq equation is added.
  Source tests show the action of an identity function normalizes to the chosen
  input witness and a constant function to reflexivity, with their result types
  checked by `::`. For an unknown function, they independently reconstruct and
  compare the computation-side result classifier without extracting returned
  endpoints. A path with wrong endpoints rejects. All ten exports are checked
  at universe levels 0-2. Final optimized check: 3.406 s (synthesis rebuilt);
  ASan/UBSan: 18.105 s (affected binaries rebuilt); all four computation suites
  pass with a 512 KiB stack. Implementation `prelude.c` +40/-11, `prelude.h` +3;
  tests `synthesis.c` +38/-4; docs separate. This export covers the current pure,
  constant `F B` result family, not general dependent `apd`, arbitrary effectful
  congruence or higher coherence. General source Act and N2 acceptance stay open.
- [x] September 8, after `89afca1`: resolve qualified source names through the
  existing lexical scopes and accepted producers. `pg_synthesis_namespace`
  publishes a closed scope, including nested namespace aliases, without a Core
  namespace node, classifier lookup by erased Core, evaluation or new evidence.
  All names in the supplied scope are public; callers build explicit exports
  from a fresh root. Member lookup stays inside that scope and never falls back
  to the importing environment. Root shadowing is lexical, including local
  definitions and binders. Ordinary terms cannot serve as namespace scopes;
  nominal type-member resolution remains unsupported, not guessed from a name.
  Source `#.refl A x :: #.Eq A x x` uses the existing checked library functions
  and post-synthesis checking. Qualified aliases use the ordinary producer and
  projection path. Namespace lookup is iterative with temporary O(path depth)
  storage, freed after resolution; it does not recursively traverse C frames.
  Tests cover closed-export ownership, missing members, no ambient-name leak,
  shadowing, nested aliases, shared Core with different classifiers, and a
  4,096-component path. Initial tests exposed punctuation tokens without text;
  `#` now resolves by token kind and does not enter identifier-only hash lookup.
  Optimized check: 3.438 s (synthesis rebuilt); ASan/UBSan: 17.720 s (affected
  binaries rebuilt); all four computation suites pass at 512 KiB stack.
  Implementation `synthesis.c` +84/-24, `synthesis.h` +9; tests +87; docs separate.
  File imports, automatic CLI library installation, `.a` loading, nominal member
  synthesis, general Act and N2 acceptance remain open. No Replay engine added.
- [x] September 8, after `fedba41`: share context-suffix abstraction between
  ADT branch functions and named checked functions (`pg_prove_abstract`). It
  composes the existing Pi/Lambda rules, retains every premise and binder,
  preserves a zero-length suffix, and inserts no RETURN or expected type.
  This is a synchronous derived operation, not a new kernel rule or bounded
  traversal claim. Source tests call the four universe transport/lifting fields
  as ordinary checked functions, compare terms and classifiers with independent
  primitive derivations, distinguish selected paths with identical endpoints,
  normalize diagonal transport and reject reversed arguments. RETURN extraction
  only exposes its payload; the diagonal final-value test requires NF, not WHNF.
  Full/staged abstraction agrees; repeated inputs allocate no new terms/proofs;
  wrong contexts, unrelated prefixes and value bodies reject. No structural Pi
  transport equation, new primitive, prelude/CLI or full N2/N3 acceptance is added.
  Optimized check: 7.692 s; ASan/UBSan: 17.442 s, with affected binaries rebuilt.
  Core/Identity/synthesis/IADT also pass with a 512 KiB stack.
  Implementation `evidence.c` +20/-0, `evidence.h` +7/-0, `iadt.c` +1/-11
  (net +17); tests `synthesis.c` +98/-6; documentation separate.
- [x] September 8, after `a8ba9a1`: connect accepted typed terms to source name
  resolution with `pg_synthesis_name`. Immutable lexical scopes point to the
  existing accepted-evidence producer, not a Core-only classifier lookup.
  Publication checks ownership/scope via ordinary projection, adds no context
  binder or coercion, and runs no computation. Aliases and shadowing preserve
  previous scopes. Tests supply ordinary checked Eq/refl functions and parse,
  synthesize and normalize their use in dependent annotations and `::`, including
  reflexivity of an Identity inhabitant. A source reflexivity function normalizes
  to its independently constructed checked implementation; unequal endpoints
  reject during post-check. The test's initially nested definition-block example
  was corrected to the existing root-only syntax, not enabled by parser changes.
  Names are supplied by the test library: no reserved Eq/refl, built-in prelude,
  general surface Act, import loader or CLI is claimed. This advances N2 without
  bypassing the unresolved Pi transport or nominal fibrancy requirements.
  Optimized check: 3.311 s (synthesis rebuilt); ASan/UBSan: 17.206 s (affected
  binaries rebuilt). Synthesis/Identity/IADT also pass with a 512 KiB stack.
  Implementation `synthesis.c` +25/-1, `synthesis.h` +9/-0 (net +33);
  test `tests/synthesis.c` +90/-0; documentation separate.
- [x] September 8, after `ce0a36d`: expose typed NF through the existing solver
  (`pg_synthesis_nf`) without another proof rule or Replay pipeline. WHNF and
  NF issue the same graph-owned `pg_reduction_certificate`, consumed by
  `PG_PURE_NORMALIZATION` with its exact source evidence and fixed pure policy.
  The receipt is the NF result authority; no separate result pointer is stored.
  NF requests remain distinct from WHNF requests, use the same reduction store,
  and do no work until advanced. Evidence remains specific to the typed
  occurrence/context even when the untyped reduction is shared. Tests cover
  suspended and Lambda-body redexes, split/bulk jobs, reuse, policy/source/owner
  rejection and receipt lifetime. This adds no function eta, runtime execution,
  new term tag or serialized trust bit. CLI/.a and general HOTT remain open.
  Final optimized check: 3.049 s; ASan/UBSan: 9.439 s, both with synthesis
  rebuilt. Core/Identity/synthesis/IADT also pass with a 512 KiB stack.
  The 158-file syntax gate remains parser coverage, not semantic parity.

  | File under `src/prototype/pointer/` | Added | Removed | Net |
  | --- | ---: | ---: | ---: |
  | `eval.c` | 28 | 16 | +12 |
  | `eval.h` | 9 | 6 | +3 |
  | `evidence.c` | 5 | 5 | 0 |
  | `evidence.h` | 3 | 3 | 0 |
  | `synthesis.c` | 27 | 17 | +10 |
  | `synthesis.h` | 5 | 0 | +5 |
  | Implementation total | 77 | 47 | +30 |
  | `tests/core.c` | 7 | 0 | +7 |
  | `tests/iadt.c` | 1 | 1 | 0 |
  | `tests/synthesis.c` | 61 | 4 | +57 |

  Documentation is counted separately from implementation and test code.
- [x] September 8, after `c03fa1c`: audit the U/Pi transport candidate before
  admitting it as a pure rewrite. `tests/identity.c:pi_transport_candidate`
  constructs the map with ordinary checked Pi/Lambda, APP, FOLD, transport
  and lifting. It handles both directions, two selected universe paths,
  constant result family `A` and dependent result family `Id A x x`.
  Domain arguments move in the opposite direction; the corresponding lifting
  path selects the codomain identification. That path must first be converted
  explicitly to the acted domain family, not accepted by weakening the checker.
  The candidate has the expected destination type. This does NOT prove it equal
  to the existing transport term. The experimental U/Pi dispatcher passed the
  non-diagonal checks but failed the diagonal comparison; it was removed.
  The retained test separates candidate typing from that rejected equation.

  For `R : Id Universe A0 A1`, let `Q(R)` be the action of `U(Pi x:A. F A)`.
  The proposed equation was:

  ```text
  trr Q(R) u = THUNK(lambda y.
      FOLD(APP(FORCE(u), trl R y), lambda z. RETURN(trr R z)))
  ```

  Substituting `R := refl A` in its right side yields
  `THUNK(lambda y. APP(FORCE(u), y))`, by existing scalar diagonal transport
  and FOLD right unit. But acting on the diagonal first, followed by existing
  strict diagonal transport, yields `u`. Equating these requires function eta,
  intentionally absent from current DefEq (`tests/synthesis.c:function_eta`).
  Unlike the previous U/F case, strong normalization alone cannot repair this
  while preserving that exclusion. This is a conflict in the proposed extension,
  not a demonstrated inconsistency of the currently admitted neutral Pi fields.
  The primary [Narya field contract](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting)
  informs the candidate's directions; the CBPV adaptation and this derivation
  are ours. Its documentation does not validate our strict regularity choice.
  Optimized pointer `make check`: 2.848 s; ASan/UBSan: 8.461 s (only Identity
  rebuilt on those final runs); Identity/synthesis/IADT pass at 512 KiB.
  A test-only omitted split advance was corrected before the final runs.
  Production implementation change: zero; tests `identity.c` +120; docs separate.
- [x] September 8: test the elimination-driven alternative in
  `tests/identity.c:pi_transport_candidate`. For a diagonal path, applying
  FORCE of the candidate and FORCE of the existing transport to the same
  typed target argument gives convertible classifiers and terms, in both
  directions, for constant and dependent codomains. Bare function comparison
  still rejects the eta equation above. These are tests of existing rules,
  not an implementation of non-diagonal Pi transport.
- [ ] Investigate elimination-driven transport before changing DefEq: keep
  non-diagonal U/Pi transport neutral as a value, and compute its action only
  when FORCE is supplied a function argument. The preceding test discharges
  the diagonal application example, not general substitution stability.
  Check non-diagonal subject reduction, higher lifting, normalization before
  versus after substitution, and effect evaluation order before admitting
  the rule. Do not install an eager eta expansion as a shortcut.
- [x] September 8, after `a4ca3f6`: implement the application equation for
  acted `U(Pi x:A. F B)` families. FORCE delegates a demanded transport answer
  to the Identity owner only when a further application argument is present.
  It transports the argument in the reverse direction, uses that lifting
  witness to select the codomain path, calls the original function, and maps
  the result with the ordinary FOLD and forward transport. Family closure
  reconstruction is shared with U/F fields. No Core tag, classifier lookup,
  function eta, eager thunk expansion or specialized conversion exception is
  introduced. Bare FORCE of a non-diagonal transported function stays neutral.
  `pi_transport_candidate` independently constructs checked recipe evidence
  for two selected paths, both directions and constant/dependent codomains;
  application now converts to that recipe. Tests also cover precomputed FORCE,
  one-step versus bulk evaluation, normalization evidence, beta-only policy
  isolation, and reduction before versus after diagonal substitution.
  A runtime-only probe confirms zero body calls during transport and one per
  application, including repeated execution outside the pure memo store.
  This admits the tested application fragment, not a general Pi lifting rule
  or arbitrary higher coherence. The N2 equational/coherence gate and nominal
  datatype fibrancy gate remain open.
  Optimized and ASan/UBSan pointer `make check` pass; Identity also passes
  with a 512 KiB stack. Implementation: +69/-11 lines; tests: +51/-1;
  documentation is separate. Syntax inventory success is not source semantic
  acceptance or completion of N0-N7.
- [x] September 8, after `fc3a5f8`: remove the returning-codomain special case
  from Pi application transport. Given the reverse-transported input and its
  lifting witness, act on `U(C(x))`, transport `THUNK(APP(FORCE(f), input))`
  along that selected path, and FORCE the result. This reuses U/F transport
  for a returning codomain and the same Pi application rule for a further Pi;
  unknown codomains retain neutral fields rather than requiring a new tag or
  solver. The dispatcher no longer constructs a special result FOLD or checks
  for `F B`. Existing dependent-return, diagonal substitution and runtime
  call-count tests remain applicable. `curried_transport` constructs checked
  two-argument functions over two selected paths and the diagonal, in both
  directions; it checks the direct recipe and pre-normalized partial
  application. General higher lifting/coherence and datatype admission remain
  open. Implementation `identity.c`: +9/-8; `identity.h`: +1/-1;
  tests: +63/-0; documentation separate. Optimized and ASan/UBSan pointer
  checks pass; Identity also passes with a 512 KiB stack.
- [x] September 8, after `a4f79d0`: extend `curried_transport` to two
  independently varied universe parameters, `U(A -> A -> F B)`. Exercise
  same-path, distinct-path and diagonal cases in both directions. The result
  path must first carry an explicit conversion to the acted universe family
  over the preceding parameter; omitting that premise is correctly rejected
  by `pg_prove_family_action`, not a reason to relax endpoint checking.
  The resulting application agrees with the independently checked recipe.
  Substituting the domain path for a distinct result path is not DefEq, even
  though their endpoint types coincide. No implementation rule was changed.
  Higher coherence and general datatype fibrancy remain unproved.
- [x] September 8, after `46b59d2`: move boundary-path post-checking into
  the existing family-action synthesis job. The input synthesizes first;
  then each supplied path is checked against its prefix-dependent family
  with the ordinary conversion machine. Accepted prefix substitutions grow
  by the existing pairing rule. The final action receives explicit converted
  path evidence; the kernel rule, Core and pure conversion policy are unchanged.
  Pending conversion uses ordinary scheduler transitions, not another solver.
  `selected_instances` now compares automatic and manual rebasing, inspects
  the retained conversion premise, tests split/bulk completion, and confirms
  that the raw kernel call still rejects an unconverted boundary. Reversed
  endpoints are rejected rather than reported as unsupported. All originally
  selected path proofs remain part of the immutable request key.
  This is not surface Act completion or higher-coherence certification.
  Implementation: `synthesis.c` +78/-8, `synthesis.h` +3/-1;
  tests: +15/-5; documentation separate. Optimized and ASan/UBSan pointer
  checks pass; synthesis also passes with a 512 KiB stack.
- [x] September 8, after `fec03b8`: family action accepts pending boundary
  producers through `pg_synthesis_family_action_jobs`. The evidence-input API
  only wraps paths with existing evidence jobs and delegates to the same
  request, preserving exact job sharing. Each path uses the ordinary dependency
  subscription before post-checking; failure propagates and a cycle waits
  without polling. Tests cover shared requests through both APIs, pending
  normalization, cyclic/non-term/failed paths, foreign producers, and the same
  accepted judgement as explicit converted evidence. This enables scheduling
  source-produced paths but does not add a surface Act notation or new rules.
  Implementation: `synthesis.c` +26/-4, `synthesis.h` +6/-0;
  tests: +19/-0; documentation separate. Optimized and ASan/UBSan pointer
  checks pass; synthesis also passes with a 512 KiB stack.
- [ ] Resolve this N2 equational choice before enabling structural U/Pi
  transport or using it to justify nominal datatype fibrancy. If the
  elimination-driven alternative is insufficient, either admit
  computation-Pi eta into fixed pure DefEq, revisiting the current eta example
  and runtime/Act compatibility, or retain the current DefEq and revise the
  unrestricted strict diagonal transport contract (with its lifting/regularity
  proofs and existing tests). Do not hide the choice behind a path-shape branch
  or a Pi-transport-only conversion exception. Every alternative leaves pointer
  interning structural: conversion must never merge Core nodes. The user has
  been asked which direction to pursue; no choice has been silently adopted.
- [ ] Universe action needs an inhabitant contract containing transport and
  lifting plus their higher action, not only an arbitrary binary relation or
  four unrelated functions. Validate this before introducing a general
  universe-Identity witness constructor. Preserve distinct choices even when
  endpoint types coincide.

#### Next N2 gate: typed higher fields, not more scalar transport cases

September 8 audit after `918e1cd`:

- `identity.c` has four scalar field owners. `action_source` deliberately keeps
  an acted field application neutral. Adding Pi application transport has not
  supplied a reduction for acting on transport/lifting themselves.
- `dimension.h:pg_term_restrict_bindings` explicitly specifies syntactic
  restriction only. `action.h:pg_context_restrict` checks a context substitution;
  neither contract alone supplies a transposed higher Identity classifier or
  uniform higher transport. Do not infer either from equal face pointers.
- The pinned [Narya Pi fibrancy construction](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/test/black/hct-hott.t/fibrant_types.ny#L118)
  uses Identity fibrancy and direction symmetry in its lifting clauses, beyond
  the scalar argument/result transport recipe. Its parametrical encoding is a
  reference derivation, not a proof of our CBPV equations.
- [Narya's field contract](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting)
  distinguishes dimension-selected uniform fields from scalar fields on an
  instantiated higher type. These must not be conflated during lowering.

Next implementation order, retaining the existing three Core forms:

1. Construct a checked square boundary with selected edge proofs and a center
   assumption. Specify the exact input/output classifiers for transport in
   either direction, including transported corners. A square assumption is
   not a generated filler.
2. Derive the corresponding direction permutation on typed boundaries, not
   merely binder pointers. Preserve which path was selected on each edge.
3. Give iterated field action a dimension-uniform typing/elimination contract;
   reuse APP/Reference and checked context substitutions. Do not allocate a
   new Core tag per dimension or insert arbitrary relation-to-Identity casts.
4. Adapt the Pi lifting derivation through U/ FORCE on computation results;
   verify its endpoints against the implemented application transport, then
   check faces, substitutions and diagonal computation before enabling it.

The general four-item contract remains unchecked. Passing first-order transport
tests does not discharge it, and datatype admission must not assume completion.

##### Typed symmetry contract, rechecked after `2567a26`

Primary references rechecked on September 8:
[Narya symmetry and degeneracy](https://narya.readthedocs.io/en/latest/observational.html#symmetries-and-degeneracies)
and [uniform transport fields](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting).
Narya symmetry transforms both a higher term and its type; higher boundary
faces may themselves require symmetry. Its uniform fields select a dimension,
and are distinct from scalar fields of an instantiated higher type. These are
reference requirements, not a derivation of equations for our CBPV kernel.

Current code evidence:

- `pg_binding_permute` transforms geometric references only.
- `pg_identity_cube_context` constructs either orientation, with a separate
  assumed center; it does not construct a map between the centers.
- `square_transposition_boundary` pairs the eight proper faces through ordinary
  checked substitution but rejects pairing the untransformed center.
- `uniform_transport` now checks iterated actions on transport/lift in both
  orientations. Neither these derivations nor their WHNF receipts connect
  the two oriented centers.

Design consequence for A Program (our inference): a typed symmetry action must
be an explicit term operation, not pointer renaming, classifier conversion, or
a supplied target classifier blessed by a new acceptance rule. Keep the three
Core forms. If a semantic reference implements this operation, its canonical
payload is the dimension map; its application spine must retain the source
term and the family/boundary arguments needed by its computation rules.
Do not add a tag or a proof rule for each dimension.

Before accepting the first such derivation:

1. Recover the source family, selected faces and orientation from checked
   evidence. Derive the target classifier, rather than trusting a caller's
   arbitrary claimed target. Operation-level evidence remains authoritative.
2. For every proper face, derive the induced permutation and its typed action.
   Moving a face to another position is not enough when its own orientation
   changes. Existing dimension composition supplies geometry, not this proof.
3. Specify the primitive center action and its typing rule together. Ordinary
   Act preserves families; the existing APIs do not already derive this rule.
   Retain the negative raw-center substitution regression.
4. Check identity/composition/inverse laws on terms AND classifiers, followed
   by restriction and substitution laws. Test a 3-cycle and adjacent-swap
   composition in addition to the square involution.
5. Only then connect axis-selected uniform fields and Pi lifting. Well-typed
   neutral field terms are not evidence of their computation equations.

This audit changes the next implementation target from more scalar cases to
the typed symmetry operation and its boundary contract. It does not authorize
an arbitrary relation-to-Identity cast, identify orientations by Core interning,
or claim a completed HOTT model. Items 1-5 above remain unimplemented gates.

September 8 follow-up against `f2dfb22`: the next typed rule must also preserve
dependency order across multiple source declarations. `cube_action` builds
each declaration from the already transformed preceding declarations; a map
between two resulting telescopes must do the same. For each target declaration,
first reindex its formation by the partial checked substitution, then construct
the image of the corresponding source face. Do not pair faces by list position
or infer a classifier from the erased permutation term. Proper faces of positive
dimension may themselves require symmetry, so checking only corner references
does not establish that the partial substitution extends to the center.

`PG_FAMILY_IDENTITY_FORM` already retains the source formation, both endpoint
substitutions, selected paths and endpoints in its immutable premises.
`PG_FAMILY_ACTION` retains that formation as premise zero. Reindex and projection
preserve provenance through premises rather than erasing it. Recover and compose
these premises when implementing the boundary contract; do not add a competing
mutable boundary registry. A geometric `pg_binding_face` alone is insufficient.
An unknown center requires an explicit primitive typed symmetry rule, not an
application of ordinary family action to an arbitrarily supplied target type.
This is a remaining implementation requirement, not an admitted axiom here.

Reference rechecked September 8:
[Narya higher-dimensional cubes and symmetries](https://narya.readthedocs.io/en/latest/observational.html#symmetries-and-degeneracies).
Narya transforms the synthesized type together with the term. Its cube boundary
conventions motivate this audit, but do not prove the CBPV rules above for
A Program; preserving value/computation polarity remains our proof obligation.

After `26fbb3c`, item 2's geometry is checked without introducing a redundant
permutation-plan API. For an ordered face `f`, factor `p o f = h o u`, where
`h` selects the moved face and `u` acts inside it. If `q o h = j o v`, direct
factorization of `(q o p) o f` must yield `j` and `v o u`. The Core regression
checks this law for all 27 ordered faces of a 3-cube and all 36 pairs of its
six permutations. Existing composition and face-factor interners suffice.
This also covers the center and vertices, but remains map algebra: the typed
action of `u` on a selected face proof is still required, not inferred from
pointer equality. No new implementation API or accepted proof rule is added.
Optimized and ASan/UBSan full pointer checks and the 512 KiB Core test pass.

- [x] After `da6f9f1`, add the formal permutation computation operator in
  `symmetry.c`. It is an ordinary Reference/Application spine, with no new
  Core form. Construction retains identity and composition explicitly; the
  pure evaluator contracts identity and composes adjacent equal-dimensional
  permutations. The composed operator retains the argument closure environment.
  This is the free permutation-action fragment, NOT a typed symmetry rule:
  no classifier, center witness, surface syntax or arbitrary cast is admitted.
  Its interaction with Act, restriction and typed boundaries remains open.
  A lazy graph-owned structural semantic-owner index interns the immutable
  permutation payload by its axis sequence. It has graph lifetime, so neither
  the operator nor its evaluator depends on a destroyed Dimensions registry.
  Core does not interpret the payload. No WHNF/alpha interning is introduced.
  Tests cover all 36 compositions in dimension three, exact repeated structure,
  non-collapsing construction, invalid maps, capture and readback after registry
  destruction. Composition currently scans the finite permutation synchronously;
  dimensional fuel accounting is still required before this is a bounded-work
  operator. This checkpoint does not close N2 or the typed symmetry gate.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Core test pass.

- [x] After `b3ce150`, permutation elimination demands its argument through
  the existing evaluator frame before inspecting a nonidentity composition.
  Syntactic-only recognition missed inverse permutations hidden behind beta
  redexes. Identity still enters its argument directly. Callback input is
  materialized by the ordinary demand mechanism, preserving captured values.
  Tests cover both direct and beta-hidden inverse composition after registry
  destruction and readback/restart budget cuts 0-31. Retaining the evaluator
  and splitting at every transition also preserves the exact total step count
  and result; this uses the same demand frames, not a Replay implementation.
  An argument that diverges remains Pending
  at finite fuel; it is not classified as a completed neutral permutation.
  This is a refinement of formal computation, not a typed symmetry theorem.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Core test pass.
  The syntax inventory remains parsing evidence only; N2 and N5 stay open.

- [x] After `f2dfb22`, permutation coordinate composition uses the existing
  deferred evaluator task: one axis per traversal transition, with caller
  configuration retained until completion. There is no second scheduler or
  serialized continuation format. A 128-axis reversal composed with itself
  exercises all retained-machine budget splits and readback/restart cuts 0-159,
  including task destruction while pending. The small capture and beta-hidden
  fixtures use the same checks. Allocation and structural operator interning
  remain synchronous; this is not a wall-clock bound or complete dimensional
  resource accounting. The typed symmetry gates above remain open.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Core test pass.
  Implementation C/header delta: +41/-7; tests: +11/-5 (documentation excluded).

- [x] After `0ce4bfb`, expose `pg_binding_face_view` for recovering embedded
  cube/face geometry from a context's binder pointer. Boundary binders retain
  `PG_BINDER`; the existing owner pointer identifies their containing object.
  No new Core kind, name lookup, reverse index or copied classifier is added.
  Geometry remains graph-owned after the Dimensions indices are destroyed.
  Ordinary binders, NULL and non-binder references are rejected by the view.
  Binding a face still obeys ordinary Lambda alpha comparison without merging
  distinct pointer structures. Both oriented square contexts recover all nine
  faces through accepted variable/regularity evidence; the negative raw-center
  substitution test remains unchanged. This provides input recovery for the
  typed boundary transformation, not permission to transpose a center proof.
  Optimized and ASan/UBSan full pointer checks and 512 KiB Core/Identity tests
  pass. Implementation C/header delta: +15/-2; tests: +22/-0; docs excluded.

- [x] After `8c9d167`, add inverse permutation to the existing dimension-map
  algebra. It validates a strict endomap and interns the reversed coordinates;
  no extra map representation or proof rule is introduced. The square boundary
  regression now recovers each source face `f` from its binder and factors
  `inverse(p) o f = h o u` in destination orientation `p`. Its destination
  binder is `p o h`, so `(p o h) o u = f`. For corners/edges, `u` is identity
  and the usual checked substitution accepts the images. At the center,
  `u` is the swap and the unchanged center is still rejected. Core checks both
  inverse laws and double inversion for all six 3D permutations, zero dimension,
  and rejection of a projection, duplicate axes and NULL.
  Optimized and ASan/UBSan full pointer checks pass. Implementation C/header:
  +20/-0; tests: +22/-1. Typed center symmetry and full N2 remain incomplete.

The remaining typed rule must produce `S_u(value)` at the source declaration's
formation reindexed by the already checked partial substitution. In particular,
when `u` is identity, that formation must be convertible to the value's own
classifier: `S_identity(value)` reduces to `value`. A primitive admission that
simply attaches the requested formation could violate subject reduction.
For dependent later cubes, the partial substitution includes earlier center
transformations; equations for symmetry of family instantiation must agree with
those replacements. Identity/composition of the raw operators alone does not
establish this condition. Keep this as a prerequisite for the typed rule, not
an accepted conversion axiom or a reason to discard the dependent case.

### Typed symmetry: pinned implementation audit after `7da9160`

The comparison source is Narya revision
`c7c92b4ec01ae2f528b97207256549242bd21334`, inspected locally on September 8.
This exposes a more specific missing contract than the map algebra above:

- [check.ml, synthesized Act](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/check.ml#L3555)
  synthesizes the operand and computes the result classifier with `act_ty`.
- [act.ml, act_normal and gact_ty](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/act.ml#L408)
  distinguishes action on a term from computation of the classifier of that
  acted term. These are not interchangeable calls on the same input type.
- [act.ml, gact_ty_instargs](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/core/act.ml#L485)
  factors the dimension action against proper faces, transforms their normals,
  and excludes the center from the boundary traversal. Pure permutations do
  not require the center term to construct that transformed boundary. General
  degeneracies can require it, as reflexivity's classifier contains its operand.

The distinction is between *uninstantiated* dimensions of a family-as-term and
*instantiated* dimensions in a term's classifier. It does not justify ValuePi,
another Core graph, or a second equality authority. Narya's dimensional values
already carry instantiation structure. Our ordinary APP spines do not identify
that structure by themselves. For example, a neutral `R x y` cannot tell Core
whether it is an ordinary application or a selected universe identification.
The accepted formation/derivation supplies this interpretation.

Consequently, the next typed-symmetry implementation must follow this sequence:

1. Recover a fully instantiated boundary from the accepted formation, following
   reindex/projection provenance and preserving the selected family. Do not
   infer its dimension merely by counting APP nodes or repeated Act heads.
2. Reuse inverse/composition/face factorization to select each destination
   proper face. Recursively transform its term AND its classifier at the
   induced intrinsic permutation. This recursion decreases face dimension;
   do not include the center and recurse on the original typing request.
3. Reconstruct the result formation from the transformed family and boundary
   with the original universe bound and CBPV polarity. The result classifier
   is not in general `pg_symmetry(p, old_classifier)`.
4. Admit the center action only with this reconstructed formation and the
   selected source formation as premises. The Core term must contain every
   operand required by its fixed computation rules; runtime must not consult
   the typing database to interpret an ambiguous application.
5. Validate reduction preservation for identity/inverse/composition, including
   dependent `A : Universe, x : A` cubes. Then connect the rule to canonical
   oriented cube contexts. Do not treat a free formal permutation action as
   already providing those instantiation equations.

September 8 provenance correction after `4bdca64`: formation recovery must also
follow `TYPE_FROM_VALUE` / `VALUE_FROM_TYPE`. A value-type formation can pass
through its universe-value presentation, including intervening checked reindex
and context projection, without losing its selected Identity boundary. The
implementation now follows these existing premises in the same iterative walk.
It still requires a formation as input and rejects an opaque universe variable
with no retained Identity formation; it does not infer Identity from APP shape.
The regression failed before the change and passes for homogeneous, selected
family-instance and square Family Identity, including substitution/projection.
This repairs step 1 provenance coverage, not steps 2-5 or typed center symmetry.
Implementation/header: +7/-2 lines; test C: +13/-0. Normal and ASan/UBSan pointer
checks pass, as does the Identity test with a 512 KiB stack limit.

This audit does not introduce a new primitive or conclude that a separate
instantiation node is necessary. If elaboration can construct the required
family/boundary action explicitly as Reference/Application spines, preserve
that representation. If an additional semantic operator is necessary, justify
its computation contract first; never recover erased typing information by
looking up a Core pointer's classifier. Steps 1-5 remain open, and scalar
transport tests or further permutation-group tests cannot close them.

September 8, after `ffcf1bb`: `pg_identity_face_endpoint` selects a
codimension-one endpoint from an explicit iterated Identity formation. Depth
zero returns its retained endpoint evidence. Deeper selection descends through
the source family formation, then transports that selection outward by the
existing Family Action rule (or reflexivity for homogeneous Identity). An
explicit temporary stack avoids recursive C calls. This is checked derivation
construction, not a new endpoint axiom, classifier cache or Core operator.

Tests enumerate every intrinsic endpoint of the generated 1-3 dimensional
`A : Universe, x : A, f : U(Pi A (F A))` boundary contexts, including all six
3D orientations. Both term reduction and classifier conversion are checked
against the independently selected geometric binder. Homogeneous nested
Identity, opaque selected-family rejection, wrong context, invalid side and
excess depth also have tests. A selected opaque universe identification still
supports its immediate endpoints, but not guessed internal directions.

This advances proper-face extraction in step 2. It does not provide induced
nonidentity action on those faces, reconstruct a permuted instantiated family,
or authorize the center action. Steps 2-5 and N2 remain incomplete. Construction
is synchronous; no bounded wall-clock or general higher-family coverage claim.
Implementation/header: +61/-0 lines; tests: +36/-0. Normal and ASan/UBSan
pointer checks pass; the expanded Identity test also passes with a 512 KiB stack.

September 8, after `0b2040b`: `pg_identity_proper_face` composes the endpoint
operation for ordered proper faces. It verifies the selected outer directions
against retained formations, fixes coordinates from the outside inward, and
uses ordinary regularity between successive endpoint selections. It stops after
the final fixed coordinate without recovering an unused classifier. Inherited
Identity directions below the selected suffix remain intact. The center,
nonidentity permutations, missing directions and opaque unexposed inner
formations are not silently accepted. No new evidence rule or Core node.

Tests select every proper face of full 1-3D cubes for the dependent type/value/
function context in each tested orientation. Results and classifiers are checked
against independent geometric restrictions. Additional tests retain an inherited
path type and reject excessive dimensions, centers and a permuted square face.
This supplies ordered boundary extraction, not the missing intrinsic symmetry
on a selected face or the transformed-family formation for the center. N2 stays
open. Implementation/header: +49/-0; test C: +33/-0. Normal and ASan/UBSan
pointer checks and the 512 KiB-stack Identity test pass.

September 8, after `174f02c`: typed face-composition regression gates now compare
both orders of fixing each pair of distinct axes, with all endpoint choices,
for full dependent type/value/function cubes in dimensions 2-3 and the tested
orientations. Each intermediate classifier comes from ordinary regularity;
the final comparison checks classifier conversion and term reduction, not just
the geometric binder identity. A second gate checks that endpoint selection
commutes with the checked square-boundary substitution, in both directions
and on both axes. Normal pointer checks pass without changing production rules.

This is concrete evidence that these boundary paths agree, not a proof of a
general coherence theorem. It narrows the next action: keep the ordinary face
selection rules and address the still-missing transformation of the instantiated
family under a nonidentity permutation. Do not add a center witness on the
strength of these tests. Test C: +31/-0; implementation C unchanged.
ASan/UBSan pointer checks and the 512 KiB-stack Identity test also pass.

September 8, after `3bdf30e`: the square transposition regression now constructs
the opposite center's *type* in a context containing only the original eight
proper faces. It takes the opposite cube telescope as a formation template,
selects each supplied image from the original formation using
`pg_identity_proper_face`, checks its classifier against the progressively
substituted declaration, and pairs it into an ordinary substitution. Finally,
`pg_prove_reindex` instantiates the template's center formation. Neither center
binder is in the resulting context. Polarity and universe bound are preserved.

The derived type is not definitionally equal to the original square type under
the current pure rules (the regression checks `PG_CONVERSION_DIFFERENT`). Thus
existing Context/substitution machinery can construct this target without a new
boundary registry, but cannot type the unchanged original center there. A typed
symmetry operation is still needed. This is a 2D formation-construction test,
not admission of a dimension-specific center rule. In higher dimension the same
template method additionally requires actions for induced proper-face
permutations; no such evidence is fabricated. Test C: +36/-0, implementation
unchanged. Normal and ASan/UBSan pointer checks pass, as does the 512 KiB-stack
Identity test.

- [x] After `a4bc683`, `pg_identity_formation` recovers an explicit Identity
  formation through a chain of accepted reindex/projection derivations. It
  composes their substitutions and rebuilds homogeneous Identity, selected
  universe-family instantiation, or Family Identity with the existing rules.
  For Family Identity the source formation stays fixed; both endpoint maps,
  selected paths and endpoints are substituted together. There is no new
  proof rule, raw APP-shape inference, copied boundary database or automatic
  conversion search. Repeated recovery reuses accepted evidence.
  Tests cover the square-center formation under reindex and later projection,
  retained source family, alpha agreement with ordinary reindex, homogeneous
  formation and selected instantiation under projection, and rejection of a
  value or plain universe formation. The raw-center rejection remains intact.
  This implements structural provenance recovery for these three formers, not
  a complete multidimensional boundary view. At this checkpoint normalization,
  conversion and other formation rules were unsupported (pure normalization
  is handled in the following checkpoint).
  Work is currently synchronous; budgeted proof traversal remains necessary.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test
  pass. Implementation C/header: +66/-0; tests: +25/-0; documentation excluded.

- [x] After `41c6952`, follow accepted pure-normalization premises during
  Identity-formation recovery. Such a premise keeps its source formation and
  fixed pure-reduction certificate; recovering the source does not require
  repeating evaluation or adding another evidence rule. The returned formation
  can have a different, convertible subject, not merely an alpha-renamed one.
  The API now makes that contract explicit: consumers retain the input proof
  and check conversion when using the recovered formation to type the input.
  General type conversion is still unsupported; do not discard a changed
  classifier or its universe bound by following it like pure normalization.
  Tests reuse the square's selected boundary for a computation Identity over
  F, checking that reindex/recovery retains computation polarity and universe
  bound and cannot be injected as a value type. Normalization before and after
  reindex recovers the same formation, while ordinary conversion relates the
  resulting subjects. This is provenance recovery, not typed center symmetry.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test
  pass. Implementation C/header: +10/-3; tests: +33/-0; documentation excluded.

- [x] After `34605ab`, exercise formation recovery throughout generated cube
  contexts for `A : Universe, x : A, f : U(Pi A (F A))`. Dimensions 0-3 and
  all six 3D orientations cover 525 declarations, including 360 positive-
  dimensional faces. For each such face, recover a Family Identity in the
  complete context, preserving its universe bound and alpha-equivalent subject.
  Independently restrict the face's last intrinsic axis to each endpoint and
  require the recovered endpoint proof to reference exactly that binder.
  The 165 vertices do not acquire an Identity dimension merely by belonging
  to a cube. This checks dependent type/value/function boundary provenance,
  not just agreement of erased classifier shapes. No implementation or
  acceptance rule is added, and no map between different centers is inferred.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test
  pass. Tests: +23/-0; implementation unchanged; documentation excluded.

- [x] After `f8c4a05`, centralize the three explicit Identity formations'
  premise layout in `pg_identity_boundary_view`. This is an allocation-free
  borrowed view of accepted immutable evidence, not another stored boundary
  record or authority. Recovery and cube endpoint tests use named components
  rather than calculating premise offsets. Family paths alias the accepted
  premise array; the computation-Identity regression no longer copies them.
  A homogeneous family's formation and a selected universe-identification
  value remain distinct kinds of premise. Only Family Identity supplies maps
  and paths, and their count must not be interpreted as an axis dimension.
  Other derivations return failure without replacing the caller's view.
  No typing rule, Core form, dimensional equation or solver is added.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test
  pass. Implementation C/header: +52/-14; tests: +22/-10; docs excluded.

- [x] After `c1b1244`, test a source declaration already typed by Identity:
  `A : Universe, x : A, p : Id A x x`. Replacing p by a zero-dimensional cube
  preserves its Identity formation, and adding one cube direction produces a
  Family Identity whose source family is itself an Identity. Thus the geometric
  dimension of a binding face cannot specify the full instantiated classifier
  dimension. The earlier vertex-negative regression concerned different source
  types; it is not a general rule that vertices cannot be paths.
  Add `pg_dimension_prefix` for identity(fixed) + map using the existing map
  interner. This supplies geometry for fixing earlier axes while acting on a
  suffix, not an inference of which axes the typed operation selects. Tests
  check unchanged prefix coordinates, zero extension, identity/inverse laws,
  composition with all 27 ordered 3D faces and six permutations, and invalid
  size/NULL rejection. Typed symmetry must obtain the selected axis layout
  from the recovered formation rather than guessing it from a binder's cube.
  Optimized and ASan/UBSan full pointer checks and 512 KiB Core/Identity tests
  pass. Implementation C/header: +25/-0; tests: +37/-0; docs excluded.

- [x] After `411ef62`, the uniform-field regression acts on both scalar
  transport and lifting over the dependent context `A, B, r : Id A B, x`.
  Four fresh cubes supply the assumed boundary data. One and two action
  dimensions, both transport directions and both two-dimensional axis orders
  produce checked value evidence with well-formed classifiers. Ordinary pure
  WHNF results also receive normalization evidence; repeated construction
  reuses the accepted derivation. Tests: +28/-0, implementation unchanged.
  This extends item 3's typing evidence beyond the original square example,
  but does not compare the two orientations or compute unknown higher fields.
  Typed transposition, uniform field equations and Pi lifting coherence remain
  open. Well-formed iterated action must not be reported as those theorems.
  Optimized and ASan/UBSan full pointer checks and the 512 KiB Identity test
  pass. No new proof rule or dimension-specific Core constructor is introduced.

- [x] September 8, after `01201c3`: `tests/identity.c:uniform_transport`
  constructs a selected universe square boundary by ordinary context action.
  Acting on a scalar transport term yields a value proof relating the two
  transported corners along the destination edge, in both transport directions.
  This uses existing family action, reindexing, classifier formation and
  explicit conversion; it adds no higher-field tag or proof rule. Swapping
  the destination endpoints is rejected. The square center remains a supplied
  assumption, not a derived filler. This verifies the initial square typing
  example in item 1 and shows that uniform transport's typing can arise from
  scalar term action. It does not establish direction transposition, a higher
  field computation rule, or Pi lifting. Unknown acted fields remain neutral.
  Optimized and ASan/UBSan pointer checks pass; Identity also passes with a
  512 KiB stack. Implementation unchanged; tests +56 lines; docs separate.
- [x] September 8, after `f6566c7`: add geometric cube-axis permutation via
  `pg_binding_permute`. For a face `f : k -> n`, restriction precomposes on
  its intrinsic k coordinates; permuting the owning n-cube instead forms
  `p o f`. Both use the existing map and binding interner. Only bijective
  dimension-preserving maps are accepted. Tests cover corners/edges, inverse
  swaps, a 3-cycle, identity, composition and commutation with restriction;
  dimension-dropping and constant-coordinate maps are rejected. This supplies
  geometric input to item 2, not a typed Identity transposition rule. No Core
  tag, proof rule or context algorithm was added. Implementation +16 lines;
  tests +19; documentation separate. Optimized and ASan/UBSan pointer checks
  pass; Core also passes with a 512 KiB stack.
- [x] September 8, after `0d04553`: localize the typed transposition gap with
  `square_transposition_boundary`. Construct both orientations of the same
  geometric universe square using ordinary context action. All four corners
  and four edges map by their geometric binders through checked substitution
  and conversion, despite their different declaration orders. The final
  center cannot be paired: its two oriented Identity classifiers are not
  current DefEq. This is an expected negative test, not evidence that square
  transposition is impossible. The next rule must transform the center proof
  with its boundary, not alpha-rename it or loosen substitution checking.
  Context/evidence algorithms remain unchanged; tests +56 lines; docs separate.
  Optimized and ASan/UBSan pointer checks pass; Identity passes at 512 KiB stack.
- [x] September 8, after `e169882`: share full cube-context construction in
  `pg_identity_cube_context`. It replaces the final declaration with a cube's
  complete boundary telescope by repeated ordinary context action, retaining
  the ambient prefix and using one supplied cube for all geometric binders.
  Tests cover dimensions 0-3, declaration classifiers, exact repeated-request
  sharing, dependent ambient prefixes and invalid inputs. The transposition
  boundary test uses this constructor. It introduces no proof rule or filler.
  Construction is synchronous and materializes 3^n declarations; a future
  solver request must suspend this work rather than charge it one fuel step.
  Implementation +51 lines; tests +25/-1; documentation separate. The typed
  center transposition rule and higher computation remain open.
  Optimized and ASan/UBSan pointer checks pass; Identity passes at 512 KiB stack.

- [x] September 8, after `d6445d4`: give cube-context construction an explicit
  axis permutation, using the existing binding permutation/composition path.
  Both square orientations now use the same repeated context-action algorithm;
  remove the manually assembled transposed telescope from the test. Repeated
  transposed construction shares terms and proofs. Null, wrong-dimensional and
  non-bijective orders are rejected. The corner/edge substitution succeeds and
  the center mismatch remains an expected negative test: constructing an
  oriented assumption context is not a rule transposing its center evidence.
  Optimized and ASan/UBSan pointer checks pass; Identity passes at 512 KiB stack.
  Source/header changes +7/-4; tests +21/-21; documentation counted separately.
  Typed center transposition and suspended cube construction remain pending.

- [x] September 8, after `8c9f198`: validate cube axis orders before allocating
  the exponential boundary workspace. Reuse `pg_dimension_face` and require
  matching source/target dimensions; no separate validator is introduced.
  Extend the dimension-0-through-3 context test to all six orders in dimension
  three, checking all 27 declaration classifiers, the exact center binder and
  repeated-request term/proof sharing. Reject a dimension-32 request with a
  mismatched order without creating boundary faces or evidence. This validates
  oriented assumption construction, not higher symmetry or coherence rules.
  Optimized and ASan/UBSan pointer checks pass; Identity passes at 512 KiB stack.
  Implementation +3/-0; tests +24/-11; documentation separate.

- [x] September 8, after `2929d12`: factor strict dimension faces into their
  position and intrinsic orientation (`pg_dimension_face_factor`). For
  f:k->n the outputs satisfy f = ordered o intrinsic, with intrinsic:k->k a
  permutation and ordered retaining endpoints while numbering encountered
  axes increasingly. Both outputs reuse the existing map interner. This does
  not canonicalize terms or add a typing rule. Tests factor every strict face
  among the existing 38 maps in dimensions 0-2, reject degeneracies without
  changing outputs, reconstruct exact maps and check ordered-factor
  idempotence. A 3-cycle moving a square face exposes a nontrivial local swap.
  Optimized/ASan/UBSan pointer checks and 512 KiB Core execution pass.
  Source/header +36/-0; tests +25/-0; documentation separate.

  Rationale: [Narya's symmetry contract](https://narya.readthedocs.io/en/latest/observational.html#symmetries-and-degeneracies)
  permutes dimensions and also transforms some higher boundary terms, not
  merely their positions. Its three-dimensional example includes symmetrized
  square faces; permutation composition obeys group equations. This is a
  reference requirement, not a proof of our CBPV extension.

  A Program implementation consequence: for ambient permutation p and face f,
  factor p o f. The ordered component identifies the destination face; the
  intrinsic component specifies the operation needed on that face's evidence.
  Do not replace this operation by binder substitution or classifier casts.
  Next, typed action must retain the selected family and all proper-face
  evidence, synthesize the permuted classifier, and use intrinsic actions on
  higher faces. Verify involution, composition and face compatibility before
  admitting a center transposition rule. Current factorization alone grants
  none of those proof-level equations and does not complete the N2 gate.

- [x] September 8, after `9d07e36`: connect typed term action to the same
  oriented cube-construction algorithm (`pg_identity_cube_action`). Initially
  reindex the source term to the zero vertex by a checked substitution, then
  use each stage's checked boundary substitutions and selected paths for
  ordinary family action. Recover classifiers through the existing regularity
  operation. The context-only API uses this same constructor without a term;
  no new proof rule, Core tag, or alternative boundary generator is added.
  Tests in dimensions 0-3 (all six cube orders in dimension three) check that
  variable action converts to the center with the expected classifier; the
  same checks on RETURN preserve computation polarity. Repeated action shares
  evidence, and missing/non-term input is rejected. Optimized and ASan/UBSan
  pointer checks pass; Identity passes with a 512 KiB stack. Source/header
  +39/-3; tests +18/-0; documentation separate. Construction and regularity
  remain synchronous. This constructs action in each oriented context, not a
  transposition between them; the typed symmetry gate remains open.

- [x] September 8, after `530e3b3`: typed cube-function tests exposed a real
  partial-action mismatch. Acting on THUNK(lambda x. RETURN(x)) under an
  unused type parameter reduced, but THUNK of the raw-function action waited
  for the inner function's boundary arguments. `action_scope` now consumes
  only complete supplied triples, using the existing argument cursor. The
  ordinary unused-binder rule can simplify this prefix; no eta rule or
  THUNK-specific comparison exception is added. Update the old shadowed-binder
  test, which incorrectly required the complete unused outer triple to remain
  neutral; add tests that one/two arguments of a triple still remain neutral.
  Typed raw Pi functions returning x or refl(x), and their THUNK values, now
  pass cube action/classifier comparisons in dimensions 0-3, including all six
  dimension-three orders. One new high-dimensional classifier comparison
  takes 161145 steps, exceeding the previous generic test cap of 100000 but
  terminating below its explicit per-case cap of 1000000. Print this maximum;
  retain the old cap for other comparisons. Optimized Identity execution was
  about 0.47 seconds locally (not a cross-machine performance guarantee).
  Optimized/ASan/UBSan pointer checks and 512 KiB Identity execution pass.
  Implementation/header +14/-3; tests +46/-3; documentation separate. This
  fixes partial-action coherence for the tested functions, not general
  higher-field computation, typed transposition or nominal datatype admission.

- [x] September 8, after `5e5bb0b`: generalize the existing cube-context and
  cube-action APIs to a nonempty dependent suffix with one supplied cube per
  declaration. All cubes use the same dimension and axis order. Build the
  zero-vertex suffix through checked reindexing, projection/composition and
  substitution pairing, so later declarations use earlier renamed images.
  Each iteration acts on all declaration faces through the existing context
  action; no parallel multi-variable construction path or new rule is added.
  Tests jointly vary A:Universe and x:A in dimensions 0-3, including all six
  dimension-three orders. The x-center classifier retains the selected
  A-center, variable action converts to the x-center, repeated construction
  shares evidence, and the A telescope is the same prefix produced alone.
  Duplicate cube owners and empty suffix requests reject; existing fixed
  ambient-prefix and single-variable tests still pass. Optimized and
  ASan/UBSan pointer checks and 512 KiB Identity execution pass. Source/header
  +55/-35; tests +44/-20 (including API migration); documentation separate.
  This removes the one-declaration restriction, not the pending typed
  transposition rule or synchronous construction cost (count * 3^dimension).

- [x] September 8, after `249f671`: check application elimination through
  dependent cube action, not only variable/constructor action. In A:Universe,
  x:A form a checked application of lambda y:A. RETURN(y) to x, then act on
  both declarations together. Dimensions 0-3 (six orders in dimension three)
  normalize to RETURN of the x-center. The shared `action_result` test helper
  checks the reduction certificate, regularity, and explicit classifier
  conversion as ordinary accepted evidence. No new implementation rule is
  required. Optimized/ASan/UBSan pointer checks and 512 KiB Identity execution
  pass. Tests +8/-0; documentation separate. This does not yet cover arbitrary
  neutral function application across permuted higher boundaries or supply
  the missing center-transposition rule.

- [ ] Add these semantic-family computation rules to a fixed pure conversion
  policy when implemented. The current wrapper admits beta, pure FORCE/FOLD
  and the implemented F/U/Pi Identity, RETURN/THUNK/FORCE action and diagonal
  value transport/lifting rules;
  its generic pair walker is not permission to certify an arbitrary callback's
  answers. Runtime handlers/oracles must not change this policy.

Acceptance examples must cover both polarities, a neutral F endpoint that stays
neutral, a returned endpoint that computes, dependent Pi action, and a second
action retaining the first action's chosen family. Transport/lifting boundary
tests are required before claiming this fragment is HOTT rather than a logical
relation. Indexed-family fibrancy remains a separate unresolved obligation.

Primary references checked again for this milestone:
[Narya observational primitives and heterogeneous Identity](https://narya.readthedocs.io/en/latest/observational.html),
[HOTT transport, lifting, glue and higher bisimulation](https://narya.readthedocs.io/en/latest/hott.html).
The latter explicitly distinguishes HOTT from parametricity and records limits
of implemented transport. Neither page specifies our CBPV F/U adaptation.
These are moving documentation pages, not the pinned Narya source revision.

Rechecked the primary [observational documentation](https://narya.readthedocs.io/en/latest/observational.html#heterogeneous-identity-types)
and [HOTT documentation](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting).
In Narya a heterogeneous identification retains the base identification through
the acted family. A universe identification has an underlying correspondence,
but instantiating that correspondence is not ordinary function application.
HOTT adds transport/lifting and higher compatibility; an arbitrary reflexive
correspondence is not thereby an equality. Its indexed-type fibrancy caveat
must not be mistaken for an already implemented generic solution.

The following is our CBPV adaptation, not a Narya theorem or implemented rule:

- A family is first an accepted formation under a boundary context. Given
  `Delta |- A type` and a checked substitution `sigma : Gamma -> Delta`, its
  instance is `Gamma |- A[sigma] type`. Use `pg_prove_substitution` and
  `pg_prove_reindex`; do not represent this operation as obtaining a runtime
  value from `F Universe`. Do not add ValueSidePi to encode this meta-level
  binding. Raw computation Pi retains its existing meaning.
- A higher family request must identify the original formation/occurrence,
  dimension operator, acted context and chosen lower-dimensional family/base
  evidence. The two endpoint types alone are not a sufficient key. A neutral
  semantic reference may eventually retain these immutable inputs, but only
  after its formation rule exists; allocating such a reference is not evidence.
- Materialize the boundary context in dependency order. Each declaration's
  classifier is an instance of an already checked lower face family. The center
  formation must have this entire context as its source. Supplying an arbitrary
  center type of the right universe does not establish that it is the action
  of the original type.
- Keep VALUE_TYPE/COMPUTATION_TYPE judgements distinct without adding Core tags.
  A raw Pi action has computation polarity. Passing its center as an ordinary
  context argument requires the existing U/thunk discipline; an implementation
  must not insert raw computation proofs into a value context. The Id rules for
  F/U remain obligations, not an assumption that all computations have values.
- Derive the classifier of an acted term by instantiating the selected acted
  family with its checked boundary terms. Lambda action must use an acted binder
  telescope; APP action must consume that same telescope, including the center.
  Checking only that corresponding subterms have some relation is insufficient.

Initial code audit: `pointer/action.c` only constructed restriction substitutions.
It now also builds checked generated contextual boundaries and Pi expansion
listed above; general dimension-map action and higher computation remain open.
`pointer/evidence.c` already supplies checked family instantiation by reindexing,
and now the symbolic Identity rules below. Legacy
`src/prototype/src/dimension/action.c` has boundary-applied family/classifier
builders; they return Term IDs rather than the fresh kernel's accepted formation
evidence. Reuse their equations only after verifying the premises, not their
successful return codes as new-kernel certificates.

#### Symbolic Identity formation and selected instantiation

- [x] One immutable `identity-action` reference represents `refl t`; applying
  `refl A` to two endpoints represents homogeneous `Id A x y`. Iteration uses
  that same Core reference, not dimension-specific tags. All source/endpoint
  terms remain ordinary APP operands, visible to substitution and comparison.
- [x] `PG_IDENTITY_FORM` checks an accepted base formation and both endpoint
  derivations in its context. Its output keeps the base polarity and universe
  bound. `PG_REFLEXIVITY` supplies only the diagonal witness with that formation
  as a premise; it cannot choose unrelated endpoints or register DefEq unions.
- [x] `PG_IDENTITY_INSTANCE` checks `R : Id Universe_i A B`, `x : A`, `y : B`
  and forms `R x y : Universe_i`. It keeps R explicitly, rather than replacing
  it with the endpoint pair. Ordinary functions and arbitrary relations are
  not accepted as R. This is not raw Pi application or a new value-side Pi.
- [x] Tests cover distinct choices R/S with identical endpoints, wrong endpoint
  types/scopes, universe bounds, three iterated symbolic diagonal witnesses,
  computation versus value polarity, ordinary reindex, and shared erased
  functions whose differently typed actions retain different classifiers.
- [x] Pure RETURN/THUNK and canonical F/U equations now share evaluator demand
  frames with beta, FORCE and zero-clause FOLD. The reducer inspects reference
  descriptors and Core operands, not classifiers or TypeViews:

  ```text
  refl (RETURN x)                     -> RETURN (refl x)
  refl (THUNK M)                      -> THUNK (refl M)
  Id (F A) (RETURN x) (RETURN y)       -> F (Id A x y)
  Id (U C) (THUNK M) (THUNK N)         -> U (Id C M N)
  ```

  Formation does not run these computations; explicit normalization may expose
  the source and endpoint heads using only the fixed pure policy. Unknown
  references remain neutral. Thunk contents are not demanded for these WHNF
  rules. Arbitrary selected R and noncanonical endpoints remain neutral.
- [x] `pg_whnf_work` replaces the beta-only memo API. Keys include the input
  and immutable policy pointer. Beta and pure results coexist without aliasing;
  conversion explicitly requests the fixed `pg_pure_policy`, never an arbitrary
  caller's dispatcher. Tests cover split budgets, capture, classifier conversion
  before/after action, suspension, neutral endpoints and policy isolation.
  Semantic-demand readback is now budgeted by the shared materialization
  checkpoint below; callback-local algorithms still need their own budgeting.
- [ ] Compute Pi/Lambda/APP action and general F/U rules on selected
  heterogeneous/higher families. The equations above are not full observational
  Identity. General action rules and complete surface action execution remain
  unfinished; typed evaluation uses directed normalization, not a second reducer.
- [x] RETURN/THUNK content jobs handle canonical reflexive actions through the
  same pure normalizer and typed inversion as other sources. Tests cover four
  iterated actions of a beta-redex source, split/bulk agreement, conversion,
  projection and nontrivial reindexing. Extracting acted THUNK code does not
  execute its stored computation. See the directed-normalization checkpoint.
- [ ] Generalize selected instantiation to acted boundary telescopes: the
  present instance rule handles a value-universe identification, not arbitrary
  higher-dimensional families. Connect the existing dimension-map operators.
- [ ] Implement transport, lifting, their computations and higher compatibility.
  No arbitrary relation-to-universe-Identity constructor has been added.

The value rules follow Narya's
[Identity of the universe and selected instantiation](https://narya.readthedocs.io/en/latest/observational.html#id-of-the-universe).
Keeping a computation base's Identity/reflexivity on the computation side is
our CBPV extension, not a theorem supplied by that document. The initial symbolic
checkpoint has now been extended by the four explicit equations above. They do not
establish termination, contextual equivalence, general higher coherence, or
the HOTT transport/lifting structure described in the
[HOTT documentation](https://narya.readthedocs.io/en/latest/hott.html).

- [x] Checked telescope pairing is shared with substitution lifting:
  `pg_prove_substitution_pair(sigma, Gamma.x, a)` constructs `(sigma,a)` using
  the existing substitution checker and canonical record. It does not add a
  Core tag, a second substitution authority, or a value-side function type.
  Reindexing with the result instantiates both value and computation families.
  Tests reject skipped dependencies, wrong classifiers, raw computations and
  out-of-context images; repeated pairing reuses the flat substitution result.
- [x] Family-instance regression: two selected formations may instantiate to
  the same Core type without losing their distinct formation premises. A
  subsequent reindex retains that choice and agrees with composed substitution.
  These are ordinary family formations, **not** Identity formation or a
  transport/lifting certificate. `PG_PI_CODOMAIN` still uses its valid direct
  substitution rule; no claim is made that Pi inversion has been migrated.

- [ ] Implement a checked acted-context/center-formation contract using these
  existing proof inputs; test different chosen correspondences with identical
  endpoints remain distinct.
- [ ] Implement the Pi/Lambda/APP center rules together, including F/U polarity
  and exact boundary instantiation; test identity and constant actions followed
  by a second action. A generic relation-preservation test cannot replace this.
- [ ] Supply transport/lifting computations and their boundary tests before
  advertising this as HOTT Identity. Pure type evaluation, generated IADT
  families and their fibrancy obligations remain part of the full goal.

- [x] Typed evaluation now consumes directed receipts from the common evaluator,
  retaining the accepted source's context and classifier. The old typed beta,
  FORCE/FOLD reducer and proof-shape-specific scheduler have been removed.
  Their earlier checkpoints are retained in Git history through `9317b13`.
  They duplicated the evaluator's demand ordering and required reconstruction
  of derivations that the normalized-source rule can retain directly.
- [x] Returned-value and THUNK-content requests use the existing synthesis job
  index, ready queue and waiters. Keys retain the typed context/input; common
  WHNF jobs share computation by exact Core and policy, not by classifier.
  There is no alternate execution route selected by the source derivation rule.
  Initialization does not evaluate; completed requests perform no new work.
- [x] Pure computed annotations work in Lambda/Pi domains and codomains and in
  the right side of `::`. The left side is synthesized independently. Tests
  cover computed callees, higher-order arguments, nested blocks, original
  annotation retention and rejection of a returned value that is not a type.
- [x] Tests independently construct checked substitutions and compare their
  reindexed bodies with beta evaluation. Reindexing APP/FORCE/FOLD is compared
  with applying the same substitution to each operand using ordinary rules.
  No dedicated reindexed-elimination or reindexed-premise exposure API remains.
  Interleaved weakening/reindexing and instantiated thunk variables retain
  their original evidence and evaluate in the supplied environment.
- [x] Explicit conversion remains a premise. Normalization retains the chosen
  classifier; RETURN/THUNK inversion extracts content at that classifier.
  Neither an erased Core lookup nor a certificate for `F A = F B` is used
  as an independent certificate for `A = B`.
- [x] APP and FOLD admit structural alpha equality of their domains, consistent
  with Lambda and checked substitution. Mismatches remain rejected; comparisons
  do not alpha-intern terms or implicitly beta-convert domain mismatches.
- [x] Regressions cover invalid contexts/polarities, distinct annotated inputs
  sharing one Core, exact-result reuse, split/whole budgets, and deep captured
  RETURN/THUNK bodies. Core evaluation and checked synthesis are compared for
  application and source block fixtures. The 158-case syntax inventory is still
  parsing coverage, not end-to-end semantic compatibility.
- [ ] Neutral/open pure type-family computations require a checked formation
  contract; `F Universe` alone does not justify extracting an unknown returned
  type. Unsupported neutral heads are not negative typing proofs.
- [ ] Budget remaining primitive formation, context-prefix and alpha traversals.
  Effectful runtime results must not enter the pure WHNF cache. Job-local
  closure work is not yet globally shared across distinct WHNF inputs.
- [x] Replace recursive common readback/substitution traversal with explicit
  heap work frames, indexed by the same `(term, lexical environment)` key.
  Children complete before their parent is constructed; repeated DAG children
  reuse one result. Lambda capture avoidance and simultaneous substitution
  retain their existing semantics, without alpha or reduction interning.
  A depth-50,040 shared APP graph substitutes successfully, including with a
  512 KiB process stack. Existing closure, conversion, restriction and typed
  evaluation tests exercise this same implementation, not a separate fast path.
- [x] Expose resumable substitution using these same work frames. Initialization
  snapshots the binding array without walking the input term; `advance` counts
  traversal transitions and each environment lookup link. Pending jobs expose
  no result. The synchronous substitution API drives this same engine to
  completion; no second substitution semantics is introduced. Tests compare
  split versus whole budgets and total steps on the deep shared graph, Lambda
  capture avoidance, long environment lookup, binding snapshot isolation,
  zero fuel, completion reuse, and destruction of suspended work.
- [x] Beta-WHNF jobs now retain readback work and materialize neutral application
  spines under the same requested budget. Public job status stays pending until
  the complete materialized answer exists; conversion consumes this interface
  unchanged. The shared readback/substitution traversal supplies the work, not
  a second term-reconstruction algorithm. Tests cover short execution with deep
  captured readback, split/whole step equivalence, capture avoidance, argument
  order, completed/pending request reuse and destruction during readback.
- [x] September 8, after `f796656`: semantic demand and final WHNF output now
  use one resumable closure/spine materializer. Explicit diagnostic readback
  drives the same algorithm synchronously; it does not introduce another
  evaluator or a Replay path. Each demand retains work in its evaluator frame,
  spends fuel on substitution/readback transitions, then copies one argument
  prefix link per step. The untouched argument tail and Core terms stay shared.
  Only a complete answer and restored caller enter the callback, exactly once.
  Suspended materialization is released on cancellation and after delivery.
  These mutable frames are invocation-local work, not new semantic authorities.
  `tests/core.c:demand_budget_test` checks a captured depth-5,000 shared DAG,
  capture avoidance, bounded per-call step increments, split/whole result and
  step agreement, diagnostic residual restart, pending WHNF receipts, and
  cancellation during traversal and prefix copying. A 65-argument neutral
  test dispatcher confirms callback timing, argument order and tail preservation.
  These are Core evaluator fixtures, not new accepted effect operations.
  Existing typed Identity, synthesis, FORCE/FOLD and nested-demand tests use
  the same path. The new steps expose previously uncounted work; this is not a
  claim of faster evaluation or constant wall-clock cost per step.
  Remaining unbudgeted work includes callback algorithms, argument lookup and
  consumption, index maintenance, allocation/freeing, and primitive checking.
  Verification passed: optimized and ASan/UBSan pointer `make check`, plus
  Core/synthesis/Identity with a 512 KiB stack. The 158 syntax outcomes still
  establish parsing only, not full legacy program compatibility.
  Sizes excluding docs: `eval.c` +100/-84, `eval.h` +3/-1
  (implementation net +18); `tests/core.c` +100/-0.
- [x] Beta execution now consumes one environment link per variable-lookup
  transition. For a reference, dropping an unrelated environment prefix changes
  neither its denotation nor captured argument closures; the shared environment
  list remains immutable. This removes the synchronous lookup loop without a
  second cursor/tag in the evaluator. A 64-level environment regression checks
  suspension at each link, residual readback, and split/whole step equivalence.
- [x] Reindex evidence construction now advances the common substitution engine
  for Core, classifier and optional annotation before accepting any conclusion.
  The synchronous API uses the same preparation/advance/accept path with local
  work storage; cached synchronous requests do not allocate a new heap job.
  Public work owns no accepted proof: completion returns the immutable premise-
  keyed evidence, including when another request has accepted it in the meantime.
  Tests cover no pending evidence, split/whole execution, cancellation, invalid
  contexts, Lambda domain annotations, and concurrent identical conclusions.
- [x] The old synthesis-only reindex job used for typed beta/content reconstruction
  has been retired with that evaluation path. The resumable checked reindex API
  above remains for genuine context substitution, with its independent tests.
- [ ] Thread resumable substitution through remaining typed evidence work and
  budget semantic callback algorithms. Binding validation/copying, allocation and
  index maintenance are not wall-clock bounded. Primitive evidence rules still
  use synchronous alpha comparison. This does not complete compiler-wide fuel.
- [x] Structural alpha and beta conversion now share one resumable scoped-pair
  walker in graph.c. Structural comparison supplies no normalization; conversion
  supplies the existing fixed beta-job policy. Pair memoization includes the
  binder correspondence, and reference lookup advances one scope link at a time.
  Pointer identity only shortcuts comparison outside a binding correspondence.
  Conversion alone issues its opaque certificate after successful comparison;
  an arbitrary comparison callback cannot produce such a certificate.
- [x] The synchronous alpha API drives that same walker; synthesis uses its
  resumable structural mode for final beta-classifier checking. No Core tag or
  alpha/WHNF interning was added. Tests compare 20,000-deep shared APP structures
  with split/whole budgets and linear task counts, reject bound/free confusion,
  and distinguish structural inequality from beta convertibility. Existing
  typed evaluation and conversion tests continue through the common algorithm.

**HOTT rather than only relation preservation.** N2 includes checked contracts
for transport/lifting and their dimensional boundaries, with actual computation
rules for the initial supported type formers. Unknown families may remain
neutral with checked types. Unsupported rules must be marked unsupported, never
fabricated as axioms or reported as complete HOTT. Derive and check symmetry and
composition in the supported fragment. Do not defer their design until after
the artifact format is frozen.

**IADT and effects.** N3 must implement action on declaration telescopes,
constructors, dependent Match and IH at the same time as ordinary IADT support.
Generate higher families with generic indexed declarations and pointer owners;
do not add a primitive per datatype or per dimension. Write the fibrancy and
transport obligations for indexed families explicitly; a generated higher IADT
alone does not discharge them. N4 must specify the action laws for CBPV
return/thunk/force and fold. Effectful observational equality needs a stated
observation model and cannot follow from equal returned values. An arbitrary
host oracle receives no automatic Identity proof.

**Early gates.** N1 tests map identity/composition, faces, degeneracies and binder
scope at dimensions 0, 1, 2 and a dimension-3 smoke test. N2 must compute action
on identity, a constant function and APP, then act again on the result and check
its boundaries and classifier. Include a non-DefEq function equality witness
once the necessary observational rules exist; same-term reflexivity alone is
insufficient. In N3, distinct Bool constructors must reject an attempted witness,
and Identity-generated datatypes must themselves support Match/action. No claim
of all coherence laws follows merely from running the dimension-3 example.

Current `src/prototype/include/a_program/dimension/action.h` already exposes
acted-family and context-aware classifier operations. Audit it and
`src/prototype/src/dimension/` for reusable equations and fixtures; do not port
their integer stores or the previous multi-stage publication machinery blindly.

## 6. One Typed Work Graph

`pointer/classifier.c` introduces structural classifier constructors/views,
not a checker. Universe objects retain distinct concrete `uint64_t` levels,
interned within one classifier owner. Pi is
`APP(APP(pi-former, domain), LAMBDA(binder, codomain))`, reusing ordinary Core
nodes and simultaneous substitution. Semantic object headers carry an owner
descriptor pointer; its name is diagnostic, not semantic identity. Core
interning/evaluation do not inspect that descriptor to select a type view.
These in-process descriptors will need explicit image relocation identities.

- [x] Test universe level distinction and index growth; Pi exact-pointer reuse,
  separate alpha/beta comparison, and codomain substitution. Sanitizers pass.
- [ ] Add checked universe/Pi formation and source elaboration. The structural
  constructors do not establish value/computation sorts, universe constraints
  or domain/codomain well-formedness. Concrete levels do not replace the planned
  level metavariables/constraints; `UINT64_MAX` has no representable successor
  and must not silently wrap when formation is implemented.

Contexts are persistent extensions containing binder pointers and classifiers;
extending a context shares its prefix. Typed occurrences preserve scope and
source evidence even when Core is shared. Context substitution is a mapping of
these bindings, not a copied parallel term tree.

`pointer/evidence.c` now accepts immutable primitive derivations for empty
context, context extension, concrete Universe formation and variables. Context
extension requires a derivation of its declared type in exactly the parent
context and a fresh binder. Universe formation produces `U(level+1)` and rejects
overflow. Variable evidence references the verified context, whose premise DAG
contains the declaration formation. Term judgments are attached to occurrences,
not only erased Core. Proofs are opaque externally and interned by rule,
conclusion and all premise pointers; no accepted record is overwritten.

- [x] Verify primitive context/Universe/variable premises, rejection of free
  variables, wrong scopes, duplicate binders and non-type declarations; retain
  distinct occurrence evidence over shared Core. Ordinary/sanitizer tests pass.
- [x] Add pure F/U/Pi formation using existing Reference/Application/Lambda
  spines. Distinguish value/computation type formation from value/computation
  inhabitation in evidence conclusions, never by duplicated Core tags.
  A value inhabiting a value universe determines a value type; computation-type
  formation does not make that type a value inhabiting the same universe.
  Formation classifiers record concrete universe bounds. Pi requires a value
  domain, its verified context extension, and a computation codomain in that
  extension; its bound is the maximum of the two premise bounds. Ordinary tests
  cover wrong scopes, wrong sorts, retained premises, interning and bounds.
- [x] Add checked RETURN/THUNK/FORCE and Lambda/APP derivations. Fixed semantic
  operation references build ordinary APP spines. Lambda uses the verified Pi
  context/codomain; APP substitutes its value argument into the codomain with
  the existing simultaneous substitution implementation. Typed operands and
  all proof premises remain attached to occurrences. Tests cover invalid
  value/computation sorts, wrong scope/domain/codomain, explicit force before
  applying a thunked function, proof reuse, and beta reduction of a typed
  identity application to its RETURN spine.
- [ ] Connect these rules to synthesis and semantic execution. The current APP
  rule requires exact classifier pointers; an explicit conversion derivation
  remains necessary for non-identical convertible classifiers. The beta-only
  evaluator intentionally keeps semantic references neutral. F currently
  describes the pure fragment, not unspecified effect rows. Effect rows and
  dependent sequencing remain pending.
- [x] Connect value-type formation to term-level universe inhabitation by an
  explicit checked derivation. This is a Russell-style value universe: F/Pi
  computation formation cannot use this rule; U of a computation type can.
  Test an assumed `f : U(Pi(A:U1, F A))`: applying `force f` to the value U0
  synthesizes `F U0`, while applying it to an open `B:U1` synthesizes `F B`.
  Neither requires executing `f` or deciding B's eventual value. Formation
  evidence is not silently accepted as argument inhabitation evidence.
- [x] Prefix-context projection (weakening) retains the original proof and Core
  and creates only a conclusion occurrence and derivation in the extended
  context. Its operands remain shared under the explicit projection evidence;
  no recursive proof copying or binder renaming occurs. Reject shrinking,
  sibling scopes and context formation used as a term premise.
- [x] Recover classifier formation from existing variable, RETURN, THUNK,
  FORCE, Lambda, type-as-value and conversion derivations. This regularity
  operation consumes an already synthesized judgement, not an expected type.
  Tests obtain the formation of a RETURN body and construct its enclosing Pi
  from that result. No `::` information enters this path.
- [x] Add checked finite context substitutions as derivations, without another
  mutable solution database. Images are value derivations in the destination
  context, ordered by source declarations. Validate each dependent declaration
  after substituting preceding images. An immutable pointer mapping is stored
  inline with the accepted derivation as a projection of those premises.
  Reindex Core/classifier/annotation through the existing capture-avoiding
  substitution traversal and retain the original proof DAG as a premise.
  Tests cover dependent renaming, empty source, invalid arity, image types,
  computation images, foreign scopes and recovery of reindexed classifiers.
- [x] Compose checked substitutions by reindexing their value premises and
  validating the resulting common substitution representation. Lift under a
  binder by reindexing its declared type, extending the destination context,
  projecting old images and adding the new variable image. No composition or
  lifting Core tag is introduced. Test the composite against successive
  reindexing, new/old variable images and rejection of incompatible contexts
  and non-fresh destination binders.
- [x] APP/FORCE regularity now uses formation inversion rather than retracing
  the provenance of a Pi/U type. From a checked `Pi(x:A,B)` and `v:A`, obtain
  formation of `B[v/x]`; from checked `U B`, obtain formation of B. These rules
  apply to generally reindexed formations too. Their derivations retain the
  type formation and argument premises, not an unchecked syntactic view alone.
  Both rules retain the parent's universe *upper bound*. In particular, Pi
  formation bounds its codomain by max(domain,codomain), so codomain inversion
  need not recover a minimal universe. This is not equality between levels.
  This removes APP's repeated reconstruction of the entire source context's
  image array solely to recover result formation.
- [ ] Connect these context actions to dimensional action. The current
  substitution checker admits structural alpha equality;
  nonstructural conversion must be made explicit before supplying an image.
  It does not decide arbitrary effectful equality.
- [x] Reindex results are retrieved from the existing derivation index by their
  immutable substitution/proof inputs before traversing or freshening binders.
  The result's fresh pointers are outputs, not the key identifying this work.
  Context-substitution requests similarly retrieve an accepted derivation before
  recomputing dependent declaration substitutions. The same index lookup code
  serves acceptance and reuse; there is no parallel result authority. Test 100
  repeated binder-containing reindex/lift requests with identical proof results
  and zero growth in Term and derivation counts. Different inputs are not merged
  by alpha or WHNF equality. This is not memoization of runtime effects.
  APP elimination and Pi-codomain formation now use this same input-keyed
  derivation lookup before substitution. A regression returning a dependent
  Pi exposed fresh classifier binders and duplicate proofs on repeated APP;
  100 identical APP/regularity requests now return the original proofs without
  increasing Term or derivation counts. Accepted premises remain part of the
  key, so distinct typed occurrences are not merged through a shared Core.
  FOLD elimination and constant-codomain formation also consult this index
  before repeating their independence check. A repeated FOLD previously
  returned the same evidence but allocated fresh test references on every call;
  the 100-request regression now checks both proof identity and zero Term growth.
  This reuses accepted typing work only, never the result of executing FOLD.
  Regularity for unsupported rules returns NULL, not a refutation. Recovery
  currently traverses the relevant proof premises; scheduling/memoization and
  source synthesis still need integration rather than a separate type authority.
- [x] Initial source-synthesis jobs are interned by syntax and lexical scope.
  Requests do not synthesize; a budgeted ready queue advances dependencies.
  A waiting parent subscribes once to its child and wakes on completion rather
  than scanning all pending work. Results point to accepted derivations, with
  no parallel mutable classifier answer. Completed jobs do not run again.
  Source scopes map names to pointer binders and verified contexts; they are
  not execution environments and do not change Core interning.
- [x] September 8, after `5328609`: intern source scopes using the common
  `pg_index`, eliminating independent allocation paths for roots, binders,
  accepted aliases, namespaces and definition scopes. Keys retain exact parent,
  context evidence, binder, producer, definition-state and export pointers plus
  name spelling/kind; source offsets are not binding identity. Mutable definition
  progress is not hashed. No alpha comparison, normalization or Core-only typing
  lookup occurs. A regression failed on repeated root construction before this
  change. Rebuilding the same inputs 100 times now adds no scopes, jobs, Core,
  proofs, reduction jobs or scheduler steps, and reuses the same source job.
  Distinct typed proofs over shared Core, contexts, binders, parents, names and
  namespace exports stay separate. Optimized check: 3.358 s (synthesis rebuilt);
  ASan/UBSan: 17.899 s (affected binaries rebuilt); all four computation suites
  pass with 512 KiB stack. Implementation `synthesis.c` +48/-22,
  `synthesis.h` +5; tests +51; documentation separate. N2/N5 remain open.
- [x] September 8, after `85c9c61`: share definition indexing and producers by
  definition AST/scope through one `DEFINITION_SCOPE_JOB` in the existing queue.
  Its completion means names/entries were registered and activated, not that
  their terms were accepted. Root and selector requests reuse those producers;
  selectors resolve a name, then await the common whole-definition root.
  The root joins every definition/check once. No selected-only validation,
  duplicate proof checker, new Core form or polling loop is introduced.
  A regression on distinct selector/root producers failed before the change.
  After completion, another selector adds only its request, not scopes, Core,
  proofs, reduction jobs or definition producers. Tests also cover duplicate
  names, invalid unselected definitions/expectations, shared rejection, and a
  missing selector rejecting despite unrelated cyclic pending definitions.
  A valid selector of that cyclic module remains pending without busy work.
  The indexing test now accounts for one shared setup transition before the
  first dormant producer is registered. Optimized check: 3.465 s (synthesis
  rebuilt); ASan/UBSan: 18.394 s (affected binaries rebuilt); four computation
  suites pass at 512 KiB stack. Implementation `synthesis.c` +32/-14,
  `synthesis.h` +4/-1; tests +76; documentation separate. File loading and
  source/export integration with nominal declarations remain open.
- [x] September 8, after `b94c5fd`: `pg_synthesis_module_namespace` mounts a
  closed source-definition job, even while pending, in the existing namespace
  lookup. Selections canonicalize to their whole-definition root. References
  await the shared registration job, resolve only that module's own names,
  then await whole-module acceptance before using the original producer proof.
  Ambient library names are not re-exported. Registration does not solve or
  execute code; scope interning includes the module pointer, not a copied
  classifier/result. Static and module namespace bindings share publication and
  member traversal. No Replay, second solver or Core namespace form is added.
  Tests synthesize source-defined Eq/refl aliases, call them from another scope,
  normalize a function through a second source module, preserve local shadowing,
  reject open/foreign/non-module inputs, avoid jobs for invalid registrations,
  propagate unselected errors, and reject missing members during cyclic pending
  work without polling. Optimized check: 3.474 s (synthesis rebuilt);
  ASan/UBSan: 18.148 s (affected binaries rebuilt); four computation suites pass
  with 512 KiB stack. Implementation `synthesis.c` +67/-17, `synthesis.h` +9;
  tests +93; documentation separate. File resolution, `import` statement wiring,
  recursive module loading, `.a` loading and N2/N5 acceptance remain open.
- [x] September 8, after `7cd8730`: bind independently pending term producers
  with `pg_synthesis_name_job`. Existing accepted-name publication delegates to
  the same scope/producer key; source references use the common subscription
  and checked projection path. No inferred classifier is copied into the scope.
  Registration does not advance work, insert CBPV coercions or execute exports.
  Tests consume a pending source export under an ordinary name, normalize an
  application, share accepted/pending publication keys (including weakening),
  reject foreign inputs, preserve failed/cyclic dependencies, and prevent open
  evidence escaping into a closed context. A completed non-term producer is
  unsupported. Incompatible projection still uses the kernel API's existing
  ERROR classification; finer premise/allocation diagnostics remain open.
  Optimized check: 3.078 s (synthesis rebuilt); ASan/UBSan: 15.978 s (affected
  binaries rebuilt); four computation suites pass at 512 KiB stack.
  Implementation `synthesis.c` +14/-5, `synthesis.h` +9; tests +69; docs separate.
- [x] September 8, after `d14b5f5`: connect source imports to driver-selected
  bindings through `pg_synthesis_import_scope`. Configuration is hidden from
  lexical lookup; an import awaits its selected producer through the ordinary
  reference job and checked projection, without assignment-time quotation.
  One name hash index holds local and imported producers; local definitions
  shadow imports in either source order. Repeated imports share a job.
  Imported names can be selected or post-checked but are not implicitly exported
  as module members; explicit aliases are exported. Invalid unused imports still
  reject the module. No copied classifier, separate solver or Replay is added.
  Tests cover pending exports, calls/NF, hidden configuration, shadowing,
  duplicate-job sharing, post-check failure, export boundaries, missing symbols,
  raw computation imports, open/foreign/non-term inputs and cyclic waiting.
  Optimized check: 4.442 s; ASan/UBSan: 19.336 s (affected binaries rebuilt).
  All four computation suites pass at 512 KiB stack. Implementation:
  `synthesis.c` +59/-19, `synthesis.h` +8; syntax diagnostic +1/-1;
  tests +110. These are test/build timings, not end-user compiler benchmarks.
- [ ] Complete driver provider resolution while preserving symbol imports:
  `import Nat;` selects an exported symbol, not a module/file named Nat
  (`src/prototype/README.md`, source imports; driver `read_file.c`). Do not
  substitute namespace mounting for symbol import. A selected export can use
  the pending-name API above. Old `prototype_ast_add_import` in frontend
  `ast.c` treats repeated imports of the same symbol as idempotent; preserve and
  test that behavior. The source-import checkpoint above implements local
  shadowing and explicit re-export. Provider ambiguity and filesystem search
  remain driver responsibilities, not solver guesses. No configuration reports
  UNSUPPORTED; a missing symbol in a supplied complete set reports REJECTED.
  Filesystem resolution, recursive module loading, `.a` restoration and full
  source compatibility remain open; source wiring alone does not close N5.
- [x] Connect value universes, scoped variables, annotated Lambda, dependent Pi,
  APP, quotation and inline post-synthesis `::`. Lambda bodies retain raw
  computation polarity, while value bodies acquire RETURN. Function-typed
  value domains use U(Pi); callable thunk values are explicitly forced during
  elaboration. Inline `::` first finishes its left job, then synthesizes its
  target and performs a resumable comparison; the expectation never flows back
  into the left job. Tests compile a raw polymorphic nested identity, execute
  an application, check function expectations and reject unresolved names.
- [x] September 8, after `49ea57d`: open source telescopes through shared
  binding jobs. Ordinary Lambda/Pi synthesis and `pg_synthesis_telescope` now
  use the same domain synthesis, pure type-input handling, fresh pointer binder
  and checked Context extension. Exact source/scope keys share those outputs;
  this does not intern by alpha or normal form. No value-side Pi, new evidence
  rule, declaration-specific context representation or expected-type synthesis
  is introduced. A telescope returns ordinary context evidence plus its source
  scope and unprocessed tail; it does not certify that tail or admit a datatype.
  Tests open parameter/index/field lists from an indexed declaration, retain
  their scope separation, and connect the result-index expression to existing
  substitution pairing and `pg_data_schema`. They also cover expression-first
  and telescope-first binder sharing, computed domains, invalid domains, empty
  and anonymous telescopes, 256 iterative binders, foreign scopes and cyclic
  waiting without polling. Nominal formation still reports UNSUPPORTED;
  Self, positivity, membership, typed Match/IH and indexed fibrancy remain N3
  obligations. The common scheduler is bounded in transitions; context extension
  and primitive proof checks remain synchronous, not constant-time operations.
  Optimized check: 3.621 s (synthesis rebuilt); ASan/UBSan check and four
  computation suites at 512 KiB stack passed. Implementation `synthesis.c`
  +85/-26, `synthesis.h` +11; tests +145; documentation separate.
- [x] September 8, after `594c23b`: `pg_synthesis_data_result` connects the
  constructor result syntax `* index...` in an already checked field scope to
  ordinary substitution evidence. Parameters retain their original binder
  references even under lexical shadowing. Index expressions are independent
  source jobs; shared pure return exposure, reindex/conversion and pairing
  check their dependent classifiers without expected-type-guided synthesis.
  The shared Context extension-size helper replaces the IADT-local traversal.
  Tests connect parsed field/result syntax to `pg_data_schema`, compare split
  and bulk work, accept computed indices, and reject wrong self heads, arities,
  dependent argument types and unrelated contexts. Empty maps, shared results,
  invalid API inputs and cyclic index waiting are covered. No membership,
  positivity, Self-family formation or indexed fibrancy is inferred from this
  map; those nominal-admission obligations remain open. Setup walks the spine
  and context linearly; existing flat substitution premises can still retain
  quadratic aggregate prefix storage. No new Core or proof rule is introduced.
  Optimized check: 3.707 s (synthesis rebuilt); ASan/UBSan: 18.741 s (affected
  binaries rebuilt); four computation suites pass at 512 KiB stack.
  Implementation: `synthesis.c` +99/-1, header +9; `typing.c` +13,
  header +4; `iadt.c` +3/-14; tests +57/-1; documentation separate.
- [x] September 8, after `a9b5055`: `pg_synthesis_data_schema` assembles a
  parsed declaration in its already opened parameter scope. Index/field
  telescopes and constructor result maps use the preceding shared jobs.
  Constructor names use the existing hash-index helper. Each constructor's
  result-map work can proceed independently even while an earlier constructor
  waits. The declaration joins all producers before creating its immutable
  schema/layout; repeated requests reuse it, distinct declarations stay fresh.
  Work retains producers, not a second persistent array of accepted answers.
  The result has no term evidence and cannot be published as an admitted type.
  Recursive Self/IH references explicitly remain UNSUPPORTED, not a missing-name
  proof of invalidity. Nominal formation, positivity, membership, Match/IH and
  higher fibrancy remain required; N3 is not complete. Tests cover indexed and
  empty schemas, exact owner reuse, distinct owners, duplicates, invalid results
  and fields, scope separation, pending/failed publication and progress of a
  later constructor's map during an earlier cyclic dependency. Schema checking
  and final input collection remain synchronous linear traversals.
  Optimized check: 3.848 s; ASan/UBSan: 10.245 s (synthesis rebuilt); four
  computation suites pass with 512 KiB stack. Implementation `synthesis.c`
  +105/-1, header +8; tests +114; documentation separate.
- [ ] Complete source lowering/synthesis: import providers and full definition diagnostics,
  literals, ADT/IADT, computation blocks/folds, implicit sequencing
  of remaining returning argument/callee cases, computed type
  annotations, structured error reasons and comprehensive surface compatibility.
  Unsupported syntax and unsupported computation-argument cases report UNSUPPORTED,
  not a theorem of untypability. Kernel APIs still conflate some allocation and
  premise failures; error classification must be completed. Budget currently
  counts scheduling/comparison transitions, not all work within a kernel rule.
  This initial expression-job store is not a serialized `.a` image yet.
- [x] Whole-source parsing collects flat definitions into the same definition
  array shape as explicit `{{...}}.name` roots, preserving explicit selection
  rather than inventing a `main` binding for libraries. Assignments, standalone
  post-checks and imports retain their entry kinds and source order. No Core
  terms, name resolution, implicit quotation or effects are produced by this
  step. Tests cover forward names, unchanged computation RHS, empty sources,
  malformed suffixes and rejection after partially consuming a source.
  This is an unresolved syntax input, not yet the N5 program image or a solved
  module. Legacy definition fixtures require implicit/explicit thunk policy;
  that policy belongs to synthesis, not this parser normalization.
- [x] Definition roots now register all local producer jobs before advancing
  bodies. Name references subscribe to those jobs and project their accepted
  evidence into the use context. Aliases share the same evidence; no parallel
  name-to-classifier answer store is introduced. Definition adaptation is a
  distinct job role keyed with syntax/scope, so expression synthesis remains
  raw and independent of the definition's implicit/explicit thunk policy.
  Standalone `::` creates an ordinary post-check job and never supplies an
  expected classifier to the producer. The root waits for all entries, not
  merely a selected prefix as a sequential block does. Unselected libraries
  expose producer jobs rather than inventing an expression or a `main` value.
  Tests cover forward references, type aliases, shared function aliases,
  post-checks before definitions, both quotation policies, duplicate/missing
  names, failure in an unselected definition, and execution after explicit force.
- [x] Definition indexing and activation advance one entry per scheduling
  transition. Producer jobs remain dormant until all names are registered;
  partial budgets cannot expose an incomplete name table to a body. Shared
  producer jobs are activated once even when multiple entries share syntax.
  Tests inspect a one-step prefix (one indexed name, no generated Core) and
  resume it, as well as a shared-syntax definition DAG.
- [ ] Definition SCC diagnostics and imports remain incomplete. A circular
  alias pair exhausts its ready work and stays Pending with no accepted proof;
  it is not a supported recursive definition. Array allocation and hash-index
  growth are still unbudgeted storage operations. Image persistence, complete
  budgeting, imported names and full definition compatibility remain required.
- [x] Active dependency inspection uses the existing subscription edge in both
  directions: the child wakes its waiters and clears their active edge, while
  a consumer exposes the child it is waiting for. There is no copied pending
  status or second dependency graph. An on-demand constant-space cycle query
  follows active edges without scanning all jobs or advancing/rejecting work.
  Tests verify completed jobs have no active dependency, a circular alias pair
  reports a closed waiting path with no accepted result, and additional fuel
  does not reschedule the stalled cycle. This is diagnostic evidence, not an
  SCC acceptance rule or a proof that every pending job can make progress.
- [x] Ordinary APP and inline `::` share one resumable comparison path. Cache
  the already synthesized input and target derivations while comparison is
  pending; do not rerun their synthesis or reconstruct adaptations on each
  comparison step. Pi domain formation is recovered by checked inversion with
  the parent's universe upper bound. A mismatch in argument classifier pointers
  invokes explicit conversion, never alpha interning or expected-type inference.
  A source fixture applies a higher-order function to a quoted Lambda with a
  distinct alpha-equivalent Pi classifier, retains the conversion certificate
  in its argument premise, and evaluates to RETURN of the original argument.
  A function with a different result type is rejected by the same comparison.
- [ ] Semantic conversion extensions, full synthesis scheduling, image checking and
  typed HOTT action still need implementation. These primitive rules do
  not constitute a complete checker; NULL currently combines invalid-premise
  and allocation failures and is not a solver-level logical rejection result.

### Directed normalization evidence (September 8)

- [x] `pg_whnf_job` publishes an opaque, graph-owned receipt only after directed
  evaluation and readback complete. The receipt owns the result reference;
  there is no separate mutable result authority. It records the exact source,
  target and evaluation policy, and survives disposal of temporary job storage.
- [x] `PG_PURE_NORMALIZATION` accepts an already checked term or formation plus
  a receipt from exactly that source under `pg_pure_policy`. Its conclusion
  retains the original context, classifier and judgement, with the source proof
  as a premise. This is subject reduction, not symmetric conversion of subjects:
  `x` being beta-convertible to `(lambda ignored. x) unbound` cannot establish
  the typing of that expansion. Unfinished and other-policy receipts are rejected.
- [x] `pg_synthesis_normalize` uses the existing scheduler and shared WHNF store.
  Computation sharing is keyed by Core/policy; accepted evidence is keyed by
  the typed source. Tests cover distinct annotated functions over one erased
  Core, different contexts, classifier recovery after weakening, acted terms
  and formations, split/bulk scheduling and receipt lifetime.
  Ordinary `check`, ASan/UBSan `check`, and the synthesis suite with a 512 KiB
  stack pass. The syntax inventory still measures parsing, not full acceptance.
- [x] RETURN/THUNK exposure now uses shared typed normalization, classifier
  normalization when necessary, explicit conversion, and canonical-head
  inversion. `PG_RETURN_VALUE` derives `v : A` from accepted `RETURN v : F A`;
  `PG_THUNK_COMPUTATION` derives `M : C` from accepted `THUNK M : U C` without
  running M. Direct introduction evidence still reuses its existing premise.
  The contents scheduler no longer dispatches on projection, reindex,
  conversion or reflexivity derivation shapes, nor recursively reconstructs
  context actions just to obtain contents. Classifier recovery handles both
  inversion rules. This relies on inversion/injectivity of the admitted F/U
  typing rules in addition to the subject-reduction obligation below.
- [x] Regression tests retain neutral-head refusal, converted classifiers,
  weakening and substitution, unchanged stored THUNK code, four iterated
  canonical actions and split/bulk scheduling. Tests distinguish identical
  cached derivations from different derivations of the same judgement; no
  proof irrelevance or alpha interning is asserted. Typed identity/composition
  actions applied to a boundary triple now yield a checked returned path via
  normalization and inversion, not only an untyped conversion comparison.
  Ordinary and ASan/UBSan suites pass; synthesis and Identity suites also pass
  with a 512 KiB stack. This integration removes 52 net implementation lines
  (excluding tests and this plan), before the subsequent removal below.
- [x] Remove the standalone typed beta/FORCE/FOLD reducer, its preparation and
  reconstruction APIs, and the reduction/reindex scheduler roles used only by
  that path. Remaining callers now use `pg_synthesis_normalize` or directly
  combine the shared WHNF result with `pg_prove_normalization`. Tests retain
  ordinary substitution/reindex checks and independently expected result terms.
  Removed APIs include `pg_reduce_beta`, `pg_reduce_computation`,
  `pg_synthesis_reduce` and the three `pg_prove_reindexed_*` exposure helpers.
  Ordinary and ASan/UBSan checks pass; synthesis and Identity tests pass with
  a 512 KiB stack. Changes against `9317b13` (documentation excluded):

  | File under `src/prototype/pointer/` | Added | Removed | Net |
  | --- | ---: | ---: | ---: |
  | `evidence.c` | 0 | 248 | -248 |
  | `evidence.h` | 0 | 41 | -41 |
  | `synthesis.c` | 1 | 145 | -144 |
  | `synthesis.h` | 0 | 4 | -4 |
  | Implementation total | 1 | 438 | -437 |
  | `tests/core.c` | 42 | 30 | +12 |
  | `tests/synthesis.c` | 55 | 49 | +6 |

- [ ] Extend sharing beyond exact `(Core, policy)` WHNF jobs where appropriate.
  Nested evaluator closures still have job-local work; using the common
  evaluator does not establish that every demanded subcomputation is memoized.

The rule adds an explicit metatheoretic obligation: every admitted pure rewrite,
including acted Lambda/APP and binder freshening during readback, must preserve
the accepted judgement. Directed evaluation prevents arbitrary expansion but is
not, by itself, a proof of preservation. Current fragment tests do not establish
general HOTT subject reduction, transport, lifting or higher coherence. Adding a
semantic owner to the pure policy requires checking its rules against this
obligation; an arbitrary runtime handler cannot issue accepted typing evidence.

These receipts are trusted in-process results, not serialized reduction traces.
N5 must reconstruct their justification through the same evaluator or validated
retained reduction evidence, not deserialize endpoint/policy fields as a proof.

### Image checking is not a separate Replay semantics

The September 7 clarification applies to every replay gate below. A `.p` file
creates an unresolved program image; solving advances that image, and loading
resumes the same solver. Do not introduce a loader-specific compilation or
proof-search pipeline. Previously solved work must not be searched again merely
because it crossed a file boundary.

Loading still checks structural references and the validity of retained evidence.
Use the same checked derivation rules used to accept new solver results. Check a
shared premise DAG once per load rather than recursively duplicating its work.
Checking evidence is distinct from rediscovering its proof: conversion premises
may nevertheless require reduction or validation of retained reduction evidence.
No unchecked serialized "accepted" bit constitutes a proof.

- [ ] N5: restore pending work and resume the existing solver without a parallel
  Replay engine; distinguish discarded work from retained validated results.
- [ ] N5: route reconstructed derivations through the existing acceptance rules;
  reject invalid evidence and test shared-DAG checking and split-budget resume.
- [ ] N5: document what seed/checkpoint retention saves and which calculations
  must be repeated. Do not promise zero recomputation for a compact image.

September 8 clarification: image progress is retained evidence, not a trusted
completion flag. There is one evaluator, one solver and one set of evidence
acceptance rules; loading is not a second implementation of any of them.

| Retained content | Loading obligation |
| --- | --- |
| Unresolved goals and their semantic inputs | Rebuild references and schedule through the ordinary solver |
| Checked derivation DAG | Validate premises with ordinary acceptance rules, once per shared node |
| Conversion endpoints without reduction evidence | Re-establish conversion through the ordinary evaluator |
| Checkable reduction evidence | Validate it under the identified pure rules before accepting the result |
| Disposable search/cache state | Rebuild when omitted; never treat it as independent evidence |

Current implementation evidence: `conversion.c` privately creates
`pg_conversion_certificate` after successful conversion, but the certificate
contains only its two endpoints. It is an in-process receipt, not a portable
proof trace. `reader.h` is a source lexer, not an image loader. Neither API
establishes N5 persistence or fresh-process resumption today.

An image of a program containing effects is not a checkpoint of external world
state. Loading/checking it must not execute an operation request. Re-running a
program may repeat effects; resuming an already effectful runtime session with
exactly-once external effects is a separate contract, not implied by `.a`.

- [ ] Specify seed/checkpoint retention options using the same image model;
  omit caches without deleting the semantic inputs required to reconstruct work.
- [ ] Record the rule/environment dependencies needed to validate retained
  evidence; incompatible dependencies invalidate reuse, not the kernel rules.
- [ ] Test fresh-process seed versus checkpoint loading: same accepted results,
  different allowed recomputation counts, with split-budget continuation.
- [ ] Test that loading and checking an effect-containing image emits no effects;
  explicit execution, not deserialization, invokes the runtime handler.
- [ ] Test that serialized conversion endpoints alone cannot manufacture an
  accepted conversion certificate, and shared derivations are not re-searched.

`pg_term_substitute` now exposes the evaluator's existing capture-avoiding
readback traversal for simultaneous binder-pointer substitution. Images are
inserted without resubstitution or reduction; later mappings shadow earlier
ones for the same pointer. This is an untyped term operation, not a certified
context morphism or a dimensional action. Typed substitution must still verify
the domain/codomain declarations. The output shares input/image nodes, so their
arenas must outlive it. The current traversal freshens traversed lambda binders;
alpha-equivalent outputs need not have identical pointers and are not interned
by alpha comparison. Empty substitution returns the original pointer.

- [x] Reuse one readback traversal for evaluator closures and explicit term
  substitution; test simultaneous swaps, capture avoidance, shadowing, a
  40-level shared DAG and preservation of unreduced APP. Ordinary and
  ASan/UBSan checks pass.
- [ ] Certify substitution against typed contexts and connect it to dimensional
  restrictions; term substitution alone does not discharge these obligations.

Intern a constraint by its complete semantic inputs: rule, context, typed
operands, policy and relevant declaration identities. Attach diagnostic source
sites separately. Allocate the goal before registering dependencies so cycles
and forward producers do not depend on AST traversal order.

Each goal owns one current answer and its reverse dependencies. The queue stores
work, not another answer. A pending consumer subscribes to actual dependencies;
publication wakes affected consumers only. Provenance needed to justify an
answer is part of the answer's change detection. SCC handling must express the
recursive rule's obligations, not accept a cycle as its own proof.

- [x] September 8, after `9eadd0a`: replace the ready stack with one FIFO queue.
  The old LIFO scheduler reinserted unfinished reductions ahead of all older
  work. A regression with 256 typed FORCE/THUNK steps and an independent `@`
  request failed on the old implementation: the latter was still pending after
  16 transitions. All insertion, resumption and dependency wakeup sites now
  share constant-time `enqueue`; no second work list or acceptance state is
  added. Short requests made before/during long work finish while it is pending.
  Tests also cover duplicate requests, consumer wakeup, draining/restarting and
  cold bulk versus split results. Existing dependency tests retain their
  assertions but wait a bounded number of transitions for subscription rather
  than relying on LIFO ordering. Finite transitions give ready-work fairness;
  synchronous kernel traversals/allocation still prevent a wall-clock bound.
  This does not change runtime effect order, pure reduction or proof rules.
  Optimized check: 3.372 s (synthesis rebuilt); ASan/UBSan: 17.323 s (affected
  binaries rebuilt); Core/Identity/synthesis/IADT pass with a 512 KiB stack.
  Implementation `synthesis.c` +21/-23, `synthesis.h` +4/-0 (net +2);
  tests `synthesis.c` +73/-8; documentation separate. N2/N5 remain open.

Keep immutable derivations with explicit premises. Multiple derivations can
conclude one proposition; do not overwrite accepted proof records. A checked
occurrence can reference its accepted evidence directly. Introduce another Claim
store only if a concrete consumer needs a distinct acceptance identity.
Expected-type checks, diagnostics and artifact roots read these results rather
than reproducing their lifecycle.

Implemented input boundary (`pointer/typing.c`): immutable declared contexts
are interned by `(parent*, binder*, declared_type*)`. An occurrence is interned
by `(context*, core*, explicit_annotation*, typed_operands[])`. Typed operands
retain the declarations erased from shared Core children; they do not duplicate
Lambda/APP computation fields. Neither allocation constitutes accepted typing
evidence. Explicit annotations are immutable syntax inputs, not solver answers;
`::` must remain a subsequent check. Diagnostics are not part of these keys.

- [x] Persistent declared contexts and exact-pointer occurrence interning.
- [x] Test distinct lambda occurrences with identical erased Core but different
  body declarations, exact-key reuse after index growth, and invalid storage
  inputs; ordinary and ASan/UBSan builds pass.
- [ ] Validate occurrence scope/operand contracts in elaboration and implement
  synthesis with explicit derivations. Storage currently accepts unchecked
  inputs and must not be presented as a kernel checker.
- [ ] Connect typed occurrences to dimensional action and its classifier rules.

The new input store uses the existing arena and hash index. Context lookup is
currently a parent walk; no claim of constant-time binding lookup is made.
N0/N1 and N2 remain incomplete; these checks do not establish HOTT or surface
compatibility.

## 7. Compatibility Inventory

Inventory execution checkpoint: the new `tests/parse_files.c` runner read all
158 listed current-worktree programs. Initial coverage was 121 parsed / 37
syntax errors. Comparing failures with the existing reader identified missing
repeated `@\\` index markers and bare annotated lambdas (`x:A=>body`); after
implementing these, the result is 133 parsed / 25 syntax errors. Remaining
errors include graph-companion and import syntax plus old drafts. This is not
a conformance percentage: negative semantic fixtures may parse, and accepted
syntax trees still need semantic/precedence comparison. Existing examples
01--09 (eight files; no 08 file) are now permanent parser smoke tests and pass
with sanitizers, without modifying the examples.

`pointer/reader.c` now provides an allocation-free bounded lexer, separated
from name resolution and type decisions. Its tokens cover the current reader's
punctuation, identifier, Int64, plain/raw-delimited Text and C-comment forms.
Words including `return`, `perform` and `import` remain identifiers at this
layer. Text payloads borrow the input; no escaping or encoding conversion is
introduced. Explicit buffer length replaces the old NUL-terminated scan.

- [x] Lexical fixtures for indexed declarations, blocks, handler punctuation,
  contextual words, integer boundaries, delimited text, comments and truncated
  input. Ordinary and ASan/UBSan checks pass.
- [ ] Build the grammar and lowering on these tokens; compare the complete
  frozen fixture inventory. Lexer tests alone do not establish compatibility.

`pointer/syntax.c` now parses the initial annotated Lambda/application/Pi
fragment, qualified names, literals, quotation, and separate `::` checks into
source-owned syntax nodes. It does not resolve names, infer types, insert CBPV
coercions or evaluate expressions. Tests check left-associated application,
dependent binder retention and separation of expected-type checks from
definitions. Ordinary and ASan/UBSan checks pass. Grammar coverage is tracked
below; successful parsing is not a compatibility claim for elaboration.

Declaration syntax now accepts `@{...}` and `@\\index:A => {...}`. Outer
lambdas remain parameters; index lambdas sit inside the declaration marker.
Constructor classifiers are retained whole, in a source-order array, with `*`
and its index applications preserved rather than rewritten to a named recursive
reference. List/Vec/Acc, empty declarations, array growth and malformed syntax
fixtures pass ordinary and sanitizer checks. Name resolution must still reject
using the defining name as a recursive reference. Positivity, index typing,
declaration lowering and HOTT declaration action are not implemented yet.

Computation-block syntax now retains an ordered array of named assignments
(with optional declared annotations), unnamed expressions and lambda-exit
items. Postfix result selection remains explicit syntax; the parser does not
drop the suffix after a selected binding or resolve an exit target. Root-only
`{{...}}.name` uses a distinct definition-block node, including separate `::`
entries. Constructor and block arrays share one storage builder. Tests cover
ordering, nesting, quotation, selected results, exit preservation, root
restrictions and malformed delimiters; ordinary and sanitizer checks pass.
The pure block path is now connected to synthesis. Each active statement is a
shared synthesis job; a continuation frame retains its accepted input, domain
and extended context. Frames discharge through the same Lambda/APP/FOLD rules.
Already available syntactic values use `FOLD(RETURN v,K) = APP(K,v)` directly;
unknown returning computations retain FOLD. This permits dependent uses of
known values without inventing a type-level result of an unknown computation.

- [x] Named/unnamed statements, nested blocks, declared annotations as post-
  synthesis checks, lexical shadowing of outer names, quoted functions and
  selected-result cutoff. Selectors stop at a direct binding; later statements
  are parsed but not resolved or executed. A block-local name index rejects
  duplicate active bindings without repeatedly scanning previous statements.
  Tests reject unknown selectors and wrong annotations and execute eight
  representative block forms to the same expected RETURN value; additional
  application fixtures cover blocks used in function and argument positions.
  A dependent result after `B := A` substitutes the known type value into its
  classifier.
- [x] The corresponding `B := (\T : @ => T) A` case now requests the shared
  checked returned-value job when nondependent FOLD cannot close its
  continuation. Only after obtaining actual value evidence does ordinary APP
  instantiate the continuation's dependent classifier. The same close operation
  serves computation blocks and computed application operands. Formation is
  retained across suspension; multiple application frames close one scheduler
  step at a time instead of an unbounded loop. Tests cover one/two dependent
  block bindings, a computed dependent function argument, classifier agreement
  with the known-value case, and independent Core evaluation. An unknown
  function returning a type remains UNSUPPORTED; no type-level future result is
  fabricated. Nondependent computations still use FOLD without forced eager
  evaluation. The public result accessor exposes only completed jobs, not a
  partially closed application while its value dependency is pending.
- [ ] Lambda-exit target/barrier semantics, remaining computed type/argument
  cases, effect requests and handlers, and complete dependency/resource checks.
  EXIT remains UNSUPPORTED. This does not establish full block compatibility or
  effect-order correctness merely from the passing pure fixtures.

Elimination syntax now retains multiple clauses with unresolved label syntax,
positional binders or named selector aliases, and bodies. `#.return` is retained
as a qualified name, not dispatched in the parser. The same clause/list storage
serves ADT and effect syntax without claiming their typing rules are identical.
Tests cover unparenthesized lambda branch bodies, explicitly grouped nested
matches, operation aliases and return labels; ordinary and sanitizer checks
pass. Name/arity validation, head classification, scope checks and iota/fold
lowering remain unimplemented. The full existing nesting/precedence fixture
inventory still needs comparison before claiming grammar compatibility.

Import and graph-companion syntax is now retained without parser-side type
construction. `\\@f:G=>...` and `\\*f=>...` retain their markers; the latter has
no fabricated domain. `@name` in argument position is distinguished from a
clause head by following syntax. Top-level/root-block imports record only the
requested name. Tests and a sanitizer inventory run now parse 154/158 files.
The four remaining syntax failures are `12_append_assoc_draft.p` and the three
`stage*.p` files. Review against `reader.c` confirms the former uses untokenized
`==` syntax and the latter unannotated lambdas (stage1 also lacks a top-level
definition). `syntax_exclusions.tsv` records their exact diagnostic outcomes;
`syntax_inventory.sh` now verifies all 158 outcomes in `make check`, including
sanitizer runs. This does not claim equivalence of resulting syntax trees. No
negative fixture has been certified merely because it parsed. Companion
origin checking, import resolution, typed lowering and full precedence/scope
conformance remain outstanding.

This table is a starting inventory from the current reader, AST and test tree,
not a claim that the failed snapshot passes every row. N0 must enumerate exact
fixtures/options and distinguish old bugs from intended behavior.

| Area | Current evidence to read/reuse | Replacement acceptance |
| --- | --- | --- |
| names, Lambda, APP, annotations | `src/prototype/src/frontend/reader.c`, `include/a_program/frontend/ast.h`, examples 01-09 | accepted/rejected syntax and synthesized types; `::` cannot steer synthesis |
| ADT, parameters, indexed `@\\i:T => {...}` and `* i` | `test_explicit_index_family_surface.sh`, Vec/Acc fixtures | generic telescopes, nominal owners, correct rejection of recursive self-name syntax |
| dependent Pi, Match, recursive `*field` | `test_dependent_pi.sh`, `test_dependent_match_refinement.sh`, List induction fixture | dependent motive/refinement, single and multiple recursive fields |
| blocks, definitions, `&`, `!` | `test_computation_block_sequence.sh`, `test_definition_block.sh`, `test_cbpv_surface.sh` | exact binding/selection/exit/force policy, not just same final pure value |
| literals, intrinsic names, requests and multi-clause folds | reader `parse_elimination_head`, CBPV fixtures | ordinary operation application, aliases, `@#.return`, output carrier and effect checking |
| imports, exported nominal declarations, CLI/REPL | driver sources, `test_artifact_flow.sh`, reader session tests | cross-process identity/relocation; WHNF/NF commands and diagnostics |
| Acc, totality, generated function graphs | `test_if8_fuel_free_quicksort.sh`, `test_totality_evidence.sh`, function-graph tests | general indexed elimination; no Acc or QuickSort special primitive |
| Identity and higher supported fragments | `src/prototype/src/identity/`, HOTT/Identity tests | port supported rules with explicit premises; no inferred full HOTT claim |
| universes, resources, effect constraints | universe/resource tests and kernel sources | distinguish pending, rejected and proved; no empty-row/unknown conflation |
| checked artifacts and resumption | checker/container and compilation-image tests | retained evidence checked by ordinary acceptance rules, rejection of invalid references, resumable `.a` contract |

Paths abbreviated as `test_*.sh` refer to `src/prototype/tests/integration/`.
Legacy tests that assert enum numbers, old Core pretty-print tags or internal DB
layout are not language semantics. Replace those assertions with results, types,
effect traces, rejection reasons or retained-evidence validation. Record each disposition;
do not delete a failing semantic test to make the new implementation pass.

September 8 source-block correction after `b4eb821`: an unnamed, nonfinal
statement whose accepted computation is directly `PG_RETURN_INTRO` no longer
allocates a continuation binder and scope. Its producer is still synthesized
and checked before this decision. The ordinary RETURN rule, not a guessed
effect row or source syntax, authorizes discarding the returned value. Named
bindings, selected/final results and general computations retain their existing
paths. No normalization search is run to discover additional discardable terms.
Tests assert that `{ x; x; x; }` produces direct RETURN evidence, a discarded
application remains a FOLD, and an undefined discarded name is rejected. The
direct-RETURN regression fails before this change. Implementation C: +8/-0;
test C: +9/-0. This reduces generated scopes, not source LOC, and does not close
the outstanding effects, exit, image or typed-symmetry gates.
Normal and ASan/UBSan pointer checks pass.

September 8 endpoint-work checkpoint after `a84c7d2`: the synchronous
`pg_identity_face_endpoint` now drives the same resumable endpoint worker that
callers can advance explicitly. A fuel unit performs one descent or unwind
step. Zero fuel does not progress, intermediate endpoint evidence is not exposed
as the final result, and cancellation frees temporary frames without retracting
already accepted premises. There is no parallel extraction algorithm or new
evidence rule. The worker borrows the typing/classifier stores and is not an
image persistence format.

Tests split every traversal of the tested full 1-3D cube endpoints at every
step: final evidence and total traversal steps match the synchronous result.
They also cover pending cancellation, unsupported depth, sticky failure and
completion with zero additional fuel. Formation recovery and individual rule
applications remain synchronous inside a step; this is not a wall-clock bound
or completed budgeted kernel. Typed center symmetry and N2 remain open.
Implementation C: +86/-30; header: +14/-0; tests: +29/-0.
Normal and ASan/UBSan pointer checks and the 512 KiB-stack Identity test pass.

September 8 endpoint scheduling after `5a497d0`: endpoint derivation requests
now use the ordinary synthesis job table and ready queue. The key is the
accepted context, accepted formation and immutable canonical endpoint-selector
map, never a Core pointer alone. The selector fixes its first coordinate and
retains the outer directions to its right; it describes a depth, not the full
dimension of the input. Validation precedes interning, and queue execution
advances the existing endpoint worker once without rescanning the selector.
The selector must remain immutable and graph-lived; equal requests using the
same interned map share work. No new public language syntax or acceptance rule.

Tests cover repeated requests, zero fuel, split/bulk budgets, ordinary dependent
consumer wakeup, distinct formation evidence over the same Core, invalid inputs,
unsupported formations and destroying a store with an active worker. Results
are published only after completion. Primitive proof work inside each traversal
step remains synchronous. This connects boundary work to the shared solver;
it does not complete typed symmetry, image resumption or N2.
Implementation C: +58/-1; header: +11/-0; test C: +56/-0.
Normal and ASan/UBSan pointer checks pass.

September 8 shared substitution scheduling after `68cc0f3`: extracted the
existing IADT index-result builder into `substitution_state` and one shared
stepping path. The new `pg_synthesis_substitution` records source/destination
context proofs and independent image jobs in declaration order. The same worker
waits for each image, exposes it through the existing value-input protocol,
post-checks its dependent classifier through the existing pairing job, and
publishes only the completed substitution. IADT syntax still generates its own
image requests; it no longer owns a separate stepping algorithm. No expected
classifier is passed into an image producer and no new kernel rule is added.

Tests check shared requests, a pending second image dependent on the first,
split/bulk budgets, reindexing the resulting dependent variable, wrong image
order, wrong arity and the empty substitution. A square boundary template is
also instantiated by this shared queue and then reindexed to obtain its opposite
center formation without that center in the destination context. Face evidence
is currently supplied before this template request; this is not a fully
asynchronous higher symmetry operation. N2/N3 remain incomplete.
Implementation C: +68/-23; header: +7/-0; tests: +92/-0. Normal pointer checks pass.
ASan/UBSan pointer checks also pass.

### Resumable proper-face selection (September 8, after e4f948b)

- [x] Replace synchronous proper-face traversal with one resumable worker;
  the synchronous API drains that same worker. Retained-family validation,
  coordinate selection and endpoint traversal retain their progress.
- [x] Schedule immutable context/formation/face requests on the existing
  synthesis queue. Publish only complete evidence; cancellation releases the
  worker, and unsupported dimensions do not become rejected propositions.
- [x] After `457681b`, make the canonical endpoint API a restricted entry to
  the same face request. Remove the separate endpoint job role, state pointer,
  dispatch and disposal path. Identical inputs through either API share work;
  the endpoint traversal remains a subroutine of the face worker.
- [x] Feed pending face jobs directly into the square-template substitution
  test, removing its synchronous precomputation of the eight boundary images.
- [x] After `2379a8f`, let reindex consume pending substitution/proof producers
  on the same queue. Both the evidence and producer APIs use one job role.
  Once inputs complete, requests converge on their accepted evidence tuple
  before allocating reindex traversal state. The square-template test now
  schedules face selection, substitution and result formation without an
  external wait between stages; incompatible completed inputs are rejected.
- [x] After `1dbb73c`, allow face selection to consume a pending formation
  producer, converging on the same accepted-evidence job before traversal.
  The square-template test now selects a vertex of its reconstructed type
  without external staging. Non-formation results reject; allocation errors
  are not logical rejection. No new evidence rule or center action is added.
- [x] Check all 26 proper faces in three dimensions with split/bulk budgets,
  shared requests, unsupported permutations and pending-worker destruction.
- Fuel bounds traversal steps, not the cost of individual acceptance rules.
  Typed center symmetry, induced face permutations and N2 remain unfinished.
- Verification: pointer `check` and ASan/UBSan `check` passed. The existing
  geometric face tests still compare against independently generated binders;
  split/bulk tests additionally cover scheduling, not a new equality theorem.

Dependent-context obstruction, verified after `5606df8`:

- [x] Exercise the same substitution solver on full cubes of `A : Universe,
  x : A`. The one-dimensional telescope accepts all six images.
- [x] For the transposed square, retaining the original center while matching
  geometric proper faces accepts the first eight images, but rejects the ninth
  (the type's center); the full eighteen-image map is also rejected and never
  published. This is a regression against silently identifying orientations,
  not a claim that a typed transposition is impossible.
- Consequently, the next rule must supply a checked transposed type-center
  before transporting the dependent element telescope. A boundary-only map
  does not finish that task. Do not bypass this failure with nominal matching,
  a Core-pointer type lookup, or a new conversion equality. This test does not
  discharge center symmetry, reduction preservation or higher coherence.

Closed degenerate square check after `da8c039`: the square-template fixture
now uses `A : Universe(1)` so it also admits the closed value `Universe(0)`.
Starting from that value, two ordinary reflexivity introductions supply a
square. Its eight proper faces instantiate the transposed template through the
shared substitution/reindex solver. The reconstructed type is convertible to
the original closed square type, and ordinary `pg_prove_conversion` accepts
the existing center at that type. This supplies a positive, closed test beside
the abstract-center rejection above. It neither admits `S_swap(center)` nor
proves a general permutation/degeneracy equation; such an evaluator rule still
needs preservation under the accepted typed action and its instantiations.

After `5cdf62f`, post-checking independently produced term/type evidence is a
shared synthesis request. Surface `::` keeps its existing type exposure and
polarity coercions, then uses this same request; no expected type enters term
synthesis. Completed evidence pairs converge before conversion. The closed
square test now schedules boundary construction, reindex and center checking
without an external conversion loop, and verifies rejection of a formation
used as a term. This is scheduling of the existing conversion rule, not a new
symmetry introduction or a general higher-dimensional coherence proof.

After `df89baf`, synthesis comparisons share a conversion job keyed by the
ordered Core endpoint pair under the fixed pure policy. Classifier exposure,
application checks, constructor checks, family paths and `::` still accept
their own typed evidence; only conversion traversal is shared. Finished jobs
release comparison work and retain their status/certificate. The regression
checks that `x:A` and `y:A` have distinct accepted proofs but create only one
conversion job. This does not intern terms by conversion, merge occurrences,
or share runtime effect executions. Standalone conversion callers outside this
synthesis store retain their existing lifetime and ownership contract.
The follow-up after `4cd6e67` verifies negative sharing too: two independent
values checked against the same incompatible type add one comparison, expose
no result, and leave earlier accepted evidence unchanged. Zero fuel leaves both
checks pending; requesting the completed failure again consumes no solver work.
Normal pointer checks and the changed synthesis test under ASan/UBSan pass.

## 8. Program Image and Persistence

September 9 continuation after `1852c9a`: `syntax_io.c` transports unresolved
`pg_syntax` DAGs without reparsing source, constructing Core or running Solve.
All node kinds use the same fields and ordered item arrays; shared pointers,
token bytes, integer payloads and diagnostic locations survive relocation.
The existing dependency collector now supports explicit absent optional slots,
so left/right and item edges are visited once without rescanning item prefixes.
The wire format is `APGSYN` version 1, a component rather than a final `.a` file.

- [x] Preserve source-node sharing without alpha/conversion interning.
- [x] Round-trip all node-kind payloads, embedded NUL text and signed integers.
- [x] Feed a restored parsed definition block to the existing synthesis API.
- [x] Reject truncated prefixes without publishing outputs; detect cycles and
  transport a 40,000-level shared DAG with iterative traversal.
- [x] Check node arities, item contracts, marker positions and identifier byte
  extents before submitting restored syntax to synthesis.
- [ ] Complete lexical-position admissibility for externally supplied syntax;
  structural validation does not establish scope or type acceptance.
- [ ] Attach module names/scopes and retained producers through one program
  image, then integrate RECOMPUTE/CHECKPOINT policies and CLI resumption.

Continuation after `21e97c7`: closed source environments now use `source_io.c`
(`APGSRC` version 3). Read-only synthesis views expose expression inputs and
lexical environment dependencies; they do not copy progress or evidence.
Environment parents, source-name producers, module namespaces, explicit export
namespaces and supplied import scopes share one dependency table and one syntax
DAG. Loading calls the existing request/name/namespace/import APIs. The source
of truth remains those APIs' interned scopes and jobs, not the transport records.

- [x] Fresh-process module, source alias, export namespace and explicit-import
  reconstruction with repeated roots sharing the same producer.
- [x] Export does not advance Solve or mutate scopes/jobs/proofs; exporting
  before and after source acceptance yields identical RECOMPUTE bytes.
- [x] Use the same codec for single-source seed convenience functions; remove
  the separate `APGSEED` read/write implementation rather than keep two paths.
- [x] Reconstruct selected-definition producers, including external aliases,
  from the registration producer and original RHS syntax. Reserve the existing
  definition job before registration; do not create a second producer kind.
- [x] Reconstruct accepted/prepared/stored rule producers in closed external
  environments through the existing derivation codec and ordinary Solve.
  Unprepared non-source producers still fail explicitly rather than being
  replaced by a provisional Core-only value.
- [ ] Retain partial source-preparation and rule work as CHECKPOINT, and connect
  the common program image to CLI/file import selection.

Binder, definition-registration and handler-local scopes are not misrepresented
as closed environments. They require reconstruction from their source inputs.
The module fixture completes in 119 transitions for budgets 1 and 64 and reuses
the same accepted exported evidence across namespace and import paths. This
closes a RECOMPUTE fragment, not N5 or full source-language compatibility.
Normal `check`, eight source checks and six execution fixtures pass. Rebuilt
ASan/UBSan source-image and seed tests, including separate-process loading,
also pass. No Main promotion is justified by this partial image support.

Definition identity now uses the exact `(registration producer, RHS syntax)`
pointer tuple, not a scope containing allocated mutable registration state.
The registered lexical scope still governs body synthesis. A direct definition
reference is not rewritten into qualified module selection: the latter waits
for all module obligations and would change pending/rejected behavior.
Round-trip tests keep a valid definition available when a sibling is rejected
or cyclic, reject duplicate registration, and reject a reserved expression
absent from the completed registration. Source bytes remain unchanged before
and after Solve. This retains immutable inputs only, not completed proof work.

Continuation after `e1f0a40`: the rule exporter now also handles unexpanded
stored derivation inputs. Previously their export depended on Solve having
created an internal rule job, or on an accepted result; that made an untouched
loaded input less persistable than the original input file. The exporter now
copies the immutable premise DAG with the common DAG collector and includes
its effect definitions without running Solve. It does not serialize queue
positions, acceptance flags or effect approximations. Raw input sharing is
preserved and process-local comparison receipts remain forbidden.

- [x] Re-export stored rule inputs before their first Solve transition.
- [x] Verify identical bytes after partial expansion, effect convergence and
  acceptance; duplicate roots remain shared.
- [x] Preserve rejected rule inputs across Solve without claiming evidence.
- [x] Join these rule roots to source-name environments in the common image.

Validation: normal `check`, `check-examples` (8/8) and
`check-example-results` (six fixtures, two budgets) pass. The rebuilt
ASan/UBSan derivation-image suite also passes, including fresh-process loading
and progress-independent re-export. Full CHECKPOINT remains open.

Continuation after `4920059`: `APGSRC` version 3 joins lexical source inputs
and selected rule producers. A name can refer to an unaccepted rule producer;
its derivation inputs and effect definitions use the existing `APGDRV` codec
and one shared Core relocation table, including nominal declaration descriptors.
Source expressions retain their original syntax even after acceptance. Loading
restores rule requests, then lexical names; neither step accepts the proof.
`pg_program` owns the imported effect worker until synthesis is destroyed.

This self-contained image requires all imported effect contributions to be
known. `require_closed` on rule export checks the original workers are sealed;
it does not require convergence or a result. An extensible worker is rejected
before writing, because its missing future contributions cannot be guessed or
treated as empty. Standalone rule-image APIs may still export partial equation
inputs when their caller separately retains the remaining producers. General
CHECKPOINT must preserve those producers, not weaken this requirement.

- [x] Mixed source/rule roots, repeated references and an external type name
  resume in fresh stores after destroying the original program.
- [x] An unaccepted return-type rule computes its imported effect row normally;
  an invalid application rule remains rejected rather than becoming evidence.
- [x] Reject unsealed effect work; permit sealed but unsolved work.
- [x] Exercise nominal and operation-bearing external names through this whole
  image, beyond their existing standalone derivation transport tests.

Validation: normal `check`, eight source checks and six runtime fixtures with
both budgets pass. Rebuilt ASan/UBSan source-image and seed tests also pass.
These results do not establish retained-progress CHECKPOINT or full N5.

Continuation after `502a049`: a separate-process whole-image fixture exports
two structurally identical but nominally distinct ADTs, their constructor
values, and an operation function whose payload/response use the first ADT.
After loading, ordinary source references accept the matching value and reject
the other declaration's value under post-synthesis `::`. Classifier pointers
share the relocated declaration roots. Calling the operation through its
external name retains exactly one latent effect and the nominal result type.
Both one-step and bulk Solve schedules pass. This tests explicit lexical
exports of type/value/function proofs; it does not infer original constructor
spellings from a bare type proof or complete retained source preparation.
The expanded source-image suite passes in normal and rebuilt ASan/UBSan builds.

Continuation after `5ab371e`: `pointer-check` exposes the same input image via
`--save FILE.a` and `--load`. No second verifier is introduced. A zero-step
source invocation can persist the parsed unresolved program immediately:

```sh
src/prototype/pointer/.build/pointer-check --steps 0 --save program.a program.p
src/prototype/pointer/.build/pointer-check --load --nf main program.a
```

The first command returns the ordinary pending exit code 3 after a successful
save. `--save` currently stores RECOMPUTE inputs, not retained execution or
solver progress; saving before/after acceptance gives identical source bytes.
`--load` applies the image's stored thunk policy;
combining it with `--strict-thunks` is an error rather than silently changing
the stored input. Source parsing and image loading converge at ordinary Solve.
The fixed read limit is 1,000,000 entries/bytes according to the codec's separate
budgets. Host effects are not executed by this command.

- [x] Save unresolved source, load in another invocation and obtain the same NF.
- [x] Preserve stored definition policy and save rejected inputs without evidence.
- [x] Re-save loaded inputs with zero transitions; preserve RECOMPUTE bytes.
- [x] CLI multi-root selection without dropping other roots on save.
- [ ] CLI retention-policy selection, filesystem
  import resolution and retained-progress CHECKPOINT remain open.

Validation: normal `check`, eight source checks and six execution fixtures pass;
the rebuilt ASan/UBSan CLI suite passes the save/load and prior diagnostics tests.

Continuation after `f98072e`: `--load --root N` chooses a one-based image root
(default 1) for status and optional definition normalization. The image's full
root list remains the save input, in its original order, including duplicate,
pending and rejected roots. The existing shared Solve queue is unchanged;
selection is not a declaration that other roots are accepted or irrelevant.
Out-of-range, zero and source-mode root selection are diagnosed. An integration
fixture selects a valid nominal type, re-saves identical bytes, then verifies
the independently rejected root and all nominal/operation roots still exist.
Normal `check`, source/example execution checks and both rebuilt sanitizer CLI
suites pass. This adds no retained-progress CHECKPOINT claim.

### Source Allocation Identity Gate (After `0133db1`)

New failing acceptance command:

```sh
make -s -f src/prototype/pointer/Makefile check-image-origins
```

The fixture saves the same nominal declaration through both its source module
and retained type/value/operation evidence, then loads in a separate process.
The evidence roots share their relocated family correctly, but resynthesizing
the original source creates a different family. The test reports
`source and retained evidence split one nominal declaration` and fails.
It is part of `check-acceptance`, not silently treated as an expected success.
The existing disjoint-source/retained-evidence tests still pass; they did not
cover this overlap. No unsound acceptance is demonstrated by this test: the
failure is loss of sharing/generative identity and potential rejection of
previously compatible values.

Cause: `declaration_step` allocates Self binders; source schema construction
calls `pg_data_schema`, which allocates a fresh declaration. The rule codec
instead reconstructs an existing declaration and checks it with
`pg_data_schema_check`. Syntax and Core currently have separate relocation
tables without a source-allocation dependency connecting these two paths.
Structural schema interning is not a fix: distinct declarations must remain
nominally distinct. Reusing an accepted proof without checking its source is
also not a fix.

This is now a prerequisite for CHECKPOINT and general mixed-image import:

- [x] Reproduce shared-origin failure across processes without changing DefEq.
- [ ] Identify allocation-producing source inputs for Self, telescope binders
  and nominal declarations; preserve their exact producer identity rather than
  names, addresses in another process, or allocation counters.
- [ ] Retain these allocation dependencies in the program input graph and
  relocate their objects through the same Core table as stored derivations.
- [ ] Reconstruct source preparation using those inputs. Check regenerated
  schema premises against the retained declaration, including binder/context
  correspondence, rather than allocating an unrelated family or accepting a
  cached proof. A declaration's existence is still not its formation evidence.
- [ ] Reject mismatched source/schema associations and preserve two distinct
  sites with identical source shapes. Cover partial universe-candidate work.
- [ ] Pass `check-image-origins`, then integrate the allocation mechanism into
  retained partial source preparation before adding further image features.

First implementation step after `d36eba3`: lexical Lambda/Pi reservation now
accepts an optional binder allocation input through `pg_synthesis_binding_at`.
The existing `(scope, syntax)` producer remains canonical; an initial restored
binder can be supplied, but a conflicting later input is rejected without
replacing the binder or allocating another job. The ordinary entry delegates
to this same function. Binder creation no longer hides in generic job allocation.
A read-only binding-input view exposes the origin tuple independently of proof
completion. This adds no Core/job tag and grants no domain/context evidence.

- [x] Seed Lambda/Pi binders before telescope/expression synthesis; check both
  share the seed. Preserve default allocation when no seed is provided.
- [x] Reject replacement before and after completion; keep rejected attempts
  from changing the job count or original binder.
- [x] A seeded binder with an unresolved domain remains pending without proof.
- [ ] Connect binding inputs to source-scope transport, then cover declaration
  Self/universe candidates and schema allocation. The nominal-origin acceptance
  gate is still failing; this reservation API alone is not the image fix.
Validation: normal `check`, eight source checks, six runtime fixtures and the
rebuilt synthesis test (including pending seeded domains) pass. The separate
`check-image-origins` gate remains an outstanding requirement, not a pass.

Next step after `02a4466`: `pg_synthesis_data_schema_at` accepts a retained
nominal declaration before source schema preparation starts. The ordinary
schema entry delegates to it without an allocation. The prepared constructor
maps go through `pg_data_schema_check` when an allocation is supplied, retaining
the kernel's exact context, binder, field and result-map validation. No prior
formation proof is reused. Late/conflicting attachments do not replace work.

- [x] Recompute nullary schema inputs against a retained allocation and preserve
  its family identity; reject changed constructor counts.
- [x] Combine restored Lambda/Pi binders with a dependent constructor field
  telescope; preserve the declaration and reject a changed dependent field type.
- [ ] Restore source declaration Self/universe inputs and connect these APIs
  to program-image allocation dependencies. `check-image-origins` remains open.
Validation: normal `check`, eight source checks and six execution fixtures pass.
Re-running `check-image-origins` still reports the nominal split, as expected
until the new source allocation inputs are actually retained and relocated.

Next step after `107c313`: source declaration requests can receive an inert
nominal allocation through `pg_synthesis_declaration_at`. The existing source
job remains canonical. A shared attachment helper prevents schema/declaration
requests from replacing an allocation once work has started. Source inference
still starts with its ordinary Universe candidate; the retained Self binder is
used only when that candidate matches the stored Self classifier. A completed
source candidate must actually use the retained declaration to be accepted.
The stored classifier cannot force synthesis to choose a higher universe.

- [x] Recheck a zero-field source declaration using its original Self and
  nominal allocation, obtaining the same accepted formation.
- [x] Reject changed constructor input and an allocation whose Self universe
  is higher than the independently inferred source candidate.
- [ ] Restore constructor allocation dependencies across universe candidates
  and serialize the source/allocation associations. The mixed-image gate is
  not fixed merely by exposing these restoration APIs.
Validation: normal `check`, eight source checks, six execution fixtures and the
rebuilt synthesis test including higher-stored-universe rejection pass.

Next step after `135a1bc`: `pg_synthesis_telescope_at` reserves an ordered
binder suffix from an inert context. It flattens that suffix once and the
existing iterative telescope job consumes one binder per source binder;
repeated prefix walks and a second type-checking traversal are not introduced.
Domains still synthesize from source. Missing/extra binders reject, and source
schema checking still compares the resulting contexts and index maps exactly.
Retained schema inputs now install these reservations for both the index
telescope and every constructor field telescope.

- [x] Restore source declarations with recursive and dependent fields, including
  declarations whose independent Universe inference raises its candidate.
- [x] Restore indexed schema telescopes and constructor binders using the same
  allocation input, not an alpha-equivalence merge of nominal declarations.
- [x] Reject missing/extra index and field binders and changed dependent field
  domains. Tests no longer manually seed constructor binders before restoration.
- [ ] Persist and relocate the source/allocation origin association itself.
  `check-image-origins` remains open until this is connected to the file codec.

Validation for this step: `check`, `check-examples` (eight source fixtures),
and `check-example-results` (six execution fixtures at both budgets) passed.
After adding the arity rejection cases, `check` passed again. The dedicated
`check-image-origins` gate was rerun and still fails with the nominal split
diagnostic; these APIs alone do not satisfy N5 or whole-image acceptance.

Next step after `df0fc24`: definition-scope allocation now precedes registration.
`pg_synthesis_definition_scope` reconstructs the canonical scope from its parent
and definition syntax without spending Solve steps or accepting any definition.
All three registration callers use one factory. The scope inherits the nearest
registration producer as a derived dependency, so source jobs wait for name
registration without repeatedly walking their parent scopes. Nested registration
itself waits for its parent's registration; no scope-local acceptance flag is
introduced. Ordinary definition activation and whole-module checking remain
separate from allocating the scope.

`APGSRC4` adds a definition-scope environment record. Loading restores the parent
and definition syntax and calls the same factory; it does not replay name lookup
or import a mutable name index. Source roots in that scope can therefore be saved
before registration, including roots alongside invalid or cyclic definitions.

- [x] Preserve the distinction between a directly selected valid definition and
  whole-module rejection/pending status across source-image restoration.
- [x] Allocate shared definition scopes at zero Solve steps and reconstruct
  source roots that refer to their local names; reject duplicate definitions.
- [ ] Connect nominal declaration origin records to these reconstructed scopes
  and the shared declaration codec. `check-image-origins` is still required.

Validation: rebuilt `check`, `check-examples`, and `check-example-results` pass.
The mixed nominal-origin gate still reports the same declaration split; the new
environment record supplies its lexical prerequisite, not its completed fix.

Next step after `7597159`: `APGSRC5` links closed source declaration sites
`(scope, syntax)` to their ordinary formation inputs. Origin formation roots
share the existing derivation/Core codec with externally retained evidence.
Restoration attaches their inert declaration allocation before source Solve;
the source still infers its Universe, field domains and result maps normally.
The retained formation input is provenance, not a replacement source result.

The writer visits source declaration producers read-only and selects those in
the saved syntax and reconstructible lexical closure. Syntax reachability uses
the same child traversal as syntax transport. It does not export unrelated jobs
merely because they exist in the synthesis store. Restored origin producers
retain the unaccepted formation input even before Solve, allowing a second
save/load without dropping the shared declaration. No second Replay exists.

- [x] Close the original `check-image-origins` failure: original source and
  external evidence now refer to the same restored declaration, while D and E
  remain distinct despite identical constructor shape.
- [x] Save and reload the mixed image a second time before any Solve step;
  recheck source roots, external constructors and the operation signature.
- [x] Run `check`, `check-examples`, `check-example-results`, and
  `check-image-origins` successfully with the revised codec.
- [ ] Generalize lexical restoration to binder-bearing source scopes. A
  declaration origin requiring such a scope is still rejected by the writer,
  rather than silently losing identity. This remains a CHECKPOINT prerequisite.
- [ ] Complete pending allocation coverage, artifact retention policy and the
  other N0--N7 acceptance requirements. This closed-origin result is not full
  CHECKPOINT, full source compatibility or permission to promote to Main.

Next step after `89e5662`: `APGSRC6` reconstructs ordinary Lambda/Pi binder
scopes from their parent, source syntax and retained context-extension input.
The shared allocation-origin reference now covers both declarations and
binders. `pg_synthesis_binding_at` installs the relocated binder while the
ordinary source domain producer supplies its type; no stored context proof is
used as the source result. Origin discovery includes binder ancestors and
collects newly discovered scopes once, rather than rescanning all scopes.

- [x] Restore a datatype beneath two type-parameter binders, with the same
  declaration and context as separately retained formation evidence.
- [x] Save the loaded parameterized source again before Solve, reload it,
  and obtain the same checked formation through ordinary source synthesis.
- [x] `check`, `check-examples`, `check-example-results` and the mixed-origin
  gate pass. The extended `image_cli.sh` also passes: example 09's direct NF
  graph equals its saved/loaded NF graph (Solve step counts may differ).
- [ ] Persist arbitrary pending binder allocations without a retained complete
  context-extension input. Handler-local, IH and other derived scope origins
  remain outside this source-environment encoding; reject unsupported exports.
- [ ] Finish the remaining N0--N7 requirements before Main promotion. Passing
  a parameterized List round trip is not general IADT/Acc/IF8 completion.

Next step after `5ec2a25`: a standalone function exposed another origin gap.
`f := &(\\A:@ => \\x:A => x)` and separately retained evidence for f both
rechecked, but pointed to different Core lambdas because declaration-only
origin discovery omitted their binders. The mixed-image gate now includes this
case in separate processes and an additional save/load before Solve.

Origin discovery now visits source allocation producers: declarations plus
the binding jobs actually requested by source Lambda/Pi expressions, and
retained binding-origin inputs. It does not indiscriminately export every
internal telescope reservation. Existing scope records and the same
context-extension input codec restore ordinary function binders; no new wire
record, kernel rule, alpha merge or Replay path is introduced.

- [x] Reproduce and fix standalone function Core identity splitting.
- [x] Preserve it through an unsolved resave and verify that no evidence is
  accepted on loading alone.
- [x] Run `check`, `check-examples`, `check-example-results`, and the extended
  `check-image-origins` successfully.

Test correction: source snapshots before and after Solve need not have identical
bytes once newly allocated binders are retained as graph inputs. The former byte
comparisons in CLI/module tests now check the same NF, shared definition results,
and rejected roots after ordinary Solve. Source input immutability and read-only
export checks remain. This does not claim serialization-order canonicality or
complete pending-allocation coverage; both retention policy and remaining scope
origins still belong to unfinished N5 work.

Audit after `6d23f1c`: origin collection is too broad. The new standalone
function case passes, but saving `examples/07_add.p` fails after ordinary
checking succeeds (1497 steps). A Lambda in a Match branch has a generated
field/IH scope. `collect_origin` visits that allocation solely because its
syntax belongs to the selected source tree, then attempts to serialize an
unsupported intermediate scope. Source syntax reachability is necessary but
not sufficient to decide which completed allocations belong to a RECOMPUTE
image. Retaining every discovered binder is not the intended policy.

The extended `check-image-origins` now reproduces this failure, after the
nominal, parameterized and standalone-function origin checks succeed. The gate
depends explicitly on both fixture and CLI binaries. Ordinary source acceptance
of example 07 must not be confused with its failing image export.

Next correction:

- [ ] Collect object dependencies of the selected retained rule inputs through
  the existing derivation parameter packing and Core descriptor-child traversal.
  Share these traversal algorithms with transport; do not invent a second rule
  table or serialize to a temporary file merely to rediscover dependencies.
- [ ] Select source allocation origins by that object dependency closure as
  well as source/lexical membership. A source-only recomputable branch may be
  re-elaborated; it need not force its transient scope into the image.
- [ ] Add necessary lexical ancestor allocations for a retained origin through
  the same collector. Newly added input dependencies must also be covered;
  avoid repeated whole-store rescans and unconditional duplicate rule export.
- [ ] Keep explicit failure when an actually retained object requires an
  unsupported origin. Merely skipping all unsupported scopes would lose shared
  identity for source-plus-evidence images and is not an acceptable fix.
- [ ] Make the example 07 source/image NF comparison pass without weakening
  the existing nominal and standalone-function pointer-sharing assertions.

No runtime or kernel semantics changed in this audit. The original full goal
remains open; the newly exposed acceptance failure is not marked complete.

Next step after `e2f093e`: dependency selection now has shared transport
primitives rather than another hand-maintained interpretation of rule payloads.
`pg_derivation_input_terms` extracts the existing eight optional Core slots in
wire order, and the derivation writer uses it. `pg_graph_collect_objects` uses
the same `transport_dependency` callback as Core serialization, including
descriptor payloads and owned binders. Neither operation invokes Solve or
accepts evidence; the wire format is unchanged.

- [x] Replace inline derivation parameter-to-Core packing with the shared
  helper; existing descriptor and derivation round trips still exercise it.
- [x] Add collection tests for a deeply shared graph, duplicate roots, hidden
  descriptor dependencies, absent unrelated objects and cyclic input rejection.
- [ ] Wire this dependency closure into source-origin selection. These shared
  primitives alone do not fix example 07 export; the acceptance gate remains
  open. Keep the incremental closure/export work in the correction above rather
  than adding repeated whole-store scans or a temporary-file analysis pass.

Validation: rebuilt `check` passes, including the new object-collection checks.
`check-image-origins` still fails at example 07 saving after its identity-sharing
fixtures pass. No full-acceptance or Main-promotion claim is made.

Next step after `82fae58`: `pg_derivation_inputs_collect_objects` batches a
shared premise DAG and its effect definitions into the transport object walk.
It uses `pg_derivation_input_terms` and `pg_effect_inference_pack`, so there is
no second list of rule payload fields or effect dependency interpretation.
Tests exercise exported pending effect parameters and relocated ADT formation,
constructor and hidden Self-binder dependencies without accepting evidence.
Source-origin selection is not yet connected; example 07 remains an open gate.
Rebuilt `check` passes. The extended origin gate still fails at example 07
export; the dependency collector alone is not the end-to-end correction.

Correction after `ebe371d`: source-origin selection now starts from retained
rule/object dependencies, not all allocations seen while elaborating source.
Candidate allocation sites are indexed once by object pointer. The exporter
observes each newly reached rule header and effect worker; transport dependency
collection selects needed candidates and appends their provenance inputs and
lexical ancestors to the same root DAG. New roots extend the existing walk;
proofs, raw inputs and workers are not exported repeatedly. The fixed-root
export API delegates to this implementation and preserves duplicate root order.

Core dependency collection also retains its term DAG across additions. Both
serialization and dependency selection use the same descriptor-child walker.
A repeated root is an indexed lookup, not another traversal of its children.
Tests instrument a descriptor callback and verify that repeated additions do
not revisit it or grow the dependency graph.

- [x] Fix example 07 source-only saving without retaining its transient Match
  branch scopes; direct and restored NF graphs agree.
- [x] Keep nominal D/E distinction, source/evidence binder sharing, parameter
  scopes and unsolved resave checks passing in `check-image-origins`.
- [x] Run `check`, `check-examples`, and `check-example-results`; rebuilt
  `graph_io_test` additionally passes the persistent-walk assertion.
- [x] Keep explicit export failure for a genuinely required unsupported scope,
  rather than dropping required provenance on a scope reconstruction error.
- [ ] Complete remaining retained-result policy, pending allocation and derived
  scope coverage. This fixes the over-retention gate, not all N5/N0--N7 work.


Historical follow-up after `ac7afa0`: `APGSEED` version 1 embedded one syntax DAG
and the definition policy, replacing source-byte persistence in `seed.c`.
The common `APGSRC` path above now supersedes that intermediate framing.
`pg_program_allocate` shares store initialization with ordinary source creation.
Reading a seed validates structural shape and its definition-block root, then
schedules ordinary synthesis without invoking the parser or advancing Solve.
Syntactically invalid source is rejected by the writer rather than retained as
a source-text archive. The read limit counts graph/items/token bytes now.

This is single-source RECOMPUTE, not retained-progress CHECKPOINT or external
module registration. Synthetic all-kind payload fixtures test raw transport
only and are rejected by structural validation; parsed definition blocks and
the eight existing examples pass that validation before and after relocation.
Seed tests compare fresh-source and restored-graph Solve results and step counts,
assert no parser input on load, and reject decoded malformed application nodes.
Normal `check`, `check-examples`, `check-example-results` and the rebuilt
ASan/UBSan syntax-image and seed tests pass. N5 and the full reimplementation remain open.

In-memory entry after `3fd0b99`: `program.c` owns the existing graph, typing,
classifiers, evaluation store, synthesis store and copied source. Creation uses
the existing program parser and creates an unresolved root without advancing
the solver. Callers use the existing synthesis API directly; there is no new
progress state machine. Parser failures retain ordinary diagnostics, and
destruction releases stores before their graph. The new `program_test` checks
source-buffer independence, equal split/bulk solver progress, pending-cycle
destruction and initialization/parse errors. This is an ownership boundary,
not a second representation of terms or proofs. It does not implement `.a`
encoding/loading, host execution or full language acceptance; N5 stays open.
After `9206226`, `pg_program_source` adds another owned source to a selected
scope in the same stores. Initial creation uses this same path. A pending
provider and a client module share registration, solving and accepted export
evidence through the existing namespace API. Per-source parser diagnostics do
not overwrite the original root's diagnostic or accepted results. The program
test mutates the caller's source buffer before solving and checks a subsequent
parse failure. This does not add filesystem import discovery or an image loader.

CLI entry after `7a738da`:

```sh
make -f src/prototype/pointer/Makefile pointer-check
src/prototype/pointer/.build/pointer-check --steps 100000 source.p
```

`-` reads standard input; `--strict-thunks` selects the existing explicit policy.
The driver owns a `pg_program` and advances its existing solver once with the
specified budget. It prints status and consumed solver steps. Exit codes are
0 done, 1 rejected/parser error, 2 input/internal error, 3 pending, 4 unsupported.
Parsing and individual rules are not wall-clock bounded by `--steps`. This
command checks only the implemented source fragment; it does not run host
effects, save `.a`, restore work between processes or select `main` implicitly.
`tests/cli.sh` is part of pointer `check` and checks stdin, zero budget, cyclic
pending work, rejection and malformed options.

After `c885b7b`, explicit pure evaluation is available:

```sh
src/prototype/pointer/.build/pointer-check --nf main examples/07_add.p
src/prototype/pointer/.build/pointer-check --whnf main examples/07_add.p
```

The source root must first succeed. The selected stored thunk is forced once;
the remaining budget advances an ordinary typed normalization job. Pending
normalization never prints a completed result. The graph listing shares the
existing iterative dependency collector with graph transport and prints each
shared node once. Display IDs are local, not semantic identities, addresses or
new Core tags. Semantic objects are currently opaque labels, not source-level
constructor readback. This is not full REPL/readback or host-effect parity.
Tests distinguish WHNF from NF beneath Lambda, check exact combined fuel
boundaries, missing/duplicate selections, and print a depth-40 duplicated DAG
in 43 lines without changing the graph or exponentially expanding its tree.
Verification: optimized component checks, source examples and example results
passed; ASan/UBSan CLI and graph I/O checks passed. Full acceptance still stops
at open-family. Delta: implementation +81/-3, prototype build +2/-2, tests +29;
documentation separate. Printed semantic-object labels are diagnostic only.

One in-memory program owns graph roots, typed occurrences, declarations and work
results. Parsing/lowering creates its initial unresolved state; bounded solving
advances it. Execution is an explicit request using the same computation engine
with a host environment. A parsed graph is not permission to run unchecked code.

`.a` uses section-local wire references reconstructed into pointers on load.
Never serialize addresses or function pointers. Resolve semantic descriptors by
versioned builtin names or declaration references and validate their payloads.
Support recursive declaration graphs through allocate-then-link-then-validate.

Writer retention options: RECOMPUTE stores sufficient immutable input; CHECKPOINT
adds validated progress and evidence. They use the same loader and solver. Omit
work queues and historical snapshots; rebuild acceleration indexes. Do not copy
the failed seed/current/handoff structs into the new implementation. A small
round-trip prototype must demonstrate which roots suffice before freezing wire
format. Legacy `.apo/.ao` import, if retained, belongs in a conversion tool, not
the core evaluator. Exact old wire compatibility is not promised by this plan.

### September 8 decision: no independent Replay engine

September 13 continuation after `32881bb`: CLI `--retain-reductions` now selects
raw reduction retention for batch `--save` and REPL `:save`. Without it, saving
still chooses RECOMPUTE and discards reduction records. Both modes preserve
source inputs and obligations through the same source-image writer. Loading
does not accept or install the retained results, and ordinary source Solve
still recomputes: this option is not full CHECKPOINT or a speedup claim.

- [x] `pg_reduction_archive_snapshot` collects completed WHNF/NF receipts and
  unfinished NF phase roots from the existing work indexes, plus imported raw
  records. It creates no evaluator, proof, effect execution or progress flag.
  Temporary root storage is released after publishing or failing to save.
- [x] The read-only `pg_reduction_find` shares the evaluator's exact lookup by
  input/policy/mode. A completed local result replaces an older raw root at
  that key in the new snapshot; no old claim is accepted by this operation.
  Pointer-identical roots are emitted once. Invalid or different old endpoints
  are not used to change the current result. A pending job is not a receipt.
- [x] Test inert imported-record resaves, default discard, zero-step images,
  batch/REPL agreement, standalone record checking and List NF agreement.
  Snapshot tests preserve pending steps and phases, distinguish beta/pure
  policies, and check repeated exact-key snapshots without new computation.
- [ ] Full resynthesis can still allocate new auxiliary classifier binders.
  `03_main.p --nf main` followed by repeated load/Solve/retain-save produces
  19/24/29 receipt roots. A debugger inspection finds five unmatched WHNF
  inputs containing binder-bearing classifier spines or Lambda bodies.
  Exact-key replacement removes duplicate answers for the same input; it must
  not merge these distinct graphs by alpha/WHNF equality. Preserve/reuse their
  producing allocations or checked work before claiming bounded repeated
  CHECKPOINT resumption. Inert zero-Solve resaves preserve root counts.

Verification: normal `check`, handler nesting and 4,067 boundary snapshots,
758 module snapshots, image origins, all eight 01-09 source examples and six
runtime-result fixtures pass. Rebuilt ASan/UBSan image-CLI and Identity/image
suites pass. Delta from `32881bb`: implementation +99/-12 (net +87), tests
+88/-0; documentation separate. Source/root acceptance rules are unchanged.

The concurrent open-family audit does not add a kernel rule. The existing
computation judgment and empty row supply no stable-result premise. In
particular, labelling an arbitrary computation as a type would escape through
`pg_prove_type_value`; treating divergence as an empty suspended type also
needs substitution and higher-action semantics, not a default implementation.
References rechecked on September 13: [Harper, Dependent Type Theory for
Programming and Proving, July 2026](https://www.cs.cmu.edu/~rwh/courses/atpl/pdfs/dependency.pdf)
for functionality/substitution obligations, and [Pedrot and Tabareau, The Fire
Triangle](https://www.xn--pdrot-bsa.fr/articles/dcbpv.pdf) for the additional
structure needed to combine dependency and effects. Neither directly proves
the proposed A Program suspended-decoding rule. The positive open-family gate
remains required and failing; preservation work does not replace it.

September 13 continuation after `46e246f`: identity substitution and binder audit.

- [x] Drop the identity prefix of a simultaneous substitution during ordinary
  initialization. An entirely identity map now returns the exact input with
  zero traversal, rather than copying Lambda binders. Keep identities after a
  nonidentity entry: in `[x := y, x := x]`, the last entry must shadow the first.
  No alpha comparison, normalization, new cache or Core tag is introduced.
- [x] Check exact Core/classifier/annotation reuse through typed reindex while
  retaining its `PG_REINDEX` evidence and both premises. Test capture avoidance,
  later-entry shadowing and two image round trips with the original graph
  destroyed. The existing readback format handles the empty environment.
- [x] Diagnose the resulting `data_cases` test failure before modifying it.
  A debugger found 45 remaining transitions in an earlier declaration's
  constructor wrapper, through CONSTRUCTOR_SCOPE/SUBSTITUTION/PAIR/REINDEX,
  not the two completed FAMILY_ACTION roots. Drain and assert quiescence of
  this setup before measuring action reuse; retain the no-additional-steps
  assertion after the actions. Faster root completion is not whole-queue
  completion.
- [ ] Nonidentity substitution remains the next allocation/reuse boundary.
  A debugger stops at `eval.c:reify_advance` from `pg_prove_application`, via
  `pg_prove_derivation` and ordinary `derivation_step`: accepted APP construction
  performs fresh capture-avoiding codomain substitution. Separately,
  `classifier_structure_step` performs that operation for provisional APP
  structure. The fresh input key cannot reuse an older WHNF receipt merely
  because the printed Lambda shapes agree. Unify/reuse justified construction
  work and preserve its allocation origins, rather than adding alpha interning
  or unconditionally reusing a binder that an image can capture.

The repeated `03_main.p` retain-save probe still reports 19/24/29 roots;
initial NF/source Solve takes 428 transitions and each reload takes 521.
This change makes identity reindex exact, not nonidentity checkpoint reuse.
Normal `check`, handler/module boundary suites, image origins, eight source
examples and six runtime fixtures pass. The required open-family gate remains
1/4 (closed 176; unsupported 210/281/324 transitions). N0--N7 stay incomplete;
no Main promotion or push. Implementation delta: +9/-1, tests +60/-0;
documentation counted separately. ASan/UBSan Core, evaluation-image and full
synthesis suites pass, including the dimension-3 action cases.

September 13 continuation after `ba8f3f3`: share nonidentity substitution work.

- [x] Add exact-input request indexing around the existing substitution machine
  in `eval.c`. The key is the input Term and ordered binder/image pointers after
  the existing identity-prefix/semantic-reference simplifications. Pending work
  and its capture-avoiding binder allocation are shared, not just final answers.
  No alpha/WHNF interning, new reduction rule or typing decision is involved.
- [x] The typing owner holds this pure-work store separately from evidence.
  APP construction, Pi instantiation, context-map checking/reindex/lifting and
  provisional classifier/effect substitution use it. Source structure is still
  not accepted evidence: the normal rule checks all typed premises afterwards.
  Contexts and typed occurrences retain their separate identities over Core.
- [x] Completed cached jobs retain the original input environment and result,
  releasing traversal indexes and temporary arenas. The existing readback codec
  saves both pending jobs and compact completed roots without a wire change.
  This is component transport, not importing trusted substitution conclusions.
- [x] Regressions cover exact provisional/accepted APP classifier agreement,
  partially shared work, different images/orders/owners, alpha-equivalent but
  distinct inputs, shadowing, capture, cancellation and result lifetime. Every
  cut of a shared substitution survives source/store destruction and resumes
  with exactly the remaining transitions. Shared reindex consumers need not
  charge the same steps twice; independent split-fuel tests remain unchanged.
- [ ] Source `.a` does not yet retain this store's allocation provenance. The
  repeated `03_main.p` probe improves from 19/24/29 to 10/11/12 reduction roots,
  but still adds one per reload. A debugger identifies an unmatched Pi-shaped
  classifier WHNF input containing a generated binder. Preserve/check the
  construction behind that input; do not merge it with an alpha-equal result
  or overwrite a result already used by accepted premises. General CHECKPOINT
  retention and the open-family gate remain required, not deferred out of scope.

Measured against a detached `ba8f3f3` build, both using the same default `-O2`
flags. Three fresh `synthesis_test` processes per revision, exit status checked;
wall time from `time.monotonic`, peak RSS per child from `wait4`:

| Fixture | Before | Shared substitution |
| --- | ---: | ---: |
| Full synthesis suite, seconds (3 samples) | 4.7658 / 4.6865 / 4.7224 | 0.1234 / 0.1342 / 0.1222 |
| Peak RSS KiB (3 samples) | 848464 / 848652 / 847984 | 68728 / 68456 / 68292 |
| 3D one-argument action, Solve transitions | 5336585 | 173559 |
| 3D two-argument action, Solve transitions | 23825756 | 378530 |
| `03_main.p`, source plus NF / reload transitions | 428 / 521 | 346 / 439 |

The action fixtures retain dimensions 1--3, both arities, chunk sizes 1/64 and
their result/type checks. Their regression budget is now one million transitions
per fixture, not a language limit. This measurement is not a whole-compiler,
IF8 or legacy/new implementation speedup claim.

Verification: normal `check`, 4067 handler and 758 module boundary snapshots,
image origins, eight 01--09 source examples and six runtime fixtures pass.
ASan/UBSan Core, evaluation-image, full synthesis, source-image and image-CLI
suites pass. Open-family remains 1/4 (176, unsupported 210/281/324 steps).
Delta from `ba8f3f3`: implementation +160/-35 (net +125), tests +162/-2
(net +160), prototype build +2/-2; documentation separate.
No Main promotion or push; N0--N7 completion remains unproven.

Continuation audit after `2f59743`, following the renewed Replay question:

- `pg_derivations_read` restores unaccepted rule inputs only. It neither
  evaluates terms nor installs accepted proof flags.
- `synthesis.c:derivation_step` schedules premises on the ordinary Solve queue
  and calls `pg_prove_derivation`, whose dispatch invokes the same named kernel
  constructors as source processing. Keep this shared rule implementation;
  do not introduce an artifact-specific copy of the typing semantics.
- Current conversion/normalization records contain endpoints, not retained
  reduction certificates. Loading them deliberately recomputes conversion or
  normalization through ordinary jobs. This is the recomputation variant,
  **not** completion of the retained-intermediate-results variant of `.a`.
- [ ] For retained-result images, specify relocatable evidence for the pure
  reduction steps and validate it with the common reduction rules. Neither a
  saved success flag nor a pair of endpoints establishes a reduction. Account
  for validation cost separately from search/reduction cost; do not promise
  zero work when loading an untrusted image.
- [ ] Expose the retained/discarded intermediate-work policy in complete
  program images and their CLI options. Omitted work may be recomputed; valid
  retained work must not trigger independent proof search merely because its
  origin was an image rather than source. Complete program images remain open.
- Effect requests are descriptions, not reusable receipts for host execution.
  Reloading a program must not suppress a requested print because an earlier
  run printed it. Pure calculation reuse and effect execution are distinct.

This audit does not change the priority of recursive datatype admission above.
It records the actual limitation of the existing codec, rather than adding a
second Replay subsystem to address it.

Raw Core section prototype (`graph_io.c`, not accepted evidence):

- [x] Serialize reachable Lambda/Application/Reference DAGs in dependency
  order using file-local object/term references. Restore plain binders freshly
  and re-intern exact pointer structures; shared roots/subgraphs remain shared,
  alpha-equivalent distinct binders stay distinct, and redexes stay unevaluated.
  Collection is iterative and indexed, rejecting cyclic raw Term structures.
- [x] Require explicit versioned descriptor naming/resolution for semantic
  objects and owned binders. Never downgrade a cube-owned binder to a plain
  binder. Unknown descriptors, wrong resolved object kinds, forward term
  references, truncated records, trailing bytes and exceeded input limits fail.
  Owners remain responsible for descriptor meaning and injective naming;
  this codec does not certify an arbitrary resolver or decode IADT payloads.
- [x] After `9d5a2cd`, enforce injective object relocation within each Core
  section: different saved object records may not resolve to one pointer.
  Previously the reader checked kind only, allowing a faulty resolver to
  collapse distinct semantic references. Reuse `pg_dag` as a temporary pointer
  index; no quadratic scan, evaluation, alpha merging or permanent authority
  is added. The two-descriptor regression accepts distinct resolutions and
  rejects a same-kind collision without publishing output roots. Descriptor
  semantics still belong to the owner; injectivity alone cannot verify them.
  Optimized and ASan/UBSan graph tests and full pointer `make check` passed.
- [x] Share little-endian integer encoding with the seed codec through
  `wire.c`; the existing seed bytes remain unchanged.
  Complete pointer `make check`, ASan/UBSan graph/seed tests and separate-process
  seed tests passed. Graph coverage includes a ten-thousand-level shared DAG,
  every truncated prefix of the small fixture, and a forward/self reference.
- [ ] Integrate these raw roots with context/occurrence/declaration and
  evidence records and ordinary acceptance. No typing status is transported
  by this Core codec. Recursive declaration allocation/linking and complete
  `.a` program-root persistence remain unimplemented.
- [x] After `0e4670e`, exercise relocated Core through ordinary acceptance in
  separate writer/reader processes (`tests/graph_acceptance.c` and `.sh`).
  In the reader's checked context `A : U, B : U`, the identical relocated
  `lambda x. RETURN x` Core receives distinct derivations for `Pi A (F A)`
  and `Pi B (F B)`. Typed occurrences/classifiers remain distinct; repeated
  acceptance of identical premises reuses evidence. Swapping the two bodies'
  context premises is rejected, as is looking up x before its declaration.
  Descriptor resolution uses the fixed RETURN operation, not host addresses.
  This is an integration regression, not a generic derivation loader: the
  declarations and rule applications are fixture-supplied, not serialized.
  Full pointer `make check` and ASan/UBSan separate-process acceptance passed.
  N5 remains open; no independent Replay implementation has been introduced.

Context declaration transport after `d8ffe86` (`context_io.c`):

- [x] Store a shared parent-first telescope forest and extra Core roots in one
  container, using the existing Core codec for every binder and declared type.
  Contexts are not encoded as executable Lambdas and do not carry accepted
  flags. File-local parent references preserve shared prefixes; empty contexts
  use zero. An iterative indexed traversal detects parent cycles.
- [x] Restore through `pg_context_bind`, retaining exact sharing with the
  relocated Core roots. The same binder may appear in distinct alternative
  contexts without merging their declared types. Declaration validity remains
  the ordinary evidence rules' responsibility: loading produces zero proofs.
- [x] Extend the separate-process acceptance test to load the declarations,
  verify their exact reuse by ordinary context formation and reject crossed
  Lambda premises. Full pointer `make check` and ASan/UBSan pass, including
  every truncated prefix of the fixture, an invalid parent reference and a
  ten-thousand-declaration telescope. Failed loads leave outputs unpublished;
  arena allocations may remain until graph destruction.
- [ ] Persist typed occurrence operands, accepted premise DAGs, pending jobs
  and recursive semantic descriptor payloads. Context transport does not
  implement these or complete the `.a` format. N5 remains open.

Occurrence input transport after `a11577d` (`occurrence_io.c`):

- [x] Store the ordered operand DAG, declaration contexts, Core and optional
  annotations using the existing context/Core codecs. Reconstruct through
  `pg_occurrence`, not through a Core-to-classifier lookup. Duplicate roots and
  operands remain shared; distinct annotations/contexts over the same Core
  remain distinct occurrences. Missing annotation is not an inferred answer.
- [x] Use iterative indexed collection, reject operand cycles and forward
  references, and bound operand edges in addition to record counts on input.
  Test separate processes, ten-thousand-level shared operand DAGs, truncated
  prefixes and invalid annotation flags. No source acceptance or evidence
  validity follows from restoring this graph; the proof index stays empty.
  Full pointer `make check` and ASan/UBSan occurrence transport passed; empty
  input and an explicit self/forward operand reference are also covered.
- [ ] Connect the stored input roots to the program image and ordinary solver;
  accepted derivation records, pending work and recursive descriptor payloads
  are not yet persisted. This component does not close N5 or replace N2/N3.

Ordinary derivation reconstruction after `6471af6` (`derivation.c`):

- [x] Add a call adapter for the existing `pg_prove_*` constructors, not a
  second set of typing rules. It dispatches a rule, accepted premises and its
  non-premise arguments (binder, level, direction or local receipt). No caller
  supplies an authoritative conclusion; the existing rule computes it.
- [x] Recover those arguments as a borrowed view of accepted evidence, without
  storing duplicate mutable recipes in proofs. Reconstructed applications must
  retain the exact claimed rule and ordered premises, including intermediate
  Identity formation/endpoint premises; matching endpoint Core alone is not
  sufficient. A loader must still compare the computed conclusion with the
  record's declared conclusion before publishing that root.
- [x] Check representative Context/Pi/CBPV/conversion/substitution and Identity
  family/action/transport/lift derivations, invalid arity/null premises and the
  wrong directional premise with coincident endpoint types. The separate-process
  relocated Lambda test now constructs its new evidence through this adapter.
  Full pointer `make check`, ASan/UBSan Core and graph-acceptance tests passed.
- [ ] Encode the DAG and connect loading to this adapter. Conversion and
  normalization parameters still borrow genuine local receipts; no bytes or
  accepted flag can manufacture one. Recompute them with ordinary work where
  necessary. Shared suspended loading, complete conclusion matching and `.a`
  integration remain open. No CHECKPOINT completion is claimed here.

Shared dependency collection after `f7f0d49` (`dag.c`):

- [x] Replace the Core, Context and occurrence codecs' three independent
  pointer-indexed DFS implementations with one iterative dependency-order
  collector. Clients enumerate only their own dependencies. Object discovery
  remains separate from term dependencies; no contexts become Core terms and
  no traversal establishes semantic equality or accepts a proof.
- [x] Preserve postorder IDs and sharing. Re-adding a completed root performs
  no dependency callbacks. Cycles, missing children and callback failures
  invalidate the temporary collector. A diamond fixture checks exactly one
  callback per edge plus one completion per node, despite repeated roots.
- [x] Full pointer `make check` and ASan/UBSan Core/graph/context/occurrence
  tests passed. Before/after writer binaries produce byte-identical Context
  and occurrence fixtures (including the nested Core section). Existing deep
  DAG and malformed-input tests remain enabled.
- [x] Use this same collection path for the derivation wire section described
  below instead of introducing a fourth traversal.

Classifier descriptor transport after `499cabb`:

- [x] After `57aefbd`, move fixed RETURN/THUNK/FORCE/FOLD descriptor names
  into their computation owner; acceptance and derivation fixtures no longer
  maintain private operation-name tables. Extend the separate-process derivation
  fixture with a checked zero-clause FOLD and its directed WHNF obligation.
  The reader retains the FOLD premise and obtains the same result as its source
  computation through ordinary Solve. Twelve roots plus source consumers take
  363 transitions for either scheduling chunk. Optimized acceptance/derivation
  tests and ASan/UBSan derivation tests passed. This covers the fixed pure
  operator, not general handler clauses, effect requests or completed `.a` roots.
- [x] The classifier owner supplies canonical versioned names and resolves its
  own Universe/Pi/F/U references. Universe levels are parsed without truncation;
  signs, whitespace, leading-zero aliases, overflow, unknown versions and
  trailing content fail before allocation. No Core tag or global classifier
  lookup is introduced. Callers supply name storage, so graph transport needs
  neither persistent name caches nor string fields in every universe object.
- [x] The derivation transport fixture uses this owner API instead of naming
  only Universe 0/1. Additional graph round trips cover dependent Pi binders,
  F/U spines and the full uint64 level representation in a distinct arena.
  Structural transport of a level is not a universe-formation proof; ordinary
  evidence rules continue to decide admissibility.
  Optimized and ASan/UBSan derivation transport tests passed with these cases.
- [x] The Identity owner names its fixed Act, left/right transport and
  left/right lifting references. Versioned names restore the existing pure
  operators, not a witness or a replacement reduction equation. Graph tests
  round-trip all five references, retaining argument sharing and direction.
  The derivation fixture now has eleven roots, adding reflexivity and both
  directional fields. Ordinary Solve takes 227 transitions with either chunk
  size 1 or 64. Changing only a transport direction is rejected even when the
  endpoint types coincide: its retained directional premise must still match.
  Optimized and ASan/UBSan separate-process runs passed. This tests the existing
  Identity rules' persistence, not new general higher coherence or fibrancy.
- [x] After `4801800`, give raw symmetry operators owner-controlled versioned
  names containing their complete ordered axis list. Resolve through the same
  exact descriptor interner used by construction; preserve dimension and fixed
  prefixes rather than reducing identities on load. The caller owns the name
  buffer; descriptors gain no persistent string cache. Reject duplicate or
  out-of-range axes, numeric aliases, overflow and unknown versions before
  interning. Cross-arena graph tests cover all six cubic permutations, zero-
  and one-dimensional identities, exact reconstruction through `pg_symmetry`,
  shared relocated arguments and explicit post-load identity reduction.
  Full pointer `make check` and ASan/UBSan graph transport tests pass. The
  examples/inventory portion still checks parsing, not semantic parity.
  This transports raw Core
  references only; it neither introduces nor verifies a typed central symmetry.
- [ ] Owned dimension binders, typed general symmetry, recursive IADT descriptors
  and module provenance still need their corresponding owner transport. These
  classifier names alone do not complete the `.a` descriptor format.

Derivation input persistence after `48d2923` (`derivation_io.c`):

- [x] Serialize shared accepted derivations as rule applications, their ordered
  premise DAG and non-premise arguments. Use `pg_dag` and the existing Core
  codec for binder/endpoint relocation. There is no authoritative conclusion
  copy: applying the ordinary rules computes each conclusion. A separate export
  declaration, when added, must still be checked against that result.
- [x] Read into immutable, unaccepted `pg_derivation_input` nodes, not proof
  records. Save conversion and directed normalization endpoints as obligations;
  restore neither receipt pointers nor accepted flags. No evaluation occurs
  during input loading. Unknown rules/directions, forward premises, malformed
  endpoint references and truncated records are rejected structurally.
- [x] A separate-process fixture reconstructs distinct typed identities sharing
  Core using the existing constructors. It explicitly recomputes a conversion
  and FORCE/THUNK normalization using ordinary work before supplying genuine
  local receipts. Without those receipts, the rule adapter rejects acceptance.
  Shared proof roots survive; every truncated prefix and an unknown rule are
  tested. This fixture is not a general resumed-load scheduler.
  Pointer `make check` passed; the final conversion-extended fixture also passed
  optimized and ASan/UBSan separate-process runs.
- [x] Connect restored inputs to ordinary `pg_synthesis_derivation` jobs. Job
  identity is the immutable input pointer; ordered premises use the existing
  dependency/wakeup mechanism. No separate replay queue or evaluator is added.
  Conversion uses shared conversion jobs; directed normalization uses the same
  WHNF/NF work as source synthesis. Stored endpoints are obligations, not
  receipts: structural alpha comparison checks them against locally recovered
  results before the ordinary derivation constructor accepts anything.
- [x] After `97b8469`, deduplicate serialized derivation Core references with
  the existing temporary pointer-keyed DAG index. Previously every non-null
  binder or comparison endpoint appended another nested Core root, even when
  it had already been referenced by another rule. Allocate the final root
  array from the unique count; retain ordered rule premises and public roots
  unchanged. This is exact pointer indexing, not alpha/normal-form interning
  or evidence merging, and requires no wire format change. A regression reads
  the nested root table, checks uniqueness and verifies that it is smaller
  than the number of references. Optimized and ASan/UBSan separate-process
  tests pass; chunk sizes 1 and 64 still take 363 Solve transitions. This is
  component storage cleanup, not completion of CHECKPOINT or typed symmetry.
- [x] Retain the producing WHNF/NF mode in normalization receipts and the
  experimental derivation section (version byte 1). Do not guess the mode by
  trying different evaluators. A separate-process six-root test covers duplicate
  roots, distinct typed identities sharing Core, conversion, WHNF, and NF below
  Lambda. Chunk sizes 1 and 64 both finish in 175 scheduler transitions; zero
  fuel accepts nothing. Wrong saved targets and a WHNF mode substituted for an
  NF result are rejected. The former fixture-specific proof loop is removed.
  Full pointer `make check` and ASan/UBSan Core, synthesis and separate-process
  derivation tests passed. Syntax inventory success remains parsing evidence,
  not evidence of full semantic compatibility with the old implementation.
- [ ] Complete module/export roots, dependent/higher descriptor relocation and
  source/image integration. Scheduler transitions are bounded, but synchronous
  primitive evidence constructors are not yet all incrementally budgeted.
  This is not yet a complete CHECKPOINT or N5 acceptance. The tests above do not
  establish general higher Identity support or full source-program acceptance.
- [x] After `9179c24`, exercise the restored producer together with source
  consumers in the ordinary queue. Register a pending closed reflexivity
  derivation through `pg_synthesis_name_job`; parsing `copy := loaded;` waits
  and returns its checked evidence. `copy := loaded :: @;` independently fails
  its post-synthesis expectation without changing the producer's result.
  A source reference to an invalid saved normalization fails with its producer
  and receives no evidence. With both initial consumers, chunk sizes 1 and 64
  take 316 transitions. Source root creation proves the empty context, but
  zero fuel neither advances nor accepts any of the imported derivations.
  This integration uses existing APIs, not a new export-acceptance mechanism.
  Optimized and ASan/UBSan separate-process derivation tests passed.
  Persistent module bindings and full `.a` roots are still required.

Input-only round-trip prototype (`seed.c`, not the final `.a` wire format):

- [x] Store a single immutable source and explicit definition policy in a
  versioned, little-endian input capsule. No addresses or accepted-state flags.
  `pg_seed_read` creates an ordinary unresolved `pg_program`; parsing and Solve
  use the existing implementations. The writer takes input, not a purported
  snapshot of a mutated program. Parse caches and all solver work are omitted.
- [x] Exercise separate writer/reader processes, split versus bulk solving,
  exact wire policy/length, truncation, unknown version/policy, trailing data,
  caller input limits and ordinary syntax-error diagnostics.
  Complete pointer `make check`, ASan/UBSan `seed_test`, and the separate-process
  seed test under the same sanitizers passed.
- [ ] Extend image roots to external module registrations and descriptor
  provenance, Core/occurrence/evidence references and retained results. The
  single-source capsule cannot stand in for these roots or CHECKPOINT.
  It is deliberately named `.seed` in tests, not advertised as completed `.a`
  persistence. N5 and the full rewrite acceptance gates remain open.

RECOMPUTE and CHECKPOINT differ in retention, not in language semantics or
acceptance rules. Loading restores graph references and establishes retained
evidence before publishing solved results to the ordinary solver. It does not
re-enact the producer's search order. Pending is not rejected, and retained is
not automatically accepted. A discarded cache is not a discarded obligation.

The existing conversion certificate stores endpoints, not a checkable reduction
trace. CHECKPOINT therefore cannot currently promise to avoid that reduction on
load. Keep this limitation explicit rather than introducing a second evaluator
or trusting a serialized certificate flag. This decision changes N5's contract;
it does not establish that an image loader has been implemented.

- [ ] Fresh-process CHECKPOINT: check each retained shared derivation once,
  publish it through ordinary acceptance, and resume only unresolved work.
- [ ] Fresh-process RECOMPUTE: reconstruct omitted results from semantic inputs
  through the same solver; compare accepted results with CHECKPOINT.
- [ ] Retained conversion endpoints: re-establish conversion with the ordinary
  pure evaluator; never treat the endpoint pair as portable proof evidence.
- [ ] Budget exhaustion while checking: retain pending work; do not report a
  logical rejection or publish an unchecked solved result.
- [ ] Neither load mode executes host operation requests. Distinguish program
  image resumption from checkpointing an effectful runtime session.

## 9. Implementation and Progress

Complete one vertical slice at a time. Update this table with commit, commands,
results, timing and net source/test LOC. An unchecked row is not implemented.

| State | Step | Concrete work | Required completion evidence |
| --- | --- | --- | --- |
| [x] | Archive | preserve failed source and stop predecessor | local archive commit and failure record |
| [ ] | N0 Baseline and HOTT rules | inventory syntax/tests; pin Narya source and define action, boundary, transport/lifting rules for the initial fragment | compatibility manifest plus concise rule/representation correspondence, including CBPV obligations |
| [ ] | N1 Dimensional pointer Core | stable arena, pointer-key interning, explicit alpha comparison, evaluator; dimension maps and shared boundary diagrams | beta/capture tests, no conversion-based interning, dimension 0-3 action laws; no depth-specific Core variants |
| [ ] | N2 Typed HOTT vertical slice | reader, synthesis, contexts; typed Act, Identity and initial transport/lifting computation | shared Core with distinct typing; post-check `::`; iterated action on Lambda/APP, checked boundaries and supported equality operations |
| [ ] | N3 Dimensional ADT/IADT | parameters/indices, self family, telescope action, nominal declarations, Match/IH and their higher rules | Nat/List/Vec/Acc; higher constructors and Match; indexed transport obligations recorded; producer-order-independent goals |
| [ ] | N4 CBPV and effects | remaining block/quote/exit forms; structural references; request/fold reducers; effect rows and continuation rules | 01-09, single versus repeated execution, nested exit boundary, operation alias, multi-clause deep handler and unhandled forwarding |
| [ ] | N5 Image/CLI | `.a` relocation, seed/checkpoint retention, imports, CLI and REPL parity | fresh-process round trips, nominal identity, split-budget solve equivalence, WHNF/NF; ordinary acceptance rejects malformed retained evidence |
| [ ] | N6 Proof feature parity | supported totality, generated graph IADTs and resources over the foundational HOTT system | IF8, supported function properties and expanded Identity fixtures; negative cases reject; remaining theory limits recorded |
| [ ] | N7 Acceptance | finish manifest, benchmark and review deletion/transfer plan | all agreed supported cases pass; per-module old/new LOC and timings; no hidden use of old solver or runtime |

Dependencies: N0 -> N1 -> N2 -> N3 -> N4 -> N5 -> N6 -> N7. Small experiments for
persistence or recursive binding may happen earlier; they do not bypass the
preceding acceptance gates. N2 must run from source without the old solver before
porting advanced features. N0/N1 already include HOTT's dimensional foundation;
N2 must demonstrate typed higher computation before broader feature migration.

For each step append only a short progress entry:

```text
Step / date / commit:
Behavior implemented:
Checks and elapsed time:
Source + / - / net; tests + / - / net; docs separately:
Unresolved issue and next action:
```

Track graph allocations, intern hits, reducer steps, goal evaluations/wakeups,
and peak memory alongside elapsed time for small examples, List/append and IF8.
Test source-to-result and artifact-to-result independently. Compare the same
input/options on consecutive implementations; do not infer speed from LOC.

## 10. Review Gates and Explicit Limits

- Pointer-based Core is the chosen direction; measure interning collisions and
  explicit alpha-comparison costs separately. Do not add binder canonicalization
  as an implicit construction pass.
- Oracle/IADT descriptors may contain complex rules. Count their code as Core
  functionality when reporting size; hiding it behind pointers is not reduction.
- Do not copy the old solution/projection/transaction infrastructure wholesale.
  Retain mathematical rule distinctions such as APP elimination and Match
  elimination even when they share allocation and scheduling utilities.
- HOTT is an initial architectural requirement, not a later feature. Completing
  every dependent/higher/Universe rule remains a separate mathematical claim;
  record coverage and missing equations without weakening the initial design.
- A complete parser does not mean a complete type checker. Maintain separate
  parsing, typing, evaluation and image-validation status for each compatibility case.
- Main replacement requires the N7 evidence and explicit acceptance. The failed
  branch remains available. No deletion of the old implementation is needed to
  start this independent prototype.

## 11. Progress: 2026-09-07 Initial Core

Historical checkpoint `6ef8a74`; its alpha-interning decision is superseded by
the exact-pointer-key correction below.

Implemented independently in `src/prototype/pointer/`:

- Stable chunk allocation and a shared hash-index implementation used by terms
  and dimensional maps. Core retains exactly Lambda, Application and Reference.
- Pointer-bound alpha comparison/interning; APP construction hashes its child
  pointers directly rather than recursively rescanning them. Lambda hashing
  still traverses its scope; collision/performance work remains to be measured.
- A resumable lexical closure machine for pure Lambda WHNF and capture-avoiding
  readback. Budgets count machine transitions, including administrative steps.
  Readback performs substitution, not normalization. Semantic references are
  currently neutral: oracle dispatch, NF and evaluation memoization remain open.
- Interned binary semicartesian dimension maps and composition. A map `m -> n`
  stores n coordinates drawn from m distinct source axes or the two endpoints;
  repeated source axes are rejected. The convention follows the plan, not the
  legacy operator API's source/target naming. Boundary diagrams and typed Act
  are not yet implemented.

Narya reference revision obtained from GitHub:
`c7c92b4ec01ae2f528b97207256549242bd21334`.
[Pinned dimension operator source](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/dim/op.ml).
This pins the comparison target; it is not evidence that all corresponding
rules have been implemented or that HOTT is complete.

Verification:

```sh
make -f src/prototype/pointer/Makefile check
make -f src/prototype/pointer/Makefile check BUILD=/tmp/a-program-pointer-sanitized CFLAGS='-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
```

Both passed: lexical capture and independent environments; split-budget
equivalence and bounded divergence; 38 maps in dimensions 0-2 with 10,422
composable triples; dimension-3 face/permutation laws; stable references across
arena/index growth. Observed build-and-test wall time was about 0.05s normally
and 0.26s with sanitizers in this session, not a compiler-performance claim.
New C/header code: +589/-0 lines. Test C: +188/-0 lines. Build/docs excluded.

Neither N0 nor N1 is marked complete. Next: complete the compatibility inventory,
implement shared boundary diagrams and scoped action using this pointer graph,
then integrate typed Act/Identity with source synthesis. Do not resume the old
single-path refactoring or push this partial implementation to main.

## 12. Progress: Pointer Identity and Boundary Bindings

The user's clarification supersedes automatic alpha interning in `6ef8a74`.
Core constructors now compare exact pointer tuples only, with no recursive
hashing, alpha comparison, WHNF evaluation or normalization in that path.
Alpha comparison remains an explicit operation and memoizes term pairs together
with their binder correspondence. It never merges them. Readback separately
memoizes term/environment pairs to preserve DAG sharing during substitution.

Lazy cube boundary binders are interned by `(cube*, face_map*)`. Restriction
composes maps before lookup, so two paths to one corner recover the same binder.
Distinct cube owners remain distinct. A degeneracy cannot allocate an independent
boundary variable; its term action still needs the typed action implementation.
These objects specify scopes, not inhabitants of Identity by themselves.

`src/prototype/pointer/tests/compatibility.tsv` now records the frozen source
inventory; its companion README explains the remaining manual review. The
parser's generated binary-Bool companion classifier was identified as semantic
work that must move to typed elaboration when preserving that syntax.

Normal and ASan/UBSan builds pass the expanded Core suite. Regressions include
40-level shared DAGs, capture-preserving readback, distinct alpha-equivalent
Lambda nodes, unchanged interning after beta reduction, and equal composite
boundary restrictions. The typed classifier for a cube, higher term action,
transport/lifting and source parsing remain incomplete. N0/N1 stay open.

The design is informed by the two handmade files and the current implementation
paths above. It is an A Program engineering proposal, not a claimed direct
implementation of an external paper or a claim that pointers establish a new
type-theoretic result.
