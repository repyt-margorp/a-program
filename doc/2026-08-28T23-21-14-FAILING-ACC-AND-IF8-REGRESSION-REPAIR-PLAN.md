# Canonical Constraint Graph and Acc Regression Repair Plan

Date: 2026-08-29 JST

Status: implementation complete; final verification passed

Implementation baseline: `cb6ad9d`

Related repository work:

- Issue #26: two Acc integration regressions
- PR #27: regression reproduction and boundary audit
- PR #25: Term Graph, Context, and codebase consolidation audit
- PR #20: historical v83 meaning-boundary audit; not adopted as current design
- Issue #13: post-hoc function-property generation; intentionally out of scope

## 1. Objective

Restore the complete integration suite without adding another phase-dependent
exception to the classifier solver.

The repair must establish this architecture:

1. lowering creates typed occurrences;
2. one generation pass creates an immutable constraint hypergraph;
3. a monotone solver writes only solution cells and edge lifecycle evidence;
4. validation reads the graph and solutions without discovering new edges;
5. freeze projects accepted solutions into semantic metadata and artifacts.

The target is not merely 54/54 passing tests. The target is that nested
indexed recursion, dependent sequencing, and future solver extensions all use
the same graph discipline.

## 2. Baseline Failures (Resolved)

The current suite has two independent failures.

### 2.1 Checked-Core Acc failure

`explicit_index_family_acc_concrete_check.p` is accepted by producer
elaboration and rejected by independent checked-Core validation.

The rejected sequence-result Context has an instantiated empty effect row,
while its exact producer occurrence retains a solver-local effect-row
variable:

```text
Context result:  Thunk(Pi(... Comp(EFFECT_ROW_EMPTY, ...)))
Producer result: Thunk(Pi(... Comp(EFFECT_ROW_VAR(27), ...)))
```

The original diagnostic decoded a Context ID as an occurrence ID. The test
diagnostic has been corrected so this disagreement is now reported in the
proper subject domain.

### 2.2 IF8 order prerequisite failure

`if8_order_check.p` fails before QuickSort is compiled. The unresolved
fragment contains nested, separately owned recursive equations:

```text
outer Nat Match: *k
inner LT Match:  *prior
```

The relevant `MOTIVE_EQUATION` and `IH_EXPECTED` constraints remain
pending. The failure is not an effort-budget exhaustion.

## 3. Architectural Finding

The repository already uses canonical graph identity in its stable semantic
stores:

| Store | Existing identity |
| --- | --- |
| TermDB | alpha-aware hash-consed Term nodes |
| ContextDB | hash-interned immutable Context extensions |
| SubstitutionDB | hash-interned immutable CwF actions |
| JudgementDB | interned Proposition, Claim, and Derivation objects |

ConstraintDB is already close to a graph:

- one classifier meta per typed occurrence;
- explicit constraint records;
- a worklist;
- reverse dependency adjacency;
- one deterministic generation pass for ordinary classifier goals;
- explicit Match-case and IH payloads.

The inconsistency is localized but fundamental:

- recursive motive ownership is also stored in
  `recursive_equation_owners[]`;
- selected projection state is also stored in
  `ih_projected_motives[]`;
- validation scans occurrence subgraphs to rediscover which IH belongs to a
  branch;
- expected-classifier propagation dynamically creates the information later
  treated as the recursive equation;
- sequence-result propagation scans variables and, in attempted fixes, Core
  Term identity was used to guess an occurrence-level producer.

These paths make graph topology depend on worklist history.

## 4. Required Semantic Separation

### 4.1 Core Terms are not typed occurrences

Two source operations may share one Core Term while having different Contexts
or classifiers. Therefore a typed constraint key must never be only a Core
Term ID.

The minimum typed identity is:

```text
(occurrence, context, constraint kind, ordered role-specific inputs)
```

The same Core lambda in Bool and Nat contexts must remain two typed nodes.

### 4.2 Constraint edges are not solutions

An edge records an equation or rule obligation. A solution cell records the
current answer for one meta. An accepted derivation records why the final
answer is valid.

No one structure may own two of these meanings.

### 4.3 Premises are not provisional answers

The constructor telescope that makes a recursive field callable is an
immutable premise. The classifier obtained by applying the selected Match
motive is the final IH solution.

