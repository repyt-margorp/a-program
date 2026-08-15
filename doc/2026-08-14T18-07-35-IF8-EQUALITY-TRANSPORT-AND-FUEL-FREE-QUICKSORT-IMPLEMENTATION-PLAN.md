# IF8 Equality, Transport, and Fuel-Free QuickSort Implementation Plan

Date: 2026-08-14

Status: complete

Baseline commit: `08861510c4eea3a7550d545e8b5916a546084719`

Parent plan:
`doc/2026-08-13T19-33-00-EXPLICIT-INDEX-FAMILY-SURFACE-AND-ACC-IMPLEMENTATION-PLAN.md`

Tracking issue: <https://github.com/repyt-margorp/a-program/issues/11>

Plan authority: this document is the sole implementation and progress
authority for IF8. The parent indexed-family plan retains only a delegation
link and one completion checkbox.

## 1. Objective

Complete IF8 by implementing a direct, fuel-free QuickSort whose recursive
calls are justified by `Acc` evidence for the actual partition outputs.

The completed path must provide only the object-equality functionality that
the algorithm and its decrease proofs actually require. It must not claim that
general Higher Observational Type Theory, Universe coherence, or arbitrary
effectful transport has been completed.

The audited dependency chain is:

```text
current indexed families and Acc
  -> audit the exact decrease-proof equations
  -> encode exact partition bounds as indexed constructor fields
  -> construct Nat accessibility and list-size evidence
  -> construct well-foundedness and QuickSort through Acc
  -> correct dependent-branch Context ownership
  -> execute and replay the accepted artifact
```

## 2. Current Baseline

The following work is complete at the baseline commit:

- explicit parameter and index telescopes;
- exact indexed constructor results;
- indexed Match refinement and impossible-branch rejection;
- indexed and lifted induction hypotheses;
- strict positivity through admitted Pi codomains;
- generic `Acc` declaration and elimination;
- concrete `accFalse` construction;
- artifact v74 write, read, relocation, and accepted replay; and
- the full prototype integration suite.

IF8 is the remaining part of the indexed-family plan. The implementation now
has both a fuel comparison fixture and an Acc-driven fuel-free fixture:

- `src/prototype/tests/fixtures/typing/if8_fuel_quicksort_comparison.p`;
- `src/prototype/tests/fixtures/typing/if8_fuel_free_quicksort_check.p`; and
- `src/prototype/tests/fixtures/typing/if8_order_check.p`.

The fuel fixture is only a behavioral comparison. The target `quickSort` and
`quickSortAcc` signatures contain no algorithmic fuel.

### 2.3 Implementation checkpoint (2026-08-15)

The proof-shape audit showed that public Eq and transport are not required for
termination. `Partition A bound` stores exact witnesses
`LT lowerSize (succ bound)` and `LT upperSize (succ bound)`. Those witnesses are
passed directly to the `down` field of `Acc Nat LT`; no arithmetic equality is
transported.

For the IF8 fixtures, source compilation, accepted replay, IH evaluation, and
complete normalization now succeed. The mixed sample normalizes in 438
comparison steps and performs 23 IH reductions. Swapping the lower and upper
bound witnesses is rejected.

Artifact v74 publication is complete. Constructor selection now owns an exact
refined branch Context and certified substitution; operation-case binder IDs
are persisted explicitly and relocated with the rest of the typed occurrence
graph. Accepted replay validates that stored refinement rather than rebuilding
an alpha-dependent approximation. Artifact closure remains fail-closed.

The audit also fixed two general inference defects exposed by IF8: a
single-branch motive may not treat a branch-local binding as a constant, and
an Operation-aware constructor check must not repeat its domain comparison in
an unrefined Context. Existing Vec, Fin, tail, Acc, CBPV, and artifact tests now
pass with the same rules.

### 2.1 Equality boundary

The current implementation contains two different facilities that must not be
conflated:

1. `PROTOTYPE_TERM_RELATION_TYPE_FORMER` and
   `PROTOTYPE_TERM_RELATION_WITNESS_FORMER` are compiler-local parametricity
   machinery. Artifact v74 explicitly rejects them as object Identity roots.
