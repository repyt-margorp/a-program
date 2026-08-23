# Function Graph Certified Execution and `Returns` Removal Plan

Date: 2026-08-22 JST

Status: FGR4 implementation complete; FGR8 performance and HOTT follow-up open

Base revision: `715eaf1e70b1c6fa7a0e6ed7a098777823fca666`

Source research:

- `doc/2026-08-22T00-00-00-FUNCTION-GRAPH-RETURNS-BRIDGES-AND-PROOF-INTERFACE-RESEARCH.md`
- GitHub pull request `#19`, merged into `main` on 2026-08-22

This plan critically revises the implementation part of the merged research. It
keeps the graph-first direction, but it does not adopt the schematic judgement
`*f x : @f x (f x)` literally. In the current A Program CBPV calculus, `f x` is
a computation, not a value of the result type. The replacement is a certified
execution that returns the result value and its function-graph witness together.

## 1. Objective

Implement a definition-specific proof interface for a named function `f`:

```text
@f
	generated ordinary indexed inductive function graph

*f
	generated certified execution returning both an output and @f evidence
```

Then remove the public and kernel-level `Returns` vocabulary without retaining
a compatibility facade or renaming the same relation.

The first complete implementation must support:

1. a small total recursive function such as `length`;
2. ordinary dependent elimination over its generated graph;
3. current `Acc`-based `quickSortAcc` and the `quickSort` wrapper;
4. two recursive worker calls in the QuickSort partition branch;
5. source compilation, artifact publication, readback, link, and accepted replay;
6. removal of `#.Returns`, `#.returns`, their Term tags, and their proof rules.

This is not a tactic implementation and does not add a function-graph primitive
to the kernel.

## 2. Progress

### Review and contract

- [x] Merge PR `#19` into `main`.
- [x] Audit current CBPV function and sequencing rules.
- [x] Audit current `Returns` Term, Context, Claim, Derivation, and artifact use.
- [x] Audit the current indexed-family and recursive-IH representation.
- [x] Compare the proposal with primary function-graph and dependent-CBPV work.
- [x] Accept this revised semantic contract before implementation.

### Implementation

- [x] FGR0: freeze syntax, ownership, and certified-execution rules.
- [x] FGR1: add graph requests and immutable accepted-definition views.
- [x] FGR2: generate `@length`, its result package, and `*length`.
- [x] FGR3: derive executable `length` by evidence erasure/projection.
- [x] FGR4: support helper calls and the `Acc` QuickSort worker graph.
- [x] FGR5: publish and replay generated graph objects in artifact v83.
- [x] FGR6: remove obsolete `Returns` fixtures and preserve separate sequencing
  coverage. QuickSort graph-property fixtures remain part of FGR4.
- [x] FGR7: remove `Returns` completely and rename sequencing provenance.
- [ ] FGR8: run correctness, negative, determinism, and performance gates.
- [x] Commit and push the green implementation checkpoint to `main`.

### 2.1 Implemented checkpoint

The current implementation checkpoint provides:

- source `@f` and `*f` requests keyed by the unique source definition;
- an immutable accepted-definition view over accepted AST, occurrence,
  classifier, effect, and totality authorities;
- an ordinary generated graph IADT, dependent result package, certified runner,
  and executable projection for the supported pure-total fragment;
- branch-precise constructors and recursive graph premises for unary direct
  Match recursion, covered by `length`;
- unsupported source shapes are residual; no forgeable catch-all graph
  constructor is generated;
- artifact v83 owner/graph/result/certified-runner associations, dense
  publication, readback, link validation, and malformed-index rejection;
- complete removal of active `Returns` syntax, Term formers, proof rules, and
  the `TERMINATES_FROM_RETURNS` bridge;
- sequence-result Context provenance and direct totality witnesses justified by
  accepted `TOTAL` computation classifiers.

The earlier coarse `executed(args, output)` fallback was removed after the
implementation audit. Because its public constructor admitted every output, it
was not merely imprecise: it failed to characterize the named execution at all.
Only structure-preserving generated constructors are accepted.

### 2.2 2026-08-23 FGR4 implementation audit

The implementation now additionally has:

- recursive accepted-AST traversal and exact recursive APP spines;
- full dependent source argument indices and two recursive-call packages;
- requested-only named helper graph/result packages;
- dependency-first generation, so one type's constructor interval is not
  interleaved with a helper type's constructors;
- direct and nested runner support for terminal named helper packages;
- explicit projection from classifier Pi binders to accepted source-operation
  binders before a dependent final result is used.

The final item fixes a real authority bug exposed by `partitionLower` and
`partitionUpper`. Their structurally shared classifier could retain the first
definition's Pi Binding IDs. Peeling that Pi and treating its codomain as if it
already lived in the second definition's operation Context leaked foreign free
Bindings. The accepted argument path now constructs the one Context
substitution from classifier binders to source-operation binders before graph
schema generation. This is a semantic projection between binder contexts, not
name-based remapping.

`partitionByDecision` now generates and type-checks a certified graph using the
ordinary helper packages for `partitionLower` and `partitionUpper`.

The `partition` boundary is higher-order. Its block computes:

```text
decision := le head pivot
```

Recording only `decision : Bool` would make the public graph constructor
forgeable. A sound implementation must carry evidence that this output belongs
to the graph interface selected for this exact `le` argument. The proof-side
callback therefore returns an ordinary dependent IADT package:

```text
CertifiedBinaryBool A LeGraph left right := @{
	returned : (output : Bool) -> LeGraph left right output -> *;
}

leCertified : (left : A) -> (right : A) ->
	CertifiedBinaryBool A LeGraph left right
```

This package is implemented using the existing indexed-family implementation;
it requires no Core tag. The elaboration contract is strict:

1. generated graph families are relationally parameterized by `LeGraph`;
2. generated certified runners receive `leCertified` and obtain `decision` and
   its graph witness in one sequencing step;
3. named `&le` arguments may be elaborated only to the associated `@le`/`*le`;
4. a local or anonymous comparator without an explicit certified interface is
   residual;
5. an arbitrary callback must never be silently paired with an unrelated raw
   comparator;
6. generated helper calls propagate the same interface through `partition`,
   `quickSortAcc`, and `quickSort`.

The compiler must not mutate an already accepted source definition to insert
these proof arguments. They belong to generated graph/result/runner objects and
to graph-reference consumer elaboration. The executable definition retains its
raw operational classifier.

The focused boundary test now generates this interface for the named
`leftIsZero` comparator, propagates it through `partition`, `quickSortAcc`, and
`quickSort`, publishes the generated closure in artifact v83, and eliminates
the 17-field `quickSortAcc.cons` graph constructor as an ordinary IADT. An
anonymous comparator remains residual because it does not select an
authoritative graph interface.

## 3. Critical Review of PR #19

### 3.1 Accepted conclusions

The following conclusions are sound and remain authoritative:

- A function graph is an ordinary generated IADT, not a new Core Term tag.
- Graph witnesses remain proof-relevant. They must not be collapsed by proof
  irrelevance or injected into global DefEq.
- Ordinary IADT elimination is the first proof interface. A generated tactic or
  special function-induction kernel rule is unnecessary.
- Local `*field` remains the current Match induction-hypothesis projection.
- Global `*f` is resolved by global definition identity, not by accidental
  TermDB hash-consing identity.
- Graph generation must happen while accepted source occurrence structure is
  available. Artifact Core readback cannot reconstruct it after `source_ast` is
  discarded.
- QuickSort requires separate worker and wrapper graphs, or an explicit checked
  bridge between them. The two worker recursive calls must remain visible.
- A QuickSort graph supplies the recursion-specific induction shape. It does
  not itself prove sortedness, permutation, comparator laws, or stability.
- The graph generator is untrusted. Existing type formation, IADT introduction,
  APP, MATCH, IH, Context, Substitution, Claim, and Derivation checks validate
  its products.

These decisions agree with the graph relations generated by Rocq `Function`
and Equations: the graph reflects the function's splitting or recursion tree,
and recursive calls become induction hypotheses. The Rocq documentation also
shows that graph generation and generation of a usable induction principle are
separate failure boundaries.

### 3.2 Rejected literal classifier

The following classifier from the research is only schematic and is not valid
in the current Core:

```text
*f : (x : A) -> @f x (f x)
```

Current A Program classifiers distinguish:

```text
f     : PI(A, Comp(E, B))
f x   : Comp(E, B)
@f    : A -> B -> @
```

The final index of `@f` requires a value of type `B`. `f x` is initially a
computation of type `Comp(E,B)`, so its Core computation node cannot be inserted
directly into the value index. The existing type-expression elaborator may,
however, ahead-of-time evaluate an accepted total and pure application through
`RETURN(v)` and insert the extracted `v`. This is A Program's ordinary static
computation policy and does not collapse the CBPV boundary.

Dependent CBPV makes this a real semantic issue, not parser inconvenience.
Dependent sequencing needs the value produced by a computation before a later
type may depend on it. The dCBPV+ literature describes this as dependent
Kleisli extension and also records the restrictions introduced by effects.

### 3.3 A separately generated witness is not yet a bridge

Generating these two objects independently from similar source trees is not
enough:

```text
f       : A -> Comp(E, B)
f_graph : A -> Comp(E, FResult)
```

The kernel can type-check both objects while having no object-language evidence
that their executions choose the same branches or produce the same result.
"Generated from the same compiler data" is a compiler invariant, not a theorem
inside A Program.

Rocq's historical function-graph interface exposes correctness/completeness
lemmas connecting the graph and `f x = y`. PR #19 intentionally removed such a
bridge by indexing `*f` directly with `f x`; that move is unavailable when
`f x` is a CBPV computation.

The implementation therefore needs one of:

1. retain a result relation such as `Returns`;
2. add a computation Identity theory strong enough to express the bridge; or
3. define executable `f` as the evidence-erasing projection of one certified
   execution.

This plan chooses option 3. It is the only option that removes `Returns` now
without adding another predicate with the same semantics.

### 3.4 Do not add a second semantic `DefinitionTree` authority

PR #19 proposes a broad mutable `DefinitionTree`. The current compiler already
has the relevant authorities:

- AST assignment and Match structure;
- `TypedOccurrenceGraph` operation identity and ordered edges;
- Match-case Contexts and refinement Substitutions;
- exact IH owner, scope, case, and field identities;
- accepted classifier, effect, and totality solutions;
- published definition labels and source-entry identity.

