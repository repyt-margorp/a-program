# Issue 12: Dependent Match Refinement and Recursive Proof Plan

Date: 2026-08-16

Status: complete; focused and full integration validation passed

Issue: [#12 Dependent recursive proof production and indexed branch refinement block certified sorting after IF8](https://github.com/repyt-margorp/a-program/issues/12)

Current repository commit at audit time: `5c55bef`

Issue baseline commit: `977f174`

Preceding authority migration:
`doc/2026-08-16T14-43-28-SEMANTIC-STORE-AUTHORITY-AND-PHYSICAL-CONSOLIDATION-PLAN.md`

That migration already consolidated classifier/effect constraint authority,
split TypeDeclaration schema from readback/cache storage, and normalized
Proposition references while preserving Proposition, Claim, and Derivation as
different semantic objects. This plan uses those completed boundaries. It does
not reopen them or create replacement stores.

## 1. Purpose

Issue 12 exposes two failures that look separate at the surface:

1. A structurally recursive comparator cannot return a proof-bearing result.
2. A branch of an indexed ADT Match cannot retain the constructor index equality
   needed to build another indexed constructor.

They are the same compiler-boundary defect.

The compiler can discover local type equalities while checking a branch, but it
does not represent one authoritative branch-local context action early enough.
Instead, ordinary ADTs and indexed ADTs currently pass through different
substitution paths. The classifier may be refined while the Core owner,
occurrence context, IH classifier, and accepted proof evidence remain in the
unrefined context.

This document defines the theory boundary, authority model, implementation order,
test matrix, artifact decision, and completed implementation record for that
defect.

## 2. Scope

This plan covers:

- dependent Match branch refinement for ordinary and indexed ADTs;
- recursive proof-producing functions whose IH result depends on branch binders;
- the constraint lifecycle that produces branch-local substitutions;
- propagation into TypedOccurrence, Judgement, P0 validation, and artifacts;
- source-located diagnostics for unsupported or inconsistent refinement;
- removal of duplicate ordinary/indexed branch substitution paths.

This plan does not cover:

- global equality reflection;
- adding every proved equality to conversion;
- public `Eq`, `refl`, `transport`, or rewrite syntax;
- general higher-order unification;
- automatic synthesis of total orders, sorting correctness, stability, or cost;
- changing the termination requirement for recursion;
- merging all semantic databases into one arena;
- changing Core Term evaluation or CBPV effect semantics.

## 3. Reproduction Matrix

### 3.1 Recursive proof-producing comparator

The first reproducer has the conceptual type:

```text
compareNat :
  (left right : Nat) ->
  Either (LE left right) (LE right left)
```

It performs nested Matches on `left` and `right`, recursively invokes
`*leftPred rightPred`, and lifts the recursive proof with `LE.succ`.

Current result at `5c55bef`:

```text
P0 occurrence validation failed occurrence=17 tag=4 context=60
core=159 classifier=158 binding=INVALID edges=INVALID+0
action=INVALID source-core=INVALID source-classifier=INVALID
P0 accepted proof graph validation failed
...:0:0: failed to compile AST graph
```

The important signal is not merely the P0 rejection. The occurrence has a
refined classifier but no persisted context action or source projection. P0 is
detecting a graph assembled from incompatible contexts.

### 3.2 Indexed branch refinement

The second reproducer is structurally:

```text
OrderedFrom := @\lower : Nat => {
  nil  : (current : Nat) -> * current;
  cons : (current : Nat) ->
         (head : Nat) ->
         LE current head ->
         * head ->
         * current;
};

rebuild := \lower : Nat => \xs : OrderedFrom lower =>
  xs @nil current => OrderedFrom.nil lower
     @cons current head currentToHead tail =>
       OrderedFrom.cons lower head
         (currentToHead :: LE lower head)
         *tail;
```

Current result at `5c55bef`:

```text
typing fixed point: classifier inference failed status=-1
...:21:4 failed
compile-diagnostic code=constructor-domain-mismatch
phase=constraint-solver ast#16 occurrence#15 constraint#16
expected=INVALID actual=INVALID
```

The branch establishes that the outer index `lower` is the constructor result
index `current`. That branch-local equality is needed when reusing
`currentToHead` as `LE lower head`. The compiler currently reaches constructor
checking before it has materialized the substitution carrying this fact.

### 3.3 Existing tests are insufficient

The existing `explicit_index_family_tail_check.p` exercises a Vec tail, but its
branch returns the recursive field directly. It does not require the branch index
equation to be consumed by a different constructor proof.

The IF8 fixture avoids public equality and transport by using exact indices. It
therefore does not exercise the missing branch-action propagation either.

## 4. Investigation Order

The implementation must be explored and changed in this order. This prevents a
local typing patch from hiding a broken accepted graph.

1. Surface Match and constructor syntax.
2. TypedOccurrence creation and branch binder identity.
3. Constraint generation and fixed-point scheduling.
4. Context pullback and Substitution construction.
5. Classifier and motive reindexing.
6. Recursive IH ownership and classifier derivation.
7. Judgement Claims and Derivations.
8. P0 accepted-graph validation.
9. Artifact write, read, and replay.
10. Diagnostics and permanent boundary tests.

The fix is not complete if only the surface program compiles. The same branch
action must survive every later boundary.

## 5. Current Mechanism Decomposition

### 5.1 Core TermDB

`TermDB` owns context-independent computation topology. Constructor, APP, Lambda,
Match, IH, Return, Thunk, operation request, and computation fold terms belong
here.

TermDB must not become the authority for a Match branch assumption. The same Core
term may appear in different typed contexts. Branch-local refinement belongs to
the typed occurrence and context morphism layers.

### 5.2 TypedOccurrenceGraph

`src/prototype/include/a_program/graph/typed_occurrence_model.h` already gives a
Match case:

- a branch `context_id`;
- `has_refinement`;
- `refinement_substitution`;
- constructor owner and constructor ID;
- exact branch `BindingId` values.

Each occurrence also records its current Context, optional context-action
substitution, source Core term/classifier, current Core term/classifier, and IH
ownership metadata.

This is the correct frozen projection layer. It must contain the solved result,
not own a second mutable solver lifecycle.

### 5.3 ContextDB and SubstitutionDB

`src/prototype/include/a_program/kernel/context.h` already represents the needed
CwF structure:

- immutable context extension in `ContextDB`;
- identity, empty, projection, extension, and composition morphisms in
  `SubstitutionDB`;
- a substitution `sigma : source -> target` mapping each target variable to a
  source term;
- context comprehension and pullback operations.

These databases are semantically distinct and must not be merged. A Context is
an object; a substitution is a morphism. Their storage implementation may share
generic allocation utilities, but their IDs and invariants must remain typed.

### 5.4 ConstraintDB

The operation constraint system in
`src/prototype/src/frontend/lowering/context_and_type_lowering.inc` currently
has classifier, effect-row, and computation domains. It does not have a
first-class branch-refinement constraint.

A separate fixed-capacity `operation_branch_refinement` array stores mutable
branch-refinement state. This makes the lifecycle independent of the normal
constraint dependency graph.

The repository's current authority policy requires every mutable elaboration
constraint to have one `ConstraintId`. Therefore branch refinement belongs in
`ConstraintDB`; it must not become another independent global arena.

### 5.5 JudgementDB

JudgementDB already supports:

- Match pattern assumptions;
- semantic action substitutions on derivations;
- `SUBSTITUTION_REINDEX` evidence;
- accepted Claims and Derivation DAGs.

No new equality database or branch-proof term is required. A solved branch
substitution and its CwF evidence are enough to explain the refinement at the
kernel boundary.

### 5.6 Artifact v76 at audit time

The current artifact stores:

- Context and Substitution graphs;
- CwF evidence Claims;
- occurrence context-action substitutions and source terms/classifiers;
- Match-case context, refinement substitution, constructor information, and
  binder identities;
- accepted substitution-reindex proof evidence.

The existing wire model may already be sufficient for solved branches. A version
bump must be decided only after the normative in-memory result is fixed.

## 6. Root Causes

### 6.1 Refinement is scheduled after the first complete classifier solve

`src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` first runs
general classifier inference to completion. Only afterwards does it freeze
contexts and call `operation_solver_materialize_indexed_branch_contexts`, then
rebuild motives and run classifier inference again.

This creates a circular dependency:

1. Constructor-domain checking needs the branch index equation.
2. The branch equation is materialized only after constructor-domain checking
   has already succeeded.

`OrderedFrom` fails at step 1, so the later materialization pass is unreachable.

### 6.2 Ordinary ADTs do not receive a persisted branch action

`operation_solver_materialize_indexed_branch_contexts` skips declarations whose
`index_count` is zero.

That is incorrect for dependent elimination over an ordinary ADT. Even when
there are no family indices, a constructor branch establishes a local scrutinee
equation. For a Nat successor branch it establishes:

```text
scrutinee := Nat.succ predecessor
```

Nested recursive proof production depends on this substitution being applied to
the entire typed occurrence subtree and IH evidence.

The compiler currently performs a local `substitute_bound_var` operation for
ordinary Matches. That may refine an expected classifier, but it does not create
or persist a Context/Substitution action. The Core owner and proof occurrence can
therefore remain unrefined.

### 6.3 Indexed and ordinary Match use different semantics

Current paths are:

- indexed ADT: delayed Context pullback and stored substitution;
- ordinary ADT: direct term substitution used during local checking.

These are not merely different optimizations. They generate different accepted
graphs for the same dependent-elimination principle.

The direct-substitution path must be removed. Both cases need one branch action;
an ordinary ADT simply has an empty index-equation set plus the scrutinee mapping.

### 6.4 General and generated-Identity refinements share a misleading API

`prototype_judgement_source_indexed_branch_refinement` is the source IADT path.
It requires a variable scrutinee and currently rejects zero-index declarations.

`prototype_judgement_indexed_branch_refinement` is a different, highly
specialized path with assumptions such as two indices and a generated relation
shape. Its name suggests a general IADT operation even though it serves generated
Identity/relation machinery.

They duplicate part of Context/Substitution construction while enforcing
different theories. The generated-Identity function must be renamed and
confined. A lower-level branch-substitution builder may be shared where the
invariants actually coincide.

### 6.5 Accepted replay can infer missing refinement

Accepted replay currently validates a stored refinement when present, but can
call source refinement again when it is absent.

Replay must not be a second elaborator. Re-solving can allocate terms, contexts,
or substitutions and can make artifact acceptance depend on implementation order.
Replay must only validate persisted object evidence.

### 6.6 Diagnostics discard the decisive data

Constructor-domain mismatch diagnostics only populate expected/actual terms for
some conversion constraints. The indexed reproducer therefore reports both as
`INVALID`.

P0 occurrence validation returns an undifferentiated failure, and the frontend
emits `0:0`. This hides whether the inconsistency is in owner, classifier,
substitution endpoint, binder image, or IH frame.

Diagnostics are part of the fix because residual refinement is a legitimate
outcome. Unsupported programs must fail at the originating branch with a precise
reason.

## 7. Theory Audit

### 7.1 Option A: add branch equalities to global DefEq

Under this option, an equation discovered in a constructor branch is added to a
global conversion relation or union-find.

Decision: rejected.

Reasons:

- the equation is valid only under that branch's constructor assumption;
- it would leak into sibling branches and outer contexts;
- conversion would depend on elaboration and proof-discovery order;
- artifact replay could change kernel behavior by importing more equations;
- it conflates eliminator-local reasoning with equality reflection.

### 7.2 Option B: require object Eq and transport in every branch

Under this option, branch checking builds an object-level equality witness and
requires explicit transport for every dependent use.

Decision: not the primitive Match rule.

Object equality and transport remain important user-facing features, but routine
dependent pattern matching should not wait for the public Higher Observational
Identity interface. Constructor elimination already supplies branch-local
equations as part of its typing rule.

Unresolved equations may later require explicit equality/transport. Solved rigid
constructor equations do not.

### 7.3 Option C: proof-relevant branch-local unification as a CwF action

Under this option, constructor matching produces a branch Context and a certified
substitution back to the source Context. Index equations are solved only inside
that branch. All classifiers, terms, and proofs are reindexed through the same
action.

Decision: adopted.

This agrees with dependent pattern-matching elaboration and with the repository's
existing Context/Substitution representation. It also preserves the distinction:

```text
branch-local constructor refinement != global DefEq
branch-local constructor refinement != arbitrary object Eq reflection
```

### 7.4 Relation to CBPV

Constructor matching refines value-side data. The branch body may produce a Value
or a Computation, but the same Context substitution reindexes its classifier.

Effect rows remain separate constraints. Branch refinement must not infer that a
calculation is pure, execute an effect, or merge value and computation stores.

### 7.5 Relation to Higher Observational Type Theory

The missing mechanism is not observational Identity. It is the local dependent
eliminator context.

HOTT may later expose equality witnesses and transport, but it must not be used to
paper over a missing Match action. Conversely, solving a branch equation must not
create a global observational witness unless an object-level rule explicitly
does so.

## 8. Normative Branch Rule

Assume a source context containing a scrutinee:

```text
Gamma, s : D parameters source_indices
```

and a constructor schema:

```text
c : constructor_fields -> D parameters constructor_indices
```

For the branch of `c`, solve the equations:

```text
source_indices =?= constructor_indices
```

under the constructor field telescope.

The solver result is one of:

- `SOLVED`: a branch Context and substitution were constructed;
- `IMPOSSIBLE`: rigid constructor heads prove the branch unreachable;
- `RESIDUAL`: the supported first-order solver cannot decide the equations;
- `INVALID`: malformed schema, scope, binder identity, or substitution data.

For a solved branch construct:

```text
sigma_c : Gamma_c -> Gamma, s : D ..., source_case_binders
```

It must map:

- unaffected outer bindings to themselves;
- solved source-index bindings to their branch terms;
- `s` to the exact constructor spine `c constructor_fields`;
- source case binders to exact branch `BindingId` values.

Then check the branch body in `Gamma_c` against the motive applied to the
constructor spine, with both sides reindexed by `sigma_c`.

For an ordinary ADT, the index equation list is empty, but `sigma_c` still maps
the scrutinee to its constructor spine. This is required for the recursive
comparator.

For an indexed ADT, `sigma_c` additionally carries solved index equations. This
is required for `OrderedFrom`.

### 8.1 Orientation invariant

The solver must preserve the identity of outer source bindings unless the branch
rule explicitly maps them. It must not silently replace an outer index with a
fresh constructor-field binding in the wrong direction.

Tests must assert the exact substitution images, not only successful compilation.

### 8.2 Solver scope

The first implementation supports proof-relevant, first-order constructor
pattern equations. It must include an occurs check and deterministic orientation.

It must return `RESIDUAL`, not guess, when encountering unsupported higher-order,
non-pattern, or ambiguous equations.

## 9. Target Authority Model

| Concern | Sole authority | Stored projection or evidence |
| --- | --- | --- |
| Mutable branch-refinement lifecycle | `ConstraintDB` | status, dependencies, result IDs |
| Refined branch object | `ContextDB` | `ContextId` |
| Branch action | `SubstitutionDB` | `SubstitutionId` |
| Frozen branch occurrence | `TypedOccurrenceGraph` | Context and substitution references |
| Accepted typing fact | `JudgementDB` | Claim and Derivation referencing the action |
| Context-free computation | `TermDB` | unchanged Core topology |
| Persistent accepted graph | artifact | frozen IDs and object evidence only |

There must be no independent authoritative `BranchRefinementDB` or unindexed
fixed-capacity refinement table.

If variable-length equation data is needed during solving, it must be a typed
extent owned by its `ConstraintId`. A generic arena utility may allocate the
extent, but it must not introduce a second semantic owner.

## 10. Data Structures to Unify and Keep Separate

### 10.1 Unify

The following represent the same branch-elaboration phenomenon and must become
one pipeline:

- ordinary Match direct bound-variable substitution;
- indexed Match delayed Context pullback;
- branch-refinement mutable status;
- branch classifier/motive reindexing;
- IH classifier reindexing;
- Match pattern semantic-action evidence;
- accepted replay validation of that action.

### 10.2 Share only a lower-level builder

Source IADT branch refinement and generated Identity relation refinement may share:

- Context-comprehension construction;
- substitution extension and composition;
- endpoint and binder-image validation;
- CwF certificate generation.

They must retain distinct rule entry points because their premises and theorems
are different.

### 10.3 Keep separate

Do not merge:

- TermDB and TypedOccurrenceGraph;
- ContextDB and SubstitutionDB;
- ConstraintDB and JudgementDB;
- Proposition, Claim, and Derivation semantics;
- classifier constraints and effect-row constraints;
- source Match refinement and object-level Identity/transport;
- mutable elaboration state and accepted artifact evidence.

A large universal arena would reduce type safety and hide kernel ownership. The
desired reduction is duplicate authority and duplicate algorithms, not distinct
semantic objects.

## 11. Implementation Phases

### Progress Dashboard

- [x] `BR0-A` Reproduce both Issue 12 failures at current HEAD.
- [x] `BR0-B` Trace surface-to-artifact ownership and identify duplicate paths.
- [x] `BR0-C` Select the branch-local CwF action theory.
- [x] `BR1` Freeze the normative rule and in-memory result shape.
- [x] `BR2` Integrate branch refinement into ConstraintDB.
- [x] `BR3` Implement the general branch-action solver.
- [x] `BR4` Integrate branch actions into the fixed point and occurrences.
- [x] `BR5` Rebuild dependent IH handling on branch actions.
- [x] `BR6` Align Judgement and P0 accepted replay.
- [x] `BR7` Complete diagnostics.
- [x] `BR8` Decide and implement the artifact boundary.
- [x] `BR9` Add permanent positive, negative, and replay tests.
- [x] `BR10` Remove obsolete paths and report physical changes.

### BR1: Normative rule and representation

- [x] Add an English invariant beside the Match occurrence structures.
- [x] Define exact `SOLVED`, `IMPOSSIBLE`, `RESIDUAL`, and `INVALID` meanings.
- [x] Define substitution orientation for parameters, indices, scrutinee, and
      case binders.
- [x] Define behavior for variable and non-variable scrutinees.
- [x] Define which first-order equation forms are supported.
- [x] Decide whether solved-result fields fit the current operation-constraint
      payload or require a new typed payload member.
- [x] Confirm no new global semantic arena is needed.

Exit criterion: two reviewers can derive the expected substitution images for
both reproducers without consulting implementation special cases.

### BR2: ConstraintDB integration

- [x] Add a branch-refinement constraint domain or typed kind to the unified
      operation ConstraintDB.
- [x] Give every Match case exactly one stable refinement `ConstraintId`.
- [x] Record dependencies on scrutinee classifier, resolved constructor schema,
      source Context, motive, and branch binders.
- [x] Store result status plus refined `ContextId` and `SubstitutionId`.
- [x] Requeue dependent classifier constraints when the result changes.
- [x] Replace the fixed `operation_branch_refinement[4096]` authority.
- [x] Keep only the final projection on TypedOccurrence Match cases.

Exit criterion: a diagnostic can name the refinement constraint responsible for
every Match branch before classifier completion.

### BR3: General branch-action solver

- [x] Split scrutinee replacement from index-equation solving.
- [x] Allow zero-index declarations; construct the scrutinee action anyway.
- [x] Generalize source indexed refinement into source Match branch refinement.
- [x] Add deterministic equation orientation and occurs checking.
- [x] Preserve exact `BindingId` identity in every substitution image.
- [x] Classify rigid constructor conflicts as `IMPOSSIBLE`.
- [x] Classify unsupported equations as `RESIDUAL` with a reason code.
- [x] Classify malformed schemas or scopes as `INVALID`.
- [x] Rename the hard-coded generated relation function to an Identity-specific
      name.
- [x] Extract only the genuinely shared Context/Substitution builder.

Exit criterion: ordinary Nat successor and `OrderedFrom.cons` produce the same
kind of branch action, differing only in index equations.

### BR4: Fixed point and occurrence pullback

- [x] Generate branch-refinement constraints as soon as case resolution and
      scrutinee classification permit.
- [x] Solve them before branch-dependent constructor-domain constraints.
- [x] Pull back each branch TypedOccurrence subtree exactly once per stable action.
- [x] Compose with an existing occurrence context action instead of overwriting it.
- [x] Reindex Core owner, classifier, motive result, constructor spine, and source
      projections together.
- [x] Invalidate and requeue dependent constraints after action installation.
- [x] Remove the ordinary-Match `substitute_bound_var` semantic path.
- [x] Remove post-completion indexed-context materialization as an authority.

Exit criterion: no occurrence may have a refined classifier with an unrefined
Core owner or missing source/action projection.

### BR5: Dependent IH

- [x] Derive the IH classifier in the same refined branch Context.
- [x] Reindex guarded recursive arguments through the branch action.
- [x] Compose actions across nested Matches deterministically.
- [x] Ensure recursive result evidence and the operational recursive term share
      the same occurrence owner.
- [x] Remove IH-specific expected-classifier overrides made obsolete by the
      general branch action.
- [x] Retain mandatory termination evidence at recursive construction.
- [x] Keep Sorted, Permutation, stability, and cost as optional Claims about the
      same operational term.

Exit criterion: `compareNat` can lift the recursive `LE` proof without duplicating
the comparator operation term.

### BR6: Judgement and P0

- [x] Produce Match-pattern assumptions from the solved action.
- [x] Produce one accepted substitution/reindex certificate for that action.
- [x] Make Match and IH Derivations cite the same substitution evidence.
- [x] Add a pure validator for persisted branch actions.
- [x] Remove accepted-replay fallbacks that invoke the elaboration solver.
- [x] Prohibit accepted replay from adding Term, Context, or Substitution nodes.
- [x] Preserve the strict constructor owner/classifier P0 invariant.
- [x] Return structured P0 failure details rather than only `-1`.

Exit criterion: accepted replay validates the frozen graph without discovering
new branch equalities.

### BR7: Diagnostics

- [x] Record expected constructor domain and actual classifier for every failed
      constructor constraint.
- [x] Report branch-refinement status and reason.
- [x] Report invalid substitution endpoint and binder image details.
- [x] Map occurrence failures back to `source_ast` spans.
- [x] Replace `0:0` failures for both reproducers.
- [x] Emit a source-located unsupported diagnostic for `RESIDUAL`.
- [x] Never relax conversion or P0 validation to make a diagnostic disappear.

Exit criterion: each negative test identifies its originating branch and the
specific unsolved or inconsistent equation.

### BR8: Artifact decision

- [x] Audit whether v76 fields can encode all accepted solved branch actions.
- [x] Confirm replay can validate solved action status from existing object data.
- [x] Determine whether impossible-branch or explicit result-status persistence is
      required for deterministic replay.
- N/A: v76 was not retained because deterministic impossible/residual branch
  status was not represented.
- [x] Otherwise define `artifact_v77.schema` before serializer code.
- [x] If v77 is needed, store only frozen accepted results, never work queues,
      solver fuel, or search history.
- [x] Do not add a backward-compatibility fallback that re-solves missing data.

Exit criterion: write/read/replay preserves branch actions byte-stably and does
not depend on machine speed or elaboration order.

### BR9: Permanent tests

Positive tests:

- [x] Ordinary ADT dependent branch substitution without recursion.
- [x] Nested ordinary ADT Match with composed substitutions.
- [x] Recursive proof-producing `compareNat`.
- [x] Indexed `OrderedFrom` reconstruction consuming the branch equality.
- [x] Existing Vec tail refinement.
- [x] Existing Acc/IF8 termination examples.
- [x] Normalization of both Issue 12 positive reproducers.
- [x] Artifact write/read/replay for both reproducers.

Negative tests:

- [x] Wrong relation orientation in `LE` reuse.
- [x] Rigid incompatible constructor indices.
- [x] Unsupported non-pattern index equation produces `RESIDUAL`.
- [x] Forged IH frame or owner.
- [x] Substitution with wrong source or target Context.
- [x] Substitution mapping a branch binder to the wrong `BindingId`.
- [x] Accepted replay rejects missing persisted refinement instead of re-solving.
- [x] Invalid dependent branch reports a real span and expected/actual terms.

Boundary assertions:

- [x] No new TermDB equality or global conversion entry is created.
- [x] Every solved branch has one refinement ConstraintId.
- [x] Every accepted branch action has one CwF evidence path.
- [x] No accepted replay allocation occurs.
- [x] Existing examples and full regression suite remain unchanged semantically.

### BR10: Cleanup and measurements

- [x] Delete the fixed branch-refinement array.
- [x] Delete the ordinary direct-substitution branch path.
- [x] Delete delayed post-classifier refinement authority.
- [x] Delete accepted-replay solver fallbacks.
- [x] Remove duplicate or misleading refinement function names.
- [x] Update architecture and artifact documentation.
- [x] Report per-file added and deleted lines.
- [x] Separate test growth from production-code growth.
- [x] Explain any net production-code increase by a new invariant or diagnostic.

## 12. Expected File Areas

Primary implementation areas:

- `src/prototype/include/a_program/graph/typed_occurrence_model.h`
- `src/prototype/include/a_program/kernel/context.h`
- `src/prototype/src/frontend/lowering/context_and_type_lowering.inc`
- `src/prototype/src/frontend/lowering/constraint_solver.inc`
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`
- `src/prototype/src/kernel/rules/formation_early.inc`
- `src/prototype/src/kernel/typing/accepted_replay.inc`
- `src/prototype/include/a_program/graph/compile_diagnostic.h`
- Match expansion and motive rebuilding modules
- artifact schema/serializer/replay modules if BR8 requires a version bump
- typing and artifact fixture directories

The exact edit list must be refreshed at the start of each phase. This list is a
dependency map, not permission to change every file.

## 13. Risks and Stop Conditions

### 13.1 Accidental equality reflection

Stop if a proposed fix registers a branch equation globally or changes conversion
outside the branch Context.

### 13.2 Replay becomes an elaborator

Stop if artifact validation needs to run unification, consume fuel, or allocate
semantic nodes. The wire representation is then incomplete.

### 13.3 Solver scope silently expands

Stop if the first-order solver begins guessing higher-order instantiations. Emit
`RESIDUAL` and specify a future extension instead.

### 13.4 Identity-specific assumptions leak into source Match

Stop if generated relation arity or two-index assumptions appear in the generic
source branch rule.

### 13.5 New semantic mega-arena

Stop if reducing duplicate code erases typed IDs or combines Context,
Substitution, Constraint, and Judgement ownership. Share physical primitives,
not semantic authority.

### 13.6 P0 is weakened

Stop if the fix bypasses owner/classifier checks or treats missing source action
as acceptable. The P0 failure is evidence of an earlier inconsistency.

## 14. Issue Acceptance Mapping

| Issue requirement | Plan phase |
| --- | --- |
| Recursive proof-producing comparator compiles | BR3-BR5, BR9 |
| Indexed branch equality is retained or transported | BR2-BR4, BR9 |
| P0 accepts or gives source-located unsupported result | BR6-BR7 |
| No `0:0` or invalid expected/actual diagnostic | BR7 |
| Positive dependent recursion tests | BR9 |
| Negative invalid-refinement tests | BR9 |
| Artifact write/read/replay | BR8-BR9 |
| Termination evidence remains mandatory | BR5 |
| Optional proofs do not duplicate operational terms | BR5 |

## 15. Definition of Done

Issue 12 is complete only when all of the following hold:

1. Both current reproducers compile and normalize.
2. Ordinary and indexed Match use one branch-action mechanism.
3. Branch refinement participates in the normal constraint fixed point.
4. TypedOccurrence Core, classifier, Context, and action are coherent.
5. Dependent IH results use the same composed branch action.
6. Judgement evidence cites the persisted substitution.
7. Accepted replay performs validation only.
8. Artifact round trips preserve the accepted graph deterministically.
9. Negative tests reject invalid refinements with source spans.
10. No branch equation is promoted to global DefEq.
11. No new duplicate semantic store is introduced.
12. Obsolete branch-special-case code is deleted.

## 16. References

The implementation is specific to A Program, but the adopted branch rule follows
the established treatment of dependent pattern matching as proof-relevant local
refinement:

- Jesper Cockx, Dominique Devriese, and Frank Piessens,
  "Proof-relevant unification: Dependent pattern matching with only the axioms
  of your type theory":
  https://researchportal.vub.be/en/publications/proof-relevant-unification-dependent-pattern-matching-with-only-t/
- Healfdene Goguen, Conor McBride, and James McKinna,
  "Eliminating Dependent Pattern Matching":
  https://research.google.com/pubs/archive/99.pdf
- Alexandre Buisse and Peter Dybjer, CwF treatment of dependent pattern matching:
  https://www.cse.chalmers.se/~peterd/papers/Philadelphia2008.pdf

These references justify the local substitution model. They do not determine A
Program's CBPV layering, TypedOccurrence ownership, accepted Claim graph, or
artifact format; those are fixed by the repository invariants documented above.

## 17. Implementation Log

### 2026-08-16: BR1-BR10

- Integrated one branch-refinement lifecycle into `ConstraintDB` with explicit
  solved, impossible, residual, and invalid outcomes.
- Generalized source Match refinement to ordinary and indexed ADTs and made the
  resulting Context substitution the authority for occurrence pullback.
- Ordered nested branch actions from outer to inner, rebuilt derived motives,
  and rederived dependent IH and zero-clause computation-fold classifiers from
  their final branch Contexts.
- Removed accepted-replay refinement solving. Replay now validates persisted
  actions and rejects missing status, wrong endpoints, and forged Binding IDs.
- Selected artifact v77 because branch result status must survive publication;
  no v76 compatibility fallback was retained.
- Added permanent comparator, indexed reconstruction, impossible-branch,
  residual-equation, IF8, normalization, and artifact-corruption tests.
- Added definition-lowering rollback for TypedOccurrence and resolution side
  tables so failed speculative attempts cannot enter the accepted graph.
- Focused dependent-Match, explicit-index-family, artifact-flow, IF8, and the
  complete type-inference example manifest passed.

### 2026-08-17: final consistency fixes and validation

- Removed in-place Match-case mutation from TermDB. Constructor resolution now
  rebuilds the exact TypedOccurrence Match and propagates immutable Core changes
  through its occurrence parents. This preserves alpha-interned Core sharing
  while allowing different typed occurrences to carry different branch owners.
- Rebuilt shared-Core Lambda proof materialization from exact occurrence edges;
  `list_map_induction` now remains valid when Bool and Nat identity Lambdas share
  one erased Core node.
- Kept Match branch premise actions on the Match elimination Derivation instead
  of generating a duplicate `CONTEXT_WEAKEN` proof path.
- Made outer branch pullback update nested solved branch constraints from the
  same occurrence-case action. Context canonicalization now derives mutable
  branch payload endpoints from the referenced Substitution, including when an
  old immutable action is preserved for committed proof edges.
- Updated the WHNF cache boundary check to assert that resolving one Match does
  not mutate the original unresolved Match node.
- Passed `test_dependent_match_refinement.sh`, artifact flow, P0 certificate,
  List map induction, resumption multiplicity, IF8, and the complete
  `test-integration` target.

### Physical change report

Relative to audit commit `5c55bef`, the staged diff contains 62 files, 6,486
added lines, and 1,358 deleted lines after Git rename detection. This includes
896 lines of implementation record, 255 lines of new fixtures/integration test,
and the v76-to-v77 schema/header/generated-wire renames. The apparent total
growth is therefore partly documentation and permanent boundary coverage; no
second semantic store was added.

Largest hand-written implementation changes:

| File | Added | Deleted | Purpose |
| --- | ---: | ---: | --- |
| `frontend/lowering/constraint_solver.inc` | 2,005 | 357 | unified branch constraints, pullback, fixed-point integration |
| `kernel/typing/accepted_replay.inc` | 603 | 203 | validation-only persisted branch replay |
| `frontend/lowering/graph_construction.inc` | 477 | 78 | immutable occurrence-level Match rebuilding |
| `frontend/lowering/context_and_type_lowering.inc` | 446 | 69 | constraint payload and branch authority model |
| `frontend/lowering/finalization_and_entrypoints.inc` | 231 | 261 | remove delayed authority and reorder finalization |
| `kernel/typing/conversion.inc` | 247 | 41 | scoped refinement conversion boundaries |
| `identity/object_term_action.inc` | 135 | 33 | exact occurrence evidence selection |

Generated artifact replacement after rename detection:

| File | Added | Deleted |
| --- | ---: | ---: |
| `src/artifact/wire_v76.c -> wire_v77.c` | 18 | 11 |
| `spec/artifact_v76.schema -> artifact_v77.schema` | 10 | 5 |
| `include/a_program/artifact/wire_v76.h -> wire_v77.h` | 2 | 2 |

The exact committed diff remains the authoritative per-file measurement.