2. The Identity subsystem can generate ordinary object-level Identity family
   declarations and Claims for a limited closed fragment. It also constructs
   degeneracy witnesses and some higher identities.

The current public source language nevertheless has no general surface Eq,
`refl`, or `transport`. The type-former descriptor still marks generic
transport and lifting as deferred.

Therefore this plan must not implement surface Eq by merely exposing
`RELATION_*` terms.

### 2.2 Existing authority boundaries to preserve

- TermDB remains the one graph of terms and classifiers.
- Operation and Type annotations remain the source-visible ownership layer.
- object Identity is represented by ordinary Terms, Claims, and Derivations;
  no separate EqualityDB is introduced.
- DefEq remains compiler conversion and is not extended by user Eq proofs.
- `::` remains a post-synthesis expectation, not an input to synthesis.
- `Acc` remains a library indexed family; no privileged Acc kernel tag or
  evaluator rule is added.
- compiler-local HOTT search state, bridges, fuel, and work queues do not become
  artifact authority.

## 3. Theory Boundary

### 3.1 Minimum equality required by IF8

IF8 requires at most a homogeneous, first-dimensional interface:

```text
Eq A x y type                 where x : A and y : A
refl x : Eq A x x
transport P p u : P y        where P : A -> @, p : Eq A x y, u : P x
transport P (refl x) u  -->  u
```

Congruence may be needed by the proof library:

```text
ap f p : Eq B (f x) (f y)
```

`sym` and `trans` should be library definitions when derivable from transport.
They are not new conversion rules.

This fragment does not require:

- equality reflection;
- global union-find updates from Eq witnesses;
- heterogeneous surface equality;
- Universe univalence or general Universe correspondence;
- proof irrelevance;
- complete higher coherence;
- effectful transport; or
- contextual-equivalence completeness.

### 3.2 HOTT compatibility

The public homogeneous Eq must elaborate to the existing object-Identity
computation for its classifier. It must not elaborate to the compiler-local
parametric relation action.

The source interface may present `x == y`, but its internal authority is the
type-directed object Identity family selected for `A`. A later heterogeneous
or higher interface may expose more of that family without changing the IF8
proofs.

`refl` is the public form of the existing degeneracy construction. It is not a
new claim that all Identity witnesses are definitionally equal.

`transport` is elimination/fibrancy data for object Identity. It is not a
conversion test and it must not merge the endpoints in TermDB.

### 3.3 CBPV boundary

The first implementation is limited to pure value families:

```text
P : A -> @
u : P x
transport P p u : P y
```

Transport of `Comp(E, A)`, effect rows, operation requests, handlers, or
runtime-dependent classifier families is out of scope. No duplicate
Value-side and Computation-side term graph is introduced.

## 4. Mandatory Proof-Shape Audit

The previous plan states that public Eq/transport is required, but this has not
yet been demonstrated by a concrete QuickSort proof term. Indexed families can
often move equations into constructor result indices, and constructor iota can
make many length equations DefEq.

Before changing the kernel, construct a source-level proof sketch for:

```text
Smaller ys xs := LT (length ys) (length xs)
```

and an indexed partition package that returns both lists and their bounds.

For every required equation, classify it as:

| Class | Required mechanism |
|---|---|
| constructor/iota computation | existing DefEq |
| exact constructor-selected index | existing indexed-family refinement |
| structural order witness | ordinary `LT` constructor |
| same proposition under non-DefEq indices | public transport |
| arithmetic lemma | explicit Eq proof plus transport, or a stronger indexed invariant |
| unresolved/unsupported | residual; never accept as a proof |

The audit must produce a table of the exact equations encountered in both
partition branches. Kernel work begins only after this table identifies the
smallest missing rule.

This gate may show that fuel-free termination can be implemented before a
general public transport. If so, implement QuickSort with the stronger indexed
partition invariant and retain public Eq/transport as a separate prerequisite
only for later correctness proofs. Do not add a primitive solely because the
old plan predicted it would be needed.

## 5. Proposed Surface Interface

