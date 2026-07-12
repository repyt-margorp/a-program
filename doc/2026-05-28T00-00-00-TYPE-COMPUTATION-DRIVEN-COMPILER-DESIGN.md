# Type-Computation-Driven Compiler Design

This note records the intended compiler architecture for the current prototype
language. The goal is to make compilation advance by computing classifier/type
information, rather than by mixing parser-time guesses, source names, and
one-shot type checking into the graph-lowering pass.

The design here is based on the current prototype state:

- source is parsed into an AST definition set
- `:=` assignments build computation graph nodes
- `:` declarations introduce body-less external/global names
- `::` creates type expectations/checking obligations
- finite constructors and match cases are part of the computation language
- `JudgementDB` stores graph-level facts such as `subject : classifier`
- `Resolution` metadata traces match-constructor resolution items and
  transitions
- `UniverseGraph` is derived summary data, not the primary graph

## Core Principle

Compilation should be a monotone computation over graph facts.

The compiler starts with source syntax, lowers the syntax into a computation
graph, and then repeatedly expands the set of known judgements. Every successful
typing/computation rule adds facts to `JudgementDB` or resolution metadata. New
facts may unlock more graph lowering, match-case resolution, motive construction,
expectation checks, universe constraints, or import/link decisions.

This gives the compiler a frontier model:

```text
source
  -> AST definition set
  -> initial graph + obligations + unresolved resolution items
  -> judgement frontier
  -> resolution frontier
  -> expectation frontier
  -> universe frontier
  -> fixed point or diagnostics
```

The graph is the computation object. Judgements, expectations, resolution items,
universe constraints, and source labels are metadata over that graph. They may
drive compilation, but they must not be confused with evaluation nodes unless
the language explicitly represents them as ordinary terms.

## Required Phase Split

The compiler should eventually use these phases.

### 1. Read Source

Input text is read by the host compiler environment. Code literals are graph
terms, but the act of reading a file is still outside the pure object language.

The source reader should produce:

- AST nodes with spans
- AST type expressions with spans
- top-level assignment entries
- top-level declaration entries
- top-level expectation entries
- source-order definition lists
- a name index for diagnostics and lookup

This phase must preserve source occurrences. It must not intern terms or decide
semantic equality.

### 2. Build Initial Graph

AST-to-graph lowering should create graph terms for computation structure:

- `APP(left, right)`
- `LAMBDA(binder, body)`
- `MATCH(scrutinee, cases)`
- constructor values
- type-former applications
- telescope structure for type-former arity
- primitive terms such as `PI` and `Code`
- code literals
- induction-hypothesis references bound to match frames

This phase may also add immediate, local, rule-justified judgement facts where
the source syntax itself provides all required data:

- type definitions generate type-former classifiers
- constructors generate constructor classifiers
- lambda binders generate binder classifiers

Code literals and host intrinsics are now lowered as graph terms only. Their
classifiers are synthesized by the later classifier-inference phase. Intrinsic
terms carry the type-symbol argument needed by `code_to_nat` and `nat_to_code`,
so this information is not lost when lowering stops before type synthesis.

It should not treat `::` expectations as proven classifiers. An expectation is
a check request, not a synthesized fact.

### 3. Build Resolution Items

Any source construct whose graph meaning depends on type/classifier information
should become a resolution item. Match constructor labels are the first example.

For a match branch:

```p
n @succ k => body
```

the AST contains source syntax `@succ`, but the graph branch should eventually
refer to the constructor by computation identity:

```text
case constructor_owner = Nat
case constructor_id    = succ index
```

The compiler should record:

- item kind: match-constructor
- source AST node/span
- scrutinee graph node
- branch index
- source label symbol
- current state: unresolved, resolved, or error
- transition iteration

The item is resolved only when the scrutinee classifier is known strongly enough
to choose a constructor namespace. This is why resolution is driven by type
computation.

### 4. Expand Judgements

Judgement expansion is the main compiler engine.

Each expansion rule should be a small API in `typing.c` or a later checker
module. The caller should not duplicate the rule. The API should inspect the
needed graph neighborhood and either:

- add a new judgement
- report "not enough information yet"
- report malformed graph or failed check

Current examples:

- `constructor : owner_type`
- `lambda : PI(domain, codomain_family)`
- `app : instantiated_codomain`
- `match_motive : universe`
- `match : motive(scrutinee)`
- `code_literal : Code`
- `subject expects expected`

The important rule is that `HAS_TYPE` facts are synthesized/proven facts.
`EXPECTS_TYPE` facts are obligations. A future checker may turn a successfully
checked expectation into a checked `HAS_TYPE` or equality fact, but the compiler
must not silently use expectation as proof.

### 5. Resolve Constructor-Dependent Graph Edges

After each judgement expansion step, the compiler should revisit unresolved
resolution items. A match-constructor item can be resolved if:

- the scrutinee has a known classifier
- that classifier points to a finite/inductive type namespace
- the source case label exists in that namespace
- the number of pattern binders matches the constructor field count
- every pattern binder classifier can be computed from the constructor fields

Resolution changes the graph-level case data from source label to constructor
identity. It also registers pattern-binder judgement facts when those facts are
justified by the resolved constructor.

### 6. Build Motives as Classifier Terms

Match is the construct where bottom-up typing is most naturally staged.

Dynamically, a match can evaluate by selecting a constructor branch. Statically,
a theorem-prover-grade match needs a classifier of the form:

```text
match(scrutinee, branches) : motive(scrutinee)
```

where `motive` is itself a term:

```text
motive : ScrutineeType -> Universe
```

For the current language, the compiler should build a motive only from branch
classifiers that are already known. It should not invent a collapsed result type
just because all visible branch classifiers look textually similar. The motive
is a graph term, usually a lambda around a match whose branch bodies are
classifier terms.

If all branch classifiers reduce to the same type, a later normalization or
join pass may record that collapsed classifier. That is a separate type
computation, not a parser rule.

### 7. Check Expectations

Expectations from `::` should be processed after enough graph and judgement
facts exist.

For:

```p
add :: Nat -> Nat -> Nat;
add := \n : Nat => ...
```

the compiler should:

- compile the expected type expression into a graph term
- record `EXPECTS_TYPE(add_term, expected_type)`
- later check whether the synthesized classifier for `add_term` is compatible
  with `expected_type`

For expression-level:

```p
body :: A
```

the graph value remains `body`. The expectation is metadata tied to the graph
node for `body` and the source span of the ascription.

Expectations should drive checking, but they should not be used as ordinary
classifier hints during graph lowering. If the compiler needs a hint to resolve
a construct, that hint should be represented explicitly as a resolution
assumption or pending obligation, not smuggled through `EXPECTS_TYPE`.

Current prototype compromise:

- `EXPECTS_TYPE` is recorded but is not used to force classifier synthesis.
- The active implementation is still a synthetic-classifier pass.
- When a synthetic classifier contains a motive application such as
  `APP(motive_lambda, scrutinee)`, the `JudgementDB` rule for `MATCH` records
  the match at that classifier. normalization equality/WHNF can then observe that the motive
  application reduces to another classifier.
- If an earlier provisional classifier was needed to type an induction
  hypothesis, the checker keeps it separate and later adds a conversion only
  when the generated motive application is normalization equality to that provisional
  classifier.

For example:

```text
MATCH(n,
  zero -> Nat -> Nat,
  succ -> Nat -> Nat)
```

is first recorded as:

```text
APP(LAMBDA(x, MATCH(x,
  zero -> Nat -> Nat,
  succ -> Nat -> Nat)), n)
```

and normalization equality can reduce that classifier to:

```text
Nat -> Nat
```

because the scrutinee is irrelevant to the classifier after every branch agrees.
This is not expectation discharge and it is not a primitive typing rule for
match. It is ordinary type computation over the generated motive graph.
The later expectation-checking phase should compare the already synthesized
classifier with `EXPECTS_TYPE`; it should not be required for the examples that
can be classified bottom-up.

Current design conclusion for `::`:

- `::` is not required for the current InductiveType and Match fragment.
- Lambda binders already carry classifiers through `\x : A => ...`.
- Constructors synthesize classifiers from type definitions.
- Match pattern binders synthesize classifiers from the scrutinee classifier and
  constructor fields.
- Induction hypotheses synthesize classifiers from the match frame and the
  structurally recursive field.
- Match expressions synthesize motive graphs from branch classifiers. Constant
  motives may reduce by normalization equality/WHNF after the motive has been built.

Therefore `::` must not be treated as an input to synthetic typing or match
resolution. In the current prototype it is only an obligation/assertion record:
`term :: A` means "later verify that the already synthesized classifier of
`term` is compatible with `A`." The expectation-discharge pass is intentionally
not implemented yet, and examples that can be classified bottom-up should not
depend on it.

Future uses for `::` are limited to cases where an explicit check boundary is
useful, such as ambiguous literals, external declarations, module/link
interfaces, dependent/indexed motives that cannot be collapsed synthetically,
or tests that assert a classifier. These are later checking features, not the
current classifier-synthesis mechanism.

### 8. Derive Universe Constraints

Universe constraints should be computed from graph and judgement facts after
initial graph construction. The universe graph is a summary and cache.

The pass should collect:

- every type-former result classifier
- every universe variable introduced by `@`
- every `T : Universe(?u)` relation
- every dependency requiring `u <= v` or `u < v`
- every positive-weight cycle that would make universe assignment impossible