Copying all of these into a new independently mutable tree would recreate the
authority duplication recently removed from the compiler.

The replacement is an immutable, non-owning `prototype_accepted_definition_view`.
It contains stable IDs and read-only pointers into frozen accepted stores. It
does not own copied Contexts, Substitutions, branch classifiers, or Core Terms.
The generated AST/IADT objects are derived products, not a second source of
truth.

### 3.5 Computation-result Contexts are sequencing, not graph evidence

`PROTOTYPE_CONTEXT_EXTENSION_COMPUTATION_RESULT` is currently used by ordinary
zero-clause `COMPUTATION_FOLD` lowering. It identifies the value bound by a
specific producer computation. `Returns` later consumes this provenance for
`RETURNS_SEQUENCE_BINDING`, but the Context extension is not itself a Returns
proof.

It must not be migrated to a "graph-evidence origin" as suggested by FG8. The
correct migration is:

```text
PROTOTYPE_CONTEXT_EXTENSION_COMPUTATION_RESULT
	-> PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT

source_computation
	-> producer_computation
```

The sequencing binder remains part of CBPV and continues to be checked by
`COMPUTATION_FOLD_ELIM`. Only the public `Returns` consumer and its proof rule
are removed.

### 3.6 Effects and partiality require a restricted first fragment

A function graph for an effectful function is not merely a graph of returned
values. It must specify operation requests, handler interaction, abnormal
outcomes, and may/must behavior. Running `f` once and `*f` separately may also
duplicate effects.

The first accepted fragment is therefore:

- accepted `TOTAL` authority;
- closed empty effect row for the generated certified runner;
- deterministic normal return;
- current Lambda, APP, constructor, Match, zero-clause computation fold, and
  guarded IH structure;
- higher-order pure parameters may be invoked, but their internal graph is not
  fabricated;
- no operation request, nonzero-clause computation fold, handler, lambda exit,
  or `MAY_DIVERGE` witness.

`@f` for partial or effectful definitions may be designed later, but neither
`@f` nor `*f` is silently generated outside the accepted fragment in this plan.

### 3.7 Higher-order arguments require an explicit graph interface

The initial FGR4 wording proposed recording a comparator's observed Bool while
not requiring evidence that the Bool came from that comparator. That is
insufficient for an ordinary public IADT: a user could apply the generated
constructor with an arbitrary Bool and forge a trace unrelated to `le`.

The corrected contract is:

```text
LeGraph : A -> A -> Bool -> @
leCertified : (x : A) -> (y : A) -> LeResult LeGraph x y
```

The graph/certified interface generated for QuickSort receives these additional
proof-side parameters. The executable `quickSort` keeps its existing raw
comparator argument and operational classifier. A named comparator may supply
its generated `@le` and `*le`; a local or anonymous comparator without an
explicit interface is residual. This is not a renamed global `Returns`
predicate: the relation is definition-specific, ordinary IADT evidence and is
passed only where a higher-order call must be reflected in another graph.

The compiler may later elaborate a named `&lessOrEqual` argument by inserting
its graph interface. The first implementation may require the interface
explicitly, but it must never fabricate comparator observations.

## 4. Revised Semantic Contract

For a graph-enabled total pure named definition:

```text
f : (x : A) -> B
```

where surface arrows elaborate to computation-level `PI(A, Comp({},B))`, the
compiler generates two ordinary indexed families.

### 4.1 Function graph

```text
@f :=
	@\x : A =>
	@\y : B =>
	{
		... one constructor per admitted execution leaf ...
	};
```

An inhabitant of `@f x y` records the function-specific finite derivation that
computes output `y` from input `x`.

### 4.2 Certified result package

The compiler also generates an ordinary one-constructor IADT equivalent to the
dependent sum `Sigma (y : B). @f x y`:

```text
f.Result :=
	\x : A =>
	{
		returned : (y : B) -> @f x y -> *;
	};
```

Here `x` is a uniform parameter, not a family index. This is generated with the
existing parameterized inductive-family mechanism and is equivalent to a
dependent package `Sigma (y : B). @f x y`. It is not a built-in Sigma,
existential, or special kernel object.

### 4.3 Global `*f`

Global `*f` denotes the certified execution:

```text
*f : (x : A) -> f.Result x
```

Operationally, `*f x` is a computation. It executes the accepted body once and
returns:

```text
f.Result.returned y graph
```

where:

```text
y     : B
graph : @f x y
```

For multiple curried parameters and arguments, all rigid parameters and value
arguments appear as graph/result-family indices in source order.

### 4.4 Executable `f`

The executable definition is the evidence-erasing projection of the certified
execution:

```text
f x =
	COMPUTATION_FOLD(
		*f x,
		\packet =>
			packet @returned y graph => RETURN(y))
```

This gives a structural connection between execution and graph evidence. There
are not two separately trusted implementations.

The initial interpreter may construct graph evidence. A later backend proof
erasure pass may erase `graph` and the package constructor, but erasure is an
optimization with its own tests. It is not used to justify the first kernel
implementation.

### 4.5 Proof use

The raw-CBPV proof pattern is:

```text
propertyFromGraph :
	(x : A) -> (y : B) -> @f x y -> P x y;

certifiedProperty := \x : A => {
	run := *f x;
	run @returned y graph =>
		propertyFromGraph x y graph;
};
```