The provisional notation is:

```a-program
p := left == right;
r := #.refl value;
v1 := #.transport family p v0;
```

This adds one symbolic equality form and no reserved word. `#.refl` and
`#.transport` are semantic system names, not machine intrinsics and not effect
operations.

The exact spelling is frozen only after phase EQ0. The semantic rules, not the
spelling, are authoritative.

Required source behavior:

- synthesize both endpoints first;
- require one homogeneous classifier modulo existing conversion;
- select the object Identity computation for that classifier;
- reject unsupported Identity computation explicitly;
- infer the endpoints and carrier of `#.refl` and `#.transport` from Claims;
- never use `::` to seed that synthesis; and
- preserve source spans in diagnostics.

## 6. Internal Representation Decision

Do not add `EQ_TYPE` and `REFL` Term tags before auditing the existing generated
Identity representation. The preferred representation is:

- Eq type: an application of the generated object-Identity type former to the
  two endpoints;
- refl witness: the ordinary constructor/witness term returned by the existing
  degeneracy path;
- transport: either an ordinary projection/application from certified
  Identity fibrancy data, or one dedicated eliminator term only if the current
  object representation cannot express it.

If a dedicated transport term is necessary, it must have one canonical shape,
one typing rule, and one reduction rule. It must not duplicate transport as an
Identity action, a Judgement-only pseudo-term, and a second runtime node.

The implementation decision record for EQ0 must answer:

1. Which existing generated Identity term is the classifier of `x == y`?
2. Which accepted Claim certifies its formation?
3. Can degeneracy be called deterministically from lowering/elaboration?
4. Where is the fibrancy/transport function represented for ordinary ADTs?
5. Is transport ordinary APP/Lambda/Match structure, or does it require one
   new eliminator node?
6. Which parts survive artifact replay without compiler-local bridge state?

## 7. Implementation Phases

### EQ0: Freeze the minimal object-Identity contract

Status: complete for IF8

Tasks:

- [x] Write the equation inventory described in section 4.
- [ ] Trace one closed Nat degeneracy witness from Identity computation through
      Claim and Derivation publication.
- [ ] Trace one nontrivial closed Nat Identity witness accepted by artifact
      v74.
- [x] Determine whether IF8 needs transport after strengthening the partition
      result indices.
- [x] Add a small `quickSortWithFuel` comparison fixture because no such source
      fixture currently exists in the repository.
- [ ] Decide the surface spelling.
- [ ] Decide whether transport can be represented using existing terms.
- [ ] Record explicit non-goals for dependent/effectful/higher transport.

Exit gate:

- every IF8 equality obligation has a named proof mechanism;
- object Identity and compiler relation actions are unambiguously separated;
- the internal representation is fixed before parser or wire changes.

### EQ1: Surface homogeneous Eq and refl

Status: superseded as an IF8 prerequisite; retained as future Equality work

Candidate files:

- `src/prototype/include/a_program/frontend/ast.h`
- `src/prototype/src/frontend/ast.c`
- `src/prototype/src/frontend/reader.c`
- `src/prototype/src/frontend/lowering.c`
- `src/prototype/src/frontend/lowering/*.inc`
- `src/prototype/include/a_program/identity/identity_computation.h`
- `src/prototype/src/identity/identity_computation.inc`
- `src/prototype/src/identity/object_term_action.inc`

Tasks:

- [ ] Add the selected surface Eq form without introducing expected-type-led
      synthesis.
- [ ] Lower endpoint Operations and recover their accepted Claims.
- [ ] Require homogeneous endpoint classifiers using structured conversion.
- [ ] Request the existing object Identity computation for that classifier.
- [ ] Materialize an ordinary Eq classifier Term and accepted IS_TYPE evidence.
- [ ] Expose degeneracy as `#.refl` or the approved notation.
- [ ] Reject relation-action terms as object Eq evidence.
- [ ] Diagnose unsupported, residual, exhausted, and invalid outcomes
      separately.

Permanent tests:

- [ ] `Nat.zero == Nat.zero` forms.
- [ ] refl has that classifier.
- [ ] endpoints with different classifiers reject.
- [ ] different Nat constructors do not receive a fabricated witness.
- [ ] conversion-exhausted identity formation remains residual/failure.
- [ ] `::` is checked after synthesis and cannot manufacture Eq.

Exit gate:

- closed homogeneous Nat/List Eq and refl compile through ordinary source;
- no DefEq table or Term interning class changes after accepting a proof.

### EQ2: Pure dependent transport

Status: superseded as an IF8 prerequisite; retained as future Equality work

Candidate files, conditional on the EQ0 representation decision:

- `src/prototype/include/a_program/core/term.h`
- `src/prototype/src/core/term.c`
- `src/prototype/include/a_program/kernel/judgement/types.h`
- `src/prototype/include/a_program/kernel/judgement/rules.h`
- `src/prototype/src/kernel/rules/`
- `src/prototype/include/a_program/identity/types.h`
- `src/prototype/src/identity/identity_computation.inc`
- `src/prototype/src/identity/object_term_action.inc`
- `src/prototype/src/core/normalization/`

Typing rule:

```text
Gamma |- A type
Gamma |- x : A
Gamma |- y : A
Gamma |- p : Eq A x y
Gamma, z : A |- P z type
Gamma |- u : P x
--------------------------------
Gamma |- transport P p u : P y
```

Tasks:

- [ ] Represent the family using its Binding ID and existing Pi/family graph.
- [ ] Validate the Eq witness against the exact endpoint Claims.
- [ ] Reindex/substitute the family with existing certified Substitution APIs.
- [ ] Emit a distinct transport elimination Derivation if a kernel rule is
      required.
- [ ] Implement only the `refl` computation law in the first fragment.
- [ ] Keep neutral transport neutral.
- [ ] Preserve proof relevance and the witness term.
- [ ] Prohibit effectful or unresolved classifier families in this phase.
- [ ] Include normalization profile and exhaustion in structured outcomes;
      never treat exhaustion as successful transport.

Permanent tests:

- [ ] `transport P (#.refl x) u` normalizes to `u`.
- [ ] transport changes `P x` to `P y` without changing endpoint Term IDs.
- [ ] wrong family domain rejects.
- [ ] wrong witness endpoints reject.
- [ ] a neutral proof produces a typed neutral transport.
- [ ] an effectful family remains unsupported.

Exit gate:

- dependent pure transport is accepted and replayable;
- conversion behavior is unchanged except for the explicit transport redex.

### EQ3: Artifact authority for Eq/transport

Status: superseded as an IF8 prerequisite; retained as future Equality work

Tasks:

- [ ] Determine whether the chosen representation changes the wire vocabulary.
- [ ] If it does, create a successor to v74 with no compatibility parser.
- [ ] Persist only object Terms, Claims, Derivations, type declarations, and
      certified Substitutions needed by exported Eq/transport terms.
- [ ] Do not persist Identity work queues, bridges, action requests, fuel, or
      relation terms.
- [ ] Validate transport premise roles and endpoint/family ownership on
      readback.
- [ ] Add forged-artifact negatives for witness, family, endpoint, and proof
      rule substitution.
- [ ] Check deterministic write-read-write bytes.

Exit gate:

- an exported transport proof round-trips and replays read-only;
- malformed transport evidence cannot be accepted by changing IDs in text.

### IF8-A: Constructive order and list length

Status: complete for the termination fragment

Add source fixtures rather than kernel primitives:

```a-program
LT :=
    @\left : Nat =>
    @\right : Nat =>
    {
        zeroSucc : (n : Nat) -> * Nat.zero (Nat.succ n);
        succSucc : (m : Nat) -> (n : Nat) -> * m n ->
            * (Nat.succ m) (Nat.succ n);
    };
```

The exact constructor telescope may be adjusted to the parser's accepted
dependent syntax, but `LT` remains an ordinary indexed family.

Tasks:

- [x] Define generic `length : (A : @) -> List A -> Nat`.
- [x] Define `LT` and the structural witnesses required by IF8.
- [x] Define the list-size relation used by accessibility.
- [x] Prove the exact strict bounds required by partition.
- [x] Use DefEq for constructor/iota equations; no transport is required.
- [x] Round-trip the order witnesses through the artifact.

