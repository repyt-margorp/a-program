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
- [ ] Nominal provenance through constant Pi codomain elimination: directly
  matching a nested computed constructor result encounters
  `PG_RETURN_CONTENT(PG_PI_CONSTANT_CODOMAIN(...))`; the instance traversal
  does not yet recover its admitted family. This is unsupported, not invalid.
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

The constant-codomain fixture still explicitly records unsupported work in
component tests; it is not a substitute for successful source compatibility
acceptance. The recursive-field fixture is now an execution success test.
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
effects, save `.a`, restore work between processes, select `main` implicitly,
or provide WHNF/NF/REPL parity yet. `tests/cli.sh` is part of pointer `check` and
checks stdin, zero budget, cyclic pending work, rejection and malformed options.

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