There is deliberately no claim `P x (f x)` with a computation placed in a
value index. A consumer that needs both the result and its proof uses the one
certified execution. A consumer that calls `f` only receives the erased result.

### 4.6 Naming and identity

- `@f` is valid only in type position and resolves a global definition.
- `*f` first resolves an induction-enabled local Match binder. If none exists,
  it resolves a global generated certified execution.
- A bare `@`, `@{...}`, and `@\index` retain their current meanings.
- Match clause `@constructor` remains parser-state-specific.
- Definition ownership uses assignment/source-entry identity locally and
  namespace plus export identity across artifacts.
- Equal Core Terms belonging to two source definitions do not share graph
  identity.
- Aliases do not acquire another definition's graph by TermDB equality.

## 5. Compiler Architecture

### 5.1 Graph request table

Add a compile-time table keyed by stable definition identity:

```text
prototype_function_graph_request
	owner_assignment_id
	owner_source_entry_id
	request_family
	request_certified_execution
	requesting_ast_id
	status
	reason
```

Requests are collected from all parsed `@f` and global `*f` AST nodes before
owner definitions are finalized. Generation is request-driven, not enabled for
every top-level function. A request recursively adds only the helper graph
dependencies required by its accepted definition view.

An imported `@f` or `*f` is available only if the producer artifact exported
the association. The importer never reconstructs a graph from Core readback.

### 5.2 Accepted definition view

Add an immutable view, not another arena:

```text
prototype_accepted_definition_view
	definition identity
	assignment AST root
	typed occurrence root
	accepted classifier
	accepted effect row and totality
	read-only ContextDB/SubstitutionDB references
	read-only Match cases and IH ownership
```

The view is created only after the owner occurrence has a solved classifier and
accepted replay authority. It is invalidated rather than mutated if fixed-point
resolution changes the owner.

### 5.3 Staged compilation

The current recursive assignment compiler cannot generate a graph before the
owner classifier and totality are solved. The new scheduler must use these
stages:

1. parse all source definitions and graph references;
2. collect graph requests and owner dependencies;
3. lower executable owner definitions and solve their ordinary constraints;
4. freeze accepted definition views;
5. generate synthetic ordinary IADT and witness AST for requested owners;
6. lower and solve generated objects through the normal compiler;
7. replace graph-enabled executable labels with the checked evidence-erasing
   projections;
8. lower definitions that depend on `@f` or `*f`;
9. freeze, replay, and publish the complete module.

Do not add a remap pass over already published labels. Definitions depending on
graph projections remain pending until stage 8.

### 5.4 Generated objects

Use existing AST builders and indexed-family lowering to generate:

- the graph type definition;
- graph constructors;
- the result-package type definition;
- its `returned` constructor;
- the certified execution Lambda/Match/COMPUTATION_FOLD term;
- the evidence-erasing executable projection;
- ordinary typed occurrences, constraints, Claims, and Derivations;
- generated-name and source-span provenance.

Do not add:

- `PROTOTYPE_TERM_FUNCTION_GRAPH`;
- `PROTOTYPE_TERM_FUNCTION_GRAPH_WITNESS`;
- a function-graph Judgement kind;
- a QuickSort-specific proof rule;
- a trusted graph-generation axiom.

### 5.5 Graph constructors

Each admitted execution leaf produces one constructor. Constructor fields may
include:

- source pattern binders needed by the result index;
- intermediate values produced by pure sequencing;
- graph evidence for recursive calls;
- graph evidence for requested named helper calls;
- existing dependent index/refinement evidence when it cannot be eliminated by
  the accepted branch Substitution.

Branch order, recursive-call order, and constructor field order are
deterministic and follow `TypedOccurrenceGraph` edge order.

Impossible branches are omitted only when the existing Match refinement status
is accepted as impossible. Residual branches make graph generation residual;
the generator must not guess.

## 6. Implementation Phases

### FGR0: Contract and boundary tests

- [ ] Add negative parser fixtures proving that bare `@`, indexed `@\i`, type
  literals, Match labels, local `*field`, and global `*f` remain unambiguous.
- [x] Add compile fixtures documenting the certified-result classifier and
  dependent package elimination.
- [x] Add a fixture proving that total/pure `f x` in a graph index is accepted
  only through existing ahead-of-time value extraction.
- [x] Record current QuickSort and result-evidence performance baselines.
- [x] Add static checks forbidding new graph-specific Core and proof tags.

Exit gate:

- the revised contract is executable in a handwritten `length.Result` model;
- no implementation depends on a computation being accepted in a value index.

### FGR1: Syntax, requests, and definition views

Primary files:

- `src/prototype/include/a_program/frontend/ast.h`
- `src/prototype/src/frontend/ast.c`
- `src/prototype/src/frontend/ast_inspect.c`
- `src/prototype/src/frontend/reader.c`
- `src/prototype/include/a_program/graph/compile_metadata.h`
- `src/prototype/include/a_program/graph/typed_occurrence_model.h`
- `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc`

Tasks:

- [x] Add `FUNCTION_GRAPH_REFERENCE` and
  `FUNCTION_GRAPH_WITNESS_REFERENCE` AST nodes.