Exit gate:

- nontrivial `LT` witnesses compile and execute;
- false fibers are rejected by indexed constructor disjointness.

### IF8-B: Partition with decrease evidence

Status: complete at source and accepted-replay boundaries

Define a generic indexed result package whose constructor stores:

- the lower partition;
- the upper partition;
- `Smaller lower original`; and
- `Smaller upper original`.

Do not add primitive Sigma. Use an ordinary indexed ADT with dependent
constructor fields. The package is the single authority for keeping a result
list and its bound proof paired.

Tasks:

- [x] Take a pure comparator `le : A -> A -> Bool` as a parameter.
- [x] Define generic partition by structural recursion on the input tail.
- [x] Extend both recursive partition results with exact `LT` witnesses.
- [x] Audit EQ0 and establish that transport is unnecessary for this encoding.
- [x] Reject a mismatched decrease witness instead of leaving it residual.
- [x] Test empty, singleton, ascending, descending, duplicate, and mixed inputs.

Exit gate:

- both partition outputs carry accepted decrease witnesses for every branch;
- removing either proof makes QuickSort fail to typecheck.

### IF8-C: Well-foundedness and Acc-driven QuickSort

Status: complete at source evaluation and artifact replay

Define a generic library proof:

```text
listSmallerWellFounded :
    (A : @) ->
    (xs : List A) ->
    Acc (List A) Smaller xs
```

Then define QuickSort with an explicit accessibility argument:

```text
quickSortAcc :
    (A : @) ->
    (le : A -> A -> Bool) ->
    (xs : List A) ->
    Acc (List A) Smaller xs ->
    List A
```

The recursive calls must consume the lifted IH supplied by the `Acc.acc`
field:

```text
*down lower lowerSmaller
*down upper upperSmaller
```

Tasks:

- [x] Construct Nat/list-size well-foundedness as source code.
- [x] Pair generic `List A` values with Nat sizes and accessibility evidence.
- [x] Define generic `quickSortAcc` using ordinary Match and the existing
      lifted IH.
- [x] Define the public `quickSort xs` by supplying well-foundedness.
- [x] Instantiate the generic algorithm with Nat and the existing pure Nat
      comparison style for executable fixtures.
- [x] Ensure no algorithmic fuel argument occurs in the public or internal
      QuickSort type.
- [x] Keep compiler normalization/search step limits distinct from algorithmic
      recursion fuel.
- [x] Compare outputs with the newly added fuel comparison fixture and exact
      reference values.

Exit gate:

- representative inputs normalize to the expected sorted lists;
- the generic function typechecks independently of the Nat instance;
- recursive calls cannot be typed without the corresponding decrease proof;
- no Acc-specific kernel or evaluator branch has been added.

### IF8-P0: Dependent branch Context ownership and certified refinement

Status: complete

Tasks:

- [x] Replace the invalid same-Context branch endomorphism with an explicit
      refined branch Context produced by constructor selection.
- [x] Restore `explicit_index_family_tail_check.p` accepted replay before
      extending the artifact boundary; this is a mandatory non-regression gate.
- [x] Store or derive one authoritative materialization Context per branch
      Operation classifier. Do not attach an outer-index classifier to the
      original constructor-binder Context.
- [x] Make branch Operations and their nested occurrences point at that refined
      Context, preserving Binding IDs while changing dependent classifiers.
- [x] Represent the constructor refinement as a certified Context morphism or
      a dedicated Match-refinement derivation, not as an unproved arbitrary
      substitution.
- [x] Materialize binder assumptions and constructor-spine Claims in the
      refined Context before reindexing APP/IH premises.
- [x] Require accepted replay to validate the exact Match owner, constructor,
      branch, source Context, and target Context.
- [x] Require artifact closure to find CwF evidence for every persisted
      `EXTEND`; do not add a permissive fallback.
- [x] Add a regression that reaches the nested `Acc.down` application used by
      QuickSort and round-trips it through artifact v74.