The universe pass should not be created directly by the parser. It should be
derived from the graph and `JudgementDB` frontier so it can be recomputed after
imports, expectations, motive expansion, or normalization add more information.

## Fixed-Point Compilation

The intended compile loop is:

```text
build_initial_graph()

iteration = 0
while true:
  before = frontier_snapshot()

  expand_available_judgements()
  resolve_available_items()
  build_available_motives()
  check_available_expectations()
  collect_or_update_universe_constraints()

  after = frontier_snapshot()
  record_iteration(before, after)

  if after has no new facts:
    break
  iteration++

finish:
  if required items remain unresolved:
    report diagnostics
  if required expectations remain unchecked:
    report diagnostics or leave them as linker obligations
  if universe constraints are inconsistent:
    report diagnostics
  otherwise:
    publish labels and compiled-file summary
```

This loop is not "retry parsing". Parsing happens once. AST-to-graph lowering
also should not repeatedly rebuild source syntax. Iteration happens over graph
facts and metadata frontiers.

## What Counts as Required

Not every missing fact is immediately a compile error.

Required for single-file execution:

- names used by evaluated terms must resolve
- match constructors needed by executable graph nodes must resolve
- constructor arities must match pattern binders
- `main`, if present, must evaluate without unresolved runtime primitives

Required for theorem-prover checking:

- every checked theorem/proof boundary must have a verified classifier
- every expectation selected for checking must be discharged
- every match used inside a checked boundary must have a valid motive
- every universe constraint in the checked boundary must be solvable

Allowed as future linker obligations:

- external declarations from `name : Type;`
- imports whose compiled-file summaries are not loaded yet
- target/runtime primitives not available to the host compiler
- expectations outside the selected static-check boundary

The compiler should distinguish these states explicitly. A term may be
executable under the dynamic evaluator while still not statically certified as a
theorem-prover object.

## Current Prototype Alignment

The current implementation already has some of the right pieces:

- `prototype_ast_db` stores assignments, declarations/expectations, spans, and
  a def index
- `prototype_term_db` is the computation graph storage
- `prototype_judgement_db` stores `HAS_TYPE`, `EXPECTS_TYPE`, equality, and
  is-type relations
- `prototype_judgement_delta` allows failed local compile attempts to rewind
  temporary judgement facts
- `prototype_resolution_item` and `prototype_resolution_event` trace
  match-constructor resolution
- `prototype_universe_collect(...)` derives a universe summary after graph
  compilation

But the current implementation is not yet the final architecture:

- there is only one resolution iteration in practice
- AST-to-graph lowering still performs some judgement expansion immediately
- graph classifier lookup no longer treats `EXPECTS_TYPE` as a classifier hint;
  remaining expectation handling is a later compatibility check
- match motive construction is now in the classifier-inference phase, but it
  still uses a narrow internal provisional classifier table while branch
  classifiers and induction-hypothesis classifiers catch up to each other
- universe collection is still shallow and type-declaration-oriented
- expectation checking is recorded but not discharged by a real checker
- there is no compiled-file/link summary format yet

These are implementation-stage limitations, not desired language semantics.

## Recommended Refactoring Direction

The next implementation steps should be ordered as follows.

### Step 1. Separate Classifier Hints from Expectations

Introduce a distinct metadata/relation kind for facts that are permitted to
drive graph resolution.

```text
HAS_TYPE        proven/synthesized classifier
EXPECTS_TYPE    requested check, not proof
RESOLUTION_HINT temporary assumption or source-local hint, not proof
```

If a match requires a classifier and only `EXPECTS_TYPE` is known, the compiler
should create a pending resolution item instead of pretending the expectation is
a classifier.

### Step 2. Move Match Motive Building Out of Branch Compilation

Branch compilation should build branch graph bodies and pattern-binder scopes.
Motive construction should be a later pass over resolved match nodes.

This avoids treating a branch body classifier that happens to be available
during lowering as the final static account of the match.

### Step 3. Implement a Real Frontier Loop

Resolution metadata should record, for each iteration:

- unresolved items before
- items resolved during the iteration
- new judgements added during the iteration
- expectations discharged during the iteration
- unresolved items after

This is the compiler-level analogue of "type computation made compilation
advance".

### Step 4. Make APP Classification Purely Rule-Driven

`APP(f, x)` should get a classifier only through judgement expansion:

```text
f : PI(A, B)
x : A
----------------
APP(f, x) : B(x)
```

The graph node itself should remain plain `APP(left, right)`.

This keeps the computation graph small and lets type-level computation be just
ordinary graph computation plus judgement expansion.

### Step 5. Add Normalization for Classifier Terms

The checker needs a controlled normalizer for type/classifier terms:

- beta-reduce lambda application
- reduce type-former telescope applications
- reduce match when the scrutinee is a known constructor
- leave stuck terms explicitly stuck

This is necessary before expectation checking, motive comparison, and universe
constraints can become robust.