- [x] Resolve `@` plus identifier in type-atom parser state as a graph request.
- [x] Resolve `*name` locally first, then globally.
- [x] Add stable owner identity and request-state records.
- [x] Add the immutable accepted-definition view API.
- [ ] Reject local Lambdas, higher-order variables, ambiguous names, nonfunction
  owners, and imports without graph exports.
- [x] Keep `::` as post-synthesis validation. It must not drive graph shape.

Exit gate:

- syntax and ownership resolve deterministically;
- no graph objects are generated yet;
- existing examples and local IH tests remain unchanged.

### FGR2: Small generated graph and certified execution

Add a generator module under `src/prototype/src/frontend/lowering/` with a small
declarative API. Do not enlarge `graph_construction.inc` with an unbounded
second compiler.

Tasks:

- [x] Implement accepted-view traversal for constructor, Match, RETURN,
  zero-clause COMPUTATION_FOLD, APP, and guarded IH.
- [x] Generate `@length` as an ordinary indexed family.
- [x] Generate `length.Result` as an ordinary indexed family.
- [x] Generate `*length` as the certified execution.
- [x] Compile every generated object through existing lowering and fixed point.
- [ ] Reuse accepted Context/Substitution IDs through view references; create
  new Context extensions only for genuinely generated binders.
- [x] Reject unsupported source operations with a precise residual diagnostic.

Exit gate:

- open `xs` is accepted by `*length xs`;
- elimination over `@length xs n` exposes the recursive graph hypothesis;
- malformed generated constructors fail ordinary accepted replay.

### FGR3: Make execution a projection of evidence

Tasks:

- [x] Generate the hidden certified runner before publishing the final `f`.
- [x] Generate `f` as sequencing/projection over that runner.
- [x] Ensure published references to `f` select the projection, not an earlier
  provisional body Term.
- [x] Keep one published definition authority; do not retain the old body as
  another published implementation.
- [x] Add runtime tests showing that `f x` and one `*f x` certified execution
  choose the same packaged output by construction.
- [ ] Add resource-usage accounting for the package and graph witness.

Exit gate:

- the graph connection is structural and contains no `Returns` premise;
- there is one operational execution, not a separately run function and proof;
- current normalization results are preserved after evidence projection.

### FGR4: QuickSort dependency closure

Initial dependency closure:

```text
measure
partition
append
quickSortAcc
quickSort
```

Tasks:

- [x] Replace syntactic root-Lambda counting with an accepted dependent binder
  path. `quickSortAcc`'s final `input` binder is introduced under the `Acc.acc`
  branch, so scanning only consecutive root Lambdas loses a real argument.
- [x] Generalize precise graph indices from `(input, output)` to the full
  dependent argument spine plus output. Project each later argument classifier
  through the binding map built by earlier arguments.
- [x] Implement recursive accepted-AST traversal for Lambda, Match, RETURN,
  APP, zero-clause COMPUTATION_FOLD, and computation blocks. Unsupported effect
  clauses must produce a residual instead of falling back to `executed`.
- [x] Lift the motive of the Match that owns an IH from ordinary output to the
  generated result package. The lifted local IH must return the recursive
  output and graph witness together; running the old IH and forging a graph
  afterward is forbidden.
- [x] Generate helper graph/result packages only when required by the requested
  QuickSort graph.
- [x] Add proof-side higher-order graph parameters for the comparator. Named
  comparators use their `@le`/`*le` association; local or anonymous functions
  without an explicit graph interface are residual.
- [x] Translate local `*down` IH occurrences to recursive
  `@quickSortAcc` graph evidence using exact IH owner/scope/case/field IDs.
- [x] Preserve both lower and upper recursive call packages in source order.
- [x] Generate a public wrapper graph that hides irrelevant raw `Acc` fields via
  a checked constructor/witness term, not via metadata deletion.
- [x] Add one ordinary IADT-elimination theorem that follows the QuickSort
  recursive shape. Sortedness and permutation remain separate theorem work.

Exit gate:

- an open QuickSort input produces a certified graph package;
- the partition branch exposes exactly two recursive graph premises;
- no QuickSort-specific Core tag, proof kind, or trusted rule exists.

Implementation stop condition: no generic `executed(args, output)` constructor
may be used to satisfy this gate. For `quickSortAcc`, a successful test must
eliminate its generated partition constructor and receive two distinct graph
premises corresponding to the lower call followed by the upper call.

### FGR5: Artifact v83

Primary files:

- `src/prototype/spec/artifact_v83.schema`
- `src/prototype/include/a_program/artifact/interface.h`
- `src/prototype/include/a_program/artifact/wire_v83.h`
- `src/prototype/src/artifact/interface.c`
- `src/prototype/src/artifact/publication/`
- `src/prototype/src/artifact/wire_v83.c`
- `src/prototype/src/artifact/link.c`

Tasks:

- [x] Archive v82 and introduce v83 with no read compatibility facade.
- [x] Add an explicit owner-export to graph-family/result-family/certified-runner
  association table.
- [x] Publish generated IADTs, constructors, Terms, Claims, and Derivations by
  the ordinary dense publication path.
- [x] Keep generated source recipe data out of the artifact unless a later
  regeneration feature proves it necessary.
- [x] Import graph ownership only through the explicit association table.
- [x] Reject malformed or out-of-range owner/graph association identities during
  read, link, and replay.

Exit gate:

- `@f` and `*f` survive write/read/link/replay;
- import never reconstructs a graph from erased Core;
- two definitions sharing one Core Term retain distinct graph ownership.

### FGR6: Migrate proof fixtures

Tasks:

- [ ] Replace the named-function parts of
  `result_evidence_dependent_check.p` with certified graph execution.
- [x] Add permanent generated `length` graph tests.
- [x] Add permanent QuickSort worker/wrapper graph tests.
- [ ] Add negative tests for wrong owner, wrong output index, wrong recursive
  premise, wrong Context, forged constructor, partial owner, effectful owner,
  and missing imported graph export.
- [x] Preserve tests for ordinary CBPV dependent sequencing independently of
  `Returns`.

Exit gate:

- no named-function property fixture needs `#.Returns`;
- sequencing tests prove that the result Context remains valid without a
  public result predicate.

### FGR7: Remove `Returns`

Current audit: `Returns`-related spellings occur on 394 lines in 46 prototype
files, including archived schemas. Archived schemas remain immutable; active
implementation and v83 must contain no active `Returns` contract.

Remove:

- [x] `PROTOTYPE_AST_RETURNS_WITNESS`.
- [x] `PROTOTYPE_AST_TYPE_EXPR_RETURNS`.
- [x] `PROTOTYPE_TERM_RETURNS_TYPE_FORMER`.
- [x] `PROTOTYPE_TERM_RETURNS_WITNESS_FORMER`.
- [x] `RETURNS_TYPE_FORMATION` proof kind.
- [x] `RETURNS_EVALUATION` proof kind.
- [x] `RETURNS_SEQUENCE_BINDING` proof kind.
- [x] `TERMINATES_FROM_RETURNS` proof kind.
- [x] `#.Returns` and `#.returns` parser branches.
- [x] Returns-specific canonicalization, conversion, HOTT/dimension action,
  replay, artifact, metadata, and diagnostic code.
- [x] Returns fixtures and the current `test_result_evidence.sh` contract.

Preserve and clarify:

- [x] `RETURN` as the CBPV computation constructor.
- [x] `COMPUTATION_FOLD` as sequencing and handler fold.
- [x] computation result binders, renamed to sequence result binders.
- [x] direct `Terminates` formation and witnesses justified by accepted
  `TOTAL` computation classifiers.
- [x] normalization and evaluation results in TermDB.

Refactor:

- [x] Replace `result_evidence.inc` with a totality-specific module after Returns
  code is deleted.
- [x] Remove Returns-only metadata arrays and counters.
- [x] Rename Context fields and artifact enum values from computation-result
  evidence terminology to sequencing provenance.
- [x] Update `src/prototype/README.md` and active schemas.

Exit gate:

```text
rg -n "PROTOTYPE_.*RETURNS|#.Returns|#.returns|TERMINATES_FROM_RETURNS" \
	src/prototype/include src/prototype/src src/prototype/spec/artifact_v83.schema \
	src/prototype/tests
```

returns no active implementation hits. Archived v80-v82 schemas are excluded.

### FGR8: Verification and performance

Correctness:

- [x] Build with `-std=c11 -Wall -Wextra` and no new warnings.
- [x] Run focused parser, IADT, Context, Substitution, CBPV, graph, QuickSort,
  artifact, link, and replay tests.
- [x] Run the complete prototype integration suite.
- [x] Run deterministic artifact regeneration checks.
- [x] Validate generated Claim premise ordering during accepted replay.
- [ ] Verify generated graph evidence remains proof-relevant under HOTT action.

Performance baselines measured at the planning revision:

```text
test_if8_fuel_free_quicksort.sh    2.463 s total
test_result_evidence.sh            3.761 s total
```

Performance gates:

- [x] Report source compile, fixed point, proof materialization, accepted replay,
  publication, readback, and artifact equality separately.
- [x] Report Term, typed occurrence, Context, Substitution, Claim, Derivation,
  type declaration, and artifact byte deltas.
- [x] Do not add Context/Substitution index rebuilds to graph generation.
- [ ] Require requested-only graph generation to leave unrelated modules within
  measurement noise.
- [ ] Require the graph-enabled QuickSort source compile to stay within 1.25x
  of the pre-graph source compile unless a measured proof-size reason is
  documented and accepted.
- [ ] Compare v83 graph artifacts against v82 Returns artifacts.
- [x] Report per-file added/deleted lines and total source-line delta.

2026-08-23 FGR4 measurement on the same build and host:

```text
                                      IF8 source   graph-enabled source
wall time                                0.421 s              5.109 s
graph build                              0.003 s              0.032 s
fixed point                              0.075 s              1.114 s
proof materialization                    0.037 s              0.378 s
evidence closure                         0.022 s              0.130 s
accepted replay                          0.183 s              2.166 s
TermDB terms                              8,650               42,730
typed occurrences                          755                2,024
published Contexts                         250                  869
published Substitutions                    546                1,748
published Claims / Derivations          703 / 707        2,106 / 2,120
artifact bytes                          482,520            1,590,285
artifact write wall                       0.468 s              5.493 s
artifact readback wall                       n/a               0.619 s
Context/Substitution index rebuilds         0 / 0                0 / 0
```