Exit gate:

- every persisted substitution is a well-formed Context morphism;
- the QuickSort artifact writes and replays without proof search; and
- the existing Vec, Fin, Acc, and insertion-sort artifact tests remain valid.

### IF8-D: Integration, artifact replay, and issue review

Status: complete

Tasks:

- [x] Add a focused order/partition/QuickSort integration script; retain public
      Eq/transport as separate future work.
- [x] Add accepted artifact replay for all exported IF8 dependencies.
- [x] Add malformed artifact and negative typing boundaries.
- [x] Run `make -f src/prototype/Makefile test-integration`.
- [x] Build reader and REPL configurations with warnings as errors.
- [x] Run representative `:whnf` and `:nf` checks.
- [x] Update README support boundaries and artifact version references.
- [x] Update the parent IF8 row and blocker log with actual evidence.
- [x] Review every GitHub Issue #11 acceptance criterion before closure.
- [x] Report added/deleted/net LOC by implementation, tests, and docs.

Exit gate:

- the complete suite passes;
- accepted replay performs no proof search;
- Issue #11 has been reviewed against all of its acceptance criteria and is
  closed only if the broader indexed-family scope, not merely IF8, is complete.

## 8. Test Matrix

| Area | Positive boundary | Negative boundary |
|---|---|---|
| Eq formation | same homogeneous classifier | different classifiers |
| refl | exact diagonal endpoints | forged non-diagonal refl |
| transport | pure dependent family | wrong endpoint/family |
| reduction | transport over refl | neutral proof must not reduce |
| conversion | explicit result type checks | Eq proof does not alter DefEq |
| LT | constructor-supported inequality | impossible index fiber |
| partition | both outputs carry bounds | missing or swapped bound proof |
| Acc | recursive calls use `*down` | direct unguarded recursion |
| QuickSort | empty/singleton/mixed/duplicate | removed decrease evidence |
| artifact | write/read/replay | forged witness/premise/rule |
| resource policy | proof use recorded normally | silent proof erasure assumptions |

## 9. Risks and Required Stops

### R1: Relation action exposed as Eq

Stop if surface lowering reaches `RELATION_TYPE_FORMER`. Object Eq must use the
object Identity family and accepted object Claims.

### R2: Transport implemented as equality reflection

Stop if accepting `p : Eq A x y` changes future conversion of `x` and `y`.
Transport creates a new term; it does not change kernel equality.

### R3: Full HOTT accidentally made an IF8 prerequisite

Stop and narrow the rule if implementation begins requiring general Universe
identity, arbitrary dependent higher identity, or effectful lifting. IF8 needs
only its explicit first-dimensional proof obligations.

### R4: Unnecessary primitive caused by a weak invariant

If an equation can be made DefEq by indexing the partition result more
precisely, prefer that data design. Do not add arithmetic conversion rules.

### R5: Proof search hidden in artifact replay

Replay validates the stored Derivation DAG. It must not rerun HOTT action
search or reconstruct missing decrease evidence.

### R6: Termination proof confused with sortedness

The decrease witnesses prove that recursive calls terminate. They do not prove
permutation or sortedness. Those are later theorem-library milestones.

## 10. Explicit Non-Goals

- complete Higher Observational Type Theory;
- general heterogeneous equality syntax;
- equality reflection;
- automatic optimizer rewrite registration from Eq proofs;
- primitive Sigma;
- primitive Acc or QuickSort;
- proof irrelevance or proof erasure policy;
- generic IADT/GADT unification;
- effectful well-founded recursion;
- sortedness and permutation proofs; and
- backend code generation changes unrelated to the accepted artifact.

## 11. Progress Dashboard

Allowed states:

```text
not-started | in-progress | blocked | complete | superseded
```