The present code places both in one classifier slot and branches on whether
the old answer is Pi-shaped. That history-dependent branch must be removed by
representing the premise and solution separately.

## 5. Canonical Constraint Hypergraph

The graph is a hypergraph because many typing rules depend on several ordered
inputs.

### 5.1 Nodes

- classifier meta node, one per typed occurrence;
- binder-classifier meta node, one per Lambda occurrence;
- motive meta node, one per Match occurrence;
- effect-row meta node;
- usage meta node;
- immutable Term, Context, Substitution, constructor-schema, and occurrence
  references.

### 5.2 Edges

The initial implementation keeps the existing rule-specific kinds:

- `HAS_TYPE`
- `EQUAL`
- `CONVERTIBLE`
- `PI_EXPECTED`
- `MOTIVE_EQUATION`
- `IH_EXPECTED`
- `CBPV_BOUNDARY`
- `COMPUTATION_FOLD_RESULT`
- `OPERATION_REQUEST_RESULT`
- `CONSTRUCTOR_FORMATION`
- `BINDER_TYPE`
- Match branch refinement
- computation equations
- effect-row equations

APP, Match, IH, and computation-fold rules remain distinct. They prove
different kernel judgements even when their scheduling plumbing is shared.

### 5.3 Canonical keys

Use direct indexes where source topology already guarantees uniqueness:

- base classifier edge by occurrence;
- IH edge by IH occurrence;
- motive edge by `(Match occurrence, case index)`;
- computation edge by computation occurrence;
- branch-refinement edge by `(Match occurrence, case index)`.

Use hash interning only where several generation paths can request the same
composite edge. Its full key is:

```text
(domain, kind, context, output meta,
 ordered input node IDs, substitution/refinement ID,
 owner Match, case, field, scope)
```

Find-or-create must return the existing ID for the same full key. It must not
merge constraints that merely share a Core Term.

### 5.4 Immutable topology

After graph generation:

- edge count does not change during solving;
- an edge's semantic inputs do not change;
- branch refinement is represented by a referenced substitution/result node,
  not by replacing edge identity;
- validation cannot register recursive equations;
- diagnostics cannot affect solving;
- freeze cannot reopen constraints.

Lifecycle state and evidence may change monotonically on an edge. Meta
solutions may move from unsolved to solved, or be invalidated through an
explicit dependency revision before freeze.

## 6. Recursive Motive Design

Every `IH_EXPECTED` record already contains:

- owner Match occurrence;
- recursive argument occurrence;
- IH scope;
- case index;
- field index.

That record is the canonical recursive motive edge. A separate
`recursive_equation_owners[]` authority is unnecessary.

The edge means:

```text
classifier(IH occurrence)
  =
lift_recursive_field(
  apply(motive(owner Match), recursive argument),
  constructor field schema,
  branch/context action)
```

An IH consumer contributes another fixed edge whose input is the consumer
expected-classifier meta. The edge payload is present from generation; its
value may remain pending until the input meta is solved.

The solver sequence is:

1. read constructor schema as a premise;
2. solve ordinary consumer and branch metas;
3. solve all motive equations jointly;
4. select one owner motive;
5. project it to every owned IH solution;
6. invalidate exact reverse dependents;
7. rerun affected edges;
8. validate all owner branch equations;
9. materialize and freeze.

There is no validation-time discovery or projection authorization.

## 7. Sequence-Result Design

A zero-clause computation fold already has an exact input occurrence edge and
an exact continuation occurrence edge.

The sequence equation is:

```text
input occurrence       : Comp(E, A)
continuation binder    : A
continuation           : Pi(A, Comp(F, B))
fold result            : Comp(E union F, B)
```

The Context classifier is a projection of `A`, not an independent inferred
answer. The exact producer occurrence comes from the fold edge or the existing
binder-owner occurrence reference, never from a Core Term scan.

If latent effect rows are instantiated at this use:

- the use occurrence receives a complete specialized computation classifier;
- the owner schema remains generalized;
- the continuation binder and finalized Context are projected from the use
  occurrence;
- checked-Core verifies the same equation.

## 8. Why This Is Not an E-Graph Replacement

E-graphs efficiently represent congruence classes and are useful for
optimization and syntactic equality saturation. A Program's constraint graph
has a different primary role: it records typed, context-indexed, directed
obligations and their proof evidence.