The graph-enabled source is about `12.1x` the no-request source compile, so the
`1.25x` gate is not met. This is not explained by Context/Substitution index
rebuilds. The requested dependency closure adds 19 type declarations, 25
constructors, 1,269 typed occurrences, and about 34,000 live TermDB terms; its
accepted certificate image is about three times the base artifact. The largest
remaining phases are accepted replay and fixed point. This remains a measured
performance obligation rather than being waived as measurement noise.

During this audit, immutable `(Substitution, Binding) -> Term` lookup caching
reduced graph source compilation from `10.95 s` to `5.11 s` and Term formation
requests from about `50.9 million` to `1.0 million`. Telescope field domains are
also projected in one left-to-right pass instead of rebuilding every prefix.
These are rebuildable runtime accelerators; neither is artifact authority.

## 7. Required Invariants

1. TermDB remains the erased computation graph. Graph ownership and source
   meaning remain in typed/source layers.
2. Value and computation categories are not split into duplicate APP/LAMBDA
   Core tags.
3. `@f` and `*f` never resolve by Core Term equality alone.
4. `::` remains a post-synthesis expectation, not a graph-generation input.
5. Graph constructors and witnesses are ordinary generated A Program objects.
6. The generator cannot publish a Claim directly. It must pass the ordinary
   constraint, proof materialization, and accepted replay pipeline.
7. The executable function is a projection of certified execution whenever a
   graph interface is published.
8. Graph evidence never changes DefEq or the global normalizer.
9. Partial/effectful definitions are rejected by the first graph fragment.
10. Removing `Returns` does not remove dependent sequencing provenance.
11. No backward-compatibility reader or remap layer is retained for v82.
12. Migration convenience is never preferred over one semantic authority.

## 8. Explicit Non-goals

- A tactic language or automatic functional induction tactic.
- A special eliminator generated outside ordinary IADT elimination.
- Automatic sortedness or permutation proofs for QuickSort.
- General graph semantics for effects, exceptions, nondeterminism, or handlers.
- Coinductive divergence graphs.
- Graph reflection for arbitrary anonymous computations.
- Graph generation for local higher-order variables without an explicit graph
  interface.
- Built-in Sigma, quotient, proof irrelevance, or equality reflection.
- Reconstructing source definitions from artifact Core Terms.

## 9. Risks and Stop Conditions

Stop the phase and revise this plan if any of the following occurs:

- `*f` requires placing a computation in a value index;
- executable `f` and certified `*f` become separately authoritative bodies;
- graph generation needs to duplicate mutable Context or Substitution stores;
- accepted replay requires a graph-specific proof kind;
- a generated graph for QuickSort loses either recursive call;
- removing `Returns` breaks ordinary zero-clause computation sequencing;
- the only repair is an internal predicate semantically identical to Returns;
- graph generation for one requested definition scans all Terms or all Claims;
- artifact import attempts to infer graph ownership from a canonical Term key;
- proof erasure is assumed before its semantic preservation is tested.

## 10. Source Guidance

Primary theory used to audit the proposal:

- Matthijs Vakar, [A Framework for Dependent Types and Effects](https://arxiv.org/abs/1512.08009).
- Ana Bove and Venanzio Capretta, [Modelling General Recursion in Type Theory](https://doi.org/10.1017/S0960129505004822).
- Matthieu Sozeau and Cyprien Mangin, [Equations Reloaded](https://sozeau.gitlabpages.inria.fr/www/research/publications/Equations_Reloaded-ICFP19.pdf).
- [Rocq Functional Induction documentation](https://rocq-prover.org/doc/V9.2.0/refman/using/libraries/funind.html).

These sources justify function-specific graphs, recursion-shaped induction, and
the need to connect a function to its graph. They do not directly specify A
Program's combination of raw CBPV, generated IADTs, Higher Observational
identity, and artifact replay. The certified-result package and executable
projection in this plan are therefore A Program's explicit synthesis of those
requirements, not a claim that one cited paper already contains this exact
calculus.

## 11. Completion Report Template

### 11.1 2026-08-22 implementation checkpoint

```text
Implemented revision: 01537a7
Artifact version: v83

Graph-enabled definitions: unary direct-Match recursion; conservative curried
                           pure-total execution packages
Unsupported definitions: structure-preserving nested Match/fold graphs,
                         higher-order calls without graph interfaces,
                         effectful and partial definitions

Correctness tests: focused function graph, totality, CBPV, IADT, and QuickSort
Negative tests: unknown owner, computation-valued graph index, removed Returns
Artifact/replay tests: v83 association read/write/link and malformed index
Full suite: 41/41 passed in 85.850 s

Per-file additions/deletions: reported by staged diff
Total additions: 4,891
Total deletions: 2,282
Net change: +2,609

Returns active references remaining: none; negative syntax fixture only
Known residual theory obligations: FGR4 certified motive lifting, two recursive
                                   QuickSort premises, wrapper graph theorem,
                                   higher/effectful function graphs
```

The 2026-08-22 checkpoint did not claim QuickSort graph induction was complete:
its graph request was residual until the structure-preserving generator and
comparator interface became available. FGR4 below closes that implementation
boundary.

### 11.2 2026-08-23 FGR4 implementation checkpoint

```text
Implemented revision: 9c981b8
Artifact version: v83

Graph-enabled definitions: requested pure-total dependent functions; nested
                           Match/IH recursion; multiple recursive calls; named
                           higher-order binary Bool callbacks with @f/*f;
                           Acc QuickSort worker and public wrapper
Unsupported definitions: anonymous higher-order callbacks without an explicit
                         graph interface; effectful or partial function graphs

Correctness tests: dependent spine, two recursive calls, QuickSort dependency
                   closure, ordinary graph IADT elimination, CBPV sequencing
Negative tests: unknown owner, computation-valued index, coarse forged graph,
                malformed artifact association
Artifact/replay tests: v83 association write/read/link/replay and deterministic
                       regeneration for generated graph artifacts
Full suite: 41/41 passed in 103.977 s

Before/after graph source timing: 10.95 s -> 5.11 s after reindex lookup cache
Before/after focused graph test: 13.86 s -> 6.31 s
Base/graph artifact bytes: 482,520 -> 1,590,285
Base/graph TermDB terms: 8,650 -> 42,730
Base/graph typed occurrences: 755 -> 2,024

Total implementation additions: 11,425
Total implementation deletions: 1,946
Net implementation change: +9,479

Returns active references remaining: negative removed-syntax fixture only
Known residual obligations: graph-specific HOTT proof-relevance test; accepted
                            replay/fixed-point performance; complete negative
                            ownership/effect/import matrix
```

Per-file implementation delta (`added deleted path`) at this checkpoint:

```text
1 1 src/prototype/calculus.h
3 0 src/prototype/include/a_program/artifact/interface.h
26 13 src/prototype/include/a_program/frontend/ast.h
7 0 src/prototype/include/a_program/graph/compile_metadata.h
4 2 src/prototype/include/a_program/graph/typed_occurrence_model.h
23 0 src/prototype/include/a_program/kernel/context.h
16 0 src/prototype/include/a_program/kernel/judgement/conversion.h
12 0 src/prototype/include/a_program/kernel/judgement/rules.h
2 1 src/prototype/include/a_program/kernel/judgement/types.h
12 4 src/prototype/spec/artifact_v83.schema
21 5 src/prototype/src/artifact/interface.c
5 0 src/prototype/src/artifact/link.c
45 8 src/prototype/src/artifact/publication/closure_marking_and_slices.inc
55 20 src/prototype/src/artifact/publication/dense_publication.inc
1 1 src/prototype/src/artifact/publication/section_writers.inc
11 2 src/prototype/src/artifact/publication/writer.inc
15 5 src/prototype/src/artifact/wire_v83.c
21 1 src/prototype/src/core/term/evaluation_and_conversion.inc
16 8 src/prototype/src/core/term/substitution.inc
0 1 src/prototype/src/driver/compiler_session.c
9 9 src/prototype/src/driver/program_storage.c
8 8 src/prototype/src/driver/read_file.c
35 18 src/prototype/src/frontend/ast.c
4 2 src/prototype/src/frontend/ast_inspect.c
7226 1306 src/prototype/src/frontend/function_graph.c
602 19 src/prototype/src/frontend/lowering/constraint/branch_refinement_and_motives.inc
113 39 src/prototype/src/frontend/lowering/constraint/classifier_and_computation_propagation.inc
65 17 src/prototype/src/frontend/lowering/constraint/effect_propagation_and_residuals.inc
940 250 src/prototype/src/frontend/lowering/constraint/evidence_and_freeze.inc
6 2 src/prototype/src/frontend/lowering/constraint/model_generation_and_index.inc
406 45 src/prototype/src/frontend/lowering/context_and_type_lowering.inc
55 36 src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc
695 47 src/prototype/src/frontend/lowering/graph_construction.inc
2 1 src/prototype/src/graph/typed_occurrence/graph_validation.inc
2 1 src/prototype/src/graph/typed_occurrence/storage.inc
85 20 src/prototype/src/kernel/context.c
165 0 src/prototype/src/kernel/rules/elimination_app.inc
45 2 src/prototype/src/kernel/rules/formation_early.inc
28 10 src/prototype/src/kernel/rules/introduction/structural.inc
59 0 src/prototype/src/kernel/type_declaration.c
159 25 src/prototype/src/kernel/typing/accepted_replay.inc
1 0 src/prototype/src/kernel/typing/candidate_replay.inc
86 16 src/prototype/src/kernel/typing/classifier_solver.inc
184 0 src/prototype/src/kernel/typing/conversion.inc
7 2 src/prototype/tests/checks/shared_term_reindex_check.c
1 0 src/prototype/tests/checks/spec_enum_check.c
72 0 src/prototype/tests/integration/test_function_graph_certified_execution.sh
8 0 src/prototype/tests/fixtures/negative/function_graph_coarse_forgery.p
32 0 src/prototype/tests/fixtures/typing/function_graph_dependent_spine_check.p
30 0 src/prototype/tests/fixtures/typing/function_graph_two_recursive_calls_check.p
```

At completion, append:

```text
Implemented revision:
Artifact version:

Graph-enabled definitions:
Unsupported definitions:

Correctness tests:
Negative tests:
Artifact/replay tests:
Full suite:

Before/after timing:
Before/after artifact bytes:
Before/after graph-store counts:

Per-file additions/deletions:
Total implementation additions:
Total implementation deletions:
Net implementation change:

Returns active references remaining:
Known residual theory obligations:
```