| Phase | Status | Depends on | Completion evidence |
|---|---|---|---|
| EQ0 minimal contract and proof-shape audit | complete | current v74/IF7 | exact indexed bounds remove transport from termination proof |
| EQ1 surface homogeneous Eq/refl | superseded | EQ0 | not required by IF8; remains future Equality work |
| EQ2 pure dependent transport | superseded | EQ1 | not required by IF8; remains future Equality work |
| EQ3 artifact authority | superseded | EQ1/EQ2 | no Eq/transport terms are introduced by IF8 |
| IF8-A LT/length/Smaller | complete | EQ0 | `if8_order_check.p` and source integration test |
| IF8-B partition decrease package | complete | IF8-A | exact lower/upper bounds; swapped witness rejects |
| IF8-C Acc-driven QuickSort | complete | IF8-B, current IF7 | source normalization equals expected; no fuel argument |
| IF8-P0 dependent branch Context ownership | complete | IF8-C | certified refined Context, exact case binders, v74 replay |
| IF8-D integration and issue review | complete | IF8-P0 | focused IF8 and complete integration suites pass |

## 12. Decision Log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-14 | Plan IF8 as a separate equality-dependent milestone after completed Acc | Acc is implemented; QuickSort and its proof library are not |
| 2026-08-14 | Audit concrete decrease equations before adding transport | exact indices and DefEq may remove part or all of the predicted need |
| 2026-08-14 | Keep compiler relation action distinct from object Eq | artifact v74 and the Identity audit explicitly enforce this boundary |
| 2026-08-14 | Limit initial transport to pure value families | sufficient for termination proofs and consistent with current dCBPV boundary |
| 2026-08-14 | Keep Acc and order as ordinary indexed families | avoids algorithm-specific kernel rules |
| 2026-08-14 | Do not include sortedness/permutation in IF8 | termination evidence and functional correctness are separate obligations |
| 2026-08-15 | Do not make public Eq/transport an IF8 prerequisite | exact `Partition` indices carry both strict bounds directly |
| 2026-08-15 | Reject the branch-endomorphism shortcut | constructor refinement changes the dependent Context; the resulting arbitrary substitution has no CwF evidence |
| 2026-08-15 | Persist exact Match occurrence binder IDs in artifact v74 | alpha-interned Core binders cannot reconstruct typed occurrence identity |
| 2026-08-15 | Keep public Eq/transport outside IF8 | exact indexed partition bounds directly supply both recursive decrease witnesses |
| 2026-08-15 | Preserve exact bindings in open IH Core terms | alpha equivalence is established under an enclosing Lambda/Match telescope; reusing a scope-slot Binding ID for open terms would conflate distinct source occurrences |

## 13. Progress Log Template

| Date | Phase | Change | Evidence | Remaining risk |
|---|---|---|---|---|
| 2026-08-15 | EQ0 | Audited partition equations and selected exact indexed bounds | no transport occurs in the completed source proof | Equality remains separate future work |
| 2026-08-15 | IF8-A-C | Added LT, Nat accessibility, SizedList, Partition, partition, append, and Acc-driven generic QuickSort | source normalization `yes`; 23 IH reductions; comparison fixture `yes` | artifact boundary remains |
| 2026-08-15 | IF8-D | Added permanent source, comparison, no-fuel, and swapped-bound tests | `test_if8_fuel_free_quicksort.sh` passes | artifact/full integration pending IF8-P0 |
| 2026-08-15 | IF8-P0 | Removed an abandoned evidence-fabrication experiment and reproduced both boundaries | `-Werror` build and IF8 source test pass; artifact closure rejects substitution 447; indexed-tail replay currently rejects operation 5 | branch classifier ownership must be corrected before artifact work continues |
| 2026-08-15 | IF8-P0 | Made refined branch Contexts, exact occurrence binders, relocation, and replay one authority | indexed-family, Acc, CBPV, and artifact integration tests pass | fixed-size IH frame storage remains an implementation limit |
| 2026-08-15 | IF8-D | Completed six-case normalization, v74 replay, malformed binder rejection, deterministic rewrite, and full integration | `test_if8_fuel_free_quicksort.sh`, `test_artifact_flow.sh`, `test_cbpv_surface.sh`, and `test-integration` pass | public Eq/transport and full Issue #11 scope remain separate |
| 2026-08-15 | IF8-D | Replaced the obsolete shared-scope-slot IH test with an exact three-way authority check | IH argument Operation Binding ID, Core VAR Binding ID, and operation-case Binding ID agree; distinct cases remain distinct; the complete integration suite passes | none for the IF8 boundary |