### Step 6. Define Static-Check Boundaries

The language should support different certification levels:

- dynamic compile/evaluate
- host-available primitive evaluation
- expectation checking
- theorem/proof checking
- whole-program universe checking

The compiler should not force the most expensive theorem-prover checks on every
ordinary execution path. But when a boundary is selected, the facts required by
that boundary must be fully checked.

## Design Invariant

The central invariant should be:

```text
No compiler phase may use a relation as proof unless that relation was produced
by the rule API responsible for that proof.
```

This prevents the current major risk: using source expectations, parse-time
names, or partial branch information as if they were synthesized classifier
facts.

Type computation can then safely drive compilation because every newly available
fact has a local introduction rule, a graph neighborhood that justifies it, and
a recorded frontier transition showing when it became available.

## Cubical Equality Direction

This section records the current design intent for adding a Cartesian-cubical
equality layer without growing the surface syntax prematurely.

The preferred direction is not to add new notation such as `[]`, `[[`, `]]`, or
special path binders at the source level first. The source language already has
enough syntactic machinery:

- type declarations and body-less declarations
- ordinary lambda/application
- match syntax
- namespace lookup
- `::` expectations
- intrinsic names under the compiler namespace `#`

Cubical structure should therefore be introduced as an intrinsic semantic
profile over selected graph nodes, not primarily as a parser extension.

The likely intrinsic namespace shape is:

```text
#.Interval
#.left
#.right
#.Path
#.refl
#.coe
#.hcomp
```

`#.Interval` may look like a two-endpoint object, but it must not behave like an
ordinary finite type. The important distinction is the eliminator:

```text
ordinary finite type:
  arbitrary match branches are allowed

cubical interval:
  endpoint substitution is allowed
  interval match is allowed only when all branches are judgementally equal
```

For example, a source-level interval match of the shape:

```text
i @left  => u
  @right => v
```

should be accepted for `#.Interval` only when `u` and `v` have compatible
classifiers and `JudgementalEq(u, v)` succeeds. This makes the interval match a
coherent/constant eliminator, not a constructor for arbitrary paths. In
particular, it prevents constructing a path from `Bool.true` to `Bool.false`
merely by branching on the interval endpoint.

Non-trivial paths should come from explicit cubical operations or trusted path
sources, not from unrestricted interval pattern matching:

- existing path variables
- `#.refl`
- `#.coe`
- `#.hcomp`
- HIT path constructors, once supported
- trusted postulates/imports, under an explicit trust boundary

`PathMap`/`ap` should not be a primitive. If the language has enough cubical
structure, it should be a derived library operation. The primitive pressure
should stay on the Kan-style operations `#.coe` and `#.hcomp`, plus the minimum
path/endpoint computation needed to use them. This keeps the intrinsic set
small and prevents the compiler from accumulating ad hoc path operations.

The important equality split is:

```text
JudgementalEq(A, B)
  compiler-side definitional equality used by checking and normalization

p : #.Path Universe A B
  graph/language object witnessing a path between types

#.coe / #.hcomp
  operations that use path/cubical data to compute across such equalities
```

Adding `#.Path` does not mean that `A` and `B` become judgementally equal.
Judgemental equality remains the compiler's normalize-and-compare relation.
`#.Path` is the internal object-language way to discuss equality when
judgemental equality is not enough.

## Graph Identity and Display Names

The lowest computation graph should not treat programmer binder names as
semantic identity. Names such as `A`, `B`, `x`, and `y` are source/display
metadata used to point at graph nodes and print diagnostics. They are not the
thing that makes two graph terms different.

The intended invariant is:

```text
\A : @ => List A
\B : @ => List B
```

should compile to the same computation shape, because the binder names differ
only by alpha-renaming. At the graph-computation level this is the same as:

```text
\_ => List _
```

with binder identity carried by the graph and the source names carried by
metadata/debug output.

There are three separate equality layers:

```text
graph identity:
  structurally identical graph terms share the same node when possible

judgemental equality:
  terms normalize to compatible graph structure

path/propositional/cubical equality:
  equality is represented by graph terms such as #.Path A a b
```

The current implementation does not yet provide a full judgemental equality
engine. It only uses a narrow weak-head classifier normalization inside the
synthetic `MATCH` typing rule. This rule is intentionally local:

- it may beta-reduce through existing graph evaluation;
- it may collapse a classifier `MATCH` whose branches have the same normalized
  classifier;
- it does not discharge `EXPECTS_TYPE`;
- it does not prove path equality;
- it is expected to move behind the future judgemental equality API once that
  API exists.

The current prototype is still transitional. `VAR` and `LAMBDA` nodes still
store symbol IDs for printing, but those names must not participate in semantic
comparison or interning. Any remaining comparison that treats binder display
names as graph identity is a design bug.