Therefore:

- TermDB hash-consing remains the canonical expression representation;
- the constraint hypergraph records inference dependencies;
- union-find may later accelerate sound meta equalities;
- e-classes do not become DefEq or Higher Identity evidence;
- no equality discovered by an optimizer is reflected into the kernel without
  an accepted proof rule.

The published dependent-type congruence-closure algorithm assumes uniqueness
of identity proofs. That assumption is incompatible with A Program's intended
higher identity structure, so it is research input rather than an adopted
kernel algorithm.

## 9. Research Basis

The design uses the following results as constraints, not as drop-in
implementations:

1. [OutsideIn(X): Modular type inference with local assumptions](https://www.microsoft.com/en-us/research/publication/outsideinx-modular-type-inference-with-local-assumptions/)
   separates common constraint generation/implication handling from a
   domain-specific solver. A Program similarly separates immutable graph
   generation from classifier, effect, usage, and motive solving.
2. [egg: Fast and Extensible Equality Saturation](https://arxiv.org/abs/2004.03082)
   motivates canonical expression sharing and deferred invariant rebuilding.
   It does not justify using an e-graph as the authority for dependent typing.
3. [Congruence Closure in Intensional Type Theory](https://arxiv.org/abs/1701.04391)
   shows that dependent congruence closure can be proof-producing, but its UIP
   assumption prevents direct adoption for Higher Observational TT.
4. [Incremental Type-Checking for Free](https://doi.org/10.1145/3563303)
   motivates explicit fine-grained dependency recording and reuse rather than
   repeated global discovery scans.

## 10. Work Packages

### CG0: Preserve the regression boundary

- [x] Merge PR #27 locally.
- [x] Merge PR #25 locally.
- [x] Keep Issue #13 out of scope.
- [x] Split IF8 order prerequisite from QuickSort source equality.
- [x] Split the concrete Acc checked-Core fixture into its own phase.
- [x] Correct Context-versus-occurrence checker diagnostics.
- [x] Remove experimental validation-time discovery and Core-scan patches.

### CG1: Canonical graph indexes

- [x] Add direct indexes for base, IH, Match-case, computation, and branch
  constraints.
- [x] Evaluate a full-key interner. No genuinely composite duplicate generation
  path was demonstrated, so direct unique indexes remain sufficient and avoid
  an unused second identity mechanism.
- [x] Reject duplicate insertion into every direct unique index.
- [x] Preserve distinct typed occurrences when one Core Term is shared by
  different Contexts.
- [x] Keep scheduling dependency adjacency derived from generated edge inputs.

### CG2: Recursive motive edges

- [x] Make `IH_EXPECTED` the owner/argument/case/field authority.
- [x] Generate and directly index consumer expectation dependencies before
  solving.
- [x] Replace the remaining read-only branch-membership traversal with an
  explicit generated branch-to-IH dependency slice. It no longer discovers or
  mutates equations.
- [x] Remove `recursive_equation_owners[]`.
- [x] Remove the remaining Pi-shape compatibility branch after recursive-field
  premise and final solution storage are physically separated.
- [x] Replace the global invalidation counter with owner-local motive revisions.
- [x] Keep multiple IHs and nested owner Matches distinct.
- [x] Validate guarded recursive equations by explicit iota contraction,
  binder alignment, and existing kernel conversion; do not accept a residual
  mismatch merely because an IH occurs in the branch.

### CG3: Sequence-result edges

- [x] Resolve the repaired sequence producer by its exact occurrence edge.
- [x] Specialize complete use-occurrence classifiers.
- [x] Project Context result classifiers from the producer solution.
- [x] Reject producer/Context disagreement in checked-Core validation.
- [x] Remove older fallback producer scans after all sequence forms expose an
  occurrence edge.

### CG4: Read-only validation and freeze

- [x] Assert classifier and branch edge counts are unchanged by solving.
- [x] Add a complete solution-cell snapshot assertion around every validation
  entry point.
- [x] Ensure diagnostics are write-only explanations.
- [x] Freeze only solved metas with accepted edge evidence.

### CG5: Remove obsolete state

- [x] Delete the duplicate recursive owner/authorization array.
- [x] Move the derived projected motive cache onto its canonical `IH_EXPECTED`
  edge and remove `ih_projected_motives[]`.
- [x] Delete the remaining read-only subgraph scan as described in CG2.
- [x] Delete blanket truthy/mismatch acceptance and other discovery-history
  branches involved in these regressions.
- [x] Retain explicit rule dispatch and contradiction handling.
- [x] Record per-file line additions and deletions.

### CG6: Verification

- [x] Acc general eliminator producer passes.
- [x] Acc concrete checked-Core passes.
- [x] IF8 order prerequisite passes.
- [x] Fuel-free QuickSort positive cases pass.
- [x] QuickSort negative decrease proofs remain rejected.
- [x] Issue 23 one-IH and two-IH tests pass.
- [x] Issue 23 incompatible-property negative test remains rejected.
- [x] Artifact write/read and deterministic equality pass.
- [x] Full integration suite passes 54/54.
- [x] Constraint generation remains one pass per graph topology, guarded by
  topology-count assertions.
- [x] Focused QuickSort dependency closure remains within the prior measured
  range (`6.996 s` in the final full-suite run).

## 11. Prohibited Shortcuts

- Do not make `::` bidirectional synthesis.
- Do not weaken checked-Core to accept producer output.
- Do not identify typed occurrences by Core Term alone.
- Do not discover constraints during validation.
- Do not keep old and new mutable Authorities during migration.
- Do not merge APP, Match, IH, and fold kernel rules merely to reduce lines.
- Do not reflect propositional or observational equality into DefEq.
- Do not preserve compatibility branches solely to reduce migration size.

## 12. Completion

This plan is complete only when:

1. the immutable graph and solution Authority are explicit in code;
2. both regressions are fixed through that graph;
3. all positive and negative integration boundaries pass;
4. topology/validation invariants have permanent tests;
5. Issue #26 has exact completion evidence and can be closed;
6. changes are committed and pushed to `main`.

## 13. Implementation Result

The completed repair establishes the following concrete boundaries:

- `MOTIVE_EQUATION`, `IH_EXPECTED`, base-classifier, and branch-refinement
  constraints have direct typed-occurrence/case indexes.
- Recursive equation activation is lifecycle evidence on the canonical
  `IH_EXPECTED` edge. There is no parallel owner array.
- A candidate motive mismatch is a contradiction. A guarded recursive branch
  succeeds only after constructor iota contraction and binder-aligned kernel
  conversion prove its exact defining equation.
- Impossible refined branches contribute no motive equation.
- Sequence-result checked-Core validation compares the exact producer
  occurrence after effect-row specialization.
- Match-case equations carry a generated branch-to-IH dependency slice, and
  each Match owner has direct adjacency to its canonical `IH_EXPECTED` edges.
- Motive validation is read-only over solution cells. Recursive invalidation is
  owner-local rather than solver-global.
- APP reverse refinement reuses one stable generated codomain binder per typed
  occurrence, so retries intern the same graph instead of creating fresh
  equation shapes.
- Recursive IH use remains a computation and is sequenced by the ordinary
  zero-clause computation fold. The former direct-value lowering shortcut has
  been removed.
- IH derivations are recorded with their exact typed-occurrence authority, and
  proof-reification cache hits are accepted only while their evidence remains
  retrievable.
- Branch effect rows and totality are joined into the selected computation
  motive. Consequently `Vec.map` now publishes accepted evidence rather than a
  spurious residual effect obligation.
- Computation-fold verification obligations require canonical reserved fields
  and the `PURE_TYPE_WHNF` profile during artifact readback.
- Shared test objects are keyed by prototype source content as well as compiler
  flags, eliminating stale-object boundary results.

Final verification on 2026-08-29 JST:

```text
-Werror prototype reader build: pass
integration suite: 54 executed, 54 passed, 0 failed
suite wall time: 54.147 s
```

Pre-commit implementation diff, including permanent tests and excluding this
document, was:

```text
11 implementation/test files changed, 872 insertions, 470 deletions
```

The net increase is 402 lines before this document update. Most new code is
generated dependency indexing, explicit guarded recursive-equation validation,
and permanent boundary tests. Removed code includes occurrence-wide producer
scans, the global recursive revision Authority, the parallel projected-motive
cache, and the IH direct-value lowering shortcut. No second solver or equality
Authority was introduced.