Issue #11 was reviewed on 2026-08-15 and remains open. IF8 satisfies its
Acc/QuickSort milestone, but the issue also requests boundaries such as an
indexed `Vec.append` fixture that are not evidenced by this change. Completing
IF8 is therefore not sufficient grounds to close the broader issue.

## 14. Blocker Log Template

| Date | Phase | Blocker | Required decision/evidence | Resolution |
|---|---|---|---|---|
| 2026-08-14 | EQ0 | It was not demonstrated which QuickSort decrease equations are non-DefEq | Construct the indexed partition proof shape and classify every equation | Resolved: exact indexed bounds need no transport |
| 2026-08-14 | EQ2 | Generic transport capability is deferred in the current Identity descriptor | Decide whether IF8 actually requires it | Removed from IF8 dependency; retained as future Equality work |
| 2026-08-15 | IF8-P0 | Dependent branch refinement is recorded as an invalid Context endomorphism | Move branch ownership to the refined Context and publish certified refinement evidence | Resolved in v74 with explicit refined Context ownership and relocation |
| 2026-08-15 | IF8-P0 | `explicit_index_family_tail_check.p` regressed at `SOLVED_MATCH_MOTIVE` replay | Make the branch Operation and its classifier share the refined Context; do not relax conversion | Resolved; stored refinement and exact occurrence binders replay successfully |

## 15. Completion Definition

IF8 is complete only when all of the following are true:

1. `quickSort` has no algorithmic fuel argument.
2. Every recursive call is justified by an accepted `Smaller` witness supplied
   to the lifted `Acc` induction hypothesis.
3. If Eq/transport is used by the proof, it is object evidence and never
   compiler-local relation evidence. The implemented stronger partition
   invariant requires no Eq/transport.
4. No accepted Eq witness changes DefEq.
5. Source compilation, evaluation, artifact write/read, and accepted replay
   pass.
6. Negative tests reject missing, forged, residual, or mismatched decrease
   evidence.
7. The full integration suite passes.
8. Documentation states the exact first-dimensional equality fragment and the
   still-unimplemented higher/dependent/effectful boundaries.
9. GitHub Issue #11 is reviewed against its complete acceptance list before it
   is closed.

## 16. Parent-Plan Synchronization

Do not update IF8 progress in both plans while implementation is underway.
Phase status, evidence, risks, and blockers belong only in this document.

After all conditions in section 15 pass:

- [x] Change the parent plan's delegated IF8 checkbox to complete.
- [x] Change the parent progress-dashboard IF8 row from `not-started` to
      `complete` and cite the final tests, artifact version, and commit.
- [x] Update the parent document's top-level status from delegated IF8 to IF8
      complete.
- [x] Replace the parent implementation report's deferred-IF8 statement with
      the exact completed scope.
- [x] Leave historical IF6/IF7/IF9 log entries unchanged.
- [x] Review Issue #11; close it only if its complete acceptance criteria, not
      merely the IF8 subset, are satisfied.

If this plan is superseded, update the parent link once. Do not copy an
incomplete progress history back into the parent document.

## 17. Physical Accounting

Relative to baseline `08861510c4eea3a7550d545e8b5916a546084719`, including
the v73 deletion and v74 replacement files:

| Area | Added | Deleted | Net |
|---|---:|---:|---:|
| implementation/schema/build | 9,556 | 4,638 | +4,918 |
| permanent tests and fixtures | 591 | 27 | +564 |
| README and plan documents | 829 | 40 | +789 |
| total | 10,976 | 4,705 | +6,271 |

The authoritative per-file accounting is `git diff --numstat` against the
baseline plus the newly added v74/IF8 paths until the implementation is
committed. The largest physical replacement is `wire_v73.c` (3,422 deleted)
to `wire_v74.c` (3,456 added); the focused IF8 fixture/script contribution is
451 added lines. These counts include documentation and generated wire-format
replacement work, not only semantic kernel changes.
